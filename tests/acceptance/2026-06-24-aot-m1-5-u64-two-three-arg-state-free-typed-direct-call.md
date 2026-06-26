# AOT M1.5 / 07-S5 U64 Two/Three-Arg State-Free Typed Direct Call

时间：2026-06-24 08:52:05 +08:00

状态：实现子切片完成；07-S5 typed direct-call ABI 收紧部分完成；M1.5/07 仍为部分完成；08-12 未开始。

## Scope

- 在 u64 no/one-arg state-free ABI 后，继续收紧 u64 two/three-arg pure typed thunk ABI：
  - 二参 add/subtract/multiply/bitwise AND/OR/XOR 使用 `static TZrUInt64 zr_aot_typed_u64_fn_N(TZrUInt64, TZrUInt64)`。
  - 三参 add/subtract/multiply/bitwise AND/OR/XOR 使用 `static TZrUInt64 zr_aot_typed_u64_fn_N(TZrUInt64, TZrUInt64, TZrUInt64)`。
- unsigned divide/modulo 仍保留 `struct SZrState *state`，因为生成的 zero-denominator path 必须调用
  `ZrCore_Debug_RunError(state, ...)`。
- 本切片不处理 f64/bool ABI parity、full typed ABI、inline structs、`in`/`out` writeback、
  完整 07-S5 acceptance 或性能计数器。

## Implementation

- `backend_aot_c_typed_u64_thunks.c`
  - 新增 `backend_aot_c_can_emit_typed_u64_two_arg_state_free_thunk()`。
  - 二参 pure thunk declaration/definition 改为不带 `state`。
  - 二参 divide/modulo 继续使用 stateful declaration/definition。
- `backend_aot_c_typed_u64_three_arg_thunks.{h,c}`
  - 新增 `backend_aot_c_can_emit_typed_u64_three_arg_state_free_thunk()`。
  - 新增三参 state-free forward declaration writer。
  - 三参 pure thunk definition 改为不带 `state`；三参 divide/modulo 保持 stateful。
- `backend_aot_c_typed_direct_u64_calls.{h,c}`
  - two/three-arg route proof 回传 `outPassStateToThunk`。
- `backend_aot_c_typed_direct_calls.c`
  - u64 two/three-arg direct-call dispatcher 保存 pass-state 决策并传给 call writer。
- `backend_aot_c_lowering_calls.c`
  - u64 two/three-arg call writer 按 `passStateToThunk` 选择 stateful 或 state-free 调用文本。

## Tooling Evidence

RED:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_typed_call_contracts_test --parallel 8 >/tmp/zr_aot_u64_two_three_state_free_red_build.log && timeout 30s ./build-wsl-gcc/bin/zr_vm_aot_c_typed_call_contracts_test'
```

结果：typed-call contracts 1/4 failed，缺失
`backend_aot_c_can_emit_typed_u64_two_arg_state_free_thunk(const SZrFunction *function)`。

Build:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_typed_call_contracts_test zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test --parallel 8'
```

结果：目标构建成功。

Focused GREEN:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && timeout 30s ./build-wsl-gcc/bin/zr_vm_aot_c_typed_call_contracts_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && timeout 360s ./build-wsl-gcc/bin/zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_source_contracts_test --parallel 8 && timeout 30s ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test'
```

结果：

- typed-call contracts 4/0。
- u64 shared-library smoke 25/0。
- source contracts 19/0。

Generated C shape:

```text
runtime_static_u64_two_arg_project/bin/aot_c/src/main.c
static TZrUInt64 zr_aot_typed_u64_fn_1(TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1);
static TZrUInt64 zr_aot_typed_u64_fn_1(TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1) {
zr_aot_u5 = zr_aot_typed_u64_fn_1(zr_aot_u6, zr_aot_u7);
```

```text
runtime_static_u64_two_arg_divide_project/bin/aot_c/src/main.c
static TZrUInt64 zr_aot_typed_u64_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1);
static TZrUInt64 zr_aot_typed_u64_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1) {
zr_aot_u5 = zr_aot_typed_u64_fn_1(state, zr_aot_u6, zr_aot_u7);
```

```text
runtime_static_u64_three_arg_add_project/bin/aot_c/src/main.c
static TZrUInt64 zr_aot_typed_u64_fn_1(TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1, TZrUInt64 zr_aot_arg2);
static TZrUInt64 zr_aot_typed_u64_fn_1(TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1, TZrUInt64 zr_aot_arg2) {
zr_aot_u6 = zr_aot_typed_u64_fn_1(zr_aot_u7, zr_aot_u8, zr_aot_u9);
```

```text
runtime_static_u64_three_arg_divide_project/bin/aot_c/src/main.c
static TZrUInt64 zr_aot_typed_u64_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1, TZrUInt64 zr_aot_arg2);
static TZrUInt64 zr_aot_typed_u64_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1, TZrUInt64 zr_aot_arg2) {
zr_aot_u6 = zr_aot_typed_u64_fn_1(state, zr_aot_u7, zr_aot_u8, zr_aot_u9);
```

## Results

- u64 pure two-arg and three-arg typed direct calls no longer pass runtime `state`.
- u64 divide/modulo retain the stateful ABI required by generated run-error paths.
- Shared u64 smoke coverage now forbids stateful call substrings for pure two/three-arg cases while still requiring stateful divide/modulo cases.
- 规模：`backend_aot_c_typed_u64_thunks.c` 839 lines，`backend_aot_c_typed_u64_three_arg_thunks.c` 139 lines，
  `backend_aot_c_lowering_calls.c` 523 lines，`backend_aot_c_typed_direct_u64_calls.c` 195 lines，
  `backend_aot_c_typed_direct_calls.c` 479 lines，`test_aot_c_typed_call_u64_contracts.c` 296 lines，
  `test_aot_c_typed_direct_call_u64_shared_library_smoke.c` 665 lines。

## Acceptance Decision

Accepted for this narrow 07-S5 u64 two/three-arg state-free typed direct-call ABI slice.

Remaining open: f64/bool ABI parity, full typed ABI, inline structs, `in`/`out` writeback,
complete 07-S5 acceptance, performance/SZrValue counters, and stages 08-12.
