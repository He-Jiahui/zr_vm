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
- `zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.c`
- `zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h`
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

## Slice 42: Multi-Slot Exact-Receiver Object Member-Name Lane Rejected

### Root Cause

After slice 39, the next visible asymmetry in `execution_member_get_cached(...)` was outside the exact-pair lane:

- single-slot exact-receiver object hits already tried direct `object/member-name` own-field lookup before the
  version/descriptor path
- multi-slot exact-receiver object hits with `cachedReceiverPair == null` still had to reach the generic
  prototype/version loop before they could use member-name fallback

The hypothesis was that copying the single-slot exact-object shortcut into the multi-slot receiver-fast loop would
remove another fixed dispatch-side cost from repeated `this.state` reads.

### Runtime Change

The rejected probe temporarily added a multi-slot exact-receiver object/member-name lane ahead of the generic
version/descriptor path so exact object hits could bypass the slower fallback scaffolding even when the cached pair was
missing.

That lane was reverted after same-session benchmarking.

### Evidence

Against a same-session local baseline on the current repository state:

- profile tier:
  - `dispatch_loops`: `484,474,648 -> 484,292,524 Ir` (`-0.04%`)
  - `execution_member_get_cached`: `51,510,145 -> 51,356,313 Ir` (`-0.30%`)
  - `map_object_access`: `116,083,635 -> 115,610,796 Ir` (`-0.41%`)
- profile wall:
  - `dispatch_loops`: `188.190 -> 202.490 ms`
  - `map_object_access`: `151.510 -> 152.940 ms`
- core wall:
  - `dispatch_loops`: `273.770 -> 429.442 ms`
  - `dispatch_loops repeat`: `309.580 -> 319.150 ms`
  - `map_object_access`: `161.500 -> 184.741 ms`
  - `map_object_access repeat`: `152.820 -> 176.689 ms`

Interpretation:

- the callgrind gain was real but too small
- same-session wall samples were consistently worse on both tracked cases
- this exact-object multi-slot bypass is not worth keeping

### Benchmark Artifacts

Same-session control and rejected snapshots kept on disk:

- `build/benchmark-gcc-release/tests_generated/performance_profile_member_multislot_object_name_local_baseline_20260417`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_member_multislot_object_name_local_baseline_20260417`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_member_multislot_object_name_local_baseline_repeat_20260417`
- `build/benchmark-gcc-release/tests_generated/performance_profile_member_multislot_object_name_after_20260417`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_member_multislot_object_name_after_20260417`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_member_multislot_object_name_after_repeat_20260417`

## Slice 43: Generic Instance-Field Receiver-Object Resolve-Once

### Root Cause

After rejecting the multi-slot exact-object bypass, the remaining object/member-name line still had one smaller fixed
cost inside `execution_member_try_cached_get(...)`:

- the generic instance-field/member-name branch re-resolved `receiverObject` from `receiver` on each matching slot
- but that loop had already stabilized `receiver` and only needed the resolved object for direct own-field lookup

This left a lower-risk micro-cut: keep the generic control flow intact, but resolve the receiver object once and feed
that stable object through the member-name get helper.

### Runtime Change

`zr_vm_core/src/zr_vm_core/execution/execution_member_access.c` now:

- resolves `receiverObject` once after `receiverPrototype` in `execution_member_try_cached_get(...)`
- routes the generic instance-field/member-name lane directly through
  `execution_member_try_cached_instance_field_object_get(...)`
- removes the dead rejected helper `execution_member_record_get_and_copy_value(...)`

This keeps the same version/descriptor semantics as the accepted baseline; it only removes repeated receiver-object
resolution inside the generic multi-slot get loop.

### Evidence

Against the same-session local baseline on the current repository state:

- profile tier:
  - `dispatch_loops`: `484,474,648 -> 483,971,925 Ir` (`-0.10%`)
  - `execution_member_get_cached`: `51,510,145 -> 51,048,885 Ir` (`-0.90%`)
  - `map_object_access`: `116,083,635 -> 115,775,373 Ir` (`-0.27%`)
- profile wall:
  - `dispatch_loops`: `188.190 -> 177.580 ms`
  - `map_object_access`: `151.510 -> 139.160 ms`
- core wall:
  - `dispatch_loops`: `273.770 -> 297.020 ms`
  - `dispatch_loops repeat`: `309.580 -> 281.490 ms`
  - `map_object_access`: `161.500 -> 154.920 ms`
  - `map_object_access repeat`: `152.820 -> 194.390 ms`

Against the accepted slice-39 state:

- profile tier:
  - `dispatch_loops`: `484,470,264 -> 483,971,925 Ir` (`-0.10%`)
  - `execution_member_get_cached`: `51,510,145 -> 51,048,885 Ir` (`-0.90%`)
  - `map_object_access`: `115,627,499 -> 115,775,373 Ir` (`+0.13%`, flat)

Interpretation:

- the deterministic dispatch-side callgrind gain is real and lands exactly on the intended hotspot
- `map_object_access` stays effectively flat versus the accepted slice-39 line and slightly better versus the
  same-session local baseline
- current core wall samples are noisy enough that they should not be treated as primary evidence here
- this resolve-once cut is worth keeping

### Validation

Accepted slice-43 state:

- WSL gcc Debug:
  - `zr_vm_execution_member_access_fast_paths_test`: `45 Tests 0 Failures`
  - `zr_vm_execution_dispatch_callable_metadata_test`: `2 Tests 0 Failures`
  - `zr_vm_object_call_known_native_fast_path_test`: `11 Tests 0 Failures`
  - `zr_vm_value_copy_fast_paths_test`: `3 Tests 0 Failures`
  - `zr_vm_container_temp_value_root_test`: `3 Tests 0 Failures`
  - `zr_vm_precall_frame_slot_reset_test`: `7 Tests 0 Failures`
- WSL clang Debug:
  - `zr_vm_execution_member_access_fast_paths_test`: `45 Tests 0 Failures`
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

Same-session control and accepted slice-43 snapshots kept on disk:

