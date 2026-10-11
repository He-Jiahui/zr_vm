---
related_code:
  - tests/parser/test_ssa_frame_layout.c
  - tests/parser/test_ssa_roots_observation.c
  - tests/parser/test_ssa_active_packed_frame_observation.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_roots.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_layout.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
  - zr_vm_core/src/zr_vm_core/execution_contract.c
  - tests/third_party/zr_unity/Unity/src/unity.c
  - zr_vm_parser/include/zr_vm_parser/exec_ir_frame_roots.h
  - zr_vm_parser/include/zr_vm_parser/exec_ir_frame_layout.h
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h
  - tests/unity_config.h
  - tests/third_party/zr_unity/Unity/src/unity.h
  - tests/third_party/zr_unity/Unity/src/unity_internals.h
  - tests/cmake/ssa-active-packed-frame-observation.cmake
  - tests/cmake/ssa-active-packed-frame-observation-direct-validation/CMakeLists.txt
  - tests/cmake/ssa-active-packed-frame-observation-check.cmake
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_frame_roots.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_roots.c
plan_sources:
  - docs/plans/ssa/04-frame-native/01-frame-layout.md
  - docs/plans/ssa/04-frame-native/04-roots-observation.md
tests:
  - tests/parser/test_ssa_active_packed_frame_observation.c
  - tests/parser/test_ssa_frame_layout.c
  - tests/parser/test_ssa_roots_observation.c
  - docs/testing-and-validation/ssa-active-packed-frame-observation.md
  - docs/acceptance/ssa-active-packed-frame-observation.md
doc_type: module-detail
status: finite-validated
---

# Active packed-frame observation

## Current state and purpose

The additive primitive has finite Windows Clang runtime acceptance and MSVC compile-only acceptance. It advances the [frame-layout plan](../plans/ssa/04-frame-native/01-frame-layout.md) and [roots-observation plan](../plans/ssa/04-frame-native/04-roots-observation.md) within the scope below. The descriptor now defaults to GREEN and is included in shared registration; explicit RED remains selectable. Root made that default change after the accepted explicit-GREEN run. Normal full-project configuration/runtime was not exercised.

The additive entry, `ZrParser_ExecIr_ObserveActiveFrame`, selects logical ValueIds in an actual packed layout. Disjoint lifetimes can reuse one physical slot; selecting its current occupant avoids observing every historical logical occupant. The legacy request and all-values observation entry retain their existing behavior.

The implementation performs complete preflight before publishing selected scalar bytes and result counters. Its original legacy implementation prefix is unchanged; the new entry adds no allocator, Unity callback, Source or VM execution.

## Input ownership and selection

The caller supplies initialized, aligned, readable typed layout storage and independent writable frame/result/output storage. Owners remain live and stable throughout a serialized call. IDs, layout hashes and address arithmetic cannot establish arbitrary-pointer readability, allocation ownership, Source provenance or VM liveness. The active list comes from a producer that knows which values are initialized and live; this primitive does not infer an instruction position.

Preflight checks every layout row, including inactive mappings, physical geometry, classes, IDs and distinct physical span overlap. Active IDs resolve through logicalValueIds rather than array ordinals. Zero, absent or repeated IDs refuse. Two distinct active IDs sharing one physical slot refuse even when mirror bits are equal.

## Scalar publication contract

Only selected SCALAR rows in VALUE slots with physical widths1–8 are supported. REF/BOXED/INLINE_SPAN selections and wider scalar slots refuse. Valid inactive mixed-class rows remain untouched. A reused slot retains its physical width: selecting a later narrower logical value still copies the actual physical width using a fully initialized mirror. Recovering its original logical type or width requires later metadata.

A null scalarValues/count0 request is read-only. A nonnull scalar array must cover the complete logical extent. Writeback capacity covers the highest selected logical index plus one; invalidation capacity covers the active count. Output cells and tails not selected remain unchanged. Invalidation follows active-list order, including read-only observation.

Validation failure preserves the request, layout, frame, writeback, invalidation and result counters. Only a genuinely independent optional diagnostic may change. Complete declared writable spans are checked for overlap with each other and readonly storage before publication. Success copies exactly each physical width, zero-extends its selected UInt64 writeback, publishes counters last and clears the independent diagnostic. Empty selection publishes zero counters without changing storage.

## Provider closure and lifetime witnesses

The [descriptor](../../tests/cmake/ssa-active-packed-frame-observation.cmake) enumerates eighteen real C sources: runner, Unity, Parser roots/layout and fourteen Core providers. Genuine LayoutPackedFrameFree reaches Core FrameLayoutFree. The child links explicit sources without broad archives, forced includes, substitute providers or unresolved-symbol bypasses. Genuine SDK inline code remains subject to selected dependency and link provenance review; linked definitions do not certify executed purity.

Tests register actual allocations/layouts immediately, save numerical return/byte witnesses, and finish cleanup before assertions. The prior-failure case uses actual Unity failure and assertion-free teardown; its following top-level test checks saved witnesses after UnityConclude. It does not clear Unity flags or run a nested test runner. Numerical root drain proves registered test resources were released, not complete heap-leak detection.

## Validation and limits

See the [testing guide](../testing-and-validation/ssa-active-packed-frame-observation.md) for the exact accepted raw protocol and the [acceptance record](../acceptance/ssa-active-packed-frame-observation.md) for evidence status. The accepted GREEN child built51 explicit C occurrences and three links, ran the focused16-case protocol and both unchanged legacy regressions once, and reported no warnings or UBSan diagnostics. The focused result was15 PASS and one deliberate prior-Unity FAIL (child1, exact wrapper0);24 registered layout owners were drained after4161 API calls. MSVC separately compiled rootsC and the runner with warnings0; it did not link or run them. Linux, full MSVC runtime, genuine VM/Source integration, moving GC, debugger resume, backend maps, complete OOM, performance and all47 milestone gates remain OPEN.
