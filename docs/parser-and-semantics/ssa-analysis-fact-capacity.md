---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_ranges.c
  - zr_vm_parser/include/zr_vm_parser/exec_ir_ranges.h
  - tests/parser/test_ssa_analysis_fact_capacity.c
  - tests/cmake/ssa-analysis-fact-capacity-tests.cmake
  - tests/CMakeLists.txt
  - tests/parser/test_ssa_gvn_range.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_ranges.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/01-core-model.md
  - docs/plans/ssa/02-automatic-optimization/02-gvn-range.md
  - "user: 2026-10-03 continue SSA tasks; finite local capacity repair"
tests:
  - tests/parser/test_ssa_analysis_fact_capacity.c
  - tests/cmake/ssa-analysis-fact-capacity-tests.cmake
  - tests/parser/test_ssa_gvn_range.c
  - tests/acceptance/ssa-analysis-fact-capacity.md
doc_type: module-detail
status: finite-direct-checkout-cmake-ubsan-accepted
---

# Analysis fact append capacity

This document records the finite checked fact append repair, historical validation
and current direct-checkout CMake/UBSan acceptance. Root V31 and its independent
audit accept the capacity fixture within nine configured finite SSA tests.
Full 02.02 and the 47-leaf milestone remain OPEN.

## Ownership and logical state

AnalysisFacts owns its range, shape and nullability arrays. Init zeros all storage fields and sets generation1. Free releases the three arrays and clears the object. Explicit generation invalidation changes generation and resets all logical counts while retaining arrays/capacities. This finite repair retains those existing rules, public APIs and fact-semantic validation. It does not change generation wrapping, negative enum admission or optimizer proof semantics.

## Count and byte checks

Two static helpers support the existing three append consumers. `fact_append_required` rejects count greater than capacity and terminal UInt32 count before adding one. `fact_capacity_bytes` rejects zero element size and capacity greater than `SIZE_MAX / elementSize` before multiplying. Outputs are written only on success. Casts from UInt32 to size_t are exact on the supported 32/64-bit hosts; actual runtime evidence must state host/fact sizes.

Shared reserve checks parameter addresses, nonzero-capacity backing, and current capacity byte representability before the no-growth success path. Growth retains initial8, doubling and the original UInt32 doubling guard. Chosen byte size is checked before realloc. Geometric growth whose bytes cannot be represented returns false, with no exact-size retry. Non-null owned storage with capacity0 remains legal for reserve; it may be reallocated normally.

The realloc result is temporary. Failure leaves old pointer/capacity intact. Each Add copies the validated typed input before reserve, computes a checked required count, writes one record at the old count after successful reserve, and publishes that required count. Capturing input makes an existing live element a valid append input across growth; its old address is never read after realloc.

False leaves the complete header, prior backing and caller input unchanged. The Bool failure API is retained; no diagnostics, public helper, allocator callback or compatibility production branch is introduced. Allocation OOM rollback is supported by the code's publication order but has not been dynamically verified.

## Dedicated fixture

`test_ssa_analysis_fact_capacity.c` includes actual ranges.c once to access static numeric helpers. The target must not compile that source a second time. It links only its single C translation unit and standard C allocation routines; parser/core/common headers provide existing types. Compile-time fixture flags select pristine or unchecked extraction epochs only for future D validation; they do not change production behavior or become default product flags.

The default checked fixture contains 17 cases:

- Three type-specific growth cases append exactly17 real records, crossing capacities at1,9,17, comparing every old/new semantic field and const input, and preserving other containers/generation.
- Three rollback cases retain one real record in every owned array and reject each existing semantic-invalid input with complete header/live-byte/input comparisons.
- Three generation/latest cases use duplicate ValueId records, newest shape/null lookup, explicit generation2 invalidation, retained pointer/capacity/content checks, stale-input rejection, and generation2 replacement. Range uses actual storage checks and existing bounds proof controls.
- Private required/byte/native-size cases use scalars and output sentinels only. No giant allocation or backing access is performed. Candidate NULL-output probes are excluded from unchecked extraction.
- Private reserve storage and preflight cases verify fast-path storage/bytes and NULL-parameter rejection without allocation or backing access.
- Three candidate-only real capacity8 input-alias growth cases compare nine records from independent snapshots. These are never executed on the old implementation.

