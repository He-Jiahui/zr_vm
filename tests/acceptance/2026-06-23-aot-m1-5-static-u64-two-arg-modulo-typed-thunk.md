# AOT M1.5 / 07-S5 Static u64 Two-Arg Modulo Typed Thunk

时间：2026-06-23 10:30:14 +08:00

状态：子切片完成；07-S5、M1.5/07 仍为部分完成；08-12 未开始。

## 完成项目

- `backend_aot_c_typed_u64_thunk_shapes.{h,c}` 新增
  `backend_aot_c_try_get_u64_arg0_arg1_modulo_return()`。
- 识别两 unsigned 参数、unsigned return callee 中 `MOD_UNSIGNED` 后接
  `FUNCTION_RETURN`，且 operand 顺序为 `arg0 % arg1` 的窄形态。
- `backend_aot_c_can_emit_typed_u64_two_arg_thunk()` 现在包含 modulo route。
- u64 thunk writer 发出带运行期取模除零保护的两参 typed thunk：
  `ZR_UNLIKELY(zr_aot_arg1 == 0u)` 时调用
  `ZrCore_Debug_RunError(state, "generated AOT unsigned modulo by zero")`，
  并 defensive 返回 `(TZrUInt64)0`。
- 正常路径发出：
  `return (TZrUInt64)(zr_aot_arg0 % zr_aot_arg1);`
- 直接调用侧复用已有 `zr_aot_static_u64_two_arg_direct_call` proof/writer
  与 scalar-only destination stack-sync elision。
- `tests/parser/test_aot_c_typed_direct_call_u64_shared_library_smoke.c` 新增
  `remainder(87, 45)` shared-library smoke，验证运行结果 42，且拒绝
  `CallStaticDirect`、`CallStackValue` 和不必要的两参 destination sync marker。

## RED

命令：

```sh
wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_typed_call_contracts_test zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test -j 2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_call_contracts_test; ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test'
```

结果：

- `zr_vm_aot_c_typed_call_contracts_test`：4 tests, 1 failure，缺少
  `generated AOT unsigned modulo by zero`。
- `zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test`：22 tests,
  1 failure，two-arg modulo case 在 generated C 中找不到期望的 modulo thunk/direct-call
  文本，失败为 `Expected Non-NULL`。

## GREEN

命令：

```sh
wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_typed_call_contracts_test zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test -j 2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_call_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test'
```

结果：

- typed call contracts：4/0。
- u64 shared-library smoke：22/0。

说明：实现后第一次 focused run 暴露 contract needle 应匹配 generator 源文件中的
`%%` 转义，而不是 generated C 中的 `%`；修正契约后 focused GREEN。

## Broader WSL GCC Validation

实际存在的较宽 AOT 目标构建通过。随后执行：

- 合约组：source 19/0、call contracts 4/0、typed call contracts 4/0、generic numeric
  contracts 1/0、global contracts 7/0、logical contracts 4/0、power contracts 2/0、
  frame setup 1/0、return 1/0、value SemIR 4/0、float contracts 1/0。
- 共享库烟测组：shared 8/0、call smoke 5/0、typed direct-call 5/0、i64 three-arg
  smoke 6/0、bool 19/0、u64 22/0、f64 13/0、typed arithmetic 5/0、typed bitwise
  6/0、value-type 1/0、float smoke 1/0、generic numeric smoke 1/0、global smoke
  9/0、logical smoke 4/0、power smoke 1/0。

## 文件规模

- `backend_aot_c_typed_u64_thunks.c`：852 physical / 767 non-empty lines。
- `backend_aot_c_typed_u64_thunk_shapes.c`：689 physical / 617 non-empty lines。
- `backend_aot_c_typed_u64_thunk_shapes.h`：21 physical / 18 non-empty lines。
- `tests/parser/test_aot_c_typed_call_contracts.c`：866 physical / 828 non-empty lines。
- `tests/parser/test_aot_c_typed_direct_call_u64_shared_library_smoke.c`：
  553 physical / 508 non-empty lines。

## 仍未完成

u64 两参 divide/modulo 窄形态与生成除零保护已覆盖。07-S5 完整验收、
inline structs、`in`/`out` writeback、deopt/dynamic bridges、general multi-arg
typed returns、dynamic value access helpers、以及 08-12 阶段仍未完成。
