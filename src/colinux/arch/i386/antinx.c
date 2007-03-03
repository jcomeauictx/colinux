/*
 * This source code is a part of coLinux source package.
 *
 * Dan Aloni <da-x@colinux.org>, 2004 (c)
 *
 * The code is licensed under the GPL. See the COPYING file at
 * the root directory.
 */ 

#include <colinux/common/common.h>
#include <colinux/common/debug.h>
#include <colinux/kernel/monitor.h>
#include <colinux/os/kernel/alloc.h>

#include "manager.h"
#include "antinx.h"
#include "defs.h"
#include "utils.h"


/* Workarrount for Intel P4 and Xeon, that's crashing with DEP/noexecute. */
/* http://download.intel.com/design/Pentium4/specupdt/249199.htm: */
/* N76  Changes to CR3 Register do not Fence Pending Instruction Page Walks. */
/* http://download.intel.com/design/Pentium4/manuals/253668.htm: */
/* 3.12 TRANSLATION LOOKASIDE BUFFERS (TLBS) */
/* Todo: */
/* Should be optimised and call only on specifics Xeon and P4 models. */
/* The INVLPG instruction should better work, but don't know paramters. */
/* Why CO_ARCH_PAGE_NX is not set ones on init of passage page? */
#define flush_tlb()							\
	do {								\
		unsigned int tmpreg;					\
									\
		__asm__ __volatile__(					\
			"movl %%cr3, %0;              \n"		\
			"movl %0, %%cr3;  # flush TLB \n"		\
			: "=r" (tmpreg)					\
			:: "memory");					\
	} while (0)

static void nx_protect_switch_wrapper(co_monitor_t *cmon)
{
	co_archdep_monitor_t archdep = cmon->archdep;
	unsigned long long pts_save;

	asm("cli");
	
	pts_save = (*archdep->fixed_pte) & CO_ARCH_PAGE_NX;
	*archdep->fixed_pte &= ~CO_ARCH_PAGE_NX;
	flush_tlb();
		
	co_switch();

	*archdep->fixed_pte |= pts_save;
	flush_tlb();
	
	/* interrupts are re-enabled by the caller */
}

co_rc_t co_arch_anti_nx_init(co_monitor_t *mon)
{
	co_archdep_monitor_t archdep;
	co_archdep_manager_t marchdep;
	co_rc_t rc;
	bool_t pae_enabled;
	co_pfn_t pfn, pfn_next;
	unsigned long cr3;
	unsigned char *page;
	unsigned long long *ptes;
	unsigned long vaddr;

	archdep = mon->archdep;
	marchdep = mon->manager->archdep;
	vaddr = (unsigned long)mon->passage_page;

	co_debug_lvl(misc, 11, "vaddr = %08x\n", vaddr);

	if (!(marchdep->caps[1] & (1 << CO_ARCH_AMD_FEATURE_NX))) {
		co_debug("AMD's NX is not enabled\n");
		return CO_RC(OK);
	}

	pae_enabled = co_is_pae_enabled();
	if (!pae_enabled) {
		co_debug("PAE is not enabled\n");
		return CO_RC(OK);
	}

	cr3 = co_get_cr3();
	co_debug_lvl(misc, 11, "cr3 = %08x\n", cr3);
	pfn = cr3 >> CO_ARCH_PAGE_SHIFT;
	co_debug("pfn = %08x\n", pfn);
	page = (unsigned char *)co_os_map(mon->manager, pfn);
	if (!page) {
		rc = CO_RC(ERROR);
		goto out;
	}

	co_debug_lvl(misc, 11, "page = %08x\n", page);
	ptes = ((unsigned long long *)&(page[((cr3 & (~CO_ARCH_PAGE_MASK)) & ~0x1f)]));
	co_debug_lvl(misc, 11, "ptes = %08x\n", ptes);
	pfn_next = ptes[vaddr >> CO_ARCH_PAE_PGD_SHIFT] >> CO_ARCH_PAGE_SHIFT;
	co_debug_lvl(misc, 11, "pfn_next = %08x\n", pfn_next);
	co_os_unmap(mon->manager, page, pfn);
	pfn = pfn_next;

	page = (unsigned char *)co_os_map(mon->manager, pfn);
	if (!page) {
		rc = CO_RC(ERROR);
		goto out;
	}

	co_debug_lvl(misc, 11, "page = %08x\n", page);
	ptes = (unsigned long long *)page;
	ptes = &ptes[(vaddr & ~CO_ARCH_PAE_PGD_MASK) >> CO_ARCH_PAE_PMD_SHIFT];
	co_debug_lvl(misc, 11, "ptes = %08x\n", ptes);

	if (!(*ptes & _PAGE_PSE)) {
		pfn_next = (*ptes) >> CO_ARCH_PAGE_SHIFT;
		co_debug_lvl(misc, 11, "pfn_next = %08x\n", pfn_next);
		co_os_unmap(mon->manager, page, pfn);
		pfn = pfn_next;
		
		page = (unsigned char *)co_os_map(mon->manager, pfn);
		if (!page) {
			rc = CO_RC(ERROR);
			goto out;
		}
		
		co_debug_lvl(misc, 11, "page = %08x\n", page);
		ptes = (unsigned long long *)page;
		ptes = &ptes[(vaddr & ~CO_ARCH_PAE_PMD_MASK) >> CO_ARCH_PAGE_SHIFT];
		co_debug_lvl(misc, 11, "ptes = %08x\n", ptes);
	}

	archdep->fixed_pte = ptes;
	archdep->fixed_page_table_pfn = pfn;
	archdep->fixed_page_table_mapping = page;
	archdep->antinx_enabled = PTRUE;

	co_host_switch_wrapper_func = nx_protect_switch_wrapper;

	co_debug("nx_protect_switch_wrapper: flush_tlb is active\n");

	return CO_RC(OK);

out:
	return CO_RC(ERROR);
}

void co_arch_anti_nx_free(co_monitor_t *mon)
{
	co_archdep_monitor_t archdep = mon->archdep;

	if (!archdep->antinx_enabled)
		return;

	co_os_unmap(mon->manager, 
		    archdep->fixed_page_table_mapping,
		    archdep->fixed_page_table_pfn);

	archdep->antinx_enabled = PFALSE;	
}
