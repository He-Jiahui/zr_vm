# SemIR result definition identity

## Scope

This is a partial 01.02 SSA construction stage. The SemIR producer rejects
duplicate result definitions without publishing a partial instruction. Snapshot
validation checks that every instruction result points back to its defining
instruction. External entry values with no instruction retain definition ID
zero; the builder's legacy hand-built-fixture lookup remains separate.

## Regression

The new `test_semantic_result_has_one_definition` first failed with 102 tests,
one failure: `SemanticIr_Validate` accepted a produced result whose cached
definition ID was set to zero. After the production check, the pre-semantic
suite exposed two old synthetic fixtures that reused one result ID: the opcode
golden now constructs formatting input directly, and the flow-join fixture uses
distinct results for independent operations. The new regression also verifies
that a rejected second emission leaves instruction, operand and source-map
pools and the original definition unchanged, while a fresh result emits the
next contiguous instruction ID. A stale in-range definition ID fails validation.
A second red run (102 tests, one failure) showed that an otherwise external
value could claim a nonzero ID for an instruction that produced another value;
the reverse validation check now rejects it while preserving true external
entry values with a zero definition ID.

## Evidence

- WSL GCC: `zr_vm_pre_semantic_ir_test`, 102/102 passed.
- WSL GCC CTest: `ssa_construction`, `ssa_builder_fact_identity`, and
  `ssa_source_straight_line_cfg`, 3/3 passed.
- WSL Clang: production `semantic_ir.c` passed `-std=c11 -fsyntax-only
  -Wall -Wextra -Wpedantic -Werror` with the parser include paths and
  production compile definitions.
- Diff whitespace check: `git -c core.safecrlf=false diff --check` passed.

The WSL Clang full target was stopped at 246/724 compilation steps because
WSL mounted-workspace directory scanning blocked in `p9_client_rpc`. The
MSVC static target likewise began rebuilding hundreds of previously missing
dependencies and was stopped before completion. Neither incomplete build is
reported as a passing test. The GCC final suite used the cached Ninja compile
and link commands to bypass a blocked CMake glob regeneration step; it compiled
both changed translation units and relinked the parser and test binaries
before the final test run.

This stage does not claim all use-dominance, effect-token, exceptional-edge,
or cross-backend SSA gates in the full plan are complete.
