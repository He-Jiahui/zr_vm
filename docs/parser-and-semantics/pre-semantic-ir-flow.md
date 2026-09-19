---
related_code:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/bound_expression.h
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_format.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_flow.c
  - zr_vm_parser/src/zr_vm_parser/bound_expression.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_struct_init.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_value_construct.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_values.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_logical.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_receiver_guard.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_flow.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_while.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir_call.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir_optional.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semir.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/bound_expression.h
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_parser/src/zr_vm_parser/semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_format.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_flow.c
  - zr_vm_parser/src/zr_vm_parser/bound_expression.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_struct_init.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_value_construct.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_values.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_logical.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_receiver_guard.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_flow.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_while.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir_call.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir_optional.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
  - docs/plans/syntax/2026-07-18-01-canonical-type-place-cfg-artifact-design.md
  - docs/plans/syntax/2026-07-18-03-struct-ref-struct-span-layout-design.md
tests:
  - tests/parser/test_pre_semantic_ir.c
  - tests/parser/test_pre_semantic_ir_optional_value.inc
  - tests/parser/test_pre_semantic_ir_general_call.inc
  - tests/parser/test_pre_semantic_ir_exception_fallback.inc
  - tests/parser/test_pre_semantic_ir_throw_cfg.inc
  - tests/parser/test_pre_semantic_ir_return_cfg.inc
  - tests/parser/test_pre_semantic_ir_loop_exit_cfg.inc
  - tests/parser/test_pre_semantic_ir_branch_exit_cfg.inc
  - tests/parser/test_struct_value_init.c
  - tests/acceptance/2026-07-19-syntax-01-m3-pre-semantic-ir.md
  - tests/acceptance/ssa-compiler-ownership-execir.md
  - tests/acceptance/ssa-compiler-load-store-provenance.md
  - tests/acceptance/ssa-compiler-literal-provenance.md
  - tests/acceptance/ssa-compiler-literal-type-provenance.md
  - tests/acceptance/ssa-compiler-source-short-circuit-cfg.md
  - tests/acceptance/ssa-compiler-source-optional-call-cfg.md
  - tests/acceptance/ssa-compiler-source-optional-value-cfg.md
  - tests/acceptance/ssa-compiler-source-general-call-cfg.md
  - tests/acceptance/ssa-compiler-source-exception-fallback.md
  - tests/acceptance/ssa-compiler-source-throw-cfg.md
  - tests/acceptance/ssa-compiler-source-return-cfg.md
  - tests/acceptance/ssa-compiler-source-loop-exit-cfg.md
  - tests/acceptance/ssa-compiler-source-branch-exit-cfg.md
doc_type: module-detail
---

# Pre-Execution Semantic IR And Flow Facts

## Purpose

M3 introduces a semantic function that exists before final ExecBC assembly. It gives reads, writes, initialization, moves, copies, drops, borrows, calls, construction, cleanup, properties, and destructuring explicit identities instead of recovering their meaning from execution bytecode.

`SZrSemanticIrFunction` owns its canonical symbol/callable IDs, Place graph, CFG blocks, locals, values, instructions, flat operand storage, regions, cleanup scopes, source map, and loan facts. Instructions use `TypeId`, `PlaceId`, `ValueId`, `LoanId`, and block IDs. VM stack slots remain private to the compiler bridge and are not part of the public semantic instruction contract.

## Instruction Contract

The public opcode set covers constants and conversions; Place construction and projection; load/store/initialize/move/copy/drop; borrow/reborrow/end-loan/dereference; typed, virtual, dynamic, and meta calls; control flow; scope and cleanup; distinct value, aggregate, field, union, GC, and ownership construction; resolved property operations; and evaluate-once destructuring operations.

Value construction, ordinary/meta calls, GC allocation, and ownership construction have different opcodes. Ownership construction additionally records explicit unique/share/degrade/wake operations; move, drop, shared borrow, and mutable borrow remain their own opcodes. No generic construct flag or default fallback opcode is used to reinterpret one family as another. Golden formatting is stable and includes instruction ID, opcode name, TypeId, PlaceId, input ValueId, and result ValueId.

