# AOT M1.5 07-S5 Generic Logical Boundary Helpers

## Scope

- Generic primitive logical lowering (`LOGICAL_NOT`, `JUMP_IF`, `LOGICAL_EQUAL`, `LOGICAL_NOT_EQUAL`) now emits helper guards instead of expanding primitive truthiness/equality branches in generated C.
- Generated C keeps the existing `zr_aot_generic_logical_*` markers, but no longer emits direct generic primitive truthiness/equality `SZrTypeValue` lookup, `ZR_VALUE_IS_TYPE_*` branch ladders, direct `ZR_VALUE_FAST_SET(...)`, or generated unsupported truthiness/equality blocks for these generic paths.
- Runtime primitive truthiness/equality semantics moved to dedicated `ZrLibrary_AotRuntime_GenericPrimitive*()` helpers in `aot_runtime_values.c`.
- The helpers intentionally do not reuse `ZrLibrary_AotRuntime_LogicalNot`, `IsTruthy`, `LogicalEqual`, or `LogicalNotEqual`, because those runtime helpers have broader dynamic/value semantics than the old generated primitive-only templates.

## Baseline

- RED: after tightening `zr_vm_aot_c_logical_contracts_test` to require helper-only generic primitive logical lowering, the logical contract failed on missing `ZrLibrary_AotRuntime_GenericPrimitiveLogicalNot(state, &frame, %u, %u)` and `ZrLibrary_AotRuntime_GenericPrimitiveLogicalEqual(state, &frame, %u, %u, %u)`.
- Existing checkout baseline remains dirty and broader repository health is not claimed by this slice.

## Test Inventory

- Focused source contract: `zr_vm_aot_c_logical_contracts_test`.
- Generated C compile/runtime smoke: `zr_vm_aot_c_logical_shared_library_smoke_test`.
- Regression guard set: `zr_vm_aot_c_source_contracts_test`, `zr_vm_aot_c_shared_library_smoke_test`, `zr_vm_aot_c_call_shared_library_smoke_test`, `zr_vm_aot_c_typed_scalar_test`, `zr_vm_aot_c_return_contracts_test`, and `zr_vm_aot_c_frame_setup_contracts_test`.
- Generated-product boundary checks:
  - generic-truthiness and generic-equality generated `.c` files must contain `ZrLibrary_AotRuntime_GenericPrimitiveLogicalNot`, `GenericPrimitiveIsTruthy`, `GenericPrimitiveLogicalEqual`, and `GenericPrimitiveLogicalNotEqual`;
  - the same generated products must not contain the old generated primitive truthiness/equality branch snippets or broad `LogicalEqual` / `IsTruthy` helper calls.

## Tooling Evidence

- Toolchain: WSL Ubuntu-22.04, GCC/Ninja build directory `build/codex-aot-07-wsl-gcc-debug`.
- RED command:
  ```bash
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_logical_contracts_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_logical_contracts_test
  ```
- GREEN focused and regression commands:
  ```bash
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_logical_contracts_test zr_vm_aot_c_logical_shared_library_smoke_test zr_vm_aot_c_source_contracts_test zr_vm_aot_c_shared_library_smoke_test zr_vm_aot_c_call_shared_library_smoke_test zr_vm_aot_c_typed_scalar_test zr_vm_aot_c_return_contracts_test zr_vm_aot_c_frame_setup_contracts_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_logical_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_logical_shared_library_smoke_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_shared_library_smoke_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_shared_library_smoke_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_return_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_frame_setup_contracts_test
  ```
- Generated-product positive check:
  ```bash
  grep -R -n "ZrLibrary_AotRuntime_GenericPrimitiveLogical\|ZrLibrary_AotRuntime_GenericPrimitiveIsTruthy" \
      build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_shared_library/generic_truthiness_project/bin/aot_c/src/main.c \
      build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_shared_library/generic_equality_project/bin/aot_c/src/main.c
  ```
- Generated-product negative check:
  ```bash
  ! grep -R -n "ZR_VALUE_IS_TYPE_NULL(zr_aot_source->type)\|TZrBool zr_aot_equal = ZR_FALSE;\|ZrLibrary_AotRuntime_LogicalEqual(state, &frame\|ZrLibrary_AotRuntime_IsTruthy(state, &frame" \
      build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_shared_library/generic_truthiness_project/bin/aot_c/src/main.c \
      build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_shared_library/generic_equality_project/bin/aot_c/src/main.c
  ```
- Diff hygiene:
  ```bash
  git diff --check
  ```

## Results

- RED observed: logical contracts failed at the new helper-only truthiness/equality contract while the generator still emitted direct primitive branches.
- GREEN focused results: logical contracts 4/0 and logical shared-library smoke 4/0.
- GREEN regression results: source contracts 19/0, aggregate shared-library smoke 8/0, call shared-library smoke 3/0, typed scalar 1/0, return contracts 1/0, and frame setup contracts 1/0.
- Generated positive check found helper calls in the refreshed generic truthiness and generic equality fixtures.
- Generated negative check passed for the refreshed generic truthiness/equality fixtures.
- `git diff --check` exited 0 with only existing LF/CRLF warnings.

## Acceptance Decision

- Accepted for the generic primitive logical boundary-helper slice.
- 07-S5 remains partial. Typed-to-typed native signature routing, in/out writeback, deopt/dynamic bridges, and remaining boundary templates are still later 07 work; 08-12 remain unstarted.
