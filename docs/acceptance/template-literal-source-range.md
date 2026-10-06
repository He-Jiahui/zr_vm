---
related_code:
  - zr_vm_core/include/zr_vm_core/gc.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_literals.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
  - tests/parser/test_template_literal_source_range.c
  - tests/parser/test_expression_fragment_parser.c
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
  - tests/cmake/ssa-source-execbc-vm.cmake
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/parser/parser_literals.c
plan_sources:
  - .codex/plans/20261005-ssa-template-ast-ranges.md
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
tests:
  - tests/parser/test_template_literal_source_range.c
  - tests/parser/test_expression_fragment_parser.c
  - tests/parser/test_ssa_source_range_identity.c
  - tests/acceptance/template-literal-source-range.md
doc_type: acceptance
status: finite-template-ast-root-range-green
---

# Template Literal Source Range Acceptance

## Finite state and direct environment

**Finite template AST root range GREEN.** This gate is separate
from frozen [multiline token acceptance](ssa-multiline-token-coordinates.md).
The [module contract](../parser-and-semantics/template-literal-source-range.md)
and [test guide](../../tests/acceptance/template-literal-source-range.md)
define the six raw-byte and actual AST expectations.

Route `01a0fe2b-2f6b-7063-9a67-ac0e08c0d82d` uses `E:/Git/zr_vm` directly,
build root `E:/cargo-targets/zr_vm/build/ssa-20261004-01a0fe2b/metadata-guards-direct-v2`
and reports `E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/`.
No source snapshot was created. Windows x64 clang-cl 19 Debug assertions and
UBSan are actual local evidence; Linux/full MSVC acceptance remain OPEN.

## Failed prerequisite retained

The initial uncommitted fixture incorrectly passed `SZrGlobalState *` to
`IgnoreObject(SZrState *, ...)`. Configure/build exited 0, but emitted a
compile warning and execution halted with a GC misaligned-access UBSan
diagnostic. Native job 25627 naturally exited 1. This was a fixture
prerequisite failure and supplies no AST source-range RED. The earlier static
claim of no blocker is withdrawn by the actual failure. Root corrected only
the IgnoreObject argument to `g_state`; the distinct UnignoreObject global
owner remains unchanged.

Retained `template-ast-prerequisite-rooting-failure-receipt.json` SHA-256:
`BB4C12F978F0BCD084E8CDA579D28762F41AEF17A765BD46CF5FF543CCEE60DF`.
The historical bad fixture SHA-256 was
`61CE42AAD8EB237DE7295B6380FBCE9B85AB82FB83B54BFAA042E8FACE654A56`;
its historical executable was
`7FA0FACEF03017246051C96616DDB96DE26182E214861271B42B47451F16ABC5`.
The receipt also retains configure, build, prerequisite logs and metadata
pins. Those superseded fixture/binary identities are historical, not current.

## Corrected actual RED

Receipt `template-ast-red-r2-receipt.json` SHA-256:
`6A17DD189EBEF0E0C67527DD54FBF9078CC221CC895808A8812EA54E21C9E24E`.
Its base HEAD is `b3a1692666b980ad4213e66dde73a6b6f9860296`.
Native driver 70594 naturally exited 0; configure/build each exited 0.
Corrected prerequisite mode passed six cases. Two existing CTest entries
passed 32 cases: 17 range and 15 expression-fragment consumers. Full new CTest
exited 8, six cases/six failures/zero ignored, all at the final AST start
offset assertion; no UBSan diagnostic was observed.

| Case order | Expected start | Actual start at RED |
| --- | --- | --- |
| single line suffix | 7 | 12 |
| LF suffix | 7 | 16 |
| CRLF suffix | 7 | 18 |
| bare CR suffix | 7 | 16 |
| EOF simple fragment | 0 | 3 |
| EOF interpolated fragment | 0 | 8 |

| RED artifact | SHA-256 |
| --- | --- |
| Corrected fixture | `C65FDFA7119814B965DCA6628AC0CAF31B7424C5B1636672D21567D23337FD91` |
| Historical `parser_literals.c` | `346FD4735FEDDE36C005B9D3C559715FD4643CB8343C1AA9A431291221E9DA13` |
| `template-ast-red-r2-configure.log` | `77B4A0804B565A87845E411B748616126022C0F293DEDA6F43FDDE8F7E5A272F` |
| `template-ast-red-r2-build.log` | `05535D10BBF49B3628BE212ABA85F01AACB266BAAD073EB5284BF385EAEA6ED0` |
| `template-ast-red-r2-prerequisites.log` | `299D70F872C3BEB9FEE7A819339F224193AA45E025C44475BD46D56EA0A872D8` |
| `template-ast-red-r2-baseline-consumers.log` | `B6F3353851E520AA88DA3631C009257EBB17BA07A7968F617179E6781FEA5BAF` |
| `template-ast-red-r2-ctest.log` | `50D31663D4A4E15FBF3FBA29CE5C372703D06A21F07673C581000E59A17AD602` |
| Historical template executable | `950402ED6B0BD72DA957B85A36E6051A6B1FD3199EDA73223085CB163184A1F3` |
| RED `source-direct-target-sources.tsv` | `D020F228FC5E731D0711A4DC533094FDAEC45E4E6069A98D579FF7B0465EB340` |
| RED `source-direct-target-settings.tsv` | `B09C262F8BA6C7B6CFD638A3050AC495A2FE7F8EB5E0972D9878E8DAF57186DA` |
| RED `source-direct-header-hashes.tsv` | `A5874FA4DC76D8A075293A6AE59E32F7C29A04FDC43ADCB4E3A06FAE675DA78B` |