`ZrParser_SemanticIr_Validate` rejects dangling Place/Value/Loan/Region/Cleanup references, malformed operand spans, non-sequential instruction/source-map identities, and invalid owned CFG ranges or edges. Empty CFG storage is valid during straight-line compiler emission; once blocks exist, entry/exit IDs, instruction ranges, terminators, and typed edges are checked.

The experimental SemIR-to-ExecIR builder consumes a `BRANCH` with one value
operand as `CONDITIONAL_BRANCH` only when its two explicit outgoing CFG edges
are ordered `TRUE_BRANCH`, then `FALSE_BRANCH`. The ExecIR successor ordinals
and oracle use that same true/false order. Reversed or untyped inline edges
fail with a source-located unsupported-edge diagnostic; a zero-operand branch
remains an unconditional branch with exactly one successor. This is a builder
boundary. The compiler producer now emits a deliberately bounded source CFG
surface for `if`, straight-line `while`, direct terminal loop exits,
linear-operand `&&`/`||`, and known
nullable optional calls with either `void`/no-op or nullable value results.
Non-fallthrough returns nested in an unsupported control arm, general loop
control, Weak optional access, cleanup, suspension, and other unmodeled
control still use the conservative legacy graph rather than publishing
partial facts.

## Compiler Bridge

Every compiler state owns an independent pre-execution semantic function and a private stack-slot bridge. A declared local is registered with the canonical `TypeId` and `SymbolId` already assigned by the semantic context. Parameters, foreach bindings, and compiler-generated locals are materialized on first semantic use so existing compilation paths do not lose Place identity.

For the current lowering surface, local initialization, identifier load, local store, and ownership operations emit semantic instructions first. The bridge then selects `GET_STACK`, `SET_STACK`, or the exact `OWN_*` ExecBC opcode from that emitted semantic instruction; AST callers no longer make a second load/store/move/borrow decision. A readonly view is declared as `var view: ref readonly T = ref owner`, a mutable view as `var view: ref T = ref owner`, GC return as `intoGc(owner)`, and deterministic release as `drop(owner)`. The other ownership transitions are `share(owner)`, `degrade(shared)`, and `wake(weak)`. Percent directives and removed ownership member-call forms stop before this bridge and only produce migration errors. Internal shared/mutable loan facts and region opcodes remain semantic implementation details, not source spellings. Unsupported ownership kinds fail instead of falling through to construction. Script compilation validates the complete pre-execution function before final function assembly, optimization sidecars, and quickening.

Ownership lowering registers both its inferred source type and its derived
result type in the canonical semantic type graph. The result qualifier is
transformed explicitly for unique, shared, weak, wake, borrowed, loaned, and
GC-box results instead of copying an unqualified construction token. A fresh
resource construction may publish a new typed slot binding when a nested
callable compilation left a stale or moved binding on the reused runtime stack
slot; ordinary ownership operations remain strict and cannot resurrect such a
binding. Receiver aliases transfer the canonical source ValueId before their
borrow fact is emitted.

The SemIR-to-ExecIR builder consumes these facts without reconstructing them
from bytecode: consuming ownership operations become `MOVE`, non-consuming
ownership and view operations become `COPY`, release remains `DROP`, and loan
lifecycle markers become source-mapped `NOP`. Values with no instruction
definition are explicit external-entry values; instruction result references
remain the compatibility authority for older synthetic fixtures whose cached
definition field is zero.

Each identifier `LOAD` defines a ValueId and materializes its result stack slot
as a distinct temporary Place initialized with that *same* value. A later
local `STORE` resolves the right-hand stack slot's existing ValueId and
records it as the source rather than allocating an unrelated, undefined
value. This preserves assignment provenance for simple identifier RHS
expressions and keeps temporary Places available to reference/contiguous
view consumers. Arithmetic and other computed producer families still
need their own explicit definitions; neither the temporary Place nor a
post-ExecBC decode manufactures those missing facts.

