# AOT M1.5 / 07-S5 I64 Three-Arg State-Free Typed Direct Call

时间：2026-06-24 07:54:44 +08:00

状态：实现子切片完成；07-S5 typed direct-call ABI 收紧部分完成；M1.5/07 仍为部分完成；08-12 未开始。

## Scope

- 在 signed i64 no/one/two-arg state-free ABI 后，继续收紧 i64 纯三参 typed thunk ABI：
  - `return arg0 + arg1 + arg2`
  - `return arg0 - arg1 - arg2`
  - `return arg0 * arg1 * arg2`
  - `return arg0 & arg1 & arg2`
  - `return arg0 | arg1 | arg2`
  - `return arg0 ^ arg1 ^ arg2`
- 目标生成物必须同时满足：
  - 纯三参 thunk forward declaration 和 definition 不再带 `struct SZrState *state`。
  - 纯三参 direct-call 调用点不再传 `state`，只传三个 i64 scalar locals。
  - 三参除法/取模仍保留 `state`，用于 `ZrCore_Debug_RunError(state, ...)`。
  - 既有 typed-destination stack sync elision、`CallStaticDirect` / `CallStackValue` 禁止项保持不变。
- 本切片不处理 u64/f64/bool ABI parity、full typed ABI、inline structs、`in`/`out` writeback、完整 07-S5 acceptance 或性能计数器。

## Implementation

- 新增 `backend_aot_c_can_emit_typed_i64_three_arg_state_free_thunk(...)`，只覆盖无运行时错误路径的 i64 三参形态。
- `backend_aot_write_c_typed_i64_thunk_forward_decls(...)` 和纯三参 thunk writer 对 state-free 形态生成：
  - `static TZrInt64 zr_aot_typed_i64_fn_N(TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1, TZrInt64 zr_aot_arg2);`
  - `static TZrInt64 zr_aot_typed_i64_fn_N(TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1, TZrInt64 zr_aot_arg2) { ... }`
- `backend_aot_can_write_c_static_direct_i64_three_arg_call(...)` 回传 `outPassStateToThunk`。
- `backend_aot_write_c_static_direct_i64_three_arg_function_call(...)` 按该标志生成带 `state` 或不带 `state` 的调用。

## Test Inventory

- Source contract: `tests/parser/test_aot_c_typed_call_i64_contracts.c`
  - 锁定 three-arg state-free predicate、stateful fallback call text、state-free call text、route proof out parameter。
- Focused smoke: `tests/parser/test_aot_c_typed_direct_call_i64_three_arg_shared_library_smoke.c`
  - 加法、减法、乘法、bitwise AND/OR/XOR 要求 state-free thunk/call。
  - 除法/取模继续要求 stateful thunk/call。

## Tooling Evidence

RED:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_typed_call_contracts_test --parallel 8 >/tmp/zr_aot_i64_three_state_free_red_build.log && timeout 30s ./build-wsl-gcc/bin/zr_vm_aot_c_typed_call_contracts_test'
```

结果：typed-call contracts 1/4 failed，缺失 `backend_aot_c_can_emit_typed_i64_three_arg_state_free_thunk(const SZrFunction *function)`。

Build:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_typed_call_contracts_test --parallel 8'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_typed_direct_call_i64_three_arg_shared_library_smoke_test --parallel 8'
```

结果：目标均构建成功。一次合同 build+run 合并验证在构建阶段超时，最终证据采用拆分后的构建与测试结果。

Focused GREEN:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && timeout 30s ./build-wsl-gcc/bin/zr_vm_aot_c_typed_call_contracts_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && timeout 240s ./build-wsl-gcc/bin/zr_vm_aot_c_typed_direct_call_i64_three_arg_shared_library_smoke_test'
```

结果：

- typed-call contracts 4/0。
- i64 three-arg shared-library smoke 8/0。

Generated C shape:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && grep -n "zr_aot_typed_i64_fn_1" build-wsl-gcc/tests_generated/aot_c_typed_direct_call_i64/runtime_static_i64_three_arg_project/bin/aot_c/src/main.c | head -n 8'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && grep -n "zr_aot_typed_i64_fn_1" build-wsl-gcc/tests_generated/aot_c_typed_direct_call_i64/runtime_static_i64_three_arg_divide_project/bin/aot_c/src/main.c | head -n 8'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && grep -n "zr_aot_typed_i64_fn_1" build-wsl-gcc/tests_generated/aot_c_typed_direct_call_i64/runtime_static_i64_three_arg_bitwise_or_project/bin/aot_c/src/main.c | head -n 8'
```

结果：

- Add project emits `zr_aot_typed_i64_fn_1(TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1, TZrInt64 zr_aot_arg2)` and calls `zr_aot_typed_i64_fn_1(zr_aot_s7, zr_aot_s8, zr_aot_s9)`.
- Divide project keeps `zr_aot_typed_i64_fn_1(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1, TZrInt64 zr_aot_arg2)` and calls `zr_aot_typed_i64_fn_1(state, zr_aot_s7, zr_aot_s8, zr_aot_s9)`.
- Bitwise OR project emits and calls the state-free three-i64 ABI.

Format check:

```text
git diff --check -- <touched production, test, docs, and acceptance files>
```

结果：exit 0；仅报告 LF/CRLF conversion warnings。

## Results

- Pure signed i64 three-arg typed direct-call thunks no longer receive runtime `state`.
- Runtime-error-capable signed three-arg divide/modulo thunks still receive `state`.
- Existing scalar-local route proof and stack-sync elision remain intact.
- 规模：`backend_aot_c_typed_i64_thunks.c` 317 lines，`backend_aot_c_lowering_calls.c` 502 lines，`backend_aot_c_typed_direct_i64_calls.c` 158 lines，`backend_aot_c_typed_direct_calls.c` 473 lines，`test_aot_c_typed_call_i64_contracts.c` 325 lines，`test_aot_c_typed_direct_call_i64_three_arg_shared_library_smoke.c` 267 lines。

## Acceptance Decision

Accepted for this narrow 07-S5 i64 three-arg state-free typed direct-call ABI slice.

Remaining open: u64/f64/bool ABI parity, full typed ABI, inline structs, `in`/`out` writeback, complete 07-S5 acceptance, performance/SZrValue counters, and stages 08-12.
