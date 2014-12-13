/*
 * This source code is a part of coLinux source package.
 *
 * Dan Aloni <da-x@colinux.org>, 2004 (c)
 *
 * The code is licensed under the GPL. See the COPYING file at
 * the root directory.
 */

#ifndef __COLINUX_ARCH_I386_CPUID_H__
#define __COLINUX_ARCH_I386_CPUID_H__

#include <colinux/common/common.h>

typedef union {
	struct {
		uintptr_t eax, ebx, ecx, edx;
	};
	struct {
		uintptr_t highest_op;
		char id_string[12];
	};
} cpuid_t;

bool_t co_i386_has_cpuid(void);
void co_i386_get_cpuid(uintptr_t op, cpuid_t *cpuid);
co_rc_t co_i386_get_cpuid_capabilities(uintptr_t *caps);

#endif
