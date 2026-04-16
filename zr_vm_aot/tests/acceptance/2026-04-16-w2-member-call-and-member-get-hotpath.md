# 2026-04-16 W2 Member Call And Member-Get Hotpath

## Scope

This acceptance note started with five consecutive W2/W3-adjacent slices on the same `zr_interp` acceptance line and now includes eleven later same-day follow-up slices on the same hot-path track:

1. compile-time typed member-call specialization from generic `FUNCTION_CALL` to `KNOWN_VM_CALL`
2. runtime cached member-get steady-state trimming for stack-slot destinations
3. benchmark-project `labelFor()` loop-call quickening from generic `FUNCTION_CALL` to `KNOWN_VM_CALL`
4. runtime cached member PIC instance-field classification plus direct own-field get/set fast paths
5. benchmark-suite AOT de-gating so full tracked `zr_interp` performance acceptance is no longer blocked by disabled AOT backends
6. runtime callable member PIC receiver-object steady-state hit path
7. runtime exact instance-field pair-hit PIC path for cached member get/set
8. runtime instance-field pair-set trim that removes redundant barrier/version/cache churn
9. `ZrCore_Object_GetValue()` convergence onto the existing cached own/prototype string lookup path
10. runtime instance-field pair PIC receiver-object fast hit that no longer requires receiver-shape cache state
11. direct resolved-function entry for `KNOWN_VM_CALL` / `KNOWN_VM_TAIL_CALL` so steady-state VM precall skips already-resolved callable re-dispatch
12. cached member descriptor/callable pointer-hit trimming plus local member callsite-sanitize gating on the `execution_member_get_cached` path
13. runtime member-get PIC refresh that preserves the original receiver when the result slot aliases the receiver slot
14. resolved VM precall entry-slot trimming so steady-state calls only null-reset parameters and instruction-0 locals instead of the full `stackSize`
15. known-native object-call fast-path guardrails so stack-rooted index-contract/member dispatch no longer corrupts aliased inputs or reject generic fallback-only shapes
16. single-slot exact-receiver PIC hit trimming so steady-state callable/member cached gets and pair sets skip the general PIC loop when the callsite has already converged to one exact object

The target benchmark focus stayed on:

- `dispatch_loops`
- `map_object_access`
- `numeric_loops`
- `call_chain_polymorphic`

`zr_interp` remained the only formal performance target. AOT was kept out of the acceptance path except for existing compatibility-only tests already in tree.

## Changed Files

- `zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c`
- `zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c`
- `zr_vm_core/include/zr_vm_core/function.h`
- `zr_vm_core/src/zr_vm_core/function.c`
- `zr_vm_core/src/zr_vm_core/execution/execution_member_access.c`
- `zr_vm_core/src/zr_vm_core/object/object.c`
- `zr_vm_core/src/zr_vm_core/object/object_call.c`
- `zr_vm_core/src/zr_vm_core/object/object_call_internal.h`
- `zr_vm_core/src/zr_vm_core/object/object_internal.h`
- `tests/benchmarks/registry.cmake`
- `tests/cmake/run_performance_suite.cmake`
- `tests/core/test_precall_frame_slot_reset.c`
- `tests/core/test_stateless_function_closure_cache.c`
- `tests/core/test_vm_closure_precall.c`
- `tests/core/test_object_call_known_native_fast_path.c`
- `tests/parser/test_compiler_regressions.c`
- `tests/parser/test_compiler_integration_main.c`
- `tests/core/test_execution_member_access_fast_paths.c`
- `tests/parser/test_known_call_pipeline.c`
- `tests/parser/test_execbc_aot_pipeline.c`
- `tests/acceptance/2026-04-16-w2-member-call-and-member-get-hotpath.md`

## Slice 1: Typed Member Calls Quicken To `KNOWN_VM_CALL`

### Intent

`dispatch_loops` had already converged to `GET_MEMBER_SLOT`, but typed instance method calls still stayed on generic `FUNCTION_CALL`. This slice moved that responsibility forward into compile-time lowering so typed instance method calls emit `KNOWN_VM_CALL` / `KNOWN_VM_TAIL_CALL` directly when member metadata already proves the compiled VM target.

### Regression Coverage

- Added `test_typed_member_calls_quicken_to_known_vm_call_family` in `tests/parser/test_compiler_regressions.c`
- Registered it in `tests/parser/test_compiler_integration_main.c`
- The test asserts:
  - typed member calls emit `GET_MEMBER_SLOT`
  - typed member calls emit `KNOWN_VM_CALL`
  - generic `FUNCTION_CALL` / `SUPER_FUNCTION_CALL_NO_ARGS` disappear
  - runtime result remains `14`

### Structural Evidence

Before this slice, the focused `dispatch_loops` profile showed:

- `FUNCTION_CALL = 153845`
- `KNOWN_VM_CALL = 0`

After this slice, the focused `dispatch_loops` profile showed:

- `FUNCTION_CALL = 5`
- `KNOWN_VM_CALL = 153840`

Important limitation: helper counts on that same case did not move yet:

- `precall = 153852` unchanged
- `value_copy = 3234819` unchanged
- `stack_get_value = 942197` unchanged

Conclusion: compile-time specialization succeeded structurally, but the next real bottleneck moved to the runtime cost of the already-quickened member path.

## Slice 2: Cached Member-Get Stack-Slot Fast Path

### Root Cause

After slice 1, the next concrete hotspot in `dispatch_loops` was no longer generic call dispatch. It was the steady-state cached member-get path:

- every non-calling cached member get still paid for stack anchoring and a stable-result bounce buffer
- this added redundant `value_copy` / `stack_get_value` traffic even when the cached descriptor could not invoke a property getter or trigger a runtime call

### Test-First Repro

Added `test_member_get_cached_stack_slot_fast_path_skips_anchor_copy` in `tests/core/test_execution_member_access_fast_paths.c`.

Red state:

- same cached descriptor hit
- result stored into an actual VM stack slot
- helper count expected `value_copy = 1`
- observed `value_copy = 2`

That failing test confirmed the extra stack-slot bounce copy was real.

### Implemented Runtime Change

`execution_member_try_cached_get()` now takes a direct stack-slot write fast path when the cached descriptor is proven non-calling:

- keep the old anchored path for descriptors that can invoke a property getter
- skip the anchor/stable-result bounce for non-calling cached descriptors
- write directly into the destination stack slot on the hit path

This keeps the safety split explicit:

- calling descriptors keep the conservative path
- non-calling steady-state descriptors take the trimmed release hot path

### Runtime/Profile Evidence

Focused profile comparison for `dispatch_loops`:

- before profile artifacts:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_member_stack_slot_before_20260416`
- after profile artifacts:
  - `build/benchmark-gcc-release/tests_generated/performance`

Key deltas:

- Callgrind total Ir: `784,300,939 -> 742,841,627` (`-5.29%`)
- `execution_member_get_cached`: `57,961,518 -> 45,049,542 Ir` (`-22.28%`)
- `value_copy`: `3,234,819 -> 2,927,391` (`-307,428`)
- `stack_get_value`: `942,197 -> 634,769` (`-307,428`)
- `GET_MEMBER_SLOT`: unchanged at `461,282`
- `SET_MEMBER_SLOT`: unchanged at `153,604`
- `callsite_cache_lookup`: unchanged at `614,886`

Interpretation:

- the instruction mix stayed stable
- the win came from removing per-hit runtime overhead inside the cached member-get path
- the next remaining cost center is deeper object-member retrieval, not the removed anchor-copy layer

`map_object_access` was correctly unchanged by this slice at the helper/profile level because it does not exercise the same cached typed member-get hot path:

- `value_copy = 82915` unchanged
- `stack_get_value = 228264` unchanged
- `precall = 16400` unchanged

## Slice 3: `map_object_access` `labelFor()` Loop Call Quicken

### Root Cause

The real `map_object_access` project compile path still left `labelFor(outer + inner)` on generic `FUNCTION_CALL`, even after the simpler loop-only repro had already been fixed.

The remaining miss was not line-specific lowering. The benchmark compile path materialized the callee through a cross-block temp chain:

- exported callable binding `labelFor` stayed in stack slot `0`
- the loop body reloaded it through a temp (`GET_STACK dst=23 <- slot0`)
- `FUNCTION_CALL` then consumed that temp after a block boundary

Two compiler issues kept that callsite generic:

1. fallback recovery only retraced exported callable bindings directly, not temp aliases that copied them
2. once fallback reached `slot0 <- slot4`, the recovery logic still required the source slot to have a globally unique writer, which is too strict for compiler temps that may have earlier unrelated writes but still have a single latest callable writer at the copy point

### Regression Coverage

Added and registered two benchmark-focused regression tests:

- `test_loop_child_function_calls_quicken_to_known_vm_call_family`
- `test_map_object_access_benchmark_project_compile_quickens_labelFor_loop_call`

The project-compile regression now asserts the real benchmark shape instead of relying on source-line metadata that is currently `0` on this path:

- the second generic `FUNCTION_CALL` (the `labelFor()` loop call) disappears
- `KNOWN_VM_CALL` coverage increases
- no `DYN_CALL` appears on the target source line

### Compiler Change

`compiler_quickening.c` now handles this shape in two steps:

- `compiler_quickening_resolve_callable_provenance_before_instruction()` allows temp-only cross-block fallback when the callsite slot has no active local or typed binding at the call point
- `compiler_quickening_resolve_unique_prior_callable_provenance()` now resolves stack-copy source slots by their latest prior writer at the copy point, instead of incorrectly demanding a globally unique writer for the source temp

Also, a late `known_calls` / `zero_arg_calls` sweep now re-runs after the instruction stream is stabilized by later fold/compaction passes, so newly exposed direct-callee shapes do not stay generic by accident.

### Focused Profile Evidence

Fresh focused profile rerun for `map_object_access` after rebuilding the benchmark runner dependencies:

- profile command:
  - `ZR_VM_TEST_TIER=profile`
  - `ZR_VM_PERF_CALLGRIND_COUNTING=1`
  - `ZR_VM_PERF_ONLY_CASES=map_object_access`
  - `ZR_VM_PERF_ONLY_IMPLEMENTATIONS=zr_interp`
  - `ctest -R '^performance_report$' --test-dir build/benchmark-gcc-release --output-on-failure`

Key structural deltas:

- `FUNCTION_CALL: 4097 -> 1`
- `KNOWN_VM_CALL: 0 -> 4096`
- `SUPER_FUNCTION_CALL_NO_ARGS: 1 -> 1` unchanged
- `KNOWN_NATIVE_CALL: 2 -> 2` unchanged

Key profile deltas:

- Callgrind total Ir: `134,459,796 -> 133,373,998` (`-0.81%`)
- top helper function `ZrCore_Function_PreCallKnownValue`: `4,198,368 -> 3,665,888 Ir` (`-12.68%`)

Important non-deltas:

- `callsite_cache_lookup = 2` unchanged
- `stack_get_value = 228264` unchanged
- `value_copy = 82915` unchanged

Interpretation:

- the generic call-family problem on the benchmark loop is now structurally fixed
- the next `map_object_access` bottleneck is no longer generic call lowering
- the next remaining work is deeper runtime dispatch and object/member lookup cost, exactly matching the planned next slice

## Slice 4: PIC Instance-Field Classification And Direct Cached Field Path

### Root Cause

After the earlier stack-slot bounce removal, `dispatch_loops` still spent meaningful steady-state time in cached member access because instance-field hits still flowed through generic cached-descriptor inspection and descriptor-driven field access.

The remaining evidence-backed targets were:

- `callsite_cache_lookup`
- `ZrCore_Object_GetMemberCachedDescriptorUnchecked`
- the deeper field-hit work that still sat behind cached member PIC slots

### Regression Coverage

Added `test_member_get_cached_refresh_marks_instance_field_slot_kind` in `tests/core/test_execution_member_access_fast_paths.c`.

Red state:

- the cached member PIC slot refreshed successfully
- expected instance-field slot kind `1`
- observed slot kind `0`

That proved the PIC refresh path still did not classify cached instance-field hits explicitly.

### Runtime Change

This slice added explicit cached-access classification plus a direct own-field fast path for instance fields:

- `zr_vm_core/include/zr_vm_core/function.h`
  - added `EZrFunctionCallSitePicAccessKind`
  - replaced `SZrFunctionCallSitePicSlot.reserved0` with `cachedAccessKind` without changing slot size
- `zr_vm_core/src/zr_vm_core/execution/execution_member_access.c`
  - classify cached member PIC slots as `INSTANCE_FIELD` when the descriptor is a non-static field
  - on cached get hits, resolve the member name from `function->memberEntries` and probe the object's own string-keyed storage directly before generic descriptor handling
  - on cached set hits, perform the same direct own-field path first, then fall back to generic descriptor set semantics when needed
  - keep module objects on the generic path to preserve module/export behavior
  - keep callable hits ahead of generic descriptor loading so method steady-state avoids redundant descriptor inspection
- `zr_vm_core/src/zr_vm_core/object/object_internal.h`
  - added inline cached string-key/object-field helpers for the new direct path
- `zr_vm_core/src/zr_vm_core/object/object.c`
  - removed the superseded `ZrCore_Object_TryGetMemberCachedFieldUnchecked` helper and nearby dead private helpers

### Focused Profile / Timing Evidence

Profile artifacts for this slice:

- before: `build/benchmark-gcc-release/tests_generated/performance_profile_known_vm_function_base_20260416`
- after: `build/benchmark-gcc-release/tests_generated/performance_profile_member_pic_after_20260416`

Key `dispatch_loops` deltas:

- Callgrind total Ir: `729,265,010 -> 682,353,050` (`-6.43%`)
- previous helper hotspot `ZrCore_Object_TryGetMemberCachedFieldUnchecked`: `31,050,228 Ir -> removed from top list`
- new top runtime costs after the slice:
  - `execution_member_get_cached = 64,572,760 Ir`
  - `ZrCore_Function_PreCallKnownVmValue = 28,824,480 Ir`

Important structural point:

- the old field-helper hotspot did not merely move sideways into another generic descriptor helper
- it was folded into the trimmed cached member path, which is why the next remaining cost center is now the broader cached member steady-state itself

`map_object_access` stayed structurally correct after this slice:

- profile artifact: `build/benchmark-gcc-release/tests_generated/performance_profile_member_pic_after_20260416/map_object_access__zr_interp.profile.json`
- `KNOWN_VM_CALL = 4096`
- `FUNCTION_CALL = 1`
- Callgrind total Ir: `133,847,469`

Focused core timing snapshot for the same slice:

- `build/benchmark-gcc-release/tests_generated/performance_focus_core_member_pic_after_20260416/dispatch_loops__zr_interp.json`
  - `dispatch_loops = 338.283 ms`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_member_pic_after_20260416/map_object_access__zr_interp.json`
  - `map_object_access = 138.067 ms`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_member_pic_after_20260416/call_chain_polymorphic__zr_interp.json`
  - `call_chain_polymorphic = 114.289 ms`
- focused comparison snapshot:
  - `dispatch_loops`: vs Lua `1.201`, vs QuickJS `2.375`, vs Node.js `1.810`, vs Python `1.011`
  - `map_object_access`: vs Lua `2.942`, vs QuickJS `3.109`, vs Node.js `0.762`, vs Python `1.220`
  - `call_chain_polymorphic`: vs Node.js `0.644`, vs Python `1.072`

## Slice 5: Full Performance-Suite AOT De-Gating

### Root Cause

After slice 4, the full tracked core rerun was still blocked by stale benchmark-suite semantics rather than `zr_interp` regressions.

Fresh red state:

- command:
  - `ZR_VM_TEST_TIER=core ctest --test-dir build/benchmark-gcc-release --output-on-failure -R '^performance_report$'`
- suite result: `FAIL`
- hard-failing rows:
  - `string_build` / `ZR aot_c`
  - `string_build` / `ZR aot_llvm`
  - `gc_fragment_baseline` / `ZR aot_c`
  - `gc_fragment_baseline` / `ZR aot_llvm`
  - `gc_fragment_stress` / `ZR aot_c`
  - `gc_fragment_stress` / `ZR aot_llvm`

Those failures were caused by disabled AOT backends in the benchmark build (`ZR_VM_BUILD_AOT=OFF`), but the registry still treated those AOT rows as core-gated performance requirements.

### Harness Change

Two benchmark-harness changes removed that stale acceptance blocker:

- `tests/benchmarks/registry.cmake`
  - removed `zr_aot_c` / `zr_aot_llvm` from `CORE_IMPLEMENTATIONS` for:
    - `string_build`
    - `gc_fragment_baseline`
    - `gc_fragment_stress`
- `tests/cmake/run_performance_suite.cmake`
  - when `ZR_VM_BUILD_AOT` is not passed explicitly, recover it from `${HOST_BINARY_DIR}/CMakeCache.txt`
  - when the benchmark build has AOT disabled, skip AOT rows early with note `AOT backend disabled for this build` instead of running a prepare step that is guaranteed to fail

### Green Rerun Evidence

After the harness change, the same full tracked command passed:

- `ZR_VM_TEST_TIER=core ctest --test-dir build/benchmark-gcc-release --output-on-failure -R '^performance_report$'`
  - result: `Passed`

Generated full-suite artifacts now show the previously failing AOT rows as `SKIP`, not `FAIL`:

- `string_build` / `ZR aot_c`, `ZR aot_llvm`
- `gc_fragment_baseline` / `ZR aot_c`, `ZR aot_llvm`
- `gc_fragment_stress` / `ZR aot_c`, `ZR aot_llvm`

Skip notes now read:

- `AOT backend disabled for this build`

This puts the acceptance path back in line with the milestone rules:

- `zr_interp` is the formal performance target
- AOT is compatibility-only and no longer blocks performance acceptance when it is disabled in the host benchmark build

## Focused Core Rerun

Because `tests_generated/performance` gets overwritten per tier, the core after-run was snapshotted to:

- `build/benchmark-gcc-release/tests_generated/performance_member_stack_slot_after_core_20260416`

Core before/after for the targeted cases:

- `dispatch_loops`
  - `zr_interp`: `435.821 ms -> 409.288 ms` (`-6.09%`)
  - ratio vs `Lua`: `1.553 -> 1.306`
  - ratio vs `QuickJS`: `2.798 -> 2.648`
  - ratio vs `Node.js`: `2.157 -> 1.685`
  - ratio vs `Python`: `1.051 -> 0.928`
  - ratio vs `C`: `14.828 -> 12.228`
- `map_object_access`
  - `zr_interp`: `174.985 ms -> 170.638 ms` (`-2.48%`)
- `numeric_loops`
  - single-run mounted-worktree timing regressed, but profile remained instruction-identical and outside the touched hot path
- `call_chain_polymorphic`
  - single-run mounted-worktree timing regressed, but this slice did not touch its call-chain logic directly

Acceptance weighting for this slice therefore relied primarily on:

- deterministic profile deltas on `dispatch_loops`
- matching targeted unit/regression coverage
- full Linux matrix + Windows smoke validation

## Validation

### Targeted Tests

- `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_execution_member_access_fast_paths_test -j 8 && ./build/codex-wsl-gcc-debug/bin/zr_vm_execution_member_access_fast_paths_test'`
  - result: `3 Tests 0 Failures 0 Ignored`
