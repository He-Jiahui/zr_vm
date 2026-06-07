# AOT Generic Integer Shift Direct Writes

Date: 2026-06-05

## Scope

This slice moves generated AOT C lowering for the integer-primitive subset of generic shift bytecode onto direct C shift expressions:

- `SHIFT_LEFT`
- `SHIFT_RIGHT`

The lowering lives in `backend_aot_c_lowering_typed_bitwise.c` beside the typed bitwise and typed shift emitters because it reuses the same integer extraction and shift-count guard helpers.

## RED

`test_aot_c_source_lowers_generic_integer_shift_to_direct_c` was added to `tests/parser/test_aot_c_shift_contracts.c`.

Initial failure:

```text
Missing source contract text: backend_aot_write_c_direct_shift_left(FILE *file
test_aot_c_source_lowers_generic_integer_shift_to_direct_c:FAIL
1 Tests 1 Failures 0 Ignored
```

The old generated-C path still emitted `ZrLibrary_AotRuntime_ShiftLeft(state, &frame, ...)` and `ZrLibrary_AotRuntime_ShiftRight(state, &frame, ...)` from `backend_aot_c_function_body.c`.

## Implementation

The generated C now validates both operands with `ZR_VALUE_IS_TYPE_INT`, extracts integer-like values through the existing helpers, checks the shift count as `[0, 63]`, and writes the `ZR_VALUE_TYPE_INT64` result with `ZR_VALUE_FAST_SET`.

Left shift emits an unsigned intermediate before casting back to `TZrInt64`:

```text
(TZrInt64)(zr_aot_left_unsigned << zr_aot_shift_count)
```

Right shift emits the signed expression:

```text
zr_aot_left_scalar >> zr_aot_shift_count
```

Unsupported non-integer, meta, or dynamic shift operands report `unsupported AOT generic integer shift` and fail through `ZR_AOT_C_FAIL()`.

## Boundary

This is the C backend route for the integer-primitive subset of generic `SHIFT_LEFT` / `SHIFT_RIGHT`. Dynamic/meta shift behavior outside that subset remains an explicit unsupported boundary until SemIR or call metadata can represent it without a runtime helper. LLVM shift parity is still future work.

## Validation

WSL GCC:

```text
zr_vm_aot_c_shift_contracts_test: 1 test, 0 failures
zr_vm_aot_c_shared_library_smoke_test: 6 tests, 0 failures
```

WSL Clang:

```text
zr_vm_aot_c_shift_contracts_test: 1 test, 0 failures
zr_vm_aot_c_shared_library_smoke_test: 6 tests, 0 failures
```

Windows MSVC focused contract smoke:

```text
zr_vm_aot_c_shift_contracts_test.exe: 1 test, 0 failures
```

`git diff --check` reported only existing LF-to-CRLF warnings and no whitespace errors. A source scan found no `ZrLibrary_AotRuntime_ShiftLeft(state, &frame` or `ZrLibrary_AotRuntime_ShiftRight(state, &frame` generated-C emission in `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/*.c`.

## Files

- `tests/parser/test_aot_c_shift_contracts.c`
- `tests/CMakeLists.txt`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_bitwise.c`
- `docs/parser-and-semantics/csharp-value-type-semir-aot.md`
- `.codex/plans/CSharp 值类型与 IL2CPP 风格 SemIR lowering 计划.md`
- `.codex/sessions/20260605-041222-csharp-value-aot-continuation.md`
