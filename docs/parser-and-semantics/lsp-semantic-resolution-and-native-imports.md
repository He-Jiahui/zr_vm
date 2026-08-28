---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_canonical_symbol_display.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_canonical_symbol_display.h
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/module/lsp_module_metadata.c
  - zr_vm_language_server/src/zr_vm_language_server/module/lsp_module_metadata.h
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project.c
  - zr_vm_library/include/zr_vm_library/native_registry.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_symbols.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_support.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_signature_help.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_canonical_signature_help.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_canonical_signature_help.h
  - zr_vm_language_server/src/zr_vm_language_server/lsp_external_callable_signature_help.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_external_callable_signature_help.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_external_callable_contract.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_external_callable_contract.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_query_diagnostics.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_scope_cache.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_super_navigation.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_decorator_navigation.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_navigation.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_token_metadata.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_tokens.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_property_contract.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_property_code_actions.c
  - zr_vm_language_server/src/zr_vm_language_server/reference_tracker.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_support.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_binary_metadata_coordinates.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_descriptor_metadata_coordinates.c
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_internal.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_bindings.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_callable_binding_refinement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_extern_declaration.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_import_metadata.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_property.c
  - tests/language_server/test_lsp_language_feature_matrix.c
  - tests/parser/test_compiler_regressions.c
  - tests/container/test_container_type_inference.c
  - tests/container/test_container_runtime.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/parser/test_parser.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_native_construct_receiver_fact_cases.h
  - tests/language_server/test_lsp_property_contract_cases.h
  - tests/language_server/test_lsp_property_incremental_cases.h
  - tests/language_server/test_lsp_property_refactor_cases.h
  - tests/language_server/test_lsp_reference_callable_consumer_cases.h
  - tests/language_server/test_lsp_local_semantic_query.c
  - tests/language_server/test_lsp_local_semantic_receiver_dependency_cases.h
  - tests/language_server/test_lsp_project_features.c
  - tests/language_server/test_lsp_project_native_callable_signature_cases.h
  - tests/language_server/test_lsp_project_native_receiver_callable_cases.h
  - tests/language_server/stdio_smoke.js
  - tests/parser/test_parser_extern.c
  - tests/parser/test_canonical_consumers.c
  - tests/acceptance/2026-08-13-lsp-l8-canonical-callable-value-signature-fact.md
  - tests/acceptance/2026-08-13-lsp-l8-canonical-closure-value-signature-fact.md
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_canonical_symbol_display.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/module/lsp_module_metadata.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project.c
  - zr_vm_library/include/zr_vm_library/native_registry.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_symbols.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_support.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_signature_help.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_canonical_signature_help.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_external_callable_signature_help.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_external_callable_contract.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_query_diagnostics.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_scope_cache.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_super_navigation.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_decorator_navigation.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_navigation.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_token_metadata.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_tokens.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_property_contract.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_property_code_actions.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_support.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_binary_metadata_coordinates.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_descriptor_metadata_coordinates.c
  - zr_vm_language_server/src/zr_vm_language_server/reference_tracker.c
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_internal.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_bindings.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_callable_binding_refinement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_extern_declaration.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_import_metadata.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_property.c
plan_sources:
  - user: 2026-04-04 实现“ZR LSP 语义内核与元信息推断增强计划”
  - user: 2026-04-05 继续把 plugin/native/binary metadata 统一链推进到更细粒度 completion/definition/references/watched refresh 覆盖
  - user: 2026-04-06 继续清理 runtime 残留，并把 imported Pair 显式绑定规则补成 LSP 断言
  - user: 2026-07-20 严格执行 LSP semantic inference 计划并逐子里程碑记录产出
  - user: 2026-08-13 approved inline callable-value semantic fact contract
  - docs/plans/lsp/01-semantic-inference-core.md
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
  - docs/plans/syntax/05-property-unified-ast/m5-property-consumers-reflection-migration-implementation-plan.md
tests:
  - tests/language_server/test_lsp_language_feature_matrix.c
  - tests/parser/test_compiler_regressions.c
  - tests/container/test_container_type_inference.c
  - tests/container/test_container_runtime.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/parser/test_parser.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_native_construct_receiver_fact_cases.h
  - tests/language_server/test_lsp_property_contract_cases.h
  - tests/language_server/test_lsp_property_incremental_cases.h
  - tests/language_server/test_lsp_property_refactor_cases.h
  - tests/language_server/test_lsp_reference_callable_consumer_cases.h
  - tests/language_server/test_lsp_local_semantic_query.c
  - tests/language_server/test_lsp_project_features.c
  - tests/language_server/test_lsp_project_native_callable_signature_cases.h
  - tests/language_server/test_lsp_project_native_receiver_callable_cases.h
  - tests/language_server/test_lsp_project_utf16_ranges.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/descriptor_plugin_fixture_int.c
  - tests/language_server/descriptor_plugin_fixture_float.c
  - tests/language_server/stdio_smoke.js
  - tests/parser/test_parser_extern.c
  - tests/parser/test_canonical_consumers.c
  - tests/acceptance/2026-08-13-lsp-l8-canonical-callable-value-signature-fact.md
doc_type: module-detail
---

# LSP Semantic Resolution And Native Imports

## 范围

这份文档说明 language server 里最近补齐的四条关键语义链路：

1. `this` / `super` / `comptime` / `#zr.testing.test#` / lambda 等局部符号，必须按真实作用域命中。
2. hover / definition / references 不能再被宽范围声明误导，必须优先命中最具体的引用范围。
3. `import("module")` 的字符串字面量必须成为一等导航目标，hover / definition 直接落在导入目标模块上。
4. `import("zr.math")` 这类导入必须在语义分析阶段就把 native/binary/source module metadata 预热进 parser/type inference，后续 LSP 才能正确解析 `init math.Vector3(...).y` 这类值类型构造链。

## Parser Token Range Invariant

LSP hover / definition / references ultimately depend on AST token ranges matching the actual source token, not just the surrounding statement. The parser now preserves the full lexer location snapshot in `SZrParserCursor`, including current-line start, current-token start, and cached lookahead token-start fields. This matters whenever speculative parsing probes a type expression before falling back to an ordinary expression: restoring only `currentPos` and `t` can leave later identifier references collapsed to the end of the token.

`get_current_token_location(...)` also uses the token's actual source text for identifiers, numbers, and booleans when the lexer cursor under-reports the end of a token. The fallback is intentionally limited to source-backed token kinds so multiline template/string ranges keep using lexer cursor spans.

Regression coverage lives in `tests/parser/test_parser.c`:

- `test_parser_cursor_restore_preserves_identifier_reference_locations`
- `test_parser_call_callee_locations_cover_identifier_text`

