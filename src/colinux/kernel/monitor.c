/*
 * This source code is a part of coLinux source package.
 *
 * Dan Aloni <da-x@gmx.net>, 2003 (c)
 *
 * The code is licensed under the GPL. See the COPYING file at
 * the root directory.
 */

#include <asm/page.h>
#include <asm/pgtable.h>

#include <memory.h>
/*
 * The main monitor code.
 *
 * FIXME: It should be architecture independant, but I am not sure if it
 * currently achieves that goal. Maybe with a few fixes...
 */

#include <colinux/common/debug.h>
#include <colinux/os/kernel/alloc.h>
#include <colinux/os/kernel/misc.h>
#include <colinux/os/kernel/monitor.h>
#include <colinux/arch/passage.h>
#include <colinux/arch/interrupt.h>

#include "monitor.h"
#include "manager.h"
#include "console.h"
#include "network.h"
#include "block.h"
#include "devices.h"
#include "fileblock.h"

co_rc_t co_monitor_alloc_pages(co_monitor_t *cmon, unsigned long pages, void **address)
{
	void *ptr = co_os_alloc_pages(pages);
	
	KASSERT(pages != 0);

	if (ptr == NULL)
		return CO_RC(OUT_OF_MEMORY);

	*address = ptr;

	cmon->allocated_pages_num += pages;

	return CO_RC(OK);
}

co_rc_t co_monitor_free_pages(co_monitor_t *cmon, unsigned long pages, void *address)
{
	KASSERT(pages != 0);
	KASSERT(address != NULL);

	co_os_free_pages(address, pages);

	cmon->allocated_pages_num -= pages;

	return CO_RC(OK);
}

co_rc_t co_monitor_malloc(co_monitor_t *cmon, unsigned long bytes, void **ptr)
{
	void *block = co_os_malloc(bytes);

	if (block == NULL)
		return CO_RC(OUT_OF_MEMORY);

	*ptr = block;

	cmon->blocks_allocated++;

	return CO_RC(OK);
}

co_rc_t co_monitor_free(co_monitor_t *cmon, void *ptr)
{
	co_os_free(ptr);

	cmon->blocks_allocated--;

	return CO_RC(OK);
}

void co_monitor_free_kernel_image(co_monitor_t *cmon)
{
	if (cmon->core_image != NULL) {
		co_monitor_free_pages(cmon, cmon->core_pages, cmon->core_image);
		cmon->core_image = NULL;
	}
}

co_rc_t co_monitor_load_section(co_monitor_t *cmon, co_monitor_ioctl_load_section_t *params)
{
	unsigned long load_size;
	unsigned char *load_address;

	load_address = ((unsigned char *)cmon->core_image) + params->address - cmon->core_vaddr;
	load_size = params->size;

	if (params->user_ptr)
		memcpy(load_address, params->buf, load_size);
	else
		memset(load_address, 0, load_size);

	return CO_RC(OK);
}

