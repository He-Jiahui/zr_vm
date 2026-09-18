# SSA 01.02: verify dominance before builder publication

## Boundary fixtures

The builder used to accept a `CALL` reading value 1 before a later
`CONSTANT` defines it in the same block. The initial MSVC red run printed
`FAIL: builder published a use occurring before its SSA definition`.
The new `ssa_builder_dominance` target expects `DOMINANCE`, function token
42, block/instruction/source 1 and definition/value [2,1], and proves
the previously initialized caller output remains unchanged.

A positive `CONSTANT`/`CALL`/`RETURN` ordering builds with the expected
definition and operand and leaves the unpublished function ID invalid.
A separate four-block diamond defines the return value only on its left
branch; the join use now reports `DOMINANCE` at block/instruction/source 4
instead of publishing an invalid SSA function. These cases reuse the core
verifier's existing SSA analysis; they do not implement phi placement.

## Observed validation

- MSVC (VSDevCmd, D:-backed build): both independent builder targets
  compiled and the adjacent SSA CTest selection passed 9/9. The first
  integration attempt failed an empty diamond with `INVALID_ARGUMENT`
  because the candidate had not yet received a module ID; the temporary
  verifier-only ID is restored after checking, including on failure.
- WSL GCC 11.4 compiled the new focused test with real builder/core sources
  under `-fsanitize=address,undefined`; it printed
  `ssa builder dominance PASS` (exit 0) without a sanitizer report.
- WSL Clang 14 compiled the same sources without sanitizers; it printed
  `ssa builder dominance PASS` (exit 0). Full CTest on GCC/Clang and
  Clang sanitizers were not run for this slice.

## Remaining gates

The builder still does not promote local places, insert pruned phis, rename
definitions, split throwing blocks, or establish source-language/backend
parity. The separately dirty `test_ssa_construction.c` remains untouched.
