/*
 * This source code is a part of coLinux source package.
 *
 * Dan Aloni <da-x@gmx.net>, 2003 (c)
 *
 * The code is licensed under the GPL. See the COPYING file at
 * the root directory.
 */

#ifndef __CO_CONFIGURATION_H__
#define __CO_CONFIGURATION_H__

#include "common.h"

/*
 * Per block device configuration
 */
typedef struct co_block_dev_desc {
	/*
	 * This bool var determines whether Linux would be given 
	 * permission to access this device.
	 */
	bool_t enabled;

	/*
	 * The pathname of the host OS file (or device) that represents 
	 * this block device. Can be relative to the pathname of the 
	 * configration file.
	 */
	co_pathname_t pathname;
} co_block_dev_desc_t;

/*
 * Per-machine coLinux configuration
 */
typedef struct co_config {
	/*
	 * Absolute pathname of the configuration file (relative pathnames
	 * will be looked upon according to this path).
	 */
	co_pathname_t config_path;
	
	/*
	 * The pathname of the vmlinux file. If this is empty then we 
	 * would try by default to locate 'vmlinux' in the same directory
	 * where the configuration file resides.
	 */
	co_pathname_t vmlinux_path;

	/*
	 * Information about block devices that we import tok
	 * Linux and the index of the block device to boot from.
	 */
	co_block_dev_desc_t block_devs[CO_MAX_BLOCK_DEVICES];
	long block_root_device_index;

	/*
	 * Parameters passed to the kernel at boot.
	 */
	char boot_parameters_line[CO_BOOTPARAM_STRING_LENGTH];

	/*
	 * Size of pseudo physical RAM for this machine (MB).
	 * 
	 * The default size is 32MB for systems with more than
	 * 128MB of physical ram and 16MB for systems with less
	 * ram.
	 */ 
	unsigned long ram_size;
} co_config_t;

#endif
