# AOT M1.5 07-S5 Direct Return Boundary Helper

## Scope

- Frame-backed `FUNCTION_RETURN` C lowering now keeps the existing `zr_aot_direct_return` marker and emits `ZR_AOT_C_RETURN(ZrLibrary_AotRuntime_Return(state, &frame, sourceSlot, ZR_FALSE))`.
- `ZrLibrary_AotRuntime_Return()` now owns the old generated return boundary work: source/caller value lookup, exception-handler discard, function-top stack guard, return escape, closure close, inline constructor receiver copy-back, constructor result-copy skip, caller result copy, and final stack-top reset.
- Export tail returns still publish exports directly before the direct-return helper. Scalar i64 local returns remain on `ZrLibrary_AotRuntime_ReturnI64()`.

## Baseline

- RED: after flipping `zr_vm_aot_c_return_contracts_test` to require the helper-owned return semantics, the test failed while `ZrLibrary_AotRuntime_Return()` still lacked `callerResultValue`, constructor handling, and inline-constructor receiver copy-back.
- Existing checkout baseline remains dirty and broader repository health is not claimed by this slice.

## Test Inventory

- Focused return source contract: `zr_vm_aot_c_return_contracts_test`.
- Aggregate AOT source contract: `zr_vm_aot_c_source_contracts_test`.
- Generated C compile/runtime smoke: `zr_vm_aot_c_shared_library_smoke_test`.
- Broader focused AOT group: source, shared-library aggregate, return, frame setup, typed scalar, value SemIR, call, control, and global contract/smoke binaries.
- Generated-product boundary check: a frame-backed direct-return generated C fixture must contain `ZrLibrary_AotRuntime_Return(state, &frame, 0, ZR_FALSE)` and must not contain the old generated result/caller locals or direct VM cleanup/copy-back template.

## Tooling Evidence

- Toolchain: WSL Ubuntu-22.04, GCC/Ninja build directory `build/codex-aot-07-wsl-gcc-debug`.
- RED command:
  ```bash
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_return_contracts_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_return_contracts_test
  ```
- GREEN focused commands:
  ```bash
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_return_contracts_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_return_contracts_test
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_shared_library_smoke_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_shared_library_smoke_test
  ```
- Broader focused binaries were built and run directly after the focused checks: source, shared-library aggregate, return, frame setup, typed scalar, value SemIR, call, call shared-library smoke, control, control shared-library smoke, global, and global shared-library smoke.

## Results

- RED observed: `Missing source contract text: SZrTypeValue *callerResultValue;`.
- GREEN focused results: return contracts 1/0, source contracts 19/0, and aggregate shared-library smoke 8/0.
- GREEN broader results: source contracts 19/0, aggregate shared-library smoke 8/0, return contracts 1/0, frame setup contracts 1/0, typed scalar 1/0, value SemIR contracts 4/0, call contracts 4/0, call shared-library smoke 3/0, control contracts 1/0, control shared-library smoke 1/0, global contracts 6/0, global shared-library smoke 7/0.
- Generated check passed: `aot_c_pending_control_direct.c` contains `ZR_AOT_C_RETURN(ZrLibrary_AotRuntime_Return(state, &frame, 0, ZR_FALSE));` and does not contain `TZrStackValuePointer zr_aot_result_slot;`, `SZrTypeValue *zr_aot_result_value;`, `SZrTypeValue *zr_aot_caller_result_value;`, `execution_discard_exception_handlers_for_callinfo(state, zr_aot_call_info);`, or `ZrCore_Function_TryCopyInlineConstructorReceiverBack(state, zr_aot_call_info);`.
- `git diff --check` exited 0. It printed only the repository's existing LF/CRLF conversion warnings.
- The first aggregate shared-library smoke assertion was too broad because the export descriptor fixture returns through the scalar i64 local path. The helper-positive generated-C assertion was narrowed to the frame-backed pending-control fixture, while the export fixture keeps forbidding old direct-return local expansion.
- Large-file note: `tests/parser/test_aot_c_shared_library_smoke.c` is still oversized and this slice only adjusted assertions in existing fixtures. The smallest follow-up boundary remains extracting shared-library smoke scaffolding/control fixtures out of the aggregate smoke file.

## Acceptance Decision

- Accepted for the frame-backed direct-return boundary-helper slice.
- 07-S5 remains partial. Typed-to-typed native signature routing, in/out writeback, deopt/dynamic bridges, and remaining boundary templates are still later 07 work; 08-12 remain unstarted.
