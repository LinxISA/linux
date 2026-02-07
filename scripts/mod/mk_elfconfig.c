// SPDX-License-Identifier: GPL-2.0
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__has_include)
#if __has_include(<elf.h>)
#include <elf.h>
#else
/* Minimal ELF definitions for hosts without <elf.h> (e.g. macOS). */
#define EI_NIDENT 16
#define ELFMAG "\177ELF"
#define SELFMAG 4
#define EI_CLASS 4
#define ELFCLASS32 1
#define ELFCLASS64 2
#endif
#else
#include <elf.h>
#endif

int
main(int argc, char **argv)
{
	unsigned char ei[EI_NIDENT];

	if (fread(ei, 1, EI_NIDENT, stdin) != EI_NIDENT) {
		fprintf(stderr, "Error: input truncated\n");
		return 1;
	}
	if (memcmp(ei, ELFMAG, SELFMAG) != 0) {
		fprintf(stderr, "Error: not ELF\n");
		return 1;
	}
	switch (ei[EI_CLASS]) {
	case ELFCLASS32:
		printf("#define KERNEL_ELFCLASS ELFCLASS32\n");
		break;
	case ELFCLASS64:
		printf("#define KERNEL_ELFCLASS ELFCLASS64\n");
		break;
	default:
		exit(1);
	}

	return 0;
}
