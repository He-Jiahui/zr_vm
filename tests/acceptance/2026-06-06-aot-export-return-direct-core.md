# AOT Export Return Direct Core

## Scope

Generated AOT C no longer emits the instruction-shaped `ZrLibrary_AotRuntime_Return(state, &frame, ..., ZR_TRUE)` helper for root/export `FUNCTION_RETURN`. Export materialization remains an explicit runtime boundary through `ZrLibrary_AotRuntime_PublishModuleExports(state, &frame)`, followed by the generated `zr_aot_direct_return` block.

## RED

- Added `tests/parser/test_aot_c_return_contracts.c` and registered `zr_vm_aot_c_return_contracts_test`.
- First focused GCC run failed as expected:
  - `Missing source contract text: backend_aot_write_c_publish_exports(FILE *file);`
  - `1 Tests 1 Failures 0 Ignored`

## Implementation

- Added `backend_aot_write_c_publish_exports()` in the C control lowering module.
- Added public runtime API `ZrLibrary_AotRuntime_PublishModuleExports()` to publish module exports from the generated frame record.
- Changed `FUNCTION_RETURN` and tail-return lowering so `publishExports` emits the publish boundary and then always emits `backend_aot_write_c_direct_return(...)`.
- Strengthened the generated shared-library smoke with a `pub var` root module that checks generated C for `zr_aot_publish_exports_boundary`, `ZrLibrary_AotRuntime_PublishModuleExports(state, &frame)`, `zr_aot_direct_return`, and no `ZrLibrary_AotRuntime_Return`.

## Validation

- GCC `build-wsl-gcc`:
  - `zr_vm_aot_c_source_contracts_test`: 17 tests, 0 failures.
  - `zr_vm_aot_c_return_contracts_test`: 1 test, 0 failures.
  - `zr_vm_aot_c_shared_library_smoke_test`: 8 tests, 0 failures.
- Clang `build-wsl-clang`:
  - `zr_vm_aot_c_source_contracts_test`: 17 tests, 0 failures.
  - `zr_vm_aot_c_return_contracts_test`: 1 test, 0 failures.
  - `zr_vm_aot_c_shared_library_smoke_test`: 8 tests, 0 failures.
- MSVC `build-msvc`:
  - `zr_vm_aot_c_source_contracts_test`: 17 tests, 0 failures.
  - `zr_vm_aot_c_return_contracts_test`: 1 test, 0 failures.
  - `zr_vm_aot_c_shared_library_smoke_test`: 8 tests, 0 failures, 8 ignored Unix-only runtime paths.

## Production Scan

`backend_aot_c*.c` now has only these `ZrLibrary_AotRuntime_` generated C boundaries:

- `ZrLibrary_AotRuntime_FailGeneratedFunction`
- `ZrLibrary_AotRuntime_BeginGeneratedFunction`
- `ZrLibrary_AotRuntime_PublishModuleExports`

The checked C backend scan found no `ZrLibrary_AotRuntime_Return(state, &frame, ...)` emission and no checked exception-control helper emission.

## Notes

- `git diff --check` completed with the repository's existing LF-to-CRLF warnings and no whitespace error.
- GCC/Clang/MSVC builds still report pre-existing warnings in `zr_vm_library/src/zr_vm_library/project/project.c`; MSVC also reports existing `aot_runtime.c` unreachable-code warnings.
- `tests/parser/test_aot_c_return_contracts.c` keeps return-shape assertions out of the oversized aggregate source contract. `tests/parser/test_aot_c_shared_library_smoke.c` remains oversized and only gained targeted live-generated assertions for this root/export path.

## Remaining Risks

- LLVM still uses its existing return helper route until LLVM parity work.
- Export materialization itself is still a runtime boundary because callable exports depend on loaded module records and closure materialization state; this slice removes the return-instruction helper fallback, not the module export publication service.
