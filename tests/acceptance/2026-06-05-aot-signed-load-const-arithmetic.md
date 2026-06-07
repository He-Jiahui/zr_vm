# AOT Signed Quickened Arithmetic

## Scope

- Added direct generated-C lowering for the signed load-const arithmetic opcode family.
- Added direct generated-C lowering for the signed load-stack-const arithmetic opcode family.
- Added direct generated-C lowering for `ADD_SIGNED_LOAD_STACK_LOAD_CONST`.
- Added direct generated-C lowering for `ADD_SIGNED_MOD_CONST`.
- Added direct generated-C lowering for `ADD_SIGNED_LOAD_STACK`.
- Added direct generated-C lowering for `MUL_SIGNED_LOAD_STACK`.
- Added direct generated-C lowering for the fused signed branch opcode family: `JUMP_IF_GREATER_SIGNED`,
  `JUMP_IF_LESS_EQUAL_SIGNED`, `JUMP_IF_NOT_EQUAL_SIGNED`, and `JUMP_IF_NOT_EQUAL_SIGNED_CONST`.
- Extended signed compare/branch quickening so statically typed bool false branches (`JUMP_IF_BOOL_FALSE`) can still fuse
  into signed branch opcodes instead of materializing a bool temporary before AOT lowering.
- Validated the generated AOT C shared-library smoke that compiles a generated `.so`, exports the ABI descriptor, and executes the entry through the runtime loader.
- Affected layers: archived AOT C source emission, executable-subset gating, AOT C source-contract tests, parser shared-library build integration, generated-C shared-library smoke coverage, semantic documentation, and the active C# value-type/AOT milestone plan.

## Baseline

- `ADD_SIGNED_LOAD_CONST` had an initial generated-C path, but the rest of the signed load-const family still lacked explicit AOT C lowering.
- `SUB_SIGNED_LOAD_CONST`, `MUL_SIGNED_LOAD_CONST`, `DIV_SIGNED_LOAD_CONST`, and `MOD_SIGNED_LOAD_CONST` were not accepted by the AOT executable-subset gate and had no direct function-body dispatcher routes.
- After the signed load-const family landed, `ADD_SIGNED_LOAD_STACK_CONST`, `SUB_SIGNED_LOAD_STACK_CONST`, `MUL_SIGNED_LOAD_STACK_CONST`, `DIV_SIGNED_LOAD_STACK_CONST`, and `MOD_SIGNED_LOAD_STACK_CONST` still lacked direct AOT C lowering and remained outside the executable subset.
- After the const/materializing forms landed, `ADD_SIGNED_LOAD_STACK` still remained a quickened stack/stack interpreter opcode without direct AOT C lowering.
- After stack/stack add landed, `MUL_SIGNED_LOAD_STACK` still remained a quickened stack/stack interpreter opcode without direct AOT C lowering.
- `JUMP_IF_GREATER_SIGNED` still called `ZrLibrary_AotRuntime_ShouldJumpIfGreaterSigned` from generated C, and
  `JUMP_IF_LESS_EQUAL_SIGNED`, `JUMP_IF_NOT_EQUAL_SIGNED`, and `JUMP_IF_NOT_EQUAL_SIGNED_CONST` were not routed through
  the AOT C function-body dispatcher or executable-subset gate.
- Compare/branch quickening still expected generic `JUMP_IF`, but statically typed bool conditions now emit
  `JUMP_IF_BOOL_FALSE`, so ordinary typed signed conditions could remain as compare-to-bool plus bool branch instead of
  collapsing into the more explicit fused branch opcodes.
