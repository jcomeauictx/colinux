/*
 * This source code is a part of coLinux source package.
 *
 * Dan Aloni <da-x@gmx.net>, 2003 (c)
 *
 * The code is licensed under the GPL. See the COPYING file at
 * the root directory.
 *
 */

#include <malloc.h>

#include <colinux/os/alloc.h>

void *
co_os_malloc(unsigned long size)
{
	return malloc(size);
}

void *
co_os_realloc(void *ptr, unsigned long size)
{
	return realloc(ptr, size);
}

void
co_os_free(void *ptr)
{
	free(ptr);
}
