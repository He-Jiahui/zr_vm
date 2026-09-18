---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_parser/include/zr_vm_parser/exec_ir_builder.h
  - zr_vm_core/include/zr_vm_core/exec_ir.h
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
  - docs/plans/ssa/guides/A-execir-builder-verifier.md
tests:
  - tests/parser/test_ssa_builder_cfg.c
  - tests/parser/test_ssa_builder_dominance.c
  - tests/parser/test_ssa_builder_control_edges.c
  - tests/cmake/ssa-tests.cmake
  - tests/acceptance/ssa-builder-cfg.md
  - tests/acceptance/ssa-builder-instruction-lowering.md
  - tests/acceptance/ssa-builder-module-transaction.md
  - tests/acceptance/ssa-builder-canonical-input-shape.md
  - tests/acceptance/ssa-builder-edge-capacity.md
  - tests/acceptance/ssa-builder-operand-bounds.md
  - tests/acceptance/ssa-builder-terminator-placement.md
  - tests/acceptance/ssa-builder-successor-arity.md
  - tests/acceptance/ssa-builder-instruction-partition.md
  - tests/acceptance/ssa-builder-edge-source.md
  - tests/acceptance/ssa-builder-cfg-fact-identity.md
  - tests/acceptance/ssa-builder-ssa-dominance.md
  - tests/acceptance/ssa-builder-opcode-arity.md
  - tests/acceptance/ssa-builder-control-edge-rejection.md
doc_type: module-detail
---

# ExecIR builder CFG adjacency

## Responsibility and data flow

`ZrParser_ExecIr_Build` copies semantic values, blocks, edges, instructions and
source positions into a temporary ExecIR function, then computes dominators and
constructs SSA. It replaces the caller's function only after every stage
succeeds. CFG block indices in SemanticIR are zero-based; emitted ExecIR block
IDs are one-based. The producer uses `outgoingEdges` when that array is valid,
otherwise the block's bounded inline `successors` array.

For each source block, outgoing IDs occupy a contiguous segment of the
function's successor side pool. The builder records the segment start before
appending its edges and increments its count for each successful append. It
does not overwrite the range with the last edge. Incoming IDs cannot be
appended while visiting sources in arbitrary order: a first pass counts edges
per destination, a prefix sum reserves one contiguous predecessor row per
destination, and a second pass writes source IDs in source/edge order. A single
side-pool append publishes those rows before dominator analysis. Duplicate
source/destination edges remain distinct entries in both adjacency arrays;
edge identity for phi construction is a separate outstanding milestone.

For dynamic outgoing rows, each edge also names its zero-based source block.
The builder checks that this `fromBlockId` matches the enclosing semantic
block before adding predecessors; otherwise a malformed edge could be
silently reattributed to another source. A mismatch reports `INVALID_BLOCK`
with the enclosing one-based block and expected/actual one-based source IDs.
The check does not apply to the legacy inline successor IDs, which contain
only destinations. See `tests/acceptance/ssa-builder-edge-source.md` for
the wrong-source and valid non-entry self-edge fixtures.

Canonical CFG blocks also carry their own zero-based `id`; it must equal
their index in the block array before the builder emits a one-based ExecIR
block. Dynamic edge kinds must be within the declared parser CFG enum.
Invalid block identity reports `INVALID_BLOCK` with expected and actual
one-based IDs; an unknown edge kind reports `INVALID_RANGE` with the highest
known enum value and the encountered value. These input checks prevent
silently accepting malformed CFG facts. They do not lower edge kinds into
ExecIR exceptional/resume semantics; see
`tests/acceptance/ssa-builder-cfg-fact-identity.md`.

Dynamic edges representing exception, cleanup, return, suspend or resume
control cannot yet be represented by this lowering. The builder rejects
them with `UNSUPPORTED` and the source block and final semantic instruction
site (when present), rather than silently publishing them as ordinary
successors. Normal, true/false and switch edges retain their order. This
is a temporary fail-closed boundary, not implementation of those control
paths; see `tests/acceptance/ssa-builder-control-edge-rejection.md`.
For legacy inline successor rows without edge kinds, a nonempty row paired
with `RETURN`, `THROW`, `SUSPEND`, `CLEANUP_DISPATCH`, or `EXIT` terminator
metadata is rejected the same way; a typed control transfer must not evade
the dynamic-edge guard by using the inline compatibility representation.

The instruction pool uses zero-based range offsets while published
`terminatorInstructionId` uses one-based instruction IDs. For each semantic
block, the builder records the current ExecIR instruction count before
appending that block's instructions; it does not mistake a semantic input
index for an output-pool offset. Lowered terminators carry the block's
successor range, so a `BRANCH` owns the same edge occurrence as the block
adjacency. A constant/branch/return fixture checks both block ranges and
terminator IDs, then passes the emitted function through core structural
verification. This does not construct phis or prove all source-level
exception/short-circuit semantics.

Before lowering, a scratch coverage map checks that every semantic
instruction index is owned by exactly one CFG block. Overlap reports the
second block and duplicated instruction; a gap reports the first unowned
source instruction. The map is freed on every success/error path. Blocks
are free to reference disjoint instruction slices in another source-array
order: the builder still emits in CFG block order and retains source IDs.
See `tests/acceptance/ssa-builder-instruction-partition.md` for the
overlap/gap/reordering fixtures.

