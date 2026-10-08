---
related_code:
  - zr_vm_core/include/zr_vm_core/aot_ir_owned_module.h
  - zr_vm_core/src/zr_vm_core/aot_ir_owned_module.c
  - zr_vm_core/include/zr_vm_core/aot_ir.h
  - zr_vm_core/src/zr_vm_core/aot_ir.c
  - tests/parser/test_ssa_source_aot_owned_module.c
  - tests/parser/support/ssa_literal_script_fixture.c
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
  - tests/cmake/ssa-source-execbc-vm.cmake
implementation_files:
  - zr_vm_core/include/zr_vm_core/aot_ir_owned_module.h
  - zr_vm_core/src/zr_vm_core/aot_ir_owned_module.c
  - zr_vm_core/include/zr_vm_core/aot_ir.h
  - zr_vm_core/src/zr_vm_core/aot_ir.c
plan_sources:
  - .codex/plans/20261007-ssa-core-owned-module.md
  - .codex/plans/20261006-ssa-core-scalar-owner-design-v2.md
tests:
  - tests/parser/test_ssa_source_aot_owned_module.c
  - tests/parser/test_ssa_source_aot_scalar_text.c
  - tests/parser/test_ssa_source_aot_descriptor.c
  - tests/acceptance/ssa-aot-owned-module.md
doc_type: acceptance
status: finite-core-owner-lifetime-green
---

# Independent Core owner finite acceptance

## Current decision

**Finite actual owner lifetime GREEN is proved by reviewed r3.**
Clone now performs checked single-arena copying, candidate validation and atomic
publication, with appended OUT_OF_MEMORY=15 and its StatusName case. Successful
MSVC r3 compiles prove only the owner and validator TUs compile. The first direct r3
driver PID73929 naturally terminated with exit 1: configure PID14212 and build
PID28332 exited 0, but the warning gate stopped before CTest. After an independent
review, an actual no-rebuild run on those binaries passed all three selected
no-argument CTest suites: descriptor20 + scalar22 + owner10 = 52 Unity cases,
zero failures/ignored and no observed UBSan diagnostic. Only this finite lifetime
profile is GREEN; production commit identity will be recorded by Root afterward.

Finite genuine RED is already proved by actual r1 and committed as
`511513d459ad4425ff0d0622444f28f137d078ab`. That historical stub returned
UNSUPPORTED (14) without allocation/destination mutation; both prerequisites
passed and both Clone-success assertions failed. The final owner10 run proves
actual 9/8 producer teardown retention, independent ownership, replacement and
bounded refusal atomicity. Allocation failure still awaits its separate gate.

The previous scalar source-text GREEN commit
`f111fb610f2e12748b2df60b7c1a63431f8abf1f` is a prerequisite only: five selected
suites, 42 Unity cases and three standalone regressions within its own finite
scope. Its [acceptance record](ssa-source-aot-scalar-text.md) does not establish
independent ownership. See the [owner module contract](../core-runtime/ssa-aot-owned-module.md)
and [test guide](../../tests/acceptance/ssa-aot-owned-module.md).

## Actual genuine RED r1

Windows x64 clang-cl 19 Debug assertions/UBSan ran directly against the checkout.
Root driver PID86154 naturally terminated with observed exit 0. Configure PID38688,
build PID6876 and prerequisite PID6504 naturally exited 0; feature PID37064
naturally exited 2. Prerequisites reported 2 tests, 0 failures, 0 ignored. Features
reported 2 tests, 2 failures, 0 ignored, each at test line 415:

```text
Expected 0 Was 14:FEATURE RED: actual source module requires independent Core ownership
```

The prerequisites verify actual Core module validity, callable ABI and both full
pure C/LLVM buffers for real 9/8 sources. Build warning count was zero and no UBSan
diagnostic was observed. Source pins, compilation graph and five explicit Core
header pins were checked. The actual owner TU occurred once in Core, with assertions
and UBSan enabled. This is genuine feature RED, not a build/prerequisite failure
or unsupported-host substitute. No new test run was performed for this document.

Reports below are under `E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b`.
The final Root receipt has `validated=true` for the RED gate.