Corrected real RED commit is `b0d145f7efaf7a7610f997c8235efc8f3142c899`,
parent `b3a1692666b980ad4213e66dde73a6b6f9860296`, author and committer
He-Jiahui <814111601@qq.com>. Native commit job 23242 naturally exited 0.
The actual GREEN evidence below follows this independent RED commit.

## Actual GREEN and bounded MSVC evidence

Native driver 62669 naturally exited 0. Configure/build/CTest each exited 0.
Three CTest suites passed 38 Unity cases: `template_literal_source_range` six,
`expression_fragment_parser` 15, and `ssa_source_range_identity` 17. There were
zero failures, zero ignored cases and no observed UBSan diagnostic. The only
production change restores `literalLoc` on a nonnull returned template root.
The corrected test fixture remains unchanged from RED.

MSVC 19.44 `/c` compiled the same changed `parser_literals.c` successfully,
with zero warnings, using `ProcessStartInfo.ArgumentList`, `UseShellExecute`
false and the native argv serializer for quoted macro definitions. Its PID
13944 terminated naturally with exit 0. This is single-TU compile-only
evidence; full MSVC matrix remains OPEN. It establishes no executable result.

The GREEN receipt refers to the RED commit and source pins. The subsequent
GREEN commit is recorded separately by Root, avoiding a receipt/commit hash
cycle. RED producer, executable and target-sources metadata identities above
are historical: GREEN superseded those artifacts in place. The retained RED
receipt/logs remain the authority for that historical execution.

All GREEN receipts, seven source pins, three phase logs, three metadata files,
three binaries, MSVC log and object were hashed against their receipt pins.

| GREEN artifact | SHA-256 |
| --- | --- |
| `template-ast-green-receipt.json` | `D8CA34E104D0B25A649278D6ADC5A32488B63294B6C7907EF642E8869C56403F` |
| `template-ast-green-msvc-smoke-receipt.json` | `7B6649BA159B0876A97FE51EBE0B0CA02815184092BEB841993DF3F8A37486EA` |
| `test_ssa_source_range_identity.c` | `FBB42872EEE119E28D96E33C075F7A20CCE1159A5773E5536A230603E533D1C8` |
| `test_template_literal_source_range.c` | `C65FDFA7119814B965DCA6628AC0CAF31B7424C5B1636672D21567D23337FD91` |
| `test_expression_fragment_parser.c` | `0E91FC5AB246802B135C0561AEF4DE5007B4C7E3A008249EEA9F097B3E4BF056` |
| `ssa-source-execbc-vm.cmake` | `14072B3295E0D76E026B7B90E8F317CC892CA0BF378119F4B6E1D3D1772ABED0` |
| `tests/cmake/ssa-source-direct-validation/CMakeLists.txt` | `21BE5EEBCB41AB266D7E1409672ED8AF29D4A4ECDF961B27B93831ED75E7CF86` |
| `parser_literals.c` | `F0046032DD2986695F745717ECFB259C93592299ADCEA013789CA8DBA0B1BF8C` |
| `parser_state.c` | `05D2A8889CE2564A70D16781F110FB7DB9234BA92D122464ADA1519BC4620204` |
| `template-ast-green-configure.log` | `ED0D0E6036E885818362AFA34F812DB9A9802C5E62444E90C405EEE02B2FFD85` |
| `template-ast-green-build.log` | `090EE12CAEFACAA38201D13B63BE2197D64F33A136AF804E5AA6D715AC95D03D` |
| `template-ast-green-ctest.log` | `982D7D091ADC9A4959F05A058C773BB6F3D8729E709A9AE742D15B35ECCCB333` |
| `source-direct-target-sources.tsv` | `1BAA85405F43A92178731CB7F62E28FD0C5721023E784B2DC02244892C85B092` |
| `source-direct-target-settings.tsv` | `B09C262F8BA6C7B6CFD638A3050AC495A2FE7F8EB5E0972D9878E8DAF57186DA` |
| `source-direct-header-hashes.tsv` | `A5874FA4DC76D8A075293A6AE59E32F7C29A04FDC43ADCB4E3A06FAE675DA78B` |
| `zr_vm_template_literal_source_range_test.exe` | `BBC44E35413F0B4720FC8052B48F2BA5320E89D4C8FAB3069AA41BCAC7FDEEE6` |
| `zr_vm_expression_fragment_parser_test.exe` | `A66D85CDF1E56DCC7AD05EF03611ED2DA6E016DCBA5D85DCA08C8F4E25F9603C` |
| `zr_vm_ssa_source_range_identity_test.exe` | `FB5F9530F6723CF0E6022137003D0014340DB10A270BCDDC80DEA3F571A8964D` |
| `template-ast-green-msvc-smoke.log` | `89FBAF07A34A8AE4C046D775D7642F874686EB5A69B256FA6C4E5DC6FC93A9D0` |
| `parser_literals.obj` | `4FE67B1815DA5C39B4A2FFB2661FDD805B06C16297AD48669F549FE4DCE61CEA` |

## Remaining OPEN gates

Segment child ranges, parser/OOM, GC stress, full Linux GCC/Clang (existing WSL
E_ACCESSDENIED), full MSVC matrix, offset-cache/snippet CR behavior,
descriptor/native source execution, retention and full SSA47 remain OPEN.
The new six cases only parse and scan. The 15 existing fragment consumers also
use local type environments/inference/semantic facts, AST import normalization
and compiler init/free, without foreign resolution or source execution.
No network, FFI, provider, capability or security evidence is added.
