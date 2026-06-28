# AOT 12-S7ZP / 11-S7 zrp Section Count Delta Markers

时间：2026-06-27 07:20:00 +08:00

状态：12-S7 / 11-S7 支撑子切片完成；完整 trim analyzer、annotation-driven policy、
cross-module export-token rewrite、field/default-value backed constant-pool remap 和独立 dump/diff
工具仍待后续。

## 完成项目

- `SZrAotZrpMetadataSizeStats` 新增 12 个 zrp metadata section count 字段，来源为 header
  section directory 的 `count`。
- `backend_aot_write_zrp_metadata_size_stats()` 新增
  `aot_size.zrpMetadataSectionCounts.<section>` marker。
- `backend_aot_write_code_stripping_zrp_metadata_size_deltas()` 新增
  `code_stripping.zrpMetadataSectionCounts.<section>Before/After/Removed` marker。
- `tests/parser/test_aot_c_zrp_metadata_size_deltas.c` 扩展为 2 个 direct 测试，覆盖 section byte
  delta、section count stats 与 section count delta。

## RED/GREEN

- RED：direct size-delta 测试新增 section count marker 后，旧 `SZrAotZrpMetadataSizeStats`
  缺少 count 字段，WSL gcc 编译失败。
- GREEN：size-delta 2/0、source contracts 21/0、code stripping 5/0、direct zrp pruning 5/0、
  pool pruning 4/0、export-token remap 2/0。

## 验证

- WSL gcc：同组可执行测试通过；focused CTest 5/5。
- WSL clang：同组可执行测试通过；focused CTest 5/5。
- Windows MSVC Debug：同组可执行测试通过；focused CTest 5/5。

CTest 过滤：
`aot_c_zrp_metadata_size_deltas|aot_c_zrp_metadata_export_token_remap|aot_c_zrp_metadata_pruning|aot_c_zrp_metadata_pool_pruning|aot_c_code_stripping`

## 备注

本切片只补 generated-C 注释级 section count 可观测性，不改变 `.zrp` ABI，也不声明完整
metadata sweep、真实跨模块 metadata publication 或 dump/diff 工具完成。
