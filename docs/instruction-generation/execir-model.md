---
related_code:
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir_call.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir_optional.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_flow.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_logical.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_receiver_guard.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_while.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa_promotion.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir_call.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir_optional.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_logical.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_receiver_guard.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
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
  - tests/parser/test_pre_semantic_ir_optional_value.inc
  - tests/parser/test_pre_semantic_ir_general_call.inc
  - tests/parser/test_pre_semantic_ir_exception_fallback.inc
  - tests/parser/test_pre_semantic_ir_throw_cfg.inc
  - tests/parser/test_pre_semantic_ir_return_cfg.inc
  - tests/parser/test_ssa_effects_verifier.c
  - tests/acceptance/ssa-external-entry-values.md
  - tests/acceptance/ssa-compiler-ownership-execir.md
  - tests/acceptance/ssa-compiler-source-optional-value-cfg.md
  - tests/acceptance/ssa-compiler-source-general-call-cfg.md
  - tests/acceptance/ssa-compiler-source-exception-fallback.md
  - tests/acceptance/ssa-compiler-source-throw-cfg.md
  - tests/acceptance/ssa-compiler-source-return-cfg.md
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

Producer-less SemanticIR values are materialized as explicit external-entry
ExecIR values. Current producers cache the defining instruction ID on each
defined value; for older hand-built fixtures that leave this field at zero,
the builder treats instruction result references as the authoritative
compatibility fallback. A value is external only when neither representation
names an instruction definition.

Ownership and view facts retain their canonical type tokens while the builder
expands them into the first executable opcode surface. Unique construction,
GC-box transfer, and artifact-only return-to-GC become `MOVE`; sharing,
degrading, waking, borrow, reborrow, reserve-borrow, and dereference become
`COPY`; deterministic release remains `DROP`. Loan activation and end markers
become source-mapped `NOP` instructions because they carry semantic lifetime
information but no standalone runtime value operation at this stage. ExecIR
ownership and nullability fields remain unknown until the dedicated metadata
projection is implemented.

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
`continue`, return/throw, calls, cleanup, suspension, and short-circuit loop
conditions still trigger the legacy-CFG fallback. If an unsupported loop
appears after a source CFG has started, the compiler abandons that partial
graph and removes its synthetic branch instructions before validation.

Source `&&` and `||` expressions with linear operands also publish their
short-circuit topology directly. `&&` sends the true edge to the RHS and the
false edge to the join; `||` reverses those targets while retaining the
canonical true/false successor order. The left value initializes a private
temporary Place, the evaluated RHS stores into that Place, and the join loads
one merged expression value. The temporary remains explicit memory rather
than being marked as a promotable source local. Unsupported operand families
abandon any partial source CFG and retain the legacy two-block path.

A resolved, non-spread source function call can also establish the source CFG
when no earlier branch has done so. The existing straight-line facts become the
entry block, which jumps to a dedicated call block. A typed `CALL_TYPED` or
`CALL_META` records the canonical callable ValueId, explicit argument ValueIds,
resolved symbol, result TypeId, and result ValueId. Ordered normal and exception
successors lower that block to `INVOKE`; only the normal successor owns the
result, while the exception successor is the same explicit propagation sink
used by the optional-call slice. An unresolved/dynamic call, a spread call, or
a call missing a canonical prerequisite does not invent a target or operand:
it leaves an inactive graph on the legacy path, or abandons an already active
partial source graph. While an enclosing `if`, loop, or short-circuit construct
is compiling after its own CFG preflight failed, inactive call-driven startup
is suppressed; a nested call therefore cannot create a detached unconditional
graph for a conditionally executed operation.

Until handler payloads, catch selection, and finally cleanup edges are part of
the source graph, `try`/`catch`/`finally` is also an explicit conservative
boundary. Entering that scope abandons any partial source CFG, and all inactive
CFG starters stay suppressed for the rest of the current SemanticIR function.
This includes its protected, handler, cleanup, and trailing source regions: a
later starter cannot absorb the earlier exception scope into a false linear
prefix. This function-level block is separate from scoped fallback suppression,
so an enclosing construct cannot clear it while restoring its own state. The
legacy compiler still emits the executable exception machinery; ExecIR does
not publish a detached graph that omits those transfers.

Outside an unmodeled handler scope, an explicit source `throw` is a real
non-call exceptional terminator. It consumes the expression's canonical
ValueId, emits a one-operand SemanticIR `THROW`, binds the current block with
`ZR_PARSER_CFG_TERMINATOR_THROW`, and publishes no successors. That block is
also the function CFG exit; when no earlier source boundary exists, the
straight-line prefix becomes the single entry/exit block. The same operation
can terminate the normal continuation of an already split `INVOKE` graph.
Afterward a function-level termination latch prevents later source text from
adding SemanticIR instructions or starting another CFG, while legacy ExecBC
emission continues for compatibility. Throws nested inside control-flow whose
source CFG preflight already fell back, and throws inside `try`/`catch`/
`finally`, remain on the conservative legacy path until handler and cleanup
edges are modeled together.

The compiler-owned entry body now publishes explicit source `return` through
the same value-terminator machinery. A value return consumes its canonical
ValueId; bare `return;` materializes the language null result as a typed
constant. The resulting one-operand SemanticIR `RETURN` closes the current
block with `ZR_PARSER_CFG_TERMINATOR_RETURN`, has no successors, and becomes
the CFG exit. It can either create a single entry/exit block or terminate an
existing call's normal `INVOKE` continuation. The termination latch keeps
later unreachable source on ExecBC compatibility lowering without allowing a
detached CFG restart. Unmodeled return expressions, fallback branch arms,
finally transfers, and ownership cleanup transfers block further CFG startup
instead of publishing a false direct return. Declared child callables still
lack separately owned published pre-execution functions; their return bytecode
therefore cannot close the entry body's graph. Child return expressions lower
against a disposable isolated SemanticIR state, keeping their instructions,
Values, Places, loans, slots, and CFG state off the entry-body sidecar.

The same producer owns a bounded optional-access slice. A canonical nullable
receiver guard emits ordered present-true and absent-false edges, and call
arguments and suffix side effects belong only to the present path. A known
typed member call is isolated in its own terminal block, where a source-owned
`CALL_TYPED`, `CALL_VIRTUAL`, or `CALL_META` carries the canonical receiver as
its typed callee operand, followed by explicit argument ValueIds and a typed
result. Runtime's hidden receiver count is not duplicated in that explicit
argument range. Ordered normal and exception edges lower the call to `INVOKE`.

For `VOID_NOOP`, the absent edge reaches the join directly. For a nullable
value result, it enters a dedicated absent block. The normal continuation
converts and stores the call result into a typed temporary Place; the absent
block stores a typed null constant into the same Place; and the join loads one
merged ValueId. The call's exception edge instead enters an explicit
propagation sink and cannot reach the merge. Isolating the call prevents
earlier present-path stores from being attributed to its exceptional transfer.
The sink has no fabricated `THROW` operand because edge-defined exception
payload values are not yet part of the model. Weak-wake guards, calls that lack
required canonical facts, and cleanup suffixes still abandon an active partial
graph and use the legacy two-block path, so this checkpoint does not claim the
complete optional-chain exit gate.

The complete ownership setup used by this nullable-call fixture now also
builds through ExecIR. Its source and result values have canonical TypeIds,
receiver aliases inherit the source value before borrowing, and every
producer-less entry value is represented explicitly rather than rejected as
an undefined operand.

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
