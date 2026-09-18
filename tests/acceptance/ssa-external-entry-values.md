---
doc_type: acceptance-record
plan: docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
implementation:
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_call_graph.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_escape.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_inline.c
tests:
  - tests/parser/test_ssa_value_validation.c
  - tests/parser/test_ssa_effects_verifier.c
  - tests/parser/test_ssa_escape_ownership.c
  - tests/parser/test_ssa_interprocedural_inlining.c
status: partial
---

# SSA 01.02: explicit external entry values

## Scope and failing baseline

ExecIR previously represented a parameter-like input only as a value with no
ordinary instruction definition. That was indistinguishable from a missing
definition and from a phi result, whose optional instruction back-pointer is
also zero. A focused test that used an explicitly flagged entry operand first
failed in `ZrParser_ExecIr_BuildSsa` with `INVALID_VALUE`. After the parser
accepted the flag, the core verifier initially exposed the same missing model
and rejected the value.

The core model now provides `ZrCore_ExecIr_FunctionAddExternalValue` and the
single allowed `ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY` bit. Parser and core SSA
verification accept that definition kind at entry, reject unknown flags,
reject unflagged undefined operands, and prevent external entries from being
ordinary instruction or phi results. Inlining and escape analysis classify
entry values by the flag instead of the ambiguous zero definition field.

## Verified cases

- An explicit external entry operand passes parser and core SSA verification.
- An unflagged undefined operand reports `INVALID_VALUE` at its use.
- Unknown value flags report `INVALID_VALUE`, including on an unused value.
- An external entry cannot be appended as an ordinary instruction result;
  core verification also rejects any external/phi result collision.
- Branch conditions and exceptional cleanup inputs remain available from
  entry without hiding cross-branch or exceptional-result dominance errors.
- Interprocedural parameter mapping and escape lifetime propagation consume
  explicit entry values while ordinary results retain their local definitions.

## Validation evidence (2026-09-18)

- MSVC 19.44 in `D:/zr-ssa-verify-871bc234`: `ssa_value_validation`,
  `ssa_effects_verifier`, `ssa_state_maps`, `ssa_escape_ownership`, and
  `ssa_interprocedural_inlining` passed 5/5.
- WSL GCC 11 in `D:/zr-ssa-verify-871bc234/wsl-gcc`: the 16-test focused SSA
  regression gate passed 16/16 after rebuilding all affected core/parser test
  targets. The source SemanticIR executable also passed 17/17.
- WSL Clang 14 in `D:/zr-ssa-verify-871bc234/wsl-clang`: the five directly
  affected suites passed 5/5.
- The builds emitted existing unused-function, missing-initializer,
  const-qualifier, and MSVC `/W3` overridden by `/W4` warnings; no new warning
  was attributed to this change.

## Acceptance boundary

This accepts the representation and verification contract for values supplied
at function entry. It does not claim that the source compiler already projects
parameters, captures, or frame roots into ExecIR, nor does it complete pruned
phi insertion, place promotion, rename, exceptional block splitting, or the
full 01.02 construction exit gate.
