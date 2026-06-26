# AOT M1.5 / 07-S5 I64 No/One-Arg State-Free Typed Direct Call

时间：2026-06-24 07:34:41 +08:00

状态：实现子切片完成；07-S5 typed direct-call ABI 收紧部分完成；M1.5/07 仍为部分完成；08-12 未开始。

## Scope

- 在 signed i64 纯二参 state-free ABI 后，继续收紧 i64 no/one-arg typed thunk ABI：
  - no-arg constant return 使用 `static TZrInt64 zr_aot_typed_i64_fn_N(void)`。
  - one-arg identity、negate、bit-not、constant bitwise、constant add/subtract/multiply 使用 `static TZrInt64 zr_aot_typed_i64_fn_N(TZrInt64 zr_aot_arg0)`。
- 目标生成物必须同时满足：
  - no/one-arg thunk forward declaration 和 definition 不再带 `struct SZrState *state`。
  - no/one-arg direct-call 调用点不再传 `state`。
  - 07:24 已完成的二参 state-free ABI 与 divide/modulo stateful ABI 保持不变。
- 本切片不处理 three-arg state-free ABI、u64/f64/bool ABI、inline structs、`in`/`out` writeback、完整 07-S5 acceptance 或性能计数器。

## Implementation

- `backend_aot_c_write_i64_no_arg_thunk_definition(...)` 生成 `void` 参数列表并删除 `(void)state`。
- `backend_aot_c_write_i64_one_arg_thunk_definition(...)` 生成单 i64 参数列表并删除 `(void)state`。
- `backend_aot_write_c_typed_i64_thunk_forward_decls(...)` 对 no/one-arg i64 thunks 生成 state-free declarations。
- `backend_aot_write_c_static_direct_i64_no_arg_function_call(...)` 与 one-arg writer 生成不带 `state` 的调用。

## Test Inventory

- Source contract: `tests/parser/test_aot_c_typed_call_i64_contracts.c`
  - 锁定 no-arg `void` signature、one-arg i64-only signature，以及对应 no-state call text。
- Smoke: `tests/parser/test_aot_c_typed_direct_call_shared_library_smoke.c`
  - no-arg constant、one-arg identity、one-arg add-const 要求 state-free ABI，并禁止 stateful call substring。
- Smoke: `tests/parser/test_aot_c_typed_direct_call_arithmetic_shared_library_smoke.c`
  - one-arg multiply-const、subtract-const、negate 要求 state-free ABI。
- Smoke: `tests/parser/test_aot_c_typed_direct_call_bitwise_shared_library_smoke.c`
  - one-arg bit-not、AND/OR/XOR const 要求 state-free ABI。

## Tooling Evidence

RED:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_typed_call_contracts_test --parallel 8 >/tmp/zr_aot_i64_no_one_state_free_red_build.log && timeout 30s ./build-wsl-gcc/bin/zr_vm_aot_c_typed_call_contracts_test'
```

结果：typed-call contracts 1/4 failed，缺失 `static TZrInt64 zr_aot_typed_i64_fn_%u(void);`。

Build:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_typed_call_contracts_test zr_vm_aot_c_typed_direct_call_arithmetic_shared_library_smoke_test zr_vm_aot_c_typed_direct_call_bitwise_shared_library_smoke_test zr_vm_aot_c_typed_direct_call_shared_library_smoke_test --parallel 8'
```

结果：目标均构建成功。

Focused GREEN:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && timeout 90s ./build-wsl-gcc/bin/zr_vm_aot_c_typed_call_contracts_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && timeout 90s ./build-wsl-gcc/bin/zr_vm_aot_c_typed_direct_call_arithmetic_shared_library_smoke_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && timeout 90s ./build-wsl-gcc/bin/zr_vm_aot_c_typed_direct_call_bitwise_shared_library_smoke_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && timeout 180s ./build-wsl-gcc/bin/zr_vm_aot_c_typed_direct_call_shared_library_smoke_test'
```

结果：

- typed-call contracts 4/0。
- arithmetic typed direct-call smoke 7/0。
- bitwise typed direct-call smoke 6/0。
- shared-library typed direct-call smoke 5/0。

Generated C shape:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && grep -n "zr_aot_typed_i64_fn_1" build-wsl-gcc/tests_generated/aot_c_typed_direct_call/runtime_static_i64_project/bin/aot_c/src/main.c | head -20'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && grep -n "zr_aot_typed_i64_fn_1" build-wsl-gcc/tests_generated/aot_c_typed_direct_call/runtime_static_i64_one_arg_project/bin/aot_c/src/main.c | head -20'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && grep -n "zr_aot_typed_i64_fn_1" build-wsl-gcc/tests_generated/aot_c_typed_direct_call_arithmetic/runtime_static_i64_divide_project/bin/aot_c/src/main.c | head -20'
```

结果：

- no-arg project emits `zr_aot_typed_i64_fn_1(void)` and calls `zr_aot_typed_i64_fn_1()`.
- one-arg project emits `zr_aot_typed_i64_fn_1(TZrInt64 zr_aot_arg0)` and calls `zr_aot_typed_i64_fn_1(zr_aot_s5)`.
- divide project keeps the stateful two-arg ABI and calls `zr_aot_typed_i64_fn_1(state, zr_aot_s6, zr_aot_s7)`.

Format check:

```text
git diff --check -- <touched production, test, docs, and acceptance files>
```

结果：exit 0；仅报告 LF/CRLF conversion warnings。

## Results

- i64 no/one-arg typed direct-call thunks no longer receive runtime `state`.
- i64 pure two-arg state-free ABI from the previous slice remains intact.
- i64 divide/modulo stateful two-arg ABI remains intact for runtime error reporting.
- 规模：`backend_aot_c_typed_i64_thunks.c` 310 lines，`backend_aot_c_lowering_calls.c` 491 lines，`test_aot_c_typed_call_i64_contracts.c` 314 lines，通用 direct-call smoke 870 lines。

## Acceptance Decision

Accepted for this narrow 07-S5 i64 no/one-arg state-free typed direct-call ABI slice.

Remaining open: i64 three-arg state-free ABI decision, u64/f64/bool ABI parity, full typed ABI, inline structs, `in`/`out` writeback, complete 07-S5 acceptance, performance/SZrValue counters, and stages 08-12.
