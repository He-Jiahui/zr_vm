# AOT M1.5 07-S5 scalar typed direct-call aggregate guardrail

Date: 2026-06-24 09:41:44 +08:00

## Scope

This slice adds an aggregate generated-product guardrail for the current scalar typed-to-typed direct-call ABI.

- A single generated fixture exercises i64, u64, f64, and bool two-argument typed direct calls.
- The guardrail requires direct typed thunk declarations, definitions, and call markers for all four scalar families.
- The guardrail rejects destination `SZrTypeValue` materialization, stack-sync markers, runtime call fallback, and `state` as the first typed-thunk argument.
- The guardrail also scopes into each generated scalar typed-thunk body and rejects interpreter environment symbols and `SZrTypeValue` usage there.

## Implementation

- `tests/parser/test_aot_c_guardrail_contracts.c` now compiles a mixed scalar typed-call source fixture, emits generated C through the AOT writer, and checks the generated product.
- The same test file now has a helper that detects stateful `zr_aot_typed_i64_fn_`, `zr_aot_typed_u64_fn_`, `zr_aot_typed_f64_fn_`, and `zr_aot_typed_bool_fn_` usages.
- A scoped function-body guardrail checks generated scalar typed-thunk definitions for zero interpreter-environment injection.
- `tests/CMakeLists.txt` links `zr_vm_aot_c_guardrail_contracts_test` with parser/core/library dependencies so guardrail tests can generate AOT C, not only classify static strings.

## Tooling Evidence

RED:

```sh
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_guardrail_contracts_test --parallel 8'
```

Result: build failed because `test_aot_c_guardrail_contracts.c` now includes `zr_vm_parser/compiler.h`, while the guardrail target still only exposed core headers.

GREEN:

```sh
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_guardrail_contracts_test --parallel 8'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && timeout 60s ./build-wsl-gcc/bin/zr_vm_aot_c_guardrail_contracts_test'
```

Results:

- `zr_vm_aot_c_guardrail_contracts_test`: 6 tests, 0 failures.

Generated C proof:

```text
1538: static TZrBool zr_aot_typed_bool_fn_4(TZrBool zr_aot_arg0, TZrBool zr_aot_arg1);
1539: static TZrFloat64 zr_aot_typed_f64_fn_3(TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1);
1540: static TZrInt64 zr_aot_typed_i64_fn_1(TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1);
1541: static TZrUInt64 zr_aot_typed_u64_fn_2(TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1);
1762: static TZrBool zr_aot_typed_bool_fn_4(TZrBool zr_aot_arg0, TZrBool zr_aot_arg1) {
1765: static TZrFloat64 zr_aot_typed_f64_fn_3(TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1) {
1768: static TZrInt64 zr_aot_typed_i64_fn_1(TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1) {
1771: static TZrUInt64 zr_aot_typed_u64_fn_2(TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1) {
2294: zr_aot_s17 = zr_aot_typed_i64_fn_1(zr_aot_s18, zr_aot_s19);
2392: zr_aot_u18 = zr_aot_typed_u64_fn_2(zr_aot_u19, zr_aot_u20);
2490: zr_aot_f19 = zr_aot_typed_f64_fn_3(zr_aot_f20, zr_aot_f21);
2585: zr_aot_b20 = zr_aot_typed_bool_fn_4(zr_aot_b21, zr_aot_b22);
```

The generated fixture has no matches for:

```text
sync_stack_slot
SZrTypeValue *zr_aot_typed_destination
ZR_VALUE_FAST_SET(zr_aot_typed_destination
ZrLibrary_AotRuntime_CallStaticDirect(state,
ZrLibrary_AotRuntime_CallStackValue(state,
zr_aot_typed_(i64|u64|f64|bool)_fn_N(state
zr_aot_typed_(i64|u64|f64|bool)_fn_N(struct SZrState *state
```

The generated scalar typed-thunk bodies are limited to direct C returns such as:

```text
static TZrBool zr_aot_typed_bool_fn_4(TZrBool zr_aot_arg0, TZrBool zr_aot_arg1) {
    return (TZrBool)(zr_aot_arg0 && zr_aot_arg1);
}
static TZrFloat64 zr_aot_typed_f64_fn_3(TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1) {
    return (TZrFloat64)(zr_aot_arg0 + zr_aot_arg1);
}
static TZrInt64 zr_aot_typed_i64_fn_1(TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1) {
    return (TZrInt64)(zr_aot_arg0 + zr_aot_arg1);
}
static TZrUInt64 zr_aot_typed_u64_fn_2(TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1) {
    return (TZrUInt64)(zr_aot_arg0 + zr_aot_arg1);
}
```

## Acceptance Decision

Accepted as a completed 07-S5 aggregate guardrail sub-slice for current scalar typed direct-call families.

07-S5 remains partial. Full typed ABI, inline structs, `in`/`out` writeback, complete 07-S5 acceptance, performance/SZrValue counters, and stages 08-12 remain open.
