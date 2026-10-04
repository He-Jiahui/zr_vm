---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_copy_aliases.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_copy_aliases.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_internal.h
  - tests/cmake/ssa-tests.cmake
  - tests/cmake/ssa-sccp-conversion-tests.cmake
  - tests/parser/ssa_pass_manager_state_maps_cases.inc
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_copy_aliases.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_copy_aliases.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
plan_sources:
  - .codex/plans/20261004-ssa-sccp-phi-copy-edges.md
  - docs/plans/ssa/02-automatic-optimization/01-pass-manager-scalar.md
  - .codex/plans/20261004-scalar-checkpoint-ownership-fixture-diagnosis.md
tests:
  - tests/parser/test_ssa_sccp_phi_copy_edges.c
  - tests/parser/test_ssa_sccp_copy_availability.c
  - tests/parser/test_ssa_sccp_conversion.c
  - tests/parser/test_ssa_pass_manager_scalar.c
  - tests/parser/ssa_pass_manager_state_maps_cases.inc
doc_type: module-detail
---

# Private ExecIR COPY alias rewrite

Status: implemented after Root's independently adopted semantic RED V32.
Independent review identified unguarded interior predecessor boundaries; Root
independently adopted actual guard RED V33 before the budgeted body-scan repair.
Root independently accepted dedicated GREEN V36. Conversion, COPY availability
and DCE regressions have passed; Root also accepted actual repaired scalar
checkpoint V43. This document preserves those separate acceptance boundaries.

## Responsibility and API

`ZrParser_ExecIr_RewriteCopyAliases(function, context, changed, diagnostic)` is
private to parser scalar passes. SCCP calls it after storage validation and
lattice computation, before constant materialization. The helper owns temporary
alias, definition and containing-block arrays and frees them on every return.
Capacity overflow and allocation failure return false with the existing
diagnostic. Budget exhaustion returns true and remains visible in the context;
SCCP stops materialization when that flag is set. The public SCCP API is unchanged.

The extraction preserves ordinary operand behavior: UNKNOWN source/result
ownership, zero flags/effect/memory boundaries, same-block definition-before-use
or existing immediate-dominator traversal, and unchanged changed/source reporting.
It scans all MOVE, DROP and DROP_IF_INITIALIZED operands before alias creation.
Consumed identities, including UNKNOWN values and later consumers, become
permanent invalid entries for the invocation. COPY classification cannot overwrite
those tombstones. Numeric lattice, executable-edge analysis and constant encoding
remain in SCCP. No parallel implementation or compatibility path remains.

## PHI incoming use sites

The new path walks block PHIs and each incoming slot independently. Incoming
count must match the block's ordered predecessor count and the predecessor ID
must agree at that slot. Counting earlier slots with the same predecessor yields
the occurrence number. The resolver selects that occurrence among the actual
predecessor's ordered successors. Two successors to the same target therefore
remain two uses, with independent incoming values.

The predecessor's terminator ID must identify its actual last instruction, and
its successor range must equal the block successor range. Only BRANCH,
CONDITIONAL_BRANCH and SWITCH without independent boundaries are accepted.
Exceptional blocks and INVOKE, SUSPEND or other terminators remain conservative.
Before accepting the incoming use, every actual predecessor body instruction
is scanned with a budget charge. Dynamic flags, binding/deopt metadata,
effect/memory tokens, intrinsic memory reads/writes and opcode schema flags
beyond pure value production reject the predecessor. A CALL with zero encoded
flags is still rejected by its intrinsic schema; no provider purity is inferred.
No executable-edge lookup, first-matching-predecessor substitution or CFG pruning
participates in this identity rewrite.

Each traversed COPY must belong to that actual predecessor and precede its
terminator. Further local COPY definitions must have strictly smaller instruction
IDs than the preceding COPY. This both establishes local order and excludes
cycles. The value-count step bound is an additional conservative limit.
Eligibility comes from the original alias table's ownership/tombstone checks;
the PHI path additionally rejects bindingRow, deoptId, flags, effect/memory tokens,
PHI ranges and successor ranges on COPY definitions. A source whose own COPY
definition is in another block is retained at that identity; it is not traversed.
The new PHI path does not read cached or fresh dominators.

