# AOT M1.5 / 07-S5 Static u64 Three-Arg Bitwise-Xor Typed Thunk

时间：2026-06-23 10:04:13 +08:00

状态：子切片完成；07-S5、M1.5/07 仍为部分完成；08-12 未开始。

## 完成项目

- `backend_aot_c_typed_u64_thunk_shapes.{h,c}` 新增
  `backend_aot_c_try_get_u64_arg0_arg1_arg2_bitwise_xor_return()`。
- 新增 `backend_aot_c_try_read_u64_bitwise_xor_operands()`，识别两条
  `BITWISE_XOR` 指令后接 `FUNCTION_RETURN` 的三参数 unsigned return 形态。
- `backend_aot_c_can_emit_typed_u64_three_arg_thunk()` 现在覆盖 add、multiply、
  subtract、bitwise-and、bitwise-or、bitwise-xor。
- u64 thunk writer 对 bitwise-xor callee 发出：
  `return (TZrUInt64)(zr_aot_arg0 ^ zr_aot_arg1 ^ zr_aot_arg2);`
- 直接调用侧复用已有 `zr_aot_static_u64_three_arg_direct_call` proof/writer
  与 scalar-only destination stack-sync elision。
- `tests/parser/test_aot_c_typed_direct_call_u64_shared_library_smoke.c` 新增
  `toggle3(63, 21, 0)` shared-library smoke，验证运行结果 42，且拒绝
  `CallStaticDirect`、`CallStackValue` 和不必要的三参 destination sync marker。

## RED

命令：

```sh
wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_typed_call_contracts_test zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test -j 2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_call_contracts_test; ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test'
```

结果：

- `zr_vm_aot_c_typed_call_contracts_test`：4 tests, 1 failure，缺少
  `return (TZrUInt64)(zr_aot_arg0 ^ zr_aot_arg1 ^ zr_aot_arg2);`。
- `zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test`：20 tests,
  1 failure，three-arg bitwise-xor case 在 generated C 中找不到期望的三参
  bitwise-xor thunk/direct-call 文本，失败为 `Expected Non-NULL`。

## GREEN

命令：

```sh
wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_typed_call_contracts_test zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test -j 2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_call_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test'
```

结果：

- typed call contracts：4/0。
- u64 shared-library smoke：20/0。

## Broader WSL GCC Validation

实际存在的较宽 AOT 目标构建通过。随后执行：

- 合约组：source 19/0、call contracts 4/0、typed call contracts 4/0、generic numeric
  contracts 1/0、global contracts 7/0、logical contracts 4/0、power contracts 2/0、
  frame setup 1/0、return 1/0、value SemIR 4/0、float contracts 1/0。
- 共享库烟测组：shared 8/0、call smoke 5/0、typed direct-call 5/0、i64 three-arg
  smoke 6/0、bool 19/0、u64 20/0、f64 13/0、typed arithmetic 5/0、typed bitwise
  6/0、value-type 1/0、float smoke 1/0、generic numeric smoke 1/0、global smoke
  9/0、logical smoke 4/0、power smoke 1/0。

## 文件规模

- `backend_aot_c_typed_u64_thunks.c`：822 physical / 739 non-empty lines。
- `backend_aot_c_typed_u64_thunk_shapes.c`：617 physical / 553 non-empty lines。
- `backend_aot_c_typed_u64_thunk_shapes.h`：19 physical / 16 non-empty lines。
- `tests/parser/test_aot_c_typed_call_contracts.c`：857 physical / 819 non-empty lines。
- `tests/parser/test_aot_c_typed_direct_call_u64_shared_library_smoke.c`：
  503 physical / 462 non-empty lines。

## 仍未完成

三参 u64 add/multiply/subtract/bitwise-and/bitwise-or/bitwise-xor 窄形态已覆盖。
runtime-failure-capable division/modulo、07-S5 完整验收、inline structs、
`in`/`out` writeback、deopt/dynamic bridges、general multi-arg typed returns、
dynamic value access helpers、以及 08-12 阶段仍未完成。
