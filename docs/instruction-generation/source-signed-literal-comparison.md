---
related_code:
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_parser/src/zr_vm_parser/semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_format.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_compare_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_compare.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_compare.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg_finalize.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build_compare.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build_compare.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_compare_types.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_compare_types.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm_canonical_types.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm_internal.h
  - tests/cmake/ssa-builder-tests.cmake
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_compare.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_compare_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build_compare.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_compare_types.c
plan_sources:
  - .codex/plans/20261004-source-comparison-producer-lowering-design.md
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
  - docs/plans/ssa/01-execir-ssa/03-effects-verifier.md
tests:
  - tests/parser/test_ssa_source_execbc_vm.c
  - tests/parser/ssa_source_execbc_vm_compare.inc
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
doc_type: module-detail
status: implemented-native-green-pending
---

# Source signed literal comparison producer

## Implemented scope and evidence

This slice produces canonical SemIR for `<` and `>` whose AST operands are
integer literals and whose selected primitive types are signed i64. It does
not admit unary expressions, local operands, loops, arbitrary operators or
new foreach behavior. The actual if entry still consumes a defined semantic
condition value; a compare-specific witness checks the source literal shape,
predicate, source offsets and bool result identity before that value starts
the CFG.

Root's immutable V46 comparison RED log reported four tests, four failures,
zero ignored, all at the executable source CFG gate after ordinary compile
and preSemanticIr validation assertions passed. The author read the actual
log at `E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/source-direct-red-v46/comparisons-red.log`:
SHA256 `242c2ec3f05978da9d1a27463c08dc297e302e525352bbc16af3e7efdaaeb13f`.
The V46 original regression process separately hit stack overflow; Root owns
that investigation. This implementation has not been compiled or executed by
its author. Native Green and regression evidence must be recorded by Root.

## Source emitter and error behavior

`compiler_semantic_compare_lower` receives the actual binary AST as well as
the source compiler's selected opcode and stack slots. It accepts only
literal LT/GT, copies real semantic operand IDs before any array mutation,
verifies their canonical primitive i64 nodes, registers the inferred result
as canonical bool, adds that result value, binds its existing compiler slot
and emits the new comparison through the normal SemIR producer/source map.
No operand IDs are synthesized and no emitted bytecode is read back as
semantic evidence.

The helper returns NOT_APPLICABLE, LOWERED or FAILED. The actual binary
compiler calls its arithmetic/legacy fallback only for NOT_APPLICABLE.
An admitted comparison whose canonical type proof or append fails emits a
compiler error; it cannot silently claim canonical success through legacy
bytecode. Inactive terminated semantic paths remain not applicable. The
legacy selected signed instruction is emitted on successful canonical
production for the existing differential compilation route.

The emitter lives in its own coherent TU. Existing compiler files receive
small calls/includes. Opcode mapping formerly local to the builder moves
unchanged into `exec_ir_build_compare.c`, with the new COMPARE mapping
appended, keeping the builder orchestrator below 1000 lines.

## Representation and validation

SemIR appends COMPARE after DIV; old opcode numbers retain their values and
ENUM_MAX increases. Instruction/spec structures append
`comparisonPredicate` and `comparisonOperandTypeId`. These structure sizes
grow; this is not an external binary layout compatibility promise.

| Field/value | Meaning |
| --- | --- |
| instruction.typeId | Canonical bool result identity |
| resultValueId | New real bool semantic value |
| operands[0..1] | Original source-evaluation-order semantic values |
| comparisonOperandTypeId | Actual canonical signed-i64 operand identity |
| comparisonPredicate | Shared named LESS=1 or GREATER=3 selector |
| matchTypeId / shared matchTypeToken | Zero for compare |

The shared Core enum names the historical selector domain EQ=0, LT=1,
LE=2, GT=3, GE=4, NE=5. A selector is not a canonical TypeId. This source
slice admits LT/GT only; naming the historical domain does not add six-mode
source/AOT support. The former VM-private duplicate LT/GT declaration is
removed.

The relational inline validator checks predicate domain, two operands,
existing IDs, no result self-use, equal operand identities matching the
explicit operand identity, a result whose identity matches instruction type,
and distinct operand/result types. Unrelated opcodes require zero comparison
metadata. SemIR emission copies metadata, records the ordinary source map
and defines the result; SemIR validation rechecks the metadata. Golden
formatting prints operand type and predicate explicitly.

Compiler validation uses the actual canonical context to prove operands
are primitive INT64 and the result is primitive BOOL. Core generic structural
and SSA checks cannot derive primitive meaning from arbitrary opaque type
IDs. The builder rechecks relational metadata and lowers the explicit named
predicate into the existing COMPARE selector field; canonical bool remains
in the result value record. Shared TYPE_TEST-only MATCH rules are unchanged.

The straight-line scanner recognizes the same literal comparison helper and
requires a same-source, same-predicate result instruction. Existing literal,
arithmetic and unsupported control-flow paths retain their checks. The if
producer is corrected at its real defined-condition seam; loop admission is
not widened.

## Canonical VM boundary

`execbc_vm_prepare_canonical_compare_types` operates on the original
projection/context plus an owned staged instruction copy. It uses each real
ValueId to index `valueSlots[id-1]`, verifies the physical slot and matching
`slotValues` ID, then resolves the original canonical type. Both operands
must be signed i64 and the result bool. Pool ranges are checked before IDs
are read. Unknown or unsupported predicates are rejected. Incoming canonical
VM match metadata may be zero or the proven canonical operand INT64 type ID;
unknown IDs, BOOL and different canonical IDs are rejected after operand and
result proof. This preserves the public canonical VM API's explicit matched
type contract. Shared Core and AOT verification still require MATCH0.

Only the owned comparison instruction derives a missing matchTypeToken or
preserves a proven explicit canonical operand type ID. The ordinary resolver
subsequently converts it to the
runtime INT64 token. Writing runtime INT64 before canonical resolution would
confuse the two domains. Bool stays in the result metadata; the branch VM
validator checks bool condition use separately. Shared projection, input
function and canonical context remain borrowed and unmodified. Failure frees
the temporary arrays and the materializer output remains empty.

## Build closure and remaining acceptance

New compiled TUs are compiler_semantic_compare.c, exec_ir_build_compare.c and
exec_ir_execbc_compare_types.c. Normal module/direct validation source
discovery includes them after reconfigure. The explicit builder fixture pool
adds exec_ir_build_compare.c. A read-only search found no explicit CMake list
of exec_ir_execbc_vm_canonical_types.c under tests/scripts: current VM tests
link the parser module. Any later explicit list of that canonical TU must
also include its compare-types helper. Do not reuse the previous 666-TU
receipt as a receipt for these new inputs.

Root's next native gate must execute the four actual source comparisons and
the preserved thirteen regressions. Malformed operand/result primitive types
must fail at canonical proof, while bad IDs/predicates fail earlier. The
separate Core unknown-selector fixture/guard work is coordinated with Root
and must preserve the generic historical domain.

Source C/LLVM remains not established: existing canonical noargs-i64 callable
lowering needs real source callable identity/frame metadata and the actual
source scratch/CFG shape must reach the backend plan without replacement
arrays. Normal compiler automatic publication before child SemIR isolation
ends is also a distinct remaining task. No milestone completion or native
execution success is claimed here.
