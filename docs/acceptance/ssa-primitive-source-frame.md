---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_source_frame.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_frame.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_layout.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_aot.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projection_common.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
  - tests/parser/test_ssa_primitive_source_frame.c
  - tests/parser/support/ssa_literal_script_fixture.h
  - tests/parser/support/ssa_literal_script_fixture.c
  - tests/cmake/ssa-source-execbc-vm.cmake
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_source_frame.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_frame.c
plan_sources:
  - docs/plans/ssa/04-frame-native/01-frame-layout.md
  - docs/parser-and-semantics/ssa-dead-source-places.md
  - docs/parser-and-semantics/ssa-host-primitive-layout.md
tests:
  - tests/parser/test_ssa_primitive_source_frame.c
  - tests/acceptance/ssa-primitive-source-frame.md
doc_type: acceptance
status: primitive-source-frame-focused-windows-green-accepted
---

# Primitive Source Frame Acceptance

## Finite scope and decision

The [module contract](../parser-and-semantics/ssa-primitive-source-frame.md)
attaches primitive packed storage transactionally using explicit row geometry.
The host row producer's historical validated commit is `19d3acd8`; current
RED ancestry uses `4f0f4c8e28857a09cae1570311d48c52797b1e6c` with an identical
Git tree. Its immutable source/receipt evidence remains unchanged. The shared
literal fixture keeps one real preparation path. Those earlier gates do not establish
the new frame entry's acceptance.

RED is established and this finite frame gate is **focused Windows GREEN
accepted**. The callable stub returned UNSUPPORTED after actual-source
prerequisites; a separate frozen implementation run establishes GREEN.

## Immutable actual RED

RED commit: `99b02fd79cffdf7801945054d357a5f3dc8508d0` (author and committer
He-Jiahui; exact five API/stub/test/CMake paths).
Receipt: `E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/primitive-source-frame-red-receipt.json`.
SHA-256: `9D3C9DF4A9365B3C1708DFA85D65E747557A3877B8C077F094CEBFFB5506AAC2`.

Windows clang-cl19 x64 Debug assertions/UBSan configure/build/prerequisite/
regression commands exited 0. Independent frame prerequisites passed 2/2;
host15/15 and compaction30/30 also passed. The full frame suite genuinely
exited CTest8 with 18 cases, 9 failures and 0 ignored. No UBSan diagnostic was
observed. Native session28076 naturally ended exit8 without termination.

Three attach features and repeat-attached-frame's initial attachment failed
at the FEATURE assertion. Remaining failures received UNSUPPORTED28 instead
of expected frame-limit21, SEALED23, INVALID_RANGE11 (both ordinary and sealed
bad range) and INVALID_ARGUMENT1. The actual nine failures are:

- `test_attach_nine`, `test_attach_eight`, `test_attach_null_diagnostic`
- `test_repeat_attached_frame` (first attach, not the later refusal)
- `test_frame_limit`, `test_sealed`
- `test_bad_return_range`, `test_bad_return_range_precedes_sealed`
- `test_required_arguments`

Source/metadata hashes in the receipt belong to the frozen RED version:

| Historical RED source | SHA-256 |
| --- | --- |
| Frame header | `0E3486C2A3D76A6D7890240AA2CAB000B7F8F2017CD77E845FAC3D92D70C54F4` |
| Unsupported stub | `D226C80F109671B2DDFD125A851FF6D48296900E0A8F71E197D37946BB9AD817` |
| Frame test | `82037D07C78ED3C9A1D2B2E255A82847C8583DB56DBFB3A3B1781E832648B091` |
| Direct CMake | `65D13795BEDD65C06AC0B34E3E6C3546F97B9EF344F94008F18993E119BBB5FA` |
| Ordinary CMake | `1B552E6C1E9DB597B73EA5064A3FFED9E5CE8CB2369D5D0338D2623BB0C56E33` |

