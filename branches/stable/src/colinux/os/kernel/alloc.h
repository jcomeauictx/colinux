/*
 * This source code is a part of coLinux source package.
 *
 * Dan Aloni <da-x@colinux.org>, 2003 (c)
 *
 * The code is licensed under the GPL. See the COPYING file at
 * the root directory.
 *
 */

#ifndef __CO_OS_KERNEL_ALLOC_H__
#define __CO_OS_KERNEL_ALLOC_H__

#include <colinux/os/alloc.h>
#include <colinux/kernel/manager.h>
#include <colinux/arch/current/mmu.h>

/*
 * Interfaces for physical memory allocation.
 */
extern co_rc_t co_os_get_page(struct co_manager *manager, co_pfn_t *pfn);
extern void *co_os_map(struct co_manager *manager, co_pfn_t pfn);
extern void co_os_unmap(struct co_manager *manager, void *ptr);
extern void co_os_put_page(struct co_manager *manager, co_pfn_t pfn);

/* 
 * Obsolete:
 */
extern void *co_os_alloc_pages(unsigned long pages);
extern void co_os_free_pages(void *ptr, unsigned long pages);

#endif
