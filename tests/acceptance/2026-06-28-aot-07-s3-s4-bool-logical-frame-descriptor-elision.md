# 2026-06-28 AOT 07-S3/S4 bool logical frame-descriptor elision

## Scope

- Slice: M1.5 / 07-S3 and 07-S4 focused bool logical frame-descriptor elision.
- Goal: let generated AOT C omit `ZrAotGeneratedFrame frame` and generated stack-frame setup when a bool logical pipeline is already proven local-only.
- Covered pipeline: i64 comparisons produce bool scalar locals, bool stack copies stay local, typed bool `!` and `==` consume bool locals, and the final bool return is packed through the native return boundary.

## RED

- Source contract RED: `zr_vm_aot_c_frame_setup_contracts_test` failed because `backend_aot_c_frame_descriptor.c` had no bool logical local-only frame proof.
- Generated-product RED: the new bool local logical shared-library smoke initially still found `/* zr_aot_generated_frame_setup */`.
- Follow-up root-cause RED after aligning return proof: frame declaration was omitted, but generated C still emitted a forced bool stack-copy write using `frame.slotBase` before typed `LOGICAL_NOT_BOOL`, so gcc failed with `frame undeclared`.

## Implementation

- Added `backend_aot_c_frame_descriptor_bool_logical_can_use_local_only()` for `LOGICAL_AND`, `LOGICAL_OR`, `LOGICAL_EQUAL_BOOL`, `LOGICAL_NOT_EQUAL_BOOL`, and `LOGICAL_NOT_BOOL`.
- Reused the same proof gates as the emitters: `backend_aot_c_scalar_locals_bool_result_can_skip_value_slot()` and `backend_aot_c_scalar_locals_bool_written_before()`.
- Aligned descriptor-free `FUNCTION_RETURN` eligibility with `backend_aot_try_write_c_typed_return()` by accepting bool/u64/f64 inferred scalar returns in addition to direct typed returns.
- Narrowed forced bool value-slot materialization in stack-copy lowering: generic `LOGICAL_NOT` remains conservative, while typed `LOGICAL_NOT_BOOL` no longer forces a value-slot write when its source bool local is already written and its result can skip the value slot.
- Added source-contract needles covering the bool logical frame proof, inferred return gates, and typed bool NOT stack-copy materialization rule.
- Added a Unix shared-library smoke that compiles and executes a bool local logical pipeline and asserts the generated C has bool local markers but no frame setup, `ZrCore_Stack_GetValue`, or `ZR_VALUE_FAST_SET`.

## GREEN

- WSL gcc:
  - `zr_vm_aot_c_frame_setup_contracts_test`: 1 test, 0 failures.
  - `zr_vm_aot_c_logical_shared_library_smoke_test`: 5 tests, 0 failures.
- WSL clang:
  - `zr_vm_aot_c_frame_setup_contracts_test`: 1 test, 0 failures.
  - `zr_vm_aot_c_logical_shared_library_smoke_test`: 5 tests, 0 failures.
  - Note: clang still warns on pre-existing generated `!x != 0u` expressions in several generated smoke products; tests pass.
- Windows MSVC Debug:
  - Build targets passed for frame setup contracts and logical shared-library smoke.
  - `zr_vm_aot_c_frame_setup_contracts_test.exe`: 1 test, 0 failures.
  - `zr_vm_aot_c_logical_shared_library_smoke_test.exe`: 5 tests, 0 failures, 5 ignored Unix-only tests.
- `ctest -R 'aot_c_(frame_setup|logical)'` on WSL gcc, WSL clang, and MSVC found no registered tests; direct binaries above are the authoritative focused validation.
- `git diff --check` on touched files passed.

## Files

- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_descriptor.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c`
- `tests/parser/test_aot_c_frame_setup_contracts.c`
- `tests/parser/test_aot_c_logical_shared_library_smoke.c`
- `docs/plans/aot/07-codegen-register-model-and-environment-isolation.md`
- `docs/plans/aot/index.md`
- `docs/parser-and-semantics/csharp-value-type-semir-aot.md`

## Remaining Work

- This does not complete all of 07-S3 or 07-S4.
- Dynamic/generic/string logical paths, frame cleanup, GC roots, exported variables, inline value layouts, byte-frame narrowing, broader typed return/result materialization, and performance-counter acceptance remain open.
- This slice only proves that an already scalar-local bool logical pipeline can omit the generated frame descriptor and stack-frame setup.
