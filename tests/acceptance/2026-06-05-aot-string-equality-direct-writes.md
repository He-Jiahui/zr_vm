# AOT String Equality Direct Writes

Date: 2026-06-05

## Scope

This slice moves generated AOT C lowering for typed string equality bytecode onto direct string byte comparisons:

- `LOGICAL_EQUAL_STRING`
- `LOGICAL_NOT_EQUAL_STRING`

The lowering lives in `backend_aot_c_lowering_generic_logical.c` next to generic primitive truthiness/equality and bool binary logical lowering.

## RED

`test_aot_c_source_lowers_string_equality_to_direct_c` was added to `tests/parser/test_aot_c_logical_contracts.c`.

Initial failure:

```text
Missing source contract text: backend_aot_write_c_direct_logical_equal_string(FILE *file
test_aot_c_source_lowers_string_equality_to_direct_c:FAIL
4 Tests 1 Failures 0 Ignored
```

The old generated-C path still emitted `ZrLibrary_AotRuntime_LogicalEqualString(state, &frame, ...)` and `ZrLibrary_AotRuntime_LogicalNotEqualString(state, &frame, ...)` from `backend_aot_c_function_body.c`.

## Implementation

The generated C now validates string operands, casts the payloads with `ZR_CAST_STRING`, reads byte lengths and native byte pointers with `ZrCore_String_GetByteLength` and `ZrCore_String_GetNativeString`, and compares equal-length spans with:

```text
memcmp(zr_aot_left_bytes, zr_aot_right_bytes, zr_aot_left_length) == 0
```

The bool result is written with `ZR_VALUE_FAST_SET`.

The following helper paths are forbidden for this generated-C route:

- `ZrLibrary_AotRuntime_LogicalEqualString`
- `ZrLibrary_AotRuntime_LogicalNotEqualString`
- `ZrCore_String_Equal`

Generated modules now emit `#include "zr_vm_core/string.h"` and `#include <string.h>` for the string accessors and `memcmp`.

## Boundary

This is the C backend path for typed string equality bytecode. Unsupported operands or null string objects emit `unsupported AOT string equality` and exit through `ZR_AOT_C_FAIL()`.

LLVM string equality still uses its existing helper route until LLVM parity is handled.

## Validation

WSL GCC:

```text
zr_vm_aot_c_logical_contracts_test: 4 tests, 0 failures
zr_vm_aot_c_logical_shared_library_smoke_test: 4 tests, 0 failures
```

The string equality smoke compiles explicitly typed string locals with `==` and `!=`, verifies generated C contains `zr_aot_string_logical_equal`, `zr_aot_string_logical_not_equal`, string include/accessor markers, and the `memcmp` equality expression, rejects old string equality helper strings, builds generated C into a shared library, executes through `ZrLibrary_AotRuntime_ExecuteEntry`, and returns integer `17`.

WSL Clang:

```text
zr_vm_aot_c_logical_contracts_test: 4 tests, 0 failures
zr_vm_aot_c_logical_shared_library_smoke_test: 4 tests, 0 failures
```

Windows MSVC focused contract smoke:

```text
zr_vm_aot_c_logical_contracts_test.exe: 4 tests, 0 failures
```

`git diff --check` reported only existing LF-to-CRLF warnings and no whitespace errors.

## Files

- `tests/parser/test_aot_c_logical_contracts.c`
- `tests/parser/test_aot_c_logical_shared_library_smoke.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_generic_logical.c`
- `docs/parser-and-semantics/csharp-value-type-semir-aot.md`
- `.codex/plans/CSharp 值类型与 IL2CPP 风格 SemIR lowering 计划.md`
- `.codex/sessions/20260605-041222-csharp-value-aot-continuation.md`
