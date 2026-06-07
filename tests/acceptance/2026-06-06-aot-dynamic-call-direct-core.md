# AOT Dynamic Call Direct Core

## Scope

- Generated AOT C dynamic calls now use direct generated C/core-call operations instead of the instruction-shaped `ZrLibrary_AotRuntime_Call(state, &frame, ...)` helper.
- Affected layers: AotExecIR/SemIR dispatch, AOT C call lowering, generated shared-library execution smoke, and source-contract tests.

## Baseline

- RED 1: `zr_vm_aot_c_call_contracts_test` first failed because `backend_aot_c_lowering_calls.c` did not contain the `zr_aot_direct_dynamic_function_call` direct-core contract.
- RED 2: after the direct call body was added, `zr_vm_aot_c_call_shared_library_smoke_test` exposed a quickening boundary: generated C had a `DYN_TAIL_CALL` SemIR row but generic call bytecode, so it still emitted `PrepareDirectCall` / `CallPreparedOrGeneric`.
- RED 3: the call contract was tightened and failed on missing `backend_aot_find_exec_ir_instruction(` before SemIR-aware dynamic-call dispatch was added.
- Repository baseline remains dirty with unrelated LSP/debug/REPL work; this slice only claims the focused AOT C call targets.

## Test Inventory

- Focused source contract: `tests/parser/test_aot_c_call_contracts.c`.
- Generated shared-library integration smoke: `tests/parser/test_aot_c_call_shared_library_smoke.c`.
- Boundary case: quickened generic function-tail-call bytecode with `ZR_SEMIR_OPCODE_DYN_TAIL_CALL` SemIR routes to direct dynamic lowering.
- Negative/source scan: touched C lowering sources and generated GCC/Clang smoke C contain no `ZrLibrary_AotRuntime_Call(state, &frame...)` dynamic call helper.
- Remaining boundary: generated meta-call execution and LLVM call-lowering parity are not handled in this slice.

## Tooling Evidence

- WSL GCC: `gcc (Ubuntu 11.4.0-1ubuntu1~22.04.3) 11.4.0`.
- WSL Clang: `Ubuntu clang version 14.0.0-1ubuntu1.1`.
- MSVC: CMake identified `MSVC 19.44.35227.0`.
- GCC command:
  `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_call_contracts_test zr_vm_aot_c_call_shared_library_smoke_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_call_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_call_shared_library_smoke_test"`
- Clang command:
  `wsl bash -lc "cd /mnt/e/Git/zr_vm && CC=clang cmake -S . -B build-wsl-clang -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIB=ON && cmake --build build-wsl-clang --target zr_vm_aot_c_call_contracts_test zr_vm_aot_c_call_shared_library_smoke_test -j2 && ./build-wsl-clang/bin/zr_vm_aot_c_call_contracts_test && ./build-wsl-clang/bin/zr_vm_aot_c_call_shared_library_smoke_test"`
- MSVC command:
  `Import-VsDevCmdEnvironment.ps1; cmake -S . -B build-msvc -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIB=ON; cmake --build build-msvc --target zr_vm_aot_c_call_contracts_test zr_vm_aot_c_call_shared_library_smoke_test --config Debug; .\build-msvc\bin\Debug\zr_vm_aot_c_call_contracts_test.exe; .\build-msvc\bin\Debug\zr_vm_aot_c_call_shared_library_smoke_test.exe`
- Source/generated scans checked `backend_aot_c_lowering_calls.c`, `backend_aot_c_function_body.c`, and generated GCC/Clang call-smoke `main.c`.

## Results

- GCC `zr_vm_aot_c_call_contracts_test`: 1 test, 0 failures.
- GCC `zr_vm_aot_c_call_shared_library_smoke_test`: 1 test, 0 failures.
- Clang `zr_vm_aot_c_call_contracts_test`: 1 test, 0 failures.
- Clang `zr_vm_aot_c_call_shared_library_smoke_test`: 1 test, 0 failures.
- MSVC `zr_vm_aot_c_call_contracts_test`: 1 test, 0 failures.
- MSVC `zr_vm_aot_c_call_shared_library_smoke_test`: target built; 1 Unix-only runtime test ignored.
- GCC/Clang generated C contains `zr_aot_direct_dynamic_function_call` and `ZrCore_Function_CallAndRestoreAnchor`.
- Scoped scans found no `ZrLibrary_AotRuntime_Call(state, &frame` in the touched C lowering sources or generated call-smoke C.

## Acceptance Decision

- Accepted for the AOT C dynamic-call direct-core slice.
- The C backend now lowers dynamic call semantics through generated stack-anchor/core-call code and consults AotExecIR/SemIR so optimized generic call bytecode does not hide dynamic call meaning.
- Remaining risks: generated meta-call dispatch, LLVM parity, and larger call-family dispatcher modularization remain future work. `backend_aot_c_function_body.c` is already above the modularization threshold; this slice kept a narrow dispatcher routing change and records call-family dispatch extraction as the next practical split boundary.
