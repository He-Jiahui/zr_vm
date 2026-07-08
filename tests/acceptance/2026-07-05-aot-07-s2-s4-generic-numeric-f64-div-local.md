# AOT 07-S2/S4 Generic Numeric F64 DIV Local Fold

- 时间：2026-07-05 04:28:30 +08:00
- 状态：完成本子切片；07-S2/S4 部分完成；07~12 总目标继续进行中。

## 完成项目

- 覆盖手工 bytecode 形态：`GET_CONSTANT float -> GET_CONSTANT float -> DIV -> RETURN`。
- proven f64 operands 保留为 `zr_aot_scalar_constant_f64_local`，供 generic numeric `DIV` 直接读取。
- generic `DIV` 在 destination/left/right 均已证明为 f64 scalar local，且结果 value slot 可跳过时输出
  `zr_aot_generic_numeric_f64_div_scalar_local`。
- 本地 DIV 发射除零检查：`if (zr_aot_f1 == (TZrFloat64)0.0)`，失败时报
  `ZrCore_Debug_RunError(state, "divide by zero")` 并 `ZR_AOT_C_FAIL()`。
- 本地 DIV 发射 `zr_aot_f2 = zr_aot_f0 / zr_aot_f1;`，不再调用
  `ZrLibrary_AotRuntime_GenericNumericDiv(state, &frame, 2, 0, 1)`。
- proven f64 destination 不再通过 `zr_aot_generic_numeric_sync_f64_local_boundary` 或
  `ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, 2, &zr_aot_f2)` 回写 value slot。
- dynamic 或 unproven generic numeric operands 仍保留 runtime helper fallback。

## RED/GREEN

- RED：focused WSL GCC smoke 新增 DIV 用例后，generic numeric smoke 5 项中只有 DIV 失败，失败点为缺少
  `zr_aot_generic_numeric_f64_div_scalar_local`；生成 C 仍走
  `ZrLibrary_AotRuntime_GenericNumericDiv(state, &frame, 2, 0, 1)` boundary。
- GREEN：`backend_aot_c_lowering_generic_numeric_arithmetic.c` 在 DIV runtime fallback 前尝试 guarded f64
  scalar-local helper；`backend_aot_c_function_body.c` / `backend_aot_c_emitter.h` 将 `instructionIndex`
  传入 DIV lowering；`backend_aot_c_scalar_locals.c` 将 generic DIV 纳入 f64 operand、destination、
  exec-write 和 f64 consumer proof。
- GREEN 目标项目不包含 `zr_aot_arith_exec_generic_numeric_binary_boundary`、
  `GenericNumericDiv(state, &frame, 2, 0, 1)`、`SyncFloatLocal(state, &frame, 2, &zr_aot_f2)` 或
  `ZR_VALUE_IS_TYPE_FLOAT(zr_aot_left->type)`。

## 验证

- WSL GCC：generic numeric smoke 5/0；generic numeric contracts 1/0；source contracts 24/0；
  power smoke 1/0；power contracts 2/0；generic LOGICAL_NOT 8/0；generic equality stack-copy 4/0。
- WSL Clang：generic numeric smoke 5/0；generic numeric contracts 1/0；source contracts 24/0；
  power contracts 2/0；generic LOGICAL_NOT 8/0；generic equality stack-copy 4/0。
- Windows MSVC Debug（`build-msvc-aot-stack-copy`）：构建 focused smoke/contract/regression targets；
  generic numeric shared-library smoke 0 failures / 5 expected ignores；generic numeric contracts 1/0；
  source contracts 24/0；power contracts 2/0；generic LOGICAL_NOT 0 failures / 8 expected ignores；
  generic equality stack-copy 0 failures / 4 expected ignores。
- scoped `git diff --check` exit 0，仅既有 LF/CRLF 提示。

## 仍未完成

- dynamic/unproven generic numeric operands 的本地化。
- 更广 value-copy migration。
- GC roots/exports/frame cleanup、更广 byte-frame narrowing。
- 性能计数、完整 typed 函数体零 `SZrValue`/frame write 证明。
