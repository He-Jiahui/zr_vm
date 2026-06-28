# AOT 11-S7X / 12-S7ZR zrp Metadata Diff Summary

时间：2026-06-27 08:14:35 +08:00

状态：11-S7 / 12-S7 支撑子切片完成；版本兼容检查、cross-module export-token rewrite、
field/default-value backed constant-pool remap、完整 metadata sweep/pruning 和完整 trim analyzer
仍待后续。

## 完成项目

- CLI 新增 `--diff-zrp-metadata <before> <after>` 独立主模式，并在 parse 结果中保存
  `zrpMetadataBeforePath` 与 `zrpMetadataAfterPath`。
- diff 模式拒绝与 run、compile、debug、output modifiers、passthrough args 混用，并在 help 文本中列出。
- 新增 `ZrCli_ZrpMetadataDump_WriteDiffSummary()` 和 `ZrCli_ZrpMetadataDump_RunDiffPath()`，读取两个
  `.zrp` 文件并校验各自 `SZrZrpMetadataHeader`。
- diff summary 输出 version/headerBytes/sectionCount 的 before/after，以及 12 个 section 的
  `bytes/count before/after/removed`，并保留 elementSize/offset 对照。after 大于 before 时
  removed 计数归零，避免无符号下溢。

## RED/GREEN

- RED：`tests/cli/test_cli_args.c` 要求 `ZR_CLI_MODE_DIFF_ZRP_METADATA` 与 before/after path 字段后，
  旧 CLI command 结构编译失败。
- RED：`tests/cli/test_cli_zrp_metadata_dump.c` 要求 diff summary/path API 后，旧 metadata dump 模块链接失败。
- GREEN：`zr_vm_cli_args_test` 和 `zr_vm_cli_zrp_metadata_dump_test` 通过。

## 验证

- WSL gcc：`zr_vm_cli_args_test`、`zr_vm_cli_zrp_metadata_dump_test` 通过；`zr_vm_cli_executable` 构建通过；
  `zr_vm_cli --help` 覆盖新增 diff 帮助文本；focused CTest 2/2。
- WSL clang：同组通过；`zr_vm_cli_executable` 构建通过；focused CTest 2/2。
- Windows MSVC Debug：同组通过；`zr_vm_cli_executable` 构建通过；focused CTest 2/2。

CTest 过滤：
`cli_args|cli_zrp_metadata_dump`

## 备注

本切片只提供 standalone section byte/count diff summary，不声明版本兼容检查、跨模块导出 token
重写、默认 metadata 保留策略、constant default-value remap 或完整 metadata sweep/pruning 完成。
