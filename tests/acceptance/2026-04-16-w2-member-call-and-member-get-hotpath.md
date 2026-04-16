# 2026-04-16 W2 Member Call And Member-Get Hotpath

## Scope

This note reconstructs and continues the same-day W2 hot-path acceptance line for `zr_interp`.
The work stays on the shared interpreter/runtime/compiler path and keeps AOT out of the performance target.

The slice sequence covered here is:

1. typed member calls quicken from generic `FUNCTION_CALL` to `KNOWN_VM_CALL`
2. cached member-get stack-slot hit path trims anchor/copy bounce
3. benchmark `labelFor()` loop call quickens off generic `FUNCTION_CALL`
4. member PIC instance-field classification plus direct own-field get/set path
5. benchmark suite AOT de-gating for `zr_interp`-only performance acceptance
6. callable member PIC receiver-object steady-state hit path
7. exact instance-field pair-hit PIC path
8. pair-set trim removing redundant barrier/version/cache churn
9. `ZrCore_Object_GetValue()` convergence onto cached own/prototype lookup
10. pair-hit receiver-object fast path without receiver-shape cache dependence
11. direct resolved-function entry for `KNOWN_VM_CALL` / `KNOWN_VM_TAIL_CALL`
12. cached member descriptor/callable pointer-hit trimming
13. PIC refresh survives receiver/result aliasing
14. resolved VM precall entry-local clear trim
15. known-native object-call guardrails for aliased/non-stack GC inputs
16. single-slot exact-receiver PIC hit trim
17. native binding inline pinned lane
18. `ZrCore_Object_CallValue()` direct known-precall cut
19. shared known-value no-yield call bridge for `ZrLib_CallValue()` and object-call users

Focused benchmarks remain:

- `dispatch_loops`
- `map_object_access`
- `numeric_loops`
- `call_chain_polymorphic`

## Changed Files

The same acceptance line spans earlier compiler/runtime work plus the later object/native-bridge slices. The active files for the later slices are:

- `zr_vm_core/include/zr_vm_core/function.h`
- `zr_vm_core/src/zr_vm_core/function.c`
- `zr_vm_core/src/zr_vm_core/object/object_call.c`
- `zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c`
- `tests/CMakeLists.txt`
- `tests/core/test_object_call_known_native_fast_path.c`
- `tests/module/test_module_system.c`

Earlier same-day slices also touched:

- `zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c`
- `zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c`
- `zr_vm_core/src/zr_vm_core/execution/execution_member_access.c`
- `zr_vm_core/src/zr_vm_core/object/object.c`
- `tests/core/test_precall_frame_slot_reset.c`
- `tests/core/test_stateless_function_closure_cache.c`
- `tests/core/test_vm_closure_precall.c`
- `tests/core/test_execution_member_access_fast_paths.c`
- `tests/parser/test_compiler_regressions.c`
- `tests/parser/test_compiler_integration_main.c`
- `tests/benchmarks/registry.cmake`
- `tests/cmake/run_performance_suite.cmake`

## Slices 1-16 Recap

The first sixteen slices already moved the interpreter onto the intended W2 line:

- compile-time quickening removed the benchmark-local generic `labelFor()` miss and locked `KNOWN_VM_CALL` coverage for typed member calls
- cached member-get/set PIC work progressively removed redundant anchor/copy/shaping overhead from `dispatch_loops`
- resolved VM precall entry setup was narrowed so temp-only frame slots no longer paid blanket entry clears
- object/member alias bugs and over-broad known-native paths were corrected so later hot-path cuts stayed sound
- by slice 16, `dispatch_loops` had converged to `515,304,032` total Ir and `map_object_access` to `125,373,843` total Ir, with the benchmark-local generic call lowering issue already eliminated

The remaining evidence-backed hotspots after slice 16 were:

- `dispatch_loops`
  - `execution_member_get_cached = 57,657,585 Ir`
  - `execution_member_set_cached = 16,742,692 Ir`
  - `ZrCore_Function_PreCallResolvedVmFunction = 16,151,040 Ir`
- `map_object_access`
  - `ZrCore_Object_CallValue`
  - `native_binding_dispatcher`
  - `ZrCore_Function_PreCallKnownValue`

## Slice 17: Native Binding Inline Pinned Lane

### Root Cause

`map_object_access` still spent too much time inside `native_binding_dispatcher` even after the benchmark-local generic callsite had been fixed.
The remaining path still routed fixed-arity inline native callbacks through the heavier generic stabilized/pinned lane.

### Regression Coverage

`tests/module/test_module_system.c` gained `test_native_binding_inline_label_inspector_keeps_raw_layout_without_losing_gc_safety`.

The test adds:

- a probe-native `inspectLabel()` callback on `NativeDevice`
- a `NativeInspectResult` payload type
- 96 temporary roots plus a forced full GC

It locks two contracts:

- the raw native call layout stays at `functionBase + 2`
- the string argument remains readable after GC stress

### Runtime Change

`native_binding_dispatch.c` added:

- `native_binding_can_use_inline_pinned_lane(...)`
- `native_binding_dispatch_inline_pinned_lane(...)`

That path:

- copies `self` and inline arguments into stable local values
- pins GC-managed operands explicitly
- calls the native callback without falling back to the heavier generic dispatcher scaffolding
- writes back a mutated receiver when needed
- releases pins and ownership copies on exit

### Evidence

Against the slice-16 profile snapshot:

- `map_object_access` total Ir: `125,373,843 -> 119,908,465` (`-4.36%`)
- `dispatch_loops` total Ir: `515,304,032 -> 515,251,177` (flat)

Fresh slice-17 hotspot picture on `map_object_access`:

- `ZrCore_Object_CallValue = 6,430,888 Ir`
- `native_binding_dispatcher = 6,279,483 Ir`
- `ZrCore_Function_PreCallKnownValue = 3,665,595 Ir`

Interpretation:

- the inline pinned lane produced a real `map_object_access` win
- `dispatch_loops` stayed structurally unchanged
- the next remaining map bottleneck moved deeper into object/value-call setup

## Slice 18: `ZrCore_Object_CallValue()` Direct Known-PreCall Cut

### Root Cause

After slice 17, `map_object_access` still showed:

- `ZrCore_Object_CallValue`
- `ZrCore_Function_PreCallKnownValue`

`ZrCore_Object_CallValue()` already had a stable callable snapshot, but its no-yield bridge still re-entered the generic known-value dispatcher instead of branching directly on the stabilized callable family.

### Regression Coverage

`tests/core/test_object_call_known_native_fast_path.c` was tightened so
`test_object_call_known_native_fast_path_accepts_non_stack_gc_inputs` also asserts:

- fallback object-call on a known native callable succeeds
- `helperCounts[ZR_PROFILE_HELPER_PRECALL] == 0`

That locks the intended contract for the narrowed object-call bridge:

- known native object-call fallback may still use the pinned object-call machinery
- it must not re-enter the generic `PreCall` dispatcher

### Runtime Change

`zr_vm_core/src/zr_vm_core/object/object_call.c` added a narrower direct call lane that:

- branches on the already-stabilized `stableCallable`
- goes straight to `ZrCore_Function_PreCallKnownNativeValue()` for known native callables
- goes straight to `ZrCore_Function_PreCallKnownVmValue()` for known VM callables
- falls back to generic `ZrCore_Function_PreCall()` only for true unknown/meta-call shapes

### Evidence

Fresh slice-18 profile vs slice 17:

- `map_object_access` total Ir: `119,908,465 -> 118,979,378` (`-0.77%`)
- `map_object_access helper precall`: `16400 -> 4104` (`-75.00%`)
- top helper function changed from:
  - `ZrCore_Function_PreCallKnownValue = 3,665,595 Ir`
  - to `ZrCore_Function_PreCallKnownNativeValue = 3,603,402 Ir`
- `dispatch_loops` total Ir: `515,251,177 -> 515,266,766` (flat)

Interpretation:

- the direct known-precall split landed structurally
- the generic helper-dispatch count collapsed as intended
- the whole-case gain was smaller than slice 17, but the remaining generic dispatcher work on this line was materially reduced

## Slice 19: Shared Known-Value No-Yield Call Bridge

### Root Cause

Slice 18 proved the direct-known-precall cut inside object-call, but the shared no-yield bridge still used the generic path.

In particular, `ZrLib_CallValue()` still ended with:

- `ZrCore_Function_CallWithoutYieldAndRestore(...)`

That meant the library/native side still paid one generic `PreCall` dispatcher hop per known native call even when the callable family was already stable.

### Test-First Repro

`tests/core/test_object_call_known_native_fast_path.c` gained:

- `test_library_call_value_known_native_path_bypasses_generic_precall_dispatcher`

Red state on WSL gcc:

- expected `helperCounts[ZR_PROFILE_HELPER_PRECALL] == 0`
- observed `1`

That confirmed the shared `ZrLib_CallValue()` bridge was still entering the generic dispatcher.

### Runtime Change

`zr_vm_core/include/zr_vm_core/function.h` and `zr_vm_core/src/zr_vm_core/function.c` now expose:

- `ZrCore_Function_CallWithoutYieldKnownValueAndRestore(...)`
- `ZrCore_Function_CallWithoutYieldKnownValueAndRestoreAnchor(...)`

Internally this adds a shared `known-or-generic` precall split that:

- dispatches directly to `PreCallKnownNativeValue()` / `PreCallKnownVmValue()` when the callee family is already known
- preserves the old generic `PreCall()` path only for true unknown cases

The shared helper now drives both:

- `zr_vm_core/src/zr_vm_core/object/object_call.c`
- `zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c`

which also removes the local duplicate no-yield known-call bridge from `object_call.c`.

### Evidence

Focused profile vs slice 18:

- `map_object_access` total Ir: `118,979,378 -> 119,588,281` (`+0.51%`, mixed)
- `ZrCore_Object_CallValue`: `6,898,136 -> 6,480,072 Ir` (`-6.06%`)
- `ZrCore_Function_PreCallKnownNativeValue`: `3,603,402 -> 3,603,402 Ir` (flat)
- `dispatch_loops` total Ir: `515,266,766 -> 515,311,768` (flat)

Focused 3-iteration core rerun vs slice 18:

- `dispatch_loops`: `295.753 -> 252.799 ms`
- `map_object_access`: `150.143 -> 134.027 ms`

Interpretation:

- the shared bridge cut did reduce the local `ZrCore_Object_CallValue` body
- deterministic whole-case callgrind evidence is mixed versus slice 18, even though wall-clock reruns improved
- the structural generic-dispatch cleanup is still correct and now shared, but this slice does not justify claiming another large `map_object_access` profile win on its own

## Slice 20: Post-Fix Rebaseline

### Context

After slice 19, the shared known-value bridge fix needed one correctness cleanup in the native binding dispatcher before the benchmark line could be trusted again.
Slice 20 is the fresh post-fix baseline used for all later same-day comparisons.

### Evidence

Fresh focused profile:

- `dispatch_loops`
  - total Ir: `515,258,439`
  - `execution_member_get_cached = 57,657,585 Ir`
  - `execution_member_set_cached = 16,742,692 Ir`
  - `ZrCore_Function_PreCallResolvedVmFunction = 16,151,040 Ir`
