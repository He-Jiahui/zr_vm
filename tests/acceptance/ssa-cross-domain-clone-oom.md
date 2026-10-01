# SSA cross-domain clone allocation-failure acceptance

The `ssa_cross_domain_clone_oom` runtime test drives a two-object cyclic graph with an aliased child through the public structured-clone transaction API. It interns and roots the target field names before starting a target-domain concurrent major cycle, then wraps that target global's upstream allocator to fail real `ZR_MEMORY_NATIVE_TYPE_HASH_PAIR` requests made while `Object_SetValue` materializes target fields. Both the initial allocation and the post-full-GC retry must fail after target temporary roots have been registered; the test catches the resulting `MEMORY_ERROR` through `ZrCore_Exception_TryRun`.

The two recovery cases require a null destination, unchanged and writable source graph, target temporary-root count restored to its baseline, and a `CLAIMED` transaction with an `ALLOCATION_FAILED` diagnostic. One case performs Abort/Free and creates a fresh clone; the other retries the same worker/epoch claim. Both successful results must preserve the two-object cycle and shared child using independent target addresses. The existing positive cycle/alias test remains the separate success-path gate.

The test reads the target mutator's private mutation depth after `TryRun`. If an OOM longjmp leaves the recursive mutation lock/depth held, it reports the observed status/root count and exits without freeing the affected transaction or states. CTest owns the executable and enforces a 15-second timeout, so a hang or unsafe-cleanup regression cannot strand a test worker.

This private-header gate is registered only with `BUILD_STATIC_LIB=ON` and `BUILD_SHARED_LIB=OFF`.

Build target: `zr_vm_ssa_cross_domain_clone_oom_test`
CTest name: `ssa_cross_domain_clone_oom`

## Observed failure and repair

Before GC scope recovery, the real pair OOM produced `MEMORY_ERROR` (3), mutation
depth `0 -> 1` and target root count `4 -> 8`; the fixture exited before unsafe
cleanup. After the protected-scope fix, mutation depth returned to zero but the
root assertion still failed at `4 -> 8`. These are separate lower-layer and
transaction failures, recorded in `clone-real-red-ctest.log` and
`clone-roots-after-scope-red.log` under `D:/tmp/zr_vm/ssa-control`.

The decoder now keeps acquired handles in its caller-owned context and runs its
allocation callback inside TryRun. It releases object and current field roots
on all callback exits. Commit resets the envelope's in-progress flag before
rethrowing the exact status, so the encoded payload stays available for retry or
abort. The existing protected-scope contract restores the abandoned mutation
lock level.

## Functional validation (2026-10-01)

| Platform/compiler | Build | Clone CTests | Provider Unity | Race Unity |
| --- | --- | --- | --- | --- |
| Windows / MSVC 19.44 | four targets, exit 0 | 2/2; 5 normal + 2 OOM cases | 24/24 | 5/5 |
| WSL / GCC 11.4.0 | four targets, exit 0 | 2/2; 5 normal + 2 OOM cases | 24/24 | 5/5 |
| WSL / Clang 14.0.0 | four targets, exit 0 | 2/2; 5 normal + 2 OOM cases | 24/24 | 5/5 |

Both OOM cases report `roots 4->4 mutation 0->0 status 3 failures 2`. The direct
provider gates also cover ordinary decode failure, partial target release and
root presence before a subsequent allocation. The race gate preserves the
commit/abort terminal-state checks.

All builds, compiler temporary files, logs and executables are on D: under
`D:/tmp/zr_vm/ssa-artifact-v6-{msvc,gcc,clang}` and `D:/tmp/zr_vm/ssa-control`.
Build targets are `zr_vm_ssa_cross_domain_clone_test`,
`zr_vm_ssa_cross_domain_clone_oom_test`,
`zr_vm_resource_cross_domain_transfer_test` and
`zr_vm_resource_cross_domain_transfer_race_test`. CTest uses the exact regex
`^(ssa_cross_domain_clone|ssa_cross_domain_clone_oom)$`; provider suites run their
executables directly. WSL CTest additionally uses `--no-tests=error`.

Evidence names: `clone-atomic-fixed-build.log`,
`clone-atomic-msvc-final-{ctest,last-test}.log`,
`clone-provider{,-races}-msvc-final-direct.log`,
`{gcc,clang}-clone-atomic-final-{build,ctest,last-test}.log` and
`{gcc,clang}-clone-atomic-zr_vm_resource_cross_domain_transfer{,_race}_test-direct.log`.

This gate establishes temporary-root and transaction recovery for target field
allocation OOM. It is not a full native-heap fault sweep or sanitizer result.
`Execute` convenience-call cleanup, source Prepare OOM and provider callback
Throw remain separate checks. SSA 06.04 remains open.