Those parser tests protect the lower-layer ranges that `tests/language_server/test_semantic_analyzer.c` then consumes for scoped local hover, callable hover, generic callable signatures, and reference lookup.

## Resolved Callable Canonical Consumers

Source callable hover and signature help now share the parser semantic-query contract instead of rebuilding a method signature from a member name. `lsp_canonical_signature_help.c` calls `ZrParser_SemanticQuery_CallAt`, formats the whole label through `ZrParser_SemanticQuery_FormatCall`, and reads ordered parameter metadata from the returned canonical function `TypeId`.

This preserves three details that were previously easy to lose in an LSP-local formatter:

- receiver effects remain visible as `const fn` or `fn`;
- function-scoped reference parameters retain `scoped ref` / `scoped ref readonly` in parameter information;
- source generic clauses remain attached to the declaration while parameter and return types use the closed canonical instantiation, for example `fn shape<const N: int>(value: Matrix<int, 4>): Matrix<int, 4>`.

Receiver hover is only selected for a resolved reference with `hasResolvedTarget` and a non-`NONE` canonical receiver effect. Receiver-base hover therefore continues to show the receiver value type, while the callee reference shows the same canonical call text as signature help.

The same target identity also closes the first receiver dependency boundary in `semantic_analyzer_scope_cache.c`. Class and struct method references compare the parser-published declaration range, not the member spelling. A changed inferred-return method invalidates a resolved direct caller, an unrelated cached scope is preserved, and unresolved/poisoned facts continue through conservative invalidation.

Named-call compatibility failures preserve the parser compiler diagnostic until `ZrParser_Compiler_PublishCurrentDiagnostic` has copied it into persistent semantic query facts. `semantic_analyzer_query_diagnostics.c` then projects that fact into LSP and removes only the same-range `cannot_infer_exact_type` placeholder, preventing one root call error from becoming two primary diagnostics.

## Native Construct Receiver Exact Expression Facts

Native source construction receiver projection is a separate fail-closed path.
For `init math.Vector3(...).y`, the LSP first selects the receiver-prefix AST
node, then reads its exact expression fact by node identity and formats only its
canonical TypeId. A range query is insufficient because the construction range
can also contain the outer member's result fact. Missing, unknown, or invalid
facts produce no native descriptor member target; the consumer does not call
`ExpressionType_Infer` or reconstruct a type from the member spelling.

A `memberIndex > 0` prefix is derived from an earlier member and is therefore
also exact-fact-only even when its terminal AST node is an identifier. Direct
plain identifiers retain the established path. Project receiver resolution is
unchanged because it consumes a distinct canonical property contract. The
interface regression covers both exactness and TypeId invalidation plus a
construct-derived chain without an expression fact.

Completion applies the same boundary before it resolves import metadata or
creates a scoped fallback analyzer. For a cursor immediately after
`init math.Vector3(...).`, a valid receiver-prefix exact fact produces the
descriptor fields. If the fact is missing, unknown, or has an invalid TypeId,
the request succeeds with an empty completion list. This prevents the
last-good AST from regenerating `x`, `y`, or `z` after an incomplete edit; the
consumer does not re-infer the construct expression, scan a member name, or
recover through generic completion. Ordinary identifier receivers and project
property receivers retain their separate canonical paths.

## Binary Metadata Declaration Identity

Binary-only imported members now keep the typed-export declaration range through the unified semantic query and into definition, references and document highlights. `lsp_semantic_query.c` and project navigation only select the specialized coordinate projection when `sourceKind` is `ZR_LSP_IMPORTED_MODULE_SOURCE_BINARY_METADATA`; no member-name or displayed-signature fallback is introduced.

The artifact range is one-based and byte-column based. `lsp_binary_metadata_coordinates.c` reconstructs byte offsets and uses the normal UTF-16 codec when a source snapshot is available. Without source text it preserves the artifact's structural line/column coordinates instead of returning a fabricated `0:0`. The inverse helper lets requests that begin on a `.zro` declaration resolve the same canonical export fact. Full behavior, limitations and tests are documented in [LSP Binary Metadata Coordinate Projection](./lsp-binary-metadata-coordinate-projection.md).

## Descriptor Plugin Type Member Identity

Descriptor-plugin receiver members now resolve before the generic import-chain path. The receiver query consumes the analyzer's inferred receiver type and the metadata provider's exact owner/member fact, so `point.y` remains a field of the resolved descriptor type instead of being accepted early as a generic imported module member. Failure to resolve a receiver still falls through to all existing import and external metadata paths.

Physical plugin files have no source text for their synthetic type declarations. `lsp_virtual_documents.c` already publishes deterministic compact member ranges; `lsp_descriptor_metadata_coordinates.c` converts those one-based structural coordinates only when the resolved source kind is `NATIVE_DESCRIPTOR_PLUGIN`. Definition, references and document highlights from either source usages or the compact declaration therefore keep the same member identity and no longer collapse to the plugin module entry at `0:0`.

Receiver completion is tested first on a valid `point.x` snapshot and then after an incomplete version 2 edit to `point.;`. The second request uses the existing last-good AST contract; the LSP does not recover the receiver by scanning member names or rebuilding type facts from local text. Full behavior and constraints are documented in [LSP Descriptor Metadata Coordinate Projection](./lsp-descriptor-metadata-coordinate-projection.md).

## Native Descriptor Callable Contracts

Native builtin and descriptor-plugin module functions now enter signature help through the same semantic query that resolves their ModuleIdentity, provider source kind and exact `ZrLibFunctionDescriptor`. `lsp_external_callable_signature_help.c` receives only the parsed callee identifier range, runs `ZrLanguageServer_LspSemanticQuery_ResolveAtPosition`, and accepts a result only when the current resolved member is a native module `FUNCTION`.

`lsp_external_callable_contract.c` is the shared structured adapter for hover and signature help. It formats the descriptor's function name, generic parameter names, ordered parameter names/types and return type, while parameter information preserves descriptor documentation and existing argument semantic facts. Descriptor-plugin reload therefore updates both features from the newly queried provider generation; the adapter does not cache descriptor pointers.

The resolver is deliberately tri-state. A non-native target continues to established source/binary/receiver consumers. A complete native function descriptor resolves. A recognized native function with incomplete or unsupported structured fields returns unavailable and blocks AST/member-name/text fallback.

Native instance method call sites now merge two structured facts instead of formatting raw `ZrLibMethodDescriptor` types. `SZrLspSemanticQuery` supplies current provider/member identity plus descriptor generic names, parameter names and documentation; `ZrParser_SemanticQuery_CallAt` supplies the closed canonical function `TypeId`. Hover and signature help format receiver effect, passing/escape contracts and closed parameter/return types from that canonical fact. Unconstrained generic methods therefore retain a structured clause such as `<T>` while displaying call-site-closed types. Descriptor reload re-runs both queries, and incomplete or constraint-bearing descriptors return unavailable without scanning owner/member text. Bare references, static methods, constrained generic clauses and effectful methods remain explicit boundaries. Full behavior, failure rules and tests are documented in [LSP Native Descriptor Function Callable Contract](./lsp-native-descriptor-function-callable-contract.md).

