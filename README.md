# LinxISA Linux Kernel

## Scope
`kernel/linux` is the Linux port used for LinxISA userspace/runtime bring-up, syscall ABI validation, boot flows, and virtfs/9p integration.

## Upstream
- Repository: `https://github.com/LinxISA/linux`
- Merge-back target branch: `main`

## What This Submodule Owns
- Linx architecture port (`arch/linx`)
- Linx trap/syscall/signal entry and return behavior
- Initramfs bring-up and Linux-on-QEMU smoke/full boot tooling

## Canonical Build and Test Commands
Run from `/Users/zhoubot/linx-isa`.

```bash
cd /Users/zhoubot/linx-isa/kernel/linux
CC="/Users/zhoubot/linx-isa/compiler/llvm/build-linxisa-clang/bin/clang --target=linx64-unknown-linux-gnu" \
  make ARCH=linx O=build-linx-fixed linxisa_virt_defconfig
CC="/Users/zhoubot/linx-isa/compiler/llvm/build-linxisa-clang/bin/clang --target=linx64-unknown-linux-gnu" \
  make ARCH=linx O=build-linx-fixed -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc)" vmlinux

python3 /Users/zhoubot/linx-isa/kernel/linux/tools/linxisa/initramfs/smoke.py
python3 /Users/zhoubot/linx-isa/kernel/linux/tools/linxisa/initramfs/full_boot.py
```

## LinxISA Integration Touchpoints
- Kernel boot lane for QEMU runtime tests
- Userspace ABI alignment with LLVM/musl/glibc
- Strict cross-repo gate inputs via `tools/regression/strict_cross_repo.sh`

## Related Docs
- `/Users/zhoubot/linx-isa/docs/project/navigation.md`
- `/Users/zhoubot/linx-isa/docs/bringup/`
- `/Users/zhoubot/linx-isa/emulator/qemu/docs/linxisa/kernel-build.md`
