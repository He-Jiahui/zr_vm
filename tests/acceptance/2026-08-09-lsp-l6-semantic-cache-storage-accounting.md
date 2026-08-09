# LSP L6 Semantic Cache Storage Accounting Acceptance

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-09 22:31 +08:00 | 已完成 | 验证 primary/scoped semantic cache 的真实 storage 计量、递归释放、scoped identity 保留和按需重新初始化。 |

## Evidence

- `test_semantic_analyzer.c` 断言 primary cache storage 非零，创建 scoped analyzer 后递归 storage 增大。
- release 后 primary/scoped cache pointer 均为空、递归字节数为零，但 scoped analyzer pointer 保持原 identity。
- 释放后分析 `var cached = 1;` 成功，primary cache 再次分配，证明 cache release 不破坏后续 semantic analysis。
- GCC、Clang 和 MSVC 的 `zr_vm_language_server_semantic_analyzer_test` 均真实 exit 0。

## Exclusions

- 不验证 workspace 256MiB LRU victim selection 或 eviction order。
- 不验证 historical semantic snapshot、process peak memory 或 stdio performance budget。
