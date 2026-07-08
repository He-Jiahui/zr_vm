# AOT 07-S2/S4 Generic String Stack-Copy Truthiness Local Fold

- 时间：2026-07-04 07:41:29 +08:00
- 状态：完成本子切片；07~12 总目标继续进行中。

## 完成项目

- 为相邻的 `GET_CONSTANT string -> SET_STACK/GET_STACK -> JUMP_IF` 与
  `GET_CONSTANT string -> SET_STACK/GET_STACK -> LOGICAL_NOT -> JUMP_IF_BOOL_FALSE`
  建立保守本地折叠。
- 新增 `backend_aot_c_constant_consumers.c`，抽出 null/bool/string 常量消费者证明，并加入
  string stack-copy consumed-by-local 判定。
- `GET_CONSTANT string` 在只被本地栈复制真值链消费时跳过源 value-slot 写入。
- `SET_STACK`/`GET_STACK` 在只被本地 string truthiness 消费时跳过 `CopyStack`。
- `JUMP_IF` 对空串/非空串生成 `zr_aot_generic_jump_if_string_stack_copy_false/true`。
- `LOGICAL_NOT` 对空串/非空串生成
  `zr_aot_generic_logical_not_string_stack_copy_local` 和直接 bool 赋值。
- frame descriptor 将该相邻链路的 GET_CONSTANT、SET_STACK/GET_STACK、JUMP_IF、LOGICAL_NOT
  统一标为 local-only。

## RED/GREEN

- RED：新增 stack-copy string `JUMP_IF` smoke 缺少
  `zr_aot_string_constant_stack_copy_local_jump_if_constant_skip slot=3`、
  `zr_aot_string_constant_stack_copy_local_jump_if_source_skip dstSlot=5 srcSlot=3` 和
  `zr_aot_generic_jump_if_string_stack_copy_false/true`。
- RED：新增 stack-copy string `LOGICAL_NOT` smoke 缺少
  `zr_aot_string_constant_stack_copy_local_logical_not_constant_skip slot=4`、
  `zr_aot_string_constant_stack_copy_local_logical_not_source_skip dstSlot=5 srcSlot=4` 和
  `zr_aot_generic_logical_not_string_stack_copy_local`。
- RED 生成 C 仍包含 targeted `CopyConstant`、`CopyStack`、
  `GenericPrimitiveIsTruthy` 或 `GenericPrimitiveLogicalNot`。
- GREEN：空串/非空串经栈复制后分别折叠到直接分支或直接 bool 赋值。
- GREEN 目标项目不再包含 targeted source value-slot 写入、`CopyStack`、temporary truthy bool、
  primitive truthiness runtime helper 或 bool sync。

## 验证

- WSL GCC：generic JUMP_IF 7/0、generic LOGICAL_NOT 6/0、logical contracts 4/0、
  logical shared-library 6/0、generic equality 4/0、frame setup contracts 1/0、
  control contracts 2/0、control shared-library 2/0。
- WSL Clang：同组 7/0、6/0、4/0、6/0、4/0、1/0、2/0、2/0。
- Windows MSVC Debug：generic JUMP_IF 0 failures / 7 expected ignores；generic LOGICAL_NOT
  0 failures / 6 expected ignores；logical shared-library 0 failures / 6 expected ignores；
  generic equality 0 failures / 4 expected ignores；control shared-library 0 failures /
  2 expected ignores；logical/frame/control contracts 4/0、1/0、2/0。

## 仍未完成

- 动态 string slot truthiness。
- 一般 object truthiness。
- value-copy migration、GC roots/exports/frame cleanup、更广 byte-frame narrowing。
- 性能计数、完整 typed 函数体零 `SZrValue`/frame write 证明。
