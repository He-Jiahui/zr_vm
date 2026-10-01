---
related_code:
  - zr_vm_core/src/zr_vm_core/exception_try_run.c
  - zr_vm_core/src/zr_vm_core/exception.c
  - zr_vm_library/src/zr_vm_library/aot_runtime.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_reference_locals.c
tests:
  - tests/parser/test_aot_c_shared_library_smoke.c
  - tests/parser/test_aot_c_shared_library_throw_root.inc
  - tests/core/test_aot_gc_root_frame.c
  - tests/cmake/aot-generated-root-tests.cmake
plan_sources:
  - docs/plans/ssa/04-frame-native/04-roots-observation.md
doc_type: acceptance-record
status: complete
---

# SSA generated C-AOT throw root restoration

Date: 2026-10-01

## Scope and required observations

`test_aot_c_generated_throw_restores_caller_root_through_full_gc` compiles a
source program, writes a binary module, emits executable C, builds a shared
library and invokes its entry through the strict C-AOT project loader. Flushed
phase markers identify source compilation, AOT emission, external compilation
and `ExecuteEntry`; an earlier-stage failure does not establish a runtime RED.

The source creates a `Marker` instance in an `object` local and calls
`throw_with_live_root`, whose only source-level Throw has string payload
`aot-root-chain-smoke`. The generated entry must directly invoke `zr_aot_fn_1`
after `PrepareStaticDirectCall`, and the generated helper must contain
`ZrLibrary_AotRuntime_Throw`. This establishes the C call path without relying
on native/VM frame flags or absent generated line hooks.

A test-only allocator decorator delegates requests to the original allocator
with their original arguments. The current metadata function must be the
throw helper, and its unique THROW source slot must contain the exact fixture
payload. For this string payload, `NormalizeThrownValue` first checks whether
it is already an Error, then creates the Error with `ZrCore_Object_New`. The
observer latches that first `OBJECT` request before delegation and samples only
after successful allocation. It performs no allocation or collection.

At that point the active generated `LOCAL_ADDRESS` map must point to an object
in the collector list. Both frame and object identities must differ from the
manually registered outer caller root. The observer also records the generated
frame-base identity. These checks require a populated live root, rather than
only generated Push/Pop text.

The unhandled Throw exits through `ExecuteEntry`'s real protected TryRun. The
retained Error's `exception` field must contain the exact payload. After that
non-local exit, the caller compares the root-chain top/depth with its snapshot
without following abandoned C-frame pointers. It restores that known valid
snapshot before any collection or cleanup.

## Checking retention after entry collection

The first qualified GCC/Clang runs reached the intended Throw and passed all
payload, active-map and restored-chain checks. Their final young-survivor
assertion was a fixture error: the outer object was created at age zero, then
normal entry loading/collection aged it to two and promoted it to old storage.
It remained live before and after the following minor GC. Evidence is
`gcc-aot-throw-root-promotion-diagnostic.log` and
`clang-aot-throw-root-promotion-diagnostic.log` under
`D:/tmp/zr_vm/ssa-control`.

An attempted fixture pause with `stopGcFlag` did not prevent explicit
allocation-pressure collection during entry. Both runs still showed the live
outer object promoted at age two; logs are
`{gcc,clang}-aot-root-pause-first-{ctest,last-test}.log`. The stop flag is not
changed in the final fixture.

The final fixture instead completes a real full collection after the Throw.
It verifies both the full-collection kind and a one-step counter increase,
requires the outer root to identify a live object before and after collection,
and requires a newly allocated unrooted control object to be collected. Full
collection tests retention even when the object has already been promoted.
The caller's VM/native frame window is explicitly reset using its saved
call-info and relocatable stack anchor before this GC; that cleanup is outside
the root-chain contract under test. The caller C root must pop normally during
cleanup. The adjacent core suite retains its separate young-root/minor checks.

This gate does not collect while a generated frame is active or prove that GC
consumed or rewrote the generated local slot. The post-throw survivor belongs
to the manually registered caller root. It does not complete packed ExecIR
root maps, LLVM roots, deopt, suspend/native failure or full SSA 04.04.

## Registration and platform

The target is `zr_vm_aot_c_shared_library_smoke_test`; `--throw-root-only` runs
this one case. `tests/cmake/aot-generated-root-tests.cmake` registers
`ssa_aot_generated_throw_root` on Linux with labels `ssa;aot` and timeout 360s.
The CTest expression must match a test, with `--no-tests=error`.

The fixture requires `ZR_PLATFORM_UNIX` and `__linux__`, because its external
compiler/linker command is Linux-specific. Other platforms ignore this Unity
case; an ignored case is not generated-AOT functional evidence. The adjacent
`zr_vm_aot_gc_root_frame_test` runs twelve core root/exception tests on all
three current toolchains.

All builds, generated files and compiler temporaries stay under
`D:/tmp/zr_vm`. WSL uses `/mnt/d/tmp/zr_vm/ssa-artifact-v6-{gcc,clang}` and
`TMPDIR=/mnt/d/tmp/zr_vm/ssa-control/compiler-tmp-{gcc,clang}`. Generated inputs
and outputs are under each cache's
`tests_generated/aot_c_shared_library_throw_root/`. The coordinator removes
abandoned compiled outputs after verification while preserving source/log
evidence and active shared caches.

## Commands and results

The D-owned runner builds the shared-library fixture and adjacent core target,
runs the exact registered CTest, then runs the twelve-case core executable:

```powershell
wsl.exe --exec bash /mnt/d/tmp/zr_vm/ssa-control/run_aot_root_final_gates.sh gcc
wsl.exe --exec bash /mnt/d/tmp/zr_vm/ssa-control/run_aot_root_final_gates.sh clang
```

Final GCC 11.4 and Clang 14 validation passed the registered
`ssa_aot_generated_throw_root` CTest, 1/1 each. The direct logs show all four
phases, the generated Throw failure, a live distinct generated root and the
restored outer root surviving full GC; each Unity case reports 1 test and
0 failures. GCC took 40.66s and Clang 70.72s. Evidence:
`gcc-aot-root-final-{build,ctest,last-test}.log` and
`clang-aot-root-final-{build,ctest,last-test}.log` under
`D:/tmp/zr_vm/ssa-control`.

Windows MSVC 19.44 rebuilt the fixture targets and core root target in 17/17
steps; its direct core root suite passed 12/12, exit 0. Evidence:
`aot-qualified-fixtures-msvc-build.log` and
`aot-root-final-msvc-core.log` under `D:/tmp/zr_vm/ssa-control`. No Windows
shared-AOT execution is claimed.

Earlier bare `Object` and qualified `zr.builtin.Object` source variants failed
before emission; the qualified name appeared inside an inferred function
signature and triggered metadata lookup for `fn(zr`. The current built-in
`object` spelling has compiled, emitted, linked and executed on both GCC and
Clang. The former line-hook assertion also lacked generated hook sites and
was not proof of Throw execution. The phase, direct-call and allocator
observations above replace those unqualified expectations.