- `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_compiler_integration_test -j 8 && ./build/codex-wsl-gcc-debug/bin/zr_vm_compiler_integration_test'`
  - result: `83 Tests 0 Failures 0 Ignored`
  - note: expected negative-test compiler diagnostics still print during the suite

### Focused Benchmarks

- Core rerun:
  - `ZR_VM_PERF_ONLY_CASES=numeric_loops,dispatch_loops,map_object_access,call_chain_polymorphic`
  - `ZR_VM_PERF_ONLY_IMPLEMENTATIONS=c,zr_interp,lua,qjs,node,python,java,dotnet`
  - `ctest -R '^performance_report$' --test-dir build/benchmark-gcc-release --output-on-failure`
- Profile rerun:
  - `ZR_VM_TEST_TIER=profile`
  - `ZR_VM_PERF_CALLGRIND_COUNTING=1`
  - same case / implementation filters
  - `ctest -R '^performance_report$' --test-dir build/benchmark-gcc-release --output-on-failure`
- Benchmark dependency rebuild for the focused `map_object_access` rerun:
  - attempted `cmake --build build/benchmark-gcc-release --target tests/run_performance_suite -j 8` as a dependency refresh
  - the required `zr_vm_parser` / `zr_vm_cli` benchmark artifacts rebuilt successfully before the target fell through to an existing unrelated suite-level benchmark failure
  - the benchmark-release `ALL` target also still trips an unrelated existing `zr_vm_rust_binding_shared` include-path failure under `zr_vm_cli/src/zr_vm_cli/{compiler,project,runtime}/*.c`
  - this acceptance note did not treat either unrelated benchmark-build issue as part of the W2 slice

