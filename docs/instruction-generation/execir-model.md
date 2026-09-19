---
related_code:
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/include/zr_vm_core/exec_ir_opcode.def
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_parser/src/zr_vm_parser/semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_format.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_scope.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg_loop.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg_catch_dispatch.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg_finally.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg_try.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir_call.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir_optional.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_for.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_foreach.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_flow.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_try.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_while.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_logical.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_receiver_guard.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa_promotion.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build_control_edges.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_normalize_cfg.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projection_common.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/exec_ir_opcode.def
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_scope.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg_loop.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg_catch_dispatch.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg_finally.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg_try.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir_call.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir_optional.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_for.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_foreach.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_flow.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_try.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_while.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_logical.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_receiver_guard.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_parser/src/zr_vm_parser/semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_format.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build_control_edges.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_normalize_cfg.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projection_common.c
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
  - tests/parser/test_pre_semantic_ir_typed_catch.inc
  - tests/parser/test_pre_semantic_ir_multi_catch.inc
  - tests/parser/test_pre_semantic_ir_catch_abrupt.inc
  - tests/parser/test_pre_semantic_ir_throw_cfg.inc
  - tests/parser/test_pre_semantic_ir_return_cfg.inc
  - tests/parser/test_pre_semantic_ir_loop_exit_cfg.inc
  - tests/parser/test_pre_semantic_ir_foreach_cfg.inc
  - tests/parser/test_pre_semantic_ir_branch_exit_cfg.inc
  - tests/parser/test_ssa_effects_verifier.c
  - tests/parser/test_ssa_builder_iterator_invokes.c
  - tests/parser/test_ssa_builder_control_edges.c
  - tests/parser/test_ssa_builder_cleanup_dispatch.c
  - tests/parser/test_ssa_cleanup_exception_state.c
  - tests/parser/test_ssa_source_cleanup_cfg.c
  - tests/parser/test_ssa_oracle_projections.c
  - tests/parser/test_ssa_gvn_range.c
  - tests/parser/test_ssa_pass_manager_scalar.c
  - tests/acceptance/ssa-external-entry-values.md
  - tests/acceptance/ssa-compiler-ownership-execir.md
  - tests/acceptance/ssa-compiler-source-optional-value-cfg.md
  - tests/acceptance/ssa-compiler-source-general-call-cfg.md
  - tests/acceptance/ssa-compiler-source-exception-fallback.md
  - tests/acceptance/ssa-compiler-source-throw-cfg.md
  - tests/acceptance/ssa-compiler-source-return-cfg.md
  - tests/acceptance/ssa-compiler-source-loop-exit-cfg.md
  - tests/acceptance/ssa-compiler-source-for-cfg.md
  - tests/acceptance/ssa-compiler-source-for-continue-cfg.md
  - tests/acceptance/ssa-compiler-source-for-break-cfg.md
  - tests/acceptance/ssa-compiler-source-infinite-for-break-cfg.md
  - tests/acceptance/ssa-compiler-source-infinite-for-cycle-cfg.md
  - tests/acceptance/ssa-compiler-source-branch-exit-cfg.md
  - tests/acceptance/ssa-compiler-source-nested-branch-exit-cfg.md
  - tests/acceptance/ssa-compiler-source-total-branch-exit-cfg.md
  - tests/acceptance/ssa-builder-iterator-invokes.md
  - tests/acceptance/ssa-source-foreach-cfg.md
  - tests/acceptance/ssa-exception-payload.md
  - tests/acceptance/ssa-source-catch-cfg.md
  - tests/acceptance/ssa-builder-control-edge-rejection.md
  - tests/acceptance/ssa-type-test-foundation.md
  - tests/acceptance/ssa-source-typed-catch-cfg.md
  - tests/acceptance/ssa-source-multiple-catch-cfg.md
  - tests/acceptance/ssa-source-cleanup-cfg.md
  - tests/acceptance/ssa-cleanup-exception-state.md
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
control-flow, exception, suspension, iterator protocol, and phi operations.
Iterator initialization, advance, and current-value retrieval are distinct
one-operand/one-result invoke terminators. Each conservatively declares managed
heap and native-FFI reads/writes plus throw/allocation effects; lowering must not
disguise protocol dispatch as a generic callable symbol.

`EXCEPTION_PAYLOAD` is the explicit handler-entry value operation. It has one
result, no operands, no memory/effect token, and is not a terminator. Structural
verification accepts it only in a non-entry, exception-marked block whose every
incoming edge is the direct exceptional successor of a may-throw terminator,
with at most one payload definition per handler block. Although it has no
observable effect, the value is handler-local state rather than a freely
interchangeable zero-operand constant; the current GVN whitelist does not
common it across blocks. The oracle, ExecBC projection, and AOT projection
reject it transactionally until their exception ABI carries the active payload.

