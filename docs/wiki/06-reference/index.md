---
related_code:
  - CMakeLists.txt
  - zr_vm_common/include/zr_vm_common/zr_version_info.h
  - zr_vm_library/include/zr_vm_library/native_registry.h
  - tests/fixtures/projects/syntax_reference_v1/syntax_reference_v1.zrp
implementation_files:
  - CMakeLists.txt
  - docs/wiki/manifest.json
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/wiki/manifest.json
tests:
  - tests/fixtures/projects/syntax_reference_v1/syntax_reference_v1.zrp
  - tests/library/test_official_provider_convergence.c
doc_type: category-index
---

# 参考资料

本目录是面向 Wiki 生成器的查表层：API 索引、产物格式、功能状态、错误分类和术语表。实现
细节仍以 `01`-`05` 章节为准；这里不复制长篇设计，而是给网页导航和交叉引用稳定锚点。

| 页面 | 用途 |
| --- | --- |
| [API 索引](api-index.md) | C 头文件、主要函数族和 provider 入口 |
| [产物格式](artifacts.md) | `.zr/.zrp/.zri/.zro/.zrs/.zrm` 与 TestManifest |
| [状态矩阵](status-matrix.md) | 当前/实验/计划边界和证据位置 |
| [错误目录](error-catalog.md) | parser、runtime、registry、AOT、FFI、CLI 错误 |
| [术语表](glossary.md) | TypeId、Place、loan、domain、provider 等术语 |

## 文档版本

Wiki 目录采用 `manifest.json` schema 1。生成器应读取 `sections` 建立顶层导航，读取
`pages` 建立完整路由，并把每页 Front Matter 的 `related_code`、`implementation_files`、
`plan_sources`、`tests` 转成源码链接和“验证证据”标签，同时保留页面中的状态词。当前
语言规范的 executable fixture checksum 为 7；文档更新不能把旧 `%` 语法示例重新标成生产语法。
