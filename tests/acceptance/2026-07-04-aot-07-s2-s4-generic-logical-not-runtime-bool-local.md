# AOT 07-S2/S4 Generic LOGICAL_NOT Runtime Bool Local

## Scope

- Slice: AOT 07-S2/S4, generic `LOGICAL_NOT` runtime primitive result consumed by a typed bool branch.
- Goal: keep runtime helper semantics for unproven primitive truthiness, but reuse the synced bool scalar local for the
  following `JUMP_IF_BOOL_FALSE`.
- Non-goals: string/object truthiness, value-copy migration, GC roots/exports/frame cleanup, byte-frame narrowing,
  performance counters, and complete zero-frame typed bodies.

## Baseline

- RED added a runtime-source `LOGICAL_NOT` shared-library fixture to
  `tests/parser/test_aot_c_generic_logical_not_numeric_local_smoke.c`.
- The old scalar-local proof only recorded generic `LOGICAL_NOT` bool destinations when the source slot was already a
  proven bool scalar local.
- That left runtime fallback results visible only through the frame, so the following typed bool branch rebuilt a
  condition pointer from `frame.slotBase[1].value` instead of reading `zr_aot_b1`.
- The first runtime-source fixture used a string source to force fallback; implementation showed the generated shape was
  fixed, but `ZrLibrary_AotRuntime_GenericPrimitiveLogicalNot` intentionally supports only primitive truthiness. The
  fixture was corrected to a null source, which still forces runtime fallback and is supported by the helper.

## Implementation

- `backend_aot_c_scalar_locals.c` now records a bool write for generic `LOGICAL_NOT` when the destination slot is
  declared as a bool scalar local.
- The record no longer requires the source slot to have a bool scalar-local proof.
- Runtime fallback generation remains intact:
  - `ZrLibrary_AotRuntime_GenericPrimitiveLogicalNot(state, &frame, 1, 0)`
  - `ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, 1, &zr_aot_b1)`
- The following branch can then use the synced local:
  - `if (!zr_aot_b1) {`

## Tooling Evidence

- Focused GREEN:
  - WSL GCC `zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test`
  - Result: `2 Tests 0 Failures 0 Ignored`.
- Generated C confirmation:
  - Required markers are present:
    - `GenericPrimitiveLogicalNot(state, &frame, 1, 0)`
    - `zr_aot_generic_logical_sync_bool_local_boundary`
    - `SyncBoolLocal(state, &frame, 1, &zr_aot_b1)`
    - `zr_aot_jump_if_bool_false_scalar_local`
    - `if (!zr_aot_b1) {`
  - Stale branch markers are absent:
    - `const SZrTypeValue *zr_aot_condition = ZR_NULL;`
    - `zr_aot_condition = &frame.slotBase[1].value;`
    - `zr_aot_condition_bool`

## Regression Matrix

- WSL GCC passed:
  - generic LOGICAL_NOT numeric/null smoke 2/0
  - logical shared-library smoke 6/0
  - generic JUMP_IF bool/numeric local 3/0
  - generic bool equality local 4/0
- WSL clang passed the same matrix:
  - generic LOGICAL_NOT numeric/null smoke 2/0
  - logical shared-library smoke 6/0
  - generic JUMP_IF bool/numeric local 3/0
  - generic bool equality local 4/0
- Windows MSVC Debug:
  - Built `zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test`.
  - Ran the test binary: `2 Tests 0 Failures 2 Ignored`.
- Patch checks:
  - `git diff --check -- tests/parser/test_aot_c_generic_logical_not_numeric_local_smoke.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_locals.c`
  - Result: no whitespace errors; only LF/CRLF normalization warnings.

## Acceptance Decision

Accepted for this slice. Generic primitive `LOGICAL_NOT` runtime results can now be synced into a bool scalar local and
reused by the following typed bool branch without rereading the frame. This is a partial 07-S2/S4 improvement only;
broader 07~12 work remains active.
