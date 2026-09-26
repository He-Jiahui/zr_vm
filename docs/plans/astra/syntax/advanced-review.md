---
related_code:
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_invoke.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_support.c
  - zr_vm_lib_container/src/zr_vm_lib_container/generational_pool.c
  - zr_vm_lib_container/src/zr_vm_lib_container/pooling_generational_runtime.c
  - zr_vm_core/src/zr_vm_core/task_frame_runtime.c
  - zr_vm_lib_task/src/zr_vm_lib_task/runtime/runtime.c
  - zr_vm_core/src/zr_vm_core/iterator/frame.c
  - zr_vm_core/src/zr_vm_core/iterator/dispatch.c
  - zr_vm_cli/src/zr_vm_cli/testing/test_runner.c
  - zr_vm_parser/src/zr_vm_parser/comptime_contract.c
  - zr_vm_parser/src/zr_vm_parser/compiler/comptime_runtime_contract.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c
implementation_files: []
plan_sources:
  - user: 2026-09-05 review Syntax 08-14, write an evidence-backed advanced review, and repair one bounded functional defect before performance work
  - docs/plans/astra/index.md
  - docs/plans/syntax/2026-07-19-08-reflection-library-type-system-design.md
  - docs/plans/syntax/2026-07-19-09-generational-pool-handle-ref-struct-design.md
  - docs/plans/syntax/2026-07-19-10-native-ffi-module-package-design.md
  - docs/plans/syntax/2026-07-20-11-compile-time-attribute-decorator-typed-generation-design.md
  - docs/plans/syntax/2026-07-20-12-async-task-job-scheduler-design.md
  - docs/plans/syntax/2026-07-20-13-iterator-enumerator-yield-design.md
  - docs/plans/syntax/2026-07-20-14-test-function-harness-design.md
tests:
  - tests/ffi/test_ffi_module.c
  - tests/ffi/test_native_extern_contract.c
  - tests/ffi/test_ffi_native_call_pin_contract.c
  - tests/container/test_generational_pool.c
  - tests/container/test_generational_pool_gc_stress.c
  - tests/container/test_generational_pool_performance_matrix.c
  - tests/task/test_task_job_scheduler.c
  - tests/task/test_task_runtime.c
  - tests/task/test_task_frame_runtime.c
  - tests/parser/test_reflection_type_surface.c
  - tests/parser/test_reflection_type_stress.c
  - tests/compileTime/test_compile_time_execution.c
  - tests/compileTime/test_comptime_contract.c
  - tests/testing/test_runner.c
  - tests/artifact/test_manifest_roundtrip.c
  - tests/acceptance/2026-08-04-syntax-08-m2-reflection-invocation-boundary.md
  - tests/acceptance/2026-08-04-syntax-08-m3-artifact-metadata-graph.md
  - tests/acceptance/2026-08-04-syntax-09-m4-artifact-reflection-lsp.md
  - tests/acceptance/2026-07-25-syntax-12-m3-job-scheduler-runtime.md
  - tests/acceptance/2026-07-25-syntax-12-m4-attached-domain-thread-scheduler.md
  - tests/acceptance/2026-07-25-syntax-13-m3-iterator-frame-runtime.md
doc_type: milestone-detail
---

# Syntax 08-14 Advanced Review

This review covers reflection, generational pooling, native FFI and package resolution, compile-time attributes, Task/Job scheduling, Iterator frames, and TestManifest. The current source tree has substantial focused coverage, but the historical milestone records are not a substitute for a fresh Astra baseline. The records are therefore treated as historical evidence and the current source is checked for lifecycle, ownership, malformed-input, and cross-boundary behavior.

## Review State

The review is **planned**. No source or test result in this document promotes the overall Syntax 08-14 package to green. Syntax 08, 09, 10 and 14 have records claiming focused promotion; Syntax 12 M6.4 still explicitly retains known LSP baseline failures, while Syntax 13's M3 record explicitly excludes compiler lowering, async iteration, artifact, AOT, debug, LSP, and migration. The root Astra plan also records that old build paths are absent and a fresh WSL provenance is required.

## Evidence And Ownership Matrix

