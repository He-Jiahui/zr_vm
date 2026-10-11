---
related_code:
  - tests/core/test_ssa_owner_try_capture.c
  - tests/core/support/ssa_exec_ir_owner_capture.h
  - tests/core/support/ssa_exec_ir_owner_capture.c
  - tests/core/support/ssa_exec_ir_owned_module_fixture.h
  - tests/core/support/ssa_exec_ir_owned_module_fixture.c
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h
  - tests/unity_config.h
  - tests/third_party/zr_unity/Unity/src/unity.c
  - tests/third_party/zr_unity/Unity/src/unity.h
  - tests/third_party/zr_unity/Unity/src/unity_internals.h
  - tests/cmake/ssa-owner-try-capture.cmake
  - tests/cmake/ssa-owner-try-capture-direct-validation/CMakeLists.txt
  - tests/cmake/ssa-owner-try-capture-check.cmake
  - tests/cmake/ssa-execir-owned-module.cmake
  - tests/cmake/ssa-execir-module-copy.cmake
  - tests/cmake/ssa-function-execir-owner.cmake
  - tests/cmake/ssa-source-finalized-module.cmake
  - tests/cmake/ssa-source-returned-execir-retention.cmake
implementation_files:
  - tests/core/support/ssa_exec_ir_owner_capture.h
  - tests/core/support/ssa_exec_ir_owner_capture.c
  - tests/core/support/ssa_exec_ir_owned_module_fixture.h
  - tests/core/support/ssa_exec_ir_owned_module_fixture.c
plan_sources:
  - .codex/plans/20261010-ssa-nonasserting-owner-observers.md
  - 'Root: 2026-10-10 finite pure TryCapture RED authoring'
tests:
  - tests/core/test_ssa_owner_try_capture.c
  - tests/cmake/ssa-owner-try-capture-check.cmake
  - docs/testing-and-validation/ssa-owner-try-capture.md
  - docs/acceptance/ssa-owner-try-capture.md
doc_type: milestone-detail
status: finite-validated
---

# Pure TryCapture acceptance boundary

## Current decision

Finite support validation is accepted: actual functional RED r2 preceded the positive implementation; independent fresh default GREEN r1, three-unit MSVC compile-only GREEN r1 and lower owned-module/module-copy/CoreModel regression r3 pass their exact gates. The Unity child intentionally remains nonzero in the pure runs. Root has appended the shared registration; the Git transaction remains tracked separately by Root. This document claims no commit. See the [module contract](../parser-and-semantics/ssa-owner-try-capture.md) and [exact testing protocol](../testing-and-validation/ssa-owner-try-capture.md).

| Gate | Actual result | Limit |
| --- | --- | --- |
| Functional RED r2 | Fresh three-C compile/link; 3 PASS/9 specified FAIL; child9/wrapper0; twelve CASE rows and FINAL; zero ignored | Earlier refusing provider, explicitly selected historical RED |
| GREEN provider | Local zeroed candidate, one checked traversal, whole-output copy only on success; refusal unchanged | Readable typed graph/output preconditions remain |
| Default GREEN r1 | Fresh configure without phase override; cache/settings GREEN; 11 PASS/1 deliberate prior FAIL; child1/wrapper0 | Finite direct target default, not full-project defaults |
| Actual graph | Three latest source/header edges; four sole providers, map/MD CRT; no FI/archive/extra project provider | SDK wrapper graph admission gives no extra execution-purity credit |
| Clang/UBSan | C11/MD/W4/UNDEBUG, seven normal definitions; zero warnings or UBSan diagnostics | Windows finite child only |
| MSVC GREEN r1 | Three natural compile-only exits0/warnings0; SourceDependencies1.2/raw basename profile | No link, runtime or fixed ABI-byte credit |
| Lower consumers r3 | Owned-module14 and module-copy20 Unity PASS; CoreModel17 default calls; three CTests0 | Function-owner/finalized/returnedSource integration remains OPEN |

The pure target closure is exactly main + captureC + UnityC. It has actual typed schema headers but no allocatorC, fixtureC, Core/Parser library, Source/VM bootstrap, forced include, archive or replacement provider. Its result gives no combined armed-ledger purity or VM identity credit. Separate lower regression uses reviewed existing allocator forced includes and real Core ExecIR implementation objects.

## Actual receipts and naturally closed controllers

All receipts and phase logs reside under E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b. Their exact hashes were independently read before this guide update.

| Receipt | SHA256 |
| --- | --- |
| [Pure RED r2](E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/owner-try-capture-red-r2-receipt.json) | 3CCF2D96615002309DA2A76FE907711D81CE3352E4D1B9C0576A32E7C8C989BF |
| [Default GREEN r1](E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/owner-try-capture-green-r1-receipt.json) | CACF88D834BC50DC195205B25B888D2AAEFBC27BB15ED20F03B5F9E4F46C94BB |
| [MSVC GREEN r1](E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/owner-try-capture-msvc-GREEN-r1-receipt.json) | 1ED09B3F9CF6A75CE769B7B9DCBE140312256E83ED6A7D105261F908222420DD |
| [Lower consumer GREEN r3](E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/owner-capture-consumer-regression-green-r3-receipt.json) | 7EB3F6F69B262E56FE95CCCF2C134D8D84517EAD9B030B7A299AF699466E6CE7 |

