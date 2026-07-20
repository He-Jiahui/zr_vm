---
plan_id: lsp-03-robustness
record_id: 2026-07-20-resolved-callable-consumer-convergence
status: completed
completed_at: 2026-07-20 11:53 +08:00
source_plans:
  - docs/plans/lsp/01-semantic-inference-core.md
  - docs/plans/lsp/02-diagnostics-and-errors.md
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
evidence_scope: resolved-callable-consumer-convergence
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_canonical_signature_help.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_canonical_signature_help.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_query_diagnostics.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_scope_cache.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
related_tests:
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_reference_callable_consumer_cases.h
  - tests/language_server/test_lsp_local_semantic_query.c
  - tests/language_server/test_lsp_local_semantic_receiver_dependency_cases.h
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/stdio_smoke.js
---

# Resolved Callable Consumer Convergence

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-07-20 11:53 +08:00 | 已完成 | LSP signature help与receiver hover统一消费`CallAt/FormatCall` canonical call facts；scoped ref parameter information保留passing/escape contract；resolved receiver `SymbolId + declarationRange`驱动method direct-caller精确失效；compiler current diagnostic发布为persistent query fact并去除同range推断级联；closed generic receiver签名保留结构化generic clause和闭合canonical类型；三工具链十六目标矩阵及三套stdio/CLI smoke完成 |

## 已实现契约

- canonical signature-help入口只通过`ZrParser_SemanticQuery_CallAt`取得callable `TypeId`，并用`ZrParser_SemanticQuery_FormatCall`取得整体label。LSP不按member name查找目标，也不从label文本反推类型或generic clause。
- parameter information来自同一canonical function node的有序parameter contracts。`REF`/`REF_READONLY`在escape upper bound为`FUNCTION`时分别显示`scoped ref`/`scoped ref readonly`；非value参数格式化pointee `TypeId`，避免重复输出reference qualifier。
- receiver hover要求`hasResolvedTarget`、resolved reference和非`NONE` receiver effect同时成立；文本与signature help复用同一`FormatCall`结果。readonly receiver显示`const fn read(): int`，mutable receiver显示`fn ...`。
- parser支持提交`95358a4`在同一resolved call fact中同时投影结构化generic parameters和闭合canonical `TypeId`。因此mutable generic receiver稳定显示`fn shape<const N: int>(value: Matrix<int, 4>): Matrix<int, 4>`，LSP没有AST/name/string补写分支。
- receiver/method declaration signature及无显式返回类型body变化使用resolved reference fact的精确declaration range分类。direct caller失效并计入direct metric；无关scope保留；unresolved call、poisoned facts、diagnostics或不支持callable继续保守失效。
- named-call兼容失败保留parser compiler current diagnostic，先处理已有ownership specialization；其余错误在清除current error前调用`ZrParser_Compiler_PublishCurrentDiagnostic`。LSP随后只从`SemanticQuery_Diagnostics`追加persistent fact，并删除同call range的`cannot_infer_exact_type`占位级联。

## TDD与边界证据

- receiver target RED最初证明旧public call query没有稳定target identity；parser M6发布`hasResolvedTarget/targetSymbolId/targetDeclarationRange`后，测试要求SymbolId等于resolved reference且declaration range等于whole method declaration。
- scoped parameter RED固定`inspect(value: scoped ref readonly int): int`的signature label和首个parameter information label；GREEN只读取canonical passing/escape/type contracts。
- diagnostic RED固定缺失`ref` marker时parser query与LSP都恰好发布一个`compiler_error`，message包含`%ref parameter requires the 'ref' argument marker`，不保留同range generic inference级联。
- receiver cache用例覆盖无关signature edit保留、无关inferred method body edit保留、direct inferred-method caller失效；direct与conservative计数分开断言。
- closed-generic stdio RED精确失败在`signatureHelp should show the closed generic method signature with normalized const generics`。parser structured generic clause提交后，LSP AST fallback被删除，interface与stdio均通过canonical fast path。
- 最后新增的hover range RED在自定义runner中产生2个`Fail -`，尽管进程仍exit 0：receiver与free callable hover都返回请求cursor range而不是resolved reference fact range。receiver canonical helper改为直接投影`query.reference->range`；free callable signature fallback通过`CallAt` resolved reference helper取得同一range；通用local-symbol hover仅在query range位于callee reference range时替换，避免实参hover误投影。最终interface为90 Pass、0 Fail。

## 工具链与回归证据

- GCC 11.4、Clang 14和MSVC 19.44 (`VSCMD_VER=17.14.36`)均重建并运行同一16目标矩阵，三套均为16/16真实process exit 0。
- focused结果包括parser query 26/26、compiler query diagnostics 18/18、semantic facts 12/12、canonical consumers 10/10、canonical type graph 19/19、expression facts 28/28；LSP semantic analyzer 46项、query diagnostics 14项、interface 90项和local semantic query 32项均无新增failure marker。
- GCC、Clang和fresh MSVC static `.codex/q`分别运行`tests/language_server/stdio_smoke.js`，三套stdio/CLI smoke均真实exit 0。测试作为最终命令或由PowerShell直接执行，未使用会被外层提前展开的bash `$?/$code` wrapper。
- 三工具链marker总数均为4，全部来自既有project binary/plugin允许基线：binary import metadata hover/completion、descriptor plugin member navigation、binary import references、binary import document highlights。closed-generic marker已从旧基线移除，未新增白名单。
- 旧MSVC static cache曾出现heap corruption且日志为空，明确作废。最终MSVC使用fresh `.codex/q`重建，16/16与stdio/CLI smoke均未复现。

## Snapshot、Identity与Schema

- 本阶段基于稳定parser支持HEAD `95358a4`完成LSP consumer。public `CallAt/FormatCall` API不变，不增加LSP私有target lookup或member-name fallback。
- resolved target只在`hasResolvedTarget=true`时消费；identity由`targetSymbolId + targetDeclarationRange`组成。unresolved结果不参与精确依赖分类。
- 本阶段不修改artifact schema generation、document snapshot generation或protocol generation。scoped cache仍要求旧/新scope range长度、位置与AST hash满足既有复用门禁。

## 未完成边界

- Syntax M6与LSP总体计划仍未完成。本记录只关闭source resolved callable的LSP consumer子里程碑，不代表source/binary/native/import/VM/AOT所有provider已达到同签名全面对等。
- property/get/set/ref-get、constructor/meta call、imported binary/native callable的resolved target identity和diagnostic parity仍需逐provider接入同一query shape。
- 主document仍整文件parse/analyze；partial reparse、多scope cache、snapshot race、cancellation、workspace LRU以及p50/p95/p99和峰值内存预算均未完成。
- project binary/plugin的4个既有marker仍需在相应provider里修复，不能作为本子里程碑的GREEN声明。