co_rc_t co_monitor_colinux_context_alloc_maps(co_monitor_t *cmon)
{
	unsigned long index;
	co_rc_t rc;

	cmon->page_tables_size = cmon->physical_frames * sizeof(unsigned long *);
	cmon->page_tables_pages = (cmon->page_tables_size + PAGE_SHIFT-1) >> PAGE_SHIFT;
	cmon->pa_maps_size = cmon->manager->host_memory_pages * sizeof(unsigned long);
	cmon->pa_maps_pages = (cmon->pa_maps_size + PAGE_SHIFT-1) >> PAGE_SHIFT;

	rc = co_monitor_alloc_pages(cmon, cmon->page_tables_pages, (void **)&cmon->page_tables);
	if (!CO_OK(rc)) {
		co_debug("Error allocating page tables\n");
		goto out_after_free_page_tables;
	}

	rc = co_monitor_alloc_pages(cmon, cmon->page_tables_pages, (void **)&cmon->pages_allocated);
	if (!CO_OK(rc)) {
		co_debug("Error allocating page tables allocation counters\n");
		goto out_after_free_pages_allocated;
	}

	rc = co_monitor_alloc_pages(cmon, cmon->pa_maps_pages, (void **)&cmon->pa_to_host_va);
	if (!CO_OK(rc)) {
		co_debug("Error allocating pa_to_host_va map\n");
		goto out_after_free_pa_to_host_va;
	}

	rc = co_monitor_alloc_pages(cmon, cmon->pa_maps_pages, (void **)&cmon->pa_to_colx_va);
	if (!CO_OK(rc)) {
		co_debug("Error allocating pa_to_colx_va map\n");
		goto out_error;
	}

	for (index=0; index < cmon->physical_frames; index++) {
		cmon->page_tables[index] = __pte(0);
		cmon->pages_allocated[index] = 0;
	}

	for (index=0; index < cmon->manager->host_memory_pages; index++) {
		cmon->pa_to_host_va[index] = 0; 
		cmon->pa_to_colx_va[index] = 0; 
	}

	return rc;

out_error:
	co_monitor_free_pages(cmon, cmon->pa_maps_pages, (void *)cmon->pa_to_colx_va);

out_after_free_pa_to_host_va:
	co_monitor_free_pages(cmon, cmon->pa_maps_pages, (void *)cmon->pa_to_host_va);

out_after_free_pages_allocated:
	co_monitor_free_pages(cmon, cmon->page_tables_pages, (void *)cmon->pages_allocated);

out_after_free_page_tables:
	return rc;
}

void co_monitor_colinux_context_free_maps(co_monitor_t *cmon)
{
	co_monitor_free_pages(cmon, cmon->page_tables_pages, (void *)cmon->page_tables);
	co_monitor_free_pages(cmon, cmon->page_tables_pages, (void *)cmon->pages_allocated);
	co_monitor_free_pages(cmon, cmon->pa_maps_pages, (void *)cmon->pa_to_host_va);
	co_monitor_free_pages(cmon, cmon->pa_maps_pages, (void *)cmon->pa_to_colx_va);
}

void co_monitor_map_pptm_address(co_monitor_t *cmon, vm_ptr_t address, void *ptr)
{
	unsigned long pa = co_os_virt_to_phys(ptr);
	
	cmon->page_tables[CO_PFN(address)] = pte_modify(__pte(pa), __pgprot(__PAGE_KERNEL));
	cmon->pa_to_host_va[pa >> PAGE_SHIFT] = ptr;
	cmon->pa_to_colx_va[pa >> PAGE_SHIFT] = address;
}

bool_t co_monitor_unmap_pptm_address(co_monitor_t *cmon, vm_ptr_t address, void *ptr)
{
	unsigned long host, pa = co_os_virt_to_phys(ptr);

	host = (unsigned long)cmon->pa_to_host_va[pa >> PAGE_SHIFT];
	cmon->page_tables[CO_PFN(address)] = __pte(0);
	cmon->pa_to_host_va[pa >> PAGE_SHIFT] = 0;
	cmon->pa_to_colx_va[pa >> PAGE_SHIFT] = 0;

	return host != 0;
}
    
void co_monitor_colinux_context_init_pagetable_directory(co_monitor_t *cmon)
{
	unsigned long address;
	unsigned long address_end;
	unsigned long pmd_index;
	unsigned char *pptm_scan;
	
	cmon->pgd_page = (pmd_t *)CO_MONITOR_KERNEL_SYMBOL(swapper_pg_dir);
	cmon->pgd = __pgd(co_os_virt_to_phys(cmon->pgd_page));

	for (pmd_index=0; pmd_index < PTRS_PER_PGD; pmd_index++)
		cmon->pgd_page[pmd_index] = __pmd(0);

	address = __PAGE_OFFSET;
	pmd_index = address >> PMD_SHIFT;
	address_end = address + cmon->memory_size;

	pptm_scan = (unsigned char *)cmon->page_tables;
	while (address < address_end) {
		pmd_t pmd;

		pmd = __pmd(co_os_virt_to_phys(pptm_scan));
		pmd_val(pmd) |= _KERNPG_TABLE;
		cmon->pgd_page[pmd_index] = pmd;

		pptm_scan += PAGE_SIZE;	
		address += 1 << PMD_SHIFT;
		pmd_index += 1;
	}
}

