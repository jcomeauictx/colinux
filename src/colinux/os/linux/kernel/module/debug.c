/*
 * This source code is a part of coLinux source package.
 *
 * Dan Aloni <da-x@colinux.org>, 2004 (c)
 *
 * The code is licensed under the GPL. See the COPYING file at
 * the root directory.
 *
 */ 

#include "linux_inc.h"
#include "manager.h"
#include <colinux/kernel/debug.h>

void co_debug_line(char *str)
{
	if (!global_manager) {
		printk("%s", str);
		return;
	}

	if (!global_manager->debug.ready) {
		printk("%s", str);
		return;
	}

	co_debug_write_str(&global_manager->debug, &global_manager->debug.section, str);
}

void co_debug_system(const char *fmt, ...)
{
	char buf[0x100];
	va_list ap;

	va_start(ap, fmt);
	co_vsnprintf(buf, sizeof(buf), fmt, ap);
	printk("%s", buf);
	va_end(ap);
}
