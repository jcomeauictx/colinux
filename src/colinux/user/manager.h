/*
 * This source code is a part of coLinux source package.
 *
 * Dan Aloni <da-x@colinux.org>, 2003 (c)
 *
 * The code is licensed under the GPL. See the COPYING file at
 * the root directory.
 *
 */ 

#ifndef __COLINUX_USER_MANAGER_H__
#define __COLINUX_USER_MANAGER_H__

#include <colinux/common/ioctl.h>
#include <colinux/os/user/manager.h>

extern co_rc_t co_manager_io_monitor(co_manager_handle_t handle, 
				     co_monitor_ioctl_op_t op,
				     co_manager_ioctl_monitor_t *ioctl,
				     unsigned long in_size,
				     unsigned long out_size);

extern co_rc_t co_manager_io_monitor_unisize(co_manager_handle_t handle, 
					     co_monitor_ioctl_op_t op,
					     co_manager_ioctl_monitor_t *ioctl,
					     unsigned long size);

extern co_rc_t co_manager_status(co_manager_handle_t handle, 
				 co_manager_ioctl_status_t *status);

extern co_rc_t co_manager_info(co_manager_handle_t handle, 
				 co_manager_ioctl_info_t *status);

extern void co_manager_debug(co_manager_handle_t handle, 
			     const char *buf, long size);

extern co_rc_t co_manager_debug_reader(co_manager_handle_t handle, 
				       co_manager_ioctl_debug_reader_t *debug_reader);

extern co_rc_t co_manager_debug_levels(co_manager_handle_t handle, 
				       co_manager_ioctl_debug_levels_t *levels);

extern co_rc_t co_manager_attach(co_manager_handle_t handle, co_manager_ioctl_attach_t *params);

#endif
