# AOT 07-S2/S4 Generic Numeric F64 MOD Local Fold

- 时间：2026-07-04 12:51:07 +08:00
- 状态：完成本子切片；07-S2/S4 部分完成；07~12 总目标继续进行中。

## 完成项目

- 覆盖手工 bytecode 形态：`GET_CONSTANT float -> GET_CONSTANT float -> MOD -> RETURN`。
- proven f64 operands 保留为 `zr_aot_scalar_constant_f64_local`，供 generic numeric `MOD` 直接读取。
- generic `MOD` 在 destination/left/right 均已证明为 f64 scalar local 时输出
  `zr_aot_generic_numeric_f64_mod_scalar_local`。
- 本地 MOD 发射除零检查：`if (zr_aot_f1 == (TZrFloat64)0.0)`，失败时报
  `ZrCore_Debug_RunError(state, "modulo by zero")` 并 `ZR_AOT_C_FAIL()`。
- 本地 MOD 发射 `zr_aot_f2 = fmod(zr_aot_f0, zr_aot_f1);`，不再调用
  `ZrLibrary_AotRuntime_GenericNumericMod(state, &frame, 2, 0, 1)`。
- proven f64 destination 不再通过 `zr_aot_generic_numeric_sync_f64_local_boundary` 或
  `ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, 2, &zr_aot_f2)` 回写 value slot。
- dynamic 或 unproven generic numeric operands 仍保留 runtime helper fallback。

## RED/GREEN

- RED 1：focused WSL GCC smoke 先失败于缺少 `zr_aot_scalar_constant_f64_local` 和
  `zr_aot_generic_numeric_f64_mod_scalar_local`；生成 C 仍调用
  `ZrLibrary_AotRuntime_GenericNumericMod(state, &frame, 2, 0, 1)`。
- RED 2：补齐 float constant/local 声明后，生成 C 仍落回 `GenericNumericMod` +
  `SyncFloatLocal`，原因是 scalar-local proof 看不到 generic MOD 对 destination 的 f64 写入。
- GREEN：`backend_aot_c_scalar_locals.c` 记录 generic numeric f64 MOD 的 operand/destination/exec-write proof；
  `backend_aot_c_lowering_generic_numeric_arithmetic.c` 在 runtime fallback 前尝试 f64 local helper；
  `backend_aot_c_function_body.c` / `backend_aot_c_emitter.h` 将 `instructionIndex` 传入 MOD lowering。
- GREEN 目标项目不包含 `zr_aot_arith_exec_generic_numeric_binary_boundary`、
  `GenericNumericMod(state, &frame, 2, 0, 1)`、`SyncFloatLocal(state, &frame, 2, &zr_aot_f2)` 或
  `ZR_VALUE_IS_TYPE_FLOAT(zr_aot_left->type)`。

## 验证

- WSL GCC：generic numeric smoke 1/0；generic numeric contracts 1/0；source contracts 24/0；
  power smoke 1/0；power contracts 2/0；generic LOGICAL_NOT 8/0；generic equality stack-copy 4/0。
- WSL Clang：generic numeric smoke 1/0；generic numeric contracts 1/0；source contracts 24/0；
  power contracts 2/0；generic LOGICAL_NOT 8/0；generic equality stack-copy 4/0。
- Windows MSVC Debug（`build-msvc-aot-stack-copy`）：构建 focused smoke/contract/regression targets；
  generic numeric contracts 1/0；source contracts 24/0；power contracts 2/0；Unix-only generic numeric smoke
  0 failures / 1 expected ignore；generic LOGICAL_NOT 0 failures / 8 expected ignores；
  generic equality stack-copy 0 failures / 4 expected ignores。
- scoped `git diff --check` exit 0，仅既有 LF/CRLF 提示。

## 仍未完成

- dynamic/unproven generic numeric operands 的本地化。
- 更广 value-copy migration。
- GC roots/exports/frame cleanup、更广 byte-frame narrowing。
- 性能计数、完整 typed 函数体零 `SZrValue`/frame write 证明。
