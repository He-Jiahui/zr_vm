# AOT 11-S7B zrp project manifest publicKeyToken normalization

时间：2026-06-24 22:26:53 +08:00

## 状态

11-S7 子切片完成；完整 11-S7 仍未关闭。

## 完成项目

- `.zrp` project loader 对 `assembly.publicKeyToken` 做十六进制校验。
- 大写 `A-F` 在解析期归一化为小写后进入 `project->assemblyPublicKeyToken`。
- 非 hex token 文本拒绝 manifest。
- `publicKeyToken: null` 仍按无 token 处理。
- `zr_vm_project_manifest_normalization_test` 从 3 个用例扩展到 5 个用例。

## RED/GREEN

- RED：新增 publicKeyToken normalization 用例后，测试 5 个用例中 2 个失败：大写 token 未小写化，`not-hex` 被接受。
- GREEN：补齐 hex 校验与小写归一化后，manifest normalization 测试 5/0。

## 验证

- WSL gcc：`zr_vm_project_manifest_normalization_test` 5/0。
- WSL gcc：`zr_vm_project_import_resolver_test` 9/0。
- WSL clang：`zr_vm_project_manifest_normalization_test` 5/0。
- WSL clang：`zr_vm_project_import_resolver_test` 9/0。
- Windows MSVC CLI smoke：`hello_world` 输出 `hello world`。

## 未关闭范围

本切片只覆盖 `.zrp` manifest identity text normalization，不声明 strong-name cryptographic validation、runtime binding 诊断、preserve 规则解析、默认最小 metadata 策略或 dump/diff 工具完成。MSVC/clang/gcc 仍报告 `project.c` 既有 const qualifier warning，本切片未扩大处理范围。
