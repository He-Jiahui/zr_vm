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
doc_type: testing-guide
status: historical-functional-acceptance-reported-current-ubsan-and-mini-ctest-verified-integration-and-commit-pending
---

# Analysis fact capacity finite acceptance evidence

Final finite six formal paths: ranges.c, dedicated17-case fixture, fragment, one parent include, module document and this acceptance document. Public API/generation semantics remain unchanged. Historical functional acceptance is reported below; fresh current E UBSan and fragment Mini CTest evidence is independently verified. Both documents exist in E; six-file integration and commit remain pending.

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

## Current E validation and integration

On 2026-10-04 Root freshly compiled the actual current E fixture and included production ranges.c using Windows Clang C11 plus LLD with UBSan only (`-fsanitize=undefined`, no recovery). Build products are under `E:/cargo-targets/zr_vm/build/ssa-20261004-01a0fe2b/llvm-capacity-san-ubsan-v5`; reports are under the matching E reports directory. This follows the user's new build-directory instruction and does not borrow old D binaries.

Root receipt `llvm-capacity-san-ubsan-v5/Root-receipt.json` has SHA256 `7da6cad37326e67971e319286e0957222bf5e1431a9ba6f4654284767618511b`. Compile, link, fixture run and Root repeat each exited naturally with status0, closed/reaped handles, zero active/terminated job processes and no cleanup actions or errors. Recorded durations are 1.3000034, 0.4693074, 0.5500994 and 0.1735969 seconds; whole invocation4.3089978 seconds. Both raw runtime logs contain the same ordered17 CASE PASS lines, RESULT cases=17 failures=0, layout size_t8/range40/shape24/null16 and three native boundary lines, with no additional diagnostics.

Independent artifact review re-read actual raw logs, source/dependency/tool/product files and statically reviewed Root native Job v2 controls without executing or importing the helper. All434 pin records across220 unique paths matched current SHA256/length, including187 actual dependency paths after normalizing the ranges.c relative include. The linked executable is an AMD64 PE of210432 bytes, SHA256 `108778fee5f2648fa284c4d2d7aadeb84a2d499209e59f76baedfe706aec5b40`; actual link output names the standalone UBSan runtime. The independent receipt is `E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/independent-capacity-ubsan-current-v1.json`, SHA256 `487632082ba84f2d7273840d1a0f7a9a991b51cc2bb4f049e98188f0b21add13`.

This establishes fresh UBSan validation of the single capacity fixture translation unit. It does not establish fresh ASan, whole Core sanitizer coverage, native32, dynamic allocation failure, full parent configuration or all47 SSA completion. Historical finite functional acceptance remains reported in the table above; the old D receipts/products are currently absent and have not been freshly reverified.

The two owned documents now exist in E. Six-file integration and commit remain pending: Root reports the current index is empty, the index-lock creator is unknown and the active profile permits reading rather than writing .git. The prior claim of a one-include cached parent projection described a historical freeze and is not current index evidence. Preserve the mixed parent file and restrict any eventual staging to the capacity include; do not add a duplicate include. No Git mutation was performed by this review.

## Fresh current Mini CMake and CTest evidence

On 2026-10-04 Root genuinely configured, built and ran the current capacity test fragment in a new E Mini project. Its real source directory is `E:/cargo-targets/zr_vm/tmp/ssa-20261004-01a0fe2b/current-capacity-cmake-source`; build and reports are under the matching `current-capacity-cmake` E task directories. The snapshot preserves forty current project files: thirty-nine actual fixture dependencies plus the unchanged `tests/cmake/ssa-analysis-fact-capacity-tests.cmake`. Original E files and snapshot bytes match. This is a configured current fragment build, not a full repository checkout configuration.

Installed CMake 3.31.6 and Ninja produced a genuine Clang 19.1.5 compiler identification and successful executable ABI try_compile. The target compiled one fixture C translation unit, which includes production ranges.c once. System headers use the installed LLVM resource include before VC/SDK headers; target project includes come from the actual fragment. The generated driver link uses Clang plus LLD with UBSan only. Actual Ninja dependency output contains 184 valid dependencies; current project and external header pins were independently checked. Generated cache reports the compiler with type STRING, which is valid.

Configure, build and CTest each exited naturally with status 0, zero active/terminated owned Job members, reaped/closed handles and no cleanup actions or errors. Durations were 3.4923958, 2.4032708 and 0.9888743 seconds. The raw verbose CTest log reports exactly 1/1 named `ssa_analysis_fact_capacity`, seventeen ordered CASE PASS lines and `RESULT cases=17 failures=0`, layout size_t 8/range 40/shape 24/null 16 and three native boundary lines. No UBSan diagnostic appears. The fresh executable is an AMD64 PE of 210432 bytes, SHA256 `4484e5cbdd238c4ceba86abf75ba851052c22423b6eff4f0236efd968a89b9a0`.

