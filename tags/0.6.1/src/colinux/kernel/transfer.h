/*
 * This source code is a part of coLinux source package.
 *
 * Dan Aloni <da-x@colinux.org>, 2003 (c)
 *
 * The code is licensed under the GPL. See the COPYING file at
 * the root directory.
 */

#ifndef __COLINUX_KERNEL_TRANSFER_H__
#define __COLINUX_KERNEL_TRANSFER_H__

/*
 * Routines to copy data between coLinux space and host space.
 */

#include "monitor.h"
#include "manager.h"

typedef enum {
	/* From Host OS to coLinux */
	CO_MONITOR_TRANSFER_FROM_HOST,

	/* From coLinux to Host OS */
	CO_MONITOR_TRANSFER_FROM_LINUX,
} co_monitor_transfer_dir_t;

/*
 * This is lambda function that does the actual copying. In most
 * cases it will copy a maximum of PAGE_SIZE bytes per call, because 
 * coLinux memory is fragmented.
 *
 * host_data - User information regarding host address
 * para - coLinux-host mapped addrsss
 * size - Number of bytes to copy
 * dir - copy direction
 */

typedef co_rc_t (*co_monitor_transfer_func_t)(
	co_monitor_t *cmon, 
	void *host_data, 
	void *linuxvm, 
	unsigned long size, 
	co_monitor_transfer_dir_t dir
	);


extern co_rc_t co_monitor_host_linuxvm_transfer(
	co_monitor_t *cmon, 
	void *host_data, 
	co_monitor_transfer_func_t host_func, 
	vm_ptr_t para, 
	unsigned long size, 
	co_monitor_transfer_dir_t dir
	);

/*
 * memcpy-like transfer implementation:
 */

extern co_rc_t co_monitor_host_to_linuxvm(co_monitor_t *cmon, void *from, 
					  vm_ptr_t to, unsigned long size); 
extern co_rc_t co_monitor_linuxvm_to_host(co_monitor_t *cmon, vm_ptr_t from, 
					  void *to, unsigned long size); 

extern co_rc_t co_monitor_copy_and_create_pfns(
	co_monitor_t *monitor, 
	vm_ptr_t address,
	unsigned long size,
	char *source
	);

extern co_rc_t co_monitor_scan_and_create_pfns(
	co_monitor_t *monitor, 
	vm_ptr_t address,
	unsigned long size
	);

extern co_rc_t co_monitor_create_ptes(
	co_monitor_t *monitor, 
	vm_ptr_t address,
	unsigned long size,
	co_pfn_t *source
	);

#endif
