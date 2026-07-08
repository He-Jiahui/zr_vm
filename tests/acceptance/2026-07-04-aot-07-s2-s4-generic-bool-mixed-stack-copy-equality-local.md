# AOT 07-S2/S4 Generic Bool/Mixed Stack-Copy Equality Local Value-Copy

- 时间：2026-07-04 11:26:29 +08:00
- 状态：完成本子切片；07~12 总目标继续进行中。

## 完成项目

- 为 `GET_CONSTANT bool -> SET_STACK -> generic LOGICAL_EQUAL -> JUMP_IF_BOOL_FALSE`
  增加 same-kind bool stack-copy equality guardrail。
- 为 `GET_CONSTANT bool -> SET_STACK -> GET_CONSTANT int -> generic LOGICAL_EQUAL ->
  JUMP_IF_BOOL_FALSE` 建立 bool/numeric mixed primitive 本地 value-copy 路径。
- `SET_STACK` 在该场景直接生成 `zr_aot_b5 = (TZrBool)(zr_aot_b0 != 0u);`，
  不再构造 `SZrTypeValue` destination 或写 `frame.slotBase[5].value`。
- generic equality 可在 bool/numeric kind 不同时直接生成 `zr_aot_b2 = ZR_FALSE;`。
- `backend_aot_c_scalar_locals.c` 将 BOOL 纳入 stack-copy generic equality copied/other
  primitive proof。
- `backend_aot_c_function_body.c` 将后续 generic equality scalar-read proof 从 numeric
  扩展为 bool/i64/u64/f64 primitive kind。

## RED/GREEN

- same-kind bool stack-copy equality guardrail 在当前实现上已 GREEN，保留为回归覆盖。
- RED：新增 `generic_bool_numeric_stack_copy_equality_local_project` 后，WSL GCC focused
  失败 `Expected Non-NULL`，缺少 direct bool stack-copy assignment 和 mixed compare marker。
- GREEN：生成 C 包含 `zr_aot_scalar_stack_copy_bool dstSlot=5 srcSlot=0`、
  `zr_aot_b5 = (TZrBool)(zr_aot_b0 != 0u);`、
  `zr_aot_generic_mixed_primitive_compare_scalar_local`、`leftKind=b rightKind=s`、
  `zr_aot_b2 = ZR_FALSE;` 和 `if (!zr_aot_b2) {`。
- GREEN 目标项目不包含 targeted `GenericPrimitiveLogicalEqual`、`SyncBoolLocal`、
  `CopyStack` 或 `frame.slotBase[0/1/5].value`。
- GREEN 运行生成 shared library，bool/numeric equality false branch 返回 int 91。

## 验证

- WSL GCC：stack-copy equality 4/0、generic equality 5/0、generic JUMP_IF 9/0、
  generic LOGICAL_NOT 8/0、logical contracts 4/0。
- WSL Clang：同组 4/0、5/0、9/0、8/0、4/0。
- Windows MSVC Debug（`build-msvc-aot-stack-copy`）：stack-copy equality smoke
  0 failures / 4 expected ignores；generic equality smoke 0 failures / 5 expected ignores；
  logical contracts 4/0。

## 仍未完成

- 更广 value-copy migration。
- GC roots/exports/frame cleanup、更广 byte-frame narrowing。
- 性能计数、完整 typed 函数体零 `SZrValue`/frame write 证明。
