/* SPDX-License-Identifier: GPL-2.0 */
#ifndef LINUX_SCRIPTS_ELF_H
#define LINUX_SCRIPTS_ELF_H

/*
 * Kbuild host tools include <elf.h> for parsing the kernel's ELF objects.
 *
 * Most hosts provide a system <elf.h> (glibc, musl, ...), but macOS does not
 * since it uses Mach-O. Provide a small fallback subset for such hosts.
 */

#if !defined(__APPLE__)
#include_next <elf.h>
#define LINUX_SCRIPTS_HAVE_SYSTEM_ELF_H 1
#elif defined(__has_include_next)
#if __has_include_next(<elf.h>)
#include_next <elf.h>
#define LINUX_SCRIPTS_HAVE_SYSTEM_ELF_H 1
#endif
#endif

#ifndef LINUX_SCRIPTS_HAVE_SYSTEM_ELF_H

#include <stdint.h>

#define EI_NIDENT 16

#define EI_MAG0   0
#define EI_MAG1   1
#define EI_MAG2   2
#define EI_MAG3   3
#define EI_CLASS  4
#define EI_DATA   5
#define EI_VERSION 6

#define ELFMAG   "\177ELF"
#define SELFMAG  4

#define ELFMAG0 0x7f
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'

#define ELFCLASS32 1
#define ELFCLASS64 2

#define ELFDATA2LSB 1
#define ELFDATA2MSB 2

#define EV_CURRENT 1

#define ET_NONE 0
#define ET_REL  1
#define ET_EXEC 2
#define ET_DYN  3

#define EM_NONE 0
#define EM_SPARC 2
#define EM_386  3
#define EM_MIPS 8
#define EM_PARISC 15
#define EM_PPC 20
#define EM_PPC64 21
#define EM_ARM  40
#define EM_SPARCV9 43
#define EM_X86_64 62
#define EM_AARCH64 183
#define EM_RISCV 243
#define EM_LOONGARCH 258

#define SHN_UNDEF     0
#define SHN_COMMON    0xfff2
#define SHN_LORESERVE 0xff00
#define SHN_HIRESERVE 0xffff
#define SHN_XINDEX    0xffff

#define SHT_NULL           0
#define SHT_PROGBITS       1
#define SHT_SYMTAB         2
#define SHT_STRTAB         3
#define SHT_RELA           4
#define SHT_NOBITS         8
#define SHT_REL            9
#define SHT_SYMTAB_SHNDX   18

#define SHF_WRITE     0x1
#define SHF_ALLOC     0x2
#define SHF_EXECINSTR 0x4

#define STB_LOCAL  0
#define STB_GLOBAL 1
#define STB_WEAK   2

#define STT_NOTYPE  0
#define STT_OBJECT  1
#define STT_FUNC    2
#define STT_SECTION 3
#define STT_FILE    4
#define STT_LOPROC  13
#define STT_HIPROC  15

#define STT_SPARC_REGISTER 13

/* i386 relocations */
#define R_386_32 1
#define R_386_PC32 2

/* ARM relocations */
#define R_ARM_PC24 0x01
#define R_ARM_ABS32 0x02
#define R_ARM_REL32 0x03
#define R_ARM_CALL 0x1c
#define R_ARM_JUMP24 0x1d
#define R_ARM_THM_JUMP24 0x1e
#define R_ARM_MOVW_ABS_NC 0x2b
#define R_ARM_MOVT_ABS 0x2c
#define R_ARM_THM_MOVW_ABS_NC 0x2f
#define R_ARM_THM_MOVT_ABS 0x30
#define R_ARM_THM_JUMP19 0x33
/* Alias: R_ARM_THM_CALL (0x0a) */
#define R_ARM_THM_PC22 0x0a

/* LoongArch relocations */
#define R_LARCH_SUB32 55
#define R_LARCH_RELAX 100
#define R_LARCH_ALIGN 102

/* MIPS relocations */
#define R_MIPS_32 2
#define R_MIPS_26 4
#define R_MIPS_LO16 6

/* RISC-V relocations */
#define R_RISCV_SUB32 39

typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Word;
typedef int32_t  Elf32_Sword;
typedef uint32_t Elf32_Addr;
typedef uint32_t Elf32_Off;

typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef int32_t  Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef int64_t  Elf64_Sxword;
typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;