- `map_object_access`
  - total Ir: `119,562,906`
  - `ZrCore_Object_CallValue = 6,480,072 Ir`
  - `native_binding_dispatcher = 6,246,700 Ir`
  - `ZrCore_Function_PreCallKnownNativeValue = 3,603,402 Ir`

Focused 3-iteration core wall:

- `dispatch_loops = 272.173 ms`
- `map_object_access = 141.338 ms`

Interpretation:

- this is the corrected same-day baseline for slices 21-23
- `dispatch_loops` still had its largest uncovered cost in cached member get/set plus resolved VM precall
- `map_object_access` had already moved off the benchmark-local generic `labelFor()` concern and was now dominated by index-contract object/native call overhead

## Slice 21: Exact Receiver Pair-Hit PIC Path

### Root Cause

`dispatch_loops` steady-state traffic is dominated by repeated `this.state` field reads and writes inside a hot method body.
Even after earlier PIC work, the cached member get/set path still re-validated prototype/version state before exploiting the fact that the callsite kept hitting the exact same receiver object and hash pair.

### Regression Coverage

`tests/core/test_execution_member_access_fast_paths.c` gained:

- `test_member_get_cached_exact_receiver_pair_hit_ignores_cached_version_mismatch`
- `test_member_set_cached_exact_receiver_pair_hit_ignores_cached_version_mismatch`

These lock the intended contract:

- exact receiver-object plus cached instance-field pair hits may return before prototype/version revalidation
- the fast path remains limited to cached instance-field PIC slots and does not weaken generic cache invalidation

### Runtime Change

`zr_vm_core/src/zr_vm_core/execution/execution_member_access.c` and `zr_vm_core/src/zr_vm_core/object/object.c` now:

- detect exact receiver-object + instance-field pair hits up front
- return/set through the cached pair before shape/version checks on that lane
- use `ZrCore_Object_SetExistingPairValueUnchecked(...)` for the setter hit so plain no-ownership values avoid the heavier generic write path

### Evidence

Focused profile vs slice 20:

- `dispatch_loops` total Ir: `515,258,439 -> 510,819,758` (`-0.86%`)
- `execution_member_get_cached`: `57,657,585 -> 53,815,325 Ir` (`-6.66%`)
- `execution_member_set_cached`: `16,742,692 -> 14,899,540 Ir` (`-11.01%`)
- `ZrCore_Function_PreCallResolvedVmFunction`: unchanged at `16,151,040 Ir`
- `map_object_access`: `119,562,906 -> 120,012,840 Ir` (flat/noise)

Interpretation:

- the exact receiver pair-hit cut produced a real deterministic `dispatch_loops` win
- the next uncovered `dispatch_loops` hotspot immediately concentrated into resolved VM precall setup
- `map_object_access` remained structurally unchanged, so the next worthwhile cut there still lived in the object/native call bridge

## Slice 22: Resolved VM Precall Entry-Clear Cache

### Root Cause

`ZrCore_Function_PreCallResolvedVmFunction()` still recomputed its entry local clear span by scanning `localVariableList` on every resolved VM call, even though the answer is function-constant.
After slice 21, that helper had become the next evidence-backed `dispatch_loops` hotspot.

### Regression Coverage

`tests/core/test_precall_frame_slot_reset.c` now locks the cached entry-clear span for both direct and resolved VM precall cases:

- `test_precall_clears_reused_frame_slot_metadata`
- `test_resolved_vm_precall_clears_reused_frame_slot_metadata_with_explicit_argument_count`
- `test_resolved_vm_precall_keeps_transient_temp_slots_intact_when_no_entry_locals_need_null_reset`

### Runtime Change

`zr_vm_core/include/zr_vm_core/function.h` and `zr_vm_core/src/zr_vm_core/function.c` now cache the resolved entry-clear span in:

- `SZrFunction::vmEntryClearStackSizePlusOne`

Resolved VM precall now reuses that cached span instead of re-scanning local metadata on every call.

### Evidence

Focused profile vs slice 21:

- `dispatch_loops` total Ir: `510,819,758 -> 506,556,232` (`-0.83%`)
- `ZrCore_Function_PreCallResolvedVmFunction`: `16,151,040 -> 11,845,924 Ir` (`-26.65%`)
- `execution_member_get_cached`: flat at `53,815,325 Ir`
- `execution_member_set_cached`: flat at `14,899,540 Ir`
- `map_object_access`: `120,012,840 -> 119,536,413 Ir` (noise / unchanged)

Focused core on this slice produced one noisy outlier run, so acceptance for slice 22 is based primarily on callgrind plus the targeted gcc/clang regression suite rather than that first wall-clock sample.

Interpretation:

- the precall cache cut landed structurally and removed the intended steady-state helper work
- `dispatch_loops` now keeps the same member-get/set shape as slice 21 but with materially cheaper resolved VM call setup
- the next map-side slice still needed to attack object/native call overhead, not `labelFor()` lowering

## Slice 23: Pinned Known-Native Fast Bridge For Index Contracts

### Root Cause

`map_object_access` was no longer held back by a generic `labelFor()` loop call.
The remaining steady-state cost lived in `GET_BY_INDEX` / `SET_BY_INDEX` calling native index-contract handlers through `ZrCore_Object_CallValue()`.

The direct reason was that the execution path stabilizes `receiver` / `key` into local C-stack values before entering `ZrCore_Object_GetByIndexUnchecked()` / `ZrCore_Object_SetByIndexUnchecked()`.
The existing narrow known-native bridge only accepted stack-rooted operands, so these stabilized GC-managed operands fell back to the heavier generic object-call path.

### Regression Coverage

`tests/core/test_object_call_known_native_fast_path.c` now covers both stack-rooted and non-stack GC-managed index-contract operands:

- `test_object_call_known_native_fast_path_accepts_non_stack_gc_inputs`
- `test_get_by_index_known_native_fast_path_accepts_non_stack_gc_inputs`
- `test_set_by_index_known_native_fast_path_accepts_non_stack_gc_inputs`

These run under a poisoning allocator and forced stack growth so the tests prove:

- non-stack GC-managed receiver/key values stay valid across growth
- the narrow known-native bridge can consume pinned stable copies
- index-contract get/set entry points preserve the intended receiver/key/value call layout

### Runtime Change

`zr_vm_core/src/zr_vm_core/object/object_call.c` now:

- replaces the stack-only operand preparation helpers with a unified fast-operand path
- anchors VM-stack operands when available
- otherwise pins GC-managed stable copies long enough to move them into scratch stack slots
- routes known native object/index contract calls through `object_call_known_native_fast(...)`

This keeps the narrow bridge available even when `GET_BY_INDEX` / `SET_BY_INDEX` stabilized operands are no longer physically on the VM stack.

### Evidence

Focused profile vs slice 22:

- `map_object_access` total Ir: `119,536,413 -> 114,622,657` (`-4.11%`)
- `ZrCore_Object_CallValue`: dropped out of the top hotspot list
- new top map functions:
  - `native_binding_dispatcher = 6,246,700 Ir`
  - `object_call_known_native_fast.part.0 = 5,594,764 Ir`
  - `ZrCore_Function_PreCallKnownNativeValue = 3,603,402 Ir`
- `dispatch_loops` total Ir: `506,556,232 -> 506,535,522` (flat)

Focused 3-iteration core wall on slice 23:

- `dispatch_loops`: `255.452 ms` (`259.002, 255.122, 252.233`)
- `map_object_access`: `138.420 ms` (`139.732, 133.676, 141.851`)

Interpretation:

- the map hot path finally moved off `ZrCore_Object_CallValue()`
- the win is specific to the native index-contract object-call bridge and leaves `dispatch_loops` unchanged, as intended
- `map_object_access` is now again slightly ahead of the corrected slice-20 wall baseline while also showing a cleaner deterministic callgrind profile

## Slice 24: State-Backed Helper Recording Diagnostic

### Root Cause

`dispatch_loops` still carried a large `__tls_get_addr` cost on the member PIC exact-hit path.
The next cut switched the hottest member/object helper recording sites from TLS-backed
`RecordHelperCurrent(...)` calls to state-backed helper recording so exact receiver/pair hits no longer needed TLS lookup in steady-state.

### Test-First Coverage

`tests/core/test_execution_member_access_fast_paths.c` gained red-first coverage for the state-backed contract:

- `test_member_get_cached_exact_receiver_pair_hit_records_helpers_from_state_without_tls_current`
- `test_member_set_cached_exact_receiver_pair_hit_records_helpers_from_state_without_tls_current`
- `test_object_get_member_cached_descriptor_records_helpers_from_state_without_tls_current`

The red state reproduced with helper counts still at `0`, proving the hot paths were only recording through TLS.

### Runtime Change

The first version of the cut:

- added state-backed helper recording on exact instance-field pair get/set hits
- moved cached descriptor/callable get paths onto state-backed helper recording
- trimmed a redundant `cachedStringLookupPair` refresh in `ZrCore_Object_SetExistingPairValueUnchecked(...)`

### Diagnostic Evidence

Focused profile vs slice 23:

- `dispatch_loops __tls_get_addr`: `9,594,060 -> 372,564 Ir` (`-96.12%`)
- `dispatch_loops` total Ir: `506,535,522 -> 506,708,500` (`+0.03%`, flat-to-worse)
- new replacement hotspot:
  - `ZrCore_Profile_FromState = 8,607,088 Ir`

Interpretation:

- the direction was correct; TLS cost really moved out of the hot path
- but the first state-backed version replaced that cost with a non-inlined `ZrCore_Profile_FromState(...)` call
- slice 24 therefore stayed diagnostic only and did not qualify as the accepted cut

## Slice 25: Inlined State-Profile Runtime Access

### Root Cause

Slice 24 proved that the real target was not TLS itself but the helper-runtime lookup mechanism.
The remaining regression came from routing hot helper recording through the exported `ZrCore_Profile_FromState(...)` function.

### Runtime Change

`zr_vm_core/src/zr_vm_core/execution/execution_member_access.c` and
`zr_vm_core/src/zr_vm_core/object/object.c` now keep the state-backed helper lookup local to the hot files:

- inline helper-runtime access directly from `state->global->profileRuntime`
- local profiled value-copy helpers reuse `ZrCore_Value_CopyNoProfile(...)`
- exact pair-hit get/set and cached descriptor/member get paths no longer bounce through `ZrCore_Profile_FromState(...)`
- `ZrCore_Object_SetExistingPairValueUnchecked(...)` keeps the redundant cached-string refresh trimmed

### Evidence

Focused profile vs slice 23:

- `dispatch_loops` total Ir: `506,535,522 -> 492,544,014` (`-2.76%`)
- `execution_member_get_cached`: `53,815,325 -> 51,816,689 Ir` (`-3.71%`)
- `__tls_get_addr`: `9,594,060 -> 372,564 Ir` (`-96.12%`)
- `ZrCore_Profile_FromState`: removed from the annotate hot list
- `ZrCore_Object_SetExistingPairValueUnchecked`: `12,594,872 -> 12,287,680 Ir` (`-2.44%`)
- `map_object_access` total Ir: `114,622,657 -> 114,797,280` (`+0.15%`, flat / noise)