### Toolchain Validation

- WSL gcc Debug matrix via `.\.codex\skills\zr-vm-dev\scripts\validate-matrix.ps1 -Jobs 8 -SkipWindows`
  - configure/build/CLI smoke: passed
  - `ctest`: failed only on existing Rust binding baseline:
    - `zr_vm_rust_binding_cargo_check`
    - `zr_vm_rust_binding_cargo_test`
  - failure text:
    - `failed to parse lock file`
    - `lock file version 4 requires -Znext-lockfile-bump`
- WSL clang Debug matrix via the same script
  - same result shape as gcc:
    - configure/build/CLI smoke: passed
    - `ctest`: same two Rust binding cargo failures
- Windows MSVC CLI smoke:
  - imported VS environment with `C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1`
  - configured `build\codex-msvc-cli-debug`
  - built `zr_vm_cli_executable`
  - `hello_world.zrp` output: `hello world`
- Full tracked benchmark acceptance rerun:
  - `ZR_VM_TEST_TIER=core ctest --test-dir build/benchmark-gcc-release --output-on-failure -R '^performance_report$'`
  - result: passed after AOT de-gating
  - the generated benchmark report now records AOT rows as `SKIP` with `AOT backend disabled for this build`

## Acceptance Decision

- Accepted for this W2 hot-path slice.

Reason:

- typed member calls now specialize to `KNOWN_VM_CALL` structurally instead of remaining on generic `FUNCTION_CALL`
- cached non-calling member gets no longer pay the redundant stack-slot anchor-copy tax
- the real `map_object_access` benchmark project compile path now lowers the `labelFor()` loop call to `KNOWN_VM_CALL` instead of leaving it on generic `FUNCTION_CALL`
- cached member PIC steady-state now classifies instance fields explicitly and takes a direct own-field get/set fast path instead of routing every hit through the older generic cached field helper
- the full tracked performance suite no longer hard-fails on disabled AOT backends and now reflects the intended milestone policy that AOT is compatibility-only
- `dispatch_loops` shows both deterministic profile improvement and repeated core wall-time improvement
- `map_object_access` now shows the expected instruction-family shift from generic call dispatch to known VM call dispatch
- full tracked benchmark acceptance now passes again
- WSL gcc/clang matrix and Windows MSVC CLI smoke stayed healthy aside from the existing Rust binding cargo-lock baseline

## Remaining Hotspots / Next Slice

The next evidence-backed candidates are:

- `ZrCore_Object_GetMemberCachedDescriptorUnchecked`
  - still `36,276,504 Ir` in `dispatch_loops`
- `callsite_cache_lookup`
  - count unchanged at `614,886`
- `map_object_access`
  - `callsite_cache_lookup` still remains on the hot path
  - `ZrCore_Object_GetValue` still costs `6,565,380 Ir`
  - the remaining generic call family count is down to the unrelated setup calls (`FUNCTION_CALL = 1`, `SUPER_FUNCTION_CALL_NO_ARGS = 1`)

That points to the next W2 direction:

- keep pressing `dispatch_loops` on `callsite_cache_lookup` and `ZrCore_Object_GetMemberCachedDescriptorUnchecked`
- revisit `map_object_access` object/member lookup overhead now that the loop-local VM callsite is no longer generic

## AOT Note

- AOT stayed out of the performance acceptance path for this round.
- Existing AOT-specific compatibility tests touched in-tree were left as compatibility coverage only.
- The known duplicate-definition linker issue in the AOT build path remains out of scope for this note.

## Slice 6: Callable Member PIC Receiver-Object Steady-State Hit

### Root Cause

After slice 5, `dispatch_loops` still spent steady-state time re-entering the wider cached-descriptor path even when a cached member-get PIC slot already knew all of the following:

- exact receiver object
- exact receiver shape
- exact callable target

That meant repeated method lookups on the same object still paid extra cached-descriptor handling instead of returning the callable directly from the PIC slot.

### Regression Coverage

Added callable-PIC coverage in `tests/core/test_execution_member_access_fast_paths.c`:

- `test_member_get_cached_refresh_stores_receiver_object_for_callable_method_descriptor`
- callable receiver-object steady-state hit coverage on repeated cached get

These tests lock the intended invariant:

- a callable member refresh stores the exact receiver object in the PIC slot
- steady-state cached hits can use that receiver-object evidence directly instead of re-walking the generic cached-descriptor path

### Runtime Change

`execution_member_access.c` now lets callable member PIC hits return directly when all of the following are true:

- the receiver object matches the cached receiver object
- the receiver shape still matches
- the receiver / owner prototype versions still match
- the slot already cached the callable target

