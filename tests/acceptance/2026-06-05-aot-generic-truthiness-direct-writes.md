# AOT Generic Truthiness Direct Writes

Date: 2026-06-05

## Scope

This slice moves generated AOT C lowering for the primitive subset of generic truthiness onto direct C expressions:

- `LOGICAL_NOT`
- `JUMP_IF`

The lowering now lives in `backend_aot_c_lowering_generic_logical.c`. The source-contract and shared-library smoke are split into logical-specific test files so the existing AOT source-contract and shared-library smoke files do not keep growing past their current large-file boundary.

## RED

`test_aot_c_source_lowers_generic_truthiness_to_direct_c` was added to `tests/parser/test_aot_c_logical_contracts.c`.

Initial failure:

```text
test_aot_c_source_lowers_generic_truthiness_to_direct_c:FAIL: Expected Non-NULL
1 Tests 1 Failures 0 Ignored
```

The missing source was `backend_aot_c_lowering_generic_logical.c`.

## Implementation

The generated C now computes primitive truthiness directly for null, bool, signed integer, unsigned integer, and float runtime value tags:

- null is false
- bool is its normalized `nativeBool` payload
- signed integer is `nativeInt64 != 0`
- unsigned integer is `nativeUInt64 != 0`
- float is `nativeDouble != 0.0`

`LOGICAL_NOT` emits `ZR_VALUE_FAST_SET(zr_aot_destination, nativeBool, !zr_aot_truthy, ZR_VALUE_TYPE_BOOL)`. `JUMP_IF` emits `if (!zr_aot_truthy) { goto ...; }`.

The following generated-C helper fallback paths are now forbidden for this lowering module and dispatcher path:

- `ZrLibrary_AotRuntime_LogicalNot`
- `ZrLibrary_AotRuntime_IsTruthy`

## Boundary

This is a primitive-only AOT C subset for generic truthiness. String length truthiness, object truthiness, meta/dynamic behavior, and logical `&&` / `||` remain explicit unsupported or helper-backed boundaries until their semantics are made static enough for a direct generated-C contract. Unsupported source tags emit `unsupported AOT generic primitive truthiness` and exit through `ZR_AOT_C_FAIL()`.

## Validation

WSL GCC:

```text
cmake -S . -B build/codex-wsl-gcc-debug
cmake --build build/codex-wsl-gcc-debug --target zr_vm_parser_shared zr_vm_aot_c_logical_contracts_test zr_vm_aot_c_logical_shared_library_smoke_test -j 8
zr_vm_aot_c_logical_contracts_test: 1 test, 0 failures
zr_vm_aot_c_logical_shared_library_smoke_test: 1 test, 0 failures
```

The GCC build compiled `backend_aot_c_lowering_generic_logical.c.o`.

WSL Clang:

```text
cmake -S . -B build/codex-wsl-clang-debug
cmake --build build/codex-wsl-clang-debug --target zr_vm_parser_shared zr_vm_aot_c_logical_contracts_test zr_vm_aot_c_logical_shared_library_smoke_test -j 8
zr_vm_aot_c_logical_contracts_test: 1 test, 0 failures
zr_vm_aot_c_logical_shared_library_smoke_test: 1 test, 0 failures
```

The Clang build compiled `backend_aot_c_lowering_generic_logical.c.o`.

Windows MSVC CLI smoke:

```text
cmake --build build\codex-msvc-cli-debug --target zr_vm_cli_executable --parallel 8
zr_vm_cli.exe tests\fixtures\projects\hello_world\hello_world.zrp
hello world
```

The MSVC build compiled `backend_aot_c_lowering_generic_logical.c`.

Language pipeline smoke was also attempted after adding the logical targets to the pipeline lists:

```text
ZR_VM_TEST_TIER=smoke ctest --test-dir build/codex-wsl-gcc-debug -R "^language_pipeline$" --output-on-failure
```

It did not reach the new logical AOT targets. The run failed first in `zr_vm_instruction_execution_test` on the existing string-concat/dynamic member and control-flow instruction execution path (`test_instruction_execution.c` / `execution_dispatch.c` are already dirty in the workspace). This acceptance therefore relies on the focused GCC/Clang logical targets plus the Windows CLI smoke above.

## Files

- `tests/CMakeLists.txt`
- `tests/parser/test_aot_c_logical_contracts.c`
- `tests/parser/test_aot_c_logical_shared_library_smoke.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_control.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_generic_logical.c`
- `docs/parser-and-semantics/csharp-value-type-semir-aot.md`
- `.codex/plans/CSharp 值类型与 IL2CPP 风格 SemIR lowering 计划.md`
- `.codex/sessions/20260605-041222-csharp-value-aot-continuation.md`
