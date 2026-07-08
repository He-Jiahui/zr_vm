# AOT 07-S2/S4 Generic Numeric Mixed i64/u64 DIV/MOD Local Fold

## Scope
- Slice: M1.5 / 07-S2/S4 generic numeric mixed i64/u64 `DIV` / `MOD` scalar-local lowering.
- Affected layers: AOT C generic numeric lowering, scalar-local proof, parser smoke/contract tests, AOT plan/status docs.
- Behavior: for `GET_CONSTANT int -> GET_CONSTANT uint -> DIV/MOD -> RETURN`, generated C keeps operands in
  `zr_aot_s0` / `zr_aot_u1`, checks the casted right operand for zero, writes signed i64 destination `zr_aot_s2`,
  and avoids generic numeric runtime boundary plus `SyncSignedIntLocal`.

## Baseline
- Before this slice, mixed i64/u64 scalar-local support covered only `ADD/SUB/MUL`.
- RED command:
  `wsl -d Ubuntu-22.04 -- bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_generic_numeric_shared_library_smoke_test -j 1 && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_numeric_shared_library_smoke_test"`
- RED result: generic numeric smoke reported 31 tests, 2 failures. The new `DIV` and `MOD` cases failed with
  `Expected Non-NULL` because generated C lacked mixed i64/u64 local markers, zero guards, and signed-result direct
  expressions.

## Test Inventory
- Focused executable smoke:
  `tests/parser/test_aot_c_generic_numeric_shared_library_smoke.c`
  - `test_aot_c_generated_shared_library_compiles_generic_numeric_div_signed_unsigned_int_local`
  - `test_aot_c_generated_shared_library_compiles_generic_numeric_mod_signed_unsigned_int_local`
- Contract tests:
  - `tests/parser/test_aot_c_generic_numeric_contracts.c`
  - `tests/parser/test_aot_c_source_contracts.c`
- Adjacent regression matrix:
  - generic numeric smoke/contracts/source contracts
  - typed/generic power smoke/contracts
  - generic LOGICAL_NOT numeric local smoke
  - generic bool equality local smoke
  - generic equality stack-copy local smoke
  - generic not-equal stack-copy jump-if local smoke
  - generic call-result stack-copy equality local smoke
- Boundary coverage:
  - mixed signed/unsigned integer operands retain runtime signed-result semantics.
  - `DIV` and `MOD` preserve `"divide by zero"` / `"modulo by zero"` error strings.
  - zero checks use the same signed i64 extraction view as the runtime path: `(TZrInt64)zr_aot_u1 == (TZrInt64)0`.
  - dynamic/unproven operands remain on the runtime fallback path.

## Tooling Evidence
- WSL GCC focused GREEN:
  `wsl -d Ubuntu-22.04 -- bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_generic_numeric_shared_library_smoke_test zr_vm_aot_c_generic_numeric_contracts_test zr_vm_aot_c_source_contracts_test -j 1 && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_numeric_shared_library_smoke_test && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_numeric_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test"`
  - generic numeric smoke 31/0
  - generic numeric contracts 1/0
  - source contracts 24/0
- WSL GCC adjacent matrix:
  `wsl -d Ubuntu-22.04 -- bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_generic_numeric_shared_library_smoke_test zr_vm_aot_c_generic_numeric_contracts_test zr_vm_aot_c_source_contracts_test zr_vm_aot_c_power_shared_library_smoke_test zr_vm_aot_c_power_contracts_test zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test zr_vm_aot_c_generic_bool_equality_local_smoke_test zr_vm_aot_c_generic_equality_stack_copy_local_smoke_test zr_vm_aot_c_generic_not_equal_stack_copy_jump_if_local_smoke_test zr_vm_aot_c_generic_call_result_stack_copy_equality_local_smoke_test -j 1 && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_numeric_shared_library_smoke_test && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_numeric_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_power_shared_library_smoke_test && ./build-wsl-gcc/bin/zr_vm_aot_c_power_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_bool_equality_local_smoke_test && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_equality_stack_copy_local_smoke_test && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_not_equal_stack_copy_jump_if_local_smoke_test && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_call_result_stack_copy_equality_local_smoke_test"`
  - 31/0, 1/0, 24/0, 1/0, 2/0, 8/0, 5/0, 4/0, 1/0, 1/0
- WSL Clang adjacent matrix:
  `wsl -d Ubuntu-22.04 -- bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_aot_c_generic_numeric_shared_library_smoke_test zr_vm_aot_c_generic_numeric_contracts_test zr_vm_aot_c_source_contracts_test zr_vm_aot_c_power_shared_library_smoke_test zr_vm_aot_c_power_contracts_test zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test zr_vm_aot_c_generic_bool_equality_local_smoke_test zr_vm_aot_c_generic_equality_stack_copy_local_smoke_test zr_vm_aot_c_generic_not_equal_stack_copy_jump_if_local_smoke_test zr_vm_aot_c_generic_call_result_stack_copy_equality_local_smoke_test -j 1 && ./build-wsl-clang/bin/zr_vm_aot_c_generic_numeric_shared_library_smoke_test && ./build-wsl-clang/bin/zr_vm_aot_c_generic_numeric_contracts_test && ./build-wsl-clang/bin/zr_vm_aot_c_source_contracts_test && ./build-wsl-clang/bin/zr_vm_aot_c_power_shared_library_smoke_test && ./build-wsl-clang/bin/zr_vm_aot_c_power_contracts_test && ./build-wsl-clang/bin/zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test && ./build-wsl-clang/bin/zr_vm_aot_c_generic_bool_equality_local_smoke_test && ./build-wsl-clang/bin/zr_vm_aot_c_generic_equality_stack_copy_local_smoke_test && ./build-wsl-clang/bin/zr_vm_aot_c_generic_not_equal_stack_copy_jump_if_local_smoke_test && ./build-wsl-clang/bin/zr_vm_aot_c_generic_call_result_stack_copy_equality_local_smoke_test"`
  - 31/0, 1/0, 24/0, 1/0, 2/0, 8/0, 5/0, 4/0, 1/0, 1/0
