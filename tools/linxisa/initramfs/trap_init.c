/*
 * Minimal trap-only PID1 entry for Linx bring-up.
 *
 * Keep the entry sequence fully assembler-authored so the compiler does not
 * synthesize an FENTRY prologue before the first user instruction. That lets
 * QEMU/Linux validation answer the narrow question we care about here: can the
 * kernel return to user mode and execute the very first instruction at all?
 */
__asm__(
	".globl _start\n"
	"_start:\n"
	"  .byte 0x00, 0x08\n"             /* C.BSTART.STD */ "\n"
	"  .byte 0x2b, 0x10, 0x10, 0x00\n" /* ebreak 0 */ "\n"
	"1:\n"
	"  .byte 0x02, 0x00\n"             /* C.BSTART DIRECT, self */ "\n"
	"  .byte 0x00, 0x00\n"             /* C.BSTOP */ "\n"
);
