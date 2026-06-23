# 2026-06-23 AOT M1.5 07-S5 i64 two-arg thunk shape split

## 状态

- 时间：2026-06-23 17:43:06 +08:00
- 状态：支撑子切片完成；07-S5 / M1.5 / 07 仍部分完成；08-12 未开始

## 完成项目

- 将 i64 两参 typed thunk shape recognizer 从
  `backend_aot_c_typed_i64_thunks.c` 迁入
  `backend_aot_c_typed_i64_thunk_shapes.{h,c}`。
- 覆盖的两参 shape 包括 add、subtract、multiply、divide、modulo、bitwise AND、
  bitwise OR、bitwise XOR。
- `backend_aot_c_typed_i64_thunks.c` 继续负责 can-emit gate、forward declaration
  和 thunk definition writer；divide/modulo 的 zero-denominator guard 保持在 writer。
- `tests/parser/test_aot_c_typed_call_contracts.c` 改为在 i64 shape source 中检查
  两参 shape 的参数、返回槽、opcode 与 `FUNCTION_RETURN` 契约。

## RED / GREEN

- RED：`zr_vm_aot_c_typed_call_contracts_test` 先失败，缺少 shape 文件中的
  `backend_aot_c_try_get_i64_arg0_arg1_add_return(`。
- Focused GREEN：
  - typed call contracts 4/0
  - i64 three-arg shared-library smoke 6/0
  - arithmetic shared-library smoke 7/0

## 验证

- WSL GCC debug 构建目录：`build-wsl-gcc`。
- 合约组通过：source 19/0、call 4/0、typed call 4/0、generic numeric 1/0、
  global 7/0、logical 4/0、power 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、
  float contracts 1/0。
- 共享库烟测组通过：shared 8/0、call smoke 5/0、typed direct-call 5/0、
  i64 three-arg 6/0、arithmetic 7/0、bool 26/0、u64 23/0、f64 19/0、global 9/0、
  logical 4/0、power 1/0。

## 文件规模

- `backend_aot_c_typed_i64_thunks.c`：749 physical / 674 non-empty lines
- `backend_aot_c_typed_i64_thunk_shapes.c`：338 / 289
- `backend_aot_c_typed_i64_thunk_shapes.h`：21 / 18
- `test_aot_c_typed_call_contracts.c`：908 / 887

## 未完成

This is behavior-preserving support work only. general typed-return ABI、inline structs、
`in`/`out` writeback、deopt/dynamic bridges、07-S5 完整验收和 08-12 仍未完成。
