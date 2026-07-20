---
plan_id: lsp-01-semantic-core
record_id: 2026-07-20-native-descriptor-function-callable-parity
status: completed
completed_at: 2026-07-20 19:30 +08:00
source_plans:
  - docs/plans/lsp/01-semantic-inference-core.md
  - docs/plans/lsp/05-implementation-blueprint.md
evidence_scope: native-descriptor-module-function-callable-parity
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_external_callable_contract.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_external_callable_contract.h
  - zr_vm_language_server/src/zr_vm_language_server/lsp_external_callable_signature_help.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_external_callable_signature_help.h
  - zr_vm_language_server/src/zr_vm_language_server/lsp_signature_help.c
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_metadata_provider.c
related_tests:
  - tests/language_server/descriptor_plugin_fixture_int.c
  - tests/language_server/descriptor_plugin_fixture_float.c
  - tests/language_server/test_lsp_project_features.c
  - tests/language_server/test_lsp_project_native_callable_signature_cases.h
  - tests/language_server/stdio_smoke.js
---

# Native Descriptor Function Callable Parity

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-07-20 19:30 +08:00 | 已完成 | native builtin与descriptor-plugin module function通过统一semantic query消费结构化function descriptor；hover/signature help共享canonical label、参数名/类型/文档和provider generation；descriptor热重载不复用旧contract；识别后缺少结构化字段时返回unavailable且禁止member-name/AST fallback；三工具链十六目标矩阵与三套stdio/CLI smoke完成且marker归零 |

## 已实现契约

- signature help从parsed call context提取精确callee member identifier range，再调用`ZrLanguageServer_LspSemanticQuery_ResolveAtPosition`。成功结果必须是native builtin或descriptor-plugin source kind、imported-member query及`FUNCTION` metadata member；LSP不接收或搜索独立member name。
- `SZrLspExternalCallableContract`直接投影当前`ZrLibFunctionDescriptor`的name、return type、ordered parameters、generic parameters和documentation。hover与signature help调用同一formatter，固定`combine(left: int, right: int): int`这类canonical label。
- signature parameter information保留descriptor参数名、类型和文档，并可附加现有argument semantic facts；active parameter仍由function-call参数范围计算。
- resolver使用`NOT_EXTERNAL / RESOLVED / UNAVAILABLE`三态。已解析native function若descriptor缺失、类型unknown、generic constraint尚无canonical formatter或buffer不足，会终止external consumer且不进入AST/name/text重建。
- native receiver method没有迁移。它继续使用既有closed-generic consumer，避免把`LinkedNode<int>`重新显示成raw descriptor的`LinkedNode<T>`。

## TDD与支持层证据

- isolated project RED先确认query已解析`zr.pluginprobe.combine`与两个structured parameters，但旧signature label为`combine(int, int): int`，参数名和descriptor文档丢失。失败层位于LSP consumer projection，而不是parser/native registry。
- 初版尝试统一function与method formatter后，既有closed-generic method hover marker失败。审计确认raw method descriptor没有closed owner substitution，因此撤销method迁移并把本子里程碑收紧为module function；原closed-generic测试重新转绿。
- project GREEN固定integer descriptor的query identity、label、两项parameter information与active parameter 1；替换为floating descriptor并reload owning project后，hover/signature同步变为`combine(left: float, right: float): float`。
- negative fixture发布`incomplete_callable(): unknown`；query仍识别native function，但signature help必须返回unavailable且result保持null，直接固定无AST/name fallback边界。
- stdio增加builtin `zr.system.gc.set_budget`协议用例，固定`textDocument/signatureHelp`与`textDocument/hover`共享`set_budget(microseconds: int): null`，并检查parameter documentation与didClose diagnostic cleanup。

## 工具链与回归证据

- 正式隔离快照为`HEAD 00deb0a + 11个LSP code/test exact paths`；11个文件与共享工作树逐一SHA-256一致，Syntax 03 parser/core/compiler/AOT/CMake/tests中间态未进入快照。
- GCC 11.4、Clang 14.0与MSVC 19.44.35228 / `VSCMD_VER=17.14.36`均fresh/build并运行同一16目标矩阵；每套16/16真实process exit 0，`Fail -`、`FAIL:`与`:FAIL:` marker为0。
- 每套project features为`52/52`，UTF-16 ranges为`3/3`，source contracts为`38/38`；parser query/facts、canonical consumers/graph、semantic analyzer/query、interface、incremental parser、language feature matrix与expression facts均在同一runner通过。
- GCC、Clang和MSVC分别直接运行更新后的`tests/language_server/stdio_smoke.js`；三套language-server stdio与CLI子进程真实exit 0。runner没有使用会被外层PowerShell提前展开的bash`$?`或`$code`。
- 新增`lsp_external_callable_*` translation units在Clang/MSVC构建无warning。Clang日志中的`lsp_signature_help.c`与`lsp_metadata_provider.c` unused-function warning属于该隔离HEAD既有函数，不由本次新增模块产生。

## Snapshot、Schema与协议边界

- 本阶段不修改parser semantic fact、artifact、native descriptor ABI、document snapshot、schema generation或cache key。LSP只消费既有current-generation `SZrLspResolvedMetadataMember.functionDescriptor`。
- descriptor pointer不跨query生命周期缓存。插件reload后重新运行semantic query，确保provider generation变化进入新contract。
- 直接覆盖协议为`textDocument/signatureHelp`与`textDocument/hover`；project fixture额外覆盖watched provider reload。
- 本阶段没有p50/p95/p99、峰值内存、cancellation latency或snapshot race压力报告，因此不晋级完整L6 robustness。

## 未完成边界

- native receiver method需要closed structured callable contract；当前不得直接格式化raw method descriptor。
- descriptor generic constraints、constructors、properties及meta-members尚未进入统一external callable query。
- binary module function与native module function仍未完全共享同一provider contract adapter。
- public type/property/layout hash、package/alias export迁移、artifact/provider replacement、snapshot race、cancellation及性能/内存预算仍待后续子里程碑。
