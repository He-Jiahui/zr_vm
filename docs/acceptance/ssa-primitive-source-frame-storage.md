---
related_code:
  - tests/parser/test_ssa_primitive_source_frame_storage.c
  - tests/parser/support/ssa_literal_script_fixture.h
  - tests/parser/support/ssa_literal_script_fixture.c
  - zr_vm_parser/include/zr_vm_parser/exec_ir_source_frame.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_frame.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_layout.c
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
  - tests/cmake/ssa-source-execbc-vm.cmake
implementation_files:
  - tests/parser/test_ssa_primitive_source_frame_storage.c
plan_sources:
  - .codex/plans/20261005-ssa-primitive-frame-storage-guards.md
  - docs/plans/ssa/04-frame-native/01-frame-layout.md
tests:
  - tests/parser/test_ssa_primitive_source_frame_storage.c
  - tests/parser/test_ssa_primitive_source_frame.c
doc_type: acceptance-evidence
status: primitive-source-frame-storage-focused-windows-green-accepted
---

# Primitive source frame storage evidence

## Decision and scope

Current decision: **focused Windows GREEN accepted**. Root's actual Windows
x64 clang-cl 19 Debug/assertions/UBSan run completed configuration, build,
independent actual 9/8 prerequisites and focused CTest with exit 0. Prerequisites
passed 2/2; the storage and frame suites passed 4/4 and 19/19. There were zero
failures/ignored cases and no observed UBSan diagnostic. Native job 69101 ended
naturally exit 0 without termination. The frozen-input owner review closed the
cleanup finding; the independent specification review found no new blocker.

The four cases cover value count/capacity inconsistency, canonical array
length/capacity inconsistency, `size_t` canonical capacity multiplication
overflow and final packed frame alignment overflow. They retain actual owned
allocations and restore temporary metadata before assertions, source lookup,
digest or teardown. No giant allocation or arbitrary pointer is used.

## Accepted dependency, not a new run

The first frame adapter GREEN was committed as
`aeef5d80e91c37947477e760c7a19ca8356a2111`.
Its immutable receipt is
`E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/primitive-source-frame-green-receipt.json`,
SHA-256 `F53F39E2D3DC0E4EC44D2CED7EC2425841C06D464C7C0AD22C4FA0C380405063`.
The approved adapter TU pin is
`B1E2FA4EFE39ECD120034FFB7DFA9F7DF550120A08CA04FA7C010287CF367AD3`.
That receipt accepted independent prerequisites 2/2, frame 19/19, host 15/15
and compaction 30/30 on the focused Windows route. It also records only the
first frame production TU's MSVC compile-only exit 0. These are historical
dependency facts, not evidence for the new four-case executable.

## Verified preservation and cleanup

The actual refusal codes are `INVALID_RANGE` for both count/capacity cases and
`CAPACITY_OVERFLOW` for both arithmetic cases. Faulted metadata is captured
before restoring the complete saved header or row; valid-state digests then
confirm preservation. The value-capacity case separately owns an unexpected
published frame before header restoration so a failing assertion cannot leak
that frame. Teardown frees its slots and header only, never a shallow captured
function. This cleanup path is a static ownership correction, not allocator
fault-injection acceptance.

Production, the existing 19-case test and shared fixture remained unchanged.
No additional host 15/compaction 30 run or fresh MSVC compile is claimed. The
first frame TU compile-only result above remains a historical dependency.

This supplement is an arithmetic and storage refusal gate. The deliberately
modified row does not establish target ABI geometry, source/context provenance,
AOT execution or a runnable native program. Linux GCC/Clang retain the existing
WSL service access denial and remain OPEN; full SSA47 and descriptor/native/
retention acceptance remain OPEN. No sensitive boundary is probed.

The [module guide](../testing-and-validation/ssa-primitive-source-frame-storage.md)
describes ownership and restoration. The [test acceptance guide](../../tests/acceptance/ssa-primitive-source-frame-storage.md)
owns the finite inventory and command route.

## Immutable current receipt