## 隐式接收者符号

`semantic_analyzer_symbols.c` 里的 `collect_function_like_scope(...)` 会为实例方法、构造函数、struct 方法建立隐式运行时符号：

- `this`
- `super`

修复前，这两个隐式符号复用了所属方法 AST 节点。结果是 hover 命中 `this` 时，LSP 会把它当成“方法符号”，显示方法签名而不是接收者类型。

当前行为改成：

- `this` / `super` 仍然是普通变量符号，参与 completion / hover / references。
- 但它们不再绑定到方法 AST 节点，因此 hover 会回落到变量类型展示。
- `this` 的 hover 现在展示当前 owner type，例如 `Derived`。
- `super` 的类型来自继承链上的 base type，后续 definition/reference 继续通过真实符号关系处理。

这条修复直接覆盖了类内 receiver 场景，不再需要 hover markdown 反推“当前是不是 method”。

## `super(...)` 构造函数导航

`lsp_signature_help.c` 和新抽出的 `lsp_super_navigation.c` 现在把 class meta function 里的 `super(...)` 识别成独立调用上下文，而不是要求它先降级成普通 `FunctionCall` AST。这个补口专门解决派生类构造函数里的场景：

```zr
class BossHero: BaseHero {
    pub @constructor(seed: int) super(seed) {
    }
}
```

当前行为是：

- signature help 会先识别当前位置是否落在 `super(...)` 参数区间。
- 若命中，LSP 直接解析当前 owner class 的 direct base type。
- 基类构造函数元方法优先走 compiler/type inference 里的结构化 member info。
- 如果当前 analyzer 尚未持有对应 prototype member，则回退到源码 AST 里的 `@constructor` 声明，按基类泛型实参做参数类型专门化。
- 最终 signature label 与 parameter list 统一走结构化 builder，所以 `super(seed)` 会展示 base constructor 的参数名和类型，而不是派生类构造函数自己的参数列表。
- `lsp_super_navigation.c` 会把两类入口统一归并到同一个“基类构造函数目标”上：
  - 派生类构造函数里的 `super(...)`
  - 基类上的 `@constructor` 声明
- `lsp_interface.c` 的 definition / references / document highlight 都改成先走这套统一目标解析；当光标落在 `super` 关键字、参数区或基类 `@constructor` 声明上时，导航和高亮会落到同一组结果。
- goto definition 会直接跳到基类 `@constructor` 声明，而不是停在派生类构造函数作用域里的隐式 `super` 变量上。
- find references 会把基类 `@constructor` 声明和所有匹配的 `super(...)` 调用归并成同一引用集。
- document highlight 会在当前文档里同时标出基类 `@constructor` 声明和对应的 `super(...)` 调用位置。

这让 `super(...)` 不再只是 signature help 的补丁式特判，而是开始具备统一的构造函数导航语义，也避免了“必须先有完整 prototype 编译结果才能提示”的额外耦合。

实现上，`lsp_super_navigation.c` 的 opened-document scan 现在先 acquire `SZrFileVersionContentSnapshot` 再解析 super target、references 和 highlights；signature help 的 code-span guard 也走同一类 owned snapshot，避免请求期间直接持有 `fileVersion->content`。

## `native extern` 函数与类型的统一语义入口

这一轮把 `native extern` 里的 function / delegate / struct / enum 从“只有 symbol table 看得见”继续推进到“navigation、references、signature help 共用同一组结构化事实”。

当前行为是：

- `semantic_analyzer_symbols.c` 会给 extern function / delegate / enum member 建立真实 source symbol，并把 definition reference 直接挂到 symbol 上。
- extern struct 声明现在也会补 definition reference，因此 type annotation 上的 `FindReferences` 不再只返回 usage，声明和 usage 会落到同一引用集合。
- server 自己维护的 `compilerState->typeEnv` / `compileTimeTypeEnv` 现在会显式注册 extern function callable binding，而不再只依赖 symbol table。
- 这一步会连同 declaration node 一起写入 type env，所以后续 overload resolution 和 signature help 能直接命中 extern declaration，而不是再回退到“源码里找不到普通 `fn` 声明”。
- parser/type inference 侧的 candidate lookup 也补进了 extern function declaration，因此基于 declaration node 的参数列表和命名参数匹配不再只支持普通函数。

这条修复让 extern function 的以下能力开始共用同一路径：

- goto definition
- find references
- document highlight
- completion
- signature help

## Extern Call Context 与源类型保真

extern function 的 signature help 之前还有两个独立缺口：

1. bare function call 的 `FunctionCall.location` 没覆盖到“刚进入 `(` 后、还没进入第一个参数 AST 节点”的区间，导致 `NativeAdd(1, 2)` 这类位置虽然已经在调用里，signature help 仍然拿不到 call context。
2. 即使解析到 extern declaration，签名展示也会把 `i32` 这样的源类型正规化成 `int`，丢掉 FFI/source declaration 的原始拼写。

当前行为改成：

- `lsp_signature_help.c` 为普通函数调用统一构造 `signature_call_context_range(...)`，调用上下文范围会从 call node 起点延伸到最后一个实参或 generic 实参结尾。
- `signature_call_matches_position(...)` 不再只依赖裸 `callNode->location`，因此光标位于 `(` 后、逗号间隔区或第一个参数前时，也能正确触发 signature help。
- extern function 的 signature label 构建优先使用 declaration AST 上的参数/返回类型文本，而不是把 resolved inferred type 直接格式化成标准化名字。
- 结果是 `native extern` 源声明里的 `NativeAdd(lhs: i32, rhs: i32): i32;` 在 hover-independent signature help 里会保持 `i32`，而不是退化成 `int`。

这条链路和 `super(...)` 一样，已经不再是 hover markdown 或 native 特判的派生结果，而是基于真实 callable declaration、真实调用区间和真实类型来源做结构化拼装。

## Decorator 导航与 parser 修复

`lsp_decorator_navigation.c` 现在把 `#singleton#`、`#trace#` 这类 decorator token 当成第一类语义入口处理：

- goto definition 会默认跳到被修饰的 class / method / field / property / function 声明。
- hover 会展示 decorator 名称、类别以及目标声明，例如 `Target: class SingletonClass`。
- 若 decorator 未来具备额外注册元信息，可以继续叠加到 hover 上，但“被修饰声明”仍然是主定义目标。

