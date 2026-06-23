# AOT M1.5 / 07-S5 Static i64 Three-Arg Subtract Typed Thunk

时间：2026-06-23 08:52:55 +08:00

状态：子切片完成；07-S5、M1.5/07 仍为部分完成；08-12 未开始。

## 完成项目

- `backend_aot_c_typed_i64_thunk_shapes.{h,c}` 新增三参 i64 subtract return
  shape：`backend_aot_c_try_get_i64_arg0_arg1_arg2_subtract_return()`。
- 新增 `backend_aot_c_try_read_i64_subtract_operands()`，读取当前动态 subtract
  enum 表面上的 `SUB_SIGNED` / `SUB_SIGNED_PLAIN_DEST` operands。
- 三参 i64 shared binary helper 增加 `preserveOperandOrder` 参数。subtract 形态
  必须证明 `(arg0 - arg1) - arg2`，add/multiply/AND/OR/XOR 继续使用 commutative
  operand set。
- `backend_aot_c_can_emit_typed_i64_three_arg_thunk()` 现在覆盖 add、subtract、
  multiply、bitwise-and、bitwise-or 和 bitwise-xor。
- i64 thunk writer 对 subtract callee 发出：
  `return (TZrInt64)(zr_aot_arg0 - zr_aot_arg1 - zr_aot_arg2);`
- `tests/parser/test_aot_c_typed_direct_call_i64_three_arg_shared_library_smoke.c`
  新增 `diff3(50, 7, 1)` shared-library smoke，验证运行结果 42，且拒绝
  `CallStaticDirect`、`CallStackValue` 和不必要的三参 destination sync marker。

## RED

命令：

```sh
wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_typed_call_contracts_test zr_vm_aot_c_typed_direct_call_i64_three_arg_shared_library_smoke_test -j 2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_call_contracts_test; ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_i64_three_arg_shared_library_smoke_test'
```

结果：

- `zr_vm_aot_c_typed_call_contracts_test`：4 tests, 1 failure，缺少
  `backend_aot_c_try_get_i64_arg0_arg1_arg2_subtract_return(`。
- `zr_vm_aot_c_typed_direct_call_i64_three_arg_shared_library_smoke_test`：6 tests,
  1 failure，subtract case 在 generated C 中找不到期望的三参 subtract thunk/direct-call
  文本，失败为 `Expected Non-NULL`。
- 初次实现尝试还因 `SUB_SIGNED_LOAD_STACK` 不存在而编译失败；随后将 reader 和
  source contract 收紧到当前存在的 `SUB_SIGNED` / `SUB_SIGNED_PLAIN_DEST`。

## GREEN

命令：

```sh
wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_typed_call_contracts_test zr_vm_aot_c_typed_direct_call_i64_three_arg_shared_library_smoke_test -j 2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_call_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_i64_three_arg_shared_library_smoke_test'
```

结果：

- typed call contracts：4/0。
- i64 three-arg shared-library smoke：6/0。

## Broader WSL GCC Validation

实际存在的较宽 AOT 目标构建通过。随后执行：

- 合约组：source 19/0、call contracts 4/0、typed call contracts 4/0、generic numeric
  contracts 1/0、global contracts 7/0、logical contracts 4/0、power contracts 2/0、
  frame setup 1/0、return 1/0、value SemIR 4/0、float contracts 1/0。
- 共享库烟测组：shared 8/0、call smoke 5/0、typed direct-call 5/0、i64 three-arg
  smoke 6/0、bool 19/0、u64 14/0、f64 13/0、typed arithmetic 5/0、typed bitwise
  6/0、value-type 1/0、float smoke 1/0、generic numeric smoke 1/0、global smoke
  9/0、logical smoke 4/0、power smoke 1/0。

## 文件规模

- `backend_aot_c_typed_i64_thunks.c`：843 physical / 755 non-empty lines。
- `backend_aot_c_typed_i64_thunk_shapes.c`：221 physical / 190 non-empty lines。
- `backend_aot_c_typed_i64_thunk_shapes.h`：13 physical / 10 non-empty lines。
- `tests/parser/test_aot_c_typed_call_contracts.c`：828 physical / 790 non-empty lines。
- `tests/parser/test_aot_c_typed_direct_call_i64_three_arg_shared_library_smoke.c`：
  197 physical / 183 non-empty lines。
- `tests/parser/aot_c_typed_direct_call_i64_smoke_support.h`：197 physical / 178
  non-empty lines。

## 仍未完成

三参 i64 add/subtract/multiply/AND/OR/XOR 窄形态已覆盖。07-S5 完整验收、
inline structs、`in`/`out` writeback、deopt/dynamic bridges、general multi-arg
typed returns、dynamic value access helpers、以及 08-12 阶段仍未完成。