- The existing typed arithmetic lowering file was 971 lines before this slice, so adding another helper family there would have pushed it past the project modularization threshold.
- RED: GCC `zr_vm_aot_c_source_contracts_test` failed in `test_aot_c_source_lowers_typed_signed_load_const_arithmetic_to_c_expressions` with `Expected Non-NULL` because `backend_aot_c_lowering_typed_arithmetic_load_const.c` did not exist.
- RED: GCC `zr_vm_aot_c_source_contracts_test` failed in `test_aot_c_source_lowers_typed_signed_load_stack_const_arithmetic_to_c_expressions` on missing `backend_aot_write_c_direct_add_signed_load_stack_const`.
- RED: GCC `zr_vm_aot_c_source_contracts_test` failed in `test_aot_c_source_lowers_typed_signed_load_stack_load_const_arithmetic_to_c_expressions` on missing `backend_aot_write_c_direct_add_signed_load_stack_load_const`.
- RED: GCC `zr_vm_aot_c_source_contracts_test` failed in `test_aot_c_source_lowers_typed_arithmetic_to_c_expressions` on missing `backend_aot_write_c_direct_add_signed_mod_const`.
- RED: GCC `zr_vm_aot_c_source_contracts_test` failed in `test_aot_c_source_lowers_typed_signed_load_stack_arithmetic_to_c_expressions` because `backend_aot_c_lowering_typed_arithmetic_load_stack.c` did not exist.
- RED: GCC `zr_vm_aot_c_source_contracts_test` failed in `test_aot_c_source_lowers_typed_signed_load_stack_arithmetic_to_c_expressions` with missing source contract text `zr_aot_left_scalar * zr_aot_right_scalar` before `MUL_SIGNED_LOAD_STACK` joined the load-stack lowering module.
- RED: GCC `zr_vm_aot_c_source_contracts_test` failed in
  `test_aot_c_source_lowers_typed_signed_branch_to_c_comparisons` on missing
  `backend_aot_write_c_direct_jump_if_less_equal_signed` before the fused branch family was routed.
- RED: GCC `zr_vm_aot_c_shared_library_smoke_test` failed before quickening was extended because a signed
  `while (cursor > 0)` source generated compare-to-bool plus `JUMP_IF_BOOL_FALSE` instead of a fused signed branch.

## Test Inventory

- `tests/parser/test_aot_c_source_contracts.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_arithmetic_load_const.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_arithmetic_load_stack.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_arithmetic.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot.c`
- `docs/parser-and-semantics/csharp-value-type-semir-aot.md`

Boundary cases covered:

- The full signed load-const family is covered: `ADD_SIGNED_LOAD_CONST`, `SUB_SIGNED_LOAD_CONST`, `MUL_SIGNED_LOAD_CONST`, `DIV_SIGNED_LOAD_CONST`, and `MOD_SIGNED_LOAD_CONST`.
- The full signed load-stack-const family is covered: `ADD_SIGNED_LOAD_STACK_CONST`, `SUB_SIGNED_LOAD_STACK_CONST`, `MUL_SIGNED_LOAD_STACK_CONST`, `DIV_SIGNED_LOAD_STACK_CONST`, and `MOD_SIGNED_LOAD_STACK_CONST`.
- `ADD_SIGNED_LOAD_STACK_LOAD_CONST` is covered as the currently defined signed load-stack-load-const opcode.
- `ADD_SIGNED_MOD_CONST` is covered as the quickened adjacent signed-add plus const-modulo opcode.
- `ADD_SIGNED_LOAD_STACK` is covered as the quickened signed stack/stack add opcode.
- `MUL_SIGNED_LOAD_STACK` is covered as the quickened signed stack/stack multiply opcode.
- The fused signed branch family is covered: `JUMP_IF_GREATER_SIGNED`, `JUMP_IF_LESS_EQUAL_SIGNED`,
  `JUMP_IF_NOT_EQUAL_SIGNED`, and `JUMP_IF_NOT_EQUAL_SIGNED_CONST`.