Focused core wall on slice 25 showed one noisy first rerun (`dispatch_loops = 293.115 ms`),
so acceptance uses the repeat core sample:

- slice 25 repeat `dispatch_loops`: `261.553 ms` (`276.578, 254.880, 253.201`)
- slice 25 repeat `map_object_access`: `137.424 ms` (`139.711, 131.765, 140.796`)

Interpretation:

- the accepted dispatch-side win is real in deterministic callgrind
- the first core rerun was noisy, but the repeat sample settled back near slice-23 wall time while preserving the profile gain
- `map_object_access` stays structurally unchanged, so the next high-value cut should move back to the map-side native bridge/dispatcher line

## Slice 26: Stable Plain-Value Copy For Native Inline Pinned Lane

### Root Cause

After slice 25, `map_object_access` was still dominated by:

- `native_binding_dispatcher`
- `object_call_known_native_fast.part.0`
- `ZrCore_Function_PreCallKnownNativeValue`

The inline pinned lane was still stabilizing plain object/string/int operands through the heavier
ownership-copy path, which paid `Value_Copy` plus release work even when the value already had
plain non-owning semantics.

### Regression Coverage

`tests/core/test_object_call_known_native_fast_path.c` gained:

- `test_native_binding_prepare_stable_value_reuses_plain_heap_object_without_release`
- `test_native_binding_prepare_stable_value_clones_struct_and_marks_release`

These lock the intended split:

- plain heap object / string / scalar values may reuse a shallow stable copy without release work
- struct-backed values must still clone and release through the old ownership path

### Runtime Change

`zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c` now routes inline pinned
lane stabilization through:

- `native_binding_prepare_stable_value(...)`
- `native_binding_release_stable_value(...)`

That change keeps the old safe path for owned / struct values, but lets plain values skip
redundant ownership assign/release churn.

### Evidence

Focused profile vs slice 25:

- `dispatch_loops` total Ir: `492,544,014 -> 492,586,083` (`+0.01%`, flat)
- `map_object_access` total Ir: `114,797,280 -> 113,572,917` (`-1.07%`)
- `native_binding_dispatcher`: `6,279,491 Ir` (flat)
- `object_call_known_native_fast.part.0`: `5,594,764 Ir` (flat)
- `ZrCore_Function_PreCallKnownNativeValue`: `3,603,402 Ir` (flat)

Focused core wall sample for `zr_interp`:

- `dispatch_loops = 261.321 ms`
- `map_object_access = 139.113 ms`

Interpretation:

- the slice is real, but the gain came from removing ownership-copy overhead under the inline pinned lane
- it did not materially reduce the top-level native bridge / dispatcher bodies yet
- the next cut still needed to move closer to lookup / dispatch setup itself

## Slice 27: Two-Slot MRU Native Registry Binding Lookup Cache

### Root Cause

Slice 26 still left `native_registry_find_binding(...)` at `2,176,569 Ir` inside
`map_object_access`, even though the hot loop was repeatedly hitting the same small set of native
closures.

That lookup still linearly scanned the registry on every native binding dispatch.

### Test-First Repro

`tests/core/test_object_call_known_native_fast_path.c` first went red with:

- `test_native_registry_find_binding_promotes_hot_closures_into_two_slot_cache`

The test locks:

- first-hit promotion into slot 0
- second-hot closure shifting the previous one into slot 1
- MRU promotion on repeated lookup

### Runtime Change

`zr_vm_library/src/zr_vm_library/native_binding/native_binding_internal.h` now adds:

- `ZR_LIBRARY_NATIVE_BINDING_LOOKUP_CACHE_CAPACITY = 2`
- `ZR_LIBRARY_NATIVE_BINDING_LOOKUP_CACHE_INVALID_INDEX`
- `ZrLibrary_NativeRegistryState::bindingLookupHotIndices[...]`

`zr_vm_library/src/zr_vm_library/native_binding/native_binding.c` initializes those cache slots on
attach, and `zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c` now:

- probes the two hot indices first
- promotes hits with simple MRU ordering
- falls back to one linear scan only on cache miss
- back-fills the hot slots after the miss resolves

### Evidence

Focused profile vs slice 26:

- `dispatch_loops` total Ir: `492,586,083 -> 492,524,942` (`-0.01%`, flat)
- `map_object_access` total Ir: `113,572,917 -> 112,214,027` (`-1.20%`)
- `native_registry_find_binding`: `2,176,569 -> 373,411 Ir` (`-82.84%`)
- `native_binding_dispatcher`: `6,279,491 Ir` (flat)
- `object_call_known_native_fast.part.0`: `5,594,764 Ir` (flat)
- `ZrCore_Function_PreCallKnownNativeValue`: `3,603,402 Ir` (flat)

Focused core wall sample for `zr_interp`:

- `dispatch_loops`: `261.321 -> 255.040 ms`
- `map_object_access`: `139.113 -> 136.828 ms`

Interpretation:

- the two-slot MRU cache is an effective slice and materially collapses registry lookup cost
- the whole-case `map_object_access` win is modest because the dominant remaining costs are now the
  bridge bodies themselves, not lookup
- the next evidence-backed target is stack-anchor / scratch-layout churn inside
  `native_binding_dispatcher` and `object_call_known_native_fast`, not the registry scan

## Slice 28: Known-Native Object-Call CallInfo Anchor Churn Trim

### Root Cause

Slice 27 made the remaining `map_object_access` bridge costs clearer:

- `native_binding_dispatcher = 6,279,491 Ir`
- `object_call_known_native_fast.part.0 = 5,594,764 Ir`
- `ZrCore_Function_StackAnchorInit = 1,678,733 Ir`
- `ZrCore_Function_StackAnchorRestore = 922,329 Ir`

Inside `object_call_known_native_fast(...)`, every receiver/argument/callable stack copy still
restored the outer `callInfo` `functionBase/functionTop/returnDestination` through separate anchors,
even though `ZrCore_Stack_GrowTo(...)` already relocates those pointers for all live call infos.

The only outer-frame state this path still truly needed to restore explicitly was the caller's
original `functionTop`, because the fast path temporarily widens it to cover the scratch window.

### Regression Coverage

`tests/core/test_object_call_known_native_fast_path.c` gained:

- `test_object_call_known_native_fast_path_restores_outer_frame_bounds_after_growth`

The new guard forces stack growth while the outer frame keeps:

- a reserved `returnDestination`
- `functionTop > stackTop`
- stack-rooted receiver / argument / result slots

It locks the contract that the fast path may widen the caller frame transiently for scratch space,
but must restore the caller's original frame bounds after the native call returns.

### Runtime Change

`zr_vm_core/src/zr_vm_core/object/object_call.c` now narrows
`object_call_known_native_fast(...)` so it keeps only the anchors that still carry real value on
this path:

- `savedStackTop`
- scratch `base`
- `result`
- call base for the native precall
- the caller's original `functionTop`

The fast path now:

- stops re-restoring outer `callInfo->functionBase`
- stops re-restoring outer `callInfo->returnDestination`
- stops maintaining the extra "active top" anchor on every scratch copy
- relies on stack growth's existing call-info relocation for those live pointers
- restores the caller's original `functionTop` once on exit so scratch growth does not leak into
  the steady-state frame shape

### Evidence

Focused profile vs slice 27:

- `dispatch_loops` total Ir: `492,524,942 -> 492,568,073` (`+0.01%`, flat)
- `map_object_access` total Ir: `112,214,027 -> 108,666,690` (`-3.16%`)
- `native_binding_dispatcher`: `6,279,491 -> 5,959,795 Ir` (`-5.09%`)
- `object_call_known_native_fast.part.0`: `5,594,764 -> 4,418,428 Ir` (`-21.03%`)
- `ZrCore_Function_PreCallKnownNativeValue`: `3,603,402 -> 3,603,402 Ir` (flat)
- `ZrCore_Function_StackAnchorInit`: `1,678,733 -> 1,420,517 Ir` (`-15.38%`)
- `ZrCore_Function_StackAnchorRestore`: `922,329 -> 565,737 Ir` (`-38.66%`)
- `ZrCore_Stack_SavePointerAsOffset`: `1,057,664 -> 910,112 Ir` (`-13.95%`)
- `ZrCore_Stack_LoadOffsetToPointer`: `1,262,556 -> 787,100 Ir` (`-37.66%`)

Focused core wall sample for `zr_interp`:

- `dispatch_loops`: `255.040 -> 256.243 ms` (`+0.47%`, noise / flat)
- `map_object_access`: `136.828 -> 134.037 ms` (`-2.04%`)

Interpretation:

- the slice hit the intended target: map-side wins came from cutting scratch/call-info anchor churn,
  not from another registry or generic-call change
- `object_call_known_native_fast` materially shrank, and the supporting stack relocation helpers
  moved down with it
- `dispatch_loops` stayed flat, which is acceptable for this map-only cut
- `ZrCore_Function_PreCallKnownNativeValue` and `native_binding_init_call_context_layout(...)` are
  now the clearer remaining bridge-side bottlenecks

## Validation

### Targeted Tests

WSL gcc Debug:

- `zr_vm_object_call_known_native_fast_path_test`: earlier red state reproduced for the two-slot cache, current green state `11 Tests 0 Failures`
- `zr_vm_precall_frame_slot_reset_test`: `3 Tests 0 Failures`
- `zr_vm_execution_member_access_fast_paths_test`: `31 Tests 0 Failures`
- `zr_vm_value_copy_fast_paths_test`: `3 Tests 0 Failures`

WSL clang Debug:

- `zr_vm_object_call_known_native_fast_path_test`: `11 Tests 0 Failures`
- `zr_vm_precall_frame_slot_reset_test`: `3 Tests 0 Failures`
- `zr_vm_execution_member_access_fast_paths_test`: `31 Tests 0 Failures`
- `zr_vm_value_copy_fast_paths_test`: `3 Tests 0 Failures`

### Windows MSVC CLI Smoke

PowerShell / MSVC Debug CLI smoke after slices 23-28:

- imported `Import-VsDevCmdEnvironment.ps1`
- rebuilt `build\codex-msvc-cli-debug` target `zr_vm_cli_executable`
- ran `tests/fixtures/projects/hello_world/hello_world.zrp`
- output remained `hello world`

### Benchmark Artifacts

Focused same-day snapshots kept on disk:

- `build/benchmark-gcc-release/tests_generated/performance_profile_dispatch_map_slice17`
- `build/benchmark-gcc-release/tests_generated/performance_profile_dispatch_map_slice18`
- `build/benchmark-gcc-release/tests_generated/performance_profile_dispatch_map_slice19`
- `build/benchmark-gcc-release/tests_generated/performance_profile_dispatch_map_slice20`
- `build/benchmark-gcc-release/tests_generated/performance_profile_dispatch_map_slice21`
- `build/benchmark-gcc-release/tests_generated/performance_profile_dispatch_map_slice22`
- `build/benchmark-gcc-release/tests_generated/performance_profile_dispatch_map_slice23`
- `build/benchmark-gcc-release/tests_generated/performance_profile_dispatch_map_slice24`
- `build/benchmark-gcc-release/tests_generated/performance_profile_dispatch_map_slice25`
- `build/benchmark-gcc-release/tests_generated/performance_profile_dispatch_map_slice26`
- `build/benchmark-gcc-release/tests_generated/performance_profile_dispatch_map_slice27`
- `build/benchmark-gcc-release/tests_generated/performance_profile_dispatch_map_slice28`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_dispatch_map_slice18`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_dispatch_map_slice19`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_dispatch_map_slice20`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_dispatch_map_slice21`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_dispatch_map_slice22`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_dispatch_map_slice22_repeat`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_dispatch_map_slice23`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_dispatch_map_slice24`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_dispatch_map_slice25`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_dispatch_map_slice25_repeat`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_dispatch_map_slice26`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_dispatch_map_slice27`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_dispatch_map_slice28`

## Current Hotspots / Next Slice

Fresh hotspot view after slice 28:

- `dispatch_loops`
  - `execution_member_get_cached = 51,816,689 Ir`
  - `execution_member_set_cached = 13,977,956 Ir`
  - `ZrCore_Function_PreCallResolvedVmFunction = 11,845,924 Ir`
- `map_object_access`
  - `native_binding_dispatcher = 5,959,795 Ir`
  - `object_call_known_native_fast.part.0 = 4,418,428 Ir`
  - `ZrCore_Function_PreCallKnownNativeValue = 3,603,402 Ir`
  - supporting setup costs now surfaced more clearly:
    - `ZrCore_Function_StackAnchorInit = 1,420,517 Ir`
    - `ZrCore_Function_StackAnchorRestore = 565,737 Ir`
    - `ZrCore_Stack_SavePointerAsOffset = 910,112 Ir`
    - `ZrCore_Stack_LoadOffsetToPointer = 787,100 Ir`
    - `native_binding_init_call_context_layout = 405,801 Ir`
    - `native_binding_prepare_stable_value = 393,472 Ir`

Recommended next slice:

- keep the benchmark-local generic `labelFor()` concern closed; it stays off the hotspot list
- treat `native_registry_find_binding()` as converged enough for now after the `-82.84%` drop
- on `map_object_access`, move one layer deeper into `native_binding_dispatcher`:
  `native_binding_init_call_context_layout(...)`, rooted object/value setup, and any remaining
  bridge-only stack relocation churn are now worth more than further cuts in `object_call_known_native_fast`
- `dispatch_loops` remains open, but its next useful slice should be evidence-backed inside
  `execution_member_get_cached` / `execution_member_set_cached`, not a regression to earlier generic-call concerns

## Open Notes

- the inline pinned lane now carries an explicit local `argumentCount` bound guard, and the gcc
  benchmark-release rebuild no longer emits the previous `-Wstringop-overflow` false positive on
  `argumentPinAdded[index]`

## Slice 29: Temp-Root Helper Rollback, Keep Only `slotAnchor` Trim

### Root Cause

The first slice-29 draft tried to push more work into the high-level temp-root helpers:

- `native_binding_begin_rooted_object(...)`
- `native_binding_begin_rooted_value(...)`
- `native_binding_begin_rooted_field_key(...)`
- `ZrLib_Object_{SetFieldCString,GetFieldCString}(...)`
- `ZrLib_Array_PushValue(...)`

That draft reused rooted slots directly and duplicated direct-slot probing above
`ZrLib_TempValueRoot_Begin()`. It produced a better callgrind number on `map_object_access`, but the
same-day focused wall regressed badly:

- `dispatch_loops`: `256.243 -> 304.776 ms`
- `map_object_access`: `134.037 -> 142.717 ms`

The draft was therefore not acceptable. Re-auditing the change isolated one safe low-level cut:
`native_binding_temp_root_try_begin_direct_slot()` no longer needed a second
`ZrCore_Function_StackAnchorInit()` for `root->slotAnchor`; it can reuse the already-initialized
`savedStackTopAnchor`.

### Regression Coverage

`tests/container/test_temp_value_root.c` keeps the focused regression for this line:

- `test_object_field_cstring_helpers_restore_existing_function_top_without_growth`

### Implementation

The accepted slice-29 state is intentionally narrow:

- keep `root->slotAnchor = root->savedStackTopAnchor;` inside
  `native_binding_temp_root_try_begin_direct_slot(...)`
- roll back the draft slice-29 high-level rooted-helper specialization so object/value/field-key
  helpers return to the shared `ZrLib_TempValueRoot_Begin()` / `Set*()` / `Value()` flow
- keep the draft slice-29 and intermediate refined artifacts on disk only as diagnostics; they are
  not accepted performance evidence

### Focused Benchmarks

Draft slice-29 diagnostic result versus slice 28:

- profile / callgrind:
  - `dispatch_loops`: `492,568,073 -> 491,893,663 Ir` (`-0.14%`)
  - `map_object_access`: `108,666,690 -> 106,107,538 Ir` (`-2.35%`)
- focused core wall:
  - `dispatch_loops`: `256.243 -> 304.776 ms`
  - `map_object_access`: `134.037 -> 142.717 ms`

Accepted slice-29 anchor-only result versus slice 28:

- profile / callgrind:
  - `dispatch_loops`: `492,568,073 -> 493,972,744 Ir` (`+0.29%`, mixed)
  - `map_object_access`: `108,666,690 -> 110,845,695 Ir` (`+2.00%`, mixed)
  - `ZrCore_Function_StackAnchorInit`: `1,420,517 -> 1,214,857 Ir` (`-14.48%`)
  - `ZrCore_Function_StackAnchorRestore`: `565,737 -> 565,737 Ir` (flat)
- focused core wall sample:
  - `dispatch_loops`: `256.243 -> 246.934 ms`
  - `map_object_access`: `134.037 -> 136.097 ms` (mixed / noise)
- focused core wall repeat:
  - `dispatch_loops`: `256.243 -> 253.836 ms`
  - `map_object_access`: `134.037 -> 132.594 ms`

Interpretation:

- the high-level rooted-helper specialization looked attractive in instruction-counting mode but was
  the wrong tradeoff for steady-state runtime, so it was rolled back
- the low-level `slotAnchor` trim is safe to keep and repeatedly preserves or improves focused wall
  without reopening the earlier function-top / temp-root correctness issues
- this slice should be treated as a stability recovery plus one small retained cut, not as a large
  new map-side structural win

### Validation

WSL gcc Debug:

- `zr_vm_container_temp_value_root_test`: `3 Tests 0 Failures`
- `zr_vm_object_call_known_native_fast_path_test`: `11 Tests 0 Failures`
- `zr_vm_execution_member_access_fast_paths_test`: `31 Tests 0 Failures`
- `zr_vm_value_copy_fast_paths_test`: `3 Tests 0 Failures`
- `zr_vm_precall_frame_slot_reset_test`: `3 Tests 0 Failures`

WSL clang Debug:

- `zr_vm_container_temp_value_root_test`: `3 Tests 0 Failures`
- `zr_vm_object_call_known_native_fast_path_test`: `11 Tests 0 Failures`
- `zr_vm_execution_member_access_fast_paths_test`: `31 Tests 0 Failures`
- `zr_vm_value_copy_fast_paths_test`: `3 Tests 0 Failures`
- `zr_vm_precall_frame_slot_reset_test`: `3 Tests 0 Failures`

Windows MSVC Debug CLI smoke after the final slice-29 rollback/retain decision:

- rebuilt `build\codex-msvc-cli-debug` target `zr_vm_cli_executable`
- ran `tests/fixtures/projects/hello_world/hello_world.zrp`
- output remained `hello world`

### Benchmark Artifacts

Slice-29 diagnostic and final focused snapshots kept on disk:

- `build/benchmark-gcc-release/tests_generated/performance_profile_dispatch_map_slice29`
- `build/benchmark-gcc-release/tests_generated/performance_profile_dispatch_map_slice29_refined`
- `build/benchmark-gcc-release/tests_generated/performance_profile_dispatch_map_slice29_anchor_only`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_dispatch_map_slice29`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_dispatch_map_slice29_refined`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_dispatch_map_slice29_anchor_only`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_dispatch_map_slice29_anchor_only_repeat`

## Current Hotspots / Next Slice (After Slice 29)

Fresh hotspot view after the accepted slice-29 rollback/retain state:

- `dispatch_loops`
  - `execution_member_get_cached = 51,816,689 Ir`
  - `execution_member_set_cached = 13,977,956 Ir`
  - `ZrCore_Function_PreCallResolvedVmFunction = 11,845,924 Ir`
- `map_object_access`
  - `native_binding_dispatcher = 5,959,795 Ir`
  - `object_call_known_native_fast.part.0 = 4,418,428 Ir`
  - `ZrCore_Function_PreCallKnownNativeValue = 3,603,402 Ir`
  - `ZrCore_Function_StackAnchorInit = 1,214,857 Ir`
  - `native_binding_begin_rooted_value = 429,064 Ir`
  - `native_binding_init_call_context_layout = 405,801 Ir`

Recommended next slice:

- on `dispatch_loops`, go back to the highest-value open path: cached member get/set, specifically
  the remaining `callsite_cache_lookup` /
  `ZrCore_Object_GetMemberCachedDescriptorUnchecked(...)` work inside
  `execution_member_get_cached` / `execution_member_set_cached`
- keep the benchmark-local generic `labelFor()` concern closed; it is still not the active hotspot
- on `map_object_access`, only revisit `native_binding_init_call_context_layout(...)` or temp-root
  teardown after the dispatch-side cached-member cut lands

## Slice 30: Exact-Receiver Object-Helper Cut (Rejected)

### Root Cause

The next attempt on `dispatch_loops` targeted the exact-receiver single-slot PIC path that still
fell back through `ZrCore_Object_GetMemberCachedDescriptorUnchecked(...)` /
`ZrCore_Object_SetMemberCachedDescriptorUnchecked(...)`.

The idea was simple:

- keep the already-proven exact-receiver hit
- stop re-deriving `receiverObject` inside the cached-descriptor helpers
- pass the resolved object directly from `execution_member_get_cached` /
  `execution_member_set_cached`

### Runtime Change

The rejected draft added:

- `ZrCore_Object_GetMemberCachedDescriptorWithObjectUnchecked(...)`
- `ZrCore_Object_SetMemberCachedDescriptorWithObjectUnchecked(...)`
- single-slot exact-receiver PIC paths in `execution_member_access.c` that called those helpers
  directly

### Validation

The first rerun surfaced an apparently random failure in
`zr_vm_object_call_known_native_fast_path_test`.

`gdb` showed that was not a member-path bug. The real regression was a separate shared trace issue:
`ZrCore_Function_PostCall()` had started reading callable metadata unconditionally even after a
native return slot had already overwritten `functionBase`.

That correctness issue was fixed separately and is retained in slice 31. After that repair, the
exact-receiver object-helper draft itself still passed the focused gcc/clang tests and Windows CLI
smoke, so its accept/reject decision came down to benchmark evidence only.

### Focused Benchmarks

Against the local control state with the same correctness repair but without the object-helper path:

- profile / callgrind:
  - `dispatch_loops`: `525,981,357 -> 526,277,493 Ir` (`+0.06%`, worse)
  - `map_object_access`: `112,764,971 -> 112,764,574 Ir` (flat)
- focused core wall:
  - local control `dispatch_loops`: `264.985 ms`, `264.220 ms`
  - object-helper draft `dispatch_loops`: `251.278 ms`, `258.828 ms`
  - local control `map_object_access`: `131.811 ms`, `133.336 ms`
  - object-helper draft `map_object_access`: `138.765 ms`, `137.200 ms`

### Interpretation

- the direct-object helper cut did not create a reproducible instruction-counting win on the
  dispatch hotspot it was supposed to help
- `map_object_access` did not gain from it either
- because the local delta stayed non-positive after the shared correctness issue was fixed, this
  slice is rejected and the object-helper helpers are removed again

### Benchmark Artifacts

Rejected diagnostics kept on disk:

- `build/benchmark-gcc-release/tests_generated/performance_profile_dispatch_map_slice30_object_helper_exact_receiver`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_dispatch_map_slice30_object_helper_exact_receiver`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_dispatch_map_slice30_object_helper_exact_receiver_repeat`

## Slice 31: Closure Metadata Safety Guard And Lazy PostCall Trace

### Root Cause

While validating slice 30, `zr_vm_object_call_known_native_fast_path_test` failed under WSL gcc
with a crash in `ZrCore_Closure_GetMetadataFunctionFromValue()`.

`gdb` showed the real issue:

- `ZrCore_Function_PostCall()` had been changed to compute trace metadata unconditionally
- on native calls, `functionBase` is already reused as the return slot before `PostCall`
- the new metadata lookup therefore tried to interpret non-callable return values as closures

That was both:

- a correctness bug
- a release hot-path tax, because steady-state runs paid metadata lookup work even when tracing was
  disabled

### Runtime Change

The accepted repair keeps two narrow pieces:

- `ZrCore_Closure_GetMetadataFunctionFromValue(...)` now rejects non-callable value kinds before
  touching the object union
- `ZrCore_Function_PostCall()` now resolves metadata lazily only when function trace is actually
  enabled

The earlier exact-receiver object-helper draft stays rolled back.

### Focused Benchmarks

Against the post-revert local control that still paid unconditional PostCall metadata lookup:

- profile / callgrind:
  - `dispatch_loops`: `525,981,357 -> 512,410,552 Ir` (`-2.58%`)
  - `ZrCore_Closure_GetMetadataFunctionFromValue`: `16,776,885 -> 12,622,954 Ir` (`-24.76%`)
  - `ZrCore_Function_PostCall`: `11,231,063 -> 9,384,875 Ir` (`-16.44%`)
  - `map_object_access`: `112,764,971 -> 116,338,927 Ir` (`+3.17%`, mixed)
- focused core wall:
  - control `dispatch_loops`: `264.985 ms`, `264.220 ms`
  - accepted lazy-trace state `dispatch_loops`: `257.251 ms`, `253.308 ms`, `254.258 ms`
  - control `map_object_access`: `131.811 ms`, `133.336 ms`
  - accepted lazy-trace state `map_object_access`: `140.346 ms`, `133.762 ms`, `137.757 ms`

### Interpretation

- the safety guard is mandatory; the crash was real and reproducible
- lazy PostCall metadata lookup clearly recovers dispatch-side instruction and wall cost that should
  never have been charged in steady state
- `map_object_access` remains mixed in the current dirty-tree environment, so this slice should be
  treated as a dispatch-side recovery and correctness repair, not as a map-side performance win

### Validation

Final candidate after slice 31:

- WSL gcc Debug:
  - `zr_vm_object_call_known_native_fast_path_test`: `11 Tests 0 Failures`
  - `zr_vm_execution_member_access_fast_paths_test`: `31 Tests 0 Failures`
  - `zr_vm_value_copy_fast_paths_test`: `3 Tests 0 Failures`
  - `zr_vm_container_temp_value_root_test`: `3 Tests 0 Failures`
  - `zr_vm_precall_frame_slot_reset_test`: `3 Tests 0 Failures`
- WSL clang Debug:
  - `zr_vm_object_call_known_native_fast_path_test`: `11 Tests 0 Failures`
  - `zr_vm_execution_member_access_fast_paths_test`: `31 Tests 0 Failures`
  - `zr_vm_value_copy_fast_paths_test`: `3 Tests 0 Failures`
  - `zr_vm_container_temp_value_root_test`: `3 Tests 0 Failures`
  - `zr_vm_precall_frame_slot_reset_test`: `3 Tests 0 Failures`
- Windows MSVC Debug CLI smoke:
  - rebuilt `build\codex-msvc-cli-debug` target `zr_vm_cli_executable`
  - ran `tests/fixtures/projects/hello_world/hello_world.zrp`
  - output remained `hello world`

### Benchmark Artifacts

Accepted focused snapshots kept on disk:

- `build/benchmark-gcc-release/tests_generated/performance_profile_dispatch_map_slice30_refined_metadata_guard_lazy_trace`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_dispatch_map_slice30_refined_metadata_guard_lazy_trace`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_dispatch_map_slice30_refined_metadata_guard_lazy_trace_repeat`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_dispatch_map_slice30_refined_metadata_guard_lazy_trace_repeat2`

