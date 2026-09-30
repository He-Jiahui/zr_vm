---
related_code:
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.h
implementation_files:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/01-core-model.md
  - docs/plans/ssa/01-execir-ssa/05-oracle-projections.md
tests:
  - tests/parser/test_ssa_module_constant_pool_verifier.c
  - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc
  - tests/acceptance/ssa-module-constant-pool-verifier.md
doc_type: module-detail
---

# ExecIR Owned Module Constants

`ZrCore_ExecIr_VerifyModule` first validates the module arrays and fully
verifies each function. Its private constant verifier then visits every
`CONSTANT` when the module owns a nonempty pool. `layoutId` must be less than
`constantCount`; the referenced entry's type must equal the result value's
type. A nonzero instruction type must also equal that result type. All types
are opaque tokens, so canonical IDs work without a runtime-enum assumption.

Bounds failures use `INVALID_RANGE`; type mismatches use `INVALID_VALUE`.
The diagnostic carries the function token, owning block, instruction and
source IDs, plus the expected bound/type and actual index/type. The owning
block is resolved only on failure from already validated instruction ranges.
The verifier allocates no memory and changes no module or function fields.

A module with zero constants does not supply a pool bound. Its function can
be verified before an Oracle or projection receives a separate constants
array. `VerifyFunction` has no module argument and therefore does not perform
this ownership-dependent check. Artifact codecs and runnable consumers keep
their stricter pool, bit-representation, and supported-type checks.

The dedicated test records the original count-boundary RED, then exercises
the valid reference, both boundary indexes, both type mismatch forms, and the
external-pool controls. Current validation evidence is maintained in
`tests/acceptance/ssa-module-constant-pool-verifier.md`.
