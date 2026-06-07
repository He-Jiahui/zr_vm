# AOT Generic Primitive Conversion Direct Writes

Date: 2026-06-05

## Scope

This slice moves generated AOT C lowering for the primitive subset of generic conversion opcodes onto direct C writes:

- `TO_BOOL`
- `TO_INT`
- `TO_UINT`
- `TO_FLOAT`

The lowering now lives in `backend_aot_c_lowering_generic_conversion.c` instead of the generic values module or runtime helper call path.

## RED

`test_aot_c_source_lowers_generic_primitive_conversions_to_direct_c_writes` was added to `tests/parser/test_aot_c_source_contracts.c`.

Initial failure:

```text
test_aot_c_source_lowers_generic_primitive_conversions_to_direct_c_writes:FAIL: Expected Non-NULL
17 Tests 1 Failures 0 Ignored
```

The missing source was `backend_aot_c_lowering_generic_conversion.c`.

## Implementation

The generated C now emits direct primitive conversion operations for null, bool, signed integer, unsigned integer, and float runtime value tags:

- `TO_BOOL`: null becomes false; bool copies directly; signed, unsigned, and float become `value != 0`.
- `TO_INT`: signed copies directly; unsigned and float cast to `TZrInt64`; bool becomes `1` or `0`.
- `TO_UINT`: unsigned copies directly; signed and float cast to `TZrUInt64`; bool becomes `1u` or `0u`.
- `TO_FLOAT`: float copies directly; signed and unsigned cast to `TZrFloat64`; bool becomes `1.0` or `0.0`.

Same-category scalar conversions use direct `*zr_aot_destination = *zr_aot_source` copies where that preserves the current value payload. Other scalar conversions write with `ZR_VALUE_FAST_SET`.

The following generated-C helper fallback paths are now forbidden for this lowering module and dispatcher path:

- `ZrLibrary_AotRuntime_ToBool`
- `ZrLibrary_AotRuntime_ToInt`
- `ZrLibrary_AotRuntime_ToUInt`
- `ZrLibrary_AotRuntime_ToFloat`

## Boundary

This is a primitive-only AOT C subset for generic conversion opcodes. Object/string/meta/dynamic conversions and any non-primitive conversion semantics remain explicit unsupported boundaries until a typed SemIR or runtime contract defines their non-helper AOT behavior. Unsupported source tags emit `unsupported AOT generic primitive conversion` and exit through `ZR_AOT_C_FAIL()` rather than silently falling back to VM helpers.

Generic shift was inspected but not included in this slice: the generic shift opcodes still carry dynamic/meta behavior, while the proven integer shift families already lower through typed bitwise lowering.

## Validation

WSL GCC:

```text
cmake -S . -B build/codex-wsl-gcc-debug
cmake --build build/codex-wsl-gcc-debug --target zr_vm_parser_shared zr_vm_aot_c_source_contracts_test zr_vm_aot_c_shared_library_smoke_test -j 8
zr_vm_aot_c_source_contracts_test: 17 tests, 0 failures
zr_vm_aot_c_shared_library_smoke_test: 6 tests, 0 failures
```

The GCC build compiled `backend_aot_c_lowering_generic_conversion.c.o` after the build directory was reconfigured.

WSL Clang:

```text
cmake -S . -B build/codex-wsl-clang-debug
cmake --build build/codex-wsl-clang-debug --target zr_vm_parser_shared zr_vm_aot_c_source_contracts_test zr_vm_aot_c_shared_library_smoke_test -j 8
zr_vm_aot_c_source_contracts_test: 17 tests, 0 failures
zr_vm_aot_c_shared_library_smoke_test: 6 tests, 0 failures
```

The Clang build compiled `backend_aot_c_lowering_generic_conversion.c.o`.

Windows MSVC CLI smoke:

```text
cmake -S . -B build\codex-msvc-cli-debug -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTS=OFF -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF
cmake --build build\codex-msvc-cli-debug --target zr_vm_cli_executable --parallel 8
zr_vm_cli.exe tests\fixtures\projects\hello_world\hello_world.zrp
hello world
```

The MSVC build compiled `backend_aot_c_lowering_generic_conversion.c`.

## Files

- `tests/parser/test_aot_c_source_contracts.c`
- `tests/parser/test_aot_c_shared_library_smoke.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_generic_conversion.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c`
- `docs/parser-and-semantics/csharp-value-type-semir-aot.md`
- `.codex/plans/CSharp 值类型与 IL2CPP 风格 SemIR lowering 计划.md`
- `.codex/sessions/20260605-041222-csharp-value-aot-continuation.md`
