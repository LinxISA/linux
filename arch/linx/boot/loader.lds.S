/* SPDX-License-Identifier: GPL-2.0 */

#include <asm/page.h>
#include <asm/pgtable.h>

OUTPUT_ARCH(linx)
ENTRY(_start)

SECTIONS
{
	. = KERNEL_LINK_ADDR;

	.payload : {
		*(.payload)
		. = ALIGN(8);
	}
}
