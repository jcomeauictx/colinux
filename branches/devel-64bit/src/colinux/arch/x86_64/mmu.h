/*
 * This source code is a part of coLinux source package.
 *
 * Dan Aloni <da-x@colinux.org>, 2004 (c)
 *
 * Based on definitions from Linux.
 *
 * The code is licensed under the GPL. See the COPYING file at
 * the root directory.
 */

#ifndef __COLINUX_ARCH_I386_MMU_H__
#define __COLINUX_ARCH_I386_MMU_H__

typedef unsigned long co_pfn_t;

#define PMD_MASK        (~(PMD_SIZE-1))
#define _KERNPG_TABLE   (_PAGE_PRESENT | _PAGE_RW | _PAGE_ACCESSED | _PAGE_DIRTY)

#define _PAGE_PRESENT   0x001
#define _PAGE_RW        0x002
#define _PAGE_USER      0x004
#define _PAGE_PWT       0x008
#define _PAGE_PCD       0x010
#define _PAGE_ACCESSED  0x020
#define _PAGE_DIRTY     0x040
#define _PAGE_PSE       0x080   /* 4 MB (or 2MB) page, Pentium+, if present.. */
#define CO_ARCH_PAGE_NX        0x8000000000000000ULL

#define PTRS_PER_PTE    1024
#define PGDIR_SHIFT     22
#define PTRS_PER_PGD    1024

#define CO_ARCH_PMD_SHIFT       22
#define CO_ARCH_PMD_MASK        (~(CO_ARCH_PMD_SIZE-1))
#define CO_ARCH_PMD_SIZE        (1UL << CO_ARCH_PMD_SHIFT)

#define CO_ARCH_PAE_PGD_SHIFT   30
#define CO_ARCH_PAE_PGD_MASK        (~(CO_ARCH_PAE_PGD_SIZE-1))
#define CO_ARCH_PAE_PGD_SIZE        (1UL << CO_ARCH_PAE_PGD_SHIFT)

#define CO_ARCH_PAE_PMD_SHIFT   21
#define CO_ARCH_PAE_PMD_MASK        (~(CO_ARCH_PAE_PMD_SIZE-1))
#define CO_ARCH_PAE_PMD_SIZE        (1UL << CO_ARCH_PAE_PMD_SHIFT)

#define CO_ARCH_PAGE_SHIFT      12
#define CO_ARCH_PAGE_SIZE       (1 << CO_ARCH_PAGE_SHIFT)
#define CO_ARCH_PAGE_MASK       (~(CO_ARCH_PAGE_SIZE-1))
#define CO_ARCH_KERNEL_OFFSET   0xc0000000

#endif
