/*
 * This source code is a part of coLinux source package.
 *
 * Dan Aloni <da-x@gmx.net>, 2003 (c)
 *
 * The code is licensed under the GPL. See the COPYING file at
 * the root directory.
 *
 */ 

#include "widget.h"
#include <colinux/user/console-fltk/main.h>

static int default_red[] = {0x00,0xaa,0x00,0xaa,0x00,0xaa,0x00,0xaa,
			    0x55,0xff,0x55,0xff,0x55,0xff,0x55,0xff};

static int default_grn[] = {0x00,0x00,0xaa,0xaa,0x00,0x00,0xaa,0xaa,
			    0x55,0x55,0xff,0xff,0x55,0x55,0xff,0xff};

static int default_blu[] = {0x00,0x00,0x00,0x00,0xaa,0xaa,0xaa,0xaa,
			    0x55,0x55,0x55,0x55,0xff,0xff,0xff,0xff};

#define FL_RGB_COLOR(i) fl_color(default_blu[i], \
				 default_grn[i], \
				 default_red[i])

console_widget_t::console_widget_t(int		x,
			           int		y,
			           int		w,
			           int 		h,
			           const char* 	label)
: Fl_Widget(x, y, w, h, label)
{
	font_size 	      = 18;
	letter_x	      = font_size;
	letter_y	      = font_size;
	cursor_blink_interval = 0.1;
	cursor_blink_state    = 1;
	console               = NULL;
	fit_x                 = 0;
	fit_y		      = 0;
	
	mouse_copy			= false;
	mouse_start_x		= 0;
	mouse_start_y		= 0;
	mouse_sx = 0;
	mouse_wx = 0;
	mouse_sy = 0; 
	mouse_wy = 0;
	loc_start = 0;
	loc_end = 0;

	Fl::add_timeout(cursor_blink_interval,
		        (Fl_Timeout_Handler)(console_widget_t::static_blink_handler),
		        this);
}

void console_widget_t::static_blink_handler(console_widget_t* widget)
{
	widget->blink_handler();
}

void console_widget_t::blink_handler()
{
	if (console) {
		damage_console(console->cursor.x, console->cursor.y, 1, 1);

		/* 

		For the cursor to blink we would do: cursor_blink_state = !cursor_blink_state

		However, we need to fix a problem with console_idle() which causes timers not
		to execute unless there are input events.
		
		*/
	}

	Fl::add_timeout(cursor_blink_interval,
		        (Fl_Timeout_Handler)(console_widget_t::static_blink_handler), 
		        this);
}

void console_widget_t::set_font_size(int size)
{
	font_size = size;
}

void console_widget_t::damage_console(int x_, int y_, int w_, int h_)
{
	int cx;
	int cy;

	cx = x();
	cy = y();

	cx -= (fit_x - w()) / 2;
	cy -= (fit_y - h()) / 2;

	damage(1, 
	       cx + x_ * letter_x, 
	       cy + y_ * letter_y, 
	       w_ * letter_x, 
	       h_ * letter_y);
}

