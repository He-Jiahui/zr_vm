# AOT M1.5 / 07-S5 Static F64 One-Arg Multiply-Constant Typed Thunk

- Timestamp: 2026-06-22 15:18:48 +08:00
- Status: sub-slice complete; 07-S5 partial; M1.5/07 partial; 08-12 not started
- Build: WSL Ubuntu-22.04 GCC debug, `build/codex-aot-07-wsl-gcc-debug`

## Scope

- Adds a one-parameter floating-point multiply-constant typed thunk direct-call route.
- Affected layers: AOT C typed thunk recognition/emission, f64 static direct-call generated C contracts, typed direct-call routing, and shared-library AOT smoke execution.
- Covered source shape: `func scale(value: float): float { return value * 21.0; }`.
- Covered SemIR shape: optional parameter copy from slot 0, then `GET_CONSTANT`, `MUL_FLOAT`, and `FUNCTION_RETURN` of the multiply result.
- Generated thunk ABI: `static TZrFloat64 zr_aot_typed_f64_fn_N(struct SZrState *state, TZrFloat64 zr_aot_arg0)`.
- Generated thunk body returns `(TZrFloat64)(zr_aot_arg0 * (TZrFloat64)K)`.
- Out of scope: f64 divide/modulo, wider f64 expressions, additional f64 arities, inline structs, `in` / `out` writeback, deopt/dynamic bridge boxing, and full 07-S5 completion.

## Baseline

- RED baseline: `zr_vm_aot_c_call_contracts_test` failed because the f64 typed thunk source had no `backend_aot_c_try_get_f64_arg0_multiply_constant_return(` contract.
- The previous f64 behavior slices covered no-arg constants, one-arg identity, one-arg add/subtract-constant, and two-arg add/subtract/multiply returns.
- The f64 smoke harness was already split into `tests/parser/aot_c_typed_direct_call_f64_smoke_support.h`.

## Test Inventory

- Focused source contract: `tests/parser/test_aot_c_call_contracts.c`.
- Integration smoke: `tests/parser/test_aot_c_typed_direct_call_f64_shared_library_smoke.c`.
- Shared smoke support: `tests/parser/aot_c_typed_direct_call_f64_smoke_support.h`.
- Boundary cases:
  - Direct C call route for one `float` argument multiplied by a constant inside the callee.
  - Runtime result 42 from `scale(seed)` with `2.0 * 21.0`.
  - Direct-call marker present and `CallStaticDirect` / `CallStackValue` markers absent.
  - Typed destination sync marker absent when scalar-local proof can satisfy later consumers.
- Route guard cases:
  - Destination slot must have f64 scalar-local coverage.
  - The call-window argument slot must have f64 scalar-local coverage.
  - The argument slot must be proven written before the static call.
  - Callee metadata must have float/double callable return and one float/double parameter.
  - Varargs are rejected.
  - Multiplication accepts the constant on either operand side.

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
- Failure: missing source contract text `backend_aot_c_try_get_f64_arg0_multiply_constant_return(`.

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
- `zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test`: 8 tests, 0 failures.
- Broader focused AOT group:
  - source contracts 19/0
  - call contracts 8/0
  - shared-library smoke 8/0
  - call smoke 5/0
  - typed direct-call smoke 5/0
  - bool smoke 2/0
  - u64 smoke 5/0
  - f64 smoke 8/0
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

- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_f64_thunks.c`: 557 physical / 486 non-empty lines.
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_direct_calls.c`: 614 physical / 547 non-empty lines.
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_calls.c`: 613 physical / 581 non-empty lines.
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h`: 729 physical / 724 non-empty lines.
- `tests/parser/test_aot_c_call_contracts.c`: 884 physical / 827 non-empty lines.
- `tests/parser/test_aot_c_typed_direct_call_f64_shared_library_smoke.c`: 200 physical / 183 non-empty lines.
- `tests/parser/aot_c_typed_direct_call_f64_smoke_support.h`: 211 physical / 191 non-empty lines.

## Acceptance Decision

- Accepted for this narrow 07-S5 sub-slice only.
- The accepted behavior is one-argument f64 direct typed calls where the callee returns the argument multiplied by a float constant.
- 07-S5 remains partial. 07, M1.5, and AOT stages 08-12 remain incomplete.