这轮实现里还修了一个更底层的 parser 缺陷：顶层 `#decorator# class Foo {}` 之前会在 statement lookahead 时吃掉 decorator，导致 AST 上类声明没有保留 decorators。`parser_statements.c` 现在会在 decorator lookahead 前后保存并恢复 parser cursor，因此：

- 顶层 class decorator 会保留在 AST 上。
- 后续 LSP decorator definition / hover 不需要绕过损坏 AST 做位置猜测。
- `tests/parser/test_parser_extern.c` 新增了顶层 class decorator 回归，确保这条支持层不会再退化。

## `import("...")` 字面量导航

`lsp_import_target_navigation.c` 现在把 `import("module")` 里的字符串字面量当成独立语义入口处理，而不是只把导入后的别名变量当作可导航对象。

这条实现专门补了一个之前的结构性缺口：

- 语义分析里导入绑定本身是存在的。
- 但是 parser 当前给 string literal AST 节点留下的 `location` 在这条路径上并不可靠，`import("greet")` 会把字面量位置漂到分号附近。
- 结果是 imported member 导航能工作，真正落在 `"greet"` 上的 hover / definition 却命不中。

当前行为改成：

- LSP 仍然复用 project/native metadata 去解析模块来源和目标记录。
- 但 `"module"` 字面量本身的命中范围，不再信任 AST string range。
- `lsp_import_target_navigation.c` 直接基于当前文档文本恢复字面量边界：
  - 从光标 offset 向左右收缩到当前字符串的引号范围。
  - 验证左侧语法前缀确实是 `import(`。
  - 现场归一化模块名，再映射到 project source record 或 native builtin descriptor。
- definition 现在会把 `import("greet")` 直接跳到 `greet.zr` 模块入口。
- hover 现在会把 `import("greet")` / `import("zr.system")` 统一展示成：
  - `module <...>`
  - `Source: project source` / `native builtin` / `external/unresolved`
- references 现在也会把 import target literal 当成真实 module-level 入口：
- `includeDeclaration=true` 时先落到 source / native descriptor plugin 的 module entry
  - binary metadata 若 `.zro` typed export 已携带 declaration span，则优先落到 symbol-level declaration；旧 schema 才回退到 module entry
  - 然后回收 project 内所有匹配的 `import("module")` 字面量位置
- document highlight 现在会在当前文档里标出同一 module target 的 import literal 范围，而不是退回普通 string token 语义

这让 `import` 字面量本身进入了和 imported member、decorator、`super(...)` 一致的第一类导航模型，也避免继续在 AST 位置不稳定的情况下做“命中不到就算了”的弱处理。

## 固定 Token 元信息

`lsp_token_metadata.c` 把现行关键字和 `@...` meta method 的固定元信息表从 `lsp_interface_support.c` 里抽出来，避免继续把新职责堆进一个 2000+ 行文件。当前这层统一提供三类消费：

- 输入关键字前缀或 `@` 时的固定 completion。
- `@constructor` 这类 token 的 hover 类别说明。
- semantic tokens 对 keyword、`@meta-method` 的识别辅助。

这意味着同一份固定表现在至少服务于 completion、hover、semantic tokens，不再出现“补全知道分类，hover/semantic token 不知道”的分裂状态。`@meta-method` hover 在读取源码 token 前会 acquire `SZrFileVersionContentSnapshot`，token 查找、code-span 过滤、descriptor lookup 和 hover range 构造都基于 owned snapshot，而不是直接持有 live `fileVersion->content`。

## `@meta-method` Hover 与 Token 分类

这轮补齐了用户显式要求的 `@xxx` 类别说明能力。当前行为是：

- hover 落在 `@constructor`、`@add`、`@toString` 等 token 上时，会直接展示：
  - meta method 名称
  - 分类，例如 `lifecycle`、`arithmetic/operator`、`conversion`、`call/access`
  - 适用声明形态，当前统一写为 `class/struct meta function`
- semantic tokens 现在会把 `@constructor` 这类声明 token 分类成 `metaMethod`
- semantic tokens 现在会把 `#singleton#` 这类 decorator token 分类成 `decorator`
- `comptime`、`import` 等现行保留字语义 token 保持为 `keyword`，以兼容现有 LSP token legend 与测试基线

这条实现虽然还没有把 meta method 自身接进 definition / references，但已经把“类别提示”和“可视分类”从 completion-only 扩展到了 hover 和 semantic tokens。

## 最窄引用优先

`reference_tracker.c` 的 `FindReferenceAt(...)` 以前采用“第一个包含当前位置的引用即返回”。这个策略在有宽范围定义引用时会产生系统性误判，例如：

- `#zr.testing.test# fn scope(): void` 的声明范围覆盖整个 test body
- 更具体的局部变量 / compile-time 变量 usage 引用虽然也存在，但因为排在后面，永远命不中

当前行为改成：

- 先收集所有包含当前位置的引用
- 再按“范围越窄越优先”选择最佳候选
- 若范围相同，则优先非 definition 引用

这使得以下行为回到一致状态：

- comptime 变量 hover 不再被测试函数声明覆盖
- 局部变量 / lambda capture / class 内 receiver 的引用命中更稳定
- LSP definition / hover / references 的命中逻辑更接近真实语义事实，而不是注册顺序

## Native Import Metadata 预热

`init math.Vector3(4.0, 5.0, 6.0).y` 之前在 parser 单测里可推断，但在 LSP 文档分析里失败。根因不是语法不支持，而是 semantic analyzer 在建立

```zr
var math = import("zr.math");
```

这个变量的类型时，只写入了模块名字符串，没有触发 parser 的 import metadata 加载流程。

当前行为改成：

- `semantic_analyzer_symbols.c` 里的 `infer_symbol_expression_type(...)` 在遇到 `ZR_AST_IMPORT_EXPRESSION` 时，优先调用 `ZrParser_ExpressionType_Infer(...)`
- parser 的 import inference 会进入 `ensure_import_module_compile_info(...)`
- native/source/binary module 的 prototype/type descriptor 会被注册到 compilerState 的 type prototype 集合
- 随后 `lsp_interface_support.c` 对 receiver 前缀做 AST 推断时，就能把 `init math.Vector3(...)` 识别成真实 `Vector3` value type

结果是：

- `init math.Vector3(...).` completion 可以列出 `x/y/z`
- `init math.Vector3(...).y` hover 可以展示 `field y: float` 和 `Receiver: Vector3`
- 这条能力直接复用 parser/type inference/native descriptor 的结构化元信息，不再靠 LSP 特判库名

## Imported Type Bindings 与嵌套 Native Module Lookup

这一轮又把 `import("zr.container")` 的显式类型绑定规则补到了 parser 和 LSP 两侧，重点覆盖两条之前会相互干扰的链：

