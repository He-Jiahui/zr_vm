---
related_code:
  - zr_vm_parser/include/zr_vm_parser/compiler_source_retention.h
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_source_retention.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_source_retention_internal.h
  - tests/parser/test_ssa_source_returned_retention.c
  - tests/parser/support/ssa_source_returned_retention_fixture.c
  - tests/parser/support/ssa_source_returned_retention_fixture.h
  - tests/cmake/ssa-source-returned-retention.cmake
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_source_retention.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
plan_sources:
  - .codex/plans/20261008-ssa-function-retention-next-finite-v4.md
  - .codex/plans/20261009-ssa-source-returned-retention-source-tests-design.md
tests:
  - tests/parser/test_ssa_source_returned_retention.c
  - tests/core/test_ssa_function_aot_owner.c
  - tests/parser/test_ssa_source_scalar_constant.c
  - tests/parser/test_ssa_source_descriptor_oom.c
  - tests/parser/test_ssa_metadata_string_heap_owner.c
doc_type: acceptance-record
status: finite-green-accepted
---

# Returned source retention acceptance

Support prerequisites are committed: Function ownership `60df9cb1` (12PASS),
source constant `017b08c2` (24PASS, corrected test `9d8283a6`) and descriptor
allocation diagnostics `7f7f3470` (16PASS). These are finite metadata support
gates, not completion of the full AOT/SSA plans.
The metadata string-heap lifecycle support is committed separately in `5bd923`;
its six-case gate joins the three gates above in the observed dependency58 run.

## Historical C source freeze

| Input | SHA256 |
| --- | --- |
| Actual compiler wrapper | `BACD01FE10D81D150AC390B7130A9012D0BC372C09F30A783069CABE3877FBDF` |
| Public options header | `E5C62557F76969FDDADEC7007285C68C4DD0668FDD11B1FAD5EE55D506BFD4D2` |
| Private header | `955301145F5E204EB22994B7EBD1A4789A5E9E2E0CCE7B6702530A586A7C75D2` |
| Safe helper stub | `D7CD5EFD6DD3E9537C0C65D74E07F4D44E259D0489A4E8CA90B6117C5C42AEF2` |
| Actual25 test | `2A716B6AAA47522380413A8A027340E5EFA0F24EB0C4FA2D49A7123F6E4A99F4` |
| Fixture C | `6FD4C0E9F2DB547A4E4EF3073AA39EC608C52DDC46E87647FDC58AE001FC1A3C` |
| Fixture H | `A14CD92BF3BBCAF0BFCA41D07ED4DA15CEE9995F27751A541A6CB5A2BE92FBB7` |

Sol independently reviewed current wrapper compatibility/lifecycle and current
test expectations. The non-I64 metadata prerequisite defect was corrected
before running the gate. Diagnostic/source closure reviews are read-only;
actual execution results are required before any GREEN claim.

The first actual r1 run observed25cases/10PASS/15featureFAIL/0ignore, with all
six prerequisites passing and no UBSan/VM ledger error. Its build warned about
an existing unused static borrowed-graph helper. Read-only reference review
found no calls or address uses; Root removed only that whole dead helper and
froze the wrapper above. r1 evidence remains historical; r2 must validate the
current source. Future driver configuration explicitly enables UBSan and checks
actual assertions/sanitizer flags for every selected unit and the link rule.

## Historical C validation

Historical C RED r2: configure36888/build16460 exit0; CTest37860
exit8. Default25 cases produce10PASS/15functionalFAIL/0ignore. All six actual
prerequisites pass, plus three option-refusal cases and the numeric parse-range
case. Six REQUIRE-return assertions receive the safe NULL refusal; two non-I64
diagnostic assertions see COMPILE3 instead of CONSTANT4; seven injected fault
cases observe zero descriptor attempts. The stub is reached after real compiler
finalization and still reports COMPILE/UNSUPPORTED without lower payload,
attachment or execution. RED failures stop before their GREEN-only assertions;
metadata success and fault cleanup/retry are not credited yet.

The historical C build has0warnings and all selected units have actual /UNDEBUG,
UBSan undefined/no-recover flags, including the link driver. No strict VM/CRT
ledger or UBSan diagnostic is present. Source/header/settings/graph/artifact
pins were rechecked with no drift; actual Parser434/Core220/Library40 sources
and two private objects are selected. The map resolves the options wrapper and
helper to current Parser archive members, with no Oracle/network/FFI member.
RED records private compile/link input scope; it does not demand dead-stripped
BuildDescriptor/Clone symbols or imply their execution.

Finite six-unit MSVC r2 module34808/compiler10644/test27752/fixture38476/
descriptor26380/owner28044 all exit0/0warnings. This is compile-only evidence,
not full MSVC or Linux validation.

Reports are in E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b:

