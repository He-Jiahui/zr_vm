# AOT M1.5 / 07-S5 Static F64 No-Arg Typed Thunk

- Timestamp: 2026-06-22 13:53:07 +08:00
- Status: sub-slice complete; 07-S5 partial; M1.5/07 partial; 08-12 not started
- Build: WSL Ubuntu-22.04 GCC debug, `build/codex-aot-07-wsl-gcc-debug`

## Scope

- Adds a zero-parameter floating-point constant-return typed thunk direct-call route.
- Affected layers: AOT C typed thunk recognition/emission, f64 static direct-call lowering, typed direct-call routing, and shared-library AOT smoke execution.
- Covered source shape: `func answer(): float { return 5.0; }`.
- Covered SemIR shape: `GET_CONSTANT` followed by `FUNCTION_RETURN` for a callable return typed as float/double.
- Generated thunk ABI: `static TZrFloat64 zr_aot_typed_f64_fn_N(struct SZrState *state)`.
- Generated thunk body returns `(TZrFloat64)constant`.
- Out of scope: f64 parameters, f64 expression returns, arbitrary f64 expression trees, inline structs, `in` / `out` writeback, deopt/dynamic bridge boxing, and full 07-S5 completion.

## Baseline

- RED baseline: `zr_vm_aot_c_call_contracts_test` failed because the AOT C call contract source had no `backend_aot_c_can_emit_typed_f64_no_arg_thunk(const SZrFunction *function)` contract.
- Intermediate failure: after f64 thunk emission existed, `zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test` still missed `/* zr_aot_static_f64_no_arg_direct_call */` because the no-arg typed direct-call helper did not route f64 and fell back to `ZrLibrary_AotRuntime_CallStaticDirect`.

## Test Inventory

- Focused source contract: `tests/parser/test_aot_c_call_contracts.c`.
- Integration smoke: `tests/parser/test_aot_c_typed_direct_call_f64_shared_library_smoke.c`.
- Boundary cases:
  - Direct C call route for a zero-argument `float` callee.
  - Runtime result 42 from `5.0`, then final `<int> value + 37`.
  - Generated f64 thunk declaration/definition with `TZrFloat64`.
  - Direct-call marker present and `CallStaticDirect` / `CallStackValue` markers absent.
  - Typed destination sync marker absent when scalar-local proof can satisfy later consumers.
- Route guard cases:
  - Destination slot must have f64 scalar-local coverage.
  - Callee metadata must have float/double callable return and no parameters.
  - Varargs are rejected by the typed thunk recognizer.

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
- Failure: missing source contract text `backend_aot_c_can_emit_typed_f64_no_arg_thunk(const SZrFunction *function)`.

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
- `zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test`: 1 test, 0 failures.
- Broader focused AOT group:
  - source contracts 19/0
  - call contracts 8/0
  - shared-library smoke 8/0
  - call smoke 5/0
  - typed direct-call smoke 5/0
  - bool smoke 2/0
  - u64 smoke 5/0
  - f64 smoke 1/0
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

## Acceptance Decision

- Accepted for this narrow 07-S5 sub-slice only.
- The accepted behavior is a zero-argument f64 direct typed call where the callee returns a float/double constant.
- 07-S5 remains partial. 07, M1.5, and AOT stages 08-12 remain incomplete.