Intermediate local-control diagnostics also kept on disk:

- `build/benchmark-gcc-release/tests_generated/performance_profile_dispatch_map_slice30_refined_metadata_guard`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_dispatch_map_slice30_refined_metadata_guard`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_dispatch_map_slice30_refined_metadata_guard_repeat`

## Current Hotspots / Next Slice (After Slice 31)

Fresh hotspot view after the accepted lazy-trace recovery state:

- `dispatch_loops`
  - `execution_member_get_cached = 51,509,261 Ir`
  - `execution_member_set_cached = 13,977,956 Ir`
  - `ZrCore_Function_PreCallResolvedVmFunction = 14,615,044 Ir`
  - `ZrCore_Closure_GetMetadataFunctionFromValue = 12,622,954 Ir`
  - `ZrCore_Function_PostCall = 9,384,875 Ir`
- `map_object_access`
  - `native_binding_dispatcher = 5,959,795 Ir`
  - `object_call_known_native_fast.part.0 = 4,418,428 Ir`
  - `garbage_collector_ignore_registry_contains = 4,276,688 Ir`
  - `ZrCore_Function_PreCallKnownNativeValue = 3,603,402 Ir`
  - `ZrCore_GarbageCollector_UnignoreObject = 3,395,566 Ir`
  - `ZrCore_GarbageCollector_IgnoreObject = 2,897,646 Ir`

Recommended next slice:

- on `dispatch_loops`, stop treating `ZrCore_Closure_GetMetadataFunctionFromValue(...)` as a
  generic helper and cut the remaining hot callsites in `execution_dispatch.c`, especially:
  - the known-VM resolve path around `callableValue->type == FUNCTION / CLOSURE`
  - VM frame refresh around `functionBaseValue` / `currentFunctionBaseValue`
- keep the cached member work open, but do not reintroduce the rejected exact-receiver
  object-helper split unless a new measurement proves it
- on `map_object_access`, the next meaningful runtime-side evidence now points at GC
  ignore/unignore churn in the rooted native-binding lane rather than the already-closed
  benchmark-local `labelFor()` generic call issue

## Slice 32: Dispatch-Local Callable Metadata Refresh And Forwarded Frame Rewrite

### Root Cause

After slice 31, `dispatch_loops` still had one shared interpreter path that mixed two bad properties:

- `execution_dispatch.c` still treated VM callable metadata resolution as a generic helper concern instead
  of a dispatch-local hot path
- VM frame refresh kept the pre-forwarding raw callable object in `functionBase`, so later steady-state
  comparisons and metadata refreshes could keep observing a stale raw object even after GC forwarding

Fresh local-control evidence confirmed this was not just a micro-optimization concern. Reverting only the
slice-32 dispatch diff made `dispatch_loops` fail its correctness run with `Segmentation fault` in both
profile and core tiers.

### Test-First Repro

`tests/core/test_execution_dispatch_callable_metadata.c` was added and registered as
`zr_vm_execution_dispatch_callable_metadata_test`.

The new red-first tests are:

- `test_execute_refreshes_forwarded_function_base_value_object`
- `test_execute_refreshes_forwarded_closure_base_value_object`

They observe the live VM frame callable slot through a debug trace callback and lock the contract that the
dispatch loop must refresh `functionBase.valuePointer->value.object` to the forwarded raw function/closure
object before the frame continues running.

Red state before the runtime fix:

- function case observed the original raw function object instead of the forwarded one
- closure case observed the original raw closure object instead of the forwarded one

Benchmark-local control state after reverting only slice 32:

- `dispatch_loops` profile tier: correctness run failed with `Segmentation fault`
- `dispatch_loops` core tier: correctness run failed with `Segmentation fault`

### Runtime Change

`zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c` now keeps this work local to the dispatch hot
path:

- adds local forwarded-object refresh helpers for raw object / function / closure values
- adds `execution_try_resolve_vm_metadata_function_fast(...)` so dispatch can resolve VM callable metadata
  without bouncing back through the generic closure metadata helper path
- updates the known-VM precall fast path to jump directly to
  `ZrCore_Function_PreCallResolvedVmFunction(...)` when the callable is already provably a VM function or
  VM closure
- refreshes and writes back the forwarded raw callable object into the live `functionBase` stack slot during
  `LZrReturning` and later frame-refresh checks
- reuses the same helper for parent-function lookup in `GET_SUB_FUNCTION`
- initializes the dispatch locals defensively so the MSVC CLI smoke no longer reports the new
  `programCounter` early-error branch as maybe-uninitialized

This keeps the optimization dispatch-local and does not add any new public runtime API.

### Evidence

Fresh local-control baseline with only slice 32 reverted:

- profile tier:
  - `dispatch_loops`: `FAIL` (`Segmentation fault`)
  - `map_object_access`: `115,737,857 Ir`, `140.527 ms`
- core tier:
  - `dispatch_loops`: `FAIL` (`Segmentation fault`)
  - `map_object_access`: `136.591 ms`

Accepted slice-32 state after restoring the dispatch-local helper cut:

- profile tier:
  - `dispatch_loops`: `PASS`, `496,811,980 Ir`, `219.919 ms`
  - `map_object_access`: `PASS`, `115,643,172 Ir`, `132.705 ms`
- core tier:
  - first sample:
    - `dispatch_loops`: `242.845 ms`
    - `map_object_access`: `153.686 ms`
  - repeat:
    - `dispatch_loops`: `242.265 ms`
    - `map_object_access`: `131.219 ms`

Against the accepted slice-31 lazy-trace state, the recovered slice-32 dispatch profile moved to:

- `dispatch_loops` total Ir: `512,410,552 -> 496,811,980` (`-3.04%`)
- `ZrCore_Function_PreCallResolvedVmFunction`: `14,615,044 -> 11,845,924 Ir` (`-18.95%`)
- `ZrCore_Function_PostCall`: `9,384,875 -> 8,769,467 Ir` (`-6.56%`)
- `ZrCore_Closure_GetMetadataFunctionFromValue(...)` dropped off the top hotspot list for this case

Current slice-32 dispatch hotspot view:

- `execution_member_get_cached = 55,199,517 Ir`
- `execution_member_set_cached = 16,435,612 Ir`
- `ZrCore_Function_PreCallResolvedVmFunction = 11,845,924 Ir`
- `ZrCore_Function_PostCall = 8,769,467 Ir`

Interpretation:

- the highest-value outcome is that the local-control `dispatch_loops` crash disappears completely
- the dispatch-local callable metadata cut is not just safe; it also recovers a measurable dispatch-side
  instruction-count win over slice 31
- `map_object_access` remains secondary and mixed at wall-clock granularity, although the repeat sample
  re-converged below the control baseline and the callgrind delta stayed effectively flat

### Validation

Final candidate after slice 32:

- WSL gcc Debug:
  - `zr_vm_execution_dispatch_callable_metadata_test`: `2 Tests 0 Failures`
  - `zr_vm_object_call_known_native_fast_path_test`: `11 Tests 0 Failures`
  - `zr_vm_execution_member_access_fast_paths_test`: `31 Tests 0 Failures`
  - `zr_vm_value_copy_fast_paths_test`: `3 Tests 0 Failures`
  - `zr_vm_container_temp_value_root_test`: `3 Tests 0 Failures`
  - `zr_vm_precall_frame_slot_reset_test`: `7 Tests 0 Failures`
- WSL clang Debug:
  - `zr_vm_execution_dispatch_callable_metadata_test`: `2 Tests 0 Failures`
  - `zr_vm_object_call_known_native_fast_path_test`: `11 Tests 0 Failures`
  - `zr_vm_execution_member_access_fast_paths_test`: `31 Tests 0 Failures`
  - `zr_vm_value_copy_fast_paths_test`: `3 Tests 0 Failures`
  - `zr_vm_container_temp_value_root_test`: `3 Tests 0 Failures`
  - `zr_vm_precall_frame_slot_reset_test`: `7 Tests 0 Failures`
- Windows MSVC Debug CLI smoke:
  - rebuilt `build\codex-msvc-cli-debug` target `zr_vm_cli_executable`
  - ran `tests/fixtures/projects/hello_world/hello_world.zrp`
  - output remained `hello world`

### Benchmark Artifacts

Slice-32 control and accepted focused snapshots kept on disk:

- `build/benchmark-gcc-release/tests_generated/performance_profile_dispatch_map_slice32_dispatch_metadata_baseline`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_dispatch_map_slice32_dispatch_metadata_baseline`
- `build/benchmark-gcc-release/tests_generated/performance_profile_dispatch_map_slice32_dispatch_metadata_after`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_dispatch_map_slice32_dispatch_metadata_after`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_dispatch_map_slice32_dispatch_metadata_after_repeat`

## Slice 33: Exact-Receiver Member-Name Fallback Completion

### Root Cause

After slice 32, the single-slot exact-receiver cached member path still had one asymmetry:

- multi-slot cached instance-field access already tried a direct member-name own-field lookup before
  falling back to descriptor helpers
- the single-slot exact-receiver lane still bounced into the descriptor/helper path when
  `cachedReceiverPair == null`, even though the receiver object and member name were already known

This was a real fallback gap in the shared runtime path, but it still needed proof that closing the gap
would matter on the tracked benchmarks.

### Test-First Repro

`tests/core/test_execution_member_access_fast_paths.c` gained:

- `test_member_get_cached_exact_receiver_object_hit_uses_member_name_when_descriptor_name_is_missing`
- `test_member_set_cached_exact_receiver_object_hit_uses_member_name_when_descriptor_name_is_missing`

Red state with only the slice-33 member-access hunk reverted:

- get path missed the exact-receiver runtime hit
- set path fell back to the slower generic write path and bumped `memberVersion`

### Runtime Change

`zr_vm_core/src/zr_vm_core/execution/execution_member_access.c` added object-level exact-receiver
helpers for cached instance-field get/set:

- `execution_member_try_cached_instance_field_object_get(...)`
- `execution_member_try_cached_instance_field_object_set(...)`

On the single-slot exact-receiver lane, cached instance-field access now:

- tries a direct member-name own-field hit first
- preserves pair-write semantics on set via
  `object_get_own_string_pair_by_name_cached_unchecked(...)` plus
  `ZrCore_Object_SetExistingPairValueUnchecked(...)`
- falls back to the descriptor/helper path only when the member-name lane truly does not apply

### Evidence

Fresh focused control with only slice 33 reverted:

- profile tier:
  - `dispatch_loops`: `496,760,093 Ir`
  - `map_object_access`: `115,665,010 Ir`
- core tier:
  - artifact kept as focused baseline snapshot; wall samples were not used as primary evidence

Accepted slice-33 state after restoring the exact-receiver member-name lane:

- profile tier:
  - `dispatch_loops`: `497,235,924 Ir`
  - `execution_member_get_cached`: `55,199,281 Ir`
  - `map_object_access`: `116,100,965 Ir`
- core tier:
  - fresh focused sample existed, but absolute wall changes tracked the C baseline in the same runs and
    were therefore treated as non-diagnostic for this slice

Against the control state:

- `dispatch_loops` total Ir: `496,760,093 -> 497,235,924` (`+0.10%`, flat-to-worse)
- `execution_member_get_cached`: `55,199,517 -> 55,199,281 Ir` (effectively flat)
- `map_object_access` total Ir: `115,665,010 -> 116,100,965` (`+0.38%`, worse/noise)

Interpretation:

- slice 33 closes a genuine cached-member fallback gap and keeps the exact-receiver lane semantically
  aligned with the multi-slot path
- the change is correctness/fallback hygiene, not a repeatable tracked-case performance win
- this cut should remain, but it does not justify claiming measurable convergence on its own

### Validation

Slice 33 passed the focused regression/smoke set used at that point:

- WSL gcc Debug:
  - `zr_vm_execution_member_access_fast_paths_test`: `33 Tests 0 Failures`
  - `zr_vm_execution_dispatch_callable_metadata_test`: `2 Tests 0 Failures`
  - `zr_vm_object_call_known_native_fast_path_test`: `11 Tests 0 Failures`
  - `zr_vm_value_copy_fast_paths_test`: `3 Tests 0 Failures`
  - `zr_vm_container_temp_value_root_test`: `3 Tests 0 Failures`
  - `zr_vm_precall_frame_slot_reset_test`: `7 Tests 0 Failures`
- WSL clang Debug:
  - same focused set, all passing
- Windows MSVC Debug CLI smoke:
  - rebuilt `build\codex-msvc-cli-debug` target `zr_vm_cli_executable`
  - ran `tests/fixtures/projects/hello_world/hello_world.zrp`
  - output remained `hello world`

### Benchmark Artifacts

Slice-33 control and after focused snapshots kept on disk:

- `build/benchmark-gcc-release/tests_generated/performance_profile_member_exact_receiver_name_baseline_20260416`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_member_exact_receiver_name_baseline_20260416`
- `build/benchmark-gcc-release/tests_generated/performance_profile_member_exact_receiver_name_after_20260416`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_member_exact_receiver_name_after_20260416`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_member_exact_receiver_name_after_repeat_20260416`

## Slice 34: Exact-Pair Plain-Value Set Fast Helper

### Root Cause

After slice 33, `dispatch_loops` still concentrated a large write-side cost in:

- `execution_member_set_cached`
- `ZrCore_Object_SetExistingPairValueUnchecked(...)`

The exact-receiver/member-name and exact-pair set lanes were still routing steady-state plain-value writes
through the generic cross-file pair-write helper, even when the hot path already knew:

- the target `pair`
- the write was a plain no-ownership value
- no hidden-items cache state needed repair

That kept the hottest member-set path paying extra helper call and generic cache/barrier branching that the
steady-state dispatch benchmark does not need.

### Test-First Repro

`tests/core/test_execution_member_access_fast_paths.c` gained:

- `test_object_try_set_existing_pair_plain_value_fast_updates_value_and_lookup_cache`
- `test_object_try_set_existing_pair_plain_value_fast_rejects_hidden_items_cached_state`

Red state with the new helper stubbed back to `false`:

- `test_object_try_set_existing_pair_plain_value_fast_updates_value_and_lookup_cache`
  failed with `Expected TRUE Was FALSE`

That confirmed the new direct plain-value pair-set lane did not exist yet.

### Runtime Change

The plain-value exact-pair write cut is now shared between the member-access hot path and the generic
object helper:

- `zr_vm_core/src/zr_vm_core/object/object_internal.h` adds
  `object_try_set_existing_pair_plain_value_fast_unchecked(...)`
- `zr_vm_core/src/zr_vm_core/execution/execution_member_access.c` uses that helper first for:
  - exact-pair cached set hits
  - member-name exact-receiver object set hits
- `zr_vm_core/src/zr_vm_core/object/object.c` reuses the same helper at the top of
  `ZrCore_Object_SetExistingPairValueUnchecked(...)`

The helper stays conservative:

- it only handles normalized no-ownership writes
- it refuses when hidden-items cache state is already engaged
- it still updates `cachedStringLookupPair`
- it still issues the GC barrier for GC-managed values
- it keeps `memberVersion` unchanged on existing-pair writes

### Evidence

Fresh local-control baseline with only slice 34 reverted:

- profile tier:
  - `dispatch_loops`: `497,240,235 Ir`
  - `execution_member_get_cached`: `55,199,281 Ir`
  - `execution_member_set_cached`: `16,896,408 Ir`
  - `ZrCore_Object_SetExistingPairValueUnchecked(...)`: `12,287,680 Ir`
  - `map_object_access`: `115,701,303 Ir`
- core tier:
  - `dispatch_loops`: `243.354 ms`
  - `map_object_access`: `166.515 ms`

Accepted slice-34 state after restoring the helper:

- profile tier:
  - `dispatch_loops`: `489,267,780 Ir`
  - `execution_member_get_cached`: `55,199,281 Ir`
  - `execution_member_set_cached`: `21,504,288 Ir`
  - `ZrCore_Object_SetExistingPairValueUnchecked(...)` dropped off the top hotspot list for this case
  - `map_object_access`: `116,081,764 Ir`
- core tier:
  - `dispatch_loops`: `247.533 ms`
  - `map_object_access`: `130.075 ms`

Against the control state:

- `dispatch_loops` total Ir: `497,240,235 -> 489,267,780` (`-1.60%`)
- `execution_member_get_cached`: unchanged at `55,199,281 Ir`
- write-side hot cost moved from:
  - `execution_member_set_cached = 16,896,408 Ir`
  - `ZrCore_Object_SetExistingPairValueUnchecked(...) = 12,287,680 Ir`
  to:
  - `execution_member_set_cached = 21,504,288 Ir`
  - `ZrCore_Object_SetExistingPairValueUnchecked(...)` no longer appearing in the top hotspot list