| Receipt | SHA256 |
| --- | --- |
| source-returned-retention-red-r2-receipt.json | `419BDC43D6D19EF300FACC64BB17FFEA0DFB484DE9C7D78269B36ED2A3339493` |
| source-returned-retention-red-r2-ctest.log | `84A74A5B1AFBC3E1120A55A062C081DE9FB4CC02C935083BD2BBEC677B3491B7` |
| source-returned-retention-red-r2-root-provenance-review.json | `4312781DB8BE34B5A539F5513A36F57FFE8E1A2C7FFC5CE51F6D41504014F39D` |
| source-returned-retention-msvc-red-r2-receipt.json | `A4FB9CC3D454DB566966D848D06352EE5180751655609B307EFAC2A8EA309F2E` |

Independent Sol reviews found no remaining C wrapper/test/registration
defect after corrections. Current test inventory and ledgers remain unchanged
for separate D implementation. C RED was committed in `e580f350`.

### C replay after foreign dependency change

The first finite commit attempt refused before any Git step because the foreign
`compiler/compiler_closure.c` changed after r2. No index/source/foreign state was
modified. Read-only scope review finds its changed capture paths are reached
only for declared functions or lambdas, absent from these literal inputs; no
external service/provider path was added. The foreign source remains untouched.

Fresh r3 configure17576/build23840 exit0, CTest27760 exit8; again25cases,
10PASS/15samefeatureFAIL/0ignore/0warnings/no UBSan or ledger diagnostic.
All current input/graph/settings/artifact hashes were rechecked without drift.
Current closure dependency SHA is
`ECDC73F15CE48CA0B18A5944948DE15AF5FDBA4CAE55C06E281D6ACCEC22F7B4`.
At the C replay, finite MSVC r2's selected six sources, headers and objects were
unchanged; the changed closure TU was compiled in the r3 native dependency graph.

| Current receipt | SHA256 |
| --- | --- |
| source-returned-retention-red-r3-receipt.json | `4373550FFADA977B29B692814D8D623819D8C30EDE06F9B25461E0D06873E1F9` |
| source-returned-retention-red-r3-ctest.log | `AA8EF778E34EA0441BB84EC43B65517B52C9BC7659708763AAE5F4A67D98381E` |
| source-returned-retention-red-r3-root-provenance-review.json | `E117BB814AB17AE5B2C26DBBA05EA379969F5358EDFB4476742FA7960CFD4B87` |
| source-returned-retention-red-commit-r1-receipt.json (refused before Git) | `B845D1EFCBBC33B0B8F93D5CCE7097B9F587F0E858BF24CADAA8F1A59224ACA7` |

## D source freeze and observed GREEN

| Input | SHA256 |
| --- | --- |
| Implemented private helper | `BF611B6260B3091448C554E3D536911771310404A91465CD4F55FCB3DEE40AF6` |
| Public options header | `2109B18B19BE1411FB6BAB5E63CCA2C40629C3F6DAB4ACA50953FF61E5D28E3F` |
| Private helper contract | `AF7A80F52A79360BD978F395370E001080FEF1F2E78A48D7558EEC95572B7C2B` |

The wrapper and frozen test/fixture hashes remain those listed in the C table.
Read-only review checked actual lower calls, range geometry, exact diagnostic
copies and zero-initialized cleanup. The helper runs the actual producer chain
and publishes one independent Function owner after descriptor/ABI validation.
Bool append refusal remains PRODUCER_FAILED without a fabricated OOM record.

Actual D GREEN r2 configure7732/build34468/CTest39472 all exited0. Default25
cases observed25PASS/0FAIL/0ignored, with no strict VM/CRT ledger or UBSan
diagnostic. The formerly uncredited metadata/lifetime and seven fault
cleanup/retry assertions now ran successfully in that run.

Dependency r4 observed58PASS across four CTest gates: Function12, scalar
constant24, descriptor16 and metadata lifecycle6. It was current at run time.
The r4 incremental build emitted0warnings, but dependency r3 inherited an unused
static `parser_types` warning. This is not a warning-free whole dependency graph
claim. Its r3 build log SHA is
`7ADB91492AA39B8CE427D73C6E904D6226B6994F9B5A6334052E358193CD6945`.

Fresh finite six-unit MSVC GREEN r3 processes3476/39576/34852/27804/15028/39236
all exited0 with0warnings. It covers helper, compiler wrapper, test, fixture,
descriptor and owner only; it is compile-only evidence.

| Observed receipt under the report root above | SHA256 |
| --- | --- |
| source-returned-retention-green-r2-receipt.json | `FD5A3E1CE2690F52BE263EEF7A6D2CF8659EDB52B8E3F2AEFB99C800F9AB3C2F` |
| source-returned-retention-green-r2-ctest.log | `0A4A50A18E09D1A00896F7AEA5F72F80563EE0457350479B92D71D4CB87D10E7` |
| source-retention-dependencies-r4-receipt.json | `42ADA88A48442D160D6B2D2EE0921005DA8CCF86AE2D4ED4E580383F117ACF63` |
| source-retention-dependencies-r4-ctest.log | `C7724065A6628036DCD95C591F5D8A6DAC66FD8C765C8C1F23A49DFF4AC750A8` |
| source-returned-retention-msvc-green-r3-receipt.json | `457F1F1E3790D725E268377D502E6E77EFBE2F69D945C81F1CECAABD15243023` |