- Generated C materializes the right-hand constant into the requested stack slot before evaluating the expression.
- Generated C materializes the left-hand source stack value into the requested staging slot with direct `SZrTypeValue` assignment before evaluating the expression.
- Generated C for `ADD_SIGNED_LOAD_STACK_LOAD_CONST` materializes both the left stack source and right constant staging slot before evaluating the expression.
- Generated C for `ADD_SIGNED_LOAD_STACK` directly reads the two stack slots, emits `left + right`, and preserves the interpreter's left-type result tag.
- Generated C for `MUL_SIGNED_LOAD_STACK` directly reads the two stack slots, emits `left * right`, and stores the interpreter's integer multiply result as `ZR_VALUE_TYPE_INT64`.
- Generated C emits direct `+`, `-`, `*`, `/`, and `%` expressions instead of routing through arithmetic runtime helper calls.
- Generated C for `ADD_SIGNED_MOD_CONST` emits direct `(left + right) % literal` lowering and stores an `int64` result.
- Division by zero and modulo by zero are checked before the generated C operator is evaluated.
- Negative modulo constants are normalized before `%`, matching the interpreter's current signed modulo load-const behavior.
- The AOT executable-subset gate accepts the full signed load-const and signed load-stack-const families.
- The AOT executable-subset gate accepts `ADD_SIGNED_LOAD_STACK`.
- The AOT executable-subset gate accepts `MUL_SIGNED_LOAD_STACK`.
- The AOT executable-subset gate accepts `ADD_SIGNED_MOD_CONST`, and step flags keep its const modulo zero guard behavior.
- The AOT executable-subset gate accepts the fused signed branch family, and AotExecIR recognizes the full family as
  conditional branches.
- Generated C emits direct signed branch comparisons (`>`, `<=`, slot/slot `!=`, slot/constant `!=`) instead of routing
  fused signed branches through `ZrLibrary_AotRuntime_ShouldJumpIfGreaterSigned`.
- Statically typed bool false branches produced from signed comparisons can fuse into signed branch opcodes.
- Generated-frame slot accounting records only the left stack operand for `JUMP_IF_NOT_EQUAL_SIGNED_CONST`; the right
  operand is a constant-pool index, not a stack slot.
- The new module is part of the CMake globbed AOT backend source set after build reconfiguration.
- The generated-C shared-library smoke covers descriptor export, generated `.so` compilation, `dlopen`/`dlsym`, embedded
  module blob metadata, runtime-loader execution, the observable integer result `42`, and a signed branch loop returning
  `10`.

## Tooling Evidence

- GCC Debug build: `build/codex-wsl-gcc-debug`
- Clang Debug build: `build/codex-wsl-clang-debug`
- GCC version observed during configure: `11.4.0`
- Clang version observed during configure: `14.0.0`

Commands run:

