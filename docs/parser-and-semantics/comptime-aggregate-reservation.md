---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_aggregate.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_aggregate.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_executor.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_executor_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_tool_evaluator.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_tool_binding.h
  - zr_vm_parser/src/zr_vm_parser/compiler/comptime_runtime_contract.c
  - zr_vm_parser/src/zr_vm_parser/comptime_contract.c
  - tests/compileTime/test_comptime_aggregate_budget.c
  - tests/compileTime/test_comptime_frame_lifetime.c
  - tests/cmake/macro-foundation-tests.cmake
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_aggregate.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_aggregate.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_executor.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_tool_evaluator.c
plan_sources:
  - docs/plans/syntax/2026-10-08-macro-tokenstream-structured-generation-design.md
  - .codex/plans/20261008-macro-tokenstream-structured-generation.md
tests:
  - tests/compileTime/test_comptime_aggregate_budget.c
  - tests/compileTime/test_comptime_frame_lifetime.c
doc_type: module-detail
---

# Comptime Aggregate Reservation

This M1 support contract covers the paired heap/aggregate reservation made
before the evaluator constructs an object or array. It is not a live-allocation
counter or a general rollback of previously successful evaluation work.

## Accounting Boundary

The existing constructors request `sizeof(SZrObject)` heap bytes and one
aggregate slot before allocation. A rejected pair must consume neither resource,
including when the heap request fits but the aggregate request does not. The
existing frame-binding declaration already establishes that transaction policy.
Fuel spent reaching the request remains charged. The canonical runtime budget
error sets compiler error flags and the resource/request/limit diagnostic; that
error path does not itself consume the explicit diagnostic-count quota.

Successful construction remains cumulatively charged. Public evaluation of an
empty literal adds one temporary result root; typed initializers may also root
intermediate argument values. The scratch frame releases those roots when it
ends. Neither scratch cleanup nor later GC refunds successful heap or aggregate
usage. Physical allocator failure after a successful budget
commit is outside this bounded repair; its existing policy is unchanged.

## Private Module

The approved repair extracts `ct_new_object` and `ct_new_array` from the
oversized executor into `compile_time_aggregate.c/.h`. All fourteen callers
retain the same names and signatures. The module reserves the two resources
with the existing budget transaction, and on rejection reports the same failed
resource through the canonical runtime diagnostic path. Allocation and object
initialization order remain unchanged; no public header or ABI is added.

Typed CompileTool construction now shares this reservation and passes its real
initializer range, while literal wrappers retain their original zero range.
The helper rejects null compiler/state safely. This does not change member
evaluation, frame bindings, caches, typed allocation/initialization or field
construction, map-storage accounting or later operation failures.
The recursive parser source glob discovers the new module; the focused test
has an explicit target and CTest registration rather than relying on a broad
aggregate to discover it.

## Real Evaluator Fixture

`test_comptime_aggregate_budget.c` parses actual `var probe = [];` and
`var probe = {};` initializers and verifies their AST categories. It invokes
the public comptime evaluator, with no fabricated aggregate values or reset of
a failed compiler. The six cases check object and array variants of:

- Aggregate-slot rejection after a fitting heap request, preserving an existing
  seven-byte heap charge, no new managed object, null result and balanced roots.
- Heap rejection before aggregate consumption, with the same failure cleanup.
- Two successful evaluations at exact heap/aggregate limits, retaining cumulative
  usage after scratch cleanup and full GC while roots return to baseline.

Diagnostic identity, requested usage, limit, null result, managed-object count,
root balance, fuel and diagnostic usage are checked before the final heap
equality assertion. Successful borrowed results are inspected without allocation,
then cleared before another evaluation or GC. The source AST remains owned until
compiler teardown, including after Unity assertion failure.

## Typed Constructor Gate

The same fixture now has twelve cases: the preceding six literal probes plus
six probes that parse actual `zr.compile.declaration` imports and qualified
`CompileDiagnostic`/`Patch` initializers. Actual BuildFacts preparation resolves
the provider; AST shape, named arguments, descriptor role and minimum phase are
checked before public evaluation. Usage is measured from that prepared baseline,
not from an assumed zero ledger. These probes do not apply a Patch or establish
that the numeric `target: 1` identifies a real declaration.

Diagnostic rejection must leave no constructor charge. Patch rejection at its
first default array must retain the previously successful Patch header only,
not roll back all earlier work. Both preserve a seeded heap/aggregate baseline,
canonical resource diagnostics, the actual initializer range and balanced roots.
Successful repeated construction checks actual fields and four empty Patch arrays,
exact cumulative header/root/fuel deltas and non-refund after full GC. Borrowed
results are inspected through existing map buckets without allocations.

