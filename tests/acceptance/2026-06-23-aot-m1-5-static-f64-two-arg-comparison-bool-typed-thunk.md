# 2026-06-23 AOT M1.5 07-S5 static f64 two-arg comparison bool typed thunk

## 状态

- 时间：2026-06-23 13:05:52 +08:00
- 状态：子切片完成；07-S5 / M1.5 / 07 仍部分完成；08-12 未开始

## 完成项目

- `backend_aot_c_typed_bool_thunks.c` 在既有 f64 `<` bool route 上补齐
  `<=`、`==`、`!=`、`>`、`>=`，当前 f64 两参数 bool-return typed thunk 覆盖
  `<`、`<=`、`==`、`!=`、`>`、`>=`。
- `backend_aot_c_try_get_bool_f64_arg0_arg1_*_return()` recognizers 覆盖
  `LOGICAL_LESS_FLOAT`、`LOGICAL_LESS_EQUAL_FLOAT`、`LOGICAL_EQUAL_FLOAT`、
  `LOGICAL_NOT_EQUAL_FLOAT`、`LOGICAL_GREATER_FLOAT`、
  `LOGICAL_GREATER_EQUAL_FLOAT` + `FUNCTION_RETURN` 窄形态。
- `backend_aot_write_c_typed_bool_thunks()` 现在可发出
  `static TZrBool zr_aot_typed_bool_fn_N(struct SZrState *state, TZrFloat64 arg0, TZrFloat64 arg1)`
  并直接返回对应的 C 比较表达式。
- f64 bool direct-call route 继续复用
  `backend_aot_write_c_static_direct_f64_bool_two_arg_function_call()` 和
  `zr_aot_static_f64_bool_two_arg_direct_call`，直接传 `zr_aot_f*` 参数并只在需要时
  同步 bool 栈槽。
- `tests/parser/test_aot_c_typed_call_contracts.c` 增加 f64 比较族 recognizer、
  opcode、writer 和 return 表达式 needles。
- `tests/parser/test_aot_c_typed_direct_call_bool_shared_library_smoke.c` 新增
  `<=`、`==`、`!=`、`>`、`>=` 五个 f64 bool typed direct-call shared-library smoke，
  每个 case 运行结果为 42，并检查 typed thunk、f64 bool direct-call marker 和
  bool 栈槽同步 marker。

## RED / GREEN

- RED 1：typed call contracts 4 tests / 1 failure，缺少
  `backend_aot_c_try_get_bool_f64_arg0_arg1_less_equal_return(`；bool shared-library
  smoke 21 tests / 1 failure，新 f64 `<=` case 为 `Expected Non-NULL`。
- GREEN 1：typed call contracts 4/0；bool typed direct-call smoke 21/0。
- RED 2：typed call contracts 4 tests / 1 failure，缺少
  `backend_aot_c_try_get_bool_f64_arg0_arg1_equal_return(`；bool shared-library smoke
  25 tests / 4 failures，`==`、`!=`、`>`、`>=` cases 均为 `Expected Non-NULL`。
- GREEN 2：typed call contracts 4/0；bool typed direct-call smoke 25/0。

## 验证

- WSL GCC debug 宽构建通过。
- 合约组通过：source 19/0、call contracts 4/0、typed call contracts 4/0、
  generic numeric 1/0、global 7/0、logical 4/0、power 2/0、frame setup 1/0、
  return 1/0、value SemIR 4/0、float contracts 1/0。
- 共享库烟测组首次 300s 命令超时，未采用该截断输出；随后以 900s 重跑通过：
  shared 8/0、call smoke 5/0、typed direct-call 5/0、i64 three-arg 6/0、bool 25/0、
  u64 22/0、f64 18/0、typed arithmetic 5/0、typed bitwise 6/0、value-type 1/0、
  float smoke 1/0、generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、
  power smoke 1/0。

## 文件规模

- `backend_aot_c_typed_bool_thunks.c`：906 physical / 791 non-empty lines
- `test_aot_c_typed_call_contracts.c`：982 / 944
- `test_aot_c_typed_direct_call_bool_shared_library_smoke.c`：702 / 651
- `docs/plans/aot/07-codegen-register-model-and-environment-isolation.md`：4878 / 4605
- `docs/plans/aot/index.md`：5007 / 4862
- `docs/parser-and-semantics/csharp-value-type-semir-aot.md`：1178 / 810

## 未完成

This is narrow f64 two-arg bool comparison typed direct-call coverage only.
inline structs、`in`/`out` writeback、deopt/dynamic bridges、dynamic value access helpers、
general typed-return ABI、07-S5 完整验收和 08-12 仍未完成。

`tests/parser/test_aot_c_typed_call_contracts.c` 已接近 1000 行；后续继续扩充 typed
call contracts 前应先拆分 focused contract/support。
