---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_constant_materialize.c
  - tests/parser/test_ssa_sccp_pooled_int64.c
  - tests/parser/support/ssa_sccp_pooled_int64_fixture.h
  - tests/parser/support/ssa_sccp_pooled_int64_fixture.c
  - tests/parser/support/ssa_sccp_pooled_int64_faults.c
  - tests/cmake/ssa-sccp-pooled-int64.cmake
  - tests/parser/test_ssa_pass_manager_scalar_metadata.c
  - zr_vm_parser/include/zr_vm_parser/exec_ir_pass_manager.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_manager.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_dce.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/execbc/exec_ir_execbc.c
  - tests/parser/test_ssa_sccp_conversion.c
  - tests/parser/test_ssa_dce_phi_liveness.c
  - tests/parser/ssa_dce_phi_liveness_cases.inc
  - tests/cmake/ssa-sccp-conversion-tests.cmake
  - tests/CMakeLists.txt
  - tests/cmake/ssa-tests.cmake
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_gc_map_internal.h
  - tests/parser/test_ssa_gc_map_parser_consumers.c
  - tests/parser/support/ssa_gc_map_parser_consumers_fixture.h
  - tests/cmake/ssa-gc-map-parser-consumers.cmake
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_constant_materialize.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_manager.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_dce.c
plan_sources:
  - .codex/plans/20261017-ssa-sccp-pooled-int64.md
  - .codex/plans/20261016-ssa-gc-map-parser-consumers.md
  - docs/plans/ssa/01-execir-ssa/03-effects-verifier.md
  - docs/plans/ssa/02-automatic-optimization/01-pass-manager-scalar.md
tests:
  - tests/parser/test_ssa_sccp_pooled_int64.c
  - tests/parser/support/ssa_sccp_pooled_int64_fixture.h
  - tests/parser/support/ssa_sccp_pooled_int64_fixture.c
  - tests/parser/support/ssa_sccp_pooled_int64_faults.c
  - tests/cmake/ssa-sccp-pooled-int64.cmake
  - tests/parser/test_ssa_pass_manager_scalar_metadata.c
  - tests/parser/test_ssa_gc_map_parser_consumers.c
  - tests/parser/support/ssa_gc_map_parser_consumers_fixture.h
  - tests/cmake/ssa-gc-map-parser-consumers.cmake
  - tests/parser/test_ssa_pass_manager_scalar.c
  - tests/parser/test_ssa_sccp_conversion.c
  - tests/parser/test_ssa_deopt_aggregates.c
  - tests/acceptance/ssa-pass-manager-token-phi-hash.md
  - tests/acceptance/ssa-pass-failure-snapshot.md
  - tests/acceptance/ssa-pass-verifier-timing.md
  - tests/acceptance/2026-10-02-ssa-sccp-conversion.md
  - tests/parser/test_ssa_dce_phi_liveness.c
  - tests/parser/ssa_dce_phi_liveness_cases.inc
  - tests/acceptance/2026-10-02-ssa-dce-phi-liveness.md
doc_type: implementation
status: active
---

# ExecIR scalar pass pipeline

The parser owns optimization policy; the core owns the ExecIR data model and
verifier.  The initial registry is deliberately small and deterministic:

1. `sccp` computes executable CFG edges and the value lattice, then performs
   dominance-safe copy propagation and folds only proven, representable
   integer results.
2. `dce` completes a bounded fixed-point liveness walk before turning unused,
   side-effect-free definitions into `NOP` tombstones.

## Direct DCE/SCCP GC-map admission

Direct `ZrParser_ExecIr_RunDcePass`, `ZrParser_ExecIr_ComputeSccp`, and
`ZrParser_ExecIr_RunSccpPass` share the Core private outer-shape predicate through
their existing storage checks. An absent map is exactly NULL/0/0; a present map
is one non-NULL header with count/capacity 1/1. Multiple entries, slot indices,
and inline-reference offsets belong to the header's independent inner pools.
The predicate checks the envelope only; callers still supply readable, live,
properly owned storage, and existing inner checks retain their responsibilities.

Each direct entry initializes a supplied `changed` to false before storage
validation. A malformed envelope follows the existing INVALID_ARGUMENT path
before any GC-header dereference or pass allocation. The diagnostic is reset
and carries the function token; the other seven fields are zero. Rejection
preserves the function and supplied analysis cache/context. `RunSccpPass`
continues to delegate to `ComputeSccp` with rewrite enabled. Healthy DCE/SCCP
calls may allocate; zero allocation is required for malformed outer rejection.
The manager's verification, budget, rollback, and hash protocols are unchanged.

The independent [Parser GC-map validation record](../testing-and-validation/ssa-gc-map-parser-consumers.md)
records the actual RED matrix, accepted 99-case GREEN results, and finite
MSVC evidence for this direct metadata path. Historical pass execution fixtures below have separate scope.

