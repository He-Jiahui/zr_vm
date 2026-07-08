# AOT 07-S2/S4 Generic Dynamic Object Slot Truthiness Local Fold

- 时间：2026-07-04 09:46:53 +08:00
- 状态：完成本子切片；07~12 总目标继续进行中。

## 完成项目

- 为相邻的 `TO_OBJECT -> JUMP_IF` 与
  `TO_OBJECT -> LOGICAL_NOT -> JUMP_IF_BOOL_FALSE` 建立保守本地 object slot 真值折叠。
- `backend_aot_c_lowering_generic_logical.c` 新增
  `backend_aot_c_to_object_slot_written_immediately_before()`，只在上一条指令确认为
  `TO_OBJECT` 且写入同一 source/condition slot 时启用。
- 新增 `backend_aot_c_write_object_slot_truthiness()`，生成 C 直接读取 frame 中已由
  `TO_OBJECT` 写入的 object/null 槽：`ZR_VALUE_IS_TYPE_NULL` 为 false，
  `ZR_VALUE_IS_TYPE_OBJECT` 为 true，其他类型 fail closed。
- `JUMP_IF` 对 dynamic object slot 生成
  `zr_aot_generic_jump_if_object_slot_local`，避免调用
  `ZrLibrary_AotRuntime_GenericPrimitiveIsTruthy`。
- `LOGICAL_NOT` 对 dynamic object slot 生成
  `zr_aot_generic_logical_not_object_slot_local`，直接写入本地 bool 结果，避免调用
  `ZrLibrary_AotRuntime_GenericPrimitiveLogicalNot` 与 bool sync。
- 保留 `TO_OBJECT` 本身的 runtime conversion 边界；本切片只覆盖相邻
  `TO_OBJECT` 产物的 object/null truthiness。

## RED/GREEN

- RED：源码契约缺少 `TO_OBJECT` 相邻 slot 证明 helper、object slot truthiness emitter、
  object/null 类型检查和 object-slot marker。
- RED：新增 dynamic object slot `JUMP_IF` smoke 缺少
  `zr_aot_generic_jump_if_object_slot_local` 与 object slot value 读取。
- RED：新增 dynamic object slot `LOGICAL_NOT` smoke 缺少
  `zr_aot_generic_logical_not_object_slot_local` 与直接
  `zr_aot_b1 = (TZrBool)(!zr_aot_object_slot_truthy);`。
- GREEN：`TO_OBJECT` 产物仍写 frame slot；后续 generic truthiness 消费直接读 object/null
  类型，不再进入 unsupported generic primitive truthiness runtime helper。

## 验证

- WSL GCC：generic JUMP_IF 9/0、generic LOGICAL_NOT 8/0、logical contracts 4/0、
  logical shared-library 6/0、generic equality 4/0、frame setup contracts 1/0、
  control contracts 2/0、control shared-library 2/0。
- WSL Clang：同组 9/0、8/0、4/0、6/0、4/0、1/0、2/0、2/0；仍有既有
  `const char *` 到 `TZrNativeString` qualifier warning。
- Windows MSVC Debug：generic JUMP_IF 0 failures / 9 expected ignores；generic LOGICAL_NOT
  0 failures / 8 expected ignores；logical shared-library 0 failures / 6 expected ignores；
  generic equality 0 failures / 4 expected ignores；control shared-library 0 failures /
  2 expected ignores；logical/frame/control contracts 4/0、1/0、2/0。
- `git diff --check`：本切片涉及文件 scoped check exit 0；仅报告既有 LF/CRLF 提示。

## 仍未完成

- value-copy migration、GC roots/exports/frame cleanup、更广 byte-frame narrowing。
- 性能计数、完整 typed 函数体零 `SZrValue`/frame write 证明。
