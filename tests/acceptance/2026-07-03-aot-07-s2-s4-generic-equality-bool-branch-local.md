# AOT 07-S2/S4 Generic Equality Bool Branch Local Reuse

## Scope

- Slice: AOT 07-S2/S4, generic primitive equality bool result reuse after a required runtime boundary.
- Goal: keep `ZrLibrary_AotRuntime_GenericPrimitiveLogicalEqual/NotEqual` for mixed primitive equality semantics, but
  avoid rereading the already-synced bool result from `frame.slotBase[9].value` when the following branch can consume
  `zr_aot_b9` directly.
- Non-goals: operand constant materialization removal, full generic equality direct lowering, dynamic/string/object
  truthiness, value-copy migration, GC roots/exports/frame cleanup, byte-frame narrowing, performance counters, and
  complete zero-frame typed bodies.

## Baseline

- RED expanded `test_aot_c_generated_shared_library_executes_generic_primitive_equality_boundary_helpers` with generated
  C assertions requiring `if (!zr_aot_b9) {` and forbidding `zr_aot_condition = &frame.slotBase[9].value;`.
- WSL GCC focused run failed with `Expected Non-NULL`, proving the old generator did not emit the bool-local branch
  after `SyncBoolLocal`.
- Generated C inspection showed the intended dynamic equality boundary was still present; the defect was only the
  downstream typed branch rereading the generated frame instead of the synced bool local.

## Implementation

- `backend_aot_c_scalar_locals.c` now records `LOGICAL_EQUAL` and `LOGICAL_NOT_EQUAL` destinations as bool local writes
  when the declared destination slot is a bool local and the destination is the observed slot.
- The exec write and bool-value write trackers no longer require both equality operands to be already-proven bool
  locals for this destination-write proof.
- Runtime equality semantics remain unchanged: `GenericPrimitiveLogicalEqual/NotEqual` and `SyncBoolLocal` are still
  emitted for the generic boundary.

## Tooling Evidence

- RED:
  - `wsl.exe bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_logical_shared_library_smoke_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_logical_shared_library_smoke_test"`
  - Result: failed as expected in the generic equality smoke with `Expected Non-NULL`.
- Focused GREEN:
  - Same WSL GCC focused command passed `6 Tests 0 Failures 0 Ignored`.
  - Rerun after removing unused-parameter warnings also passed cleanly.
- Generated C confirmation:
  - `GenericPrimitiveLogicalEqual` / `GenericPrimitiveLogicalNotEqual` remain present.
  - `ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, 9, &zr_aot_b9)` remains present.
  - `if (!zr_aot_b9) {` is present.
  - `zr_aot_condition = &frame.slotBase[9].value;` is absent.

## Regression Matrix

- WSL GCC passed:
  - logical shared-library smoke 6/0
  - shared-library smoke 13/0
  - generic LOGICAL_NOT numeric local smoke 1/0
  - generic JUMP_IF bool/numeric local smoke 3/0
  - source contracts 24/0
  - guardrail contracts 6/0
  - typed direct-call bool 28/0
  - typed direct-call u64 25/0
  - typed direct-call f64 19/0
- WSL clang passed the same matrix:
  - logical shared-library smoke 6/0
  - shared-library smoke 13/0
  - generic LOGICAL_NOT numeric local smoke 1/0
  - generic JUMP_IF bool/numeric local smoke 3/0
  - source contracts 24/0
  - guardrail contracts 6/0
  - typed direct-call bool 28/0
  - typed direct-call u64 25/0
  - typed direct-call f64 19/0
- Windows MSVC Debug:
  - Built the same target group successfully.
  - Source contracts passed 24/0.
  - Guardrail contracts passed 6/0.
  - Unix-only shared-library/direct-call smoke cases reported ignored with 0 failures.

## Acceptance Decision

Accepted for this slice. The generic equality bool result can now stay in a bool scalar local for the immediate typed
branch after runtime boundary sync. This is a partial 07-S2/S4 improvement only; broader 07~12 work remains active.
