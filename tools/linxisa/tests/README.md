# LinxISA bring-up microtests (planned)

This directory is the landing zone for small, reproducible tests used to
validate LinxISA Linux bring-up milestones.

Current state (as of 2026-02-07):

- The bring-up initramfs provides an interactive shell and a smoke test exists:
  `/Users/zhoubot/linux/tools/linxisa/initramfs/smoke.py`.
- A fuller userspace boot validation exists:
  `/Users/zhoubot/linux/tools/linxisa/initramfs/full_boot.py`.
- A virtio disk smoke validation exists:
  `/Users/zhoubot/linux/tools/linxisa/initramfs/virtio_disk_smoke.py`.
- The actual conformance microtests listed in
  `Documentation/linxisa/bringup-plan.md` are not implemented yet.

Run both boot checks:

```bash
cd /Users/zhoubot/linux
python3 tools/linxisa/tests/run.py
```

Run only baseline smoke:

```bash
cd /Users/zhoubot/linux
python3 tools/linxisa/tests/run.py --smoke-only
```

Run smoke + full userspace but skip virtio disk:

```bash
cd /Users/zhoubot/linux
python3 tools/linxisa/tests/run.py --skip-virtio-disk
```

When adding tests here, keep them:

- Small and single-purpose (one bug class per test).
- Runnable under QEMU `virt`.
- Suitable for QEMU-vs-RTL difftest gating (produce deterministic outcomes, or
  define comparison rules).
