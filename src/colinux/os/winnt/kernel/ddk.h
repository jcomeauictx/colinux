/*
 * This source code is a part of coLinux source package.
 *
 * Dan Aloni <da-x@colinux.org>, 2003 (c)
 *
 * The code is licensed under the GPL. See the COPYING file at
 * the root directory.
 *
 */

#ifndef __NESTED_WINNT_DDK_H__
#define __NESTED_WINNT_DDK_H__

// Keep Linux kernel typedefs and MinGW CRT typedefs compatible during cross-compilation
#ifndef _SIZE_T
#define _SIZE_T
#endif

#ifndef _SSIZE_T
#define _SSIZE_T
#endif

#ifndef _PTRDIFF_T
#define _PTRDIFF_T
#endif

#ifndef _TIME_T
#define _TIME_T
#endif

#ifndef _CLOCK_T
#define _CLOCK_T
#endif

#ifndef __uintptr_t_defined
#define __uintptr_t_defined
typedef unsigned long uintptr_t;
#endif

#include <ddk/ntddk.h>

#endif
