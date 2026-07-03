# AOT 11-S7ZSF / 12-S7ZZW - `.zrp` manifest export unbound declaration publication

时间：2026-07-03 02:04:51 +08:00

## 范围

本切片关闭 unbound manifest export declaration 的文件级持久化策略：没有 type/member token binding 的
type/method/field export declaration 会进入 `.zrp` `manifestExports` section，并以 `flags = 0`、`typeToken = 0`、
`memberToken = 0` 表示。既有持久 unbound rows 在 pruning/rebuild 时保留，且只 remap target string offset。

## RED

- `test_zrp_metadata_manifest_exports_roundtrip_and_validate_token_shapes` 增加 unbound type/method/field rows 后，
  WSL GCC `zrp_metadata_format` 失败：旧 validator 对 zero-token rows 返回 false。
- `test_aot_c_zrp_metadata_pruning_publishes_unbound_manifest_export_declarations_as_rows` 增加未绑定 declaration
  publication 覆盖后，旧 prepared blob 仍为 708 bytes，而预期追加 `api.dynamic`、`api.DynamicType`、`api.value`
  rows/string 后为 806 bytes。

## GREEN

- `.zrp` manifest export row validation 接受合法 kind 且 flags/tokens 全 0 的 unbound rows；unknown flags、
  kind/token mismatch、未设置 flag 却携带 token 仍 fail closed。
- `backend_aot_c_zrp_copy_manifest_exports()` 保留 existing unbound rows 并继续 remap target string offsets。
- `backend_aot_c_zrp_publish_manifest_export_declarations()` 追加 unbound type/method/field declarations 为持久 rows，
  target string 复用或追加到 string pool，token flags/fields 保持 0。

## 验证

- WSL GCC focused CTest `zrp_metadata_format|aot_c_zrp_metadata|aot_c_code_stripping|aot_c_guardrail_contracts`：8/8。
- WSL clang 同组：8/8。
- Windows MSVC Debug 同组：8/8。

## 未关闭范围

本记录不声明完整 11-S7、12-S7、07~12 总目标、完整 metadata sweep/pruning、compacted-token file publication、
full trim analyzer、annotation/promotion policy 或更完整 ABI drift/deopt 闭环完成。Runtime manifest export binding
gate 仍只把 token-bound export view 用作 ABI drift 校验。
