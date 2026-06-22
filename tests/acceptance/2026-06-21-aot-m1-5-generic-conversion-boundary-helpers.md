# AOT M1.5 07-S5 Generic Conversion Boundary Helpers

## Scope

- Generic primitive conversion lowering (`TO_BOOL`, `TO_INT`, `TO_UINT`, `TO_FLOAT`) now emits helper guards instead of expanding stack-slot lookup and conversion branches in generated C.
- Generated C keeps the existing `zr_aot_convert_generic_to_*` markers, but no longer emits direct `SZrTypeValue *zr_aot_destination` / `zr_aot_source` locals, generated `ZR_VALUE_IS_TYPE_*` branch ladders, generated `ZR_VALUE_FAST_SET(...)`, or the generated unsupported-conversion failure block.
- Runtime conversion semantics moved to dedicated `ZrLibrary_AotRuntime_ConvertGenericTo*()` helpers in `aot_runtime_values.c`.
- The helpers intentionally do not reuse `ZrLibrary_AotRuntime_ToBool/ToInt/ToUInt/ToFloat()`, because those runtime conversions can honor meta hooks and are not equivalent to the old generated primitive-conversion template.

## Baseline

- RED: after tightening `zr_vm_aot_c_source_contracts_test` to require helper-only generic primitive conversion lowering, the source contract failed because `ZrLibrary_AotRuntime_ToBool(state, &frame, %u, %u)` was missing from the old generated template.
- Investigation: the first implementation reused `ZrLibrary_AotRuntime_ToBool()`, and `zr_vm_aot_c_shared_library_smoke_test` crashed in the generic conversion smoke. `gdb` showed `ZrLibrary_AotRuntime_ToBool()` entering `aot_runtime_invoke_unary_meta()` and then `aot_runtime_functions_equivalent()`, proving this helper has meta-conversion semantics not present in the old generated primitive conversion path.
- Existing checkout baseline remains dirty and broader repository health is not claimed by this slice.

## Test Inventory

- Focused source contract: `zr_vm_aot_c_source_contracts_test`.
- Generated C compile/runtime smoke: `zr_vm_aot_c_shared_library_smoke_test`.
- Regression guard set: `zr_vm_aot_c_call_shared_library_smoke_test`, `zr_vm_aot_c_typed_scalar_test`, `zr_vm_aot_c_return_contracts_test`, and `zr_vm_aot_c_frame_setup_contracts_test`.
- Generated-product boundary checks:
  - generic-conversion generated `.c` must contain `zr_aot_convert_generic_to_bool` and `ZrLibrary_AotRuntime_ConvertGenericToBool(state, &frame, 4, 1)`;
  - generic-conversion generated `.c` and the generic conversion lowering source must not contain the old generated unsupported block, direct generic conversion stack-slot locals, or `ZrLibrary_AotRuntime_To*()` reuse.

## Tooling Evidence

- Toolchain: WSL Ubuntu-22.04, GCC/Ninja build directory `build/codex-aot-07-wsl-gcc-debug`.
- RED command:
  ```bash
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_shared_library_smoke_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_shared_library_smoke_test
  ```
- Crash root-cause command:
  ```bash
  gdb -batch -ex run -ex "bt full" --args ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_shared_library_smoke_test
  ```
- GREEN focused commands:
  ```bash
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_shared_library_smoke_test zr_vm_aot_c_call_shared_library_smoke_test zr_vm_aot_c_typed_scalar_test zr_vm_aot_c_return_contracts_test zr_vm_aot_c_frame_setup_contracts_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_shared_library_smoke_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_shared_library_smoke_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_return_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_frame_setup_contracts_test
  ```
- Generated-product positive check:
  ```bash
  grep -R --include="*.c" "ZrLibrary_AotRuntime_ConvertGenericToBool\|ZrLibrary_AotRuntime_ConvertGenericToInt\|ZrLibrary_AotRuntime_ConvertGenericToUInt\|ZrLibrary_AotRuntime_ConvertGenericToFloat\|zr_aot_convert_generic_to_" \
      build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_shared_library \
      build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_call_shared_library -n
  ```
- Generated-product negative check:
  ```bash
  grep -R --include="*.c" "unsupported AOT generic primitive conversion\|backend_aot_c_write_generic_conversion_unsupported\|ZrLibrary_AotRuntime_ToBool(state, &frame\|ZrLibrary_AotRuntime_ToInt(state, &frame\|ZrLibrary_AotRuntime_ToUInt(state, &frame\|ZrLibrary_AotRuntime_ToFloat(state, &frame\|SZrTypeValue \*zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + 4);\|const SZrTypeValue \*zr_aot_source = ZrCore_Stack_GetValue(frame.slotBase + 1);" \
      zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_generic_conversion.c \
      build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_shared_library/generic_conversion_project/bin/aot_c/src/main.c -n
  ```
- Registered-test probe:
  ```bash
  ctest --test-dir build/codex-aot-07-wsl-gcc-debug -R "aot_c_source|aot_c_shared|aot_c_call_shared|aot_c_typed_scalar|aot_c_return|aot_c_frame_setup" --output-on-failure
  ```
- Diff hygiene:
  ```bash
  git diff --check
  ```

## Results

- RED observed: source contracts failed at the new helper-only conversion contract while the generator still emitted direct conversion branches.
- Root cause confirmed: generic primitive conversion could not reuse `ZrLibrary_AotRuntime_ToBool()` because that helper includes meta-conversion dispatch. Dedicated `ConvertGenericTo*()` helpers preserve the old generated primitive conversion semantics.
- GREEN focused results: source contracts 19/0, aggregate shared-library smoke 8/0, call shared-library smoke 3/0, typed scalar 1/0, return contracts 1/0, and frame setup contracts 1/0.
- Generated positive check passed with `ZrLibrary_AotRuntime_ConvertGenericToBool(state, &frame, 4, 1)` in the refreshed generic-conversion fixture.
- Generated negative check passed for the generic-conversion fixture and lowering source.
- Registered-test probe matched only `aot_c_typed_scalar` in this build and passed 1/1; focused source/shared/call/typed/return/frame binaries were run directly.
- `git diff --check` exited 0 with only existing LF/CRLF warnings.

## Acceptance Decision

- Accepted for the generic primitive conversion boundary-helper slice.
- 07-S5 remains partial. Typed-to-typed native signature routing, in/out writeback, deopt/dynamic bridges, and remaining boundary templates are still later 07 work; 08-12 remain unstarted.
