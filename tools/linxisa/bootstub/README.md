# LinxISA bootstub (Linux bring-up helper)

This is a tiny freestanding program intended to validate the **LinxISA Linux
boot ABI** used by the QEMU LinxISA `virt` machine:

- `a0`: hart/cpu id
- `a1`: physical address of the flattened device tree (FDT/DTB)

It prints the values and a few parsed DT properties via the QEMU `virt` UART,
then exits by writing to the `virt` exit register.

## Build

Requires the LinxISA clang toolchain (default path assumes your local build):

```bash
make LLVM_BUILD=$HOME/llvm-project/build-linxisa-clang
```

Outputs:

- `build/bootstub.o` (ELF relocatable, runnable via QEMU `-kernel`)

Optional (ET_EXEC image):

```bash
make elf LLVM_BUILD=$HOME/llvm-project/build-linxisa-clang
```

## Run (via QEMU)

From your QEMU tree (see `/Users/zhoubot/qemu/docs/linxisa/README.md`):

```bash
VMLINUX=$HOME/linux/tools/linxisa/bootstub/build/bootstub.o
QEMU_BIN=/Users/zhoubot/qemu/build/qemu-system-linx64

$QEMU_BIN -nographic -monitor none -machine virt -kernel "$VMLINUX"
```
