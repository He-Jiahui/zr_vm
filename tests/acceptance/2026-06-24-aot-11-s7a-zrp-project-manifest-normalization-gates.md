# AOT 11-S7A zrp project manifest normalization gates

时间：2026-06-24 22:19:43 +08:00

## 状态

11-S7 子切片完成；完整 11-S7 仍未关闭。

## 完成项目

- 新增 `zr_vm_project_manifest_normalization_test` 独立测试目标，避免继续扩大已有 project import resolver 测试文件。
- `.zrp` project loader 现在校验 `manifestVersion`，只接受缺省或 `1`。
- 旧 `dependencies.$alias` 与新 `references.alias` 归一化到同一 package、assembly name、min/max version range 时只保留一条 dependency ref。
- 同 alias 但 package 或 version range 不一致时仍拒绝，作为 `zrp_reference_conflict` 方向的 loader gate。
- 去重时保留新 reference 所需的 alias-for-module-key 语义，确保后续 assembly identity 查询可返回目标 assembly name。

## RED/GREEN

- RED：新增 manifest normalization 测试后 3 个用例中 2 个失败：同值 old/new reference 被拒绝，unsupported `manifestVersion: 2` 被接受；conflicting old/new reference 拒绝用例已通过。
- GREEN：补齐 manifestVersion gate 与 dependency-ref idempotent duplicate 规则后，identical old/new reference 只生成 1 个 package / 1 条 ref，conflicting old/new reference 被拒绝，`manifestVersion: 2` 被拒绝。

## 验证

- WSL gcc：`zr_vm_project_manifest_normalization_test` 3/0。
- WSL gcc：`zr_vm_project_import_resolver_test` 9/0。
- WSL clang：`zr_vm_project_manifest_normalization_test` 3/0。
- WSL clang：`zr_vm_project_import_resolver_test` 9/0。
- Windows MSVC CLI smoke：`hello_world` 输出 `hello world`。

## 未关闭范围

本切片只覆盖 `.zrp` project manifest loader normalization gate，不声明 preserve 规则 DSL、按 symbol/token 保留、AOT mode、默认最小 metadata 策略、跨模块 runtime binding 诊断、数据元数据文件 dump/diff 或 `12-S4` 后端 preserve root 解析完成。
