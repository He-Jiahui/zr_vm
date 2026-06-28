# AOT 07-S2/S4 Signed Branch Constant Frame Elision

Date: 2026-06-28 10:06:29 +08:00

Status: complete for this focused slice. This does not close all of 07-S2/S4 or 07/M1.5.

## Scope

- Remove value-slot materialization for a signed integer constant used only by a typed signed branch comparison.
- Focused case: the branch shared-library smoke loop `while (cursor > 0)` where the branch right operand is a generated `CONST i64=0` temp.
- Affected layers: AOT scalar-local analysis, signed branch C lowering, generated-C shared-library smoke tests, and source contracts.

## Baseline

- Before the change, `branch_project` generated:
  - `zr_aot_value_exec_primitive_constant` for slot 3.
  - `zr_aot_destination = &frame.slotBase[3].value;`.
  - `const SZrTypeValue *zr_aot_right = &frame.slotBase[3].value;` inside `zr_aot_jump_if_signed_compare`.
- The left operand was already scalar-local, but the right constant temp was not recognized as an i64 local consumer path.
- Repository-level broad test failures from the project baseline are unchanged and were not reclassified by this slice.

## Test Inventory

- Focused integration case:
  - `tests/parser/test_aot_c_shared_library_smoke.c`
  - `test_aot_c_generated_shared_library_executes_signed_branch_comparisons`
- Static source contracts:
  - `tests/parser/test_aot_c_source_contracts.c`
  - `test_aot_c_source_lowers_typed_bool_equality_to_c_expressions`
  - `test_aot_c_source_lowers_typed_signed_branch_to_c_comparisons`
- Frame setup guard:
  - `tests/parser/test_aot_c_frame_setup_contracts.c`
- Boundary covered:
  - back-edge loop branch with right operand `0`;
  - signed branch direct compare after scalar-local constant emission;
  - generated-C prohibition of the old slot 3 value-slot path.
- Out of scope:
  - generic runtime boundaries, dynamic/string comparisons, GC roots, exports, frame cleanup, and full typed function-body zero-frame proof.

## Tooling Evidence

- WSL GCC was used as the primary RED/GREEN and execution environment.
- WSL Clang was used as the secondary Linux compiler compatibility check.
- Windows MSVC Debug was used for compatibility build and source/frame contract execution.

Commands:

```bash
wsl bash -lc 'cmake --build build-wsl-gcc --target zr_vm_aot_c_shared_library_smoke_test -j 4 && ./build-wsl-gcc/bin/zr_vm_aot_c_shared_library_smoke_test'
wsl bash -lc 'cmake --build build-wsl-gcc --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_frame_setup_contracts_test -j 4 && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_frame_setup_contracts_test'
wsl bash -lc 'cmake --build build-wsl-clang --target zr_vm_aot_c_shared_library_smoke_test zr_vm_aot_c_source_contracts_test zr_vm_aot_c_frame_setup_contracts_test -j 4 && ./build-wsl-clang/bin/zr_vm_aot_c_shared_library_smoke_test && ./build-wsl-clang/bin/zr_vm_aot_c_source_contracts_test && ./build-wsl-clang/bin/zr_vm_aot_c_frame_setup_contracts_test'
```

```powershell
Import-Module 'E:\Visual Studio\Common7\Tools\Microsoft.VisualStudio.DevShell.dll'
Enter-VsDevShell -VsInstallPath 'E:\Visual Studio' -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
cmake --build build-msvc --target zr_vm_aot_c_shared_library_smoke_test zr_vm_aot_c_source_contracts_test zr_vm_aot_c_frame_setup_contracts_test --config Debug -j 4
.\build-msvc\bin\Debug\zr_vm_aot_c_shared_library_smoke_test.exe
.\build-msvc\bin\Debug\zr_vm_aot_c_source_contracts_test.exe
.\build-msvc\bin\Debug\zr_vm_aot_c_frame_setup_contracts_test.exe
```

## Results

- RED:
  - Added generated-C assertions requiring `zr_aot_s3 = (TZrInt64)0;` and `if (zr_aot_s1 <= zr_aot_s3)`.
  - Added forbidden assertions for `zr_aot_value_exec_primitive_constant` and `frame.slotBase[3].value`.
  - WSL GCC shared-library smoke failed at the new branch assertion before production changes.
- GREEN:
  - `backend_aot_c_scalar_locals_slot_has_later_scalar_consumer()` now treats signed branch opcodes as later scalar consumers.
  - `backend_aot_c_scalar_locals_signed_consumer_reads_slot()` recognizes signed branch operands as i64 reads.
  - `backend_aot_c_scalar_locals_signed_consumer_has_i64_operand_locals()` now validates branch operands through the same i64 local proof.
  - The branch smoke now emits `zr_aot_s3 = (TZrInt64)0;` and `if (zr_aot_s1 <= zr_aot_s3)` without slot 3 value materialization.
- Verified:
  - WSL GCC shared-library smoke: 13 tests, 0 failures.
  - WSL GCC source contracts: 21 tests, 0 failures.
  - WSL GCC frame setup contracts: 1 test, 0 failures.
  - WSL Clang shared-library smoke: 13 tests, 0 failures.
  - WSL Clang source contracts: 21 tests, 0 failures.
  - WSL Clang frame setup contracts: 1 test, 0 failures.
  - Windows MSVC Debug shared-library smoke target builds; runtime reports 13 tests, 0 failures, 13 ignored Unix-only tests.
  - Windows MSVC Debug source contracts: 21 tests, 0 failures.
  - Windows MSVC Debug frame setup contracts: 1 test, 0 failures.

## Acceptance Decision

Accepted for this focused 07-S2/S4 slice.

The generated signed branch constant operand no longer requires `SZrValue`/`frame.slotBase` materialization in the focused scalar loop. Remaining 07 work includes broader branch forms, runtime/generic/dynamic boundaries, GC roots, exports, frame cleanup, byte-frame narrowing, performance/SZrValue counters, and full 07/M1.5 completion gates.