| Domain | Current foundation and source evidence | Direct tests and historical status | Boundary gaps to close | Repair owner and gate |
| --- | --- | --- | --- | --- |
| Reflection (08) | `reflection_*` core files, canonical TypeId/metadata records, native descriptor projection. The design says `IdentityOnly/Members/Full` must be distinct and missing state must fail closed. | `tests/parser/test_reflection_type_surface.c`, `test_reflection_type_stress.c`, module reflection/token tests; M1-M5 records claim direct evidence on 2026-08-04. | Replay current artifact reader against truncated/unknown mandatory sections and generation changes; verify no dynamic construction of ref-like descriptors and no stale cache after module reload. | Reflection/library owner; gate 08 M3-M5 plus current GCC/Clang/MSVC focused runs. |
| Pooling (09) | `generational_pool.c` has PoolId/slot/generation state, deferred reclaim, reader/writer guards, and GcFree/Mapped/Barriered scan classes. `pooling_generational_runtime.c` stores native runtime and guard pointers in GC object fields. | `tests/container/test_generational_pool*.c` cover stale/ABA, borrow conflict, partial initialization, deferred Drop, GC scan, churn, and performance; M4/M5 records claim promotion. | Finalizer with active guard returns `POOL_BUSY` at `pooling_generational_runtime.c:258-266`; prove whether GC retries finalization. Add owner-dispose and active guard lifecycle test. Confirm concurrent pool state is not observed without the lock on all read-only APIs. | Container owner. Reserve `pooling_generational_runtime.c` before production edits. Gate 09 M3/M5 and sanitizer run. |
| Native FFI/package (10) | `ffi_runtime_callback.c:3-20` converts every signed/unsigned integer to `double`; `ffi_runtime_callback.c:273-350` then casts that rounded value into each target integer. This is a losslessness violation for 64-bit values above 2^53. `ffi_runtime_invoke.c` uses the helper for ordinary symbol calls. | `tests/ffi/test_ffi_module.c`, `test_native_extern_contract.c`, pin contract, callback and fixture tests; 10F/10C records claim three-toolchain promotion. | Add exact `u64/i64` identity and callback roundtrips around 2^53, signed minimum/maximum, unsigned high bit, and reject float-to-integer precision loss where the contract cannot prove exactness. | FFI owner. Proposed production paths to reserve: `zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c` and `zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_invoke.c`; test-only paths: `tests/ffi/ffi_fixture.c`, `tests/ffi/test_ffi_module.c`, acceptance record. Gate 10F ABI/marshal tests plus sanitizer. |
| Compile-time attributes (11) | `comptime_contract.c`, `compiler/comptime_runtime_contract.c`, and declaration patch writer implement typed facts, transactional patches, cache, and provider imports. The design requires complete type-check before production pruning and atomic rollback on allocation failure. | `tests/compileTime/*`, decorator fixtures, CLI incremental cache tests; M1-M5 records claim promotion. | Replay cache invalidation after provider graph/hash changes; force allocation/error rollback during multi-field patch; verify unknown feature and recursive provider diagnostics preserve source spans. | Compiler owner; no shared production edit reserved by this review. Gate 11 M4/M5 and artifact hash parity. |
| Task/Job (12) | `task_frame_runtime.c` owns frame slots, roots, Drop and result transfer; `zr_vm_lib_task/runtime/runtime.c` uses scheduler object fields and native `ZrVmTaskSchedulerRuntime` pointers. | `tests/task/test_task_runtime.c`, `test_task_job_scheduler.c`, `test_task_frame_runtime.c`; M1-M5 and M6.1-M6.3 records claim focused completion. M6.4 records known LSP baseline failures. | Inspect scheduler runtime pointer finalization and close paths; exercise worker fault, queue abort, shutdown, result owner transfer, repeated await, and process/isolate disposal under sanitizer. Verify no completion can reference a freed scheduler runtime. | Task owner. Production path reservation required before any runtime change. Gate 12 M6 and task runner/isolate baseline separation. |
| Iterator (13) | `iterator/frame.c` roots current values, enforces producer reentrancy, clears current values, and pools terminal frames; `iterator/dispatch.c` rejects terminal/reentrant `MoveNext`. | `tests/parser/test_aot_c_iterator_contracts.c`, AOT iterator smoke, core iterator runtime/GC-drop tests; M1-M3 records claim direct core promotion. | `ZrCore_IteratorFrame_Publish` calls `ZrCore_Value_Copy` without checking allocation/error state before marking `YIELDED`; add allocation-failure and producer-throw cleanup tests. M3 explicitly does not cover compiler lowering, async iterator, artifact, AOT, debug, LSP, or migration. | Iterator/core owner. Reserve `zr_vm_core/src/zr_vm_core/iterator/frame.c` before production edits. Gate M2/M3 plus sanitizer. |
| TestManifest (14) | `zr_vm_cli/testing/test_runner.c` consumes parser entries, expands cases, filters IDs, runs module groups, and applies timeout status after the executor returns. `tests/testing/test_runner.c` is a source-level harness test. | `tests/artifact/test_manifest_roundtrip.c`, `tests/testing/*`, testing reference project and acceptance records; design record claims 219/219 in historical three-toolchain runs. | Runner timeout is measured after executor completion (`test_runner.c:203-210`), so a non-cooperative executor can block indefinitely; require host executor enforcement or mark this as a host policy limitation. Add malformed manifest, duplicate case IDs, empty filter, skip reason, and timeout/crash precedence tests. | CLI/test harness owner. Reserve `zr_vm_cli/src/zr_vm_cli/testing/test_runner.c` before production changes. Gate 14 runner/manifest and process isolation tests. |