Receipt history is preserved precisely. The original controller `current-capacity-cmake/Root-receipt.json`, SHA256 `7383771344aad6c305a3de506478ea9b3b691fbd1be12acc8318e0565e65f70d`, remains FAILED: after all three completed successful steps it asserted a guessed compiler cache type FILEPATH rather than the actual STRING. Read-only adoption audit v2, SHA256 `a0ad9ff7044e35b050e78d2f99a1eae662b9aa2b4202b021843a327a6d68b511`, also remains FAILED after two successful natural Ninja inspection commands because it resolved their relative dependency paths incorrectly. Root's separate read-only adoption audit v3 resolves paths against the actual build directory and accepts only this current Mini scope; its SHA256 is `8f43b128bec9029b5b8df13dc23632fd6a6fdf5329ba48cfea43c0089da8c307`. No fixture rerun or rewrite of the failed receipts was used for adoption.

Independent review read actual raw logs, commands, generated cache/compiler/CTest files, current source/snapshot/tool/resource/product files and Ninja dependency records. All 309 receipt pin records across 271 unique paths matched current SHA256/length; forty current snapshot inputs and 184 actual Ninja dependencies were verified separately. The independent report is `E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/independent-capacity-cmake-current-v1.json`, SHA256 `397df45c2b5edb0e200e12af326c757c24ec3f88eeeb713a75b5eb5877bbcb6f`. This reviewer made no compiler, runtime, Job helper or Git call.

This establishes fresh current fragment Mini CTest acceptance with UBSan. It adds no ASan, full repository configure/build, MSVC compiler runtime, Linux, native32, dynamic OOM, forced allocation movement, GVN compatibility rerun or all 47 SSA credit. Six-file integration and commit remain pending under the unchanged Git/index restrictions.
## Fresh failed gates retained

- WSL GNU entry was rejected by the service with `Wsl/Service/E_ACCESSDENIED`; no Linux compiler or fixture run occurred. Root receipt SHA256 `a77effbd64b0db9dd6e2c49b9ae73b95def9af9bd3f2b75c211708eb6e97edb8` records failure.
- Fresh MSVC's primary compile/link process exited0, but the owned descendants did not exit naturally and required `TERMINATE_OWN_JOB`. Whole gate remains FAILED; no fresh fixture runtime credit. Root receipt SHA256 `ce0fc42630da1a04d1ede0379445c7dc78c6ebc1d427f1f0441c3aa1ed46d3b2`.
- Fresh Windows Clang ASan/UBSan v4 compiled and linked, then failed at runtime startup with interception_win unhandled-instruction diagnostics and natural status3221225477 (0xC0000005), before any fixture PASS output. This is an actual failed ASan runtime gate, not a passing fixture or an ignored diagnostic. Root receipt SHA256 `99796ad42e619f397df07f2b5afdb530a92c60f70c20491a49a12ab98d8adc42`.

## Historical failed optional repeat retained

First genuine configured Mini17/real1-of1 adoption411e remains accepted. The optional later Mini repeat wholeFAILED auditf82f is separate and preserved: Root129.554, CTestchild343355 timeout31.5 with TERM/reap/EMPTY, log17PASS but no CTest summary; WSL46348 timeout/TERM/reap and Linuxcontroller343353 UNKNOWN, native postflight incomplete. Partial fixture logs do not credit runtime/CTest acceptance. That historical attempt is separate from the fresh current E Mini acceptance recorded above. Mini alias236568 remains limited read-subproduct adoption; its outer wrapper wholeFAILED is not promoted.

## Limits retained

The fixture uses standard allocation. No controlled allocation failure or hook was added; allocation-NULL rollback follows temporary realloc/publication order by source inspection only. Native32 and dynamic OOM remain unexecuted. Pristine safe reserve RED and separately extracted numeric RED do not execute unsafe old terminal-count append or alias paths. Production helpers are static in ranges.c, not a new private arithmetic header.

GVN compatibility is24 actual bound test-function calls per product and two real1-of1 CTest runs, not24 individual assertions, a printed24 summary or facts-consumption proof. The17-case fixture covers invalid-input/count/backing/size checks, genuine growth, live-input alias for all3 arrays and generation/latest controls. Borrowed plain Core objects/fixture sanitizers do not establish wholeCoreSAN or raw latestE compilation. No full E parent configure/build/107-library/all47/full02.02 completion is claimed. All47 SSA leaves and01.01/02.02 remainOPEN.
