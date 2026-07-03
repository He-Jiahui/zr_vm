# AOT 07-S2/S4 Generic LOGICAL_NOT Bool Stack-Copy Local Branch

- 时间：2026-07-04 06:09:04 +08:00
- 状态：完成本子切片；07~12 总目标继续进行中。

## 完成项目

- 修复 stack-copy destination consumer 反推：当 `SET_STACK` 目标槽后续作为 generic
  `LOGICAL_NOT` 源槽时，按 `backend_aot_c_scalar_locals_truthiness_consumer_kind(candidateKind)`
  收窄，而不是被 u64/f64 泛型真值消费者路径抢先声明。
- 新增 `GET_CONSTANT bool -> SET_STACK -> LOGICAL_NOT -> JUMP_IF_BOOL_FALSE` shared-library smoke。
- 源码契约新增连续 needle，锁定 `LOGICAL_NOT` 在 stack-copy destination consumers 中的真值收窄规则。

## RED/GREEN

- RED：新增 smoke 失败在缺少 `zr_aot_scalar_stack_copy_bool dstSlot=5 srcSlot=0`。
  生成 C 只声明 `zr_aot_b0`、`zr_aot_b1`、`zr_aot_u5`，`SET_STACK` 走
  `ZrLibrary_AotRuntime_CopyStack(state, &frame, 5, 0)`，但后续仍生成
  `zr_aot_b1 = (TZrBool)(!zr_aot_b5);`。
- GREEN：生成 C 声明 `TZrBool zr_aot_b5`，输出
  `zr_aot_scalar_stack_copy_bool dstSlot=5 srcSlot=0`、
  `zr_aot_b5 = (TZrBool)(zr_aot_b0 != 0u);`、
  `zr_aot_generic_logical_not_scalar_local` 和
  `zr_aot_b1 = (TZrBool)(!zr_aot_b5);`，且不包含 `zr_aot_u5`、`CopyStack`、
  `GenericPrimitiveLogicalNot(state, &frame, 1, 5)` 或 `SyncBoolLocal(state, &frame, 1)`。

## 验证

- WSL GCC：generic JUMP_IF 6/0、generic LOGICAL_NOT 5/0、logical shared-library 6/0、
  generic equality 4/0、logical contracts 4/0、frame setup contracts 1/0、control contracts 2/0。
- WSL Clang：同组 6/0、5/0、6/0、4/0、4/0、1/0、2/0。
- Windows MSVC Debug：generic JUMP_IF 0 failures / 6 expected ignores；generic LOGICAL_NOT
  0 failures / 5 expected ignores；logical shared-library 0 failures / 6 expected ignores；
  generic equality 0 failures / 4 expected ignores；logical/frame/control contracts 4/0、1/0、2/0。
- `git diff --check`：仅报告仓库既有 LF/CRLF 提示。

## 仍未完成

- dynamic/string/object truthiness。
- value-copy migration、GC roots/exports/frame cleanup、更广 byte-frame narrowing。
- 性能计数、完整 typed 函数体零 `SZrValue`/frame write 证明。