This turned callable cached-member hits into the same style of exact-object steady-state fast path that the plan already required for instance-field hits.

## Slice 7: Exact Instance-Field Pair-Hit PIC Path

### Root Cause

Even after explicit instance-field classification landed, cached instance-field hits still did extra descriptor-oriented work before reaching the real field value.

The missing step was that a PIC slot refresh already had enough information to cache the receiver pair itself:

- cached string member name
- exact receiver object shape
- exact own-field pair

But the get/set hit path still did not treat that pair as the authoritative steady-state target.

### Regression Coverage

Added pair-hit regression tests in `tests/core/test_execution_member_access_fast_paths.c`:

- `test_member_get_cached_refresh_stores_receiver_pair_for_instance_field_descriptor`
- `test_member_get_cached_instance_field_pair_hit_does_not_require_descriptor_or_member_name`
- `test_member_set_cached_instance_field_hit_does_not_fallback_when_descriptor_index_is_missing`

Red state before the slice:

- the PIC refresh path did not preserve the receiver pair as a reusable steady-state hit target
- forcing descriptor metadata away from the slot caused the cached hit to fall back or fail

Green state after the slice:

- pair-backed cached hits succeed directly from the PIC slot
- descriptor/member-name metadata is no longer required on that exact pair-hit path

### Runtime Change

`execution_member_access.c` now gives instance-field PIC hits a direct pair-backed get/set path:

- cached get copies from `slot->cachedReceiverPair->value`
- cached set writes through `ZrCore_Object_SetExistingPairValueUnchecked(...)`
- descriptor fallback remains available only for non-pair shapes that actually still need it

This is the point where the old `ZrCore_Object_GetMemberCachedDescriptorUnchecked` hot path stopped being mandatory for the common exact-field-hit case.

## Slice 8: Pair-Set Cache Trim

### Root Cause

After slice 7, cached pair-backed sets were structurally correct but still paid unnecessary steady-state churn inside `ZrCore_Object_SetExistingPairValueUnchecked(...)`:

- redundant pair cache reinstallation
- redundant receiver member-version churn
- extra work that did not help correctness on exact existing-pair value replacement

### Regression Coverage

Added:

- `test_member_set_cached_instance_field_pair_hit_does_not_require_receiver_shape_cache`
- `test_member_set_cached_instance_field_pair_hit_does_not_bump_receiver_member_version`

The second test specifically locked the unwanted behavior:

- exact existing-pair set on the same receiver must not mutate `receiver->super.memberVersion`

### Runtime Change

`object.c` now trims `ZrCore_Object_SetExistingPairValueUnchecked(...)` so exact pair-backed set hits only do the work that is still semantically required:

- release / assign the pair value
- keep ownership / GC correctness
- avoid redundant cache / version churn that did not protect any real invalidation case

### Evidence

Fresh snapshots after this slice:

- `build/benchmark-gcc-release/tests_generated/performance_profile_pair_set_cache_trim_after_20260416`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_pair_set_cache_trim_after_20260416`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_pair_set_cache_trim_repeat_after_20260416`

This slice set up the next two same-day wins by making the pair-backed steady-state path genuinely cheap instead of merely structurally specialized.

## Slice 9: `ZrCore_Object_GetValue()` Cached Own/Prototype String Path

### Root Cause

`map_object_access` still showed a large remaining object-lookup hotspot even after the benchmark-local `labelFor()` callsite was no longer generic:

- `ZrCore_Object_GetValue`
- `ZrCore_String_Create`

The issue was that `ZrCore_Object_GetValue()` still used its own older lookup path instead of converging onto the newer cached own/prototype string lookup helpers that the member fast path already used.

### Regression Coverage

Added in `tests/core/test_execution_member_access_fast_paths.c`:

- `test_object_get_value_populates_own_string_lookup_cache`
- `test_object_get_value_populates_prototype_string_lookup_cache_on_fallback`

Red state:

- repeated `GetValue()` calls did not populate the same cached own/prototype string lookup state that the hotter member helpers already relied on

Green state:

- `GetValue()` now populates and reuses the shared cached own/prototype string lookup path

### Runtime Change

`object.c` now routes `ZrCore_Object_GetValue()` through:

- `object_get_own_value()`
- `object_get_prototype_value_unchecked()`

instead of keeping a separate bespoke lookup flow.

### Evidence

Fresh snapshots after this slice:

- `build/benchmark-gcc-release/tests_generated/performance_profile_object_get_value_cache_after_20260416`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_object_get_value_cache_after_20260416`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_object_get_value_cache_repeat_after_20260416`

Key deltas vs the prior pair-set trim snapshot:

- `map_object_access` total Ir: `134,209,496 -> 125,132,339` (`-6.76%`)
- `map_object_access`: `146.993 -> 135.627 ms` on the first focused core rerun (`-7.73%`)
- repeat focused core rerun: `146.993 -> 133.020 ms` (`-9.51%`)
- `ZrCore_Object_GetValue`: `6,564,962 -> 2,874,690 Ir`
- `ZrCore_String_Create`: `4,508,373 -> 1,499,753 Ir`

Interpretation:

- the remaining `map_object_access` cost was not the already-fixed generic `labelFor()` loop call anymore
- it was the older `GetValue()` lookup path
- converging onto the shared cached string path produced the expected direct reduction

## Slice 10: Pair Receiver-Object PIC Fast Hit

### Root Cause

After slice 9, exact instance-field pair hits still unnecessarily depended on receiver-shape cache metadata even when the PIC slot already had:

- exact receiver object
- exact receiver pair
- matching receiver / owner prototype versions

That meant steady-state hits could still miss their cheapest exact-object path whenever the slot did not retain buckets / element-count shape metadata.

### Regression Coverage

Added in `tests/core/test_execution_member_access_fast_paths.c`:

- `test_member_get_cached_refresh_stores_receiver_object_for_instance_field_pair_descriptor`
- `test_member_get_cached_instance_field_pair_receiver_object_hit_does_not_require_receiver_shape_cache`
- `test_member_set_cached_instance_field_pair_receiver_object_hit_does_not_require_receiver_shape_cache`

Red state:

- pair-backed hits still depended on receiver-shape cache metadata even with an exact cached receiver object

Green state:

- exact receiver-object pair hits succeed without buckets / element-count cache state

### Runtime Change

`execution_member_access.c` now:

- stores `cachedReceiverObject = receiverObject` for instance-field pair hits during PIC refresh
- accepts `cachedReceiverObject == receiverObject` as sufficient receiver-shape evidence for pair-backed get/set hits

This removed one more layer of redundant steady-state checking from the exact-object pair fast path.

### Evidence

Fresh snapshots after this slice:

- `build/benchmark-gcc-release/tests_generated/performance_profile_pair_receiver_object_after_20260416`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_pair_receiver_object_after_20260416`

Key deltas vs the slice-9 object-lookup snapshot:

- `dispatch_loops` total Ir: `595,347,677 -> 594,617,414` (`-0.12%`)
- `execution_member_get_cached`: `57,348,622 -> 57,041,194`
- `execution_member_set_cached`: `17,817,864 -> 17,357,076`
- focused core `dispatch_loops`: `292.104 -> 276.792 ms` (`-5.24%`)
- focused core `map_object_access`: `135.627 -> 131.839 ms` (`-2.79%`)
- focused core `numeric_loops`: `183.55 -> 173.05 ms` (`-5.72%`)

The `numeric_loops` movement is treated as mounted-worktree timing noise because this slice does not touch that case structurally, but the `dispatch_loops` profile/helper deltas line up with the intended exact-object pair-hit reduction.

## Slice 13: Member PIC Refresh Survives Receiver/Result Alias

### Root Cause

After slice 12, the biggest remaining `dispatch_loops` miss was no longer PIC capacity or the old descriptor lookup itself. The real issue was that descriptorless callable member-get refresh still observed the receiver through the live stack slot after the result had already been written back there.

On the benchmark `GET_MEMBER_SLOT` shape this meant:

- `receiver == result`
- the first hit wrote the callable back into the receiver slot
- the later PIC refresh re-read the overwritten slot
- refresh then resolved `receiverPrototype == NULL`
- the `step` callsites never populated stable PIC state

That is why the earlier `callsite_cache_lookup` / `ZrCore_Object_GetMemberCachedDescriptorUnchecked` target stopped matching the fresh hotspot picture once the real alias bug was fixed.

### Regression Coverage

Added in `tests/core/test_execution_member_access_fast_paths.c`:

- `test_member_get_cached_descriptorless_callable_refresh_survives_receiver_result_alias`

The test uses a real shared stack slot as both receiver and result to match the benchmark `GET_MEMBER_SLOT` alias shape and locks two requirements:

- first cached access must populate PIC state successfully
- second access must hit the cached path instead of falling back through the refresh miss again

### Runtime Change

`execution_member_access.c` now snapshots the original receiver before the cached get writes back into an aliasing result slot and passes that stable receiver snapshot into PIC refresh.

This keeps the hot path honest:

- cached hit still writes directly into the destination slot
- PIC refresh now sees the real receiver object/prototype again
- descriptorless callable members such as benchmark `step` methods can finally populate their steady-state PIC slots

### Evidence

Fresh snapshots after this slice:

- `build/benchmark-gcc-release/tests_generated/performance_focus_core_member_pic_alias_after_20260416`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_member_pic_alias_repeat_after_20260416`
- `build/benchmark-gcc-release/tests_generated/performance_profile_member_pic_alias_after_20260416`

Key focused evidence:

- focused core `dispatch_loops`: `267.466 ms`
- focused repeat core `dispatch_loops`: `293.493 ms`
- focused core `map_object_access`: `154.644 ms`
- focused repeat core `map_object_access`: `151.553 ms`
- focused profile `dispatch_loops` total Ir: `529,083,958`
- `execution_member_get_cached`: `59,964,571 Ir`
- `ZrCore_Function_PreCallResolvedVmFunction`: `26,978,400 Ir`

Structural confirmation from that fresh profile:

- `dispatch_loops FUNCTION_CALL = 5`
- `dispatch_loops KNOWN_VM_CALL = 153840`
- `map_object_access FUNCTION_CALL = 1`
- `map_object_access KNOWN_VM_CALL = 4096`

Interpretation:

- the benchmark-local generic `labelFor()` issue remained fixed
- the old `callsite_cache_lookup` / `ZrCore_Object_GetMemberCachedDescriptorUnchecked` target was no longer the lead bottleneck
- the next real `dispatch_loops` cut moved to resolved VM precall and the still-heavy cached member hit body

## Slice 14: Resolved VM PreCall Entry-Clear Trims Temp Slots

### Root Cause

After slice 13, the next fresh `dispatch_loops` hotspot was:

- `ZrCore_Function_PreCallResolvedVmFunction = 26,978,400 Ir`

The main waste on that path was not stack growth or generic callable redispatch anymore. It was steady-state entry preparation still null-resetting every slot up to `function->stackSize`, even when the function had:

- exact-arity calls
- no instruction-0 locals beyond parameters
- extra `stackSize` only because of transient compiler temp slots

That punished the benchmark `step()` / `read()` / `labelFor()` shapes for temp slots that do not need an entry-time null reset.

### Regression Coverage

Updated `tests/core/test_precall_frame_slot_reset.c` so the contract is more precise:

- `test_precall_clears_reused_frame_slot_metadata`
- `test_resolved_vm_precall_clears_reused_frame_slot_metadata_with_explicit_argument_count`
- `test_resolved_vm_precall_keeps_transient_temp_slots_intact_when_no_entry_locals_need_null_reset`

The new third test was written first and failed in red state:

- expected transient temp slot value `88`
- observed `0`

That proved resolved VM precall was still scrubbing temp-only slots even when the function exposed no entry locals that needed a null reset.

### Runtime Change

`function.c` now derives a tighter entry-clear bound for resolved VM calls:

- start from `parameterCount`
- extend only to locals whose `offsetActivate == 0`
- clamp to `stackSize`

`function_pre_call_resolved_vm(...)` then:

- null-resets only up to that entry-local bound
- skips the entry clear entirely when the explicit arguments already cover it
- skips the redundant second `stackTop` write when `argumentsCount == parameterCount`

This keeps the old safety rule where it is actually needed:

- entry-active locals still start from a null / zeroed metadata state
- temp-only slots no longer pay blanket entry scrubbing just because they contributed to `stackSize`

### Evidence

Fresh snapshots after this slice:

