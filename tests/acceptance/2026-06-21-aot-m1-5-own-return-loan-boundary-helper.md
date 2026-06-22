# AOT M1.5 07-S5 Own Return Loan Boundary Helper

## Scope

- `OWN_RETURN_LOAN` C lowering now emits `ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_OwnReturnLoan(state, &frame, destinationSlot, sourceSlot))` instead of expanding the ownership slot lookup and `ZrCore_Ownership_ReturnLoanValue(...)` call in generated C.
- `ZrLibrary_AotRuntime_OwnReturnLoan()` is now part of the public AOT runtime boundary and delegates through the shared `aot_runtime_own_value()` slot validation path.
- This slice is intentionally narrow: the other ownership operations still keep their current direct-core generated shape.

## Baseline

- RED: after flipping the ownership source contract to require the runtime helper declaration and source implementation, `zr_vm_aot_c_ownership_contracts_test` failed while the helper did not exist.
- Existing checkout baseline remains dirty and broader repository health is not claimed by this slice.

## Test Inventory

- Focused source contract: `zr_vm_aot_c_ownership_contracts_test`.
- Generated C compile/runtime smoke: `zr_vm_aot_c_ownership_shared_library_smoke_test`.
- Aggregate AOT contracts and smoke: `zr_vm_aot_c_source_contracts_test`, `zr_vm_aot_c_shared_library_smoke_test`, `zr_vm_aot_c_return_contracts_test`, and `zr_vm_aot_c_value_semir_contracts_test`.
- Generated-product boundary check: ownership generated C must contain `ZrLibrary_AotRuntime_OwnReturnLoan(state, &frame, 1, 3)`, keep the expected direct-core shape for non-return-loan ownership operations, and not contain the old generated `ZrCore_Ownership_ReturnLoanValue(state, zr_aot_destination, zr_aot_source)` template.

## Tooling Evidence

- Toolchain: WSL Ubuntu-22.04, GCC/Ninja build directory `build/codex-aot-07-wsl-gcc-debug`.
- RED command:
  ```bash
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_ownership_contracts_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_ownership_contracts_test
  ```
- GREEN focused commands:
  ```bash
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_ownership_contracts_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_ownership_contracts_test
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_ownership_shared_library_smoke_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_ownership_shared_library_smoke_test
  ```
- Broader focused command:
  ```bash
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_ownership_contracts_test zr_vm_aot_c_ownership_shared_library_smoke_test zr_vm_aot_c_shared_library_smoke_test zr_vm_aot_c_return_contracts_test zr_vm_aot_c_value_semir_contracts_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_ownership_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_ownership_shared_library_smoke_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_shared_library_smoke_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_return_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_value_semir_contracts_test
  ```

## Results

- RED observed: `Missing source contract text: ZrLibrary_AotRuntime_OwnReturnLoan(struct SZrState *state,`.
- GREEN focused results: ownership contracts 1/0 and ownership shared-library smoke 1/0.
- GREEN broader results: source contracts 19/0, ownership contracts 1/0, ownership shared-library smoke 1/0, aggregate shared-library smoke 8/0, return contracts 1/0, and value SemIR contracts 4/0.
- Generated check passed: `aot_c_ownership_smoke.c` contains `ZrLibrary_AotRuntime_OwnReturnLoan(state, &frame, 1, 3)`, keeps direct `ZrCore_Ownership_UniqueValue`, `ZrCore_Ownership_UpgradeValue`, and `ZrCore_Ownership_ReleaseValue(state, zr_aot_source)` calls, and does not contain the old generated return-loan core template.
- `git diff --check` exited 0. It printed only the repository's existing LF/CRLF conversion warnings.
- The first generated-product check expected `OWN_RELEASE` to use the generic destination/source helper shape; the actual release lowering is intentionally single-source and resets the destination. The shape check was corrected to the generated `ZrCore_Ownership_ReleaseValue(state, zr_aot_source)` contract.
- An earlier focused ownership shared-library build emitted existing `project.c` const-qualifier warnings. They are unrelated to this helperization and did not fail the build.

## Acceptance Decision

- Accepted for the `OWN_RETURN_LOAN` boundary-helper slice.
- 07-S5 remains partial. Typed-to-typed native signature routing, in/out writeback, deopt/dynamic bridges, and remaining boundary templates are still later 07 work; 08-12 remain unstarted.
