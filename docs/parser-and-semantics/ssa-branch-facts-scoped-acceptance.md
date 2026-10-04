---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_branch_facts.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_branch_facts.c
  - tests/parser/test_ssa_branch_range_null.c
  - tests/cmake/ssa-branch-range-null-tests.cmake
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_branch_facts.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_branch_facts.c
plan_sources:
  - docs/plans/ssa/02-automatic-optimization/02-gvn-range.md
tests:
  - tests/parser/test_ssa_branch_range_null.c
doc_type: testing-guide
---

# Branch facts scoped acceptance

## Actual evidence sequence

All report directories below are under
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/`.

| Stage | Report directory | Actual outcome |
| --- | --- | --- |
| Missing-feature RED against the original stub | `llvm-branch-red-v4` | 36 cases, 33 failures; feature absent |
| Expanded producer matrix with invalid common fixture | `llvm-branch-green-v7` | 53 cases, 49 failures; nearly all positives rejected by Analyze before checking facts |
| Repaired fixture with nonzero function identity | `llvm-branch-green-v9` | 53 cases, 0 failures; repeated run also 53 cases, 0 failures |

The v7 failure remains part of the evidence. The shared fixture constructor and
independent 128-block DAG constructor left `functionToken` at zero. Core requires
a nonzero token before SSA validation. Both constructors now set it to 1, and
the rejection diagnostic prints code, block, instruction, source, expected and
actual values. Neither the verifier nor the original fact assertions were
weakened. The producer stayed unchanged during this fixture repair.

## Passing receipt and exact scope

The Root receipt is
`llvm-branch-green-v9/Root-receipt.json`, SHA256
`e6fda7fc878d719608257bf1324e435a977d4b930f2855bb4c28c7529734a7c8`.
It records successful native Windows Clang/LLD producer and fixture compiles,
link, runtime and repeat runtime, all with natural exit code 0. Whole elapsed
time was 8.1352159 seconds. Both runtime logs end with
`branch range/null: 53 cases, 0 failures`; no UBSan diagnostics were observed.
Expected rejected-input diagnostics are present for negative tests.

UBSan instruments the new producer and fixture translation units. Nineteen
current Core objects, compiled and individually accepted earlier, were reused
without sanitizer instrumentation. Their source/dependency and product hashes
were checked against the current bytes. This is not a whole Core sanitizer run.
The native process scope was empty on completion with no forced cleanup actions.

Accepted fixture coverage includes six comparison selectors, reversed operands,
INT64 extrema, opaque representation identities, explicit signed/null witnesses,
witness copying, invalid graph/input rejection, stale/rebuilt results, PHI
joins, parallel successor occurrences, reachable/unreachable cycles,
nested/contradictory paths, and a 128-block DAG.

An independent static review found no concrete unsound fact under the documented
caller-proof contract. Dedicated call, throwing-result, SUSPEND and allocation
failure tests are absent. A default `overflowed = false` is not an independent
arithmetic no-overflow proof. Consumers must use the explicit fact domain and
proof contract, rather than treating that bit alone as sufficient evidence.

## Registration and remaining gates

`tests/cmake/ssa-branch-range-null-tests.cmake` defines a standalone executable
with the exact 19 Core source closure, producer and fixture used by the native
validation. Existing fragments have inline Core lists but no shared reusable
list with that complete closure; the new fragment therefore names its own
sources. It adds target `zr_vm_ssa_branch_range_null_test` and CTest
`ssa_branch_range_null`. Parent inclusion and CMake configure/build validation
are not established by the direct native receipt.

The full 02.02 milestone remains open: loop widening/narrowing, consumer check
elimination and proof remarks are not accepted by this producer fixture. Runtime
differential validation, pooled-format proof production, allocation fault
injection, dedicated call/exception/suspension tests, Linux toolchains, MSVC,
ASan, native 32-bit and the full 47-task plan are separate outstanding gates.
