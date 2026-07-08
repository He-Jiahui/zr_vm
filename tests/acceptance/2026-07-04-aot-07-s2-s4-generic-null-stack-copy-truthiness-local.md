# AOT 07-S2/S4 Generic Null Stack-Copy Truthiness Local Fold

- 时间：2026-07-04 08:14:28 +08:00
- 状态：完成本子切片；07~12 总目标继续进行中。

## 完成项目

- 为相邻的 `GET_CONSTANT null -> SET_STACK/GET_STACK -> JUMP_IF` 与
  `GET_CONSTANT null -> SET_STACK/GET_STACK -> LOGICAL_NOT -> JUMP_IF_BOOL_FALSE`
  建立保守本地折叠。
- `backend_aot_c_constant_consumers.c` 新增 null stack-copy consumed-by-local 判定。
- `GET_CONSTANT null` 在只被本地栈复制真值链消费时跳过源 value-slot 写入。
- `SET_STACK`/`GET_STACK` 在只被本地 null truthiness 消费时跳过 `CopyStack`。
- `JUMP_IF` 对 null stack-copy 生成 `zr_aot_generic_jump_if_null_stack_copy_false`
  和直接 false successor `goto`。
- `LOGICAL_NOT` 对 null stack-copy 生成
  `zr_aot_generic_logical_not_null_stack_copy_local` 和 `zr_aot_bD = ZR_TRUE;`。
- frame descriptor 将该相邻链路的 GET_CONSTANT、SET_STACK/GET_STACK、JUMP_IF、LOGICAL_NOT
  统一标为 local-only。

## RED/GREEN

- RED：新增 stack-copy null `JUMP_IF` smoke 缺少
  `zr_aot_null_constant_stack_copy_local_jump_if_constant_skip slot=2`、
  `zr_aot_null_constant_stack_copy_local_jump_if_source_skip dstSlot=3 srcSlot=2` 和
  `zr_aot_generic_jump_if_null_stack_copy_false`。
- RED：新增 stack-copy null `LOGICAL_NOT` smoke 缺少
  `zr_aot_null_constant_stack_copy_local_logical_not_constant_skip slot=3`、
  `zr_aot_null_constant_stack_copy_local_logical_not_source_skip dstSlot=4 srcSlot=3` 和
  `zr_aot_generic_logical_not_null_stack_copy_local`。
- RED 生成 C 仍包含 targeted `CopyConstant`、`CopyStack`、
  `GenericPrimitiveIsTruthy`、`GenericPrimitiveLogicalNot` 或 `SyncBoolLocal`。
- GREEN：null 经栈复制后分别折叠到直接 false 分支或直接 `ZR_TRUE` bool 赋值。
- GREEN 目标项目不再包含 targeted source value-slot 写入、`CopyStack`、temporary truthy bool、
  primitive truthiness runtime helper、bool sync 或相关 `frame.slotBase` 访问。

## 验证

- WSL GCC：generic JUMP_IF 7/0、generic LOGICAL_NOT 6/0、logical contracts 4/0、
  logical shared-library 6/0、generic equality 4/0、frame setup contracts 1/0、
  control contracts 2/0、control shared-library 2/0。
- Windows MSVC Debug：generic JUMP_IF 0 failures / 7 expected ignores；generic LOGICAL_NOT
  0 failures / 6 expected ignores；logical shared-library 0 failures / 6 expected ignores；
  generic equality 0 failures / 4 expected ignores；control shared-library 0 failures /
  2 expected ignores；logical/frame/control contracts 4/0、1/0、2/0。
- `git diff --check`：退出码 0，仅报告 LF/CRLF 提示。
- WSL Clang：本轮重跑未完成。清理上一次超时留下的 0-byte
  `build-wsl-clang/lib/libzr_vm_parser.so` 后，`zr_vm_parser_shared` relink 被当前工作区无关
  type-inference 重复定义阻塞：
  `type_inference_bitwise_identity_direct_range.c` 与未跟踪
  `type_inference_bitwise_identity_bitwise_not_range.c` 同时定义同名 bitwise-not range 函数。

## 仍未完成

- reset-null stack-copy truthiness。
- 动态 string slot truthiness。
- 一般 object truthiness。
- value-copy migration、GC roots/exports/frame cleanup、更广 byte-frame narrowing。
- 性能计数、完整 typed 函数体零 `SZrValue`/frame write 证明。
