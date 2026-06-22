# AOT M1.5 Generic Numeric Boundary Helpers

## Scope
- Changed 07-S5 generic numeric arithmetic C lowering for `ADD`, `SUB`, `MUL`, `DIV`, `MOD`, and `NEG`.
- Affected layers: AOT C backend, AOT runtime helper ABI, parser source contracts, generated shared-library smoke tests, and AOT plan/status docs.

## Baseline
- Before this slice, `backend_aot_c_lowering_generic_numeric_arithmetic.c` expanded generated `SZrTypeValue *` destination/source locals, numeric tag branches, generated zero guards, direct `+ - * / %`, direct unary negation, and generated `fmod(zr_aot_left_float, zr_aot_right_float)`.
- That shape violated the 07-S5 boundary-template direction because dynamic numeric primitive semantics lived inside generated C instead of a runtime boundary helper.

## Test Inventory
- Source contract: `tests/parser/test_aot_c_source_contracts.c`.
- Focused generic numeric contract: `tests/parser/test_aot_c_generic_numeric_contracts.c`.
- Generated product smoke: `tests/parser/test_aot_c_generic_numeric_shared_library_smoke.c`.
- Regression group: aggregate shared-library smoke, call shared-library smoke, global shared-library smoke, logical shared-library smoke, typed scalar, return contracts, and frame setup contracts.
- Boundary cases covered: binary numeric helper dispatch, unary numeric helper dispatch, float/signed/unsigned runtime handling, generic `MOD` float fallback, divide/modulo zero failure strings, unsupported generic numeric failure, and absence of generated direct numeric tag/expression templates.

## Tooling Evidence
- Tool: WSL `Ubuntu-22.04`, GCC Debug build directory `build/codex-aot-07-wsl-gcc-debug`.
- RED command:
  ```bash
  wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_generic_numeric_contracts_test zr_vm_aot_c_generic_numeric_shared_library_smoke_test -j 8 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_generic_numeric_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_generic_numeric_shared_library_smoke_test'
  ```
- RED result: `zr_vm_aot_c_source_contracts_test` failed 1/19 with missing source contract text `zr_aot_arith_exec_generic_numeric_binary_boundary`.
- GREEN command:
  ```bash
  wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_generic_numeric_contracts_test zr_vm_aot_c_generic_numeric_shared_library_smoke_test zr_vm_aot_c_shared_library_smoke_test zr_vm_aot_c_call_shared_library_smoke_test zr_vm_aot_c_global_shared_library_smoke_test zr_vm_aot_c_logical_shared_library_smoke_test zr_vm_aot_c_typed_scalar_test zr_vm_aot_c_return_contracts_test zr_vm_aot_c_frame_setup_contracts_test -j 8 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_generic_numeric_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_generic_numeric_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_global_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_logical_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_return_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_frame_setup_contracts_test'
  ```
- Generated-product check:
  ```powershell
  Select-String -Path build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_generic_numeric_shared_library/src/aot_c_generic_numeric_mod_smoke.c -Pattern 'GenericNumericMod|zr_aot_arith_exec_generic_numeric_binary|fmod\(|ZrCore_Debug_RunError\(state, "modulo by zero"|ZR_VALUE_IS_TYPE_FLOAT\(zr_aot_left->type\)'
  ```
- Generated-product result: only the boundary marker and `ZrLibrary_AotRuntime_GenericNumericMod(state, &frame, 2, 0, 1)` matched.

## Results
- `zr_vm_aot_c_source_contracts_test`: 19 tests, 0 failures.
- `zr_vm_aot_c_generic_numeric_contracts_test`: 1 test, 0 failures.
- `zr_vm_aot_c_generic_numeric_shared_library_smoke_test`: 1 test, 0 failures.
- `zr_vm_aot_c_shared_library_smoke_test`: 8 tests, 0 failures.
- `zr_vm_aot_c_call_shared_library_smoke_test`: 3 tests, 0 failures.
- `zr_vm_aot_c_global_shared_library_smoke_test`: 9 tests, 0 failures.
- `zr_vm_aot_c_logical_shared_library_smoke_test`: 4 tests, 0 failures.
- `zr_vm_aot_c_typed_scalar_test`: 1 test, 0 failures.
- `zr_vm_aot_c_return_contracts_test`: 1 test, 0 failures.
- `zr_vm_aot_c_frame_setup_contracts_test`: 1 test, 0 failures.
- Build emitted existing `project.c` const-qualifier warnings outside this slice.

## Acceptance Decision
- Accepted for this 07-S5 sub-slice.
- Reason: generated C now keeps generic numeric arithmetic at a single helper boundary, while the runtime helper preserves primitive numeric semantics and focused generated-product checks reject the old expanded templates.
- Remaining risks: 07-S5 is still partial; typed-to-typed native signature routing, in/out writeback, deopt/dynamic bridges, and remaining boundary templates are still later 07 work.
