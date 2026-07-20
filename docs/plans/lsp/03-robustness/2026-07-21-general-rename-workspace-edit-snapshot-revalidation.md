---
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/incremental_parser.h
  - zr_vm_language_server/src/zr_vm_language_server/incremental_parser.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_workspace_edit_snapshot.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_workspace_edit_snapshot.h
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_source_rename.c
  - zr_vm_language_server/stdio/stdio_rename.c
  - zr_vm_language_server/stdio/stdio_workspace_files.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio_internal.h
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/incremental_parser.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_workspace_edit_snapshot.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_source_rename.c
  - zr_vm_language_server/stdio/stdio_rename.c
  - zr_vm_language_server/stdio/stdio_workspace_files.c
plan_sources:
  - user: 2026-07-21 strict LSP semantic inference plan execution with per-submilestone records and commits
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
tests:
  - tests/language_server/test_incremental_parser.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_project_source_rename_edit_cases.h
  - tests/language_server/stdio_smoke.js
doc_type: milestone-detail
plan_id: lsp-03-robustness
record_id: 2026-07-21-general-rename-workspace-edit-snapshot-revalidation
status: completed
completed_at: 2026-07-21 01:27 +08:00
source_plans:
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
evidence_scope: general-rename-workspace-edit-snapshot-revalidation
---

# General Rename Workspace Edit Snapshot Revalidation

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-07-21 01:27 +08:00 | 已完成 | 发布显式opened/disk document provenance和通用workspace-edit fingerprint API；允许synthetic disk version 0到client didOpen version 0的一次origin transition并继续严格拒绝后续same/stale version；普通`textDocument/rename`与source rename统一在JSON提交前复验URI/version/generation/open-state/length/hash，`documentChanges`只消费captured version；三工具链十六目标矩阵和三套stdio/CLI smoke真实exit 0、marker归零 |

## 已实现契约

- `SZrFileVersion`与`SZrFileVersionContentSnapshot`新增显式`isOpenDocument`。版本号只表示版本，不再承担document origin；project disk cache和client overlay即使同为version 0也可区分。
- `ZrLanguageServer_IncrementalParser_UpdateOpenDocument`只允许closed synthetic snapshot到opened overlay的一次同版本转换。转换后仍执行严格单调版本门禁，same/stale update不会进入content allocation、change classification、parse或semantic work。
- `SZrLspWorkspaceEditDocumentSnapshot`按edited URI记录stable hash、length、version、`contentGeneration`和open state。`CaptureDocumentSnapshots`按URI去重，`ValidateDocumentSnapshots`完整重捕获，`FindDocumentSnapshot`只返回已捕获结果。
- opened document从incremental-parser content snapshot捕获。unopened document读取disk；若已有closed cache，则cache snapshot必须可获取且hash/length与disk一致。missing cache block、missing file、disk/cache漂移或任一指纹字段变化均使整批plan失败。
- 普通`textDocument/rename`在semantic locations生成后捕获并复验全部document，再把snapshot传给共用serializer。opened version 0写`TextDocumentEdit.version: 0`，unopened source写`version: null`，不再在序列化阶段读取live version。
- source rename的`CollectSourceRenameEditPlan`、`ValidateSourceRenameEditPlan`与`FindSourceRenameDocumentSnapshot`保留兼容API，但内部委托通用workspace-edit snapshot实现；ModuleIdentity、replacement与exact range契约不变。

## TDD与根因证据

- 第一轮RED在fresh GCC隔离源上固定两个缺口：incremental parser拒绝合法的version-zero `didOpen`；source rename把opened version-zero document误判为disk source，导致canonical edits缺失。
- 第二轮RED仅声明通用API，链接精确缺失`CaptureDocumentSnapshots`、`FindDocumentSnapshot`和`ValidateDocumentSnapshots`，证明此前只有source-rename专用实现。
- review RED删除unopened secondary cache的text block；旧disk fallback错误接受该plan，project test精确失败。GREEN收紧为：已有closed cache但snapshot不可获取时必须拒绝，不能绕过cache/disk一致性门禁。
- 最终incremental parser为`8/8`，interface为`90/90`，project features为`54/54`。stdio smoke在首次`didChange`前对client version-zero document执行rename并断言captured version为0，随后更新到version 2并继续验证既有rename行为。

## 工具链与回归证据

- 正式隔离源码为`.codex/snapshots/l6-general-rename-red-gcc-r1`，对应当时稳定HEAD加15个LSP code/test exact paths；15个文件与共享工作树SHA-256逐一一致，Syntax 03 M2 parser/core/test中间态未进入快照。
- GCC 11.4.0、Clang 14.0.0和MSVC 19.44.35228 / `VSCMD_VER=17.14.36`使用独立fresh static cache。每套同一16目标矩阵均16/16真实process exit 0，failure marker为0。
- 三套incremental parser均`8/8`、interface均`90/90`、project features均`54/54`、language feature matrix均`8/8`、project UTF-16 ranges均`3/3`、source contracts均`38/38`；parser query/facts、canonical consumers/graph、semantic analyzer/query、local query/hover和expression facts同轮通过。
- GCC、Clang和MSVC分别直接运行更新后的`tests/language_server/stdio_smoke.js`，并向对应stdio server传入同工具链CLI；三套真实exit 0。每个runner以真实process exit和独立marker扫描判定，不依赖外层shell变量转义。

## Snapshot、Schema与协议边界

- persistent document snapshot schema新增显式open provenance。`SZrLspWorkspaceEditDocumentSnapshot`仍是单次request内的瞬时POD；semantic fact、public-contract hash、cache key、artifact和binary schema均未改变。
- stable hash只证明内容一致，不是SymbolId、ModuleIdentity或semantic identity。rename target、replacement与range仍只来自canonical query/project facts，失败后不按member name、source text或URI猜测重建。
- 校验发生在server返回workspace edit之前。response交付后若client再次编辑，client仍必须按`TextDocumentEdit.version`拒绝stale apply。
- 本阶段没有采集p50/p95/p99、峰值内存、cancellation latency或并发snapshot压力报告，因此不晋级完整L6 robustness。

## 未完成边界

- 普通`textDocument/rename`和source-file rename已统一；code action、safe fix及其他workspace-edit producer尚未接入通用fingerprint API。
- package/alias/public import edge migration、`.zrp/.zrm` generation、binary/native/artifact provider replacement及public type/property/layout hash仍待后续。
- cancellation、100次乱序edit/race stress、partial reparse、多scope cache、workspace cache预算、性能百分位和峰值内存门禁仍未完成。