`TYPE_TEST` is the canonical, pure one-operand/one-result type-membership fact.
The ordinary `typeToken` remains the result value's type (normally the language
boolean type); the separate `matchTypeToken` names the resolved target type.
The verifier requires a nonzero match token exactly for `TYPE_TEST` and rejects
the field on every other opcode. This prevents a result type, source spelling,
or layout token from being reused as the match identity. The builder copies the
resolved SemanticIR `matchTypeId` directly, and all structural hashes, GVN keys,
clone-compatible instruction storage, and DCE cleanup preserve or clear that
identity consistently. Existing opcode numbers remain stable because the new
opcode is appended to the schema.

`TYPE_TEST` is deliberately not executable in this phase. The oracle, ExecBC
projection, and AOT projection reject it transactionally until runtime subtype
testing is connected to canonical type metadata. A bounded source typed-catch
producer now uses this operation in SemanticIR/ExecIR; it never substitutes a
type-name string comparison or claims an executable projection.

Cleanup identity currently lives on both sides of the CFG boundary: SemanticIR
uses `ZR_PARSER_CFG_EDGE_CLEANUP` and `ZR_PARSER_CFG_BLOCK_CLEANUP`, while
ExecIR preserves cleanup regions with `ZR_EXEC_IR_BLOCK_FLAG_CLEANUP`. The
builder accepts a cleanup edge only when it is the sole successor of an
operand-free `BRANCH` and either endpoint is a cleanup block. This is sufficient
for an entry edge, a chain within cleanup, and a cleanup-to-continuation edge;
the ordinary predecessor/successor pools retain the exact adjacency and the
cleanup block retains its flag. A cleanup edge between two ordinary blocks
remains `UNSUPPORTED`.

A multi-successor cleanup block may use
`ZR_PARSER_CFG_TERMINATOR_CLEANUP_DISPATCH` only when its last SemanticIR
instruction is a `SWITCH` consuming exactly one SSA selector. The dynamic edge
list must contain at least one ordered `SWITCH_CASE` followed by exactly one
terminal `SWITCH_DEFAULT`; the builder preserves that selector and adjacency as
an ExecIR `SWITCH`, then structural and SSA verification prove that the selector
exists and dominates the cleanup dispatch. Missing selectors, inline successor
rows, ordinary source blocks, and unordered cases fail transactionally. This is
the representation for a pending-completion discriminator, not yet a source
producer or a definition of return/throw payload storage.

An INVOKE result remains unavailable along the transitive closure of its
exceptional successor, including ordinary branches into cleanup and later
cleanup dispatch successors. Therefore a pending-state selector defined before
the INVOKE may be read in shared cleanup, while substituting the INVOKE result
is rejected with `EXCEPTION_EDGE` even though the INVOKE's block dominates the
cleanup block. This is the interrupted-assignment gate required before source
exceptional `finally` entry can be published.

The source compiler publishes that representable cleanup subset for a
preflighted `try/finally` with no catches, ownership cleanup, calls,
declarations, or abrupt/nonlinear statements in either body. It closes the
protected block with a cleanup edge, compiles the `finally` body in a cleanup
block, and closes that block with a cleanup edge to one join. Both transfers
are operand-free semantic `BRANCH` instructions. Preflight examines the
complete protected and cleanup bodies before activating a graph; `return`,
`throw`, calls, nested control flow, catch-plus-finally, and other unsupported
shapes retain the legacy-CFG fail-closed path. Source production of pending
completion state and abrupt cleanup dispatch remain later milestones.

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
critical edges. For an already split generic or iterator invoke, a pre-invoke
definition is available on both successors while the invoke result is available
only on the normal successor; the core verifier checks that boundary.

ExecBC and AOT projections currently reject all three iterator invoke opcodes
with `UNSUPPORTED`, transactionally preserving any previously published
projection. This is a deliberate backend boundary until iterator ABI lowering
exists; the canonical opcodes are not silently copied into a runnable artifact.

The source compiler supplies a canonical loop path directly for a
straight-line `while`. Before compiling the condition it closes the current
prefix with an unconditional edge to a dedicated header. The header contains
the condition facts and ordered true/body and false/join edges; a successful
body closes with a normal backedge to that same header. The compiler restores
the pre-loop semantic slot snapshot before entering the join, so body-only
temporaries cannot leak into later source lowering. This graph is independent
of ExecBC label offsets and reaches the existing dominator/frontier promotion
path, which inserts the loop-carried Place phi.

