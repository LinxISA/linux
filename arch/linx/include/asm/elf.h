/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASM_LINX_ELF_H
#define _ASM_LINX_ELF_H

#include <asm/page.h>
#include <asm/ptrace.h>
#include <linux/types.h>
#include <uapi/linux/elf.h>

/*
 * LinxISA bring-up: define the minimal ELF personality for binfmt_elf.
 */
#define ELF_CLASS	ELFCLASS64
#define ELF_DATA	ELFDATA2LSB

/*
 * These are used to set parameters in core dumps and by the FDPIC loader.
 */
#define ELF_ARCH	EM_LINXISA
#define ELF_FDPIC_CORE_EFLAGS	0

/*
 * This is used to ensure we don't load something for the wrong architecture.
 */
#define elf_check_arch(x)	(((x)->e_machine == EM_LINXISA) && \
				 ((x)->e_ident[EI_CLASS] == ELF_CLASS))

#define ELF_EXEC_PAGESIZE	PAGE_SIZE

#define ELF_HWCAP		(0)
#define ELF_PLATFORM		(NULL)

/*
 * FDPIC loader ABI: pass the loadmap/dynamic addresses in a1/a2/a3, matching
 * the existing Linx scalar ABI register naming in Documentation/linxisa/abi.md.
 */
#define ELF_FDPIC_PLAT_INIT(_r, _exec_map_addr, _interp_map_addr, dynamic_addr) \
	do {								     \
		(_r)->regs[PTR_R3] = _exec_map_addr;			     \
		(_r)->regs[PTR_R4] = _interp_map_addr;			     \
		(_r)->regs[PTR_R5] = dynamic_addr;			     \
	} while (0)

typedef unsigned long elf_greg_t;

#define ELF_NGREG		NUM_PTRACE_REG
typedef elf_greg_t elf_gregset_t[ELF_NGREG];

typedef struct {
	unsigned long dummy;
} elf_fpregset_t;

#endif /* _ASM_LINX_ELF_H */
