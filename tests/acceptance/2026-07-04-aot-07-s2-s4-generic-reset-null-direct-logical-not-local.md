# AOT 07-S2/S4 Generic Reset-Null Direct LOGICAL_NOT Local Fold

- 时间：2026-07-04 12:14:51 +08:00
- 状态：完成本子切片；07~12 总目标继续进行中。

## 完成项目

- 将相邻 `RESET_STACK_NULL -> LOGICAL_NOT -> JUMP_IF_BOOL_FALSE` 纳入保守本地折叠。
- `backend_aot_c_lowering_values.c` 新增 direct reset-null consumed-by-local logical-not 判定。
- `RESET_STACK_NULL` 在该相邻链路中输出
  `zr_aot_reset_stack_null_local_logical_not_skip`，不再调用
  `ZrLibrary_AotRuntime_ResetStackNull(state, &frame, sourceSlot)`。
- generic `LOGICAL_NOT` 对 direct reset-null 源输出
  `zr_aot_generic_logical_not_reset_null_local` 和 `zr_aot_bD = ZR_TRUE;`。
- frame descriptor 将该 direct reset-null logical-not 链路的 reset 源和 bool 结果统一标为 local-only。
- `test_aot_c_call_shared_library_smoke.c` 的 static numeric call 断言改为允许 typed direct-call deopt fallback
  的显式 `SyncUnsignedIntLocal` / `SyncFloatLocal`，同时继续禁止 stack-copy 边界同步和目的槽 materialization。

## RED/GREEN

- RED：`test_aot_c_generated_shared_library_executes_generic_logical_not_reset_null_source_local_bool_branch`
  先失败于缺少 `zr_aot_reset_stack_null_local_logical_not_skip slot=0`。
- GREEN：生成 C 对 direct reset-null logical-not 发出 `ZR_TRUE` bool local，不再包含 targeted
  `GenericPrimitiveLogicalNot(state, &frame, 1, 0)`、`SyncBoolLocal(state, &frame, 1, ...)`、
  `ResetStackNull(state, &frame, 0)` 或 `frame.slotBase[0].value`。
- GREEN：既有 reset-null stack-copy logical-not 路径仍保留
  `zr_aot_reset_null_stack_copy_local_logical_not_*` 与
  `zr_aot_generic_logical_not_reset_null_stack_copy_local` 覆盖。

## 验证

- WSL GCC：generic LOGICAL_NOT 8/0；call shared-library 5/0；call-result stack-copy equality guardrail 1/0；
  generic not-equal stack-copy JUMP_IF guardrail 1/0；logical contracts 4/0；generic JUMP_IF 9/0；
  generic equality stack-copy 4/0。
- WSL Clang：generic LOGICAL_NOT 8/0；logical contracts 4/0；generic JUMP_IF 9/0；
  generic equality stack-copy 4/0。
- Windows MSVC Debug：构建 generic LOGICAL_NOT 与 logical contracts 目标；generic LOGICAL_NOT
  0 failures / 8 expected ignores；logical contracts 4/0。
- `git diff --check`：exit 0，仅报告既有 LF/CRLF 提示。

## 仍未完成

- 更广 value-copy migration。
- GC roots/exports/frame cleanup、更广 byte-frame narrowing。
- 性能计数、完整 typed 函数体零 `SZrValue`/frame write 证明。
- 07~12 总体计划中的 generic sharing、reflection、metadata、code stripping 后续闭环。
