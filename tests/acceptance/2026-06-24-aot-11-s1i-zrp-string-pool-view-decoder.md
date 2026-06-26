# AOT 11-S1I zrp string pool view decoder

时间：2026-06-24 21:46:03 +08:00

## 状态

11-S1 子切片完成；完整 11-S1 仍未关闭。

## 完成项目

- 新增 `SZrZrpMetadataStringView`。
- 新增 `ZrCore_ZrpMetadata_GetString()`。
- 支持从已验证 zrp metadata buffer 的 string pool 中按 offset 解析 NUL-terminated 字符串 view。
- 返回的 `byteLength` 不包含终止 NUL。
- 拒绝 offset 越界、缺少终止 NUL、空输出指针，并在失败时清空输出 view。

## RED/GREEN

- RED：`tests/module/test_zrp_metadata_format.c` 新增 string pool view decoder 用例后，构建失败，缺少 `SZrZrpMetadataStringView` 与 `ZrCore_ZrpMetadata_GetString()`。
- GREEN：补齐 public API 与实现后，format 测试可解析 `Zr`、`VM` 与空字符串；offset 等于 pool 长度被拒绝；缺少终止 NUL 的 entry 被拒绝。

## 验证

- WSL gcc：`zr_vm_zrp_metadata_format_test` 10/0。
- WSL gcc CTest：`zrp_metadata_format|metadata_runtime_query|metadata_token_model` 3/3。
- WSL clang：`zr_vm_zrp_metadata_format_test` 10/0。
- WSL clang CTest：`zrp_metadata_format|metadata_runtime_query|metadata_token_model` 3/3。
- Windows MSVC CLI smoke：`hello_world` 输出 `hello world`。

## 未关闭范围

本切片只覆盖 string pool 的只读 view 解码入口，不声明 UTF-8 语义校验、字符串 intern、compiler 真实 string pool 导出、签名 blob 标准化解析、文件级 zrp manifest 读写或 dump/diff 工具完成。
