---
plan_id: lsp-03-robustness
record_id: 2026-07-19-owning-function-scoped-query-cache-preservation
status: completed
completed_at: 2026-07-19 22:58 +08:00
source_plans:
  - docs/plans/lsp/01-semantic-inference-core.md
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
evidence_scope: owning-function-scoped-query-cache-preservation
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/incremental_parser.h
  - zr_vm_language_server/include/zr_vm_language_server/semantic_analyzer.h
  - zr_vm_language_server/src/zr_vm_language_server/incremental_parser.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_analysis.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_scope_cache.c
related_tests:
  - tests/language_server/test_lsp_local_semantic_query.c
  - tests/language_server/test_lsp_local_semantic_scope_cases.h
  - tests/language_server/test_incremental_parser.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/stdio_smoke.js
---

# Owning-Function Scoped Query Cache Preservation

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-07-19 22:58 +08:00 | 已完成 | declaration-body分类驱动owning scope最小失效；同长度且未触及cached function、完整坐标与scope hash稳定时跨AST复用scoped semantic context；连续更新旧AST所有权移交与释放；preservation/invalidation计数；GCC/Clang/MSVC十四目标矩阵、incremental parser与stdio/CLI冒烟 |

## 已完成契约

- dirty update先在旧AST上完成change classification。只有`DECLARATION_BODY`、声明owner存在、old/new byte长度相同且cached scope不与owner范围相交时，才进入保留候选；owner body、signature、module、fallback和长度变化均立即失效。
- incremental parser新增显式旧AST移交入口。仅当scoped analyzer仍直接引用当前旧AST时才延迟释放，非引用的中间AST继续由parser正常释放；scoped analyzer在compiler state之后释放其拥有的旧AST。
- parse完成后重新在新AST定位cached scope，并比较start/end的offset、line、column和scope AST hash。任一不匹配都会撤销候选、释放旧snapshot并增加invalidation计数。
- 通过提交校验后，主document analyzer仍分析新AST；scoped analyzer保留旧semantic context，并在相同新scope请求上以`scopeAstHash + full range`作为第二道cache guard跨AST命中。
- 两次连续同长度body edit把同一scoped analyzer从`2 requests / 1 execution / 1 hit`推进到`4/1/3`，owner记录`2 preservation / 0 invalidation`；等长空格换换行导致下游line坐标漂移后记录`2/1`并销毁缓存。

## TDD与验证证据

- RED只增加未受影响第二函数缓存用例；WSL GCC运行24项时23项通过，新用例精确失败为`impact=3`但`ownerCache=(nil)`。
- 第一轮GREEN证明body edit后analyzer/context不变且计数为`3/1/2`；扩展GREEN证明第二轮稳定更新为`4/1/3`，numeric fact仍为`30..30`。
- 同长度空格换换行负边界先暴露change range只有offset且line/column为0；将最终校验移到新AST scope后，该更新稳定触发一次invalidation，不复用旧位置事实。
- 最新`HEAD=a3fa73f`加本阶段overlay及外部core profile baseline上，WSL GCC 11.4、WSL Clang 14和Windows MSVC 19.44.35228均通过local query 24/24、interface 87/87、incremental parser 7/7和semantic analyzer 46/46。
- 三套工具链均通过相同十四语义/LSP目标加incremental parser，结果为`run=15`、零退出失败、零`Fail -`/`:FAIL:`标记，并通过`language_server_stdio_smoke`。MSVC刷新最新HEAD后的首次增量构建混用较新旧对象并失败；临时build执行CMake clean后的完整578对象重建和全部运行通过，陈旧结果不计为代码失败。

## 未完成边界

- 本阶段只保留一个lazily owned scoped-query cache，不是多函数CFG/fact cache，也不表示主document parse或analysis已局部化。
- 长度变化、cached scope位置/hash变化和无法重新定位scope仍保守全失效；尚未实现位置重映射或结构化fact克隆。
- signature/generic/receiver变化的direct-caller失效、public type/import变化的ModuleIdentity reverse dependency传播仍未完成。
- cancellation、snapshot race、历史snapshot上限、LRU/256MiB内存上限、延迟百分位和剩余provider parity仍待后续子里程碑。
- 隔离源树继续把未提交core profiling helper作为外部baseline overlay；该无关core改动不属于本记录。
