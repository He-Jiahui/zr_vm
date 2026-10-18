---
related_code:
  - tests/parser/test_ssa_sccp_pooled_bool.c
  - tests/parser/support/ssa_sccp_pooled_bool_fixture.h
  - tests/parser/support/ssa_sccp_pooled_bool_fixture.c
  - tests/parser/support/ssa_sccp_pooled_bool_faults.c
  - tests/cmake/ssa-sccp-pooled-bool.cmake
  - tests/parser/test_ssa_dce_dead_phis.c
  - tests/parser/support/ssa_dce_dead_phis_fixture.h
  - tests/parser/support/ssa_dce_dead_phis_fixture.c
  - tests/parser/support/ssa_dce_dead_phis_faults.c
  - tests/cmake/ssa-dce-dead-phis.cmake
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp_owner.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp_internal.h
  - tests/parser/test_ssa_sccp_branch_sensitive_pool.c
  - tests/parser/support/ssa_sccp_branch_sensitive_pool_fixture.h
  - tests/parser/support/ssa_sccp_branch_sensitive_pool_fixture.c
  - tests/parser/support/ssa_sccp_branch_sensitive_pool_faults.c
  - tests/cmake/ssa-sccp-branch-sensitive-pool.cmake
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
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp_owner.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_constant_materialize.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_manager.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_dce.c
plan_sources:
  - .codex/plans/20261018-ssa-scalar-completion.md
  - .codex/plans/20261017-ssa-scalar-pipeline.md
  - .codex/plans/20261017-ssa-sccp-bool-edges.md
  - .codex/plans/20261017-ssa-sccp-pooled-int64.md
  - .codex/plans/20261016-ssa-gc-map-parser-consumers.md
  - docs/plans/ssa/01-execir-ssa/03-effects-verifier.md
  - docs/plans/ssa/02-automatic-optimization/01-pass-manager-scalar.md
tests:
  - tests/parser/test_ssa_sccp_pooled_bool.c
  - tests/parser/support/ssa_sccp_pooled_bool_fixture.h
  - tests/parser/support/ssa_sccp_pooled_bool_fixture.c
  - tests/parser/support/ssa_sccp_pooled_bool_faults.c
  - tests/cmake/ssa-sccp-pooled-bool.cmake
  - tests/parser/test_ssa_dce_dead_phis.c
  - tests/parser/support/ssa_dce_dead_phis_fixture.h
  - tests/parser/support/ssa_dce_dead_phis_fixture.c
  - tests/parser/support/ssa_dce_dead_phis_faults.c
  - tests/cmake/ssa-dce-dead-phis.cmake
  - tests/parser/test_ssa_sccp_branch_sensitive_pool.c
  - tests/parser/support/ssa_sccp_branch_sensitive_pool_fixture.h
  - tests/parser/support/ssa_sccp_branch_sensitive_pool_fixture.c
  - tests/parser/support/ssa_sccp_branch_sensitive_pool_faults.c
  - tests/cmake/ssa-sccp-branch-sensitive-pool.cmake
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
source_refs:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp_owner.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp_internal.h
  - tests/parser/test_ssa_sccp_branch_sensitive_pool.c
  - tests/parser/support/ssa_sccp_branch_sensitive_pool_fixture.h
  - tests/parser/support/ssa_sccp_branch_sensitive_pool_fixture.c
  - tests/parser/support/ssa_sccp_branch_sensitive_pool_faults.c
  - tests/cmake/ssa-sccp-branch-sensitive-pool.cmake
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

DCE validates storage, ranges, and ordinary value IDs before reading new PHI
consumers. GC roots, deoptimization values and aggregate fields, and state-map
live/root values seed liveness. Observable instructions retain their operands:
control flow, owner operations, memory reads/writes, explicit effect or memory
tokens, throw/GC/suspend/allocation/debug/guard boundaries, and existing GC or
state-map sites. NOP remains observable under the existing opcode policy.

