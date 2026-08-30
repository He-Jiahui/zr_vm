---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_internal.h
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/language_server/test_lsp_source_contracts.c
doc_type: milestone-record
---

# Plan 03 Task 7.54: Dead Project URI Ensure Removal

## Goal

删除project层全仓无调用的旧project-URI ensure入口，继续收敛semantic/project migration后遗留
的内部公开surface。保留当前interface实际调用的project-URI get-or-create入口及其load行为。

## Contract

- `ZrLanguageServer_Lsp_ProjectEnsureProjectByProjectUri` 不再声明或实现。
- `ZrLanguageServer_LspProject_GetOrCreateByProjectUri` 保持活跃，继续负责project URI lookup、
  path conversion、index创建和semantic module load。
- `ProjectEnsureProjectForUri` 与document/project discovery路径不变。
- 本阶段不修改interface实现、project identity、URI normalization或semantic producer。

## RED/GREEN

dead-project source-contract增加旧API的source/header禁止项；固定旧生产代码真实exit 1并精确
报告两项。删除完整函数块与内部头声明后同一测试转GREEN；全仓该名称只剩两个
source-contract禁止文本。

## Verification

- 固定 `1cab9f1 + 3 code/test overlays` 的WSL GCC/Ninja快照完成source-contract、project
  features与interface目标重链；source-contract 70/70，真实exit 0。
- GCC project features进程exit 0，日志为42 Pass/18个已登记producer marker；与Task 7.52
  overlay失败名称delta 0，runner exit 0不覆盖marker，因此该目标不计GREEN。
- GCC interface真实exit 1，与固定parent保持同一8个已登记producer marker，delta 0。
- 独立Clang/Ninja缓存完成source-contract与project features重链；source-contract 70/70
  真实exit 0，project保持同一42 Pass/18 marker且与GCC名称delta 0。
- `git diff --check`通过；本任务未执行MSVC、完整三工具链16-target matrix或三套stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-30 17:22 +08:00。
- 状态：Task 7.54 focused GREEN；Plan 03 Task 7/Task 8总门禁仍进行中。
- 完成项目：无调用project URI API审计；source-contract RED/GREEN；旧ensure实现/声明删除；
  GCC/Clang fixed snapshot重链；project/interface marker复核；计划状态记录。
- 未完成项目：Syntax05 imported declaration/property producer、project identity producer parity、
  source/binary/native完整relation parity、其余analyzer/symbol-table第二套语义删除、MSVC与完整
  三工具链16-target matrix、三套stdio smoke和Plan 03 Task 8总门禁。