void console_widget_t::draw()
{
	if (console == NULL) {
		int x_, y_, w_, h_;
		fl_clip_box(x(),y(),w(),h(), x_, y_, w_, h_);
		fl_color(FL_BLACK);
		fl_rectf(x_, y_, w_, h_);
		return;
	}

	fl_font(FL_SCREEN, font_size);

	int x_, y_, w_, h_, cx, cy;	
	fl_clip_box(x(), y(), w(), h(), x_, y_, w_, h_);

	cx = x();
	cy = y();

	cx -= (fit_x - w()) / 2;
	cy -= (fit_y - h()) / 2;
	
	x_ -= cx;
	y_ -= cy;

	int x1 = x_ / letter_x;
	int x2 = (x_ + w_) / letter_x + 1;
	int y1 = y_ / letter_y;
	int y2 = (y_ + h_) / letter_y + 1;

	int yi = 0;

	if (x_ < 0) {
		fl_color(FL_BLACK);
		fl_rectf(x(), y(), -x_, h_);

		x1 = 0;
		cx += x1 * letter_x;
	}

	if (y_ < 0) {
		fl_color(FL_BLACK);
		fl_rectf(cx, y(), fit_x, -y_);
	}

	if (x_ + w_ > fit_x) {
		fl_color(FL_BLACK);
		fl_rectf(cx + fit_x, y(), x_ + w_ - fit_x, h_);

		x2 = console->config.x;
	}

	if (y_ + h_ > fit_y) {
		fl_color(FL_BLACK);
		fl_rectf(cx, cy + fit_y,
			 fit_x, y_ + h_ - fit_y);
	}

	fl_push_clip(x(), y(), w(), h());

	for (yi = y1; yi < y2; yi++) {
		if (yi < 0)
			continue;

		if (yi >= console->config.y)
			break;

		co_console_cell_t* row_start = &console->screen[yi * console->config.x];
		co_console_cell_t* cell;
		co_console_cell_t* start;
		co_console_cell_t* end;
		char               text_buff[0x100];

		start = row_start + x1;
		end   = row_start + x2;
		cell  = start;

		if (end > cell_limit) {
			// co_debug("BUG: end=%p limit=%p row=%p start=%p x1=%d x2=%d y1=%d y2=%d",
			//     end, limit, row_start, start, x1, x2, y1, y2);
			end = cell_limit; // Hack: Fix the overrun!
		}
		
		while (cell < end) {
			while (cell < end  &&  start->attr == cell->attr  &&
			       cell - start < (int)sizeof(text_buff)) {
				text_buff[cell - start] = cell->ch;
				cell++;
			}

			FL_RGB_COLOR((start->attr >> 4) & 0xf);
			fl_rectf(cx + letter_x * (start - row_start), 
				 cy + letter_y * (yi),
				 (cell - start) * letter_x,
				 letter_y);
			
			FL_RGB_COLOR((start->attr) & 0xf);
			fl_draw(text_buff, cell - start, 
				cx + letter_x * (start - row_start), 
				cy + letter_y * (yi + 1) - fl_descent());

			start = cell;
		}

		// The cell under the cursor
		if (cursor_blink_state && console->cursor.y == yi &&
		    console->cursor.x >= x1 && console->cursor.x <= x2 &&
		    console->cursor.height != CO_CUR_NONE) {
			int cursize;

			if (console->cursor.height <= CO_CUR_BLOCK &&
			    console->cursor.height > CO_CUR_NONE)
				cursize = cursize_tab[console->cursor.height];
			else
				cursize = cursize_tab[CO_CUR_DEF];

			fl_color(0xff, 0xff, 0xff);
			fl_rectf(cx + letter_x * console->cursor.x, 
				 cy + letter_y * console->cursor.y + letter_y - cursize,
				 letter_x,
				 cursize);
		}
	}
	
	fl_pop_clip();
}

void console_widget_t::set_console(co_console_t* _console)
{
	console = _console;

	fl_font(FL_SCREEN, font_size);

	// Calculate constats for this font
	letter_x = (int)fl_width('A');
	letter_y = (int)fl_height();

	fit_x = letter_x * console->config.x;
	fit_y = letter_y * console->config.y;

	cell_limit = &console->screen[console->config.y * console->config.x];

	cursize_tab[CO_CUR_UNDERLINE]	= letter_y / 6 + 1; /* round up 0.1667 */
	cursize_tab[CO_CUR_LOWER_THIRD]	= letter_y / 3;
	cursize_tab[CO_CUR_LOWER_HALF]	= letter_y / 2;
	cursize_tab[CO_CUR_TWO_THIRDS]	= (letter_y * 2) / 3 + 1; /* round up 0.667 */
	cursize_tab[CO_CUR_BLOCK]	= letter_y;
	cursize_tab[CO_CUR_DEF]		= cursize_tab[console->config.curs_type_size];
}

co_console_t* console_widget_t::get_console()
{
	co_console_t* _console = console;
	console = NULL;

	return _console;
}

