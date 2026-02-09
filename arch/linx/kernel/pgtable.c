// SPDX-License-Identifier: GPL-2.0-only

#include <linux/linkage.h>

#include <asm/pgtable.h>

pgd_t swapper_pg_dir[PTRS_PER_PGD] __page_aligned_bss;
