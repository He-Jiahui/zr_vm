# AOT M1.5 / 07-S5 Static F64 One-Arg Typed Thunk

- Timestamp: 2026-06-22 14:06:21 +08:00
- Status: sub-slice complete; 07-S5 partial; M1.5/07 partial; 08-12 not started
- Build: WSL Ubuntu-22.04 GCC debug, `build/codex-aot-07-wsl-gcc-debug`

## Scope

- Adds a one-parameter floating-point identity-return typed thunk direct-call route.
- Affected layers: AOT C typed thunk recognition/emission, f64 static direct-call generated C contracts, typed direct-call routing, and shared-library AOT smoke execution.
- Covered source shape: `func pass(value: float): float { return value; }`.
- Covered SemIR shape: direct `FUNCTION_RETURN` of parameter slot 0.
- Generated thunk ABI: `static TZrFloat64 zr_aot_typed_f64_fn_N(struct SZrState *state, TZrFloat64 zr_aot_arg0)`.
- Generated thunk body returns `zr_aot_arg0`.
- Out of scope: f64 expression returns, multi-argument f64 routes, inline structs, `in` / `out` writeback, deopt/dynamic bridge boxing, and full 07-S5 completion.

## Baseline

- RED baseline: `zr_vm_aot_c_call_contracts_test` failed because the f64 typed thunk source had no `backend_aot_c_can_emit_typed_f64_one_arg_thunk(const SZrFunction *function)` contract.
- The previous f64 slice only covered zero-argument constant returns and did not prove f64 parameter ABI or call-window argument routing.

## Test Inventory

- Focused source contract: `tests/parser/test_aot_c_call_contracts.c`.
- Integration smoke: `tests/parser/test_aot_c_typed_direct_call_f64_shared_library_smoke.c`.
- Boundary cases:
  - Direct C call route for one `float` argument.
  - Runtime result 42 from `pass(seed)`, then final `<int> value + 37`.
  - Direct-call marker present and `CallStaticDirect` / `CallStackValue` markers absent.
  - Typed destination sync marker absent when scalar-local proof can satisfy later consumers.
- Route guard cases:
  - Destination slot must have f64 scalar-local coverage.
  - Call-window argument slot must have f64 scalar-local coverage.
  - Argument slot must be proven written before the static call.
  - Callee metadata must have float/double callable return and one float/double parameter.
  - Varargs are rejected.

## Tooling Evidence

Tooling:

- WSL Ubuntu-22.04 GCC debug build.
- `cmake --build` for focused test targets.
- Direct binary execution for contract and smoke tests.

RED command:

```powershell
wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_call_contracts_test zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test -j 2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test'
```

RED result:

- `zr_vm_aot_c_call_contracts_test`: 8 tests, 1 failure.
- Failure: missing source contract text `backend_aot_c_can_emit_typed_f64_one_arg_thunk(const SZrFunction *function)`.

Focused GREEN command:

```powershell
wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_call_contracts_test zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test -j 2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test'
```

Broader focused AOT command:

```powershell
wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_arithmetic_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_bitwise_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_value_type_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_generic_numeric_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_global_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_logical_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_power_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_return_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_value_semir_contracts_test'
```

## Results

- `zr_vm_aot_c_call_contracts_test`: 8 tests, 0 failures.
- `zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test`: 2 tests, 0 failures.
- Broader focused AOT group:
  - source contracts 19/0
  - call contracts 8/0
  - shared-library smoke 8/0
  - call smoke 5/0
  - typed direct-call smoke 5/0
  - bool smoke 2/0
  - u64 smoke 5/0
  - f64 smoke 2/0
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

- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_f64_thunks.c`: 145 physical / 122 non-empty lines.
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_direct_calls.c`: 558 physical / 496 non-empty lines.
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_calls.c`: 574 physical / 544 non-empty lines.
- `tests/parser/test_aot_c_typed_direct_call_f64_shared_library_smoke.c`: 304 physical / 281 non-empty lines.
- `tests/parser/test_aot_c_call_contracts.c`: 851 physical / 794 non-empty lines.

## Acceptance Decision

- Accepted for this narrow 07-S5 sub-slice only.
- The accepted behavior is one-argument f64 direct typed calls where the callee returns the argument unchanged.
- 07-S5 remains partial. 07, M1.5, and AOT stages 08-12 remain incomplete.