co_rc_t co_monitor_colinux_context_alloc_and_map_bootmem(co_monitor_t *cmon)
{
	unsigned long address, index;
	unsigned char *bootmem_scan;
	co_rc_t rc;

	rc = co_monitor_alloc_pages(cmon, cmon->bootmem_pages, &cmon->bootmem);
	if (!CO_OK(rc))
		return rc;

	bootmem_scan = (char *)cmon->bootmem;
	address = cmon->core_end;
    
	for (index=0; index < cmon->bootmem_pages; index++) {
		co_monitor_map_pptm_address(cmon, address, bootmem_scan);

		bootmem_scan += PAGE_SIZE;
		address += PAGE_SIZE;
	}

	return CO_RC(OK);
}

void co_monitor_colinux_context_map_kernel_image(co_monitor_t *cmon)
{
	unsigned long address;
	char *core_image = (char *)cmon->core_image;
	unsigned long pfn;

	/* 
	 * This is no good. We go against the page table levels
	 * abstruction here.
	 *
	 * Definitely needs a rewrite.
	 */

	address = cmon->core_vaddr & ~((1 << PGDIR_SHIFT) - 1);

	for (pfn=0; pfn < PTRS_PER_PTE; pfn++) {
		if (address >= cmon->core_vaddr  &&  address < cmon->core_end) {
			co_monitor_map_pptm_address(cmon, address, core_image);
			core_image += PAGE_SIZE;
		}

		address += PAGE_SIZE;
	}
}

void co_monitor_colinux_context_map_passage_page(co_monitor_t *cmon)
{
	co_monitor_map_pptm_address(cmon, cmon->passage_page_vaddr, cmon->passage_page);
}

void co_monitor_colinux_context_map_maps(co_monitor_t *cmon)
{
	unsigned long pte_index;
	unsigned char *pptm_scan;

	pptm_scan = (unsigned char *)cmon->page_tables;

	for (pte_index=0; pte_index < cmon->page_tables_pages; pte_index++) {
		co_monitor_map_pptm_address(cmon, CO_PPTM_OFFSET + (pte_index << PAGE_SHIFT), 
				       pptm_scan);

		pptm_scan += PAGE_SIZE;
	}

	pptm_scan = (unsigned char *)cmon->pa_to_colx_va;

	for (pte_index=0; pte_index < cmon->pa_maps_pages; pte_index++) {
		co_monitor_map_pptm_address(cmon, CO_RPPTM_OFFSET + (pte_index << PAGE_SHIFT),
				       pptm_scan);
		pptm_scan += PAGE_SIZE;
	}
}

void co_monitor_colinux_context_unmap_maps(co_monitor_t *cmon)
{
	unsigned long pte_index;
	unsigned char *pptm_scan;

	pptm_scan = (unsigned char *)cmon->page_tables;

	for (pte_index=0; pte_index < cmon->page_tables_pages; pte_index++) {
		co_monitor_unmap_pptm_address(cmon, CO_PPTM_OFFSET + (pte_index << PAGE_SHIFT), 
					 pptm_scan);

		pptm_scan += PAGE_SIZE;
	}

	pptm_scan = (unsigned char *)cmon->pa_to_colx_va;

	for (pte_index=0; pte_index < cmon->pa_maps_pages; pte_index++) {
		co_monitor_unmap_pptm_address(cmon, CO_RPPTM_OFFSET + (pte_index << PAGE_SHIFT),
					 pptm_scan);
		pptm_scan += PAGE_SIZE;
	}
}

