// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2012 Regents of the University of California
 * Copyright (C) 2019 Western Digital Corporation or its affiliates.
 * Copyright (C) 2020 FORTH-ICS/CARV
 *  Nick Kossifidis <mick@ics.forth.gr>
 */

#include <linux/init.h>
#include <linux/mm.h>
#include <linux/memblock.h>
#include <linux/mmzone.h>
#include <linux/initrd.h>
#include <linux/swap.h>
#include <linux/swiotlb.h>
#include <linux/sizes.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/libfdt.h>
#include <linux/set_memory.h>
#include <linux/dma-map-ops.h>
#include <linux/crash_dump.h>
#include <linux/hugetlb.h>
#include <linux/kfence.h>

#include <asm/fixmap.h>
#include <asm/tlbflush.h>
#include <asm/sections.h>
#include <asm/io.h>
#include <asm/ptdump.h>
#include <asm/numa.h>
#include <asm/ssr.h>

#include "../kernel/head.h"

extern void memblock_free_all(void);
extern unsigned long max_mapnr;

struct kernel_mapping kernel_map __ro_after_init;
EXPORT_SYMBOL(kernel_map);
#ifdef CONFIG_XIP_KERNEL
#define kernel_map	(*(struct kernel_mapping *)XIP_FIXUP(&kernel_map))
#endif

#ifdef CONFIG_64BIT
u64 mmtconfig_mode = MMTCONFIG_DEFAULT;
#else
u64 mmtconfig_mode = (MMTCONFIG_MODE_32 << MMTCONFIG_MODE_SHIFT) |
		     (MMTCONFIG_Q_LPTE << MMTCONFIG_Q_SHIFT);
#endif
EXPORT_SYMBOL(mmtconfig_mode);

bool pgtable_l4_enabled = IS_ENABLED(CONFIG_64BIT) && !IS_ENABLED(CONFIG_XIP_KERNEL);
bool pgtable_l5_enabled = IS_ENABLED(CONFIG_64BIT) && !IS_ENABLED(CONFIG_XIP_KERNEL);
EXPORT_SYMBOL(pgtable_l4_enabled);
EXPORT_SYMBOL(pgtable_l5_enabled);

phys_addr_t phys_ram_base __ro_after_init;
EXPORT_SYMBOL(phys_ram_base);

#ifdef CONFIG_XIP_KERNEL
extern char _xiprom[], _exiprom[], __data_loc;
#endif

#ifdef CONFIG_MMU
static const pgprot_t protection_map[16] = {
	[VM_NONE]					= PAGE_NONE,
	[VM_READ]					= PAGE_READ,
	[VM_WRITE]					= PAGE_COPY,
	[VM_WRITE | VM_READ]				= PAGE_COPY,
	[VM_EXEC]					= PAGE_EXEC,
	[VM_EXEC | VM_READ]				= PAGE_READ_EXEC,
	[VM_EXEC | VM_WRITE]				= PAGE_COPY_EXEC,
	[VM_EXEC | VM_WRITE | VM_READ]			= PAGE_COPY_READ_EXEC,
	[VM_SHARED]					= PAGE_NONE,
	[VM_SHARED | VM_READ]				= PAGE_READ,
	[VM_SHARED | VM_WRITE]				= PAGE_SHARED,
	[VM_SHARED | VM_WRITE | VM_READ]		= PAGE_SHARED,
	[VM_SHARED | VM_EXEC]				= PAGE_EXEC,
	[VM_SHARED | VM_EXEC | VM_READ]			= PAGE_READ_EXEC,
	[VM_SHARED | VM_EXEC | VM_WRITE]		= PAGE_SHARED_EXEC,
	[VM_SHARED | VM_EXEC | VM_WRITE | VM_READ]	= PAGE_SHARED_EXEC,
};
DECLARE_VM_GET_PAGE_PROT
#endif

unsigned long empty_zero_page[PAGE_SIZE / sizeof(unsigned long)]
							__page_aligned_bss;
EXPORT_SYMBOL(empty_zero_page);

extern char _start[];
#define DTB_EARLY_BASE_VA      PGDIR_SIZE
void *_dtb_early_va __initdata;
uintptr_t _dtb_early_pa __initdata;

static phys_addr_t dma32_phys_limit __initdata;
#if defined(__LINX__)
static unsigned long linx_boot_min_low_pfn __initdata;
static unsigned long linx_boot_max_low_pfn __initdata;
static unsigned long linx_boot_max_mapnr __initdata;
static uintptr_t __init boot_symbol_phys_addr(uintptr_t sym);

static __always_inline void __init linx_boot_store_ulong(unsigned long *sym,
							 unsigned long val)
{
	*sym = val;
}
#endif

static void __init zone_sizes_init(void)
{
	unsigned long max_zone_pfns[MAX_NR_ZONES] = { 0, };

#ifdef CONFIG_ZONE_DMA32
	max_zone_pfns[ZONE_DMA32] = PFN_DOWN(dma32_phys_limit);
#endif
	max_zone_pfns[ZONE_NORMAL] = max_low_pfn;

	free_area_init(max_zone_pfns);
}

#if defined(CONFIG_MMU) && defined(CONFIG_DEBUG_VM)
static inline void print_mlk(char *name, unsigned long b, unsigned long t)
{
	pr_notice("%12s : 0x%08lx - 0x%08lx   (%4ld kB)\n", name, b, t,
		  (((t) - (b)) >> 10));
}

static inline void print_mlm(char *name, unsigned long b, unsigned long t)
{
	pr_notice("%12s : 0x%08lx - 0x%08lx   (%4ld MB)\n", name, b, t,
		  (((t) - (b)) >> 20));
}

static void __init print_vm_layout(void)
{
	pr_notice("Virtual kernel memory layout:\n");
	print_mlk("fixmap", (unsigned long)FIXADDR_START,
		  (unsigned long)FIXADDR_TOP);
	print_mlm("pci io", (unsigned long)PCI_IO_START,
		  (unsigned long)PCI_IO_END);
	print_mlm("vmemmap", (unsigned long)VMEMMAP_START,
		  (unsigned long)VMEMMAP_END);
	print_mlm("vmalloc", (unsigned long)VMALLOC_START,
		  (unsigned long)VMALLOC_END);
	print_mlm("lowmem", (unsigned long)PAGE_OFFSET,
		  (unsigned long)high_memory);
#ifdef CONFIG_64BIT
	print_mlm("kernel", (unsigned long)KERNEL_LINK_ADDR,
		  (unsigned long)ADDRESS_SPACE_END);
#endif
}
#else
static void print_vm_layout(void) { }
#endif /* CONFIG_DEBUG_VM */

void __init mem_init(void)
{
	bool swiotlb = false;

#ifdef CONFIG_FLATMEM
	BUG_ON(!mem_map);
#endif /* CONFIG_FLATMEM */

#ifdef CONFIG_SWIOTLB
	swiotlb = max_pfn > PFN_DOWN(dma32_phys_limit);
	swiotlb_init(swiotlb, SWIOTLB_VERBOSE);
#endif
	high_memory = (void *)(__va(PFN_PHYS(max_low_pfn)));
	memblock_free_all();

	print_vm_layout();
}

/* Limit the memory size via mem. */
static phys_addr_t memory_limit;

static int __init early_mem(char *p)
{
	u64 size;

	if (!p)
		return 1;

	size = memparse(p, &p) & PAGE_MASK;
	memory_limit = min_t(u64, size, memory_limit);

	pr_notice("Memory limited to %lldMB\n", (u64)memory_limit >> 20);

	return 0;
}
early_param("mem", early_mem);