Checks remain active under NDEBUG. Every real owning-container fixture returns through Free even after a failed assertion; main prints per-case results after cleanup and returns failure if any case failed.

## Historical reported accepted finite evidence

| Gate | Result | Root receipt SHA256 |
| --- | --- | --- |
| GNU17 | native64 exit0;17 cases/0 | e09f4d048b5fce7360260204f3580c2038fcd7b55d264a990226ad3322f53258 |
| MSVC17 | native64 exit0;17 custom CASE/RESULT failures0;134 current pins | 2a76fbcf4516da9ec4329e9281bf2d034a6c411d4bddd0f5678af970d0859932 |
| Pristine_safe_RED | exit1;10 cases/1 reserve_storage expected failure | 65bd5fa20c086fab4a3710007111035bac5de3c4f40993c9526b0b97cf7fc404 |
| Extraction_numeric_RED | exit1;3 cases/2 numeric failures; distinct source epoch | f985a0641497e8b7e3c48a25d0fbdeaa42003d07ba0d9fe53b22ce7f3096af6a |
| GVN24_twoCTest | two products completed24 bound source calls each; two real CTest each1/1 | 95718a5c5d1e97e0edd653b8be9b4e76923ea73d7abf18d02c131573b13a20ea |
| Mini_actual17_CTest | 17 ordered custom cases/0; real configured CTest exactly1/1;264 current pins | 411e2c168ae837baa9c72f2a636d0edddc441bc7964241c6e93e832eb7a52136 |
| Clang_compile_only | actual C11 ASan/UBSan compile/link;93 MD/20 selected SDK link inputs/151 current pins; compile-only epoch; runtime accepted separately below | 57f1b35df283c8a4c24241e5c5e7579a272359eac401530f3f69d7cba5d662e9 |
| Mini_build_only | 3 products/95 actual MD paths/13 link inputs; build-only | e5c157651eb3ec302a6e51022972c6fd097c0a55f27d8c685f007149cef0acc8 |
| Clang_actual17_SAN_runtime | native64 natural0;17 custom cases/0; default sanitizer options/no diagnostics; scoped fixture TU only | 4823d4b1fdad8965efbc09fb751e07a361f2b3a63955bd49733306abccdeb812 |
| Root_personal_current_four_Clang17_repeat | scoped SAN17/0; full current28/E4 pre/post byte equality; frozen actual product, no fresh E compile | af3c0c75b710ca28832cb9d0e40ee916dd29f55de277f0cf338de794714d06cc |

## Earlier E standalone UBSan evidence

On 2026-10-04 Root freshly compiled the actual current E fixture and included production ranges.c using Windows Clang C11 plus LLD with UBSan only (`-fsanitize=undefined`, no recovery). Build products are under `E:/cargo-targets/zr_vm/build/ssa-20261004-01a0fe2b/llvm-capacity-san-ubsan-v5`; reports are under the matching E reports directory. This follows the user's new build-directory instruction and does not borrow old D binaries.

Root receipt `llvm-capacity-san-ubsan-v5/Root-receipt.json` has SHA256 `7da6cad37326e67971e319286e0957222bf5e1431a9ba6f4654284767618511b`. Compile, link, fixture run and Root repeat each exited naturally with status0, closed/reaped handles, zero active/terminated job processes and no cleanup actions or errors. Recorded durations are 1.3000034, 0.4693074, 0.5500994 and 0.1735969 seconds; whole invocation4.3089978 seconds. Both raw runtime logs contain the same ordered17 CASE PASS lines, RESULT cases=17 failures=0, layout size_t8/range40/shape24/null16 and three native boundary lines, with no additional diagnostics.

Independent artifact review re-read actual raw logs, source/dependency/tool/product files and statically reviewed Root native Job v2 controls without executing or importing the helper. All434 pin records across220 unique paths matched current SHA256/length, including187 actual dependency paths after normalizing the ranges.c relative include. The linked executable is an AMD64 PE of210432 bytes, SHA256 `108778fee5f2648fa284c4d2d7aadeb84a2d499209e59f76baedfe706aec5b40`; actual link output names the standalone UBSan runtime. The independent receipt is `E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/independent-capacity-ubsan-current-v1.json`, SHA256 `487632082ba84f2d7273840d1a0f7a9a991b51cc2bb4f049e98188f0b21add13`.

