/*
 * This source code is a part of coLinux source package.
 *
 * Dan Aloni <da-x@colinux.org>, 2003 (c)
 *
 * The code is licensed under the GPL. See the COPYING file at
 * the root directory.
 */

#ifndef __COLINUX_KERNEL_MANAGER_H__
#define __COLINUX_KERNEL_MANAGER_H__

#include <colinux/arch/manager.h>
#include <colinux/arch/mmu.h>
#include <colinux/common/queue.h>

#include "debug.h"

typedef struct co_osdep_manager *co_osdep_manager_t;

typedef enum {
	CO_MANAGER_STATE_NOT_INITIALIZED,
	CO_MANAGER_STATE_INITIALIZED_DEBUG,
	CO_MANAGER_STATE_INITIALIZED_ARCH,
	CO_MANAGER_STATE_INITIALIZED_OSDEP,
	CO_MANAGER_STATE_INITIALIZED,
} co_manager_state_t;

struct co_monitor;
struct co_manager_open_desc_os;

typedef struct co_manager_open_desc_os *co_manager_open_desc_os_t;

typedef struct co_manager_open_desc {
	co_list_t node;

	bool_t active;
	int ref_count;
	co_os_mutex_t lock;

	bool_t monitor_owner;
	struct co_monitor *monitor;

	co_queue_t out_queue;

	co_manager_open_desc_os_t os;
	co_debug_section_t *debug_section;
} *co_manager_open_desc_t;

/*
 * The manager module manages the running coLinux systems.
 */
typedef struct co_manager {
	co_manager_state_t state;

	uintptr_t hostmem_amount;
	uintptr_t hostmem_used;
	uintptr_t hostmem_usage_limit;
	uintptr_t hostmem_pages;

	co_pfn_t *reversed_map_pfns;
	uintptr_t reversed_page_count;
	uintptr_t *reversed_map_pgds;
	uintptr_t reversed_map_pgds_count;

	co_osdep_manager_t osdep;
	co_archdep_manager_t archdep;

	co_manager_debug_t debug;

	co_list_t monitors;
	uintptr_t monitors_count;

	co_list_t opens;
	uintptr_t num_opens;
	co_os_mutex_t lock;
} co_manager_t;

extern co_manager_t *co_global_manager;

extern co_rc_t co_manager_load(co_manager_t *manager);

extern co_rc_t co_manager_ioctl(co_manager_t *manager, uintptr_t ioctl,
				void *io_buffer, uintptr_t in_size,
				uintptr_t out_size, uintptr_t *return_size,
				co_manager_open_desc_t opened);

extern co_rc_t co_manager_send_eof(co_manager_t *manager, co_manager_open_desc_t opened);
extern co_rc_t co_manager_send(co_manager_t *manager, co_manager_open_desc_t opened, co_message_t *message);
extern co_rc_t co_manager_open(co_manager_t *manager, co_manager_open_desc_t *opened_out);
extern co_rc_t co_manager_open_ref(co_manager_open_desc_t opened);
extern co_rc_t co_manager_open_desc_deactive_and_close(co_manager_t *manager, co_manager_open_desc_t opened);
extern co_rc_t co_manager_close(co_manager_t *manager, co_manager_open_desc_t opened);

extern void co_manager_unload(co_manager_t *manager);

#endif
