/*
 * This source code is a part of coLinux source package.
 *
 * Dan Aloni <da-x@colinux.org>, 2003 (c)
 *
 * The code is licensed under the GPL. See the COPYING file at
 * the root directory.
 *
 */

#include "linux_inc.h"

#include <colinux/os/kernel/time.h>
#include <colinux/common/common.h>
#include <colinux/os/timer.h>

unsigned long co_os_get_time(void)
{
	return get_seconds();
}

void co_os_get_timestamp(co_timestamp_t *dts)
{
	struct timeval tv;

	do_gettimeofday(&tv);

	dts->quad = tv.tv_sec;
	dts->quad *= 1000000;
        dts->quad += tv.tv_usec;
}

void co_os_get_timestamp_freq(co_timestamp_t *dts)
{
	dts->quad = 1000000;
}