1. imported type 不再回到“裸全局类型空间”
2. nested native module prototype 在 compile-time lookup 时不能再自递归

### 显式绑定规则

当前规则固定为：

```zr
var container = import("zr.container");
var pair1: container.Pair<int, float> = init container.Pair<int, float>(1, 2.0);

var {Pair} = import("zr.container");
var pair2: Pair<int, float> = init Pair<int, float>(1, 2.0);
```

允许：

- 模块限定名 `container.Pair`
- destructuring import 显式引入后的裸名 `Pair`

不允许：

- 没有 qualifier 或 destructuring import 的裸 `Pair`
- 在 `var {Pair} = import("zr.container")` 之后再声明第二个 `Pair`

这条规则现在由三层共同维持：

- parser/type inference 继续把 “type name 是否在当前上下文显式可见” 当成硬约束
- `semantic_analyzer_symbols.c` 会在 `var {Pair} = import("zr.container")` 时把 destructured type alias 注册进 type environment，并把重复声明立即转成 LSP diagnostic
- `semantic_analyzer_typecheck.c` 对复杂 initializer 不再一律回退成 `object`，而是回到 `ZrParser_ExpressionType_Infer(...)`，这样 `init container.Pair(...)` / `init Pair(...)` 的真实泛型实例类型会进入 LSP initializer compatibility 检查

结果是：

- hover / completion / diagnostics 对 `container.Pair` 与 destructured `Pair` 现在保持一致
- bare `Pair` 的显式绑定错误会直接出现在 LSP diagnostics，而不是只在 parser 单测里可见
- destructured alias 再声明本地 `Pair` 时，LSP 会和编译器一样给出 duplicate-type 诊断

### Imported Stub 与 nested module lookup

这次 runtime 残留清理同时暴露出一个更底层的 compile-time lookup 问题：

- imported source/native/binary module prototype 现在都会被标记为 compile-only stub
- `find_compiler_type_prototype_inference_exact(...)` 如果在处理 `zr.system` / `zr.system.console` 这类 dotted module name 时继续盲目走 `module.member -> fieldTypeName` 回查，就会把模块自己重新解析回自己
- 结果是 nested native module lookup 在 LSP document analysis 里能堆栈自旋，Windows 上表现为 `0xC00000FD`

当前行为改成：

- exact prototype 一旦已经命中，就优先返回 exact match
- 若 module member 的 `fieldTypeName` 与当前查询名完全相同，lookup 会在这一层短路，不再递归回自己
- imported source module 的 runtime type stub 仍然保持 compile-only，不会再被序列化进 entry function `prototypeData`

这三条一起保证：

- `zr` / `zr.system` / `zr.system.console` 这种 nested native module preheat 可以安全完成
- project source import 只参与 compile-time type analysis，不再污染 runtime prototype materialization
- language server 可以在同一份 compile-time metadata 上稳定跑 imported type hover/completion/navigation，而不会为了修 LSP 又把 runtime shadowing 问题重新引回来

## Project-Local Descriptor Plugin 优先级

这一轮把 descriptor plugin 的“同名模块跨 project 漂移”问题补到了 registry 和 LSP metadata resolver 两层。

之前的行为是：

- native registry 只按逻辑模块名缓存 descriptor record。
- 一旦某个测试工程先加载了 `zr.pluginprobe`，后续另一个工程即使在自己的 `native/` 目录下放了同名 plugin，LSP 也会继续命中旧 record。
- hover 的 `Source: native descriptor plugin` 仍然是对的，但 definition / references 可能跳到上一个工程的 `.so/.dll` 路径。

当前行为改成：

- `native_registry` 增加了 `EnsureProjectDescriptorPlugin(...)` 这一类 project-aware 入口，优先检查当前 project `native/` 目录里的实际 plugin 文件。
- `lsp_module_metadata.c` 不再“先查到 registry 就立即返回”，而是先给当前 project 一次覆盖当前 record 的机会，再统一从 registry 读结构化 descriptor/source-kind/source-path。
- descriptor plugin record 现在持久保存 `moduleName/sourcePath` 副本，不再把 cache key 和 watched refresh 的身份信息借在插件导出的静态 descriptor 内存上。
- imported-member hover 的优先级也改成 `source > binary metadata > native/plugin descriptor > module prototype fallback`。这样即使 analyzer 里还保留旧的 module prototype，hover 也不会再被它盖过当前 project 的真实 plugin descriptor。
- imported-member completion 现在跟随同一套优先级：先用 source/binary/native/plugin metadata 生成主 completion 集，再只用 module prototype 补缺失 label，不再让旧 prototype detail 覆盖当前 plugin descriptor 的真实签名。

对应的回归固定覆盖在 `test_lsp_descriptor_plugin_project_local_definition_overrides_stale_registry(...)`：

- 先打开一个返回 `int` 的 `zr.pluginprobe`
- 再打开另一个返回 `float` 的同名 plugin 工程
- hover / completion / definition / references 都必须切到第二个工程自己的 plugin 副本

这条规则把 project-local native plugin 从“可能命中”提升为“同名模块下的第一优先元信息源”。

## Watched `.dll/.so/.dylib` Refresh 的加载路径

watched dynamic library refresh 现在仍然和 binary metadata refresh 走同一条 project discovery + project refresh 入口，但 plugin loader 本身已经改成“从 shadow copy 装载”，不再直接把 project 里的 `.dll/.so/.dylib` 原文件 `dlopen` 到当前进程。

这条修复针对的是一个真实崩溃场景：

1. open document 已经因为 `import("zr.pluginprobe")` 把 project-local descriptor plugin 载入进程。
2. 外部构建工具在原路径上直接覆盖同名 `.so/.dll`。
3. server 收到 `workspace/didChangeWatchedFiles` 后尝试 `dlclose` 旧句柄。
4. 如果旧句柄直接映射的就是被覆盖中的原文件，卸载阶段可能读到被替换后的 fini / dynamic 元数据，最终在 `dlclose` 里崩溃。

当前行为改成：

- `native_registry_load_plugin_descriptor(...)` 会先把 project/source plugin 文件复制到 OS 临时目录下的 `zr_vm_native_plugin_cache/` shadow path。
- 真正 `dlopen` 的是这份 shadow copy，而不是 workspace 里的原始 plugin 文件。
- registry 继续把 workspace 中的真实 plugin 路径记录为 `sourcePath`，因此 watched-files invalidation 和 definition/source-kind 展示仍然指向用户工程里的原文件。
- plugin handle record 额外保存 `loadedPath`，invalidate 或按模块替换时会：
  - `dlclose` shadow-loaded handle
  - 删除对应 shadow copy
  - 清掉 module record / module cache
