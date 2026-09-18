---
related_code:
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_while.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa_promotion.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/01-core-model.md
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
tests:
  - tests/parser/test_ssa_core_model.c
  - tests/parser/test_ssa_value_validation.c
  - tests/parser/test_ssa_place_eligibility.c
  - tests/parser/test_ssa_place_promotion.c
  - tests/parser/test_pre_semantic_ir_source_cfg.inc
  - tests/parser/test_ssa_effects_verifier.c
  - tests/acceptance/ssa-external-entry-values.md
doc_type: module-detail
---

# ExecIR model and ownership boundary

The first ExecIR model lives in
[`zr_vm_core/include/zr_vm_core/exec_ir.h`](../../zr_vm_core/include/zr_vm_core/exec_ir.h).
Core owns scalar IDs, opcode metadata, ranges, lifecycle, structural checks,
and deep cloning.  Parser construction is declared separately in
`zr_vm_parser/include/zr_vm_parser/exec_ir_builder.h`; the core header does
not include parser AST or the private AOT ExecIR header.

IDs are function-local and start at one.  Zero is the explicit invalid
sentinel; block one is reserved for an explicitly flagged entry block.  The
instruction, operand, result, phi, predecessor, successor, source, deopt, and
GC collections are module/function-owned side arrays.  Instructions contain
indices and stable tokens only, never runtime pointers or host addresses.

Values normally have exactly one ordinary instruction or phi definition.
Parameters, captures, and implicit frame roots are the exception: construct
them with `ZrCore_ExecIr_FunctionAddExternalValue`, which sets
`ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY`. These values are available from
function entry and keep an invalid ordinary instruction definition. Unknown
value flags, an external value reused as an instruction or phi result, and an
unflagged undefined operand are invalid. The explicit flag prevents analyses
from confusing a phi result or an unused reserved value with a parameter.

Place address values carry `ZR_EXEC_IR_VALUE_FLAG_PLACE_ADDRESS`. A screened
local root may also carry `ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE`; that flag
is invalid without the address flag. These bits are declarative facts rather
than completed optimization state: until SSA construction rewrites the Place,
its `PLACE_BASE`, `LOAD`, and `STORE` instructions remain authoritative.

`exec_ir_opcode.def` is the single opcode schema source for the enum and
metadata table.  It records operand bounds, terminator/value flags, and effect
classes for arithmetic, place, memory, call, allocation, ownership/drop,
control-flow, exception, suspension, and phi operations.

The SemanticIR builder preserves the original semantic value IDs and appends
two stable ranges for each canonical Place. The first range contains address
values defined by `PLACE_BASE` or `PLACE_PROJECT`; the second contains explicit
entry values for the storage root or projection descriptor. `LOAD` consumes
the address value, while both `STORE` and SemanticIR `INITIALIZE` consume the
address followed by the stored data value. Static projections use their entry
descriptor as the second operand; dynamic projections use their canonical
index value. This keeps frame roots and selectors explicit without encoding a
host pointer or reconstructing facts from ExecBC.

Promotion eligibility is likewise explicit. The SemanticIR producer supplies
the scalar-local fact, and the builder clears eligibility by omission for
parameters, projected roots, loans, and escapes. The value flags are included
by existing clone/hash paths and validated by core, so downstream passes do
not need access to parser-owned Place or canonical-type objects.

`ZrParser_ExecIr_BuildSsa` consumes that fact transactionally. It computes
pruned phi placement from live-in blocks and iterated dominance frontiers,
then renames along the dominator tree. An eligible `STORE` becomes a `NOP`
after updating the current definition, and an eligible `LOAD` becomes a
`COPY` from the current definition. Existing value and instruction IDs remain
stable; only phi result values and incoming rows are appended. The dead
`PLACE_BASE` remains as the stable Place identity in this stage. Unsupported
uses of an address disable promotion for that Place, and ineligible addresses
retain their original `LOAD` and `STORE` operations.

Promotion runs on a deep clone and replaces the caller's function only after
the rewritten candidate passes structural and SSA verification. A read before
any reaching definition reports `ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE` without
changing the input. Repeating the pass is a no-op because the promoted Place
has no remaining memory operations. Phi incoming rows preserve predecessor
occurrences, including loop backedges, parallel predecessor entries, and
critical edges. For an already split `INVOKE`, a pre-invoke definition is
available on both successors while the invoke result is available only on the
normal successor; the core verifier checks that boundary.

The source compiler supplies a canonical loop path directly for a
straight-line `while`. Before compiling the condition it closes the current
prefix with an unconditional edge to a dedicated header. The header contains
the condition facts and ordered true/body and false/join edges; a successful
body closes with a normal backedge to that same header. The compiler restores
the pre-loop semantic slot snapshot before entering the join, so body-only
temporaries cannot leak into later source lowering. This graph is independent
of ExecBC label offsets and reaches the existing dominator/frontier promotion
path, which inserts the loop-carried Place phi.

The source-loop subset intentionally accepts only linear conditions and
fall-through bodies whose nested statements are already modeled. `break`,
`continue`, return/throw, calls, cleanup, suspension, and short-circuit paths
still trigger the legacy-CFG fallback. If an unsupported loop appears after a
source CFG has started, the compiler abandons that partial graph and removes
its synthetic branch instructions before validation.

Before SSA construction, the parser normalizes a canonical block whose final
typed call already carries ordered normal/exception edges. Each earlier typed,
virtual, dynamic, or meta call becomes the terminator of a new `INVOKE` block:
its normal edge enters the next segment and its exception edge enters the same
handler as the final call. Original block targets are remapped to the first
segment of their destination, and predecessor occurrences are rebuilt after
the transform. The input SemanticIR and caller-owned output remain unchanged
if allocation or later verification fails. Operations that are schema-marked
may-throw but cannot be represented by the current call-shaped `INVOKE` remain
an explicit unsupported diagnostic rather than borrowing a later call's edge.

`RETURN` accepts zero operands for a void function and one operand for a value
return. The opcode metadata exposes this as a zero minimum and one maximum, so
the builder, SSA precheck, core verifier, and oracle use the same range.

`ZrCore_ExecIr_CloneModule` and `ZrCore_ExecIr_CloneFunction` build a temporary
deep copy and publish it only after every side-array allocation succeeds.
Module clone rollback includes the function currently being copied, even if a
later side-array copy fails after earlier arrays have allocated storage; the
previous destination remains published. `ssa_core_model` exercises this with
two source functions, an invalid instruction pool in the second function, and
a pre-existing destination. GCC AddressSanitizer with leak detection caught
the partial-function leak before the rollback fix and reports no leak after it.
Structural validation reports the first unknown opcode, invalid range, block,
or value with a stable diagnostic identity.  This slice is intentionally not
the default compiler path yet; SSA construction and projections consume it in
the following M1 tasks.

The direct reference execution boundary is documented separately in
[`execir-oracle-memory.md`](execir-oracle-memory.md). Its memory callback is
caller-owned and deterministic, as is the pointer-free allocation callback;
neither boundary is represented as a host-pointer field inside ExecIR.
