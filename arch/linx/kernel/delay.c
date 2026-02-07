// SPDX-License-Identifier: GPL-2.0-only

#include <linux/delay.h>
#include <linux/export.h>

#include <asm/processor.h>

void __delay(unsigned long loops)
{
	while (loops--)
		cpu_relax();
}
EXPORT_SYMBOL(__delay);

void __const_udelay(unsigned long xloops)
{
	unsigned long loops;

	loops = (unsigned long)(((unsigned __int128)xloops * loops_per_jiffy) >> 32);
	__delay(loops);
}
EXPORT_SYMBOL(__const_udelay);

void __udelay(unsigned long usecs)
{
	__const_udelay(usecs * UDELAY_CONST_MULT);
}
EXPORT_SYMBOL(__udelay);

void __ndelay(unsigned long nsecs)
{
	__const_udelay(nsecs * NDELAY_CONST_MULT);
}
EXPORT_SYMBOL(__ndelay);
