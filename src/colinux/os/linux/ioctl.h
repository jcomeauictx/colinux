/*
 * This source code is a part of coLinux source package.
 *
 * Dan Aloni <da-x@colinux.org>, 2003 (c)
 *
 * The code is licensed under the GPL. See the COPYING file at
 * the root directory.
 *
 */

#ifndef __CO_LINUX_IOCTL_H__
#define __CO_LINUX_IOCTL_H__

typedef struct co_linux_io {
	uintptr_t 	code;
	void*		input_buffer;
	uintptr_t	input_buffer_size;
	void*		output_buffer;
	uintptr_t	output_buffer_size;
	uintptr_t*	output_returned;
} co_linux_io_t;

#define CO_LINUX_IOCTL_ID  0x12340000

#endif
