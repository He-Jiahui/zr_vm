---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_imports.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_internal.h
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/language_server/test_lsp_source_contracts.c
doc_type: milestone-record
---

# Plan 03 Task 7.52: Dead Imported Member Position Scan Removal

## Goal

删除 project import 层在 canonical semantic query 迁移后全仓无调用的 imported-member
position lookup API，以及只服务该 API 的整树 AST walker。保留仍被 imported reference
location projection 使用的 structured import binding/member extractor。

## Contract

- `ZrLanguageServer_LspProject_FindImportedMemberHit` 不再声明或实现。
- `find_imported_member_hit_recursive/in_node_array` 不再扫描整棵 AST 以按 cursor position
  恢复 module/member pair。
- 活跃 `FindImportBindingHit` 及其 `find_import_binding_hit_recursive` 保持不变；import alias
  declaration navigation 仍可定位 exact binding。
- `primary_expression_get_imported_member` 继续服务 cross-project imported-location projector；
  本阶段不改变其既有 adapter contract，也不增加新的 name/text fallback。

## RED/GREEN

dead-project source-contract 增加 API header/source 与两层 walker 四个禁止项；固定旧生产代码
真实 exit 1并精确报告四项。首个 GREEN 尝试用连续区间删除，误含交错的活跃 import-binding
walker，链接精确失败 `find_import_binding_hit_recursive`；该轮作废。按函数边界恢复 binding
walker后，production 仅净删 imported-member walker 249行和 header 声明4行，同一
source-contract 转 GREEN。

## Verification

- 固定 `78d862c + 3 code/test overlays` 的 WSL GCC/Ninja 快照完成 source-contract、semantic
  parity、project features 与 interface 目标重链。
- GCC source-contract 70/70、semantic parity 15/15，均真实 exit 0。
- GCC project features 的同构 parent/overlay 均为进程 exit 0、42 Pass/18 个已登记 producer
  marker，失败名称 delta 0；runner exit 0不覆盖 marker，因此该目标不计 GREEN。
- GCC interface 真实 exit 1，与固定 parent 保持同一8个已登记 producer marker，delta 0。
- 独立 Clang/Ninja 缓存完成 source-contract、semantic parity 与 project features重链；前两项
  70/70、15/15真实 exit 0，project保持同一42 Pass/18 marker且与GCC名称delta 0。
- `git diff --check` 通过；本任务未执行 MSVC、完整三工具链16-target matrix或三套stdio
  smoke。

## 状态与产出记录

- 完成时间：2026-08-30 17:08 +08:00。
- 状态：Task 7.52 focused GREEN；Plan 03 Task 7/Task 8 总门禁仍进行中。
- 完成项目：无调用 project API/closure 审计；source-contract RED/GREEN；误删 binding walker
  的 support-first 链接诊断与精确恢复；253行 dead position scan删除；GCC/Clang fixed snapshot
  重链；project/interface A/B marker复核；计划状态记录。
- 未完成项目：Syntax05 imported declaration/property producer、project imported-location adapter
  producer parity、source/binary/native完整relation parity、其余 analyzer/symbol-table 第二套
  语义删除、MSVC与完整三工具链16-target matrix、三套stdio smoke和Plan 03 Task 8总门禁。