typedef struct {
	unsigned char	e_ident[EI_NIDENT];
	Elf32_Half	e_type;
	Elf32_Half	e_machine;
	Elf32_Word	e_version;
	Elf32_Addr	e_entry;
	Elf32_Off	e_phoff;
	Elf32_Off	e_shoff;
	Elf32_Word	e_flags;
	Elf32_Half	e_ehsize;
	Elf32_Half	e_phentsize;
	Elf32_Half	e_phnum;
	Elf32_Half	e_shentsize;
	Elf32_Half	e_shnum;
	Elf32_Half	e_shstrndx;
} Elf32_Ehdr;

typedef struct {
	unsigned char	e_ident[EI_NIDENT];
	Elf64_Half	e_type;
	Elf64_Half	e_machine;
	Elf64_Word	e_version;
	Elf64_Addr	e_entry;
	Elf64_Off	e_phoff;
	Elf64_Off	e_shoff;
	Elf64_Word	e_flags;
	Elf64_Half	e_ehsize;
	Elf64_Half	e_phentsize;
	Elf64_Half	e_phnum;
	Elf64_Half	e_shentsize;
	Elf64_Half	e_shnum;
	Elf64_Half	e_shstrndx;
} Elf64_Ehdr;

typedef struct {
	Elf32_Word	sh_name;
	Elf32_Word	sh_type;
	Elf32_Word	sh_flags;
	Elf32_Addr	sh_addr;
	Elf32_Off	sh_offset;
	Elf32_Word	sh_size;
	Elf32_Word	sh_link;
	Elf32_Word	sh_info;
	Elf32_Word	sh_addralign;
	Elf32_Word	sh_entsize;
} Elf32_Shdr;

typedef struct {
	Elf64_Word	sh_name;
	Elf64_Word	sh_type;
	Elf64_Xword	sh_flags;
	Elf64_Addr	sh_addr;
	Elf64_Off	sh_offset;
	Elf64_Xword	sh_size;
	Elf64_Word	sh_link;
	Elf64_Word	sh_info;
	Elf64_Xword	sh_addralign;
	Elf64_Xword	sh_entsize;
} Elf64_Shdr;

typedef struct {
	Elf32_Word	st_name;
	Elf32_Addr	st_value;
	Elf32_Word	st_size;
	unsigned char	st_info;
	unsigned char	st_other;
	Elf32_Half	st_shndx;
} Elf32_Sym;

typedef struct {
	Elf64_Word	st_name;
	unsigned char	st_info;
	unsigned char	st_other;
	Elf64_Half	st_shndx;
	Elf64_Addr	st_value;
	Elf64_Xword	st_size;
} Elf64_Sym;

typedef struct {
	Elf32_Addr	r_offset;
	Elf32_Word	r_info;
} Elf32_Rel;

typedef struct {
	Elf32_Addr	r_offset;
	Elf32_Word	r_info;
	Elf32_Sword	r_addend;
} Elf32_Rela;

typedef struct {
	Elf64_Addr	r_offset;
	Elf64_Xword	r_info;
} Elf64_Rel;

typedef struct {
	Elf64_Addr	r_offset;
	Elf64_Xword	r_info;
	Elf64_Sxword	r_addend;
} Elf64_Rela;

#define ELF32_ST_BIND(i)   ((i) >> 4)
#define ELF32_ST_TYPE(i)   ((i) & 0xf)
#define ELF32_ST_INFO(b, t) (((b) << 4) + ((t) & 0xf))

#define ELF64_ST_BIND(i)   ELF32_ST_BIND(i)
#define ELF64_ST_TYPE(i)   ELF32_ST_TYPE(i)
#define ELF64_ST_INFO(b, t) ELF32_ST_INFO(b, t)

#define ELF32_R_SYM(i)     ((i) >> 8)
#define ELF32_R_TYPE(i)    ((unsigned char)(i))
#define ELF32_R_INFO(s, t) (((s) << 8) + (unsigned char)(t))

#define ELF64_R_SYM(i)     ((uint32_t)((i) >> 32))
#define ELF64_R_TYPE(i)    ((uint32_t)(i))
#define ELF64_R_INFO(s, t) (((uint64_t)(s) << 32) + (uint32_t)(t))

#endif /* !LINUX_SCRIPTS_HAVE_SYSTEM_ELF_H */

#endif /* LINUX_SCRIPTS_ELF_H */
