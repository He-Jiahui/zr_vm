---
related_code:
  - tests/parser/test_ssa_frame_layout.c
  - tests/parser/test_ssa_roots_observation.c
  - tests/parser/test_ssa_active_packed_frame_observation.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_roots.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_layout.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
  - zr_vm_core/src/zr_vm_core/execution_contract.c
  - tests/third_party/zr_unity/Unity/src/unity.c
  - zr_vm_parser/include/zr_vm_parser/exec_ir_frame_roots.h
  - zr_vm_parser/include/zr_vm_parser/exec_ir_frame_layout.h
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h
  - tests/unity_config.h
  - tests/third_party/zr_unity/Unity/src/unity.h
  - tests/third_party/zr_unity/Unity/src/unity_internals.h
  - tests/cmake/ssa-active-packed-frame-observation.cmake
  - tests/cmake/ssa-active-packed-frame-observation-direct-validation/CMakeLists.txt
  - tests/cmake/ssa-active-packed-frame-observation-check.cmake
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_frame_roots.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_roots.c
plan_sources:
  - docs/plans/ssa/04-frame-native/01-frame-layout.md
  - docs/plans/ssa/04-frame-native/04-roots-observation.md
tests:
  - tests/parser/test_ssa_active_packed_frame_observation.c
  - tests/parser/test_ssa_frame_layout.c
  - tests/parser/test_ssa_roots_observation.c
  - docs/testing-and-validation/ssa-active-packed-frame-observation.md
  - docs/acceptance/ssa-active-packed-frame-observation.md
doc_type: milestone-detail
status: finite-validated
---

# Active packed-frame observation acceptance

## Status

**Finite validated.** The additive selected-ValueId scalar primitive has accepted functional RED, Windows Clang GREEN2 runtime and two unchanged legacy regressions, plus MSVC compile-only checks. Shared registration now includes the module. Root separately tracks Git transactions; this record claims no commit or normal full-project execution.

The canonical [frame-layout](../plans/ssa/04-frame-native/01-frame-layout.md) and [roots-observation](../plans/ssa/04-frame-native/04-roots-observation.md) plans retain their broader status. Borrowed typed storage and trusted active-liveness context remain caller preconditions; numerical geometry does not authenticate arbitrary pointers.

## Actual finite gates

| Gate | Actual result | Scope |
| --- | --- | --- |
| Functional RED2 | Six native phases exit0; focused18 C compiles/link;14 exact MISSING_FEATURE FAIL, one deliberate prior FAIL, one legacy PASS; child15/wrapper0 | Functional result independently accepted; orchestration receipt remains false |
| GREEN2 | Six native phases exit0; fresh51 C compiles/three links;51 VALID dependency records and19 sole providers; no warnings/UBSan diagnostics | Focused18 plus legacy layout16 and roots17 TUs; no FI or broad project archives |
| Focused GREEN |16 ordered cases:15 PASS, sole case15 deliberate prior FAIL; child1/wrapper0; stderr empty |4161 calls,4112 accepted,4110 observations/invalidations;24 registered layout owners acquired/freed |
| Legacy regression | Both unchanged original test executables ran once and exited0 | Layout and roots observation only |
| Prior Unity failure | Actual failure flags preserved during assertion-free observation/cleanup; subsequent top-level saved witness PASS | Ignored-test path remains OPEN |
| MSVC r1 | RootsC and runner compiled naturally with exit0/warnings0; sourceDependencies1.2 includes79/86 headers | Two compile-only units; no link, executable, runtime or ABI probe invocation |
| Linux / normal project / full47 | No credit from these children | OPEN |

### GREEN2 retained evidence

The [GREEN2 receipt](E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/active-packed-frame-green-r2-receipt.json) has SHA256 `65F546DABCE763F75C2866623A7A8834C1419D8D066D0909C8887A3A28B567F9`, validated true. Controller22312 closed naturally with exit0. Actual configure24304, build7904, latest-deps24272, defined-nm15168, undefined-nm19692 and CTest31076 all reached natural exit0. Root's terminal and independent pin/raw reviews accepted this finite evidence.

The fresh child is [ssa-20261011-01a0fe2b-active-packed-frame-green-r2](E:/cargo-targets/zr_vm/build/ssa-20261011-01a0fe2b-active-packed-frame-green-r2). Its [focused stdout](E:/cargo-targets/zr_vm/build/ssa-20261011-01a0fe2b-active-packed-frame-green-r2/active-packed-frame-observation-GREEN-stdout.log) SHA256 is `1D5EC74294439D87284174975AD96A27F82552D3B07DFC539C2445A7FBA4C326`; stderr has zero bytes and SHA256 `E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855`. The36 nonempty lines contain the exact16 CASE rows, FINAL drain24, and `16 Tests 1 Failures 0 Ignored ` with its final ASCII space. Only `:FAIL:ACTIVE_PACKED_FRAME_PRIOR_FAILURE` is admitted.

The [focused map](E:/cargo-targets/zr_vm/build/ssa-20261011-01a0fe2b-active-packed-frame-green-r2/ssa-active-packed-frame-observation-link.map) SHA256 is `7027C232857A4724767CB659120A67444DD4C1DF4355C9380A6A14AC9FA66571`; the focused executable is `206CF3D20529DCBDDC3A9C5290AA13E0481E84A4135AD8726209460308ABE837`. Legacy layout map/executable hashes are `61AA8D22A1B3F80E60A3067F15FBC5F80B6C56046BECC2C9C6C375C81780A788` / `C9C372FC38786B69EB4293BEEF12FF999032DA2A3C7E212302AAFF71103D9BA8`; legacy roots hashes are `D0250228D0F8770E266B3681D3C7B82C8F650572933809C6988145324E7FCC36` / `3C837D4911FEBE7669C8A10675A4BBB9CE12499C04963FD7E6C952778F187242`. These are retained build artifacts, not source copies.

