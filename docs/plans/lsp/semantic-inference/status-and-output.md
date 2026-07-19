---
plan_id: lsp-semantic-inference
record_id: status-and-output
status: in_progress
updated_at: 2026-07-20 02:10 +08:00
source_plans:
  - docs/plans/lsp/01-semantic-inference-core.md
  - docs/plans/lsp/02-diagnostics-and-errors.md
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
---

# Semantic Inference Status And Output

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 | 证据 |
|---|---|---|---|
| 2026-07-19 03:35 +08:00 | 已完成 | 结构化诊断、稳定descriptor registry、详细type mismatch、duplicate definition related information、英文message catalog与本地化fallback基础 | [Structured diagnostic baseline](../02-diagnostics/2026-07-19-structured-diagnostic-baseline.md) |
| 2026-07-19 18:42 +08:00 | 已完成 | 声明级作用域分析与completion回退基础；非callable wrapper回退边界；canonical expression fact和cleanup CFG回归适配；三工具链十四目标矩阵 | [Scoped semantic analysis foundation](../03-robustness/2026-07-19-scoped-semantic-analysis-foundation.md) |
| 2026-07-19 19:17 +08:00 | 已完成 | 相同内容document version更新复用text block、AST与semantic cache；增加请求/执行/cache-hit计数；变化内容失效对照；三工具链十四目标矩阵与stdio/CLI冒烟 | [Identical-content snapshot and semantic cache reuse](../03-robustness/2026-07-19-identical-content-snapshot-cache-reuse.md) |
| 2026-07-19 20:12 +08:00 | 已完成 | 真实内容变化old/new最小byte range；基于匹配旧文本AST的module、declaration signature、declaration body分类；声明owner事实、fallback AST和保守边界；三工具链十四目标矩阵与stdio/CLI冒烟 | [Minimal change range and declaration classification](../03-robustness/2026-07-19-minimal-change-range-and-declaration-classification.md) |
| 2026-07-19 20:53 +08:00 | 已完成 | 同长度真实内容变化的token语义/坐标等价判定；注释更新推进text generation但复用AST与semantic cache；token值、坐标、fallback/lex error负边界；当前HEAD上的三工具链十四目标矩阵、incremental parser与stdio/CLI冒烟 | [Token-equivalent semantic snapshot reuse](../03-robustness/2026-07-19-token-equivalent-semantic-snapshot-reuse.md) |
| 2026-07-19 21:12 +08:00 | 已完成 | 已打开document的严格version单调门禁；相同与stale version在text分配、change classification、parse与semantic request之前拒绝；快照与语义指标不变；三工具链十四目标矩阵、incremental parser与stdio/CLI冒烟 | [Strict document version rejection](../03-robustness/2026-07-19-strict-document-version-rejection.md) |
| 2026-07-19 21:45 +08:00 | 已完成 | completion空结果回退的独立scoped semantic analyzer/cache；重复查询2 request/1 execution/1 hit；token等价更新保留、真实编辑在旧AST释放前失效；三工具链十四目标矩阵、local query、interface、incremental parser与stdio/CLI冒烟 | [Scoped query semantic cache](../03-robustness/2026-07-19-scoped-query-semantic-cache.md) |
| 2026-07-19 22:16 +08:00 | 已完成 | canonical call fact保留extern参数名；canonical signature-help provider从callable contracts构建parameter information并复用argument expression/numeric/logical/ownership文档；canonical consumer 5/5、signature semantic facts 9/9、interface 87/87与stdio/CLI冒烟；最新HEAD三工具链矩阵 | [Canonical signature-help provider parity](../03-robustness/2026-07-19-canonical-signature-help-provider-parity.md) |
| 2026-07-19 22:58 +08:00 | 已完成 | declaration-body分类驱动owning scope最小失效；同长度且未触及cached function、坐标/hash稳定时跨AST复用semantic context；连续更新4 request/1 execution/3 hit、owner 2 preservation/0 invalidation；坐标漂移负边界；最新HEAD三工具链矩阵与stdio/CLI冒烟 | [Owning-function scoped query cache preservation](../03-robustness/2026-07-19-owning-function-scoped-query-cache-preservation.md) |
| 2026-07-20 02:10 +08:00 | 已完成 | 既有ModuleIdentity/moduleName图上的首个可计数反向依赖失效范围；显式返回类型顶层函数body edit保留importer，无注解函数body与公开signature变化保守重分析；累计/单次传播计数；最新HEAD三工具链十五目标矩阵、project场景与stdio/CLI烟测 | [ModuleIdentity reverse-dependency invalidation](../03-robustness/2026-07-20-module-identity-reverse-dependency-invalidation.md) |

## 当前状态

- 总体目标进行中。当前记录只表示十个子里程碑完成，不表示L1-L8整体完成。
- 下一步继续按L6顺序为signature/generic/receiver变化建立direct-caller失效，并扩展module public hash、ModuleIdentity edge migration及public type/import变化的反向依赖传播证据。
- 每个后续子里程碑继续提交代码、文档和测试，并在本表写入完成时间、状态、完成项目和详细记录链接。
