/*
 * This source code is a part of coLinux source package.
 *
 * Dan Aloni <da-x@gmx.net>, 2003-2004 (c)
 * Some code taken from WINE's X11 keyboard driver (c).
 *
 * The code is licensed under the GPL. See the COPYING file at
 * the root directory.
 *
 */ 

extern "C" {
	#include <colinux/common/debug.h>
}

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <FL/x.h>
#include <colinux/user/console-fltk/main.h>

#include <stdio.h>

COLINUX_DEFINE_MODULE("colinux-fltk-console");

/* 
 * Stolen WINE goodies...
 *
 * For coLinux's keyboard driver we need something than can convert
 * X11 keyboard events to keyboard scancode. Because WINE already did
 * something similar for converting to Windows events, I just took
 * some code from there.
 *
 * - DA
 */

static const short int nonchar_key_scan[256] =
{
	/* unused */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FF00 */
	/* special keys */
	0x0E, 0x0F, 0x00, /*?*/ 0, 0x00, 0x1C, 0x00, 0x00,           /* FF08 */
	0x00, 0x00, 0x00, 0x45, 0x46, 0x00, 0x00, 0x00,              /* FF10 */
	0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,              /* FF18 */
	/* unused */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FF20 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FF28 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FF30 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FF38 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FF40 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FF48 */
	/* cursor keys */
	0x147, 0x14B, 0x148, 0x14D, 0x150, 0x149, 0x151, 0x14F,      /* FF50 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FF58 */
	/* misc keys */
	/*?*/ 0, 0x137, /*?*/ 0, 0x152, 0x00, 0x00, 0x00, 0x00,      /* FF60 */
	/*?*/ 0, /*?*/ 0, 0x38, 0x146, 0x00, 0x00, 0x00, 0x00,       /* FF68 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FF70 */
	/* keypad keys */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x138, 0x145,            /* FF78 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FF80 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x11C, 0x00, 0x00,             /* FF88 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x47, 0x4B, 0x48,              /* FF90 */
	0x4D, 0x50, 0x49, 0x51, 0x4F, 0x4C, 0x52, 0x53,              /* FF98 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FFA0 */
	0x00, 0x00, 0x37, 0x4E, /*?*/ 0, 0x4A, 0x53, 0x135,          /* FFA8 */
	0x52, 0x4F, 0x50, 0x51, 0x4B, 0x4C, 0x4D, 0x47,              /* FFB0 */
	0x48, 0x49, 0x00, 0x00, 0x00, 0x00,                          /* FFB8 */
	/* function keys */
	0x3B, 0x3C,
	0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44,              /* FFC0 */
	0x57, 0x58, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FFC8 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FFD0 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FFD8 */
	/* modifier keys */
	0x00, 0x2A, 0x36, 0x1D, 0x11D, 0x3A, 0x00, 0x38,             /* FFE0 */
	0x138, 0x38, 0x138, 0x00, 0x00, 0x00, 0x00, 0x00,            /* FFE8 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FFF0 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x153              /* FFF8 */
};

static const short int nonchar_key_scan2[256] =
{
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FE00 */
	0x1D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FE08 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /*      */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /*      */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /*      */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /*      */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /*      */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /*      */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /*      */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /*      */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /*      */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /*      */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /*      */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /*      */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /*      */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /*      */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /*      */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /*      */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /*      */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /*      */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /*      */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /*      */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /*      */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /*      */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /*      */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /*      */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /*      */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /*      */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /*      */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /*      */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /*      */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /*      */
};

static int scan_code_state[0x200];

static int global_event_hook(const XEvent& thisevent)
{
	XEvent xevent = thisevent;

	switch (xevent.type) {
	case KeyPress:
	case KeyRelease: {
		co_scan_code_t sc;
		int len;
		char buffer[0x40];
		KeySym keysym;
		int keycode = xevent.xkey.keycode;
		int scan = 0;

		len = XLookupString((XKeyEvent *)&(xevent.xkey), buffer, sizeof(buffer)-1, &keysym, 0);
		
		if (keysym) {
			if ((keysym >> 8) == 0xFF) {
				scan = nonchar_key_scan[keysym & 0xff];
			} else if ((keysym >> 8) == 0xFE) {
				scan = nonchar_key_scan2[keysym & 0xff];
			} else if (keysym == 0x20) {
				scan = 0x39;
			} else {
				scan = keycode - 8;
			}
		}	

		if (xevent.type != KeyPress) {
			if (scan_code_state[scan] == 0)
				return 0;	/* ignore release of not pressed keys */
			scan_code_state[scan] = 0;
			scan |= 0x80;
		} else {
			scan_code_state[scan] = 1;
		}

		sc.mode = CO_KBD_SCANCODE_RAW;
		/* send e0 if extended key */
		if (scan & 0xFF00) {
			sc.code = 0xE0;
			co_user_console_handle_scancode(sc);
		}
		sc.code = scan & 0xFF;
		co_user_console_handle_scancode(sc);

		break;
	}
	}

	return 0;
}

void co_user_console_keyboard_focus_change(unsigned long keyboard_focus)
{
}

int main(int argc, char **argv) 
{
	fl_x_global_event_hook = global_event_hook;

	return co_user_console_main(argc, argv);
}
