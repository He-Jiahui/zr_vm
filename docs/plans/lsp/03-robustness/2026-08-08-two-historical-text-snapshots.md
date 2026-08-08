---
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/conf.h
  - zr_vm_language_server/include/zr_vm_language_server/incremental_parser.h
  - zr_vm_language_server/src/zr_vm_language_server/incremental_parser.c
implementation_files:
  - zr_vm_language_server/include/zr_vm_language_server/conf.h
  - zr_vm_language_server/include/zr_vm_language_server/incremental_parser.h
  - zr_vm_language_server/src/zr_vm_language_server/incremental_parser.c
tests:
  - tests/language_server/test_incremental_parser.c
  - tests/acceptance/2026-08-08-lsp-l6-two-historical-text-snapshots.md
doc_type: milestone-detail
plan_id: lsp-03-robustness
record_id: 2026-08-08-two-historical-text-snapshots
status: completed
completed_at: 2026-08-08 18:14 +08:00
---

# Two Historical Text Snapshots

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-08 18:14 +08:00 | 已完成 | 每个file version保留当前text block加最近两份历史text block，并发布有界历史snapshot acquire。 |

## 已实现契约

- 每次真实内容更新先分配新text block，再把原current block的文件所有权移入固定两槽历史队列；相同内容的version更新不制造多余历史块。
- 历史顺序是新到旧。第四次版本更新后历史为v3/v2；第五次更新后历史为v4/v3，最旧块的文件所有权被释放。
- 历史Acquire返回捕获时的version、open provenance、fallback flag与content generation，并为content block增加引用；因此已借出的v3/v2快照跨v5 rollover仍可安全读取。
- 文件释放会释放current和两个历史所有权；外部已Acquire的snapshot继续由其独立引用管理。

## 验证

- RED：MSVC在新增历史snapshot API尚未定义时以两个未解析外部符号链接失败。
- GREEN：MSVC incremental parser executable真实exit 0，9/9；覆盖顺序、容量、rollover和借出快照生命周期。
- GREEN：MSVC LSP source-contract executable真实exit 0。

## 未完成边界

- 此leaf只限制text block数量，不保留或淘汰semantic snapshot。
- workspace semantic cache的256MiB LRU、peak memory report、GCC/Clang可比验证和完整rapid edit/cancel/close snapshot stress仍未完成。
- 聚合language_server CTest未作为证据：该build中第一个companion executable未构建，CTest在运行任何suite项前即失败。