/* Process console messages:
	CO_OPERATION_CONSOLE_PUTC		- Put single char
	CO_OPERATION_CONSOLE_PUTCS		- Put char array
	
	CO_OPERATION_CONSOLE_CURSOR_MOVE	- Move cursor
	CO_OPERATION_CONSOLE_CURSOR_DRAW	- Move cursor
	CO_OPERATION_CONSOLE_CURSOR_ERASE	- Move cursor
	
	CO_OPERATION_CONSOLE_SCROLL_UP		- Scroll lines up
	CO_OPERATION_CONSOLE_SCROLL_DOWN	- Scroll lines down
	CO_OPERATION_CONSOLE_BMOVE		- Move region up/down
	
	CO_OPERATION_CONSOLE_CLEAR		- Clear region
	
	CO_OPERATION_CONSOLE_STARTUP		- Ignored
	CO_OPERATION_CONSOLE_INIT		- Ignored
	CO_OPERATION_CONSOLE_DEINIT		- Ignored
	CO_OPERATION_CONSOLE_SWITCH		- Ignored
	CO_OPERATION_CONSOLE_BLANK		- Ignored
	CO_OPERATION_CONSOLE_FONT_OP		- Ignored
	CO_OPERATION_CONSOLE_SET_PALETTE	- Ignored
	CO_OPERATION_CONSOLE_SCROLLDELTA	- Ignored
	CO_OPERATION_CONSOLE_SET_ORIGIN		- Ignored
	CO_OPERATION_CONSOLE_SAVE_SCREEN	- Ignored
	CO_OPERATION_CONSOLE_INVERT_REGION	- Ignored
	CO_OPERATION_CONSOLE_CONFIG		- Ignored
*/
co_rc_t console_widget_t::handle_console_event(co_console_message_t* message)
{
	co_rc_t		rc;
	co_cursor_pos_t saved_cursor = {0, };
	
	if (!console) {
		return CO_RC(ERROR);
	}

	switch (message->type) 
	{
	case CO_OPERATION_CONSOLE_CURSOR_DRAW:
	case CO_OPERATION_CONSOLE_CURSOR_ERASE:
	case CO_OPERATION_CONSOLE_CURSOR_MOVE:
		// Only for using below
		saved_cursor = console->cursor;
		break;
	default:
		break;
	}

	rc = co_console_op(console, message);
	if (!CO_OK(rc))
		return rc;

	mouse_clear();
	switch (message->type) 
	{
	case CO_OPERATION_CONSOLE_SCROLL_UP:
	case CO_OPERATION_CONSOLE_SCROLL_DOWN: {
		unsigned long t = message->scroll.top;		/* Start of scroll region (row) */
		unsigned long b = message->scroll.bottom + 1;  	/* End of scroll region (row)	*/

		damage_console(0, t, console->config.x, b - t + 1);
		break;
	}
	case CO_OPERATION_CONSOLE_PUTCS: {
		int x       = message->putcs.x;
		int y	    = message->putcs.y;
		int count   = message->putcs.count;
		int  sx	    = x;
		int  scount = 0;

		while (x < console->config.x  &&  count > 0) {
			x++;
			count--;
			scount++;
		}

		damage_console(sx, y, scount, 1);
		break;
	}
	case CO_OPERATION_CONSOLE_PUTC: {
		int x = message->putc.x;
		int y = message->putc.y;
		
		damage_console(x, y, 1, 1);
		break;
	}
	case CO_OPERATION_CONSOLE_CURSOR_MOVE:
	case CO_OPERATION_CONSOLE_CURSOR_DRAW:
	case CO_OPERATION_CONSOLE_CURSOR_ERASE: {
		damage_console(saved_cursor.x, saved_cursor.y, 1, 1);
		damage_console(console->cursor.x, console->cursor.y, 1, 1);
		break;
	}

	case CO_OPERATION_CONSOLE_CLEAR:{
		unsigned t = message->clear.top;
		unsigned l = message->clear.left;
		unsigned b = message->clear.bottom;
		unsigned r = message->clear.right;
		damage_console(l, t, r - l + 1, b - t + 1);
	}

	case CO_OPERATION_CONSOLE_BMOVE:{
		unsigned y = message->bmove.row;
		unsigned x = message->bmove.column;
		unsigned t = message->bmove.top;
		unsigned l = message->bmove.left;
		unsigned b = message->bmove.bottom;
		unsigned r = message->bmove.right;
		damage_console(x, y, r - l + 1, b - t + 1);

	}

	case CO_OPERATION_CONSOLE_STARTUP:
	case CO_OPERATION_CONSOLE_INIT:
	case CO_OPERATION_CONSOLE_DEINIT:
	case CO_OPERATION_CONSOLE_SWITCH:
	case CO_OPERATION_CONSOLE_BLANK:
	case CO_OPERATION_CONSOLE_FONT_OP:
	case CO_OPERATION_CONSOLE_SET_PALETTE:
	case CO_OPERATION_CONSOLE_SCROLLDELTA:
	case CO_OPERATION_CONSOLE_SET_ORIGIN:
	case CO_OPERATION_CONSOLE_SAVE_SCREEN:
	case CO_OPERATION_CONSOLE_INVERT_REGION:
	case CO_OPERATION_CONSOLE_CONFIG:
		break;
	}

	return CO_RC(OK);
}