- `ZrLanguageServer_LspProject_ReloadOwningProjectForWatchedUri(...)` 不再对 `.dll/.so/.dylib` 走早退分支；它会继续执行完整 project refresh，并把已打开文档重新挂到新的 project metadata 上。

结果是：

- project 内的 plugin 文件可以被外部构建工具原地替换，而不会把当前进程里已加载句柄的卸载元数据破坏掉。
- watched plugin refresh 之后，open document 的 completion / hover / local inference 会在同一轮 project refresh 中切到新 descriptor，而不再依赖“下一次查询时再碰巧重新加载”。
- unopened-project 的 watched plugin bootstrap 语义保持不变，仍然可以沿路径回溯最近 `.zrp` 自举 owning project。

## 统一模块元信息入口与优先级

`lsp_module_metadata.c` 现在是 server 侧统一的 imported-module metadata 入口，负责把四类来源归一到同一套 source-kind 判定上：

1. 当前工作区源码 record
2. `.zro` binary metadata
3. native builtin descriptor
4. native descriptor plugin

当前优先级固定为：

- `project source`
- `binary metadata`
- `native builtin` / `native descriptor plugin`
- `external/unresolved`

这条 helper 现在同时被以下消费层复用：

- `lsp_project_features.c` 的 import hover / completion
- `lsp_interface_support.c` 的 native receiver/type descriptor 查找
- `lsp_semantic_tokens.c` 的 imported module semantic token 解析
- `semantic_analyzer.c` 的 imported module completion 递归

结果是原来分散在 `NativeRegistry_FindModule(...)`、内建模块硬编码和 import 特判里的逻辑，开始收敛到同一份 source-kind / descriptor 解析路径。

`lsp_semantic_import_chain.c` 在需要刷新目标 URI analyzer 时，会优先从打开文档 acquire `SZrFileVersionContentSnapshot`，把 snapshot content/version 交给 `ZrLanguageServer_Lsp_UpdateDocumentCore`，然后释放 snapshot。只有目标不是已打开文档且 URI 可映射为文件路径时，才保留磁盘读取 fallback。

## Imported Member References And Highlights

`lsp_project_navigation.c` 现在把 `import(...)` alias 后的第一段 member 命中先还原成统一的 imported-member 事实，再让 `definition / references / document highlight` 复用这条路径，而不是继续依赖“source symbol 找得到就工作、找不到就失效”的分裂行为。

当前这条路径统一记录：

- `moduleName`
- `memberName`
- metadata source kind
- 可选的 binary metadata module-entry 位置

消费结果按来源分层：

- source-backed imported member
  - definition 继续跳到真实源码声明
  - references 继续合并真实源码声明、真实源码引用、跨文件 import usage
  - document highlight 现在也会在当前文档里标出所有 imported usage
- binary-only imported member
  - definition 会落到 `.zro` module entry
  - references 在 `includeDeclaration=true` 时会把 `.zro` module entry 和项目 usage 归并到同一结果集
  - document highlight 会在当前文档里标出全部 `moduleAlias.member` usage
- native builtin / native descriptor plugin imported member
  - references 现在至少返回项目内或当前文档内的 usage 集合
  - document highlight 会返回当前文档的 usage 集合
  - 若当前还没有可导航声明元数据，则保持 usage-only，不伪造 definition

这意味着 imported member 不再只是 hover/completion 可用，references/highlights 失效；binary/native/plugin 也开始共享同一套结构化导航入口。

这一轮又把“声明侧 metadata 文件本身”接回了同一条导航链，补掉了之前只支持“从 source usage 反推 declaration”的单向路径：

- `.zro` 路径现在会先按 project binary root 反推出 `moduleName`
- 若光标落在 binary metadata file entry，仍然统一成 module-entry 级结果
- 若光标落在 `.zro` typed export declaration span，resolver 会直接返回该 exported member，并把 references / document highlight 归并到同一条 imported-member 链
- descriptor plugin `.dll/.so/.dylib` 入口会先尝试走 owning project 的 import bindings 反解 `moduleName`，避免同名 plugin 跨 project 时只依赖全局 registry
- 若 project 侧暂时还没把 plugin 重新解析进 analyzer，再回退到 native registry 的 `sourcePath -> moduleName` 反查
- document highlight 现在也能落在 external metadata declaration 自身：
  - `.zro` module entry 会在 metadata 文档里高亮 module entry 自身
  - descriptor plugin file entry 会在 plugin 文档里高亮 module entry 自身
- module-entry 级 references 也补进了 import-binding 声明：
  - binary metadata / plugin file entry 除了继续聚合 `moduleAlias.member` usages，还会回收 project 内 `var moduleAlias = import("...")` 的 alias declaration
  - 这样 module entry 不再只有“成员被访问过”的粗粒度引用，而开始具备真正的 module-binding 导航覆盖
- 这条 module-entry 聚合链现在也覆盖源码模块与 `native extern` wrapper 源文件：
  - `greet.zr` 这类 project source file entry 在 `0:0` module entry 上会回收到 `import("greet")` 字面量、`greetModule` alias declaration，以及同模块的 imported-member usages
  - `native_api.zr` 这类 ffi source wrapper file entry 走同一条 declaration resolver，不再因为它是 source-backed wrapper 就退回“只能从 import literal 一侧导航”
  - project 级 import-target references 不再依赖“当前文件必须已打开”；若文档未打开，server 会先确认该 analyzer 是否真的导入了目标模块，再从磁盘文本恢复 `import("...")` 的精确 inner-string range；必要时才回退到 AST binding range
  - source module entry / ffi wrapper module entry 的 references 也不再局限于 `projectIndex->files`；server 现在会递归 sourceRoot 下的 `.zr` 文件，按需加载 analyzer，把未打开源码里的 import literal、alias binding、imported-member usage 一起并回同一个 module target
  - 这样 source / ffi wrapper / binary metadata / descriptor plugin 四种 module entry 现在都共享同一套 declaration + import-target + alias-binding + imported-member usage 聚合模型

结果是 binary metadata module entry、plugin file entry、source/ffi module entry、source-side imported member 这几类入口现在都能回到同一套 definition/references/document highlight 目标模型，而不是继续分裂成“只能从 usage 侧工作”的半通路径。

## Imported Member Diagnostics

project-level diagnostics also consume the same imported-member facts. When a module resolves but the requested export does not, the server keeps the user-facing message in the existing form, for example `Import member 'greet.missing' could not be resolved`, and publishes the stable diagnostic code `plugin_unknown_export`.

The diagnostic location stays on the member access. Its relatedInformation includes both the usage/import site and the resolved module entry, so editors can surface the missing export without losing the import trace. The stdio server serializes the same code into both the top-level diagnostic and diagnostic `data.code` for follow-up actions.