Only `incoming.value` is mutated. PHI result, predecessor IDs, ordered occurrences,
counts, CFG, instruction opcodes and metadata stay intact. Changed-source reporting
uses the actual predecessor terminator's source ID. Slot scans, successor scans
predecessor body scans and COPY chain steps consume pass budget. A chain that exhausts budget retains
its original incoming value; already finished rewrites retain normal pass semantics.

## Conservative maps and validation limits

State-map presence and populated GC/deopt identity tables skip the new PHI path.
This does not repair or prove logical map liveness. General ownership transfer,
cross-block COPY propagation, exceptional result availability and whole backend
differential acceptance remain outside this finite slice. Legal Core SSA input
is required; the helper is not a replacement verifier.

The dedicated fourteen-case fixture covers local COPY, chains, parallel occurrences,
cross-block and ownership preservation, MOVE/DROP, mapped and exceptional/suspend
boundaries, interior suspension/CALL token ordering, and malformed Core diagnostics.
The guards use recognized CALL heap/FFI token/effect metadata and STRUCTURE|SSA
validation only. Root observed14 cases with exactly these two failures and
zero precondition failures before the body-scan repair. Root accepted V36 GREEN
with14 passes,94 precondition passes, six pure paths and three diagnostics.
Only pure pointer-free COPY/branch/
PHI/RETURN inputs enter the Oracle. Existing conversion, COPY availability and
scalar pass manager targets have the fresh scoped evidence below.

## Actual verification and checkpoint ownership repair

Root's V36 independent receipt confirms420 input pins checked twice unchanged
and all35 actual project TUs built with UBSan/UNDEBUG:
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/independent-sccp-phi-green-v36-current-v1.json`,
SHA256 `1298e24afed4cdf9005b553700ac1de4249d65c93944ee1caae08dcb244bc86d`.
Dedicated PHI acceptance is STRUCTURE|SSA, not full EFFECT or mapped liveness.

V39 conversion33 and COPY availability19 cases x3 plus budget passed separately.
Its scalar assertion failure was an older GC alias expectation, inconsistent
with the already-existing UNKNOWN-only ordinary alias gate. The failed overall
receipt is retained. V40 independently accepted16 DCE PHI-liveness groups;
receipt SHA256 `e4c820433f31f4b172d0cec0bded1426623037fdfa5a2144df1572f451755dab`
(231793 bytes),
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/sccp-dce-v40/Root-receipt.json`.

The fixture-only checkpoint repair runs GC preservation and UNKNOWN map rebuild
inside the existing scalar entry, keeping16 main calls. GC retains copied as
RETURN, single live value, single root and materialized identity. UNKNOWN
retains zero roots and must change its single live/materialized identity from
copied to source. Exact counts/IDs and VerifyALL guard both modes; the request
is zero-initialized and materializes only logical IDs. Production ownership,
ordinary alias eligibility and provider behavior are unchanged. Root accepted
actual scalar V43 with natural exit0, all assertions active,16 main entry calls
and both ownership subcases, fresh fixture/formal24-TU closure. Receipt:
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/sccp-scalar-v43/Root-receipt.json`,
SHA256 `ec86e523923d6aff3068f91214889b0723afac78d8a59a290b9850a627b24a1a`
(223262 bytes). V39's failed overall receipt remains immutable. Root owns final
independent regression adoption and finite commit; no full scalar/SSA milestone
closure is implied.

## Build closure

The new helper TU is mandatory beside SCCP in both explicit lists in
`tests/cmake/ssa-tests.cmake` and the conversion fragment. PHI and availability
fragments inherit conversion sources, so they need no duplicate registration.
Production parser static/shared modules discover it through the existing
recursive source glob. The dedicated fixture closure grows from34 to35 TUs.

Final independent regression adoption checked449 relevant pins twice and
accepted conversion33 cases, COPY availability19 cases repeated three times,
DCE16 groups and the repaired16-entry scalar fixture. Evidence:
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/independent-sccp-four-regressions-current-v1.json`,
SHA256 `2510ef577e5561b84f7effc3274e818fa7bf33fa0b1e719490ac7a6e2a43e964`.
Only successful steps and their actual input closures are adopted from V39;
the original failed receipt remains unchanged.