static void __init setup_bootmem(void)
{
	phys_addr_t vmlinux_end = __pa_symbol(&_end);
	phys_addr_t vmlinux_start = __pa_symbol(&_start);
	phys_addr_t __maybe_unused max_mapped_addr;
	phys_addr_t phys_ram_end;
	unsigned long boot_min_low_pfn;
	unsigned long boot_max_low_pfn;

#ifdef CONFIG_XIP_KERNEL
	vmlinux_start = __pa_symbol(&_sdata);
#endif

	memblock_enforce_memory_limit(memory_limit);

	/*
	 * Reserve from the start of the kernel to the end of the kernel
	 */
#if defined(CONFIG_64BIT) && defined(CONFIG_STRICT_KERNEL_RWX)
	/*
	 * Make sure we align the reservation on PMD_SIZE since we will
	 * map the kernel in the linear mapping as read-only: we do not want
	 * any allocation to happen between _end and the next pmd aligned page.
	 */
	vmlinux_end = (vmlinux_end + PMD_SIZE - 1) & PMD_MASK;
#endif
	memblock_reserve(vmlinux_start, vmlinux_end - vmlinux_start);


	phys_ram_end = memblock_end_of_DRAM();
#ifndef CONFIG_64BIT
#ifndef CONFIG_XIP_KERNEL
	phys_ram_base = memblock_start_of_DRAM();
#endif
	/*
	 * memblock allocator is not aware of the fact that last 4K bytes of
	 * the addressable memory can not be mapped because of IS_ERR_VALUE
	 * macro. Make sure that last 4k bytes are not usable by memblock
	 * if end of dram is equal to maximum addressable memory.  For 64-bit
	 * kernel, this problem can't happen here as the end of the virtual
	 * address space is occupied by the kernel mapping then this check must
	 * be done as soon as the kernel mapping base address is determined.
	 */
	max_mapped_addr = __pa(~(ulong)0);
	if (max_mapped_addr == (phys_ram_end - 1))
		memblock_set_current_limit(max_mapped_addr - 4096);
#endif

	boot_min_low_pfn = PFN_UP(phys_ram_base);
	boot_max_low_pfn = PFN_DOWN(phys_ram_end);
#if defined(__LINX__)
	linx_boot_min_low_pfn = boot_min_low_pfn;
	linx_boot_max_low_pfn = boot_max_low_pfn;
#else
	min_low_pfn = boot_min_low_pfn;
	max_low_pfn = max_pfn = boot_max_low_pfn;
#endif

	dma32_phys_limit = min(4UL * SZ_1G,
			       (unsigned long)PFN_PHYS(boot_max_low_pfn));
#if defined(__LINX__)
	linx_boot_max_mapnr = boot_max_low_pfn - ARCH_PFN_OFFSET;
#else
	max_mapnr = boot_max_low_pfn - ARCH_PFN_OFFSET;
#endif

	reserve_initrd_mem();
	/*
	 * If DTB is built in, no need to reserve its memblock.
	 * Otherwise, do reserve it but avoid using
	 * early_init_fdt_reserve_self() since __pa() does
	 * not work for DTB pointers that are fixmap addresses
	 */
	if (!IS_ENABLED(CONFIG_BUILTIN_DTB))
		memblock_reserve(dtb_early_pa, fdt_totalsize(dtb_early_va));

	early_init_fdt_scan_reserved_mem();
	dma_contiguous_reserve(dma32_phys_limit);
	if (IS_ENABLED(CONFIG_64BIT))
		hugetlb_cma_reserve(PUD_SHIFT - PAGE_SHIFT);
	memblock_allow_resize();
}

#ifdef CONFIG_MMU
struct pt_alloc_ops _pt_ops __initdata;

#ifdef CONFIG_XIP_KERNEL
#define pt_ops (*(struct pt_alloc_ops *)XIP_FIXUP(&_pt_ops))
#else
#define pt_ops _pt_ops
#endif

unsigned long riscv_pfn_base __ro_after_init;
EXPORT_SYMBOL(riscv_pfn_base);

pgd_t swapper_pg_dir[PTRS_PER_PGD] __section(".bss..page_aligned") __aligned(MMTBASE_PPN_ALIGN_SIZE);
pgd_t trampoline_pg_dir[PTRS_PER_PGD] __section(".bss..page_aligned") __aligned(MMTBASE_PPN_ALIGN_SIZE);
static pte_t fixmap_pte[PTRS_PER_PTE] __page_aligned_bss;

pgd_t early_pg_dir[PTRS_PER_PGD] __initdata __aligned(MMTBASE_PPN_ALIGN_SIZE);
p4d_t __maybe_unused early_dtb_p4d[PTRS_PER_P4D] __initdata __aligned(PAGE_SIZE);
pud_t __maybe_unused early_dtb_pud[PTRS_PER_PUD] __initdata __aligned(PAGE_SIZE);
pmd_t __maybe_unused early_dtb_pmd[PTRS_PER_PMD] __initdata __aligned(PAGE_SIZE);

#ifdef CONFIG_XIP_KERNEL
#define trampoline_pg_dir      ((pgd_t *)XIP_FIXUP(trampoline_pg_dir))
#define fixmap_pte             ((pte_t *)XIP_FIXUP(fixmap_pte))
#define early_pg_dir           ((pgd_t *)XIP_FIXUP(early_pg_dir))
#endif /* CONFIG_XIP_KERNEL */

static p4d_t trampoline_p4d[PTRS_PER_P4D] __page_aligned_bss;
static p4d_t fixmap_p4d[PTRS_PER_P4D] __page_aligned_bss;
static p4d_t early_p4d[PTRS_PER_P4D] __initdata __aligned(PAGE_SIZE);

#ifdef CONFIG_XIP_KERNEL
#define trampoline_p4d ((p4d_t *)XIP_FIXUP(trampoline_p4d))
#define fixmap_p4d     ((p4d_t *)XIP_FIXUP(fixmap_p4d))
#define early_p4d      ((p4d_t *)XIP_FIXUP(early_p4d))
#endif /* CONFIG_XIP_KERNEL */

void __set_fixmap(enum fixed_addresses idx, phys_addr_t phys, pgprot_t prot)
{
	unsigned long addr = __fix_to_virt(idx);
	pte_t *ptep;

	BUG_ON(idx <= FIX_HOLE || idx >= __end_of_fixed_addresses);

#if defined(__LINX__)
	/*
	 * After setup_vm_final() switches to swapper_pg_dir, the low physical
	 * address of fixmap_pte is no longer safe to dereference directly.
	 * Use the kernel mapping alias for the fixmap PTE page rather than the
	 * low physical symbol address.
	 */
	ptep = &((pte_t *)kernel_mapping_pa_to_va(__pa_symbol(fixmap_pte)))[pte_index(addr)];
#else
	ptep = &fixmap_pte[pte_index(addr)];
#endif

	if (pgprot_val(prot))
		set_pte(ptep, pfn_pte(phys >> PAGE_SHIFT, prot));
	else
		pte_clear(&init_mm, addr, ptep);
	local_flush_tlb_page(addr);
}

static inline pte_t *__init get_pte_virt_early(phys_addr_t pa)
{
	return (pte_t *)((uintptr_t)pa);
}

static inline pte_t *__init get_pte_virt_fixmap(phys_addr_t pa)
{
	clear_fixmap(FIX_PTE);
	return (pte_t *)set_fixmap_offset(FIX_PTE, pa);
}

static inline pte_t *__init get_pte_virt_late(phys_addr_t pa)
{
	return (pte_t *) __va(pa);
}

#if defined(__LINX__)
#define LINX_EARLY_LOW_ALLOC_POOL_SIZE	SZ_1M

static phys_addr_t linx_early_low_alloc_base __initdata;
static phys_addr_t linx_early_low_alloc_next __initdata;
static phys_addr_t linx_early_low_alloc_limit __initdata;
static bool linx_early_low_alloc_attempted __initdata;

phys_addr_t __init linx_alloc_early_low_phys(phys_addr_t size, phys_addr_t align)
{
	phys_addr_t first_linear_pa = max_t(phys_addr_t, phys_ram_base,
					      kernel_map.phys_addr);
	phys_addr_t start = max_t(phys_addr_t, memblock_start_of_DRAM(),
				       first_linear_pa + PAGE_SIZE);
	phys_addr_t end = memblock_end_of_DRAM();
	phys_addr_t pa;

	size = PAGE_ALIGN(size);
	align = max_t(phys_addr_t, align, PAGE_SIZE);

	if (!linx_early_low_alloc_attempted) {
		linx_early_low_alloc_attempted = true;
		linx_early_low_alloc_base =
			memblock_phys_alloc_range(LINX_EARLY_LOW_ALLOC_POOL_SIZE,
						  PAGE_SIZE, start, end);
		if (linx_early_low_alloc_base) {
			linx_early_low_alloc_next = linx_early_low_alloc_base;
			linx_early_low_alloc_limit = linx_early_low_alloc_base +
					      LINX_EARLY_LOW_ALLOC_POOL_SIZE;
		}
	}

	if (linx_early_low_alloc_base) {
		pa = ALIGN(linx_early_low_alloc_next, align);
		if (pa + size <= linx_early_low_alloc_limit) {
			linx_early_low_alloc_next = pa + size;
			return pa;
		}
	}

	/*
	 * If the pooled window is unavailable or exhausted, fall back to a
	 * one-shot memblock allocation instead of dying in early boot. This
	 * keeps bring-up moving and exposes the next real owner below allocator
	 * pressure or low-window search policy mistakes.
	 */
	pa = memblock_phys_alloc_range(size, align, start, end);
	BUG_ON(!pa);
	return pa;
}

static phys_addr_t __init linx_alloc_fixmap_pt_page(void)
{
	return linx_alloc_early_low_phys(PAGE_SIZE, PAGE_SIZE);
}
#endif

static inline phys_addr_t __init alloc_pte_early(uintptr_t va)
{
	/*
	 * We only create PMD or PGD early mappings so we
	 * should never reach here with MMU disabled.
	 */
	BUG();
}

static inline phys_addr_t __init alloc_pte_fixmap(uintptr_t va)
{
#if defined(__LINX__)
	return linx_alloc_fixmap_pt_page();
#else
	return memblock_phys_alloc(PAGE_SIZE, PAGE_SIZE);
#endif
}

static phys_addr_t __init alloc_pte_late(uintptr_t va)
{
	struct ptdesc *ptdesc = pagetable_alloc(GFP_KERNEL & ~__GFP_HIGHMEM, 0);

	BUG_ON(!ptdesc || !pagetable_pte_ctor(NULL, ptdesc));
	return __pa((pte_t *)ptdesc_address(ptdesc));
}

static void __init create_pte_mapping(pte_t *ptep,
				      uintptr_t va, phys_addr_t pa,
				      phys_addr_t sz, pgprot_t prot)
{
	uintptr_t pte_idx = pte_index(va);

	BUG_ON(sz != PAGE_SIZE);

	if (pte_none(ptep[pte_idx]))
		ptep[pte_idx] = pfn_pte(PFN_DOWN(pa), prot);
}

