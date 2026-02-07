# LinxISA Linux bring-up status

This is a living note capturing the current bring-up status of the LinxISA
Linux port in this tree (`arch/linx/`) when booted under the LinxISA QEMU
`virt` machine (`~/qemu`, `qemu-system-linx64 -machine virt`).

## Toolchain / linker notes

- The LinxISA LLVM integrated assembler must emit a `R_LINX_PCREL_HI20`
  relocation for `ADDTPC` fixups even when the referenced symbol is in the same
  section (link-time layout can still change the page delta).
  - Without this, Linux can crash via a miscomputed local function pointer.

## Kernel build

See `~/qemu/docs/linxisa/kernel-build.md` for the macOS host build commands.

The current bring-up kernel is built as:

- `ARCH=linx` (64-bit, NOMMU)
- ABI profile: `linx64` (LP64) – see `Documentation/linxisa/abi.md`

## Boot status (as of 2026-02-07)

Working:

- Kernel boots on QEMU `virt` and executes initramfs `/init` as PID 1.
- The bring-up initramfs `/init` mounts `/proc` and `/sys` and drops to an
  interactive `# ` prompt on the console.
- Basic bring-up syscalls validated from userspace:
  - `openat`, `read`, `getdents64`, `mount`, `close`
  - `reboot`/`poweroff` (current platform halts rather than powering off)

Repro (initramfs + QEMU):

```bash
cd /Users/zhoubot/linux/tools/linxisa/initramfs
./build.sh

/Users/zhoubot/qemu/build/qemu-system-linx64 \
  -nographic -monitor none -machine virt -m 512M -smp 1 \
  -kernel /Users/zhoubot/linux/build-linx-fixed/vmlinux \
  -initrd /Users/zhoubot/linux/build-linx-fixed/linx-initramfs/initramfs.cpio \
  -append "lpj=1000000 loglevel=1"
```

Once booted, try: `help`, `ls /`, `probe /init`, `cat /no-such`.

Smoke test (kills QEMU after a timeout because the guest currently halts
without triggering a QEMU shutdown request):

```bash
cd /Users/zhoubot/linux
SKIP_BUILD=1 TIMEOUT=25 python3 tools/linxisa/initramfs/smoke.py
```

## Known issues

- **Syscall return sign-extension**: syscall return values currently arrive in
  userspace with a 31-bit signed encoding where the sign bit lives in bit 30.
  The initramfs bring-up shim fixes this by sign-extending bit 30 into bit 31
  before interpreting negative errno values.
- **`/proc` root enumeration**: `ls /proc` currently returns only `.`/`..`,
  while `ls /proc/self` lists expected entries (e.g. `cmdline`, `status`,
  `mounts`).
- **`/sys` directory enumeration**: `ls /sys` currently shows repeated/garbled
  entries (control characters) and appears to loop. Likely a sysfs + `getdents`
  interaction bug (file position / dir offset handling).
- **Bring-up debug noise**:
  - SLUB allocator debug dumps and repeated `abcd12Ss3efg` prints still appear
    during boot.
  - `arch/linx/kernel/traps.c` contains temporary syscall debug printing.

## Linux image size snapshot (LinxISA vs RISC-V)

Measured from this workspace on 2026-02-07:

- LinxISA kernel (`/Users/zhoubot/linux/build-linx-fixed/vmlinux`):
  `7,170,408` bytes.
- LinxISA raw image (`/Users/zhoubot/linux/build-linx-fixed/vmlinux.bin`):
  `3,585,984` bytes.
- LinxISA initramfs (`/Users/zhoubot/linux/build-linx-fixed/linx-initramfs/initramfs.cpio`):
  `34,304` bytes.
- RISC-V kernel (`/Users/zhoubot/linux/build-riscv-virt/vmlinux`):
  `7,101,720` bytes.
- RISC-V raw image (`/Users/zhoubot/linux/build-riscv-virt/arch/riscv/boot/Image`):
  `3,527,716` bytes.

Delta:

- `vmlinux`: LinxISA is `68,688` bytes larger (`+0.97%`) than RISC-V.
- Raw image: LinxISA is `58,268` bytes larger (`+1.65%`) than RISC-V.

## Next steps

1. Fix syscall return sign-extension at the arch/QEMU boundary so real libc and
   BusyBox can run without userspace shims.
2. Root-cause `/proc` root enumeration (procfs mount vs proc root init).
3. Root-cause `/sys` `getdents` looping/corruption.
4. Remove temporary bring-up debug printing and early-boot bypasses.
