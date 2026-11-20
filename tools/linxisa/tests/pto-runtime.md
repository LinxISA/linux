# PTO group manager API

`arch/linx/kernel/pto_runtime.c` supplies admission and lifecycle bookkeeping,
not a device driver or a userspace launch syscall. A registered backend must
validate/pin the descriptor and user mappings, supply four independent contexts
at one entry PC, bind private stacks/TLS/arguments, and propagate target faults.
The context validator must reject overlap with descriptor/state/result/error and
all bootstrap/init storage, in addition to the generic private range checks.

Launch callback success admits all four PEs under a fresh invocation identity;
failure must leave none running and no outstanding request. The backend copies
or retains the launch description until completion. Callbacks execute in process
context under the manager mutex and must not re-enter these APIs synchronously.
The manager is per active Core instance represented by this implementation;
multiple device Cores require separate manager instances, not concurrent reuse
of this singleton.

`pto_runtime_group_finish(context, identity, status)` rejects stale completion
before dereferencing the old context. Backend quiesce must stop all four PEs,
retire/cancel invocation-owned work including unissued TLSU, and wait for older
irrevocable GM work. Then end/abort is called and the next launch may proceed.
End/Abort preserve committed effects and published Shared parents. The backend
owns synchronization generation/release retirement and trap/interrupt restart.

The compatibility `pto_runtime_group_complete` only serves completion-only
managers; it cannot retire a group admitted by the launch API. Manager unregister
returns `-EBUSY` while active, so a module must finish/abort before unloading.
The backend module and all context storage remain valid throughout callbacks.

Run the host manager logic regression:

```sh
python3 tools/linxisa/tests/pto_runtime_manager_test.py
```

This compiles the real C implementation with kernel API shims. It checks range
admission, lifecycle order and identity isolation; it does not test kernel
mutex concurrency, actual PE execution, device drain or a Linux boot image.
No launch backend is currently supplied: admission returns `-EOPNOTSUPP` until
one is registered. This is an explicit platform gap, not emulated success.