co_rc_t co_monitor_alloc_and_map(co_monitor_t *cmon, unsigned long colx_vaddr, long num)
{
	unsigned long pfn = CO_PFN(colx_vaddr);
	unsigned long index;
	unsigned char *host_vaddr;
	co_rc_t rc;

	if (cmon->pages_allocated[pfn] == num)
		return CO_RC(OK);
		
	for (index=pfn; index < pfn+num; index++)
		if (!(pte_val(cmon->page_tables[index]) >> PAGE_SHIFT))
			break;

	if (index == pfn+num) {
		/* co_debug("already allocated %x/%d\n"); */
		return CO_RC(OK);
	}
		
	if (index != pfn) {
		co_debug("Insane allocation\n");
		return CO_RC(ERROR);
	}

	rc = co_monitor_alloc_pages(cmon, num, (void **)&host_vaddr);
	if (!CO_OK(rc)) {
		co_debug("No more host OS memory!\n");
		return CO_RC(ERROR);
	}

	cmon->pages_allocated[pfn] = num;

	for (index=0; index < num; index++) {
		unsigned long pa = co_os_virt_to_phys(host_vaddr);
	
		cmon->page_tables[pfn] = pte_modify(__pte(pa), __pgprot(__PAGE_KERNEL));
		cmon->pa_to_host_va[pa >> PAGE_SHIFT] = host_vaddr;
		cmon->pa_to_colx_va[pa >> PAGE_SHIFT] = colx_vaddr;

		pfn++;
		colx_vaddr += PAGE_SIZE;
		host_vaddr += PAGE_SIZE;
	}

	return CO_RC(OK);
}

co_rc_t co_monitor_unmap_and_free(co_monitor_t *cmon, unsigned long colx_vaddr)
{
	unsigned long pfn = CO_PFN(colx_vaddr), pa_pfn;
	unsigned long num;
	unsigned char *host_vaddr;
	unsigned long index;

	num = cmon->pages_allocated[pfn];
	if (!num)
		return CO_RC(OK);

	pa_pfn = pte_val(cmon->page_tables[pfn]) >> PAGE_SHIFT;
	if (!pa_pfn)
		return CO_RC(OK);

	cmon->pages_allocated[pfn] = 0;

	host_vaddr = cmon->pa_to_host_va[pa_pfn];
	for (index=0; index < num; index++) {
		pa_pfn = pte_val(cmon->page_tables[pfn]) >> PAGE_SHIFT;
		cmon->pa_to_host_va[pa_pfn] = 0;
		cmon->pa_to_colx_va[pa_pfn] = 0;
		cmon->page_tables[pfn] = __pte(0);
		pfn++;
	}	

	co_monitor_free_pages(cmon, num, host_vaddr);
		
	return CO_RC(OK);
}

void co_monitor_colinux_context_free_mapped_pages(co_monitor_t *cmon)
{
	unsigned long pte_index;
	vm_ptr_t vaddr = __PAGE_OFFSET;

	for (pte_index=0; pte_index < cmon->physical_frames; pte_index++) {
		unsigned long num = cmon->pages_allocated[pte_index];
		if (num)
			co_monitor_unmap_and_free(cmon, vaddr);

		vaddr += PAGE_SIZE;
	}
}

co_rc_t co_monitor_colinux_context_alloc_and_map_all(co_monitor_t *cmon)
{
	unsigned long pte_index;
	vm_ptr_t vaddr = __PAGE_OFFSET;
	co_rc_t rc = CO_RC_OK;

	for (pte_index=0; pte_index < cmon->physical_frames; pte_index++) {
		if (!(pte_val(cmon->page_tables[pte_index]) >> PAGE_SHIFT)) {
			rc = co_monitor_alloc_and_map(cmon, vaddr, 1);
			if (!CO_OK(rc)) {
				co_debug("Error allocating PP RAM (%d) at %d out of %d\n",
					 rc, pte_index, cmon->physical_frames);
				break;
			}
		}

		vaddr += PAGE_SIZE;
	}

	if (!CO_OK(rc))
		co_monitor_colinux_context_free_mapped_pages(cmon);

	return rc;
}

