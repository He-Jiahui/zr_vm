# AOT M1.5 / 07-S5 Static U64 One-Arg Multiply-Constant Typed Thunk

- Timestamp: 2026-06-22 16:29:39 +08:00
- Status: sub-slice complete; 07-S5 partial; M1.5/07 partial; 08-12 not started
- Build: WSL Ubuntu-22.04 GCC debug, `build/codex-aot-07-wsl-gcc-debug`

## Scope

- Adds a one-parameter unsigned integer multiply-constant typed thunk direct-call route.
- Affected layers: AOT C typed thunk recognition/emission, AOT static direct-call generated C contracts, u64 shared-library AOT smoke execution, and AOT plan/module documentation.
- Covered source shape: `func scale(value: uint): uint { return value * 21; }`.
- Covered SemIR shapes:
  - direct const multiply opcodes with argument slot 0
  - `GET_CONSTANT` followed by signed or unsigned multiply opcodes, accepting either operand order
  - narrow parameter-copy / `GET_CONSTANT` / `TO_INT` / signed-multiply forms before `FUNCTION_RETURN`
- Generated thunk ABI: `static TZrUInt64 zr_aot_typed_u64_fn_N(struct SZrState *state, TZrUInt64 zr_aot_arg0)`.
- Generated thunk body returns `(TZrUInt64)(zr_aot_arg0 * (TZrUInt64)constant)`.
- Out of scope: negative signed constants, arbitrary u64 expression trees, multi-arg multiply routes, dynamic overflow policy changes, inline structs, `in` / `out` writeback, deopt/dynamic bridge boxing, and full 07-S5 completion.

## Baseline

- RED baseline: `zr_vm_aot_c_call_contracts_test` failed because the u64 typed thunk source had no `backend_aot_c_try_get_u64_arg0_multiply_constant_return(` contract.
- The previous u64 one-arg constant slices covered addition and ordered subtraction. They did not prove multiply-constant through the same one-argument typed ABI.

## Test Inventory

- Focused source contract: `tests/parser/test_aot_c_call_contracts.c`.
- Integration smoke: `tests/parser/test_aot_c_typed_direct_call_u64_shared_library_smoke.c`.
- Shared smoke support: `tests/parser/aot_c_typed_direct_call_u64_smoke_support.h`.
- Boundary cases:
  - Direct C call route for one `uint` argument.
  - Runtime result 42 from `2 * 21`.
  - Direct-call marker present and `CallStaticDirect` / `CallStackValue` markers absent.
  - Typed destination sync marker absent when scalar-local proof can satisfy later consumers.
- Route guard cases reused from the previous u64 one-arg route:
  - Destination slot must have u64 scalar-local coverage.
  - Call-window argument slot must have u64 scalar-local coverage.
  - Argument slot must be proven written before the static call.
  - Callee metadata must have unsigned callable return and one unsigned parameter.
  - Varargs are rejected.

## Tooling Evidence

RED command:

```powershell
wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_call_contracts_test zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test -j 2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test'
```

RED result:

- First combined RED command hit the 120 second command timeout while rebuilding.
- Repeated focused build completed; direct test execution failed in `zr_vm_aot_c_call_contracts_test`.
- `zr_vm_aot_c_call_contracts_test`: 8 tests, 1 failure.
- Failure: missing source contract text `backend_aot_c_try_get_u64_arg0_multiply_constant_return(`.

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
- `zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test`: 7 tests, 0 failures.
- Broader focused AOT group:
  - source contracts 19/0
  - call contracts 8/0
  - shared-library smoke 8/0
  - call smoke 5/0
  - typed direct-call smoke 5/0
  - bool smoke 2/0
  - u64 smoke 7/0
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

- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_u64_thunks.c`: 788 physical / 709 non-empty lines.
- `tests/parser/test_aot_c_typed_direct_call_u64_shared_library_smoke.c`: 175 physical / 160 non-empty lines.
- `tests/parser/aot_c_typed_direct_call_u64_smoke_support.h`: 211 physical / 191 non-empty lines.
- `tests/parser/test_aot_c_call_contracts.c`: 906 physical / 849 non-empty lines.

## Acceptance Decision

- Accepted for this narrow 07-S5 sub-slice only.
- The accepted behavior is one-argument u64 direct typed calls where the callee returns `arg0 * non-negative-constant`.
- 07-S5 remains partial. 07, M1.5, and AOT stages 08-12 remain incomplete.
