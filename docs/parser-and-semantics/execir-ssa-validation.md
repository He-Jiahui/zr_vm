---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_builder.h
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/include/zr_vm_core/exec_ir_opcode.def
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa_promotion.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_place_eligibility.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_call_graph.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_escape.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_inline.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
tests:
  - tests/parser/test_ssa_construction.c
  - tests/parser/test_ssa_value_validation.c
  - tests/parser/test_ssa_place_promotion.c
  - tests/parser/test_ssa_place_eligibility.c
  - tests/parser/test_ssa_effects_verifier.c
  - tests/parser/test_ssa_escape_ownership.c
  - tests/parser/test_ssa_interprocedural_inlining.c
  - tests/cmake/ssa-tests.cmake
  - tests/acceptance/ssa-value-validation.md
  - tests/acceptance/ssa-external-entry-values.md
  - tests/acceptance/ssa-place-promotion.md
  - tests/acceptance/ssa-construction-builder-phi.md
doc_type: module-detail
---

# Parser ExecIR SSA value validation

## Scope and entry point

`ZrParser_ExecIr_BuildSsa` checks stable definitions assigned to SemanticIR
values, then promotes eligible Places with pruned phis and dominator-tree
renaming. Exceptional-result availability is checked by the core SSA verifier.
The builder invokes this step after emitting CFG adjacency and computing
immediate dominators, and before synthesizing memory/effect tokens.

The `ssa_construction` integration fixture feeds a canonical four-block
SemanticIR diamond into `ZrParser_ExecIr_Build`. Stores to the same eligible
local in both arms produce one join phi whose incoming values follow the
predecessor row; the join load becomes a `COPY` from that phi. Replacing the
load with a constant prunes the unused phi. Removing an arm's reaching store
instead reports `INVALID_VALUE` at the join and leaves previously published
builder output intact. These assertions cover normal diamond control flow;
they do not establish source-level optional, exception, cleanup, suspend, or
loop parity required by the full 01.02 gate.

Before this check, source-produced canonical Places are assigned separate
ExecIR address values. Place results are ordinary instruction definitions;
their storage roots and static projection descriptors are explicit external
entry values. Lowered loads and stores therefore reach SSA verification with
fully defined operands even though the backing storage remains unpromoted.
Semantic data value IDs retain their original numeric identity; appended
address/provenance values cannot renumber source facts.

The builder preserves the first promotion-screening decision on those address
values. `PLACE_ADDRESS` identifies every lowered Place address, while
`PROMOTABLE_PLACE` identifies only a direct non-parameter local whose producer
proved a canonical primitive representation and for which the function has no
child projection, loan fact, or escape fact. The screen is fail-closed:
parameters, aggregate or unknown representations, projected Places,
address-taken locals, and escaped locals retain explicit load/store form. This
metadata is the input contract for phi insertion and renaming; the promotion
pass replaces eligible stores with NOP and loads with COPY.

When `BuildSsa` is called directly with pre-existing memory tokens, promotion
validates both the input and its rewritten clone at the effect level as well
as the structural and SSA levels. If the rewrite invalidates a token chain,
the pass returns the effect diagnostic without publishing the clone. Inputs
without existing tokens are checked for effects later by the builder, after
effect synthesis; they are not required to carry token facts prematurely.

The pass checks function-level storage consistency before reading instructions
or operand/value side pools: counts may not exceed allocated capacities and a
nonempty pool needs backing storage. Per instruction, the opcode must be known,
fixed operand arity must match, and `[start, start + count)` must fit within
the logical operand pool. The range comparison subtracts only after checking
`start <= operandCount`, so a `UINT32_MAX` start cannot wrap around to a small
index. Operand IDs must reference either a locally defined value or an
explicit external entry value in the function's value pool. No input arrays
or value definitions are modified by the initial validation; promotion only
publishes a fully verified candidate.

## External entry values

`ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY` distinguishes parameters, captures,
and implicit frame roots from missing SSA definitions. Producers create these
values through `ZrCore_ExecIr_FunctionAddExternalValue`; their ordinary
instruction definition stays invalid because they are available at function
entry. The parser pass rejects unknown flags and rejects an external entry
that also carries an ordinary instruction definition. An ordinary unflagged
operand still requires a definition.

The core SSA verifier seeds external entries as a separate definition kind.
They dominate every ordinary use and every valid phi predecessor edge, but
cannot appear as an instruction or phi result. Call-graph and inlining
parameter discovery, plus escape function-lifetime initialization, use the
flag rather than inferring parameter status from `definition == 0`; that
inference was unsound for phi results and unused reserved values.

`ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE` is valid only together with
`ZR_EXEC_IR_VALUE_FLAG_PLACE_ADDRESS`. Both the parser SSA entry point and the
core structural verifier reject a promotable bit on an ordinary value. Value
flags remain part of the existing function hashes, so the eligibility decision
participates in cache and pass identity without a parallel side channel.

## Diagnostics and ownership

Invalid storage, arity or operand range reports `INVALID_RANGE`; an unknown
opcode reports `UNKNOWN_OPCODE`; an invalid or undefined value reports
`INVALID_VALUE`. Instruction-level diagnostics carry the function token,
one-based instruction ID, and the instruction's existing source ID. When a
valid block table identifies the containing instruction range, they also
carry its block ID; incomplete or malformed block tables do not prevent the
original validation error from being reported. Function-level malformed
backing storage carries the function token and instruction ID zero. Null
function input returns false without dereferencing the diagnostic target.
Promotion allocates a temporary clone and releases it on failure; the
surrounding builder similarly owns and releases its candidate if construction
fails.

## Verification boundary

`ssa_value_validation` links the production parser validation and core model.
Its positive cases define an operand before its use and consume an explicit
external entry. Failure cases cover an unflagged undefined operand, an
external entry reused as an instruction result, a source-located undefined
operand use, unknown value flags, a logical out-of-range operand whose
physical memory contains a valid value, a wrapped range, null backing
storage, and an unknown opcode. Promotion's
CFG-aware and tokenized-input coverage is recorded separately in
`tests/acceptance/ssa-place-promotion.md`; the full 01.02 exit gate remains
outside these focused fixtures. See `tests/acceptance/ssa-value-validation.md`
and `tests/acceptance/ssa-external-entry-values.md` for earlier validation.
