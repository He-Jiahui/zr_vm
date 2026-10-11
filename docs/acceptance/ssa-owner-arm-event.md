---
related_code:
  - tests/core/test_ssa_owner_arm_event.c
  - tests/core/support/ssa_exec_ir_owned_module_allocator.h
  - tests/core/support/ssa_exec_ir_owned_module_allocator.c
  - tests/core/support/ssa_exec_ir_owned_module_shim.h
  - tests/unity_config.h
  - tests/third_party/zr_unity/Unity/src/unity.c
  - tests/third_party/zr_unity/Unity/src/unity.h
  - tests/third_party/zr_unity/Unity/src/unity_internals.h
  - tests/cmake/ssa-owner-arm-event.cmake
  - tests/cmake/ssa-owner-arm-event-direct-validation/CMakeLists.txt
  - tests/cmake/ssa-owner-arm-event-check.cmake
implementation_files:
  - tests/core/support/ssa_exec_ir_owned_module_allocator.h
  - tests/core/support/ssa_exec_ir_owned_module_allocator.c
plan_sources:
  - .codex/plans/20261010-ssa-nonasserting-owner-observers.md
  - 'Root: 2026-10-10 finite nonasserting Arm/Event primitive gate'
tests:
  - tests/core/test_ssa_owner_arm_event.c
  - tests/cmake/ssa-owner-arm-event-check.cmake
  - docs/testing-and-validation/ssa-owner-arm-event.md
  - docs/acceptance/ssa-owner-arm-event.md
doc_type: milestone-detail
status: finite-support-accepted
---

# Owner Arm/Event acceptance boundary

## Current decision

Root accepted the finite support gate on 2026-10-10 after actual functional RED, independently fresh default GREEN under Clang UBSan, and three-unit MSVC compilation. GREEN retained the one deliberate prior Unity failure and passed the other eleven cases. Actual Unity child exit1 and CTest checker exit0 are separately recorded. Shared registration and the Git transaction remain separately tracked by Root; this record claims no commit or normal full-project validation.

The [module contract](../parser-and-semantics/ssa-owner-arm-event.md) describes implemented nonasserting API behavior; the [testing guide](../testing-and-validation/ssa-owner-arm-event.md) preserves all twelve names, eighteen CASE fields and eight FINAL fields. Main SHA256 `8CA522546CD63B67F86A32EB7DC455306E8599C5E629D6CA704C31772C5EE7FD`, 416 lines/20575 bytes, stayed unchanged across all three receipts. It uses actual header declarations and the single authoritative allocator. Historical RED refused positive paths after real guards; current GREEN schedules the fault/copies the event. This documentation update grants no new native result.

## Required evidence

| Gate | Required finite evidence | Current status |
| --- | --- | --- |
| Functional RED | Fresh actual three-C compile/link; eight exact failures/four passes/zero ignored; child exit8; exact support/drain witnesses | Accepted r1 |
| Provider GREEN | Root-authorized TryArm/TryEvent implementation after accepted RED; original asserting wrappers and unrelated allocator bytes preserved | Accepted exact memory inverses |
| Functional GREEN | Eleven passes/one deliberate prior failure/zero ignored; child exit1; exact support and positive-feature witnesses | Accepted r1 |
| Default GREEN | Fresh configure without phase override; actual cache/settings GREEN | Accepted same GREEN r1 |
| Dependency/provider graph | Actual three sources/latest header edges, object imports/definitions, fourteen sole providers/map/MD CRT and frozen settings | Accepted finite closure |
| Compiler/UBSan | Actual C11/MD/W4/UNDEBUG, zero warnings and no undefined-behavior diagnostic under Clang no-recovery UBSan | Accepted RED/GREEN |
| MSVC | Three actual compile-only units, JSON1.2 dependency evidence and no forced include/archive | Accepted r1; runtime OPEN |

The exact executed closure is main + authoritative owner allocator C + actual Unity C, target `zr_vm_ssa_owner_arm_event_test`, CTest `ssa_owner_arm_event`. Actual dependency/symbol/map evidence confirms this closure and sole providers. It imports no Source/VM/Core constructor, archive or replacement provider. This finite child supplies no genuine Source or VM lifecycle credit.