This establishes fresh UBSan validation of the single capacity fixture translation unit. It does not establish fresh ASan, whole Core sanitizer coverage, native32, dynamic allocation failure, full parent configuration or all47 SSA completion. Historical finite functional acceptance remains reported in the table above; the old D receipts/products are currently absent and have not been freshly reverified.

The earlier six-file staging, read-only `.git` permission and index-lock concerns
describe a prior session state. The external user commit `b3b42d53` absorbed the
finite capacity source; it is not a Root-authored commit. Root's later preflight
found the October 3 lock absent, without Root deleting it, and effective Git
permissions now allow ordinary index operations and finite commits. No current
integration block is inferred from that historical lock or permission state.
This document reviewer performed no Git action.

## Historical copied Mini CMake and CTest evidence

Earlier on 2026-10-04 Root configured, built and ran the capacity fragment in a
copied Mini project under the task's `current-capacity-cmake` directories. That
run used forty copied project inputs: thirty-nine fixture dependencies plus the
then-current capacity CMake fragment. The audit recorded matching original and
copied bytes at that time. This is historical copied-input evidence, not current
direct-checkout acceptance. Those copied inputs have since been removed; this
document does not require or re-read them. The current gate is V31 below.

Installed CMake 3.31.6 and Ninja produced Clang 19.1.5 identification and a
successful executable ABI try_compile. That historical target compiled one
fixture TU including production ranges.c once and linked UBSan through Clang/LLD.
The audit checked 184 actual Ninja dependencies and the compiler cache type
STRING. Those observations belong to the copied Mini run.

Configure, build and CTest each exited naturally with status 0, zero active/terminated owned Job members, reaped/closed handles and no cleanup actions or errors. Durations were 3.4923958, 2.4032708 and 0.9888743 seconds. The raw verbose CTest log reports exactly 1/1 named `ssa_analysis_fact_capacity`, seventeen ordered CASE PASS lines and `RESULT cases=17 failures=0`, layout size_t 8/range 40/shape 24/null 16 and three native boundary lines. No UBSan diagnostic appears. The fresh executable is an AMD64 PE of 210432 bytes, SHA256 `4484e5cbdd238c4ceba86abf75ba851052c22423b6eff4f0236efd968a89b9a0`.

Receipt history is preserved precisely. The original controller `current-capacity-cmake/Root-receipt.json`, SHA256 `7383771344aad6c305a3de506478ea9b3b691fbd1be12acc8318e0565e65f70d`, remains FAILED: after all three completed successful steps it asserted a guessed compiler cache type FILEPATH rather than the actual STRING. Read-only adoption audit v2, SHA256 `a0ad9ff7044e35b050e78d2f99a1eae662b9aa2b4202b021843a327a6d68b511`, also remains FAILED after two successful natural Ninja inspection commands because it resolved their relative dependency paths incorrectly. Root's separate read-only adoption audit v3 resolves paths against the actual build directory and accepts only this current Mini scope; its SHA256 is `8f43b128bec9029b5b8df13dc23632fd6a6fdf5329ba48cfea43c0089da8c307`. No fixture rerun or rewrite of the failed receipts was used for adoption.

The historical independent review recorded 309 matching pin records across
271 unique paths, forty copied project inputs and 184 actual Ninja dependencies.
Its report is
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/independent-capacity-cmake-current-v1.json`,
SHA256 `397df45c2b5edb0e200e12af326c757c24ec3f88eeeb713a75b5eb5877bbcb6f`.
This is a record of that earlier audit, not a new check of removed copied inputs.

This established the historical fragment Mini CTest scope with UBSan. It adds no
ASan, full repository configure/build, MSVC compiler runtime, Linux, native32,
dynamic OOM, forced allocation movement, GVN compatibility rerun or all 47 SSA
credit. Earlier integration and Git restrictions are historical as described
above; they do not describe the current finite direct-checkout gate.

## Current direct-checkout CMake acceptance

Root receipt:
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/direct-ssa-cmake-v31/Root-receipt.json`.
SHA256: `17c71e728b1d84f679bfa7d2d60f2f46793a3c76ab6430782509d244b39e88e2`;
258,995 bytes, acceptance true. The actual source directory was
`E:/Git/zr_vm/tests/cmake/ssa-direct-validation`. The capacity fragment resolves
the real repository root relative to its own directory and reads the fixture and headers
directly from `E:/Git/zr_vm`; no copied project inputs were used.

