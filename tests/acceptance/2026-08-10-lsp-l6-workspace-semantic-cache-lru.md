# LSP L6 Workspace Semantic Cache LRU Acceptance

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-10 00:20 +08:00 | 已完成 | 验证 context-local exact cache-storage LRU 对 primary 和 historical semantic analyzer 的受限预算、recency、release 和 state preservation。 |

## Evidence

- 两 URI 用例以实际单 analyzer storage 作为 cap，验证 oldest primary
  cache 先释放，重新分析后的 recent analyzer 保留且旧 B cache 后释放。
- 一 URI 的 v1/v2/v3 用例以 current storage 作为 cap，验证两份 history
  cache 释放而 current/history semantic context 仍保持有效。
- `storageBytes <= limitBytes`、`peakStorageBytes`、eviction count 和
  cumulative released bytes 都由公共 storage-info API 断言。
- GCC、Clang 与 MSVC 的 LSP interface 和 local semantic-query test
  executables 均真实 exit 0；MSVC 为独立 fresh static build。

## Open Scope

- 不把 exact cache-storage 数值表述为 process RSS 或 allocator peak。
- 不替代 L6 的完整 stdio/CLI stress、跨平台性能或峰值内存报告。
