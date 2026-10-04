---
doc_type: testing-guide
status: focused-validated-integration-pending
plan: .codex/plans/20261004-ssa-branch-arithmetic-constraints.md
tests:
  - tests/parser/test_ssa_branch_arithmetic_constraints.c
  - tests/parser/test_ssa_branch_range_null.c
---

# Branch arithmetic and constraints scoped acceptance

Root supplied the actual execution results below. This agent wrote the
implementation and documentation but did not execute compilers, jobs or Git.

| Stage | Actual outcome | Receipt/audit SHA256 |
| --- | --- | --- |
| Preimplementation RED | 40 cases, 31 failures, natural exit 1; `add-one` and `lt-true` fail semantic bounds assertions, no verifier/Analyze/freshness precondition failures | V14 independent counterexample audit `a45eb0d8366c943ebfd1d628f0afc1c73b5169d20249880ee22565b9e76730d3` |
| V17 new finite slice | 40 cases, 0 failures; repeated run PASS, natural exit 0 | `6594ab37c0214b8ebe5378d02a4d4aae4bed82f9256cc52e72407c0c7b56cdf0` |
| V18 original regression on extended producer | 53 cases, 0 failures; repeated run PASS, natural exit 0 | `0e27e0bf7bfc475e61b279b75e43096ceceeec4211f1a57a40dda399546a3450` |

V17 receipt is `reports/task/llvm-branch-green-v17/Root-receipt.json` in the
Root report bundle. V18 receipt is identified by the supplied SHA256; this
record does not guess a directory name. V17/V18 validation durations were
6.3969307s and 5.1509873s respectively, not performance measurements. Producer
and fixture were UBSan-instrumented; 19 Core support translation units remained
plain. Root reported current source/tool/resource pins, actual dependency
records, natural process completion, EMPTY jobs and no cleanup actions.

The RED V13 controller remains FAILED because the planned case marker
`add_0_7_plus_1` differed from actual `add-one`. Root's V14 independent audit
verified current closure, logs and products as expected semantic RED; no failed
controller receipt is rewritten as success.

## Frozen implementation under these focused results

| File | SHA256 |
| --- | --- |
| `analysis/exec_ir_branch_facts.c` | `b94f8cd91c7cb7b36486e65c0e9931caea30316c15dd54d56d091d1fe2fcc229` |
| `analysis/exec_ir_branch_intervals.h` | `d53bc6a6a4ea031efd32aeaa599c920fa55ec0beeb6d4257a66e091234c3be84` |
| `exec_ir_branch_facts.h` | `4d9d9f5deb650b3052bd91b84e9c1dbf92302f3f4cf23054a44aaa2772cec915` |
| `test_ssa_branch_arithmetic_constraints.c` | `f85ddff439ab1edfb353bc0f4bd56baad67b3867401c996d19dcef724d3402ad` |
| `ssa-branch-arithmetic-constraints-tests.cmake` | `ad58e3b55259882165e59a5b697e62e89dc017740cb48f6f4b408718bcdaebe8` |
| Original `test_ssa_branch_range_null.c`, unchanged | `0088eb944f33cc14d7189ca9e140174ad6831a2a560a391564c1f8ea257e0acb` |

## Limits and remaining work

The new 40-case fragment is not yet included in the parent test integration.
The historical foundation acceptance remains at
`ssa-branch-facts-scoped-acceptance.md`; Root also saved its original production
snapshot in E: tmp `branch-facts-foundation-53` for separate finite integration.
The old `tests/acceptance/ssa-branch-range-null-analysis.md` was not modified.

Independent static interval soundness review found no defect, but dedicated
same-value alias, half-bounded and joined-poison fixtures remain coverage gaps.
Loop widening/narrowing, production representation witnesses, check-carrier
design and actual check deletion, memory GVN and proof remarks remain planned.
These results do not close 02.02 or the full 47-leaf release matrix, and do not
claim runtime/FFI/network/capability/HOTPATCH validation.

No Git staging or commit was performed. Root reported that the explicitly
authorized exact lock deletion still failed AccessDenied; lock/index remain
preserved. This record adds no new cleanup action.
