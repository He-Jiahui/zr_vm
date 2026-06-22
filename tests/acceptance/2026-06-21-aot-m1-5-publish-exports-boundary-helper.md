# AOT M1.5 07-S5 Publish Exports Boundary Helper

## Scope

- Export-tail lowering now emits a single `ZrLibrary_AotRuntime_PublishModuleExports()` guard at the VM/native boundary.
- Generated C keeps an explicit `zr_aot_publish_exports_boundary` marker but no longer expands module export materialization, callable constant publication, closure capture publication, direct `ZrCore_Value_Copy()`, or `ZrCore_Module_AddPubExport(...)` templates.
- Runtime export publication remains centralized in `aot_runtime_materialize_exports()`, including closure/function materialization and `moduleExecuted` handling.
- The runtime helper can resolve the loaded-module record from the generated frame's function when generated frame setup has already elided `frame.recordHandle`.

## Baseline

- RED: after tightening the return/source/shared-library contracts to require helper-only export publication, `zr_vm_aot_c_return_contracts_test` failed on the old generated source because `/* zr_aot_publish_exports_boundary */` was missing.
- Existing checkout baseline remains dirty and broader repository health is not claimed by this slice.

## Test Inventory

- Focused source contracts: `zr_vm_aot_c_return_contracts_test` and `zr_vm_aot_c_source_contracts_test`.
- Generated C compile/runtime smoke: `zr_vm_aot_c_shared_library_smoke_test` and `zr_vm_aot_c_call_shared_library_smoke_test`.
- Regression guard set: `zr_vm_aot_c_frame_setup_contracts_test` and `zr_vm_aot_c_typed_scalar_test`.
- Generated-product boundary checks:
  - generated `.c` sources under `aot_c_shared_library` and `aot_c_call_shared_library` must contain `zr_aot_publish_exports_boundary` and `ZrLibrary_AotRuntime_PublishModuleExports(state, &frame)`;
  - generated `.c` sources in those fixtures must not contain `zr_aot_publish_exports_direct`, direct `ZrCore_Module_AddPubExport(state, frame.module...)`, direct `ZrCore_Value_Copy(state, &zr_aot_published_value...)`, direct closure capture publication, or the old unsupported closure-capture generated failure template.

## Tooling Evidence

- Toolchain: WSL Ubuntu-22.04, GCC/Ninja build directory `build/codex-aot-07-wsl-gcc-debug`.
- RED command:
  ```bash
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_return_contracts_test zr_vm_aot_c_source_contracts_test zr_vm_aot_c_shared_library_smoke_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_return_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_shared_library_smoke_test
  ```
- GREEN focused command:
  ```bash
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_return_contracts_test zr_vm_aot_c_source_contracts_test zr_vm_aot_c_shared_library_smoke_test zr_vm_aot_c_call_shared_library_smoke_test zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_typed_scalar_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_return_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_shared_library_smoke_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_shared_library_smoke_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_frame_setup_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test
  ```
- Generated-product positive check:
  ```bash
  grep -R --include="*.c" "zr_aot_publish_exports_boundary\|ZrLibrary_AotRuntime_PublishModuleExports(state, &frame)" \
      build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_shared_library \
      build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_call_shared_library -n
  ```
- Generated-product negative check:
  ```bash
  grep -R --include="*.c" "zr_aot_publish_exports_direct\|ZrCore_Module_AddPubExport(state, frame\.module\|ZrCore_Value_Copy(state, &zr_aot_published_value\|ZrCore_Closure_FindOrCreateValue(state, frame\.slotBase + zr_aot_closure_variable->index\|unsupported AOT module export closure capture materialization" \
      build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_shared_library \
      build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_call_shared_library -n
  ```
- Registered-test probe:
  ```bash
  ctest --test-dir build/codex-aot-07-wsl-gcc-debug -R "aot_c_return|aot_c_source|aot_c_shared|aot_c_call_shared|aot_c_frame_setup|aot_c_typed_scalar" --output-on-failure
  ```
- Diff hygiene:
  ```bash
  git diff --check
  ```

## Results

- RED observed: return contracts failed 1/1 at the new helper-only export-publication contract while the generator still emitted the old expanded export publication block.
- GREEN focused results: return contracts 1/0, source contracts 19/0, aggregate shared-library smoke 8/0, call shared-library smoke 3/0, frame setup contracts 1/0, and typed scalar 1/0.
- Generated positive check passed with helper-only export publication in refreshed shared and call-shared fixtures.
- Generated negative check passed: refreshed shared/call `.c` sources no longer contain the old direct export marker or generated export materialization templates.
- Registered-test probe matched only `aot_c_typed_scalar` in this build and passed 1/1; focused return/source/shared/call/frame binaries were run directly.
- `git diff --check` exited 0 with only existing LF/CRLF warnings.

## Acceptance Decision

- Accepted for the export-publication boundary-helper slice.
- 07-S5 remains partial. Typed-to-typed native signature routing, in/out writeback, deopt/dynamic bridges, and remaining boundary templates are still later 07 work; 08-12 remain unstarted.