Receipt: `E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/primitive-source-frame-storage-receipt.json`.
SHA-256: `24311804C1681F02DCEC07B0C396A32B24317C1E7D541F0AB6F679704B72DAC1`.
The 12 source pins, four logs, three metadata TSVs and storage binary below
were checked against this receipt and current files. No supplement commit ID
is claimed before Root completes its normal-index transaction.

| Current source | SHA-256 |
| --- | --- |
| `tests/parser/test_ssa_primitive_source_frame_storage.c` | `4A0F1E6ED8A47D77363441B9775C7483C9F90B5893B1F4652A6418471D942471` |
| `tests/parser/test_ssa_primitive_source_frame.c` | `5C62A58C1EE9925FB40AC2D3D4222B4BAEE2D31C8FC91F6AAC58E2EA56259D07` |
| `tests/parser/support/ssa_literal_script_fixture.h` | `17FD2BBBFA6F625999CDFECA89693BAEB46F0F7F4F31AC7FD8A8C7FD6B5F7F39` |
| `tests/parser/support/ssa_literal_script_fixture.c` | `05FB240863B3EE4D3039836B9457D6A40E08BBF65A42711DF7EB2AA7AC8CCA23` |
| `zr_vm_parser/include/zr_vm_parser/exec_ir_source_frame.h` | `A3C9DA4AE05542DC49CC43CD42C0C3120810047248763DC1C3138E9517F2D882` |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_frame.c` | `B1E2FA4EFE39ECD120034FFB7DFA9F7DF550120A08CA04FA7C010287CF367AD3` |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_host_primitive_layout.c` | `46ADA3B39E9D54D7F780A9077B76BA684F2A4461E6667BE22F15407CBC07ECE2` |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_dead_source_places.c` | `5EC5BC993FE65D15A00F45380AB58FF35C20E78862D592E5C783DC6BC5796E6B` |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_layout.c` | `365A6CE75E8B38C0E975EB24315E2303434FA6D83FF0AA0226A2D0A05B81F0D3` |
| `zr_vm_core/include/zr_vm_core/array.h` | `C4DDA2BA07A964326346328D5522C0C5AA68D7BC1EBE5864F3D80D88FFFC7BAF` |
| `tests/cmake/ssa-source-direct-validation/CMakeLists.txt` | `A2BEF843E0AF048020A4769F1F9AC8D8A5DED7913A958E892831F69611FB0ED8` |
| `tests/cmake/ssa-source-execbc-vm.cmake` | `FAD8B984072C96B8D3F88A1F2A090E1CF318B54CFCFA6FAC67EC634ECDA6F78A` |

| Current evidence | SHA-256 |
| --- | --- |
| `primitive-source-frame-storage-configure.log` | `79CEFF084202E50A1F0CC2600EF7E99F1CB8A07AA3ED664CD72F74BE209CC3C1` |
| `primitive-source-frame-storage-build.log` | `A32D890DDE0CC71D0793A8BF1D019D119DAA25A41D9B337DC79AD3429EF31C53` |
| `primitive-source-frame-storage-prerequisites.log` | `870AE9FFF3D7CCBEB1025AD4519252A096CFDB88F8E7738B5688CF830F4D6978` |
| `primitive-source-frame-storage-ctest.log` | `DC3B17BF4411E475EB10D36029B2CB0859361AC44E512C17DD03F5ED16B90C0F` |
| `source-direct-target-sources.tsv` | `D38D8473A43C06BD6EF5F1A029C61F25E8121660DB1A79AC9D50B22CFC7B0C84` |
| `source-direct-target-settings.tsv` | `B0CF3E1FA19AA96025BB64A8E7D314D70EA2487E32A48578722F790C1C1B773E` |
| `source-direct-header-hashes.tsv` | `A5874FA4DC76D8A075293A6AE59E32F7C29A04FDC43ADCB4E3A06FAE675DA78B` |
| `zr_vm_ssa_primitive_source_frame_storage_test.exe` | `1C718B78E5CFC35A74F17BDE6693ABC71587AB517A341B5DFE8BBCACCF592D44` |
