# AOT M1.5 07-S5 Ownership Boundary Helpers

## Scope

- All C ownership lowering now emits `ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_Own*(state, &frame, destinationSlot, sourceSlot))` helper calls.
- The generated C no longer expands ownership destination/source slot lookup or direct `ZrCore_Ownership_*Value(...)` calls for `OWN_UNIQUE`, `OWN_BORROW`, `OWN_LOAN`, `OWN_RETURN_LOAN`, `OWN_SHARE`, `OWN_WEAK`, `OWN_DETACH`, `OWN_UPGRADE`, or `OWN_RELEASE`.
- Runtime helper semantics are unchanged: the existing `ZrLibrary_AotRuntime_Own*` helpers still validate frame slots and delegate to the matching core ownership operation.

## Baseline

- RED: after flipping `zr_vm_aot_c_ownership_contracts_test` to require helper-only C ownership lowering, the test failed while `backend_aot_c_lowering_values.c` still used the generated direct-core ownership template.
- The immediately prior `OWN_RETURN_LOAN` slice centralized only return-loan. Other ownership operations still emitted generated destination/source lookup and direct core calls.
- Existing checkout baseline remains dirty and broader repository health is not claimed by this slice.

## Test Inventory

- Focused source contract: `zr_vm_aot_c_ownership_contracts_test`.
- Generated C compile/runtime smoke: `zr_vm_aot_c_ownership_shared_library_smoke_test`.
- Aggregate AOT contracts and smoke: `zr_vm_aot_c_source_contracts_test`, `zr_vm_aot_c_shared_library_smoke_test`, `zr_vm_aot_c_return_contracts_test`, and `zr_vm_aot_c_value_semir_contracts_test`.
- Generated-product boundary check: ownership generated C must contain all nine `ZrLibrary_AotRuntime_Own*` helper calls with the expected slots and must not contain the old generated ownership-core marker or direct core-call templates.

## Tooling Evidence

- Toolchain: WSL Ubuntu-22.04, GCC/Ninja build directory `build/codex-aot-07-wsl-gcc-debug`.
- RED command:
  ```bash
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_ownership_contracts_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_ownership_contracts_test
  ```
- GREEN focused command:
  ```bash
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_ownership_contracts_test zr_vm_aot_c_ownership_shared_library_smoke_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_ownership_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_ownership_shared_library_smoke_test
  ```
- Broader build and test commands were run separately after one all-in-one build-and-run command timed out at the harness limit:
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

- RED observed: `Missing source contract text: backend_aot_write_c_direct_ownership_helper_call(`.
- GREEN focused results: ownership contracts 1/0 and ownership shared-library smoke 1/0.
- Generated check passed: `aot_c_ownership_smoke.c` contains `OwnUnique`, `OwnBorrow`, `OwnLoan`, `OwnReturnLoan`, `OwnShare`, `OwnWeak`, `OwnDetach`, `OwnUpgrade`, and `OwnRelease` helper calls with the expected frame slots. It does not contain `zr_aot_value_exec_ownership_core`, `zr_aot_value_exec_ownership_release`, or the old direct `ZrCore_Ownership_*Value` generated templates.
- Broader GREEN results: source contracts 19/0, ownership contracts 1/0, ownership shared-library smoke 1/0, aggregate shared-library smoke 8/0, return contracts 1/0, and value SemIR contracts 4/0.
- While rerunning broader contracts, `zr_vm_aot_c_source_contracts_test` first exposed an existing scalar-stack-copy contract drift: the checked implementation reads `instruction->instruction.operand.operand1[0] == stackSlot`, but the contract still expected `nextInstruction->instruction.operand.operand1[0] == destinationSlot`. The assertion was corrected to the existing implementation shape, then source contracts passed 19/0.
- `git diff --check` exited 0. It printed only the repository's existing LF/CRLF conversion warnings.

## Acceptance Decision

- Accepted for the ownership boundary-helper slice.
- 07-S5 remains partial. Typed-to-typed native signature routing, in/out writeback, deopt/dynamic bridges, and remaining boundary templates are still later 07 work; 08-12 remain unstarted.
