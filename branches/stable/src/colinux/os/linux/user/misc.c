/*
 * This source code is a part of coLinux source package.
 *
 * Dan Aloni <da-x@colinux.org>, 2003 (c)
 *
 * The code is licensed under the GPL. See the COPYING file at
 * the root directory.
 *
 */ 

#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

#include <colinux/os/alloc.h>
#include <colinux/os/user/misc.h>

void co_terminal_print(const char *format, ...)
{
	char buf[0x100];
	va_list ap;
	int len;

	va_start(ap, format);
	vsnprintf(buf, sizeof(buf), format, ap);
	va_end(ap);

	printf("%s: %s", _colinux_module, buf);

	len = strlen(buf);
	while (len > 0  &&  buf[len-1] == '\n')
		buf[len - 1] = '\0';
		
	co_debug_lvl(prints, 11, "prints \"%s\"\n", buf);
}

double co_os_timer_highres()
{
        struct timeval tv;

        gettimeofday(&tv, NULL);

        return ((double)tv.tv_sec) + ((double)tv.tv_usec)/1000000;
}

void co_os_get_debug_timestamp(co_debug_timestamp_t *dts)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);

	dts->high = tv.tv_sec;
        dts->low = tv.tv_usec;
}
