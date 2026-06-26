# AOT 11-S7C zrp project manifest legacy identity/schema parity

时间：2026-06-24 22:39:26 +08:00

## 状态

11-S7 子切片完成；完整 11-S7 仍未关闭。

## 完成项目

- `.zrp` project loader 对 compatibility mapping 的 top-level `name` 使用 assembly-name shape 校验。
- 非 string/null 或含空白、路径分隔符、`@`、`$`、非法点段的 legacy `name` 会拒绝 manifest。
- top-level `version` 存在但不是 string/null 时拒绝 manifest；合法 legacy version 仍参与 assembly identity。
- 缺省 assembly identity 字段保持 version `0.0.0`、culture `neutral`、kind `library`、无 public key token。
- `zrp.schema.json` 同步收紧 `manifestVersion` 只允许 1、legacy `name` pattern、`publicKeyToken` hex/null 与 `kind` enum。
- `zr_vm_project_manifest_normalization_test` 从 5 个用例扩展到 8 个用例。

## RED/GREEN

- RED：新增 legacy identity/defaults 用例后，测试 7 个用例中 1 个失败：`name: "app render"` 被接受。
- GREEN：补齐 legacy `name` shape 校验后，manifest normalization 测试 8/0；identity defaults 与 legacy version guard 均保持通过。

## 验证

- WSL gcc：`zr_vm_project_manifest_normalization_test` 8/0。
- WSL gcc：`zr_vm_project_import_resolver_test` 9/0。
- WSL clang：`zr_vm_project_manifest_normalization_test` 8/0。
- WSL clang：`zr_vm_project_import_resolver_test` 9/0。
- Schema syntax：`python3 -m json.tool zr_vm_language_server_extension/schemas/zrp.schema.json` 通过。
- Windows MSVC CLI smoke：`hello_world` 输出 `hello world`。

## 未关闭范围

本切片只覆盖 `.zrp` manifest Layer 1 identity/schema parity，不声明 preserve 规则 DSL、按 symbol/token 保留、AOT mode、runtime binding 诊断、默认最小 metadata 策略或 dump/diff 工具完成。MSVC/clang/gcc 仍报告 `project.c` 既有 const qualifier warning，本切片未扩大处理范围。
