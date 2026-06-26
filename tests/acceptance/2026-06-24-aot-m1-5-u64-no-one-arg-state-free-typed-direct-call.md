# AOT M1.5 / 07-S5 U64 No/One-Arg State-Free Typed Direct Call

时间：2026-06-24 08:26:14 +08:00

状态：实现子切片完成；07-S5 typed direct-call ABI 收紧部分完成；M1.5/07 仍为部分完成；08-12 未开始。

## Scope

- 在 signed i64 no/one/two/three-arg state-free ABI 后，继续收紧 u64 no/one-arg typed thunk ABI：
  - no-arg unsigned constant/boundary return 使用 `static TZrUInt64 zr_aot_typed_u64_fn_N(void)`。
  - one-arg unsigned identity、constant add/subtract/multiply、constant bitwise AND/OR/XOR 使用 `static TZrUInt64 zr_aot_typed_u64_fn_N(TZrUInt64 zr_aot_arg0)`。
- 目标生成物必须同时满足：
  - u64 no/one-arg thunk forward declaration 和 definition 不再带 `struct SZrState *state`。
  - u64 no/one-arg direct-call 调用点不再传 `state`。
  - u64 two/three-arg ABI 不在本切片收紧；现有 two/three-arg smoke 仍保持可编译和可执行。
  - typed signed-const fallback 和 forced stack-copy 的 scalar-local 同步不能破坏已有 u64 two-arg direct-call 参数传递。
- 本切片不处理 u64 two/three-arg state-free ABI、f64/bool ABI parity、full typed ABI、inline structs、`in`/`out` writeback、完整 07-S5 acceptance 或性能计数器。

## Baseline

- RED baseline：u64 typed-call contracts 先按新 no-arg `void` signature 失败，证明旧生成物仍要求 `struct SZrState *state`。
- 调试中发现两个下层问题：
  - typed signed-const fallback 在直接 u64 调用后的 signed-const 链上从 frame slot 读取旧值，并且没有把 fallback 结果写回 i64 scalar local。
  - forced stack-copy 在调用参数槽可声明多种 scalar local 时优先走 i64 source local，导致 u64 two-arg direct-call 的第一个参数从未初始化的 i64 local 进入 thunk。
- 这些问题会影响本切片的 u64 smoke acceptance，因此作为 07-S5 下层支持修复纳入本记录。

## Implementation

- `backend_aot_c_typed_u64_thunks.c`
  - u64 no-arg forward declaration/definition 改为 `static TZrUInt64 zr_aot_typed_u64_fn_N(void)`。
  - u64 one-arg forward declaration/definition 改为 `static TZrUInt64 zr_aot_typed_u64_fn_N(TZrUInt64 zr_aot_arg0)`。
  - 删除 no/one-arg thunk body 内的 `(void)state`。
- `backend_aot_c_lowering_calls.c`
  - u64 no-arg direct-call writer 生成 `zr_aot_typed_u64_fn_N()`。
  - u64 one-arg direct-call writer 生成 `zr_aot_typed_u64_fn_N(zr_aot_uA)`。
- `backend_aot_c_lowering_typed_arithmetic.c`
  - signed-const fallback 接收 `functionIr` 和 `execInstructionIndex`，当左操作数 i64 scalar local 已写入时读取 `zr_aot_sN`。
  - fallback 计算 `zr_aot_result` 后，在目标有 i64 scalar local 时同步写回 `zr_aot_sN`，同时保留 frame-slot fallback 写入。
- `backend_aot_c_scalar_stack_copy.c`
  - forced value-slot write 场景下优先选择可用 source-local 类型，顺序为 bool、u64、i64、f64。
  - u64 调用参数槽现在保留源槽 u64 local，避免复用 i64 local 污染 u64 direct-call 参数。

## Test Inventory

- Source contracts:
  - `tests/parser/test_aot_c_typed_call_u64_contracts.c`
  - `tests/parser/test_aot_c_source_contracts.c`
- Focused shared-library smoke:
  - `tests/parser/test_aot_c_typed_direct_call_u64_shared_library_smoke.c`
- Boundary cases:
  - u64 no-arg constant and boundary return.
  - u64 one-arg identity, add/subtract/multiply const, bitwise AND/OR/XOR const.
  - existing u64 two-arg shared-library routes after scalar stack-copy source-type repair.
