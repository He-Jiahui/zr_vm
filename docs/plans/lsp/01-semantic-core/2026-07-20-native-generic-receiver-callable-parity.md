---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_external_callable_contract.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_external_callable_contract.h
  - zr_vm_language_server/src/zr_vm_language_server/lsp_external_callable_signature_help.c
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_generic_calls.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_external_callable_contract.c
plan_sources:
  - user: 2026-07-20 严格执行 LSP semantic inference 计划并逐子里程碑记录产出
  - docs/plans/lsp/01-semantic-inference-core.md
  - docs/plans/lsp/05-implementation-blueprint.md
tests:
  - tests/parser/test_type_inference.c
  - tests/parser/test_canonical_consumers.c
  - tests/language_server/descriptor_plugin_fixture_int.c
  - tests/language_server/descriptor_plugin_fixture_float.c
  - tests/language_server/test_lsp_project_features.c
  - tests/language_server/test_lsp_project_native_receiver_callable_cases.h
  - tests/language_server/stdio_smoke.js
doc_type: milestone-detail
plan_id: lsp-01-semantic-core
record_id: 2026-07-20-native-generic-receiver-callable-parity
status: completed
completed_at: 2026-07-20 22:33 +08:00
source_plans:
  - docs/plans/lsp/01-semantic-inference-core.md
  - docs/plans/lsp/05-implementation-blueprint.md
evidence_scope: native-unconstrained-generic-receiver-callable-parity
---

# Native Generic Receiver Callable Parity

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-07-20 22:33 +08:00 | 已完成 | descriptor-plugin unconstrained generic instance method通过exact metadata identity与parser closed callable TypeId进入统一external callable contract；hover/signature help共享`fn echo<T>(value: int): int`，generic/parameter names来自structured descriptor且closed types/receiver effect来自canonical fact；constraint-bearing与字段缺失合同保持unavailable且没有member-name、AST或文本替换fallback；三工具链十六目标矩阵及三套stdio/CLI smoke真实exit 0、marker归零 |

## 已实现契约

- `ZrLanguageServer_LspExternalCallableContract_FromResolvedMethod`只接收同一callee range解析出的native `METHOD` descriptor和`ZrParser_SemanticQuery_CallAt`发布的canonical function `TypeId`。LSP不按owner/member name重新查找方法。
- descriptor generic clause只消费`ZrLibGenericParameterDescriptor.name`。每个generic parameter必须有结构化非空name且`constraintTypeCount == 0`；缺数组、缺name或存在constraint时返回unavailable。
- descriptor继续提供当前provider generation的method identity、generic names、parameter names和documentation。closed parameter/return types、passing/escape以及readonly/mutable receiver effect全部来自parser canonical function fact。
- formatter让module function与receiver method共享generic clause输出。mutable receiver固定显示`fn echo<T>(value: int): int`；没有读取raw `T`来推断或替换closed `int`。
- metadata semantic query仍可结构化观察constraint-bearing method；callable hover/signature consumer明确拒绝它。这保留了availability边界，而不是把unsupported contract伪造成可调用签名。

## TDD与支持层证据

- descriptor fixtures同时发布unconstrained `echo<T>(value: T): T`、constraint-bearing `constrained_echo<T>`和既有incomplete method。project test先固定parser `CallAt/FormatCall`，再固定LSP semantic query、hover、signature help及negative availability。
- 首轮support-first RED暴露generic struct registration缺口；parser层以`04b917ba`修复registration并以`20a94017`清理MSVC shadow warning。随后`fe8bd1b7`发布native generic receiver inferred/explicit binding、closed callable TypeId和structured parameter names。
- 在稳定`fe8bd1b7 + 4个LSP RED路径`全新GCC static cache中，直接parser gate已通过，唯一Unity失败为`Descriptor generic receiver callable mismatch (kind=4 label=fn total(): int)`，证明剩余缺口仅在LSP projection。
- LSP GREEN只修改external callable adapter：结构化校验method generic parameters、借用generic array并让method进入既有generic formatter。project features随后为`54/54`且marker为0。
- stdio创建真实descriptor-plugin临时project，加载构建产物旁的plugin fixture，打开`point.echo(1)`，断言零diagnostics、signature help与hover共享exact label和documentation，并验证didClose清理。

## 工具链与回归证据

- 正式隔离源码为`HEAD fe8bd1b7224ae7e7dd8263167781a3f4ad49d430 + 5个LSP code/test exact paths`，目录`.codex/snapshots/q6-native-generic-fe8bd1-red-r1`。5个文件与共享工作树逐一SHA-256一致；Syntax 03 M2 parser/AST中间态未进入快照。
- GCC 11.4.0、Clang 14.0.0和MSVC 19.44.35228 / `VSCMD_VER=17.14.36`分别使用独立fresh static cache。每套同一16目标矩阵均16/16真实process exit 0，`Fail -`、`FAIL:`与`:FAIL:` marker为0。
- 三套project features均`54/54`、interface均`90/90`、language feature matrix均`8/8`、UTF-16 ranges均`3/3`、source contracts均`38/38`；parser query/facts、canonical consumers/graph、semantic analyzer/query、local query/hover、incremental parser和expression facts同轮通过。
- GCC、Clang和MSVC分别直接执行更新后的`tests/language_server/stdio_smoke.js`，并向对应stdio server传入同工具链CLI；三套真实exit 0且server stderr为空。runner未使用会被外层PowerShell提前展开的bash`$?`或`$code`。
- 生产改动通过Clang与MSVC `/W4`编译，无新增warning。全新缓存中的既有unused、descriptor initializer和第三方warning不属于本阶段改动。

## Snapshot、Schema与协议边界

- 本阶段不修改native descriptor ABI、parser public query API、artifact schema、snapshot generation或cache key。consumer只在当前query生命周期内借用descriptor和canonical context，不跨provider generation缓存指针。
- 直接协议覆盖为`textDocument/signatureHelp`、`textDocument/hover`、`textDocument/publishDiagnostics`和didClose。project fixture同时覆盖current provider identity、structured constraints与incomplete descriptor。
- 本阶段没有p50/p95/p99、峰值内存、cancellation latency或snapshot race压力报告，因此不晋级完整L6 robustness。

## 未完成边界

- constraint-bearing generic receiver methods仍缺canonical constraint display；effectful、bare/static method、constructor和meta-member尚未统一到同一callable adapter。
- property/accessor parity尚未实现。当前native descriptor ABI和public semantic query没有property identity及get/set/ref-get effect contract；在LSP按member name补齐会违反Q4/Q6 no-fallback要求。
- binary callable provider、public type/property/layout hash、package/alias export传播、artifact/provider replacement、snapshot race、cancellation及性能/内存预算仍待后续子里程碑。