Root reports all controllers naturally closed: RED r2 driver3292/unified38338; default GREEN28544/unified38297/terminal d378e6 exit0; MSVC22048/unified21583/terminal 2e31bc exit0; consumer23040/unified32150/terminal 4f02df exit0. Runtime receipts retain six actual WaitForExit natural terminal phases, all0; MSVC retains three such compile phases, all0. These are actual child closures, not inferred success from a started process.

Fresh runtime children are E:/cargo-targets/zr_vm/build/ssa-20261011-01a0fe2b-owner-try-capture-red-r2 and the corresponding owner-try-capture-green-r1 child. Both compile all three C units and link once. GREEN configure args omit the phase override. Latest actual header counts are RED190/177/29 and GREEN190/183/29. Actual definitions/map prove four sole providers: TryCapture, VisitContents, UnityDefaultTestRun and UnityEnd. Bounded ordinary AMD64 COFF records five RED/six GREEN owned functions, seven separate SDK inline COMDAT wrappers and the sole stdio import in _vsnprintf_l. This reviewed allowance changes neither project source nor compiler flags and explicitly carries no extra execution-purity credit.

MSVC's fresh child is E:/cargo-targets/zr_vm/build/ssa-20261011-01a0fe2b-owner-try-capture-msvc-GREEN-r1. Compile PIDs15732/5360/9748 exit0, warnings0, stdout is only the respective source basename and stderr is empty. Actual JSON1.2 includes85/78/19; ProvidedModule is present and empty, ImportedModules/ImportedHeaderUnits are absent. Raw protocol in its freeze is metadata only; MSVC does not execute it.

## Exact raw discrimination and artifact pins

Historical RED cases1–7/12 fail only with OWNER_TRY_CAPTURE_MISSING_FEATURE, case11 fails only with OWNER_TRY_CAPTURE_PRIOR_FAILURE, and cases8–10 pass. Current GREEN only case11 deliberately fails. Actual Unity exits9/1 are preserved separately from wrapper0. Both have twelve tests/zero ignored, twelve ordered result/CASE pairs, FINAL, one 23-dash separator, exact summary and FAIL footer: 28 nonempty stdout lines, exactly empty stderr.

The [testing guide](../testing-and-validation/ssa-owner-try-capture.md) retains every exact name and fifteen-field row unchanged. GREEN verifies the independent word lists, unused zeros, whole initialized refusal output, sixty shape refusals and exact4096/over4099 budgets. Case11 captures under the actual failed flag, then frees its real root; case12 verifies saved output and released roots after conclusion without a new call/allocation. FINAL reports drain1/live0/acquired90/freed90/prior_failed0/prior_ignored0/flags_preserved1. Ordinary release/support observation precedes feature assertions; final drain contains no Unity assertion. Ignore execution remains OPEN.

| Actual artifact | SHA256 |
| --- | --- |
| RED raw stdout | 214F70F4653EB5EE382DE8A9C55D70C41D371B939ED1CDA19F521A9F696FF63D |
| GREEN raw stdout | 3749761583273E17FE832C53D706652E3E07B63758A262571BBCE8EC80D3ED76 |
| Both empty raw stderr | E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855 |
| GREEN owner-try-capture-green-r1.map | 333C3F73947FFBB9449A67A7721FBD9086D2A97662D463A52567B40E297770AD |
| GREEN zr_vm_ssa_owner_try_capture_test.exe | 4BB2DC71BDE2298373E7ED800BF97E77395BD670C167C40F5399A39B1B12B567 |
| Consumer r3 owned map | 44875F74D5ACDF5F9C2397DC398C7D71F692011FD9BCB9503FC81C5B5D04039D |
| Consumer r3 copy map | 019E0D3A4BF4CFF35CE30DD4EB28CF08ADF20E938A4340200924FEE90B377C94 |

Raw logs are in each pure build child as owner-try-capture-RED/GREEN-stdout/stderr.log. Consumer maps reside in its r3 build child with the owner-capture-consumer-regression-green-r3-owned/copy.map basenames. Receipts retain the objects, executable pins, metadata and complete phase logs. Existing raw logs are refused before CMake writes captured streams; that writer uses Root's unique fresh child/single writer, not OS exclusive creation. Driver receipts/phase logs use CreateNew.

## Separate lower consumer regression