- Negative/forbidden generated-C checks:
  - u64 no/one-arg stateful call substrings are forbidden in the focused smoke.
  - source contracts lock scalar-local fallback helper names and signed-const fallback source/destination sync text.

## Tooling Evidence

Tooling:

- WSL GCC: `gcc (Ubuntu 11.4.0-1ubuntu1~22.04.3) 11.4.0`
- CMake: `cmake version 3.22.1`
- Build directory: `build-wsl-gcc`

RED:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_typed_call_contracts_test --parallel 8 >/tmp/zr_aot_u64_no_one_state_free_red_build.log && timeout 30s ./build-wsl-gcc/bin/zr_vm_aot_c_typed_call_contracts_test'
```

结果：typed-call contracts 1/4 failed，缺失 `static TZrUInt64 zr_aot_typed_u64_fn_%u(void);`。

Build:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test zr_vm_aot_c_typed_call_contracts_test --parallel 8'
```

结果：目标均构建成功，并重新编译包含 `backend_aot_c_scalar_stack_copy.c.o` 的 parser shared 路径。

Focused GREEN:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && timeout 30s ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && timeout 30s ./build-wsl-gcc/bin/zr_vm_aot_c_typed_call_contracts_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && timeout 360s ./build-wsl-gcc/bin/zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test'
```

结果：

- source contracts 19/0。
- typed-call contracts 4/0。
- u64 shared-library smoke 25/0。

Generated C shape:

```text
build-wsl-gcc/tests_generated/aot_c_typed_direct_call_u64/runtime_static_u64_no_arg_project/bin/aot_c/src/main.c
354: static TZrUInt64 zr_aot_typed_u64_fn_1(void);
419: static TZrUInt64 zr_aot_typed_u64_fn_1(void) {
560: /* zr_aot_static_u64_no_arg_direct_call */
561: zr_aot_u3 = zr_aot_typed_u64_fn_1();
```

```text
build-wsl-gcc/tests_generated/aot_c_typed_direct_call_u64/runtime_static_u64_one_arg_project/bin/aot_c/src/main.c
402: static TZrUInt64 zr_aot_typed_u64_fn_1(TZrUInt64 zr_aot_arg0);
476: static TZrUInt64 zr_aot_typed_u64_fn_1(TZrUInt64 zr_aot_arg0) {
673: /* zr_aot_static_u64_one_arg_direct_call */
674: zr_aot_u4 = zr_aot_typed_u64_fn_1(zr_aot_u5);
```

```text
build-wsl-gcc/tests_generated/aot_c_typed_direct_call_u64/runtime_static_u64_two_arg_project/bin/aot_c/src/main.c
774: /* zr_aot_scalar_stack_copy_u64 dstSlot=6 srcSlot=7 */
791: zr_aot_u6 = zr_aot_u_value;
834: /* zr_aot_static_u64_two_arg_direct_call */
835: zr_aot_u5 = zr_aot_typed_u64_fn_1(state, zr_aot_u6, zr_aot_u7);
```

Format check:

```text
git diff --check -- <touched production, test, docs, session, and acceptance files>
```

结果：exit 0；仅报告 LF/CRLF conversion warnings。

## Results

- u64 no-arg typed direct-call thunks no longer receive runtime `state` and call sites use an empty argument list.
- u64 one-arg typed direct-call thunks no longer receive runtime `state` and call sites pass only the u64 scalar local.
- signed-const fallback now preserves scalar-local read/write correctness when the expression chain crosses typed direct-call results.
- forced stack-copy now preserves u64 source-local arguments for existing u64 two-arg direct calls.
- 规模：`backend_aot_c_typed_u64_thunks.c` 830 lines，`backend_aot_c_lowering_calls.c` 502 lines，`backend_aot_c_lowering_typed_arithmetic.c` 1168 lines，`backend_aot_c_scalar_stack_copy.c` 645 lines，`test_aot_c_typed_call_u64_contracts.c` 269 lines，`test_aot_c_typed_direct_call_u64_shared_library_smoke.c` 645 lines，`test_aot_c_source_contracts.c` 2156 lines。

## Acceptance Decision

Accepted for this narrow 07-S5 u64 no/one-arg state-free typed direct-call ABI slice.

Remaining open: u64 two/three-arg state-free ABI, f64/bool ABI parity, full typed ABI, inline structs, `in`/`out` writeback, complete 07-S5 acceptance, performance/SZrValue counters, and stages 08-12.
