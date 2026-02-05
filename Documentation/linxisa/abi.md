# LinxISA ABI notes (compiler + Linux bring-up)

This document captures the **current** LinxISA scalar ABI as implemented by the
bring-up toolchain, and the **Linux boot ABI** used by the QEMU LinxISA `virt`
machine.

## Scalar register ABI (R0..R23)

The scalar GPR file has 24 architectural registers.

| Phys reg | ABI name | Role |
| ---: | --- | --- |
| R0 | `zero` | Constant 0 |
| R1 | `sp` | Stack pointer |
| R2..R9 | `a0..a7` | Argument / return registers |
| R10 | `ra` | Return address |
| R11 | `fp` / `s0` | Frame pointer / callee-saved |
| R12..R19 | `s1..s8` | Callee-saved |
| R20..R23 | `x0..x3` | Caller-managed scratch bank |

Calling convention (C):

- Integer/pointer args: `a0..a7` (R2..R9), then stack.
- Return value: `a0` (R2). (Large aggregates: `sret` pointer.)
- Callee-saved: `s0..s8` (R11..R19).
- Caller-saved: `a*`, `ra`, `x0..x3` (and any other scratch).

Stack:

- Stack grows downward.
- Stack alignment: **16 bytes**.

Endianness:

- Little-endian.

## Linux boot ABI (QEMU LinxISA `virt`)

The QEMU LinxISA `virt` machine boots the kernel with:

- `a0` (R2): hart / cpu id (`0` for now)
- `a1` (R3): physical address of the flattened device tree (FDT/DTB)
- `sp` (R1): initial stack pointer (provided by QEMU)
- `ra` (R10): exit trampoline (kernel should not return)

Kernel images accepted by QEMU `virt`:

- ELF relocatable (`ET_REL`) object (`.o`) with `_start` (legacy bring-up path)
- ELF executable (`ET_EXEC`) or PIE (`ET_DYN`) (recommended for Linux bring-up)

Machine model reference and UART/exit MMIO are documented in
`/Users/zhoubot/qemu/docs/linxisa/README.md`.