- `map_object_access` total Ir: `115,701,303 -> 116,081,764` (`+0.33%`, flat-to-worse)

Interpretation:

- the deterministic dispatch-side callgrind win is real
- the write-side cost did not disappear; it was pulled up and inlined into `execution_member_set_cached`
  instead of being split between the dispatch helper and the generic object helper
- the single-sample core wall numbers are mixed and should be treated as noisy for this slice
- `map_object_access` stays unchanged; the slice is specific to the dispatch/member-set line

### Validation

Final candidate after slice 34:

- WSL gcc Debug:
  - `zr_vm_execution_member_access_fast_paths_test`: `35 Tests 0 Failures`
  - `zr_vm_execution_dispatch_callable_metadata_test`: `2 Tests 0 Failures`
  - `zr_vm_object_call_known_native_fast_path_test`: `11 Tests 0 Failures`
  - `zr_vm_value_copy_fast_paths_test`: `3 Tests 0 Failures`
  - `zr_vm_container_temp_value_root_test`: `3 Tests 0 Failures`
  - `zr_vm_precall_frame_slot_reset_test`: `7 Tests 0 Failures`
- WSL clang Debug:
  - `zr_vm_execution_member_access_fast_paths_test`: `35 Tests 0 Failures`
  - `zr_vm_execution_dispatch_callable_metadata_test`: `2 Tests 0 Failures`
  - `zr_vm_object_call_known_native_fast_path_test`: `11 Tests 0 Failures`
  - `zr_vm_value_copy_fast_paths_test`: `3 Tests 0 Failures`
  - `zr_vm_container_temp_value_root_test`: `3 Tests 0 Failures`
  - `zr_vm_precall_frame_slot_reset_test`: `7 Tests 0 Failures`
- Windows MSVC Debug CLI smoke:
  - rebuilt `build\codex-msvc-cli-debug` target `zr_vm_cli_executable`
  - ran `tests/fixtures/projects/hello_world/hello_world.zrp`
  - output remained `hello world`

### Benchmark Artifacts

Slice-34 control and after focused snapshots kept on disk:

- `build/benchmark-gcc-release/tests_generated/performance_profile_member_pair_plain_set_fast_baseline_20260416`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_member_pair_plain_set_fast_baseline_20260416`
- `build/benchmark-gcc-release/tests_generated/performance_profile_member_pair_plain_set_fast_after_20260416`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_member_pair_plain_set_fast_after_20260416`

## Slice 35: Non-Hidden String-Pair Fast Set Mark

### Root Cause

Slice 34 removed the generic object helper from the write-side hotspot list, but the new hottest write body
still lived inside `execution_member_set_cached` itself.

The remaining steady-state exact-pair set lane still paid one avoidable check family on every hit:

- it had to re-run the broader plain-pair helper gate that refuses when hidden-items cache state is present
- but PIC refresh already knows whether the cached member name is an ordinary field or the special hidden-items
  field

The next cut was therefore to cache that fact once at PIC refresh time and route exact-pair set hits through
an even narrower non-hidden string-pair helper.

### Test-First Repro

`tests/core/test_execution_member_access_fast_paths.c` gained:

- `test_object_try_set_existing_string_pair_plain_value_assume_non_hidden_updates_value_with_hidden_items_cached_state`

Red state before the runtime change:

- the new helper symbol did not exist, so the test target failed to build

During implementation, the local workspace already contained
`test_member_get_cached_refresh_keeps_non_hidden_plain_value_fast_set_flag_clear_for_instance_field`, which
locks that `reserved1` must remain clear on this PIC slot.
The implementation therefore keeps that test green by storing the hot-path mark outside `reserved1`.

### Runtime Change

The narrow non-hidden string-pair lane is now split out and cached at PIC refresh time:

- `zr_vm_core/src/zr_vm_core/object/object_internal.h` adds
  `object_try_set_existing_string_pair_plain_value_assume_non_hidden_unchecked(...)`
- `zr_vm_core/src/zr_vm_core/execution/execution_member_access.c` computes a non-hidden fast-set mark when
  PIC state is stored
- to avoid colliding with the existing `reserved1` clear contract, that mark is carried in the high bit of
  `cachedIsStatic`, while the low bit still preserves the static-access boolean
- the exact-pair member-set lane checks that mark first and only then uses the narrower helper

This leaves the generic pair-write helper in place as fallback, but trims another layer of dead control flow
out of the hottest non-hidden field-write path.

### Evidence

Fresh local-control baseline with the slot mark disabled:

- profile tier:
  - `dispatch_loops`: `489,433,634 Ir`
  - `execution_member_get_cached`: `55,199,281 Ir`
  - `execution_member_set_cached`: `21,657,884 Ir`
  - `map_object_access`: `115,813,800 Ir`
- core tier:
  - `dispatch_loops`: `264.521 ms`
  - `map_object_access`: `177.862 ms`

Accepted slice-35 state after restoring the slot mark:

- profile tier:
  - `dispatch_loops`: `488,167,684 Ir`
  - `execution_member_get_cached`: `55,199,281 Ir`
  - `execution_member_set_cached`: `20,429,116 Ir`
  - `map_object_access`: `115,786,728 Ir`
- core tier:
  - `dispatch_loops`: `300.969 ms`
  - `map_object_access`: `146.210 ms`

Against the control state:

- `dispatch_loops` total Ir: `489,433,634 -> 488,167,684` (`-0.26%`)
- `execution_member_set_cached`: `21,657,884 -> 20,429,116 Ir` (`-5.67%`)
- `execution_member_get_cached`: unchanged at `55,199,281 Ir`
- `map_object_access` total Ir: `115,813,800 -> 115,786,728` (`-0.02%`, flat)

Interpretation:

- this is a small but repeatable dispatch-side profile win
- the gain is exactly where expected: inside the inlined `execution_member_set_cached` body
- single-sample core wall numbers remain too noisy to use as primary evidence for this slice
- `map_object_access` stays structurally unchanged

### Validation

Final candidate after slice 35:

- WSL gcc Debug:
  - `zr_vm_execution_member_access_fast_paths_test`: `39 Tests 0 Failures`
  - `zr_vm_execution_dispatch_callable_metadata_test`: `2 Tests 0 Failures`
  - `zr_vm_object_call_known_native_fast_path_test`: `11 Tests 0 Failures`
  - `zr_vm_value_copy_fast_paths_test`: `3 Tests 0 Failures`
  - `zr_vm_container_temp_value_root_test`: `3 Tests 0 Failures`
  - `zr_vm_precall_frame_slot_reset_test`: `7 Tests 0 Failures`
- WSL clang Debug:
  - same focused set, all passing
- Windows MSVC Debug CLI smoke:
  - rebuilt `build\codex-msvc-cli-debug` target `zr_vm_cli_executable`
  - ran `tests/fixtures/projects/hello_world/hello_world.zrp`
  - output remained `hello world`

### Benchmark Artifacts

Slice-35 control and after focused snapshots kept on disk:

- `build/benchmark-gcc-release/tests_generated/performance_profile_member_pair_slot_flag_fast_set_baseline_20260416`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_member_pair_slot_flag_fast_set_baseline_20260416`
- `build/benchmark-gcc-release/tests_generated/performance_profile_member_pair_slot_flag_fast_set_after_20260416`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_member_pair_slot_flag_fast_set_after_20260416`

## Slice 36: Exact Callable Slot Flag Probe Rejected

### Root Cause

After slice 35, one hypothesis was that callable member PIC hits were still paying too much descriptor and
loop work because the runtime had no dedicated "exact receiver callable" mark.

The experimental cut tried to cache that fact explicitly in the PIC slot so exact callable hits could bypass
more of the generic callable/member path.

### Runtime Change

The rejected probe temporarily added a new PIC slot flag and corresponding exact-callable receiver fast path in:

- `zr_vm_core/include/zr_vm_core/function.h`
- `zr_vm_core/src/zr_vm_core/execution/execution_member_access.c`
- `tests/core/test_execution_member_access_fast_paths.c`

That runtime line was fully reverted after benchmarking.

### Evidence

Against the accepted slice-35 baseline:

- profile tier:
  - `dispatch_loops`: `488,132,635 -> 489,539,166 Ir` (`+0.29%`)
  - `execution_member_get_cached`: `55,199,281 -> 56,582,869 Ir` (`+2.51%`)
  - `map_object_access`: `115,607,970 -> 115,615,021 Ir` (`+0.01%`, flat-to-worse)
- core tier:
  - `dispatch_loops`: `248.622 -> 256.006 ms`
  - `map_object_access`: `133.611 -> 146.261 ms`

Interpretation:

- the exact-callable slot mark made the hottest dispatch-side member-get path worse
- `map_object_access` did not benefit either
- this probe is negative evidence and remains rejected

### Benchmark Artifacts

Rejected slice-36 snapshots kept on disk:

- `build/benchmark-gcc-release/tests_generated/performance_profile_member_exact_callable_fast_get_baseline_20260416`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_member_exact_callable_fast_get_baseline_20260416`
- `build/benchmark-gcc-release/tests_generated/performance_profile_member_exact_callable_fast_get_after_20260416`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_member_exact_callable_fast_get_after_20260416`

## Slice 37: Multi-Slot Callable-First Loop Reorder Rejected

### Root Cause

After rejecting slice 36, the next smaller probe stayed inside the same multi-slot member-get loop but removed
the extra flagging idea.

The hypothesis was that multi-slot cached member-get still wasted work by checking pair/shape lanes before
testing callable PIC slots that already carried an exact receiver object.

### Runtime Change

`zr_vm_core/src/zr_vm_core/execution/execution_member_access.c` temporarily reordered the multi-slot
`execution_member_try_cached_receiver_fast_get(...)` loop so callable slots were tested first when
`slot->cachedFunction != null`.

That loop reordering was reverted after benchmarking.

### Evidence

Against the same accepted slice-35 baseline:

- profile tier:
  - `dispatch_loops`: `488,132,635 -> 488,310,854 Ir` (`+0.04%`)
  - `execution_member_get_cached`: `55,199,281 -> 55,353,113 Ir` (`+0.28%`)
  - `map_object_access`: `115,607,970 -> 115,614,845 Ir` (`+0.01%`, flat-to-worse)
- core tier:
  - `dispatch_loops`: `248.622 -> 242.428 ms`
  - `map_object_access`: `133.611 -> 134.319 ms`

Interpretation:

- the deterministic profile result is still negative/flat on the actual dispatch hotspot
- the single core `dispatch_loops` sample improved, but not enough to override the profile regression
- this loop-order cut is also rejected

### Validation

Rejected slice 37 still passed the focused WSL regression set before being reverted:

- WSL gcc Debug:
  - `zr_vm_execution_member_access_fast_paths_test`: `39 Tests 0 Failures`
  - `zr_vm_execution_dispatch_callable_metadata_test`: `2 Tests 0 Failures`
  - `zr_vm_object_call_known_native_fast_path_test`: `11 Tests 0 Failures`
  - `zr_vm_value_copy_fast_paths_test`: `3 Tests 0 Failures`
  - `zr_vm_container_temp_value_root_test`: `3 Tests 0 Failures`
  - `zr_vm_precall_frame_slot_reset_test`: `7 Tests 0 Failures`
