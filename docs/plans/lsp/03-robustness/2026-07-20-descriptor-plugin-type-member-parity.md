---
plan_id: lsp-03-robustness
record_id: 2026-07-20-descriptor-plugin-type-member-parity
status: completed
completed_at: 2026-07-20 15:15 +08:00
source_plans:
  - docs/plans/lsp/01-semantic-inference-core.md
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
evidence_scope: descriptor-plugin-type-member-parity
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_descriptor_metadata_coordinates.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_navigation.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
related_tests:
  - tests/language_server/test_lsp_project_features.c
  - tests/language_server/test_lsp_project_utf16_ranges.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_smoke.js
---

# Descriptor Plugin Type Member Parity

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-07-20 15:15 +08:00 | 已完成 | descriptor-plugin receiver type member通过canonical receiver/type metadata fact进入completion、definition、references和documentHighlight；receiver resolver优先于通用import-chain；显式descriptor compact-coordinate adapter保留member declaration identity；valid v1到incomplete v2的last-good AST completion边界；三工具链十六目标矩阵及三套stdio/CLI smoke完成且marker归零 |

## 已实现契约

- `ZrLanguageServer_LspSemanticQuery_ResolveAtPosition`先尝试`semantic_query_resolve_receiver_type_member_target`，再进入通用import-chain。成功结果来自analyzer推断的receiver type和metadata provider发布的resolved member fact；不按member name、display text或raw AST重建target。
- receiver resolution失败时仍继续现有import-chain、import binding/alias、external metadata和local symbol路径。优先级只提升更具体的receiver fact，不删除fallback。
- descriptor provider已通过compact virtual records发布稳定member declaration URI/range。`ZrLanguageServer_Lsp_TryRangeFromDescriptorMetadataCoordinates`校验positive/ordered one-based structural range并转换为zero-based LSP range。
- semantic query与project navigation仅在`sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_DESCRIPTOR_PLUGIN`时使用descriptor adapter。binary metadata继续使用独立adapter，普通document helper继续要求content snapshot。
- completion先在valid version 1的`point.x`上建立精确receiver事实，再更新为incomplete version 2的`point.;`并复用既有last-good AST。首版本无有效receiver declaration时不会新增LSP文本推断旁路。

## TDD与根因证据

- 初始project RED在`point.;`首版本只返回generic completion labels，`x/y`缺失。GDB证明parser接受了partial AST但该AST没有保留`point` declaration；这不是metadata provider缺member，而是测试绕过了已有valid-to-incomplete snapshot合同。测试改为valid v1再更新incomplete v2，固定真实编辑器恢复路径。
- valid source上的completion转绿后，field definition仍返回plugin `.so [0:0-0:0]`。查询调试证明同一位置存在精确descriptor field fact，但旧解析顺序先被通用import-chain消费为module entry；把receiver fact置于import-chain之前后，query保留field kind、descriptor source kind和compact declaration range。
- query raw range精确但outbound location仍为`0:0`，最低失败层是普通document helper无法为physical plugin URI取得text snapshot。没有恢复generic no-content fallback，也没有复用binary artifact adapter；新增21行descriptor-only structural adapter并由两条consumer路径显式门禁。
- project fixture最终覆盖field/method的source-to-declaration definition、双向references、source/declaration documentHighlight、valid与incomplete snapshot completion；UTF fixture固定`2:9-2:10 -> 1:8-1:9`和invalid rejection；source contract固定descriptor helper与binary adapter隔离。

## 工具链与回归证据

- 正式源码快照为`HEAD 2275876 + 7个LSP code/test exact paths`，与共享工作树中的Syntax 03 parser/core/AOT/CMake中间态隔离。GCC、Clang和MSVC三套build都指向同一snapshot，新增glob模块触发各自CMake reconfigure并实际编译。
- GCC 11.4、Clang 14和MSVC 19.44.35228均构建并运行同一16目标矩阵；每套16/16真实process exit 0，所有日志中`Fail -`、`FAIL:`和`:FAIL:` marker为0。
- parser leaf每套包括semantic query 26/26、compiler query diagnostics 18/18、semantic facts 12/12、canonical consumers 10/10、canonical type graph 19/19和expression facts 28/28。
- LSP每套包括semantic analyzer 46项、query diagnostics 14项、interface 90项、local semantic query 32项、local hover 12项、incremental parser 7项、project features 49/49、language feature matrix 8项、descriptor/UTF range 3/3和source contracts 38/38。
- GCC、Clang和MSVC分别直接运行`tests/language_server/stdio_smoke.js`，三套language-server stdio与CLI进程均真实exit 0。验证脚本逐进程读取exit code并拒绝failure marker，没有使用会被外层PowerShell提前展开的bash`$?`/`$code` wrapper。
- MSVC configure保留既有长object-path warning，binary adapter保留既有C4701 warning；本轮新descriptor adapter和修改的query/navigation/test translation units在三工具链均无新增warning。

## Snapshot、Schema与协议边界

- tested source baseline为`2275876 + LSP exact paths`；project fixture先打开version 1 semantic snapshot，再用version 2 incomplete member edit验证last-good AST。snapshot generation、schema generation和cache key未改变。
- descriptor ABI/schema、native registry和parser semantic fact没有修改。本阶段只消费既有type/member descriptor fact并修复LSP query/projection顺序。
- 直接覆盖的协议能力是`textDocument/completion`、`textDocument/definition`、`textDocument/references`和`textDocument/documentHighlight`；stdio smoke继续覆盖initialize、open/change/request/close及CLI进程。
- 本子里程碑没有采集p50/p95/p99、峰值内存、cancellation latency或snapshot race压力报告，因此不晋级完整L6 robustness。

## 未完成边界

- property、constructor、meta-member及imported/native callable target identity仍需统一query shape，不得由LSP按member name兼容。
- compact physical-plugin range当前只对ASCII member-name fixture声明精确。若descriptor允许非ASCII synthetic names，需要provider发布encoding-aware coordinate map或rendered content identity。
- public type/property/layout hash、ModuleIdentity edge migration、binary/native/artifact全面provider parity和public import变化的reverse-dependency传播仍未完成。
- partial reparse、多scope cache、snapshot race、cancellation、workspace cache预算、性能百分位和峰值内存门禁仍未完成。