For every nonempty semantic block, the last instruction must carry the
canonical terminator opcode flag and no earlier instruction may terminate.
A nonterminal tail reports `MISSING_TERMINATOR`; an early terminator reports
`INVALID_RANGE` at its source instruction. Input operand bounds are checked
first when both conditions are malformed, preserving the more specific
side-pool diagnostic. Empty CFG blocks remain valid during this construction
stage. See `tests/acceptance/ssa-builder-terminator-placement.md` for the
focused negative and positive boundaries.

For terminators with fixed normal-edge meaning, the builder checks successor
arity before publishing their ranges: `BRANCH` needs exactly one target,
`SWITCH` needs at least one, and `RETURN` needs none. A mismatched count
reports `INVALID_RANGE` with the source block/instruction and the expected
boundary versus actual count. `THROW` and `SUSPEND` are intentionally not
constrained by these normal-edge rules until exceptional/resume CFG lowering
has its own contract; typed exceptional/resume edges are rejected in the
meantime. The positive switch fixture retains its successor in
structurally verifiable ExecIR. See
`tests/acceptance/ssa-builder-successor-arity.md`.

## Validation, ownership and failure

The builder checks CFG block array shape, outgoing edge storage and declared
edge capacity, inline edge capacity and all destinations before publishing an
edge. A nonempty dynamic edge row cannot advertise more entries than its
backing array's capacity, even when its pointer and element width are valid.
Bad storage reports
`INVALID_RANGE` with the source block; a missing destination reports
`INVALID_BLOCK` with source block, valid block count and offending one-based
destination. A 32-bit predecessor count or allocation-size overflow reports
`CAPACITY_OVERFLOW`; allocation failure reports `OUT_OF_MEMORY`. Scratch
counts, cursors and rows are freed on every path. The outer builder frees a
failed temporary function and leaves existing caller output unchanged.

Once predecessors, dominators and the conservative parser SSA pass succeed,
the builder also asks the existing core verifier for `VERIFY_SSA` before
publishing its temporary function. Core verification requires a nonzero
module function ID; the candidate uses an ID only during this check and
restores its unassigned ID afterward, including on failure. This catches
uses before a definition in the same block and definitions that do not
dominate every incoming path to a join. A failed dominance check carries
the source block/instruction/value site and leaves caller output untouched.
The independent `ssa_builder_dominance` test owns these cases; it reuses
the same production builder/core sources as `ssa_builder_cfg` without
growing that CFG fixture past its single-purpose boundary. See
`tests/acceptance/ssa-builder-ssa-dominance.md`. Verification does not
insert phi nodes or rename local variables.

Before reading canonical facts, the builder checks the block, instruction,
value, and value-operand arrays for logical length within capacity and the
32-bit ExecIR bound. Nonempty arrays must be valid, backed by storage, and
have their declared element width. Bad shape reports `INVALID_RANGE` without
touching caller output; it does not attempt to infer facts by reading raw
memory or fall back to post-ExecBC decoding. See
`tests/acceptance/ssa-builder-canonical-input-shape.md` for fault fixtures.

Each instruction's operand range is checked against the logical operand
side-pool length before copying. The check tests `start <= length` and then
`count <= length - start`, so wraparound cannot make a missing range look
valid. An invalid range reports `INVALID_RANGE` with the originating block
and semantic instruction ID; it is never silently converted to a zero-operand
variadic call. A valid constant/call/return fixture retains both the call
operand and return value and passes structural ExecIR verification. See
`tests/acceptance/ssa-builder-operand-bounds.md` for focused evidence.

After the side-pool check, the builder compares fixed operand and result
counts with the mapped ExecIR opcode schema. A missing `CONSTANT` result or
missing `LOAD` operand is reported at the semantic block and instruction,
with expected/actual counts and source ID, before the generic verifier
would lose the source-block location. Variadic operand arities are left to
their schema bounds; a missing physical operand range still takes priority
over an arity mismatch. This does not fabricate absent canonical facts from
legacy ExecBC. See `tests/acceptance/ssa-builder-opcode-arity.md`.

The module-level entry builds an isolated candidate before reserving the next
published function slot. If canonical-fact lowering fails, the module's
function count and existing entries are unchanged; successful publication
retains the module-assigned one-based function ID, caller-provided token and
signature, and its initialized execution contract (including generation).
An invalid zero token is diagnosed before construction, and a failed slot
reservation frees the prepared candidate. Failed module builds report the
caller-supplied function token while preserving the lower-level block and
instruction diagnostic. See
`tests/acceptance/ssa-builder-module-transaction.md` for the independent
failure/success/retry fixture. This is a builder transaction boundary, not
pruned SSA promotion or full source-language parity.

This preflight covers adjacency bounds, not semantic correctness of the
terminator opcode, exceptional-result availability, pruned phi insertion or
source-program behavioral parity. The dominator analysis has its own
validation and is described in `execir-cfg-dominators.md`.
The nested edge-capacity regression is recorded in
`tests/acceptance/ssa-builder-edge-capacity.md`.

## Regression boundary

The independent `ssa_builder_cfg` target uses real builder, dominator, SSA and
core ExecIR implementations. It checks both arms of a diamond and both merge
predecessors, a two-edge inline successor array, invalid target diagnostics and
unchanged caller output, and malformed edge storage. See
`tests/acceptance/ssa-builder-cfg.md` for the commands actually executed and
the remaining 01.02 acceptance gaps.
The separate instruction-range and branch-successor regression is recorded
in `tests/acceptance/ssa-builder-instruction-lowering.md`.