At the observed r2/r4 runs, subsequent Root proof failed on foreign property
source drift, and four foreign dependency sources changed after dependency r4.
Later inspection also found a call-arguments source change after returned r2;
these observations are historical drift reports, not a current complete list.
Those runs did not establish current provenance. Their raw results and failed
proof attempts remain historical; foreign sources were preserved rather than
restored to prior pins. The fresh accepted replay is recorded below.

## Accepted D replay

Dependency r6 configure37788/build17164/CTest32464 all exited0, with exactly
58PASS/0fail/0ignore across Function12, constant24, descriptor16 and metadata6.
Root separately read all named cases and four raw provider maps, then checked
current source/header/settings/artifact pins before and after its review.

Returned GREEN r3 configure25972/build40708/CTest37628 all exited0, with
25PASS/0fail/0ignore. Root current provenance review confirms all selected
compile units use assertions and UBSan undefined/no-recover, the link rule
enables UBSan, and the two private objects supply BuildDescriptor and
CloneOwnedModule. The actual wrapper/helper resolve from the current Parser
archive. No Oracle/network/FFI/runtime-support map member is selected, and no
strict VM/CRT ledger or UBSan diagnostic appears.

The frozen 25-case test is unchanged from C RED. Metadata survives actual
CompilerState/AST cleanup; independent owners, explicit/free versus shutdown,
16 alternating compile/free calls, six descriptor faults and the final owner
fault all complete their cleanup and clean retry assertions. Generic append
OOM, nonlocal VM exceptions and the full producer fault denominator are not
claimed. The earlier six-unit MSVC GREEN r3 header pins became stale at
parser_internal.h, while dependency r6 and returned r3 source/header pins stayed
current. Root refreshed only the finite MSVC gate: GREEN r4 processes
17964/19212/22988/37280/25960/6304 all exited0 with0warnings. It remains
compile-only evidence; no optional runtime gate was broadened.

The dependency r6 incremental build emitted no warning, but two actual prior
diagnostics remain disclosed: unused wrap_type_in_task_identifier in
parser_types.c and unused parse_type_literal_expression in parser_expressions.c.
Root and Sol verified each helper has no calls or address references in HEAD
or current source. Only these exact pinned diagnostics are admitted; other
warnings fail. No compiler warning flag or foreign source was changed.

| Accepted receipt under the report root above | SHA256 |
| --- | --- |
| source-retention-dependencies-r6-receipt.json | `633926A3BBD52035863E7B8974837D4699D2D49E26CDE7BBFB0097CC30C66C54` |
| source-retention-dependencies-r6-ctest.log | `CC1A26C6A054FB5CEA85BE99BC12C56B2C4E272C934A88F4AC0F689EDE6F5B18` |
| source-retention-dependencies-r6-root-review.json | `B50B203EA1D152CD1099DBD6D1E4E39E7FEC3F71C1444FB804E929F064744D1F` |
| source-returned-retention-green-r3-receipt.json | `9D02F467E3624C6AFA43D1F9C0D5E9FCF8161B84EFAEB13B48A6C5C4F5E35F6C` |
| source-returned-retention-green-r3-ctest.log | `689FE5AB2809BEEA70CF8B0845C1C3E985B1A9C4C624CD8F522AF8BE2FB5B06C` |
| source-returned-retention-green-r3-root-provenance-review.json | `FB7011F1CA00166769B4EBD6A13739BEF7D4A30D67112915041AA566DE23CFD5` |
| source-returned-retention-msvc-green-r4-receipt.json | `89BF070832532FBD6D3AA2A11121FA8C547DD931F12103CB3455C373B3303D24` |
| source-retention-dependencies-r5-build.log (two reviewed warnings) | `3217FBA530289397E2070ADD3DADF7087FA4FEAA0A2C84F07DB59E7087C77892` |

Each gate pins actual checkout inputs without copying source. Reconfiguring
the reused build tree for the subsequent returned gate changes CMake metadata;
the earlier dependency review records its own configuration at review time.
Its shared production source/header pins must still agree with the returned
gate at finite commit. Root serializes the exact six-path He-Jiahui commit
after this accepted evidence and records its transaction in the session note.

## Open acceptance

D producer implementation and explicit finite returned GREEN are accepted.
Default canonical
returned output, broader source profiles, exhaustive producer/VM/nonlocal OOM,
Linux/full MSVC, backend/native execution parity and all47 SSA leaf gates remain
OPEN. No external loader/network/FFI/security/provider execution was tested.
