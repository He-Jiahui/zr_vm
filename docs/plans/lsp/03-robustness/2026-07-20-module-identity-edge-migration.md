---
plan_id: lsp-03-robustness
record_id: 2026-07-20-module-identity-edge-migration
status: completed
completed_at: 2026-07-20 17:11 +08:00
source_plans:
  - docs/plans/lsp/01-semantic-inference-core.md
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
evidence_scope: module-identity-edge-migration
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_source_rename.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_internal.h
  - zr_vm_language_server/stdio/stdio_workspace_files.c
related_tests:
  - tests/language_server/test_lsp_project_features.c
  - tests/language_server/test_lsp_project_module_identity_edge_cases.h
  - tests/language_server/stdio_smoke.js
---

# ModuleIdentity Edge Migration

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-07-20 17:11 +08:00 | 已完成 | `workspace/didRenameFiles`对同一project source root内的`.zr` rename保留旧canonical module/hash snapshot；public-contract refresh同时传播removed与added ModuleIdentity反向边；重叠importer按URI去重且只重分析一次；新增边hover/definition更新；三工具链十六目标矩阵及三套stdio/CLI smoke完成且marker归零 |

## 已实现契约

- `ZrLanguageServer_LspProject_PrepareSourceRename`只接受已有project source record、同source root `.zr` old/new URI和无record碰撞的新路径。成功后移除旧URI analyzer/parser entry，只迁移record URI/path，保留旧`moduleName`、public-contract hash/count/availability作为下一次标准更新的previous snapshot。
- 新URI仍通过`ZrLanguageServer_Lsp_UpdateDocument`执行parse、project analysis、canonical module key验证和public-contract query。LSP不从文件名、raw import literal、member name、display text或AST spelling重建语义身份。
- `project_refresh_transitive_importers`在previous/current module identity不同时依次播种两条反向边，并共享一个`discovered` URI集合。单个document同时import old/new identity时只进入queue一次；transitive importer继续按刷新后的record canonical module name传播。
- stdio `workspace/didRenameFiles`只在上述准备成功时使用迁移路径并清理旧URI diagnostics；未知record、跨project/source-root移动、非`.zr`文件或碰撞继续走既有delete/create fallback。

## TDD与根因证据

- 首个内容编辑RED尝试在`legacy.zr`内把`%module "legacy"`改为`%module "modern"`，被canonical project resolver正确拒绝，因为显式module key必须匹配project relative path。该错误前提未作为实现依据。
- 合法RED改为物理`legacy.zr -> modern.zr` rename并同步canonical `%module`。测试先以缺失`ZrLanguageServer_LspProject_PrepareSourceRename`链接失败，证明现有didRename delete/create路径没有old/new snapshot桥接。
- GREEN保留old record contract直到new URI更新：old-only importer执行次数`+1`；同时import old/new identity的overlap importer执行次数也仅`+1`；累计和latest reverse-dependency reanalysis均精确`+2/2`。
- overlap importer的`cached` hover从未解析added edge更新为`float`。真实stdio notification随后固定同一hover和`modern.value()` definition指向`modern.zr`。

## 工具链与回归证据

- 正式源码快照为`HEAD 229022f + 5个tracked LSP code/test paths + 2个new LSP code/test paths`。共享工作树的Syntax 03 parser/core/AOT/CMake/tests中间态均按HEAD内容进入snapshot，未作为本阶段输入。
- GCC 11.4、Clang 14.0和MSVC 19.44.35228 / `VSCMD_VER=17.14.36`均构建并运行同一16目标矩阵；每套16/16真实process exit 0，日志中`Fail -`、`FAIL:`和`:FAIL:` marker为0。
- 每套project features为`50/50`，descriptor/UTF range为`3/3`，source contracts为`38/38`。parser leaf、semantic analyzer/query、interface、incremental parser、language feature matrix和expression facts均在同一runner内通过。
- GCC、Clang和MSVC分别直接运行`tests/language_server/stdio_smoke.js`；三套language-server stdio与CLI进程真实exit 0，实际覆盖`workspace/didRenameFiles`、added-edge hover和renamed-source definition。
- MSVC使用fresh static cache；configure保留既有long object-path warning，新增rename模块在三工具链无新增warning。

## Snapshot、Schema与协议边界

- rename preparation只桥接old/new project record identity；document snapshot、semantic schema、public-contract hash schema v1和cache key未改变。
- old analyzer和incremental parser entry在record迁移前移除；new URI必须经标准parse/semantic/project pipeline重新生成facts，禁止把old analyzer改挂到new URI继续使用。
- 直接覆盖的协议能力是`workspace/didRenameFiles`，并通过后续`textDocument/hover`、`textDocument/definition`和diagnostics发布观察结果。
- 本阶段没有采集p50/p95/p99、峰值内存、cancellation latency或snapshot race压力报告，因此不晋级完整L6 robustness。

## 未完成边界

- `workspace/willRenameFiles`尚未生成import specifier workspace edits；调用方仍需同步源文件声明和import文本。
- property、constructor、meta-member及imported/native callable target identity仍需统一query shape，不得由LSP按名称或文本兼容。
- public type/property/layout/import hash、package/alias export迁移、`.zrp/.zrm` generation、binary/native/artifact provider replacement仍待后续。
- partial reparse、多scope cache、snapshot race、cancellation、workspace cache预算、性能百分位和峰值内存门禁仍未完成。
