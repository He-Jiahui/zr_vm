---
plan_id: lsp-03-robustness
record_id: 2026-07-20-binary-export-declaration-identity
status: completed
completed_at: 2026-07-20 14:16 +08:00
source_plans:
  - docs/plans/lsp/01-semantic-inference-core.md
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
evidence_scope: binary-export-declaration-identity
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_binary_metadata_coordinates.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_navigation.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
related_tests:
  - tests/language_server/test_lsp_project_features.c
  - tests/language_server/test_lsp_project_utf16_ranges.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_smoke.js
---

# Binary Export Declaration Identity

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-07-20 14:16 +08:00 | 已完成 | binary typed-export declaration的精确artifact range通过统一semantic query进入definition/references/documentHighlight；显式binary metadata坐标adapter在有snapshot时按byte offset转换UTF-16、无snapshot时保留structural coordinates；source usage与`.zro` declaration双向消费同一fact且不按member name重建；三工具链十六目标矩阵及三套stdio/CLI smoke完成 |

## 已实现契约

- artifact producer、reader、direct resolver和`SZrLspSemanticQuery.resolvedMember`原本已保留typed-export declaration URI与精确one-based byte line/column range。本阶段不改变该fact，只修复LSP projection边界丢失identity的问题。
- `ZrLanguageServer_Lsp_TryRangeFromBinaryMetadataCoordinates`仅在权威`sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_BINARY_METADATA`时使用。有document snapshot时先从artifact line/byte-column重建byte offset，再调用统一UTF-16 codec；无snapshot时把one-based structural coordinate投影为zero-based LSP coordinate。
- `ZrLanguageServer_Lsp_TryFilePositionFromBinaryMetadataCoordinates`提供逆向查询：有snapshot时走统一UTF-16-to-byte codec，无snapshot时进入artifact one-based coordinate space。负数、溢出、零列、逆序或无法映射的range返回unavailable，不伪造`0:0` declaration。
- definition、references和documentHighlight从source usage或`.zro` declaration发起时均消费同一typed-export declaration fact。普通source/native/diagnostic路径继续要求content snapshot，不增加generic no-content fallback，也不按member name、display text或AST重建binary declaration。

## TDD与根因证据

- 初始project RED中，definition和references已经命中正确`.zro` URI但range为`[0:0-0:0]`，从binary declaration发起的documentHighlight为0项。逐层诊断证明artifact reader、direct resolver和semantic query中的raw declaration range始终精确，丢失发生在普通document helper无法为未打开`.zro`取得text snapshot之后。
- 通用no-content fallback会把UTF-8 byte column泄漏成UTF-16 character，因此没有修改普通document helper。GREEN实现被隔离在126行的`lsp_binary_metadata_coordinates.c`，并由binary `sourceKind`在semantic query与project navigation两条consumer路径显式门禁。
- project fixture最终固定source usage和`.zro` declaration的双向definition/references/documentHighlight。UTF-16 fixture在binary source前缀包含`lambda`多字节字符时，断言raw metadata range为`[1:18-1:28]`、LSP range为`[0:16-0:26]`。
- source-contract测试固定两个adapter名称、snapshot acquisition和两条binary `sourceKind`门禁，同时继续禁止普通document helper恢复旧no-content API；三工具链均为37项`PASS:`。

## 工具链与回归证据

- 正式证据使用隔离snapshot：`HEAD 32996f6 + 11个LSP exact paths`。构造后核对34个tracked dirty path：11个自有路径逐字节等于工作区，其余23个均逐blob等于`32996f6`；5个Syntax 03 M1 untracked parser文件未进入snapshot。因此矩阵没有消费并行任务的未提交parser/core/CMake源码。
- GCC 11.4、Clang 14和MSVC 19.44.35228 (`VSCMD_VER=17.14.36`)均fresh configure、构建并运行同一16目标矩阵，三套均为16/16真实process exit 0。parser leaf包括semantic query 26/26、compiler query diagnostics 18/18、semantic facts 12/12、canonical consumers 10/10、canonical type graph 19/19和expression facts 28/28。
- LSP focused结果每套均包括semantic analyzer 46项、query diagnostics 14项、interface 90项、local semantic query 32项、local hover 12项、incremental parser 7项、UTF-16 2项和source contracts 37项；均无新增failure marker。
- 每套project features均精确为48个`Pass -`和1个`Fail -`。唯一marker是`LSP Descriptor Plugin Type Member Navigation`；binary metadata hover/completion、references和document highlights三项均已转为Pass，未新增或扩大marker白名单。
- GCC、Clang和MSVC分别直接运行`tests/language_server/stdio_smoke.js`，三套stdio/CLI smoke均真实exit 0。空日志只与真实process exit配对计证据；未使用会被外层PowerShell提前展开的bash`$?`/`$code` wrapper。
- 作废证据不计入验收：首次GCC configure只暴露隔离snapshot的Unity目录嵌套；首次MSVC build只暴露CLI CMake target应为`zr_vm_cli_executable`。修正snapshot依赖布局与target名称后，最终r2三套均从fresh configure/build取得上述结果。

## Snapshot、Schema与协议边界

- tested source baseline为`32996f6 + LSP exact paths`；测试打开的source/binary document使用version 1 snapshot。普通document generation、semantic generation和cache门禁未改变。
- artifact继续使用既有compiled binary patch v34与typed-export declaration range。本阶段没有修改artifact writer/reader schema、没有schema version bump，也没有从LSP生成替代metadata。
- 直接覆盖的协议能力是`textDocument/definition`、`textDocument/references`（含`includeDeclaration=true`）和`textDocument/documentHighlight`；stdio smoke继续覆盖initialize、open、request和close的真实协议进程。
- 本子里程碑没有采集p50/p95/p99或峰值内存，也没有运行cancellation、snapshot race或workspace LRU压力报告，因此不晋级完整L6 robustness。

## 未完成边界

- descriptor-plugin receiver type-member completion/navigation仍保留1个明确marker，后续必须从descriptor fact/completion provider修复，不能由binary adapter兼容。
- 无source snapshot时，artifact只有byte column而没有source text或UTF-16 line map。当前structural projection对ASCII前缀精确；非ASCII前缀的精确UTF-16宽度需要未来artifact encoding map、source digest/text或显式UTF-16 columns。
- 本记录只关闭Q5/Q6中首个binary export metadata-location parity切片及L2/L6对应consumer边界，不表示`.zri/.zro/.zrm`、native descriptor、property/constructor/meta callable、public type/layout或全部source/binary/native query shape已对等。
- 主document partial reparse、多scope cache、snapshot race、cancellation、workspace cache预算及性能门禁仍未完成。