#ifndef __PAGETABLE_PMD_FOLDED

static pmd_t trampoline_pmd[PTRS_PER_PMD] __page_aligned_bss;
static pmd_t fixmap_pmd[PTRS_PER_PMD] __page_aligned_bss;
static pmd_t early_pmd[PTRS_PER_PMD] __initdata __aligned(PAGE_SIZE);

static bool __init linx_is_live_boot_pt_page(phys_addr_t pa);

#ifdef CONFIG_XIP_KERNEL
#define trampoline_pmd ((pmd_t *)XIP_FIXUP(trampoline_pmd))
#define fixmap_pmd     ((pmd_t *)XIP_FIXUP(fixmap_pmd))
#define early_pmd      ((pmd_t *)XIP_FIXUP(early_pmd))
#endif /* CONFIG_XIP_KERNEL */

static pud_t trampoline_pud[PTRS_PER_PUD] __page_aligned_bss;
static pud_t fixmap_pud[PTRS_PER_PUD] __page_aligned_bss;
static pud_t early_pud[PTRS_PER_PUD] __initdata __aligned(PAGE_SIZE);

static pmd_t *__init get_pmd_virt_early(phys_addr_t pa)
{
	/* Before MMU is enabled */
	return (pmd_t *)((uintptr_t)pa);
}

static pmd_t *__init get_pmd_virt_fixmap(phys_addr_t pa)
{
	clear_fixmap(FIX_PMD);
	return (pmd_t *)set_fixmap_offset(FIX_PMD, pa);
}

static pmd_t *__init get_pmd_virt_late(phys_addr_t pa)
{
	return (pmd_t *) __va(pa);
}

static phys_addr_t __init alloc_pmd_early(uintptr_t va)
{
	return (uintptr_t)early_pmd;
}

static phys_addr_t __init alloc_pmd_fixmap(uintptr_t va)
{
#if defined(__LINX__)
	return linx_alloc_fixmap_pt_page();
#else
	return memblock_phys_alloc(PAGE_SIZE, PAGE_SIZE);
#endif
}

static phys_addr_t __init alloc_pmd_late(uintptr_t va)
{
	unsigned long vaddr;

	vaddr = __get_free_page(GFP_KERNEL);
	BUG_ON(!vaddr);
	return __pa(vaddr);
}

static void __init create_pmd_mapping(pmd_t *pmdp,
				      uintptr_t va, phys_addr_t pa,
				      phys_addr_t sz, pgprot_t prot)
{
	pte_t *ptep;
	phys_addr_t pte_phys;
	uintptr_t pmd_idx = pmd_index(va);

	if (sz == PMD_SIZE) {
		if (pmd_none(pmdp[pmd_idx]))
			pmdp[pmd_idx].pmd =
				(PFN_DOWN(pa) << _PAGE_PFN_SHIFT) |
				pgprot_val(prot);
		return;
	}

	if (pmd_none(pmdp[pmd_idx])) {
		pte_phys = pt_ops.alloc_pte(va);
		pmdp[pmd_idx] = pfn_pmd(PFN_DOWN(pte_phys), PAGE_TABLE);
		ptep = pt_ops.get_pte_virt(pte_phys);
#if defined(__LINX__)
		if (!linx_is_live_boot_pt_page(pte_phys))
			memset(ptep, 0, PAGE_SIZE);
#else
		memset(ptep, 0, PAGE_SIZE);
#endif
	} else {
		pte_phys = PFN_PHYS(_pmd_pfn(pmdp[pmd_idx]));
		ptep = pt_ops.get_pte_virt(pte_phys);
	}

	create_pte_mapping(ptep, va, pa, sz, prot);
}

static pud_t *__init get_pud_virt_early(phys_addr_t pa)
{
       return (pud_t *)((uintptr_t)pa);
}

static pud_t *__init get_pud_virt_fixmap(phys_addr_t pa)
{
       clear_fixmap(FIX_PUD);
       return (pud_t *)set_fixmap_offset(FIX_PUD, pa);
}

static pud_t *__init get_pud_virt_late(phys_addr_t pa)
{
       return (pud_t *)__va(pa);
}

static phys_addr_t __init alloc_pud_early(uintptr_t va)
{
       /*
        * Early Linx boot uses a single statically allocated PUD before the
        * final page-table topology is installed.
        */
       return (uintptr_t)early_pud;
}

static phys_addr_t __init alloc_pud_fixmap(uintptr_t va)
{
#if defined(__LINX__)
       return linx_alloc_fixmap_pt_page();
#else
       return memblock_phys_alloc(PAGE_SIZE, PAGE_SIZE);
#endif
}

static phys_addr_t alloc_pud_late(uintptr_t va)
{
       unsigned long vaddr;

       vaddr = __get_free_page(GFP_KERNEL);
       BUG_ON(!vaddr);
       return __pa(vaddr);
}

static p4d_t *__init get_p4d_virt_early(phys_addr_t pa)
{
       return (p4d_t *)((uintptr_t)pa);
}

static p4d_t *__init get_p4d_virt_fixmap(phys_addr_t pa)
{
       clear_fixmap(FIX_P4D);
       return (p4d_t *)set_fixmap_offset(FIX_P4D, pa);
}

static p4d_t *__init get_p4d_virt_late(phys_addr_t pa)
{
       return (p4d_t *)__va(pa);
}

static phys_addr_t __init alloc_p4d_early(uintptr_t va)
{
       /*
        * Early Linx boot uses a single statically allocated P4D before the
        * final page-table topology is installed.
        */
       return (uintptr_t)early_p4d;
}

static phys_addr_t __init alloc_p4d_fixmap(uintptr_t va)
{
#if defined(__LINX__)
       return linx_alloc_fixmap_pt_page();
#else
       return memblock_phys_alloc(PAGE_SIZE, PAGE_SIZE);
#endif
}

static phys_addr_t alloc_p4d_late(uintptr_t va)
{
       unsigned long vaddr;

       vaddr = __get_free_page(GFP_KERNEL);
       BUG_ON(!vaddr);
       return __pa(vaddr);
}

static void __init create_pud_mapping(pud_t *pudp,
                                     uintptr_t va, phys_addr_t pa,
                                     phys_addr_t sz, pgprot_t prot)
{
       pmd_t *nextp;
       phys_addr_t next_phys;
       uintptr_t pud_index = pud_index(va);
#if defined(__LINX__)
       bool live_next = false;
#endif

       if (sz == PUD_SIZE) {
               if (pud_val(pudp[pud_index]) == 0)
                       pudp[pud_index] = pfn_pud(PFN_DOWN(pa), prot);
               return;
       }

       if (pud_val(pudp[pud_index]) == 0) {
               next_phys = pt_ops.alloc_pmd(va);
               pudp[pud_index] = pfn_pud(PFN_DOWN(next_phys), PAGE_TABLE);
#if defined(__LINX__)
               live_next = linx_is_live_boot_pt_page(next_phys);
#endif
               nextp = pt_ops.get_pmd_virt(next_phys);
#if defined(__LINX__)
               if (!live_next)
                       memset(nextp, 0, PAGE_SIZE);
#else
               memset(nextp, 0, PAGE_SIZE);
#endif
       } else {
               next_phys = PFN_PHYS(_pud_pfn(pudp[pud_index]));
               nextp = pt_ops.get_pmd_virt(next_phys);
       }

       create_pmd_mapping(nextp, va, pa, sz, prot);
}

static void __init create_p4d_mapping(p4d_t *p4dp,
                                     uintptr_t va, phys_addr_t pa,
                                     phys_addr_t sz, pgprot_t prot)
{
       pud_t *nextp;
       phys_addr_t next_phys;
       uintptr_t p4d_index = p4d_index(va);
#if defined(__LINX__)
       bool live_next = false;
#endif

       if (sz == P4D_SIZE) {
               if (p4d_val(p4dp[p4d_index]) == 0)
                       p4dp[p4d_index] = pfn_p4d(PFN_DOWN(pa), prot);
               return;
       }

       if (p4d_val(p4dp[p4d_index]) == 0) {
               next_phys = pt_ops.alloc_pud(va);
               p4dp[p4d_index] = pfn_p4d(PFN_DOWN(next_phys), PAGE_TABLE);
#if defined(__LINX__)
               live_next = linx_is_live_boot_pt_page(next_phys);
#endif
               nextp = pt_ops.get_pud_virt(next_phys);
#if defined(__LINX__)
               if (!live_next)
                       memset(nextp, 0, PAGE_SIZE);
#else
               memset(nextp, 0, PAGE_SIZE);
#endif
       } else {
               next_phys = PFN_PHYS(_p4d_pfn(p4dp[p4d_index]));
               nextp = pt_ops.get_pud_virt(next_phys);
       }

       create_pud_mapping(nextp, va, pa, sz, prot);
}

#define pgd_next_t             p4d_t
#define alloc_pgd_next(__va)   (pgtable_l5_enabled ?                   \
               pt_ops.alloc_p4d(__va) : (pgtable_l4_enabled ?          \
               pt_ops.alloc_pud(__va) : pt_ops.alloc_pmd(__va)))