## Failure discrimination

RED requires cases1–6/12 to report only OWNER_ARM_EVENT_MISSING_FEATURE and case11 only OWNER_ARM_EVENT_PRIOR_FAILURE; cases7–10 pass. GREEN requires case11's same deliberate failure and all other cases pass. The executable has no phase parameter. Both phases must end with twelve exact ordered result/CASE pairs, one exact FINAL, one 23-dash separator, the actual summary and FAIL footer. A checker success/CTest exit0 is acceptance of this precisely expected negative run; the actual Unity process must retain exit8 or exit1 respectively.

All eighteen CASE fields must match the testing guide. Support, drained and preserved flags are 1; live/errors/ignored are 0. Case10's invalid-output checks also preserve a real pending fault, so its attempts2/records1/frees1/failures1 are required in both phases. Case11 preserves the existing failed flag through nonasserting observations and cleanup; case12 verifies saved pre-free0 and post-free1 per-record witnesses after actual Unity conclusion without Reset. Successful public observer results and exact event fields supply feature evidence; Stats supplies health and purity only.

FINAL requires drain1/live0/errors0/flags_preserved1, with records/frees/queries/record_frees RED2 or GREEN1. Empty stderr is mandatory. Unexpected warning/sanitizer/ledger text, other failure reasons, extra output, ignored tests, incomplete/reordered witnesses, crash and timeout are blockers. Saving actual stdout/stderr must precede interpreting a refusal; existing raw logs must be retained. The checker follows the accepted refuse-existing/file-write policy under a unique fresh single-writer build child and claims no OS-exclusive log creation.

## Actual receipt and artifact record

Evidence base: E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b. Each receipt is validated true with error null. Root verified natural controller exit0/closed: RED PID41648/session84428; default GREEN PID164/session22891; MSVC PID17148, immediate exec chunk20bd2a with unified session null. Six runtime phases and three MSVC compile phases actually reached WaitForExit terminal0. Both runtime routes were fresh, compiling all three units and linking without warnings/UBSan diagnostics. Main/allocator/Unity MSVC PIDs28812/42492/42600 exited0 with warnings0 and basename-only raw output.

| Actual artifact | Basename / SHA256 |
| --- | --- |
| RED receipt | owner-arm-event-red-r1-receipt.json — D320E7E6CA066A9B4B90FA38C6A6ED84A9F3E7BBA52FBD7196797723B81699F6 |
| Default GREEN receipt | owner-arm-event-green-r1-receipt.json — 922FA62B6CEAB894060F8FB42A172FD1F2E7AEAC772C44B76B859BE7353524F9 |
| MSVC receipt | owner-arm-event-msvc-GREEN-r1-receipt.json — 92BCAFDB914FB7C4551C654D9E015EB5F1377CA62E00CAFAC55482A513564AEC |
| RED stdout | owner-arm-event-RED-stdout.log — 6FE2197C1F60D2D744E3556BDBA46935E05E2F804A5C6EB926C63871E9DBB738 |
| GREEN stdout | owner-arm-event-GREEN-stdout.log — 21A7F59450F54B8E9C4CA4B6F5B1814BB893760143E05D108FC010F7D21ACAA6 |
| Both zero-byte stderr files | E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855 |
| RED executable / map | F4A550B7F377AF543885C23DAA6BD65EAE27C6BB375FBF04EC8A7FA815A82C70 / B2684905F18148AF7FF55BF5F5004A21315B34B44923F375D844E8649A4498CC |
| GREEN executable / map | 0D8247BD6954B026A4B8FDE926E74BA18B1EEFE48024EA0BC0A23A076DE224D7 / 2B60578CF50E9019CAB511E3FB867CE36EEB62BCBA285C8E937B74FAC0AD056B |
| GREEN source graph / settings TSV | 89F5C9798CA9325AB77DC97489D486550C7E5BA9F0D4B036A4CCC8898E8C89F0 / 8E89ACFC4A655824DECFE9FB2E770E080D151A512AD2440EC7A61311E4819D64 |