Source literal expressions now emit `CONSTANT(resultValueId)` with an explicit
`hasConstantPoolIndex`/`constantPoolIndex` reference to the compiler's existing
constant pool before selecting the same `GET_CONSTANT` ExecBC operation.
The literal's runtime value category is registered through the existing
canonical inferred-type graph; its CONSTANT result, source ValueId, and
temporary Place share that source TypeId. Contextual destination typing remains
the local-binding CONVERT's responsibility, so a literal is not stamped with
its destination's declared type without conversion. Registration uses the
type graph's normal kind inference so string literals retain their reference
category while integer literals remain values.
Their expression stack slots are temporary Places initialized from that
defined value. If expression normalization copies a known produced value to
an as-yet-unmaterialized stack slot, the destination temporary retains that
ValueId; binding
an initialized local then emits its existing typed `CONVERT` definition from
the temporary. Synthetic SemIR functions may still use `CONSTANT` without a
compiler-owned pool, and a pool index alone does not make a standalone SemIR
function self-contained. Other computed expressions, conversions outside the
local-binding path, and full CFG/phi construction remain open.
Overwriting an already materialized destination must use an explicit semantic
`STORE`; expression normalization does not silently retag an existing Place.

Logical short-circuit lowering does not treat `SET_STACK` as a semantic merge.
The left operand initializes a compiler-private temporary Place before the
conditional `BRANCH`. Only the evaluate-RHS block emits the RHS facts and a
`STORE` to that Place. The join restores the pre-branch slot snapshot and
emits a `LOAD` into one fresh result slot, so later consumers see a single
defined ValueId while the skipped path never owns the RHS side effects. `&&`
uses true-to-RHS/false-to-join edges; `||` uses true-to-join/false-to-RHS.
This temporary is not a scalar source local and is intentionally not eligible
for local Place promotion.

A source-owned `while` may now end its direct body with `break;` or
`continue;`. The active loop label carries semantic block targets alongside
the existing ExecBC label IDs. `break` closes the current body block with one
normal edge to the loop join; `continue` closes it with one normal edge to the
condition header. Because either statement invalidates the current semantic
block, the loop compiler does not append the ordinary fall-through backedge.
The subset also accepts already-modeled linear statements before the terminal
exit. An exit followed by reachable syntax, nested under a CFG shape that has
already fallen back, or crossing `finally` or ownership cleanup remains
legacy-only. These cases abandon any partial source graph and keep later CFG
starters blocked, so a trailing call cannot publish a detached graph that
omits the loop transfer. Declared child callables still do not publish their
own semantic functions; their `while`, `for`, and `foreach` statements compile
against a disposable isolated SemanticIR state so either a supported loop CFG
or a fallback barrier cannot mutate the entry body's graph.

A supported source `if` may now end exactly one direct arm with a linear-value
`return` or `throw` while the other arm falls through. The abrupt arm emits its
ordinary zero-successor terminator and invalidates only that arm's current
block; it does not trip the whole-function termination latch or become the
function CFG exit. The compiler therefore omits the abrupt arm's synthetic
jump to the join, restores the pre-branch value snapshot for the other arm,
and gives the join exactly one predecessor. Source after the conditional stays
in the same graph, including a later resolved call and its normal/exception
edges. A naturally falling-through function still receives its separate
synthetic zero-operand return in the final exit block.

This conditional slice preflights the complete arm before publishing blocks.
Both arms terminating, a nested abrupt transfer, a non-linear return/throw
payload, reachable syntax after the transfer, or cleanup/finally context keeps
the entire conditional on conservative legacy lowering and persistently
blocks later CFG startup. Declared child callables still lack separately
published semantic functions, so their statement-form `if` nodes compile in a
disposable isolated SemanticIR state just like their loops; a child branch
cannot add a barrier, instruction, value, Place, slot, or block to the entry
body's sidecar.

Resolved, non-spread function calls now own a source control boundary even when
they are the first non-linear operation in an otherwise straight-line caller.
The compiler promotes the preceding facts to an entry block, isolates the
typed call in an invoke block, and records its callable, explicit arguments,
canonical symbol, result type, and result ValueId. Its ordered normal and
exception edges lower to one ExecIR `INVOKE`; the result is available only on
the normal continuation. Calls without a resolved canonical symbol or operand
set, and spread calls whose fixed operand range is not represented, remain
conservative fallback cases instead of being reconstructed from ExecBC. A call
nested under an `if`, loop, or short-circuit expression whose CFG preflight
already failed also remains on that enclosing legacy path; it cannot restart
an inactive graph and falsely model conditional execution as unconditional.

