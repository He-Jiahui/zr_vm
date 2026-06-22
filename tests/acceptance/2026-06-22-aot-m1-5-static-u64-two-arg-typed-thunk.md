# AOT M1.5 / 07-S5 Static U64 Two-Arg Typed Thunk

- Timestamp: 2026-06-22 13:15:44 +08:00
- Status: sub-slice complete; 07-S5 partial; M1.5/07 partial; 08-12 not started
- Build: WSL Ubuntu-22.04 GCC debug, `build/codex-aot-07-wsl-gcc-debug`

## Scope

- Adds a two-parameter unsigned integer add-return typed thunk direct-call route.
- Affected layers: AOT C typed thunk recognition/emission, AOT static direct-call generated C contracts, typed direct-call route selection, and shared-library AOT smoke execution.
- Covered source shape: `func sum(left: uint, right: uint): uint { return left + right; }`.
- Covered SemIR shapes:
  - direct `ADD_UNSIGNED` / `ADD_UNSIGNED_PLAIN_DEST` followed by `FUNCTION_RETURN`
  - signed add / plain-destination forms followed by `FUNCTION_RETURN`
  - `ADD_SIGNED_LOAD_STACK` followed by `FUNCTION_RETURN`
  - narrow parameter-copy / `TO_INT` / signed-add forms followed by `FUNCTION_RETURN`
- Generated thunk ABI: `static TZrUInt64 zr_aot_typed_u64_fn_N(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1)`.
- Generated thunk body returns `(TZrUInt64)(zr_aot_arg0 + zr_aot_arg1)`.
- Out of scope: arbitrary u64 expression trees, non-add two-arg u64 routes, f64 typed routes, inline structs, `in` / `out` writeback, deopt/dynamic bridge boxing, and full 07-S5 completion.

## Baseline

- RED baseline: `zr_vm_aot_c_call_contracts_test` failed because the u64 typed thunk source/header had no `backend_aot_c_can_emit_typed_u64_two_arg_thunk(const SZrFunction *function)` contract.
- The previous u64 add-constant slice only covered one argument plus a non-negative constant. It did not prove two scalar u64 parameters through the call-window ABI.

## Test Inventory

- Focused source contract: `tests/parser/test_aot_c_call_contracts.c`.
- Integration smoke: `tests/parser/test_aot_c_typed_direct_call_u64_shared_library_smoke.c`.
- Boundary cases:
  - Direct C call route for two `uint` arguments.
  - Runtime result 42 from `19 + 18`, then final `+ 5`.
  - Direct-call marker present and `CallStaticDirect` / `CallStackValue` markers absent.
  - Typed destination sync marker absent when scalar-local proof can satisfy later consumers.
- Route guard cases:
  - Destination slot must have u64 scalar-local coverage.
  - Both call-window argument slots must have u64 scalar-local coverage.
  - Both argument slots must be proven written before the static call.
  - Callee metadata must have unsigned callable return and two unsigned parameters.
  - Varargs are rejected.

## Tooling Evidence

Tooling:

- WSL Ubuntu-22.04 GCC debug build.
- `cmake --build` for focused test targets.
- Direct binary execution for contract and smoke tests.

RED command:

```powershell
wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_call_contracts_test zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test -j 2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test'
```

RED result:

- `zr_vm_aot_c_call_contracts_test`: 7 tests, 1 failure.
- Failure: missing source contract text `backend_aot_c_can_emit_typed_u64_two_arg_thunk(const SZrFunction *function)`.

Focused GREEN command:

```powershell
wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_call_contracts_test zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test -j 2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test'
```

Broader focused AOT command:

```powershell
wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_arithmetic_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_bitwise_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_value_type_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_generic_numeric_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_global_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_logical_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_power_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_return_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_value_semir_contracts_test'
```

## Results

- `zr_vm_aot_c_call_contracts_test`: 7 tests, 0 failures.
- `zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test`: 4 tests, 0 failures.
- Broader focused AOT group:
  - source contracts 19/0
  - call contracts 7/0
  - shared-library smoke 8/0
  - call smoke 5/0
  - typed direct-call smoke 5/0
  - bool smoke 2/0
  - u64 smoke 4/0
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

- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_u64_thunks.c`: 431 physical / 384 non-empty lines.
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_direct_calls.c`: 449 physical / 398 non-empty lines.
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_calls.c`: 502 physical / 476 non-empty lines.
- `tests/parser/test_aot_c_typed_direct_call_u64_shared_library_smoke.c`: 606 physical / 561 non-empty lines.
- `tests/parser/test_aot_c_call_contracts.c`: 756 physical / 704 non-empty lines.

## Acceptance Decision

- Accepted for this narrow 07-S5 sub-slice only.
- The accepted behavior is two-argument u64 direct typed calls where the callee returns `arg0 + arg1`.
- 07-S5 remains partial. 07, M1.5, and AOT stages 08-12 remain incomplete.
