/*
 * This source code is a part of coLinux source package.
 *
 * Dan Aloni <da-x@colinux.org>, 2004 (c)
 *
 * The code is licensed under the GPL. See the COPYING file at
 * the root directory.
 *
 */

#include "linux_inc.h"

#include <colinux/os/kernel/alloc.h>

/*
 * Interfaces for physical memory allocation.
 */
co_rc_t co_os_get_page(struct co_manager *manager, co_pfn_t *pfn)
{
        struct page *page;
	void *ret;

        page = alloc_pages(GFP_KERNEL | __GFP_REPEAT, 0);
        if (!page)
                return CO_RC(ERROR);
	
	*pfn = page_to_pfn(page);

	/* zero the page */

	ret = kmap(page);
	if (ret) {
		memset(ret, 0, PAGE_SIZE);
		kunmap(ret);
	}
	
	return CO_RC(OK);
}

void *co_os_map(struct co_manager *manager, co_pfn_t pfn)
{
	return kmap(pfn_to_page(pfn));
}

void co_os_unmap(struct co_manager *manager, void *ptr, co_pfn_t pfn)
{
	kunmap(pfn_to_page(pfn));
}

void co_os_put_page(struct co_manager *manager, co_pfn_t pfn)
{
	__free_page(pfn_to_page(pfn));
}

void *co_os_alloc_pages(unsigned long pages)
{
	return (void *)__get_free_pages(GFP_KERNEL, get_order(pages << PAGE_SHIFT));
}

void co_os_free_pages(void *ptr, unsigned long pages)
{
	free_pages((unsigned long)ptr, get_order(pages << PAGE_SHIFT));
}
