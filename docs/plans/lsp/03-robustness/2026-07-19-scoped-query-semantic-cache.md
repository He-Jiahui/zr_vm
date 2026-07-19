---
plan_id: lsp-03-robustness
record_id: 2026-07-19-scoped-query-semantic-cache
status: completed
completed_at: 2026-07-19 21:45 +08:00
source_plans:
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
evidence_scope: scoped-query-semantic-cache
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/semantic_analyzer.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_scope_cache.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_analysis.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
related_tests:
  - tests/language_server/test_lsp_local_semantic_query.c
  - tests/language_server/test_lsp_local_semantic_scope_cases.h
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_incremental_parser.c
  - tests/language_server/stdio_smoke.js
---

# Scoped Query Semantic Cache

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-07-19 21:45 +08:00 | 已完成 | completion空结果回退的独立scoped semantic analyzer/cache；重复同作用域查询从2次execution收敛为1次execution+1次cache hit；token等价更新保留缓存，真实编辑在旧AST释放前失效；三工具链十四目标矩阵、local query、semantic analyzer、interface、incremental parser与stdio/CLI冒烟 |

## 已完成契约

- 每个document的主`SZrSemanticAnalyzer`可惰性拥有一个独立scoped-query analyzer；主分析器的全文件symbol/reference/semantic context/cache不会被completion回退分析覆盖。
- completion在主provider没有产出时复用该scoped analyzer，并继续使用现有AST hash + analysis-root hash + source range缓存键。
- 同一AST/作用域的两次查询记录`request=2, execution=1, cacheHit=1`；切换AST或真实内容编辑后，新scoped analyzer从`request=1, execution=1, cacheHit=0`开始。
- byte-identical和token-equivalent更新不会丢弃scoped cache；真实编辑依据`fileVersion->isDirty`在incremental parse释放旧AST之前销毁该缓存。
- analyzer cache disable/clear/free传播到所有的scoped analyzer；直接使用analysis API切换AST时也有第二道失效门禁。
- 所有所有权与失效实现集中在37行`semantic_analyzer_scope_cache.c`，大型analyzer/query/interface文件只保留初始化、生命周期和调用编排。

## TDD与验证证据

- 编译RED：测试首先因`scopedQueryAnalyzer`字段和`GetOrCreateScopedQueryAnalyzer`导出API不存在而失败。
- GREEN：直接作用域缓存用例证明重复命中、comment-only token等价更新继续命中，以及body token变化后旧指标被清空。
- 协议集成：completion测试用空主symbol provider固定既有回退边界，连续两次请求得到`2/1/1`指标，证明`lsp_semantic_query.c`已使用owner cache。
- WSL GCC 11.4、WSL Clang 14和Windows MSVC 19.44.35228的定向结果均为local semantic query 23/23、semantic analyzer 46/46、LSP interface 87/87和incremental parser 7/7。
- 最终回归以当前`HEAD=b6bcd4a`及本阶段overlay运行；三套工具链均通过相同十四语义/LSP目标及incremental parser，所有可执行目标exit code为0且无`Fail -`标记；`language_server_stdio_smoke`均1/1。

## 未完成边界

- 本记录分区的是completion回退查询与主document analyzer，不是新旧AST之间的声明事实搬运或partial reparse。
- 真实body token编辑仍会整文件parse和主semantic analysis；owning function CFG/query cache最小失效尚未接入。
- declaration signature/import变化的direct caller与ModuleIdentity依赖传播、cached token inventory、cancellation、snapshot race、provider parity、延迟与内存预算仍待后续子里程碑。
- 隔离源树继续把未提交core profiling helper作为外部baseline overlay；该无关core改动不属于本记录。
