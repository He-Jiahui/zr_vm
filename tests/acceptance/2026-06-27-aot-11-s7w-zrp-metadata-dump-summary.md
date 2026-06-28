# AOT 11-S7W / 12-S7ZQ zrp Metadata Dump Summary

时间：2026-06-27 07:48:22 +08:00

状态：11-S7 / 12-S7 支撑子切片完成；metadata diff、版本兼容检查、cross-module
export-token rewrite、field/default-value backed constant-pool remap 和完整 metadata sweep/pruning
仍待后续。

## 完成项目

- CLI 新增 `--dump-zrp-metadata <file>` 独立主模式，并在 parse 结果中保存 `zrpMetadataPath`。
- dump 模式拒绝与 run、compile、debug、output modifiers、passthrough args 混用，避免误把诊断工具当作运行路径。
- 新增 `ZrCli_ZrpMetadataDump_WriteSummary()` 和 `ZrCli_ZrpMetadataDump_RunPath()`，读取 `.zrp` 文件并通过
  `ZrCore_ZrpMetadata_ReadHeader()` / `ZrCore_ZrpMetadata_ValidateHeader()` 校验 header。
- summary 输出 `zrp.metadata.version/headerBytes/sectionCount`，以及 12 个 section 的
  `bytes/count/elementSize/offset`。

## RED/GREEN

- RED：`tests/cli/test_cli_args.c` 要求 `ZR_CLI_MODE_DUMP_ZRP_METADATA` 与 `zrpMetadataPath` 后，
  旧 CLI command 结构编译失败。
- RED：新增 `zr_vm_cli_zrp_metadata_dump_test` 目标后，CMake 因缺少
  `zr_vm_cli/src/zr_vm_cli/metadata/zrp_metadata_dump.c` 配置失败。
- GREEN：`zr_vm_cli_args_test` 和 `zr_vm_cli_zrp_metadata_dump_test` 通过。

## 验证

- WSL gcc：`zr_vm_cli_args_test`、`zr_vm_cli_zrp_metadata_dump_test` 通过；`zr_vm_cli_executable` 构建通过；
  focused CTest 2/2。
- WSL clang：同组通过；`zr_vm_cli_executable` 构建通过；focused CTest 2/2。
- Windows MSVC Debug：同组通过；`zr_vm_cli_executable` 构建通过；focused CTest 2/2。

CTest 过滤：
`cli_args|cli_zrp_metadata_dump`

## 备注

本切片只提供 standalone section summary dump，不声明 metadata diff、版本兼容检查、跨模块导出 token
重写、默认 metadata 保留策略或完整 metadata sweep/pruning 完成。
