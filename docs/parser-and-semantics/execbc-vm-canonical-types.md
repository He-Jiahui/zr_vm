---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_execbc_vm.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm_canonical_types.c
  - zr_vm_parser/include/zr_vm_parser/canonical_type.h
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm_canonical_types.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/05-oracle-projections.md
tests:
  - tests/parser/test_ssa_execbc_vm_canonical_types.inc
  - tests/parser/test_ssa_execbc_vm.c
  - tests/parser/test_ssa_source_execbc_vm.c
  - tests/acceptance/ssa-source-execbc-vm.md
doc_type: module-detail
---

# ExecBC VM Canonical Types

Source-built ExecIR uses semantic canonical IDs for scalar types. Those IDs
are local to the semantic context and are not `EZrValueType` values. The
canonical materializer entry point takes that owning context explicitly and
resolves each type through `ZrParser_CanonicalType_Find`.

Only primitive bool and i64 nodes are accepted. Narrow integer, reference,
owner, nominal, unknown, and missing-context inputs fail before a Core
function is allocated. No type is inferred from the numeric ID. Constant pool
tokens use the same canonical identity as their instruction result.

The canonical type array must be initialized, have the correct element size,
and have a non-overflowing capacity with `length <= capacity` and storage for
nonempty contents. The adapter rejects malformed descriptors before lookup.

The adapter copies the instruction, slot-value, and constant arrays into a
temporary borrowed view, translates their type fields, and calls the normal
materializer. `COMPARE.typeToken` is a comparison-kind tag and stays unchanged;
its `matchTypeToken` is resolved as a type. CFG, source/PC maps, phi schedules,
memory/effect fields and GC/deopt metadata stay intact and pass the ordinary
materializer's validation. The original projection remains unchanged on both
success and failure. Only the three copied arrays are freed by this adapter.

Successful function rooting and emission ownership follow the normal
materializer contract. The focused tests deliberately reserve IDs before
interning primitive types, execute the emitted function through Core, and
check unknown/narrow/mismatched/missing inputs plus byte-for-byte preservation
of all copied input arrays. Real source branch tests resolve against the
compiler's own semantic context.

The 2026-09-30 root MSVC test-first run accepted an invalid type-array
descriptor in `canonical-type-shape-red-direct.log`. After array validity and
capacity checks were added, `ssa_exec_ir_execbc_vm` passed all 18 Unity cases
in `current-scalar-shape-fixtures-ctest.log`, including eight negative adapter
inputs with empty output and unchanged projection arrays. The same selection
passed both real source branch cases. Broader GCC/Clang and full semantic
matrix results are maintained in the source acceptance record.
