/*
 * This source code is a part of coLinux source package.
 *
 * Dan Aloni <da-x@colinux.org>, 2003 (c)
 *
 * The code is licensed under the GPL. See the COPYING file at
 * the root directory.
 *
 */

#ifndef __CO_OS_KERNEL_MISC_H__
#define __CO_OS_KERNEL_MISC_H__

#include <colinux/common/common.h>

extern uintptr_t co_os_virt_to_phys(void *addr);
extern co_rc_t co_os_physical_memory_pages(uintptr_t *pages);
extern co_id_t co_os_current_id(void);

#endif
