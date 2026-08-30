---
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/reference_tracker.h
  - zr_vm_language_server/src/zr_vm_language_server/reference_tracker.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/language_server/test_reference_tracker.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_lsp_source_contracts.c
doc_type: milestone-record
---

# Plan 03 Task 7.44: Reference Tracker Query Surface Removal

## Goal

删除 consumer 已迁移到 parser relation query 后仍由旧测试保活的 reference tracker
relation query API 与 SymbolId 哈希索引，继续收敛 LSP 自有 reference collection，且不
跨入 Syntax05 正在修改的 parser import producer、property consumer 和 interface 路径。

## Contract

- `FindReferences`、`GetReferenceCount`、`GetReferenceLocations` 不再作为 LSP 第二套
  relation query；definition/references/highlights/rename 的 canonical relation consumer
  继续位于 parser semantic query projector。
- 删除仅由上述三个 API 读取的 `symbolToReferencesMap`、哈希池生命周期和 tracker 中
  无读取的 state/symbol-table 字段。
- 保留 `AddReference` 与 `FindReferenceAt`，供尚未迁完的 analyzer syntax-recovery
  position hit 使用；每条 `SZrReference` 继续保存 parser SymbolId。
- exact source URI/range 匹配保持 fail closed；缺失 source 不能互相视为同一文件。
- 不修改 Syntax05 exact-owned parser import metadata、`lsp_interface.c`、property consumer
  或并发脏 `semantic_analyzer_support.c`。

## RED/GREEN

source-contract 先禁止哈希索引字段和三个 query API。旧实现真实 exit 1，精确产生 4 项
失败。GREEN 删除 public declarations、production query/index code和旧测试诊断探针；
tracker tests 改为验证仍有生产用途的 add/find-at、多个 exact positions、source identity
与每条 reference 的 canonical SymbolId 保存。

## Verification

- 全仓 production/test 扫描确认三个 query API 与 `symbolToReferencesMap` 仅剩
  source-contract 的禁止文本，没有调用或字段访问。
- WSL GCC 与 Clang 对 production `reference_tracker.c` 真实 syntax exit 0；GCC/Clang
  对两个修改测试的 syntax checks 真实 exit 0。
- 固定 `67bcd9676417 + 5 code/test overlays` 的独立 `/tmp` GCC/Ninja 快照完成
  `reference_tracker`、`semantic_analyzer`、`lsp_source_contracts` 三目标重链，build exit 0。
- reference tracker 5/5、source-contract 全套均真实 exit 0。
- semantic analyzer 中本任务修改的 creation/free 与 local-reference case 均 PASS；完整
  executable 仍因计划已登记的 closed-generic 与 owner-generic producer marker 真实
  exit 1，不将其计入本任务 GREEN，也不增加 LSP fallback。
- `git diff --check` 通过。production header/source 净删除 query/index/lifecycle代码；
  本任务未重跑完整三工具链 16-target matrix 或三套 stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-30 14:58 +08:00。
- 状态：Task 7.44 focused GREEN；Plan 03 Task 7/Task 8 总门禁仍进行中。
- 完成项目：production零调用审计；4项 source-contract RED/GREEN；三个 dead query API
  删除；SymbolId哈希索引与死字段删除；remaining exact-position tests重写；固定快照三目标
  重链；GCC/Clang syntax与focused runtime验证；计划状态记录。
- 未完成项目：Syntax05 imported declaration producer、source/binary/native relation parity、
  semantic analyzer 两项既有 generic producer marker、三工具链完整 16-target matrix、三套
  stdio smoke及其余active LSP symbol-table/typecheck consumers。
