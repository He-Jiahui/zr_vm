---
related_code:
  - zr_vm_core/include/zr_vm_core/execution_binding_guard.h
  - zr_vm_core/include/zr_vm_core/call_binding.h
  - zr_vm_core/src/zr_vm_core/execution/execution_binding_guard.c
  - zr_vm_core/src/zr_vm_core/call_binding.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/execution/execution_binding_guard.c
plan_sources:
  - docs/plans/ssa/03-interpreter-binding/03-guarded-caches.md
tests:
  - tests/core/test_ssa_guarded_caches.c
  - tests/acceptance/ssa-binding-guard-target-generation.md
doc_type: module-detail
---

# Binding guard VM target generation

`ZrCore_Execution_CheckBindingGuard` classifies a callsite witness using a
supplied frame generation, optional contract expectations and receiver facts.
Its return value and diagnostic distinguish expired generations from legal
shape misses. This local API does not dispatch the target or reset a binding.

## Independent generation checks

The binding's `generation` must match the caller-supplied `activeGeneration`.
For a resolved VM target with a non-null function and nonzero
`targetGeneration`, the function's `callBindingGeneration` must separately
match that recorded target generation. Caller and callee generation numbers
need not equal each other. A frame supplying its matching older generation
remains valid when the target witness also matches its own function.

The API does not fetch a newest global generation. A zero target generation
retains the existing unchecked-target convention; a null VM function retains
the existing target-missing check. Target pointers must already refer to valid
runtime metadata. This change adds no ownership or pointer-lifetime mechanism.

## Error priority

The check order is:

1. Invalid input and persistent contract validity.
2. Binding versus frame generation.
3. Optional module, signature and layout expectations, in their existing order.
4. Nonzero resolved VM target generation.
5. Receiver presence, shape and declared-slot bounds.
6. Existing target availability checks and successful hit reporting.

A stale VM target therefore returns `STALE_GENERATION` even when a receiver
shape ID or shape generation misses and the declared slot is in bounds. It
cannot become `SLOT_FALLBACK` or `SHAPE_MISS`. Contract expectation failures
still return their existing `CONTRACT_MISMATCH` classification before inspecting
that target generation. Other target availability precedence is unchanged.

For this VM stale result, diagnostic `bindingStatus` is
`ZR_CALL_BINDING_STALE_GENERATION`, `expected` is the recorded
`targetGeneration`, and `actual` is the observed function generation. This
matches the existing VM diagnostic in `ZrCore_CallBinding_Validate`; the guard's
former target-stale diagnostic used zero for both values. Target kind and
dispatch slot continue to identify the supplied binding. No ABI changes.

## Shape fallback and counters

A fresh VM witness still permits `SLOT_FALLBACK` for a shape miss when fallback
is allowed and the declared slot lies within the receiver's bounded slot table.
With fallback disabled it reports `SHAPE_MISS`. Matching shape facts and valid
target return `OK`.

Every failure, including fallback, increments `runtimeMissCount` once if a
cache entry is supplied and the count has not saturated at `UINT32_MAX`.
`OK` increments `runtimeHitCount` once with the same saturation rule. A miss
does not increment hits; a hit does not increment misses. Repeated calls report
the same classification while accumulating the corresponding count. Binding,
target metadata and receiver are not modified by this guard.

## Validation scope

The real Unity tests use local valid binding/function/prototype structures and
call the public guard. They cover both shape mismatch routes, allowed and
disabled fallback, repeated stale diagnostics, counter saturation, contract
priority, independent older caller generation and zero/null target controls.
They do not invoke the target callback or function.

Current C/H consumers of this API are the guarded-cache unit test; this finite
correction does not demonstrate interpreter dispatch integration, reload
leases, PIC bounds, GC movement, or completion of SSA milestone 03.03. Actual
build and execution evidence is recorded in the linked acceptance document.
