# AOT 07-S2/S4 Generic I64 Stack-Copy Equality Local Value-Copy

- 时间：2026-07-04 11:00:12 +08:00
- 状态：完成本子切片；07~12 总目标继续进行中。

## 完成项目

- 为 `GET_CONSTANT i64 -> SET_STACK -> generic LOGICAL_EQUAL -> JUMP_IF_BOOL_FALSE`
  建立保守本地 value-copy 路径。
- `SET_STACK` 在该场景直接生成 `zr_aot_s5 = zr_aot_s0;`，不再构造
  `SZrTypeValue` destination 或写 `frame.slotBase[5].value`。
- generic equality 继续使用同类 i64 scalar-local compare：
  `zr_aot_b2 = (TZrBool)((zr_aot_s5 == zr_aot_s1) != 0u);`。
- 后续 typed bool branch 直接读取 `zr_aot_b2`。
- `backend_aot_c_scalar_locals.c` 将 `slotKinds` 传入 stack-copy destination consumer 扫描，
  并只在 copied slot 与另一个 operand 都可证明为同类 primitive 时，把目的槽声明为对应 scalar local。
- `backend_aot_c_function_body.c` 在后续 generic equality 已能读取 scalar operands 时，不再强制
  stack-copy 写回 value slot。

## RED/GREEN

- RED：新增 `generic_i64_stack_copy_equality_local_project` 后，WSL GCC focused 失败
  `Expected Non-NULL`，缺少 `zr_aot_s5 = zr_aot_s0;`。
- RED 生成 C 已有 `zr_aot_generic_i64_compare_scalar_local`，但 stack-copy 仍写
  `frame.slotBase[5].value` 并通过 `zr_aot_s_value` 同步到 `zr_aot_s5`。
- GREEN：生成 C 包含 `zr_aot_scalar_stack_copy_i64 dstSlot=5 srcSlot=0`、
  `zr_aot_s5 = zr_aot_s0;`、`zr_aot_generic_i64_compare_scalar_local`、
  `zr_aot_b2 = (TZrBool)((zr_aot_s5 == zr_aot_s1) != 0u);` 和 `if (!zr_aot_b2) {`。
- GREEN 目标项目不包含 targeted `GenericPrimitiveLogicalEqual`、`SyncBoolLocal`、`CopyStack`、
  `frame.slotBase[0/1/5].value` 或 `ZrCore_Stack_GetValue(frame.slotBase + 0/1/5)`。
- GREEN 运行生成 shared library，入口返回 int 17。

## 验证

- WSL GCC：新增 stack-copy equality 1/0、generic equality 5/0、generic JUMP_IF 9/0、
  generic LOGICAL_NOT 8/0、logical contracts 4/0。
- WSL Clang：同组 1/0、5/0、9/0、8/0、4/0。
- Windows MSVC Debug（`build-msvc-aot-stack-copy`）：新增 stack-copy equality smoke
  0 failures / 1 expected ignore；generic equality smoke 0 failures / 5 expected ignores；
  logical contracts 4/0。

## 仍未完成

- 更广 value-copy migration。
- GC roots/exports/frame cleanup、更广 byte-frame narrowing。
- 性能计数、完整 typed 函数体零 `SZrValue`/frame write 证明。
