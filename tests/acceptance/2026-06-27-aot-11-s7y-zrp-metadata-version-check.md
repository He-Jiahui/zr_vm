# AOT 11-S7Y / 12-S7ZS zrp Metadata Version Check

时间：2026-06-27 08:35:30 +08:00

状态：11-S7 / 12-S7 支撑子切片完成；11-S6 runtime ABI 漂移 deopt/拒绝、cross-module
export-token rewrite、field/default-value backed constant-pool remap、完整 metadata sweep/pruning
和完整 trim analyzer 仍待后续。

## 完成项目

- CLI 新增 `--check-zrp-metadata-version <file>` 独立主模式，并在 parse 结果中保存
  `zrpMetadataVersionCheckPath`。
- version-check 模式拒绝与 run、compile、debug、output modifiers、passthrough args 混用，并在 help
  文本中列出。
- 新增 `ZrCli_ZrpMetadataDump_WriteVersionCheck()` 和
  `ZrCli_ZrpMetadataDump_RunVersionCheckPath()`，读取 `.zrp` 文件头前 16 字节并报告
  actual/expected header shape。
- version check 输出 `zrp.metadata.versionCheck.status`、magic、version、headerBytes、
  sectionCount 及其 expected 值。当前 header shape 经完整 header 校验后返回 `ok`；版本或头形状不匹配时
  输出 `unsupported` 并以失败码退出。

## RED/GREEN

- RED：`tests/cli/test_cli_args.c` 要求 `ZR_CLI_MODE_CHECK_ZRP_METADATA_VERSION` 与
  `zrpMetadataVersionCheckPath` 后，旧 CLI command 结构编译失败。
- RED：`tests/cli/test_cli_zrp_metadata_dump.c` 要求 version-check summary/path API 后，旧 metadata
  dump 模块链接失败。
- GREEN：`zr_vm_cli_args_test` 和 `zr_vm_cli_zrp_metadata_dump_test` 通过。

## 验证

- WSL gcc：`zr_vm_cli_args_test`、`zr_vm_cli_zrp_metadata_dump_test` 通过；`zr_vm_cli_executable`
  构建通过；`zr_vm_cli --help` 覆盖新增 version-check 帮助文本；focused CTest 2/2。
- WSL clang：同组通过；`zr_vm_cli_executable` 构建通过；focused CTest 2/2。
- Windows MSVC Debug：同组通过；`zr_vm_cli_executable` 构建通过；`zr_vm_cli.exe --help`
  覆盖新增 version-check 帮助文本；focused CTest 2/2。

CTest 过滤：
`cli_args|cli_zrp_metadata_dump`

## 备注

本切片只提供 standalone zrp metadata header version/shape check，不声明 11-S6 runtime binding 的 ABI
漂移 deopt/拒绝路径、跨模块导出 token 重写、默认 metadata 保留策略、constant default-value remap
或完整 metadata sweep/pruning 完成。