| Actual r1 evidence | SHA-256 |
| --- | --- |
| source-aot-owned-module-red-r1-receipt.json | 5AC6E5B9FFA2CC4304428C719F9EAE346429F096A6DF39619A6CB8FB4F969501 |
| source-aot-owned-module-red-r1-configure.log | C6E8057022A5822ACF79A163EFDB7D37B815F331E9D12324CFD02487AEDF6899 |
| source-aot-owned-module-red-r1-build.log | 384B5192AFF0D406482AFCB57804645AD4F003F90BF042C68C6C14A987D5C893 |
| source-aot-owned-module-red-r1-prerequisites.log | C5DDB2A96630D3F76689B90F5DA4F97B511B95A733C35112CCF8D7CC18251432 |
| source-aot-owned-module-red-r1-features.log | 91C364CE3209A1029AEDAFD8852E9085DEBCCD088DBB5DF79562F7E80523F896 |

| Historical RED source pin | SHA-256 |
| --- | --- |
| tests/parser/test_ssa_source_aot_owned_module.c | 3FB6B64DBA7CA37995132647C08A19C53A41E27B986EF53C04AD0D2947094A32 |
| zr_vm_core/include/zr_vm_core/aot_ir_owned_module.h | 1CE282BD6726FDE921253074AB0EC0D7F6A9130F961A8C43ADB956D130C8904B |
| zr_vm_core/src/zr_vm_core/aot_ir_owned_module.c | DF3E74B094E8AC38AEE37ED5FB03068E2DA15E113B3895243699A199F68411D0 |
| tests/cmake/ssa-source-direct-validation/CMakeLists.txt | C5C64915365A02E3B256A5235DFD321E56AAF791975D06CAFAADC22DD858EE34 |
| tests/cmake/ssa-source-execbc-vm.cmake | 892BCB75D7C4F107BA5F1569B19CB940FC662227A383BF05F7497BD5C152D38C |

The receipt also pins actual source/settings/header TSV metadata,
compile_commands.json and built binaries. The finite RED commit is
`511513d459ad4425ff0d0622444f28f137d078ab`; production GREEN commit is not yet created.
The final no-argument ten-case owner run below establishes its finite functional
gate. No OOM or direct private arithmetic-boundary test was run in this phase.

## Retained failures and actual MSVC r3 compile evidence

MSVC r1 owner PID25308 naturally exited 2 because MSVC C lacked max_align_t;
its receipt has validated=false. The private alignment typedef now uses double
under _MSC_VER, matching actual Microsoft-ABI definitions in MSVC cstddef:30 and
clang 19 __stddef_max_align_t.h:13-14, and max_align_t on other toolchains.
All record alignment assertions remain; unsupported over-aligned records still
fail compilation rather than being silently under-aligned.

Direct attempted GREEN r1 configure PID9740 and build PID34052 exited 0. The
driver exited 1 before CTest because the build emitted 52 warnings in existing
files, with zero warnings in the current finite units. The warning inventory
preserves those diagnostics; no foreign edits or warning suppression repaired
them. Compilation success did not prove lifetime success. There is no whole-tree
warning-free claim. r2 dispatch observation failed: no actual r2 results exist,
and the observed fresh metadata was still r1, so r2 earns no GREEN credit.

Direct attempted GREEN r3 also configured and built successfully, then stopped
before CTest on 17 Parser warnings; owner/scalar/test finite units emitted zero
warnings. Source/header inputs advanced between October 7 and October 8, so these
17 are not declared identical to r1's 52. The failed receipt has validated=false.
Root subsequently checked current source/header pins and obtained a separate
warning review before the successful actual three-suite default run below.
Both failed runs remain historical evidence; the reviewed run reused the binaries
without rebuild/suppression. No whole-tree warning-free claim follows.

MSVC r3 owner PID26584 and validator PID24300 naturally exited 0, each with zero
warnings. They compile the actual checkout owner TU and aot_ir.c using /TC /std:c11
/experimental:c11atomics /W4 /c. Receipt validated=true, compile_only=true and
source_unchanged=true. This is two-TU compile evidence, not full MSVC runtime or
direct lifetime acceptance.

