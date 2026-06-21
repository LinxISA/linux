/*
 * Minimal poweroff PID1 for LinxISA Linux bring-up.
 *
 * Keep the startup sequence fully assembler-authored so the compiler does not
 * synthesize an FENTRY prologue before the first user instruction. The emitted
 * bytes are the known-good syscall sequence previously produced by the tiny
 * C variant, minus the unstable prologue.
 */
__asm__(
	".globl _start\n"
	"_start:\n"
	"  .byte 0x00, 0x08\n"                         /* C.BSTART.STD */ "\n"
	"  .byte 0x15, 0x0f, 0x00, 0x02\n"            /* addi zero, 32, ->u */ "\n"
	"  .byte 0xee, 0xfe, 0x97, 0xdf, 0xea, 0x1d\n"/* hl.lui 4276215469, ->t */ "\n"
	"  .byte 0x85, 0x7f, 0xcc, 0x01\n"            /* sll t#1, u#1, ->t */ "\n"
	"  .byte 0x05, 0x53, 0xcc, 0x01\n"            /* srl t#1, u#1, ->a4 */ "\n"
	"  .byte 0x1e, 0x28, 0x97, 0x93, 0x96, 0x21\n"/* hl.lui 672274793, ->a5 */ "\n"
	"  .byte 0x2e, 0x43, 0x17, 0xc4, 0xed, 0x1f\n"/* hl.lui 1126301404, ->a6 */ "\n"
	"  .byte 0x06, 0xa0\n"                        /* c.movr zero, ->x0 */ "\n"
	"  .byte 0x95, 0x0a, 0xe0, 0x08\n"            /* addi zero, 142, ->x1 */ "\n"
	"  .byte 0x86, 0x11\n"                        /* c.movr a4, ->a0 */ "\n"
	"  .byte 0xc6, 0x19\n"                        /* c.movr a5, ->a1 */ "\n"
	"  .byte 0x06, 0x22\n"                        /* c.movr a6, ->a2 */ "\n"
	"  .byte 0x06, 0x2d\n"                        /* c.movr x0, ->a3 */ "\n"
	"  .byte 0x46, 0x4d\n"                        /* c.movr x1, ->a7 */ "\n"
	"  .byte 0x2b, 0x30, 0x10, 0x00\n"            /* acrc */ "\n"
	"  .byte 0x00, 0x00\n"                        /* C.BSTOP */ "\n"
	"  .byte 0x00, 0x08\n"                        /* C.BSTART.STD */ "\n"
	"  .byte 0x86, 0x30\n"                        /* c.movr a0, ->a4 */ "\n"
	"  .byte 0x95, 0x0f, 0xf0, 0x03\n"            /* addi zero, 63, ->t */ "\n"
	"  .byte 0x05, 0x6f, 0x83, 0x01\n"            /* sra a4, t#1, ->u */ "\n"
	"  .byte 0x18, 0x35\n"                        /* c.sub x0, a4, ->t */ "\n"
	"  .byte 0x05, 0x23, 0x8e, 0x07\n"            /* and u#1, t#1, ->a4 */ "\n"
	"  .byte 0x95, 0x03, 0xe0, 0x05\n"            /* addi zero, 94, ->a5 */ "\n"
	"  .byte 0x86, 0x11\n"                        /* c.movr a4, ->a0 */ "\n"
	"  .byte 0x06, 0x1d\n"                        /* c.movr x0, ->a1 */ "\n"
	"  .byte 0x06, 0x25\n"                        /* c.movr x0, ->a2 */ "\n"
	"  .byte 0x06, 0x2d\n"                        /* c.movr x0, ->a3 */ "\n"
	"  .byte 0xc6, 0x49\n"                        /* c.movr a5, ->a7 */ "\n"
	"  .byte 0x2b, 0x30, 0x10, 0x00\n"            /* acrc */ "\n"
	"  .byte 0x00, 0x00\n"                        /* C.BSTOP */ "\n"
	"  .byte 0x00, 0x08\n"                        /* C.BSTART.STD */ "\n"
	"  .byte 0x86, 0x30\n"                        /* c.movr a0, ->a4 */ "\n"
	"1:\n"
	"  .byte 0x02, 0x00\n"                        /* C.BSTART DIRECT, self */ "\n"
	"  .byte 0x00, 0x00\n"                        /* C.BSTOP */ "\n"
);
