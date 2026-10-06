---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_source_module_contract.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_module_contract.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_entry_metadata.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_entry_metadata.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_aot_projection_descriptor.c
  - tests/parser/test_ssa_source_aot_descriptor.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_entry_metadata.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_entry_metadata.c
  - zr_vm_parser/include/zr_vm_parser/exec_ir_source_module_contract.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_module_contract.c
plan_sources:
  - .codex/plans/20261006-ssa-source-module-contract.md
  - docs/plans/ssa/07-aot-backends/01-aotir-contract.md
tests:
  - tests/parser/test_ssa_source_aot_descriptor.c
  - tests/acceptance/ssa-source-aot-descriptor.md
doc_type: acceptance
status: finite-source-aot-descriptor-green
---

# Real Source Module Contract and Descriptor Acceptance

## Finite functional decision

**Finite original module binder/descriptor GREEN after committed actual RED.**
The [module contract](../parser-and-semantics/ssa-source-module-contract.md)
and [test guide](../../tests/acceptance/ssa-source-aot-descriptor.md) define the
finite original-module binder and borrowed AOT descriptor gate.

Root route `01a0fe2b-2f6b-7063-9a67-ac0e08c0d82d` builds `E:/Git/zr_vm` directly
at `E:/cargo-targets/zr_vm/build/ssa-20261004-01a0fe2b/metadata-guards-direct-v2`,
without source snapshots. Report names below are relative to
`E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b`.
Windows x64 clang-cl 19 Debug assertions/UBSan is actual dynamic evidence.
Preceding host GREEN `6bdedf45c1b202cd1f2c1f4164bc0c235611fc46` and its
`6DD930B18D2F1F395055C6185EF7B1994EAD0ECE90F26F191922133CDF2C7BDA`
commits receipt retain their historical scope.

## Genuine RED

RED commit `12fee64a7297864aa8adcb074b46afe0e60fd8be`, parent
`6bdedf45c1b202cd1f2c1f4164bc0c235611fc46`, contains exactly five authorized
paths, author/committer He-Jiahui <814111601@qq.com>.
Configure/build/prerequisite execution exited 0. Two actual 9/8 prerequisites
passed after proving unbound BuildDescriptor VERSION_MISMATCH. Feature mode
exited 2; both cases failed only the FEATURE RED binder bool assertion at test
line 339 against the linkable UNSUPPORTED stub. No UBSan diagnostic was observed.
Default20, its repeat/replacement case and 15 refusal cases/16 calls were not
executed at RED. Native driver 19349 was observed naturally terminal exit 0.

| Frozen RED receipt | SHA-256 |
| --- | --- |
| `source-module-contract-red-receipt.json` | `F4B3D56A2A87AF15656640BDCD43203006867544F2B4B9895342397BEAB6C5CD` |
| `source-module-contract-red-commit-receipt.json` | `D8D174758A1CB66923A0ACD836514C90F811068CCD4671E4E9E9B80B949E1F5E` |

## Functional GREEN and retained warning repair

r1 functional results passed, with four signed-comparison Clang warnings in
SOURCE_RANGE. Its retained receipt is `source-module-contract-green-receipt.json`,
SHA-256 `EB9440F6327BB7426D2C8C80B95FD3689DC07D02B69E4A3428D03DEB6E947C76`.
Both uint32 source identity and int32 AST coordinate are widened to signed64
for the comparison. Independent review found no broadened admission; checks
were retained. The current producer SHA is
`7AC91D0229A5BB7C65F94C06F3AEF4A41879902E433AB7F943014D5ED9B06BD3`.

r2 configure/build/four CTest exited 0. Native driver 38840 was actually observed
naturally terminal with exit 0. All 55 Unity cases passed: descriptor20,
host14, entry tokens8 and entry identity13, with zero failures, zero ignored,
zero Clang warnings and no observed UBSan diagnostic. The descriptor suite
contains two prerequisites, two features, one repeat/failed-owned-replacement
case and 15 negative cases with 16 refusal calls.

The real original module is bound before compaction. Existing BuildDescriptor,
Core ValidateModule and RequireExecutableAbi pass on the real 9/8 metadata,
while projection.runnable stays false. Opaque hash-helper allocation failure
was not injected. Failure atomicity is limited to the binder publication
boundary: no fallible operation remains before two final assignments. It does
not promise allocator bookkeeping rollback or function-body equivalence.

## MSVC recovery and final compile-only proof

The r2 MSVC driver joined `/Fo` output and source into one argument; actual
D8003 exited 2. r3 corrected transport but both actual translation units hit
C1189, exit 2, because the C11 atomics support option was missing. Logs and
smoke receipts retain those failed driver/setup attempts; neither was source
GREEN. r4 added the repository's existing CMakeLists.txt:38 requirement
`/experimental:c11atomics`, without source changes.

Both r4 `/c` compilations passed with exit 0 and zero warnings. Binder PID35048
and helper PID9436 were actually observed naturally terminal via WaitForExit;
Root observed the aggregate smoke driver completed with exit 0. Argument
transport was ProcessStartInfo.ArgumentList. This is only two changed TU
compile evidence, without executable/native or full MSVC matrix acceptance.

The final r2 functional receipt includes actual native38840 terminal0 and r4
aggregate0. Its 37 current records (21 sources, three metadata, four binaries,
three functional logs and six MSVC source/log/object records) matched their
pins. Retained RED phase logs also matched; old source/binary/metadata pins
are historical identities of superseded artifacts. No GREEN commit identity
is asserted before Root actually creates that commit.

