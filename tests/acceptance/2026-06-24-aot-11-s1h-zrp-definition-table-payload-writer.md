# AOT 11-S1H zrp definition table payload writer

时间：2026-06-24 21:33:41 +08:00

## 状态

11-S1 子切片完成；完整 11-S1 仍未关闭。

## 完成项目

- 新增 `ZrCore_ZrpMetadata_WriteDefinitionTablePayload()`。
- 支持向已验证的 zrp metadata buffer 写入完整 TypeDef、MethodDef、FieldDef、GenericParam、GenericParamConstraint、TypeSpec、MethodSpec、ModuleRef row payload。
- 拒绝非 definition-table section、非空 row payload 空指针、row count 与 section `count` 不一致、element size 与 section `elementSize` 不一致、截断 buffer。

## RED/GREEN

- RED：`tests/module/test_zrp_metadata_format.c` 新增 definition-table payload writer 用例后，构建在链接阶段失败，缺少 `ZrCore_ZrpMetadata_WriteDefinitionTablePayload`。
- GREEN：补齐 public API 与实现后，format 测试可写入 TypeDef/MethodDef payload 并通过 `GetSectionView()` 读回；合法行通过 `ValidateDefinitionTables()`；非表 section、空 row payload、count 不一致、element size 不一致与截断 buffer 均被拒绝。

## 验证

- WSL gcc：`zr_vm_zrp_metadata_format_test` 9/0。
- WSL gcc CTest：`zrp_metadata_format|metadata_runtime_query|metadata_token_model` 3/3。
- WSL clang：`zr_vm_zrp_metadata_format_test` 9/0。
- WSL clang CTest：`zrp_metadata_format|metadata_runtime_query|metadata_token_model` 3/3。
- Windows MSVC CLI smoke：`hello_world` 输出 `hello world`。

## 未关闭范围

本切片只覆盖 definition-table row payload 的格式层写入入口，不声明 compiler 已从真实 symbol/type/function metadata 导出定义表内容，也不声明字符串解码、签名 blob 标准化解析、文件级 zrp manifest 读写或 dump/diff 工具完成。
