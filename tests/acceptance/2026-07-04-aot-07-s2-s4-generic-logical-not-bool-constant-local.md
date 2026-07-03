# AOT 07-S2/S4 Generic LOGICAL_NOT Bool-Constant Local Fold

- 时间：2026-07-04 05:23:13 +08:00
- 状态：完成本子切片；07~12 总目标继续进行中。

## 完成项目

- 新增 `backend_aot_c_bool_constant_consumed_by_local_logical_not()`，把 immediate
  `GET_CONSTANT bool -> LOGICAL_NOT` 的证明集中到同一 helper。
- 常量发射在该形态下输出 `zr_aot_bool_constant_local_logical_not_source_skip`，跳过源 bool
  local/value-slot 写入。
- generic `LOGICAL_NOT` 对 true 常量直接生成 `zr_aot_bD = ZR_FALSE;`，对 false 常量直接生成
  `zr_aot_bD = ZR_TRUE;`，并打上 `zr_aot_generic_logical_not_bool_constant_local` 标记。
- frame descriptor 复用同一证明，允许源 bool 常量和对应 `LOGICAL_NOT` 走 local-only。
- 普通 bool-source generic `LOGICAL_NOT` fixture 改为 `LOGICAL_NOT_BOOL -> LOGICAL_NOT`，保留
  `zr_aot_generic_logical_not_scalar_local` 覆盖。

## RED/GREEN

- RED：新增 bool-constant shared-library smoke 后，WSL GCC focused 失败在缺少
  `zr_aot_bool_constant_local_logical_not_source_skip`；逻辑源码契约也缺少新 helper。
- GREEN：生成 C 包含 source-skip marker、`zr_aot_generic_logical_not_bool_constant_local`
  和 `zr_aot_b1 = ZR_FALSE;`，且不包含 `zr_aot_scalar_constant_bool_local`、
  `GenericPrimitiveLogicalNot(state, &frame, 1, 0)`、`SyncBoolLocal(state, &frame, 1)`、
  `frame.slotBase[0].value`。

## 验证

- WSL GCC：generic JUMP_IF 6/0、generic LOGICAL_NOT 4/0、logical shared-library 6/0、
  generic equality 4/0、logical contracts 4/0、frame setup contracts 1/0、control contracts 2/0。
- WSL Clang：同组 6/0、4/0、6/0、4/0、4/0、1/0、2/0。
- Windows MSVC Debug：focused JUMP_IF 0 failures / 6 expected Unix-only ignores；focused
  LOGICAL_NOT 0 failures / 4 expected Unix-only ignores；logical shared-library 0 failures /
  6 expected Unix-only ignores；logical/frame/control contracts 4/0、1/0、2/0。
- `git diff --check`：仅报告仓库既有 LF/CRLF 提示。

## 仍未完成

- dynamic/string/object truthiness。
- value-copy migration、GC roots/exports/frame cleanup、更广 byte-frame narrowing。
- 性能计数、完整 typed 函数体零 `SZrValue`/frame write 证明。
