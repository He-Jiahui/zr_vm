# AOT M1.5 07-S5 bool state-free typed direct-call ABI

Date: 2026-06-24 09:26:01 +08:00

## Scope

This slice tightens the bool typed direct-call ABI for scalar thunks that do not need runtime state.

- No-arg bool constant-return thunks now emit `static TZrBool zr_aot_typed_bool_fn_N(void)`.
- One-arg bool identity/logical-not thunks now emit `static TZrBool zr_aot_typed_bool_fn_N(TZrBool)`.
- Two-arg and three-arg bool logical/equality thunks now emit state-free `TZrBool` signatures.
- I64, u64, and f64 two-arg comparison thunks returning bool now emit state-free bool result signatures and are called with only scalar locals.

## Implementation

- `backend_aot_c_typed_bool_thunks.c` emits state-free forward declarations and definitions for no/one-arg bool thunks and mixed i64/u64/f64 comparison thunks returning bool.
- `backend_aot_c_typed_bool_two_arg_thunks.c` emits state-free forward declarations and definitions for bool two-arg equality and logical thunks.
- `backend_aot_c_typed_bool_three_arg_thunks.c` emits state-free forward declarations and definitions for bool three-arg logical thunks.
- `backend_aot_c_lowering_typed_bool_calls.c` emits no-state calls for bool no/one/two/three-arg direct calls and mixed i64/u64/f64 comparison direct calls.
- Contracts and bool shared-library smokes now lock state-free bool direct-call signatures and calls.

## Tooling Evidence

RED:

```sh
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_typed_call_contracts_test zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test --parallel 8 >/tmp/zr_aot_bool_state_free_red_build.log && timeout 60s ./build-wsl-gcc/bin/zr_vm_aot_c_typed_call_contracts_test && timeout 360s ./build-wsl-gcc/bin/zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test'
```

Result: typed-call contracts failed 1/4 on missing `static TZrBool zr_aot_typed_bool_fn_%u(void);`.

GREEN:

```sh
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_typed_call_contracts_test zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test --parallel 8'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && timeout 60s ./build-wsl-gcc/bin/zr_vm_aot_c_typed_call_contracts_test && timeout 360s ./build-wsl-gcc/bin/zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_source_contracts_test --parallel 8 && timeout 60s ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test'
```

Results:

- `zr_vm_aot_c_typed_call_contracts_test`: 4 tests, 0 failures.
- `zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test`: 28 tests, 0 failures.
- `zr_vm_aot_c_source_contracts_test`: 19 tests, 0 failures.

Generated C shape checks:

```text
runtime_static_bool_no_arg_project: static TZrBool zr_aot_typed_bool_fn_1(void)
runtime_static_bool_no_arg_project: zr_aot_typed_bool_fn_1()
runtime_static_bool_two_arg_project: static TZrBool zr_aot_typed_bool_fn_1(TZrBool, TZrBool)
runtime_static_bool_two_arg_project: zr_aot_typed_bool_fn_1(zr_aot_b6, zr_aot_b7)
runtime_static_bool_three_arg_project: static TZrBool zr_aot_typed_bool_fn_1(TZrBool, TZrBool, TZrBool)
runtime_static_bool_three_arg_project: zr_aot_typed_bool_fn_1(zr_aot_b7, zr_aot_b8, zr_aot_b9)
runtime_static_i64_two_arg_less_bool_project: static TZrBool zr_aot_typed_bool_fn_1(TZrInt64, TZrInt64)
runtime_static_i64_two_arg_less_bool_project: zr_aot_typed_bool_fn_1(zr_aot_s6, zr_aot_s7)
runtime_static_u64_two_arg_less_bool_project: static TZrBool zr_aot_typed_bool_fn_1(TZrUInt64, TZrUInt64)
runtime_static_u64_two_arg_less_bool_project: zr_aot_typed_bool_fn_1(zr_aot_u6, zr_aot_u7)
runtime_static_f64_two_arg_less_bool_project: static TZrBool zr_aot_typed_bool_fn_1(TZrFloat64, TZrFloat64)
runtime_static_f64_two_arg_less_bool_project: zr_aot_typed_bool_fn_1(zr_aot_f6, zr_aot_f7)
```

The same generated files do not contain `zr_aot_typed_bool_fn_1(state` or `zr_aot_typed_bool_fn_1(struct SZrState`.

## Acceptance Decision

Accepted as a completed 07-S5 bool direct-call ABI tightening sub-slice.

07-S5 remains partial. Full typed ABI, inline structs, `in`/`out` writeback, complete 07-S5 acceptance, performance/SZrValue counters, and stages 08-12 remain open.