### MSVC retained evidence

The [MSVC GREEN r1 receipt](E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/active-packed-frame-msvc-GREEN-r1-receipt.json), SHA256 `FA08F2FD47BC8CA9150B65E73E259584A4EEFB755A29DB08B0CAD19154BFE7B6`, is validated true. Controller7228 and real cl children16332/17084 closed naturally with exit0. Raw stdout was exactly each source basename and stderr was empty; warnings0. C11, /MD, /W4, /UNDEBUG and normal project CRT policy were retained, without forced includes or extra diagnostic suppression.

The [MSVC child](E:/cargo-targets/zr_vm/build/ssa-20261011-01a0fe2b-active-packed-frame-msvc-GREEN-r1) retains eight raw/object/dependency artifacts. Both actual JSON files are version1.2 with Source, Includes and present-empty ProvidedModule; imported-module/header-unit fields are absent. Included-header counts are79 for rootsC and86 for the runner. No fixed ABI-byte claim, MSVC link, runtime or executable probe is inferred.

## Failure history retained

| Historical route | Receipt SHA256 | Actual limitation |
| --- | --- | --- |
| RED1 | `6568213F723F9215081C6C8FD1DB36CD43F5A89F5BFD44EBEBC78C87C2954DDA` | Configure exited0; driver inspected the wrong ABI cache location, then stopped before build/CTest |
| RED2 orchestration | `62EB5BEEA81F6B86AAC7FC551EDE7A6676374D60955BA3D2FA7A7767512ACA6B` | All six native phases passed, but controller exited1 because the expected Unity footer omitted its final ASCII space; receipt remains false |
| GREEN1 | `D4C506B3A04BB8917FED7CC3FBA9728A152EAB22F120B671F5C3A6E2F3AB7F43` | Fresh51 compiles/three links and five phases exited0; strict import admission refused legacy roots' real __imp_exit before CTest |

The [RED2 postterminal Root audit](E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/active-packed-frame-red-r2-postterminal-root-audit.json), SHA256 `A2013BCCC7FE1897B01249EE78E336C3BAAF762BE40A4FC58D3AAEA9883B2DA0`, independently accepts the actual functional RED while retaining historical orchestration false/controller14052 exit1. Its exact raw stdout hash is `B3E2B33D23564628531EB9822D2291B9BFD9613A2B1B6679F69919E481AB0CE9`; child15, wrapper0,14 MISSING_FEATURE, one prior FAIL and one legacy PASS were checked. The footer correction was not rerun as a new RED proof.

GREEN2's narrowly scoped legacy import admission requires the unchanged roots-test source, unique selected object/U row, UCRT map provider and current selected stdlib.h provenance. It permits __imp_exit only for that legacy CHECK failure path; unrelated imports remain refused. No project source, compiler diagnostics or runtime flags were weakened, and no execution-purity credit follows from linked SDK/CRT definitions.

## Current source and historical bridges

The current roots implementation SHA256 is `0A4D78BCB93EDB626F0FC9D45CE8C4CA9D116E6392AE96F06CE31B0232A005B0`; rootsH is `46F7745BCDF8E254F9B8F452F57621E2717362A501836D1715AC66F56FAB26ED` and the unchanged runner is `AB85BD52D7E91C0C545FB628C229D1CBD739E5BC1AB53AAAF680DF09B9582940`. The original356-line roots implementation prefix remains byte-identical; the RED stub's historical full-file hash is `2704D39661293F9FCB3E0C40F978C04D0D64C42B41B594AFE3662E0DE42D4F41`.

The reviewed receipts froze the original module/testing/acceptance guides at `30F703CDEE42E357E16588B5120C8DC33ADEB8B586475EC94EC70B97EE22437A`, `98DD0D588B654D877D3D3D03C0F47800C572380D5CA84A6D89458C92F80F35BC` and `610181EF9F29720DDC9E82D624F52419C177A92F0F9F825A8F51A413C2E4C71F`. These are historical protocol-document pins checked before this evidence update, not claims that the updated guides still match old hashes. The16 names/order and numerical protocol are unchanged.

After accepted explicit-GREEN validation, Root changed only the descriptor's default literal RED→GREEN: historical `A04C8E9022F88870168072223B475779AE3EA2D4C5B7CB80F6CBF9370C404D82` → current `E427F026F828C93F7232FCFB7D6F44C842A29B84C03196CC2AF34CAA7D90A710`. The same explicit-GREEN command/settings were tested; default omission and normal shared-project execution were not rerun. Historical source/docs/default bridges are tracked separately from current commit-input freezes. No recursive document self-hash or private-plan commit is required.

## Cleanup and OPEN boundaries

Registered layout owners are released before final assertions; the actual prior-failure teardown performs observation and drain without Unity assertions. Saved numerical witnesses are checked by the following top-level test. The24-owner drain covers registered layout lifetimes, including empty layouts; it does not count every malloc, detect all heap leaks or establish complete allocator failure behavior.

All47, full04.01/04.04, genuine Source/Function/VM integration, GC/relocation/unwind, debugger resume, backend maps, Linux, full MSVC runtime, normal full-project configuration/runtime, complete OOM, ignored-test paths, heap-leak detection and performance remain OPEN. Trusted typed storage and active-liveness preconditions remain essential. Scalar width1–8 is implemented; this finite width matrix directly exercises1/2/4/8 and mixed-width reuse.

See the [testing protocol](../testing-and-validation/ssa-active-packed-frame-observation.md) and [module contract](../parser-and-semantics/ssa-active-packed-frame-observation.md).
