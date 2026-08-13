---
related_code:
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_bindings.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_callable_binding_refinement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_symbols.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_bindings.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_callable_binding_refinement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_symbols.c
plan_sources:
  - user: 2026-08-13 approved inline callable-value semantic fact contract
  - docs/plans/lsp/01-semantic-inference-core.md
  - docs/plans/lsp/05-implementation-blueprint.md
tests:
  - tests/parser/test_canonical_consumers.c
  - tests/language_server/test_lsp_reference_callable_consumer_cases.h
  - tests/language_server/test_lsp_interface.c
  - tests/acceptance/2026-08-13-lsp-l8-canonical-callable-value-signature-fact.md
doc_type: milestone-detail
---

# Canonical Callable-Value Signature Fact

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
| --- | --- | --- |
| 2026-08-13 11:38 +08:00 | 已完成 | LSP 08 第十四个独立合同：source callable-value call发布canonical call fact，signature help在fact缺失时fail closed。 |

## Contract

- `pub var runBossScenario = runBossScenarioImpl`通过公开compiler callable-binding入口，把initializer的精确callable identity注册到当前type environment；LSP symbol bootstrap复用同一入口，不复制类型规则。
- callable-value identifier注册遇到无显式返回类型的source function时，以declaration AST identity定位既有function binding，再以原SymbolId重绑定推断后的canonical function TypeId。未绑定为callable value的函数不进入本leaf refinement；已有同SymbolId reference facts同步到该TypeId。
- `runBossScenario(30, 7, 5)`只通过`ZrParser_SemanticQuery_CallAt`和`ZrParser_SemanticQuery_FormatCall`提供`runBossScenario(seed: int, prepareAmount: int, battleAmount: int): int`。
- 同一expression的`hasCallInfo`不可用时，LSP signature help直接unavailable；不得从local variable name、initializer AST、callee text或overload fallback重建。

## TDD Evidence

RED先证明无注解`runBossScenarioImpl`的callable binding仍保留初始`object`返回类型，alias调用不能发布期望的`fn(int, int, int) -> int`合同；LSP bootstrap也没有注册variable initializer的callable binding。

GREEN把返回类型refinement限定在callable-value identifier注册入口，并让compiler variable lowering与LSP symbol collection调用同一公开binding入口。positive case同时断言resolved reference、target SymbolId、canonical TypeId和formatted call label；negative case清除同一expression fact的`hasCallInfo`并要求signature help不可用。

扩大compiler回归时，初版“所有无注解函数完成编译后统一refine”的实现令既有嵌套函数destructuring shadow用例把局部值错误投影为function type。最终实现撤销该全局行为，只在callable-value identifier实际注册时按declaration identity refine；同一GCC compiler integration随后恢复127/127，并在Clang/MSVC复验。

## Validation

固定验收快照为`HEAD=5922bcb + 9-path callable-value code/test overlay`，九条overlay均做byte-exact校验。独立GCC 11.4、Clang 14.0与MSVC 17.14.38构建并直接执行：

| Toolchain | Canonical | Facts | Local query | Expression hover | Local hover | Interface | Project | Compiler integration | stdio/CLI |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| GCC | 18/18 | 13/13 | 32/32 | 9/9 | 12/12 | 111/111 | 58/58 | 127/127 | 2/2 |
| Clang | 18/18 | 13/13 | 32/32 | 9/9 | 12/12 | 111/111 | 58/58 | 127/127 | 2/2 |
| MSVC | 18/18 | 13/13 | 32/32 | 9/9 | 12/12 | 111/111 | 58/58 | 127/127 | 2/2 |

每个测试进程与`ctest -R "^(language_server_stdio_smoke|cli_integration)$"`均真实exit 0。本leaf不表示L8整体完成。

## Open Scope

- closure/lambda value尚未纳入本leaf，必须先发布同等精确fact再删除对应fallback。
- binary/native/provider callable value与完整L8 fallback收敛继续按独立record验收。
