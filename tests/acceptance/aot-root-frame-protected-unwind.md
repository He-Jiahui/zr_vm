# AOT root-frame recovery across protected throws

Date: 2026-09-29

## Scope

The current C11 `longjmp` implementation of `ZrCore_Exception_TryRun` must
prevent a caught local Throw from leaving callback-local AOT root-frame nodes
linked after their C stack lifetime ends. The restoration boundary is the
root-chain top and depth captured on entry. Throw restores them after status
normalization and before `MutatorUnwindScopes` marks the mutator inactive and
broadcasts, so another mutator cannot begin a stop-the-world scan while
abandoned callback nodes are still linked. The recovery writes the snapshot
directly and never walks those nodes. TryRun catch repeats the assignment
idempotently. This does not change the root-frame ABI or claim recovery for OOM
paths outside the protected `TryRun` boundary.

The forced-C++ Throw path also unlinks callback roots before C++ stack
unwinding invokes destructors. Destructor reentry into VM/GC during that unwind
is not covered by this change. Direct native C++ exceptions that bypass Throw
are repaired only when TryRun catch is reached. The current focused fixture
validates C11 `longjmp`; it does not force a C++ exception build.

## RED

- MSVC `zr_vm_aot_gc_root_frame_test` build: 6/6 steps, exit 0.
- Direct run: 9 tests, 3 failures, 0 ignored. The existing six tests passed;
  the empty-chain, outer-chain, and nested-`TryRun` throw cases each failed
  their saved top check. The fixture captured top bytes/depth, immediately
  restored its valid caller snapshot, skipped GC on imbalance, then cleaned up
  before reporting the failures.

## Implementation

- `TryRun` saves entry root top/depth before installing its recovery point.
- A volatile normal-return flag distinguishes an ordinary return from a
  non-local exit, including `Throw(ZR_THREAD_STATUS_FINE)`.
- On non-local exit it restores the two saved fields by assignment and does not
  dereference or traverse abandoned callback frames. Local `Throw` performs
  this restoration before the GC-domain inactive publication; the catch path
  confirms it idempotently.
- A normal `TryRun` callback still performs ordinary Push/Pop and preserves the
  caller's active outer chain.
- Legacy worker-forward uses `restoreRoots=false` so it cannot mutate the main
  thread's private recovery snapshot from the forwarding worker. Existing
  cross-thread longjmp behavior receives no new guarantee.

## Focused coverage

`tests/core/test_aot_gc_root_frame_exception.inc`, included by
`tests/core/test_aot_gc_root_frame.c`, covers:

- `test_aot_root_frame_tryrun_throw_restores_empty_chain`;
- `test_aot_root_frame_tryrun_throw_fine_restores_empty_chain`, where status
  alone cannot distinguish Throw from normal return;
- `test_aot_root_frame_tryrun_throw_preserves_outer_chain`, where a valid outer
  root keeps a young raw object in the survivor region during minor GC;
- `test_aot_root_frame_nested_tryrun_throw_restores_outer_chain`;
- `test_aot_root_frame_tryrun_normal_push_pop_keeps_outer_chain`.
- `test_aot_root_frame_tryrun_throw_preserves_relocated_outer_frame_base`,
  which combines a callback-local C root and Throw with a forced moving stack
  allocation. It checks the outer `FRAME_BYTE_OFFSET` base against the current
  stack before minor GC, then requires the young object to reach survivor.

Every throw fixture avoids reading an abandoned pointer as a C pointer value.
It compares the state field's object representation with a still-valid caller
snapshot, records depth, and immediately restores both fields before any GC or
cleanup. GC runs only when the chain was already balanced. The tests pop valid
outer frames and destroy their state before Unity assertions.

## Verified focused gates (root)

MSVC 19.44.35228 C11 Debug compiled the frozen current `exception.c` and
current root-frame test as standalone objects, then linked them before the
existing native Core archive and test harness. Both compilation steps and the
link succeeded. The suite passed **12 tests, 0 failures, 0 ignored**, exit 0;
root independently reran the binary with the same result. This is a standalone
check using the cached stack/root implementation, not a full current-Core
CMake or CTest result.

The frozen exception SHA256 was
`eff7b5949d0d4837b793d738f3f8f897637353c118b92db6517723ac6b1ce5f6`.
Root compared the copied exception and test sources with the working files.
The test include was read from the repository through its explicit include
path. The native link map attributes `TryRun` and `Throw` to
`exception_frozen_standalone.obj`, with no archive `exception.c.obj` provider.
Compiler TEMP/TMP, new objects, binary, PDBs, maps and logs were under
`D:/tmp/zr_vm/aot-root-throw-order-gcc`.

GNU 11.4.0 on WSL compiled the current exception, a catch-only variant, and the
unchanged HEAD exception as separate objects. The catch-only variant removed
only the two assignments before `MutatorUnwindScopes`; catch recovery stayed
intact. The HEAD source matched the Git blob byte for byte (SHA256
`519573479583d58b7a00dd79450687148ca6ee4c9e1552d5d3b596e0f9a8c105`).
Each object was linked before the cached GCC Core archive using
`--wrap=ZrCore_GcDomain_MutatorUnwindScopes`. The wrapper observes the actual
entry, then calls the real unwind function. Link maps and symbol traces prove
the respective standalone exception providers and exclude the archive
exception member.

