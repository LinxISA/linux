# LinxISA ABI notes (compiler + Linux bring-up)

This document captures the **current** LinxISA scalar ABI as implemented by the
bring-up toolchain, and the **Linux boot ABI** used by the QEMU LinxISA `virt`
machine.

## Toolchain ABI profiles (linx32 vs linx64)

The LinxISA toolchain supports two profiles (see the LinxISA ISA manual):

- **linx32**: ILP32 (32-bit `int`, `long`, and pointers)
- **linx64**: LP64 (32-bit `int`, 64-bit `long` and pointers)

For the current Linux bring-up on `qemu-system-linx64`, use **linx64**.
The `arch/linx/` port in this tree is configured as a 64-bit kernel
(`CONFIG_64BIT=y`).

## `/proc/cpuinfo` ISA reporting (Linx v0.3)

The Linx Linux bring-up port reports ISA identity in two fields:

- `isa`: profile name (currently `linx64`)
- `isa_extensions`: enabled extension tokens in canonical order

Current static bring-up output:

- `isa : linx64`
- `isa_extensions : lnx-s32 lnx-s64 lnx-c lnx-f lnx-a lnx-sys lnx-v lnx-m`

`isa_extensions` ordering is fixed to:
`lnx-s32`, `lnx-s64`, `lnx-c`, `lnx-f`, `lnx-a`, `lnx-sys`, `lnx-v`, `lnx-m`.
This is declarative for now (no runtime HWCAP/hwprobe feature probing in this
phase).

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

This boot ABI is validated by the bring-up bootstub in `tools/linxisa/bootstub/`.

## Linux syscall ABI (bring-up)

Current bring-up convention (LP64, QEMU `virt`):

- Trap instruction: `acrc 1` (SERVICE_REQUEST)
- Syscall number: `a7` (R9)
- Args 0..5: `a0..a5` (R2..R7)
- Return value / errno: `a0` (R2), negative errno on error
- Syscall numbers: currently `asm-generic` (`include/asm-generic/unistd.h`)

Known issue (bring-up):

- Syscall return values currently arrive in userspace with a 31-bit signed
  encoding where the sign bit lives in bit 30. The initramfs bring-up shim in
  `tools/linxisa/initramfs/busybox.c` corrects this by sign-extending bit 30
  into bit 31 before interpreting negative errno values. This must be fixed at
  the arch/QEMU boundary before running a real libc/BusyBox userspace.
