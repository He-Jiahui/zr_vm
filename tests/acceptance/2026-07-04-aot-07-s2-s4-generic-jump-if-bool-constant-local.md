# AOT 07-S2/S4 Generic JUMP_IF Bool-Constant Direct Branch

- 时间：2026-07-04 04:51:23 +08:00
- 状态：完成本子切片；07~12 总目标继续进行中。

## 完成项目

- 新增 `backend_aot_c_bool_constant_consumed_by_local_jump_if()`，把 immediate
  `GET_CONSTANT bool -> JUMP_IF` 的证明集中到同一 helper。
- 常量发射在该形态下输出 `zr_aot_bool_constant_local_jump_if_source_skip`，跳过
  `zr_aot_scalar_constant_bool_local` 源写入。
- generic `JUMP_IF` 对 false 常量生成 `zr_aot_generic_jump_if_bool_constant_false`
  和直接 `goto`，对 true 常量生成 `zr_aot_generic_jump_if_bool_constant_true` 后直接落下。
- frame descriptor 复用同一证明，允许源 bool 常量和对应 `JUMP_IF` 走 local-only。
- 原 bool scalar-local `JUMP_IF` fixture 改为 `GET_CONSTANT -> SET_STACK -> JUMP_IF`，
  保留 `zr_aot_scalar_stack_copy_bool` 与 `zr_aot_generic_jump_if_bool_scalar_local` 覆盖。

## RED/GREEN

- RED：新增 bool-constant shared-library smoke 后，WSL GCC focused 失败在缺少
  `zr_aot_bool_constant_local_jump_if_source_skip`。
- GREEN：生成 C 包含 source-skip marker、`zr_aot_generic_jump_if_bool_constant_false`
  和 `goto zr_aot_fn_0_ins_4;`，且不包含 `zr_aot_scalar_constant_bool_local`、
  `GenericPrimitiveIsTruthy(state, &frame, 0)`、`TZrBool zr_aot_truthy = ZR_FALSE;`、
  `frame.slotBase[0].value`。

## 验证

- WSL GCC：generic JUMP_IF 6/0、generic LOGICAL_NOT 3/0、logical shared-library 6/0、
  generic equality 4/0、logical contracts 4/0、frame setup contracts 1/0、control contracts 2/0。
- WSL Clang：同组 6/0、3/0、6/0、4/0、4/0、1/0、2/0。
- Windows MSVC Debug：focused JUMP_IF 0 failures / 6 expected Unix-only ignores；
  logical/frame/control contracts 4/0、1/0、2/0。
- `git diff --check`：仅报告仓库既有 LF/CRLF 提示。

## 仍未完成

- dynamic/string/object truthiness。
- value-copy migration、GC roots/exports/frame cleanup、更广 byte-frame narrowing。
- 性能计数、完整 typed 函数体零 `SZrValue`/frame write 证明。
