# LSP L6 Two Historical Semantic Snapshots Acceptance

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-09 23:37 +08:00 | 已完成 | 验证两份历史 semantic analyzer state 的顺序、rollover、稳定 primary identity 与 scoped-cache AST 借用释放。 |

## Evidence

- v1 到 v4 的接口 case 保留历史 v3/v2，拒绝第三个历史索引，并验证每个
  返回 analyzer 的原始 AST 和 semantic context identity。
- 当前 URI analyzer 的指针跨版本更新不变；历史 analyzer 直接拥有被
  解析器移交的旧 AST。
- local semantic-query suite 保留 unaffected body scope cache，并在旧 AST
  被历史 snapshot rollover 淘汰时安全失效且计入一次失效。
- MSVC、GCC 和 Clang 的 LSP interface 与 local semantic-query executables
  均真实 exit 0。

## Open Scope

- 不验证 workspace 256MiB LRU、全局峰值内存、cache recency/victim
  selection 或完整 L6 stdio/CLI stress matrix。