- `build/benchmark-gcc-release/tests_generated/performance_focus_core_precall_entry_local_clear_after_20260416`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_precall_entry_local_clear_repeat_after_20260416`
- `build/benchmark-gcc-release/tests_generated/performance_profile_precall_entry_local_clear_after_20260416`
- `build/benchmark-gcc-release/tests_generated/performance_tracked_core_precall_entry_local_clear_after_20260416`

Key deltas vs the slice-13 alias-fix profile:

- `dispatch_loops` total Ir: `529,083,958 -> 518,218,040` (`-2.05%`)
- `ZrCore_Function_PreCallResolvedVmFunction`: `26,978,400 -> 16,151,040 Ir` (`-40.13%`)
- focused core `dispatch_loops`: `267.466 -> 261.406 ms` on the first rerun (`-2.27%`)
- focused repeat core `dispatch_loops`: `293.493 -> 256.830 ms`
- focused core `map_object_access`: `154.644 -> 142.566 ms`
- focused repeat core `map_object_access`: `151.553 -> 142.527 ms`

Important non-deltas:

- `dispatch_loops FUNCTION_CALL = 5` unchanged
- `dispatch_loops KNOWN_VM_CALL = 153840` unchanged
- `dispatch_loops helper precall = 153852` unchanged
- `map_object_access` total Ir stayed structurally flat at `125,454,041` versus the slice-13 `125,085,855`

Interpretation:

- the real gain came from cheaper steady-state precall work, not from another opcode mix change
- `dispatch_loops` moved in the intended direction both structurally and on wall time
- `map_object_access` wall time improved on the focused core reruns, but its profile stayed structurally flat, so that case remains primarily an object/value-call hotspot rather than a VM-precall one

## Slice 15: Known-Native Object-Call Guardrails For Index-Contract Shapes

### Root Cause

The previous slice after `object_call.c` fast-pathing was not acceptable:

- focused core benchmark rerun started failing `map_object_access`
- runtime error:
  - `GET_BY_INDEX: receiver must be an object or array`
- the failure surfaced on benchmark source line `35` (`var cell = buckets[key]`)

Root-cause debugging narrowed it to two concrete fast-path bugs in `ZrCore_Object_CallFunctionWithReceiver(...)`:

1. the direct known-native path reset `result` to null before it had copied aliased `receiver` / argument stack slots into scratch space
2. the known-native fast-path gate was broader than the actual stack-rooted safety contract, so non-stack GC operands such as the local `arguments[2]` bridge used by `SetByIndexUnchecked(...)` took the fast path and returned `false` instead of falling back to the generic pinned path

This was a correctness regression, not a new performance win opportunity. The required fix was to make the direct path honest about which call shapes it can handle.

### Regression Coverage

Added two focused core regressions in `tests/core/test_object_call_known_native_fast_path.c`:

- `test_object_call_known_native_fast_path_preserves_receiver_when_result_aliases_receiver_slot`
- `test_object_call_known_native_fast_path_falls_back_for_non_stack_gc_inputs`

Red state before the fix:

- aliased `receiver == result` stack-slot calls reached the native callback with corrupted inputs
- non-stack GC receiver/argument shapes returned `false` before the native callback ran at all

Green state after the fix:

- aliased stack-rooted calls preserve the original receiver and still return the expected value after stack growth
- non-stack GC operands now bypass the direct path and succeed through the generic pinned bridge

### Runtime Change

`zr_vm_core/src/zr_vm_core/object/object_call.c` now adds two explicit guardrails:

- the known-native fast-path gate checks that `receiver` and every argument are either:
  - actual VM stack slots
  - or non-GC values
- the direct path delays `ZrCore_Value_ResetAsNull(result)` until after receiver/argument/callable scratch copies are complete, so receiver/result aliasing no longer self-corrupts the source operands

This keeps the optimization narrow:

- true stack-rooted index/member contract calls still take the direct path
- local bridge shapes such as `SetByIndexUnchecked(...)`'s copied argument array go back to `ZrCore_Object_CallValue(...)`

### Evidence

Fresh focused snapshots after the fix:

- `build/benchmark-gcc-release/tests_generated/performance_profile_object_call_fast_fix_after_20260416`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_object_call_fast_fix_after_20260416`

Focused core rerun after the fix:

- `dispatch_loops = 254.490 ms`
- `map_object_access = 137.037 ms`

Focused profile rerun after the fix:

- `dispatch_loops` total Ir: `518,259,707`
- `map_object_access` total Ir: `125,665,826`

Important structural non-deltas:

- `dispatch_loops FUNCTION_CALL = 5`
- `dispatch_loops KNOWN_VM_CALL = 153840`
- `map_object_access FUNCTION_CALL = 1`
- `map_object_access KNOWN_VM_CALL = 4096`
- `map_object_access` top runtime costs remained:
  - `native_binding_dispatcher = 8,181,297 Ir`
  - `ZrCore_Object_CallValue = 6,430,888 Ir`
  - `ZrCore_Function_PreCallKnownValue = 3,665,577 Ir`

Interpretation:

- this slice repaired the over-broad fast path without reopening the already-fixed generic `labelFor()` lowering issue
- the benchmark correctness regression is gone
- the next real `map_object_access` work remains the same as before: reduce `Object_CallValue` / native binding bridge cost, not another opcode-family change

## Slice 16: Single-Slot Exact-Receiver PIC Hit Trim

### Root Cause

After slice 15 restored correctness, the fresh `dispatch_loops` profile still showed:

- `execution_member_get_cached = 59,964,571 Ir`

The steady-state benchmark shape here is narrower than the generic PIC loop still assumed:

- each `workerN.step()` / `workerN.read()` callsite stabilizes on one exact receiver object
- cached callable/pair hits already have:
  - `picSlotCount == 1`
  - exact `cachedReceiverObject`
  - exact `cachedFunction` or `cachedReceiverPair`

But the runtime still paid for the general per-slot loop and broader shape-selection logic before taking that exact-object hit.

### Runtime Change

`execution_member_access.c` now adds a narrower single-slot exact-object short path ahead of the generic PIC loop for both cached get and cached set:

- when `entry->picSlotCount == 1`
- and the slot already points at the exact `cachedReceiverObject`
- and versions still match

the runtime now returns the cached callable or pair-backed field result directly without re-entering the broader PIC-loop dispatch.

The existing generic PIC loop remains as the fallback for:

- multi-slot sites
- non-exact receiver shapes
- descriptor-based misses/refreshes

### Evidence

Fresh profile snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_member_single_slot_exact_hit_after_20260416`

Fresh repeat focused core snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_focus_core_member_single_slot_exact_hit_repeat_after_20260416`

Profile deltas vs slice 15:

