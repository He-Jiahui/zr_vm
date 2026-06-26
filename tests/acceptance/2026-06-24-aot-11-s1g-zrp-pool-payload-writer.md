# AOT 11-S1G zrp pool payload writer

时间：2026-06-24 21:25:01 +08:00

## 状态

11-S1 子切片完成；完整 11-S1 仍未关闭。

## 完成项目

- 新增 `ZrCore_ZrpMetadata_WritePoolPayload()`。
- 支持向已验证的 zrp metadata buffer 写入完整 string pool、signature blob pool、constant pool payload。
- 拒绝非 pool section、非空 payload 空指针、payload 长度与 section `byteLength`/`count` 不一致、截断 buffer。
- 允许 0 长度空 pool 使用 `ZR_NULL` payload no-op 写入。

## RED/GREEN

- RED：`tests/module/test_zrp_metadata_format.c` 新增 pool payload writer 用例后，构建在链接阶段失败，缺少 `ZrCore_ZrpMetadata_WritePoolPayload`。
- GREEN：补齐 public API 与实现后，format 测试可写入 string/signature pool 并通过 `GetPoolSlice()` 读回；空 constant pool 写入合法；非 pool、空 payload、长度不一致与截断 buffer 均被拒绝。

## 验证

- WSL gcc：`zr_vm_zrp_metadata_format_test` 8/0。
- WSL gcc CTest：`zrp_metadata_format|metadata_runtime_query|metadata_token_model` 3/3。
- WSL clang：`zr_vm_zrp_metadata_format_test` 8/0。
- WSL clang CTest：`zrp_metadata_format|metadata_runtime_query|metadata_token_model` 3/3。
- Windows MSVC CLI smoke：`hello_world` 输出 `hello world`。

## 未关闭范围

本切片只覆盖三类 byte pool 的完整 payload 写入入口，不声明定义表内容导出、字符串解码、签名 blob 标准化解析、文件级 zrp manifest 读写或 dump/diff 工具完成。
