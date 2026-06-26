# AOT M1.5 / 07-S5 I64 Two-Arg State-Free Typed Direct Call

时间：2026-06-24 07:24:50 +08:00

状态：实现子切片完成；07-S5 typed direct-call ABI 收紧部分完成；M1.5/07 仍为部分完成；08-12 未开始。

## Scope

- 收紧 signed i64 纯二参 typed thunk ABI：
  - `return arg0 + arg1`
  - `return arg0 - arg1`
  - `return arg0 * arg1`
  - `return arg0 & arg1`
  - `return arg0 | arg1`
  - `return arg0 ^ arg1`
- 目标生成物必须同时满足：
  - 纯二参 thunk forward declaration 和 definition 不再带 `struct SZrState *state`。
  - 纯二参 direct-call 调用点不再传 `state`，只传两个 i64 scalar locals。
  - 除法/取模仍保留 `state`，用于 `ZrCore_Debug_RunError(state, ...)`。
  - 既有 typed-destination stack sync elision、`CallStaticDirect` / `CallStackValue` 禁止项保持不变。
- 本切片不处理 no/one/three-arg state-free ABI、u64/f64/bool ABI、inline structs、`in`/`out` writeback、完整 07-S5 acceptance 或性能计数器。

## Baseline

- 07-S5 已有 typed direct-call 路由和 i64 no/one/two/three-arg thunk recognizer。
- 但所有 i64 typed thunks 统一带 `struct SZrState *state`，即使纯 add/sub/multiply/bitwise 二参 thunk 没有 runtime/error path。
- 除法/取模二参 thunk 确实需要 `state` 来报告 divide/modulo-by-zero，因此不能全局移除。

## Implementation

- 新增 `backend_aot_c_can_emit_typed_i64_two_arg_state_free_thunk(...)`，只覆盖无运行时错误路径的 i64 二参形态。
- `backend_aot_write_c_typed_i64_thunk_forward_decls(...)` 和纯二参 thunk writer 对 state-free 形态生成：
  - `static TZrInt64 zr_aot_typed_i64_fn_N(TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1);`
  - `static TZrInt64 zr_aot_typed_i64_fn_N(TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1) { ... }`
- `backend_aot_can_write_c_static_direct_i64_two_arg_call(...)` 回传 `outPassStateToThunk`。
- `backend_aot_write_c_static_direct_i64_two_arg_function_call(...)` 按该标志生成带 `state` 或不带 `state` 的调用。

## Test Inventory

- Focused smoke: `tests/parser/test_aot_c_typed_direct_call_arithmetic_shared_library_smoke.c`
  - 乘法和按位与要求 state-free thunk/call。
  - 除法/取模继续要求 stateful thunk/call。
- Focused smoke: `tests/parser/test_aot_c_typed_direct_call_bitwise_shared_library_smoke.c`
  - OR/XOR 二参 bitwise 要求 state-free thunk/call。
- Regression smoke: `tests/parser/test_aot_c_typed_direct_call_shared_library_smoke.c`
  - i64 二参加法/减法要求 state-free thunk/call，并禁止 stateful call substring。
- Source contract: `tests/parser/test_aot_c_typed_call_i64_contracts.c`
  - 锁定 state-free predicate、stateful fallback call text、state-free call text、route proof out parameter。

## Tooling Evidence

Build:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_typed_direct_call_arithmetic_shared_library_smoke_test --parallel 8'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_typed_direct_call_bitwise_shared_library_smoke_test zr_vm_aot_c_typed_direct_call_shared_library_smoke_test zr_vm_aot_c_typed_call_contracts_test --parallel 8'
```

结果：目标均构建成功。

Focused GREEN:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && timeout 90s ./build-wsl-gcc/bin/zr_vm_aot_c_typed_direct_call_arithmetic_shared_library_smoke_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && timeout 90s ./build-wsl-gcc/bin/zr_vm_aot_c_typed_direct_call_bitwise_shared_library_smoke_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && timeout 90s ./build-wsl-gcc/bin/zr_vm_aot_c_typed_call_contracts_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && timeout 180s ./build-wsl-gcc/bin/zr_vm_aot_c_typed_direct_call_shared_library_smoke_test'
```

结果：

- arithmetic typed direct-call smoke 7/0。
- bitwise typed direct-call smoke 6/0。
- typed-call contracts 4/0。
- shared-library typed direct-call smoke 5/0。

Generated C shape:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && grep -n "zr_aot_typed_i64_fn_1" build-wsl-gcc/tests_generated/aot_c_typed_direct_call_arithmetic/runtime_static_i64_project/bin/aot_c/src/main.c | head -20'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && grep -n "zr_aot_typed_i64_fn_1" build-wsl-gcc/tests_generated/aot_c_typed_direct_call_arithmetic/runtime_static_i64_divide_project/bin/aot_c/src/main.c | head -20'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && grep -n "zr_aot_typed_i64_fn_1" build-wsl-gcc/tests_generated/aot_c_typed_direct_call_bitwise/runtime_static_i64_bitwise_or_project/bin/aot_c/src/main.c | head -20'
```

结果：

- Multiply project emits `zr_aot_typed_i64_fn_1(TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1)` and calls `zr_aot_typed_i64_fn_1(zr_aot_s6, zr_aot_s7)`.
- Divide project keeps `zr_aot_typed_i64_fn_1(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1)` and calls `zr_aot_typed_i64_fn_1(state, zr_aot_s6, zr_aot_s7)`.
- Bitwise OR project emits and calls the state-free two-i64 ABI.

Format check:

```text
git diff --check -- <touched production and test files>
```

结果：exit 0；仅报告 LF/CRLF conversion warnings。

## Results

- Pure signed i64 two-arg typed direct-call thunks no longer receive runtime `state`.
- Runtime-error-capable signed divide/modulo thunks still receive `state`.
- Existing scalar-local route proof and stack-sync elision remain intact.
- 规模：`backend_aot_c_typed_i64_thunks.c` 312 lines，`backend_aot_c_lowering_calls.c` 491 lines，`backend_aot_c_typed_direct_i64_calls.c` 155 lines，`backend_aot_c_typed_direct_calls.c` 470 lines，`test_aot_c_typed_call_i64_contracts.c` 314 lines，通用 direct-call smoke 867 lines。

## Acceptance Decision

Accepted for this narrow 07-S5 i64 two-arg state-free typed direct-call ABI slice.

Remaining open: no/one/three-arg state-free ABI decisions, u64/f64/bool ABI parity, full typed ABI, inline structs, `in`/`out` writeback, complete 07-S5 acceptance, performance/SZrValue counters, and stages 08-12.