Both empty-chain and outer `LOCAL_ADDRESS` cases enter real mutator and
GC-aware native scopes. Every variant runs the **same required restoration
invariant**:

| Exception implementation | At real unwind entry | After TryRun | Exit |
| --- | --- | --- | --- |
| Current | Saved top/depth restored | Saved top/depth restored | 0 |
| Catch-only | Top/depth still include callback node | Saved top/depth restored | 1 (RED) |
| HEAD | Top/depth still include callback node | Callback node still linked | 1 (RED) |

All six case runs also confirmed RUNNING with nonzero execution/native depths
before real unwind, inactive with zero depths afterward, and successful caller
root cleanup. Root independently reran all three binaries and checked the
logs/maps. The probe restores valid caller fields before cleanup and does not
interpret the ended callback pointer or attempt GC through a failed chain.
This bounded boundary observation is not a concurrent-collector stress test.

Commands used for the final standalone gates and independent reruns:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File D:\tmp\zr_vm\aot-root-throw-order-gcc\msvc_standalone_gate.ps1
& D:\tmp\zr_vm\aot-root-throw-order-gcc\msvc-bin\zr_vm_aot_gc_root_frame_standalone.exe
wsl.exe -e bash /mnt/d/tmp/zr_vm/aot-root-throw-order-gcc/run_gcc_links_only.sh
wsl.exe -e /mnt/d/tmp/zr_vm/aot-root-throw-order-gcc/bin/aot_root_throw_order_final final
wsl.exe -e /mnt/d/tmp/zr_vm/aot-root-throw-order-gcc/bin/aot_root_throw_order_catch_only final
wsl.exe -e /mnt/d/tmp/zr_vm/aot-root-throw-order-gcc/bin/aot_root_throw_order_old final
```

The GCC compile used `-std=c11 -fPIC -g`, Debug/platform definitions and the
Core/common include directories; TMPDIR pointed to the scratch directory on
D. A first runner was interrupted by rewriting its script while WSL was
reading it. Its completed objects were reused by a new immutable links-only
runner. Diagnostic modes that return success upon observing the old bug were
not counted as RED; the final table uses the common `final` invariant.

## Full native Core gate (root)

After the shared Core prerequisite compile/link defects were repaired, the
final MSVC C11 Debug increment built **5/5 steps, exit 0**. This used the
current complete Core archive. Direct root-frame execution passed
**12 tests, 0 failures, 0 ignored**, exit 0 (1.07s). The adjacent legacy
capability-validation binary also exited 0.

CTest executed both registered tests and passed **2/2**:
`aot_gc_root_frame` 1.01s and `ssa_capability_validation` 0.04s;
total reported test time was 1.19s. Earlier shared-Core failures were a missing
ZRAF public-type include, an incorrect diagnostic member name, and an omitted
binding diagnostic helper during extraction. They were not counted as
functional RED evidence for this root-chain fix. The introduced unused local
in the combined fixture was removed before the final native rebuild; the
callback Push result remains explicitly asserted through its context.

```powershell
cmd.exe /d /s /c 'call "E:\Visual Studio\Common7\Tools\VsDevCmd.bat" -no_logo -arch=x64 -host_arch=x64 && set "TEMP=D:\tmp\zr_vm\ssa-artifact-v6-msvc\compiler-tmp" && set "TMP=D:\tmp\zr_vm\ssa-artifact-v6-msvc\compiler-tmp" && cmake --build D:\tmp\zr_vm\ssa-artifact-v6-msvc --target zr_vm_aot_gc_root_frame_test zr_vm_ssa_capability_validation_test -j 2'
& D:\tmp\zr_vm\ssa-artifact-v6-msvc\bin\zr_vm_aot_gc_root_frame_test.exe
& D:\tmp\zr_vm\ssa-artifact-v6-msvc\bin\zr_vm_ssa_capability_validation_test.exe
ctest --test-dir D:\tmp\zr_vm\ssa-artifact-v6-msvc -R '^(aot_gc_root_frame|ssa_capability_validation)$' --output-on-failure
```

## Pending broader gates

- Full GCC and Clang root-frame suite builds/runs. The GNU wrapped probe above
  checks only its two boundary cases.
- The focused fixture is C11; it does not force a C++ compile of the exception
  implementation. The existing C++ catch status mapping was left unchanged.
- Unix generated-AOT shared-library throw smoke. The C fixture exercises real
  `TryRun`, `ZrCore_Gc_AotRootFramePush`, and `ZrCore_Exception_Throw`; it is not
  an emitter-generated AOT integration test.

The source-authoring agent ran no build or commit; the verified gates above
were executed by the root verification workflow.

## Discarded output cleanup

After recording and checking the gate evidence, root removed the standalone
probe scratch: **40 files, 53,264,257 bytes, 7 directories**. Each absolute
target was checked to stay under `D:/tmp/zr_vm/aot-root-throw-order-gcc`, with
reparse points rejected; files and then empty directories were deleted
individually. The scratch directory no longer exists. The historical
standalone commands above refer to those discarded artifacts. The native
CMake cache remains active for subsequent SSA verification.