co_rc_t co_monitor_colinux_context_init(co_monitor_t *cmon)
{
	co_rc_t rc;

	rc = co_monitor_arch_passage_page_alloc(cmon);
	if (!CO_OK(rc)) {
		co_debug("Error allocating passage page (%d)\n", rc);
		goto out_after_free_passage_page;
	}

	rc = co_monitor_colinux_context_alloc_maps(cmon);
	if (!CO_OK(rc)) {
		co_debug("Error allocating maps (%d)\n", rc);
		goto out_after_free_maps;
	}

	co_monitor_colinux_context_init_pagetable_directory(cmon);
	co_monitor_colinux_context_map_kernel_image(cmon);

	rc = co_monitor_colinux_context_alloc_and_map_bootmem(cmon);
	if (!CO_OK(rc)) {
		co_debug("Error allocating and mapping bootmem (%d)\n", rc);
		goto out_after_free_bootmem;
	}

	co_monitor_colinux_context_map_maps(cmon);
	co_monitor_colinux_context_map_passage_page(cmon);

	rc = co_monitor_arch_passage_page_init(cmon);
	if (!CO_OK(rc)) {
		co_debug("Error initializing passage page (%d)\n", rc);
		goto out_error;
	}

	rc = co_monitor_colinux_context_alloc_and_map_all(cmon);
	if (!CO_OK(rc)) {
		co_debug("Error initializing passage page (%d)\n", rc);
		goto out_error;
	}

	return rc;

	/* Error path */
out_error:
	co_monitor_free_pages(cmon, cmon->bootmem_pages, cmon->bootmem);

out_after_free_bootmem:
	co_monitor_colinux_context_free_maps(cmon);

out_after_free_maps:
	co_monitor_arch_passage_page_free(cmon);

out_after_free_passage_page:		
	return rc;
}

void co_monitor_colinux_context_free(co_monitor_t *cmon)
{
	co_monitor_colinux_context_free_mapped_pages(cmon);
	co_monitor_colinux_context_unmap_maps(cmon);
	co_monitor_colinux_context_free_maps(cmon);
	co_monitor_arch_passage_page_free(cmon);
	co_monitor_free_pages(cmon, cmon->bootmem_pages, cmon->bootmem);

	co_monitor_free_kernel_image(cmon);
}

bool_t co_monitor_device_request(co_monitor_t *cmon, co_device_t device, unsigned long *params)
{
	switch (device) {
	case CO_DEVICE_BLOCK: {
		co_block_request_t *request;
		unsigned long dev_index = params[0];
		co_rc_t rc;

		request = (co_block_request_t *)(&params[1]);

		rc = co_monitor_block_request(cmon, dev_index, request);

		if (CO_OK(rc))
			request->rc = 0;
		else
			request->rc = -1;
		
		return PTRUE;
	}

	case CO_DEVICE_CONSOLE: {
		co_monitor_console_send_near(cmon, params);
		break;
	}

	case CO_DEVICE_NETWORK: {
		co_monitor_device_t *mon_device;

		mon_device = co_monitor_get_device(cmon, device);

		co_monitor_network_send_near(cmon, mon_device, params);

		co_monitor_put_device(cmon, mon_device);

		break;
	}
	default:
		break;
	}	

	return PTRUE;
}

bool_t co_monitor_iteration(co_monitor_t *cmon)
{
	co_switch();

	switch (co_passage_page->operation) {
	case CO_OPERATION_FORWARD_INTERRUPT: {
		co_monitor_arch_real_hardware_interrupt(cmon);
		co_monitor_check_devices(cmon);
		return PTRUE;
	}
	case CO_OPERATION_IDLE: {
		co_monitor_os_idle(cmon);
		co_monitor_check_devices(cmon);
		return PTRUE;
	}
	case CO_OPERATION_MORE_DATA: {
		co_monitor_check_devices(cmon);
		return PTRUE;
	}
	case CO_OPERATION_TERMINATE:
		co_debug("Linux terminated (%d)\n", co_passage_page->params[0]);
		return PFALSE;

	case CO_OPERATION_DEBUG_LINE: {
		char *p = (char *)&co_passage_page->params[0];
		p[0x200] = '\0';
		co_debug_line(p);

		break;
	}
#if (0)
        /*
	 * Once upon a time, all of coLinux's pseudo physical memory wasn't
	 * all allocated from the start. So instead, I modified __alloc_pages()
	 * and __free_pages() to call these interfaces below accordingly. 
	 *
	 * We can use this method sometime again in the future because it can 
	 * potentially save some run-time memory, but on the long run, the 
	 * entire coLinux's RAM is going to be used anyway for cache, so we 
	 * aren't saving much memory with this method anyway.
	 */ 
	case CO_OPERATION_ALLOC_PAGE: {
		unsigned long colx_vaddr = co_passage_page->params[0];
		unsigned long num = 1 << co_passage_page->params[1];

		return co_monitor_alloc_and_map(cmon, colx_vaddr, num);
	}
	case CO_OPERATION_FREE_PAGE: {
		unsigned long colx_vaddr = co_passage_page->params[0];

		return co_monitor_unmap_and_free(cmon, colx_vaddr);
	}
#endif
	case CO_OPERATION_DEVICE: {
		unsigned long device = co_passage_page->params[0];

		return co_monitor_device_request(cmon, device, &co_passage_page->params[1]);
	}
	default:
		co_debug("operation %d not handled\n", co_passage_page->operation);
		return PFALSE;
	}
	
	return PTRUE;
}

