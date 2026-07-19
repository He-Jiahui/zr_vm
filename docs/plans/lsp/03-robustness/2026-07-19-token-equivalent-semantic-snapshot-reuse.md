---
plan_id: lsp-03-robustness
record_id: 2026-07-19-token-equivalent-semantic-snapshot-reuse
status: completed
completed_at: 2026-07-19 20:53 +08:00
source_plans:
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
evidence_scope: token-equivalent-semantic-snapshot-reuse
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/incremental_parser.h
  - zr_vm_language_server/src/zr_vm_language_server/incremental_token_equivalence.h
  - zr_vm_language_server/src/zr_vm_language_server/incremental_token_equivalence.c
  - zr_vm_language_server/src/zr_vm_language_server/incremental_parser.c
related_tests:
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_snapshot_cache_cases.h
  - tests/language_server/test_incremental_parser.c
  - tests/language_server/stdio_smoke.js
---

# Token-Equivalent Semantic Snapshot Reuse

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-07-19 20:53 +08:00 | 已完成 | 同长度真实内容更新的lexer token等价判定；注释文本变化创建新text generation但复用AST、semantic context和semantic cache；token值与token坐标变化负边界；fallback AST和lex error保守回退；当前HEAD上的GCC/Clang/MSVC十四目标矩阵、incremental parser和stdio/CLI JSON-RPC冒烟 |

## 已完成契约

- 只有旧、新内容byte长度相同，旧AST有效且不是fallback AST时才尝试token等价检查。
- 两份内容分别通过现有parser lexer读取；每个token必须类型相同、无lex error、语义值相同，并具有相同的token start offset/line/line-start与lexer current offset/line状态。EOS也必须在相同坐标结束。
- identifier、integer、float、string和template string比较实际字符串值；boolean、integer、float和char比较其语义值。token值变化不会因长度相同而复用旧语义结果。
- token等价更新仍替换text block并增加content generation，使后续协议位置映射和原文读取看到新内容；parser保留AST和现有parser diagnostics，更新content hash，semantic analyzer通过原AST/cache key命中而不执行重算。
- 注释文本变化只有在未移动任何token或EOF坐标时才能复用；空格改换行等坐标变化即使token种类和值相同，也必须重建AST并执行语义分析。

## TDD与验证证据

- 编译RED：测试首先引用`SZrFileChangeInfo.isTokenEquivalent`，因字段和token等价实现不存在而失败。
- GREEN：同长度`// old note -> // new note`更新得到新text block和generation，同时保持AST、semantic context、execution count不变，并增加一次semantic request和cache hit。
- 负边界：同长度空格改换行必须因line坐标变化失效；`return 1 -> return 2`必须因integer token值变化失效。后者在完整比较实现上直接通过，记录为discovery/GREEN。
- WSL GCC 11.4、WSL Clang 14和Windows MSVC 19.44.35228的聚焦接口套件均为86/86，incremental parser均为6/6。
- 最终回归：隔离源覆盖到当前`HEAD=1413501`及本阶段文件后，三套工具链分别运行相同十四目标矩阵，均为`run=14 failures=0`且无`Fail -`或`:FAIL:`标记；各自的`language_server_stdio_smoke`均为1/1。
- 稳定计数套件包括semantic query 16/16、compiler query diagnostics 16/16、parser 75/75、expression facts 28/28、type inference 118/118和dataflow 9/9；其余矩阵目标全部通过。

## 未完成边界

- 本记录只复用token和值及坐标完全等价的语义快照，不表示partial reparse、declaration body重算或L6完成。
- 任何token值、token类型、token坐标、内容长度、lex error或fallback AST变化都继续走整文件parse和semantic analysis。
- 当前非相同内容更新会各运行一次old/new lexer比较；尚未建立per-document token inventory、声明级CFG/query cache、direct caller/ModuleIdentity传播或性能百分位预算。
- stale version拒绝、cancellation、immutable snapshot race、历史快照上限、provider parity和峰值内存门槛仍待后续子里程碑。
- 冻结快照编译当前LSP变更时使用未提交core profiling helper作为外部baseline overlay；该无关core改动不属于本记录。
