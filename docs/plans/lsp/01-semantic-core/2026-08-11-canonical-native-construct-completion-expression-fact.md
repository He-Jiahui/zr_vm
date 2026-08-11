---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_support.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_native_construct_receiver_fact_cases.h
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_support.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
plan_sources:
  - docs/plans/lsp/01-semantic-inference-core.md
  - docs/plans/lsp/05-implementation-blueprint.md
tests:
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_native_construct_receiver_fact_cases.h
  - tests/acceptance/2026-08-11-lsp-l8-canonical-native-construct-completion-expression-fact.md
doc_type: acceptance_record
plan_id: lsp-01-semantic-core
record_id: 2026-08-11-canonical-native-construct-completion-expression-fact
status: completed
completed_at: 2026-08-11 19:57 +08:00
---

# Canonical Native Construct Completion Expression Fact

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 | 证据 |
|---|---|---|---|
| 2026-08-11 19:57 +08:00 | 已完成 | L8 独立 native construct completion 合同：`init math.Vector3(...).` 的不完整快照只消费 receiver-prefix 的 exact expression fact；事实不可用时成功返回空补全，阻断 semantic/import/general/scoped fallback | 本记录、[验收记录](../../../tests/acceptance/2026-08-11-lsp-l8-canonical-native-construct-completion-expression-fact.md) |

## Contract

`ZrLanguageServer_LspSemanticQuery_CollectCompletionItems` 在解析 semantic
query、import metadata 或 scoped fallback analyzer 之前，检查当前文本是否停在
非标识符 receiver 的 member dot。该上下文只能通过 receiver-prefix AST node 的
`ZR_SEMANTIC_FACT_EXACT` expression fact 和有效 canonical `TypeId` 取得 receiver
type。

事实有效时，现有 descriptor/prototype collector 继续返回 `Vector3` 的 `x`、`y`、`z`
成员。事实为 unknown、missing 或 invalid 时，接口成功返回空 completion array；它不
调用 AST type inference，也不允许 `ResolveAtPosition`、import module、general
completion 或 last-good scoped analyzer 以旧 AST 重建 receiver。

普通标识符 receiver 保留既有解析路径。此叶不改变 project receiver canonical
property 合同，也不修改 parser semantic-fact schema。

## Evidence

- RED: 在 version 1 的完整 `init math.Vector3(...).y` 后更新 version 2 为
  `init math.Vector3(...).`，将 fallback AST 的 receiver expression fact 标为
  `UNKNOWN`；旧 completion 仍错误返回 `x/y/z`。
- GREEN: 同一 fixture 的 valid fact 仍返回 `x/y/z`；unknown fact 返回成功的空数组。
- 根因: receiver collector 位于 semantic/import completion 和 scoped analyzer
  fallback 之后，原有局部 fail-closed 信号无法阻断这些更早的生产者。
- 修复: 精确事实 gate 前移到 completion dispatcher，并保留 collector 的
  fail-closed 信号，防止后续路径重新开启 generic fallback。

## Validation Matrix

固定验证源码为 `3333d4a + 本叶 5 个 LSP code/test overlays`；每条测试命令均以
真实进程退出码 `0` 结束。

| Toolchain | Semantic facts | Local query | Interface | Project | stdio/CLI |
|---|---:|---:|---:|---:|---:|
| GCC | 13/13 | 32/32 | 106/106 | 58/58 | 2/2 |
| Clang | 13/13 | 32/32 | 106/106 | 58/58 | 2/2 |
| MSVC | 13/13 | 32/32 | 106/106 | 58/58 | 2/2 |

三个工具链还分别运行 expression-fact hover 与 local semantic hover，均零失败。
GCC/Clang 使用本地 WSL source snapshot，避免 Windows 挂载目录的 CMake glob
等待；MSVC 使用独立静态快照构建。

## Open Scope

这只完成 L8 的第十个独立 consumer 合同，不表示 L8 完成。其余 local fallback
删除、provider/project 覆盖和完整 protocol matrix 仍开放。
`language_server_stdio_inline_value_semantic_smoke` 的 computed-member payload
既有基线失败不属于本叶，未被计作完整 33-test CTest 集合通过。
