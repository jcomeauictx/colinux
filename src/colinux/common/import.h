/*
 * This source code is a part of coLinux source package.
 *
 * Dan Aloni <da-x@gmx.net>, 2003 (c)
 *
 * The code is licensed under the GPL. See the COPYING file at
 * the root directory.
 */

#ifndef __CO_KERNEL_SYMBOLS_IMPORT_H__
#define __CO_KERNEL_SYMBOLS_IMPORT_H__

typedef struct {
	unsigned long kernel_start;
	unsigned long kernel_end;
	unsigned long kernel_init_task_union;
	unsigned long kernel_colinux_start;
	unsigned long kernel_swapper_pg_dir;
	unsigned long kernel_pg0;
	unsigned long kernel_pg1;
	unsigned long kernel_pg2;
	unsigned long kernel_idt_table;
	unsigned long kernel_gdt_table;
} co_symbols_import_t;

#endif
