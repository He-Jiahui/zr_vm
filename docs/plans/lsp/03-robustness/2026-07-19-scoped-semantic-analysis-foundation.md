---
plan_id: lsp-03-robustness
record_id: 2026-07-19-scoped-semantic-analysis-foundation
status: completed
completed_at: 2026-07-19 18:42 +08:00
source_plans:
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
evidence_scope: scoped-analysis-foundation
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/semantic_analyzer.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_analysis.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_scope.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_union.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow.c
related_tests:
  - tests/language_server/test_lsp_local_semantic_query.c
  - tests/language_server/test_lsp_local_semantic_scope_cases.h
  - tests/language_server/test_semantic_analyzer_exact_type_cases.h
  - tests/parser/test_dataflow_engine.c
---

# Scoped Semantic Analysis Foundation

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-07-19 18:42 +08:00 | 已完成 | 声明级语义分析入口、结构化作用域根解析、作用域缓存键、completion空结果回退、canonical expression fact适配、cleanup CFG双向transfer、Windows DLL查询导出契约、GCC/Clang/MSVC十四目标矩阵 |

## 已完成契约

- `AnalyzeScope` 保留全模块符号可见性，只在选定声明根内执行reference、typecheck、CFG、dataflow和query diagnostics。
- `FindAnalysisRootAtPosition` 支持function/method/meta function/test/lambda/getter/setter，并穿透class、struct、property和compile-time wrapper。
- property和compile-time wrapper只有在实际包含accessor/callable时才返回作用域根；空包装位置保留全模块fallback。
- completion已有查询为空时按光标声明触发作用域分析；找不到安全根时退回全模块分析。
- cache identity同时包含全AST hash、scope hash和scope range，避免不同声明复用错误结果。
- cleanup block与statement block在forward/backward dataflow中采用相同transfer规则。

## 验证证据

- RED：缺失作用域API导致链接失败；首个实现因范围包含假设返回`AnalyzeScope=0`；复审用例证明空property wrapper会被误判为分析根；cleanup forward/backward新增用例均为`Expected 1 Was 0`；MSVC暴露两个跨DLL私有符号。
- GREEN：结构成员校验、公共导出API和cleanup transfer谓词修复上述边界。
- WSL GCC 11.4、WSL Clang 14、Windows MSVC 19.44.35228分别运行同一十四目标矩阵，三套结果均为`run=14 failures=0`，且无`Fail -`或`:FAIL:`标记。
- 计数结果：semantic query 16/16、compiler query diagnostics 16/16、parser 75/75、expression facts 28/28、type inference 118/118、dataflow 9/9、local query 21/21；其余七个LSP/semantic可执行目标均通过。

## 未完成边界

- L6未完成。symbol collection仍是全模块，document update和多数hover/query仍走全量分析。
- declaration incremental parse、按依赖传播失效、cancellation、immutable snapshot race、source/binary/native/package provider parity、p50/p95/p99和峰值内存门槛仍待实现。
- 冻结快照为编译当前`HEAD`引用的枚举使用了未提交core profiling helper作为外部baseline overlay；该无关core改动不属于本记录。
