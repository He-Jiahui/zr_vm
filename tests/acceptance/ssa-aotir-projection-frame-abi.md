# SSA 07.01: owned projection frame ABI metadata

## Scope

ExecBC and AOT projections now carry the complete scalar frame legalization
metadata and an owned copy of frame slots. `MoveProjectionToAot` transfers that
allocation, and the AOT projection destructor releases it exactly once. This
slice does not yet construct an `SZrAotIrModule` view or emit native code.

## Evidence

- `exec_ir_projection_common.c` and `exec_ir_lower_aot.c` compile with GCC
  11.4 using strict C11 warnings as errors.
- `test_ssa_aotir_state_map.c` adds a Unity assertion for metadata transfer,
  pointer transfer, source nulling and destination cleanup; both that fixture
  and `test_ssa_oracle_resume.c` compile with strict GCC after the new test is
  registered.
- The schema consumer and state-map direct fixtures remain covered by the
  cross-toolchain evidence in `ssa-aotir-logical-map-schema.md`.

## Ownership invariant

The source projection relinquishes `frameSlots` when the destination takes the
view. A destination with the projection ownership tag owns the allocation; a
zero-tag or partially initialized record is not freed. No pointer is copied
into a persistent artifact hash.

## Acceptance

Accepted as the frame-metadata producer slice. Shared descriptor conversion,
target ABI selection, source-map transfer and executable C/LLVM artifacts
remain open.