## DCE PHI Liveness And Publication

DCE first validates storage and seeds uses from GC roots, deoptimization
values and aggregate fields, and state-map live/root values. Owner states are
paired with state-map live values, so those definitions remain rooted. It then
repeatedly visits PHI incoming edge uses and instruction operands until marking
does not discover any new used value. Control-flow instructions, owner operations,
memory reads/writes, explicit memory/effect-token carriers, throw/GC/suspend/
allocation/debug/guard boundaries, and existing GC/state-map sites retain their
operands even when their result is unused.

PHIs remain structurally present in this pass. Every retained PHI incoming is
therefore a structural use, including when the PHI result is unused: the full
SSA verifier still requires its incoming definitions. This also keeps transitive
instruction operands behind incoming copies and chained PHIs alive. Removing
unused PHIs and their CFG bookkeeping is a separate optimization, and this DCE
pass makes no claim to remove definitions needed by retained unused PHIs.

The marking phase never rewrites instructions or metadata. PHI and instruction
visits consume the pass work budget. After the fixed point, DCE reserves the
entire instruction-deletion sweep budget before changing the function. Budget
exhaustion returns success with `budgetExhausted` set and `changed` false; direct
callers receive unchanged IR and metadata, with no source remark published.
After reservation, deletion cannot encounter a budget cancellation halfway
through publishing tombstones. Deleted instructions become NOPs in place and
their source-map metadata is removed without changing instruction IDs or CFG.
The pass manager retains its existing full verification and rollback protocol.

The independent `ssa_dce_phi_liveness` CTest exercises a strict diamond, incoming
copies, chained PHIs, a loop backedge, unused structural PHIs, and unrelated dead
pure chains. Each successful fixture fully verifies and executes through both
Oracle and ExecBC before/after DCE and, where applicable, SCCP+DCE; a repeated
pipeline must be idempotent. Metadata and observable-boundary cases cover GC/
deopt roots, debug state maps, owner MOVE, and throw effects. Every smaller work
budget than a complete direct DCE run must leave the function unchanged, and an
intentionally invalid following pass verifies transaction rollback after DCE.

The public `SZrExecIrPassInfo` contract records required, preserved, and
invalidated analyses.  SCCP requires dominators when a function has blocks;
its rewrite invalidates its own lattice.  DCE invalidates all current
analyses.  A cache carries an IR/input hash as well as a revision, so callers
that reuse a cache cannot accidentally consume facts computed for an older
operand or constant pool.  The function hash includes each block's effect
phi result and incoming range, and every memory region's phi result and
incoming range.  Changing only a token phi therefore invalidates cached
analyses and is visible to pass-change accounting.  The hash excludes the
derived immediate dominator, which is an analysis result rather than IR.

SCCP uses `unknown`, `constant`, `overdefined`, and `must-throw` states.  Add,
subtract, multiply, negate, and divide use checked signed-64-bit helpers.
Binary overflow, divide-by-zero, and `INT64_MIN / -1` become `must-throw`;
NEG of `INT64_MIN` retains its existing `overdefined` result. These operations
remain executable when their results cannot be safely folded. A module pool
is read through the pass context. In the public function-only path, pooled
arithmetic remains unchanged because `layoutId` is a pool index. The private
module-owner path now supplies the typed materializer described below; copy
propagation keeps its existing dominance and ownership checks.

`CONVERT` allows unchanged constant bits only when source and result have the
same canonical signed integer token, any explicit instruction target agrees,
and the private lattice proves that the runner value already has SIGNED
representation. An immediate `CONST` establishes SIGNED storage regardless
of its annotation. A pool constant establishes that proof only for a canonical
signed integer type; its supplied runtime value must follow the typed pool
contract. COPY/MOVE retain the proof, including across annotation changes.
Equal-bit PHI joins retain it only when every executable input proves SIGNED;
losing the proof is a lattice change that propagates to dependent conversions.
Signed arithmetic and NEG retain it only when their operands establish it,
while COMPARE produces BOOL and does not establish SIGNED storage. These are
private provenance facts: other opcodes keep their existing numeric transfers
and branch rules, and the public analysis-cache format stays unchanged.

All other constant conversions become `overdefined`, while an `unknown`
source remains `unknown`. DOUBLE/FLOAT, BOOL, unsigned, custom, unspecified,
and cross-type conversions remain executable. In particular, a no-pool CONST
annotated DOUBLE still stores SIGNED7, so DOUBLE-to-DOUBLE must run to produce
FLOAT7. A DOUBLE pool identity preserves its FLOAT runtime value without
claiming its encoded payload is a signed lattice identity. SCCP does not
simulate floating conversions. CONVERT itself does not use the new owner-aware
Int64 materialization path; its existing signed-representation contract remains.

