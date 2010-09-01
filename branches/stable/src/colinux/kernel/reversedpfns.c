/*
 * This source code is a part of coLinux source package.
 *
 * Dan Aloni <da-x@colinux.org>, 2003-2004 (c)
 *
 * The code is licensed under the GPL. See the COPYING file at
 * the root directory.
 */

#include <colinux/common/libc.h>
#include <colinux/os/kernel/alloc.h>
#include <colinux/os/kernel/monitor.h>
#include <colinux/os/kernel/manager.h>
#include <colinux/os/kernel/misc.h>
#include <colinux/os/kernel/mutex.h>
#include <colinux/arch/mmu.h>

#include "manager.h"
#include "monitor.h"
#include "pages.h"

co_rc_t co_manager_alloc_reversed_pfns(co_manager_t *manager)
{
	unsigned long map_size, covered_physical;
	unsigned long reversed_page_count;
	co_rc_t rc = CO_RC(OK);
	int i, j;

	co_debug("allocating reversed physical mapping");
	manager->reversed_page_count = manager->hostmem_pages / PTRS_PER_PTE;
	map_size = sizeof(co_pfn_t) * manager->reversed_page_count;

	co_debug("allocating top level pages map (%ld bytes)", map_size);
	manager->reversed_map_pfns = co_os_malloc(map_size);
	if (manager->reversed_map_pfns == NULL)
		return CO_RC(OUT_OF_MEMORY);

	co_memset(manager->reversed_map_pfns, 0, map_size);

	co_debug("allocating %ld top level pages", manager->reversed_page_count);
	for (i=0; i < manager->reversed_page_count; i++) {
		co_pfn_t pfn;

		rc = co_manager_get_page(manager, &pfn);
		if (!CO_OK(rc))
			return rc;

		manager->reversed_map_pfns[i] = pfn;
	}

	manager->reversed_map_pgds_count =
		(((unsigned long)(CO_VPTR_BASE - CO_VPTR_PHYSICAL_TO_PSEUDO_PFN_MAP)
		  / PTRS_PER_PTE)  / PTRS_PER_PGD) / sizeof(linux_pgd_t);

	co_debug("using %ld table entries for reversed physical mapping", manager->reversed_map_pgds_count);
	manager->reversed_map_pgds = co_os_malloc(manager->reversed_map_pgds_count*sizeof(linux_pgd_t));
	if (!manager->reversed_map_pgds) {
		if (!CO_OK(rc)) /* TODO: handle error */
			return rc;
	}

	covered_physical = 0;
	reversed_page_count = 0;
	for (i=0; i < manager->reversed_map_pgds_count; i++) {
		co_pfn_t pfn;
		linux_pte_t *pte;

		if (covered_physical >= manager->hostmem_pages) {
			manager->reversed_map_pgds[i] = 0;
			continue;
		}

		rc = co_manager_get_page(manager, &pfn);
		if (!CO_OK(rc))
			return rc;

		pte = co_os_map(manager, pfn);
		for (j=0; j < PTRS_PER_PTE; j++) {
			co_pfn_t rpfn;
			if (reversed_page_count >= manager->reversed_page_count) {
				pte[j] = 0;
				continue;
			}

			rpfn = manager->reversed_map_pfns[reversed_page_count];
			pte[j] = (rpfn << CO_ARCH_PAGE_SHIFT) | (_PAGE_PRESENT | _PAGE_RW | _PAGE_DIRTY | _PAGE_ACCESSED);
			reversed_page_count++;
		}
		co_os_unmap(manager, pte, pfn);

		manager->reversed_map_pgds[i] = (pfn << CO_ARCH_PAGE_SHIFT) | _KERNPG_TABLE;

		covered_physical += (PTRS_PER_PTE * PTRS_PER_PGD);
	}

	return CO_RC(OK);
}

void co_manager_free_reversed_pfns(co_manager_t *manager)
{
	int i;

	if (manager->reversed_map_pfns) {
		for (i=0; i < manager->reversed_page_count; i++)
			co_os_put_page(manager, manager->reversed_map_pfns[i]);

		co_os_free(manager->reversed_map_pfns);
	}

	if (manager->reversed_map_pgds) {
		for (i=0; i < manager->reversed_map_pgds_count; i++) {
			if (manager->reversed_map_pgds[i] != 0) {
				co_os_put_page(manager, manager->reversed_map_pgds[i] >> CO_ARCH_PAGE_SHIFT);
			}
		}

		co_os_free(manager->reversed_map_pgds);
	}
}

co_rc_t co_manager_set_reversed_pfn(co_manager_t *manager, co_pfn_t real_pfn, co_pfn_t pseudo_pfn)
{
	int entry, top_level;
	co_pfn_t *reversed_pfns;
	co_pfn_t mapped_pfn;

	top_level = real_pfn / PTRS_PER_PTE;
	if (top_level >= manager->reversed_page_count)
		return CO_RC(ERROR);

	entry = real_pfn % PTRS_PER_PTE;

	mapped_pfn = manager->reversed_map_pfns[top_level];
	reversed_pfns = co_os_map(manager, mapped_pfn);
	reversed_pfns[entry] = pseudo_pfn;
	co_os_unmap(manager, reversed_pfns, mapped_pfn);

	return CO_RC(OK);
}
