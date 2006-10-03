/*
 * This source code is a part of coLinux source package.
 *
 * Dan Aloni <da-x@colinux.org>, 2003 (c)
 *
 * The code is licensed under the GPL. See the COPYING file at
 * the root directory.
 */

#include <stdlib.h>
#include <strings.h>
#include <memory.h>

#include "libc.h"

#ifdef CO_LIBC__MISC

void *co_memset(void *s, int c, long n)
{
	return memset(s, c, n);
}

void *co_memcpy(void *dest, const void *src, long n)
{
	return memcpy(dest, src, n);
}

void *co_memmove(void *dest, const void *src, long n)
{
	return memmove(dest, src, n);
}

void co_bzero(void *s, long n)
{
	co_memset(s, 0, n);
}

const char *co_strstr(const char *haystack, const char *needle)
{
	return strstr(haystack, needle);
}

int co_strlen(const char *s)
{
	return strlen(s);
}

/* copied from Linux's string lib */
static int internal_strcmp(const char * cs,const char * ct)
{
	register signed char __res;

	while (1) {
		if ((__res = *cs - *ct++) != 0 || !*cs++)
			break;
	}

	return __res;
}

int co_strcmp(const char *s1, const char *s2)
{
	return internal_strcmp(s1, s2);
}

/* copied from Linux's string lib */
static int internal_strncmp(const char * cs,const char * ct,size_t count)
{
	register signed char __res = 0;

	while (count) {
		if ((__res = *cs - *ct++) != 0 || !*cs++)
			break;
		count--;
	}

	return __res;
}


int co_strncmp(const char *s1, const char *s2, int n)
{
	return internal_strncmp(s1, s2, n);
}

#endif

#ifdef CO_LIBC__STRTOL

int co_strtol(const char *s1, char **s2, int n)
{
	return strtol(s1, s2, n);
}

#endif