The source-loop subset intentionally accepts only linear conditions and body
statements whose nested control is already modeled. A direct terminal
`break;` emits the body's normal edge to the loop join; a direct terminal
`continue;` emits it to the condition header. Either abrupt edge consumes the
current semantic block, so the loop compiler does not add the fall-through
backedge afterward. Linear statements may precede that final exit. Exits with
trailing reachable statements, exits hidden by an enclosing fallback shape,
return/throw, calls, cleanup, suspension, and short-circuit loop conditions
still trigger the legacy-CFG fallback. If an unsupported loop appears after a
source CFG has started, the compiler abandons that partial graph and removes
its synthetic branch instructions before validation; it also blocks later CFG
startup so subsequent source cannot publish a detached suffix graph.
Loops compiled inside a declared child callable use a disposable isolated
SemanticIR state because child functions do not yet publish independent
pre-execution functions; their loop edges and fallback barriers therefore do
not mutate the entry-body graph.

A supported statement-form `for` extends that canonical loop path with a
dedicated step block. After a falling-through initializer, the prefix jumps
to the condition. Its ordered true/body and false/join edges are followed by
body-to-step and step-to-condition normal edges. Restoring the pre-loop slot
snapshot at the join keeps body and step temporaries off the false path, while
the explicit backedge remains available to dominance and phi placement. A
direct terminal, unvalued `continue`, optionally after a linear statement
prefix, also closes the body at the step. Separate legacy condition and
`continue` labels make the ExecBC transfer execute the step before returning
to the condition. A direct terminal, unvalued `break`, optionally after a
linear prefix, instead closes the body at the join. Since that shape has no
path to the step, its source graph omits the step block and backedge; the
unreachable ExecBC step is compiled without publishing SemanticIR. This
bounded slice normally requires a linear condition and step expressions plus
a falling-through initializer. It additionally accepts a conditionless loop
whose body ends in a direct, unvalued `break`: the header has one normal edge
to the body and the body one normal edge to the join, with no conditional edge
or semantic step/backedge. The legacy break skips the still-emitted,
unreachable backedge. Valued or nonterminal loop exits, nonlinear forms, and
cleanup remain on the legacy-CFG fallback instead of publishing an incomplete
graph.

A bounded source `foreach` path now consumes the canonical iterator invoke
contract. For a static, protocol-resolved iterable with an identifier binding,
`ITER_INIT`, `ITER_MOVE_NEXT`, and `ITER_CURRENT` each terminate their own
SemanticIR block with ordered normal/exception successors. Move-next's normal
continuation branches true to current-value retrieval and false to the join.
Current-value retrieval initializes the binding Place only in its normal body
continuation, so the exceptional path cannot observe that result. Body
fallthrough and direct `continue;` close the cycle at move-next; direct
`break;` reaches the same join as iterator exhaustion. The compiler captures
the pre-iteration slot shape and refreshes its surviving facts after the
one-time iterable evaluation. The join restores that snapshot before later
source is compiled, preserving iterable assignments while truncating the
iterable result, iterator, guard, binding, and body temporary slots. ExecIR
therefore receives three explicit may-throw/may-allocate iterator operations
and an ordinary loop backedge, without recovering either fact from ExecBC
offsets.
Dynamic dispatch, destructuring, unresolved element types, nonlinear iterable
expressions, nonterminal exits, cleanup-sensitive bodies, and body declarations
whose inferred cleanup cannot be resolved during preflight remain fail-closed
on the legacy CFG. Nested branch conditions must also pass the complete
linear/short-circuit preflight. Explicit ownership, close contracts, and
unsupported nested conditions are checked before the iterable or any
canonical iterator operation is emitted. Their
`DYN_ITER_*`/`ITER_*` bytecode behavior is unchanged.

The conditionless subset also models loops that cannot exit: a falling-through
body or direct terminal, unvalued `continue` reaches the step, and the step
returns to the header. The reachable graph has no false edge or join. Its
function-level `exitBlockId` names a zero-predecessor EXIT block, preserving
the current SemanticIR schema without adding a fabricated reachable edge.
After binding that block, the compiler latches semantic termination so source
after the infinite loop remains ExecBC-only. Each unreachable suffix statement
uses disposable SemanticIR isolation: its legacy instructions, locals, and
type bindings are retained, while instructions, Values, Places, loans, operand
rows, slots, and any detached CFG are discarded. Nonterminal exits and
otherwise unsupported bodies still abandon a partial active graph and keep
suffix CFG startup blocked.

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

