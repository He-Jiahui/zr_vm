---
related_code:
  - zr_vm_core/src/zr_vm_core/execution/execution_control.c
  - zr_vm_core/src/zr_vm_core/state.c
  - zr_vm_core/src/zr_vm_core/ownership.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/execution/execution_control.c
  - zr_vm_core/src/zr_vm_core/state.c
  - zr_vm_core/src/zr_vm_core/ownership.c
plan_sources:
  - docs/superpowers/specs/2026-08-10-ownership-object-member-separation-design.md
  - docs/plans/astra/using/review.md
  - docs/plans/astra/syntax/advanced-review.md
  - user: 2026-09-05 approved Astra ownership implementation plan
tests:
  - tests/parser/test_resource_shared_weak.c
  - tests/parser/test_ownership_intrinsic_member_separation.c
  - tests/scripts/test_syntax_status_records.py
  - tests/scripts/test_syntax_migration_inventory.py
doc_type: milestone-detail
---

# Ownership And Object Member Separation Supplement

Status: implementing; final integrated acceptance remains open.
Baseline: `c95e5387` on `main`, with unrelated concurrent edits preserved.

## Fixed Language Contract

`share`, `degrade`, `wake`, `intoGc`, and `drop` are reserved intrinsics.
`.` and `?.` access object or weak-reference targets; `?.(args)` is optional
invocation. Direct null/expired target access raises `NullReferenceError`.
This supplement adds no grammar, public API, or artifact ABI. Target Using
syntax remains `surfacePending`; whole-repository optimization is out of scope.

## Implementation Sequence

- [ ] U-F1: reproduce and release pending Shared/Weak retention on clear,
  exception replacement, valueless transfer, normal resume, and thread reset.
  Initialization must only initialize storage.
- [ ] Reentry: repeat clear, replace return, self-alias, nested Drop calls and
  exceptions. Release once and preserve pending results/exception roots.
- [x] U-F2: validate Shared/Weak domain before reset; rejected release preserves
  identity/counts and origin-domain cleanup. Keep function signatures.
- [ ] U-F3: nested finally, exception override, break/continue, weak chain
  results and hidden Shared cleanup. Reuse other Astra fixes when available.
- [ ] Regenerate migration golden with the official scanner; two deterministic
  runs, exact comparison, negative fixtures and zero findings remain required.

## Validation Gates

1. Public-runtime counts, expiry and Drop tests before upper-layer suites.
2. Same source manifest on GCC, Clang and MSVC; sanitizer lifecycle execution.
3. VM, generated C, LLVM and artifact readback parity for values, error identity,
   side-effect order and cleanup counts. Unsupported capabilities are not passes.
4. Generate the language matrix twice; unchanged second-run SHA-256, then all
   toolchains consume the same binary and return `64`.
5. Current complete registered graph, CLI/LSP smoke, all 55 syntax records and
   both Python verifiers; discover test counts rather than adopting old totals.
6. Five original receiver performance scenarios; one wake per protected chain,
   no member-name dispatch and no extra wrapper allocation.

## Current Evidence

The pre-implementation Python check ran 15 tests: 14 passed, one failed.
The failing exact migration-golden comparison includes stale fixture line
positions (first observed difference: 302 versus 355). All four syntax-status
checks passed. Historical 53/53 ownership evidence is not current acceptance.

### Implementation Replay

- U-F1/U-F2 RED directly reproduced retained strong/weak counts and erased
  foreign-domain handles. Shared clear left count 2 instead of 1; Weak clear
  left 3 instead of 2. Foreign release reset kind to NONE while the origin
  domain retained the count.
- Pending lifecycle, callback isolation and AOT reentry regressions have been
  implemented. GCC's intermediate 45-case replay passed with no ignored tests.
  A subsequent three-case AOT RED reproduced stale frame pointers and a throw
  payload changed from 73 to 99 during Drop. Those three cases now pass.
- Nested break/continue exposed stale absolute destinations after quickening:
  GDB observed an old target 60 where the remapped destination was 49 and a
  continue target of 0. Label patching and quickening remapping now pass their
  direct runtime regressions.
- Native callback pending isolation had a separate 52-case RED with exactly
  three failures. The final member-result transfer repair removes the extra
  retained `stableResult`; the frozen GCC/Clang/MSVC replay now passes
  Shared/Weak54/54, member access108/108, ownership separation53/53,
  Unique/Drop20/20, CFGfinally7/7, exceptions8/8, and all five receiver
  performance scenarios. These are real exit0 runs with no ignored cases.
- Replayed syntax status verifier reports TOTAL=55, COMPLETE=55, no missing
  status/time. This confirms record consistency, not absence of runtime bugs.
- Frozen source manifest `09f5af644fe0d7e4982c03e2cbb37e67eb3f775462dd4efe66d4dcaa0cbd9454`
  contains6320 files. It preserves current dependency bytes, not merely HEAD;
  three newer unrelated LSP changes were deliberately excluded. Subsequent
  support corrections use separately recorded revisions and replays.
- Current registered GCC graph has154 entries; full build passed. The initial
  CLI/LSP smoke replay passes18 CLI cases but fails the LSP generic completion
  closed-instantiation detail assertion. This remains an external LSP gate.
- Compiler integration's pooled hash-node invalid free is proved present in
  `c95e5387`. A separate minimal hash-set storage repair has RED/GREEN evidence;
  integrated replay is pending. Sanitizer member108/108 passes, while Shared/Weak
  exposed a cold-cache null-member-address UB now guarded and awaiting replay.
- AOT derived DIRECT_VALUE validation is repaired: original C/LLVM8/8 pass.
  New abrupt parity2/2 still fail at direct-call result handoff; generated code
  reads a dense slot cleared after ownership transfer to physical VM storage.
- The formal matrix CLI regenerated all four modules twice. All eight artifact
  hashes are unchanged on the second run; GCC/Clang/MSVC consume the same files,
  print `matrix` and64, report `executed_via=binary`, and exit0.

### Defect Commits

- `7bf4cdbe`: preserve foreign-domain Shared/Weak handles.
- `29bd129c`: transfer aliased cached member results.
- `3ab8312f`: owning pending storage and reentrant Drop/VM/AOT caller cleanup.
- `ba3086a6`: exclude derived execution hints from canonical AOT frame projection.

Runtime details are maintained in
`docs/core-runtime/ownership-pending-control.md`. No overall completed status
is promoted while the parity or integrated validation gates remain open.

## Commit And Completion Rules

Commit each verified defect with regression and documentation using an exact
path list and detailed message. Check the shared index before staging. Do not
stage other Astra authors' untracked review documents or unrelated dirty files.
Link fresh evidence to the original design gates before promoting completion.
Record external failures with reproduction and ownership; continue independent
work. Remove only task-owned build/log output, never tracked test artifacts or
other sessions' caches.