void console_widget_t::mouse_push(int x, int y, bool drag_type)
{
	set_rect(mouse_sx, mouse_sy, mouse_wx, mouse_wy);
	damage_console(0, 0, console->config.x, console->config.y);
	
	mouse_start_x = x;
	mouse_start_y = y;
	mouse_sx = 0;
	mouse_wx = 0;
	mouse_sy = 0; 
	mouse_wy = 0;
	loc_start = 0;
	loc_end = 0;
	mouse_copy = true;
	mouse_drag_type = drag_type;
	
	return;
}

void console_widget_t::mouse_drag(int x, int y)
{
	/* feature to add, ALT does a rectangle copy */
	if (!mouse_copy)
		return;

	/* clear old selection */
	set_rect(mouse_sx, mouse_sy, mouse_wx, mouse_wy);
	damage_console(0, 0, console->config.x, console->config.y);
	calc_area(x, y);

	/* set new selection */
	set_rect(mouse_sx, mouse_sy, mouse_wx, mouse_wy);
	damage_console(0, 0, console->config.x, console->config.y);
	/*
	if(mouse_drag_type) // rect select
		damage_console(MIN(mouse_sx, old_sx), MIN(mouse_sy, old_sy), 
			MAX(mouse_wx, old_wx), MAX(mouse_wy, old_wy));
	else
		damage_console(0, MIN(mouse_sy, old_sy), 
			console->config.x, MAX(mouse_wy, old_wy)-MIN(mouse_sy, old_sy));
	*/
	return;
}

void console_widget_t::mouse_clear(void)
{
	if(mouse_copy)
	{
		set_rect(mouse_sx, mouse_sy, mouse_wx, mouse_wy);
		damage_console(0, 0, console->config.x, console->config.y);
	}
	mouse_start_x = 0;
	mouse_start_y = 0;
	mouse_sx = 0;
	mouse_wx = 0;
	mouse_sy = 0; 
	mouse_wy = 0;
	loc_start = 0;
	loc_end = 0;
	mouse_copy = false;
	
	return;
}

void console_widget_t::mouse_release(int x, int y)
{
	/* copy and paste into the command line by faking keyboard */
	/*
	if(!mouse_copy)
		return;
		
	co_scan_code_t sc;
	sc.mode = CO_KBD_SCANCODE_ASCII;

	for(int i_y=mouse_sy; i_y<mouse_sy+mouse_wy; i_y++)
	{
		for(int i_x=mouse_sx; i_x<mouse_sx+mouse_wx; i_x++)
		{
			sc.code = (console->screen+i_x+i_y*console->config.x)->ch;
			co_user_console_handle_scancode( sc );
		}
	}
	*/
	return;
}