```bash
cmake --build build/codex-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test -j 8
./build/codex-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test
gcc -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_arithmetic_load_const.c
clang -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_arithmetic_load_const.c
cmake --build build/codex-wsl-clang-debug --target zr_vm_aot_c_source_contracts_test -j 8
./build/codex-wsl-clang-debug/bin/zr_vm_aot_c_source_contracts_test
cmake -S . -B build/codex-wsl-gcc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build build/codex-wsl-gcc-debug --target zr_vm_parser_shared -j 8
cmake --build build/codex-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test -j 8
./build/codex-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test
cmake --build build/codex-wsl-clang-debug --target zr_vm_parser_shared -j 8
gcc -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c
clang -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c
cmake --build build/codex-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test -j 8
./build/codex-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test
gcc -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_arithmetic_load_const.c
clang -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_arithmetic_load_const.c
gcc -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c
clang -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c
cmake --build build/codex-wsl-clang-debug --target zr_vm_aot_c_source_contracts_test -j 8
./build/codex-wsl-clang-debug/bin/zr_vm_aot_c_source_contracts_test
cmake --build build/codex-wsl-gcc-debug --target zr_vm_parser_shared -j 8
cmake --build build/codex-wsl-clang-debug --target zr_vm_parser_shared -j 8
cmake --build build/codex-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test -j 8
./build/codex-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test
gcc -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_arithmetic.c
gcc -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c
gcc -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot.c
cmake --build build/codex-wsl-clang-debug --target zr_vm_aot_c_source_contracts_test -j 8
./build/codex-wsl-clang-debug/bin/zr_vm_aot_c_source_contracts_test
clang -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_arithmetic.c
clang -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c
clang -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot.c
cmake --build build/codex-wsl-gcc-debug --target zr_vm_parser_shared -j 8
cmake --build build/codex-wsl-clang-debug --target zr_vm_parser_shared -j 8
cmake --build build/codex-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test -j 8
./build/codex-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test
gcc -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_arithmetic_load_stack.c
gcc -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c
gcc -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot.c
clang -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_arithmetic_load_stack.c
clang -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c
clang -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot.c
cmake --build build/codex-wsl-clang-debug --target zr_vm_aot_c_source_contracts_test -j 8
./build/codex-wsl-clang-debug/bin/zr_vm_aot_c_source_contracts_test
cmake --build build/codex-wsl-clang-debug --target zr_vm_parser_shared -j 8
cmake -S . -B build/codex-wsl-gcc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build build/codex-wsl-gcc-debug --target zr_vm_parser_shared -j 8
cmake --build build/codex-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test -j 8
./build/codex-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test
cmake --build build/codex-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test -j 8
./build/codex-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test
gcc -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_library/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_arithmetic_load_stack.c
gcc -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_library/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c
gcc -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_library/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot.c
clang -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_library/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_arithmetic_load_stack.c
clang -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_library/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c
clang -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_library/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot.c
cmake --build build/codex-wsl-gcc-debug --target zr_vm_parser_shared -j 8
cmake --build build/codex-wsl-clang-debug --target zr_vm_aot_c_source_contracts_test -j 8
./build/codex-wsl-clang-debug/bin/zr_vm_aot_c_source_contracts_test
cmake --build build/codex-wsl-clang-debug --target zr_vm_parser_shared -j 8
cmake --build build/codex-wsl-gcc-debug --target zr_vm_aot_c_shared_library_smoke_test -j 8
./build/codex-wsl-gcc-debug/bin/zr_vm_aot_c_shared_library_smoke_test
cmake --build build/codex-wsl-clang-debug --target zr_vm_aot_c_shared_library_smoke_test -j 8
./build/codex-wsl-clang-debug/bin/zr_vm_aot_c_shared_library_smoke_test
gcc -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_library/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_control.c
gcc -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_library/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c
gcc -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_library/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.c
gcc -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_library/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot.c
gcc -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_library/include zr_vm_core/src/zr_vm_core/function.c
clang -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_library/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_control.c
clang -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_library/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c
clang -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_library/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.c
clang -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_library/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot.c
clang -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_library/include zr_vm_core/src/zr_vm_core/function.c
clang -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_library/include zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c
```

Windows smoke commands run:

```powershell
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
where.exe cl
cmake -S . -B build\codex-msvc-cli-debug -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTS=OFF -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF
cmake --build build\codex-msvc-cli-debug --config Debug --target zr_vm_cli_executable --parallel 8
.\build\codex-msvc-cli-debug\bin\Debug\zr_vm_cli.exe .\tests\fixtures\projects\hello_world\hello_world.zrp
```

## Results

