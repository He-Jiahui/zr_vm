# AOT M1.5 MethodInfo Skeleton

## Scope
- Continued AOT 07-S3 by introducing the public `SZrAotMethodInfo` ABI type and emitting one static method-info constant per generated AOT C function.
- Affected layers: shared AOT ABI header, C backend emitter, frame setup source contract, and focused typed scalar generated-product test.

## Baseline
- 07-S3 had already removed default setup-time observation/debug initialization from generated frame setup.
- Generated AOT C still had no per-function `SZrAotMethodInfo` descriptor, so function identity and frame sizing were not represented as readonly generated metadata.

## Test Inventory
- `tests/parser/test_aot_c_frame_setup_contracts.c`
  - RED source-contract checks require the ABI header to define `SZrAotMethodInfo`.
  - RED emitter checks require a method-info writer that emits `static const SZrAotMethodInfo zr_aot_method_info_%u`.
- `tests/parser/test_aot_c_typed_scalar.c`
  - RED generated-product checks require the focused generated C to contain `zr_aot_method_info_0` with function index, current register-frame byte count, null GC/signature descriptors, and no observation policy.
  - Runtime path still compiles generated C into a shared library and compares AOT execution against the interpreter result.
- `tests/parser/test_aot_c_source_contracts.c` and `tests/parser/test_aot_c_return_contracts.c`
  - Regression coverage for existing AOT source and return contracts.

## Tooling Evidence
- RED frame setup contract:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_frame_setup_contracts_test -j2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_frame_setup_contracts_test"`
  - Result: failed 1/1 because `struct SZrFunction;` and the new MethodInfo ABI text were missing.
- RED typed scalar generated product:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_typed_scalar_test -j2 && timeout 90s ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test"`
  - Result: failed 1/1 because generated C did not contain `static const SZrAotMethodInfo zr_aot_method_info_0`.
- GREEN focused tests:
  - Same commands after implementation.
  - Results: frame setup contracts 1 test, 0 failures; typed scalar 1 test, 0 failures.
- WSL GCC source and return contracts:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_return_contracts_test zr_vm_aot_c_frame_setup_contracts_test -j2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_return_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_frame_setup_contracts_test"`
  - Result: source contracts 19 tests, 0 failures; return contracts 1 test, 0 failures; frame setup contracts 1 test, 0 failures.
- CTest:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm/build/codex-aot-07-wsl-gcc-debug && ctest -R 'aot_c_typed_scalar|frame_setup|return_contracts|source_contracts' --output-on-failure"`
  - Result: registered `aot_c_typed_scalar` matched and passed 1/1. Frame setup, source-contract, and return-contract binaries are not registered as CTests in this build.
- Generated C inspection:
  - Focused generated C contains:
    - `static const SZrAotMethodInfo zr_aot_method_info_0 = {`
    - `.functionIndex = 0u,`
    - `.registerFrameBytes = 6272u,`
    - `.gcRootMap = ZR_NULL,`
    - `.signature = ZR_NULL,`
    - `.observationPolicy = 0u,`
- Difference check:
  - Full `git diff --check` timed out in the currently large dirty worktree.
  - Scoped `git diff --check -- zr_vm_common/include/zr_vm_common/zr_aot_abi.h zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c tests/parser/test_aot_c_frame_setup_contracts.c tests/parser/test_aot_c_typed_scalar.c docs/plans/aot/07-codegen-register-model-and-environment-isolation.md docs/plans/aot/index.md docs/parser-and-semantics/csharp-value-type-semir-aot.md` exited 0 with only existing CRLF/LF warnings.

## Results
- `zr_aot_abi.h` now exposes `SZrAotMethodInfo` with function index, metadata function pointer, register-frame byte count, GC root map pointer, signature pointer, and observation policy.
- `backend_aot_c_emitter.c` now emits one `static const SZrAotMethodInfo` per generated function immediately after function forward declarations.
- The first slice deliberately does not read MethodInfo on the typed scalar hot path. `metadataFunction`, `gcRootMap`, and `signature` are null until later 07/09/10/11 work binds those descriptors.

## Modularization Note
- `backend_aot_c_emitter.c` is 322 lines after this slice and remains below the large-file threshold.
- `tests/parser/test_aot_c_frame_setup_contracts.c` is 257 lines and remains focused.
- `tests/parser/test_aot_c_typed_scalar.c` is 1038 lines. This slice adds only seven focused generated-product assertions. The smallest follow-up split remains extracting typed-scalar generated-C marker/forbidden-token assertions once 07-S2/07-S3 checks stabilize.

## Acceptance Decision
- Accepted as the second 07-S3 sub-slice.
- 07-S3 remains partial. Remaining 07-S3 work includes using MethodInfo as the typed function descriptor, removing fat-frame fields from typed scalar prologue where possible, and preserving required dynamic/boundary fallback paths.
- 07-S4 remains responsible for narrowing pure scalar `registerFrameBytes` to 0; the focused generated C still records the current 6272-byte frame requirement.