The legacy public/function-only pool mode still reads encoded payload bits
into the existing scalar lattice. For example, a direct DOUBLE negative-zero branch has nonzero encoded bits
but false runtime truth: SCCP can mark the false arm nonexecutable even though
both runners execute it. The current branch instruction remains unchanged,
so this diagnostic demonstrates incorrect analysis facts rather than a changed
runtime return. The CONVERT guard stops that fact from propagating through a
DOUBLE identity conversion; direct pooled branch/arithmetic interpretation
requires a separate typed-lattice repair in that legacy path. The new nonempty
owner path conservatively marks every normal conditional successor executable,
so an unproven BOOL condition cannot hide a PHI input and create a strict Int64
proof downstream. This does not repair or change legacy numeric branch facts. See the
[representation acceptance record](../../tests/acceptance/2026-10-02-ssa-sccp-representation.md).

Copy propagation never substitutes a definition across a non-dominating
edge, across a same-instruction use, or through a `MOVE` whose ownership
semantics are not proven.  Phi incoming values are left edge-sensitive.  This
conservative rule is preferable to turning a malformed or exceptional CFG
into a different program.

DCE treats opcode schema memory/effect flags and instruction boundary flags
(`MAY_THROW`, `MAY_ALLOCATE`, `MAY_GC`, `MAY_SUSPEND`, debug polls, guard exits,
drop, calls, loads, stores, barriers, and returns) as observable even when no
result is used.  GC roots, deoptimization values, and state-map live/root
ranges seed liveness; phi inputs are marked only when their result is live.
When a pure tombstone is emitted, source-map and non-semantic state-map/GC
site entries for that instruction are compacted so metadata never points at a
deleted definition.

`ZrParser_ExecIr_RunPassPipeline` verifies the complete structure/SSA/effect
contract before and after every pass and is transactional: a failed verifier,
pass, malformed range, or remark allocation restores the original function,
analysis cache, diagnostics context, and remark count.  A work budget emits a
`BLOCKED` remark and keeps the last valid IR; budget exhaustion is not reported
as a compiler error.  Remark sinks are append-only and owned by the caller.
Callers may provide an initialized `SZrExecIrPassFailure` in the pass context
or module optimization options.  On a named pass failure, the manager transfers
that function's failed IR, a copy of the pass name, and its diagnostic into
the record before restoring the original function/module.  This is the
function-level failure record; it does not own the module constant pool, so a
pooled function capture alone is not a self-contained module replay artifact.
The caller owns it and frees it with
`ZrParser_ExecIr_PassFailureFree`.  The next pipeline call clears an older
record.  Invalid input rejected before a pass starts has a diagnostic but no
pass-failure record.  Allocation failure while copying the name cannot
prevent rollback and leaves the failure record empty.
Each successful pass remark separately records `verifierChecks=2` and their
total `verifierTicks` (CPU ticks from `clock()`).  The pipeline preflight check
is not charged to an individual pass.  `elapsedTicks` still measures only the
pass execution; verifier timing is diagnostic, may round down to zero, and
does not affect work budgets or optimization decisions.

`ZrParser_ExecIr_Optimize` applies the same registry to every module function,
shares the compile budget across functions, supplies the module constant view,
and reports an aggregate module hash. Individual passes can be disabled for
binary diagnosis without changing legality checks.

## Nonempty pooled Int64 writeback and module transaction

The new private `RunModulePassPipeline(owner, function, ...)` and
`RunModuleSccpPass(owner, function, ...)` take an explicit mutable candidate
owner. They check function membership with pointer equality after count,
capacity and NULL guards, and refresh the context's constant view at entry,
before passes, after pool growth and before the next function. The public
PassContext and cache layouts are unchanged; no const cast or scratchArena
ownership channel is used. NULL owner and an empty pool retain the legacy
numeric transfer, branch and immediate-constant protocol.

The owner path maintains a separate strict Int64 provenance bit. A canonical
Int64 pool tuple with flags=0 and a canonical Int64 result seeds it. An absent
instruction type annotation (0) or explicit Int64 annotation is accepted;
other explicit annotations are rejected. COPY/MOVE and identity CONVERT cannot
launder an unproven or retagged operand. Arithmetic requires every input and
the actual result to be canonical Int64. PHIs require at least one executable
input and proof from every such input, including external UNKNOWN inputs.
Compatible constant joins take the current transfer proof, allowing both
promotion and erasure; UNKNOWN erases proof even when legacy numeric join
retains constant bits. Owner convergence compares the actual joined value and
proof; a round cap without convergence clears all strict proofs before rewrite.