## Watched Metadata Refresh

`stdio_requests.c` 现在消费 `workspace/didChangeWatchedFiles`，覆盖：

- `.zr`
- `.zrp`
- `.zro`
- `.dll`
- `.so`
- `.dylib`

server 不再只把 `.zrp` 刷新当成 document-sync 副作用。外部 metadata 变更会走：

1. `workspace/didChangeWatchedFiles`
2. `ZrLanguageServer_LspProject_ReloadOwningProjectForWatchedUri(...)`
3. `ZrLanguageServer_Lsp_ProjectRefreshForUpdatedDocument(...)`

刷新时不再只是替换 project index。当前行为还会：

- 失效旧 project record 持有的 analyzer
- 保留 parser 里已加载文档的最新内容
- 在新 project index 下重新分析这些已加载文档
- 在 project-aware reanalyze 前，为这些已加载文档预加载当前 project `native/` 目录里的 descriptor plugin imports
- 重新挂回 import bindings / imported module facts
- 如果变更来自尚未预热的 `.zro/.dll/.so/.dylib`，会沿文件路径回溯最近 `.zrp` 并先自举 owning project，再走同一套 refresh

这条修复专门解决两类问题：

- binary/native metadata 已经变了，但 open 文档 hover / completion 仍然吃旧 analyzer
- metadata 先变、project 还没被任何 source doc 触发发现，导致 watched-files 事件根本进不了 project refresh
- watched plugin refresh 之后，importer locals 仍然停留在旧的 descriptor return type，而没有重新走 compiler-side local inference

## Canonical symbol documentation types

Source symbol markdown no longer treats the symbol table's inferred type object
as an independent display authority. Hover and completion documentation pass
the owning analyzer to a shared canonical display helper, query the exact
resolved declaration by SymbolId, require its TypeId to match the symbol
snapshot, and format only through `ZrParser_CanonicalType_Format`.

Missing analyzer, unresolved declaration, invalid identity, or mismatched
TypeId omits the type section. The consumer does not rebuild text from
`symbol->typeInfo`, declaration syntax, ownership names, or member names.
Inlay hints reuse the same helper, so both paths apply the same identity
agreement rule. The helper lives in the private
`interface/lsp_canonical_symbol_display` module rather than adding another
responsibility to the large interface support source.

Project receiver member type projection applies the same rule after resolving
an exact source declaration. `TryResolveReceiverProjectMember` may preserve the
member declaration URI/range and SymbolId for navigation, but it sets
`resolvedTypeText` from a declaration symbol only through the shared canonical
helper. If that declaration fact is unresolved, invalid, or disagrees with the
symbol TypeId, member type text remains unavailable. The consumer does not use
`declarationSymbol->typeInfo`, member spelling, or AST type syntax as a display
fallback.

Project-source imported module members use the same declaration identity for
both completion details and hover. The metadata provider formats
`resolvedTypeText` only through the shared helper. It also bypasses the generic
symbol-hover builder for this source kind, because that builder would otherwise
reintroduce the independent inferred-type display authority. A missing or
mismatched declaration fact therefore reaches both consumers as `cannot infer
exact type`, without a symbol-type, module-prototype, member-name, or AST
fallback.

## Canonical property consumers

Unified properties are projected from parser `PropertyAt`/`PropertyBySymbolId` results. Hover,
completion, definition, prepare-rename, semantic tokens, and contextual accessor `value` all retain
the visible PropertySymbol, canonical value TypeId, exact accessor/value-parameter SymbolIds, and
selection/reference ranges. The LSP layer neither strips `__get_`/`__set_` prefixes nor infers a
property from a member display label.

Property signature display applies the same authority boundary. The formatter receives the owning
analyzer, queries `PropertyBySymbolId`, requires the returned PropertySymbol and value TypeId to
match the symbol snapshot, and formats the value type only through the canonical formatter. A copied
symbol contract or precise `symbol->typeInfo` is insufficient. If the PropertyDef fact is missing or
inconsistent, ordinary symbol documentation may remain available but the property signature is
omitted rather than reconstructed.

Binary imports first merge exact compiled property/accessor rows into an empty imported placeholder
using `propertyIdentity`, accessor role, TypeId, reference fields, and module provenance. LSP then
joins the visible row by PropertySymbolId. Missing, conflicting, native-only, or invalid metadata is
unavailable; it does not fall back to a property name or hidden accessor. Source and binary hover,
completion, and definition therefore share one consumer contract even though the current `.zro`
v34 executable carrier remains the documented compatibility format rather than a nested canonical
PropertyDef table.

Receiver prototype lookup may lazily import native metadata and grow the compiler state's prototype
array. Consumers therefore capture the prototype name, import-module name, kind and native-import
flag before recursive lookup, and never dereference an array element after an operation that may
reallocate that array.

Legacy migration and property refactors are also fact consumers. A quick fix is emitted only from a
structured parser diagnostic containing an exact machine-applicable replacement. Missing interface
set/init and explicit-field proxy actions query an unambiguous canonical requirement; reference,
binary-only, stale, invalid, or ambiguous cases publish no action. Snapshot validation runs before
serialization, so an edit is never paired with a newer document version than the ranges it captured.

Incremental coverage uses a 64-property document. A body-only edit preserves the edited and
unrelated PropertySymbolId/TypeId values; changing one property contract changes only that TypeId
while an unrelated property identity remains stable. This is the declaration-fact boundary used by
scoped reanalysis, not a member-name cache heuristic.

## `.zro` 与 `.zri` 的职责分流

当前 binary metadata 的正式机器可消费入口固定为 `.zro`：

- `.zro`
  - 继续走 `ZrCore_Io_ReadSourceNew(...)`
  - 直接读取序列化 binary module/source
  - imported member hover / completion / definition / references / document highlight 都以这条路径为准
  - compiler/type inference 侧的 imported-module compile info 也只消费这条路径
- `.zri`
  - 保留为 debug / intermediate 文本产物
  - 不再作为 server 的语义推断、导航、hover、completion 事实源
  - 也不再被当作 `.zro` 的替代输入

这条分流很关键，因为 `.zri` 虽然仍然是有用的 debug 文件，但它不是 `io.c` 里的 binary source 布局，也不应该再驱动正式的 LSP 语义链。之前把 `.zri` 当 `.zro` 读会在 refresh 后重新加载 imported member hover 时触发无效 IO 读取和断言崩溃。

## 相关回归

本轮回归覆盖了下面几类样例：

