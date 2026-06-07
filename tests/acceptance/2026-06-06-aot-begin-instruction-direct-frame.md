# AOT Begin Instruction Direct Frame

## Scope

- Generated AOT C instruction-entry bookkeeping now emits a direct `zr_aot_begin_instruction` block instead of calling `ZrLibrary_AotRuntime_BeginInstruction`.
- The generated block refreshes active frame state from `SZrCallInfo`, assigns `frame.currentInstructionIndex`, and inlines the observation/line-debug branch.
- Affected layers: AOT C control lowering, source-contract tests, and the live generated-C pipeline assertion helper.
- LLVM parity and stale generated fixture refresh remain separate future work.

## Baseline

- RED: `zr_vm_aot_c_source_contracts_test` first failed because `test_aot_c_source_emits_value_frame_cleanup_exit` required `zr_aot_begin_instruction` while the C backend still emitted `ZrLibrary_AotRuntime_BeginInstruction`.
- The repository baseline remains dirty with unrelated work; this slice only claims the focused AOT C lowering/test/docs changes.

## Test Inventory

- Focused source contract: `tests/parser/test_aot_c_source_contracts.c`.
- Generated-C compile/runtime smoke: `tests/parser/test_aot_c_shared_library_smoke.c`.
- Live generated-C pipeline assertion helper: `zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c` now checks the direct C block shape while leaving LLVM helper expectations intact.
- Source scan: `backend_aot_c*.c` production files contain no generated-C `ZrLibrary_AotRuntime_BeginInstruction` emission.

## Tooling Evidence

- RED command:
  `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_source_contracts_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test"`
- GCC source-contract command:
  `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_source_contracts_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test"`
- GCC generated-C smoke command:
  `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_shared_library_smoke_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_shared_library_smoke_test"`
- Clang command:
  `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_shared_library_smoke_test -j2 && ./build-wsl-clang/bin/zr_vm_aot_c_source_contracts_test && ./build-wsl-clang/bin/zr_vm_aot_c_shared_library_smoke_test"`
- MSVC command:
  `cmake --build build-msvc --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_shared_library_smoke_test --config Debug -j 2; .\build-msvc\bin\Debug\zr_vm_aot_c_source_contracts_test.exe; .\build-msvc\bin\Debug\zr_vm_aot_c_shared_library_smoke_test.exe`

## Results

- RED result: `test_aot_c_source_emits_value_frame_cleanup_exit` failed on missing `zr_aot_begin_instruction`.
- GCC `zr_vm_aot_c_source_contracts_test`: 17 tests, 0 failures.
- GCC `zr_vm_aot_c_shared_library_smoke_test`: 7 tests, 0 failures.
- Clang `zr_vm_aot_c_source_contracts_test`: 17 tests, 0 failures.
- Clang `zr_vm_aot_c_shared_library_smoke_test`: 7 tests, 0 failures.
- MSVC `zr_vm_aot_c_source_contracts_test`: 17 tests, 0 failures.
- MSVC `zr_vm_aot_c_shared_library_smoke_test`: target built; 7 Unix-only runtime tests ignored.
- Production scan over `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c*.c` found only the new `zr_aot_begin_instruction` marker and no `ZrLibrary_AotRuntime_BeginInstruction` emission.

## Acceptance Decision

- Accepted for the AOT C begin-instruction helper-removal slice.
- Generated C now owns the common instruction-entry fast path and preserves the previous observation semantics inline.
- Remaining risks: LLVM still calls `ZrLibrary_AotRuntime_BeginInstruction`; checked-in generated fixtures still contain older helper-backed output until fixture refresh; other generated scaffolding boundaries such as generated-function begin, explicit export publication, and fail handling remain runtime-backed.
