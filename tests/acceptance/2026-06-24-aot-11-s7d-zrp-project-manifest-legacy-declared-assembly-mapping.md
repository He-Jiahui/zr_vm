# AOT 11-S7D zrp project manifest legacy declared assembly mapping

时间：2026-06-24 22:56:42 +08:00

## 状态

11-S7 子切片完成；完整 11-S7 仍未关闭。

## 完成项目

- 旧 `dependencies.$alias` object 现在接受 `assembly` 或 legacy `name` 作为声明目标 assembly identity。
- `assembly`/`name` 使用与 `assembly.name` 相同的 assembly-name shape 校验；两者同时存在但不一致时拒绝 manifest。
- 声明 assembly 与被引用 `.zrp` manifest 的实际 assembly identity 不一致时拒绝加载。
- 声明 assembly 含点段时，dependency package 仍使用 `$alias` 的 alias key 做 import module key，真实 assembly identity 写入 package/ref 元数据。
- `ZrLibrary_Project_GetDependencyImportVersionRange()` 现在能为显式声明 assembly 的 legacy dependency 返回真实 assembly identity，供后续 AssemblyRef 元数据使用。
- `zrp.schema.json` 同步为旧 `dependencies.$alias` object 增加 `assembly` 和 `name` 字段。
- `zr_vm_project_manifest_normalization_test` 从 8 个用例扩展到 10 个用例。

## RED/GREEN

- RED：新增 legacy declared assembly 用例后，测试 10 个用例中 2 个失败：`assembly: "zr.math"` 被拒绝，`name: "math"` 与目标 `physics` 不一致却被接受。
- RED refinement：补充版本范围查询断言后，显式声明 assembly 的 dependency 仍返回空 assembly identity。
- GREEN：loader 校验 declared assembly、alias package key 与 resolver metadata 查询修正后，manifest normalization 测试 10/0，import resolver 回归 9/0。

## 验证

- WSL gcc：`zr_vm_project_manifest_normalization_test` 10/0。
- WSL gcc：`zr_vm_project_import_resolver_test` 9/0。
- WSL clang：`zr_vm_project_manifest_normalization_test` 10/0。
- WSL clang：`zr_vm_project_import_resolver_test` 9/0。
- Schema syntax：`python3 -m json.tool zr_vm_language_server_extension/schemas/zrp.schema.json` 通过。
- Windows MSVC CLI smoke：`hello_world` 输出 `hello world`。

## 未关闭范围

本切片只覆盖 `.zrp` manifest compatibility mapping 中 legacy dependency 的声明 assembly 映射，不声明 preserve 规则 DSL、按 symbol/token 保留、AOT mode、runtime binding 诊断、默认最小 metadata 策略或 dump/diff 工具完成。`project.c` 与 `project_import_resolver.c` 已超过 1000 行；本次为窄行为补丁，未在同一切片内进行 manifest loader/resolver 拆分，后续应在 manifest surface 稳定后独立拆分。