Pure-value context and pre-expansion phase controls both reject the declaration
build effect before allocation. They require `comptime.effect_violation`, not
an isolated descriptor-phase error. Typed RED-r1 native78398 naturally exits1/
build1 before tests because the initial inspector used raw pointers as derived
Objects. RED-r2 native45952 naturally exits1/build0/test3: two intended quota
failures and one incorrect ARRAY raw-tag assertion. Neither run establishes a
control-clean RED. The fixture corrects only tag-checked casts, actual ARRAY raw
tag and aggregate counting of both raw OBJECT/ARRAY headers.

Fresh RED-r3 native83423 naturally exits1/build0/test2: twelve cases, ten PASS,
two failures, zero ignored. Diagnostic heap expected7/actual463 and Patch heap
expected463/actual919 fail only the final ledger assertion after all prerequisites.
Both success/fullGC and effect controls pass. Independent fixture review accepts
56410678/CMakeAC3604EB. Production pins6945A33D/D0EE6A2A/26285FDC are unchanged for
this RED. Only the existing private reservation helper and typed caller are now
authorized to change; current executor remains88964C20. Fresh GREEN/full replay
are required; preceding six-case GREEN/full48 results do not grant new credit.

The three-file candidate936C56F3/F3C8E6F2/DAB18EE4 is frozen and independently
reviewed. Root and author reconstruct each complete preimage in RAM, matching
all2298/333/32791 bytes and hashes6945A33D/D0EE6A2A/26285FDC. No source copy or
public-header change is made. Root's first inverse reader stopped on PowerShell
pipeline syntax; its corrected assignment version naturally exits0 with all
three complete matches. This reader failure does not confer behavioral RED
credit. Focused82488 naturally exits0/build0/test0: all twelve cases PASS, zero
failures or ignored. Root verifies twelve PASS rows, one exact12/0/0 footer and
unchanged candidate/fixture/CMake pins. Existing D9025 and unchanged executor
`currentValue` C4701 warnings are retained, not waived. Full65127 naturally
exits1: both builds0, foundation47 PASS/1 TIMEOUT at unchanged
`macro_syntax_hygiene_nominal_types` (40.89s; original limit30s), aggregate8
with its two existing catalogue assertions. All121 remaining programs run:
108 PASS/13 FAIL. Comparator98749 naturally0 verifies zero exit/footer/failure/
normalized-diagnostic changes, no missing or unaccounted result. A separate
unchanged-binary/source CTest probe directly exits0: all47 Unity cases PASS,
0ignored,0.63s at the original30s property. The timeout cause is unconfirmed;
its original evidence is retained. Whole48 unchanged-input replay24286 naturally
exits0:48/48 PASS in100.27s, nominal_types1.46s, no rebuild or property changes.
This separate confirmation does not relabel65127 or the single-test probe.

## Evidence

Native80224 exits1/build1 because the first test draft used a nonexistent value
initialization API. No test executes and no behavioral RED credit is assigned.
Root corrects only three calls to existing `ZrCore_Value_ResetAsNull`.

Corrected RED-r2 native99959 naturally exits1/build0/test2: six tests, four PASS,
two failures, zero ignored. Both aggregate-slot cases fail only the final heap
assertion, expected7/actual463, after every prerequisite above passes. Heap
rejection and cumulative success/GC controls pass. Production executor99ECACD7
and support643156D2 are unchanged for this RED. Independent review finds no
false-RED or ownership defect in fixture861E6C39/CMake96C7E457.

The frozen repair uses executor88964C20, private module6945A33D and header
D0EE6A2A; support643156D2 stays unchanged. Root and author independently verify
that two unique RAM inverse fragments restore all212786 prelease bytes and
SHA99ECACD7. No filesystem source copy is created. Independent review finds no
actionable defect in the transaction, diagnostics or unchanged fourteen callers.

Fresh GREEN native30938 naturally exits0/build0/test0: all six cases PASS,
zero failures or ignored. New module compilation succeeds; existing compiler
and foreign dependency warnings are retained, not waived. Full25248 naturally
exits1: original126 build0, foundation build0 and48/48 PASS, aggregate exit8,
remaining121 inventory108 PASS/13 FAIL. Comparator3626 naturally exits0 with
zero exit/footer/failure-detail changes against BASE-final-r1. The preceding
47-test BASE replay remains separate evidence. Root candidate pins are unchanged;
this is not a whole-checkout freeze. Fresh WSL availability still returns
`Wsl/Service/E_ACCESSDENIED`; no GCC/Clang execution is credited. M1 and original
pipeline failures remain open. See the
[validation ledger](../testing-and-validation/macro-foundation-validation.md).