- 类实例方法中的 `this` / `super` / locals hover 与 completion
- 派生类构造函数里的 `super(...)` base constructor signature help
- 派生类构造函数里的 `super(...)` goto definition 到 base constructor
- 派生类构造函数里的 `super(...)` find references 命中 base constructor + super call
- 派生类构造函数里的 `super(...)` document highlight 命中 base constructor + super call
- `comptime` 变量与函数作用域
- `#zr.testing.test# fn scope(): void` 的局部作用域
- typed lambda 的局部参数和 capture
- 现行关键字与 meta method `@...` completion
- `@constructor` meta method hover 类别说明
- semantic tokens 对 `#decorator#` 与 `@meta-method` 的分类
- native value constructor `init math.Vector3(...).y`
- native `init math.Vector3(...)` 的 `STRUCT_INIT_EXPRESSION` signature help：同节点 exact expression fact 有效时投影 `x/y/z` 参数，unknown、invalid TypeId或missing fact时直接unavailable
- watched binary metadata refresh 对 unopened project 的 bootstrap
- watched binary metadata refresh 后 open 文档 hover 的更新
- `.zro` 作为 binary metadata 载体的 imported member hover / completion
- binary import literal definition 到 `.zro` module entry
- binary imported member definition 到 `.zro` exported declaration span（旧 schema 回退 module entry）
- binary imported member references 到项目 usage + `.zro` exported declaration span
- binary imported member document highlight 到当前文档 usage
- source module entry / ffi wrapper module entry references 到 `import(...)` literal + alias binding + imported-member usage
- source module entry / ffi wrapper module entry references 覆盖未打开 project source files
- source module entry / ffi wrapper module entry document highlight 命中 module entry 自身
- native imported member references / document highlight 到 usage-only 结果
- missing imported member diagnostics 使用 `plugin_unknown_export` code，并保留跨文件 relatedInformation trace
- 类成员 definition / references / comment hover

对应证据见：

- `tests/language_server/test_semantic_analyzer.c`
- `tests/language_server/test_lsp_interface.c`
- `tests/language_server/test_lsp_project_features.c`

## 验证证据

2026-04-05 这一轮 watched refresh / external metadata reanalysis 修复的验证结果如下：

- WSL gcc `build/codex-wsl-gcc-debug`
  - 定向重编 `libzr_vm_library.so`、`libzr_vm_language_server.so`
  - `./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_lsp_project_features_test_relaxed` 通过
  - `./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_lsp_interface_test` 通过
  - watched binary metadata refresh 的 importer local inference 回归通过
  - watched descriptor plugin refresh 的 importer local inference 回归通过
  - plugin source overwrite 后的 unload/reload 不再在 `dlclose` 崩溃
- WSL clang `build/codex-wsl-clang-debug`
  - 这一轮未重跑；当前验证以 WSL 定向构建为准
- Windows MSVC
  - 这一轮未重跑；仓库仍按已知基线限制处理

## 当前已知限制

- 完整验证以仓库当前 CMake 配置和语言服务回归套件为准；导入字面量导航不依赖额外的 parser 私有构建步骤。
- Windows MSVC 的 LSP 测试可执行体仍然存在独立的 `0xC0000005` 退出问题，这不是本轮 Linux/WSDL 语义修复引入的新回归。
- `zr_vm_language_server_lsp_interface_test` 在 WSL 通过时仍会打印两条 `Construct target must resolve to a registered prototype` 编译日志；当前没有导致目标测试失败，但说明 constructor prototype 解析路径仍有额外清理空间。

## Canonical Direct-Call Signature Boundary

source declaration-backed的直接free或receiver调用由
`ZrParser_SemanticQuery_CallAt`和`ZrParser_SemanticQuery_FormatCall`提供signature
help的唯一合同。LSP先消费该canonical result；同一调用缺失call fact时直接返回
unavailable，不能改由本地overload/member检索、callee名称或AST文本重建签名。
source callable-value assignment也使用同一boundary。compiler把initializer的精确callable
binding注册到type environment；若source function没有显式返回类型，该注册入口会从body
return metadata推断类型并沿原declaration AST identity找到function binding，以原SymbolId重绑定新的canonical function
TypeId，并更新该SymbolId已有的reference facts。LSP symbol bootstrap调用同一公开binding入口，
因此`pub var runBossScenario = runBossScenarioImpl`的后续调用直接发布`CallAt/FormatCall`，不从
variable name、initializer AST或callee文本重建签名。移除同一expression的`hasCallInfo`后，
signature help必须返回unavailable。

闭合generic receiver也属于同一边界。`Box<int>.shape(...)`缺少同一expression的
call fact时，不得以receiver的AST、open generic declaration或const-generic
substitution临时重建`shape`的闭合signature。

该合同也覆盖source lambda callable value。`var add = fn(left: int, right: int): int => left + right;`
的绑定把精确`ZR_AST_LAMBDA_EXPRESSION`作为declaration identity，发布同一lambda的
resolved SymbolId、canonical function TypeId和declaration range。`add(20, 22)`的
`CallAt/FormatCall`、hover、definition和signature help都只消费这些事实；definition返回
lambda declaration的完整range，而不是请求position。清除同一expression的`hasCallInfo`后，
signature help直接unavailable，不能从lambda AST、变量名或callee文本重建。binary/native/provider
callable value仍需独立canonical fact验收，不能借用本项作为fallback授权。

## 统一 PropertyDecl 的 interface variance

LSP semantic analysis 对 interface generic variance 直接消费统一
`ZR_AST_PROPERTY_DECLARATION` 的 accessor kind。只有 `get` 的 property 将其
declared type 放在 output/covariant position；只有 `set` 或 `init` 的 property
放在 input/contravariant position；同时具有读写 accessor 的 property 为
invariant。这个规则与 compiler generic semantics 共用同一 AST/accessor 合同，
不检查 property 名称、hidden accessor spelling 或源文本。

因此 interface variance 回归使用 current syntax，例如
`property item: T { get; }` 和 `property item: T { set; }`，而不保留已经不再
产生 semantic member 的 legacy `pub get/set` declaration。2026-07-26 的
M5.1 follow-up 在 GCC、Clang 和 MSVC 上通过 semantic-analyzer regression、
18-target LSP matrix 和三类 stdio/CLI smoke；仍有的 native constructor、
receiver completion、foreach/container Unity marker 被记录为独立后续工作。

## Snapshot-Only Canonical Signature Dispatch

Source direct-call signature help no longer requires an attached analyzer
compiler state or LSP symbol table. Once the current AST locates the call syntax,
the semantic result is supplied only by parser `CallAt/FormatCall` and its
canonical function TypeId. Parameter labels and optional argument facts are
read from that snapshot.

Legacy compiler-based constructor, super, member, and function resolution is
guarded only after both local and external canonical paths have been attempted.
If a resolved source call loses its canonical call payload, signature help
returns unavailable instead of re-entering overload, callee-name, or AST
signature reconstruction.
