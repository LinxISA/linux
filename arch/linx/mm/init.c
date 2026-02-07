// SPDX-License-Identifier: GPL-2.0-only

#include <linux/init.h>
#include <linux/memblock.h>
#include <linux/mm.h>

#include <asm/pgtable.h>

/*
 * NOMMU bring-up: use memblock's physical memory description to seed the buddy
 * allocator and slab.
 */
void __init paging_init(void)
{
	unsigned long max_zone_pfn[MAX_NR_ZONES] = { 0 };
	phys_addr_t mem_start;
	phys_addr_t mem_end;

	mem_start = memblock_start_of_DRAM();
	mem_end = memblock_end_of_DRAM();

	min_low_pfn = PHYS_PFN(mem_start);
	max_low_pfn = PHYS_PFN(mem_end);
	max_pfn = max_low_pfn;

	/*
	 * LinxISA bring-up uses a single flat physical memory range under QEMU
	 * `virt`. Keep everything in ZONE_NORMAL.
	 */
	max_zone_pfn[ZONE_NORMAL] = max_low_pfn;

	free_area_init(max_zone_pfn);
}