Configure, build, CTest, Ninja dependency inspection and Ninja command inspection
all naturally exited zero. All 138 project C compile edges across the nine
configured finite targets used UBSan and UNDEBUG. The capacity target had one
fixture C translation unit, embedding production `exec_ir_ranges.c` exactly once.
The actual CTest log contains seventeen named CASE PASS rows,
`RESULT cases=17 failures=0`, and a PASS for `ssa_analysis_fact_capacity`.
The complete configured gate passed exactly 9/9 tests with zero failures.

Current capacity pins match the receipt:

| Input | SHA256 |
| --- | --- |
| `tests/parser/test_ssa_analysis_fact_capacity.c` | `061575a077e817b3f971b6178d3f5eee9b2686208e1b0756a32e4130d5c8af7d` |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_ranges.c` | `3ed906ef8bc69d9c3b8605ed6ea7e4c3f134d3c1c1afa8e0c4f56bd3567449e3` |
| `tests/cmake/ssa-analysis-fact-capacity-tests.cmake` | `491f195092ee789ebec472a4ad9da049c380b3ca703ae98ca8fe87befb7de51a` |

Independent audit:
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/independent-direct-cmake-v31-current-v1.json`.
SHA256: `506ad7a314b4b37af3a75af6b746e23298742f73b9079d8d9da9b1a4bb78802e`;
12,198 bytes, acceptance true. All 40 checks passed, including current pins,
138 valid dependency blocks, all 138 UBSan/UNDEBUG compile edges and exact
nine-test CTest agreement. This independently adopts the finite configured gate,
including capacity17. It establishes neither full parent/repository acceptance
nor fresh ASan, Linux, native32 or dynamic allocation-failure evidence.
## Fresh failed gates retained

- WSL GNU entry was rejected by the service with `Wsl/Service/E_ACCESSDENIED`; no Linux compiler or fixture run occurred. Root receipt SHA256 `a77effbd64b0db9dd6e2c49b9ae73b95def9af9bd3f2b75c211708eb6e97edb8` records failure.
- Fresh MSVC's primary compile/link process exited0, but the owned descendants did not exit naturally and required `TERMINATE_OWN_JOB`. Whole gate remains FAILED; no fresh fixture runtime credit. Root receipt SHA256 `ce0fc42630da1a04d1ede0379445c7dc78c6ebc1d427f1f0441c3aa1ed46d3b2`.
- Fresh Windows Clang ASan/UBSan v4 compiled and linked, then failed at runtime startup with interception_win unhandled-instruction diagnostics and natural status3221225477 (0xC0000005), before any fixture PASS output. This is an actual failed ASan runtime gate, not a passing fixture or an ignored diagnostic. Root receipt SHA256 `99796ad42e619f397df07f2b5afdb530a92c60f70c20491a49a12ab98d8adc42`.

## Historical failed optional repeat retained

The first configured Mini17/1-of-1 adoption411e remains historical acceptance.
The later optional repeat wholeFAILED auditf82f is preserved separately:
Root129.554, CTestchild343355 timeout31.5 with TERM/reap/EMPTY, log17PASS without
a CTest summary; WSL46348 timeout/TERM/reap and Linuxcontroller343353 UNKNOWN,
with incomplete native postflight. Partial logs do not establish runtime/CTest
acceptance. Mini alias236568 is limited read-subproduct adoption; its failed
outer wrapper is not promoted. These historical attempts do not alter V31.

## Limits retained

The fixture uses standard allocation. No controlled allocation failure or hook was added; allocation-NULL rollback follows temporary realloc/publication order by source inspection only. Native32 and dynamic OOM remain unexecuted. Pristine safe reserve RED and separately extracted numeric RED do not execute unsafe old terminal-count append or alias paths. Production helpers are static in ranges.c, not a new private arithmetic header.

Historical GVN compatibility covers 24 actual bound test-function calls per
product and two real 1-of-1 CTest runs; it does not prove facts consumption.
The current 17-case capacity fixture covers invalid-input/count/backing/size
checks, genuine growth, live-input alias for all three arrays and generation/latest
controls. Earlier borrowed plain Core objects do not establish whole-Core
sanitizer coverage. V31 separately establishes current direct-checkout compilation
for its nine finite targets. Full parent/repository acceptance, 107-library
acceptance, the 47-leaf milestone and full 01.01/02.02 remain OPEN.
