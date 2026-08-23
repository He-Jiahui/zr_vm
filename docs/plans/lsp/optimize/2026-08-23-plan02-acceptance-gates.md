---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/snapshot/lsp_semantic_snapshot.c
  - zr_vm_language_server/src/zr_vm_language_server/incremental_parser.c
  - zr_vm_language_server/stdio/stdio_diagnostics.c
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/snapshot/lsp_semantic_snapshot.c
  - tests/language_server/test_lsp_semantic_snapshot.c
  - tests/language_server/test_lsp_incremental_equivalence.c
  - tests/language_server/stdio_snapshot_workspace_diagnostics_smoke.js
plan_sources:
  - docs/plans/lsp/optimize/02-snapshots-workspaces-and-diagnostics.md
tests:
  - tests/language_server/test_lsp_uri.c
  - tests/language_server/test_lsp_semantic_snapshot.c
  - tests/language_server/test_lsp_incremental_equivalence.c
  - tests/language_server/stdio_snapshot_workspace_diagnostics_smoke.js
  - tests/language_server/stdio_smoke.js
doc_type: milestone-record
---

# Plan 02 Task 7: Acceptance Gates

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 | 证据 |
| --- | --- | --- | --- |
| 2026-08-23 22:51 +08:00 | 已完成 | 完成 URI、snapshot、增量重解析与 multi-root diagnostics 的 Plan 02 验收门禁。 | GCC、Clang、MSVC 均以真实 process exit 0 通过 focused 和完整 stdio smoke。 |

## Delivered Contract

- `SZrLspSemanticSnapshot` 以规范 URI 递归记录已经由 project index 解析的 import
  图。primary URI 与已记录 dependency URI 组成 visited set，因而循环 import 不会重复
  捕获；直接或传递依赖的 file generation 改变都会使 primary snapshot 失效。没有进入该
  集合的 URI 改变不会造成 `ContentModified`。
- `test_lsp_incremental_equivalence` 对 10,000 次确定性随机等长度编辑执行 incremental
  与 clean full-parse differential。序列覆盖 ASCII、三字节 CJK 和四字节 astral UTF-8
  code point，并在每轮校验 UTF-16 position round-trip、snapshot 内容和最终 LSP JSON。
  三套工具链都报告 `fallback=0/10000`。
- 新增 multi-root stdio smoke：workspace diagnostics 返回未打开 importer、provider 和
  第二 root 的 source URI，且未打开文档的 `version` 为 `null`。provider 在磁盘更新并
  通过 watched-files 通知重载后，importer 的 `resultId` 必须改变。
- 性能数据记录为同一测试进程的 `clock()` ticks，不将不同编译器或平台的数值作为墙钟
  阈值比较。GCC p50/p95/p99 为 109/530/2123 ticks，Clang 为 107/425/10807 ticks，
  MSVC 为 0/1/2 ticks（该平台 `clock()` 分辨率更粗）。

## Validation

- GCC Debug shared、Clang Debug shared 与 MSVC 19.44.35228 Debug shared 分别通过
  `lsp_uri_test`、`lsp_semantic_snapshot_test`、
  `lsp_incremental_equivalence_test`，全部真实 exit 0。URI matrix 为 Unix 14/14，
  Windows 增加 UNC coverage 后为 16/16；snapshot 包含 transitive project-import
  invalidation；incremental differential 均为 10,000/10,000。
- 每套工具链都以真实 exit 0 运行
  `stdio_snapshot_workspace_diagnostics_smoke.js`（7/7）、
  `stdio_diagnostics_generation_smoke.js` 和
  `stdio_workspace_folders_smoke.js`（12/12），随后构建 CLI 与 descriptor fixture
  并运行完整 `stdio_smoke.js`。
- 完整 stdio smoke 在三套工具链都报告 warm hover/completion/signature、diagnostics、
  100-file workspace incremental diagnostics 的 percentile 和 process peak working
  set，未出现 failure；GCC、Clang、MSVC 的 peak 分别约为 32.77MiB、32.07MiB、40.98MiB，
  均低于默认 512MiB 预算。

## Scope Boundary

此记录完成 `02-snapshots-workspaces-and-diagnostics.md` 的 Task 7，且仅在当前 native
stdio/toolchain matrix 下给出验收结论。Plan 03 之后的 canonical semantic query、编辑器
feature correctness、native/web capability parity 与完整 WASM/browser runtime gate 保持为
独立后续里程碑。
