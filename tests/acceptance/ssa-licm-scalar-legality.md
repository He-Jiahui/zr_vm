---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_loops.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_licm.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_licm_scalar.h
  - tests/parser/test_ssa_licm_scalar_legality.c
  - tests/parser/test_ssa_licm_scalar_context.inc
  - tests/parser/test_ssa_loops_specialization.c
  - tests/cmake/ssa-licm-scalar-legality.cmake
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_licm.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_licm_scalar.h
plan_sources:
  - .codex/plans/20261004-ssa-licm-legality.md
  - docs/plans/ssa/02-automatic-optimization/05-loops-specialization.md
tests:
  - tests/parser/test_ssa_licm_scalar_legality.c
  - tests/parser/test_ssa_licm_scalar_context.inc
  - tests/parser/test_ssa_loops_specialization.c
doc_type: testing-guide
---

# Focused scalar LICM legality acceptance

## Current result

Root accepted the actual V16 compile, link, runtime, and repeated runtime, all
exit 0. The separate legacy pooled-2 case returned baseline 18 and optimized 18
with no identity rewrite. All 29 semantic-context cases passed both runs.
The unchanged unmapped positive cases include same-kind integer identities,
safe signed ADD/SUB/MUL/DIV, and safe unsigned DIV; the DIV cases preserve the
schema's MAY_THROW flag and pass full Core verification after staged effect
reconstruction. Both mapped cases preserve borrowed maps and exact instruction
and block order, with Oracle equality and logical checkpoint validation.

The authoritative Root receipt is
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/llvm-licm-green-v16/Root-receipt.json`.

| Evidence | SHA256 |
| --- | --- |
| Root V16 receipt | `e5da76c7767d80a9d628dcd74de8729610e6c63306ddf14248745c5fe5a395ab` |
| Actual runtime log | `868cbfee1460996ae97807e1f0c00b8251e9d1f14023443e1deee34d3228e397` |
| Root repeat log | `868cbfee1460996ae97807e1f0c00b8251e9d1f14023443e1deee34d3228e397` |

The receipt records 6.1750171 seconds for the full V16 gate. Root verified actual
dependency files, source identities, compiler/resource identities, and object/
linked product identities before and after runtime. Processes exited naturally,
with the monitored job empty and reaped, and no controller intervention.

## Case requirements

The fixture executes real Core Oracle code before and after the optimization.
It checks value kind/payload bits, event kind/source/operand order, failure code
and exact source/block location, actual target placement, transformation counts,
and full Core verification. Semantic mismatches explicitly return failure.

| Coverage | Required behavior |
| --- | --- |
| Pool slot 1 contains signed 1 | Context identity rewrites and preserves signed result |
| Pool slot 1 contains signed 2 | Legacy/context APIs preserve multiplication and result |
| Absent or different-function context | No numeric proof or identity rewrite |
| Signed/unsigned safe arithmetic | Required positive movement with Oracle equality |
| DIV zero and signed MIN/-1 | Stay in body; exact failure diagnostic preserved |
| Nonzero branch slot holding false | Trapping body remains unexecuted, including DIV and CONVERT |
| Signed/unsigned overflow/underflow | Stay in body; checked failure preserved |
| FLOAT negative zero | No identity rewrite; result kind and bits preserved |
| Mixed signed/unsigned identity | No representation-changing COPY rewrite |
| UNDEFINED base | No rewrite; exact INVALID_VALUE code/source/block preserved |
| NaN/out-of-range CONVERT | No speculation; conversion failure timing preserved |
| Borrowed logical/GC maps | No hoist or map/site mutation; actual result/events preserved |

## Repair evidence retained

The genuine old-API counterexample returned 18 before optimization and 9 after,
with one incorrect reduction. Root's report
`licm-old-api-assertion-counterexample-v6.json` has SHA256
`231412e9b70f43c64b7ee113ee118b3bf14ae458ef3a80c6f84c710c5bbf3881`.

V12 compiled/linked, passed the first four context cases, then failed full
verification of safe DIV with INVALID_VALUE: old effect-only incoming rows lost
classification after their ranges were cleared. The helper now retires those
rows and remaps preserved value-phi starts before resynthesis.

V15 passed 28/29 cases, including both safe DIV and mapped negatives. The
undefined-base fixture incorrectly expected ARITHMETIC_ERROR. Core rejects an
UNDEFINED operand with INVALID_VALUE during operand collection, before
arithmetic. The corrected named expectation preserves all equality, diagnostic,
source/block, event and no-transformation checks; production was unchanged.

## Acceptance limits and outstanding gates

V16 is a Windows clang-cl gate with a fresh UBSan fixture, five individually
accepted current UBSan Parser objects, and 19 plain Core objects. It supplies
no ASan, whole-Core UBSan, Linux, recurrence optimization, or allocation-failure
injection evidence. Clone staging is a source-level guarantee for the new
destructive DIV step; no injected allocation failure has been executed.

The modified existing loops positive fixture passed the V20 eight-case regression
described below. Root added the isolated CMake fragment inclusion, but broader
genuine CMake configuration/registration remains unvalidated. Full
02.05 acceptance, owned/COW map-aware hoisting, and compiler semantic-pool
production/pass-manager integration remain open.

## Existing loops/profile/specialization regression

Root's actual V20 gate compiled the existing loops test and profile TU with
UBSan, reused five individually accepted current UBSan Parser objects plus 19
plain Core objects, linked and executed the assertion-based main. Compile,
link and runtime all exited naturally with code 0; the empty runtime log has
no assertion failures or sanitizer diagnostics. The eight named cases cover:

1. Loop forest and trip count.
2. Nontrapping CONSTANT hoisting.
3. Typed binding-row instruction-ID remapping.
4. Zero-trip and throwing-instruction preservation.
5. Positive context-aware checked identity multiply.
6. Profile key import and mismatch.
7. Profile hash including typed binding payload.
8. Specialization budget and cooldown.

Receipt:
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/llvm-loops-regression-v20/Root-receipt.json`,
SHA256 `0c1f6d6d40cad060c0f141c9c8fa285be292103712d59e3d81494f7517add157`,
137833 bytes; whole elapsed 6.7047965 seconds.

The separate read-only audit
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/licm-owned-loops-v20-audit.json`,
SHA256 `cda0c16caaa231ebecbf2a705029bf8995adcdb77b6198df0bd0921a5dbc2c66`,
accepted 590 pinned records across 315 files, all 26 actual dependency files and
232 unique dependencies, exact current main-call names and assertion-enabled
compilation, and natural/reaped/empty/no-intervention lifecycle evidence. It
did not invoke a compiler or runtime. V19 failed on a controller report-path
error before compiling; that immutable failure is not a test failure.

V20 supplements focused V16 acceptance without expanding its platform/sanitizer,
full-milestone, map-aware hoisting, or allocation-failure proof limits.
