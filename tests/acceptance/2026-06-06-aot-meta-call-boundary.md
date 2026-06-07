# AOT Meta Call Boundary

## Scope

- Generated AOT C meta-call bytecodes now emit an explicit unsupported boundary instead of the prepared meta-call helper path.
- Affected layers: AOT C call lowering, call source-contract tests, and the generated call shared-library smoke.
- Real generated meta dispatch, LLVM parity, and source-level meta-call execution fixtures remain separate future slices.

## Baseline

- RED: `zr_vm_aot_c_call_contracts_test` first failed because the call lowering source did not expose `backend_aot_write_c_unsupported_meta_call(FILE *file,` and still used the direct meta-call helper route.
- The repository baseline remains dirty with unrelated work; this slice only claims the focused AOT C call lowering/test/docs changes.

## Test Inventory

- Focused source contract: `tests/parser/test_aot_c_call_contracts.c`.
- Generated-C compile smoke: `tests/parser/test_aot_c_call_shared_library_smoke.c` hand-builds `SUPER_META_CALL_NO_ARGS`, `META_CALL`, and `META_TAIL_CALL`, checks the generated unsupported-boundary source, and compiles the generated Unix shared library without executing the intentional failure path.
- Source scan: `backend_aot_c_lowering_calls.c` and `backend_aot_c_function_body.c` contain the new unsupported meta-call route and no old meta-call prepared-call helper emission.

## Tooling Evidence

- RED command:
  `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_call_contracts_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_call_contracts_test"`
- GCC command:
  `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_call_contracts_test zr_vm_aot_c_call_shared_library_smoke_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_call_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_call_shared_library_smoke_test"`
- Clang command:
  `wsl bash -lc "cd /mnt/e/Git/zr_vm && CC=clang cmake -S . -B build-wsl-clang -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIB=ON && cmake --build build-wsl-clang --target zr_vm_aot_c_call_contracts_test zr_vm_aot_c_call_shared_library_smoke_test -j2 && ./build-wsl-clang/bin/zr_vm_aot_c_call_contracts_test && ./build-wsl-clang/bin/zr_vm_aot_c_call_shared_library_smoke_test"`
- MSVC command:
  `Import-VsDevCmdEnvironment.ps1; cmake --build build-msvc --target zr_vm_aot_c_call_contracts_test --config Debug; .\build-msvc\bin\Debug\zr_vm_aot_c_call_contracts_test.exe; cmake --build build-msvc --target zr_vm_aot_c_call_shared_library_smoke_test --config Debug; .\build-msvc\bin\Debug\zr_vm_aot_c_call_shared_library_smoke_test.exe`

## Results

- RED result: `test_aot_c_source_makes_meta_calls_explicit_boundary` failed on missing `backend_aot_write_c_unsupported_meta_call(FILE *file,`.
- GCC `zr_vm_aot_c_call_contracts_test`: 4 tests, 0 failures.
- GCC `zr_vm_aot_c_call_shared_library_smoke_test`: 3 tests, 0 failures.
- Clang `zr_vm_aot_c_call_contracts_test`: 4 tests, 0 failures.
- Clang `zr_vm_aot_c_call_shared_library_smoke_test`: 3 tests, 0 failures.
- MSVC `zr_vm_aot_c_call_contracts_test`: 4 tests, 0 failures.
- MSVC `zr_vm_aot_c_call_shared_library_smoke_test`: target built; 3 Unix-only runtime tests ignored.
- Production scan of `backend_aot_c_lowering_calls.c` and `backend_aot_c_function_body.c` found no `backend_aot_write_c_direct_meta_call`, `ZrLibrary_AotRuntime_PrepareMetaCall`, `ZrLibrary_AotRuntime_CallPreparedOrGeneric`, or `ZrAotGeneratedDirectCall zr_aot_direct_call` emission.

## Acceptance Decision

- Accepted for the AOT C meta-call helper-removal boundary slice.
- Meta-call opcodes now preserve their runtime-only nature as an explicit generated failure boundary with destination, receiver, and argument-count metadata instead of hiding behind prepared-call runtime helpers.
- Remaining risks: real generated meta dispatch is not implemented; LLVM keeps its current helper routes until parity work; meta value access and dynamic member/index execution remain explicit unsupported or future direct-core contracts.