void console_widget_t::copy_mouse_selection(char*str)
{
	/* copies mouse selection to supplied string */
	if(str==NULL)
		return;
	if(!mouse_copy)
		return;
	int indx_str = 0;
	if(mouse_drag_type)
	{
		/* copy rect */
		for(int i_y=mouse_sy; i_y<mouse_sy+mouse_wy; i_y++)
		{
			for(int i_x=mouse_sx; i_x<mouse_sx+mouse_wx; i_x++)
			{
				*(str+indx_str) = (console->screen+i_x+i_y*console->config.x)->ch;
				indx_str++;
			}
			*(str+indx_str) = '\n';
			indx_str++;
		}
	}
	else
	{
		/* copy lines */
		for(int i_loc=loc_start; i_loc<loc_end; i_loc++)
		{
			*(str+indx_str) = (console->screen+i_loc)->ch;
			indx_str++;
		}
	}
	
	/* null terminate the string */
	*(str+indx_str) = 0;
}

int console_widget_t::screen_size_bytes(void)
{
	return (console->config.x * console->config.y);
}

void console_widget_t::set_rect(int sx, int sy, int wx, int wy)
{
	if(mouse_drag_type) 
	{
		/* rect select */
		for(int i_y=sy; i_y<sy+wy; i_y++)
			for(int i_x=sx; i_x<sx+wx; i_x++)
				(console->screen+i_x+i_y*console->config.x)->attr = 
					~((console->screen+i_x+i_y*console->config.x)->attr);
	}
	else
	{
		/* line select */
		for(int i_loc=loc_start; i_loc<loc_end; i_loc++)
			(console->screen+i_loc)->attr = ~((console->screen+i_loc)->attr);
	}
	return;
}

void console_widget_t::calc_area(int x, int y)
{
	if(mouse_start_x<x)
	{
		mouse_sx = loc_x(mouse_start_x);
		mouse_wx = loc_x(x)-loc_x(mouse_start_x);
	}
	else
	{
		mouse_sx = loc_x(x);
		mouse_wx = loc_x(mouse_start_x)-loc_x(x);
	}

	if(mouse_start_y<y)
	{
		mouse_sy = loc_y(mouse_start_y);
		mouse_wy = loc_y(y)-loc_y(mouse_start_y);
	}
	else
	{
		mouse_sy = loc_y(y);
		mouse_wy = loc_y(mouse_start_y)-loc_y(y);
	}

	/* no point if mouse selection is empty */
	mouse_wx = (mouse_wx == 0) ? 1 : mouse_wx;
	mouse_wy = (mouse_wy == 0) ? 1 : mouse_wy;
	
	if(mouse_sx+mouse_wx > console->config.x)
		mouse_wx = console->config.x - mouse_sx;
	if(mouse_sy+mouse_wy > console->config.y)
		mouse_wy = console->config.y - mouse_sy;
		
	loc_start = loc_x(mouse_start_x) + loc_y(mouse_start_y) * console->config.x;
	loc_end = loc_x(x) + loc_y(y) * console->config.x;
	if(loc_start>loc_end)
	{
		loc_start = loc_x(x) + loc_y(y) * console->config.x;
		loc_end = loc_x(mouse_start_x) + loc_y(mouse_start_y) * console->config.x;
	}

	return;
}

int console_widget_t::loc_x(int mouse_x)
{
	int loc_x = mouse_x/letter_x;
	loc_x = (loc_x>console->config.x) ? console->config.x : loc_x;
	loc_x = (loc_x<0) ? 0 : loc_x;
	return loc_x;
}

int console_widget_t::loc_y(int mouse_y)
{
	int loc_y = (mouse_y-MENU_SIZE_PIXELS)/letter_y;
	loc_y = (loc_y>console->config.y) ? console->config.y : loc_y;
	loc_y = (loc_y<0) ? 0 : loc_y;
	return loc_y;
}
