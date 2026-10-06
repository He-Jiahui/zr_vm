---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_host_aot_target.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_host_aot_target.c
  - tests/parser/test_ssa_host_noargs_i64_aot_target.c
  - tests/parser/support/ssa_literal_script_fixture.h
  - tests/parser/support/ssa_literal_script_fixture.c
  - zr_vm_core/include/zr_vm_core/aot_ir.h
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_host_aot_target.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_host_aot_target.c
plan_sources:
  - .codex/plans/20261005-ssa-host-aot-target.md
  - docs/plans/ssa/07-aot-backends/01-aotir-contract.md
tests:
  - tests/parser/test_ssa_host_noargs_i64_aot_target.c
  - tests/parser/test_ssa_host_primitive_layout.c
  - tests/parser/test_ssa_primitive_source_frame.c
  - tests/acceptance/ssa-host-aot-target.md
doc_type: acceptance
status: finite-host-aot-target-green
---

# Host No-Argument I64 AOT Target Acceptance

## Finite decision and environment

**Finite Win64 no-argument I64 target GREEN.** This produces the existing scalar
Core target record from a real canonical callable and a recomputed actual host
layout witness. The [module contract](../parser-and-semantics/ssa-host-aot-target.md)
and [test guide](../../tests/acceptance/ssa-host-aot-target.md) define the scope.
This restricted ABI fingerprint is independent of Common's full platform ABI.

Root route `01a0fe2b-2f6b-7063-9a67-ac0e08c0d82d` builds `E:/Git/zr_vm` directly,
without source snapshots. Build root is
`E:/cargo-targets/zr_vm/build/ssa-20261004-01a0fe2b/metadata-guards-direct-v2`;
reports below are relative to `E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b`.
Actual dynamic environment is Windows x64 clang-cl 19 Debug assertions with
UBSan, `halt_on_error=1:print_stacktrace=1`. Linux remains OPEN from previous
WSL E_ACCESSDENIED, without a new retry.

The preceding finite template AST gate at
`a986da50b7d68ada4b17e175eaed99d99c40f9b0` retains its independent historical
proof. It does not establish this target or any later backend dependency.

## Actual RED and retained recovery history

RED commit is `ebb1db928eb01dc6351d3b659971423d4846e2c4`; the RED receipt's
base HEAD is `0d715d25b171a05e26c14b36e398f504fa863929`. Configure/build and
`--prerequisites-only` exited 0, with two real 9/8 prerequisites passing.
`--features-only` exited 3: three feature cases failed target success against
the explicit UNSUPPORTED stub, zero ignored and no observed UBSan diagnostic.
The full default/refusal suite was not run at RED: nine refusal cases and
11 calls were withheld. Stub source and header pins below are historical;
current sources and binary superseded them without source copies.

Before GREEN compilation, Root's validation driver hit a PowerShell parser
error, exit 1, Unexpected token `-V`. A JavaScript replacement interpreted
`$'` inside replacement regex text; a literal replacement callback corrected
it. Configure/build/tests/MSVC had not started, and source/index were unchanged.
This driver failure is retained separately and is not a producer or feature RED.

Initial functional GREEN passed the three suites, but MSVC `/c` emitted five
C4127 constant-conditional warnings at the host fact checks (lines 113, 116,
119, 122 and 125). Its actual exit was 0, not warning-free acceptance. The r1
functional and smoke receipts remain retained. The producer introduced five
actual observed host-fact locals; it removed no check and suppressed no warning.
The final 288-line TU was then verified again as r2.

## Actual r2 phase results and observation limit

r2 configure, build and CTest each exited 0. Three CTest suites passed 48 Unity
cases: 14 `ssa_host_noargs_i64_aot_target`, 15 `ssa_host_primitive_layout`,
19 `ssa_primitive_source_frame`. The target split is two prerequisite, three
feature and nine refusal cases; the latter contain 11 refusal calls. There
were zero failures, zero ignored and no observed UBSan diagnostic.
MSVC 19.44 compiled the new TU with `/c`, naturally terminal exit 0 and zero
warnings. Its argument transport was `ProcessStartInfo.ArgumentList` with
`UseShellExecute=false`. This is single-TU compile-only evidence, without
executable or full MSVC matrix acceptance.
The MSVC receipt records PID 5016 and WaitForExit's naturally terminal result.

Native session 64586 observation was interrupted by user goal continuation.
On recovery the tool returned `Unknown process id 64586`; Root confirmed the
handle was absent and did not restart the job. The internal phase exits and
MSVC natural terminal state are retained in receipts. No observed aggregate
r2 driver exit code or natural-terminal output is claimed. The receipt's
prospective `native_job_state` wording is not an observation of that result.

