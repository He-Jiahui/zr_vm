# AOT 07-S2/S4 Generic Numeric F64 SUB/MUL Local Fold

- 时间：2026-07-05 03:38:44 +08:00
- 状态：完成本子切片；07-S2/S4 部分完成；07~12 总目标继续进行中。

## 完成项目

- 覆盖手工 bytecode 形态：`GET_CONSTANT float -> GET_CONSTANT float -> SUB/MUL -> RETURN`。
- proven f64 operands 保留为 `zr_aot_scalar_constant_f64_local`，供 generic numeric `SUB` / `MUL` 直接读取。
- generic `SUB` / `MUL` 在 destination/left/right 均已证明为 f64 scalar local，且结果 value slot 可跳过时输出
  `zr_aot_generic_numeric_f64_sub_scalar_local` / `zr_aot_generic_numeric_f64_mul_scalar_local`。
- 本地 SUB/MUL 分别发射 `zr_aot_f2 = zr_aot_f0 - zr_aot_f1;` 和
  `zr_aot_f2 = zr_aot_f0 * zr_aot_f1;`，不再调用
  `ZrLibrary_AotRuntime_GenericNumericSub(state, &frame, 2, 0, 1)` /
  `ZrLibrary_AotRuntime_GenericNumericMul(state, &frame, 2, 0, 1)`。
- proven f64 destination 不再通过 `zr_aot_generic_numeric_sync_f64_local_boundary` 或
  `ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, 2, &zr_aot_f2)` 回写 value slot。
- dynamic 或 unproven generic numeric operands 仍保留 runtime helper fallback。

## RED/GREEN

- RED：focused WSL GCC smoke 新增 SUB/MUL 用例后失败于缺少 f64 SUB/MUL local marker；生成 C 仍走
  `GenericNumericSub` / `GenericNumericMul` boundary。
- GREEN：f64 binary scalar-local helper 增加 operator token，ADD/SUB/MUL 共用同一 proof；`backend_aot_c_function_body.c`
  / `backend_aot_c_emitter.h` 将 `instructionIndex` 传入 SUB/MUL lowering；`backend_aot_c_scalar_locals.c`
  将 generic SUB/MUL 纳入 f64 operand、destination、exec-write 和 f64 consumer proof。
- GREEN 目标项目不包含 `zr_aot_arith_exec_generic_numeric_binary_boundary`、
  `GenericNumericSub(state, &frame, 2, 0, 1)`、`GenericNumericMul(state, &frame, 2, 0, 1)`、
  `SyncFloatLocal(state, &frame, 2, &zr_aot_f2)` 或 `ZR_VALUE_IS_TYPE_FLOAT(zr_aot_left->type)`。

## 验证

- WSL GCC：generic numeric smoke 4/0；generic numeric contracts 1/0；source contracts 24/0；
  power smoke 1/0；power contracts 2/0；generic LOGICAL_NOT 8/0；generic equality stack-copy 4/0。
- WSL Clang：generic numeric smoke 4/0；generic numeric contracts 1/0；source contracts 24/0；
  power contracts 2/0；generic LOGICAL_NOT 8/0；generic equality stack-copy 4/0。
- Windows MSVC Debug（`build-msvc-aot-stack-copy`）：构建 focused smoke/contract/regression targets；
  generic numeric shared-library smoke 0 failures / 4 expected ignores；generic numeric contracts 1/0；
  source contracts 24/0；power contracts 2/0；generic LOGICAL_NOT 0 failures / 8 expected ignores；
  generic equality stack-copy 0 failures / 4 expected ignores。
- scoped `git diff --check` exit 0，仅既有 LF/CRLF 提示。

## 仍未完成

- generic numeric DIV 及 dynamic/unproven numeric operands 的本地化。
- 更广 value-copy migration。
- GC roots/exports/frame cleanup、更广 byte-frame narrowing。
- 性能计数、完整 typed 函数体零 `SZrValue`/frame write 证明。