E:/cargo-targets/zr_vm/build/ssa-20261011-01a0fe2b-owner-capture-consumer-regression-green-r3 built exactly43 C translation units and three links:25 unique C files,43 latest VALID dependency records,32 reviewed forced-include units and eleven without FI. No project archive, Function/Global bootstrap or Source execution is selected. Twenty-six unselected configured compile commands remain unbuilt/unrun.

The actual CTest log contains owned-module14 PASS, module-copy20 PASS, zero failures/ignored, and CoreModel's seventeen default calls with its actual PASS marker. Owned allocation sweeps each measure61/complete61 with clone60; module-copy measures module60+replacement28=composite88/completed88. This is bounded fault coverage of the selected operations, not complete OOM or leak instrumentation. Real definitions/maps retain the legacy Capture fixture and exactly one shared visitor provider per lower consumer. Root's complete review checked578 selected pin occurrences before this documentation update; these receipts do not acquire new native credit from the later text edits.

Exactly one captureC source line remains beside fixtureC in each of five existing descriptors. The owned-module/module-copy additions now have finite regression credit. Function-owner, Source-finalized and Source-returned-retention additions still need genuine link/provider/runtime gates; their presence alone provides none.

## Preserved failure history

| Failed receipt | Actual stopping reason | SHA256 |
| --- | --- | --- |
| Pure RED r1 | Five phases0; before CTest, rejected normal SDK __imp___stdio_common_vsprintf inline emission | C971EA258894BC584370AB436A8E0D771E80ADBA6CAFC2D1BA6BA2067265D580 |
| Consumer r1 | Configure0; before compile, CMake3.31 double-underscore compiler-rule selector mismatch | 0713231AC4891D45142805FDC9AD462A808DE801C4CBF7AD686504FF00E7C85A |
| Consumer r2 | Four phases0; before CTest, nm COFF R ledger-format constants falsely matched as emitted diagnostics | 81A55EACF2FCB8CC5E55B19DD0916FD31C25575B6F490289F886F338E71B1A6D |

Reviewed driver corrections add bounded SDK graph admission, match actual rule names and interpret symbol-list diagnostics according to phase. Failed receipts remain false; no production warning suppression or altered test protocol hides these failures.

## Historical source and documentation bridges

The fifteen-path pure freeze is C3, captureH, exec_ir.h, exec_ir_state_map.h, unity_config.h, UnityH/internalsH, CMake3 and Docs3. Main remains9584D2B54B7F2AE07B1265A235F8C016592852F8FE71D80457020B0175B37CA7. Root reviewed whole-memory inverses for the minimal positive provider and single default literal change.

| Path | Historical RED SHA256 | GREEN SHA256 |
| --- | --- | --- |
| tests/core/support/ssa_exec_ir_owner_capture.c | 410853F9D0DFB9C4F8E5520593C5B3D488F2AF66176B439C285A66D185D58EFD | B3402C451D473D64ADDF9277F9CCF0742C94EA39C7DBC4FE7DCFEE5B6CCD3DFE |
| tests/core/support/ssa_exec_ir_owner_capture.h | 127DBAB53B307594AD29800B59F78E248F2503891745E462CA319A1CE8E0E74F | D1DE3ED5A3CD580A0B252B1E4F3822BF8D6AF18CEA2AC1C829F6D882CB60BE50 |
| tests/cmake/ssa-owner-try-capture.cmake | 6FC382E73D702C570BDA3DD46000EBA94058243164F0D504CF86A4DF6E571B78 | 8CE34B5E37598EDD887994287F219E1824AD114C1315CC8FD38804621CC83BA4 |

The REDr1/REDr2/GREENr1/MSVCr1 freezes each record these original guide pins: module0AB6C30C30F24ACB8A33E879F4524CABB0DD044A3F7FBE8A4F2AACC734730767; testing45E2D91B4B3FDCE0395493CEEE18954706151DA3EB00787615BFEBD9C05DCFE6; acceptanceCAB03F8546B8CF1F29B49A3CE03B8DC0A537CD4329FCBB3705E03AEDC2550A3E. These are twelve distinct historical guide-pin references, not current guide hashes. This evidence-only update changes just Docs3, whose current physical pins are frozen separately by Root. It causes no runtime rerun or changed C/header/tool proof. No old entire freeze list is declared current after its authorized source/default/documentation transitions. No source snapshot, recursive self-hash or invented commit is embedded.

## OPEN limits

Readable typed graph/output preconditions remain. Geometry cannot authenticate arbitrary pointers, ownership, schema correctness, opaque owners or VMFunction identity. Earlier UnityIgnore, stack/resource exhaustion, complete OOM/leak instrumentation and combined armed-ledger Capture purity remain OPEN.

Function-owner, genuine finalizer wiring, Source's twenty cases, VM/Function execution, network/FFI/security, full47, normal full-project defaults, Linux and full MSVC runtime remain OPEN. Prior Arm/Event, cleanup, CRT, LiveSpan and CFG evidence supplies no additional native credit for this gate.
