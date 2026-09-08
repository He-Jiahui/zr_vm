---
related_code:
  - zr_vm_common/include/zr_vm_common/zr_path_conf.h
  - zr_vm_core/include/zr_vm_core/artifact_schema.h
  - zr_vm_parser/include/zr_vm_parser/writer.h
  - zr_vm_library/include/zr_vm_library/project.h
  - zr_vm_library/include/zr_vm_library/zrm.h
  - zr_vm_parser/include/zr_vm_parser/test_contract.h
implementation_files:
  - zr_vm_core/src/zr_vm_core/artifact_schema.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
  - zr_vm_library/src/zr_vm_library/zrm.c
  - zr_vm_library/src/zr_vm_library/project/project_manifest_v2.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/plans/aot/11-metadata.md
  - docs/plans/syntax/2026-07-19-10-native-ffi-module-package-design.md
tests:
  - tests/parser/test_artifact_schema.c
  - tests/parser/test_artifact_schema_source_roundtrip.c
  - tests/library/test_zrm_container.c
  - tests/artifact/test_manifest_roundtrip.c
  - tests/cli/test_cli_zrp_metadata_dump.c
doc_type: reference
---

# 产物与格式

| 后缀/对象 | 生产者 | 消费者 | 身份校验 |
| --- | --- | --- | --- |
| `.zr` | 用户/生成器 | parser/source loader | module declaration + source hash |
| `.zrp` | project author | library/CLI/Rust | manifest version、entry、依赖、assembly |
| `.zri` | parser writer | compiler/AOT/LSP | schema、module signature、canonical facts |
| `.zro` | parser writer | VM/AOT loader | artifact schema、ABI、module/input hash |
| `.zrs` | parser writer | IDE/诊断工具 | AST projection/source ranges |
| `.zrm` | library packer | project resolver | `zr.zrm/v1`、assembly、provider phase/hash |
| TestManifest | Test compiler | CLI runner/LSP/DAP | manifest schema、module signature |

## `.zrp`

manifest 记录 `name`、`assembly`、`version`、`source`、`binary`、`intermediate`、`entry`、
path aliases、package exports、runtime/build dependencies、resources、feature switches、
AOT mode/preserve rules。entry 是 module key，不是任意函数名；运行时按 module export contract
解析。V2 lock 记录 resolved version/content hash/transitive identity/provider phase。

## Canonical binary

artifact sections 包含 module identity、TypeDef/TypeSpec、Member/Property、layout、metadata
state/record、domain-transfer、scheduler、relocation、call-binding 和 native-import rows。
每个 section 有长度/计数边界，reader 拒绝未知 schema、截断、过大计数、非法 token 和 trailing
bytes。`ZrCore_Artifact_HashBytes`、metadata hash 和 module signature 用于增量缓存与 stale
检测。

## `.zrm`

格式常量：`ZR_LIBRARY_ZRM_FORMAT="zr.zrm/v1"`，manifest entry
`META-INF/zrm.json`；模块位于 `modules/`，资源位于 `resources/`，compile-tool executable
位于 `compile-tools/`。entry 可 STORE 或 DEFLATE。`OpenBytes` 借用 immutable bytes，必须
在 `Close` 前保持地址和内容不变；`ReadEntry` 返回由 `ZrLibrary_Zrm_FreeBytes` 释放的副本。

## 兼容策略

reader 先验证 ABI/schema/hash，再 materialize；不支持的 section 应返回明确 status 而不是
跳过可能影响语义的 row。写入失败后调用方应删除临时文件，避免下一次增量构建误用半产物。
