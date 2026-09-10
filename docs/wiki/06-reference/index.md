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
| [Canonical Artifact 二进制 Schema](artifact-binary-schema-reference.md) | header/section/row、public identity、typed reader、writer 与 `.zrm` 所有权 |
| [ABI 与兼容性参考](abi-compatibility-reference.md) | runtime/plugin/AOT/artifact/FFI/layout/call-binding 的独立版本轴与升级规则 |
| [状态矩阵](status-matrix.md) | 当前/实验/计划边界和证据位置 |
| [错误目录](error-catalog.md) | parser、runtime、registry、AOT、FFI、CLI 错误 |
| [诊断生命周期与修复契约](diagnostic-lifecycle-reference.md) | structured diagnostic、registry、fix、LSP 映射、native/runtime 失败投影 |
| [术语表](glossary.md) | TypeId、Place、loan、domain、provider 等术语 |
| [SemIR/CFG 实现参考](../08-compiler-semantic-ir-facts-reference.md) | compiler 的 Place/Value/Loan/cleanup/bounds facts 与流分析 API |
| [VM 栈与执行参考](../09-runtime-stack-execution-reference.md) | stack anchor、call-info、解释器、cleanup、GC safepoint 与 native/AOT 边界 |
| [AOT lowering 与注册参考](../10-aot-lowering-registration-reference.md) | generated frame、root map、direct call/deopt、异常和 module registration |
| [Native Call Context 参考](../05-interop/native-call-context-reference.md) | native callback 参数/结果、temp root、inline storage 和重入规则 |
| [泛型实例化参考](../02-language/generic-parameter-instantiation-reference.md) | 当前 generic/where grammar、约束、canonical TypeId、C 实例表与 AOT sharing 边界 |
| [异常与清理运行时参考](../02-language/exception-cleanup-runtime-reference.md) | handler metadata、pending control、finally、native recovery 与 AOT helper |
| [系统/容器/反射运行时参考](../03-modules/system-container-reflection-runtime-reference.md) | 文件/集合/TypeId/pool 的组合使用、注册、handle 与 C API 生命周期 |
| [官方库 API 总目录](../03-modules/official-library-api-catalog.md) | 25 个官方 module identity、源级 API 分组和 C 注册入口 |
| [Wiki 编写规范](../04-tools/wiki-authoring.md) | front matter、manifest、链接校验和 Pages 发布流程 |

## 文档版本

Wiki 目录采用 `manifest.json` schema 1。生成器应读取 `sections` 建立顶层导航，读取
`pages` 建立完整路由，并把每页 Front Matter 的 `related_code`、`implementation_files`、
`plan_sources`、`tests` 转成源码链接和“验证证据”标签，同时保留页面中的状态词。当前
语言规范的 executable fixture checksum 为 7；文档更新不能把旧 `%` 语法示例重新标成生产语法。
