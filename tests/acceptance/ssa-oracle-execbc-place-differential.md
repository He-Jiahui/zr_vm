# SSA 01.05: pointer-free Place projection differential

## Scope

ExecBC projection preserves `PLACE_BASE` and `PLACE_PROJECT` as runnable
instructions in the pointer-free test runner. The caller supplies a Place
provider that translates canonical operand/descriptor tokens into an owned
address token; the runner never interprets that token as a host pointer.
`LOAD` still requires a separate memory provider. AOT projections remain
non-runnable, and production ExecBC code generation is not switched over.

## Test Inventory

- `test_ssa_execbc_place_differential` constructs base-only and base-plus-
  project functions with an explicit entry block and RETURN terminator. Both
  pass core STRUCTURE and SSA verification before execution. Independent
  Oracle and ExecBC providers map input token
  5 to base address 7, then optional index 9 to projected address 11. A load
  returns 99, and both runners agree on return kind/value, provider counts,
  LOAD event identity, and the address snapshot.
- A missing ExecBC Place provider returns `UNSUPPORTED` at the first Place
  instruction/source. Rejection returns `ORACLE_PLACE_ERROR`, and an undefined
  token returns `INVALID_VALUE`, at either base or project as selected by the
  fixture. Failed runs preserve the previously published result and events.
- The previously metadata-only `PLACE_BASE` and `PLACE_PROJECT` checks in
  `ssa_oracle_projections` now expect runnable ExecBC records; they still
  expect non-runnable AOT records. Other unsupported operations remain gated.

## Evidence and Boundary

The test was RED on MSVC 19.44.35228.0 before execution support: the new
fixture asserted a runnable ExecBC projection and failed at that assertion.
After the Place callback and dispatch were added, MSVC Debug static rebuilt
`zr_vm_ssa_oracle_projections_test` and its CTest passed 1/1. The parser static
library, source straight-line fixture and parallel-edge projection fixture
also rebuilt; the focused CTest matrix
`^ssa_(oracle_projections|oracle_parallel_edges|source_straight_line_cfg)$`
passed 3/3. After appending the Place fields to the end of the public input
structure, all affected MSVC targets rebuilt and the final regex also included
`ssa_value_validation`, passing 4/4. GCC's 45-second WSL attempt never reached
source compilation:
Ninja remained in `build.ninja` regeneration on the mounted repository and
reported interruption when the time limit expired. No fresh GCC/Clang or
sanitizer success is claimed. No physical Place layout, bytecode emitter,
AOT C/LLVM execution, or full 01.05 completion is claimed.
