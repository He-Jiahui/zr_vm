---
related_code:
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg_finalize.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_format.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg_finalize.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
  - docs/plans/ssa/01-execir-ssa/03-effects-verifier.md
tests:
  - tests/parser/test_ssa_source_cfg_faults.c
  - tests/parser/ssa_source_cfg_faults.c
  - tests/parser/gdb_ssa_source_straight_line_cfg.gdb
  - tests/parser/test_ssa_source_straight_line_cfg.c
  - tests/parser/test_ssa_source_value_facts.c
  - tests/parser/test_pre_semantic_ir.c
  - tests/acceptance/ssa-source-straight-line-cfg.md
  - tests/acceptance/ssa-source-cfg-promotion-recovery.md
doc_type: module-detail
---

# Source-owned CFG finalization

## Purpose and boundary

The source compiler must publish a real terminal operation even when an entry
body contains no explicit branch. An analysis graph that merely labels the
entry block RETURN and connects it to an empty exit is not executable IR:
the operation and edge contract disagree. ExecIR continues to reject that
shape rather than guessing a return or decoding generated ExecBC.

The finalization boundary in `compiler_semantic_cfg_finalize.c` distinguishes
an executable source CFG from the conservative graph used for flow analysis.
The surrounding validator still resolves canonical value facts and performs
SemanticIR and loan/availability analysis. A successful pre-SemIR analysis
alone is not proof that the function can be built or executed as SSA.

## Common terminal shape

For supported, unblocked straight-line bodies, finalization activates the
existing source CFG producer and uses its normal finish operation. The entry
contains the pre-existing source instructions followed by a real unconditional
BRANCH; its ordinary edge reaches an exit block containing a zero-operand
RETURN. The return has no successors. Existing explicit source returns and
throws retain their own operands and terminal blocks.

The compiler-private current-block sentinel marks a finished graph. Repeated
validation therefore refreshes facts and validates existing storage without
appending another branch or return. The compiler's ExecBC instruction stream
is not the source of these graph facts and is not rewritten by finalization.

## Completeness before activation

An inactive graph is not automatically a complete straight-line program.
Some source operations still have only legacy bytecode emission. The source
capability preflight must reject such operations, including discarded
expressions, before promoting an inactive body. A canonical value that happens
to exist elsewhere cannot stand in for an omitted computation or effect.

Startup-blocked and startup-suppressed compilation retains the analysis-only
fallback. Its RETURN edge remains unsupported by the ExecIR builder. The
fallback does not enable an optimization and is not a successful SSA result.
Declaration and unreachable-code isolation must remain intact; no child
callable body may become an entry-body instruction merely because finalization
now runs for ordinary fallthrough.

The accepted inactive-source forms are literals, initialized identifier locals,
identifier reads with a matching source-local LOAD and prior INITIALIZE/STORE,
plain identifier assignment with a matching source-local STORE, member-free
parenthesized expressions, and ownership wrappers/intrinsics over already
existing identifier places. Empty undecorated nongeneric class declarations
have no entry initializer to skip. Global/closure/function/type reads,
global assignments, named child function declarations, new/resource instance
construction, class members/initializers, uninitialized local defaults,
cross-type numeric conversions, unsupported binary operators, unary expressions,
compound assignment and block/object expressions
retain analysis-only status until their producer contracts are complete.
An isolated child body does not justify skipping the declaration's parent
CREATE_CLOSURE/SET_STACK operations. `own Value()` must not silently skip its
instance seed, close marker or constructor call.
Ordinary standalone statement blocks are not a top-level grammar form; this
stage does not change parsing to invent one.

This capability check is scoped to previously inactive graphs. Existing active
control-flow producers retain their own preflight checks; this stage does not
prove that every source expression supported by the legacy compiler has a
canonical producer. Broader producer completeness is still required before
the normal compiler pipeline can rely exclusively on ExecIR.

Typed numeric `+`, `-`, and `*` are now a supported producer subset. When type
inference selects a signed, unsigned, or floating-point operation and both
operand values already have the same canonical `TypeId` as the result, the
compiler emits a canonical `add`, `sub`, or `mul` SemanticIR instruction with
two value operands and a result value. The existing ExecBC instruction remains
as a compatibility sidecar, while the strict builder maps the canonical
instruction to the corresponding ExecIR arithmetic opcode. Nested expressions
are matched by source range, so source traversal does not infer a producer from
the legacy instruction stream. Division remains analysis-only until its
exception-edge contract is represented; modulo, implicit numeric conversion,
string/dynamic arithmetic, comparison, unary, and compound assignment forms
likewise remain outside this producer subset.

The capability walk uses an explicit, checked worklist rather than recursive
AST descent. Identifier reads follow source order and require corresponding
SemanticIR provenance and a previously initialized Place; assignment writes
require their own source STORE. The initialized-Place bitmap and instruction
cursor make repeated local reads linear in the emitted instruction stream.
Different input/output TypeIds on CONVERT or STORE retain the analysis graph
until executable conversion semantics are implemented. Its
initial and growth allocation failures release scratch storage and return
failure before replacing the graph; allocation failure is not treated as an
unsupported source form. The failure suite enumerates every scratch allocation
for 256 source read statements, both before first graph publication and with
an existing analysis graph, then retries successfully. This covers preflight
scratch allocation, not the pre-existing CFG/IR allocator's behavior.

Straight-line promotion now retains the previous graph until both activation
and finalization succeed. A recoverable failure in either helper frees the
temporary CFG, restores instruction/source-map/operand lengths and compiler
CFG cursors, and leaves the prior analysis graph available for retry. Fault
injection also covers failures immediately after activation and finish, both
before any analysis graph and after one was previously published. This does
not claim recovery from the core array allocator's exception or process-level
out-of-memory behavior; those operations do not return failure to this helper.

## Constant identity

Source CONSTANT instructions already carry a canonical constant-pool index.
The builder projects a present index into the existing ExecIR CONSTANT field
`layoutId`, which the oracle uses to select the caller-provided constant table.
It does not copy runtime pointers into the IR or synthesize constant values
from source text. A real multi-constant assignment/return regression verifies
that the second constant is not replaced by pool entry zero.

## Ownership and execution limits

Canonical ownership/nullability snapshots survive finalization unchanged in
meaning. Source resource-construction tests no longer append a dummy `if`;
they verify metadata and builder rejection until instance construction has a
canonical producer. A supported explicit `drop` still lowers as strict DROP,
not guarded cleanup. CFG completion itself does not generate memory/effect token chains,
infer omitted scope cleanup, implement weak runtime protocols or enable an
ownership-sensitive optimization.

The focused source suite checks the graph itself and executes the supported
oracle subset. Resource-construction facts tests prove metadata and rejection,
not native allocation or release. The complete verification matrix and any
observed limitations are recorded in the acceptance document.

## Design evidence

Lua's `close_func`, QuickJS's `emit_class_init_end` and Rust's MIR body builder
all materialize terminal operations. Rust's terminator fixtures and CPython's
implicit-return source-location tests reinforce the distinction between an
implicit operation and a missing operation. Zr reuses its existing no-value
fallthrough return contract; this change does not adopt another language's
return-value rules.
