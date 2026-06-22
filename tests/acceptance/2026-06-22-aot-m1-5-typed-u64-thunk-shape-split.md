# AOT M1.5 / 07-S5 Typed U64 Thunk Shape Split

- Timestamp: 2026-06-22 16:56:34 +08:00
- Status: support sub-slice complete; 07-S5 partial; M1.5/07 partial; 08-12 not started
- Build: WSL Ubuntu-22.04 GCC debug, `build/codex-aot-07-wsl-gcc-debug`

## Scope

- Splits u64 two-argument typed thunk return-shape recognition out of the main u64 thunk emitter.
- New support files:
  - `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_u64_thunk_shapes.h`
  - `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_u64_thunk_shapes.c`
- Moved recognizers:
  - `backend_aot_c_try_get_u64_arg0_arg1_add_return`
  - `backend_aot_c_try_get_u64_arg0_arg1_multiply_return`
  - `backend_aot_c_try_get_u64_arg0_arg1_subtract_return`
- Preserved in the main u64 thunk file:
  - u64 type predicate
  - no-arg constant return recognizer
  - one-arg identity and constant-expression recognizers
  - can-emit gates
  - thunk forward declaration and definition writers
- Out of scope: new u64 behavior, new direct-call routes, u64 division/modulo policy, inline structs, `in` / `out` writeback, deopt/dynamic bridges, and full 07-S5 completion.

## Baseline

- `backend_aot_c_typed_u64_thunks.c` reached 908 physical / 820 non-empty lines after the static two-arg u64 multiply route.
- The next u64 behavior slice would have grown an already large module, so this support split was required before adding more recognizers.

## Test Inventory

- Focused source contract: `tests/parser/test_aot_c_call_contracts.c`.
- Integration smoke: `tests/parser/test_aot_c_typed_direct_call_u64_shared_library_smoke.c`.
- The call contract now verifies:
  - main u64 thunk source includes `backend_aot_c_typed_u64_thunk_shapes.h`
  - shape source contains the two-arg add/multiply/subtract recognizers
  - generated thunk declarations and return expressions remain in the main writer
  - static direct-call and typed direct-call route contracts still point at the existing u64 writer/gates

## Tooling Evidence

RED command:

```powershell
wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_call_contracts_test -j 2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_contracts_test'
```

RED result:

- `zr_vm_aot_c_call_contracts_test`: 8 tests, 1 failure.
- Failure: the u64 contract read the not-yet-created `backend_aot_c_typed_u64_thunk_shapes.c` as NULL.

Focused GREEN command:

```powershell
wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_call_contracts_test zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test -j 2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test'
```

Broader focused AOT command:

```powershell
wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_arithmetic_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_bitwise_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_value_type_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_generic_numeric_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_global_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_logical_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_power_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_return_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_value_semir_contracts_test'
```

## Results

- `zr_vm_aot_c_call_contracts_test`: 8 tests, 0 failures.
- `zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test`: 8 tests, 0 failures.
- Broader focused AOT group:
  - source contracts 19/0
  - call contracts 8/0
  - shared-library smoke 8/0
  - call smoke 5/0
  - typed direct-call smoke 5/0
  - bool smoke 2/0
  - u64 smoke 8/0
  - f64 smoke 11/0
  - arithmetic smoke 5/0
  - bitwise smoke 6/0
  - typed scalar 1/0
  - value-type smoke 1/0
  - generic numeric smoke 1/0
  - global smoke 9/0
  - logical contracts 4/0
  - power smoke 1/0
  - frame setup contracts 1/0
  - return contracts 1/0
  - value SemIR contracts 4/0

Implementation files after this slice:

- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_u64_thunks.c`: 569 physical / 509 non-empty lines.
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_u64_thunk_shapes.c`: 356 physical / 326 non-empty lines.
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_u64_thunk_shapes.h`: 10 physical / 7 non-empty lines.
- `tests/parser/test_aot_c_call_contracts.c`: 923 physical / 866 non-empty lines.

## Acceptance Decision

- Accepted as a behavior-preserving support split.
- No new typed thunk route is accepted by this file; the accepted behavior remains the previously validated u64 no-arg, one-arg, and two-arg direct-call subset.
- 07-S5 remains partial. 07, M1.5, and AOT stages 08-12 remain incomplete.
- `tests/parser/test_aot_c_call_contracts.c` is now near the large-file threshold and should be split before further call-contract expansion.
