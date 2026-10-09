---
doc_type: acceptance-record
status: finite-red-established
---

# Returned source retention acceptance

Support prerequisites are committed: Function ownership `60df9cb1` (12PASS),
source constant `017b08c2` (24PASS, corrected test `9d8283a6`) and descriptor
allocation diagnostics `7f7f3470` (16PASS). These are finite metadata support
gates, not completion of the full AOT/SSA plans.

## Current C source freeze

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

## Current validation

Historical C RED r2: configure36888/build16460 exit0; CTest37860
exit8. Default25 cases produce10PASS/15functionalFAIL/0ignore. All six actual
prerequisites pass, plus three option-refusal cases and the numeric parse-range
case. Six REQUIRE-return assertions receive the safe NULL refusal; two non-I64
diagnostic assertions see COMPILE3 instead of CONSTANT4; seven injected fault
cases observe zero descriptor attempts. The stub is reached after real compiler
finalization and still reports COMPILE/UNSUPPORTED without lower payload,
attachment or execution. RED failures stop before their GREEN-only assertions;
metadata success and fault cleanup/retry are not credited yet.

Current native build has0warnings and all selected units have actual /UNDEBUG,
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

Independent Sol reviews found no remaining current wrapper/test/registration
defect after corrections. Current test inventory and ledgers remain unchanged
for separate D implementation. Commit evidence is recorded in the active
session note after the actual normal-index transaction completes.

### Current replay after foreign dependency change

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
Finite MSVC r2's selected six sources, headers and objects remain current and
unchanged; the changed closure TU is compiled in the r3 native dependency graph.

| Current receipt | SHA256 |
| --- | --- |
| source-returned-retention-red-r3-receipt.json | `4373550FFADA977B29B692814D8D623819D8C30EDE06F9B25461E0D06873E1F9` |
| source-returned-retention-red-r3-ctest.log | `AA8EF778E34EA0441BB84EC43B65517B52C9BC7659708763AAE5F4A67D98381E` |
| source-returned-retention-red-r3-root-provenance-review.json | `E117BB814AB17AE5B2C26DBBA05EA379969F5358EDFB4476742FA7960CFD4B87` |
| source-returned-retention-red-commit-r1-receipt.json (refused before Git) | `B845D1EFCBBC33B0B8F93D5CCE7097B9F587F0E858BF24CADAA8F1A59224ACA7` |

## Open acceptance

D producer implementation and real returned GREEN are OPEN. Default canonical
returned output, broader source profiles, exhaustive producer/VM/nonlocal OOM,
Linux/full MSVC, backend/native execution parity and all47 SSA leaf gates remain
OPEN. No external loader/network/FFI/security/provider execution was tested.