| Retained failure / actual compile evidence | SHA-256 |
| --- | --- |
| source-aot-owned-module-msvc-r1-receipt.json | 8F800087D6FB9B5D6EBDF27A1A04D584B8353BE7FF05A0B3F4230EF2C1F370A4 |
| source-aot-owned-module-green-r1-warning-inventory.json | D8462BA52E50F25B9F01821AA4567787A892DAA489A0C8EEB39DA3B08CE0AB3B |
| source-aot-owned-module-green-r3-receipt.json (failed before CTest) | D7DF32FCC34BBBBCD7035C716A741ADB3D80B76879CB7073FA3B4DEAA4895CF3 |
| source-aot-owned-module-green-r3-build.log | 2F9E3D1B645AB99CCD25E3477C98447014BA7ACD12A4C2B40CC42689B862B812 |
| source-aot-owned-module-msvc-r3-receipt.json | 8D6F95B4C9D968EDB8617FE6093C646947AAAE85416CF414F4370BF64E1435E2 |
| source-aot-owned-module-msvc-r3-owner.log | 402683259F2B83BD6000AB99F8C7CC3C0156C29622B833E71E85F878A090E3E5 |
| source-aot-owned-module-msvc-r3-validator.log | D36D48F4986CF714948B2D3469C48481C60D0AB467AF2B22CAF866BA87EE9113 |

| Frozen implementation source pin (MSVC r3) | SHA-256 |
| --- | --- |
| zr_vm_core/src/zr_vm_core/aot_ir_owned_module.c | 54B3179C21FEF38B1D6528CBE7D430F70FAE12139F92760B655CF188820AAD4A |
| zr_vm_core/include/zr_vm_core/aot_ir_owned_module.h | FC70013A2EE16F42070B56CAAF6C7920B13881421D2C1E9B0FAC6583B112802A |
| zr_vm_core/src/zr_vm_core/aot_ir.c | 89047DDDC5338EA41C63425FA9445051F07AB1664A00AD94457BB01E6E028BC4 |
| zr_vm_core/include/zr_vm_core/aot_ir.h | 53C7554615867DBE79678E28758D5D568BCF72EB9895B5B237866A60DD0D6898 |

## Final reviewed lifetime r3

Root's reviewed native PID75467 naturally terminated with exit 0. Actual CTest
PID27400 completed WaitForExit with exit 0. It reused configure PID14212/build
PID28332's successful actual r3 binaries, without rebuilding or suppressing any
diagnostic. All 48 source/header pins, the entire actual source graph, five
explicit schema headers, four metadata artifacts and three binaries were checked
before and after; the final receipt records sources_and_artifacts_unchanged=true.
The owner TU remains present once in Core with assertions and UBSan enabled.

All three CTest commands used no arguments. Actual descriptor20, scalar22 and
owner10 summaries each report zero failures and ignored cases: 52 total. The
owner suite passes actual 9/8 after producer release, owner-to-owner after first
Free, distinct fresh exact self-source replacement, actual 9-to-8 replacement,
opcode-category/contract error atomicity, NULL arguments and idempotent Free.
Its successful lifetime consumption validates/ABI-checks the retained module and
parses both complete pure C/LLVM buffers after all Parser producers are destroyed.
No source/generated-text execution is part of this proof.

The independent gpt-6-sol review preserves 17 existing Parser diagnostics:
8 unused-function, 4 unused-parameter, 3 missing-braces and 2 discarded-const.
It found no concrete blocker for this bounded gate: range initialization was
complete and the observed const consumers did not write through the arguments.
Current finite owner/validator/scalar/test units emitted zero diagnostics. This
review and successful finite tests do not accept all whole-tree warnings; that
broader warning acceptance remains OPEN.

