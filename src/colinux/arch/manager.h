/*
 * This source code is a part of coLinux source package.
 *
 * Dan Aloni <da-x@colinux.org>, 2003 (c)
 *
 * The code is licensed under the GPL. See the COPYING file at
 * the root directory.
 */

#ifndef __COLINUX_ARCH_MANAGER_H__
#define __COLINUX_ARCH_MANAGER_H__

#include <colinux/kernel/monitor.h>

typedef struct co_archdep_manager *co_archdep_manager_t;

extern co_rc_t co_manager_arch_init(struct co_manager *manager, co_archdep_manager_t *out_archdep);
extern void co_manager_arch_free(co_archdep_manager_t out_archdep);

#endif
