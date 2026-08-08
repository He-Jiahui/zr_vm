# LSP L6 Two Historical Text Snapshots Acceptance

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-08 18:14 +08:00 | 已完成 | 每个file version保存current与最近两份历史text snapshot，顺序和借出生命周期均受测试约束。 |

## Evidence

- 四次更新后历史Acquire按新到旧返回v3/v2；第五次更新后队列为v4/v3。
- 在rollover前Acquire的v3/v2保持内容和generation，即使其文件版本所有权已被逐出。
- MSVC incremental parser executable 9/9与LSP source-contract executable均真实exit 0。

## Open Scope

- 不覆盖semantic snapshot历史、workspace cache 256MiB LRU、peak memory、GCC/Clang可比性能或完整snapshot压力矩阵。
