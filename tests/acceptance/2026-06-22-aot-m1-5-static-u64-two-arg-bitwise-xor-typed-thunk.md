# AOT M1.5 / 07-S5 static two-arg u64 bitwise-xor typed thunk

## Scope

- Time: 2026-06-22 18:49:12 +08:00.
- Changed behavior: static typed direct-call lowering can now use a native two-argument `TZrUInt64` thunk for `return arg0 ^ arg1`.
- Affected layers: AOT C typed u64 thunk shape recognition, AOT C typed u64 thunk emission, parser/AOT source contracts, u64 generated shared-library smoke.
- Refactor included: the u64 two-argument AND/OR/XOR shape checks now share a private bitwise recognizer helper.
- Still out of scope: division/modulo failure channels, inline structs, `in` / `out` writeback, deopt/dynamic bridges, and 07-S5 completion.

## Baseline

- Before this slice, u64 two-argument typed direct-call coverage included add, subtract, multiply, bitwise AND, and bitwise OR, but not bitwise XOR.
- The new RED contract failed because generated/source contract text had no `return (TZrUInt64)(zr_aot_arg0 ^ zr_aot_arg1);`.
- 07-S5 was already partial; 08-12 were not started.

## Test Inventory

- Focused contract target: `zr_vm_aot_c_typed_call_contracts_test`.
- Focused executable smoke: `zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test`.
- Runtime source case:
  ```zr
  func toggle(left: uint, right: uint): uint {
      return left ^ right;
  }
  var left: uint = 63;
  var right: uint = 21;
  var value: uint = toggle(left, right);
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
- RED result: `zr_vm_aot_c_typed_call_contracts_test` failed 1/4 on missing `return (TZrUInt64)(zr_aot_arg0 ^ zr_aot_arg1);`.
- GREEN focused command: same command after implementation.
- Broader GREEN command: manual WSL GCC focused AOT group including source, call, typed call, shared-library, call smoke, typed direct-call, bool, u64, f64, arithmetic, bitwise, typed scalar, value-type, generic numeric, global, logical, power, frame setup, return, and value SemIR binaries.

## Results

- `zr_vm_aot_c_typed_call_contracts_test`: 4 tests, 0 failures.
- `zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test`: 11 tests, 0 failures.
- Broader focused AOT group: source 19/0, call contracts 4/0, typed call contracts 4/0, shared 8/0, call smoke 5/0, typed direct-call 5/0, bool 2/0, u64 11/0, f64 11/0, arithmetic 5/0, bitwise 6/0, typed scalar 1/0, value-type 1/0, generic numeric 1/0, global 9/0, logical 4/0, power smoke 1/0, frame setup 1/0, return 1/0, and value SemIR 4/0.
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_u64_thunk_shapes.c`: 405 physical / 368 non-empty lines.
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_u64_thunks.c`: 584 physical / 524 non-empty lines.
- `tests/parser/test_aot_c_typed_direct_call_u64_shared_library_smoke.c`: 275 physical / 252 non-empty lines.

## Acceptance Decision

- Accepted as a narrow 07-S5 behavior slice.
- The direct-call route is accepted because the test first failed on the missing u64 `^` thunk expression, then the contract and runtime smoke both passed after adding the recognizer and emitter branch.
- Remaining risk is in broader 07-S5 ABI work, especially runtime-failure-capable u64 division/modulo routes, inline structs, `in` / `out` writeback, and deopt/dynamic bridges.