- `build/benchmark-gcc-release/tests_generated/performance_profile_member_multislot_object_name_local_baseline_20260417`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_member_multislot_object_name_local_baseline_20260417`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_member_multislot_object_name_local_baseline_repeat_20260417`
- `build/benchmark-gcc-release/tests_generated/performance_profile_member_object_resolve_once_after_20260417`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_member_object_resolve_once_after_20260417`
- `build/benchmark-gcc-release/tests_generated/performance_focus_core_member_object_resolve_once_after_repeat_20260417`

## Slice 44: `try_builtin_add` Exact-Pair Fast Paths

### Root Cause

After slice 43, `dispatch_loops` still showed one large non-member body:

- `try_builtin_add = 16,909,200 Ir`
- `value_to_int64 = 8,303,040 Ir`

That hotspot was dominated by exact signed-int arithmetic on the benchmark path, but the helper still routed every
call through the wider `number-or-bool` classification plus `value_to_*` conversion helpers.

`map_object_access` also still had string-add traffic, but the exact `string + string` path was mixed into the same
generic helper that reserved scratch stack slots even when both operands were already stable strings.

### Regression Coverage

`tests/core/test_execution_add_stack_relocation.c` now covers both add lanes explicitly:

- `test_execution_add_restores_stack_destination_after_generic_string_concat_growth`
  keeps the old relocation contract alive on the remaining generic safe-concat path via `string + int`
- `test_try_builtin_add_exact_string_pair_avoids_scratch_stack_growth`
  proves exact `string + string` builtin add no longer grows scratch stack space just to concatenate two existing
  strings

The new test failed first on WSL gcc Debug with:

- expected `allocatorContext.moveCount == 0`
- observed `1`

That red state confirmed the old exact string pair path was still paying the generic scratch-stack concat cost.

### Runtime Change

`zr_vm_core/src/zr_vm_core/execution/execution_numeric.c` now:

- fast-paths exact signed-int pairs directly to `INT64` results without `value_to_int64(...)`
- fast-paths exact unsigned and exact float pairs directly to `UINT64` / `DOUBLE` results
- routes exact `string + string` pairs through a dedicated direct concat helper instead of the generic scratch-stack
  safe-concat path

`zr_vm_core/src/zr_vm_core/string.c` and `zr_vm_core/include/zr_vm_core/string.h` now expose:

- `ZrCore_String_ConcatPair(...)`

That helper builds exact two-string results without materializing a temporary VM stack concat window, which is the
behavior locked by the new moving-allocator regression test.

### Evidence

Against the accepted slice-43 profile snapshot:

- `dispatch_loops`: `483,971,925 -> 473,985,777 Ir` (`-2.06%`)
- `value_to_int64`: `8,303,040 -> 2,769,120 Ir` (`-66.65%`)
- `try_builtin_add` self Ir: `16,909,200 -> 16,909,200` (flat; the gain landed in eliminated callee work, not helper self cost)
- `map_object_access`: `115,775,373 -> 116,460,641 Ir` (`+0.59%`)
- `map_object_access repeat`: `116,619,774 Ir` (`+0.73%` vs slice 43)

Fresh current-wall sample on core tier (`3` iterations, `dispatch_loops,map_object_access` only):

- `dispatch_loops`: mean `252.029 ms`
- `map_object_access`: mean `157.840 ms`

Interpretation:

- this slice is a real dispatch-side win: the exact numeric pair cut collapses the large `value_to_int64(...)` tail
  and moves the tracked `dispatch_loops` total down materially
- `map_object_access` does not show a matching win from this slice, which matches the hotspot evidence:
  `try_builtin_add` is tiny on that case, while the still-visible `concat_values_to_destination` cost remains on the
  generic safe-concat line rather than the new exact string pair helper
- keep this slice for `dispatch_loops`; do not treat it as the `map_object_access` string-add fix

### Validation

Accepted slice-44 state:

- WSL gcc Debug:
  - `zr_vm_execution_add_stack_relocation_test`: `6 Tests 0 Failures`
  - `zr_vm_instructions_test`: passed
  - `zr_vm_execution_member_access_fast_paths_test`: passed
  - `zr_vm_execution_dispatch_callable_metadata_test`: passed
  - `zr_vm_object_call_known_native_fast_path_test`: passed
  - `zr_vm_value_copy_fast_paths_test`: passed
  - `zr_vm_container_temp_value_root_test`: passed
  - `zr_vm_precall_frame_slot_reset_test`: passed
- WSL clang Debug:
  - `zr_vm_execution_add_stack_relocation_test`: passed
  - `zr_vm_instructions_test`: passed
  - `zr_vm_execution_member_access_fast_paths_test`: passed
  - `zr_vm_execution_dispatch_callable_metadata_test`: passed
  - `zr_vm_object_call_known_native_fast_path_test`: passed
  - `zr_vm_value_copy_fast_paths_test`: passed
  - `zr_vm_container_temp_value_root_test`: passed
  - `zr_vm_precall_frame_slot_reset_test`: passed
- Windows MSVC Debug CLI smoke:
  - rebuilt `build\codex-msvc-cli-debug` target `zr_vm_cli_executable`
  - ran `tests/fixtures/projects/hello_world/hello_world.zrp`
  - output remained `hello world`

### Benchmark Artifacts

Accepted slice-44 snapshots kept on disk:

- `build/benchmark-gcc-release/tests_generated/performance_profile_try_builtin_add_after_20260417`
- `build/benchmark-gcc-release/tests_generated/performance_profile_try_builtin_add_after_repeat_20260417`
- `build/benchmark-gcc-release/tests_generated/performance_core_try_builtin_add_after_20260417`

## Current Hotspots / Next Slice (After Slice 44)

Fresh hotspot view after the accepted `try_builtin_add` exact-pair cut:

- `dispatch_loops`
  - `execution_member_get_cached = 51,048,885 Ir`
  - `execution_member_set_cached = 20,429,116 Ir`
  - `try_builtin_add` self body is no longer the main follow-up on this case; the removed callee tail was the real win
  - `ZrCore_Function_PreCallResolvedVmFunction = 11,845,924 Ir`
- `map_object_access`
  - `native_binding_dispatcher = 5,959,795 Ir`
  - `object_call_known_native_fast.part.0 = 4,418,428 Ir`
  - `ZrCore_Function_PreCallKnownNativeValue = 3,603,402 Ir`
  - `concat_values_to_destination = 1,069,317 Ir` still sits on the generic safe-concat path and was not materially
    moved by the exact string pair helper

Recommended next slice:

- keep `execution_member_get_cached` secondary unless another clearly bounded object/member-name or descriptor-side
  cut still shows deterministic profile wins
- move the next builtin-add/string work only onto the remaining generic safe-concat lane if we can first prove which
  mixed-type adds are still feeding `concat_values_to_destination`
- otherwise the hotter W2 follow-up is still on the known-native call/object-access side of `map_object_access`

## Slice 45: Resolved Native PreCall For Known-Native Object Calls

### Root Cause

After slice 44, `map_object_access` was still dominated by the native index-contract object-call bridge:

- `native_binding_dispatcher = 5,959,795 Ir`
- `object_call_known_native_fast.part.0 = 4,418,428 Ir`
- `ZrCore_Function_PreCallKnownNativeValue = 3,603,402 Ir`

That bridge already knew it was holding a permanent native closure on the hot path, but it still routed the call through
`ZrCore_Function_PreCallKnownNativeValue(...)`, which re-read the callable slot and re-decoded the native function pointer
before entering the native frame.

### Regression Coverage

`tests/core/test_object_call_known_native_fast_path.c` now adds:

- `test_precall_resolved_native_function_restores_stack_rooted_arguments_after_growth_without_callable_value`

That test prepares a stack-rooted receiver plus argument near the stack tail, leaves the base slot as a plain null
value, then calls a resolved-native precall entry directly. It proves the runtime can still:

- grow and relocate the stack safely
- preserve stack-rooted receiver / argument inputs
- execute the native function without first reconstructing a native callable value in the base slot

The test failed first on WSL gcc with:

- `undefined reference to 'ZrCore_Function_PreCallResolvedNativeFunction'`

That red state confirmed the runtime still lacked a direct resolved-native precall entry.

### Runtime Change

`zr_vm_core/include/zr_vm_core/function.h` and `zr_vm_core/src/zr_vm_core/function.c` now expose:

- `ZrCore_Function_PreCallResolvedNativeFunction(...)`

`ZrCore_Function_PreCallKnownNativeValue(...)` now resolves the native function once, then forwards into that shared
helper instead of duplicating the final native-precall step inline.

`zr_vm_core/src/zr_vm_core/object/object_call.c` now resolves `nativeClosure->nativeFunction` once at the top of
`object_call_known_native_fast(...)` and enters the native frame via
`ZrCore_Function_PreCallResolvedNativeFunction(...)` instead of routing back through
`ZrCore_Function_PreCallKnownNativeValue(...)`.

This slice does not yet change the scratch stack layout, callable-slot copy, or native binding dispatcher contract.
It only removes the redundant callable-to-native-function re-resolution on the hottest known-native map path.

### Evidence

Against the accepted slice-44 profile snapshot:

- `map_object_access`: `116,460,641 -> 115,799,490 Ir` (`-0.57%`)
- `object_call_known_native_fast.part.0`: `4,418,428 -> 4,393,836 Ir` (`-0.56%`)
- `ZrCore_Function_PreCallKnownNativeValue`: `3,603,402 Ir` on slice 44, removed from the hotspot view here
- replacement `ZrCore_Function_PreCallResolvedNativeFunction`: `3,554,198 Ir` (`-1.37%` versus the old helper body)
- `stack_get_value` helper count: `231,618 -> 219,322` (`-5.31%`)
- `native_binding_dispatcher`: `5,959,795 -> 5,959,795 Ir` (flat)
- `dispatch_loops`: `473,985,777 -> 474,030,679 Ir` (`+0.01%`, flat / unchanged)

Fresh focused core reruns (`dispatch_loops,map_object_access`, `3` measured iterations):

- first rerun:
  - `dispatch_loops`: `284.782 ms`
  - `map_object_access`: `157.670 ms`
- repeat:
  - `dispatch_loops`: `273.800 ms`
  - `map_object_access`: `145.917 ms`
- second repeat:
  - `dispatch_loops`: `273.910 ms`
  - `map_object_access`: `170.503 ms`

Interpretation:

- the deterministic map-side callgrind gain is real, but intentionally narrow: this slice only removes duplicated
  native-function resolution inside the known-native bridge
- `map_object_access` wall-clock samples remain noisy, but the first repeat re-converged materially below the
  accepted slice-44 wall sample (`157.840 -> 145.917 ms`)
- `dispatch_loops` behaves exactly as the instruction profile predicts: effectively flat in callgrind, with the current
  wall-clock slowdown not supported by any matching profile movement on the untouched dispatch-side hotspots
- the remaining map-side cost is still the same cluster:
  `native_binding_dispatcher`, `object_call_known_native_fast`, `ZrCore_Stack_CopyValue`, and
  `ZrCore_Function_ReserveScratchSlots`

### Validation

Accepted slice-45 state:

- WSL gcc (`build-wsl-gcc`):
  - rebuilt `zr_vm_object_call_known_native_fast_path_test`
  - rebuilt `zr_vm_execution_dispatch_callable_metadata_test`
  - rebuilt `zr_vm_cli_executable`
  - `zr_vm_object_call_known_native_fast_path_test`: `12 Tests 0 Failures`
  - `zr_vm_execution_dispatch_callable_metadata_test`: `2 Tests 0 Failures`
  - CLI smoke `tests/fixtures/projects/hello_world/hello_world.zrp`: `hello world`
- WSL clang (`build-wsl-clang`):
  - rebuilt `zr_vm_object_call_known_native_fast_path_test`
  - rebuilt `zr_vm_execution_dispatch_callable_metadata_test`
  - rebuilt `zr_vm_cli_executable`
  - `zr_vm_object_call_known_native_fast_path_test`: `12 Tests 0 Failures`
  - `zr_vm_execution_dispatch_callable_metadata_test`: `2 Tests 0 Failures`
  - CLI smoke `tests/fixtures/projects/hello_world/hello_world.zrp`: `hello world`
- Windows MSVC CLI smoke (`build\codex-msvc-cli-debug`):
  - imported `VsDevCmd`
  - rebuilt target `zr_vm_cli_executable`
  - ran `tests/fixtures/projects/hello_world/hello_world.zrp`
  - output remained `hello world`

### Benchmark Artifacts

Slice-45 snapshots kept on disk:

- `build/benchmark-gcc-release/tests_generated/performance_profile_resolved_native_precall_after_full_20260417`
- `build/benchmark-gcc-release/tests_generated/performance_core_resolved_native_precall_after_full_20260417`
- `build/benchmark-gcc-release/tests_generated/performance_core_resolved_native_precall_after_full_repeat_20260417`
- `build/benchmark-gcc-release/tests_generated/performance_core_resolved_native_precall_after_full_repeat2_20260417`

## Current Hotspots / Next Slice (After Slice 45)

Fresh hotspot view after the resolved-native precall cut:

- `dispatch_loops`
  - `execution_member_get_cached = 51,048,885 Ir`
  - `execution_member_set_cached = 20,429,116 Ir`
  - `ZrCore_Function_PreCallResolvedVmFunction = 11,845,924 Ir`
  - this slice did not materially move the dispatch-side line
- `map_object_access`
  - `native_binding_dispatcher = 5,959,795 Ir`
  - `object_call_known_native_fast.part.0 = 4,393,836 Ir`
  - `ZrCore_Function_PreCallResolvedNativeFunction = 3,554,198 Ir`
  - `ZrCore_Stack_CopyValue = 913,998 Ir`
  - `ZrCore_Function_ReserveScratchSlots = 852,602 Ir`

Recommended next slice:

- stay on the known-native/object-call line for `map_object_access`
- target scratch-slot reservation / argument copy pressure or a narrower native-binding dispatcher lane before returning
  to mixed-type safe-concat work
- treat `dispatch_loops` as unchanged by this slice; only move back there if a new bounded member-get/member-set cut
  has deterministic profile evidence

## Slice 46: Known-Native Scratch Reservation Trim

### Root Cause

After slice 45, the remaining `map_object_access` object-call hotspot still showed:

- `object_call_known_native_fast.part.0 = 4,393,836 Ir`
- `ZrCore_Stack_CopyValue = 913,998 Ir`
- `ZrCore_Function_ReserveScratchSlots = 852,602 Ir`

The known-native object-call lane was already holding a permanent native closure plus stabilized operands, but it still
used the heavier scratch-slot reservation path and repeatedly bounced through profiled stack-slot plumbing while
preparing the scratch window.

### Regression Coverage

`tests/core/test_object_call_known_native_fast_path.c` gained:

- `test_object_call_known_native_fast_path_overwrites_prefilled_future_scratch_slots`

The test pre-fills the future scratch window with stale values, forces stack growth under the poisoning allocator, and
locks two contracts:

- known-native fast calls may overwrite future scratch slots directly instead of routing through the generic
  reservation helper
- stale ownership metadata in that future window must still be sanitized before the call executes

### Runtime Change

`zr_vm_core/src/zr_vm_core/object/object_call.c` now:

- uses `ZrCore_Function_CheckStackAndGc(...)` instead of `ReserveScratchSlots(...)` on the known-native fast lane
- writes receiver/arguments/callable into the scratch window before raising `stackTop` / outer `functionTop`
- clears only scratch destinations that still carry ownership metadata instead of blanking the whole window
- keeps the scratch-slot plumbing on `ZrCore_Stack_GetValueNoProfile(...)` so the new destination checks do not add
  helper-recording/TLS overhead back onto the hot path

### Evidence

Against the accepted slice-45 profile snapshot:

- `map_object_access`: `115,799,490 -> 114,706,895 Ir` (`-0.94%`)
- `ZrCore_Function_ReserveScratchSlots`: `852,602 -> 172,210 Ir` (`-79.80%`)
- `stack_get_value` helper count: `219,322 -> 207,026` (`-5.60%`)
- `dispatch_loops`: `474,030,679 -> 473,989,260 Ir` (`-0.01%`, flat)

Focused core rerun vs slice 45:

- `map_object_access`: `157.670 -> 125.216 ms`
- `dispatch_loops`: `284.782 -> 299.539 ms`

Interpretation:

- the first draft of this slice removed `ReserveScratchSlots(...)` cost but reintroduced too much profiled
  `stack_get_value` plumbing; that version was rejected
- the kept version restores the deterministic profile win by leaving the overwrite fix in place while moving the
  new scratch-slot reads onto no-profile plumbing
- `map_object_access` now shows a real deterministic win on the intended object-call line
- current wall samples remain noisy on `dispatch_loops`, but the untouched dispatch-side callgrind shape stays flat

### Validation

Accepted slice-46 state:

- WSL gcc (`build-wsl-gcc`):
  - rebuilt `zr_vm_object_call_known_native_fast_path_test`
  - rebuilt `zr_vm_execution_dispatch_callable_metadata_test`
  - rebuilt `zr_vm_cli_executable`
  - `zr_vm_object_call_known_native_fast_path_test`: `13 Tests 0 Failures`
  - `zr_vm_execution_dispatch_callable_metadata_test`: `2 Tests 0 Failures`
  - CLI smoke `tests/fixtures/projects/hello_world/hello_world.zrp`: `hello world`
- WSL clang (`build-wsl-clang`):
  - rebuilt `zr_vm_object_call_known_native_fast_path_test`
  - rebuilt `zr_vm_execution_dispatch_callable_metadata_test`
  - rebuilt `zr_vm_cli_executable`
  - `zr_vm_object_call_known_native_fast_path_test`: `13 Tests 0 Failures`
  - `zr_vm_execution_dispatch_callable_metadata_test`: `2 Tests 0 Failures`
  - CLI smoke `tests/fixtures/projects/hello_world/hello_world.zrp`: `hello world`
- Windows MSVC CLI smoke (`build\\codex-msvc-cli-debug`):
  - imported `VsDevCmd`
  - rebuilt target `zr_vm_cli_executable`
  - ran `tests/fixtures/projects/hello_world/hello_world.zrp`
  - output remained `hello world`

### Benchmark Artifacts

Slice-46 snapshots kept on disk:

- `build/benchmark-gcc-release/tests_generated/performance_profile_object_call_scratch_trim_noprofile_after_20260417`
- `build/benchmark-gcc-release/tests_generated/performance_core_object_call_scratch_trim_noprofile_after_20260417`

## Current Hotspots / Next Slice (After Slice 46)

Fresh hotspot view after the kept scratch-reservation trim:

- `dispatch_loops`
  - `execution_member_get_cached = 51,048,885 Ir`
  - `execution_member_set_cached = 20,429,116 Ir`
  - `ZrCore_Function_PreCallResolvedVmFunction = 11,845,924 Ir`
- `map_object_access`
  - `native_binding_dispatcher = 5,959,795 Ir`
  - `object_call_known_native_fast.part.0 = 4,565,984 Ir`
  - `ZrCore_Function_PreCallResolvedNativeFunction = 3,554,198 Ir`
  - `ZrCore_Stack_CopyValue = 913,998 Ir`
  - `ZrCore_Function_ReserveScratchSlots = 172,210 Ir` is no longer the main follow-up

Recommended next slice:

- stay on the known-native/object-call line for `map_object_access`
- move next into a narrower `native_binding_dispatcher` fixed-shape lane before returning to mixed-type safe-concat
- treat the remaining `object_call_known_native_fast` work as secondary unless a smaller copy/layout cut shows
  deterministic gains on top of the dispatcher lane

## Slice 47: Native Binding Dispatcher Stack-Read No-Profile Trim

### Root Cause

After slice 46, the map-side hotspot had shifted cleanly into the native binding dispatcher:

- `native_binding_dispatcher = 5,959,795 Ir`
- `object_call_known_native_fast.part.0 = 4,565,984 Ir`
- `stack_get_value` helper count still at `207,026`

The current map/index-contract workload was already entering the inline pinned lane, but the dispatcher file still paid
helper-recording/TLS overhead on repeated internal `ZrCore_Stack_GetValue(...)` reads even though those reads are pure
runtime plumbing.

### Regression Coverage

No new semantic coverage was needed for this trim; the slice keeps the same native binding contracts and reuses the
existing focused regression surface:

- `tests/module/test_module_system.c`
  - `test_native_binding_inline_label_inspector_keeps_raw_layout_without_losing_gc_safety`
  - `test_container_map_runtime_supports_computed_key_access`
- `tests/core/test_object_call_known_native_fast_path.c`
  - stack-rooted and non-stack known-native get/set-by-index coverage
  - shared known-native library-call bridge coverage

### Runtime Change

`zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c` now keeps internal stack-slot reads on
`ZrCore_Stack_GetValueNoProfile(...)`, matching the earlier object-call trim:

- the dispatcher still exposes the same callback layout and stable-copy/pinning behavior
- only helper-recording/TLS work on repeated internal stack reads was removed

### Evidence

Against the accepted slice-46 profile snapshot:

- `map_object_access`: `114,706,895 -> 112,031,005 Ir` (`-2.33%`)
- `native_binding_dispatcher`: `5,959,795 -> 5,738,433 Ir` (`-3.71%`)
- `stack_get_value` helper count: `207,026 -> 53,303` (`-74.25%`)
- `dispatch_loops`: `473,989,260 -> 473,437,856 Ir` (`-0.12%`)

Focused core rerun vs slice 46:

- `map_object_access`: `125.216 -> 114.447 ms`
- `dispatch_loops`: `299.539 -> 429.293 ms`

Interpretation:

- this cut lands directly on the intended native-binding dispatcher cost center
- the large helper-count drop confirms most remaining `stack_get_value` traffic on `map_object_access` was dispatcher
  bookkeeping, not useful execution work
- `map_object_access` shows a clear deterministic win and a matching wall-clock improvement on this run
- `dispatch_loops` callgrind stays essentially flat-to-better; current wall-clock noise is not supported by any
  matching dispatch-side hotspot regression

### Validation

Accepted slice-47 state:

- WSL gcc (`build-wsl-gcc`):
  - rebuilt `zr_vm_module_system_test`
  - rebuilt `zr_vm_object_call_known_native_fast_path_test`
  - rebuilt `zr_vm_execution_dispatch_callable_metadata_test`
  - rebuilt `zr_vm_cli_executable`
  - `zr_vm_module_system_test`: `86 Tests 0 Failures`
  - `zr_vm_object_call_known_native_fast_path_test`: `13 Tests 0 Failures`
  - `zr_vm_execution_dispatch_callable_metadata_test`: `2 Tests 0 Failures`
  - CLI smoke `tests/fixtures/projects/hello_world/hello_world.zrp`: `hello world`
- WSL clang (`build-wsl-clang`):
  - rebuilt `zr_vm_module_system_test`
  - rebuilt `zr_vm_object_call_known_native_fast_path_test`
  - rebuilt `zr_vm_execution_dispatch_callable_metadata_test`
  - rebuilt `zr_vm_cli_executable`
  - `zr_vm_module_system_test`: `86 Tests 0 Failures`
  - `zr_vm_object_call_known_native_fast_path_test`: `13 Tests 0 Failures`
  - `zr_vm_execution_dispatch_callable_metadata_test`: `2 Tests 0 Failures`
  - CLI smoke `tests/fixtures/projects/hello_world/hello_world.zrp`: `hello world`
- Windows MSVC CLI smoke (`build\\codex-msvc-cli-debug`):
  - imported `VsDevCmd`
  - rebuilt target `zr_vm_cli_executable`
  - ran `tests/fixtures/projects/hello_world/hello_world.zrp`
  - output remained `hello world`

### Benchmark Artifacts

Slice-47 snapshots kept on disk:

- `build/benchmark-gcc-release/tests_generated/performance_profile_native_binding_stack_get_noprofile_after_20260417`
- `build/benchmark-gcc-release/tests_generated/performance_core_native_binding_stack_get_noprofile_after_20260417`

## Current Hotspots / Next Slice (After Slice 47)

Fresh hotspot view after the dispatcher stack-read trim:

- `dispatch_loops`
  - `execution_member_get_cached = 51,048,885 Ir`
  - `execution_member_set_cached = 20,429,116 Ir`
  - `ZrCore_Function_PreCallResolvedVmFunction = 11,845,924 Ir`
- `map_object_access`
  - `native_binding_dispatcher = 5,738,433 Ir`
  - `object_call_known_native_fast.part.0 = 4,565,984 Ir`
  - `ZrCore_Function_PreCallResolvedNativeFunction = 3,554,198 Ir`
  - `ZrCore_Stack_CopyValue = 913,998 Ir`

Recommended next slice:

- stay on `map_object_access`
- split the inline pinned dispatcher lane into narrower fixed-shape `self + 1 arg` / `self + 2 args` paths before
  revisiting mixed-type safe-concat
- leave `object_call_known_native_fast` as the follow-up only if the narrower dispatcher lane stops paying off

## Slice 48: Fixed-Shape Native Binding Lane With Hot-Helper Re-Inlining

### Root Cause

The first fixed-shape dispatcher-lane extraction proved the shape split was directionally right, but the extracted build
was not acceptable as-is.

The rejected intermediate profile showed:

- `map_object_access`: `112,031,005 -> 114,133,382 Ir` (`+1.88%`)
- new standalone hot helpers:
  - `native_binding_pin_value_object = 1,776,266 Ir`
  - `native_binding_prepare_stable_value_raw = 975,524 Ir`
  - `native_binding_unpin_value_object = 631,970 Ir`
  - `native_binding_can_use_fast_lane = 590,250 Ir`

Interpretation:

- the fixed-shape `self + 1 arg` / `self + 2 args` split itself was not the problem
- the regression came from pulling formerly same-TU tiny helpers into external functions, which added call overhead
  back onto the `map_object_access` hot path

### Regression Coverage

No new semantic regression surface was required for the accepted version.

This slice keeps the same callable layout, GC pinning contracts, and receiver writeback semantics, so it reuses the
focused surface already covering the native binding line:

- `tests/module/test_module_system.c`
  - `test_native_binding_inline_label_inspector_keeps_raw_layout_without_losing_gc_safety`
  - `test_container_map_runtime_supports_computed_key_access`
- `tests/core/test_object_call_known_native_fast_path.c`
  - known-native stack-root and non-stack input coverage
  - stable-value copy/release coverage
  - registry cache promotion coverage

### Runtime Change

`zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.c` and
`zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h` now keep the fixed-shape lane split
while restoring hot helper inlining:

- the extracted lane module stays in place so the main dispatcher file no longer has to carry the fixed-shape bodies
- pin/unpin, stable-copy, fast-lane eligibility, and receiver writeback helpers moved to header inlined form
- `native_binding_prepare_stable_value(...)` / `native_binding_release_stable_value(...)` remain as wrappers for tests
  and existing call sites, but the hot dispatcher/lane path now calls the inline forms directly
- `native_binding_dispatch.c` keeps using the narrower fixed-shape lane selection without paying the extra helper-call
  tax introduced by the first extraction draft

### Evidence

Against the accepted slice-47 profile snapshot:

- `map_object_access`: `112,031,005 -> 110,966,247 Ir` (`-0.95%`)
- `dispatch_loops`: `473,437,856 -> 473,528,120 Ir` (`+0.02%`, flat)

Fresh slice-48 hotspot picture on `map_object_access`:

- `native_binding_dispatch_inline_pinned_lane = 2,885,516 Ir`
- `native_binding_dispatcher = 2,496,270 Ir`
- `object_call_known_native_fast.part.0 = 4,565,984 Ir`
- `ZrCore_Function_PreCallResolvedNativeFunction = 3,554,198 Ir`

Interpretation:

- the fixed-shape lane is now kept because the total case regressed in the first extraction draft but wins once the hot
  helper calls are re-inlined
- the dispatcher cost center has been split into a thinner front-half plus the dedicated inline pinned lane, which is
  the intended W2 direction for later specialization
- `dispatch_loops` remains effectively flat, so this cut is still isolated to the intended `map_object_access`
  known-native/object-call line

Focused core rerun on the accepted slice-48 state:

- `dispatch_loops`: `224.640 ms`
- `map_object_access`: `112.375 ms`

### Validation

Accepted slice-48 state:

- WSL gcc (`build/codex-wsl-gcc-debug`):
  - rebuilt `zr_vm_module_system_test`
  - rebuilt `zr_vm_object_call_known_native_fast_path_test`
  - rebuilt `zr_vm_execution_dispatch_callable_metadata_test`
  - rebuilt `zr_vm_cli_executable`
  - `zr_vm_module_system_test`: `86 Tests 0 Failures`
  - `zr_vm_object_call_known_native_fast_path_test`: `13 Tests 0 Failures`
  - `zr_vm_execution_dispatch_callable_metadata_test`: `2 Tests 0 Failures`
  - CLI smoke `tests/fixtures/projects/hello_world/hello_world.zrp`: `hello world`
- WSL clang (`build/codex-wsl-clang-debug`):
  - rebuilt `zr_vm_module_system_test`
  - rebuilt `zr_vm_object_call_known_native_fast_path_test`
  - rebuilt `zr_vm_execution_dispatch_callable_metadata_test`
  - rebuilt `zr_vm_cli_executable`
  - `zr_vm_module_system_test`: `86 Tests 0 Failures`
  - `zr_vm_object_call_known_native_fast_path_test`: `13 Tests 0 Failures`
  - `zr_vm_execution_dispatch_callable_metadata_test`: `2 Tests 0 Failures`
  - CLI smoke `tests/fixtures/projects/hello_world/hello_world.zrp`: `hello world`
- Windows MSVC CLI smoke (`build\\codex-msvc-cli-debug`):
  - rebuilt target `zr_vm_cli_executable`
  - ran `tests/fixtures/projects/hello_world/hello_world.zrp`
  - output remained `hello world`

### Benchmark Artifacts

Slice-48 snapshots kept on disk:

- `build/benchmark-gcc-release/tests_generated/performance_profile_native_binding_lane_inline_recovery_after_20260417`
- `build/benchmark-gcc-release/tests_generated/performance_core_native_binding_lane_inline_recovery_after_20260417`

## Current Hotspots / Next Slice (After Slice 48)

Fresh hotspot view after the accepted fixed-shape lane + inline-recovery cut:

- `dispatch_loops`
  - `execution_member_get_cached = 51,048,885 Ir`
  - `execution_member_set_cached = 20,429,116 Ir`
  - `ZrCore_Function_PreCallResolvedVmFunction = 11,845,924 Ir`
- `map_object_access`
  - `object_call_known_native_fast.part.0 = 4,565,984 Ir`
  - `garbage_collector_ignore_registry_contains = 4,276,688 Ir`
  - `ZrCore_Function_PreCallResolvedNativeFunction = 3,554,198 Ir`
  - `native_binding_dispatch_inline_pinned_lane = 2,885,516 Ir`
  - `native_binding_dispatcher = 2,496,270 Ir`

Recommended next slice:

- stay on `map_object_access`
- return to the hotter known-native/object-call line before mixed-type safe-concat
- target the remaining `object_call_known_native_fast.part.0` / `PreCallResolvedNativeFunction` overlap, now that the
  fixed-shape native binding lane is no longer the first blocker

## Slice 49: Direct Stack-Root Native Binding Lane

### Context

Several later exploratory slices on the `map_object_access` line were intentionally left out of acceptance because they
only moved local hotspots without producing a repeatable whole-case win.

Before slice 49, the fresh focused baseline for the active line was:

- `dispatch_loops`: `470,311,382 Ir`
- `map_object_access`: `74,448,004 Ir`

The remaining dominant accepted blocker on `map_object_access` was no longer the generic object-call bridge. It was the
native binding dispatcher still paying the inline pinned lane for callbacks that only needed direct, stack-backed
`self/argument` access plus relocation safety during temp-root / GC activity.

### Root Cause

Fresh call-tree evidence on the active baseline showed:

- `native_binding_dispatch_inline_pinned_lane` inclusive: `28,827,614 Ir`
- children:
  - `zr_container_map_set_item`
  - `zr_container_map_get_item`

The pinned lane was still stabilizing and pinning `self/arguments` even when the callback only needed:

- direct stack-backed layout
- argument re-read safety after temp roots / full GC
- no detached stable copies

That meant `map_object_access` still paid avoidable stable-copy / pin / unpin overhead inside the hottest map meta
method callbacks.

### Regression Coverage

`tests/module/test_module_system.c` tightened
`test_native_binding_inline_label_inspector_keeps_raw_layout_without_losing_gc_safety`.

The probe-native `inspectLabel()` callback now also asserts:

- `directStackContext == 1`

which locks the stronger contract for the accepted lane:

- native callbacks may opt into a direct stack-root context
- `self` and arguments stay stack-backed instead of being pre-copied into stable dispatcher buffers
- callbacks may still re-read arguments after temp-root churn and forced full GC

### Runtime Change

`zr_vm_library/include/zr_vm_library/native_binding.h` now adds an explicit
`ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT` opt-in on native descriptors plus anchored stack-layout metadata on
`ZrLibCallContext`.

`zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h`,
`zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.c`, and
`zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c` now:

- select a new `native_binding_dispatch_stack_root_lane(...)` before the copy/pin lanes when the descriptor opts in
- anchor the original `functionBase` so callbacks can refresh `self/argument` pointers after stack relocation
- refresh stack-backed `self/argument` access in `ZrLib_CallContext_Self(...)` and `ZrLib_CallContext_Argument(...)`
  instead of forcing stable copies up front
- keep receiver writeback semantics unchanged

The first consumers are:

- `tests/module/test_module_system.c`
  - probe-native `inspectLabel()`
- `zr_vm_lib_container/src/zr_vm_lib_container/module.c`
  - `Map` meta `getItem`
  - `Map` meta `setItem`

### Evidence

Against the fresh pre-slice-49 focused baseline:

- `map_object_access`: `74,448,004 -> 72,621,921 Ir` (`-2.45%`)
- `dispatch_loops`: `470,311,382 -> 470,319,904 Ir` (`+0.00%`, flat)

Fresh slice-49 hotspot picture on `map_object_access`:

- `native_binding_dispatch_stack_root_lane = 27,311,235 Ir` inclusive
- `zr_container_map_set_item = 15,124,120 Ir` inclusive under that lane
- `zr_container_map_get_item = 10,969,811 Ir` inclusive under that lane
- `garbage_collector_ignore_registry_contains = 2,537,901 Ir`

Focused wall on the same run:

- `dispatch_loops = 141.086 ms`
- `map_object_access = 101.303 ms`

Interpretation:

- the accepted win is not a benchmark-local special case; it comes from a shared native-binding/runtime lane
- the native binding line no longer spends its hottest map path on stable-copy / pin scaffolding first
- `dispatch_loops` stayed flat, so the cut remains isolated to the intended `map_object_access` line
- the next hotter uncovered costs moved inward to `zr_container_map_find_index.part.0` and `ZrLib_Array_Get`

### Validation

Accepted slice-49 state:

- WSL gcc (`build/codex-wsl-gcc-debug`):
  - rebuilt `zr_vm_module_system_test`
  - `zr_vm_module_system_test`: `86 Tests 0 Failures`
  - `zr_vm_object_call_known_native_fast_path_test`: `17 Tests 0 Failures`
  - `zr_vm_instructions_test`: `88 Tests 0 Failures`
- WSL clang (`build/codex-wsl-clang-debug`):
  - rebuilt `zr_vm_module_system_test`
  - rebuilt `zr_vm_object_call_known_native_fast_path_test`
  - rebuilt `zr_vm_instructions_test`
  - `zr_vm_module_system_test`: `86 Tests 0 Failures`
  - `zr_vm_object_call_known_native_fast_path_test`: `17 Tests 0 Failures`
  - `zr_vm_instructions_test`: `88 Tests 0 Failures`
- Windows MSVC CLI smoke (`build\\codex-msvc-cli-debug`):
  - rebuilt target `zr_vm_cli_executable`
  - ran `tests/fixtures/projects/hello_world/hello_world.zrp`
  - output remained `hello world`

### Benchmark Artifacts

Slice-49 snapshot kept on disk:

- `build/benchmark-gcc-release/tests_generated/performance_profile_native_binding_stack_root_lane_after_20260417`

## Current Hotspots / Next Slice (After Slice 49)

Fresh hotspot view after the accepted direct stack-root lane:

- `dispatch_loops`
  - `execution_member_get_cached = 51,048,885 Ir`
  - `execution_member_set_cached = 20,429,116 Ir`
  - `ZrCore_Function_PreCallResolvedVmFunction = 11,845,924 Ir`
- `map_object_access`
  - `zr_container_map_find_index.part.0 = 10,003,967 Ir`
  - `ZrLib_Array_Get = 6,237,900 Ir`
  - `zr_container_get_own_field_value_fast = 3,289,939 Ir`
  - `garbage_collector_ignore_registry_contains = 2,537,901 Ir`

Recommended next slice:

- stay on `map_object_access`
- cut `zr_container_map_find_index.part.0` first, because it now dominates both `Map` get and set traffic
- treat `ZrLib_Array_Get` as the likely companion cut, ideally by removing repeated generic array object lookups from
  the same map-entry scan path

## Slice 50: Dense Array Pair-Read Fast Path For Map Entry Scans

### Context

After the accepted stack-root native binding lane, the active focused baseline on the current line was:

- `dispatch_loops`: `470,319,904 Ir`
- `map_object_access`: `72,621,921 Ir`

The remaining dominant `map_object_access` blocker had moved inside the container runtime itself:

- `zr_container_map_find_index.part.0 = 10,003,967 Ir`
- `ZrLib_Array_Get = 6,237,900 Ir`
- `zr_container_get_own_field_value_fast = 3,289,939 Ir`

That meant the hottest map meta callbacks were no longer limited by native binding setup first. They were now paying
repeated generic array element lookups while scanning the dense backing array of `Pair<K,V>` entries.

### Root Cause

`Map` stores entries in an internal array-like object whose backing storage is maintained on the dense sequential int-key
path. But `zr_container_map_find_index(...)`, `zr_container_map_get_item(...)`, and `zr_container_map_set_item(...)`
were still reading those entries through:

- `ZrLib_Array_Get(...)`
- followed by repeated object/value unpacking
- followed by generic string-field lookup on the `Pair`

On the accepted slice-49 baseline, that left `ZrLib_Array_Get` as the second hottest map-specific helper even though the
data was already sitting in directly index-addressable dense buckets.

### Runtime Change

`zr_vm_lib_container/src/zr_vm_lib_container/module.c` now adds a local dense-array read helper pair:

- `zr_container_array_get_value_fast(...)`
- `zr_container_array_get_object_fast(...)`

These helpers:

- detect the steady-state dense sequential int-key layout directly on the backing array object
- read the entry pair from `nodeMap.buckets[index]` when the bucket is directly index-addressable
- fall back to `ZrLib_Array_Get(...)` only when the storage is not on the dense fast path

`Map` runtime paths now use that fast read in:

- `zr_container_map_find_index(...)`
- `zr_container_map_get_item(...)`
- `zr_container_map_set_item(...)`

The same slice also pushed more of the container-owned field traffic onto the existing local fast field helpers so the
entry-scan path stays on container-owned exact fields instead of re-entering the more generic object/member path.

### Evidence

Against the fresh pre-slice-50 focused baseline:

- `map_object_access`: `72,621,921 -> 67,033,624 Ir` (`-7.70%`)
- `dispatch_loops`: `470,319,904 -> 470,325,039 Ir` (`+0.00%`, flat)

Focused wall on the same run:

- `dispatch_loops`: `141.086 -> 144.550 ms`
- `map_object_access`: `101.303 -> 95.464 ms`

Fresh slice-50 hotspot picture on `map_object_access`:

- `zr_container_map_find_index.part.0 = 5,958,205 Ir`
- `zr_container_get_own_field_value_fast = 3,289,599 Ir`
- `garbage_collector_ignore_registry_contains = 2,537,901 Ir`
- `ZrLib_Array_Get` dropped out of the new top-function surface

Interpretation:

- the intended `Map` entry-scan line moved materially, not just locally
- the dense-array fast read successfully removed a first-order generic helper from the hot loop
- `dispatch_loops` stayed flat, so the win remains isolated to the intended `map_object_access` path
- the next worthwhile cut now sits one layer deeper in exact own-field lookup and residual key hash/equality work

### Validation

Accepted slice-50 state:

- WSL gcc (`build/codex-wsl-gcc-debug`):
  - `zr_vm_module_system_test`: `86 Tests 0 Failures`
  - `zr_vm_container_runtime_test`: `35 Tests 0 Failures`
- WSL clang (`build/codex-wsl-clang-debug`):
  - `zr_vm_module_system_test`: `86 Tests 0 Failures`
  - `zr_vm_container_runtime_test`: `35 Tests 0 Failures`
- Windows MSVC CLI smoke (`build\\codex-msvc-cli-debug`):
  - rebuilt target `zr_vm_cli_executable`
  - ran `tests/fixtures/projects/hello_world/hello_world.zrp`
  - output remained `hello world`

### Benchmark Artifacts

Slice-50 snapshot kept on disk:

- `build/benchmark-gcc-release/tests_generated/performance_profile_container_dense_array_pair_fast_after_20260417`

## Current Hotspots / Next Slice (After Slice 50)

Fresh hotspot view after the accepted dense-array pair-read cut:

- `dispatch_loops`
  - `execution_member_get_cached = 51,048,885 Ir`
  - `execution_member_set_cached = 20,429,116 Ir`
  - `ZrCore_Function_PreCallResolvedVmFunction = 11,845,924 Ir`
- `map_object_access`
  - `zr_container_map_find_index.part.0 = 5,958,205 Ir`
  - `zr_container_get_own_field_value_fast = 3,289,599 Ir`
  - `garbage_collector_ignore_registry_contains = 2,537,901 Ir`
  - `zr_container_value_hash.part.0 = 1,721,120 Ir`
  - `zr_container_values_equal = 959,573 Ir`

Recommended next slice:

- stay on `map_object_access`
- keep cutting `zr_container_map_find_index.part.0`, because it still dominates both `getItem` and `setItem`
- prioritize exact own-string pair lookup plus a direct string-key compare path before moving away from the current map
  entry scan line

## Slice 51: Lazy Construct-Target Prototype Resolution On Cached Stack-Root Native Calls

### Context

On the current W2 line, the focused accepted baseline before this slice was:

- `dispatch_loops`: `467,794,364 Ir`
- `map_object_access`: `44,547,276 Ir`

The active `map_object_access` hotspot stack had moved to the known-native/container side:

- `native_binding_dispatch_cached_stack_root_one_argument = 1,475,280 Ir`
- `object_call_known_native_fast_one_argument.part.0 = 1,360,536 Ir`
- `object_complete_known_native_fast_call = 1,192,712 Ir`
- `ZrCore_Function_CallPreparedResolvedNativeFunctionSingleResultFast = 909,904 Ir`

### Rejected Side Cut

This slice first tried extending the prepared single-result native fast helper so object-call could pass a stack
`returnDestination` directly.

That behavior is still covered by:

- `tests/core/test_object_call_known_native_fast_path.c`
  - `test_call_prepared_resolved_native_function_single_result_fast_supports_stack_return_destination`

but it was **not** kept on the object-call hot path because the fresh profile evidence regressed:

- `map_object_access`: `44,547,276 -> 44,678,125 Ir` (`+0.29%`)

Interpretation:

- the helper extension is functionally valid
- but on the current known-native path it only moved a cheap copy instead of deleting a real one
- the extra targeting/anchor cost did not pay back on `map_object_access`

So the accepted runtime path deliberately keeps object-call on the previous copy-back behavior.

### Root Cause

The accepted cut came from the native binding side instead.

`native_binding_prepare_cached_stack_root_dispatch(...)` and the generic native dispatcher were eagerly calling
`ZrCore_Object_IsInstanceOfPrototype(...)` every time just to precompute `constructTargetPrototype`, even though the
value is only needed by a narrow set of constructor helpers.

For hot `Map` meta `getItem` / `setItem` traffic, that prototype derivation was dead work paid on every cached stack-root
dispatch.

### Runtime Change

`zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_cached.c` and
`zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c` now stop eagerly materializing
`constructTargetPrototype` during cached/generic native dispatch setup.

`ZrLib_CallContext_GetConstructTargetPrototype(...)` now resolves the actual construct target lazily by:

- refreshing the stack-backed call layout only when needed
- checking the current `selfValue` against `ownerPrototype` on demand
- returning the derived instance prototype only for callers that actually query it

This keeps ordinary method/meta-method dispatch off that prototype walk entirely.

### Evidence

Against the fresh pre-slice-51 focused baseline:

- `map_object_access`: `44,547,276 -> 44,187,075 Ir` (`-0.81%`)
- `dispatch_loops`: `467,794,364 -> 467,802,558 Ir` (`+0.00%`, noise)

Fresh slice-51 hotspot picture on `map_object_access`:

- `native_binding_dispatch_cached_stack_root_one_argument = 1,294,968 Ir`
- `object_call_known_native_fast_one_argument.part.0 = 1,360,536 Ir`
- `object_complete_known_native_fast_call = 1,205,008 Ir`
- `ZrCore_Function_CallPreparedResolvedNativeFunctionSingleResultFast = 1,008,272 Ir`
- `ZrLib_TempValueRoot_Begin = 542,836 Ir`
- `ZrLib_TempValueRoot_End = 238,108 Ir`

Interpretation:

- the accepted gain is real and lands exactly on the cached stack-root native binding line
- the object-call line remains hot, but the attempted direct `returnDestination` cut is not worth keeping there
- the next `map_object_access` target should stay on native/container fixed costs, especially:
  - `native_binding_dispatch_cached_stack_root_one_argument`
  - `ZrLib_TempValueRoot_Begin`
  - `ZrLib_TempValueRoot_End`

### Validation

Accepted slice-51 state:

- WSL gcc (`build-wsl-gcc`):
  - `zr_vm_object_call_known_native_fast_path_test`: `25 Tests 0 Failures`
  - `zr_vm_container_runtime_test`: `37 Tests 0 Failures`
  - `zr_vm_module_system_test`: `87 Tests 0 Failures`
  - `zr_vm_execution_member_access_fast_paths_test`: `46 Tests 0 Failures`
- WSL clang (`build-wsl-clang`):
  - `zr_vm_object_call_known_native_fast_path_test`: `25 Tests 0 Failures`
  - `zr_vm_container_runtime_test`: `37 Tests 0 Failures`
  - `zr_vm_module_system_test`: `87 Tests 0 Failures`
  - `zr_vm_execution_member_access_fast_paths_test`: `46 Tests 0 Failures`
- Windows MSVC CLI smoke (`build\\codex-msvc-cli-debug`):
  - rebuilt target `zr_vm_cli_executable`
  - ran `tests/fixtures/projects/hello_world/hello_world.zrp`
  - output remained `hello world`
- Fresh focused benchmarks (`build/benchmark-gcc-release`):
  - profile tier rerun passed on `dispatch_loops,map_object_access`
  - core tier rerun passed on `dispatch_loops,map_object_access`

## Slice 55: `Array_PushValue` Temp-Root Slot Churn Removal

### Context

After slice 54, the hottest remaining `map_object_access` container-side costs were no longer the generic index-contract
wrappers. The visible map insert line still contained:

- `zr_container_map_set_item = 669,379 Ir`
- `ZrLib_TempValueRoot_Begin = 542,836 Ir`
- `ZrLib_TempValueRoot_End = 238,108 Ir`
- `ZrCore_GarbageCollector_IgnoreObject = 229,872 Ir`
- `ZrCore_GarbageCollector_UnignoreObject = 177,034 Ir`

Reading through `zr_container_map_set_item(...)` showed that fresh pair insertion still routed array append through the
shared `ZrLib_Array_PushValue(...)` helper, and that helper was still materializing GC values through
`ZrLib_TempValueRoot_Begin/SetValue/End` scratch-slot plumbing.

### Root Cause

On the hot fresh-insert path, `ZrLib_Array_PushValue(...)` already knows the exact destination slot and only needs the
source value to stay live across capacity growth / element write. The old helper still paid the heavier generic temp-root
sequence:

- reserve a scratch stack slot through `ZrLib_TempValueRoot_Begin(...)`
- copy the source value into that slot
- write from the temp root back into the destination object slot
- tear the temp root down again

For `map_object_access`, that was redundant insurance. The array object and GC value can be pinned directly during the
write, so the scratch-slot churn was pure steady-state overhead on the shared insert helper.

### Regression Coverage

This slice keeps the existing container runtime GC-object preservation contract and adds a tighter scratch-slot contract
in:

- `tests/container/test_container_runtime.c`
  - `test_container_native_binding_temp_roots_preserve_gc_object_values_without_extra_pin_scope`
  - `test_container_array_push_value_preserves_future_scratch_slots_for_gc_values`

The new test locks the important observable behavior directly: `ZrLib_Array_PushValue(...)` must preserve future
scratch slots instead of consuming them as hidden temp-root storage when pushing a GC object value.

### Runtime Change

`zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c` now narrows
`ZrLib_Array_PushValue(...)` for the hot GC-value lane:

- pin the array object and GC value directly
- write the incoming value into the final array slot with `ZrCore_Object_SetValue(...)`
- unpin on exit

The helper no longer routes this lane through `ZrLib_TempValueRoot_Begin/SetValue/End`, so the shared append path keeps
its existing semantics without spending scratch-stack setup/teardown on every inserted map entry.

### Evidence

Against the accepted slice-54 map-only profile baseline:

- `map_object_access`: `28,867,249 -> 28,694,492 Ir` (`-0.60%`)
- `ZrLib_TempValueRoot_Begin`: `542,836 -> 486,552 Ir` (`-10.37%`)
- `ZrLib_TempValueRoot_End`: `238,108 -> 213,400 Ir` (`-10.38%`)
- `ZrCore_GarbageCollector_IgnoreObject`: `229,872 -> 219,832 Ir` (`-4.37%`)
- `ZrCore_GarbageCollector_UnignoreObject`: `177,034 -> 170,006 Ir` (`-3.97%`)
- `ZrLib_Array_PushValue`: `18,278 -> 29,174 Ir` (`+59.61%`)

`zr_container_map_set_item` itself stayed flat at `669,379 Ir`, so the gain here is not a large top-level map symbol
drop. The acceptance case is that the shared append helper now does more of its own work directly while the old
temp-root shell materially shrinks, and the total benchmark `Ir` still falls instead of shifting upward into pin/unpin
cost.

Fresh map-only core wall on the same code stayed in the same band and improved slightly versus the slice-54 archive:

- median wall: `105.461 ms -> 104.689 ms` (`-0.73%`)
- mean wall: `119.706 ms -> 107.204 ms`

Interpretation:

- this slice is acceptance-worthy because it deletes real shared-helper stack-slot churn from the hot fresh-insert lane
- the gain is modest and mostly internal to `Array_PushValue(...)`, but it is directionally clean in profile evidence
- `IgnoreObject/UnignoreObject` did not balloon; they also declined, so the temp-root cost was not merely moved
- the next profitable work should now return to the still-hot `map_object_access` object/container line:
  - `zr_container_map_get_item`
  - `ZrCore_Object_GetByIndexUnchecked`
  - any remaining known-native/object-call cost that resurfaces after the container cuts

### Validation

Accepted slice-55 state:

- WSL gcc (`build-wsl-gcc`):
  - `zr_vm_container_runtime_test`: `41 Tests 0 Failures`
  - `zr_vm_object_call_known_native_fast_path_test`: `47 Tests 0 Failures`
- WSL clang (`build-wsl-clang`):
  - `zr_vm_container_runtime_test`: `41 Tests 0 Failures`
  - `zr_vm_object_call_known_native_fast_path_test`: `47 Tests 0 Failures`
- Windows MSVC CLI smoke (`build-msvc-cli-smoke`):
  - rebuilt target `zr_vm_cli_executable`
  - ran `tests/fixtures/projects/hello_world/hello_world.zrp`
  - output remained `hello world`
- Fresh focused benchmarks (`build/benchmark-gcc-release`):
  - existing map-only core rerun archived on the slice code
  - fresh map-only profile rerun passed with callgrind counting enabled
  - archived evidence directories:
    - `build/benchmark-gcc-release/tests_generated/performance_core_array_push_pin_only_after_20260417_map_only`
    - `build/benchmark-gcc-release/tests_generated/performance_profile_array_push_pin_only_after_20260417_map_only`

## Slice 57: Cached Index-Contract Direct Fast-Callback Lane

### Context

After slice 56, `map_object_access` had already switched the `Map` meta-method body over to readonly-inline fast
callbacks, but the runtime still paid two shared shells on every hot get/set:

- `ZrCore_Object_TryCallIndexContractDirectBindingReadonlyInlineOneArgumentStack = 573,720 Ir`
- `ZrCore_Object_TryCallIndexContractDirectBindingReadonlyInlineTwoArgumentsNoResultStack = 307,500 Ir`

The surrounding object helpers were still visible too:

- `ZrCore_Object_GetByIndexUnchecked = 573,748 Ir`
- `ZrCore_Object_SetByIndexUnchecked = 299,328 Ir`

That meant the callback body was already narrow, but `object.c` still asked a generic readonly-inline wrapper to
re-validate the same stack/debug/flag facts every time.

### Root Cause

The first follow-up cut on this line only reordered the runtime so cached direct-dispatch readonly-inline hits could
succeed even when the callable/native-function cache slots were cleared. That behavior was correct and is now locked by
tests, but the profile win by itself was too small to accept.

The real remaining cost was the wrapper layer itself. On the accepted hot shape, `object.c` already owns all the facts
needed to run the readonly-inline meta-method fast callback directly:

- cached direct-dispatch metadata
- exact arity / flag shape
- stack-resident receiver and arguments
- debug-hook off

The profitable cut was therefore to keep the no-reresolve contract, but also bypass
`TryCallIndexContractDirectBindingReadonlyInline*` entirely on the readonly-inline fast-callback lane.

### Regression Coverage

This slice extends the same known-native/index-contract regression suite in:

- `tests/core/test_object_call_known_native_fast_path.c`
  - `test_get_by_index_known_native_readonly_inline_fast_path_prefers_meta_method_fast_callback`
  - `test_set_by_index_known_native_readonly_inline_fast_path_prefers_meta_method_fast_callback`
  - `test_get_by_index_known_native_readonly_inline_fast_path_reuses_cached_direct_dispatch_without_callable_reresolve`
  - `test_set_by_index_known_native_readonly_inline_fast_path_reuses_cached_direct_dispatch_without_callable_reresolve`

The new tests lock the important steady-state contract directly: once readonly-inline direct-dispatch metadata is
cached, hot hits must stay correct even if the callable/native-function cache slots are cleared, and the runtime must
not need to repopulate them just to reach the fast callback.

### Runtime Change

`zr_vm_core/src/zr_vm_core/object/object.c` now imports `zr_vm_library/native_binding.h` and narrows the cached
readonly-inline map lane further:

- add a local stack-residency probe for `SZrTypeValue *`
- add dedicated readonly-inline fast-callback helpers for cached `getByIndex` / `setByIndex`
- invoke the meta-method fast callbacks directly from `object.c` on both:
  - pre-resolve cached direct-dispatch hits
  - post-resolve known-native direct-dispatch hits

The runtime still falls back to the older generic direct-binding / known-native path when the cached shape is absent or
the operands are not stack-resident. This slice only deletes repeated wrapper work from the accepted readonly-inline
steady-state lane.

### Evidence

Against the accepted slice-56 focused profile baseline:

- `map_object_access`: `28,163,286 -> 27,533,243 Ir` (`-2.24%`)
- `ZrCore_Object_TryCallIndexContractDirectBindingReadonlyInlineOneArgumentStack`: dropped out of the visible top set
- `ZrCore_Object_TryCallIndexContractDirectBindingReadonlyInlineTwoArgumentsNoResultStack`: dropped out of the visible
  top set

Against the older slice-54/55 direct-binding map-only archive:

- `map_object_access`: `28,867,249 -> 27,533,243 Ir` (`-4.62%`)

Fresh slice-57 hotspot picture on `map_object_access`:

- `zr_container_map_get_item_readonly_inline_fast = 996,093 Ir`
- `ZrCore_Object_GetByIndexUnchecked = 795,055 Ir`
- `zr_container_map_set_item_readonly_inline_no_result_fast = 544,218 Ir`
- `ZrCore_Object_SetByIndexUnchecked = 405,943 Ir`

Interpretation:

- this slice is acceptance-worthy because the readonly-inline wrapper shell is no longer a top hotspot
- `GetByIndexUnchecked/SetByIndexUnchecked` themselves become fatter because they now own the direct fast-callback gate,
  but the removed wrapper cost is larger than the added local branching
- the profile result is materially better than both the slice-56 state and the older direct-binding archive

Fresh map-only core wall on the same code:

- versus slice 56:
  - mean wall: `124.978 ms -> 112.465 ms` (`-10.01%`)
  - median wall: `114.908 ms -> 112.812 ms` (`-1.82%`)
- versus the older direct-binding archive:
  - mean wall: `119.706 ms -> 112.465 ms` (`-6.05%`)
  - median wall: `105.461 ms -> 112.812 ms` (`+6.97%`, still noisy)

Interpretation:

- mean wall recovered strongly after the intermediate tiny-gain reorder cut
- median wall improves again relative to slice 56, but the wall signal is still noisier than callgrind
- acceptance therefore rests primarily on the deterministic profile drop plus the visible removal of the generic
  readonly-inline wrapper symbols

### Validation

Accepted slice-57 state:

- WSL gcc (`build-wsl-gcc`):
  - `zr_vm_object_call_known_native_fast_path_test`: `51 Tests 0 Failures`
  - `zr_vm_container_runtime_test`: `41 Tests 0 Failures`
- WSL clang (`build-wsl-clang`):
  - `zr_vm_object_call_known_native_fast_path_test`: `51 Tests 0 Failures`
  - `zr_vm_container_runtime_test`: `41 Tests 0 Failures`
- Windows MSVC CLI smoke (`build-msvc-cli-smoke`):
  - rebuilt target `zr_vm_cli_executable`
  - ran `tests/fixtures/projects/hello_world/hello_world.zrp`
  - output remained `hello world`
- Fresh focused benchmarks (`build/benchmark-gcc-release`):
  - profile tier rerun passed on `map_object_access`
  - core tier rerun passed on `map_object_access`
  - archived evidence directories:
    - `build/benchmark-gcc-release/tests_generated/performance_profile_index_direct_fast_callback_after_20260418_map_only`
    - `build/benchmark-gcc-release/tests_generated/performance_core_index_direct_fast_callback_after_20260418_map_only`

## Slice 58: `ADD_STRING` Direct Exact-String Dispatch

### Context

After slice 57, `map_object_access` had already moved off the generic `FUNCTION_CALL` on the `labelFor()` loop line,
but the profile still carried a visible concat shell:

- `map_object_access = 27,533,243 Ir`
- `concat_values_to_destination = 213,044 Ir`
- `ZrCore_String_ConcatPair = 397,784 Ir`

The compiler side had already done its job. Fresh profile data still showed:

- `ADD_STRING = 4,097`

so the remaining work was in the runtime dispatch body, not in quickening.

### Root Cause

`tests/core/gdb_map_object_access_concat_types.gdb` proved the remaining hits inside
`concat_values_to_destination(...)` were arriving as:

- exact string `opA`
- exact string `opB`
- `safeMode = 0`

That meant the `ADD_STRING` instruction in `execution_dispatch.c` was still routing exact-string pairs through the
generic concat shell even though the benchmark line had already quickened to the specialized opcode.

### Regression Coverage

`tests/parser/test_compiler_regressions.c` extends:

- `test_map_object_access_benchmark_project_compile_quickens_labelFor_loop_call`

The regression now also asserts that the compiled `labelFor()` benchmark function actually contains `ADD_STRING`,
locking the intended compile-time lowering so future regressions cannot silently slide that line back onto generic
`ADD` / `FUNCTION_CALL`.

### Runtime Change

The runtime now deletes the redundant generic shell around exact-string `ADD_STRING` dispatch:

- `zr_vm_core/src/zr_vm_core/execution/execution_internal.h`
  - adds shared inline helper `execution_try_concat_exact_strings(...)`
- `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c`
  - changes `ZR_INSTRUCTION_LABEL(ADD_STRING)` to call the exact-string helper directly
- `zr_vm_core/src/zr_vm_core/execution/execution_numeric.c`
  - removes the duplicate local exact-string helper body

The mixed-type / safe concat path remains on `concat_values_to_destination(...)`; this slice only narrows the already
proven exact-string opcode lane.

### Evidence

Against the accepted slice-57 `map_object_access` profile archive:

- `map_object_access`: `27,533,243 -> 27,393,536 Ir` (`-0.51%`)
- `concat_values_to_destination`: `213,044 Ir ->` dropped out of the visible annotate top set
- `ZrCore_String_ConcatPair`: stayed at `397,784 Ir`

Interpretation:

- the real concat work is still present
- the extra runtime dispatch shell around it is gone

Fresh map-only core wall versus the accepted slice-57 map-only rerun:

- mean wall: `112.465 ms -> 96.860 ms` (`-13.87%`)
- median wall: `112.812 ms -> 96.605 ms` (`-14.37%`)

Fresh `string_build` helper-only comparison against the 2026-04-17 profile snapshot on the same instruction mix
(`ADD_STRING = 1,801` in both runs):

- `value_copy`: `18,315 -> 16,189` (`-11.61%`)
- `stack_get_value`: `8,592 -> 4,745` (`-44.78%`)

Interpretation:

- this slice is acceptance-worthy because exact-string `ADD_STRING` no longer re-enters the generic concat shell on the
  hot `map_object_access` lane
- `map_object_access` shows both deterministic annotate cleanup and a strong single-case wall recovery
- `string_build` shows the same-lane helper reductions, but its wall remained in the same broad band rather than
  producing a clean standalone win, so the acceptance claim for this slice stays anchored on the deterministic
  `map_object_access` evidence

### Validation

Accepted slice-58 state:

- WSL gcc (`build-wsl-gcc`):
  - `zr_vm_instructions_test`: `90 Tests 0 Failures`
  - `zr_vm_compiler_integration_test`: `89 Tests 0 Failures`
- WSL clang (`build-wsl-clang`):
  - `zr_vm_instructions_test`: `90 Tests 0 Failures`
  - `zr_vm_compiler_integration_test`: `89 Tests 0 Failures`
- Windows MSVC CLI smoke (`build-msvc-cli-smoke`):
  - rebuilt target `zr_vm_cli_executable`
  - ran `tests/fixtures/projects/hello_world/hello_world.zrp`
  - output remained `hello world`
- Fresh focused benchmarks (`build/benchmark-gcc-release`):
  - profile tier rerun passed on `map_object_access,string_build`
  - core tier rerun passed on `map_object_access,string_build`
  - single-case core reruns passed on `map_object_access` and `string_build`
  - archived evidence directories:
    - `build/benchmark-gcc-release/tests_generated/performance_profile_add_string_dispatch_refresh_20260418_map_string_v2`
    - `build/benchmark-gcc-release/tests_generated/performance_core_add_string_dispatch_refresh_20260418_map_string_v2`
    - `build/benchmark-gcc-release/tests_generated/performance_core_add_string_dispatch_refresh_20260418_map_only_v2`
    - `build/benchmark-gcc-release/tests_generated/performance_core_add_string_dispatch_refresh_20260418_string_only_v2`

## Slice 59: Cached Readonly-Inline Callback Metadata

### Context

After slice 58, `map_object_access` had already dropped the exact-string concat shell, but the hottest remaining steady
state on the `Map` line was still the object-side index wrapper rather than the container callback body:

- `map_object_access = 27,393,536 Ir`
- `zr_container_map_get_item_readonly_inline_fast = 996,093 Ir`
- `ZrCore_Object_GetByIndexUnchecked = 795,055 Ir`
- `zr_container_map_set_item_readonly_inline_no_result_fast = 544,218 Ir`
- `ZrCore_Object_SetByIndexUnchecked = 405,943 Ir`

The container bodies were already on the intended narrow lane. The remaining shell lived around them.

### Root Cause

`object_try_call_cached_known_native_get_by_index_readonly_inline(...)` and
`object_try_call_cached_known_native_set_by_index_readonly_inline(...)` still re-opened the meta-method descriptor on
every hit just to rediscover the readonly-inline fast callback pointer:

- load cached direct dispatch
- re-check shape flags / arity
- read `metaMethodDescriptor`
- read `readonlyInline*FastCallback`

That meant the cached direct-dispatch metadata was still incomplete for the hottest readonly-inline steady-state hit.
The profitable cut was to cache the readonly-inline fast callback pointers directly inside
`SZrObjectKnownNativeDirectDispatch` when the binding cache is refreshed, then let the object-side hit path call those
cached pointers directly.

### Regression Coverage

`tests/core/test_object_call_known_native_fast_path.c` now strengthens three existing contracts:

- `test_native_binding_cached_binding_primes_direct_dispatch_cache_and_clears_on_rebind`
- `test_get_by_index_known_native_readonly_inline_fast_path_reuses_cached_direct_dispatch_without_callable_reresolve`
- `test_set_by_index_known_native_readonly_inline_fast_path_reuses_cached_direct_dispatch_without_callable_reresolve`

The tests now assert two things explicitly:

- readonly-inline direct-dispatch cache stores the fast callback pointers when the binding is cached
- after the cache is primed, clearing `metaMethodDescriptor` and known-native callable/function cache entries still
  keeps the readonly-inline hit path on the fast callback instead of falling back through the generic wrapper lane

That locks the intended steady-state contract directly.

### Runtime Change

The readonly-inline callback metadata is now cached as part of the shared direct-dispatch record:

- `zr_vm_core/include/zr_vm_core/object_known_native_dispatch.h`
  - adds typed cached function-pointer fields:
    - `readonlyInlineGetFastCallback`
    - `readonlyInlineSetNoResultFastCallback`
- `zr_vm_library/src/zr_vm_library/native_binding/native_binding_internal.h`
  - populates those cached fields when native binding direct-dispatch cache is refreshed
- `zr_vm_core/src/zr_vm_core/object/object_call.c`
  - mirrors the same cached-field population on the core-side direct-dispatch refresh helper
- `zr_vm_core/src/zr_vm_core/object/object.c`
  - changes cached readonly-inline get/set hit paths to call the cached fast callback fields directly
- `zr_vm_core/src/zr_vm_core/object/object_index_contract_direct_binding.c`
  - keeps the narrower direct-binding readonly-inline helpers aligned with the same cached metadata contract

This slice does not add benchmark-only branching. It only finishes the cached metadata needed by the already-proven
readonly-inline object/index fast path.

### Evidence

Against the accepted slice-58 focused profile archive:

- `map_object_access`: `27,393,536 -> 27,353,141 Ir` (`-0.15%`)
- `zr_container_map_get_item_readonly_inline_fast`: stayed at `996,093 Ir`
- `ZrCore_Object_GetByIndexUnchecked`: `795,055 -> 770,468 Ir` (`-3.09%`)
- `zr_container_map_set_item_readonly_inline_no_result_fast`: stayed at `544,218 Ir`
- `ZrCore_Object_SetByIndexUnchecked`: `405,943 -> 393,644 Ir` (`-3.03%`)

Interpretation:

- the `Map` container callback bodies themselves stayed flat
- the object-side get/set shell around those callbacks shrank measurably
- this is the intended cut: finish the cached readonly-inline metadata so the wrapper work drops without touching the
  already-hot container core

Fresh map-only core reruns were noisy on this host after rebuilds:

- accepted slice-58 baseline: mean `96.860 ms`, median `96.605 ms`
- first slice-59 rerun: mean `98.216 ms`, median `97.695 ms`
- repeat slice-59 rerun: mean `112.771 ms`, median `112.320 ms`

Interpretation:

- the deterministic callgrind evidence is positive and aligned with the intended wrapper cut
- the wall-clock signal is currently too noisy to claim a timing win or a stable regression from this slice alone
- acceptance for this slice therefore rests on the deterministic profile drop plus the strengthened cache-contract tests,
  not on the unstable wall samples

### Validation

Accepted slice-59 state:

- WSL gcc (`build-wsl-gcc`):
  - `zr_vm_object_call_known_native_fast_path_test`: `51 Tests 0 Failures`
  - `zr_vm_container_runtime_test`: `41 Tests 0 Failures`
- WSL clang (`build-wsl-clang`):
  - `zr_vm_object_call_known_native_fast_path_test`: `51 Tests 0 Failures`
  - `zr_vm_container_runtime_test`: `41 Tests 0 Failures`
- Windows MSVC CLI smoke (`build-msvc-cli-smoke`):
  - rebuilt target `zr_vm_cli_executable`
  - ran `tests/fixtures/projects/hello_world/hello_world.zrp`
  - output remained `hello world`
- Fresh focused benchmarks (`build/benchmark-gcc-release`):
  - profile tier rerun passed on `map_object_access`
  - core tier rerun passed on `map_object_access`
  - archived evidence directories:
    - `build/benchmark-gcc-release/tests_generated/performance_profile_index_cached_readonly_inline_callback_cache_after_20260418_map_only`
    - `build/benchmark-gcc-release/tests_generated/performance_core_index_cached_readonly_inline_callback_cache_after_20260418_map_only`
    - `build/benchmark-gcc-release/tests_generated/performance_core_index_cached_readonly_inline_callback_cache_after_20260418_map_only_repeat`

## Slice 56: Readonly-Inline Meta-Method Fast Callback

### Context

After slice 55, the `map_object_access` object/container line had already been narrowed into cached index-contract
direct dispatch, but the runtime still entered the generic direct-binding callback context even for the exact `Map`
readonly-inline shape.

Fresh hotspot evidence before this slice showed:

- `map_object_access`: `28,867,249 Ir`
- `zr_container_map_get_item = 1,127,229 Ir`
- `zr_container_map_set_item = 669,379 Ir`
- `ZrCore_Object_TryCallIndexContractDirectBindingReadonlyInlineOneArgumentStack = 704,856 Ir`
- `ZrCore_Object_TryCallIndexContractDirectBindingReadonlyInlineTwoArgumentsNoResultStack = 414,100 Ir`

### Root Cause

The `Map` meta-method descriptors already proved a narrower lane than the generic readonly-inline bound-callback shell:

- fixed receiver usage
- readonly inline value context
- no self rebind
- no-result set variant

But the runtime still built the generic bound callback context and reached the callback through the older
meta-method/bound dispatcher path. For `Map` steady-state access, that was redundant. The profitable cut was to let the
descriptor expose direct readonly-inline fast callbacks and have the index-contract direct-binding lane call them first.

### Regression Coverage

This slice extends the same known-native fast-path suite in:

- `tests/core/test_object_call_known_native_fast_path.c`
  - `test_get_by_index_known_native_readonly_inline_fast_path_prefers_meta_method_fast_callback`
  - `test_set_by_index_known_native_readonly_inline_fast_path_prefers_meta_method_fast_callback`

The tests lock the intended contract directly: once the readonly-inline direct-dispatch shape is proven, the `Map`
meta-method fast callbacks must win over the generic fallback callback lane.

### Runtime Change

The readonly-inline fast-callback contract is now wired across the native binding and object-call stack:

- `zr_vm_library/include/zr_vm_library/native_binding.h`
  - add `readonlyInlineGetFastCallback`
  - add `readonlyInlineSetNoResultFastCallback`
- `zr_vm_core/src/zr_vm_core/object/object_call.c`
  - mirror the new fast-callback fields into the runtime-side meta-method descriptor layout
- `zr_vm_core/src/zr_vm_core/object/object_index_contract_direct_binding.c`
  - call the meta-method fast callbacks before falling back to the generic bound callback context
- `zr_vm_lib_container/src/zr_vm_lib_container/module.c`
  - split the old `Map` get/set callbacks into shared cores plus:
    - `zr_container_map_get_item_readonly_inline_fast(...)`
    - `zr_container_map_set_item_readonly_inline_no_result_fast(...)`
  - wire both into `kMapMetaMethods[]`

This slice does not add benchmark-only branches. It only lets the already-proven `Map` meta-method shape run through a
narrower shared runtime lane.

### Evidence

Against the older direct-binding map-only profile archive:

- `map_object_access`: `28,867,249 -> 28,163,286 Ir` (`-2.44%`)
- `zr_container_map_get_item -> zr_container_map_get_item_readonly_inline_fast`: `1,127,229 -> 996,093 Ir`
- `zr_container_map_set_item -> zr_container_map_set_item_readonly_inline_no_result_fast`: `669,379 -> 544,218 Ir`
- `ZrCore_Object_TryCallIndexContractDirectBindingReadonlyInlineOneArgumentStack`: `704,856 -> 573,720 Ir`
- `ZrCore_Object_TryCallIndexContractDirectBindingReadonlyInlineTwoArgumentsNoResultStack`: `414,100 -> 307,500 Ir`
- `ZrLib_TempValueRoot_Begin`: `542,836 -> 486,552 Ir`
- `ZrLib_TempValueRoot_End`: `238,108 -> 213,400 Ir`

Fresh map-only core wall on the same slice was noisy:

- mean wall: `119.706 ms -> 124.978 ms`
- median wall: `105.461 ms -> 114.908 ms`

Interpretation:

- this slice is acceptance-worthy on deterministic callgrind evidence, not on wall mean
- the structural win is real: the `Map` get/set bodies and readonly-inline wrappers both shrink materially
- the next profitable follow-up should stay on the same line and delete the remaining wrapper shell around those fast
  callbacks instead of moving back up into generic object-call machinery

### Validation

Accepted slice-56 state:

- WSL gcc (`build-wsl-gcc`):
  - `zr_vm_object_call_known_native_fast_path_test`: `49 Tests 0 Failures`
  - `zr_vm_container_runtime_test`: `41 Tests 0 Failures`
- WSL clang (`build-wsl-clang`):
  - `zr_vm_object_call_known_native_fast_path_test`: `49 Tests 0 Failures`
  - `zr_vm_container_runtime_test`: `41 Tests 0 Failures`
- Windows MSVC CLI smoke (`build-msvc-cli-smoke`):
  - rebuilt target `zr_vm_cli_executable`
  - ran `tests/fixtures/projects/hello_world/hello_world.zrp`
  - output remained `hello world`
- Fresh focused benchmarks (`build/benchmark-gcc-release`):
  - profile tier rerun passed on `map_object_access`
  - core tier rerun passed on `map_object_access`
  - archived evidence directories:
    - `build/benchmark-gcc-release/tests_generated/performance_profile_index_meta_fast_callback_after_20260418_map_only`
    - `build/benchmark-gcc-release/tests_generated/performance_core_index_meta_fast_callback_after_20260418_map_only`

## Slice 54: Index-Contract Direct-Binding Narrow Lanes

### Context

After slice 53, the hottest remaining `map_object_access` object/native line was still the cached index-contract
dispatch shell, not the container body itself.

Fresh pre-slice hotspot evidence on `map_object_access` showed:

- callgrind total Ir: `29,454,754`
- `ZrCore_Object_CallDirectBindingFastOneArgument = 811,404 Ir`
- `ZrCore_Object_CallDirectBindingFastTwoArguments = 565,800 Ir`
- combined generic direct-binding wrapper cost: `1,377,204 Ir`
- `zr_container_map_set_item = 800,599 Ir`

That matched the active hypothesis from the previous note: the cache already proved a narrow
`NO_SELF_REBIND + INLINE_VALUE_CONTEXT + READONLY_INLINE...` shape, but `object.c` still paid the generic
`CallDirectBindingFast*` wrapper tax on every hot `Map` get/set.

### Root Cause

`ZrCore_Object_GetByIndexUnchecked(...)` and `ZrCore_Object_SetByIndexUnchecked(...)` resolved and cached the direct
dispatch metadata correctly, but then still entered the shared generic direct-binding wrappers:

- `ZrCore_Object_CallDirectBindingFastOneArgument(...)`
- `ZrCore_Object_CallDirectBindingFastTwoArgumentsNoResult(...)`

Those wrappers must keep all fallback logic alive:

- flag decoding
- debug-hook gating
- generic readonly-inline reuse checks
- generic no-result / result shell selection

For `Map` steady-state access, that work is redundant. The cached index contract already proves the exact shape that is
executing, and the interpreter hot path is overwhelmingly stack-resident. The profitable cut was therefore to add a
much narrower callsite lane instead of asking the generic wrapper to rediscover the same facts every time.

### Regression Coverage

This slice kept the existing set-side readonly-inline contracts and added a matching get-side contract:

- `tests/core/test_object_call_known_native_fast_path.c`
  - `test_get_by_index_known_native_readonly_inline_fast_path_reuses_input_pointers`
  - `test_set_by_index_known_native_readonly_inline_fast_path_can_ignore_result`
  - `test_set_by_index_known_native_readonly_inline_fast_path_can_ignore_result_with_non_contiguous_stack_inputs`
  - `test_get_by_index_known_native_stack_root_fast_path_caches_direct_dispatch`
  - `test_set_by_index_known_native_stack_root_fast_path_caches_direct_dispatch`

The new get-side test locks the new intended contract directly: when the cached index dispatch is readonly-inline and
the operands already live on the VM stack, the runtime must reuse those exact input pointers instead of paying through
the generic direct-binding shell first.

### Runtime Change

This slice adds a dedicated module,
`zr_vm_core/src/zr_vm_core/object/object_index_contract_direct_binding.c`, instead of stacking more logic into the
already oversized `object.c` / `object_call.c` pair.

That module now provides two narrow lanes:

- `ZrCore_Object_TryCallIndexContractDirectBindingReadonlyInlineOneArgumentStack(...)`
- `ZrCore_Object_TryCallIndexContractDirectBindingReadonlyInlineTwoArgumentsNoResultStack(...)`

`zr_vm_core/src/zr_vm_core/object/object.c` now probes those lanes first after cached known-native index-contract
resolution and before falling back to the generic direct-binding wrappers.

The new lanes only activate when the already-cached dispatch proves the exact hot steady-state shape:

- `usesReceiver`
- fixed raw arity (`2` for get, `3` for set)
- readonly inline value-context flags already present
- stack-resident inputs
- debug hook inactive

If any of those facts stop being true, the old generic wrapper chain still handles the call exactly as before.

### Evidence

Against the fresh pre-slice profile hotspot:

- `map_object_access`: `29,454,754 -> 28,867,249 Ir` (`-1.99%`)
- combined generic direct-binding wrapper pair:
  - `811,404 + 565,800 -> 704,856 + 414,100 Ir`
  - `1,377,204 -> 1,118,956 Ir` (`-18.75%`)
- `zr_container_map_set_item`: `800,599 -> 669,379 Ir` (`-16.39%`)
- `value_copy` helper count: `74,716 -> 70,616` (`-5.49%`)
- `value_reset_null` helper count: `8,704 -> 4,604` (`-47.10%`)

Fresh slice-54 hotspot picture on `map_object_access`:

- `zr_container_map_get_item = 1,127,229 Ir`
- `ZrCore_Object_TryCallIndexContractDirectBindingReadonlyInlineOneArgumentStack = 704,856 Ir`
- `zr_container_map_set_item = 669,379 Ir`
- `ZrCore_Object_GetByIndexUnchecked = 573,748 Ir`
- `ZrCore_Object_TryCallIndexContractDirectBindingReadonlyInlineTwoArgumentsNoResultStack = 414,100 Ir`

Compared with the pre-slice annotate snapshot, the old generic wrapper symbols dropped out of the visible top set
entirely and were replaced by the narrower index-contract lanes.

Fresh map-only core wall rerun (`1` warmup, `5` measured iterations, `zr_interp` only):

- previous accepted local reference: `108.177 ms`
- slice-54 rerun: `106.297 ms`
- mean delta: `-1.74%`
- median delta: `107.557 ms -> 103.790 ms` (`-3.50%`)

An immediate post-change four-case focused spot-check (`dispatch_loops,map_object_access,call_chain_polymorphic,
mixed_service_loop`, `1` warmup, `3` measured iterations) also landed at:

- `dispatch_loops = 256.119 ms`
- `map_object_access = 94.807 ms`
- `call_chain_polymorphic = 88.048 ms`
- `mixed_service_loop = 123.959 ms`

Later chained multi-case reruns on the same workstation were materially noisier, so slice acceptance for this cut is
anchored on the stable map-only/core and map-only/profile evidence above rather than those aggregate wall samples.

Interpretation:

- this slice is acceptance-worthy because it deletes repeated generic wrapper work from the exact hot cached
  index-contract line the benchmark is actually spending time on
- the map-only wall gain is modest but repeatable, while the instruction-count reduction is clean and directly aligned
  with the intended callsite cut
- the next profitable work should now move one level deeper into the new remaining tops:
  - `zr_container_map_get_item`
  - `zr_container_map_set_item`
  - `ZrLib_TempValueRoot_Begin`

### Validation

Accepted slice-54 state:

- WSL gcc (`build-wsl-gcc`):
  - `zr_vm_object_call_known_native_fast_path_test`: `47 Tests 0 Failures`
  - `zr_vm_container_runtime_test`: `40 Tests 0 Failures`
- WSL clang (`build-wsl-clang`):
  - `zr_vm_object_call_known_native_fast_path_test`: `47 Tests 0 Failures`
  - `zr_vm_container_runtime_test`: `40 Tests 0 Failures`
- Windows MSVC CLI smoke (`build-msvc-cli-smoke`):
  - rebuilt target `zr_vm_cli_executable`
  - ran `tests/fixtures/projects/hello_world/hello_world.zrp`
  - output remained `hello world`
- Fresh focused benchmarks (`build/benchmark-gcc-release`):
  - core tier map-only rerun passed (`1` warmup, `5` measured)
  - profile tier map-only rerun passed with callgrind counting enabled
  - four-case focused spot-check rerun passed, but later chained repeats were archived only as noise records
  - archived evidence directories:
    - `build/benchmark-gcc-release/tests_generated/performance_core_index_direct_binding_after_20260418_map_only`
    - `build/benchmark-gcc-release/tests_generated/performance_profile_index_direct_binding_after_20260418_map_only`
    - `build/benchmark-gcc-release/tests_generated/performance_core_index_direct_binding_after_20260418_four_case_repeat`

## Slice 54: Four-Slot Exact-Key Map Lookup Cache

### Context

After rejecting the content-equal dynamic-string cache branch, the focused `map_object_access` profile had settled at:

- `map_object_access = 28,663,995 Ir`
- `zr_container_map_find_index.part.0 = 1,794,018 Ir`
- `zr_container_map_get_item_readonly_inline_fast = 909,633 Ir`
- `ZrCore_Object_GetByIndexUncheckedStackOperands = 647,530 Ir`

That left the exact-key map lookup line as the largest remaining container-local hotspot on this benchmark. The benchmark
itself cycles only four labels through `labelFor(slot) + "_slot"`, so the single-slot exact-key cache was still
thrashing even after the earlier readonly-inline and direct-dispatch cuts.

### Root Cause

The regressed content-equal cache attempt already proved that broadening the cache by string equality was the wrong
direction for steady-state throughput.

The profitable remaining cut was narrower:

- keep the cache exact-key only
- keep it bound to the current `entries` array
- expand it from one slot to a very small hot set that can survive the benchmark's four-key rotation
- validate each cache hit against the current array cell so stale entry reuse never leaks past structural mutation

### Regression Coverage

`tests/container/test_container_runtime.c` now adds and keeps:

- `test_container_map_runtime_repeated_index_access_primes_entries_and_pair_field_caches`
- `test_container_map_runtime_four_key_concat_cycle_preserves_values`

The first test locks the existing map-entry/pair-field cache priming contract. The second reproduces the hot benchmark
shape directly: repeated `labelFor()` string concat over a four-key cycle must keep returning the correct accumulated
values while the map cache rotates through multiple exact key objects.

### Runtime Change

`zr_vm_lib_container/src/zr_vm_lib_container/module.c` now changes the hot map lookup cache from a single exact-key slot
to a four-slot exact-key cache:

- `ZrContainerHotMapLookupCache` stores `slots[4]` plus the active `entries` array
- `zr_container_update_hot_map_lookup_cache(...)` promotes exact-key hits to the front and inserts new hits in a small
  LRU-style order
- `zr_container_try_hot_map_lookup_cache(...)` only accepts a hit when:
  - the cached `entries` pointer still matches
  - the cached index is still in range
  - the current array cell still points at the same cached entry object
- string-key lookup still falls back to the existing exact-key scan and then the content-equality comparison path on
  cache miss

This slice deliberately does **not** reintroduce any content-equal sticky cache reuse.

### Evidence

Against the immediate post-revert focused profile snapshot:

- `dispatch_loops`: `437,553,933 -> 437,531,705 Ir` (`-0.01%`, flat)
- `map_object_access`: `28,663,995 -> 28,321,889 Ir` (`-1.19%`)
- `zr_container_map_find_index.part.0`: `1,794,018 -> 1,342,562 Ir` (`-25.17%`)
- `zr_container_map_get_item_readonly_inline_fast`: `909,633 -> 909,633 Ir` (flat)
- `ZrCore_Object_GetByIndexUncheckedStackOperands`: `647,530 -> 647,530 Ir` (flat)

Fresh slice-54 hotspot picture on `map_object_access`:

- `zr_container_map_find_index.part.0 = 1,342,562 Ir`
- `zr_container_map_get_item_readonly_inline_fast = 909,633 Ir`
- `ZrCore_Object_GetByIndexUncheckedStackOperands = 647,530 Ir`

Fresh focused core wall rerun on this slice:

- `dispatch_loops = 207.399 ms`
- `map_object_access = 89.406 ms`

Interpretation:

- this slice is acceptance-worthy because it materially shrinks the hottest remaining container-local lookup function
  without moving cost into `dispatch_loops`
- the whole-case `map_object_access` callgrind total also moves down, so this is not just a local reshuffle
- the remaining hotter shared-low-level line on `map_object_access` is now
  `ZrCore_Object_GetByIndexUncheckedStackOperands`, not the exact-key cache itself
- the next profitable cut should pivot to the shared object index contract lane:
  - `ZrCore_Object_GetByIndexUncheckedStackOperands`
  - `object_get_by_index_unchecked_core`
  - the surrounding known-native/index direct-dispatch path

### Validation

Accepted slice-54 state:

- WSL gcc (`build-wsl-gcc`):
  - `zr_vm_container_runtime_test`: `37 Tests 0 Failures`
  - `zr_vm_execution_add_stack_relocation_test`: `14 Tests 0 Failures`
  - `zr_vm_object_call_known_native_fast_path_test`: `51 Tests 0 Failures`
- WSL clang (`build-wsl-clang`):
  - `zr_vm_container_runtime_test`: `37 Tests 0 Failures`
  - `zr_vm_execution_add_stack_relocation_test`: `14 Tests 0 Failures`
  - `zr_vm_object_call_known_native_fast_path_test`: `51 Tests 0 Failures`
- Windows MSVC CLI smoke (`build-msvc-cli-smoke`):
  - imported Visual Studio x64 command-line environment
  - rebuilt target `zr_vm_cli_executable`
  - ran `tests/fixtures/projects/hello_world/hello_world.zrp`
  - output remained `hello world`
- Fresh focused benchmarks (`build/benchmark-gcc-release`):
  - profile tier rerun passed on `dispatch_loops,map_object_access`
  - core tier rerun passed on `dispatch_loops,map_object_access`
  - archived evidence directories:
    - `build/benchmark-gcc-release/tests_generated/performance_profile_t66_multislot_exact_key_after_20260418`
    - `build/benchmark-gcc-release/tests_generated/performance_core_t66_multislot_exact_key_after_20260418`

## Slice 53: Exact-String Concat Stack-Restore Removal

### Context

After slice 52, `map_object_access` still kept a visible concat cost on:

- `var key = label + "_slot";`

The immediate pre-simplify focused profile sample on the same narrowed benchmark subset had:

- `dispatch_loops`: `459,812,252 Ir`
- `map_object_access`: `43,131,769 Ir`

and the `map_object_access` annotate snapshot still showed the exact-string concat shell itself:

- `execution_try_concat_exact_strings.part.0 = 606,356 Ir`
- `ZrCore_String_ConcatPair = 323,663 Ir`
- `ZrCore_Function_StackAnchorInit = 210,315 Ir`

### Root Cause

`execution_try_concat_exact_strings(...)` was still treating ordinary stack-rooted exact string pairs like a generic
relocation-sensitive path:

- initialize stack anchors for both operands
- restore both stack slots
- copy both values into locals
- refresh forwarded object pointers

That work was dead on the steady-state exact-string lane.

This helper does **not** reserve scratch stack space, and `ZrCore_String_ConcatPair(...)` copies source bytes before it
enters the allocating string-creation step. So the benchmark's `label + "_slot"` path was still paying two stack-slot
restores plus the surrounding insurance even though exact concat never needed them.

### Regression Coverage

This slice extended the existing exact-concat relocation suite in:

- `tests/core/test_execution_add_stack_relocation.c`

The active contracts now include:

- `test_try_builtin_add_exact_string_pair_avoids_scratch_stack_growth`
- `test_concat_values_to_destination_exact_string_pair_avoids_scratch_stack_growth`
- `test_execution_add_exact_string_pair_writes_directly_without_value_copy_helper`
- `test_try_builtin_add_exact_string_pair_stack_inputs_avoid_stack_get_value_helper`
- `test_try_builtin_add_exact_string_pair_pinned_stack_inputs_avoid_stack_get_value_helper`

The new tests lock the intended steady-state contract directly: exact string pairs must stay off the scratch stack and
must not bounce back through `stack_get_value` restore work just because the operands currently live in normal stack
slots.

### Runtime Change

`zr_vm_core/src/zr_vm_core/execution/execution_numeric.c` now simplifies
`execution_try_concat_exact_strings(...)` to the actual fast-path behavior:

- validate exact string operands
- call `ZrCore_String_ConcatPair(...)` directly on the incoming operands
- materialize the result without stack-anchor / restore / stable-copy insurance

The generic mixed-type concat path in `concat_values_to_destination(...)` stays unchanged. This slice only deletes dead
work from the exact-string lane.

### Evidence

Against the accepted slice-52 focused baseline:

- `dispatch_loops`: `459,798,847 -> 459,789,514 Ir` (`-0.00%`, flat)
- `map_object_access`: `43,979,996 -> 42,253,979 Ir` (`-3.92%`)

Against the immediate pre-simplify focused profile sample in this same slice:

- `map_object_access`: `43,131,769 -> 42,253,979 Ir` (`-2.04%`)
- `stack_get_value` helper count on `map_object_access`: `24,618 -> 16,424` (`-33.29%`)

Fresh slice-53 hotspot picture on `map_object_access`:

- `ZrCore_Function_CallPreparedSingleResultFastRestoreCallback = 1,647,664 Ir`
- `zr_container_map_find_index.part.0 = 1,745,742 Ir`
- `ZrCore_Object_GetByIndexUnchecked = 614,711 Ir` (top helper)
- `ZrCore_String_ConcatPair = 323,663 Ir`

Compared to the pre-simplify annotate snapshot, `execution_try_concat_exact_strings.part.0` dropped out of the visible
top set entirely after the stack-restore shell was removed.

Fresh focused core wall reruns (`3` measured iterations, `dispatch_loops,map_object_access` only):

- run 1:
  - `dispatch_loops = 228.064 ms`
  - `map_object_access = 98.907 ms`
- run 2:
  - `dispatch_loops = 212.793 ms`
  - `map_object_access = 96.896 ms`
- run 3:
  - `dispatch_loops = 215.601 ms`
  - `map_object_access = 98.528 ms`

Compared with the accepted slice-52 wall:

- `map_object_access`: `100.811 ms -> 96.896~98.907 ms` (`-1.89%` to `-3.88%`)
- `dispatch_loops`: profile stayed flat, while core wall returned near the slice-52 range after the first noisy rerun

Interpretation:

- this slice is acceptance-worthy because it deletes real exact-string steady-state work from a benchmarked hot line
- the gain lands directly on `map_object_access`, with repeatable wall improvement and a clear helper-count reduction
- `dispatch_loops` does not show a profile regression; its wall remains in the same general band with one noisy outlier
- the next profitable `map_object_access` work should go back to the hotter object/native line:
  - `ZrCore_Function_CallPreparedSingleResultFastRestoreCallback`
  - `object_call_known_native_fast_one_argument.part.0`
  - `object_direct_binding_stack_root_callback_fast`

### Validation

Accepted slice-53 state:

- WSL gcc (`build-wsl-gcc`):
  - `zr_vm_execution_add_stack_relocation_test`: `10 Tests 0 Failures`
  - `zr_vm_instructions_test`: `90 Tests 0 Failures`
- WSL clang (`build-wsl-clang`):
  - `zr_vm_execution_add_stack_relocation_test`: `10 Tests 0 Failures`
  - `zr_vm_instructions_test`: `90 Tests 0 Failures`
- Windows MSVC CLI smoke (`build\\codex-msvc-cli-debug`):
  - rebuilt target `zr_vm_cli_executable`
  - ran `tests/fixtures/projects/hello_world/hello_world.zrp`
  - output remained `hello world`
- Fresh focused benchmarks (`build/benchmark-gcc-release`):
  - profile tier rerun passed on `dispatch_loops,map_object_access`
  - core tier rerun passed repeatedly on `dispatch_loops,map_object_access`
  - archived evidence directories:
    - `build/benchmark-gcc-release/tests_generated/performance_profile_exact_concat_stack_restore_after_20260417`
    - `build/benchmark-gcc-release/tests_generated/performance_core_exact_concat_stack_restore_after_20260417`
    - `build/benchmark-gcc-release/tests_generated/performance_core_exact_concat_stack_restore_after_repeat_20260417`

## Slice 52: Known-Native Single-Result Restore Merge

### Context

On the current W2 line, the focused accepted baseline before this slice was:

- `dispatch_loops`: `467,802,558 Ir`
- `map_object_access`: `44,187,075 Ir`

The remaining `map_object_access` object/native stack was still concentrated in:

- `object_call_known_native_fast_one_argument.part.0 = 1,360,536 Ir`
- `native_binding_dispatch_cached_stack_root_one_argument = 1,294,968 Ir`
- `object_complete_known_native_fast_call = 1,205,008 Ir`
- `ZrCore_Function_CallPreparedResolvedNativeFunctionSingleResultFast = 1,008,272 Ir`

### Root Cause

The one/two-argument known-native object-call fast path had already eliminated the generic `PreCall` hop, but the
single-result return still paid through two layers:

- `object_complete_known_native_fast_call(...)`
- `ZrCore_Function_CallPreparedResolvedNativeFunctionSingleResultFast(...)`

That meant the prepared native helper normalized the return back into scratch `functionBase`, then `object_call.c`
restored outer-frame anchors and copied the same value again into the final destination.

The previously tried direct `returnDestination` rewrite remains rejected on this line. The remaining profitable cut was
to delete object-call-local post-call work instead of retargeting the prepared native helper.

### Regression Coverage

This slice reused the existing known-native object-call regression suite rather than adding another dedicated test file.
The active contracts stayed locked by:

- `tests/core/test_object_call_known_native_fast_path.c`
  - `test_object_call_known_native_fast_path_restores_outer_frame_bounds_after_growth`
  - `test_object_call_known_native_fast_path_preserves_receiver_when_result_aliases_receiver_slot`
  - `test_object_call_known_native_fast_path_reuses_single_nested_call_info_node`
  - `test_object_call_known_native_fast_path_accepts_non_stack_gc_inputs`
  - `test_set_by_index_known_native_fast_path_accepts_two_stack_rooted_arguments`

These already cover stack growth, nested call-info reuse, result alias safety, and index-contract call layout on the
same narrowed object/native lane.

### Runtime Change

`zr_vm_core/include/zr_vm_core/function.h`, `zr_vm_core/src/zr_vm_core/function.c`, and
`zr_vm_core/src/zr_vm_core/object/object_call.c` now add and use:

- `ZrCore_Function_CallPreparedResolvedNativeFunctionSingleResultFastRestore(...)`

That helper keeps the existing stack-local native call-info fast path but absorbs the hot object-call tail work:

- saved stack-top restore
- saved call-info top restore
- anchored result restore
- final single-result copy into the caller-visible destination

The debug-hook path still stays on the older generic resolved-native helper, so the accepted steady-state cut only
touches the release/profile fast lane.

### Evidence

Against the fresh slice-51 focused baseline:

- `dispatch_loops`: `467,802,558 -> 459,798,847 Ir` (`-1.71%`)
- `map_object_access`: `44,187,075 -> 43,979,996 Ir` (`-0.47%`)

Fresh slice-52 hotspot picture on `map_object_access`:

- `ZrCore_Function_CallPreparedResolvedNativeFunctionSingleResultFastRestore = 1,524,704 Ir`
- `object_call_known_native_fast_one_argument.part.0 = 1,360,536 Ir`
- `native_binding_dispatch_cached_stack_root_one_argument = 1,270,380 Ir`
- `object_call_known_native_fast_two_arguments.part.0 = 799,500 Ir`
- `object_complete_known_native_fast_call = 737,760 Ir`
- `native_binding_dispatch_stack_root_callback_lane = 565,616 Ir`
- `ZrLib_TempValueRoot_Begin = 542,836 Ir`

Compared to slice 51, the old object-call-local completion shell dropped materially:

- `object_complete_known_native_fast_call`: `1,205,008 -> 737,760 Ir` (`-38.77%`)

Fresh focused core wall on this slice:

- `dispatch_loops = 210.994 ms`
- `map_object_access = 100.811 ms`

Interpretation:

- this slice is acceptance-worthy because it deletes a real object-call-local layer instead of merely moving the same
  cost around
- the gain is modest on `map_object_access`, but deterministic and aligned with the still-hot known-native object-call
  line
- `dispatch_loops` also benefits because the same shared result/copy machinery sits on broader resolved-call traffic
- the next profitable `map_object_access` work should remain on:
  - `object_call_known_native_fast_one_argument.part.0`
  - `native_binding_dispatch_cached_stack_root_one_argument`
  - `ZrLib_TempValueRoot_Begin`

### Validation

Accepted slice-52 state:

- WSL gcc (`build-wsl-gcc`):
  - `zr_vm_object_call_known_native_fast_path_test`: `26 Tests 0 Failures`
  - `zr_vm_container_runtime_test`: `37 Tests 0 Failures`
  - `zr_vm_module_system_test`: `87 Tests 0 Failures`
  - `zr_vm_execution_add_stack_relocation_test`: `8 Tests 0 Failures`
  - `zr_vm_execution_member_access_fast_paths_test`: `46 Tests 0 Failures`
  - `zr_vm_instructions_test`: `90 Tests 0 Failures`
- WSL clang (`build-wsl-clang`):
  - `zr_vm_object_call_known_native_fast_path_test`: `26 Tests 0 Failures`
  - `zr_vm_container_runtime_test`: `37 Tests 0 Failures`
  - `zr_vm_module_system_test`: `87 Tests 0 Failures`
  - `zr_vm_execution_add_stack_relocation_test`: `8 Tests 0 Failures`
  - `zr_vm_execution_member_access_fast_paths_test`: `46 Tests 0 Failures`
  - `zr_vm_instructions_test`: `90 Tests 0 Failures`
- Windows MSVC CLI smoke (`build\\codex-msvc-cli-debug`):
  - rebuilt target `zr_vm_cli_executable`
  - ran `tests/fixtures/projects/hello_world/hello_world.zrp`
  - output remained `hello world`
- Fresh focused benchmarks (`build/benchmark-gcc-release`):
  - profile tier rerun passed on `dispatch_loops,map_object_access`
  - core tier rerun passed on `dispatch_loops,map_object_access`
