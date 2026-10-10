---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build_compare.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effects.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effects_linear.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effect_loops.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build_control_edges.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_cfg.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_normalize_cfg.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_place_eligibility.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_scalar_scratch.c
  - zr_vm_parser/src/zr_vm_parser/place_queries.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa_promotion.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c
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
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owned_module.c
  - tests/parser/test_ssa_retained_builder_cfg.c
  - tests/parser/support/ssa_retained_builder_cfg_fixture.c
  - tests/parser/support/ssa_retained_builder_cfg_fixture.h
  - tests/cmake/ssa-retained-builder-cfg.cmake
  - tests/cmake/ssa-retained-builder-direct-validation/CMakeLists.txt
  - zr_vm_parser/include/zr_vm_parser/exec_ir_builder.h
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_parser/include/zr_vm_parser/semantic_value_facts.h
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/include/zr_vm_core/exec_ir_owned_module.h
  - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h
implementation_files:
  - tests/parser/test_ssa_retained_builder_cfg.c
  - tests/parser/support/ssa_retained_builder_cfg_fixture.c
  - tests/parser/support/ssa_retained_builder_cfg_fixture.h
  - tests/cmake/ssa-retained-builder-cfg.cmake
  - tests/cmake/ssa-retained-builder-direct-validation/CMakeLists.txt
plan_sources:
  - docs/plans/ssa/01-execir-ssa/01-core-model.md
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
  - docs/plans/ssa/01-execir-ssa/03-effects-verifier.md
  - docs/plans/ssa/01-execir-ssa/04-state-maps.md
  - "user: 2026-10-10 finite retained builder CFG gate; execution reserved for Root review"
tests:
  - tests/parser/test_ssa_retained_builder_cfg.c
  - docs/acceptance/ssa-retained-builder-cfg.md
doc_type: milestone-detail
status: finite-green
---

# Retained builder CFG acceptance

## Status: finite green

Recorded 2026-10-10 from the actual E:/Git/zr_vm checkout. Root accepted runtime
r2: a fresh 31-source build, actual link and eight ordered PASS results with
UBSan, zero warnings and no sanitizer diagnostics. The separate MSVC gate
accepted compilation of the same 31 sources only; it did not link or execute.

The [fixture design](../parser-and-semantics/ssa-retained-builder-cfg.md) fixes
the graph/lifetime expectations. The [validation guide](../testing-and-validation/ssa-retained-builder-cfg.md)
fixes the target, 31-C-source closure, exact case denominator, diagnostics and
cleanup requirements. Root reviewed actual source dependencies, object imports,
defined symbols and map providers before the finite runtime invocation.

| Receipt | Current result |
| --- | --- |
| Static explicit source inventory | 29 production TUs plus runner/support; no module libraries |
| Eight functional cases | PASS, exact order preserved |
| Exact diagnostic payload/recovery/original preservation | PASS in cases6–8; all nine diagnostic fields checked |
| Producer release before retained verification | PASS through pinned numerical checks and retained reads |
| Windows clang-cl UBSan | Runtime r2 accepted; fresh 31 compilation steps and one link |
| MSVC | 31 C units compile-only, zero warnings; no link/runtime credit |
| Full MSVC, Linux/WSL/GCC/Clang, normal/default project | Open |
| Complete OOM/leak/ownership denominator | Open; no injection or leak claim here |
| Source, M1, all47 | Incomplete |

FiniteLiveSpan12 UBSan and the four-TU MSVC compile-only support receipts are
prerequisites, not this gate or full architecture acceptance.

## Actual receipts and bounded evidence

Receipt and raw files are retained under
`E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b`:

| File | SHA256 | Actual result |
| --- | --- | --- |
| `retained-builder-cfg-green-r1-receipt.json` | `6C73C264FF7F76026132917DAAB0908207B39B1AC3B922AFB5945F53A70B6ADB` | False; configure/build exit0, 23 warnings, no CTest |
| `retained-builder-cfg-green-r2-receipt.json` | `477F5BBD38066F62752FB6004C2DC74160355453737FB4130F6D89610793C326` | True; six actual phases exit0 |
| `retained-builder-cfg-green-r2-ctest.log` | `A3725075142DC6C8D511D4F51BCC823D259E220DBA5F186076B3E1BFA8CBB6C5` | Eight ordered PASS; cases=8 failed=0 |
| `retained-builder-cfg-msvc-GREEN-r1-receipt.json` | `9021FE2F944EFA7588837EC4CC255F937FEF71E89917F1C50DCED86F355A4933` | True; 31 compile-only phases exit0, zero warnings |