- MSVC Debug compatibility:
  `cmd.exe /c "call ""E:\Visual Studio\Common7\Tools\VsDevCmd.bat"" -arch=x64 && cmake --build build-msvc-aot-stack-copy --config Debug --target zr_vm_aot_c_generic_numeric_shared_library_smoke_test zr_vm_aot_c_generic_numeric_contracts_test zr_vm_aot_c_source_contracts_test zr_vm_aot_c_power_shared_library_smoke_test zr_vm_aot_c_power_contracts_test zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test zr_vm_aot_c_generic_bool_equality_local_smoke_test zr_vm_aot_c_generic_equality_stack_copy_local_smoke_test zr_vm_aot_c_generic_not_equal_stack_copy_jump_if_local_smoke_test zr_vm_aot_c_generic_call_result_stack_copy_equality_local_smoke_test -- /m:1 && build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_generic_numeric_shared_library_smoke_test.exe && build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_generic_numeric_contracts_test.exe && build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_source_contracts_test.exe && build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_power_shared_library_smoke_test.exe && build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_power_contracts_test.exe && build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test.exe && build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_generic_bool_equality_local_smoke_test.exe && build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_generic_equality_stack_copy_local_smoke_test.exe && build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_generic_not_equal_stack_copy_jump_if_local_smoke_test.exe && build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_generic_call_result_stack_copy_equality_local_smoke_test.exe"`
  - generic numeric smoke: 31 tests, 0 failures, 31 ignored as Unix-only
  - generic numeric contracts 1/0
  - source contracts 24/0
  - power smoke: 1 test, 0 failures, 1 ignored as Unix-only
  - power contracts 2/0
  - generic LOGICAL_NOT: 8 tests, 0 failures, 8 ignored as Unix-only
  - generic bool equality: 5 tests, 0 failures, 5 ignored as Unix-only
  - generic equality stack-copy: 4 tests, 0 failures, 4 ignored as Unix-only
  - generic not-equal stack-copy jump-if: 1 test, 0 failures, 1 ignored as Unix-only
  - call-result stack-copy equality: 1 test, 0 failures, 1 ignored as Unix-only
- Static checks:
  `git diff --check -- .codex/sessions/20260620-2321-aot-07-12-codegen.md docs/parser-and-semantics/csharp-value-type-semir-aot.md docs/plans/aot/07-codegen-register-model-and-environment-isolation.md docs/plans/aot/index.md tests/parser/test_aot_c_generic_numeric_contracts.c tests/parser/test_aot_c_generic_numeric_shared_library_smoke.c tests/parser/test_aot_c_source_contracts.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_generic_numeric_arithmetic.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_locals.c`
  - exit 0; only line-ending normalization warnings were reported.
  `Select-String -Path tests/acceptance/2026-07-05-aot-07-s2-s4-generic-numeric-mixed-i64-u64-sub-mul-local.md,tests/acceptance/2026-07-05-aot-07-s2-s4-generic-numeric-mixed-i64-u64-div-mod-local.md -Pattern '[ \t]+$'`
  - no matches.

## Results
- `backend_aot_c_lowering_generic_numeric_arithmetic.c` adds guarded mixed i64/u64 `DIV/MOD` helpers that reuse
  scalar-kind prefixes, check the casted right operand for zero, and write direct signed i64 `/` / `%` expressions.
- `backend_aot_c_scalar_locals.c` extends the mixed i64/u64 opcode proof from `ADD/SUB/MUL` to
  `ADD/SUB/MUL/DIV/MOD`, so constant preservation, destination declaration, exec write, and live-value consumer proof
  agree.
- Generated C for the new smokes contains:
  - `zr_aot_generic_numeric_mixed_i64_u64_div_scalar_local`
  - `if ((TZrInt64)zr_aot_u1 == (TZrInt64)0)`
  - `ZrCore_Debug_RunError(state, "divide by zero")`
  - `zr_aot_s2 = zr_aot_s0 / (TZrInt64)zr_aot_u1;`
  - `zr_aot_generic_numeric_mixed_i64_u64_mod_scalar_local`
  - `ZrCore_Debug_RunError(state, "modulo by zero")`
  - `zr_aot_s2 = zr_aot_s0 % (TZrInt64)zr_aot_u1;`
- Generated C for the focused shape does not contain the targeted generic numeric runtime helpers or i64 sync helper.

## Acceptance Decision
- Accepted for this slice: proven mixed i64/u64 `DIV/MOD` scalar-local generic numeric lowering is complete.
- This closes proven mixed i64/u64 scalar-local generic `ADD/SUB/MUL/DIV/MOD`.
- Remaining 07 work: mixed u64/f64, dynamic/unproven operands, broader value-copy migration, GC roots/exports/frame
  cleanup, byte-frame narrowing, performance counters, and complete zero-frame typed bodies.
- Broader AOT 07~12 objective remains active and incomplete.
