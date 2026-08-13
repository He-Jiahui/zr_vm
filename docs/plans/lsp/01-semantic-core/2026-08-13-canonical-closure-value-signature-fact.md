---
plan_id: lsp-semantic-inference
record_id: 2026-08-13-canonical-closure-value-signature-fact
status: completed
completed_at: 2026-08-13 13:46 +08:00
plan_sources:
  - docs/plans/lsp/01-semantic-inference-core.md
  - docs/plans/lsp/05-implementation-blueprint.md
  - user: 2026-08-13 approved inline closure callable-value semantic fact contract
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_bindings.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_callable_binding_refinement.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_signature_help.c
tests:
  - tests/parser/test_canonical_consumers.c
  - tests/language_server/test_lsp_reference_callable_consumer_cases.h
  - tests/language_server/test_lsp_interface.c
  - tests/acceptance/2026-08-13-lsp-l8-canonical-closure-value-signature-fact.md
doc_type: milestone-detail
---

# Canonical Closure Callable-Value Signature Fact

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
| --- | --- | --- |
| 2026-08-13 13:46 +08:00 | 已完成 | LSP 08 第十五个独立合同：source lambda callable value发布canonical target identity、declaration range和signature fact；事实缺失时signature help fail closed。 |

## Contract

- `var add = fn(left: int, right: int): int => left + right;`的compiler callable binding保留精确`ZR_AST_LAMBDA_EXPRESSION`节点，而不是仅保存lambda data payload。
- 成功注册的lambda binding以该节点的完整source range发布resolved declaration reference fact，并与既有binding共享稳定SymbolId和canonical callable TypeId。
- `add(20, 22)`只经`ZrParser_SemanticQuery_CallAt`和`ZrParser_SemanticQuery_FormatCall`发布`add(left: int, right: int): int`；query target declaration range与lambda node range完全一致。
- LSP hover、definition和signature help只消费该resolved fact。signature-help canonical-source gate把lambda纳入已发布的declaration kind；同一expression的`hasCallInfo`被清除后直接unavailable，禁止lambda AST、variable name、callee text或overload fallback。

## TDD Evidence

RED在parser canonical-consumer test中证明lambda call已存在call target但`DeclarationOf(targetSymbolId)`为空，无法向LSP发布精确declaration range。LSP RED随后证明在移除`hasCallInfo`后，signature help仍通过lambda AST恢复，违反fail-closed合同。

GREEN将lambda AST node传入现有binding注册，准确追加declaration fact并将function binding的declaration range同步为lambda range。type-inference call parameter extraction支持lambda参数，LSP canonical-source gate只增加已发布的`ZR_AST_LAMBDA_EXPRESSION`种类，不增加文本分支。正向用例同时断言target SymbolId、canonical TypeId、formatted label、hover range、definition range和signature label；反向用例清除同一call fact并断言signature help为unavailable。

## Validation

固定验收源码为`HEAD=6d9a22e + 8-path code/test overlay`，八条overlay byte-exact为`8/8`。GCC 11.4、Clang 14.0和MSVC 17.14.38使用独立静态Debug source/build目录，直接执行定向可执行文件：

| Toolchain | Canonical | Facts | Semantic query | Expression hover | Local hover | Interface | Project | Compiler integration |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| GCC | 19/19 | 13/13 | 29/29 | 9/9 | 12/12 | 112/112 | 58/58 | 127/127 |
| Clang | 19/19 | 13/13 | 29/29 | 9/9 | 12/12 | 112/112 | 58/58 | 127/127 |
| MSVC | 19/19 | 13/13 | 29/29 | 9/9 | 12/12 | 112/112 | 58/58 | 127/127 |

GCC和Clang的`language_server_stdio_smoke`与`cli_integration`均为`2/2`真实exit 0。MSVC的CLI CTest真实exit 0；首次两次stdio独立采样分别出现hover `p95=63.57ms`与completion `p95=113.22ms`的单尾部性能门禁失败。未修改实现、阈值或白名单后，在无构建进程的同一固定快照连续重跑三次，三次stdio进程均真实exit 0，hover p95为`7.01/12.98/24.69ms`，completion p95为`22.46/47.47/14.45ms`。

## Open Scope

- binary/native/provider callable value尚未发布同等source declaration identity，必须独立fail-closed验收。
- L8其余本地fallback删除、provider/project覆盖和完整protocol矩阵仍未完成。