Source catch selection is intentionally narrow: one or more catch parameters,
each annotated with one simple already-resolvable canonical type, optionally
followed by one terminal catch-all, no `finally`, one resolved direct protected
call with either no
arguments or one unmarked positional `int` identifier that exactly matches one
value parameter without conversion, ownership, reference, or GC-bridge work,
and for every handler either an empty catch body, one expression statement that
reads the catch binding, the exact cleanup-free sequence
`var local = binding; local;`, a
direct `return` whose result is void, a literal, or the catch binding, or an
exact `throw binding;` rethrow.
Direct catch returns are supported only in the compiler-owned entry body;
declared-child catch returns stay on their disposable legacy path.
That local-flow sequence is admitted only when neither identifier conflicts
with an existing variable, runtime/compile-time callable, or type prototype,
so inference cannot resolve the catch read through an outer declaration or
overwrite an outer binding.
The argument's `LOAD` is defined before the dedicated call block, so it
dominates the call and does not appear in the handler instructions' explicit
value-operand arrays.
The call's `INVOKE` exceptional successor enters the first dispatch block and
defines `EXCEPTION_PAYLOAD` once. Annotated catches emit
`TYPE_TEST(payload, matchTypeId)` in source order; a true edge selects that
handler and a false edge continues to the next dispatch. A terminal catch-all
receives the final false edge directly. When every clause is annotated, the
last false edge reaches an unmatched zero-successor `THROW` of the original
payload. Matching handlers initialize their source-local Place with the
resolved catch TypeId, while the catch-all uses the payload TypeId. Only the
first dispatch block carries the exception-block flag.
For the local-flow form, that read feeds the temporary-to-local `CONVERT`, the
new local Place initialization, and its final `LOAD`, all in the handler block.
A falling-through handler and the normal continuation both branch to one join.
A direct return or binding rethrow instead closes only that handler as a zero-
successor abrupt sink; later catch clauses remain independently selectable and
any other falling-through path can reach the join and a later call. The payload
is not fabricated as an entry value, and the invoke result is never made
available on the handler path. The compiler clears the active handler target
before compiling the catch body, so a call introduced by a later phase cannot
recursively target the same handler; after the join, a later call uses its
independent propagation sink. Declared callable bodies use disposable
SemanticIR isolation for this control form and cannot publish their handler
graph into the entry sidecar; direct child catch returns preflight to fallback
because child returns do not publish entry-sidecar terminators. If the
syntactic shape passes preflight but the
protected call later lacks canonical call facts, the compiler abandons the
partial graph and compiles every catch body, including later siblings, inside
disposable SemanticIR isolation; only legacy exception bytecode survives that
fallback.

A catch-all followed by another clause; unresolved, generic, qualified, array,
ownership-qualified, or reference-qualified catch annotations; catch bodies
other than the empty,
single binding read, exact nonshadowing inferred-local propagation, canonical
direct return, or exact binding-rethrow shapes;
protected calls with multiple, named, marked, generic, member, literal,
computed, type-converting, or non-value arguments, protected bodies with other
control or effects, direct handler exits under active ownership/`@close`
cleanup, and all `finally` cleanup shapes remain an explicit conservative
boundary. Such a scope
abandons any partial source CFG and keeps
inactive starters suppressed for the rest of the SemanticIR function. The
legacy compiler remains authoritative for these executable exception paths;
ExecIR does not publish a detached or partially selected handler graph.

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
`finally` other than the bounded catch-binding rethrow, remain on the
conservative legacy path until handler and cleanup edges are modeled together.

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

One direct branch-local `return` or `throw` can now coexist with a
fall-through sibling arm. The producer binds the abrupt arm as a
zero-successor terminator but treats that closure as local to the active
conditional, so the function-level termination latch and exit ID remain
available to the continuing path. Only the fall-through arm connects to the
join; its single predecessor then continues into later blocks and the final
synthetic exit. ExecIR construction preserves that topology without inventing
an edge from the return/throw block to the join. A branch-local source return
therefore appears alongside the final synthetic return, while a branch-local
throw remains a distinct sink.

The producer recursively composes fully preflighted statement-form
conditionals. When an inner conditional has one abrupt arm and one
fall-through arm, its single-predecessor join remains the active continuation
and may feed the enclosing arm's join. The abrupt block is never listed as a
predecessor of either join. When both inner arms terminate, the conditional
has no join and is itself an abrupt arm for its parent. An enclosing sibling
may still provide the sole path to the outer join. When both top-level arms
terminate, the function termination latch prevents unreachable source from
starting a detached graph; the most recently emitted abrupt sink supplies the
CFG's required representative exit ID while every return/throw block remains
zero-successor.

Trailing syntax after a transfer or no-fall-through nested conditional,
non-linear payloads, expression-form nested conditionals, and cleanup/finally
transfers abandon the source graph and block detached restarts.
Statement-form `if` compilation inside a declared child callable is isolated
from the entry-body sidecar until child callables own independent published
functions.

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
The sink still has no fabricated `THROW` operand: the low-level payload
operation exists, but this propagation-only source path neither binds a catch
value nor consumes the active exception. Weak-wake guards, calls that lack
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
