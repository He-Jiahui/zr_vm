---
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/incremental_parser.h
  - zr_vm_language_server/src/zr_vm_language_server/incremental_parser.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_workspace_edit_snapshot.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_workspace_edit_snapshot.h
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_source_rename.c
  - zr_vm_language_server/stdio/stdio_workspace_files.c
  - zr_vm_language_server/stdio/stdio_rename.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio_internal.h
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_workspace_edit_snapshot.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_source_rename.c
  - zr_vm_language_server/stdio/stdio_workspace_files.c
  - zr_vm_language_server/stdio/stdio_rename.c
plan_sources:
  - user: 2026-07-20 strict LSP semantic inference plan execution with per-submilestone records and commits
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
tests:
  - tests/language_server/test_incremental_parser.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_project_features.c
  - tests/language_server/test_lsp_project_source_rename_edit_cases.h
  - tests/language_server/stdio_smoke.js
doc_type: milestone-detail
plan_id: lsp-03-robustness
record_id: 2026-07-20-source-rename-workspace-edit-snapshot-revalidation
status: completed
completed_at: 2026-07-20 23:50 +08:00
source_plans:
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
evidence_scope: source-rename-workspace-edit-snapshot-revalidation
---

# Source Rename Workspace Edit Snapshot Revalidation

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-07-20 23:50 +08:00 | 已完成 | `workspace/willRenameFiles`为每个edited URI记录version/generation/length/stable hash/open-state瞬时指纹；打开document版本或内容变化、未打开disk source变化、open/closed状态变化都会在JSON提交前使整批plan失效；`documentChanges`只使用captured version，unopened source固定为`version: null`；三工具链十六目标矩阵和三套stdio/CLI smoke真实exit 0、marker归零 |

## 已实现契约

- `ZrLanguageServer_LspProject_CollectSourceRenameEdits`继续提供兼容的canonical location collection；新`CollectSourceRenameEditPlan`在同一结果上按URI去重并发布`SZrLspSourceRenameDocumentSnapshot`，没有复制content或保留AST指针。
- opened document指纹来自当前incremental-parser text block：URI、LSP version、`contentGeneration`、content length和`ZrCore_Hash_CreateStable64`。后续通用化已为file version发布显式`isOpenDocument` provenance；client version 0与synthetic disk version 0不再靠数值约定区分。unopened指纹读取disk content，并要求任何已有closed cache都可获取且与disk一致。
- `ValidateSourceRenameEditPlan`重新捕获每个URI。任一missing file、open/closed状态、version、generation、length、hash或cache/disk不一致均返回false；不按range、module name或source text尝试补救。
- stdio在每个batch item序列化前验证全部指纹。capture/validation/serialization失败会删除已构造的整个workspace edit并返回JSON `null`，不发布partial mixed-snapshot edit。
- source-rename `TextDocumentEdit`从fingerprint读取captured version；unopened source显式写JSON `null`。普通`textDocument/rename`已在后续[general rename workspace edit snapshot revalidation](./2026-07-21-general-rename-workspace-edit-snapshot-revalidation.md)中接入同一通用capture/validate/captured-version路径。

## TDD与根因证据

- RED只修改project test和internal declarations，在隔离GCC cache中重编后唯一失败为三个新API的undefined references：`CollectSourceRenameEditPlan`、`FindSourceRenameDocumentSnapshot`、`ValidateSourceRenameEditPlan`。
- GREEN固定provider declaration、opened importer和unopened importer三处canonical exact range及三个unique document snapshots；初始plan可验证，main document以相同content从version 1更新到2后旧plan必须失败。
- 测试随后重新采集fresh plan，改写未打开`secondary.zr`磁盘内容而保留import语义，旧plan必须失败。该场景证明校验不是只比较AST identity、range或module name。
- stdio smoke在现有ModuleIdentity rename fixture上新增序列化断言：两个opened importer均为captured `version: 1`，未打开provider为`version: null`；既有三处edit、`didRenameFiles` hover和definition继续通过。

## 工具链与回归证据

- 正式隔离源码为`HEAD 5e3c68ea6d106c5cdcaf93f2b03b4bf57255866c + 7个LSP code/test exact paths`，目录`.codex/snapshots/l6-source-rename-red-gcc-r1`；7个文件与共享工作树逐一SHA-256一致，Syntax 03 M2 parser/core/compiler中间态未进入快照。
- GCC 11.4.0、Clang 14.0.0和MSVC 19.44.35228 / `VSCMD_VER=17.14.36`使用独立fresh static cache。每套同一16目标矩阵均16/16真实process exit 0，`Fail -`、`FAIL:`与`:FAIL:` marker为0。
- 三套project features均`54/54`、interface均`90/90`、language feature matrix均`8/8`、project UTF-16 ranges均`3/3`、source contracts均`38/38`；parser query/facts、canonical consumers/graph、semantic analyzer/query、local query/hover、incremental parser和expression facts同轮通过。
- GCC、Clang和MSVC分别直接运行更新后的`tests/language_server/stdio_smoke.js`，并向对应stdio server传入同工具链CLI；三套真实exit 0。runner以每个process的真实exit和独立marker扫描判定，没有使用会被外层PowerShell展开的bash exit变量。

## Snapshot、Schema与协议边界

- `SZrLspSourceRenameDocumentSnapshot`现为通用`SZrLspWorkspaceEditDocumentSnapshot`的兼容alias，仍是单次request内的瞬时POD。后续persistent document snapshot只新增显式open provenance；semantic fact、public-contract hash、cache key和artifact schema未改变。
- stable hash用于变化检测，不作为semantic identity。rename replacement与range仍完全来自canonical project record和parsed import/module facts。
- 校验发生在server返回workspace edit之前。返回之后若client document再次变化，LSP client仍必须按`TextDocumentEdit.version`拒绝stale apply；server不能在response交付后重新验证client本地状态。
- 本阶段没有p50/p95/p99、峰值内存、cancellation latency或并发snapshot压力报告，因此不晋级完整L6 robustness。

## 未完成边界

- 本记录只关闭source-file rename的提交前重校验；普通general rename已由[2026-07-21 follow-up](./2026-07-21-general-rename-workspace-edit-snapshot-revalidation.md)统一到同一fingerprint plan，code action、safe fix与其他workspace edit producer仍未接入。
- package/alias/public import edge migration、`.zrp/.zrm` generation、binary/native/artifact provider replacement及public type/property/layout hash仍待后续。
- cancellation、100次乱序edit/race stress、partial reparse、多scope cache、workspace cache预算、性能百分位和峰值内存门禁仍未完成。
