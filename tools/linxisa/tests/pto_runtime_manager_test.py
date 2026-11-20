#!/usr/bin/env python3
"""Exercise the actual manager C code on the host with kernel API shims.

This checks admission/lifetime logic, not PE execution, mutex concurrency or
device quiescence. No source transformations or replacement implementation.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
with tempfile.TemporaryDirectory(prefix='pto-manager-') as directory:
    tmp = Path(directory)
    (tmp / 'linux').mkdir()
    headers = {
        'types.h': '#include <stdint.h>\n#include <stdbool.h>\n'
                   '#include <stddef.h>\n#include <assert.h>\n'
                   'typedef uint64_t u64; typedef uint32_t u32; '
                   'typedef uint16_t u16; typedef int32_t s32;\n',
        'compiler.h': '#define READ_ONCE(x) (x)\n#define WRITE_ONCE(x,y) ((x)=(y))\n',
        'errno.h': '#define EINVAL 22\n#define EBUSY 16\n#define ENOENT 2\n'
                   '#define EOPNOTSUPP 95\n#define EOVERFLOW 75\n#define ESTALE 116\n',
        'export.h': '#define EXPORT_SYMBOL_GPL(x)\n',
        'mutex.h': '#define DEFINE_MUTEX(x) int x\n'
                   '#define mutex_lock(x) ((void)(x))\n'
                   '#define mutex_unlock(x) ((void)(x))\n',
        'limits.h': '#define U64_MAX UINT64_MAX\n',
    }
    for name, text in headers.items():
        (tmp / 'linux' / name).write_text(text)
    source = tmp / 'test.c'
    source.write_text(r'''
#include <assert.h>
#include <linux/errno.h>
#include <asm/pto_runtime.h>
static int launches, drains, ends, aborts, sequence, failure;
static u64 launched_id;
static int validate(const struct pto_runtime_group_context *c) { return 0; }
static int launch(const struct pto_runtime_launch *l, u64 id) {
  ++launches; launched_id = id; return failure;
}
static void drain(const struct pto_runtime_group_context *c, u64 id, bool aborting) {
  assert(id == launched_id); assert(sequence == 0); sequence = 1; ++drains;
}
static void end(const struct pto_runtime_group_context *c, int status) {
  assert(sequence == 1); sequence = 0; ++ends;
}
static void abort_group(const struct pto_runtime_group_context *c, int status) {
  assert(sequence == 1); sequence = 0; ++aborts;
}
int main(void) {
  u32 state[4] = {0}, error = 0; s32 result[4] = {0};
  struct pto_runtime_group_context context = {PTO_RUNTIME_GROUP_CONTEXT_MAGIC,
    1, 4, state, result, &error};
  struct pto_runtime_launch l = {.context = &context, .entry = 0x2000};
  struct pto_runtime_manager_ops ops = {.validate = validate, .end = end,
    .abort = abort_group, .launch = launch, .quiesce = drain};
  u64 id = 0, second = 0;
  for (unsigned i = 0; i < 4; ++i) {
    l.pe[i].stack_base = 0x10000 + i * 0x4000;
    l.pe[i].stack_size = 0x1000;
    l.pe[i].stack_pointer = l.pe[i].stack_base + 0x1000;
    l.pe[i].tls_base = l.pe[i].stack_base + 0x2000;
    l.pe[i].tls_size = 0x1000;
    l.pe[i].thread_pointer = l.pe[i].tls_base;
  }
  assert(pto_runtime_group_launch(&l, &id) == -EOPNOTSUPP);
  assert(pto_runtime_register_manager(&ops) == 0);
  assert(pto_runtime_register_manager(&ops) == -EBUSY);
  u64 saved = l.pe[1].tls_base;
  l.pe[1].tls_base = l.pe[0].tls_base;
  l.pe[1].thread_pointer = l.pe[1].tls_base;
  assert(pto_runtime_group_launch(&l, &id) == -EINVAL);
  assert(launches == 0);
  l.pe[1].tls_base = l.pe[1].thread_pointer = saved;
  assert(pto_runtime_group_launch(&l, &id) == 0 && id != 0);
  assert(pto_runtime_group_launch(&l, &second) == -EBUSY);
  assert(pto_runtime_unregister_manager(&ops) == -EBUSY);
  assert(pto_runtime_group_finish(&context, id + 1, 0) == -ESTALE);
  pto_runtime_group_complete(&context, 0);
  assert(drains == 0 && ends == 0);
  assert(pto_runtime_group_finish(&context, id, -5) == 0);
  assert(drains == 1 && aborts == 1 && ends == 0);
  assert(pto_runtime_group_launch(&l, &second) == 0 && second > id);
  assert(pto_runtime_group_finish(&context, id, 0) == -ESTALE);
  assert(drains == 1);
  assert(pto_runtime_group_finish(&context, second, 0) == 0);
  assert(drains == 2 && ends == 1);
  failure = -22;
  assert(pto_runtime_group_launch(&l, &id) == -22);
  assert(pto_runtime_unregister_manager(&ops) == 0);
  assert(pto_runtime_group_finish(&context, second, 0) == -ESTALE);
  return 0;
}
''')
    binary = tmp / 'test'
    subprocess.run(['cc', '-std=gnu11', '-I' + str(tmp),
                    '-I' + str(ROOT / 'arch/linx/include'), str(source),
                    str(ROOT / 'arch/linx/kernel/pto_runtime.c'),
                    '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print('PASS: manager admission, quiescence ordering, stale completion, relaunch')
