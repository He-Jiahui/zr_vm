# AOT 07-S2/S4 Generic String-Constant Truthiness Local Fold

- 时间：2026-07-04 06:49:49 +08:00
- 状态：完成本子切片；07~12 总目标继续进行中。

## 完成项目

- 为立即相邻的 `GET_CONSTANT string -> JUMP_IF` 与
  `GET_CONSTANT string -> LOGICAL_NOT -> JUMP_IF_BOOL_FALSE` 建立保守本地折叠。
- 新增 `backend_aot_c_string_constant_truthy()`，用
  `ZrCore_String_GetByteLength(stringValue) > 0u` 判定空串 false、非空串 true。
- 常量发射、function-body dispatch、frame descriptor 与 generic logical lowering 共用
  `backend_aot_c_string_constant_consumed_by_local_jump_if()` /
  `backend_aot_c_string_constant_consumed_by_local_logical_not()`。
- 源码契约新增 string helper、marker、source-skip 分流和 frame-descriptor local-only needles。

## RED/GREEN

- RED：新增 string `LOGICAL_NOT` smoke 缺少
  `zr_aot_string_constant_local_logical_not_source_skip slot=0`；新增 string `JUMP_IF`
  smoke 缺少 `zr_aot_string_constant_local_jump_if_source_skip slot=0`。
- RED 生成 C 仍调用
  `ZrLibrary_AotRuntime_GenericPrimitiveLogicalNot(state, &frame, 1, 0)` 与
  `ZrLibrary_AotRuntime_GenericPrimitiveIsTruthy(state, &frame, 0, &zr_aot_truthy)`。
- GREEN：空串/非空串 `JUMP_IF` 生成 source-skip 和 true/false 直接分支 marker；
  空串/非空串 `LOGICAL_NOT` 生成 source-skip 和直接 bool 赋值 marker。
- GREEN 目标项目不再包含 targeted primitive truthiness runtime helper、temporary truthy bool、
  `CopyConstant` 源写入或 bool sync。

## 验证

- WSL GCC：generic JUMP_IF 7/0、generic LOGICAL_NOT 6/0、logical shared-library 6/0、
  generic equality 4/0、logical contracts 4/0、frame setup contracts 1/0、control contracts 2/0。
- WSL Clang：同组 7/0、6/0、6/0、4/0、4/0、1/0、2/0。
- Windows MSVC Debug：generic JUMP_IF 0 failures / 7 expected ignores；generic LOGICAL_NOT
  0 failures / 6 expected ignores；logical shared-library 0 failures / 6 expected ignores；
  generic equality 0 failures / 4 expected ignores；logical/frame/control contracts 4/0、1/0、2/0。
- `git diff --check`：仅报告仓库既有 LF/CRLF 提示。

## 仍未完成

- 动态 string slot truthiness。
- 一般 object truthiness。
- value-copy migration、GC roots/exports/frame cleanup、更广 byte-frame narrowing。
- 性能计数、完整 typed 函数体零 `SZrValue`/frame write 证明。
