---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_builder.h
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/include/zr_vm_core/exec_ir_opcode.def
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_place_eligibility.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_call_graph.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_escape.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_inline.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
tests:
  - tests/parser/test_ssa_value_validation.c
  - tests/parser/test_ssa_place_eligibility.c
  - tests/parser/test_ssa_effects_verifier.c
  - tests/parser/test_ssa_escape_ownership.c
  - tests/parser/test_ssa_interprocedural_inlining.c
  - tests/cmake/ssa-tests.cmake
  - tests/acceptance/ssa-value-validation.md
  - tests/acceptance/ssa-external-entry-values.md
doc_type: module-detail
---

# Parser ExecIR SSA value validation

## Scope and entry point

`ZrParser_ExecIr_BuildSsa` currently verifies the stable definitions already
assigned to values produced by SemanticIR. It does not insert pruned phi
nodes, promote address-taken places, rename definitions across a dominance
frontier, or prove exceptional-result availability. The builder invokes this
step only after it emits CFG adjacency and computes immediate dominators.

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
metadata is the input contract for phi insertion and renaming; this stage does
not remove memory operations yet.

The pass checks function-level storage consistency before reading instructions
or operand/value side pools: counts may not exceed allocated capacities and a
nonempty pool needs backing storage. Per instruction, the opcode must be known,
fixed operand arity must match, and `[start, start + count)` must fit within
the logical operand pool. The range comparison subtracts only after checking
`start <= operandCount`, so a `UINT32_MAX` start cannot wrap around to a small
index. Operand IDs must reference either a locally defined value or an
explicit external entry value in the function's value pool. No input arrays
or value definitions are modified on either path.

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
`INVALID_VALUE`. Instruction-level diagnostics carry the function token and
one-based instruction ID. Function-level malformed backing storage carries
the function token and instruction ID zero. Null function input returns false
without dereferencing the diagnostic target. The pass owns no allocations;
the surrounding builder owns and releases its temporary ExecIR function if
construction fails.

## Verification boundary

`ssa_value_validation` links the production parser validation and core model.
Its positive cases define an operand before its use and consume an explicit
external entry. Failure cases cover an unflagged undefined operand, an
external entry reused as an instruction result, unknown value flags, a
logical out-of-range operand whose physical memory contains a valid value, a
wrapped range, null backing storage, and an unknown opcode. Full CFG-aware SSA
construction and differential language fixtures remain separate 01.02 work;
see `tests/acceptance/ssa-value-validation.md` and
`tests/acceptance/ssa-external-entry-values.md` for actual test runs.