## Highest-Priority Functional Defect

The FFI integer path is the first repair candidate because it silently corrupts valid ABI values while returning success. `zr_ffi_extract_numeric_value` in `ffi_runtime_callback.c:3-20` erases the distinction between integer and floating-point values by converting both to `double`. The switch in `zr_ffi_build_scalar_argument` (`:296-350`) then converts the rounded value back to `i64`/`u64`. For `9007199254740993` (`2^53 + 1`), IEEE-754 double stores `9007199254740992`; the native call receives a different integer and no marshal error. The same helper is used by callback return conversion, so the corruption crosses both ordinary symbol calls and callback trampolines.

The repair should preserve integer values in their native signed/unsigned lanes and only use floating conversion for float targets or explicit, range-checked numeric coercion. The API should reject a float source for an integer target when the value is non-finite, fractional, out of range, or not exactly representable. Existing integer-to-narrower conversions also need explicit range checks; silent C casts are not a valid structured FFI contract.

Regression sequence:

1. Extend `tests/ffi/ffi_fixture.c` with exported `u64` and `i64` identity functions.
2. Add source-level calls in `tests/ffi/test_ffi_module.c` for `2^53+1`, `INT64_MIN`, `INT64_MAX`, `UINT64_MAX`, and a callback returning `u64`.
3. Run the focused FFI target before production edits and record the observed mismatch in `tests/acceptance/2026-09-05-astra-syntax-ffi-integer-boundary.md`.
4. Implement the smallest helper change in the reserved FFI files, keeping callback and symbol marshalling on the same exact conversion contract.
5. Re-run focused FFI tests, malformed/range tests, GCC and Clang WSL targets, and the Windows MSVC smoke. Add sanitizer evidence if the conversion change touches storage or callback cleanup.

## Detailed Repair Backlog

### Functional and safety repairs before performance

| Priority | Trigger and source | Required regression | Repair shape | Acceptance |
| --- | --- | --- | --- | --- |
| P0 | 64-bit integer silently rounded through `double` in FFI helper. | Exact `i64/u64` symbol and callback roundtrips; fractional/out-of-range float rejection. | Typed numeric extraction with target-aware range/exactness checks; no feature-name dispatch. | 10F focused matrix, malformed ABI cases, GCC/Clang/MSVC. |
| P1 | Pool object finalizer cannot destroy a pool while a guard remains active and simply returns `POOL_BUSY` (`pooling_generational_runtime.c:258-266`). | Drop pool with active read/write view, release view after GC/finalizer attempt, assert exactly-once element Drop and runtime memory release. | Define retry/deferred finalization contract; never free owner/runtime while guard points into slab. | 09 lifecycle tests plus ASan/UBSan. |
| P1 | Task scheduler stores a heap runtime pointer in an object field (`runtime.c:243-265`) without a visible finalizer/dispose path in the reviewed file. | Dispose scheduler with queued/pending work, then prove no worker signals or completion uses freed mutex/condition/runtime. | Centralize scheduler runtime ownership and shutdown; clear field only after queue/worker join. | 12 shutdown/worker fault tests plus thread sanitizer where available. |
| P1 | Iterator publish marks yielded after an unchecked `ZrCore_Value_Copy` (`iterator/frame.c:96-105`). | Inject copy/root allocation failure during publish; assert frame remains ready/faulted, root and current value cleaned once. | Make copy transactional, release newly created root on failure, set terminal fault with structured status. | M3 runtime and GC-drop tests plus sanitizer. |
| P1 | Test runner timeout is evaluated only after executor returns (`test_runner.c:191-210`). | Executor that blocks beyond timeout; assert bounded host return and `TimedOut` status without waiting forever. | Put timeout enforcement in process/isolate executor boundary; runner remains result aggregator. | 14 runner process-isolation target. |
| P2 | Compile-time cache and declaration patch behavior require failure injection and provider graph invalidation replay. | Provider hash change, recursive import, unknown feature, allocation failure after partial patch. | Preserve immutable snapshot/transaction rollback; invalidate by canonical provider graph hash. | 11 M4/M5 artifact and CLI cache tests. |
| P2 | Reflection generation cache and stripped artifact behavior need current replay. | Module reload generation, missing metadata state, unknown mandatory section, ref-like construction attempt. | Keep cache keyed by canonical module generation and fail closed on missing state. | 08 M3-M5 artifact/reflection matrix. |

