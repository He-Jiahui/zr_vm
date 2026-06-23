# AOT M1.5 / 07-S5 Static i64 Three-Arg Multiply Typed Thunk

时间：2026-06-23 08:13:44 +08:00

状态：子切片完成；07-S5、M1.5/07 仍为部分完成；08-12 未开始。

## 完成项目

- `backend_aot_c_typed_i64_thunk_shapes.{h,c}` 新增三参 i64 multiply-return shape：
  `backend_aot_c_try_get_i64_arg0_arg1_arg2_multiply_return()`。
- 三参 i64 add/multiply 共享私有
  `backend_aot_c_try_get_i64_arg0_arg1_arg2_binary_return()`，统一校验三参数 i64
  metadata、callable return、两条二元指令和 `FUNCTION_RETURN` result。
- `backend_aot_c_can_emit_typed_i64_three_arg_thunk()` 现在覆盖 add/multiply 两种三参
  i64 shape。
- i64 thunk writer 对 multiply callee 发出：
  `return (TZrInt64)(zr_aot_arg0 * zr_aot_arg1 * zr_aot_arg2);`
- `tests/parser/test_aot_c_typed_direct_call_i64_three_arg_shared_library_smoke.c`
  新增 `product3(2, 3, 7)` shared-library smoke，验证运行结果 42，且拒绝
  `CallStaticDirect`、`CallStackValue` 和不必要的三参 destination sync marker。

## RED

命令：

```sh
wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_typed_call_contracts_test zr_vm_aot_c_typed_direct_call_i64_three_arg_shared_library_smoke_test -j 2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_call_contracts_test; ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_i64_three_arg_shared_library_smoke_test'
```

结果：

- `zr_vm_aot_c_typed_call_contracts_test`：4 tests, 1 failure，缺少
  `backend_aot_c_try_get_i64_arg0_arg1_arg2_multiply_return(`。
- `zr_vm_aot_c_typed_direct_call_i64_three_arg_shared_library_smoke_test`：2 tests,
  1 failure，multiply case 在 generated C 中找不到期望的三参乘法 thunk/direct-call 文本，
  失败为 `Expected Non-NULL`。

## GREEN

命令：

```sh
wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_typed_call_contracts_test zr_vm_aot_c_typed_direct_call_i64_three_arg_shared_library_smoke_test -j 2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_call_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_i64_three_arg_shared_library_smoke_test'
```

结果：

- typed call contracts：4/0。
- i64 three-arg shared-library smoke：2/0。

## Broader WSL GCC Validation

实际存在的较宽 AOT 目标构建通过。随后执行：

- 合约组：source 19/0、call contracts 4/0、typed call contracts 4/0、generic numeric
  contracts 1/0、global contracts 7/0、logical contracts 4/0、power contracts 2/0、
  frame setup 1/0、return 1/0、value SemIR 4/0、float contracts 1/0。
- 共享库烟测组：shared 8/0、call smoke 5/0、typed direct-call 5/0、i64 three-arg
  smoke 2/0、bool 19/0、u64 14/0、f64 13/0、typed arithmetic 5/0、typed bitwise
  6/0、value-type 1/0、float smoke 1/0、generic numeric smoke 1/0、global smoke
  9/0、logical smoke 4/0、power smoke 1/0。

说明：最初按旧清单尝试 broader build 时，Ninja 拒绝了两个当前 CMake 中已不存在的
目标名；这些命令没有执行测试。最终 broader 使用构建系统中实际存在的目标清单并通过。

## 文件规模

- `backend_aot_c_typed_i64_thunks.c`：823 physical / 735 non-empty lines。
- `backend_aot_c_typed_i64_thunk_shapes.c`：117 physical / 102 non-empty lines。
- `backend_aot_c_typed_i64_thunk_shapes.h`：9 physical / 6 non-empty lines。
- `tests/parser/test_aot_c_typed_call_contracts.c`：807 physical / 769 non-empty lines。
- `tests/parser/test_aot_c_typed_direct_call_i64_three_arg_shared_library_smoke.c`：
  73 physical / 67 non-empty lines。
- `tests/parser/aot_c_typed_direct_call_i64_smoke_support.h`：197 physical / 178
  non-empty lines。

## 仍未完成

这是三参 i64 乘法窄覆盖。07-S5 完整验收、inline structs、`in`/`out` writeback、
deopt/dynamic bridges、general multi-arg typed returns、dynamic value access helpers、
以及 08-12 阶段仍未完成。
