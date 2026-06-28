# AOT 12-S7ZN / 11-S7 export member-token remap surface

- 时间：2026-06-27 06:30:32 +08:00
- 状态：子切片完成；完整 12-S7 / 11-S7 仍未关闭。
- 范围：emitted zrp metadata pruning 后的 exported `MEMBER_DEF` token remap surface。

## 完成项目

- `backend_aot_c_zrp_metadata_remap.{h,c}` 新增
  `backend_aot_c_zrp_remap_export_member_token()`。
- 保留 exported MethodDef token 会映射到 compacted MethodDef RID。
- FieldDef export token 会按 retained MethodDef count 后移到 compacted FieldDef RID。
- 已被 MethodDef pruning 删除的 exported method token 返回 false，供后续跨模块 export
  manifest/table 写回时拒绝或诊断。
- 新增 focused 测试 `tests/parser/test_aot_c_zrp_metadata_export_token_remap.c`，避免继续扩大
  已接近 1000 行的 direct zrp pruning 测试文件。

## RED/GREEN

- RED：先在 direct pruning 测试中要求导出方法旧 RID2 在 RID1/RID3 删除后映射到 compacted
  RID1；旧实现缺少 `backend_aot_c_zrp_remap_export_member_token()`，WSL gcc 链接失败。
- GREEN：新增独立 export-token remap 测试后，export-token remap 2/0、direct zrp pruning 5/0、
  pool pruning 4/0、code stripping 5/0、source contracts 21/0。

## 验证

- WSL gcc：同组可执行测试通过；focused CTest
  `aot_c_zrp_metadata_export_token_remap|aot_c_zrp_metadata_pruning|aot_c_zrp_metadata_pool_pruning|aot_c_code_stripping`
  4/4。
- WSL clang：同组可执行测试通过；focused CTest 4/4。
- Windows MSVC Debug：同组可执行测试通过；focused CTest 4/4。

## 备注

本切片只提供 pruning 后导出 member token remap surface；尚未把 remap 写回跨模块 `.zrp`
export manifest/table，也不声明完整 metadata sweep、annotation-driven metadata policy、constant
literal/default-value remap 或 dump/diff 工具完成。
