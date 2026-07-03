# AOT 12-S7ZZG / 11-S7 Member Token Remap Sidecar Source Duplicate Guard

时间：2026-07-01 19:43:02 +08:00

## 状态

完成一个 12-S7 / 11-S7 generated zrp metadata remap sidecar 支撑子切片：`backend_aot_c_zrp_member_token_remap_build()`
现在会在生成 `SZrAotCEmbeddedZrpMetadata.memberTokenRemapEntries` 时拒绝重复 source member token，避免 generated C
在运行期 ABI validation 前发布一对多的 source-token remap 表。

完整 11-S7 / 12-S7 仍未关闭；cross-module provider binding、真实 export manifest/table rewrite/publication、完整
metadata sweep/pruning、完整 trim analyzer、annotation policy 和更完整的 runtime ABI drift/deopt coverage 仍待后续。

## 完成项目

- `backend_aot_c_zrp_member_token_remap_append()` 在写入新 entry 前扫描已写 entries。
- 重复 `sourceToken` 会让 sidecar build 失败，释放临时 entries，并保持 metadata remap 指针/count 为空。
- `test_aot_c_zrp_metadata_export_token_remap.c` 新增 method/field 共用同一 `MEMBER_DEF` source token 的拒绝用例。
- source contract 同步锁定 member-token sidecar helper 的 source-token duplicate 比较。

## RED/GREEN

- RED：新增 duplicate source sidecar 用例后，旧 builder 生成两条同 source token 的 remap entries，WSL GCC 失败
  `Expected FALSE Was TRUE`。
- GREEN：builder 在第二条 source token 重复时返回 false，并保持 `memberTokenRemapEntries` / `ownedMemberTokenRemapEntries`
  为 null、`memberTokenRemapCount` 为 0。

## 验证

- WSL GCC：direct export-token remap 4/0、source contracts 24/0。
- WSL Clang：direct export-token remap 4/0、source contracts 24/0。
- Windows MSVC Debug：direct export-token remap 4/0、source contracts 24/0。
- Registered CTest：`aot_c_zrp_metadata_export_token_remap` 在 WSL GCC、WSL Clang、Windows MSVC Debug 均为 1/1。
- `git diff --check`：exit 0，仅输出既有 LF/CRLF 提示。

## 备注

本切片只收紧生成侧 member-token remap sidecar 的 source-token 唯一性。它不声明跨模块 provider target resolution、
真实 export manifest/table rewrite/publication、attribute/annotation policy、完整 metadata sweep 或 full trim analyzer 已完成。