Runtime controller43332, unified session86819, and MSVC controller29652,
unified session30480, reached actual natural exit0 and closed. The runtime
phases were configure25624, build24244, latest-deps25944, defined-symbols42880,
undefined-symbols7300 and CTest34996; each reached natural exit0. The fresh
route had no previous cache or fixed map and compiled all 31 sources.

Runtime r1 controller27972 rejected the build's actual 20 CRT deprecation
warnings plus three missing-brace warnings despite compilation/link exit0.
The correction uses the standard Windows target-private
`_CRT_SECURE_NO_WARNINGS` configuration and three `{{0u}, 0u}` initializers on
two builder lines. `/W4`, `/UNDEBUG`, actual headers and UBSan were preserved;
no `/wd` or `-Wno` category suppression was introduced. The false first receipt
remains separate from the accepted fresh runtime r2.

The actual runtime closure contains the baseline28 production TUs, one opaque
owner TU and two test TUs. Latest source/header dependencies cover all31;
defined-symbol and import inventories cover all31 objects. Actual map/nm rows
identify sole providers for `ZrParser_ExecIr_BuildModule`,
`ZrCore_ExecIr_CloneOwnedModule`, `ZrCore_ExecIr_VerifyModule` and
`ZrCore_ExecIr_ValidateStateMap`, with `/MD` CRT imports and no project archive,
forced include or Source/VM/provider replacement.

The runner requires stage0, producerAllocated=producerReleased=functionsBuilt
=count, transientReleased=ownerPublished=1 and a NULL owner slot after cleanup
before reporting PASS. The raw output has no separate success-counter dump.
These pinned checks and actual retained reads supply finite lifetime evidence;
they supply no heap-instrumentation, complete leak or OOM denominator.

MSVC used actual C11, `/experimental:c11atomics`, `/MD`, `/W4`, `/UNDEBUG` and
no forced includes. All31 raw outputs contain only their source basenames.
Actual SourceDependencies Version1.2 records contain Source/Includes and a
present-empty ProvidedModule; ImportedModules and ImportedHeaderUnits are
absent. No compiler receipt establishes runtime behavior or fixed ABI bytes.

Before this documentation update, Root reviewed 294 selected runtime inputs,
seven metadata files, 33 artifacts and six raw logs with zero drift. The three
guides were among those 294 inputs; their historical review pins were parser
`E78A53D0B8F40EBD8CABE0EECBE992E7182DA2803B3B854EC12349517CDC3867`, testing
`86237829E1354A912AD8425F506B2F83D0A256978E4FDDCFEF9CF17BDCE2EE89` and acceptance
`89974736D1E44D554FCCAD43CE53E7ADE07EF7A51CFEE4EFCC62BB1E51095BCC`.
Those are historical protocol/review metadata after this owned guide update.
The other291 selected inputs and unchanged metadata/artifacts/raw evidence
retain their pins; these current guides are frozen separately for Root's Git
transaction. No runtime rerun or current all334-zero-drift claim is made.
Root also verified 240 selected MSVC inputs and all93 raw/object/JSON artifacts
before this update.

Shared registration and the Git transaction are tracked separately by Root.
Source, M1, all47, full Linux, full MSVC runtime, normal/default project gates,
complete OOM/leak coverage and genuine Function/finalizer integration remain open.

## Physical file pins for static review

SHA256 values below identify physical source/fixture/CMake files read during
this review and accepted runtime handoff, including the two reviewed warning
corrections. They are not copied source snapshots. The actual configure TSV,
latest header records and compiler receipts identify compilation inputs; this
34-file table remains a smaller repository inventory, not a complete
preprocessed dependency or toolchain manifest.