- WSL clang Debug:
  - same focused set, all passing

### Benchmark Artifacts

Rejected slice-37 snapshots kept on disk:

- `build/benchmark-gcc-release/tests_generated/performance_profile_member_multislot_callable_first_after_20260416`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_member_multislot_callable_first_after_20260416`

## Slice 38: Callable Receiver Exact-Hit Shape Guard Relaxation

### Root Cause

The next hypothesis was narrower still: on the multi-slot callable receiver-hit lane, the runtime already had:

- receiver prototype match
- receiver version match
- owner version match
- exact cached receiver object hit

That made the additional receiver-shape cache check look redundant.

### Test-First Repro

`tests/core/test_execution_member_access_fast_paths.c` gained:

- `test_member_get_cached_callable_receiver_object_hit_does_not_require_receiver_shape_cache`

The test locks the contract that an exact callable receiver-object hit may still succeed when the cached
shape metadata is absent, provided the outer prototype/version guards still hold.

### Runtime Change

`execution_member_try_cached_callable_receiver_hit(...)` no longer requires
`execution_member_cached_receiver_shape_matches(...)` once the exact receiver object is known.

### Evidence

Against the same accepted slice-35 baseline:

- profile tier:
  - `dispatch_loops`: `488,132,635 -> 488,298,084 Ir` (`+0.03%`)
  - `execution_member_get_cached`: `55,199,281 -> 55,353,113 Ir` (`+0.28%`)
  - `map_object_access`: `115,607,970 -> 116,074,551 Ir` (`+0.40%`)
- core tier:
  - `dispatch_loops`: `248.622 -> 246.202 ms`
  - `map_object_access`: `133.611 -> 134.780 ms`

Interpretation:

- the semantic cleanup is valid and now regression-covered
- on its own, it still does not buy a repeatable tracked-case win
- the change remained in the immediately following member-get experiment, but slice 38 alone is not claimed as an
  accepted performance win

### Benchmark Artifacts

Standalone slice-38 snapshots kept on disk:

- `build/benchmark-gcc-release/tests_generated/performance_profile_member_callable_receiver_no_shape_after_20260416`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_member_callable_receiver_no_shape_after_20260416`

## Slice 39: Exact-Pair Plain-Value Get Fast Copy

### Root Cause

Re-reading `dispatch_loops` showed the remaining `execution_member_get_cached` bulk is dominated by repeated
`this.state` field reads inside worker methods, not by callable member lookup.

Even the exact-pair steady-state get lane still routed those reads through the generic
`ZrCore_Value_CopyNoProfile(...)` wrapper, paying:

- a self-copy guard that cannot trigger for member reads
- the generic fast-copy dispatcher

on a hot path that is overwhelmingly copying plain integer values out of cached field pairs.

### Runtime Change

`zr_vm_core/src/zr_vm_core/execution/execution_member_access.c` now narrows the member-get copy helper:

- `execution_member_copy_value_profiled(...)` first checks `ZrCore_Value_CanFastCopyPlainValue(...)`
- when that fast plain-value contract holds, it copies with direct assignment and returns immediately
- only non-plain or ownership-sensitive values still fall back to `ZrCore_Value_CopyNoProfile(...)`

This tighter copy helper is used by both:

- `execution_member_try_cached_instance_field_pair_get(...)`
- `execution_member_try_cached_instance_field_object_get(...)`

### Evidence

Against the accepted slice-35 baseline:

- profile tier:
  - `dispatch_loops`: `488,132,635 -> 484,470,264 Ir` (`-0.75%`)
  - `execution_member_get_cached`: `55,199,281 -> 51,510,145 Ir` (`-6.68%`)
  - `execution_member_set_cached`: unchanged at `20,429,116 Ir`
  - `map_object_access`: `115,607,970 -> 115,627,499 Ir` (`+0.02%`, flat)
- core tier:
  - first sample:
    - `dispatch_loops`: `248.622 -> 234.861 ms`
    - `map_object_access`: `133.611 -> 143.391 ms`
  - repeat:
    - `dispatch_loops`: `233.549 ms`
    - `map_object_access`: `130.975 ms`

Interpretation:

- this is the first post-slice-35 cut to move `execution_member_get_cached` materially in the expected direction
- the dispatch-side callgrind win is deterministic and concentrated exactly on the targeted member-get hotspot
- `map_object_access` stays effectively flat in profile terms
- the first core `map_object_access` sample was noisy, but the repeat re-converged below baseline, so this slice
  is accepted

### Validation

Final candidate after slice 39:

- WSL gcc Debug:
  - `zr_vm_execution_member_access_fast_paths_test`: `40 Tests 0 Failures`
  - `zr_vm_execution_dispatch_callable_metadata_test`: `2 Tests 0 Failures`
  - `zr_vm_object_call_known_native_fast_path_test`: `11 Tests 0 Failures`
  - `zr_vm_value_copy_fast_paths_test`: `3 Tests 0 Failures`
  - `zr_vm_container_temp_value_root_test`: `3 Tests 0 Failures`
  - `zr_vm_precall_frame_slot_reset_test`: `7 Tests 0 Failures`
- WSL clang Debug:
  - same focused set, all passing
- Windows MSVC Debug CLI smoke:
  - rebuilt `build\codex-msvc-cli-debug` target `zr_vm_cli_executable`
  - ran `tests/fixtures/projects/hello_world/hello_world.zrp`
  - output remained `hello world`

### Benchmark Artifacts

Accepted slice-39 snapshots kept on disk:

- `build/benchmark-gcc-release/tests_generated/performance_profile_member_exact_callable_fast_get_baseline_20260416`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_member_exact_callable_fast_get_baseline_20260416`
- `build/benchmark-gcc-release/tests_generated/performance_profile_member_pair_plain_get_after_20260416`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_member_pair_plain_get_after_20260416`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_member_pair_plain_get_after_repeat_20260416`

## Slice 40: Exact-Get Helper Recording Batch Rejected

### Root Cause

After slice 39, the next micro-cut stayed on the exact pair/object get lane and tried to remove another fixed
cost: pair/object gets still recorded `get_member` and `value_copy` through two separate helper-recording calls.

The hypothesis was that batching those two counter bumps behind one runtime lookup and one `recordHelpers` branch
might buy a little more on steady-state `this.state` reads.

### Runtime Change

The rejected probe temporarily introduced `execution_member_record_get_and_copy_value(...)` so:

- exact pair gets
- exact object/member-name gets

could record both helper counts through one state-backed profile-runtime lookup before copying the value.

That batching helper was reverted after benchmarking.

### Evidence

Against the accepted slice-39 state:

- profile tier:
  - `dispatch_loops`: `484,470,264 -> 484,610,897 Ir` (`+0.03%`)
  - `execution_member_get_cached`: `51,510,145 -> 51,663,977 Ir` (`+0.30%`)
  - `map_object_access`: `115,627,499 -> 116,196,216 Ir` (`+0.49%`)
- core tier:
  - `dispatch_loops`: `234.861 -> 230.784 ms`
  - `map_object_access`: `143.391 -> 145.727 ms`

Interpretation:

- the single core `dispatch_loops` sample improved again
- the deterministic profile result did not
- `map_object_access` regressed materially enough that this batching cut is not worth keeping

### Validation

Rejected slice 40 still passed the direct member-access regression target before being reverted:

- WSL gcc Debug:
  - `zr_vm_execution_member_access_fast_paths_test`: `40 Tests 0 Failures`
- WSL clang Debug:
  - `zr_vm_execution_member_access_fast_paths_test`: `40 Tests 0 Failures`

### Benchmark Artifacts

Rejected slice-40 snapshots kept on disk:

- `build/benchmark-gcc-release/tests_generated/performance_profile_member_get_helper_batch_after_20260416`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_member_get_helper_batch_after_20260416`

## Slice 41: Exact-Pair Get Shell Removal Rejected

### Root Cause

After rejecting helper batching, the next narrower probe touched only the exact pair-get lane itself.

The hypothesis was that the hot dispatch path might still be wasting work by wrapping exact pair hits in a
`try/guard/bool-return` helper when all four call sites had already proven the slot shape they needed.

### Runtime Change

The rejected probe temporarily replaced `execution_member_try_cached_instance_field_pair_get(...)` with a narrower
direct-copy helper and rewired the four exact pair-get call sites in
`execution_member_try_cached_receiver_fast_get(...)` to call it directly.

That direct-call shell removal was reverted after benchmarking.

### Evidence

Against the accepted slice-39 state:

- profile tier:
  - `dispatch_loops`: `484,470,264 -> 487,215,242 Ir` (`+0.57%`)
  - `execution_member_get_cached`: `51,510,145 -> 54,276,997 Ir` (`+5.37%`)
  - `map_object_access`: `115,627,499 -> 115,639,439 Ir` (`+0.01%`, flat)
- core tier:
  - `dispatch_loops`: `234.861 -> 247.590 ms`
  - `map_object_access`: `143.391 -> 130.400 ms`

Interpretation:

- this is decisive negative evidence on the tracked dispatch hotspot
- whatever looked redundant in that helper shell was in fact helping the generated code shape
- the cut remains rejected and reverted

### Validation

Rejected slice 41 still passed the direct member-access regression target before being reverted:

- WSL gcc Debug:
  - `zr_vm_execution_member_access_fast_paths_test`: `40 Tests 0 Failures`
- WSL clang Debug:
  - `zr_vm_execution_member_access_fast_paths_test`: `40 Tests 0 Failures`

### Benchmark Artifacts

Rejected slice-41 snapshots kept on disk:

- `build/benchmark-gcc-release/tests_generated/performance_profile_member_pair_get_shell_after_20260416`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_member_pair_get_shell_after_20260416`

## Current Hotspots / Next Slice (After Slice 39)

Fresh hotspot view after the accepted exact-pair plain-value get cut:

- `dispatch_loops`
  - `execution_member_get_cached = 51,510,145 Ir`
  - `execution_member_set_cached = 20,429,116 Ir`
  - `try_builtin_add = 16,909,200 Ir`
  - `ZrCore_Function_PreCallResolvedVmFunction = 11,845,924 Ir`
- `map_object_access`
  - `native_binding_dispatcher = 5,959,795 Ir`
  - `object_call_known_native_fast.part.0 = 4,418,428 Ir`
  - `ZrCore_Function_PreCallKnownNativeValue = 3,603,402 Ir`

Recommended next slice:

- stay on `execution_member_get_cached` while it is still the dominant dispatch hotspot and is still yielding
  deterministic profile wins
- deprioritize callable receiver-hit micro-cuts; three successive probes showed that line is not where the tracked
  dispatch cost lives
- split the remaining get-side work between:
  - exact-pair/helper recording overhead that still executes on every `this.state` read
  - object/member-name fallback work that still survives after the new plain-value copy trim
- keep `map_object_access` secondary until the member-get line stops producing repeatable convergence