An ordinary PHI propagates its incoming values only when its result is live.
Repeated PHI and instruction visits find a monotone fixed point, so an unrooted
self cycle or mutually recursive pure component can be removed. A legacy PHI
opcode remains observable and conservatively roots its result and ordinary
incoming dependencies. Metadata roots continue to keep the whole required
producer chain alive.

DCE compacts the logical `phiPool` in its original physical order. A prefix
indexed by the old PHI position remaps every block's `phis` range, including
ranges whose block order differs from physical storage and inactive holes.
It preserves `phiIncoming` storage, pointer, count, capacity, and bytes, together
with all surviving ordinary, legacy, effect, and memory incoming ranges.
Unused backing rows remain allocated. Value IDs, instruction IDs, CFG edges,
predecessor occurrences, and successor order remain stable.

The shared incoming pool contains separate ordinary and token domains. Ordinary
PHI consumers and legacy opcode PHI ranges independently require ordinary value
IDs, even when a row also belongs to a token range. Effect and memory tokens
can exceed `valueCount`; they never index the ordinary liveness array. Initial
zero tokens remain legal where Core's predecessor terminal contract permits
them. Domain classification does not replace the complete Core effect verifier.

All scratch allocations, validation, marking, deletion decisions, and publication
work reservations finish before the first IR mutation. Budget exhaustion returns
success with `budgetExhausted` set and `changed` false, leaving IR, backing pools,
and metadata unchanged. Allocation failure returns false before publication.
After reservation, logical PHI compaction and instruction deletion cannot fail.
Dead pure instructions become NOPs in place and their removable source/GC/state
site entries are compacted. PHI-only deletion reports `changed=true`; a genuine
repeat with no IR change reports false. The manager's full verification,
transaction rollback, hash, remark, and analysis policy remain in force.

The IR-only `ssa_dce_dead_phis` fixture covers dead diamonds, chains, cycles,
mixed live/dead PHIs, shared token storage, duplicate predecessor occurrences,
metadata and observable roots, publication budgets, allocation failure/retry,
module transactions, and actual changed/cache/remark fixed points. The original
`ssa_dce_phi_liveness` fixture has a separate Oracle/ExecBC execution scope.
Source39's current native evidence and its explicit validation limits are recorded
in [the dead-PHI validation record](../testing-and-validation/ssa-dce-dead-phis.md).

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
requires a separate typed-lattice repair in that legacy path. The nonempty
owner path now uses the separate typed analysis described below: an exact BOOL
selects one successor slot, TOP admits both, and unresolved BOTTOM is deferred
until the residual fallback. This does not repair or change legacy numeric
branch facts. See the
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

The nonempty owner path uses a private monotone typed lattice, separate from
the legacy numeric lattice and public cache: BOTTOM rises to one exact typed
constant and then to TOP on conflict. Canonical Int64 pool tuples with flags=0
and matching result types establish strict Int64 facts. A missing instruction
type annotation (0) or matching canonical annotation is accepted. External
values and unsupported definitions seed TOP; supported, unresolved definitions
seed BOTTOM. Retagged COPY/MOVE and CONVERT do not manufacture typed proof.
Only canonical BOOL bits 0 or 1 establish a condition; other BOOL encodings
remain TOP, without declaring those encodings globally invalid. Arithmetic
requires canonical Int64 operands and results. PHIs merge only executable
predecessor occurrences, with exact incoming-slot/edge-occurrence matching;
an executable external input is TOP and prevents a false constant proof.
Budget interruption or a defensive convergence cap returns incomplete facts;
the caller skips cache publication and rewrite and invalidates old SCCP facts.