### Shared ownership reservations

Before touching production files, send the exact path set to root for reservation. Current proposed reservations are:

- FFI: `zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c`, `zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_invoke.c`.
- Pooling: `zr_vm_lib_container/src/zr_vm_lib_container/pooling_generational_runtime.c`.
- Task: `zr_vm_lib_task/src/zr_vm_lib_task/runtime/runtime.c`, plus any shared task header root identifies.
- Iterator: `zr_vm_core/src/zr_vm_core/iterator/frame.c`.
- Test runner: `zr_vm_cli/src/zr_vm_cli/testing/test_runner.c`.

This review owns documentation and FFI-specific tests. It does not claim ownership of shared production files until root confirms the reservation.

## Performance Recommendations

Performance work starts only after functional repairs have direct regressions. Measurements must preserve the root Astra gates: three or more steady-state samples, coefficient of variation below 5%, at least 3% benefit, and a 95% confidence interval excluding zero. Report cold load, bytecode/interpreter execution, native execution, and allocation/GC work separately.

- FFI: benchmark exact integer marshalling separately from float and aggregate paths. Cache immutable `FfiSignature` conversion metadata, but do not cache a conversion that bypasses range/exactness checks. Report callback setup cost separately from callback invocation cost.
- Pooling: keep `tryBorrow` validation outside the direct field loop; measure slab allocation events, guard transitions, scan bytes, and reclamation latency independently for `GcFree`, `GcMapped`, and `GcBarriered`.
- Task: separate synchronous completion (zero frame allocation), pending await promotion, worker queue latency, and cross-domain transport. Do not fold shutdown or thread creation into steady-state await samples.
- Iterator: report ready/yielded/terminal transitions, pooled frame reuse, current-value root operations, and async pending paths separately. A fast path must not skip root/drop cleanup.
- Reflection: measure TypeId/descriptor cache hits and misses separately, with metadata preservation level recorded. Dynamic invocation must not contaminate static call benchmarks.
- Compile-time/TestManifest: benchmark cache hit, cache invalidation, manifest serialization, discovery, filtering, and test execution separately. Runner parallelism must retain stable output order and explicit process/isolate overhead.

## Reference-Language Evidence To Recheck

The design plans cite Lua/QuickJS for host and coroutine behavior, CPython for import/exception and negative-test breadth, Mono/JDK for metadata/FFI/managed lifecycle, and Rust for ownership and compile-time separation. Before a new semantic change, re-run repository-local evidence searches and record exact implementation plus test files. Relevant checks include Lua `loadlib.c`/`ldo.c`, QuickJS module and callback code, CPython `import.c`/`errors.c`, Mono metadata/marshal/GC tests, JDK class/module loading tests, and Rust UI/codegen ownership regressions. No new syntax should be justified only by a design paragraph when an existing reference tree can provide executable precedent.

## Acceptance Evidence To Produce

Each repair gets its own `tests/acceptance/2026-09-05-astra-<topic>.md` with scope, baseline, exact configure/build/test commands, compiler versions, focused and boundary inventory, tool-assisted evidence, result classification, and remaining known failures. The root Astra integration record must preserve historical failures and distinguish them from newly reproduced defects and repaired regressions.

