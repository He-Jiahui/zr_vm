# AOT M1.5 / 07-S5 Static u64 Three-Arg Add Typed Thunk

时间：2026-06-23 09:13:42 +08:00

状态：子切片完成；07-S5、M1.5/07 仍为部分完成；08-12 未开始。

## 完成项目

- `backend_aot_c_typed_u64_thunk_shapes.{h,c}` 新增
  `backend_aot_c_try_get_u64_arg0_arg1_arg2_add_return()`。
- 新增三参 u64 binary-return helper 和 add operand reader，覆盖当前 `uint`
  三参 add lowering 生成的两条 add 指令加 `FUNCTION_RETURN` shape。
- `backend_aot_c_can_emit_typed_u64_three_arg_thunk()` 接入 can-emit gate。
- u64 thunk writer 对三参 add callee 发出：
  `return (TZrUInt64)(zr_aot_arg0 + zr_aot_arg1 + zr_aot_arg2);`
- `backend_aot_can_write_c_static_direct_u64_three_arg_call()` 证明 destination
  和三个 call-window argument 都是已写入的 u64 scalar local。
- `backend_aot_write_c_static_direct_u64_three_arg_function_call()` 发出
  `zr_aot_static_u64_three_arg_direct_call`，并保留 scalar-only destination
  stack-sync elision。
- `tests/parser/test_aot_c_typed_direct_call_u64_shared_library_smoke.c` 新增
  `sum3(12, 20, 10)` shared-library smoke，验证运行结果 42，且拒绝
  `CallStaticDirect`、`CallStackValue` 和不必要的三参 destination sync marker。

## RED

命令：

```sh
wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_typed_call_contracts_test zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test -j 2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_call_contracts_test; ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test'
```

结果：

- `zr_vm_aot_c_typed_call_contracts_test`：4 tests, 1 failure，缺少
  `backend_aot_c_can_emit_typed_u64_three_arg_thunk(const SZrFunction *function)`。
- `zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test`：15 tests,
  1 failure，three-arg add case 在 generated C 中找不到期望的三参 add thunk/direct-call
  文本，失败为 `Expected Non-NULL`。

## GREEN

命令：

```sh
wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_typed_call_contracts_test zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test -j 2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_call_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test'
```

结果：

- typed call contracts：4/0。
- u64 shared-library smoke：15/0。

## Broader WSL GCC Validation

实际存在的较宽 AOT 目标构建通过。随后执行：

- 合约组：source 19/0、call contracts 4/0、typed call contracts 4/0、generic numeric
  contracts 1/0、global contracts 7/0、logical contracts 4/0、power contracts 2/0、
  frame setup 1/0、return 1/0、value SemIR 4/0、float contracts 1/0。
- 共享库烟测组：shared 8/0、call smoke 5/0、typed direct-call 5/0、i64 three-arg
  smoke 6/0、bool 19/0、u64 15/0、f64 13/0、typed arithmetic 5/0、typed bitwise
  6/0、value-type 1/0、float smoke 1/0、generic numeric smoke 1/0、global smoke
  9/0、logical smoke 4/0、power smoke 1/0。

## 文件规模

- `backend_aot_c_typed_u64_thunks.c`：792 physical / 709 non-empty lines。
- `backend_aot_c_typed_u64_thunk_shapes.c`：485 physical / 439 non-empty lines。
- `backend_aot_c_typed_u64_thunk_shapes.h`：14 physical / 11 non-empty lines。
- `backend_aot_c_typed_direct_calls.c`：909 physical / 817 non-empty lines。
- `backend_aot_c_lowering_calls.c`：812 physical / 770 non-empty lines。
- `backend_aot_c_emitter.h`：766 physical / 761 non-empty lines。
- `tests/parser/test_aot_c_typed_call_contracts.c`：845 physical / 807 non-empty lines。
- `tests/parser/test_aot_c_typed_direct_call_u64_shared_library_smoke.c`：
  373 physical / 342 non-empty lines。

## 仍未完成

这是 u64 三参 add 的窄覆盖。u64 三参 subtract/multiply/bitwise、
runtime-failure-capable division/modulo、07-S5 完整验收、inline structs、
`in`/`out` writeback、deopt/dynamic bridges、general multi-arg typed returns、
dynamic value access helpers、以及 08-12 阶段仍未完成。