Pure flags=0 ADD/ARITHMETIC, SUB, MUL and NEG with no effect or memory
boundary become pooled Int64 CONSTANT instructions. The owner path also writes
canonical BOOL results of EQ, LT, LE, GT, GE and NE when both actual operands
retain strict canonical Int64 facts. This comparison rewrite requires a BOOL
result and exact BOOL lattice fact with bits 0/1, two operands, one result and
no flags, effect/memory, binding, deopt, match or control-range payload. Its
CONSTANT typeToken is explicitly BOOL: the former COMPARE typeToken encoded a
predicate and cannot be retained as a result type. Instruction/result/source
identities and CFG remain stable. Normal DIV retains MAY_THROW,
effect tokens and its instruction; its safely evaluated quotient may feed a
pure ADD/NEG consumer. Zero, MIN/-1, overflow, non-Int64 origins and independent
observable boundaries do not gain a writeback proof. Negative values and
results above UINT32_MAX use full 64-bit bits in the pool, never an immediate
truncated into layoutId.

The single private `MaterializeScalarConstant` accepts only Int64 or canonical
BOOL and deduplicates exactly `(typeToken, flags=0, bits)`. Identical Int64 and
BOOL bits are distinct tuples, and BOOL bits outside 0/1 are rejected before
pool mutation. The existing Int64 instruction typeToken=0 compatibility is
retained. Each scanned tuple consumes one work unit; reuse reserves one rewrite unit and a
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

<a id="branch-sensitive-nonempty-owner-analysis-source38-validation-in-progress"></a>

## Branch-sensitive nonempty owner analysis (Source38, finite delivery closed)

The implementation is split across three production files. `exec_ir_sccp.c`
keeps public/owner dispatch, scratch lifetime, cache publication and the existing
pure rewrite gate. `passes/exec_ir_sccp_owner.c` solves typed values and executable
edge slots; `passes/exec_ir_sccp_internal.h` shares only private facts, checked
Int64 arithmetic helpers and the complete/incomplete analysis interface. Public
PassContext/cache ABI and NULL-owner/empty-pool legacy semantics are unchanged.

Exact BOOL constants and the six strict Int64 comparisons (EQ, NE, LT, LE, GT,
GE) can prove a condition. COPY and PHI preserve only matching canonical facts.
A known condition admits its true slot (ordinal 0) or false slot (ordinal 1);
TOP admits both. BOTTOM initially admits neither, allowing a nested condition
to become known after an earlier branch selects a BOOL PHI. At a stalled fixed
point, executable residual BOTTOM conditions rise to TOP and both slots are
added before solving again. Facts and edge marks only rise; loop and duplicate
successor slots do not rely on a frozen all-successors certificate. PHI matching
counts predecessor occurrences, not just equal source/target block IDs, and
checks the terminator's ordered successor contents even when its flat row is
distinct from the block row. Branch/PHI instructions and CFG storage are retained;
this slice improves facts and folds eligible pure consumers, not structural CFG
compaction or arbitrary BOOL/foreign numeric truthiness.

Analysis traversal consumes the existing work budget. Incomplete/budget-stopped
owner facts are never exported to the cache or used for rewriting. A real rewrite
invalidates pre-rewrite SCCP cache values even if the caller supplies no `changed`
output. The existing typed scalar materializer, tuple deduplication, pre-paid append and
rewrite work, public module candidate transaction, Verify-before-publication and
private whole-candidate failure rule above remain in force. DIV keeps its throwing
effect; dead or unknown arms do not authorize deleting observable instructions.

[Source38 validation](../testing-and-validation/ssa-sccp-branch-sensitive-pool.md)
records accepted actual GREEN: 22 cases/47 profiles, four independently measured
W=113 sweeps (452 cutoffs), 746/746 fault sites, 388768 attempts and live/errors=0.
The initial 31-TU build used four actual compiles plus 27 exact objects from one
RED history and one link, with fresh compile warnings/errors zero. After the main
counter type was changed to UNITY_UINT, focused r2 reran configure/build/test:
one fresh main compile, 30 exact GREEN-r1 cached objects and one link; warnings
zero and the same 22/47, 452 cutoffs and 746 faults passed. The focused-r2 final
independent review is accepted (D9532A0D6DDB0A74F7594B685681EFB2286EA134577B01CDA35939CB2E139835).

