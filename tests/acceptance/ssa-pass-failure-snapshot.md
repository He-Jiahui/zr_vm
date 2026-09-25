---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_pass_manager.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_manager.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_pass_manager.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_manager.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/03-effects-verifier.md
  - docs/plans/ssa/02-automatic-optimization/01-pass-manager-scalar.md
tests:
  - tests/parser/test_ssa_pass_manager_scalar.c
doc_type: testing-guide
status: focused-passed
---

# SSA failed-pass function snapshot acceptance

## Scope

A pass that damages its function IR must stop compilation, identify the pass,
preserve the malformed function as an owned diagnostic record, and restore the
caller's pre-pipeline function.  The captured function is the smallest
self-contained unit available for verification replay; no minimized
instruction-only fixture is claimed.  The pass name is copied, so a caller
may later modify its original buffer.  A successful reuse of the pipeline
clears the previous record.  No record is required for an invalid input
rejected before a pass begins or when the optional sink is omitted.

## Reproduce

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_pass_manager_scalar_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_pass_manager_scalar$' --output-on-failure --no-tests=error
```

The regression fixture asserts the exact diagnostic code, instruction and
source, an independently verifiable malformed snapshot, restored input hash,
copied pass name, and reset-on-reuse behavior.  The pre-implementation GCC
build failed because the failure record API did not yet exist.  Adding the
exact source assertion then exposed that the core verifier's unknown-opcode
diagnostic lacked a source ID; a guarded lookup in its diagnostic helper
supplies that ID only when instruction storage and bounds are valid.

## Toolchain results (2026-09-26)

- WSL GCC 11.4: rebuilt `ssa_pass_manager_scalar`, `ssa_effects_verifier`,
  `ssa_deopt_validation`, and `ssa_deopt_aggregates`; focused CTest 4/4
  passed.  The SSA-labelled CTest run passed 80/80, but only the four directly
  affected binaries were rebuilt in this phase.
- WSL Clang 14: rebuilt those four targets and ran focused CTest 4/4 passed.
- Windows MSVC 19.44: rebuilt those four targets and ran focused CTest 4/4
  passed.  The build still emitted the existing `/W3` to `/W4` override
  warning.
- WSL GCC AddressSanitizer/UndefinedBehaviorSanitizer in
  `build/ssa-gcc-asan-phase80`: rebuilt all four targets; focused CTest 4/4
  passed with no sanitizer error.

Accepted for the in-memory failing-function snapshot and guarded structural
source ID only.  A serialized disk artifact, automatic instruction-level
minimization, and broader M1 backend equivalence are not claimed.
