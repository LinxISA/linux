/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm/asm-offsets.h>
#include <asm/asm.h>
#include <linux/init.h>
#include <linux/linkage.h>
#include <asm/thread_info.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/ssr.h>
#include <asm/hwcap.h>
#include <asm/image.h>
#include <asm/block-def.h>
#include "efi-header.S"

__HEAD
ENTRY(_start)
/* jump to start kernel */
___start_kernel:
    block_next_direct _start_kernel

.macro linx_relocate_inline setvector_label
    BSTART.sys fall

    1: addtpc %tpcrel_hi(.Lkernel_virt_start_value), -> t
    addi t#1, %tpcrel_lo(.Lkernel_virt_start_value), -> t
    ldi [t#1, 0], -> t  /* load KERNEL_LINK_ADDR */
    addi t#3, 0, -> a1 /* relocation delta == KERNEL_LINK_ADDR for direct kernel boot */

    1: addtpc %tpcrel_hi(\setvector_label), -> t
    addi t#1, %tpcrel_lo(\setvector_label), -> t
    add t#1, a1, -> t
    hl.ssrset t#1, SSR_EVBASE

    BSTART.std fall
    srli a0, PAGE_SHIFT, -> t
    slli t#1, MMTBASE_PPN_SHIFT, -> a2

    BSTART.std fall
    1: addtpc %tpcrel_hi(trampoline_pg_dir), -> t
    addi t#1, %tpcrel_lo(trampoline_pg_dir), -> t
    srli t#1, PAGE_SHIFT, -> t
    slli t#1, MMTBASE_PPN_SHIFT, -> a0

    BSTART.sys fall
    tlb.iall
    hl.ssrset a0, SSR_MMTBASE

    hl.ld.pcr [mmtconfig_mode], -> t
    hl.ssrset t#1, SSR_MMCONFIG
\setvector_label:
    BSTART.sys fall
    1: addtpc %tpcrel_hi(.Lsecondary_park), -> t
    addi t#1, %tpcrel_lo(.Lsecondary_park), -> t
    hl.ssrset t#1, SSR_EVBASE

    BSTART.sys fall
    1: addtpc %tpcrel_hi(__global_pointer$), -> t
    addi t#1, %tpcrel_lo(__global_pointer$), -> t
    ssrset t#1, SSR_GP

    BSTART.sys fall
    hl.ssrset a2, SSR_MMTBASE
    tlb.iall
.endm

.macro force_boot_pmd_leaf pmd_sym, slot
    BSTART.std fall
    addi zero, \slot, -> t
    slli t#1, 41, -> t
    addi t#1, 271, -> t
    1: addtpc %tpcrel_hi(\pmd_sym), -> a0
    addi a0, %tpcrel_lo(\pmd_sym), -> a0
    addi zero, \slot, -> a1
    slli a1, 3, -> a1
    add a0, a1, -> a0
    sdi t#1, [a0, 0]
.endm

#ifdef CONFIG_MMU
relocate:
    /* Args: (early_pg_dir ) */
    /* Relocate return address */
    BSTART.sys fall

    1: addtpc %tpcrel_hi(.Lkernel_virt_start_value), -> t
    addi t#1, %tpcrel_lo(.Lkernel_virt_start_value), -> t
    ldi [t#1, 0], -> t  /* load KERNEL_LINK_ADDR */
    addi t#3, 0, -> a1 /* relocation delta == KERNEL_LINK_ADDR for direct kernel boot */
    add ra, a1, -> ra

    /* Point stvec to virtual address of intruction after satp write */
    1: addtpc %tpcrel_hi(.Lsetvector), -> t
    addi t#1, %tpcrel_lo(.Lsetvector), -> t
    add t#1, a1, -> t
    hl.ssrset t#1, SSR_EVBASE


    /* Compute mmtbase for kernel page tables (early_pg_dir), but don't load it yet, till trampoline completed */
    BSTART.std fall
    srli a0, PAGE_SHIFT, -> t   /* a0: early_pg_dir */
    slli t#1, MMTBASE_PPN_SHIFT, -> a2

    /*
	 * Load trampoline page directory, which will cause us to trap to
	 * stvec if VA != PA, or simply fall through if VA == PA.  We need a
	 * full fence here because setup_vm() just wrote these PTEs and we need
	 * to ensure the new translations are in use.
	 */

    BSTART.std fall
    1: addtpc %tpcrel_hi(trampoline_pg_dir), -> t
    addi t#1, %tpcrel_lo(trampoline_pg_dir), -> t
    srli t#1, PAGE_SHIFT, -> t
    slli t#1, MMTBASE_PPN_SHIFT, -> a0

    BSTART.sys fall
    tlb.iall
    hl.ssrset a0, SSR_MMTBASE

    hl.ld.pcr [mmtconfig_mode], -> t
    hl.ssrset t#1, SSR_MMCONFIG
.align 2
.Lsetvector:
    /* Set trap vector to spin forever to help debug */
    BSTART.sys fall
    1: addtpc %tpcrel_hi(.Lsecondary_park), -> t
    addi t#1, %tpcrel_lo(.Lsecondary_park), -> t
    hl.ssrset t#1, SSR_EVBASE

    /* Reload the global pointer */
    BSTART.sys fall
    1: addtpc %tpcrel_hi(__global_pointer$), -> t
    addi t#1, %tpcrel_lo(__global_pointer$), -> t
    ssrset t#1, SSR_GP

    /*
	 * Switch to kernel page tables.  A full fence is necessary in order to
	 * avoid using the trampoline translations, which are only correct for
	 * the first superpage.  Fetching the fence is guarnteed to work
	 * because that first superpage is translated the same way.
	 */
    BSTART.sys fall
    hl.ssrset a2, SSR_MMTBASE
    tlb.iall

    block_next_ret
#endif /* CONFIG_MMU */

#ifdef CONFIG_SMP
    .global secondary_start_sbi
secondary_start_sbi:
    /* Mask all interrupts */
    BSTART.sys fall
    hl.ssrset zero, SSR_ECONFIG
    hl.ssrset zero, SSR_IPENDING

    BSTART.std fall
    /* Load the global pointer */
    1: addtpc %tpcrel_hi(__global_pointer$), -> t
    addi t#1, %tpcrel_lo(__global_pointer$), -> t
    ssrset t#1, SSR_GP

    /* Set trap vector to spin forever to help debug */
    BSTART.sys fall
    1: addtpc %tpcrel_hi(.Lsecondary_park), -> t
    addi t#1, %tpcrel_lo(.Lsecondary_park), -> t
    hl.ssrset t#1, SSR_EVBASE

    BSTART.std fall
    1: addtpc %tpcrel_hi(__cpu_up_stack_pointer), -> t
    addi t#1, %tpcrel_lo(__cpu_up_stack_pointer), -> t
    slli a0, LGREG, -> t
	ld [t#2, t#1], -> sp
    1: addtpc %tpcrel_hi(__cpu_up_task_pointer), -> t
    addi t#1, %tpcrel_lo(__cpu_up_task_pointer), -> t
	ld [t#1, t#3], -> t
    ssrset t#1, SSR_TP

    .global secondary_start_common
secondary_start_common:

    // BSTART.sys fall
    // hl.ld.pcr [mmtconfig_mode], -> t
    // hl.ssrset t#1, SSR_MMCONFIG

#ifdef CONFIG_MMU
    /* Enable virtual memory and relocate to virtual address */
    /* set the formal pg_dir */
    1: addtpc %tpcrel_hi(swapper_pg_dir), -> t
    addi t#1, %tpcrel_lo(swapper_pg_dir), -> a0
    linx_relocate_inline .Lsecondary_setvector
#endif /* CONFIG_MMU */

    BSTART.sys fall
    ssrget SSR_CSTATE, -> t
    ori t#1, CSTATE_ACR1, -> t
    ssrset t#1, SSR_CSTATE

    /* Inline trap-vector setup to avoid another low-head indcall/ret edge. */
    BSTART.sys fall
    1: addtpc %tpcrel_hi(handle_exception), -> t
    addi t#1, %tpcrel_lo(handle_exception), -> t
    hl.ssrset t#1, SSR_EVBASE
    hl.ssrset zero, SSR_ETEMP
    block_next_ind smp_callin
#endif /* CONFIG_SMP */

.align 2
setup_trap_vector:
    /* Set trap vector to exception handler */
    BSTART.sys fall
    1: addtpc %tpcrel_hi(handle_exception), -> t
    addi t#1, %tpcrel_lo(handle_exception), -> t
    hl.ssrset t#1, SSR_EVBASE

    /*
	 * Set sup0 scratch register to 0, indicating to exception vector that
	 * we are presently executing in kernel.
	 */
    hl.ssrset zero, SSR_ETEMP

    block_next_ret

.align 2
.Lsecondary_park:
    BSTART.std direct, .Lsecondary_park
    bwi zero

.balign 8
.Lkernel_virt_start_value:
    .dword KERNEL_LINK_ADDR

END(_start)

.L_start_kernel:
ENTRY(_start_kernel)
    /* Args: (by BIOS) (hart id, DTB )*/
    /* Mask all interrupts */
    BSTART.sys fall
    hl.ssrset zero, SSR_ECONFIG
    hl.ssrset zero, SSR_IPENDING

    BSTART.std fall
    /* Load the global pointer */
    1: addtpc %tpcrel_hi(__global_pointer$), -> t
    addi t#1, %tpcrel_lo(__global_pointer$), -> t
    ssrset t#1, SSR_GP
    BSTOP

#ifdef CONFIG_SMP
    BSTART.std cond, .Lgood_cores
    addi zero, CONFIG_NR_CPUS, -> t
    setc.lt a0, t#1

    block_next_ind .Lsecondary_park
.Lgood_cores:

#endif

    /* Pick one hart to run the main boot sequence. */
#ifdef CONFIG_SMP
    BSTART.std cond, .Lsecondary_start
    B.CATR atomic

    hl.lw.pcr [hart_lottery], -> t
    addi t#1, 1, -> t
    hl.sw.pcr t#1, [hart_lottery]
    /* the hart picking the `0` will serve as main hart, others go waiting */
    setc.ne t#2, zero
#endif

    /* Clear BSS for flat non-ELF images. */
#if defined(__LINX__)
    /*
     * Linx bring-up currently boots the ELF vmlinux image through QEMU's
     * loader, which already provides a zeroed BSS. Running the low-head BSS
     * clear loop under the current ACR1 low-identity window faults partway
     * through the .bss span, so skip it for this boot lane.
     */
#else
    BSTART.std cond, clear_bss_done
    1: addtpc %tpcrel_hi(__bss_start), -> t
    addi t#1, %tpcrel_lo(__bss_start), -> t
    addi t#1, 0, -> a3

    1: addtpc %tpcrel_hi(__bss_stop), -> t
    addi t#1, %tpcrel_lo(__bss_stop), -> t
    addi t#1, 0, -> a4
    setc.ge t#3, t#1

clear_bss:
    BSTART.std cond, clear_bss
    sdi zero, [a3, 0]
    addi a3, RISCV_SZPTR, -> a3
    setc.lt a3, a4
#endif

clear_bss_done:
    /* Save hart ID and DTB physical address */
    BSTART.std fall
    c.movr a0, -> s0
    c.movr a1, -> s1
    1: addtpc %tpcrel_hi(boot_cpu_hartid), -> t
    addi t#1, %tpcrel_lo(boot_cpu_hartid), -> t
    sdi a0, [t#1, 0]

    /* Initialize page tables and relocate to virtual addresses */
    HL.BSTART.CALL setup_vm, 2f, ->ra
    1: addtpc %tpcrel_hi(init_thread_union + THREAD_SIZE), -> t
    addi t#1, %tpcrel_lo(init_thread_union + THREAD_SIZE), -> sp
    // subi t#1, EB_SIZE_ON_STACK, -> sp
    addi s1, 0, -> a0    /* first arg, dtb's address */
    2:

    // BSTART.sys fall
    // hl.ld.pcr [mmtconfig_mode], -> t
    // hl.ssrset t#1, SSR_MMCONFIG
    // ssrset sp, SSR_EBSTATEP     /* EBSTATEP = sp, [sp, init_thread_union + THREAD_SIZE] is reserved for EBSTATE */

#ifdef CONFIG_MMU
    /*
     * Force both the low bootstrap alias root slots and the high-kernel alias
     * root slots before the first MMU switch. The C-side setup_vm() path is
     * still leaving pgd[0] observable as zero in the current direct-boot lane,
     * which immediately strands the first ACR1 fetch after MMU enable.
     */
    BSTART.std fall
    1: addtpc %tpcrel_hi(trampoline_p4d), -> t
    addi t#1, %tpcrel_lo(trampoline_p4d), -> t
    srli t#1, PAGE_SHIFT, -> t
    slli t#1, 32, -> t
    addi t#1, 1, -> t
    1: addtpc %tpcrel_hi(trampoline_pg_dir), -> a0
    addi a0, %tpcrel_lo(trampoline_pg_dir), -> a0
    sdi t#1, [a0, 0]

    BSTART.std fall
    1: addtpc %tpcrel_hi(trampoline_pud), -> t
    addi t#1, %tpcrel_lo(trampoline_pud), -> t
    srli t#1, PAGE_SHIFT, -> t
    slli t#1, 32, -> t
    addi t#1, 1, -> t
    1: addtpc %tpcrel_hi(trampoline_p4d), -> a0
    addi a0, %tpcrel_lo(trampoline_p4d), -> a0
    sdi t#1, [a0, 0]

    BSTART.std fall
    1: addtpc %tpcrel_hi(trampoline_pmd), -> t
    addi t#1, %tpcrel_lo(trampoline_pmd), -> t
    srli t#1, PAGE_SHIFT, -> t
    slli t#1, 32, -> t
    addi t#1, 1, -> t
    1: addtpc %tpcrel_hi(trampoline_pud), -> a0
    addi a0, %tpcrel_lo(trampoline_pud), -> a0
    sdi t#1, [a0, 0]

    BSTART.std fall
    1: addtpc %tpcrel_hi(early_p4d), -> t
    addi t#1, %tpcrel_lo(early_p4d), -> t
    srli t#1, PAGE_SHIFT, -> t
    slli t#1, 32, -> t
    addi t#1, 1, -> t
    1: addtpc %tpcrel_hi(early_pg_dir), -> a0
    addi a0, %tpcrel_lo(early_pg_dir), -> a0
    sdi t#1, [a0, 0]

    BSTART.std fall
    1: addtpc %tpcrel_hi(early_pud), -> t
    addi t#1, %tpcrel_lo(early_pud), -> t
    srli t#1, PAGE_SHIFT, -> t
    slli t#1, 32, -> t
    addi t#1, 1, -> t
    1: addtpc %tpcrel_hi(early_p4d), -> a0
    addi a0, %tpcrel_lo(early_p4d), -> a0
    sdi t#1, [a0, 0]

    BSTART.std fall
    1: addtpc %tpcrel_hi(early_pmd), -> t
    addi t#1, %tpcrel_lo(early_pmd), -> t
    srli t#1, PAGE_SHIFT, -> t
    slli t#1, 32, -> t
    addi t#1, 1, -> t
    1: addtpc %tpcrel_hi(early_pud), -> a0
    addi a0, %tpcrel_lo(early_pud), -> a0
    sdi t#1, [a0, 0]

    /*
     * Force the first low bootstrap PMD leaves as well. The low root chain now
     * reaches early_pmd/trampoline_pmd, but the first ACR1 fetch is still
     * observing pmd[0] as zero in the direct-boot lane.
     */
    force_boot_pmd_leaf trampoline_pmd, 0
    force_boot_pmd_leaf trampoline_pmd, 1
    force_boot_pmd_leaf trampoline_pmd, 2
    force_boot_pmd_leaf trampoline_pmd, 3
    force_boot_pmd_leaf trampoline_pmd, 4
    force_boot_pmd_leaf trampoline_pmd, 5
    force_boot_pmd_leaf trampoline_pmd, 6
    force_boot_pmd_leaf trampoline_pmd, 7
    force_boot_pmd_leaf trampoline_pmd, 8
    force_boot_pmd_leaf trampoline_pmd, 9
    force_boot_pmd_leaf trampoline_pmd, 10
    force_boot_pmd_leaf trampoline_pmd, 11
    force_boot_pmd_leaf trampoline_pmd, 12

    force_boot_pmd_leaf early_pmd, 0
    force_boot_pmd_leaf early_pmd, 1
    force_boot_pmd_leaf early_pmd, 2
    force_boot_pmd_leaf early_pmd, 3
    force_boot_pmd_leaf early_pmd, 4
    force_boot_pmd_leaf early_pmd, 5
    force_boot_pmd_leaf early_pmd, 6
    force_boot_pmd_leaf early_pmd, 7
    force_boot_pmd_leaf early_pmd, 8
    force_boot_pmd_leaf early_pmd, 9
    force_boot_pmd_leaf early_pmd, 10
    force_boot_pmd_leaf early_pmd, 11
    force_boot_pmd_leaf early_pmd, 12

    /*
     * Keep the early DT alias root chain explicit as well:
     *   early_pg_dir[1]      -> early_dtb_p4d
     *   early_dtb_p4d[0]     -> early_dtb_pud
     *   early_dtb_pud[0]     -> early_dtb_pmd
     * The current direct-boot lane can still observe pgd[1] as zero before the
     * first DT validation access even though setup_vm() populated it earlier.
     */
    BSTART.std fall
    1: addtpc %tpcrel_hi(early_dtb_p4d), -> t
    addi t#1, %tpcrel_lo(early_dtb_p4d), -> t
    srli t#1, PAGE_SHIFT, -> t
    slli t#1, 32, -> t
    addi t#1, 1, -> t
    1: addtpc %tpcrel_hi(early_pg_dir), -> a0
    addi a0, %tpcrel_lo(early_pg_dir), -> a0
    addi a0, 8, -> a0
    sdi t#1, [a0, 0]

    BSTART.std fall
    1: addtpc %tpcrel_hi(early_dtb_pud), -> t
    addi t#1, %tpcrel_lo(early_dtb_pud), -> t
    srli t#1, PAGE_SHIFT, -> t
    slli t#1, 32, -> t
    addi t#1, 1, -> t
    1: addtpc %tpcrel_hi(early_dtb_p4d), -> a0
    addi a0, %tpcrel_lo(early_dtb_p4d), -> a0
    sdi t#1, [a0, 0]

    BSTART.std fall
    1: addtpc %tpcrel_hi(early_dtb_pmd), -> t
    addi t#1, %tpcrel_lo(early_dtb_pmd), -> t
    srli t#1, PAGE_SHIFT, -> t
    slli t#1, 32, -> t
    addi t#1, 1, -> t
    1: addtpc %tpcrel_hi(early_dtb_pud), -> a0
    addi a0, %tpcrel_lo(early_dtb_pud), -> a0
    sdi t#1, [a0, 0]

    /*
     * Map the first DT superpage explicitly from the boot DT physical address
     * saved in s1. The current direct-boot lane still observes early_dtb_pmd[0]
     * as zero at the first fdt_check_header() access.
     */
    BSTART.std fall
    hl.andi s1, -2097152, -> t
    srli t#1, PAGE_SHIFT, -> t
    slli t#1, 32, -> t
    addi t#1, 271, -> t
    1: addtpc %tpcrel_hi(early_dtb_pmd), -> a0
    addi a0, %tpcrel_lo(early_dtb_pmd), -> a0
    sdi t#1, [a0, 0]

    /* Force the high-kernel alias root slots before the first MMU switch. */
    BSTART.std fall
    1: addtpc %tpcrel_hi(trampoline_pg_dir), -> t
    addi t#1, %tpcrel_lo(trampoline_pg_dir), -> t
    addi zero, 1, -> a1
    slli a1, 12, -> a1
    add t#1, a1, -> t
    srli t#1, PAGE_SHIFT, -> t
    slli t#1, 32, -> t
    addi t#1, 1, -> t
    1: addtpc %tpcrel_hi(trampoline_pg_dir), -> a0
    addi a0, %tpcrel_lo(trampoline_pg_dir), -> a0
    addi a0, 4088, -> a0
    sdi t#1, [a0, 0]

    BSTART.std fall
    1: addtpc %tpcrel_hi(trampoline_pg_dir), -> t
    addi t#1, %tpcrel_lo(trampoline_pg_dir), -> t
    addi zero, 2, -> a1
    slli a1, 12, -> a1
    add t#1, a1, -> t
    srli t#1, PAGE_SHIFT, -> t
    slli t#1, 32, -> t
    addi t#1, 1, -> t
    1: addtpc %tpcrel_hi(trampoline_pg_dir), -> a0
    addi a0, %tpcrel_lo(trampoline_pg_dir), -> a0
    addi zero, 1, -> a1
    slli a1, 12, -> a1
    add a0, a1, -> a0
    addi a0, 4088, -> a0
    sdi t#1, [a0, 0]

    BSTART.std fall
    1: addtpc %tpcrel_hi(trampoline_pg_dir), -> t
    addi t#1, %tpcrel_lo(trampoline_pg_dir), -> t
    addi zero, 3, -> a1
    slli a1, 12, -> a1
    add t#1, a1, -> t
    srli t#1, PAGE_SHIFT, -> t
    slli t#1, 32, -> t
    addi t#1, 1, -> t
    1: addtpc %tpcrel_hi(trampoline_pg_dir), -> a0
    addi a0, %tpcrel_lo(trampoline_pg_dir), -> a0
    addi zero, 2, -> a1
    slli a1, 12, -> a1
    add a0, a1, -> a0
    addi a0, 4080, -> a0
    sdi t#1, [a0, 0]

    BSTART.std fall
    1: addtpc %tpcrel_hi(early_p4d), -> t
    addi t#1, %tpcrel_lo(early_p4d), -> t
    srli t#1, PAGE_SHIFT, -> t
    slli t#1, 32, -> t
    addi t#1, 1, -> t
    1: addtpc %tpcrel_hi(early_pg_dir), -> a0
    addi a0, %tpcrel_lo(early_pg_dir), -> a0
    addi a0, 4088, -> a0
    sdi t#1, [a0, 0]

    BSTART.std fall
    1: addtpc %tpcrel_hi(early_pud), -> t
    addi t#1, %tpcrel_lo(early_pud), -> t
    srli t#1, PAGE_SHIFT, -> t
    slli t#1, 32, -> t
    addi t#1, 1, -> t
    1: addtpc %tpcrel_hi(early_p4d), -> a0
    addi a0, %tpcrel_lo(early_p4d), -> a0
    addi a0, 4088, -> a0
    sdi t#1, [a0, 0]

    BSTART.std fall
    1: addtpc %tpcrel_hi(early_pmd), -> t
    addi t#1, %tpcrel_lo(early_pmd), -> t
    srli t#1, PAGE_SHIFT, -> t
    slli t#1, 32, -> t
    addi t#1, 1, -> t
    1: addtpc %tpcrel_hi(early_pud), -> a0
    addi a0, %tpcrel_lo(early_pud), -> a0
    addi a0, 4080, -> a0
    sdi t#1, [a0, 0]

    1: addtpc %tpcrel_hi(early_pg_dir), -> t
    addi t#1, %tpcrel_lo(early_pg_dir), -> t
    addi t#1, 0, -> a0
    linx_relocate_inline .Lboot_setvector
#endif /* CONFIG_MMU */

    BSTART.sys fall
    ssrget SSR_CSTATE, -> t
    ori t#1, CSTATE_ACR1, -> t
    ssrset t#1, SSR_CSTATE

    /* Inline trap-vector setup to avoid another low-head indcall/ret edge. */
    BSTART.sys fall
    1: addtpc %tpcrel_hi(handle_exception), -> t
    addi t#1, %tpcrel_lo(handle_exception), -> t
    hl.ssrset t#1, SSR_EVBASE
    hl.ssrset zero, SSR_ETEMP

    /* Restore C environment */
	BSTART.std fall
    1: addtpc %tpcrel_hi(init_task), -> t
    addi t#1, %tpcrel_lo(init_task), -> t
    ssrset t#1, SSR_TP
    1: addtpc %tpcrel_hi(init_thread_union + THREAD_SIZE), -> t
    addi t#1, %tpcrel_lo(init_thread_union + THREAD_SIZE), -> sp
    // subi t#1, EB_SIZE_ON_STACK, -> sp

    /*
     * Switch execution itself onto the linked high-kernel alias before
     * entering generic C code. If we keep executing through the low alias,
     * later ADDTPC+ADDI address materialization computes low addresses for
     * globals while the actual in-memory self-pointers live at the linked
     * high VA, which breaks empty-list and callback-path logic much later.
     */
    BSTART.std ind
    1: addtpc %tpcrel_hi(.Lkernel_virt_start_value), -> t
    addi t#1, %tpcrel_lo(.Lkernel_virt_start_value), -> t
    ldi [t#1, 0], -> t
    add sp, t#1, -> sp
    1: addtpc %tpcrel_hi(start_kernel), -> a0
    addi a0, %tpcrel_lo(start_kernel), -> a0
    add a0, t#1, -> t
    setc.tgt t#1
    BSTOP

.Lsecondary_start:
#ifdef CONFIG_SMP
    /* Set trap vector to spin forever to help debug */
    BSTART.sys fall
    1: addtpc %tpcrel_hi(.Lsecondary_park), -> t
    addi t#1, %tpcrel_lo(.Lsecondary_park), -> t
    hl.ssrset t#1, SSR_EVBASE

    BSTART.std fall
    1: addtpc %tpcrel_hi(__cpu_up_stack_pointer), -> t
    addi t#1, %tpcrel_lo(__cpu_up_stack_pointer), -> t
	slli a0, LGREG, -> t
    add t#2, t#1, -> a1    /* &__cpu_up_stack_pointer[hart_id] */
    1: addtpc %tpcrel_hi(__cpu_up_task_pointer), -> t
    addi t#1, %tpcrel_lo(__cpu_up_task_pointer), -> t
    add t#1, t#3, -> a2    /* &__cpu_up_task_pointer[hart_id] */

    /*
	 * This hart didn't win the lottery, so we wait for the winning hart to
	 * get far enough along the boot process that it should continue.
	*/
.Lwait_for_cpu_up:
    /* FIXME: We should WFI to save some energy here. */
    /* Non-zero sp and tp imply secondary harts are ready */
    BSTART.std cond, .Lwait_for_cpu_up
    ldi [a1, 0], -> t
    addi t#1, 0, -> sp
    setc.eq t#1, zero

    BSTART.std cond, .Lwait_for_cpu_up
    ldi [a2, 0], -> t
    ssrset t#1, SSR_TP
    setc.eq t#2, zero

	BSTART.sys fall
    fence.d rw, rw

    BSTART.std ind
    1: addtpc %tpcrel_hi(.Lkernel_virt_start_value), -> t
    addi t#1, %tpcrel_lo(.Lkernel_virt_start_value), -> t
    ldi [t#1, 0], -> t
    1: addtpc %tpcrel_hi(secondary_start_common), -> a0
    addi a0, %tpcrel_lo(secondary_start_common), -> a0
    add a0, t#1, -> t
    setc.tgt t#1
    BSTOP
#endif

END(_start_kernel)

__PAGE_ALIGNED_BSS
	/* Empty zero page */
	.balign PAGE_SIZE
