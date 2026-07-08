# AOT 07-S2/S4 Generic Dynamic String Slot Truthiness Local Fold

- 时间：2026-07-04 09:19:31 +08:00
- 状态：完成本子切片；07~12 总目标继续进行中。

## 完成项目

- 为相邻的 `TO_STRING -> JUMP_IF` 与
  `TO_STRING -> LOGICAL_NOT -> JUMP_IF_BOOL_FALSE` 建立保守本地 string slot 真值折叠。
- `backend_aot_c_lowering_generic_logical.c` 新增
  `backend_aot_c_to_string_slot_written_immediately_before()`，只在上一条指令确认为
  `TO_STRING` 且写入同一 source/condition slot 时启用。
- 新增 `backend_aot_c_write_string_slot_truthiness()`，生成 C 直接读取 frame 中已由
  `TO_STRING` 写入的 string 槽，检查 `ZR_VALUE_IS_TYPE_STRING` 与 object 非空后用
  `ZrCore_String_GetByteLength(...) > 0u` 得到真值。
- `JUMP_IF` 对 dynamic string slot 生成
  `zr_aot_generic_jump_if_string_slot_local`，避免调用
  `ZrLibrary_AotRuntime_GenericPrimitiveIsTruthy`。
- `LOGICAL_NOT` 对 dynamic string slot 生成
  `zr_aot_generic_logical_not_string_slot_local`，直接写入本地 bool 结果，避免调用
  `ZrLibrary_AotRuntime_GenericPrimitiveLogicalNot` 与 bool sync。
- 保留 `TO_STRING` 本身的 runtime conversion 边界；本切片不处理一般 object truthiness。

## RED/GREEN

- RED：新增 dynamic string slot `JUMP_IF` smoke 缺少
  `zr_aot_generic_jump_if_string_slot_local`、string slot value 读取与
  `ZrCore_String_GetByteLength(zr_aot_string_slot_string) > 0u`。
- RED：新增 dynamic string slot `LOGICAL_NOT` smoke 缺少
  `zr_aot_generic_logical_not_string_slot_local` 与直接
  `zr_aot_b1 = (TZrBool)(!zr_aot_string_slot_truthy);`。
- RED：源码契约缺少 `TO_STRING` 相邻 slot 证明 helper 与 string slot truthiness emitter。
- GREEN：`TO_STRING` 产物仍写 frame slot；后续 generic truthiness 消费直接读 string 槽长度，
  不再进入 unsupported generic primitive truthiness runtime helper。

## 验证

- WSL GCC：generic JUMP_IF 8/0、generic LOGICAL_NOT 7/0、logical contracts 4/0、
  logical shared-library 6/0、generic equality 4/0、frame setup contracts 1/0、
  control contracts 2/0、control shared-library 2/0。
- WSL Clang：同组 8/0、7/0、4/0、6/0、4/0、1/0、2/0、2/0；仍有既有
  `const char *` 到 `TZrNativeString` qualifier warning。
- Windows MSVC Debug：generic JUMP_IF 0 failures / 8 expected ignores；generic LOGICAL_NOT
  0 failures / 7 expected ignores；logical shared-library 0 failures / 6 expected ignores；
  generic equality 0 failures / 4 expected ignores；control shared-library 0 failures /
  2 expected ignores；logical/frame/control contracts 4/0、1/0、2/0。
- `git diff --check`：本切片涉及文件 scoped check exit 0；仅报告既有 LF/CRLF 提示。

## 仍未完成

- 一般 object truthiness。
- value-copy migration、GC roots/exports/frame cleanup、更广 byte-frame narrowing。
- 性能计数、完整 typed 函数体零 `SZrValue`/frame write 证明。
