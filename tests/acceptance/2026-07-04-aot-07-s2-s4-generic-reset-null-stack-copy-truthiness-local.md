# AOT 07-S2/S4 Generic Reset-Null Stack-Copy Truthiness Local Fold

- 时间：2026-07-04 08:47:05 +08:00
- 状态：完成本子切片；07~12 总目标继续进行中。

## 完成项目

- 为相邻的 `RESET_STACK_NULL -> SET_STACK/GET_STACK -> JUMP_IF` 与
  `RESET_STACK_NULL -> SET_STACK/GET_STACK -> LOGICAL_NOT -> JUMP_IF_BOOL_FALSE`
  建立保守本地折叠。
- `backend_aot_c_lowering_values.c` 新增 reset-null stack-copy consumed-by-local 判定。
- `RESET_STACK_NULL` 在只被本地栈复制真值链消费时跳过 runtime reset。
- `SET_STACK`/`GET_STACK` 在只被本地 reset-null truthiness 消费时跳过 `CopyStack`。
- `JUMP_IF` 对 reset-null stack-copy 生成
  `zr_aot_generic_jump_if_reset_null_stack_copy_false` 和直接 false successor `goto`。
- `LOGICAL_NOT` 对 reset-null stack-copy 生成
  `zr_aot_generic_logical_not_reset_null_stack_copy_local` 和 `zr_aot_bD = ZR_TRUE;`。
- frame descriptor 将该相邻链路的 RESET_STACK_NULL、SET_STACK/GET_STACK、JUMP_IF、LOGICAL_NOT
  统一标为 local-only。
- 保留普通 `RESET_STACK_NULL -> LOGICAL_NOT` runtime fallback 覆盖。

## RED/GREEN

- RED：新增 stack-copy reset-null `JUMP_IF` smoke 缺少
  `zr_aot_reset_null_stack_copy_local_jump_if_reset_skip slot=2`、
  `zr_aot_reset_null_stack_copy_local_jump_if_source_skip dstSlot=3 srcSlot=2` 和
  `zr_aot_generic_jump_if_reset_null_stack_copy_false`。
- RED：新增 stack-copy reset-null `LOGICAL_NOT` smoke 缺少
  `zr_aot_reset_null_stack_copy_local_logical_not_reset_skip slot=3`、
  `zr_aot_reset_null_stack_copy_local_logical_not_source_skip dstSlot=4 srcSlot=3` 和
  `zr_aot_generic_logical_not_reset_null_stack_copy_local`。
- RED：源码契约缺少 reset-null stack-copy helper family。
- GREEN：reset-null 经栈复制后分别折叠到直接 false 分支或直接 `ZR_TRUE` bool 赋值。
- GREEN：目标 stack-copy 段不再包含 targeted `ResetStackNull`、`CopyStack`、temporary truthy bool、
  primitive truthiness runtime helper、bool sync 或相关 `frame.slotBase` 访问。

## 验证

- WSL GCC：generic JUMP_IF 7/0、generic LOGICAL_NOT 6/0、logical contracts 4/0、
  logical shared-library 6/0、generic equality 4/0、frame setup contracts 1/0、
  control contracts 2/0、control shared-library 2/0。
- WSL Clang：同组 7/0、6/0、4/0、6/0、4/0、1/0、2/0、2/0；仍有既有
  `const char *` 到 `TZrNativeString` qualifier warning。
- Windows MSVC Debug：generic JUMP_IF 0 failures / 7 expected ignores；generic LOGICAL_NOT
  0 failures / 6 expected ignores；logical shared-library 0 failures / 6 expected ignores；
  generic equality 0 failures / 4 expected ignores；control shared-library 0 failures /
  2 expected ignores；logical/frame/control contracts 4/0、1/0、2/0。
- `git diff --check`：exit 0，仅报告既有 LF/CRLF 提示。

## 仍未完成

- 动态 string slot truthiness。
- 一般 object truthiness。
- value-copy migration、GC roots/exports/frame cleanup、更广 byte-frame narrowing。
- 性能计数、完整 typed 函数体零 `SZrValue`/frame write 证明。
