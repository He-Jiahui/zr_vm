# 2026-06-23 AOT M1.5 07-S5 static f64 two-arg less bool typed thunk

## 状态

- 时间：2026-06-23 12:45:38 +08:00
- 状态：子切片完成；07-S5 / M1.5 / 07 仍部分完成；08-12 未开始

## 完成项目

- `backend_aot_c_typed_bool_thunks.c` 新增 f64 参数 bool-return recognizer，覆盖
  `float < float -> bool` 的 `LOGICAL_LESS_FLOAT` + `FUNCTION_RETURN` 窄形态。
- `backend_aot_write_c_typed_bool_thunks()` 现在可发出
  `static TZrBool zr_aot_typed_bool_fn_N(struct SZrState *state, TZrFloat64 arg0, TZrFloat64 arg1)`
  并直接返回 `(TZrBool)(arg0 < arg1)`。
- `backend_aot_write_c_static_direct_f64_bool_two_arg_function_call()` 生成
  `zr_aot_static_f64_bool_two_arg_direct_call`，直接以 `zr_aot_f*` 参数调用 bool thunk，
  只在 scalar-local proof 需要时同步 bool 栈槽。
- 新增 `backend_aot_c_typed_direct_f64_calls.{h,c}`，把 f64 direct-call route proof
  从 `backend_aot_c_typed_direct_calls.c` 拆出；主调度文件降至 868 physical /
  785 non-empty lines。
- `tests/parser/test_aot_c_typed_call_contracts.c` 锁住 bool thunk、f64->bool route proof、
  lowering writer，以及 f64 direct-call route ownership 拆分。
- `tests/parser/test_aot_c_typed_direct_call_bool_shared_library_smoke.c` 新增
  `smaller(left: float, right: float): bool { return left < right; }` shared-library
  smoke，运行结果为 42，并检查 typed thunk 与 f64 bool direct-call markers。

## RED / GREEN

- RED：typed call contracts 4 tests / 1 failure，缺少
  `backend_aot_c_can_emit_typed_bool_f64_two_arg_thunk(...)`；bool shared-library smoke
  20 tests / 1 failure，新 f64 less bool case 为 `Expected Non-NULL`。
- Focused GREEN：typed call contracts 4/0；bool typed direct-call smoke 20/0。
- 拆分后 focused GREEN：typed call contracts 4/0；bool typed direct-call smoke 20/0；
  f64 typed direct-call smoke 18/0。

## 验证

- WSL GCC debug 目标构建通过，并在新增 f64 route `.c` 后触发一次 CMake glob
  reconfigure；后续宽构建无新增 glob mismatch 结尾提示。
- 合约组通过：source 19/0、call contracts 4/0、typed call contracts 4/0、
  generic numeric 1/0、global 7/0、logical 4/0、power 2/0、frame setup 1/0、
  return 1/0、value SemIR 4/0、float contracts 1/0。
- 共享库烟测组通过：shared 8/0、call smoke 5/0、typed direct-call 5/0、
  i64 three-arg 6/0、bool 20/0、u64 22/0、f64 18/0、typed arithmetic 5/0、
  typed bitwise 6/0、value-type 1/0、float smoke 1/0、generic numeric smoke 1/0、
  global smoke 9/0、logical smoke 4/0、power smoke 1/0。

## 文件规模

- `backend_aot_c_typed_bool_thunks.c`：821 physical / 716 non-empty lines
- `backend_aot_c_typed_direct_calls.c`：868 / 785
- `backend_aot_c_typed_direct_f64_calls.c`：189 / 165
- `backend_aot_c_typed_direct_f64_calls.h`：53 / 50
- `backend_aot_c_lowering_calls.c`：892 / 846
- `backend_aot_c_emitter.h`：781 / 776
- `test_aot_c_typed_call_contracts.c`：965 / 927
- `test_aot_c_typed_direct_call_bool_shared_library_smoke.c`：562 / 521

## 未完成

This is narrow f64 `<` bool-return direct-call coverage plus f64 route modularization.
inline structs、`in`/`out` writeback、deopt/dynamic bridges、dynamic value access helpers、
general typed-return ABI、07-S5 完整验收和 08-12 仍未完成。