void co_monitor_free_file_blockdevice(co_monitor_t *cmon, co_block_dev_t *dev)
{
	co_monitor_file_block_dev_t *fdev = (co_monitor_file_block_dev_t *)dev;

	co_monitor_file_block_shutdown(fdev);
	co_monitor_free(cmon, dev);
}

co_rc_t co_monitor_load_configuration(co_monitor_t *cmon)
{
	co_rc_t rc = CO_RC_OK; 
	long i;

	for (i=0; i < CO_MAX_BLOCK_DEVICES; i++) {
		co_monitor_file_block_dev_t *dev;
		co_block_dev_desc_t *conf_dev = &cmon->config.block_devs[i];
		if (!conf_dev->enabled)
			continue;

		rc = co_monitor_malloc(cmon, sizeof(co_monitor_file_block_dev_t), (void **)&dev);
		if (!CO_OK(rc))
			goto out;

		rc = co_monitor_file_block_init(dev, &conf_dev->pathname);
		if (CO_OK(rc)) {
			co_monitor_block_register_device(cmon, i, (co_block_dev_t *)dev);
			dev->dev.free = co_monitor_free_file_blockdevice;
		} else {
			co_monitor_free(cmon, dev);
			co_debug("Error opening block device\n");
			goto out;
		}
	}

out:
	if (!CO_OK(rc))
		co_monitor_unregister_and_free_block_devices(cmon);

	return rc;
}


void co_monitor_timer_cb(void *data)
{
	co_monitor_t *cmon = (co_monitor_t *)data;

	co_monitor_signal_device(cmon, CO_DEVICE_TIMER);
}

