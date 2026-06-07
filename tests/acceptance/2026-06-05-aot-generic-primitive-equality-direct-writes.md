# AOT Generic Primitive Equality Direct Writes

Date: 2026-06-05

## Scope

This slice moves generated AOT C lowering for the primitive subset of generic equality onto direct C expressions:

- `LOGICAL_EQUAL`
- `LOGICAL_NOT_EQUAL`

The lowering lives in `backend_aot_c_lowering_generic_logical.c` with the existing generic truthiness lowering.

## RED

`test_aot_c_source_lowers_generic_primitive_equality_to_direct_c` was added to `tests/parser/test_aot_c_logical_contracts.c`.

Initial failure:

```text
test_aot_c_source_lowers_generic_truthiness_to_direct_c:PASS
Missing source contract text: backend_aot_c_write_generic_equality_unsupported
test_aot_c_source_lowers_generic_primitive_equality_to_direct_c:FAIL
2 Tests 1 Failures 0 Ignored
```

The old generated-C path still emitted `ZrLibrary_AotRuntime_LogicalEqual(state, &frame, ...)` from `backend_aot_c_lowering_values.c`.

## Implementation

The generated C now computes primitive equality directly when both runtime value tags are the same primitive kind:

- null equals null
- bool compares normalized `nativeBool != 0u` payloads
- signed integer compares `nativeInt64`
- unsigned integer compares `nativeUInt64`
- float compares `nativeDouble`

Different runtime types produce false. `LOGICAL_EQUAL` writes `zr_aot_equal`; `LOGICAL_NOT_EQUAL` writes `!zr_aot_equal`.

The following helper fallback paths are now forbidden for this generic primitive equality lowering:

- `ZrLibrary_AotRuntime_LogicalEqual`
- `ZrCore_Value_Equal`

## Boundary

This is a primitive-only AOT C subset for generic equality. String equality, object/native pointer equality, dynamic/meta equality, and user-defined equality remain outside this direct generated-C contract. Unsupported runtime tags emit `unsupported AOT generic primitive equality` and exit through `ZR_AOT_C_FAIL()`.

## Validation

WSL GCC:

```text
cmake --build build/codex-wsl-gcc-debug --target zr_vm_parser_shared zr_vm_aot_c_logical_contracts_test zr_vm_aot_c_logical_shared_library_smoke_test -j 8
zr_vm_aot_c_logical_contracts_test: 2 tests, 0 failures
zr_vm_aot_c_logical_shared_library_smoke_test: 2 tests, 0 failures
```

The generated-C equality smoke compiles a mixed int/bool-returning function so `choose(true) == 3`, `choose(true) != 4`, and `choose(true) == 4` stay on the generic equality path. It verifies direct generic equality markers, rejects the old equality helper strings, builds the generated C into a shared library, executes through `ZrLibrary_AotRuntime_ExecuteEntry`, and returns integer `11`.

WSL Clang:

```text
cmake -S . -B build/codex-wsl-clang-debug
cmake --build build/codex-wsl-clang-debug --target zr_vm_parser_shared zr_vm_aot_c_logical_contracts_test zr_vm_aot_c_logical_shared_library_smoke_test -j 8
zr_vm_aot_c_logical_contracts_test: 2 tests, 0 failures
zr_vm_aot_c_logical_shared_library_smoke_test: 2 tests, 0 failures
```

The Clang build compiled `backend_aot_c_lowering_generic_logical.c.o`.

Windows MSVC CLI smoke:

```text
cmake --build build\codex-msvc-cli-debug --target zr_vm_cli_executable --parallel 8
zr_vm_cli.exe tests\fixtures\projects\hello_world\hello_world.zrp
hello world
```

The MSVC build compiled `backend_aot_c_lowering_generic_logical.c` and `backend_aot_c_lowering_values.c`.

## Files

- `tests/parser/test_aot_c_logical_contracts.c`
- `tests/parser/test_aot_c_logical_shared_library_smoke.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_generic_logical.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c`
- `docs/parser-and-semantics/csharp-value-type-semir-aot.md`
- `.codex/plans/CSharp 值类型与 IL2CPP 风格 SemIR lowering 计划.md`
- `.codex/sessions/20260605-041222-csharp-value-aot-continuation.md`