All seven receipt hashes, current r2 source/log/metadata/binary pins, the MSVC
log/object and retained RED phase logs were independently checked. Historical
r1 source/object and RED binary/metadata pins are identities of superseded
artifacts, not assertions that current files still match them. A GREEN commit
will be recorded separately by Root after the actual commit, avoiding a hash
cycle.

## Frozen schema and pinned evidence

The fixed `x86_64-pc-windows-msvc` triple is 22 ASCII bytes. The triple payload
is 52 bytes: 22 domain bytes + u32 schema + u32 count + 22 triple bytes.
The ABI payload is 88 bytes: 28 domain bytes + two u32 prefix fields + u64
triple hash + eleven u32 ABI facts. Neither hash includes NUL, struct padding,
addresses, local IDs, row IDs, structural hashes, literal bits or frame hash.
Zero hash is refused, never converted to 1.

| Receipt | SHA-256 |
| --- | --- |
| `host-aot-target-red-receipt.json` | `7875C47A07E99B3F5881A5DE1230A06515337A15BA9A3047D654B432F289CE78` |
| `host-aot-target-green-driver-argv-failure-receipt.json` | `4A2A86746D644536F5EB0243A85170CD114BDB03CBD7836A7CA7D09D85C9ABE8` |
| `host-aot-target-green-receipt.json` (r1) | `B5E7ABE9940DB188C1EBD1E696D043520E829A97464D0806090C8C36286C8EC0` |
| `host-aot-target-green-msvc-smoke-receipt.json` (r1) | `FB7D6C1C648288103989636C3504F206A02F856E19C5199BB39D73C5CC28730A` |
| `host-aot-target-green-r2-receipt.json` | `10220576835A56CC7C48AC874DA9D7F2065F1807F8F04F4634526E519121FAB1` |
| `host-aot-target-green-r2-msvc-smoke-receipt.json` | `35404805FF175516D70E8D30B75600F5D1580FC518B2DCFB3E3C7041DB4B0FEA` |
| `host-aot-target-green-r2-observation-receipt.json` | `E5FED6731366E50F82DF361572FD93D7D0BFA55449663BFCFFDB721F1A8824F0` |