co_rc_t co_monitor_init(co_monitor_t *cmon, co_manager_ioctl_create_t *params)
{
	co_rc_t rc = CO_RC_OK;
	co_symbols_import_t *import = &params->import;
 
	if (cmon->state != CO_MONITOR_STATE_START) {
		co_debug("Tried to init cmon during an invalid state (%d)\n",
			 cmon->state);
		return CO_RC(ERROR);
	}

	cmon->core_vaddr = import->kernel_start;
	cmon->core_pages = (import->kernel_end - import->kernel_start + 
			    PAGE_SIZE - 1) >> PAGE_SHIFT;

	rc = co_monitor_alloc_pages(cmon, cmon->core_pages, &cmon->core_image);
	if (!CO_OK(rc))
		goto out_error;
	
	cmon->core_end = cmon->core_vaddr + (cmon->core_pages << PAGE_SHIFT);
	cmon->import = params->import;
	cmon->config = params->config;

	/*
	 * FIXME: Currently the number of bootmem pages is hardcored, but we can easily
	 * modify this to depend on the amount of pseudo physical RAM, which
	 * is approximately sizeof(struct page)*(pseudo physical pages) plus
	 * more smaller structures. 
	 */
	cmon->bootmem_pages = 0x140;

	/*
	 * FIXME: Be careful with that. We don't want to exhaust the host OS's memory
	 * pools because it can destablized it. 
	 *
	 * We need to find the minimum requirement for size of the coLinux RAM 
	 * for achieving the best performance assuming that the host OS is 
	 * caching away the coLinux swap device into the host system's other
	 * RAM.
	 */
	if (cmon->manager->host_memory_amount >= 128*1024*1024) {
		cmon->memory_size = 32*1024*1024; /* 32MB */
	} else {
		cmon->memory_size = 16*1024*1024; /* 16MB */
	}

	cmon->physical_frames = cmon->memory_size >> PAGE_SHIFT;
	cmon->end_physical = __PAGE_OFFSET + cmon->memory_size;
	cmon->passage_page_vaddr = cmon->core_end + (cmon->bootmem_pages << PAGE_SHIFT);

	rc = co_os_timer_create(&co_monitor_timer_cb, cmon, 10, &cmon->timer);
	if (!CO_OK(rc)) {
		co_debug("Error creating host OS timer (%d)\n", rc);
		goto out_free_image;
	}

	rc = co_monitor_load_configuration(cmon);
	if (!CO_OK(rc)) {
		co_debug("Error loading monitor configuration (%d)\n", rc);
		goto out_destroy_timer;
	}

	rc = co_monitor_setup_devices(cmon);
	if (!CO_OK(rc)) {
		goto out_unregister_block_devs;
	}

	cmon->state = CO_MONITOR_STATE_INITIALIZED;

	return rc;

/* error path */
out_unregister_block_devs:
	co_monitor_unregister_and_free_block_devices(cmon);

out_destroy_timer:
	co_os_timer_destroy(cmon->timer);

out_free_image:
	co_monitor_free_kernel_image(cmon);

out_error:
	while (1) {};
	return rc;
}

co_rc_t co_monitor_unload(co_monitor_t *cmon)
{
	co_rc_t rc = CO_RC_OK;

	co_debug("cmon before free: %d kb, %d blocks\n", 
		    (cmon->allocated_pages_num << PAGE_SHIFT) >> 10,
		    cmon->blocks_allocated);

	co_monitor_colinux_context_free(cmon);
	co_monitor_unregister_and_free_block_devices(cmon);
	co_monitor_cleanup_devices(cmon);
	co_os_timer_destroy(cmon->timer);

	co_debug("cmon after free: %d kb, %d blocks\n", 
		    (cmon->allocated_pages_num << PAGE_SHIFT) >> 10,
		    cmon->blocks_allocated);
	
	return rc;
}

co_rc_t co_monitor_run(co_monitor_t *cmon)
{
	co_rc_t rc;

	rc = co_monitor_colinux_context_init(cmon);
	if (!CO_OK(rc)) {
		co_debug("Error initializing coLinux context (%d)\n", rc);
		return rc;
	}

	cmon->state = CO_MONITOR_STATE_RUNNING;	

	/* Be ready to receive timer devices for the coLinux machine */
	co_os_timer_activate(cmon->timer);

	co_passage_page->operation = CO_OPERATION_START;
	co_passage_page->params[0] = cmon->core_end;
	co_passage_page->params[1] = cmon->bootmem_pages;
	co_passage_page->params[2] = cmon->memory_size;
	co_passage_page->params[3] = cmon->pa_maps_size;

	while (co_monitor_iteration(cmon));

	co_os_timer_deactivate(cmon->timer);

	cmon->state = CO_MONITOR_STATE_AWAITING_TERMINATION;

	if (cmon->console_state == CO_MONITOR_CONSOLE_STATE_ATTACHED)
		co_monitor_os_wait_console_detach(cmon);

	cmon->state = CO_MONITOR_STATE_UNLOADING;	

	co_monitor_unload(cmon);

	cmon->state = CO_MONITOR_STATE_DOWN;

	return CO_RC(OK);
}

co_rc_t co_monitor_terminate(co_monitor_t *cmon)
{
	co_debug("Terminating coLinux\n");
	
	return co_monitor_signal_device(cmon, CO_DEVICE_POWER);
}

