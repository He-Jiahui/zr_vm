# AOT 07-S2/S4 Generic Mixed Stack-Copy Equality Local Value-Copy

- 时间：2026-07-04 11:12:07 +08:00
- 状态：完成本子切片；07~12 总目标继续进行中。

## 完成项目

- 为 `GET_CONSTANT i64 -> SET_STACK -> GET_CONSTANT u64 -> generic LOGICAL_EQUAL ->
  JUMP_IF_BOOL_FALSE` 建立保守本地 value-copy 路径。
- `SET_STACK` 在该场景直接生成 `zr_aot_s5 = zr_aot_s0;`，不再构造
  `SZrTypeValue` destination 或写 `frame.slotBase[5].value`。
- generic equality 识别 copied i64 与 u64 operand 是不同 numeric primitive kind，
  直接生成 `zr_aot_b2 = ZR_FALSE;`。
- 后续 typed bool branch 直接读取 `zr_aot_b2`。
- `backend_aot_c_scalar_locals.c` 允许 stack-copy generic equality consumer 在 copied
  kind 与另一个 operand 都是可证明 numeric primitive local 时保留 copied kind。
- `backend_aot_c_function_body.c` 对 mixed numeric scalar operands 建立 written-kind proof，
  让后续 generic equality 可证明已完整读取 scalar operands，从而不强制 value-slot 写回。

## RED/GREEN

- RED：新增 `generic_mixed_stack_copy_equality_local_project` 后，WSL GCC focused 失败
  `Expected Non-NULL`，缺少 `zr_aot_s5 = zr_aot_s0;` 和 mixed compare marker。
- GREEN：生成 C 包含 `zr_aot_scalar_stack_copy_i64 dstSlot=5 srcSlot=0`、
  `zr_aot_s5 = zr_aot_s0;`、`zr_aot_generic_mixed_primitive_compare_scalar_local`、
  `leftKind=s rightKind=u`、`zr_aot_b2 = ZR_FALSE;` 和 `if (!zr_aot_b2) {`。
- GREEN 目标项目不包含 targeted `GenericPrimitiveLogicalEqual`、`SyncBoolLocal`、
  `CopyStack` 或 `frame.slotBase[0/1/5].value`。
- GREEN 运行生成 shared library，入口返回 int 91。

## 验证

- WSL GCC：stack-copy equality 2/0、generic equality 5/0、generic JUMP_IF 9/0、
  generic LOGICAL_NOT 8/0、logical contracts 4/0。
- WSL Clang：同组 2/0、5/0、9/0、8/0、4/0。
- Windows MSVC Debug（`build-msvc-aot-stack-copy`）：stack-copy equality smoke
  0 failures / 2 expected ignores；generic equality smoke 0 failures / 5 expected ignores；
  logical contracts 4/0。

## 仍未完成

- 更广 value-copy migration。
- GC roots/exports/frame cleanup、更广 byte-frame narrowing。
- 性能计数、完整 typed 函数体零 `SZrValue`/frame write 证明。