- RED: GCC `zr_vm_aot_c_source_contracts_test` failed before implementation because the new load-const lowering module was missing.
- RED: GCC `zr_vm_aot_c_source_contracts_test` failed before stack-const implementation because the stack-const emitter declarations and routes were missing.
- RED: GCC `zr_vm_aot_c_source_contracts_test` failed before load-stack-load-const implementation because the direct emitter declaration and route were missing.
- RED: GCC `zr_vm_aot_c_source_contracts_test` failed before add-mod-const implementation because the fused direct emitter declaration and route were missing.
- RED: GCC `zr_vm_aot_c_source_contracts_test` failed before load-stack implementation because the direct load-stack lowering module was missing.
- RED: GCC `zr_vm_aot_c_source_contracts_test` failed before `MUL_SIGNED_LOAD_STACK` implementation because the stack/stack multiply expression text was missing.
- PASS: GCC `zr_vm_aot_c_source_contracts_test`, 10 tests, 0 failures.
- PASS: Clang `zr_vm_aot_c_source_contracts_test`, 10 tests, 0 failures.
- PASS: GCC `zr_vm_aot_c_source_contracts_test`, 11 tests, 0 failures.
- PASS: Clang `zr_vm_aot_c_source_contracts_test`, 11 tests, 0 failures.
- PASS: GCC syntax-only check for `backend_aot_c_lowering_typed_arithmetic_load_const.c`.
- PASS: Clang syntax-only check for `backend_aot_c_lowering_typed_arithmetic_load_const.c`.
- PASS: GCC syntax-only check for `backend_aot_c_function_body.c`.
- PASS: Clang syntax-only check for `backend_aot_c_function_body.c`.
- PASS: GCC syntax-only check for `backend_aot_c_lowering_typed_arithmetic.c`.
- PASS: Clang syntax-only check for `backend_aot_c_lowering_typed_arithmetic.c`.
- PASS: GCC syntax-only check for `backend_aot.c`.
- PASS: Clang syntax-only check for `backend_aot.c`.
- PASS: GCC `zr_vm_aot_c_source_contracts_test`, 12 tests, 0 failures.
- PASS: Clang `zr_vm_aot_c_source_contracts_test`, 12 tests, 0 failures.
- PASS: GCC syntax-only check for `backend_aot_c_lowering_typed_arithmetic_load_stack.c`.
- PASS: Clang syntax-only check for `backend_aot_c_lowering_typed_arithmetic_load_stack.c`.
- PASS: GCC `zr_vm_parser_shared` build after reconfiguring the build directory; the new module compiled as `backend_aot_c_lowering_typed_arithmetic_load_const.c.o`.
- PASS: Clang `zr_vm_parser_shared` build; the new module compiled as `backend_aot_c_lowering_typed_arithmetic_load_const.c.o`.
- PASS: GCC `zr_vm_parser_shared` build after reconfiguring the build directory; the new load-stack module compiled as `backend_aot_c_lowering_typed_arithmetic_load_stack.c.o`.
- PASS: Clang `zr_vm_parser_shared` build; the new load-stack module compiled as `backend_aot_c_lowering_typed_arithmetic_load_stack.c.o`.
- PASS: Windows MSVC CLI smoke in `build/codex-msvc-cli-debug`; the parser build compiled `backend_aot_c_lowering_typed_arithmetic_load_stack.c`, the CLI executable ran `hello_world.zrp`, and the program printed `hello world`.
- PASS: GCC `zr_vm_aot_c_source_contracts_test`, 12 tests, 0 failures after `MUL_SIGNED_LOAD_STACK` joined the load-stack lowering module.
- PASS: Clang `zr_vm_aot_c_source_contracts_test`, 12 tests, 0 failures after `MUL_SIGNED_LOAD_STACK` joined the load-stack lowering module.
- PASS: GCC and Clang syntax-only checks for `backend_aot_c_lowering_typed_arithmetic_load_stack.c`, `backend_aot_c_function_body.c`, and `backend_aot.c`.
- PASS: GCC `zr_vm_parser_shared` build; it compiled `backend_aot_c_lowering_typed_arithmetic_load_stack.c.o`, `backend_aot_c_function_body.c.o`, and `backend_aot.c.o`.
- PASS: Clang `zr_vm_parser_shared` build; it compiled `backend_aot_c_lowering_typed_arithmetic_load_stack.c.o`, `backend_aot_c_function_body.c.o`, and `backend_aot.c.o`.
- PASS: Windows MSVC CLI smoke in `build/codex-msvc-cli-debug`; the parser build compiled `backend_aot_c_lowering_typed_arithmetic_load_stack.c`, the CLI executable ran `hello_world.zrp`, and the program printed `hello world`.
- PASS: GCC `zr_vm_aot_c_shared_library_smoke_test`, 2 tests, 0 failures. The test compiled generated C into a shared library, loaded `ZrVm_GetAotCompiledModule`, checked the descriptor, and executed entry through `ZrLibrary_AotRuntime_ExecuteEntry` with result `42`.
- PASS: Clang `zr_vm_aot_c_shared_library_smoke_test`, 2 tests, 0 failures. The same generated-C shared-library and runtime-loader path passed under the Clang build.
- PASS: GCC `zr_vm_aot_c_source_contracts_test`, 13 tests, 0 failures after the fused signed branch source contract was added.
- PASS: Clang `zr_vm_aot_c_source_contracts_test`, 13 tests, 0 failures after the fused signed branch source contract was added.
- PASS: GCC and Clang syntax-only checks for `backend_aot_c_lowering_control.c`, `backend_aot_c_function_body.c`,
  `backend_aot_exec_ir.c`, `backend_aot.c`, `zr_vm_core/src/zr_vm_core/function.c`, and
  `zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c`.