Unmodeled `try`/`catch`/`finally` scopes form the same conservative boundary.
They abandon an earlier partial source CFG and suppress nested `if`, loop,
short-circuit, optional-guard, and call-driven startup until all protected,
handler, and finally blocks have compiled. Startup remains suppressed for the
rest of that SemanticIR function as well, so a later source construct cannot
treat the earlier exception scope as a linear prefix. This keeps the validated
two-block legacy graph even when an enclosing fallback scope restores its own
temporary suppression state. Canonical exception payload, handler, and cleanup
edges must become available together before this function-level block can be
removed.

An explicit `throw` outside that boundary now consumes its source expression's
canonical ValueId and terminates the source-owned graph directly. The compiler
emits one-operand `THROW`, marks the current block with the matching zero-
successor terminator, and makes that block the CFG exit. This works both when
the throw establishes the first source CFG block and when it closes the normal
continuation of an existing call/invoke graph. Once closed, the function keeps
its validated graph but suppresses subsequent SemanticIR instruction and CFG
startup; unreachable source still follows the existing ExecBC compilation
path. Nested throws in an unsupported branch/loop shape and handled throws in
`try`/`catch`/`finally` stay on legacy lowering until the full handler-payload
and cleanup-edge contract exists.

An explicit return from the compiler-owned entry body now closes the graph by
the same value-terminator path. A value return consumes the expression's
canonical ValueId; `return;` first materializes the language's null result as
a typed `CONSTANT`. Both forms emit a one-operand SemanticIR `RETURN`, bind a
zero-successor return terminator, and make that block the CFG exit. The return
can establish a single entry/exit block or close the normal continuation of an
existing `INVOKE`. Later unreachable source remains on ExecBC compatibility
lowering without adding SemanticIR or restarting a graph. A return whose value
has no canonical producer, or which appears under scoped fallback, promotes
the conservative startup barrier instead. Returns that cross finally or
ownership cleanup also remain legacy-only. Declared child callables do not yet
own independent published pre-execution functions, so their returns cannot
terminate the entry body's graph. Their return expressions use a disposable
isolated SemanticIR state and retain their existing ExecBC output without
appending instructions, Values, Places, loans, slots, or CFG state to the
entry-body function.

Nullable `receiver?.method(arguments)` chains publish ordered present-true and
absent-false edges from the receiver ValueId. Argument and suffix facts are
emitted only on the present path. A supported known member call receives a
dedicated invoke block and records a typed `CALL_*` with the receiver/callee
ValueId, explicit argument ValueIds, resolved symbol, and typed result. The
runtime-only hidden receiver count is excluded from the semantic explicit-
argument range. Ordered normal and exception edges lower the call to `INVOKE`
without assigning an earlier present-path operation the call's exceptional
transfer.

`VOID_NOOP` absent edges reach the join directly. A `NULLABLE` result instead
uses a typed temporary Place: the normal block converts and stores the call
result, the dedicated absent block stores a typed null constant, and the join
restores the pre-branch slot snapshot and loads one merged ValueId. The
exception continuation is an explicit zero-instruction propagation sink and
does not reach that merge. The current model has no edge-defined exception
payload value, so the producer does not invent a normal-entry value for
`THROW`. Ownership results remain bound to their result slots so an explicitly
awakened nullable receiver can be the branch operand. Weak-wake guard frames
and calls missing a canonical result type, symbol, callable, or explicit
argument value remain outside this subset; after a source graph has started,
they abandon the partial CFG and remove its synthetic branches.

Struct value construction follows the same semantic-first rule. The contextual `init TypeRef(...)` syntax produces a dedicated AST node, and `SZrBoundValueConstruct` resolves the canonical constructor plus named/default argument mapping. Lowering emits `VALUE_CONSTRUCT(destinationPlaceId, typeId, constructorId, arguments)` before ExecBC selection. Local, field, fixed-array element, and return construction all pass the final destination Place into this path; ordinary call, GC allocation, and ownership construction remain separate and do not serve as fallback routes.