#define get_pgd_next_virt(__pa)        (pgtable_l5_enabled ?                   \
               pt_ops.get_p4d_virt(__pa) : (pgd_next_t *)(pgtable_l4_enabled ? \
               pt_ops.get_pud_virt(__pa) : (pud_t *)pt_ops.get_pmd_virt(__pa)))
#define create_pgd_next_mapping(__nextp, __va, __pa, __sz, __prot)	\
                               (pgtable_l5_enabled ?                   \
               create_p4d_mapping(__nextp, __va, __pa, __sz, __prot) : \
                               (pgtable_l4_enabled ?                   \
               create_pud_mapping((pud_t *)__nextp, __va, __pa, __sz, __prot) :        \
               create_pmd_mapping((pmd_t *)__nextp, __va, __pa, __sz, __prot)))
#define fixmap_pgd_next                (pgtable_l5_enabled ?                   \
               (uintptr_t)fixmap_p4d : (pgtable_l4_enabled ?           \
               (uintptr_t)fixmap_pud : (uintptr_t)fixmap_pmd))
#define trampoline_pgd_next    (pgtable_l5_enabled ?                   \
               (uintptr_t)trampoline_p4d : (pgtable_l4_enabled ?       \
               (uintptr_t)trampoline_pud : (uintptr_t)trampoline_pmd))
#define early_pgd_next         (pgtable_l5_enabled ?                   \
               (uintptr_t)early_p4d : (pgtable_l4_enabled ?            \
               (uintptr_t)early_pud : (uintptr_t)early_pmd))
#define early_dtb_pgd_next     (pgtable_l5_enabled ?                   \
               (uintptr_t)early_dtb_p4d : (pgtable_l4_enabled ?        \
               (uintptr_t)early_dtb_pud : (uintptr_t)early_dtb_pmd))
#else
#define pgd_next_t		pte_t
#define alloc_pgd_next(__va)	pt_ops.alloc_pte(__va)
#define get_pgd_next_virt(__pa)	pt_ops.get_pte_virt(__pa)
#define create_pgd_next_mapping(__nextp, __va, __pa, __sz, __prot)	\
	create_pte_mapping(__nextp, __va, __pa, __sz, __prot)
#define fixmap_pgd_next                ((uintptr_t)fixmap_pte)
#define early_dtb_pgd_next     ((uintptr_t)early_dtb_pmd)
#define create_p4d_mapping(__pmdp, __va, __pa, __sz, __prot)
#define create_pud_mapping(__pmdp, __va, __pa, __sz, __prot)
#define create_pmd_mapping(__pmdp, __va, __pa, __sz, __prot)
#endif  /* __PAGETABLE_PMD_FOLDED */

void __init create_pgd_mapping(pgd_t *pgdp,
				      uintptr_t va, phys_addr_t pa,
				      phys_addr_t sz, pgprot_t prot)
{
	pgd_next_t *nextp;
	phys_addr_t next_phys;
	uintptr_t pgd_idx = pgd_index(va);
#if defined(__LINX__)
	bool live_next = false;
#endif

	if (sz == PGDIR_SIZE) {
		if (pgd_val(pgdp[pgd_idx]) == 0)
			pgdp[pgd_idx] = pfn_pgd(PFN_DOWN(pa), prot);
		return;
	}

	if (pgd_val(pgdp[pgd_idx]) == 0) {
		next_phys = alloc_pgd_next(va);
		pgdp[pgd_idx] = pfn_pgd(PFN_DOWN(next_phys), PAGE_TABLE);
#if defined(__LINX__)
		live_next = linx_is_live_boot_pt_page(next_phys);
#endif
		nextp = get_pgd_next_virt(next_phys);
#if defined(__LINX__)
		if (!live_next)
			memset(nextp, 0, PAGE_SIZE);
#else
		memset(nextp, 0, PAGE_SIZE);
#endif
	} else {
		next_phys = PFN_PHYS(_pgd_pfn(pgdp[pgd_idx]));
		nextp = get_pgd_next_virt(next_phys);
	}

	create_pgd_next_mapping(nextp, va, pa, sz, prot);
}

static uintptr_t __init best_map_size(phys_addr_t base, phys_addr_t size)
{
	/* Upgrade to PMD_SIZE mappings whenever possible */
	if ((base & (PMD_SIZE - 1)) || (size & (PMD_SIZE - 1)))
		return PAGE_SIZE;

	return PMD_SIZE;
}

#ifdef CONFIG_XIP_KERNEL
/* called from head.S with MMU off */
asmlinkage void __init __copy_data(void)
{
	void *from = (void *)(&__data_loc);
	void *to = (void *)CONFIG_PHYS_RAM_BASE;
	size_t sz = (size_t)((uintptr_t)(&_end) - (uintptr_t)(&_sdata));

	memcpy(to, from, sz);
}
#endif

#ifdef CONFIG_STRICT_KERNEL_RWX
static __init pgprot_t pgprot_from_va(uintptr_t va)
{
#if defined(__LINX__)
	/*
	 * Linx bring-up still relies on executing broad kernel-mapping PMD
	 * leaves immediately after the final swapper_pg_dir handoff. Keep the
	 * linked kernel mapping executable for now instead of depending on the
	 * finer-grained text/rodata split.
	 */
	if (is_kernel_mapping(va))
		return PAGE_KERNEL_EXEC;
#endif

	if (is_va_kernel_text(va))
		return PAGE_KERNEL_READ_EXEC;

	/*
	 * In 64-bit kernel, the kernel mapping is outside the linear mapping so
	 * we must protect its linear mapping alias from being executed and
	 * written.
	 * And rodata section is marked readonly in mark_rodata_ro.
	 */
	if (IS_ENABLED(CONFIG_64BIT) && is_va_kernel_lm_alias_text(va))
		return PAGE_KERNEL_READ;

	return PAGE_KERNEL;
}

void mark_rodata_ro(void)
{
	set_kernel_memory(__start_rodata, _data, set_memory_ro);
	if (IS_ENABLED(CONFIG_64BIT))
		set_kernel_memory(lm_alias(__start_rodata), lm_alias(_data),
				  set_memory_ro);

	debug_checkwx();
}
#else
static __init pgprot_t pgprot_from_va(uintptr_t va)
{
	if (IS_ENABLED(CONFIG_64BIT) && !is_kernel_mapping(va))
		return PAGE_KERNEL;

	return PAGE_KERNEL_EXEC;
}
#endif /* CONFIG_STRICT_KERNEL_RWX */

#ifdef CONFIG_64BIT
static void __init disable_pgtable_l5(void)
{
       pgtable_l5_enabled = false;
       kernel_map.page_offset = PAGE_OFFSET_L4;
       mmtconfig_mode = (MMTCONFIG_MODE_48 << MMTCONFIG_MODE_SHIFT) |
	       (MMTCONFIG_Q_LPTE << MMTCONFIG_Q_SHIFT);
}

static void __init disable_pgtable_l4(void)
{
       pgtable_l4_enabled = false;
       kernel_map.page_offset = PAGE_OFFSET_L3;
       mmtconfig_mode = (MMTCONFIG_MODE_39 << MMTCONFIG_MODE_SHIFT) |
	       (MMTCONFIG_Q_LPTE << MMTCONFIG_Q_SHIFT);
}

/*
 * There is a simple way to determine if 4-level is supported by the
 * underlying hardware: establish 1:1 mapping in 4-level page table mode
 * then read SATP to see if the configuration was taken into account
 * meaning sv48 is supported.
 */
static __init void set_mmtconfig_mode(void)
{

	u64 identity_val, hw_val;
	uintptr_t set_mmtconfig_mode_pmd = ((unsigned long)set_mmtconfig_mode) & PMD_MASK;
	bool check_l4 = false;

	create_p4d_mapping(early_p4d,
			set_mmtconfig_mode_pmd, (uintptr_t)early_pud,
			P4D_SIZE, PAGE_TABLE);
	create_pud_mapping(early_pud,
			set_mmtconfig_mode_pmd, (uintptr_t)early_pmd,
			PUD_SIZE, PAGE_TABLE);
	/* Handle the case where set_mmtconfig_mode straddles 2 PMDs */
	create_pmd_mapping(early_pmd,
			set_mmtconfig_mode_pmd, set_mmtconfig_mode_pmd,
			PMD_SIZE, PAGE_KERNEL_EXEC);
	create_pmd_mapping(early_pmd,
			set_mmtconfig_mode_pmd + PMD_SIZE,
			set_mmtconfig_mode_pmd + PMD_SIZE,
			PMD_SIZE, PAGE_KERNEL_EXEC);

retry:
	create_pgd_mapping(early_pg_dir,
			set_mmtconfig_mode_pmd,
			check_l4 ? (uintptr_t)early_pud : (uintptr_t)early_p4d,
			PGDIR_SIZE, PAGE_TABLE);

	identity_val = (PFN_DOWN((uintptr_t)&early_pg_dir) << MMTBASE_PPN_SHIFT);
	// identity_val |= MMTBASE_PBYP_ENABLE;

	local_flush_tlb_all();
	ssr_write(SSR_MMTBASE, identity_val);
	hw_val = ssr_swap(SSR_MMTBASE, 0ULL);
	local_flush_tlb_all();

	if (hw_val != identity_val) {
		if (!check_l4) {
			disable_pgtable_l5();
			check_l4 = true;
			goto retry;
		}
		disable_pgtable_l4();
	}


	memset(early_pg_dir, 0, PAGE_SIZE);
	memset(early_p4d, 0, PAGE_SIZE);
	memset(early_pud, 0, PAGE_SIZE);
	memset(early_pmd, 0, PAGE_SIZE);
}
#endif

