/*
 * This source code is a part of coLinux source package.
 *
 * Dan Aloni <da-x@gmx.net>, 2003 (c)
 *
 * The code is licensed under the GPL. See the COPYING file at
 * the root directory.
 */ 

#ifndef __COLINUX_USER_WINNT_DAEMON_H__
#define __COLINUX_USER_WINNT_DAEMON_H__

#include <windows.h>

#include <colinux/os/user/daemon.h>

struct co_daemon_handle {
	HANDLE handle;
};


#endif