| GREEN/recovery receipt | SHA-256 |
| --- | --- |
| `source-module-contract-green-r2-receipt.json` | `C33BE5BF92A63C640E51FFB45B68408E31CAAF41875D6C3F733A95090E1038C6` |
| `source-module-contract-green-r2-msvc-smoke-receipt.json` | `A7D69461BDB38C769B68134AD924DD568740DE39DD553846E1B69104E49E6BC4` |
| `source-module-contract-green-r3-msvc-smoke-receipt.json` | `EFAF556C306DBE5A454C4D621854A79DD819A706D723B5442CCB4DC1074601BC` |
| `source-module-contract-green-r4-msvc-smoke-receipt.json` | `6647271659F7E8A0B790B78F54ABE3100A56067E2AE97D4388367A5A8FCDAC8A` |

| Current functional/MSVC artifact | SHA-256 |
| --- | --- |
| `tests/parser/test_ssa_source_aot_descriptor.c` | `FE845B5DC76B4CE40732E72196B4FA69520950B57EFA71B03904F84018B704E1` |
| `zr_vm_parser/include/zr_vm_parser/exec_ir_source_module_contract.h` | `8423ED686FF460510BD0E0375C34079210A8F8886C49D79DB165223415CB01A1` |
| `zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_entry_metadata.c` | `0376F7EEEA17E5F6BEB048D30C1D164DCCEAC19D668998BF53D86EB9845648EC` |
| `zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_entry_metadata.h` | `2CDD09B62F14E96C7148C21408B1DA6967812281218A2CFC71792414C9F03082` |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_module_contract.c` | `7AC91D0229A5BB7C65F94C06F3AEF4A41879902E433AB7F943014D5ED9B06BD3` |
| `source-module-contract-green-r2-configure.log` | `48352D79545D8175F73BA65A66A95115BB00A9BC4DBA2E447A71ADE41F8E1011` |
| `source-module-contract-green-r2-build.log` | `A810C45A79B4EBD5E55A81EA86FD50DAEF8B3E65684D1BEA092D58CB3FCD49A0` |
| `source-module-contract-green-r2-ctest.log` | `79BEC8B77DD76715DB97CD2CBBFCE6447173B63C709B242863638BE93FA47BC8` |
| `source-direct-target-sources.tsv` | `B2BC1B263D31F046601B880DFB56753C2A928078BEF77A2E3F2A730E33F8D23D` |
| `source-direct-target-settings.tsv` | `7E337CFCA1660D8CB8425DD173A8C0219E0A4E71E8C68C528E0169E467BCBDEB` |
| `source-direct-header-hashes.tsv` | `977E6767724D29733C00686055E940B76170F4398D0D1051582A9214910BF5FF` |
| `zr_vm_ssa_source_aot_descriptor_test.exe` | `BE1708DB24142739BB42D4C8E0A26C0ED9B857421FA377D61864C62B8B7D23FA` |
| `zr_vm_ssa_host_noargs_i64_aot_target_test.exe` | `9C5240567A1DEB48727D7876962542F6C74DC65FC1AE870B4BD33B875F26BDAD` |
| `zr_vm_ssa_source_script_entry_tokens_test.exe` | `FF4E89D47CBBAD3535BAE4143C4C5D905C80316D1BDA673EE4052D7D6D8E9544` |
| `zr_vm_ssa_source_script_entry_identity_test.exe` | `AD4D8E694E696C05DCA5ED407F4102F5D72195FDBB2F87C6AE38C818D7ECD06E` |
| `source-module-contract-green-r4-msvc-exec_ir_source_module_contract.log` | `028D2F03ADB142B0C1745A8348E38E65917D0933EF4EE67C3EBE84C291372F33` |
| `exec_ir_source_module_contract.obj` | `A0683B4116C2AB33D08F83B1E026B915F19D1F6A1B892BCB31EE91AAD771CD54` |
| `source-module-contract-green-r4-msvc-compiler_script_entry_metadata.log` | `0755485F3F96A00DC5DA8DED393974561FB76D5F6DA6DF803307973B10FD3122` |
| `compiler_script_entry_metadata.obj` | `81FD267D6869F9BE494585D15554DA38AFE2113F6332F58BE1B28993DE170A66` |

## Contract and owner boundary

SCRIPT entry ABI, MODULE blob and canonical callable structural hashes are
three distinct identities. module layoutHash zero means no separate module
physical layout; it is not the frame hash. Generation comes from actual Core
entry generation1, not source moduleVersion or runtime metadataGeneration.
Source record/heap ranges are checked before blob/paired-row reads and reused
helpers validate signatures and recompute hashes. The existing entry ABI helper
has only const qualification in its declaration/definition.

Descriptors borrow projection views; source/projection owners remain live,
and descriptors are released first. True NOP/IDs/maps/row/frame/state metadata
are preserved. A valid descriptor ABI is not source/backend/native execution.
Normal returned artifact retention is not established by this fixture lifetime.

Real-frame scalar emission, source/Oracle/backend/native execution, normal
returned artifact retention, Linux (prior WSL E_ACCESSDENIED), full MSVC matrix,
complete 07.01 migration and full SSA47 remain OPEN. No network/import loader,
FFI, provider, capability, security execution or external delivery is implied.
