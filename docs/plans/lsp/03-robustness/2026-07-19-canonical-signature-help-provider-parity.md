---
plan_id: lsp-03-robustness
record_id: 2026-07-19-canonical-signature-help-provider-parity
status: completed
completed_at: 2026-07-19 22:16 +08:00
source_plans:
  - docs/plans/lsp/01-semantic-inference-core.md
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
evidence_scope: canonical-signature-help-provider-parity
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_canonical_signature_help.c
related_tests:
  - tests/parser/test_canonical_consumers.c
  - tests/language_server/test_lsp_inlay_semantic_facts.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/stdio_smoke.js
---

# Canonical Signature Help Provider Parity

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-07-19 22:16 +08:00 | 已完成 | canonical call fact保留extern function参数名；canonical signature-help provider从callable contracts构建parameter information并复用argument semantic-fact文档；canonical consumer 5/5、signature semantic facts 9/9、interface 87/87与stdio/CLI冒烟恢复；最新HEAD上的GCC/Clang/MSVC十四目标矩阵与附加provider验收 |

## 已完成契约

- resolved call的`signatureDisplay`同时从普通function和`ZR_AST_EXTERN_FUNCTION_DECLARATION`读取parameter AST，因此extern签名保留`lhs/rhs`等源码参数名。
- canonical LSP provider仍使用parser-owned `CallAt`/`FormatCall`生成整体签名，不回退到LSP本地类型字符串推断。
- provider从canonical function type的parameter contracts生成按序parameter information，保留`in/ref/ref readonly/out`传递形式和canonical type label。
- 每个parameter按相同索引关联call argument AST，复用`BuildSignatureArgumentSemanticFactDocumentation`发布expression、constant、numeric range、logical short-circuit和ownership文档。
- canonical参数构建失败时释放部分help并返回false，使既有provider可继续正常回退，不发布半成品响应。

## TDD与验证证据

- 当前HEAD集成RED 1：LSP interface为86/87，extern签名测试期望`NativeAdd(lhs: i32, rhs: i32): i32`，canonical provider返回无参数名签名。
- parser最低层RED：新canonical consumer用例精确报告`Expected 'NativeAdd(lhs: i32, rhs: i32): i32' Was 'NativeAdd(i32, i32): i32'`；共享fact emitter修复后5/5。
- 当前HEAD集成RED 2：`language_server_stdio_smoke`1/1失败，报告parameter documentation缺少argument numeric facts；底层LSP semantic-fact套件稳定为7/9，numeric/logical parameter docs均为`<null>`。
- provider参数构建修复后，WSL GCC 11.4、WSL Clang 14和Windows MSVC 19.44.35228均通过canonical consumer 5/5、signature/inlay semantic facts 9/9、LSP interface 87/87和stdio smoke 1/1。
- 最终回归以当前`HEAD=1fe46d6`及本阶段overlay运行；三套工具链均通过相同十四语义/LSP目标、incremental parser、canonical consumer和signature semantic-fact目标，所有可执行目标exit code为0且无`Fail -`/`:FAIL`标记；`language_server_stdio_smoke`均1/1。

## 未完成边界

- 本记录关闭canonical signature-help的extern签名与argument semantic-fact对等性，不表示所有LSP provider parity完成。
- 还需继续验证member/meta/constructor/imported/native descriptor的canonical signature parameter name、documentation、generic constraint和effect parity。
- declaration CFG/query cache最小失效、direct caller/ModuleIdentity传播、cancellation、snapshot race、延迟与内存预算仍待后续子里程碑。
- 隔离源树继续把未提交core profiling helper作为外部baseline overlay；该无关core改动不属于本记录。