Executables/maps/raw child streams and TSV metadata remain in their actual separate E build children; receipts pin their full paths plus object/cache/build/dependency and phase-log hashes. GREEN configure had no -DZR_SSA_OWNER_ARM_EVENT_EXPECTED_PHASE argument and actual cache/settings GREEN, proving this finite descriptor's default. It does not prove normal full-project defaults. MSVC compiled only: JSON1.2 actual header counts26/31/19, ProvidedModule present/empty, imported module/header-unit fields absent; no link, executable probe, runtime or fixed ABI-byte claim.

## Exact historical bridges and current freeze

The actual RED fourteen-file freeze differs from GREEN only in allocator H/C and the target's one default literal. Independent memory inverses reproduced complete RED and original allocator bytes. Current source pins are:

| Path | Historical RED SHA256 | Current GREEN SHA256 |
| --- | --- | --- |
| tests/core/support/ssa_exec_ir_owned_module_allocator.h | 12FFF569F8BABA4EF6C8F67B605E035378352D162FBBBAE918F97AAAE3D769DD | 9FCD62B275871CF358A7B0BE1C6E10F96DF4B1F90556C4FE2A8D0BB3A35EB040 |
| tests/core/support/ssa_exec_ir_owned_module_allocator.c | 4D3EC29464A97088541F31C2E604D063A08F40D55654B1DEB7CE42B3AA5CF53C | 98C2EE5D7CC7E24547686D90948D3812D69E26BCC93A03FA1D81075BCE2D1820 |
| tests/cmake/ssa-owner-arm-event.cmake | 15F5094CD0E00DDF92C69D3FD74995454BA671EB725EF1FACAE524B405F0E4DF | 2287CF36FC5CCF29EB25692DF9EE3D951C24DD692C370470299661862A029C63 |

GREEN runtime and MSVC Root fourteen-file freezes both hash `C28C8870E2D08052D29B4B78E3A5740238055D592C28CD2B5FE9585E30AA7BDA`. All other selected repository sources remained unchanged, including main8CA522..., direct descriptor and checker. Before this guide update, independent review rechecked GREEN's174 listed input/metadata/artifact/raw entries and MSVC's135 entries with zero drift. RED had only the three authorized historical source/default paths, repeated across its lists; every other entry matched current bytes. These are review-time observations, not a claim that the old whole lists stay current after documentation changes.

All three receipts froze the prior parser/testing/acceptance guides at `E1F23E2605BFB5C9878E7A018CAD3298AAFCB98FB92D6612642D622047E55340`, `CE7A1F2F6269CB8BA9A15C479DA2724812CC256B1A7BA4FD0273D49BB2B94037` and `E6B5C21A4C1F4B63A9B86EAEB9E6D37247F552936CD8CCD9E8B17D0B30C1627B`. Final Root commit metadata must separately freeze these updated guides and admit exactly nine receipt/Doc-path historical bridges plus the three RED H/C/default bridges, twelve logical bridges total. Every remaining source/header/tool/metadata/log/artifact pin stays strict. This update creates no runtime rerun or source snapshot and adds no recursive self-hash. Historical cleanup/Returned CRT/LiveSpan/CFG evidence gives no additional credit here.

## OPEN boundaries

The private 65536-event exhaustion boundary remains unexecuted. Actual finite guards cover UINT_MAX at nonzero attempts and invalid event calls preserving an armed next fault; they do not prove all capacity/OOM paths. Earlier UnityIgnore is unexecuted even though failed/ignored flags are compared numerically. Numerical registered-root drain is accepted; complete OOM/leak instrumentation remains OPEN.

Pure TryCapture and its logical traversal, combined armed-ledger Capture purity, original fixture consumers, Source's twenty cases and genuine post-finalizer lifetime integration need later leases and actual execution. No numeric live span authenticates an opaque owner or VMFunction. VM/Function, network/FFI, full47, full normal/default project, Linux and full MSVC runtime remain OPEN. Existing accepted cleanup guides are untouched by this task.
