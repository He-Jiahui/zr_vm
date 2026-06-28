# AOT 12-S7ZO / 11-S7 zrp Section Delta Markers

时间：2026-06-27 06:51:55 +08:00

状态：12-S7 / 11-S7 支撑子切片完成；完整 trim analyzer、annotation-driven policy、
cross-module export-token rewrite、field/default-value backed constant-pool remap 和独立 dump/diff
工具仍待后续。

## 完成项目

- `backend_aot_write_code_stripping_zrp_metadata_size_deltas()` 继续输出既有 zrp metadata 总量、
  token-record、definition-table 和 pool before/after/removed marker。
- 新增 12 个 section 的 `code_stripping.zrpMetadataSectionBytes.<section>Before/After/Removed`
  marker：tokenRecords、typeDefs、methodDefs、fieldDefs、genericParams、
  genericParamConstraints、typeSpecs、methodSpecs、moduleRefs、stringPool、
  signatureBlobPool、constantPool。
- 新增 `tests/parser/test_aot_c_zrp_metadata_size_deltas.c`，直接调用 size-delta writer 并验证所有
  section marker 文本。
- `tests/CMakeLists.txt` 新增 `aot_c_zrp_metadata_size_deltas` CTest 入口，并在 Windows shared-DLL
  构建下直接链接私有 size 模块。

## RED/GREEN

- RED：新增 direct size-delta 测试后，旧实现缺少
  `code_stripping.zrpMetadataSectionBytes.tokenRecordsBefore`，WSL gcc 失败 1/1。
- GREEN：size-delta 1/0、source contracts 21/0、code stripping 5/0、direct zrp pruning 5/0、
  pool pruning 4/0、export-token remap 2/0。

## 验证

- WSL gcc：同组可执行测试通过；focused CTest 5/5。
- WSL clang：同组可执行测试通过；focused CTest 5/5。
- Windows MSVC Debug：同组可执行测试通过；focused CTest 5/5。

CTest 过滤：
`aot_c_zrp_metadata_size_deltas|aot_c_zrp_metadata_export_token_remap|aot_c_zrp_metadata_pruning|aot_c_zrp_metadata_pool_pruning|aot_c_code_stripping`

## 备注

本切片只补 generated-C 注释级 section delta 可观测性，不声明完整 metadata sweep、真实跨模块
metadata publication 或 dump/diff 工具完成。