| RED log | SHA-256 |
| --- | --- |
| `primitive-source-frame-red-configure.log` | `A667405A41B307AD0BCEEAAC7D82DC567A90452303D742F6E42A6713DA86A54E` |
| `primitive-source-frame-red-build.log` | `1A01F864872A2C33C87266A91223F13044272960DD165E4CD0A46B3310DDF486` |
| `primitive-source-frame-red-prerequisites.log` | `6E16FD3AA826970DE2FA5E3F1D67C10F69BFB0A51FE5C0A8AD84268A69A82D61` |
| `primitive-source-frame-red-regressions.log` | `AED2AAF6211B1D5A6EC9B9D415BAFE868710EBC59E4F7AAB4093FDBFBFED38BE` |
| `primitive-source-frame-red-ctest.log` | `3082A8622DD58EFEE0448772AECAA53FE6C20226823F3B4AD63C005BEC8FD24F` |

The RED binary SHA-256 is
`D5D137BC91BECB104C9AB7425EC6ED45224B258DA900B576578845651BF67B5B`.
Passing prerequisites and behavior-only failures establish RED; no link or
compile failure is counted. These historical pins do not represent GREEN.

## Evidence requirements

The production review found that Core permits optional ordinary
`value.definition == 0`. The implementation was corrected to reject invalid
or beyond-instructionCount definitions before any `definition - 1` read; the
unsafe intermediate version was not executed. The real Core-valid optional
definition guard now passes, restoring metadata before assertion. Historical
RED remains 18 cases/9 failures; actual GREEN is 19 (2 prerequisites, 3 features,
14 guards).
Admission is explicitly CONSTANT/NOP/RETURN with nonzero ordinary definitions.
Verifier-diagnostic preservation is after storage preflight, under the caller's
independent valid allocations; malformed storage has its own preflight refusal.

The executed RED inventory has 18 cases: 2 prerequisites, 3 attachment
features and 13 guards. Prerequisites prove actual source Oracle9/8, compaction,
zero-place Oracle and actual host row/table append without calling frame entry.
Feature cases attach 9/8 or use null diagnostic, then check owned geometry,
graph/source/table preservation, VERIFY_ALL/Oracle and the canonical AOT
projection with actual SemIR callable TypeId. That projection remains nonrunnable.

Guard cases include the original address graph, a Core-valid external value
contract mutation (explicitly not original source), missing/duplicate/invalid
rows, frame byte limit, valid sealed input, repeat attachment, malformed return
ranges including sealed priority, valid richer state/deopt metadata and required
arguments. Refusal preserves complete graph/frame observations. Mutation
fixtures do not manufacture BOOL/callable provenance or prove target ABI.

The final receipt pins exact source/metadata/log/binary bytes and actual terminal
counts. Same checkout inputs are used directly on E:,
with shared support compiled into the target; no source snapshot or cross-drive
copy is created. The new MSVC smoke remains new-TU compile-only.
Linux GCC/Clang retain the prior WSL service access denial. No full runtime
matrix, descriptor/native/retention acceptance or SSA47 completion is implied.

The [test acceptance guide](../../tests/acceptance/ssa-primitive-source-frame.md)
owns the detailed command route, inventory and final decision.

## Frozen actual Windows GREEN

Receipt: `E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/primitive-source-frame-green-receipt.json`.
SHA-256: `F53F39E2D3DC0E4EC44D2CED7EC2425841C06D464C7C0AD22C4FA0C380405063`.
Configure/build/independent prerequisites/CTest exited 0. Prerequisites passed
2/2; three CTest suites passed frame 19/19, host 15/15 and compaction 30/30,
with failures 0/ignored 0 and no observed UBSan diagnostic. No new eight-suite
consumer run is claimed. Native 7817 naturally ended exit 0 without termination.
Independent frozen-input storage/specification reviews found no blocker; static
security-surface review found only the pure memory call chain. No sensitive
call path or native/descriptor execution was probed.

Actual attached 9/8 graphs retain body/source/table observations and empty
state-map pointer/header. Canonical AOT uses actual constants/layouts, live
context and SemIR.callableTypeId, produces NOARGS_I64 and remains runnable=false.
The frame hash is the existing packed geometry hash, not a row fingerprint,
source certificate or target ABI proof. The new definition-zero guard passes.