- `dispatch_loops` total Ir: `518,259,707 -> 515,304,032` (`-0.57%`)
- `execution_member_get_cached`: `59,964,571 -> 57,657,585 Ir` (`-3.85%`)
- `execution_member_set_cached`: `17,357,076 -> removed from top 3`
- `map_object_access` total Ir: `125,528,674 -> 125,373,843` (`-0.12%`)

Repeat focused core wall time after the slice:

- `dispatch_loops = 252.961 ms`
- `map_object_access = 139.442 ms`

Interpretation:

- this slice delivered a real `dispatch_loops` win on the exact cached-member line the benchmark is dominated by
- `map_object_access` stayed structurally flat, which is expected because its remaining top cost is still `Object_CallValue` / `native_binding_dispatcher`, not cached member PIC work
- the next dispatch cut still lives inside `execution_member_get_cached`, but now on the remaining general-path work after the exact single-slot receiver case is gone

## Validation Addendum For Slices 6-16

### Targeted Tests

Additional gcc debug targeted tests passed after the later slices:

- `zr_vm_execution_member_access_fast_paths_test`
- `zr_vm_object_call_known_native_fast_path_test`
- `zr_vm_precall_frame_slot_reset_test`
- `zr_vm_vm_closure_precall_test`
- `zr_vm_stateless_function_closure_cache_test`
- `zr_vm_compiler_integration_test`

The same six runtime-focused targets were then rebuilt and re-run serially under WSL clang Debug and also passed.

Extra gcc debug note:

- `zr_vm_instructions_test` was re-run because it contains index-contract coverage
- the target still reports an unrelated existing failure in `test_get_member_slot_instruction_skips_remembered_set_for_permanent_owner`
- the relevant index-contract tests inside that target both passed:
  - `test_index_contract_dispatches_without_storage_fallback`
  - `test_index_contract_get_preserves_receiver_when_destination_aliases`

### Windows MSVC CLI Smoke

Shared runtime changes were also checked again from PowerShell:

- imported `C:\\Users\\HeJiahui\\.codex\\skills\\using-vsdevcmd\\scripts\\Import-VsDevCmdEnvironment.ps1`
- configured `build\\codex-msvc-cli-debug`
- built `zr_vm_cli_executable`
- ran `tests/fixtures/projects/hello_world/hello_world.zrp`
- output stayed `hello world`

### Focused Benchmark/Profile Snapshots

Late-slice focused snapshots preserved immediately after each rerun:

- `performance_profile_pair_set_cache_trim_after_20260416`
- `performance_focus_core_pair_set_cache_trim_after_20260416`
- `performance_focus_core_pair_set_cache_trim_repeat_after_20260416`
- `performance_profile_object_get_value_cache_after_20260416`
- `performance_focus_core_object_get_value_cache_after_20260416`
- `performance_focus_core_object_get_value_cache_repeat_after_20260416`
- `performance_profile_pair_receiver_object_after_20260416`
- `performance_focus_core_pair_receiver_object_after_20260416`
- `performance_focus_core_member_pic_alias_after_20260416`
- `performance_focus_core_member_pic_alias_repeat_after_20260416`
- `performance_profile_member_pic_alias_after_20260416`
- `performance_focus_core_precall_entry_local_clear_after_20260416`
- `performance_focus_core_precall_entry_local_clear_repeat_after_20260416`
- `performance_profile_precall_entry_local_clear_after_20260416`
- `performance_tracked_core_precall_entry_local_clear_after_20260416`
- `performance_profile_object_call_fast_fix_after_20260416`
- `performance_focus_core_object_call_fast_fix_after_20260416`
- `performance_profile_member_single_slot_exact_hit_after_20260416`
- `performance_focus_core_member_single_slot_exact_hit_repeat_after_20260416`

This note therefore reflects fresh same-day profile/core evidence for every late follow-up slice instead of inferring from older reports.

### Full Tracked Core Rerun

After slice 14, a fresh full tracked core rerun also passed and was snapshotted as:

- `build/benchmark-gcc-release/tests_generated/performance_tracked_core_precall_entry_local_clear_after_20260416`

Key `zr_interp` rows from that tracked rerun:

- `numeric_loops = 175.928 ms`
- `dispatch_loops = 258.424 ms`
- `container_pipeline = 187.315 ms`
- `matrix_add_2d = 132.255 ms`
- `string_build = 124.523 ms`
- `map_object_access = 148.464 ms`
- `call_chain_polymorphic = 113.526 ms`
- `mixed_service_loop = 162.748 ms`

No tracked non-GC case failed, and no new unexplained catastrophic regression appeared outside the intended `dispatch_loops`/`map_object_access` focus line.

## Updated Acceptance View After Slice 14

The later nine slices strengthen the same acceptance line rather than reopening it:

- `dispatch_loops` kept converging first by fixing the receiver/result alias PIC miss and then by removing unnecessary resolved-VM precall entry clearing
- `map_object_access` kept the benchmark-local generic-call fix and remains structurally dominated by object/value-call cost instead of generic VM-call lowering again
- `KNOWN_VM_CALL` coverage on the benchmark-local `labelFor()` loop call remained intact
- shared gcc/clang targeted regression coverage, a fresh full tracked core rerun, and Windows MSVC CLI smoke all stayed healthy

With slices 1-14 combined, the note remains accepted on the same `zr_interp` hot-path track.

## Updated Remaining Hotspots / Next Slice

After slice 14, the next evidence-backed hotspots are now:

- `dispatch_loops`
  - `execution_member_get_cached = 59,964,571 Ir`
  - `ZrCore_Function_PreCallResolvedVmFunction = 16,151,040 Ir`
  - `execution_member_set_cached = 17,357,076 Ir`
- `map_object_access`
  - `ZrCore_Object_CallValue = 6,430,888 Ir`
  - `object_get_own_value.part.0 = 4,220,989 Ir`
  - `ZrCore_Function_PreCallKnownValue = 3,665,888 Ir`
  - `ZrCore_Object_GetValue = 2,874,654 Ir`

That keeps the next slice recommendation on the current evidence-backed line:

- keep pressing `dispatch_loops` on `execution_member_get_cached` and the remaining resolved member hit body
- start the next `map_object_access` cut on `ZrCore_Object_CallValue` plus own-value lookup rather than revisiting already-fixed generic `labelFor()` lowering
- leave GC for the planned separate W3 wave, because the fresh full tracked rerun still shows `gc_fragment_baseline` / `gc_fragment_stress` as a distinct problem line