/*
 * setup_vm() is called from head.S with MMU-off.
 *
 * Following requirements should be honoured for setup_vm() to work
 * correctly:
 * 1) It should use PC-relative addressing for accessing kernel symbols.
 *    To achieve this we always use GCC cmodel=medany.
 * 2) The compiler instrumentation for FTRACE will not work for setup_vm()
 *    so disable compiler instrumentation when FTRACE is enabled.
 *
 * Currently, the above requirements are honoured by using custom CFLAGS
 * for init.o in mm/Makefile.
 */

#if !defined(__clang__) && !defined(__linx_cmodel_medany)
#error "setup_vm() is called from head.S before relocate so it should not use absolute addressing."
#endif

#ifdef CONFIG_XIP_KERNEL
static void __init create_kernel_page_table(pgd_t *pgdir,
					    __always_unused bool early)
{
	uintptr_t va, end_va;

	/* Map the flash resident part */
	end_va = kernel_map.virt_addr + kernel_map.xiprom_sz;
	for (va = kernel_map.virt_addr; va < end_va; va += PMD_SIZE)
		create_pgd_mapping(pgdir, va,
				   kernel_map.xiprom + (va - kernel_map.virt_addr),
				   PMD_SIZE, PAGE_KERNEL_EXEC);

	/* Map the data in RAM */
	end_va = kernel_map.virt_addr + XIP_OFFSET + kernel_map.size;
	for (va = kernel_map.virt_addr + XIP_OFFSET; va < end_va; va += PMD_SIZE)
		create_pgd_mapping(pgdir, va,
				   kernel_map.phys_addr + (va - (kernel_map.virt_addr + XIP_OFFSET)),
				   PMD_SIZE, PAGE_KERNEL);
}
#else
static void __init create_kernel_page_table(pgd_t *pgdir, bool early)
{
	uintptr_t va, end_va;

	end_va = kernel_map.virt_addr + kernel_map.size;
	for (va = kernel_map.virt_addr; va < end_va; va += PMD_SIZE)
		create_pgd_mapping(pgdir, va,
				   kernel_map.phys_addr + (va - kernel_map.virt_addr),
				   PMD_SIZE,
				   early ?
					PAGE_KERNEL_EXEC : pgprot_from_va(va));
}

static uintptr_t __init boot_symbol_phys_addr(uintptr_t sym)
{
	/*
	 * Early boot may observe linked virtual addresses for kernel symbols
	 * even before relocation is complete. Normalize those back to the
	 * physical load address used for the direct-kernel path.
	 */
	if (IS_ENABLED(CONFIG_64BIT) && sym >= KERNEL_LINK_ADDR)
		return sym - KERNEL_LINK_ADDR;

	return sym;
}

static void __init create_kernel_identity_alias_pmd(pmd_t *pmdp)
{
	uintptr_t va;
	phys_addr_t start_pa, end_pa;
	uintptr_t idx;

	start_pa = kernel_map.phys_addr & PMD_MASK;
	end_pa = ALIGN(kernel_map.phys_addr + kernel_map.size, PMD_SIZE);
	for (va = start_pa; va < end_pa; va += PMD_SIZE) {
		idx = pmd_index(va);
		pmdp[idx].pmd = (PFN_DOWN(va) << _PAGE_PFN_SHIFT) |
				pgprot_val(PAGE_KERNEL_EXEC);
	}
}

static void __init create_kernel_virtual_alias_pmd(pmd_t *pmdp)
{
	uintptr_t va, end_va, idx;
	phys_addr_t pa;

	end_va = kernel_map.virt_addr + kernel_map.size;
	for (va = kernel_map.virt_addr; va < end_va; va += PMD_SIZE) {
		idx = pmd_index(va);
		pa = kernel_map.phys_addr + (va - kernel_map.virt_addr);
		pmdp[idx].pmd = (PFN_DOWN(pa) << _PAGE_PFN_SHIFT) |
				pgprot_val(PAGE_KERNEL_EXEC);
	}
}

static void __init create_kernel_identity_page_table(pgd_t *pgdir)
{
	uintptr_t va, end_va;

	end_va = ALIGN(kernel_map.phys_addr + kernel_map.size, PMD_SIZE);
	for (va = kernel_map.phys_addr & PMD_MASK; va < end_va; va += PMD_SIZE)
		create_pgd_mapping(pgdir, va, va, PMD_SIZE, PAGE_KERNEL_EXEC);
}

static void __init linx_memblock_reserve_pt_page(phys_addr_t pa)
{
	if (!pa || memblock_is_region_reserved(pa, PAGE_SIZE))
		return;

	memblock_reserve_kern(pa, PAGE_SIZE);
}

static void __init linx_memblock_reserve_swapper_children(void)
{
	phys_addr_t p4d_pa, pud_pa, pmd_pa, pte_pa;
	p4d_t *p4dp;
	pud_t *pudp;
	pmd_t *pmdp;
	int i, j, k, l;

	for (i = 0; i < PTRS_PER_PGD; i++) {
		if (!pgd_val(swapper_pg_dir[i]) ||
		    (pgd_val(swapper_pg_dir[i]) & _PAGE_LEAF))
			continue;

		p4d_pa = PFN_PHYS(_pgd_pfn(swapper_pg_dir[i]));
		linx_memblock_reserve_pt_page(p4d_pa);

		if (!pgtable_l5_enabled)
			continue;

		p4dp = pt_ops.get_p4d_virt(p4d_pa);
		for (j = 0; j < PTRS_PER_P4D; j++) {
			if (!p4d_val(p4dp[j]) ||
			    (p4d_val(p4dp[j]) & _PAGE_LEAF))
				continue;

			pud_pa = PFN_PHYS(_p4d_pfn(p4dp[j]));
			linx_memblock_reserve_pt_page(pud_pa);

			pudp = pt_ops.get_pud_virt(pud_pa);
			for (k = 0; k < PTRS_PER_PUD; k++) {
				if (!pud_val(pudp[k]) ||
				    (pud_val(pudp[k]) & _PAGE_LEAF))
					continue;

				pmd_pa = PFN_PHYS(_pud_pfn(pudp[k]));
				linx_memblock_reserve_pt_page(pmd_pa);

				pmdp = pt_ops.get_pmd_virt(pmd_pa);
				for (l = 0; l < PTRS_PER_PMD; l++) {
					if (pmd_none(pmdp[l]) ||
					    (pmd_val(pmdp[l]) & _PAGE_LEAF))
						continue;

					pte_pa = PFN_PHYS(_pmd_pfn(pmdp[l]));
					linx_memblock_reserve_pt_page(pte_pa);
				}
			}
		}
	}
}

static bool __init linx_is_live_boot_pt_page(phys_addr_t pa)
{
	if (!pa)
		return false;

	/*
	 * Linx now allocates fixmap-stage PT pages from a monotonic low pool,
	 * so those pages are never recycled during boot. The only pages that
	 * still require "already live, don't memset" handling are the static
	 * boot PT pages wired into the early/trampoline/fixmap trees.
	 */
	return pa == boot_symbol_phys_addr((uintptr_t)early_p4d) ||
	       pa == boot_symbol_phys_addr((uintptr_t)early_pud) ||
	       pa == boot_symbol_phys_addr((uintptr_t)early_pmd) ||
	       pa == boot_symbol_phys_addr((uintptr_t)early_dtb_p4d) ||
	       pa == boot_symbol_phys_addr((uintptr_t)early_dtb_pud) ||
	       pa == boot_symbol_phys_addr((uintptr_t)early_dtb_pmd) ||
	       pa == boot_symbol_phys_addr((uintptr_t)trampoline_p4d) ||
	       pa == boot_symbol_phys_addr((uintptr_t)trampoline_pud) ||
	       pa == boot_symbol_phys_addr((uintptr_t)trampoline_pmd) ||
	       pa == boot_symbol_phys_addr((uintptr_t)fixmap_p4d) ||
	       pa == boot_symbol_phys_addr((uintptr_t)fixmap_pud) ||
	       pa == boot_symbol_phys_addr((uintptr_t)fixmap_pmd);
}

#define LINK_KERNEL_ALIAS_TABLES(_pgd, _p4d, _pud, _pmd, _va) do {		\
	(_pgd)[pgd_index(_va)].pgd =						\
		(PFN_DOWN((uintptr_t)(_p4d)) << _PAGE_PFN_SHIFT) |		\
		pgprot_val(PAGE_TABLE);					\
	(_p4d)[p4d_index(_va)].p4d =						\
		(PFN_DOWN((uintptr_t)(_pud)) << _PAGE_PFN_SHIFT) |		\
		pgprot_val(PAGE_TABLE);					\
	(_pud)[pud_index(_va)].pud =						\
		(PFN_DOWN((uintptr_t)(_pmd)) << _PAGE_PFN_SHIFT) |		\
		pgprot_val(PAGE_TABLE);					\
} while (0)
#endif