Only pure flags=0 ADD/ARITHMETIC, SUB, MUL and NEG with no effect or memory
boundary become pooled CONSTANT instructions. Normal DIV retains MAY_THROW,
effect tokens and its instruction; its safely evaluated quotient may feed a
pure ADD/NEG consumer. Zero, MIN/-1, overflow, non-Int64 origins and independent
observable boundaries do not gain a writeback proof. Negative values and
results above UINT32_MAX use full 64-bit bits in the pool, never an immediate
truncated into layoutId.

`MaterializeInt64Constant` deduplicates exactly `(Int64, flags=0, bits)`. Each
scanned tuple consumes one work unit; reuse reserves one rewrite unit and a
new append reserves two units for append plus rewrite before changing the
pool. Instruction IDs are one based and returned pool indices zero based.
TRUE/not-ready means budget exhaustion without an append. TRUE/ready is
followed immediately by a non-failing instruction rewrite with no intervening
allocation or budget check. The helper distinguishes guarded capacity overflow
from allocator OOM. A private FALSE requires discarding the whole candidate;
it does not promise local constant-pool rollback.

Public Optimize first runs real module verification, clones an independent
candidate, optimizes its functions and pool, then verifies the whole candidate
again before publishing it and freeing the previous owner. FALSE frees only
the candidate, restores the caller's remark count and zeros the result; the
original module's content and owner identities remain intact. Budget exhaustion
is a successful, verified partial optimization and may publish that valid
candidate; it is not a failure rollback. Public function-only pipeline rollback
retains its historical function/cache/context contract.

The [pooled Int64 validation record](../testing-and-validation/ssa-sccp-pooled-int64.md)
retains actual RED53/107 with25 registered feature failures and212 completed
fault observations. Actual GREEN independently passes53cases/113profiles,
492metrics,236/236 measured faults and100 owner-budget cutoffs (four W=25),
with108726 allocation attempts and live/errors0. Independently reconciled
metadata15 and GC-parser99 pass from24 original natural-zero phases:
19 compiles,25 exact-cache objects, two links and two CTests. The original
failed controller receipt stays false; repaired strict parsing reconciles its
preserved raw evidence without repeating native execution. Independent MSVC
acceptance covers seven /Zs C syntax checks, eight phases including environment,
99 headers/106 consumed inputs and34 protected build products unchanged,
with zero warnings/errors/unknown output. This is finite syntax W0, with no
MSVC object/link/runtime/full-build W0 or Linux credit. Source37 is committed as
`729c950d969663230e0171d60592939aabd60a00` with independent publication acceptance.
Finite cleanup is closed: six owned files released2080877B;91 reusable build
files totaling17409863B and required raw evidence remain. Completed outcomes
are37 (17features/10fixes/5validation/5docs). This documentation synchronization
adds no outcome; full02.01/SSA47 and global unknown ownership remain open.

Focused validation examples (from the repository root, using separately
registered physical output directories; these examples are not new run evidence):

```bash
cmake --build E:/cargo-targets/zr_vm/build/ssa-gcc-debug --target zr_vm_ssa_pass_manager_scalar_test -j 4
ctest --test-dir E:/cargo-targets/zr_vm/build/ssa-gcc-debug -R '^ssa_pass_manager_scalar$' --output-on-failure --no-tests=error
cmake --build E:/cargo-targets/zr_vm/build/ssa-clang-debug --target zr_vm_ssa_pass_manager_scalar_test -j 4
ctest --test-dir E:/cargo-targets/zr_vm/build/ssa-clang-debug -R '^ssa_pass_manager_scalar$' --output-on-failure --no-tests=error
```

The focused fixture covers fixed-point idempotence, checked division and
overflow, preservation of an unused call, source-map cleanup, malformed input
diagnostics, failed-pass rollback, bounded execution, and hash sensitivity to
effect and per-region memory phi results and range coordinates.  The failed
pass fixture also checks that the captured function reproduces the verifier
diagnostic after rollback and that its pass name survives caller mutation.
Every successful scalar pass remark is checked for two verifier boundaries.

The separate `ssa_sccp_conversion` CTest target uses always-active checks and
compares Oracle and ExecBC results before and after SCCP. Its 33 cases cover
explicit and implicit integer-to-double targets, double-to-integer truncation,
integer-to-boolean normalization, same-type pool constants, same-type immediate
folding, a rounding-sensitive integer/double/integer round trip, and an
unspecified-type conversion accepted at both pipeline verifier boundaries,
and an explicit scalar target that differs from the result annotation. They
also cover explicit/fallback same-token typed immediates, every canonical
signed width, conservative custom tokens, FLOAT pool COPY/NEG retagging,
signed COPY propagation, and loop PHIs whose late backedge either retains
SIGNED proof or loses it through a BOOL-producing comparison with equal bits.
Run with `ctest --test-dir E:/cargo-targets/zr_vm/build/<registered-config> -R '^ssa_sccp_conversion$'
--output-on-failure --no-tests=error`. The acceptance record contains the
concrete build paths, commands, and observed results.