| Current GREEN source | SHA-256 |
| --- | --- |
| `zr_vm_parser/include/zr_vm_parser/exec_ir_source_frame.h` | `A3C9DA4AE05542DC49CC43CD42C0C3120810047248763DC1C3138E9517F2D882` |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_frame.c` | `B1E2FA4EFE39ECD120034FFB7DFA9F7DF550120A08CA04FA7C010287CF367AD3` |
| `tests/parser/test_ssa_primitive_source_frame.c` | `5C62A58C1EE9925FB40AC2D3D4222B4BAEE2D31C8FC91F6AAC58E2EA56259D07` |
| `tests/parser/support/ssa_literal_script_fixture.h` | `17FD2BBBFA6F625999CDFECA89693BAEB46F0F7F4F31AC7FD8A8C7FD6B5F7F39` |
| `tests/parser/support/ssa_literal_script_fixture.c` | `05FB240863B3EE4D3039836B9457D6A40E08BBF65A42711DF7EB2AA7AC8CCA23` |
| `zr_vm_parser/include/zr_vm_parser/exec_ir_host_primitive_layout.h` | `ED1DF6C2372CECC910B9E3887C37AC56CFB46DE2956EAD0D1769E1F77150F836` |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_host_primitive_layout.c` | `46ADA3B39E9D54D7F780A9077B76BA684F2A4461E6667BE22F15407CBC07ECE2` |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_dead_source_places.c` | `5EC5BC993FE65D15A00F45380AB58FF35C20E78862D592E5C783DC6BC5796E6B` |
| `tests/cmake/ssa-source-direct-validation/CMakeLists.txt` | `65D13795BEDD65C06AC0B34E3E6C3546F97B9EF344F94008F18993E119BBB5FA` |
| `tests/cmake/ssa-source-execbc-vm.cmake` | `1B552E6C1E9DB597B73EA5064A3FFED9E5CE8CB2369D5D0338D2623BB0C56E33` |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_layout.c` | `365A6CE75E8B38C0E975EB24315E2303434FA6D83FF0AA0226A2D0A05B81F0D3` |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projection_common.c` | `4C764358B2C7F6587A2C67D945D13DEA40C72034B3870E85722A82127BC4E578` |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_aot.c` | `C7A00295CBDD0480BEB311CFB62F3CE121F6B6CF7C1EF91C56848D3CD1E9BEC4` |
| `zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c` | `9E4C0B7389B180A9B61591CB9ED4D22D89CCA92C76D613FE6CB0E61D1FEF2F54` |

| GREEN evidence | SHA-256 |
| --- | --- |
| `primitive-source-frame-green-configure.log` | `F01F3827B6DDC7036EEF7DCE1ACF17586EC4964A07331FC9091B5F6B3C7E5761` |
| `primitive-source-frame-green-build.log` | `42134B431B3C9159CE1742331B899788EB6E473E7F8B26ACAF9776BE399F865B` |
| `primitive-source-frame-green-prerequisites.log` | `870AE9FFF3D7CCBEB1025AD4519252A096CFDB88F8E7738B5688CF830F4D6978` |
| `primitive-source-frame-green-ctest.log` | `6D2DE7386E3CE5B30B5F59D4B1F9B819B2F3444B085B06973E09D3FC7B07E9BF` |
| `primitive-source-frame-green-msvc.log` | `62B61AE6697BFF2D4252346724C0E54536AD0CA4EF31AE2ACB01935ED1FAAC57` |
| `source-direct-target-sources.tsv` | `3DD2E4050F2CB2243A53043B81B1CD67D8BE8A6ECA69AF828AF3EB750A135351` |
| `source-direct-target-settings.tsv` | `6092E2F3F34EADA8B83E85FF4333861EE50B7C382D1DA35F1AC2C923C0E9BD57` |
| `source-direct-header-hashes.tsv` | `A5874FA4DC76D8A075293A6AE59E32F7C29A04FDC43ADCB4E3A06FAE675DA78B` |
| Frame binary | `A7E45DAA0476D4B774723A5965495229BA5DAF76821961D1F125696FF73B94EF` |

MSVC 14.44.35207 compiled only the new frame TU with absolute cl.exe, exit 0
and no warnings observed in its one-filename log. The full args/INCLUDE/TEMP
configuration is preserved in this receipt. Object SHA-256 is
`6C8EC6E7EF958D3FA8A5D0F08192673EBA66A83194E316A8BED7C3B2BD41D660`;
args receipt SHA-256 is
`9C6A6E6161B539D0DBB67C882BFFBB9FD2651647F75773C3222FB347BEA56883`.
This compile-only result does not establish a warning-free repository or MSVC
runtime. Linux/full47/descriptor/native/retention remain OPEN.
