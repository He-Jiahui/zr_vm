---
plan_id: lsp-03-robustness
record_id: 2026-07-20-source-rename-workspace-edits
status: completed
completed_at: 2026-07-20 18:13 +08:00
source_plans:
  - docs/plans/lsp/01-semantic-inference-core.md
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
evidence_scope: source-rename-workspace-edits
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_source_rename.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_navigation.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_internal.h
  - zr_vm_language_server/stdio/stdio_workspace_files.c
  - zr_vm_language_server/stdio/stdio_rename.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio_internal.h
related_tests:
  - tests/language_server/test_lsp_project_features.c
  - tests/language_server/test_lsp_project_source_rename_edit_cases.h
  - tests/language_server/stdio_smoke.js
---

# Canonical Source Rename Workspace Edits

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-07-20 18:13 +08:00 | 已完成 | `workspace/willRenameFiles`按old project record与new canonical path ModuleIdentity生成source rename workspace edit；精确更新provider `%module` declaration、opened importer及unopened source-root importer；批量操作统一输出`changes + documentChanges`；同URI/跨root/collision/unknown/no-location返回无编辑；三工具链十六目标矩阵与三套stdio/CLI smoke完成且marker归零 |

## 已实现契约

- `ZrLanguageServer_LspProject_CollectSourceRenameEdits`复用`PrepareSourceRename`的只读resolution前置条件：old URI必须持有project source record，old/new必须是不同`.zr` URI，new path必须位于同一source root且无URI record碰撞。replacement text只由`ZrLibrary_Project_DeriveCurrentModuleKey`生成。
- collection不修改record URI/path、analyzer、incremental parser、dependency graph或filesystem。真正的状态迁移仍只发生在client应用edit并完成物理rename后的`workspace/didRenameFiles`。
- provider declaration只在parsed `%module` string value等于record canonical old ModuleIdentity时进入结果；quoted literal只替换内部value range。importer通过既有project navigation解析的import binding和精确`modulePathLocation`匹配canonical old identity，不扫描raw source text。
- project traversal覆盖source root下opened与unopened `.zr`文件。stdio将每个supported batch item附加到同一个workspace edit，同时发布`changes`与version-aware `documentChanges`；unsupported item跳过，零有效编辑或serialization失败返回JSON `null`。

## TDD与根因证据

- focused project RED在加入provider/open importer/unopened importer exact-range断言后链接失败，唯一缺失符号为`ZrLanguageServer_LspProject_CollectSourceRenameEdits`，证明project层没有rename edit collection contract。
- stdio RED在既有ModuleIdentity rename fixture上精确失败为`workspace/willRenameFiles must edit the opened old-edge import specifier`，证明旧handler固定返回`null`。
- 初版GREEN真实process exit为0但仍命中1个test marker。诊断枚举3个canonical location后确认import ranges正确，而provider string literal range包含引号；最低层修正为只投影semantic string value范围，随后focused project marker归零。
- 最终project case精确固定provider declaration、opened importer、unopened importer三处`legacy -> modern`范围，并固定same URI不产生edit。stdio smoke固定两个open importer与provider declaration合计3个`documentChanges`，随后继续通过既有`didRenameFiles` hover/definition检查。

## 工具链与回归证据

- 正式验证快照为`HEAD d52bd4f + 8个tracked LSP code/test paths + 1个new LSP test path`；9个文件与共享工作树SHA-256完全一致，Syntax 03 parser/core/AOT/CMake/tests中间态未进入快照。
- GCC 11.4、Clang 14.0和MSVC 19.44.35228 / `VSCMD_VER=17.14.36`均构建并运行同一16目标矩阵；每套16/16真实process exit 0，日志中`Fail -`、`FAIL:`和`:FAIL:` marker为0。
- 每套project features为`51/51`，descriptor/UTF range为`3/3`，source contracts为`38/38`；parser query/facts、canonical consumers/graph、semantic analyzer/query、interface、incremental parser、language feature matrix和expression facts均在同一runner内通过。
- GCC、Clang和MSVC分别直接运行`tests/language_server/stdio_smoke.js`；三套language-server stdio与CLI进程真实exit 0。MSVC使用fresh static cache；三工具链新增rename模块无新增warning。

## Snapshot、Schema与协议边界

- 本阶段不改变document snapshot schema、semantic fact schema、public-contract hash schema v1、cache key或artifact schema。WorkspaceEdit只是既有canonical source/import facts的协议投影。
- `willRenameFiles`返回的provider URI仍是old URI，importer URI保持各自document identity；open importer携带known version，unopened importer的`TextDocumentEdit` version为null。
- 直接覆盖的协议能力是`workspace/willRenameFiles`，并与后续`workspace/didRenameFiles`的old/new ModuleIdentity迁移组成完整source rename workflow。
- 本阶段没有采集p50/p95/p99、峰值内存、cancellation latency或snapshot race压力报告，因此不晋级完整L6 robustness。

## 未完成边界

- property、constructor、meta-member及imported/native callable target identity仍需统一query shape，不得由LSP按名称或文本兼容。
- public type/property/layout/import hash、package/alias export迁移、`.zrp/.zrm` generation和binary/native/artifact provider replacement仍待后续。
- workspace edit在client apply前的document version/checksum复验、partial reparse、多scope cache、snapshot race、cancellation、workspace cache预算、性能百分位和峰值内存门禁仍未完成。
