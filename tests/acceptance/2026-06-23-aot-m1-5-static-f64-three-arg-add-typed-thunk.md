# 2026-06-23 AOT M1.5 07-S5 static f64 three-arg add typed thunk

## 状态

- 时间：2026-06-23 10:54:32 +08:00
- 状态：子切片完成；07-S5 / M1.5 / 07 仍部分完成；08-12 未开始

## 完成项目

- `backend_aot_c_typed_f64_thunks.c` 新增三参数 float 加法识别：
  `backend_aot_c_try_get_f64_arg0_arg1_arg2_add_return()` 只接受三个 f64 参数、
  两条 `ADD_FLOAT` 后接 `FUNCTION_RETURN` 的窄形态。
- `backend_aot_c_can_emit_typed_f64_three_arg_thunk()` 和 f64 thunk forward/definition writer
  生成 `TZrFloat64` 三参数 typed thunk：
  `return (TZrFloat64)(zr_aot_arg0 + zr_aot_arg1 + zr_aot_arg2);`。
- `backend_aot_c_typed_direct_calls.c` 证明 destination 和三个 call-window 参数槽都是已写入的
  f64 scalar locals。
- `backend_aot_c_lowering_calls.c` / `backend_aot_c_emitter.h` 增加
  `zr_aot_static_f64_three_arg_direct_call` writer，并在需要时同步 `nativeDouble` 栈槽。
- `tests/parser/test_aot_c_typed_call_contracts.c` 和
  `tests/parser/test_aot_c_typed_direct_call_f64_shared_library_smoke.c` 覆盖契约与共享库执行。

## RED / GREEN

- RED：typed call contracts 4 tests / 1 failure，缺
  `backend_aot_c_can_emit_typed_f64_three_arg_thunk(const SZrFunction *function)`。
- RED：f64 shared-library smoke 14 tests / 1 failure，新增三参 add 用例为 `Expected Non-NULL`。
- Focused GREEN：typed call contracts 4/0；f64 typed direct-call smoke 14/0。

## 验证

- WSL GCC debug build：`cmake --build build/codex-aot-07-wsl-gcc-debug -j 2` 全量构建通过。
- 合约组通过：source 19/0、call contracts 4/0、typed call contracts 4/0、generic numeric 1/0、
  global 7/0、logical 4/0、power 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、
  float contracts 1/0。
- 共享库烟测组通过：shared 8/0、call smoke 5/0、typed direct-call 5/0、i64 three-arg 6/0、
  bool 19/0、u64 22/0、f64 14/0、typed arithmetic 5/0、typed bitwise 6/0、value-type 1/0、
  float smoke 1/0、generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、power smoke 1/0。
- 验证备注：CTest 在该构建目录未注册这些测试名；最终使用已有测试二进制逐个执行并通过。

## 文件规模

- `backend_aot_c_emitter.h`：774 physical / 769 non-empty lines
- `backend_aot_c_typed_f64_thunks.c`：968 / 849
- `backend_aot_c_lowering_calls.c`：853 / 809
- `backend_aot_c_typed_direct_calls.c`：973 / 876
- `test_aot_c_typed_call_contracts.c`：886 / 848
- `test_aot_c_typed_direct_call_f64_shared_library_smoke.c`：348 / 319

大文件备注：`backend_aot_c_typed_f64_thunks.c` 和 `backend_aot_c_typed_direct_calls.c`
仍低于 1000 行，且当前职责分别保持为 f64 typed thunk 生成与 typed direct-call route proof。
本切片不强拆以避免把行为切片扩大成结构性重排；后续再添加 f64 多参数/更多 direct-call
route 时，最小拆分边界应分别抽出 f64 thunk shape helpers 与按标量类型拆分的 direct-call
route 模块。

## 未完成

inline structs、`in`/`out` writeback、deopt/dynamic bridges、dynamic value access helpers、
general multi-arg typed-return ABI、07-S5 完整验收和 08-12 仍未完成。
