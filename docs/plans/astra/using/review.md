---
related_code:
  - zr_vm_core/src/zr_vm_core/ownership.c
  - zr_vm_core/src/zr_vm_core/ownership_resource.c
  - zr_vm_core/src/zr_vm_core/ownership_shared.c
  - zr_vm_core/src/zr_vm_core/execution/execution_control.c
  - zr_vm_core/src/zr_vm_core/closure.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_scope.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_flow.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_cleanup.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_using.c
implementation_files: []
plan_sources:
  - docs/plans/astra/index.md
  - docs/plans/using/index.md
  - docs/plans/using/00-current-state.md
  - docs/plans/using/01-ownership-as-generics.md
  - docs/plans/using/02-using-scopes-and-plugin-guards.md
  - docs/plans/using/03-metadata-and-token-model.md
  - docs/plans/using/04-union-types.md
  - docs/plans/using/05-migration-and-phasing.md
  - docs/plans/using/06-syntax-and-semantic-checks.md
  - docs/plans/using/07-implementation-blueprint.md
tests:
  - tests/parser/test_resource_unique_drop.c
  - tests/parser/test_resource_shared_weak.c
  - tests/parser/test_resource_owner_borrow_receiver.c
  - tests/parser/test_cfg_finally_abrupt.c
  - tests/parser/test_aot_c_ownership_contracts.c
  - tests/parser/test_aot_c_scope_contracts.c
doc_type: milestone-detail
---

# Ownership And Using Review

Status: reviewing; code findings below require executable confirmation before repair.
Source baseline: `c95e5387` on `main`, including the pre-existing dirty files
listed in the Astra master plan. The master plan owns integration and commits.

## Scope And Interpretation

The target `UsingStatementSyntax` is still `surfacePending`. This review does
not freeze a spelling, turn the existing guard parser into the target grammar,
or treat an old `%using` fixture as new Close-protocol acceptance. Resource
Drop, lexical Close, and GC finalization remain independent contracts.

Current code has advanced beyond the June historical summaries: `resource
class`, `own`, canonical owner identity, direct Unique resource roots,
Shared/Weak controls, and an explicit `intoGc` opcode all have implementations
and focused tests. A fresh build is required before promoting their recorded
results. The following table records source/test inventory, not pass claims.

## Plan Versus Implementation

| Plan | Current implementation evidence | Existing focused coverage | Remaining promotion gap |
|---|---|---|---|
| 00 current state | Resource runtime is split into `ownership_resource.c`, `ownership_shared.c`, and generic ownership entry points. June records explicitly label themselves historical. | `test_resource_unique_drop.c`, `test_resource_shared_weak.c` | Reconcile current target-language coverage with the old baseline inventory after fresh execution. |
| 01 owner types O1-O5 | `own` creates direct Unique without control allocation; `share` creates the non-atomic isolation-bound control; Weak retains control identity; `intoGc` consumes direct Unique into a GC box. | Direct root count, move/drop order, partial construction, GC box, weak expiry, final strong release, cross-domain copy/wake rejection | Returned owner lifetime through finally; failed operations preserving owners; full cross-domain rejection matrix; artifact and backend parity. |
| 02 Close C1-C6 | `compiler_scope.c` registers owner Drop and ordinary close separately; `closure.c` executes registered cleanup; exception handlers snapshot the registration boundary. Existing parser still supports mixed drop/union/plugin guard nodes. | Simple normal/return/throw/break/continue owner tests; exception-scope tests; historical using project fixtures | Canonical Close protocol identity/effects, all nested cleanup edges, initialization failure, exception aggregation, async rejection, and surface freeze remain separate gates. |
| 03 metadata M1-M5 | Canonical owner types and resource modifier flags exist; scope cleanup currently lowers through ExecBC registration and Drop/Close opcodes. | Ownership AOT helper checks, metadata token and project/artifact tests | Structural Close callable/effect hash, malformed cleanup sections, and independently executed four-backend parity. Source-text helper checks alone do not establish runtime parity. |
| 04 patterns | Union layout, CFG branches, and switch/exhaustiveness tests exist; legacy using pattern lowering remains in `compile_statement.c`. | `test_union.c`, `test_cfg_union_exhaustiveness.c`, union metadata tests | Target payload Place moves, partial Drop, nested generic payloads and all consumers must be assessed by the syntax owner. Guard success does not prove Close. |
| 05 migration | The legacy parser and compiler still carry drop, pattern and plugin guard roles. | Migration and LSP fix fixtures; old using projects | AST-role inventory, target rebind, idempotence and allowlist must remain explicit. Close edits cannot be machine applicable while surface is pending. |
| 06 static checks | Dataflow exposes ownership moves/regions/owner sets; compiler validates return escape and receiver guard facts. | `test_resource_owner_borrow_receiver.c`, `test_compiler_return_ownership_diagnostics.c`, semantic/dataflow and LSP ownership tests | Demonstrate stable diagnostics for every cleanup/control-flow failure; test runtime-only errors independently of static success. |
| 07 U1-U7 | Reusable U1/U2 implementation and consumer projections exist, alongside legacy U3/U4/U5 paths. | Leaf targets above plus project and reference harness | No whole-family completion claim until fresh leaf-to-project runs and independent backend evidence meet the master gates. |

## Concrete Findings And Reproduction Plan

### U-F1: Pending Return Owner Retention Is Discarded Without Release

Priority: P1. State: source-confirmed hypothesis, reproduction pending.

`execution_control.c:449` (`execution_set_pending_control`) copies a return
value into `state->pendingControl.value` using `ZrCore_Value_Copy`. Shared and
Weak copies retain a count in `ownership.c:775` and `ownership.c:781`.
`execution_clear_pending_control` at `execution_control.c:436` clears that
storage using `ZrCore_Value_ResetAsNull`, which does not release ownership.
`execution_set_pending_exception` and the valueless setter take the same
reset path. The VM and AOT finally completion paths both copy the pending
value back before calling the common clear helper.

