# AOT M1.5 07-S5 f64 state-free typed direct-call ABI

Date: 2026-06-24 09:10:18 +08:00

## Scope

This slice tightens the f64 typed direct-call ABI for pure scalar thunks that do not need runtime state.

- No-arg f64 constant-return thunks now emit `static TZrFloat64 zr_aot_typed_f64_fn_N(void)`.
- One-arg f64 identity, unary, and nonzero constant arithmetic/modulo thunks now emit `static TZrFloat64 zr_aot_typed_f64_fn_N(TZrFloat64)`.
- Two-arg and three-arg f64 add/subtract/multiply thunks now emit state-free `TZrFloat64` signatures.
- Two-arg and three-arg f64 divide/modulo keep `struct SZrState *state` because their generated zero-denominator paths call `ZrCore_Debug_RunError(state, ...)`.

## Implementation

- `backend_aot_c_typed_f64_thunks.c` now exposes state-free predicates for two-arg and three-arg pure f64 thunks and emits state-free forward declarations and definitions for no/one/two/three-arg pure shapes.
- `backend_aot_c_typed_direct_f64_calls.{h,c}` now reports `outPassStateToThunk` for two-arg and three-arg f64 direct-call routes.
- `backend_aot_c_typed_direct_calls.c` threads f64 pass-state decisions into the f64 call writers.
- `backend_aot_c_lowering_calls.c` emits no-state f64 calls for no/one-arg thunks and conditionally emits stateful versus state-free calls for two/three-arg thunks.
- Contracts and f64 shared-library smokes now lock state-free pure f64 direct-call signatures and stateful divide/modulo signatures.

## Tooling Evidence

RED:

```sh
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_typed_call_contracts_test zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test --parallel 8 >/tmp/zr_aot_f64_state_free_red_build.log && timeout 60s ./build-wsl-gcc/bin/zr_vm_aot_c_typed_call_contracts_test && timeout 240s ./build-wsl-gcc/bin/zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test'
```

Result: typed-call contracts failed 1/4 on missing `backend_aot_c_can_emit_typed_f64_two_arg_state_free_thunk(const SZrFunction *function)`.

GREEN:

```sh
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_typed_call_contracts_test zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test --parallel 8'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && timeout 60s ./build-wsl-gcc/bin/zr_vm_aot_c_typed_call_contracts_test && timeout 300s ./build-wsl-gcc/bin/zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_source_contracts_test --parallel 8 && timeout 60s ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test'
```

Results:

- `zr_vm_aot_c_typed_call_contracts_test`: 4 tests, 0 failures.
- `zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test`: 19 tests, 0 failures.
- `zr_vm_aot_c_source_contracts_test`: 19 tests, 0 failures.

Generated C shape checks:

```text
runtime_static_f64_two_arg_project: static TZrFloat64 zr_aot_typed_f64_fn_1(TZrFloat64, TZrFloat64)
runtime_static_f64_two_arg_project: zr_aot_typed_f64_fn_1(zr_aot_f6, zr_aot_f7)
runtime_static_f64_two_arg_divide_project: static TZrFloat64 zr_aot_typed_f64_fn_1(struct SZrState *state, TZrFloat64, TZrFloat64)
runtime_static_f64_two_arg_divide_project: zr_aot_typed_f64_fn_1(state, zr_aot_f6, zr_aot_f7)
runtime_static_f64_three_arg_add_project: static TZrFloat64 zr_aot_typed_f64_fn_1(TZrFloat64, TZrFloat64, TZrFloat64)
runtime_static_f64_three_arg_add_project: zr_aot_typed_f64_fn_1(zr_aot_f7, zr_aot_f8, zr_aot_f9)
runtime_static_f64_three_arg_divide_project: static TZrFloat64 zr_aot_typed_f64_fn_1(struct SZrState *state, TZrFloat64, TZrFloat64, TZrFloat64)
runtime_static_f64_three_arg_divide_project: zr_aot_typed_f64_fn_1(state, zr_aot_f7, zr_aot_f8, zr_aot_f9)
```

## Acceptance Decision

Accepted as a completed 07-S5 f64 direct-call ABI tightening sub-slice.

07-S5 remains partial. Bool ABI parity, full typed ABI, inline structs, `in`/`out` writeback, complete 07-S5 acceptance, performance/SZrValue counters, and stages 08-12 remain open.
