/*
 * This source code is a part of coLinux source package.
 *
 * Dan Aloni <da-x@colinux.org>, 2003 (c)
 *
 * The code is licensed under the GPL. See the COPYING file at
 * the root directory.
 */

#ifndef __CO_KERNEL_SYMBOLS_IMPORT_H__
#define __CO_KERNEL_SYMBOLS_IMPORT_H__

typedef struct {
	uintptr_t kernel_start;
	uintptr_t kernel_end;
	uintptr_t kernel_init_task_union;
	uintptr_t kernel_colinux_start;
	uintptr_t kernel_swapper_pg_dir;
	uintptr_t kernel_idt_table;
	uintptr_t kernel_gdt_table;
	uintptr_t kernel_co_arch_info;
	uintptr_t kernel_co_info;
} co_symbols_import_t;

#endif