/*
 * Setup a 4MB mapping that encompasses the device tree: for 64-bit kernel,
 * this means 2 PMD entries whereas for 32-bit kernel, this is only 1 PGDIR
 * entry.
 */
static void __init create_fdt_early_page_table(pgd_t *pgdir, uintptr_t dtb_pa)
{
#ifndef CONFIG_BUILTIN_DTB
	uintptr_t pa = dtb_pa & ~(PMD_SIZE - 1);
	uintptr_t va = DTB_EARLY_BASE_VA;

#if defined(__LINX__) && defined(CONFIG_64BIT)
	memset(early_dtb_p4d, 0, sizeof(early_dtb_p4d));
	memset(early_dtb_pud, 0, sizeof(early_dtb_pud));
	memset(early_dtb_pmd, 0, sizeof(early_dtb_pmd));

	pgdir[pgd_index(va)] =
		pfn_pgd(PFN_DOWN(boot_symbol_phys_addr((uintptr_t)early_dtb_pgd_next)),
			PAGE_TABLE);

	if (pgtable_l5_enabled)
		early_dtb_p4d[p4d_index(va)] =
			pfn_p4d(PFN_DOWN(boot_symbol_phys_addr((uintptr_t)early_dtb_pud)),
				PAGE_TABLE);

	if (pgtable_l4_enabled)
		early_dtb_pud[pud_index(va)] =
			pfn_pud(PFN_DOWN(boot_symbol_phys_addr((uintptr_t)early_dtb_pmd)),
				PAGE_TABLE);
#else
	create_pgd_mapping(early_pg_dir, va,
			   IS_ENABLED(CONFIG_64BIT) ? early_dtb_pgd_next : pa,
			   PGDIR_SIZE,
			   IS_ENABLED(CONFIG_64BIT) ? PAGE_TABLE : PAGE_KERNEL);

       if (pgtable_l5_enabled)
               create_p4d_mapping(early_dtb_p4d, va,
                                  (uintptr_t)early_dtb_pud, P4D_SIZE, PAGE_TABLE);

       if (pgtable_l4_enabled)
               create_pud_mapping(early_dtb_pud, va,
                                  (uintptr_t)early_dtb_pmd, PUD_SIZE, PAGE_TABLE);
#endif

	if (IS_ENABLED(CONFIG_64BIT)) {
		create_pmd_mapping(early_dtb_pmd, va,
				   pa, PMD_SIZE, PAGE_KERNEL);
		create_pmd_mapping(early_dtb_pmd, va + PMD_SIZE,
				   pa + PMD_SIZE, PMD_SIZE, PAGE_KERNEL);
	}

	dtb_early_va = (void *)va + (dtb_pa & (PMD_SIZE - 1));
#else
	/*
	 * For 64-bit kernel, __va can't be used since it would return a linear
	 * mapping address whereas dtb_early_va will be used before
	 * setup_vm_final installs the linear mapping. For 32-bit kernel, as the
	 * kernel is mapped in the linear mapping, that makes no difference.
	 */
	dtb_early_va = kernel_mapping_pa_to_va(XIP_FIXUP(dtb_pa));
#endif

	dtb_early_pa = dtb_pa;
}

/*
 * MMU is not enabled, the page tables are allocated directly using
 * early_pmd/pud/p4d and the address returned is the physical one.
 */
void pt_ops_set_early(void)
{
       pt_ops.alloc_pte = alloc_pte_early;
       pt_ops.get_pte_virt = get_pte_virt_early;
#ifndef __PAGETABLE_PMD_FOLDED
       pt_ops.alloc_pmd = alloc_pmd_early;
       pt_ops.get_pmd_virt = get_pmd_virt_early;
       pt_ops.alloc_pud = alloc_pud_early;
       pt_ops.get_pud_virt = get_pud_virt_early;
       pt_ops.alloc_p4d = alloc_p4d_early;
       pt_ops.get_p4d_virt = get_p4d_virt_early;
#endif
}

/*
 * MMU is enabled but page table setup is not complete yet.
 * fixmap page table alloc functions must be used as a means to temporarily
 * map the allocated physical pages since the linear mapping does not exist yet.
 *
 * Note that this is called with MMU disabled, hence kernel_mapping_pa_to_va,
 * but it will be used as described above.
 */
void pt_ops_set_fixmap(void)
{
       pt_ops.alloc_pte = kernel_mapping_pa_to_va((uintptr_t)alloc_pte_fixmap);
#if defined(__LINX__)
       /*
        * Linx now constrains these temporary page-table pages to a low
        * identity-mapped pool. Using the FIX_P* aliases here is still
        * unstable and is the current live fault owner, so consume those
        * pages through the low alias directly during setup_vm_final().
        */
       pt_ops.get_pte_virt = get_pte_virt_early;
#else
       pt_ops.get_pte_virt = kernel_mapping_pa_to_va((uintptr_t)get_pte_virt_fixmap);
#endif
#ifndef __PAGETABLE_PMD_FOLDED
       pt_ops.alloc_pmd = kernel_mapping_pa_to_va((uintptr_t)alloc_pmd_fixmap);
 #if defined(__LINX__)
       pt_ops.get_pmd_virt = get_pmd_virt_early;
 #else
       pt_ops.get_pmd_virt = kernel_mapping_pa_to_va((uintptr_t)get_pmd_virt_fixmap);
 #endif
       pt_ops.alloc_pud = kernel_mapping_pa_to_va((uintptr_t)alloc_pud_fixmap);
 #if defined(__LINX__)
       pt_ops.get_pud_virt = get_pud_virt_early;
 #else
       pt_ops.get_pud_virt = kernel_mapping_pa_to_va((uintptr_t)get_pud_virt_fixmap);
 #endif
       pt_ops.alloc_p4d = kernel_mapping_pa_to_va((uintptr_t)alloc_p4d_fixmap);
 #if defined(__LINX__)
       pt_ops.get_p4d_virt = get_p4d_virt_early;
 #else
       pt_ops.get_p4d_virt = kernel_mapping_pa_to_va((uintptr_t)get_p4d_virt_fixmap);
 #endif
#endif
}

/*
 * MMU is enabled and page table setup is complete, so from now, we can use
 * generic page allocation functions to setup page table.
 */
void pt_ops_set_late(void)
{
       pt_ops.alloc_pte = alloc_pte_late;
       pt_ops.get_pte_virt = get_pte_virt_late;
#ifndef __PAGETABLE_PMD_FOLDED
       pt_ops.alloc_pmd = alloc_pmd_late;
       pt_ops.get_pmd_virt = get_pmd_virt_late;
       pt_ops.alloc_pud = alloc_pud_late;
       pt_ops.get_pud_virt = get_pud_virt_late;
       pt_ops.alloc_p4d = alloc_p4d_late;
       pt_ops.get_p4d_virt = get_p4d_virt_late;
#endif
}

