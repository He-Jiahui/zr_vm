# AOT M1.5 / 07-S5 typed call contract split

## Scope

- Time: 2026-06-22 18:17:42 +08:00.
- Changed test ownership for AOT typed direct-call source contracts.
- Affected layers: parser/AOT C test harness, CMake test registration, AOT 07 plan records.
- No production typed thunk behavior changed in this support slice.

## Baseline

- Before this slice, `tests/parser/test_aot_c_call_contracts.c` mixed VM call-boundary contracts with i64/bool/u64/f64 typed thunk contracts.
- The file had reached 923 physical / 866 non-empty lines after the u64 thunk shape split, so further 07-S5 typed direct-call expansion would keep growing an aggregate boundary-contract file.
- 07-S5 was already partial; 08-12 were not started.

## Test Inventory

- Focused source contract target: `zr_vm_aot_c_call_contracts_test`.
- New focused source contract target: `zr_vm_aot_c_typed_call_contracts_test`.
- Regression smoke target: `zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test`.
- Broader WSL GCC focused AOT group: source, call, typed call, shared-library, call smoke, typed direct-call, bool, u64, f64, arithmetic, bitwise, typed scalar, value-type, generic numeric, global, logical, power, frame setup, return, and value SemIR binaries.
- Boundary for this support slice: ensure the old call contract target keeps only dynamic/generic/static/meta boundary contracts while the new target preserves all typed thunk contract coverage.

## Tooling Evidence

- Tool: WSL Ubuntu-22.04 GCC/CMake/Ninja build tree `build/codex-aot-07-wsl-gcc-debug`.
- RED command:
  ```sh
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_typed_call_contracts_test -j 2
  ```
- RED result: CMake failed because `/mnt/e/Git/zr_vm/tests/parser/test_aot_c_typed_call_contracts.c` did not exist after registering the target.
- Focused GREEN command:
  ```sh
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_call_contracts_test zr_vm_aot_c_typed_call_contracts_test zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test -j 2
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_call_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test
  ```
- Broader GREEN command: manual WSL GCC focused AOT group including the new `zr_vm_aot_c_typed_call_contracts_test` binary.

## Results

- `zr_vm_aot_c_call_contracts_test`: 4 tests, 0 failures.
- `zr_vm_aot_c_typed_call_contracts_test`: 4 tests, 0 failures.
- `zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test`: 8 tests, 0 failures.
- Broader focused AOT group: source 19/0, call contracts 4/0, typed call contracts 4/0, shared 8/0, call smoke 5/0, typed direct-call 5/0, bool 2/0, u64 8/0, f64 11/0, arithmetic 5/0, bitwise 6/0, typed scalar 1/0, value-type 1/0, generic numeric 1/0, global 9/0, logical 4/0, power smoke 1/0, frame setup 1/0, return 1/0, and value SemIR 4/0.
- `tests/parser/test_aot_c_call_contracts.c`: 386 physical / 348 non-empty lines after the split.
- `tests/parser/test_aot_c_typed_call_contracts.c`: 635 physical / 597 non-empty lines after the split.

## Acceptance Decision

- Accepted as a behavior-preserving 07-S5 support split.
- The split is accepted because the old and new focused contract targets both pass, the u64 typed direct-call smoke remains green, and the broader focused AOT group passes with the new target included.
- Remaining work: this does not close 07-S5 and does not add new typed thunk behavior. Continue with TDD for the next typed direct-call behavior slice.
