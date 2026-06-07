# AOT Unsupported Instruction Boundary

## Scope

- Generated AOT C unsupported opcode, fallthrough, and invalid branch-target paths now emit an explicit unsupported-instruction boundary instead of calling the old instruction-report helper.
- Affected layers: AOT C control/function-body lowering, source-contract tests, and the generated shared-library smoke.
- Real unsupported opcode execution, LLVM parity, and broader unsupported policy remain separate future slices.

## Baseline

- RED: `zr_vm_aot_c_source_contracts_test` first failed because `test_aot_c_source_emits_value_frame_cleanup_exit` required `backend_aot_write_c_unsupported_instruction(file,` while generated C lowering still emitted `ZrLibrary_AotRuntime_ReportUnsupportedInstruction`.
- The repository baseline remains dirty with unrelated work; this slice only claims the focused AOT C lowering/test/docs changes.

## Test Inventory

- Focused source contract: `tests/parser/test_aot_c_source_contracts.c`.
- Generated-C compile smoke: `tests/parser/test_aot_c_shared_library_smoke.c` hand-builds an invalid `JUMP` target, checks the generated `zr_aot_unsupported_instruction` source, rejects `ZrLibrary_AotRuntime_ReportUnsupportedInstruction`, and compiles the generated Unix shared library without executing the intentional failure path.
- Source scan: `backend_aot_c*.c` production files contain no generated-C `ZrLibrary_AotRuntime_ReportUnsupportedInstruction` emission.

## Tooling Evidence

- RED command:
  `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_source_contracts_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test"`
- GCC command:
  `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_shared_library_smoke_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_shared_library_smoke_test"`
- Clang command:
  `wsl bash -lc "cd /mnt/e/Git/zr_vm && CC=clang cmake -S . -B build-wsl-clang -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIB=ON && cmake --build build-wsl-clang --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_shared_library_smoke_test -j2 && ./build-wsl-clang/bin/zr_vm_aot_c_source_contracts_test && ./build-wsl-clang/bin/zr_vm_aot_c_shared_library_smoke_test"`
- MSVC command:
  `Import-VsDevCmdEnvironment.ps1; cmake --build build-msvc --target zr_vm_aot_c_source_contracts_test --config Debug; .\build-msvc\bin\Debug\zr_vm_aot_c_source_contracts_test.exe; cmake --build build-msvc --target zr_vm_aot_c_shared_library_smoke_test --config Debug; .\build-msvc\bin\Debug\zr_vm_aot_c_shared_library_smoke_test.exe`

## Results

- RED result: `test_aot_c_source_emits_value_frame_cleanup_exit` failed on missing `backend_aot_write_c_unsupported_instruction(file,`.
- GCC `zr_vm_aot_c_source_contracts_test`: 17 tests, 0 failures.
- GCC `zr_vm_aot_c_shared_library_smoke_test`: 7 tests, 0 failures.
- Clang `zr_vm_aot_c_source_contracts_test`: 17 tests, 0 failures.
- Clang `zr_vm_aot_c_shared_library_smoke_test`: 7 tests, 0 failures.
- MSVC `zr_vm_aot_c_source_contracts_test`: 17 tests, 0 failures.
- MSVC `zr_vm_aot_c_shared_library_smoke_test`: target built; 7 Unix-only runtime tests ignored.
- Production scan over `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c*.c` found no `ZrLibrary_AotRuntime_ReportUnsupportedInstruction` emission.

## Acceptance Decision

- Accepted for the AOT C unsupported-instruction helper-removal boundary slice.
- Unsupported instruction paths now preserve their runtime-only nature as an explicit generated failure boundary with function, instruction, and opcode metadata instead of hiding behind `ZrLibrary_AotRuntime_ReportUnsupportedInstruction`.
- Remaining risks: unsupported opcode execution is still intentionally unsupported; LLVM keeps its current helper routes until parity work; generated-C scaffolding still has other runtime/control boundaries such as explicit export publication and begin/fail helper contracts.
