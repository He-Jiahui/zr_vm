# AOT M1.5 / 07-S5 static one-arg u64 bitwise-and-constant typed thunk

## Scope

- Time: 2026-06-22 19:06:21 +08:00.
- Changed behavior: static typed direct-call lowering can now use a native one-argument `TZrUInt64` thunk for `return arg0 & K`.
- Affected layers: AOT C typed u64 thunk recognition, AOT C typed u64 thunk emission, parser/AOT typed call source contracts, u64 generated shared-library smoke.
- Still out of scope: one-arg u64 OR/XOR constants, division/modulo failure channels, inline structs, `in` / `out` writeback, deopt/dynamic bridges, and 07-S5 completion.

## Baseline

- Before this slice, u64 one-argument typed direct-call coverage included identity, add-constant, subtract-constant, and multiply-constant returns.
- u64 bitwise coverage existed for two-argument AND/OR/XOR returns, but not for one-argument constant bitwise returns.
- 07-S5 was already partial; 08-12 were not started.

## Test Inventory

- Focused contract target: `zr_vm_aot_c_typed_call_contracts_test`.
- Focused executable smoke: `zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test`.
- Runtime source case:
  ```zr
  func mask(value: uint): uint {
      return value & 58;
  }
  var seed: uint = 47;
  var value: uint = mask(seed);
  return <int> value;
  ```
- Expected runtime result: `42`.
- Negative generated-C checks: no `ZrLibrary_AotRuntime_CallStaticDirect`, no `ZrLibrary_AotRuntime_CallStackValue`, and no typed destination stack sync marker on the scalar-only path.

## Tooling Evidence

- Tool: WSL Ubuntu-22.04 GCC/CMake/Ninja build tree `build/codex-aot-07-wsl-gcc-debug`.
- RED command:
  ```sh
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_typed_call_contracts_test zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test -j 2
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_call_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test
  ```
- RED result 1: `zr_vm_aot_c_typed_call_contracts_test` failed 1/4 on missing `backend_aot_c_try_get_u64_arg0_bitwise_and_constant_return(`.
- RED result 2: after adding the first recognizer path, `zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test` failed because the generated source did not contain the expected typed thunk. Generated C showed the callee shape was `SET_STACK`, `GET_CONSTANT`, `TO_UINT`, `BITWISE_AND`, `FUNCTION_RETURN`.
- GREEN focused command: same command after extending the recognizer to the current `TO_UINT` constant-conversion shape.
- Broader GREEN command: manual WSL GCC focused AOT group including source, call, typed call, shared-library, call smoke, typed direct-call, bool, u64, f64, arithmetic, bitwise, typed scalar, value-type, generic numeric, global, logical, power, frame setup, return, and value SemIR binaries.

## Results

- `zr_vm_aot_c_typed_call_contracts_test`: 4 tests, 0 failures.
- `zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test`: 12 tests, 0 failures.
- Broader focused AOT group: source 19/0, call contracts 4/0, typed call contracts 4/0, shared 8/0, call smoke 5/0, typed direct-call 5/0, bool 2/0, u64 12/0, f64 11/0, arithmetic 5/0, bitwise 6/0, typed scalar 1/0, value-type 1/0, generic numeric 1/0, global 9/0, logical 4/0, power smoke 1/0, frame setup 1/0, return 1/0, and value SemIR 4/0.
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_u64_thunks.c`: 737 physical / 658 non-empty lines.
- `tests/parser/test_aot_c_typed_call_contracts.c`: 648 physical / 610 non-empty lines.
- `tests/parser/test_aot_c_typed_direct_call_u64_shared_library_smoke.c`: 299 physical / 274 non-empty lines.

## Acceptance Decision

- Accepted as a narrow 07-S5 behavior slice.
- The direct-call route is accepted because the test first failed on missing recognizer/source contract evidence, then the smoke exposed and locked the actual five-instruction u64 constant bitwise shape, and finally the contract plus runtime smoke both passed.
- Remaining risk is in broader 07-S5 ABI work, especially one-arg u64 OR/XOR constant routes, runtime-failure-capable division/modulo routes, inline structs, `in` / `out` writeback, and deopt/dynamic bridges.
