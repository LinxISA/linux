/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _ASM_LINX_SIGNAL_H
#define _ASM_LINX_SIGNAL_H

/*
 * LinxISA bring-up: userspace provides a signal restorer/trampoline.
 *
 * This matches common 64-bit ports that use SA_RESTORER for NOMMU/FDPIC
 * environments during early bring-up.
 */
#define SA_RESTORER 0x04000000

#include <asm-generic/signal.h>

#endif /* _ASM_LINX_SIGNAL_H */
