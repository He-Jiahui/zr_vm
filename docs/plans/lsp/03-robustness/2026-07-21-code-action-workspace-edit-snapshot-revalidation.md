---
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/conf.h
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_workspace_edit_snapshot.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_workspace_edit_snapshot.h
  - zr_vm_language_server/stdio/stdio_editing.c
  - zr_vm_language_server/stdio/stdio_editing_json.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio_internal.h
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_workspace_edit_snapshot.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_workspace_edit_snapshot.h
  - zr_vm_language_server/stdio/stdio_editing.c
  - zr_vm_language_server/stdio/stdio_editing_json.c
plan_sources:
  - user: 2026-07-21 strict LSP semantic inference plan execution with per-submilestone records and commits
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
tests:
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_advanced_editor_features.c
  - tests/language_server/stdio_smoke.js
doc_type: milestone-detail
plan_id: lsp-03-robustness
record_id: 2026-07-21-code-action-workspace-edit-snapshot-revalidation
status: completed
completed_at: 2026-07-21 02:45 +08:00
source_plans:
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
evidence_scope: code-action-workspace-edit-snapshot-revalidation
---

# Code Action Workspace Edit Snapshot Revalidation

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-07-21 02:45 +08:00 | 已完成 | `textDocument/codeAction`在生成edit前捕获单document fingerprint、生成后复验并只序列化captured version；URI/version/generation/open-state/length/hash进入opaque `CodeAction.data.snapshot`；`codeAction/resolve`再次复验，stale或malformed token会删除edit并返回disabled reason，禁止按title/kind/text重建；三工具链十七目标矩阵和三套stdio/CLI smoke真实exit 0、marker归零 |

## 已实现契约

- 通用workspace-edit模块新增`CaptureDocumentSnapshot`和`ValidateDocumentSnapshot`单document API；原批量API委托同一实现，不再维护两套字段比较逻辑。
- stdio code-action handler在range转换和`GetCodeActions`之前捕获请求URI的fingerprint。semantic/editor producer完成后必须与当前document再次完全匹配，否则返回空action数组，不序列化mixed-generation edit。
- `documentChanges`只使用捕获时的open provenance与version。合法client version 0写入`TextDocumentEdit.version: 0`；closed disk source不伪造versioned document change。
- 每个返回action的opaque `data.snapshot`包含version、`contentGeneration`、content length、显式open state和固定16位十六进制stable hash。hash使用string避免`uint64`经JSON double丢精度；其他整数必须在`SIZE_MAX`和JSON safe-integer范围内，否则整项序列化失败。
- `codeAction/resolve`只解析并复验server此前返回的fingerprint。完全匹配时原样保留edit；version、generation、open state、length或hash任一漂移，以及missing/malformed snapshot，都会删除edit并返回`disabled.reason = "Document changed since this code action was computed"`。
- resolve不按action title、kind、replacement text、diagnostic message或当前source重新定位/重建edit。canonical code-action producer仍负责原有organize-import、remove-unused、missing-import和semicolon范围；本阶段只增加提交边界。

## TDD与根因证据

- interface RED只在test中声明wished-for单document API；fresh GCC构建到最终link后精确缺失`ZrLanguageServer_LspWorkspaceEdit_CaptureDocumentSnapshot`和`ValidateDocumentSnapshot`，没有其他test-source错误。
- stdio RED用旧server运行更新后的协议脚本，真实exit 1并精确失败为`textDocument/codeAction must attach exact version-zero snapshot resolve data`，证明旧handler只附加title/kind/URI并读取live version。
- GREEN固定version-zero organize-import action的captured `documentChanges.version`和opaque snapshot；同内容推进到version 1后，旧action resolve必须无edit且disabled，fresh action重新携带version 1并可正常resolve。
- 首次GREEN smoke在执行协议前因fresh cache未构建descriptor-plugin fixture退出，该轮不计行为证据；补建既有int/float fixtures后原样重跑exit 0。focused interface为90 Pass、advanced editor为39 Pass，marker为0。

## 工具链与回归证据

- 正式隔离源码为`.codex/snapshots/l6-code-action-red-gcc-r1`，对应稳定`HEAD 479bd3b`加8个LSP code/test exact paths；8个文件与共享工作树SHA-256逐一一致，Syntax 03 M2 core/parser/AOT/test中间态未进入快照。
- GCC 11.4.0、Clang 14.0.0和MSVC 19.44.35228 / `VSCMD_VER=17.14.36`使用独立static cache。每套运行原16目标矩阵并增加advanced-editor target，共17/17真实process exit 0；51个测试日志均有汇总，`Fail -`、`FAIL:`与`:FAIL:` marker为0。
- 三套interface均90 Pass、advanced editor均39 Pass；semantic query/facts、canonical consumers/graph、semantic analyzer/query、local query/hover、incremental parser、project features、language feature matrix、UTF-16 ranges、source contracts和expression facts同轮通过。
- GCC、Clang和MSVC分别直接运行同一更新后的`stdio_smoke.js`，并传入对应工具链stdio server与CLI；三套真实exit 0、stdout/stderr日志为空。测试二进制或Node进程均作为各自runner的真实命令执行，没有使用会被外层shell提前展开的bash exit变量。

## Snapshot、Schema与协议边界

- 本阶段不改变persistent document snapshot、semantic fact、public-contract hash、cache key、artifact或binary schema。新增字段只存在于request-scoped fingerprint POD和opaque `CodeAction.data`往返token。
- stable hash只用于内容漂移检测，不是semantic identity。code-action edit仍来自既有producer；snapshot失败不会触发name/text/diagnostic fallback。
- 初始response返回后若client不调用resolve而直接应用edit，client仍必须按`TextDocumentEdit.version`拒绝stale apply；server无法在response交付后重校验client本地状态。
- 本阶段没有采集p50/p95/p99、峰值内存、cancellation latency或并发snapshot压力报告，因此不晋级完整L6 robustness。

## 未完成边界

- 普通rename、source-file rename和当前code-action workspace edits已接入fingerprint门禁；parser diagnostic safe-fix、workspace command及其他workspace-edit producer仍待统一。
- package/alias/public import edge migration、`.zrp/.zrm` generation、binary/native/artifact provider replacement及public type/property/layout hash仍待后续。
- cancellation、100次乱序edit/race stress、partial reparse、多scope cache、workspace cache预算、性能百分位和峰值内存门禁仍未完成。