| Historical RED artifact | SHA-256 |
| --- | --- |
| `zr_vm_parser/include/zr_vm_parser/exec_ir_host_aot_target.h` | `9B4F87A17A320AABCEC9B466C9410CA7171836237647E71EFD15040E307375E2` |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_host_aot_target.c` | `714564E25ACF7490EA15184C20257E05842003274E6413ACAD190FB01CEDF94F` |
| `host-aot-target-red-configure.log` | `DD9C46E9865602E0462724E06895BD17191E0465DA691FC123040950612DB8EB` |
| `host-aot-target-red-build.log` | `ECF38FBC17FEC7BA55A4BD5BB29C225FF5C445A8A51BD4AC12A70A435ED7FD43` |
| `host-aot-target-red-prerequisites.log` | `0AB535D55589B8AD4133B9B5B205FCB9680115357E27BC1F2FECCC9E483037AF` |
| `host-aot-target-red-features.log` | `C8274E4CD09EFFED90A6A47508C38AA89FCFB66862EBCF3CE64DCDAEFAF4CC65` |
| `source-direct-target-sources.tsv` | `8B9F5AF6EEB214521B44228133B190C1D3CA79A8CBE4B6E7C664D0E13428499F` |
| `source-direct-target-settings.tsv` | `8E4F115AC41CD4F81EDCA0E5A32BBDBED6ADFC35FB45704C7C78C4B80CEF837C` |
| `source-direct-header-hashes.tsv` | `BFA5C9BB32029FBA559F2FB102737DFB605040BE2164CB3815B09A717F216192` |
| `zr_vm_ssa_host_noargs_i64_aot_target_test.exe` | `869653AD23EF7688768EF80F3FCBCBF48A7384C5021D453506E3B2CE8E127C85` |

| Current r2 artifact | SHA-256 |
| --- | --- |
| `tests/parser/test_ssa_host_noargs_i64_aot_target.c` | `A5099B5494382E1447CD9E1EB0063F57518A1609211317B34CDDCB47B833D88C` |
| `zr_vm_parser/include/zr_vm_parser/exec_ir_host_aot_target.h` | `743C93F0CD4BF398BE4F18E17C37A9A0D761869846B32A1C49B80A6EF36AD5AA` |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_host_aot_target.c` | `6FA9B29EBFCF7A3F4CD832633D12B10977AD96A97F3A03135F9505545B06139E` |
| `tests/cmake/ssa-source-execbc-vm.cmake` | `E1C0666AC600FA925C06157D0A55C007F09C917C8A370FDF5246A130A70289B0` |
| `tests/cmake/ssa-source-direct-validation/CMakeLists.txt` | `A9337B1C0D154BEDBBD59C34B600CA1BE62E18857C6F016A109004991073CFFB` |
| `tests/parser/support/ssa_literal_script_fixture.h` | `17FD2BBBFA6F625999CDFECA89693BAEB46F0F7F4F31AC7FD8A8C7FD6B5F7F39` |
| `tests/parser/support/ssa_literal_script_fixture.c` | `05FB240863B3EE4D3039836B9457D6A40E08BBF65A42711DF7EB2AA7AC8CCA23` |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_dead_source_places.c` | `5EC5BC993FE65D15A00F45380AB58FF35C20E78862D592E5C783DC6BC5796E6B` |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_host_primitive_layout.c` | `46ADA3B39E9D54D7F780A9077B76BA684F2A4461E6667BE22F15407CBC07ECE2` |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_frame.c` | `B1E2FA4EFE39ECD120034FFB7DFA9F7DF550120A08CA04FA7C010287CF367AD3` |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_aot.c` | `C7A00295CBDD0480BEB311CFB62F3CE121F6B6CF7C1EF91C56848D3CD1E9BEC4` |
| `zr_vm_parser/include/zr_vm_parser/canonical_type.h` | `4FE7CB1EF33EF87859A3D522DD2DE2FCE2D42BBEF461D7ED849C396328670C1A` |
| `zr_vm_core/include/zr_vm_core/aot_ir.h` | `1601A14AA5086DBEA4AFAAA18E201135CDBF55FEDA5D6083C4D09D3FD5BAEE89` |
| `tests/parser/test_ssa_host_primitive_layout.c` | `B939C262062E11B116697ECAEECC85C7CDDBE5F7339A7479FE4BBCAC4DFABDFE` |
| `tests/parser/test_ssa_primitive_source_frame.c` | `5C62A58C1EE9925FB40AC2D3D4222B4BAEE2D31C8FC91F6AAC58E2EA56259D07` |
| `host-aot-target-green-r2-configure.log` | `4F5DC04B7C942304731494743C6B9474FC77272767E51B5A65C16D230C89672F` |
| `host-aot-target-green-r2-build.log` | `8A4E3D166D13843F39E021191FDD05CEA945BB1CE26D911FE40D80D92B6183A9` |
| `host-aot-target-green-r2-ctest.log` | `A9ACE31B9479A954EA0D97E62E91F37741FE03F505CBCD31589222F25ED35F81` |
| `source-direct-target-sources.tsv` | `788CD7153817C04873D9F48710A9DC87C79F4514CB70833DF10E95113983013D` |
| `source-direct-target-settings.tsv` | `8E4F115AC41CD4F81EDCA0E5A32BBDBED6ADFC35FB45704C7C78C4B80CEF837C` |
| `source-direct-header-hashes.tsv` | `1DFCA847371C5628BAAADF69B4A778063B139F3107E0503D87E2FC2B363895E9` |
| `zr_vm_ssa_host_noargs_i64_aot_target_test.exe` | `E0D0E2D2FC6E02E629C90DF48BDF227A1662C29A31B5ABE4EE5383352A252063` |
| `zr_vm_ssa_host_primitive_layout_test.exe` | `EEFC6E184020EE95DBD6F45E9E8687E00E9940FF53AD4FA0B4B84CDEF61739D1` |
| `zr_vm_ssa_primitive_source_frame_test.exe` | `BA70EED5B5662701FC118512FA27C3E2EBB181C00CABE7817B453F505893AE3F` |
| `host-aot-target-green-r2-msvc-smoke.log` | `00C14766178C03B128030E346B410392D8FEFF7A336BE2D594134D2DD2EF8CF3` |
| `exec_ir_host_aot_target.obj` | `A0BF1D3A47450F5C1F52E8ECEDCC6B15188BA04C2EE26ECE71554386DDC332B2` |

## Remaining OPEN gates

The real graphs retain NOP/IDs/source maps, layout tables, owned frame and
empty state-map identity. Target record construction does not bind a module
descriptor or copy function contracts into invented module IDs. The scalar
emitter still refuses real frame/layout/state metadata and expects two
instructions; deleting the true NOP or clipping metadata is not acceptance.
Projection borrowed views require actual owner life and separate retention.

Module descriptor binding, real-frame scalar emission, source/native execution,
normal returned artifact retention, Linux GCC/Clang, full MSVC matrix and full
SSA47 remain OPEN. No network/import loader, FFI, provider, capability,
hotpatch or security execution is claimed. No external delivery or whole-plan
acceptance follows from this finite result.