Trigger: return a Shared/Weak value through `try/finally`, or replace a pending
owner return with a finally exception/break/return. Expected: pending storage
releases precisely the retention it acquired. Suspected actual behavior:
Shared survives the last user owner and resource Drop does not run; Weak
control storage may remain live after every visible weak handle is released.

Regression ownership requested: `tests/parser/test_resource_shared_weak.c`
and the common helper in `execution_control.c`. Start with direct pending
control refcount assertions, then source-level return/finally lifetime checks.
Do not edit the user's dirty `execution_dispatch.c` merely to reach this helper.

### U-F2: Rejected Cross-Domain Release Loses The Handle

Priority: P2. State: source-confirmed hypothesis, reproduction pending.

`ownership.c:663` resets a Shared or Weak value before calling
`ZrCore_OwnershipShared_ReleaseStrong` or `ReleaseWeak`. Those helpers reject a
different isolation domain in `ownership_shared.c:133` and
`ownership_shared.c:188`. The caller therefore loses the only handle while
the strong/weak count remains unchanged.

Trigger: embedder calls `ZrCore_Ownership_ReleaseValue(otherState, &owner)` on a
handle belonging to the original state. Expected: a rejected operation leaves
the handle available for release in its owning domain. Suspected actual:
owner becomes null and a resource/control allocation remains retained.
Existing cross-domain coverage checks only copy and wake at
`test_resource_shared_weak.c:250`.

Regression ownership requested: the same Shared/Weak leaf test and
`ownership.c`; preserve counts, value identity and origin-domain cleanup for
both owner kinds. Direct Unique cross-domain ownership is a separate transfer
contract and must not be altered by guessing from Shared controls.

### U-F3: Abrupt Cleanup And Finally Nesting Need Executable Coverage

Priority: P1 investigation. State: source risk, not yet a demonstrated defect.

`compile_return_statement` resumes finally before emitting all active owner
cleanups, whereas `compile_break_continue_statement` emits cleanup before
scheduling finally. `execution_resume_pending_via_outer_finally` traverses
active handler state, while the exception path closes registrations at each
handler boundary. A loop nested inside a try also calls the innermost-finally
lookup without an explicit target-scope filter at the call site.

Required reproductions: resource inside try drops before its finally body;
resource outside try remains live during finally; break/continue to a target
still inside try do not prematurely execute finally; nested finally preserves
the pending return and releases each owner once; a local finally abrupt exit
replaces the prior transfer without retaining an abandoned owner.

These cases belong in the existing resource leaf target, with lower-level
pending-control tests where an owner count identifies the failure directly.
Only the highest demonstrated cause should be repaired first.

## Reference Evidence

- Lua `lua/src/lfunc.c:213` and `:227` pop a to-be-closed registration before
  calling its close method; `lua/testes/locals.lua` covers stack relocation
  during close. This supports pop-before-callback and restored stack pointers.
- Rust `lua/rust/library/alloc/src/rc.rs:2468` drops on the final strong count;
  `:3649` releases weak retention; `:3523` checks strong liveness on upgrade.
  An internal temporary holding an Rc/Weak must obey the same retention rule
  as visible values.
- CPython `lua/cpython/Python/codegen.c:535` and `:649` unwind actual block
  stacks, including finally/with and the loop destination. Its
  `lua/cpython/Lib/test/test_with.py` supplies normal and exceptional exit
  precedent. This does not justify inventing ZR using grammar.

## Prioritized Execution

1. Establish current GCC leaf results for resource Unique, Shared/Weak, owner
   receiver and CFG finally tests. Root owns fresh build provenance.
2. Add an owner-retention regression for U-F1 before changing the shared
   helper. Confirm failure and exact count/root/drop consequence.
3. Repair U-F1 through the common pending-control ownership contract, then
   run Unique/Shared/Weak and exception/finally integration on GCC and Clang.
4. Add and confirm U-F2; repair only after ownership of `ownership.c` is
   coordinated. Validate no changes to legal same-domain release behavior.
5. Exercise U-F3 nesting matrix; escalate producer/runtime ownership when a
   source-level failure demonstrates the exact responsible boundary.
6. Update module and acceptance documents, request root review, then let root
   stage explicit files and commit the verified item independently.

## Measured Optimization Queue

No optimization is accepted from this review's source inspection.

| Priority | Candidate | Required measurement | Acceptance constraint |
|---|---|---|---|
| After functional repairs | Cleanup registration search and per-owner `OWN_DROP` plus `CLOSE_SCOPE` traffic | Callgrind and wall time on normal/abrupt owner-heavy loops; exact Drop trace and active root counts | No semantic widening; >=3% benefit with 95% CI excluding zero, CV <5%; original representative and memory gates. |
| After functional repairs | Redundant owner copies around pending return and call boundaries | Retain/release observer counts, allocated controls and repeated return/finally throughput | Optimize only proven redundant ownership with matching source/binary/AOT outcomes. |
| After valid baseline | Direct Unique first-share allocation and last-strong weak cleanup | Allocation counters plus RSS over repeated create/share/degrade/wake/drop cycles | No leaked controls, no unexplained >5% RSS increase; retain first-share allocation policy. |

Run controlled timings serially with root's benchmark schedule. Instruction
counts are attribution evidence, not a replacement for the master wall-clock
or four-backend gates.

## Validation Ledger

- Fresh executions: pending; no historical pass counts adopted.
- Production edits: none at review publication.
- New syntax: none; target using remains `surfacePending`.
- Whole U1-U7 acceptance: open.