The private stack-slot bridge is backed by a growable array. Any operation that
materializes another slot can relocate that storage, so lowering code carries
slot contents as value snapshots across materialization and re-resolves a slot
by `stackSlot` immediately before a writeback. This applies to value construction,
contiguous-view/bounds facts, receiver projections, and property-ref load/store;
no semantic instruction may retain an interior slot pointer across a possible
array growth. Local store snapshots both source and destination before
updating the destination bridge slot, because materializing the RHS can grow
the bridge array.
The existing compiler Semantic IR producer is already a large, shared bridge;
this change stays inside its slot-transfer responsibility. Once literal and
computed-value producers are added, the temporary-slot/materialization and
load/store transfer group is the smallest coherent boundary to extract into
a focused compiler-internal module without exposing the bridge to core IR.

The old `SZrSemIrInstruction` table is an execution compatibility projection. `compiler_semir.c` builds it only after final ExecBC assembly. It is not consulted when the compiler chooses local or ownership semantics, and it may legitimately be empty for source programs whose front-end Semantic IR is non-empty.

## Flow Facts

`ZrParser_SemanticFlow_Analyze` runs fixed-point analyses over the function CFG. Each reachable block owns separate entry and exit state for every Place:

- initialization: uninitialized, initialized, or maybe initialized;
- availability: available, moved, maybe moved, or dropped;
- borrowing: a conservative shared-loan set plus an optional mutable loan;
- escape: local, function, caller, heap/static, or unknown;
- reachability: recorded independently on the block facts.

Unreachable predecessors do not participate in joins. Initialization and availability join conservatively, and escape takes the widest bound. A store restores an assignable moved Place to initialized/available. Shared/mutable borrowing is then replaced by the backward CFG liveness result: ref values propagate through values and Place Store/Load, Store performs kill/gen for overwritten ref slots, reborrow keeps its parent live, and each block receives the exact live loan set at its entry and exit. Place access conflicts use projection overlap and end after the final possible use instead of the lexical block. The diagnostic pass reports uninitialized or maybe-uninitialized use, use after move/drop, maybe-moved use, NLL loan conflicts, and escape violations with instruction, Place, loan, block, overlap, origin, declaration, last-use, and source-range identities. See `reference-loan-nll.md` for the loan algorithm and M3 boundary.

`FIELD_INITIALIZE` requires a field-projected destination. Flow analysis marks the field initialized and records the matching parent-field bit without claiming that sibling fields are initialized. The resulting bitmap is the semantic source for partial-constructor cleanup; joins preserve the independent field facts rather than collapsing the whole aggregate to initialized.

## Boundaries

This graph remains compilation-session data. Canonical public contracts and hashes enter `.zrs`, `.zri`, and `.zro`, while local Place, block, loan, field-initialization bitmap, and flow state do not become `.zro` ABI by default. VM, AOT, LSP, reflection, and debug consume canonical projections where their contract requires them. Execution-side optimization and quickening continue to consume assembled execution structures rather than redefining front-end semantics. Owner/resource field teardown and the complete cross-function exception protocol remain Syntax 04 promotion gates; struct M1 only publishes the generic layout maps and the partial-initialization mechanism they will consume.

## Verification

`test_pre_semantic_ir.c` fixes the complete opcode-family golden, destination-bearing `VALUE_CONSTRUCT`, field-projected `FIELD_INITIALIZE`, parent cleanup bitmap behavior, source-level local initialize/load/store provenance, explicit ownership-operation and shared-loan lowering, source `if`/`while`/`&&`/`||` CFGs, structural validation before execution-sidecar construction, CFG join negatives for definite assignment, move availability, loan conflicts, and caller escape, plus store-after-move and NLL replacement of compatibility borrow states. `test_struct_value_init.c` covers contextual parsing, qualified/generic TypeRef targets, named/default binding, constructor isolation, destination-first local/field/array lowering, runtime constructor aliases, and partial unwind. `test_reference_loan_nll.c` covers last-use release, shared/mutable conflicts, ref-slot overwrite, branch/loop liveness, dynamic-index unknown overlap, nested reborrow, and move/drop rejection. The compiler integration and ownership suites protect existing ExecBC behavior while the new semantic source is introduced.

The ownership compiler fixture sends its top-level resource class through the
class-declaration entry and borrows twice from a shared owner. A direct
statement-compile call is only valid for the following executable statements;
borrowing the unique owner with `ref` instead produces a mutable loan, not the
shared-loan fact that this fixture asserts.
