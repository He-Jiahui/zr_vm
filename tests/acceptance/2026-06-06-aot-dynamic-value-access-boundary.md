# AOT Dynamic Member/Index Value-Access Boundary

Date: 2026-06-06

Scope: generated AOT C lowering for `GET_MEMBER`, `GET_MEMBER_SLOT`, `GET_BY_INDEX`, `SET_MEMBER`, `SET_MEMBER_SLOT`, and `SET_BY_INDEX`.

## Contract

- Generated AOT C must not call AOT runtime instruction helpers for dynamic member/index access.
- Dynamic member/index opcodes emit an explicit unsupported boundary marked `zr_aot_value_unsupported_dynamic_value_access`.
- The boundary records the opcode name, primary and secondary stack operands, and operand/member/cache/index metadata.
- The boundary reports `unsupported AOT dynamic value access` through `ZrCore_Debug_RunError` and exits through `ZR_AOT_C_FAIL()`.
- `GET_MEMBER_SLOT` and `SET_MEMBER_SLOT` still first preserve proven value SemIR direct field lowering. The unsupported boundary only handles unresolved dynamic fallback.
- This does not implement generated dynamic member/index execution; it prevents silent instruction-helper fallback until a static metadata contract exists.
- LLVM parity remains future work.

## RED

`zr_vm_aot_c_global_contracts_test` first failed because `backend_aot_write_c_unsupported_dynamic_value_access(FILE *file,` was missing.

Result: 6 tests, 1 failure, 0 ignored.

## Test Inventory

- `test_aot_c_source_makes_dynamic_member_index_access_explicit_boundary`
- `test_aot_c_generated_shared_library_compiles_dynamic_value_access_boundary`
- The source contract verifies the generated boundary writer, all six opcode routes, operand metadata, error reporting, and absence of old helper calls.
- The generated-C smoke hand-builds all six opcodes, rejects old helper strings, and compiles the generated C under the Unix toolchain path.

## Validation

- GCC: source contracts 17, constant contracts 4, global contracts 6, global shared-library smoke 7, shared-library smoke 6; all with 0 failures.
- Clang: source contracts 17, constant contracts 4, global contracts 6, global shared-library smoke 7, shared-library smoke 6; all with 0 failures.
- MSVC: source contracts 17, constant contracts 4, global contracts 6, global shared-library smoke 7; all with 0 failures. The 7 global shared-library smoke generated-compile tests were ignored as Unix-only.
- Source scan found only the new `zr_aot_value_unsupported_dynamic_value_access` marker in the checked C backend path and none of the six old dynamic member/index helper strings.

## Files

- `tests/parser/test_aot_c_global_contracts.c`
- `tests/parser/test_aot_c_global_shared_library_smoke.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c`