| Repository-relative file | SHA256 |
| --- | --- |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c` | `03c0bf89eb8b5c7f07844f4fc6312ecfaf942d22b1e004d68ded8f076e826e64` |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build_compare.c` | `94198992de07f5871af57f4b05068798a919e0805caac4c1bdeb23f44f9da07d` |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effects.c` | `68394ba3c96d05008e8da648809559e2cb4e1fddd7005f5fb830a9377a5fa958` |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effects_linear.c` | `bde18e8a7fde9ac00c7419ea9032571654b81568d3b1ee47af7e378354f62d6f` |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effect_loops.c` | `304997b9378e9746e48a3b5602582cf1e289a70323257b9401b0e3f1f2742fac` |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build_control_edges.c` | `a5ba98298bcb77f2f7a5144af379d718fbcf381339e22738f4eabd3238395bfe` |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_cfg.c` | `994c4326aa1e5898fae92e634f789502eeb9af251515efb3238493478bcc65d1` |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_normalize_cfg.c` | `2dc97972bf408591c4391f451f6fd77eb9527c62251d0e10e174757b5d930976` |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_place_eligibility.c` | `6fa88ff069364790f71431201f0725eb2c01e00ce7a589c62a4a0a115137546a` |
| `zr_vm_parser/src/zr_vm_parser/semantic_ir_scalar_scratch.c` | `c2b5b259489e79b1ef7cb1b5c971228996a6bcb70d4762990eec5b2005bf4ecf` |
| `zr_vm_parser/src/zr_vm_parser/place_queries.c` | `8367489da5cd3d67eb6800d4b7379ec75e5359d67c8b1d5613252518f2b936de` |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa.c` | `a7560195f3b5e6723a00a30cafb267a825467c1f16a24c8a56003ad980dcbac2` |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa_promotion.c` | `d96b4ded130ca1863e54cc515f6f49e64e70ae9e2b15a53f8cf052cb5c9ada56` |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c` | `c0ff46cd4769be0f5bb045413bac831001cc61c2046243f31da9e407813146e4` |
| `zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c` | `7f5bd0c576723714461459fd9447cc1c121d89c89e35c8e9be9300f4fca644ac` |
| `zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c` | `9b195e3220168f4997ecb2143c7d8bdcfe3e098fe7ed7cb4f57c0ac45ef55e40` |
| `zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c` | `fc0fe85282efb8044d4f68de012b353f3c8d770221c230220fe46980143a5cee` |
| `zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c` | `158ea0772f4b1402225f7fa6fe9e1c085e36308026d71405d5ea002bd8c35362` |
| `zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c` | `9e4c0b7389b180a9b61591cb9ed4d22d89cca92c76d613fe6cb0e61d1fef2f54` |
| `zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c` | `8abea829a1807aa8074b8f20e8b4b31f252065dc9ae140229e0ab1b6dae4df7a` |
| `zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c` | `3667ef260fa2c56545e8749fc100a8a14e1c3769531a283a92492ea9a7530155` |
| `zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c` | `ba0580ae7b545306d3ec0277686babc088d4cc94e9a274c97e5235b94c0715c4` |
| `zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c` | `db2419783d7098acd785341210c3c439b123164915bf0989ce353224cbac7842` |
| `zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c` | `fa6da50bc4c93767bdbf2ef1ea5f399c9a80824ba2511c0c6e8124d6f36dfd1e` |
| `zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c` | `d42453db780331f0ac2292f0fcb562c84cf3d9946e2254fb0135b463e61e83b5` |
| `zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c` | `d1b8d887efa9df86df5446b689447a9918bdbd71f0acb2c46e3973f48a10e81d` |
| `zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c` | `ee19de76aa3c3a7b2c7673ff49f5e630ae8a4a2eb1b5e007b95889b9b8f9ebd6` |
| `zr_vm_core/src/zr_vm_core/execution_contract.c` | `d7d5240be5ec2de58ee4a45fb277d9bfc135d0c9c7f1fc3a806a603c5ef369c2` |
| `zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owned_module.c` | `02a1ff2b00a22d58a14031d57cbe22c5c00d688b50cb3b6d4f812ee861d888cf` |
| `tests/parser/test_ssa_retained_builder_cfg.c` | `9b87e882990f6b266b6b6fee86c3cf7a06dcba8fea654b22f1d0f6152d023d67` |
| `tests/parser/support/ssa_retained_builder_cfg_fixture.c` | `b421cea2d49d2c617d3be8d376ed12639f392a36182136ef2240e1fc8e1421aa` |
| `tests/parser/support/ssa_retained_builder_cfg_fixture.h` | `66bd12a4b48af1e4fd40bd81394afa40f31008bd612368af680d607bedf8a120` |
| `tests/cmake/ssa-retained-builder-cfg.cmake` | `331e554a655fe294e4e245424a49d892b63d53e6c704d26499069b5c1d1e69f3` |
| `tests/cmake/ssa-retained-builder-direct-validation/CMakeLists.txt` | `c7a0bab9917269824b09c85215704ec59a4676778acf3c69247c69521d75d018` |