Current-production regressions are independently accepted: 31 natural-zero
phases (25 compiles, 26 exact GREEN cached objects, three links and three CTests),
strong53/113 with 239 faults, four W=51 sweeps (204 cutoffs), 117024 attempts
and live/errors zero; metadata15 and GC99 also pass. GC emits 99 LEGAL rows
(21 standalone plus 78 fresh baselines) and 78 MALFORMED rows. MSVC syntax r2
retains its one main C4244 as historical evidence. After the type repair, one
fresh main /W4 /Zs phase plus four unchanged actual syntax phases cover five
current C files, 96 actual headers and 35 protected build products unchanged,
with warning/error/unknown output zero. Its supplemental independent review is
accepted (334CA90113E2EC69D890C5049676FF5DC736068B6D2F46D347496F3F733AF786).
This is finite syntax only, not full MSVC/full-build W0 or Linux. Source38 is
committed as `c90666fa38ef4eff3d5b4ceaed54744e9cad8eaf`; independent postcommit review
and finite-close are accepted. Exact standard cleanup released five owned files,
2026503B; 610 required preserved pins match and the owned task tmp is empty.
The retained build inventory (390 files/69825703B) includes other/unknown items
and is not a claim that all belong to Source38 or are independently validated.
Completed outcomes are 38 (18 features/10 fixes/5 validation/5 docs); this closure
document update adds zero. Full 02.01/SSA47 and global unknown ownership remain active.

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

## Canonical BOOL comparison writeback validation

[Source40 validation](../testing-and-validation/ssa-sccp-pooled-bool.md) records
the initial real RED: two cases, four NULL/writable-diagnostic profiles, all
Core/Parser pre/post verification and safety checks successful; EQ BOOL
writeback alone missing, disabled-SCCP control passes, allocation ledger zero.
The three production files implement typed BOOL writeback. Focused GREEN and
its independent review A9214238 are accepted: 22 cases/84 profiles, eight
independently measured private W=37 sweeps (296 cutoffs), and 746/746 actual
allocation faults with successful retries. The public two-function sweep uses
an explicit upper bound of 138 for each diagnostic mode (276 cutoffs); this is
not measured public workUsed. The run recorded 459698 allocation attempts and
live/errors=0. The current 31-TU evidence consists of 27 individually accepted
prior phases plus four fresh compiles, one link and the focused CTest; fresh
diagnostics are zero and the original failed compile receipt is preserved.

Finite MSVC /W4 /Zs syntax evidence is independently accepted (AC90E867) for
eight current C files, 98 actual headers and 233 protected build products.
Five unchanged successful syntax phases are combined with three fresh phases
after the old Int64 main counter repair; the original warning-bearing receipt
remains failed. This grants no object/link/runtime, full MSVC/W0 or Linux credit.
The five IR-only regressions also pass: pooled Int64 53 cases/113 profiles,
branch-sensitive 22/47, Source39 DCE 21/140, metadata15 and GC99. Their actual
fault schedules complete 239, 746 and 610 injected failures respectively, with
zero live allocations and ledger errors. The regression build uses 35 fresh
role objects and 26 exactly validated provider objects, five links and five
anchored CTests. The first link's normal verbose stderr required a collector
correction; its failed controller receipt remains failed. Publication and owned
artifact cleanup are recorded separately in the Source40 ledger. No VM, Oracle,
ExecBC, FFI or MaterializeState execution is credited; the full scalar leaf and
SSA47 remain open.

The historical Source37 compare-BOOL control is migrated to require a BOOL
constant while still rejecting an Int64 same-bit replacement. Source38's
representation-sensitive COMPARE witness is migrated only for proven pure
comparisons, with separate exact BOOL tuple/identity checks; ordered CFG/PHI,
DIV/effect and unsupported comparisons remain protected. Original historical
receipts are not rewritten as if the new behavior existed then.
