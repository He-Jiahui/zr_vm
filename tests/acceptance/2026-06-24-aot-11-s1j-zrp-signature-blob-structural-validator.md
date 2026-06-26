# AOT 11-S1J zrp signature blob structural validator

时间：2026-06-24 21:56:34 +08:00

## 状态

11-S1 子切片完成；完整 11-S1 仍未关闭。

## 完成项目

- 新增 `ZrCore_ZrpMetadata_ValidateSignatureBlob()`。
- 支持 method signature 与 field signature root 的边界安全结构校验。
- 支持 primitive、type ref、type def、array、tuple、generic inst、ownership、union、nullable、module、member ref、assembly ref 等常用 type node 的递归结构校验。
- 要求 param count、type arg count、tuple count 等递归计数与 payload 边界一致，且整个 blob 被精确消费。
- 拒绝 null blob、空 blob、截断 payload、未知 node、非法 root、嵌套 method/field signature 与尾随字节。

## RED/GREEN

- RED：`tests/module/test_zrp_metadata_format.c` 新增 signature blob structural validator 用例后，构建在链接阶段失败，缺少 `ZrCore_ZrpMetadata_ValidateSignatureBlob`。
- GREEN：补齐 public API 与实现后，合法 method signature、field signature、generic inst type signature 均通过；null/empty blob、field signature 尾随字节、截断 method signature 与未知 node 均被拒绝。

## 验证

- WSL gcc：`zr_vm_zrp_metadata_format_test` 11/0。
- WSL gcc CTest：`zrp_metadata_format|metadata_runtime_query|metadata_token_model` 3/3。
- WSL clang：`zr_vm_zrp_metadata_format_test` 11/0。
- WSL clang CTest：`zrp_metadata_format|metadata_runtime_query|metadata_token_model` 3/3。
- Windows MSVC CLI smoke：`hello_world` 输出 `hello world`。

## 未关闭范围

本切片只覆盖 signature blob 的结构层校验，不声明 token/type/string 语义解析、compiler 真实 signature pool 导出、文件级 zrp manifest 读写或 dump/diff 工具完成。
