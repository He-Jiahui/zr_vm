# 2026-06-23 AOT M1.5 07-S5 static f64 three-arg divide typed thunk

## 状态

- 时间：2026-06-23 11:38:58 +08:00
- 状态：子切片完成；07-S5 / M1.5 / 07 仍部分完成；08-12 未开始

## 完成项目

- `backend_aot_c_typed_f64_thunk_shapes.{h,c}` 新增三参数 float 除法识别：
  `backend_aot_c_try_get_f64_arg0_arg1_arg2_divide_return()` 只接受三个 f64
  参数、两条有序 `DIV_FLOAT` 后接 `FUNCTION_RETURN` 的窄形态。
- shape 识别保持非交换运算顺序：第一步必须为 `arg0 / arg1`，第二步必须为
  first-result `/ arg2`。
- `backend_aot_c_can_emit_typed_f64_three_arg_thunk()` 纳入三参 divide shape。
- `backend_aot_c_typed_f64_thunks.c` 新增三参 divide thunk writer，在 `arg1` 或
  `arg2` 为 0.0 时调用 `ZrCore_Debug_RunError()` 并 defensive 返回 0.0；正常路径发出
  `return (TZrFloat64)(zr_aot_arg0 / zr_aot_arg1 / zr_aot_arg2);`。
- 已有 `zr_aot_static_f64_three_arg_direct_call` route proof 和 lowering writer 复用
  三个 f64 scalar-local 参数与 `nativeDouble` destination sync guard。
- `tests/parser/test_aot_c_typed_call_contracts.c` 和
  `tests/parser/test_aot_c_typed_direct_call_f64_shared_library_smoke.c` 覆盖契约与共享库执行。

## RED / GREEN

- RED：typed call contracts 4 tests / 1 failure，缺
  `backend_aot_c_write_f64_three_arg_divide_thunk_definition(`。
- RED：f64 shared-library smoke 17 tests / 1 failure，新增三参 divide 用例为
  `Expected Non-NULL`。
- Focused GREEN：typed call contracts 4/0；f64 typed direct-call smoke 17/0。

## 验证

- WSL GCC debug 目标构建通过。
- 合约组通过：source 19/0、call contracts 4/0、typed call contracts 4/0、generic numeric 1/0、
  global 7/0、logical 4/0、power 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、
  float contracts 1/0。
- 共享库烟测组通过：shared 8/0、call smoke 5/0、typed direct-call 5/0、i64 three-arg 6/0、
  bool 19/0、u64 22/0、f64 17/0、typed arithmetic 5/0、typed bitwise 6/0、value-type 1/0、
  float smoke 1/0、generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、
  power smoke 1/0。

## 文件规模

- `backend_aot_c_typed_f64_thunks.c`：252 physical / 232 non-empty lines
- `backend_aot_c_typed_f64_thunk_shapes.c`：894 / 778
- `backend_aot_c_typed_f64_thunk_shapes.h`：24 / 21
- `test_aot_c_typed_call_contracts.c`：903 / 865
- `test_aot_c_typed_direct_call_f64_shared_library_smoke.c`：426 / 391

## 未完成

This is narrow f64 three-arg divide coverage only. inline structs、`in`/`out`
writeback、deopt/dynamic bridges、dynamic value access helpers、general multi-arg
typed-return ABI、07-S5 完整验收和 08-12 仍未完成。
