# 2026-06-23 AOT M1.5 07-S5 f64 three-arg shape split

## 状态

- 时间：2026-06-23 12:10:24 +08:00
- 状态：支撑切片完成；07-S5 / M1.5 / 07 仍部分完成；08-12 未开始

## 完成项目

- 新增 `backend_aot_c_typed_f64_three_arg_shapes.{h,c}`，集中承载 f64 三参数
  typed thunk shape 识别函数。
- `backend_aot_c_typed_f64_thunk_shapes.{h,c}` 保留 f64 常量、一参数和二参数 shape，
  并通过 header 纳入三参数 shape 模块。
- `tests/parser/test_aot_c_typed_call_contracts.c` 改为分别检查基础 f64 shape source
  与三参数 f64 shape source，锁住 add/subtract/multiply/divide/modulo 五个窄三参形态。

## RED / GREEN

- RED：typed call contracts 4 tests / 1 failure，f64 测试读取
  `backend_aot_c_typed_f64_three_arg_shapes.c` 时为 `Expected Non-NULL`。
- Focused GREEN：typed call contracts 4/0；f64 typed direct-call smoke 18/0。

## 验证

- WSL GCC debug 目标构建通过；CMake 因新增 glob source 自动重新配置并编译
  `backend_aot_c_typed_f64_three_arg_shapes.c`。
- 合约组通过：source 19/0、call contracts 4/0、typed call contracts 4/0、
  generic numeric 1/0、global 7/0、logical 4/0、power 2/0、frame setup 1/0、
  return 1/0、value SemIR 4/0、float contracts 1/0。
- 共享库烟测组通过：shared 8/0、call smoke 5/0、typed direct-call 5/0、
  i64 three-arg 6/0、bool 19/0、u64 22/0、f64 18/0、typed arithmetic 5/0、
  typed bitwise 6/0、value-type 1/0、float smoke 1/0、generic numeric smoke 1/0、
  global smoke 9/0、logical smoke 4/0、power smoke 1/0。

## 文件规模

- `backend_aot_c_typed_f64_thunks.c`：267 physical / 246 non-empty lines
- `backend_aot_c_typed_f64_thunk_shapes.c`：698 / 602
- `backend_aot_c_typed_f64_thunk_shapes.h`：21 / 18
- `backend_aot_c_typed_f64_three_arg_shapes.c`：256 / 229
- `backend_aot_c_typed_f64_three_arg_shapes.h`：12 / 9
- `test_aot_c_typed_call_contracts.c`：919 / 881
- `test_aot_c_typed_direct_call_f64_shared_library_smoke.c`：452 / 415

## 未完成

This is behavior-preserving support work only. inline structs、`in`/`out` writeback、
deopt/dynamic bridges、dynamic value access helpers、general multi-arg typed-return ABI、
07-S5 完整验收和 08-12 仍未完成。
