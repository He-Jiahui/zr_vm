# Typed Numeric Conversion

## Scope

- Added typed explicit numeric conversion opcodes for signed/unsigned int to float, float to signed/unsigned int, and signed/unsigned integer cross-casts.
- Affected layers: shared instruction enum, parser/compiler lowering, compiler slot/type metadata, interpreter dispatch, AOT C source lowering, focused parser tests, AOT C source-contract tests, and semantic documentation.

## Baseline

- Before this slice, explicit `<float>` from statically signed/unsigned sources, `<int>` / `<uint>` from statically float sources, and signed/unsigned integer cross-casts lowered to generic `TO_FLOAT` / `TO_INT` / `TO_UINT`.
- The first RED build failed because `TO_FLOAT_SIGNED` and `TO_INT_FLOAT` were not declared. The follow-up RED build for this continuation failed because `TO_FLOAT_UNSIGNED` and `TO_UINT_FLOAT` were not declared, proving the runtime test required the expanded typed instruction surface.
- The integer cross-cast RED build failed because `TO_INT_UNSIGNED` and `TO_UINT_SIGNED` were not declared. A follow-up AOT source-contract RED run failed on missing `zr_aot_unsigned_to_signed_limit`, proving the high-bit unsigned-to-signed boundary needed an explicit generated expression rather than an implementation-defined C cast.
- Existing broader GCC runs still show unrelated current-worktree failures in `zr_vm_literal_surface_test` and `zr_vm_compiler_integration_test`, including typed equality/tail-call assertions and a string/int compile error in the old mixed conversion fixture. These were observed but not changed in this slice.

## Test Inventory

- `tests/parser/test_typed_numeric_conversion.c`
- `tests/parser/test_aot_c_source_contracts.c`
- `tests/parser/test_typed_numeric_neg.c`
- `tests/parser/test_lexer_parser_compiler_execution.c`

Boundary cases covered:

- Same-target typed casts still collapse in compiler logic and are not materialized as typed conversion opcodes.
- Signed int to float uses `TO_FLOAT_SIGNED` and returns `7.0`.
- Unsigned int to float uses `TO_FLOAT_UNSIGNED` and returns `9.0`.
- Float to signed int uses `TO_INT_FLOAT` and truncates `2.75` to `2`.
- Unsigned int to signed int uses `TO_INT_UNSIGNED` and returns `17`.
- Unsigned `-1` materialized through `<uint>-1` then converted with `TO_INT_UNSIGNED` returns `-1`, matching C# unchecked-style high-bit wrapping without depending on implementation-defined signed C casts.
- Float to unsigned int uses `TO_UINT_FLOAT` and truncates `12.75` to `12`.
- Signed int to unsigned int uses `TO_UINT_SIGNED` and converts `-3` to its unsigned modulo payload.
- Dynamic, bool, string, and object conversions remain on generic conversion opcodes for future explicit contracts.

## Tooling Evidence

- GCC Debug build: `build/codex-wsl-gcc-debug`
- Clang Debug build: `build/codex-wsl-clang-debug`

Commands run:

```bash
cmake -S . -B build/codex-wsl-gcc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build build/codex-wsl-gcc-debug --target zr_vm_typed_numeric_conversion_test zr_vm_aot_c_source_contracts_test -j 8
./build/codex-wsl-gcc-debug/bin/zr_vm_typed_numeric_conversion_test
./build/codex-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test
gcc -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_conversion.c
cmake --build build/codex-wsl-gcc-debug --target zr_vm_lexer_parser_compiler_execution_test zr_vm_instruction_execution_test zr_vm_compiler_integration_test -j 8
./build/codex-wsl-gcc-debug/bin/zr_vm_lexer_parser_compiler_execution_test
./build/codex-wsl-gcc-debug/bin/zr_vm_instruction_execution_test
cmake -S . -B build/codex-wsl-clang-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build/codex-wsl-clang-debug --target zr_vm_typed_numeric_conversion_test zr_vm_aot_c_source_contracts_test zr_vm_lexer_parser_compiler_execution_test -j 8
./build/codex-wsl-clang-debug/bin/zr_vm_typed_numeric_conversion_test
./build/codex-wsl-clang-debug/bin/zr_vm_aot_c_source_contracts_test
./build/codex-wsl-clang-debug/bin/zr_vm_lexer_parser_compiler_execution_test
clang -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_conversion.c
```

## Results

- RED: GCC focused build failed on undeclared `ZR_INSTRUCTION_OP_TO_FLOAT_SIGNED` and `ZR_INSTRUCTION_OP_TO_INT_FLOAT`.
- RED: GCC focused build failed on undeclared `ZR_INSTRUCTION_OP_TO_FLOAT_UNSIGNED` and `ZR_INSTRUCTION_OP_TO_UINT_FLOAT`.
- RED: GCC focused build failed on undeclared `ZR_INSTRUCTION_OP_TO_INT_UNSIGNED` and `ZR_INSTRUCTION_OP_TO_UINT_SIGNED`.
- RED: GCC `zr_vm_aot_c_source_contracts_test` failed on missing `zr_aot_unsigned_to_signed_limit`.
- PASS: GCC `zr_vm_typed_numeric_conversion_test`, 7 tests.
- PASS: GCC `zr_vm_aot_c_source_contracts_test`, 5 tests.
- PASS: GCC `backend_aot_c_lowering_typed_conversion.c` syntax-only check.
- PASS: GCC `zr_vm_lexer_parser_compiler_execution_test`, 11 tests.
- PASS: GCC `zr_vm_instruction_execution_test` build; runtime still fails in unrelated `test_execute_string_concat_with_dynamic_member_number` (`checksum=null` instead of `checksum=7`).
- PASS: GCC `zr_vm_compiler_integration_test` build; full runtime remains outside this focused acceptance because the current worktree already has broader integration failures.
- PASS: Clang focused build for conversion, AOT source contracts, and lexer/parser/compiler execution tests.
- PASS: Clang `zr_vm_typed_numeric_conversion_test`, 7 tests.
- PASS: Clang `zr_vm_aot_c_source_contracts_test`, 5 tests.
- PASS: Clang `zr_vm_lexer_parser_compiler_execution_test`, 11 tests.
- PASS: Clang `backend_aot_c_lowering_typed_conversion.c` syntax-only check.

## Acceptance Decision

Accepted for the narrow typed numeric conversion slice.

Remaining risks:

- Only signed/unsigned-to-float, float-to-signed/unsigned, and signed/unsigned integer cross-casts are covered.
- Bool/string/object conversion lowering remains on generic paths.
- The full repository integration suite still has unrelated current-worktree failures and was not accepted as green by this slice.
