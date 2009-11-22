
/* WinNT host: low level console routines */

#ifndef __CO_OS_USER_WINNT_OS_CONSOLE_H__
#define __CO_OS_USER_WINNT_OS_CONSOLE_H__

#if defined __cplusplus
extern "C" {
#endif

int co_console_set_cursor_size(void* out_h, int curs_size_prc);
  /* Set cursor size 
     Parameters:
       out_h         - console output handle
       curs_size_prc - size of cursor in percent
         <= 0   - turn of the cursor
         == 100 - set "fat" (full size) cursor
        
     Returns:
       >= 0 - norm
                old size of cursor in percent 
                  == 0 - cursor is invisible 
       <  0 - error
  */


#if defined __cplusplus
}
#endif

#endif /* __CO_OS_USER_WINNT_OS_CONSOLE_H__ */
