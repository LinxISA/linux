// SPDX-License-Identifier: GPL-2.0-only

#include <linux/init.h>
#include <linux/memblock.h>
#include <linux/mm.h>
#include <linux/linkage.h>
#include <linux/sizes.h>

#include <asm/pgtable.h>
#include <asm/ssr.h>

/*
 * NOMMU bring-up: use memblock's physical memory description to seed the buddy
 * allocator and slab.
 */

#ifdef CONFIG_MMU

#define LINX_VIRT_UART_BASE 0x10000000UL

static const pgprot_t protection_map[16] = {
	[VM_NONE]					= PAGE_NONE,
	[VM_READ]					= PAGE_READ,
	[VM_WRITE]					= PAGE_COPY,
	[VM_WRITE | VM_READ]				= PAGE_COPY,
	[VM_EXEC]					= PAGE_EXEC,
	[VM_EXEC | VM_READ]				= PAGE_READ_EXEC,
	[VM_EXEC | VM_WRITE]				= PAGE_COPY_EXEC,
	[VM_EXEC | VM_WRITE | VM_READ]			= PAGE_COPY_EXEC,
	[VM_SHARED]					= PAGE_NONE,
	[VM_SHARED | VM_READ]				= PAGE_READ,
	[VM_SHARED | VM_WRITE]				= PAGE_SHARED,
	[VM_SHARED | VM_WRITE | VM_READ]		= PAGE_SHARED,
	[VM_SHARED | VM_EXEC]				= PAGE_EXEC,
	[VM_SHARED | VM_EXEC | VM_READ]			= PAGE_READ_EXEC,
	[VM_SHARED | VM_EXEC | VM_WRITE]		= PAGE_SHARED_EXEC,
	[VM_SHARED | VM_EXEC | VM_WRITE | VM_READ]	= PAGE_SHARED_EXEC
};
DECLARE_VM_GET_PAGE_PROT

unsigned long empty_zero_page[PAGE_SIZE / sizeof(unsigned long)] __page_aligned_bss;

static void * __init linx_alloc_pgtable_page(void)
{
	phys_addr_t pa = memblock_phys_alloc(PAGE_SIZE, PAGE_SIZE);
	void *va = __va(pa);

	memset(va, 0, PAGE_SIZE);
	return va;
}

static inline pgprot_t __init linx_pgprot_device_rw(void)
{
	return __pgprot(LINX_PTE_AF | LINX_PTE_R | LINX_PTE_W |
			((unsigned long)LINX_MAIR_ATTR_DEVICE <<
			 LINX_PTE_ATTRIDX_SHIFT));
}

static void __init linx_mmu_map_direct(phys_addr_t mem_start, phys_addr_t mem_end)
{
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	phys_addr_t addr;
	const phys_addr_t block_size = (phys_addr_t)1ull << PMD_SHIFT;

	/*
	 * v0.2 bring-up uses a very small physical memory map on QEMU `virt`.
	 * Use 4 KiB pages for the initial direct map to avoid depending on
	 * PMD-level block descriptors while the MMU model is still stabilizing.
	 */
	if (mem_end > (phys_addr_t)(1ull << PUD_SHIFT))
		mem_end = (phys_addr_t)(1ull << PUD_SHIFT);

	pud = (pud_t *)linx_alloc_pgtable_page();
	pmd = (pmd_t *)linx_alloc_pgtable_page();

	/* PGD[0] -> PUD, PUD[0] -> PMD */
	swapper_pg_dir[0] = linx_mk_pgd_table(pud);
	set_pud(&pud[0], linx_mk_pud_table(pmd));

	/* Map [0, mem_end) as normal memory (4 KiB PTEs). */
	for (addr = 0; addr < ALIGN(mem_end, block_size); addr += block_size) {
		const unsigned long idx = (unsigned long)((addr >> PMD_SHIFT) & (PTRS_PER_PMD - 1));
		unsigned int i;

		pte = (pte_t *)linx_alloc_pgtable_page();
		for (i = 0; i < PTRS_PER_PTE; i++) {
			const phys_addr_t pa = addr + (phys_addr_t)i * PAGE_SIZE;

			if (pa >= mem_end)
				break;
			pte[i] = pfn_pte(PHYS_PFN(pa), PAGE_KERNEL);
		}

		pmd[idx] = __pmd((unsigned long)__pa(pte) | LINX_PTE_TYPE_TABLE);
	}

	/*
	 * Keep the QEMU virt UART page strongly ordered after MMU enable.
	 * The linear mapping above marks everything as normal WB memory, which
	 * can make post-MMU UART output disappear or become unstable.
	 */
	{
		const phys_addr_t uart_pa = (phys_addr_t)LINX_VIRT_UART_BASE;
		const unsigned long pmd_idx =
			(unsigned long)((uart_pa >> PMD_SHIFT) & (PTRS_PER_PMD - 1));
		const unsigned long pte_idx =
			(unsigned long)((uart_pa >> PAGE_SHIFT) & (PTRS_PER_PTE - 1));
		pte_t *uart_pte;

		if (pmd_none(pmd[pmd_idx])) {
			pte_t *new_pte = (pte_t *)linx_alloc_pgtable_page();
			pmd[pmd_idx] =
				__pmd((unsigned long)__pa(new_pte) | LINX_PTE_TYPE_TABLE);
		}

		uart_pte = (pte_t *)pmd_page_vaddr(pmd[pmd_idx]);
		uart_pte[pte_idx] = pfn_pte(PHYS_PFN(uart_pa),
					    linx_pgprot_device_rw());
	}
}

static void __init linx_mmu_enable(void)
{
	const u64 ttbr = (u64)__pa(swapper_pg_dir);
	const u64 mair =
		(0x00ull << 0) |	/* Attr0: Device-nGnRnE */
		(0xffull << 8) |	/* Attr1: Normal WB RA/WA */
		(0x44ull << 16);	/* Attr2: Normal NC */
	const u64 tcr =
		(1ull << 0) |		/* MME */
		(16ull << 1) |		/* T0SZ (48-bit VA) */
		(16ull << 7);		/* T1SZ (48-bit VA) */

	/*
	 * Program ACR1 privileged registers (see LinxISA manual).
	 * Order matters: set table bases/attrs first, then enable MME in TCR.
	 */
	linx_hl_ssrset(ttbr, 0x1f10); /* TTBR0_ACR1 */
	linx_hl_ssrset(ttbr, 0x1f11); /* TTBR1_ACR1 (unused for now) */
	linx_hl_ssrset(mair, 0x1f13); /* MAIR_ACR1 */
	linx_hl_ssrset(tcr, 0x1f12);  /* TCR_ACR1 (MME=1) */
	asm volatile("tlb.iall" : : : "memory");
}

#endif /* CONFIG_MMU */

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

#ifdef CONFIG_MMU
	linx_mmu_map_direct(mem_start, mem_end);
	linx_mmu_enable();
#endif

	free_area_init(max_zone_pfn);
}