- PASS: GCC `zr_vm_parser_shared` build after the fused signed branch and quickening changes.
- PASS: Clang `zr_vm_parser_shared` build after the fused signed branch and quickening changes.
- PASS: GCC `zr_vm_aot_c_shared_library_smoke_test`, 3 tests, 0 failures. The third test verifies generated C contains
  `zr_aot_jump_if_signed_compare`, omits `ZrLibrary_AotRuntime_ShouldJumpIfGreaterSigned`, compiles the generated C into
  a shared library, executes it through `ZrLibrary_AotRuntime_ExecuteEntry`, and returns signed loop result `10`.
- PASS: Clang `zr_vm_aot_c_shared_library_smoke_test`, 3 tests, 0 failures for the same generated-C signed branch path.
- PASS: Corrected `JUMP_IF_NOT_EQUAL_SIGNED_CONST` generated-frame slot accounting so it reserves the left stack slot
  without treating the constant-pool operand as a stack slot; the source contracts and shared-library smokes still pass.
- NOTE: The first GCC parser shared build attempt did not compile the new module because the globbed AOT backend source list had not been regenerated. Re-running CMake for `build/codex-wsl-gcc-debug` fixed the target graph.
- NOTE: The first GCC parser shared build after adding the load-stack module likewise did not compile the new module until `build/codex-wsl-gcc-debug` was reconfigured; the rerun compiled the new object.
- NOTE: Clang `zr_vm_parser_shared` emitted existing warnings in unrelated parser/type-inference files; none were from the new load-const module.
- NOTE: The MSVC CLI smoke emitted existing-style warnings in broad runtime/parser/library files; it did not fail the build or CLI run.
- NOTE: The first parallel Clang parser shared build timed out during concurrent CMake regeneration; a single rerun completed successfully.
- NOTE: The parallel GCC/Clang parser shared build attempt for the load-stack-load-const slice timed out; no lingering build process was visible afterward, and single-toolchain reruns completed successfully.

## Acceptance Decision

- Accepted for the narrow AOT C signed quickened arithmetic and fused signed branch lowering slice covering the
  load-const, load-stack-const, load-stack-load-const, add-mod-const, add-load-stack, multiply-load-stack, and signed
  branch forms above, with generated-C shared-library smoke coverage passing on both WSL GCC and WSL Clang.

Remaining risks:

- This does not add LLVM lowering for the signed load-const/load-stack family.
- This does not add LLVM lowering for the fused signed branch family.
- Generic numeric fallback paths and float fallback behavior remain future work.