| Final actual evidence | SHA-256 |
| --- | --- |
| source-aot-owned-module-green-reviewed-r3-r1-receipt.json | B7E56AB14E85CACFD0C840D7B0680C98153306F980D2D3CF1AEC9EC9D1C2A070 |
| source-aot-owned-module-green-reviewed-r3-r1-ctest.log | EF9BB8E8B0EB7286FAB099FFC6D0AF6FD06C11B0B436535A56EBE723DBBB549D |
| source-aot-owned-module-r3-warning-review.json | 36DFDAD661866A433D22CF4755F4B58022EA627747C3099A0805AC1CCF470265 |

The final receipt pins actual build metadata under the existing build directory:

| Metadata artifact | SHA-256 |
| --- | --- |
| source-direct-target-sources.tsv | F19F0CCEF8A2554E80FA3FD0F5D742B21D3F5D7B7A713A6D2A268104BB553541 |
| source-direct-target-settings.tsv | 7F9AADD23E1B73A40A46163B236A68CD7BEA374436E61A00AD513AE1CB3F93AB |
| source-direct-header-hashes.tsv | 124AE07E3FAB9DA92F31F997313CBCFE07455A20B0ABDCC17D14BB6583700A56 |
| compile_commands.json | DED72A25265D685DBCD6A9D625B87A5F756E591217FD690A851533174E12EC89 |

These pins and the three binary hashes in the receipt observe the actual checkout
and built outputs; they are not source snapshots. Root will create the real finite
GREEN commit afterward. This document does not guess that commit identity.

## Actual source and registration route

Sources are compiled directly from `E:/Git/zr_vm`, without snapshots, copies or
worktrees. The existing build location is
`E:/cargo-targets/zr_vm/build/ssa-20261004-01a0fe2b/metadata-guards-direct-v2`;
reports belong under `E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b`.
Root owns execution and exact-path Git work.

The direct target `zr_vm_ssa_source_aot_owned_module_test` compiles the actual
lifetime test and shared literal SCRIPT fixture, uses the existing five-source
scalar support library and common validation settings, and receives the owner TU
from Core's actual source glob. Its normal registration uses the existing
parser/core/library link helper. The owner source is not separately added to the
test executable. Both register `ssa_source_aot_owned_module`; CTest invokes the
binary without arguments. Metadata must establish one actual owner TU in Core
and the genuine checkout paths/settings; the registration text alone does not
establish compilation or runtime results.

## Finite behavior proved after RED

The actual real SCRIPT chain binds module contracts, compacts while retaining
source coordinates, attaches a primitive frame/layout, lowers with canonical
callable facts and builds the host descriptor. The tested success path clones
while those producers live, checks independent copied spans, then releases the
descriptor, projection, compacted function, fixture/compiler and runtime state
before validating/ABI-checking and emitting from the owner view.

The actual owner10 evidence preserves the three CONSTANT/NOP/RETURN instructions,
actual literal/function identity, contracts, target, source maps, frame/value
slots and non-NULL empty logical header with identity/generation. A value-only
witness and Core HashModule support semantic comparisons. Local pointer-bearing
observations measure refusal mutation, not ABI identity. Projection stays
non-runnable; text is parsed as a buffer and never compiled or executed.

Further passing cases cover owner-to-owner cloning followed by first-owner release,
exact self-source replacement with distinct arena addresses, actual 9-to-8
replacement, Core-invalid instruction flags and contract signature perturbations,
NULL diagnostics/required arguments, repeat-safe Free and owned buffer boundaries.
Perturbed producer fields are restored before assertions. The final actual
owner10 log proves these bounded cases; generic nonempty GC/CFG/deopt/multiple-function
fixtures are not present and receive no acceptance from the inventory traversal.

## Explicit OPEN scope

Production Clone, checked geometry and OOM enum/status naming are implemented;
the actual scalar lifetime gate is GREEN. Actual-TU allocator/private-helper
tests belong to the following separate finite gate; no OOM proof exists now. An OBJECT
shim must compile the real owner TU once with local allocator redirection; copied
helper formulas or production .c inclusion do not provide that proof. General
multiple-function/GC/CFG/deopt retention has no acceptance from the finite scalar
inventory. Linux/full MSVC, normal returned-program ownership and the full47
matrix remain OPEN. SourceExecute, oracle, generated-native execution, loader,
network, FFI, provider and security probes provide no evidence in this gate.