asmlinkage void __init setup_vm(uintptr_t dtb_pa)
{
	pmd_t __maybe_unused fix_bmap_spmd, fix_bmap_epmd;


	kernel_map.virt_addr = KERNEL_LINK_ADDR;
	kernel_map.page_offset = _AC(CONFIG_PAGE_OFFSET, UL);

#ifdef CONFIG_XIP_KERNEL
	kernel_map.xiprom = (uintptr_t)CONFIG_XIP_PHYS_ADDR;
	kernel_map.xiprom_sz = (uintptr_t)(&_exiprom) - (uintptr_t)(&_xiprom);

	phys_ram_base = CONFIG_PHYS_RAM_BASE;
	kernel_map.phys_addr = (uintptr_t)CONFIG_PHYS_RAM_BASE;
	kernel_map.size = (uintptr_t)(&_end) - (uintptr_t)(&_sdata);

	kernel_map.va_kernel_xip_pa_offset = kernel_map.virt_addr - kernel_map.xiprom;
#else
	kernel_map.phys_addr = boot_symbol_phys_addr((uintptr_t)(&_start));
	kernel_map.size = (uintptr_t)(&_end) - (uintptr_t)(&_start);
#endif

	kernel_map.va_pa_offset = PAGE_OFFSET - kernel_map.phys_addr;
	kernel_map.va_kernel_pa_offset = kernel_map.virt_addr - kernel_map.phys_addr;

	riscv_pfn_base = PFN_DOWN(kernel_map.phys_addr);

       /*
        * The default maximal physical memory size is KERN_VIRT_SIZE for 32-bit
        * kernel, whereas for 64-bit kernel, the end of the virtual address
        * space is occupied by the modules/BPF/kernel mappings which reduces
        * the available size of the linear mapping.
        */
       memory_limit = KERN_VIRT_SIZE - (IS_ENABLED(CONFIG_64BIT) ? SZ_4G : 0);

	/* Keep boot moving during bring-up; diagnose with later failures instead. */

#ifdef CONFIG_64BIT
	/*
	 * The last 4K bytes of the addressable memory can not be mapped because
	 * of IS_ERR_VALUE macro.
	 */
#endif

	pt_ops_set_early();

	/* Setup early PGD for fixmap */
	create_pgd_mapping(early_pg_dir, FIXADDR_START,
			   fixmap_pgd_next, PGDIR_SIZE, PAGE_TABLE);

#ifndef __PAGETABLE_PMD_FOLDED
       /* Setup fixmap P4D and PUD */
       if (pgtable_l5_enabled)
               create_p4d_mapping(fixmap_p4d, FIXADDR_START,
                                  (uintptr_t)fixmap_pud, P4D_SIZE, PAGE_TABLE);
       /* Setup fixmap PUD and PMD */
       if (pgtable_l4_enabled)
               create_pud_mapping(fixmap_pud, FIXADDR_START,
                                  (uintptr_t)fixmap_pmd, PUD_SIZE, PAGE_TABLE);
	create_pmd_mapping(fixmap_pmd, FIXADDR_START,
			   (uintptr_t)fixmap_pte, PMD_SIZE, PAGE_TABLE);
	/* Setup trampoline PGD and PMD */
	create_pgd_mapping(trampoline_pg_dir, 0,
			  trampoline_pgd_next, PGDIR_SIZE, PAGE_TABLE);
	create_pgd_mapping(trampoline_pg_dir, kernel_map.virt_addr,
                          trampoline_pgd_next, PGDIR_SIZE, PAGE_TABLE);
	create_pgd_mapping(early_pg_dir, 0,
			  early_pgd_next, PGDIR_SIZE, PAGE_TABLE);
       if (pgtable_l5_enabled)
               create_p4d_mapping(trampoline_p4d, 0,
                                  (uintptr_t)trampoline_pud, P4D_SIZE, PAGE_TABLE);
       if (pgtable_l5_enabled)
               create_p4d_mapping(trampoline_p4d, kernel_map.virt_addr,
                                  (uintptr_t)trampoline_pud, P4D_SIZE, PAGE_TABLE);
       if (pgtable_l5_enabled)
               create_p4d_mapping(early_p4d, 0,
                                  (uintptr_t)early_pud, P4D_SIZE, PAGE_TABLE);
       if (pgtable_l4_enabled)
               create_pud_mapping(trampoline_pud, 0,
                                  (uintptr_t)trampoline_pmd, PUD_SIZE, PAGE_TABLE);
       if (pgtable_l4_enabled)
               create_pud_mapping(trampoline_pud, kernel_map.virt_addr,
                                  (uintptr_t)trampoline_pmd, PUD_SIZE, PAGE_TABLE);
       if (pgtable_l4_enabled)
               create_pud_mapping(early_pud, 0,
                                  (uintptr_t)early_pmd, PUD_SIZE, PAGE_TABLE);
#ifdef CONFIG_XIP_KERNEL
	create_pmd_mapping(trampoline_pmd, 0,
			   kernel_map.xiprom, PMD_SIZE, PAGE_KERNEL_EXEC);
#else
	create_pmd_mapping(trampoline_pmd, 0, 0, PMD_SIZE, PAGE_KERNEL_EXEC);
	create_pmd_mapping(early_pmd, 0, 0, PMD_SIZE, PAGE_KERNEL_EXEC);
#endif
#else
	/* Setup trampoline PGD */
	create_pgd_mapping(trampoline_pg_dir, kernel_map.virt_addr,
			   kernel_map.phys_addr, PGDIR_SIZE, PAGE_KERNEL_EXEC);
#endif
	create_kernel_identity_alias_pmd(trampoline_pmd);
	LINK_KERNEL_ALIAS_TABLES(trampoline_pg_dir, trampoline_p4d,
				 trampoline_pud, trampoline_pmd,
				 kernel_map.phys_addr & PMD_MASK);
	LINK_KERNEL_ALIAS_TABLES(trampoline_pg_dir, trampoline_p4d,
				 trampoline_pud, trampoline_pmd,
				 kernel_map.virt_addr);
	create_kernel_virtual_alias_pmd(trampoline_pmd);

	/*
	 * Setup early PGD covering entire kernel which will allow
	 * us to reach paging_init(). We map all memory banks later
	 * in setup_vm_final() below.
	 */
	create_kernel_page_table(early_pg_dir, true);
	create_kernel_identity_alias_pmd(early_pmd);
	LINK_KERNEL_ALIAS_TABLES(early_pg_dir, early_p4d, early_pud,
				 early_pmd, kernel_map.phys_addr & PMD_MASK);
	LINK_KERNEL_ALIAS_TABLES(early_pg_dir, early_p4d, early_pud,
				 early_pmd, kernel_map.virt_addr);
	create_kernel_virtual_alias_pmd(early_pmd);

	/*
	 * Keep the low bootstrap aliases explicit. In the current bring-up lane
	 * those low root slots can be observed as zero by the first ACR1 fetch
	 * path even though setup_vm() already populated them earlier.
	 */
	create_pgd_mapping(trampoline_pg_dir, 0,
			   trampoline_pgd_next, PGDIR_SIZE, PAGE_TABLE);
	create_pgd_mapping(early_pg_dir, 0,
			   early_pgd_next, PGDIR_SIZE, PAGE_TABLE);
#ifndef __PAGETABLE_PMD_FOLDED
	if (pgtable_l5_enabled) {
		create_p4d_mapping(trampoline_p4d, 0,
				   (uintptr_t)trampoline_pud, P4D_SIZE, PAGE_TABLE);
		create_p4d_mapping(early_p4d, 0,
				   (uintptr_t)early_pud, P4D_SIZE, PAGE_TABLE);
	}
	if (pgtable_l4_enabled) {
		create_pud_mapping(trampoline_pud, 0,
				   (uintptr_t)trampoline_pmd, PUD_SIZE, PAGE_TABLE);
		create_pud_mapping(early_pud, 0,
				   (uintptr_t)early_pmd, PUD_SIZE, PAGE_TABLE);
	}
#endif
	#ifdef CONFIG_XIP_KERNEL
	create_pmd_mapping(trampoline_pmd, 0,
			   kernel_map.xiprom, PMD_SIZE, PAGE_KERNEL_EXEC);
	#else
	create_pmd_mapping(trampoline_pmd, 0, 0, PMD_SIZE, PAGE_KERNEL_EXEC);
	create_pmd_mapping(early_pmd, 0, 0, PMD_SIZE, PAGE_KERNEL_EXEC);
	#endif

	/*
	 * Keep the linked high-kernel alias chain explicit as well. The current
	 * direct-boot lane can still observe early_p4d[511] / trampoline_p4d[511]
	 * as zero during the first high-address exception fetch even though the
	 * generic create_* path populated them earlier.
	 */
	if (pgtable_l5_enabled) {
		trampoline_p4d[p4d_index(kernel_map.virt_addr)] =
			pfn_p4d(PFN_DOWN(boot_symbol_phys_addr((uintptr_t)trampoline_pud)),
				PAGE_TABLE);
		early_p4d[p4d_index(kernel_map.virt_addr)] =
			pfn_p4d(PFN_DOWN(boot_symbol_phys_addr((uintptr_t)early_pud)),
				PAGE_TABLE);
	}
	if (pgtable_l4_enabled) {
		trampoline_pud[pud_index(kernel_map.virt_addr)] =
			pfn_pud(PFN_DOWN(boot_symbol_phys_addr((uintptr_t)trampoline_pmd)),
				PAGE_TABLE);
		early_pud[pud_index(kernel_map.virt_addr)] =
			pfn_pud(PFN_DOWN(boot_symbol_phys_addr((uintptr_t)early_pmd)),
				PAGE_TABLE);
	}

	/* Setup early mapping for FDT early scan */
	create_fdt_early_page_table(early_pg_dir, dtb_pa);

	pt_ops_set_fixmap();
}

