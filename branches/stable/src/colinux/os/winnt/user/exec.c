/*
 * This source code is a part of coLinux source package.
 *
 * Dan Aloni <da-x@colinux.org>, 2003 (c)
 *
 * The code is licensed under the GPL. See the COPYING file at
 * the root directory.
 *
 */ 

#include <windows.h>

#include <stdio.h>

#include <colinux/os/user/exec.h>

co_rc_t co_launch_process(char *command_line, ...)
{
	BOOL ret;
	char buf[0x100];
	va_list ap;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	
	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
	memset(&pi, 0, sizeof(pi));

	va_start(ap, command_line);
	vsnprintf(buf, sizeof(buf), command_line, ap);
	va_end(ap);

	co_debug("executing: %s\n", buf);

	ret = CreateProcess(NULL,
			    buf,              // Command line. 
			    NULL,             // Process handle not inheritable. 
			    NULL,             // Thread handle not inheritable. 
			    FALSE,            // Set handle inheritance to FALSE. 
			    0,                // No creation flags. 
			    NULL,             // Use parent's environment block. 
			    NULL,             // Use parent's starting directory. 
			    &si,              // Pointer to STARTUPINFO structure.
			    &pi);             // Pointer to PROCESS_INFORMATION structure.

	if (!ret) {
		co_terminal_print("error in execution '%s' (%d)\n", buf, GetLastError());
		return CO_RC(ERROR);
	}

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	return CO_RC(OK);
}