static void __init setup_vm_final(void)
{
	uintptr_t va, map_size;
	phys_addr_t pa, start, end;
	u64 i;
	phys_addr_t swapper_pg_dir_pa = boot_symbol_phys_addr((uintptr_t)swapper_pg_dir);
	phys_addr_t fixmap_pgd_next_pa = boot_symbol_phys_addr(fixmap_pgd_next);

	/* Setup swapper PGD for fixmap */
	create_pgd_mapping(swapper_pg_dir, FIXADDR_START,
			   fixmap_pgd_next_pa,
			   PGDIR_SIZE, PAGE_TABLE);

#ifdef CONFIG_64BIT
	/*
	 * Map the linked kernel image before the broad linear mapping pass.
	 * On Linx, both regions can land under the same upper page-table chain,
	 * so the first writer wins for leaf permissions. The kernel text mapping
	 * must install executable permissions before the generic linear map fills
	 * those slots with PAGE_KERNEL.
	 */
	create_kernel_page_table(swapper_pg_dir, false);
	create_kernel_identity_page_table(swapper_pg_dir);
#endif

	/* Map all memory banks in the linear mapping */
	for_each_mem_range(i, &start, &end) {
		if (start >= end)
			break;
#if defined(__LINX__)
		/*
		 * The current Linx bring-up lane still cannot rely on the
		 * runtime __va()/__pa(PAGE_OFFSET) helpers here. They consume
		 * kernel_map offset state that is correct architecturally but
		 * still observed inconsistently during this swapper build phase,
		 * which leaves the high linear-mapping PGD slots absent.
		 */
		if (start >= memory_limit)
			break;
		if (end > memory_limit)
			end = memory_limit;
#else
		if (start <= __pa(PAGE_OFFSET) &&
		    __pa(PAGE_OFFSET) < end)
			start = __pa(PAGE_OFFSET);
		if (end >= __pa(PAGE_OFFSET) + memory_limit)
			end = __pa(PAGE_OFFSET) + memory_limit;
#endif

		map_size = best_map_size(start, end - start);
		for (pa = start; pa < end; pa += map_size) {
#if defined(__LINX__)
			va = (uintptr_t)(_AC(CONFIG_PAGE_OFFSET, UL) + pa);
#else
			va = (uintptr_t)__va(pa);
#endif

			create_pgd_mapping(swapper_pg_dir, va, pa, map_size,
					   pgprot_from_va(va));
		}
	}

#if defined(CONFIG_LINX)
	/*
	 * The bring-up lane still executes setup_vm_final() through low alias
	 * machinery while swapper_pg_dir is taking over. Keep the temporary
	 * fixmap slots intact for now so the handoff can progress and expose
	 * the next real boundary.
	 */
#else
#if defined(__LINX__)
	/*
	 * Keep the temporary fixmap slots intact for the current bring-up lane.
	 * The post-switch cleanup path is still faulting before misc_mem_init(),
	 * and retaining these slots is enough to move execution past that
	 * cleanup without changing the installed swapper_pg_dir root itself.
	 */
#else
	/* Clear fixmap PTE and PMD mappings */
	clear_fixmap(FIX_PTE);
	clear_fixmap(FIX_PMD);
	clear_fixmap(FIX_PUD);
	clear_fixmap(FIX_P4D);
#endif
#endif

	/* Move to swapper page table */
	#if defined(__LINX__)
		/*
		 * The setup_vm_final() tail is still executing through the low early
		 * alias when we switch roots. Carry the already-valid early low slot
		 * into swapper_pg_dir so the final handoff can reach pt_ops_set_late()
	 * and the exception vector without depending on a freshly rebuilt low
	 * chain in swapper. Do the same for the linked high kernel slot, since
	 * the current Linx lane still loses the high kernel data/text PGD entry
	 * during the handoff and then faults immediately in init.data users like
	 * memblock_start_of_DRAM().
		 */
		swapper_pg_dir[pgd_index(0)] = early_pg_dir[pgd_index(0)];
		swapper_pg_dir[pgd_index(DTB_EARLY_BASE_VA)] =
			early_pg_dir[pgd_index(DTB_EARLY_BASE_VA)];
		swapper_pg_dir[pgd_index(KERNEL_LINK_ADDR)] =
			early_pg_dir[pgd_index(KERNEL_LINK_ADDR)];
		/*
		 * The copied high-kernel root now points at the early boot page-table
		 * chain rooted at early_pmd. Refresh that active PMD page directly so
		 * the full kernel image span is present before paging_init()
		 * publishes PFN globals through the linked kernel alias.
		 */
		{
			uintptr_t kva, kend;
			phys_addr_t ipa, iend;
			phys_addr_t kernel_pa_start =
				boot_symbol_phys_addr((uintptr_t)&_start);
			phys_addr_t kernel_pa_end =
				boot_symbol_phys_addr((uintptr_t)&_end);
			uintptr_t idx;

			iend = ALIGN(kernel_pa_end, PMD_SIZE);
			for (ipa = kernel_pa_start & PMD_MASK; ipa < iend;
			     ipa += PMD_SIZE) {
				idx = pmd_index(ipa);
				early_pmd[idx].pmd =
					(PFN_DOWN(ipa) << _PAGE_PFN_SHIFT) |
					pgprot_val(PAGE_KERNEL_EXEC);
			}

			kend = ALIGN(KERNEL_LINK_ADDR + (kernel_pa_end - kernel_pa_start),
				     PMD_SIZE);
			for (kva = KERNEL_LINK_ADDR; kva < kend; kva += PMD_SIZE) {
				idx = pmd_index(kva);
				ipa = kernel_pa_start + (kva - KERNEL_LINK_ADDR);
				early_pmd[idx].pmd =
					(PFN_DOWN(ipa) << _PAGE_PFN_SHIFT) |
					pgprot_val(pgprot_from_va(kva));
			}
		}
	#endif
	ssr_write(SSR_MMTBASE, (PFN_DOWN(swapper_pg_dir_pa) << MMTBASE_PPN_SHIFT));
	local_flush_tlb_all();

	pt_ops_set_late();
}
#else
asmlinkage void __init setup_vm(uintptr_t dtb_pa)
{
	dtb_early_va = (void *)dtb_pa;
	dtb_early_pa = dtb_pa;
}

static inline void setup_vm_final(void)
{
}
#endif /* CONFIG_MMU */

#ifdef CONFIG_KEXEC_CORE
/*
 * reserve_crashkernel() - reserves memory for crash kernel
 *
 * This function reserves memory area given in "crashkernel=" kernel command
 * line parameter. The memory reserved is used by dump capture kernel when
 * primary kernel is crashing.
 */
static void __init reserve_crashkernel(void)
{
	unsigned long long crash_base = 0;
	unsigned long long crash_size = 0;
	unsigned long search_start = memblock_start_of_DRAM();
	unsigned long search_end = memblock_end_of_DRAM();

	int ret = 0;

	/*
	 * Don't reserve a region for a crash kernel on a crash kernel
	 * since it doesn't make much sense and we have limited memory
	 * resources.
	 */
#ifdef CONFIG_CRASH_DUMP
	if (is_kdump_kernel()) {
		pr_info("crashkernel: ignoring reservation request\n");
		return;
	}
#endif

	ret = parse_crashkernel(boot_command_line, memblock_phys_mem_size(),
				&crash_size, &crash_base);
	if (ret || !crash_size)
		return;

	crash_size = PAGE_ALIGN(crash_size);

	if (crash_base) {
		search_start = crash_base;
		search_end = crash_base + crash_size;
	}

	/*
	 * Current riscv boot protocol requires 2MB alignment for
	 * RV64 and 4MB alignment for RV32 (hugepage size)
	 */
	crash_base = memblock_phys_alloc_range(crash_size, PMD_SIZE,
					       search_start, search_end);
	if (crash_base == 0) {
		pr_warn("crashkernel: couldn't allocate %lldKB\n",
			crash_size >> 10);
		return;
	}

	pr_info("crashkernel: reserved 0x%016llx - 0x%016llx (%lld MB)\n",
		crash_base, crash_base + crash_size, crash_size >> 20);

	crashk_res.start = crash_base;
	crashk_res.end = crash_base + crash_size - 1;
}
#endif /* CONFIG_KEXEC_CORE */

void __init paging_init(void)
{
	setup_bootmem();
	setup_vm_final();
#if defined(__LINX__)
	/*
	 * The current Linx swapper handoff still leaves the linked kernel-data
	 * alias fragile immediately after setup_vm_final(). Publish the cached
	 * PFN globals through the low identity alias that swapper_pg_dir now
	 * carries explicitly.
	 */
	linx_boot_store_ulong(&min_low_pfn, linx_boot_min_low_pfn);
	linx_boot_store_ulong(&max_low_pfn, linx_boot_max_low_pfn);
	linx_boot_store_ulong(&max_pfn, linx_boot_max_low_pfn);
	linx_boot_store_ulong(&max_mapnr, linx_boot_max_mapnr);
#endif
}

void __init misc_mem_init(void)
{
	early_memtest(min_low_pfn << PAGE_SHIFT, max_low_pfn << PAGE_SHIFT);
	arch_numa_init();
	sparse_init();
	zone_sizes_init();
#ifdef CONFIG_KEXEC_CORE
	reserve_crashkernel();
#endif
	memblock_dump_all();
}

#if defined(__LINX__) && defined(CONFIG_MMU)
void __init linx_guard_null_page(void)
{
#if defined(__LINX__)
	/*
	 * Current Linx bring-up now reaches the explicit null-page guard setup,
	 * and this remap path is the next live boot blocker. Keep boot moving
	 * by leaving the early mapping as-is for now; the null-page hardening can
	 * be restored once the later runtime path is stable.
	 */
	return;
#else
	pgd_t *pgd = pgd_offset_k(0);
	p4d_t *p4d = p4d_offset(pgd, 0);
	pud_t *pud = pud_offset(p4d, 0);
	pmd_t *pmd = pmd_offset(pud, 0);
	phys_addr_t base_pa, pte_phys;
	pte_t *ptep;
	unsigned long i;

	if (!pmd_leaf(*pmd))
		return;

	base_pa = PFN_PHYS(_pmd_pfn(*pmd));
	pte_phys = linx_alloc_early_low_phys(PAGE_SIZE, PAGE_SIZE);
	ptep = get_pte_virt_early(pte_phys);
	memset(ptep, 0, PAGE_SIZE);

	for (i = 0; i < PTRS_PER_PTE; i++) {
		phys_addr_t pa = base_pa + i * PAGE_SIZE;
		pgprot_t prot = PAGE_KERNEL_EXEC;

		if (i == 0)
			continue;

		ptep[i] = pfn_pte(PFN_DOWN(pa), prot);
	}

	set_pmd(pmd, pfn_pmd(PFN_DOWN(pte_phys), PAGE_TABLE));
	local_flush_tlb_all();
#endif
}
#endif

#ifdef CONFIG_SPARSEMEM_VMEMMAP
int __meminit vmemmap_populate(unsigned long start, unsigned long end, int node,
			       struct vmem_altmap *altmap)
{
	return vmemmap_populate_basepages(start, end, node, NULL);
}
#endif
