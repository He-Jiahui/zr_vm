# Semantic Inference Fact Layer

## Scope

- First-stage shared semantic fact, diagnostic, and local LSP query layer.
- Affected layers: parser semantic context, parser diagnostics, type inference fact emission, language-server semantic analysis, LSP hover/local query, stdio diagnostic JSON, tests, and parser/semantic docs.
- Parser/LSP diagnostics now include empty control-statement conditions such as `if ()`, `while ()`, and `switch ()` as structured `missing_condition` diagnostics with concrete cause and suggestion text instead of a generic expected-token fallback.
- Parser/LSP diagnostics now also include malformed member/index syntax such as `value.` and `value[0;`, malformed declaration/expression close delimiters such as `func pick(value: int: int { ... }`, `class Box { func read(value: int: int { ... } }`, `interface Readable { read(value: int: int; }`, `interface Callable { @call(value: int: int; }`, `%extern("fixture") { NativeAdd(value: int: int; }`, `%extern("fixture") { delegate Callback(value: int: int; }`, and `return (1 + 2`, malformed array/object literal closes such as `return [1, 2` and `return {a: 1`, malformed aggregate separators such as `return [1 2];` and `{a: 1 b: 2}`, malformed object literal property syntax such as `{a 1}`, and missing statement terminators such as `return 1\nvar next = 2;`, `var seed = 1\nvar next = 2;`, `%module "main"\nvar next = 2;`, `break\nvar next = 2;`, `continue\nvar next = 2;`, `throw 1\nvar next = 2;`, `out 1\nvar next = 2;`, and `%using resource\nvar next = 2;`, as structured diagnostics with stable codes, concrete problem text, and repair suggestions instead of only expected-token fallback text.
- Parser/LSP missing statement terminator coverage now also includes interface method signatures such as `interface Readable { read(value: int): int }`, which reports `missing_statement_semicolon` with interface-method-signature-specific problem text and a repair suggestion.
- Parser/LSP missing statement terminator coverage now also includes interface meta signatures such as `interface Callable { @call(value: int): int }`, which reports `missing_statement_semicolon` with interface-meta-signature-specific problem text and a repair suggestion.
- Parser/LSP missing statement terminator coverage now also includes interface property signatures such as `interface Sized { get length: int }`, which reports `missing_statement_semicolon` with interface-property-signature-specific problem text and a repair suggestion.
- Parser/LSP missing statement terminator coverage now also includes interface field declarations such as `interface Entity { var id: int }`, which reports `missing_statement_semicolon` with interface-field-declaration-specific problem text and a repair suggestion.
- Parser/LSP missing statement terminator coverage now also includes class field declarations such as `class Entity { var id: int }`, which reports `missing_statement_semicolon` with class-field-declaration-specific problem text and a repair suggestion.
- Parser/LSP missing statement terminator coverage now also includes class getter/setter accessors such as `class Sized { get length: int }` and `class Sized { set length(value: int) }`, which report `missing_statement_semicolon` with accessor-specific problem text and repair suggestions.
- Parser/LSP missing statement terminator coverage now also includes class method/meta function signatures such as `class Box { func read(value: int): int }` and `class Callable { @call(value: int): int }`, which report `missing_statement_semicolon` with method/meta-specific problem text and repair suggestions.
- Parser/LSP declaration body opener coverage now includes class, interface, function, enum, extern block, and test declaration headers such as `class Box`, `interface Sized`, `func pick(): int`, `enum Tone`, `%extern("fixture")`, and `%test("smoke")`, which report `missing_declaration_body_open` with declaration-kind-specific problem text and repair suggestions.
- Parser/LSP statement body opener coverage now includes control statement and branch headers such as `if (ready)\nreturn 1;`, `while (ready)\nreturn 1;`, `for (;;)\nreturn 1;`, `for (var item in items)\nreturn item;`, `switch (choice)\nreturn 1;`, `switch (choice) { (1)\nreturn 1; }`, `switch (choice) { ()\nreturn 1; }`, `if (ready) { return 1; } else\nreturn 2;`, `try\nreturn 1;`, `try { throw 1; } catch (error)\nreturn 2;`, `try { return 1; } finally\nreturn 2;`, and `%using (resource)\nreturn resource;`, which report `missing_statement_body_open` with statement-kind-specific problem text and repair suggestions.
- Parser/LSP block-close coverage now includes missing closing braces such as `if (ready) { return 1;`, which report `missing_block_close` with concrete problem text and a repair suggestion instead of a generic expected-token fallback.
- Parser/LSP catch-pattern-close coverage now includes malformed catch headers such as `try { throw 1; } catch (error { return 2; }`, which report `missing_catch_pattern_close` with concrete problem text and a repair suggestion instead of a generic expected-token fallback.
- Parser/LSP using-resource-close coverage now includes malformed block-scoped using headers such as `%using (resource { return resource; }`, which report `missing_using_resource_close` with concrete problem text and a repair suggestion instead of a generic expected-token fallback.
- Debug and REPL are now partial consumers of the same diagnostic/expression direction; full shared fact reuse remains follow-up work.
- Second-stage Debug expression diagnostics now cover missing right operands for evaluate and conditional-breakpoint expressions, numeric semantic failures such as non-numeric operands and division by zero, composed comparison/logical evaluation for Debug conditions, conditional branch-missing diagnostics, and local member/index syntax failures such as missing member names or missing `]`.
- Debug runtime data inference now has protocol slices: `variables` value previews and `evaluate` results expose `semanticSummary` for expandable child shape, integer results, logical results, index windows, and ownership-tagged runtime values; successful `evaluate` results also append parser/type-inference expression, numeric, logical-flow, skipped-branch reachability, and parser-owned reference facts when the expression source can be parsed and inferred. Paused-frame evaluate fact replay now seeds visible frame slots, stable Debug globals, and compiled entry-function top-level callable metadata into the temporary parser type environment, so `inside + 1` can expose `reference read inside`, `zr` can expose `reference read zr`, and direct semantic-summary fact replay for `pick(1 + 2)` can expose `reference call pick` plus folded argument facts in `semanticSummary`. Top-level scope variables, simple identifier evaluate results, actual reads inside supported compound evaluate expressions, supported index-window base expressions, selected conditional branches, and successful ordinary member/index postfix evaluate expressions expose `referenceSummary` such as `global zr`, `local inside`, `global zr, global loadedModules`, or `global zr, index access`. Skipped short-circuit/conditional paths and index-window bounds do not produce reference summaries. This is still partial Debug fact consumption, not complete parser semantic parity inside the debugger, and safe evaluate still rejects executing function calls.
- Third-stage REPL expression execution now covers fresh bare expression submissions, aggregate-start array expressions, first-level array result display, line-start object literal expressions, and parser-backed missing-right-operand diagnostics. REPL `:type <expression>` now covers the first shared parser/type-inference fact consumption path for expression type, root and nested expression/numeric/ownership facts, logical/reachability facts, conditional-branch nested logical/reachability display, call/member expression payload display, member-access reference display, member-write reference display, and escaped string constant display for quote/backslash/control-character payloads. This acceptance record still does not claim full Debug expression inference, full Debug data inference, persistent REPL scope, or full REPL/LSP shared local-query parity are complete.
- LSP local expression queries now expose numeric facts, logical short-circuit facts, unary logical-not facts, constant numeric-comparison logical facts, composed comparison logical facts, and conditional-branch facts for operator/branch-position queries; this acceptance record still does not claim full VSCode UI integration or complete float/unsigned/variable range propagation.
- LSP local expression queries now also return ownership violation facts at the relevant ownership builtin expression, including `%loan(...)` return-escape diagnostics.
- LSP local hover and rich hover now surface reachability causes for unreachable code, for example `unreachable after return`, instead of reducing reachability to a bare boolean state.
- LSP local hover and rich hover now also surface constant-false loop body causes, for example `unreachable because the condition is false` inside `while (constFalse)`.
- LSP local hover and rich hover now also surface expression fact kind/exactness/constants and call/member expression payloads when local query hits an existing parser expression fact payload, for example `Expression: binary exact`, `Constant: 3`, escaped string constants such as `Constant: "a\"b\\c\n\t"`, `Call: pick args=1`, and `Member: value`. This acceptance record still does not claim call execution, member-chain type resolution, or UI synthesis for expressions without shared payload facts.
- Parser/type-inference reference facts now distinguish assignment writes from identifier reads for known local targets: `seed = 3;` records a `ZR_SEMANTIC_REFERENCE_WRITE` on the left `seed` token while preserving left expression/numeric facts.
- LSP local hover and rich hover now expose the same assignment write-reference fact for identifier assignment targets: hovering the left `seed` in `seed = 3;` shows `Reference: write`, symbol name, and declaration location.
- Parser/type-inference reference facts now also classify ordinary dot-member reads, computed member reads, and member/index assignment targets: `seed.value;` records unresolved `ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS` on `value`, `seed[index];` records unresolved `ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS` on the full member expression while preserving the `index` token's variable read fact, `seed.value = 3;` records unresolved `ZR_SEMANTIC_REFERENCE_MEMBER_WRITE` on `value`, and `seed[index] = 4;` records the write kind on the computed index expression. This does not claim member declaration resolution.
- LSP local query and REPL `:type` now surface ordinary and computed member-access reference facts for property reads; LSP local query, hover, rich hover, and REPL `:type` surface member-write reference facts for property assignment targets as `Reference: member write` with the member token name.
- Exact integer variable range propagation now covers the first local constant slice: `var seed = 2; seed + 3;` produces an exact `5..5` numeric fact in parser/type inference and through LSP local query. This acceptance record still does not claim unsigned or non-exact interval propagation.
- LSP inlay hints now consume initializer semantic facts for existing unannotated-local type hints, so `var sum = 1 + 2;` can surface `: int, range 3..3` without adding expression-level hint noise.
- LSP completion item details now consume initializer semantic facts for local variables through `lsp_completion_semantic_facts.c`, so completions can surface expression kind/exactness/constants, escaped string constants, numeric ranges, deterministic logical facts, and ownership violation reasons without duplicating parser/type-inference rules. This acceptance record still does not claim full completion resolve UI design, signature-help expression fact rendering, or non-local/interprocedural completion facts.
- LSP stdio inline values now consume numeric/logical/reference facts for local initializers, simple single-line `return` expressions, literal/boolean expression statements, identifier-led arithmetic expression statements, unary `!`/`-` expression statements, call/member expression statements that start at the source line after indentation when shared expression/reference payload facts exist, array-literal aggregate expression statements, computed-key object aggregate expression statements, ordinary object-literal expression statements whose first key/value shape is visible to the scanner, and simple operator-split expression statements when the request range starts on the continuation line. This acceptance record still does not claim runtime debugger value display, source-synthesized call/member inference, stdio-owned aggregate inference, full parser block/object disambiguation, or arbitrary multi-line expression range support.
- LSP stdio linked editing now filters fallback non-code tokens: `textDocument/linkedEditingRange` still returns same-document code identifier ranges, but whole-document fallback scanning no longer uses identifiers inside line comments, block comments, or string literals to synthesize linked edit ranges.
- LSP stdio moniker now filters non-code tokens: `textDocument/moniker` still returns document-scoped identities for real source identifiers, but ignores identifiers inside line comments, block comments, and string literals. This acceptance record still does not claim cross-document moniker linking or workspace symbol indexing.
- LSP shared semantic query now filters raw fallback identifiers for non-code spans: `textDocument/documentHighlight` still resolves real code identifiers and import literals through semantic paths, but line comments, block comments, and string literals no longer let ordinary matching text resolve as local symbols. This acceptance record still does not claim a parser token-stream replacement.
- LSP super-constructor navigation now filters non-code tokens: `textDocument/definition`, `textDocument/references`, and `textDocument/documentHighlight` still resolve real `super(...)` constructor calls and constructor declarations, but `super` text inside constructor comments or string literals no longer resolves to a constructor target. This acceptance record still does not claim parser token-stream replacement or whole-repository green.
- LSP completion now filters non-code token prefixes: `textDocument/completion` still offers directive/meta-method completions for real `%` / `@` code prefixes, but the same characters inside line comments or string literals return an empty completion list. This acceptance record still does not claim parser token-stream replacement, import-string path completion, or whole-repository green.
- LSP meta-method hover now filters non-code tokens: `textDocument/hover` still describes real `@constructor` declarations, but `@constructor` text inside line comments or string literals no longer renders meta-method documentation. This acceptance record still does not claim parser token-stream replacement or whole-repository green.
- LSP semantic tokens now filter backtick/template string token text: `textDocument/semanticTokens/full` still classifies real source `%import` directives and import-chain members, but `%import(...)`, `@constructor`, and `#trace#` inside backtick strings no longer produce keyword/meta-method/decorator tokens. This acceptance record still does not claim parser token-stream replacement, complete string-literal token parity beyond this scanner path, stdio smoke parity, or whole-repository green.
- LSP stdio inline completion now filters non-code typed prefixes: `textDocument/inlineCompletion` still expands real code prefixes such as `ret` / `retu` to `return `, but the same keyword prefixes inside line comments, block comments, and string literals return an empty array. This acceptance record still does not claim parser token-stream replacement, parser-backed inline completion, or whole-repository green.
- LSP stdio document colors now filter comment-only hex colors and presentation edits: `textDocument/documentColor` still exposes real source string color literals such as `"#336699"`, and `colorPresentation` still formats edits for those exposed literal ranges, but `#RRGGBB` / `#RRGGBBAA` examples inside line or block comments no longer produce color entries or direct presentation edits. This acceptance record still does not claim parser-backed color typing or whole-repository green.
- LSP code action semicolon quickfixes now filter block-comment bodies: real code such as `return answer // note` still receives a semicolon edit before the line comment, but `return answer` text inside a block comment no longer offers a missing-semicolon quickfix. This acceptance record still does not claim AST-backed code actions or whole-repository green.
- LSP stdio inline values now filter block-comment and string-literal raw scans: real code inline values and shared semantic facts are preserved, but variable-looking text inside multi-line or single-line block comments, double-quoted strings, single-quoted strings, or backtick/template strings no longer produces `InlineValueVariableLookup` or fact-backed inline text. This acceptance record still does not claim parser token-stream replacement or whole-repository green.
- LSP signature help now filters non-code cursor spans before AST call-context matching: real call arguments still surface signatures, but cursor positions inside comments embedded in call argument lists no longer return call signatures. This acceptance record still does not claim parser token-stream replacement, broader signature-help UI redesign, or whole-repository green.
- LSP receiver/member semantic navigation now filters non-code cursor spans before raw `receiver.member` fallback resolution: real `return box.value;` still resolves member definitions, but `box.value` text inside comments or string literals no longer resolves as a member target. This acceptance record still does not claim parser token-stream replacement, broader member-resolution redesign, or whole-repository green.
- LSP call hierarchy raw call scans now filter non-code spans: incoming/outgoing calls still see real direct calls, but `helper(value)` text inside comments or string literals no longer creates call hierarchy edges. This acceptance record still does not claim parser-backed call graph resolution or whole-repository green.
- LSP CodeLens `%test(...)` run marker scans now filter non-code spans: real test blocks still expose `zr.runCurrentProject`, but `%test(...)` text inside comments or string literals no longer creates run commands. This acceptance record still does not claim parser-backed test discovery or whole-repository green.

## Baseline

- The checkout already contains broad unrelated changes across runtime, parser, LSP, CLI, library, Rust binding, docs, and tests.
- Existing project policy expects WSL-first validation and records known whole-repository instability in `zr-vm-dev`.
- The Task 8 validation matrix still has one whole-repository ctest failure in `core_runtime`:
  - `zr_vm_execution_member_access_fast_paths_test::test_member_get_cached_property_getter_stack_slot_skips_anchor_restore_stack_lookup_when_stack_unchanged`
  - failure text: `Expected 0 Was 1`
  - scope: runtime member-access fast path, outside this parser/LSP semantic fact slice.

## Test Inventory

- Unit/focused parser:
  - `zr_vm_parser_test`
  - `zr_vm_semantic_facts_test`
  - `zr_vm_expression_fact_emission_test`
  - `zr_vm_logical_fact_emission_test`
  - `zr_vm_type_inference_test`
- Language-server focused:
  - `zr_vm_language_server_semantic_analyzer_test`
  - `zr_vm_language_server_ownership_diagnostics_test`
  - `zr_vm_language_server_local_semantic_query_test`
  - `zr_vm_language_server_logical_semantic_query_test`
  - `zr_vm_language_server_inlay_semantic_facts_test`
  - `zr_vm_language_server_local_semantic_hover_test`
  - `zr_vm_language_server_lsp_interface_test`
- CLI focused:
  - `zr_vm_cli_repl_e2e_test`
  - `tests/cli/repl_type_expression_fact_smoke.js`
  - `tests/cli/repl_type_ownership_smoke.js`
  - `tests/cli/repl_type_call_member_smoke.js`
  - `tests/cli/repl_type_logical_comparison_smoke.js`
  - `tests/cli/repl_type_reachability_smoke.js`
  - `tests/cli/repl_type_conditional_branch_smoke.js`
  - `tests/cli/repl_type_nested_numeric_smoke.js`
  - `tests/cli/repl_type_nested_expression_fact_smoke.js`
  - `tests/cli/repl_type_nested_ownership_smoke.js`
  - `tests/cli/repl_expression_object_smoke.js`
- Integration/stdio:
  - `language_server_stdio_smoke`
- Boundary and negative cases:
  - `var x = ;` parser diagnostic with stable code, problem text, and suggestion.
  - local semantic query at parser-blocked expression returns diagnostic failure.
  - hover at parser-blocked expression returns no misleading fallback result.
  - `1 + 2` operator-position expression fact returns exact `int`.
  - Composite binary/logical expression ranges cover operator gaps, so `1 + 2` at `+` and `true || false` at `||` both resolve to the structural expression.
  - `true || false` type inference records an exact `bool` expression fact aligned to the logical expression range.
  - registered identifier `seed` records an identifier expression fact and exact numeric range fact when the binding carries a `7..7` range.
  - `var result = pick(42);` records an exact expression fact for the function-call initializer after return-type inference, while intentionally not creating a numeric fact for the call expression.
  - `seed = 3;` records an assignment expression fact, plus identifier and literal facts for the left and right child expressions; when `seed` is a registered binding, the left token also records `ZR_SEMANTIC_REFERENCE_WRITE` and resolves to the declaration token.
  - `(x:int)->{ return x; }` records a lambda expression fact with callable closure type `%func(int)->int`.
  - `%borrow(owner);` records an ownership-builtin expression fact plus the matching shared borrow ownership fact when `owner` is `%unique`.
  - `var values = [1 + 2];` records the array literal expression fact and also materializes nested element facts for `1 + 2`, including a binary expression fact and exact `3..3` numeric promotion fact.
  - `var obj = {a: 1 + 2};` records the object literal expression fact and also materializes nested property-value facts for `1 + 2`, including a binary expression fact and exact `3..3` numeric promotion fact.
  - `var obj = {[1 + 2]: 4};` records `keyIsComputed=true`, the object literal expression fact, and nested computed-key facts for `1 + 2`, including a binary expression fact and exact `3..3` numeric promotion fact; plain `{a: ...}` keeps `keyIsComputed=false`.
  - `var obj = {[key]: 1, name: 2};` lowers the computed identifier key through `SET_BY_INDEX` while the plain identifier key still lowers through `SET_MEMBER`.
  - integer literal numeric facts record exact single-value range metadata.
  - integer literal AST ranges and numeric fact ranges remain aligned to the real literal token instead of drifting after token consumption.
  - integer constant `+` and `*` binary expressions record exact result range metadata when the result is representable.
  - integer constant overflow candidates such as `9223372036854775807 + 1` and `3037000500 * 3037000500` do not synthesize a fake range and set `mayOverflow`.
  - local LSP expression query at the `+` operator in `1 + 2` returns the binary expression numeric promotion fact with exact `3..3` range.
  - local LSP expression query at the `+` operator in `seed + 3` returns the binary expression numeric promotion fact with exact `5..5` range after `seed` is initialized from `2`.
  - local LSP expression query at the `+` operator in `9223372036854775807 + 1` returns the binary expression numeric promotion fact with `mayOverflow=true` and no fake range.
  - local LSP expression query at the `||` operator in `true || false` returns an exact `bool` expression fact, a short-circuit logical fact, and a short-circuit reachability fact.
  - local LSP expression query at the `!` operator in `!true` returns an exact `bool` unary expression fact and `ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_FALSE` with `knownValue=false`.
  - parser/type-inference records exact bool constants and logical facts for constant numeric comparisons: `1 < 2` records `ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_TRUE`, while `3 <= 2` records `ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_FALSE`.
  - local LSP expression query at the `<` operator in `1 < 2` returns a binary bool expression fact with `hasConstant=true` plus the matching `ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_TRUE`.
  - parser/type-inference now propagates comparison bool constants through logical-not and non-short-circuit `&&` / `||`: `!(1 < 2)` records false, and `(1 < 2) && (3 < 4)` records true with `ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_TRUE`.
  - local LSP expression query at the `&&` operator in `(1 < 2) && (3 < 4)` returns a binary bool expression fact with `hasConstant=true` and the matching exact logical fact.
  - local LSP expression query at the skipped alternate branch in `true ? 1 : 2` returns the conditional expression fact, selected numeric range `1..1`, constant-condition logical fact, and `ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH`.
  - parser/type-inference member reads emit unresolved `ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS` facts: `seed.value;` records `value` as the member-access token, while `seed[index];` records a wider member-access fact and preserves the narrower `index` variable read fact.
  - REPL `:type !(1 < 2)` and `:type (1 < 2) && (3 < 4)` print shared logical values without executing the expressions.
  - LSP inlay hint for `var sum = 1 + 2;` keeps the existing inferred-type hint location but enriches the label from the initializer numeric fact as `: int, range 3..3`.
  - LSP completion detail for `var sum = 1 + 2;` keeps the existing local completion item and enriches detail from the initializer numeric fact with `range 3..3`.
  - LSP completion detail for `var const flag = true || false;` keeps the existing local completion item and enriches detail from the initializer logical fact with `logical true` and `short-circuits`.
  - stdio `textDocument/inlineValue` for `var x = 20;` and `var flag = true || false;` keeps variable lookup entries and adds fact-backed inline text for `range 20..20` and `logical true, short-circuits`.
  - stdio `textDocument/inlineValue` for `return 1 + 2;` adds fact-backed inline text on the `1 + 2` expression range with `range 3..3`.
- stdio `textDocument/inlineValue` for line-start `1 + 2;`, `true || false;`, `seed + 3;`, `!true;`, and `-42;` expression statements adds fact-backed inline text on the expression range with `range 3..3`, `logical true, short-circuits`, `range 5..5`, `logical false`, and `range -42..-42`.
- stdio `textDocument/inlineValue` for line-start `pick(42);` and `seed.value;` expression statements adds fact-backed inline text on the expression range with `call pick args=1` and `member value`.
- stdio `textDocument/inlineValue` for line-start `seed[index];` expression statements adds fact-backed inline text on the expression range with `member index` and `reference member access`.
- stdio `textDocument/inlineValue` for line-start `[1 + 2];` and `[true || false];` aggregate expression statements adds fact-backed inline text on the aggregate expression range with `range 3..3` and `logical true, short-circuits`.
- stdio `textDocument/inlineValue` for object aggregate expression statements such as `{[1 + 2]: 4};`, `{a: 1 + 2};`, and `{\n  a: 1 + 2\n};` adds fact-backed inline text on the object expression range with `range 3..3` when parser/type inference exposes the nested numeric fact.
- stdio `textDocument/inlineValue` for a request range starting on the continuation line of `1 +\n 2;` adds fact-backed inline text on the full expression range with `range 3..3`.
- stdio `textDocument/inlineValue` returns no values for variable-looking text inside multi-line block-comment bodies, indented single-line block comments, or zero-column single-line block comments such as `/* var topGhost = 3; */`.
- stdio `textDocument/inlineValue` returns no values for variable-looking text inside double-quoted, single-quoted, and backtick/template string literals such as `"var stringGhost = 4;"`.
- stdio `textDocument/linkedEditingRange` returns ranges for code occurrences such as `var x = 20; var y = x;`, but returns `null` when only one code occurrence exists and the other matching words are inside comments or strings.
- stdio `textDocument/moniker` for `var x = 20;` returns a `zr` document-scoped identity for `x`, while comment/string candidates such as `// commentOnly`, `"stringOnly"`, and `/* blockOnly */` return empty moniker arrays.
- stdio `textDocument/documentHighlight` for `var highlightOnly = 1;` still returns code-token highlights, while matching `highlightOnly` text inside `//` comments, `"..."` string literals, and `/* ... */` comments returns empty highlight arrays through the shared semantic query fallback guard.
- direct LSP interface `definition`, `references`, and `documentHighlight` for `super(seed)` still resolve the base constructor call path, while `super` text inside constructor `//` comments and `"..."` string literals returns no locations/highlights.
- direct LSP advanced editor code action for a missing semicolon still inserts before a trailing line comment, while the same statement-looking text inside a block comment produces no semicolon quickfix.
  - local hover at a statement after `return` returns `Reachability: unreachable after return`, and rich hover exposes the same text through the `reachability` role.
  - local hover inside `var const keepGoing = false; while (keepGoing) { ... }` returns `Logical value: false` and `Reachability: unreachable because the condition is false`, and rich hover exposes `logicalValue` plus `reachability` roles.
  - local hover at `pick(42)` returns `Call: pick args=1`, and rich hover exposes the same payload through the `call` role.
  - local hover at `seed.value` returns `Member: value`, and rich hover exposes the same payload through the `member` role.
  - local hover at the assignment target `seed` in `var seed = 1; seed = 3;` returns `Reference: write`, `Symbol: seed`, and `Declared at: 1:5`, and rich hover exposes `reference`, `symbol`, and `declaration` roles.
  - parser/type-inference member assignment writes emit unresolved `ZR_SEMANTIC_REFERENCE_MEMBER_WRITE` facts: `seed.value = 3;` records `value`, and `seed[index] = 4;` records the computed index expression token range.
  - local LSP reference query at the member token `value` in `seed.value;` returns `ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS`, while the same token in `seed.value = 3;` still returns `ZR_SEMANTIC_REFERENCE_MEMBER_WRITE`.
  - local LSP reference query at `[` in `seed[index];` returns `ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS`, while the `index` token returns the resolved variable read reference.
  - local LSP reference query and hover at the assignment target `value` in `seed.value = 3;` return `Reference: member write` and `Symbol: value`, and rich hover exposes `reference` and `symbol` roles without pretending a declaration was resolved.
  - REPL `:type seed.value = 3` displays `Reference: member write value` from the same unresolved parser/type-inference reference fact and does not print a fake `Declared at:` location.
  - local LSP expression query at the return expression's `%loan(...)` returns the matching ownership violation fact instead of `UNKNOWN`.
  - reference fact at local symbol use points back to declaration token.
  - parser/type-inference resolved function calls now emit `ZR_SEMANTIC_REFERENCE_CALL` facts: `pick(42)` records the callee token range, the function declaration-name range, and resolved symbol/type ids after `ZrParser_TypeEnvironment_RegisterFunctionEx`.
  - parser/type-inference assignment writes now emit `ZR_SEMANTIC_REFERENCE_WRITE` facts: `var seed = 1; seed = 3;` records the write at the assignment target token, keeps the declaration range, and shares the declaration symbol/type ids.
  - Debug evaluate `1 +` returns a concrete missing-right-operand error with cause and suggestion.
  - Debug conditional-breakpoint expression `true &&` uses the same concrete missing-right-operand diagnostic path.
  - Debug evaluate `(1 < 2) && (3 < 4)` returns `bool true`, and `!(1 < 2)` returns `bool false`.
  - Debug conditional-breakpoint expressions `(1 < 2) || missingLocal` and `(2 < 1) && missingLocal` short-circuit without resolving the skipped local.
  - Debug conditional expressions `true ? : 2` and `true ? 1 :` report missing consequent/alternate branch diagnostics with concrete cause and suggestion.
  - Debug `variables` and `evaluate` protocol responses expose `semanticSummary` for `expandable ... named ...`, `integer 3`, and `logical true` cases.
  - Debug successful `evaluate` summaries now append parser-owned logical flow and reachability facts: `evaluate("true || false")` exposes `short-circuits` and `unreachable because short-circuit skips evaluation`, while `evaluate("true ? 1 : 2")` exposes constant-branch skipped-path reachability.
  - Debug successful paused-frame `evaluate("inside + 1")` now replays visible frame slots into the parser fact pass and exposes parser-owned `reference read inside` in `semanticSummary`.
  - Debug successful `evaluate("zr")` now replays stable Debug globals into the parser fact pass and exposes parser-owned `reference read zr` in `semanticSummary`.
  - Debug `variables`, simple identifier `evaluate`, compound `evaluate("inside + 1")`, index-window `evaluate("zr[1..3]")`, and normal indexed `evaluate("zr[1]")` protocol responses expose `referenceSummary` for stable top-level/current-frame/base-expression references such as `global zr` and `local inside` / `argument inside`; successful postfix index access adds `index access`, and skipped paths such as `true || inside` and `false ? inside : 2` do not expose a reference summary for `inside`.
  - Debug evaluate `"text" + 1` reports a numeric operand type error with concrete cause and suggestion.
  - Debug evaluate `1 / 0` reports division by zero with concrete cause and suggestion.
  - `loan_escape` ownership diagnostic emits concrete message, cause, suggestion, and ownership fact for `return %loan(resource);`.
  - Source compile rejects parser-reported expression errors instead of letting recovered AST reach runtime execution.
  - LSP parser diagnostics for `1 +` expose `missing_right_operand`, concrete problem text, and suggestion.
  - LSP parser diagnostics for `value.` expose `missing_member_name`, concrete problem text, and suggestion.
  - LSP parser diagnostics for `value[0;` expose `missing_index_close`, concrete problem text, and suggestion.
  - LSP parser diagnostics for `func pick(value: int: int { return value; }`, `class Box { func read(value: int: int { return value; } }`, `interface Readable { read(value: int: int; }`, `interface Callable { @call(value: int: int; }`, `%extern("fixture") { NativeAdd(value: int: int; }`, and `%extern("fixture") { delegate Callback(value: int: int; }` expose `missing_parameter_list_close`, concrete problem text, and suggestion.
  - LSP parser diagnostics for `return (1 + 2` expose `missing_group_close`, concrete problem text, and suggestion.
  - LSP parser diagnostics for `return [1, 2` expose `missing_array_close`, concrete problem text, and suggestion.
  - LSP parser diagnostics for `return [1 2];` expose `missing_array_element_separator`, concrete problem text, and suggestion.
  - LSP parser diagnostics for `return {a: 1` expose `missing_object_close`, concrete problem text, and suggestion.
  - LSP parser diagnostics for `return {[seed: 1}` expose `missing_object_computed_key_close`, concrete problem text, and suggestion.
  - LSP parser diagnostics for `{a 1}` expose `missing_object_property_colon`, concrete problem text, and suggestion.
  - LSP parser diagnostics for `{a: 1 b: 2}` expose `missing_object_property_separator`, concrete problem text, and suggestion.
  - REPL `1 + 2` prints `3`.
  - REPL `1 +` reports the user-facing missing-right-operand diagnostic without leaking the internal expression wrapper.
  - REPL `:type 1 + 2` reports `Type: int` and `Numeric range: 3..3` through parser/type-inference facts without executing the expression and printing `3`.
  - REPL `:type %borrow(owner)` reports `Type: %borrowed int` and `Ownership: borrow %borrowed` through parser/type-inference ownership facts without executing the expression.
  - REPL `:type pick(42)` displays `Call: pick args=1` from the shared expression fact without executing the function call.
  - REPL `:type seed.value` displays `Member: value` from the shared expression fact and `Reference: member value` from the shared reference fact.
  - REPL `:type seed[index]` displays `Member: index` from the shared expression fact and `Reference: member index` from the shared computed member-access reference fact.
  - REPL `:type seed.value = 3` displays `Reference: member write value` from the shared reference fact without executing the assignment and without pretending the unresolved member target has a declaration.
  - REPL `:type true || false` displays `Logical flow: short-circuits right operand` plus `Reachability: unreachable because short-circuit skips evaluation` from shared parser/type-inference facts without executing the expression.
  - REPL `:type 1 < 2` displays `Type: bool` and `Logical value: true` from shared parser/type-inference facts without executing the comparison.
  - REPL `:type true ? 1 : 2` displays `Expression: conditional exact`, selected `Numeric range: 1..1`, constant condition `Logical value: true`, and skipped-branch reachability from shared parser/type-inference facts without executing the expression.
  - REPL `:type [1 + 2]` displays `Expression: array exact` plus the nested element expression's `Numeric range: 3..3` from shared parser/type-inference facts without executing the aggregate expression.
  - REPL `:type [1 + 2]` also displays the nested element expression's `Expression: binary exact` and folded `Constant: 3`, while avoiding literal leaf `Constant: 1` / `Constant: 2` noise.
  - REPL `:type [%borrow(owner)]` after `var owner: %unique int;` displays aggregate borrowed type, nested ownership builtin expression fact, and nested `Ownership: borrow %borrowed` from shared parser/type-inference facts.
  - A direct REPL reference-display smoke for `var seed = 2; :type seed + 3` is deferred in this dirty checkout because submitting the prior declaration currently trips an unrelated core assertion in `execution_member_access.c`; parser/type-inference reference fact emission is covered independently by `zr_vm_reference_fact_emission_test`.

## Tooling Evidence

- Tooling:
  - WSL gcc Debug build: `build/codex-wsl-gcc-debug`
  - WSL clang Debug build: `build/codex-wsl-clang-debug`
  - Windows MSVC CLI Debug build: `build/codex-msvc-cli-debug`
  - Focused semantic WSL gcc Debug build: `build/codex-semantic-wsl-gcc-debug`
  - Focused semantic WSL clang Debug build: `build/codex-semantic-wsl-clang-debug`
  - Focused semantic Windows MSVC Debug build: `build\codex-semantic-msvc-debug`
  - Focused WSL Debug build for Debug/REPL diagnostics: `.codex/debug-expression-diagnostics-build`
  - PowerShell validation matrix: `.\.codex\skills\zr-vm-dev\scripts\validate-matrix.ps1 -Jobs 12`

- Focused stdio command:

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_stdio zr_vm_cli_executable -j 8 > build/codex-wsl-gcc-debug/zr_task8_stdio_build.out 2>&1 && ctest --test-dir build/codex-wsl-gcc-debug -R language_server_stdio_smoke --output-on-failure > build/codex-wsl-gcc-debug/zr_task8_stdio_ctest.out 2>&1"
```

- Focused inline-completion non-code validation (2026-06-05 17:07 +08:00):
  - RED WSL gcc `language_server_stdio_smoke` failed with `textDocument/inlineCompletion must ignore prefixes inside line comments` after the stdio smoke requested inline completion at `// ret`.
  - GREEN WSL gcc `build/codex-semantic-wsl-gcc-debug`: `zr_vm_language_server_stdio` / `zr_vm_cli_executable` rebuilt and `ctest -R language_server_stdio_smoke` passed.
  - GREEN WSL clang `build/codex-semantic-wsl-clang-debug`: same target rebuild and registered stdio smoke passed.
  - GREEN Windows MSVC `build\codex-semantic-msvc-debug`: after importing VsDevCmd and verifying `cl.exe`, `zr_vm_language_server_stdio` / `zr_vm_cli_executable` rebuilt and registered `language_server_stdio_smoke` passed.

- Focused document-color comment filtering validation (2026-06-05 17:22 +08:00):
  - RED WSL gcc `language_server_stdio_smoke` failed with `textDocument/documentColor must ignore hex colors inside comments` after the stdio smoke added `// "#112233"` and `/* "#445566" ... */`.
  - GREEN WSL gcc `build/codex-semantic-wsl-gcc-debug`: `zr_vm_language_server_stdio` rebuilt and registered stdio smoke passed.
  - GREEN WSL clang `build/codex-semantic-wsl-clang-debug`: same target rebuild and registered stdio smoke passed.
  - GREEN Windows MSVC `build\codex-semantic-msvc-debug`: after importing VsDevCmd and verifying `cl.exe`, `zr_vm_language_server_stdio` / `zr_vm_cli_executable` rebuilt and registered `language_server_stdio_smoke` passed.

- Focused color-presentation comment range validation (2026-06-05 17:31 +08:00):
  - RED WSL gcc `language_server_stdio_smoke` failed with `textDocument/colorPresentation must ignore comment-only hex colors` after the stdio smoke requested a presentation edit for the line-comment `#112233` range.
  - GREEN WSL gcc `build/codex-semantic-wsl-gcc-debug`: `zr_vm_language_server_stdio` rebuilt and registered stdio smoke passed.
  - GREEN WSL clang `build/codex-semantic-wsl-clang-debug`: same target rebuild and registered stdio smoke passed.
  - GREEN Windows MSVC `build\codex-semantic-msvc-debug`: after importing VsDevCmd and verifying `cl.exe`, `zr_vm_language_server_stdio` / `zr_vm_cli_executable` rebuilt and registered `language_server_stdio_smoke` passed.

- Focused inlineValue block-comment scanner validation (2026-06-05 18:14 +08:00):
  - RED WSL gcc `stdio_inline_value_semantic_smoke.js` first failed with `textDocument/inlineValue must ignore variable-looking text inside block comments` after requesting inline values over `var ghost = 1;` inside a multi-line block comment.
  - RED follow-up WSL gcc failed with `textDocument/inlineValue must ignore zero-column block-comment variables` after adding `/* var topGhost = 3; */`, proving a closed block comment starting at column zero was still scanned as code.
  - GREEN WSL gcc `build/codex-semantic-wsl-gcc-debug`: `zr_vm_language_server_stdio` rebuilt and the direct inline-value smoke passed.
  - GREEN WSL clang `build/codex-semantic-wsl-clang-debug`: same target rebuild and direct inline-value smoke passed.
  - GREEN Windows MSVC `build\codex-semantic-msvc-debug`: after importing VsDevCmd and verifying `cl.exe`, `zr_vm_language_server_stdio` rebuilt and direct inline-value smoke passed.

- Focused inlineValue string-literal scanner validation (2026-06-05 18:22 +08:00):
  - RED WSL gcc `stdio_inline_value_semantic_smoke.js` failed with `textDocument/inlineValue must ignore variable-looking text inside double-quoted strings` after requesting inline values over `"var stringGhost = 4;"`.
  - GREEN implementation moved raw line code-span detection into `stdio_inline_value_scan.c/.h`, skips double-quoted, single-quoted, and backtick/template string literals along with existing line/block comments, and keeps `stdio_inline_value.c` as request orchestration at 853 lines.
  - GREEN WSL gcc `build/codex-semantic-wsl-gcc-debug`: `zr_vm_language_server_stdio` rebuilt and the direct inline-value smoke passed.
  - GREEN WSL clang `build/codex-semantic-wsl-clang-debug`: same target rebuild and direct inline-value smoke passed.
  - GREEN Windows MSVC `build\codex-semantic-msvc-debug`: after importing VsDevCmd and verifying `cl.exe`, `zr_vm_language_server_stdio` rebuilt and direct inline-value smoke passed.

- Focused first-stage command:

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_semantic_facts_test zr_vm_type_inference_test zr_vm_language_server_semantic_analyzer_test zr_vm_language_server_lsp_interface_test zr_vm_language_server_stdio -j 8 > build/codex-wsl-gcc-debug/zr_task8_focus_build.out 2>&1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_semantic_facts_test > build/codex-wsl-gcc-debug/zr_task8_semantic_facts.out 2>&1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_type_inference_test > build/codex-wsl-gcc-debug/zr_task8_type_inference.out 2>&1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_semantic_analyzer_test > build/codex-wsl-gcc-debug/zr_task8_semantic_analyzer.out 2>&1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_lsp_interface_test > build/codex-wsl-gcc-debug/zr_task8_lsp_interface.out 2>&1 && ctest --test-dir build/codex-wsl-gcc-debug -R language_server_stdio_smoke --output-on-failure > build/codex-wsl-gcc-debug/zr_task8_stdio_smoke.out 2>&1"
```

- Full matrix command:

```powershell
.\.codex\skills\zr-vm-dev\scripts\validate-matrix.ps1 -Jobs 12
```

- Focused second-stage Debug diagnostics command:

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake -S . -B .codex/debug-expression-diagnostics-build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_STATIC_LIB=ON -DBUILD_SHARED_LIB=OFF && cmake --build .codex/debug-expression-diagnostics-build --target zr_vm_debug_expression_diagnostics_test zr_vm_debug_agent_test zr_vm_debug_agent_protocol_test -j 12 && ctest --test-dir .codex/debug-expression-diagnostics-build -R '^(debug_expression_diagnostics|debug_agent_protocol|debug_agent)$' --output-on-failure"
```

- Focused Debug numeric semantic diagnostics commands:

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_debug_expression_diagnostics_test zr_vm_debug_agent_test zr_vm_debug_agent_protocol_test -j 8 && ctest --test-dir build/codex-wsl-gcc-debug -R '^(debug_expression_diagnostics|debug_agent_protocol|debug_agent)$' --output-on-failure"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_debug_expression_diagnostics_test -j 8 && ./build/codex-wsl-clang-debug/bin/zr_vm_debug_expression_diagnostics_test"
```

- Focused Debug member/index syntax diagnostics commands:

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_debug_expression_diagnostics_test -j 8 && ./build/codex-wsl-gcc-debug/bin/zr_vm_debug_expression_diagnostics_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_debug_expression_diagnostics_test -j 8 && ./build/codex-wsl-clang-debug/bin/zr_vm_debug_expression_diagnostics_test"
```

- Focused Debug composed comparison/logical characterization commands:

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_debug_expression_diagnostics_test -j 6 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_debug_expression_diagnostics_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_debug_expression_diagnostics_test -j 6 && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_debug_expression_diagnostics_test"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-semantic-msvc-debug --config Debug --target zr_vm_debug_expression_diagnostics_test --parallel 6; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_debug_expression_diagnostics_test.exe
```

The focused Debug composed comparison/logical characterization rerun at 2026-06-05 00:58 +08:00 passed in all three isolated builds. `zr_vm_debug_expression_diagnostics_test` reported 15 tests, 0 failures under WSL GCC, WSL Clang, and Windows MSVC. No production Debug evaluator change was needed; the existing safe-evaluate parser already handled comparison results, unary logical-not, and comparison-driven `&&` / `||` short-circuiting.

- Focused Debug conditional branch diagnostics commands:

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_debug_expression_diagnostics_test -j 6 && build/codex-semantic-wsl-gcc-debug/bin/zr_vm_debug_expression_diagnostics_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_debug_expression_diagnostics_test -j 6 && build/codex-semantic-wsl-clang-debug/bin/zr_vm_debug_expression_diagnostics_test'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-semantic-msvc-debug --target zr_vm_debug_expression_diagnostics_test --config Debug --parallel 6; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_debug_expression_diagnostics_test.exe
```

The focused Debug conditional branch diagnostics rerun at 2026-06-05 +08:00 passed in all three isolated builds. `zr_vm_debug_expression_diagnostics_test` reported 18 tests, 0 failures under WSL GCC, WSL Clang, and Windows MSVC.

- Focused REPL/LSP conditional branch fact commands:

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-semantic-wsl-gcc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_cli_executable zr_vm_language_server_local_semantic_query_test -j 6 && node tests/cli/repl_type_conditional_branch_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli && ctest --test-dir build/codex-semantic-wsl-gcc-debug -R "^cli_repl_type_conditional_branch_smoke$" --output-on-failure && build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_query_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-semantic-wsl-clang-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_cli_executable zr_vm_language_server_local_semantic_query_test -j 6 && node tests/cli/repl_type_conditional_branch_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && ctest --test-dir build/codex-semantic-wsl-clang-debug -R "^cli_repl_type_conditional_branch_smoke$" --output-on-failure && build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_query_test'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake -S . -B build\codex-semantic-msvc-debug; cmake --build build\codex-semantic-msvc-debug --target zr_vm_cli_executable zr_vm_language_server_local_semantic_query_test --config Debug --parallel 6; node tests\cli\repl_type_conditional_branch_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe; ctest --test-dir build\codex-semantic-msvc-debug -C Debug -R "^cli_repl_type_conditional_branch_smoke$" --output-on-failure; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_local_semantic_query_test.exe
```

The focused REPL/LSP conditional branch fact rerun at 2026-06-05 +08:00 passed in all three isolated builds. The WSL gcc RED run for the new REPL smoke failed because `:type true ? 1 : 2` did not print `Logical value: true`; after the REPL semantic-fact writer traversed nested logical facts, WSL gcc, WSL clang, and Windows MSVC passed the direct smoke, registered CTest, and `zr_vm_language_server_local_semantic_query_test`.

- Focused REPL nested numeric fact commands:

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-semantic-wsl-gcc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_cli_executable -j 6 && node tests/cli/repl_type_nested_numeric_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli && node tests/cli/repl_type_expression_fact_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli && node tests/cli/repl_type_conditional_branch_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli && node tests/cli/repl_type_reachability_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli && ctest --test-dir build/codex-semantic-wsl-gcc-debug -R "^cli_repl_type_nested_numeric_smoke$" --output-on-failure'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-semantic-wsl-clang-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_cli_executable -j 6 && node tests/cli/repl_type_nested_numeric_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && node tests/cli/repl_type_expression_fact_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && node tests/cli/repl_type_conditional_branch_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && node tests/cli/repl_type_reachability_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && ctest --test-dir build/codex-semantic-wsl-clang-debug -R "^cli_repl_type_nested_numeric_smoke$" --output-on-failure'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake -S . -B build\codex-semantic-msvc-debug; cmake --build build\codex-semantic-msvc-debug --target zr_vm_cli_executable --config Debug --parallel 6; node tests\cli\repl_type_nested_numeric_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe; node tests\cli\repl_type_expression_fact_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe; node tests\cli\repl_type_conditional_branch_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe; node tests\cli\repl_type_reachability_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe; ctest --test-dir build\codex-semantic-msvc-debug -C Debug -R "^cli_repl_type_nested_numeric_smoke$" --output-on-failure
```

The focused REPL nested numeric rerun at 2026-06-05 +08:00 passed in all three isolated builds. The WSL gcc RED run for the new REPL smoke failed because `:type [1 + 2]` did not print `Numeric range: 3..3`; after adding the REPL expression walker and expression-wide numeric writer, WSL gcc, WSL clang, and Windows MSVC passed the direct smoke, adjacent REPL fact smokes, and registered CTest.

- Focused REPL nested expression fact commands:

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-semantic-wsl-gcc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_cli_executable -j 6 && node tests/cli/repl_type_nested_expression_fact_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli && node tests/cli/repl_type_nested_numeric_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli && node tests/cli/repl_type_expression_fact_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli && node tests/cli/repl_type_conditional_branch_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli && node tests/cli/repl_type_reachability_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli && ctest --test-dir build/codex-semantic-wsl-gcc-debug -R "^cli_repl_type_nested_expression_fact_smoke$" --output-on-failure'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-semantic-wsl-clang-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_cli_executable -j 6 && node tests/cli/repl_type_nested_expression_fact_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && node tests/cli/repl_type_nested_numeric_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && node tests/cli/repl_type_expression_fact_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && node tests/cli/repl_type_conditional_branch_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && node tests/cli/repl_type_reachability_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && ctest --test-dir build/codex-semantic-wsl-clang-debug -R "^cli_repl_type_nested_expression_fact_smoke$" --output-on-failure'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake -S . -B build\codex-semantic-msvc-debug; cmake --build build\codex-semantic-msvc-debug --target zr_vm_cli_executable --config Debug --parallel 6; node tests\cli\repl_type_nested_expression_fact_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe; node tests\cli\repl_type_nested_numeric_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe; node tests\cli\repl_type_expression_fact_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe; node tests\cli\repl_type_conditional_branch_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe; node tests\cli\repl_type_reachability_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe; ctest --test-dir build\codex-semantic-msvc-debug -C Debug -R "^cli_repl_type_nested_expression_fact_smoke$" --output-on-failure
```

The focused REPL nested expression fact rerun at 2026-06-05 +08:00 passed in all three isolated builds. The WSL gcc RED run for the new REPL smoke failed because `:type [1 + 2]` did not print `Expression: binary exact` / `Constant: 3`; after reusing the REPL expression walker for expression facts, WSL gcc, WSL clang, and Windows MSVC passed the direct smoke, adjacent REPL fact smokes, and registered CTest.

- Focused REPL nested ownership fact commands:

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-semantic-wsl-gcc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_cli_executable -j 6 && node tests/cli/repl_type_nested_ownership_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli && node tests/cli/repl_type_ownership_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli && node tests/cli/repl_type_nested_expression_fact_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli && node tests/cli/repl_type_nested_numeric_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli && node tests/cli/repl_type_expression_fact_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli && ctest --test-dir build/codex-semantic-wsl-gcc-debug -R "^cli_repl_type_nested_ownership_smoke$" --output-on-failure'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-semantic-wsl-clang-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_cli_executable -j 6 && node tests/cli/repl_type_nested_ownership_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && node tests/cli/repl_type_ownership_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && node tests/cli/repl_type_nested_expression_fact_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && node tests/cli/repl_type_nested_numeric_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && node tests/cli/repl_type_expression_fact_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && ctest --test-dir build/codex-semantic-wsl-clang-debug -R "^cli_repl_type_nested_ownership_smoke$" --output-on-failure'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake -S . -B build\codex-semantic-msvc-debug; cmake --build build\codex-semantic-msvc-debug --target zr_vm_cli_executable --config Debug --parallel 6; node tests\cli\repl_type_nested_ownership_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe; node tests\cli\repl_type_ownership_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe; node tests\cli\repl_type_nested_expression_fact_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe; node tests\cli\repl_type_nested_numeric_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe; node tests\cli\repl_type_expression_fact_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe; ctest --test-dir build\codex-semantic-msvc-debug -C Debug -R "^cli_repl_type_nested_ownership_smoke$" --output-on-failure
```

The focused REPL nested ownership fact rerun at 2026-06-05 +08:00 passed in all three isolated builds. The WSL gcc RED run for the new REPL smoke failed because `:type [%borrow(owner)]` did not print `Ownership: borrow %borrowed`; after moving expression-tree fact visitors into `repl_semantic_fact_walkers.c` and adding ownership traversal, WSL gcc, WSL clang, and Windows MSVC passed the direct smoke, adjacent REPL fact smokes, and registered CTest.

- Focused Debug runtime semantic summary commands:

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_debug_variable_child_shape_test zr_vm_debug_expression_diagnostics_test && ctest --test-dir build/codex-wsl-gcc-debug -R '^(debug_variable_child_shape|debug_expression_diagnostics)$' --output-on-failure"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_debug_variable_child_shape_test zr_vm_debug_expression_diagnostics_test && ctest --test-dir build/codex-wsl-clang-debug -R '^(debug_variable_child_shape|debug_expression_diagnostics)$' --output-on-failure"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-semantic-msvc-debug --config Debug --target zr_vm_debug_variable_child_shape_test zr_vm_debug_expression_diagnostics_test --parallel 6; ctest --test-dir build\codex-semantic-msvc-debug -C Debug -R "^(debug_variable_child_shape|debug_expression_diagnostics)$" --output-on-failure
```

- Focused third-stage REPL/parser diagnostics commands:

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build .codex/debug-expression-diagnostics-build --target zr_vm_parser_test zr_vm_cli_executable zr_vm_cli_repl_e2e_test zr_vm_language_server_lsp_interface_test zr_vm_debug_expression_diagnostics_test -j 12"
wsl bash -lc "cd /mnt/e/Git/zr_vm && .codex/debug-expression-diagnostics-build/bin/zr_vm_parser_test && .codex/debug-expression-diagnostics-build/bin/zr_vm_cli_repl_e2e_test && .codex/debug-expression-diagnostics-build/bin/zr_vm_language_server_lsp_interface_test && .codex/debug-expression-diagnostics-build/bin/zr_vm_semantic_facts_test && .codex/debug-expression-diagnostics-build/bin/zr_vm_type_inference_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && ctest --test-dir .codex/debug-expression-diagnostics-build -R '^(cli_repl_e2e|debug_expression_diagnostics)$' --output-on-failure"
```

- Focused REPL type-query command:

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_cli_executable zr_vm_cli_repl_e2e_test -j 8 && ./build/codex-wsl-gcc-debug/bin/zr_vm_cli_repl_e2e_test"
```

- Focused REPL ownership type-query command:

```powershell
wsl -e bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_cli_executable zr_vm_type_inference_test zr_vm_semantic_facts_test zr_vm_expression_fact_emission_test -j 4 && node tests/cli/repl_type_ownership_smoke.js build/codex-wsl-gcc-debug/bin/zr_vm_cli && build/codex-wsl-gcc-debug/bin/zr_vm_semantic_facts_test && build/codex-wsl-gcc-debug/bin/zr_vm_type_inference_test && build/codex-wsl-gcc-debug/bin/zr_vm_expression_fact_emission_test'
wsl -e bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_cli_executable zr_vm_type_inference_test zr_vm_semantic_facts_test zr_vm_expression_fact_emission_test -j 4 && node tests/cli/repl_type_ownership_smoke.js build/codex-wsl-clang-debug/bin/zr_vm_cli && build/codex-wsl-clang-debug/bin/zr_vm_semantic_facts_test && build/codex-wsl-clang-debug/bin/zr_vm_type_inference_test && build/codex-wsl-clang-debug/bin/zr_vm_expression_fact_emission_test'
node tests\cli\repl_type_ownership_smoke.js build\codex-msvc-debug\bin\Debug\zr_vm_cli.exe
.\build\codex-msvc-debug\bin\Debug\zr_vm_semantic_facts_test.exe
.\build\codex-msvc-debug\bin\Debug\zr_vm_type_inference_test.exe
.\build\codex-msvc-debug\bin\Debug\zr_vm_expression_fact_emission_test.exe
wsl -e bash -lc 'cd /mnt/e/Git/zr_vm && ctest --test-dir build/codex-wsl-gcc-debug -R cli_repl_type_ownership_smoke --output-on-failure'
```

- Focused REPL call/member type-query commands:

```powershell
wsl -e bash -lc 'cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-wsl-gcc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ && cmake --build build/codex-wsl-gcc-debug --target zr_vm_cli_executable zr_vm_expression_fact_emission_test zr_vm_semantic_facts_test zr_vm_type_inference_test -j 4 && node tests/cli/repl_type_call_member_smoke.js build/codex-wsl-gcc-debug/bin/zr_vm_cli && ctest --test-dir build/codex-wsl-gcc-debug -R cli_repl_type_call_member_smoke --output-on-failure && ./build/codex-wsl-gcc-debug/bin/zr_vm_semantic_facts_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_expression_fact_emission_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_type_inference_test'
wsl -e bash -lc 'cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-wsl-clang-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ && cmake --build build/codex-wsl-clang-debug --target zr_vm_cli_executable zr_vm_expression_fact_emission_test zr_vm_semantic_facts_test zr_vm_type_inference_test -j 4 && node tests/cli/repl_type_call_member_smoke.js build/codex-wsl-clang-debug/bin/zr_vm_cli && ./build/codex-wsl-clang-debug/bin/zr_vm_semantic_facts_test && ./build/codex-wsl-clang-debug/bin/zr_vm_expression_fact_emission_test && ./build/codex-wsl-clang-debug/bin/zr_vm_type_inference_test'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-msvc-debug --target zr_vm_cli_executable --config Debug; node tests\cli\repl_type_call_member_smoke.js build\codex-msvc-debug\bin\Debug\zr_vm_cli.exe
```

- Focused REPL short-circuit reachability type-query commands:

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_cli_executable -j 8 && node tests/cli/repl_type_reachability_smoke.js build/codex-wsl-gcc-debug/bin/zr_vm_cli"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_expression_fact_emission_test -j 8 && ./build/codex-wsl-gcc-debug/bin/zr_vm_expression_fact_emission_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-wsl-gcc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ && cmake --build build/codex-wsl-gcc-debug --target zr_vm_expression_fact_emission_test zr_vm_cli_executable -j 8 && ctest --test-dir build/codex-wsl-gcc-debug -R '^cli_repl_type_(ownership|call_member|reachability)_smoke$|^cli_repl_e2e$' --output-on-failure"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_expression_fact_emission_test zr_vm_cli_executable -j 8 && ./build/codex-wsl-clang-debug/bin/zr_vm_expression_fact_emission_test && node tests/cli/repl_type_reachability_smoke.js build/codex-wsl-clang-debug/bin/zr_vm_cli"
wsl bash -lc "cd /mnt/e/Git/zr_vm && ./build/codex-wsl-clang-debug/bin/zr_vm_expression_fact_emission_test && ctest --test-dir build/codex-wsl-clang-debug -R '^cli_repl_type_(ownership|call_member|reachability)_smoke$' --output-on-failure"
cmake --build build\codex-msvc-debug --config Debug --target zr_vm_expression_fact_emission_test zr_vm_cli_executable --parallel 8; if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }; .\build\codex-msvc-debug\bin\Debug\zr_vm_expression_fact_emission_test.exe; if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }; node tests\cli\repl_type_reachability_smoke.js build\codex-msvc-debug\bin\Debug\zr_vm_cli.exe
node tests\cli\repl_type_ownership_smoke.js build\codex-msvc-debug\bin\Debug\zr_vm_cli.exe; if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }; node tests\cli\repl_type_call_member_smoke.js build\codex-msvc-debug\bin\Debug\zr_vm_cli.exe; if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }; node tests\cli\repl_type_reachability_smoke.js build\codex-msvc-debug\bin\Debug\zr_vm_cli.exe
```

- Focused parser/LSP/REPL constant and composed comparison logical fact commands:

```powershell
wsl cmake --build build/codex-wsl-gcc-debug --target zr_vm_logical_fact_emission_test zr_vm_expression_fact_emission_test zr_vm_language_server_logical_semantic_query_test zr_vm_language_server_local_semantic_query_test zr_vm_cli_executable -j 8
wsl build/codex-wsl-gcc-debug/bin/zr_vm_logical_fact_emission_test
wsl build/codex-wsl-gcc-debug/bin/zr_vm_expression_fact_emission_test
wsl build/codex-wsl-gcc-debug/bin/zr_vm_language_server_logical_semantic_query_test
wsl build/codex-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_query_test
wsl node tests/cli/repl_type_logical_comparison_smoke.js build/codex-wsl-gcc-debug/bin/zr_vm_cli
wsl node tests/cli/repl_type_reachability_smoke.js build/codex-wsl-gcc-debug/bin/zr_vm_cli
wsl cmake --build build/codex-wsl-clang-debug --target zr_vm_logical_fact_emission_test zr_vm_expression_fact_emission_test zr_vm_language_server_logical_semantic_query_test zr_vm_language_server_local_semantic_query_test zr_vm_cli_executable -j 8
wsl build/codex-wsl-clang-debug/bin/zr_vm_logical_fact_emission_test
wsl build/codex-wsl-clang-debug/bin/zr_vm_expression_fact_emission_test
wsl build/codex-wsl-clang-debug/bin/zr_vm_language_server_logical_semantic_query_test
wsl build/codex-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_query_test
wsl node tests/cli/repl_type_logical_comparison_smoke.js build/codex-wsl-clang-debug/bin/zr_vm_cli
wsl node tests/cli/repl_type_reachability_smoke.js build/codex-wsl-clang-debug/bin/zr_vm_cli
& "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Invoke-VsDevCommand.ps1" cmake --build build\codex-msvc-debug --config Debug --target zr_vm_logical_fact_emission_test zr_vm_expression_fact_emission_test zr_vm_language_server_logical_semantic_query_test zr_vm_language_server_local_semantic_query_test zr_vm_cli_executable --parallel 8
.\build\codex-msvc-debug\bin\Debug\zr_vm_logical_fact_emission_test.exe
.\build\codex-msvc-debug\bin\Debug\zr_vm_expression_fact_emission_test.exe
.\build\codex-msvc-debug\bin\Debug\zr_vm_language_server_logical_semantic_query_test.exe
.\build\codex-msvc-debug\bin\Debug\zr_vm_language_server_local_semantic_query_test.exe
node tests\cli\repl_type_logical_comparison_smoke.js build\codex-msvc-debug\bin\Debug\zr_vm_cli.exe
node tests\cli\repl_type_reachability_smoke.js build\codex-msvc-debug\bin\Debug\zr_vm_cli.exe
```

The 2026-06-05 post-modularization rerun used isolated build directories `build/codex-semantic-wsl-gcc-debug`, `build/codex-semantic-wsl-clang-debug`, and `build\codex-semantic-msvc-debug` because another active session was using the normal WSL GCC build directory. The same focused parser/LSP/REPL targets passed in all three isolated builds.

- Focused ownership diagnostics command:

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build .codex/debug-expression-diagnostics-build --target zr_vm_language_server_ownership_diagnostics_test -j 12 && .codex/debug-expression-diagnostics-build/bin/zr_vm_language_server_ownership_diagnostics_test"
```

- Focused expression fact emission commands:

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build .codex/debug-expression-diagnostics-build --target zr_vm_expression_fact_emission_test zr_vm_type_inference_test zr_vm_semantic_facts_test -j 12 && .codex/debug-expression-diagnostics-build/bin/zr_vm_expression_fact_emission_test && .codex/debug-expression-diagnostics-build/bin/zr_vm_semantic_facts_test && .codex/debug-expression-diagnostics-build/bin/zr_vm_type_inference_test >/tmp/zr_type_after_expression_fact.out 2>&1; status=$?; tail -12 /tmp/zr_type_after_expression_fact.out; exit $status"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build .codex/debug-expression-diagnostics-build --target zr_vm_language_server_lsp_interface_test zr_vm_language_server_semantic_analyzer_test zr_vm_parser_test -j 12 && .codex/debug-expression-diagnostics-build/bin/zr_vm_language_server_lsp_interface_test >/tmp/zr_lsp_after_expression_fact.out 2>&1; status=$?; grep -E 'All LSP Interface Tests Completed|FAIL|Fail|Expression Fact|Hover' /tmp/zr_lsp_after_expression_fact.out | tail -20; exit $status"
```

- Focused parser/type-inference assignment write-reference commands:

```powershell
wsl cmake --build build/codex-wsl-gcc-debug --target zr_vm_reference_fact_emission_test zr_vm_expression_fact_emission_test -j 8
wsl ./build/codex-wsl-gcc-debug/bin/zr_vm_reference_fact_emission_test
wsl ./build/codex-wsl-gcc-debug/bin/zr_vm_expression_fact_emission_test
wsl cmake --build build/codex-wsl-clang-debug --target zr_vm_reference_fact_emission_test zr_vm_expression_fact_emission_test -j 8
wsl ./build/codex-wsl-clang-debug/bin/zr_vm_reference_fact_emission_test
wsl ./build/codex-wsl-clang-debug/bin/zr_vm_expression_fact_emission_test
& "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Invoke-VsDevCommand.ps1" cmake --build build\codex-msvc-debug --config Debug --target zr_vm_reference_fact_emission_test zr_vm_expression_fact_emission_test --parallel 8
.\build\codex-msvc-debug\bin\Debug\zr_vm_reference_fact_emission_test.exe
.\build\codex-msvc-debug\bin\Debug\zr_vm_expression_fact_emission_test.exe
```

- Focused LSP local assignment write-reference hover commands:

```powershell
wsl cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_local_semantic_query_test -j 8
wsl ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_hover_test
wsl ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_query_test
wsl cmake --build build/codex-wsl-clang-debug --target zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_local_semantic_query_test -j 8
wsl ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_hover_test
wsl ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_query_test
& "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Invoke-VsDevCommand.ps1" cmake --build build\codex-msvc-debug --config Debug --target zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_local_semantic_query_test --parallel 8
.\build\codex-msvc-debug\bin\Debug\zr_vm_language_server_local_semantic_hover_test.exe
.\build\codex-msvc-debug\bin\Debug\zr_vm_language_server_local_semantic_query_test.exe
```

- Focused parser/LSP member write-reference commands:

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_reference_fact_emission_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_local_semantic_hover_test -j 6 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_reference_fact_emission_test && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_query_test && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_hover_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_reference_fact_emission_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_local_semantic_hover_test -j 6 && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_reference_fact_emission_test && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_query_test && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_hover_test'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
cmake --build build\codex-semantic-msvc-debug --config Debug --target zr_vm_reference_fact_emission_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_local_semantic_hover_test --parallel 6
.\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_reference_fact_emission_test.exe
.\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_local_semantic_query_test.exe
.\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_local_semantic_hover_test.exe
```

- Focused REPL member write-reference display commands:

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_cli_executable -j 6 && node tests/cli/repl_type_call_member_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_reference_fact_emission_test zr_vm_cli_executable -j 6 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_reference_fact_emission_test && node tests/cli/repl_type_call_member_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_reference_fact_emission_test zr_vm_cli_executable -j 6 && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_reference_fact_emission_test && node tests/cli/repl_type_call_member_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
cmake --build build\codex-semantic-msvc-debug --config Debug --target zr_vm_reference_fact_emission_test zr_vm_cli_executable --parallel 6
.\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_reference_fact_emission_test.exe
node tests\cli\repl_type_call_member_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe
```

- Focused numeric range and overflow fact command:

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build .codex/debug-expression-diagnostics-build --target zr_vm_expression_fact_emission_test zr_vm_semantic_facts_test zr_vm_type_inference_test zr_vm_language_server_lsp_interface_test -j 12 && .codex/debug-expression-diagnostics-build/bin/zr_vm_expression_fact_emission_test && .codex/debug-expression-diagnostics-build/bin/zr_vm_semantic_facts_test && .codex/debug-expression-diagnostics-build/bin/zr_vm_type_inference_test >/tmp/zr_type_numeric_fact2.out 2>&1 && .codex/debug-expression-diagnostics-build/bin/zr_vm_language_server_lsp_interface_test >/tmp/zr_lsp_numeric_fact2.out 2>&1; status=$?; tail -12 /tmp/zr_type_numeric_fact2.out; grep -E 'All LSP Interface Tests Completed|FAIL|Fail|Expression Fact|Hover' /tmp/zr_lsp_numeric_fact2.out | tail -20; exit $status"
```

- Focused LSP local numeric query command:

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build .codex/debug-expression-diagnostics-build --target zr_vm_parser_test zr_vm_expression_fact_emission_test zr_vm_semantic_facts_test zr_vm_type_inference_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_lsp_interface_test -j 12 && .codex/debug-expression-diagnostics-build/bin/zr_vm_parser_test >/tmp/zr_parser_local_numeric.out 2>&1 && .codex/debug-expression-diagnostics-build/bin/zr_vm_expression_fact_emission_test >/tmp/zr_expr_local_numeric.out 2>&1 && .codex/debug-expression-diagnostics-build/bin/zr_vm_semantic_facts_test >/tmp/zr_semantic_facts_local_numeric.out 2>&1 && .codex/debug-expression-diagnostics-build/bin/zr_vm_type_inference_test >/tmp/zr_type_local_numeric.out 2>&1 && .codex/debug-expression-diagnostics-build/bin/zr_vm_language_server_local_semantic_query_test >/tmp/zr_lsp_local_numeric.out 2>&1 && .codex/debug-expression-diagnostics-build/bin/zr_vm_language_server_lsp_interface_test >/tmp/zr_lsp_interface_local_numeric.out 2>&1; status=$?; echo '--- parser ---'; tail -8 /tmp/zr_parser_local_numeric.out; echo '--- expression ---'; tail -8 /tmp/zr_expr_local_numeric.out; echo '--- semantic facts ---'; tail -8 /tmp/zr_semantic_facts_local_numeric.out; echo '--- type inference ---'; tail -8 /tmp/zr_type_local_numeric.out; echo '--- lsp local ---'; tail -12 /tmp/zr_lsp_local_numeric.out; echo '--- lsp interface ---'; grep -E 'All LSP Interface Tests Completed|FAIL|Fail|Local Semantic Query|Expression Fact|Hover' /tmp/zr_lsp_interface_local_numeric.out | tail -24; exit $status"
```

- Focused LSP local logical short-circuit query command:

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build .codex/debug-expression-diagnostics-build --target zr_vm_expression_fact_emission_test zr_vm_semantic_facts_test zr_vm_language_server_local_semantic_query_test -j 4 && .codex/debug-expression-diagnostics-build/bin/zr_vm_expression_fact_emission_test && .codex/debug-expression-diagnostics-build/bin/zr_vm_semantic_facts_test && .codex/debug-expression-diagnostics-build/bin/zr_vm_language_server_local_semantic_query_test"
```

- Focused LSP local unary logical-not query commands:

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_expression_fact_emission_test zr_vm_language_server_local_semantic_query_test -j 8 && ./build/codex-wsl-gcc-debug/bin/zr_vm_expression_fact_emission_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_query_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_expression_fact_emission_test zr_vm_language_server_local_semantic_query_test -j 8 && ./build/codex-wsl-clang-debug/bin/zr_vm_expression_fact_emission_test && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_query_test"
cmake --build build\codex-msvc-debug --config Debug --target zr_vm_expression_fact_emission_test zr_vm_language_server_local_semantic_query_test --parallel 8; if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }; .\build\codex-msvc-debug\bin\Debug\zr_vm_expression_fact_emission_test.exe; if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }; .\build\codex-msvc-debug\bin\Debug\zr_vm_language_server_local_semantic_query_test.exe
```

- Focused first-stage logical short-circuit regression command:

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build .codex/debug-expression-diagnostics-build --target zr_vm_parser_test zr_vm_expression_fact_emission_test zr_vm_semantic_facts_test zr_vm_type_inference_test zr_vm_language_server_semantic_analyzer_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_lsp_interface_test -j 4 && .codex/debug-expression-diagnostics-build/bin/zr_vm_parser_test >/tmp/zr_parser_phase1.out 2>&1 && .codex/debug-expression-diagnostics-build/bin/zr_vm_expression_fact_emission_test >/tmp/zr_expr_facts_phase1.out 2>&1 && .codex/debug-expression-diagnostics-build/bin/zr_vm_semantic_facts_test >/tmp/zr_semantic_facts_phase1.out 2>&1 && .codex/debug-expression-diagnostics-build/bin/zr_vm_type_inference_test >/tmp/zr_type_inference_phase1.out 2>&1 && .codex/debug-expression-diagnostics-build/bin/zr_vm_language_server_semantic_analyzer_test >/tmp/zr_lsp_semantic_analyzer_phase1.out 2>&1 && .codex/debug-expression-diagnostics-build/bin/zr_vm_language_server_local_semantic_query_test >/tmp/zr_lsp_local_query_phase1.out 2>&1 && .codex/debug-expression-diagnostics-build/bin/zr_vm_language_server_lsp_interface_test >/tmp/zr_lsp_interface_phase1.out 2>&1"
```

- Focused local ownership query regression command:

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_ownership_diagnostics_test zr_vm_language_server_local_semantic_query_test -j 8 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_ownership_diagnostics_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_query_test"
```

- Focused variable numeric range propagation commands:

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_expression_fact_emission_test zr_vm_type_inference_test zr_vm_language_server_local_semantic_query_test -j 8 && ./build/codex-wsl-gcc-debug/bin/zr_vm_expression_fact_emission_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_type_inference_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_query_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_expression_fact_emission_test zr_vm_type_inference_test zr_vm_language_server_local_semantic_query_test -j 8 >/tmp/zr_var_range_clang_build.out 2>&1 && ./build/codex-wsl-clang-debug/bin/zr_vm_expression_fact_emission_test >/tmp/zr_var_range_clang_expr.out 2>&1 && ./build/codex-wsl-clang-debug/bin/zr_vm_type_inference_test >/tmp/zr_var_range_clang_type.out 2>&1 && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_query_test >/tmp/zr_var_range_clang_lsp.out 2>&1; status=$?; tail -14 /tmp/zr_var_range_clang_expr.out; tail -10 /tmp/zr_var_range_clang_type.out; tail -20 /tmp/zr_var_range_clang_lsp.out; exit $status"
```

- Focused reachability hover cause commands:

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_lsp_interface_test -j 8 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_hover_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_query_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_lsp_interface_test >/tmp/zr_lsp_interface_reachability_hover_gcc_rerun.out 2>&1"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_local_semantic_query_test -j 1 && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_hover_test && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_query_test"
```

- Focused constant-false loop reachability hover RED/GREEN command:

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-wsl-gcc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_local_semantic_hover_test -j 8 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_hover_test"
```

- Focused LSP completion semantic fact commands:

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-wsl-gcc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_inlay_semantic_facts_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_lsp_interface_test zr_vm_language_server_semantic_analyzer_test -j 8 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_inlay_semantic_facts_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_query_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_hover_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_semantic_analyzer_test >/tmp/zr_lsp_semantic_analyzer_completion_fact_gcc.out 2>&1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_lsp_interface_test >/tmp/zr_lsp_interface_completion_fact_gcc.out 2>&1"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-wsl-clang-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ && cmake --build build/codex-wsl-clang-debug --target zr_vm_language_server_inlay_semantic_facts_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_lsp_interface_test zr_vm_language_server_semantic_analyzer_test -j 8 && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_inlay_semantic_facts_test && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_query_test && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_hover_test && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_semantic_analyzer_test >/tmp/zr_lsp_semantic_analyzer_completion_fact_clang.out 2>&1 && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_lsp_interface_test >/tmp/zr_lsp_interface_completion_fact_clang.out 2>&1"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake -S . -B build\codex-msvc-lsp-smoke -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTS=OFF -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF; cmake --build build\codex-msvc-lsp-smoke --config Debug --target zr_vm_language_server_shared zr_vm_language_server_stdio --parallel 8; cmake -S . -B build\codex-msvc-cli-debug -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTS=OFF -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF; cmake --build build\codex-msvc-cli-debug --config Debug --target zr_vm_cli_executable --parallel 8; .\build\codex-msvc-cli-debug\bin\Debug\zr_vm_cli.exe .\tests\fixtures\projects\hello_world\hello_world.zrp
```

- Focused LSP stdio return-expression inline value commands:

```powershell
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && node tests/language_server/stdio_smoke.js build/codex-wsl-clang-debug/bin/zr_vm_language_server_stdio build/codex-wsl-clang-debug/bin/zr_vm_cli"
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_language_server_stdio zr_vm_cli_executable -j 4 && node tests/language_server/stdio_smoke.js build/codex-wsl-clang-debug/bin/zr_vm_language_server_stdio build/codex-wsl-clang-debug/bin/zr_vm_cli"
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_stdio zr_vm_cli_executable -j 4 && node tests/language_server/stdio_smoke.js build/codex-wsl-gcc-debug/bin/zr_vm_language_server_stdio build/codex-wsl-gcc-debug/bin/zr_vm_cli"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-msvc-lsp-smoke --config Debug --target zr_vm_language_server_shared zr_vm_language_server_stdio --parallel 4; node tests\language_server\stdio_smoke.js build\codex-msvc-lsp-smoke\bin\Debug\zr_vm_language_server_stdio.exe build\codex-msvc-cli-debug\bin\Debug\zr_vm_cli.exe
```

- Focused LSP stdio expression-statement inline value commands:

```powershell
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_language_server_stdio zr_vm_cli_executable -j 4 > /tmp/zr_inline_expr_clang_build.out 2>&1; status=$?; tail -80 /tmp/zr_inline_expr_clang_build.out; exit $status"
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && node tests/language_server/stdio_smoke.js build/codex-wsl-clang-debug/bin/zr_vm_language_server_stdio build/codex-wsl-clang-debug/bin/zr_vm_cli"
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_stdio zr_vm_cli_executable -j 4 > /tmp/zr_inline_expr_gcc_build.out 2>&1; status=$?; tail -80 /tmp/zr_inline_expr_gcc_build.out; exit $status"
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && node tests/language_server/stdio_smoke.js build/codex-wsl-gcc-debug/bin/zr_vm_language_server_stdio build/codex-wsl-gcc-debug/bin/zr_vm_cli"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-msvc-lsp-smoke --config Debug --target zr_vm_language_server_shared zr_vm_language_server_stdio --parallel 4; node tests\language_server\stdio_smoke.js build\codex-msvc-lsp-smoke\bin\Debug\zr_vm_language_server_stdio.exe build\codex-msvc-cli-debug\bin\Debug\zr_vm_cli.exe
```

- Focused LSP stdio identifier expression-statement inline value commands:

```powershell
wsl -e bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_language_server_local_semantic_query_test zr_vm_language_server_stdio -j 4 > /tmp/zr_inline_identifier_clang_build.out 2>&1; status=$?; tail -80 /tmp/zr_inline_identifier_clang_build.out; exit $status'
wsl -e bash -lc 'cd /mnt/e/Git/zr_vm && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_query_test && node tests/language_server/stdio_inline_value_semantic_smoke.js build/codex-wsl-clang-debug/bin/zr_vm_language_server_stdio && node tests/language_server/stdio_smoke.js build/codex-wsl-clang-debug/bin/zr_vm_language_server_stdio build/codex-wsl-clang-debug/bin/zr_vm_cli'
wsl -e bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_local_semantic_query_test zr_vm_language_server_stdio -j 4 > /tmp/zr_inline_identifier_gcc_build.out 2>&1; status=$?; tail -80 /tmp/zr_inline_identifier_gcc_build.out; exit $status'
wsl -e bash -lc 'cd /mnt/e/Git/zr_vm && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_query_test && node tests/language_server/stdio_inline_value_semantic_smoke.js build/codex-wsl-gcc-debug/bin/zr_vm_language_server_stdio && node tests/language_server/stdio_smoke.js build/codex-wsl-gcc-debug/bin/zr_vm_language_server_stdio build/codex-wsl-gcc-debug/bin/zr_vm_cli'
& "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Invoke-VsDevCommand.ps1" cmake --build build\codex-msvc-debug --config Debug --target zr_vm_language_server_stdio zr_vm_language_server_local_semantic_query_test --parallel 4
.\build\codex-msvc-debug\bin\Debug\zr_vm_language_server_local_semantic_query_test.exe
node .\tests\language_server\stdio_inline_value_semantic_smoke.js .\build\codex-msvc-debug\bin\Debug\zr_vm_language_server_stdio.exe
node .\tests\language_server\stdio_smoke.js .\build\codex-msvc-debug\bin\Debug\zr_vm_language_server_stdio.exe .\build\codex-msvc-cli-debug\bin\Debug\zr_vm_cli.exe
```

## Results

- Passed:
  - `tests/language_server/stdio_smoke.js` against WSL clang `build/codex-wsl-clang-debug`: RED first failed on `textDocument/inlineValue must expose semantic numeric facts for return expressions`, then GREEN passed after rebuilding `zr_vm_language_server_stdio` and `zr_vm_cli_executable`.
  - `tests/language_server/stdio_smoke.js` against WSL gcc `build/codex-wsl-gcc-debug`: passed after rebuilding `zr_vm_language_server_stdio` and `zr_vm_cli_executable`.
  - `tests/language_server/stdio_smoke.js` against Windows MSVC `build\codex-msvc-lsp-smoke`: passed after rebuilding `zr_vm_language_server_shared` and `zr_vm_language_server_stdio`.
  - `tests/language_server/stdio_smoke.js` expression-statement inline-value RED first failed on `textDocument/inlineValue must expose semantic numeric facts for expression statements`; GREEN passed on WSL clang, WSL gcc, and Windows MSVC after `stdio_inline_value.c` scanned line-start expression statements and reused the shared local semantic query.
  - `tests/language_server/stdio_inline_value_semantic_smoke.js` identifier expression-statement RED first failed with no `range 5..5` inline text for `seed + 3;`; GREEN passed on WSL clang, WSL gcc, and Windows MSVC after expression statements inferred their root expression while local bindings were still in scope and stdio accepted identifier-starting expression statements.
  - `tests/language_server/stdio_inline_value_semantic_smoke.js` unary expression-statement RED first failed with `textDocument/inlineValue must expose logical facts for unary expression statements; values=[]` for `!true;`; GREEN passed on WSL gcc, WSL clang, and Windows MSVC after stdio accepted `!` and `-` as conservative line-start expression starts while still querying shared semantic facts.
  - `tests/language_server/stdio_inline_value_semantic_smoke.js` call/member expression-statement RED first failed with `textDocument/inlineValue must expose call payload facts for call expression statements; values=[]` for `pick(42);`; GREEN passed on WSL gcc, WSL clang, and Windows MSVC after stdio formatted existing `SZrSemanticExpressionFact` call/member payloads and queried the call target/member token.
  - `tests/language_server/stdio_inline_value_semantic_smoke.js` aggregate expression-statement RED first failed with `textDocument/inlineValue must expose nested numeric facts for aggregate expression statements; values=[]` for `[1 + 2];`; GREEN passed on WSL gcc, WSL clang, and Windows MSVC after stdio accepted `[` as a conservative line-start expression start while still querying shared nested semantic facts.
  - REPL member-write reference display RED first failed in `tests/cli/repl_type_call_member_smoke.js` with missing `Reference: member write value` after `:type seed.value = 3` still inferred `Type: int`; GREEN passed on WSL gcc, WSL clang, and Windows MSVC after `repl_semantic_facts.c` traversed non-computed member properties and formatted `ZR_SEMANTIC_REFERENCE_MEMBER_WRITE` as `member write`.
  - `zr_vm_reference_fact_emission_test` passed in the same WSL gcc, WSL clang, and Windows MSVC focused matrix, keeping parser member-write fact production covered while the REPL smoke covers CLI consumption.
  - Parser member-access reference RED first failed in `tests/parser/test_reference_fact_emission.c` with `Expected Non-NULL` for `seed.value` at the `value` token; GREEN passed on WSL gcc, WSL clang, and Windows MSVC after type inference emitted `ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS` for non-computed dot-member reads.
  - `zr_vm_language_server_local_semantic_query_test` and `tests/cli/repl_type_call_member_smoke.js` passed in the same focused matrix, proving LSP local reference query can return `MEMBER_ACCESS` and REPL `:type seed.value` displays `Reference: member value`.
  - Reference query priority now prefers write/member-write over access/read on the same token range, keeping `seed.value = 3` classified as `ZR_SEMANTIC_REFERENCE_MEMBER_WRITE` after member-access emission was added.
  - Object literal computed-key semantic fact RED/GREEN: WSL gcc first failed `zr_vm_expression_fact_emission_test` with `Expected Non-NULL` after adding `test_object_literal_computed_key_expression_records_nested_facts`; after adding `SZrKeyValuePair.keyIsComputed` and computed-key inference, focused WSL gcc direct relink passed `zr_vm_expression_fact_emission_test` with 26 tests/0 failures.
  - Object literal computed-key lowering RED/GREEN: the isolated `zr_vm_object_literal_key_lowering_test` first failed with `Expected 1 Was 0` under the old identifier-key lowering condition; after making computed identifier keys compile as expressions, WSL gcc direct parser relink passed 1 test/0 failures.
  - WSL clang normal target rebuild/run passed `zr_vm_expression_fact_emission_test` with 26 tests/0 failures and `zr_vm_object_literal_key_lowering_test` with 1 test/0 failures.
  - Windows MSVC normal target rebuild/run passed `zr_vm_expression_fact_emission_test` with 26 tests/0 failures and `zr_vm_object_literal_key_lowering_test` with 1 test/0 failures.
  - `zr_vm_semantic_facts_test`: 4 tests, 0 failures.
  - `zr_vm_parser_test`: 67 tests, 0 failures.
  - `zr_vm_expression_fact_emission_test`: 8 tests, 0 failures.
  - `zr_vm_language_server_local_semantic_query_test`: 3 tests, 0 failures.
  - `zr_vm_parser_test`: 67 tests, 0 failures.
  - `zr_vm_semantic_facts_test`: 4 tests, 0 failures.
  - `zr_vm_type_inference_test`: 103 tests, 0 failures.
  - `zr_vm_language_server_semantic_analyzer_test`: `All Semantic Analyzer Tests Completed`.
  - `zr_vm_language_server_ownership_diagnostics_test`: `All Ownership Diagnostics Tests Completed`.
  - `zr_vm_language_server_lsp_interface_test`: `All LSP Interface Tests Completed`.
  - `zr_vm_language_server_local_semantic_query_test`: `All LSP Local Semantic Query Tests Completed`, including numeric, logical, reference, and ownership violation local facts.
  - REPL ownership `:type` smoke passed on WSL gcc, WSL clang, and Windows MSVC after parser type inference began recording ownership builtin facts and REPL read them via `ZrParser_SemanticFacts_FindOwnershipByNode`.
  - WSL gcc focused parser/REPL set for the ownership type-query slice passed: `repl_type_ownership_smoke.js`, `zr_vm_semantic_facts_test` 4 tests/0 failures, `zr_vm_type_inference_test` 103 tests/0 failures, and `zr_vm_expression_fact_emission_test` 19 tests/0 failures.
  - WSL clang focused parser/REPL set for the same slice passed with the same test counts. A separate clang debug REPL probe that only submitted a class declaration still aborts in `ZrCore_Closure_CloseStackValue`, so this smoke intentionally uses a primitive owner and does not claim class declaration teardown coverage.
  - Windows MSVC focused parser/REPL set for the same slice passed with `repl_type_ownership_smoke.js`, `zr_vm_semantic_facts_test` 4 tests/0 failures, `zr_vm_type_inference_test` 103 tests/0 failures, and `zr_vm_expression_fact_emission_test` 19 tests/0 failures.
  - WSL CTest registration for `cli_repl_type_ownership_smoke` passed when run inside WSL. A host-mismatched Windows `ctest --test-dir build\codex-wsl-gcc-debug` attempt was not accepted because it tried to execute `/usr/bin/node` from Windows and reported the test as Not Run.
  - REPL call/member `:type` smoke passed on WSL gcc, WSL clang, and Windows MSVC after the REPL type-query path began printing parser expression payload facts.
  - WSL gcc focused parser/REPL set for the call/member type-query slice passed: `cli_repl_type_call_member_smoke` via CTest, `repl_type_call_member_smoke.js`, `zr_vm_semantic_facts_test` 4 tests/0 failures, `zr_vm_expression_fact_emission_test` 19 tests/0 failures, and `zr_vm_type_inference_test` 103 tests/0 failures.
  - WSL clang focused parser/REPL set for the same slice passed with the same parser test counts and the direct Node smoke. Clang still reports existing GNU label-extension warnings in the active core dispatch table.
  - Windows MSVC focused CLI smoke passed: `build\codex-msvc-debug` rebuilt `zr_vm_cli_executable`, and `node tests\cli\repl_type_call_member_smoke.js build\codex-msvc-debug\bin\Debug\zr_vm_cli.exe` passed. MSVC still reports existing core/runtime warnings during the build.
  - Constant comparison logical fact RED/GREEN: new focused parser/LSP/REPL tests first proved `1 < 2` only had `Type: bool` with no bool constant or logical fact; after extending type-inference bool constant evaluation and logical fact emission, WSL gcc, WSL clang, and Windows MSVC all passed `zr_vm_logical_fact_emission_test`, `zr_vm_expression_fact_emission_test`, `zr_vm_language_server_logical_semantic_query_test`, `zr_vm_language_server_local_semantic_query_test`, and direct `repl_type_logical_comparison_smoke.js` / `repl_type_reachability_smoke.js`.
  - Function-call reference fact RED/GREEN: the new `zr_vm_reference_fact_emission_test` first failed because the call fact's declaration range fell back to the call-site range for a function declaration starting at offset 0; GREEN passed after function registrations preserved declaration ranges/symbol IDs and call inference emitted `ZR_SEMANTIC_REFERENCE_CALL`.
  - WSL gcc focused function-call reference set passed: `cmake --build build/codex-wsl-gcc-debug --target zr_vm_reference_fact_emission_test zr_vm_expression_fact_emission_test -j 8`, `zr_vm_reference_fact_emission_test` 1 test/0 failures, and `zr_vm_expression_fact_emission_test` 27 tests/0 failures.
  - WSL clang focused function-call reference set passed: `cmake --build build/codex-wsl-clang-debug --target zr_vm_reference_fact_emission_test -j 8` and `zr_vm_reference_fact_emission_test` 1 test/0 failures. The build still reports the existing const-qualifier warning in `type_inference.c`.
  - Windows MSVC focused function-call reference set passed: `cmake --build build\codex-msvc-debug --config Debug --target zr_vm_reference_fact_emission_test --parallel 8` and `zr_vm_reference_fact_emission_test.exe` 1 test/0 failures. Existing core/parser warnings remain unrelated.
  - WSL gcc LSP call/member hover RED first failed because `ExpressionAt` returned a call payload fact for `pick(42)` but hover/rich-hover only showed type/reference lines. GREEN passed after `lsp_local_semantic_query.c` appended `Call:` / `Member:` payload lines and `lsp_interface.c` mapped them to `call` / `member` roles.
  - WSL gcc, WSL clang, and Windows MSVC focused LSP call/member hover set passed: `zr_vm_language_server_local_semantic_hover_test` includes `LSP Local Hover Surfaces Call/Member Payloads`, and `zr_vm_language_server_call_member_semantic_query_test` still passes its call target, incomplete-edit preservation, signature-help preservation, and member payload cases.
  - WSL gcc reachability hover focused set: `zr_vm_language_server_local_semantic_hover_test` passed with `LSP Local Hover Surfaces Reachability Cause`; `zr_vm_language_server_local_semantic_query_test` and `zr_vm_language_server_lsp_interface_test` also completed successfully.
  - WSL gcc constant-false loop hover RED/GREEN: `zr_vm_language_server_local_semantic_hover_test` first failed because `deadLoop` had no reachability/logical facts, then passed after `semantic_analyzer_reachability.c` recorded `ZR_SEMANTIC_REACHABILITY_CONDITION_FALSE` and the condition logical fact.
  - WSL clang reachability hover focused set: `zr_vm_language_server_local_semantic_hover_test` and `zr_vm_language_server_local_semantic_query_test` completed successfully.
  - WSL gcc variable range focused set: `zr_vm_expression_fact_emission_test` 11 tests/0 failures, `zr_vm_type_inference_test` 103 tests/0 failures, and `zr_vm_language_server_local_semantic_query_test` completed successfully.
  - WSL clang variable range focused set: same target set passed; expression facts reported 11 tests/0 failures and type inference reported 103 tests/0 failures.
  - WSL gcc completion fact focused set: `zr_vm_language_server_inlay_semantic_facts_test` passed with three cases, including numeric and logical completion detail facts; `zr_vm_language_server_local_semantic_query_test`, `zr_vm_language_server_local_semantic_hover_test`, `zr_vm_language_server_semantic_analyzer_test`, and `zr_vm_language_server_lsp_interface_test` also passed.
  - WSL clang completion fact focused set: the same five LSP focused targets passed.
  - Windows MSVC completion fact smoke: `build\codex-msvc-lsp-smoke` rebuilt `zr_vm_language_server_shared` and `zr_vm_language_server_stdio`; build output shows `lsp_completion_semantic_facts.c` compiled. `build\codex-msvc-cli-debug` rebuilt `zr_vm_cli_executable`, and `hello_world.zrp` printed `hello world`.
  - `zr_vm_cli_repl_e2e_test`: passed.
  - `zr_vm_cli_repl_e2e_test`: passed after adding `:type 1 + 2` coverage for `Type: int`, `Numeric range: 3..3`, and no expression execution result.
  - `language_server_stdio_smoke`: passed and now asserts `missing_expression_after_assignment` reaches stdio JSON with suggestion text.
  - WSL gcc configure/build and `hello_world` smoke passed in the matrix.
  - WSL clang configure/build and `hello_world` smoke passed in the matrix.
  - MSVC configure/build and Windows `hello_world` smoke passed in the matrix.
  - `debug_expression_diagnostics`: 6 tests, 0 failures after adding numeric operand and division-by-zero diagnostic coverage.
  - `debug_agent_protocol`: passed.
  - `debug_agent`: passed.
  - WSL clang `zr_vm_debug_expression_diagnostics_test`: 6 tests, 0 failures.
  - `debug_expression_diagnostics`: 8 tests, 0 failures after adding missing-member-name and missing-index-close diagnostic coverage.
  - Manual REPL smoke for `1 + 2`, `return 1 + 2;`, `1 +`, and `true &&` matched the expected value output or concrete missing-right-operand diagnostic.

- Failed:
  - WSL gcc `ctest`: `core_runtime` failed in `zr_vm_execution_member_access_fast_paths_test`.
  - WSL clang `ctest`: same `core_runtime` failure.
  - Extra support-first smoke `zr_vm_value_type_runtime_test` currently aborts in `ZrCore_Value_TryCopyFastNoProfile` ownership normalization assertion. This is in the active inline-struct/runtime slice, outside the local semantic hover change, and is not accepted as green here.

- Fixes made in response:
  - Added stdio diagnostic JSON assertion for `var x = ;`.
  - Added parser/LSP/REPL regression coverage for missing right operands after binary/logical operators.
  - Added dedicated `loan_escape` ownership diagnostic regression coverage in a focused language-server test target.
  - Added `missing_right_operand` structured parser diagnostic and routed it through terminal diagnostics and LSP diagnostics.
  - Added `ZrParser_Source_Compile` parser-error gating so recovered ASTs with reported errors cannot reach runtime execution.
  - Added REPL bare-expression wrapping for expression-like submissions while preserving statement/declaration and incomplete-expression diagnostics.
  - Centralized successful `ZrParser_ExpressionType_Infer` expression fact emission through the shared helper, expanding coverage beyond literal/binary expressions while keeping numeric facts limited to explicit numeric literal/promotion/conversion paths.
  - Added integer numeric fact enrichment for exact literal ranges, exact constant binary ranges, and safe overflow marking on integer constant expressions.
  - Added parser literal token-range preservation so literal AST locations and semantic fact ranges stay aligned with the consumed token.
  - Added LSP local semantic query numeric fact exposure through `SZrLspLocalSemanticQueryResult.numericFact`.
  - Added parser composite expression range merging for binary/logical/conditional/assignment expressions so operator-position local queries hit the structural expression.
  - Added logical expression bool inference and expression fact emission for `ZR_AST_LOGICAL_EXPRESSION`.
  - Added semantic logical fact position query through `ZrParser_SemanticFacts_FindLogicalAtPosition`.
  - Added LSP local semantic query logical fact exposure through `SZrLspLocalSemanticQueryResult.logicalFact`.
  - Added focused LSP local semantic query regression coverage for exact operator-position numeric range facts and operator-position overflow facts.
  - Added focused LSP local semantic query regression coverage for operator-position logical short-circuit facts and matching reachability facts.
  - Added unary logical-not fact emission for boolean-constant operands and focused LSP local query coverage at the `!` operator token.
  - Added constant numeric-comparison bool folding, expression constant payloads, exact logical facts, focused LSP operator-query coverage, and `:type 1 < 2` REPL smoke coverage.
  - Adjusted ownership fact spans for ownership builtin diagnostics so `%loan(...)` local queries hit the semantic fact at the user-visible `%` prefix, while diagnostics can keep their broader display range.
  - Added exact integer range metadata to literal inference results, preserved it through variable bindings, and folded exact integer binary ranges so local queries can report propagated constant ranges.
  - Added Debug safe-evaluate numeric semantic diagnostic refinement for non-numeric operands, unary non-numeric operands, division by zero, and modulo operand failures.
  - Added Debug safe-evaluate member/index syntax diagnostic refinement for missing member names after `.` and missing closing `]` after index expressions.
  - Added LSP local hover/rich-hover reachability-cause rendering so unreachable facts retain concrete causes such as `after return`.
  - Added `lsp_completion_semantic_facts.c` so completion item metadata can reuse initializer numeric/logical facts without extending the oversized `lsp_interface_support.c` with another fact-formatting responsibility.
  - Added completion detail RED/GREEN regressions for numeric initializer range facts and deterministic logical short-circuit facts in `zr_vm_language_server_inlay_semantic_facts_test`.
  - Added forward declarations for inline-frame member slot helpers to remove a C implicit-declaration build blocker encountered while validating the LSP targets; this was a compile-order fix only.
  - Added expression-statement root-expression inference in the language-server typecheck pass, so `seed + 3;` records the same exact numeric fact as `return seed + 3;` while local variable bindings remain available.
  - Added a focused stdio inline-value smoke for identifier-led expression statements and registered it in the active top-level CTest list.
  - Added parser ownership builtin fact emission for successful `%borrow/%loan/%shared/%weak/%release/%detach` construct inference.
  - Added `ZrParser_SemanticFacts_FindOwnershipByNode` so non-LSP consumers can query ownership facts by the exact AST expression node.
  - Extracted REPL fact formatting into `repl_semantic_facts.c` and added ownership fact display for `:type`.
  - Added `tests/cli/repl_type_ownership_smoke.js` and CTest registration for `cli_repl_type_ownership_smoke`.
  - Added call/member expression fact display in `repl_semantic_facts.c` for `Call: name args=N` and `Member: name`.
  - Added `tests/cli/repl_type_call_member_smoke.js` and CTest registration for `cli_repl_type_call_member_smoke`.
  - Added object literal property-value inference traversal so nested value expressions materialize shared expression and numeric facts while the object literal itself remains best-effort `object`.
  - Added `SZrKeyValuePair.keyIsComputed`; parser marks bracketed object keys, object literal inference visits computed keys and values, and compiler/static/compile-time consumers use the marker so `[identifier]` keys are expressions while plain identifier keys remain property names.
  - Added `tests/parser/test_object_literal_key_lowering.c` plus `zr_vm_object_literal_key_lowering_test` for focused computed-key lowering coverage.
  - Added array literal element expression fact coverage proving the existing array inference path already materializes nested element facts.
  - Added focused remaining-kind expression fact coverage for registered identifiers, assignments, lambdas, and ownership builtin expressions; all passed without production changes.
  - Updated parser/semantic docs and plan/session records to include Task 8 validation and residual matrix failure.

## Acceptance Decision

Accepted for the first-stage parser/LSP semantic fact slice:

- Shared fact storage and query coverage for the first-stage behaviors is verified by focused parser, type inference, semantic analyzer, LSP interface, and stdio tests.
- Structured parser diagnostic text reaches stdio JSON and therefore the VSCode plugin transport path.
- Full repository green is not claimed. The remaining matrix failure is a runtime member-access fast-path failure outside this slice and must be handled separately before claiming whole-repository acceptance.

Accepted for the second-stage Debug expression diagnostic slice:

- Missing right operands after binary/logical operators are no longer reported as generic `expected expression in debug evaluate`.
- `ZrDebug_Evaluate` returns concrete problem text, cause, and suggestion for malformed evaluate expressions.
- Numeric semantic failures such as `"text" + 1` and `1 / 0` now return concrete cause and suggestion text instead of short opaque evaluator errors.
- Member/index syntax failures such as `true.` and `true[0` now return concrete cause and suggestion text instead of short opaque evaluator errors.
- Unterminated string literals such as `"open` now preserve the specific string diagnostic with cause and suggestion instead of being overwritten by generic `expected expression in debug evaluate`.
- The internal expression evaluator used by conditional breakpoints shares the same diagnostic refinement path.
- Existing Debug agent and Debug protocol regression tests pass after the change.

Accepted for the Debug unterminated-string diagnostic slice:

- `ZrDebug_Evaluate` reports `Unterminated string literal in debug evaluate` with `Cause:` and `Suggestion:` for `"open`.
- The string parse error is no longer overwritten by the primary-expression fallback `expected expression in debug evaluate`.
- Focused WSL gcc, WSL clang, and Windows MSVC `zr_vm_debug_expression_diagnostics_test` validation passed with 12 tests and 0 failures.
- This slice does not claim full Debug expression-language parity or parser semantic fact reuse inside the debugger; it only improves safe-evaluate diagnostic precision.

Accepted for the Debug unsupported-string-escape diagnostic slice:

- `ZrDebug_Evaluate` reports `Unsupported string escape '\q' in debug evaluate` with `Cause:` and `Suggestion:` for `"bad\q"`.
- The diagnostic names the offending escape and suggests supported escapes instead of returning the old short `unsupported string escape in debug evaluate` text.
- Focused WSL gcc, WSL clang, and Windows MSVC `zr_vm_debug_expression_diagnostics_test` validation passed with 13 tests and 0 failures.
- This slice does not claim full Debug expression-language parity or parser semantic fact reuse inside the debugger; it only improves safe-evaluate diagnostic precision.

Accepted for the third-stage REPL/parser missing-right-operand slice:

- Bare REPL expressions now execute through the existing compile/runtime path and print values for simple expression submissions.
- Incomplete operator expressions now produce concrete parser diagnostics with cause and suggestion in parser, LSP, Debug, and REPL-facing paths.
- Source compile refuses parser-reported expression errors before runtime execution, improving robustness for malformed local snippets.

Accepted for the ownership `loan_escape` diagnostic coverage slice:

- `loan_escape` is now covered by a dedicated focused test instead of being only builder/emitter wiring.
- The regression verifies stable code, concrete message, cause, suggestion, and ownership fact at the `%loan(...)` expression.

Accepted for the expression fact emission expansion slice:

- Function-call/primary expression inference now records an exact expression fact after successful type inference, closing the gap where only literal and binary expression paths wrote facts.
- Registered identifier, assignment, lambda, and ownership builtin expression facts now have direct focused parser coverage.
- Array literal inference is covered for both the array expression fact and nested element expression/numeric facts.
- Object literal inference now walks key/value pair value expressions, so nested property values such as `{a: 1 + 2}` materialize their own expression and numeric facts.
- Object literal computed keys now have a parser-owned `keyIsComputed` marker; `{[1 + 2]: 4}` materializes nested key facts, while `{[key]: 1, name: 2}` keeps indexed assignment and member assignment distinct during lowering.
- Numeric facts remain deliberately narrower and are not synthesized for function calls without range/source metadata.
- Existing type-inference, semantic-fact, and LSP expression-fact focused regressions still pass after centralizing expression fact emission.

Accepted for the parser/type-inference function-call reference fact slice:

- Function registrations with source declaration nodes now preserve declaration-name ranges, semantic type ids, and semantic symbol ids.
- Resolved primary function calls now emit `ZR_SEMANTIC_REFERENCE_CALL` facts at the callee token and point back to the function declaration token.
- The regression covers declaration names at file offset 0, preventing first top-level functions from losing declaration ranges.
- The slice does not claim REPL reference display is green in the current dirty checkout; the direct REPL smoke is blocked by an unrelated core runtime assertion when executing the prior declaration setup.

Accepted for the integer numeric range/overflow fact slice:

- Integer literal facts and representable integer constant `+`, `-`, `*`, `/`, `%` expressions now carry exact range metadata.
- Integer constant expressions that cannot be safely folded because of signed overflow keep `hasRange=false` and set `mayOverflow=true`.
- The slice does not claim complete numeric analysis for floats, unsigned integers, variables, conversions, division-by-zero diagnostics, or interprocedural range propagation.

Accepted for the LSP local numeric fact query slice:

- `ExpressionAt` now returns numeric facts next to expression facts, so VSCode-side localized inference can consume exact range and overflow metadata without repeating parser/type-inference logic.
- Operator-position queries for binary integer expressions resolve to the structural binary expression fact, not the right-hand literal.
- Literal AST token ranges are verified to remain stable, reducing false local-query hits caused by parser cursor drift.
- The slice does not claim full UI rendering, full LSP protocol extension design, or complete numeric analysis beyond the currently emitted integer literal/constant-expression facts.

Accepted for the LSP local logical short-circuit query slice:

- `ExpressionAt` now returns logical facts next to expression, numeric, reference, reachability, and ownership facts.
- Operator-position queries for `true || false` resolve to the full logical expression and return an exact `bool` expression fact.
- The same `||` query returns `ZR_SEMANTIC_LOGICAL_FACT_SHORT_CIRCUIT` and `ZR_SEMANTIC_REACHABILITY_SHORT_CIRCUIT`, giving VSCode-side localized inference a concrete reason for skipped evaluation.
- Operator-position queries for `!true` resolve to the unary expression, not the operand literal, and return an exact `bool` expression fact with `ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_FALSE`.
- The slice does not claim complete boolean algebra, branch pruning beyond current short-circuit/constant-condition facts, or final UI rendering.

Accepted for the LSP local ownership violation query slice:

- `ExpressionAt` now returns an ownership violation fact when the query position is inside the relevant `%loan(...)` return expression.
- Ownership diagnostic display range remains broad enough for the user-facing diagnostic, while the stored fact range is precise enough for local cursor queries.
- The slice does not claim complete ownership-region reasoning, move tracking, or all borrow/loan UI surfaces.

Accepted for the exact variable numeric range propagation slice:

- Integer and char literal inference results now carry exact single-value range metadata.
- Type-environment variable bindings preserve that metadata, so identifier inference can participate in later numeric fact emission.
- Exact integer binary arithmetic with exact operand ranges now records exact result ranges for the supported safe arithmetic operators.
- LSP local query consumes the same numeric fact for `seed + 3`, proving the behavior reaches localized editor inference.
- The slice does not claim unsigned overflow modeling, non-exact interval joins, loop/branch-sensitive range refinement, or interprocedural propagation.

Accepted for the REPL semantic type-query slice:

- `:type <expression>` now consumes parser/type-inference results and the same `SZrSemanticContext` facts used by parser/LSP work.
- `:type 1 + 2` reports `Type: int` and exact `Numeric range: 3..3` without executing the expression.
- `:type %borrow(owner)` reports the parser-inferred borrowed type (`Type: %borrowed int` in the smoke fixture) plus `Ownership: borrow %borrowed` from a shared ownership fact, not from CLI-only string inspection.
- `:type pick(42)` displays `Call: pick args=1` from `SZrSemanticExpressionFact`.
- `:type seed.value` displays `Member: value` from the same expression fact family.
- `:type seed.value = 3` now displays `Reference: member write value` by traversing the queried AST and reading `ZR_SEMANTIC_REFERENCE_MEMBER_WRITE` from the same semantic context; because the fact is unresolved, no `Declared at:` line is printed.
- The slice is intentionally fresh-expression only; it does not claim persistent REPL runtime cells, full LSP local-query parity, or complete Debug/REPL data inference.

Accepted for the REPL short-circuit reachability type-query slice:

- Parser/type inference records a `ZR_SEMANTIC_REACHABILITY_SHORT_CIRCUIT` fact on the skipped right-hand operand of deterministic logical short-circuit expressions.
- `:type true || false` now reports `Type: bool`, `Logical flow: short-circuits right operand`, and `Reachability: unreachable because short-circuit skips evaluation`.
- The REPL does not run a CLI-only analyzer for this path. It traverses the queried expression and reads reachability facts from the same `SZrSemanticContext` used by parser/LSP work.
- Focused WSL gcc parser and registered REPL smokes passed after the change. Focused WSL clang parser and type smokes passed; the broader clang `cli_repl_e2e` check still fails in an unrelated active core closure assertion. Focused Windows MSVC parser and direct type smokes passed with existing runtime const-qualifier warnings.

Accepted for the parser/LSP/REPL constant comparison logical fact slice:

- Parser/type inference records exact bool constants and `ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_TRUE/ALWAYS_FALSE` for numeric constant comparison expressions.
- `1 < 2` now carries `hasConstant=true`, `constantValue=true`, and an exact `ALWAYS_TRUE` logical fact on the comparison expression range; `3 <= 2` records the false counterpart.
- LSP local query at the comparison operator returns the shared expression and logical facts instead of requiring the language server to duplicate comparison evaluation.
- REPL `:type 1 < 2` reports `Type: bool` and `Logical value: true` without executing the expression.
- Focused WSL gcc, WSL clang, and Windows MSVC parser/LSP/REPL tests passed after the change. The first GCC/clang rebuilds each encountered a transient dirty build-tree shared-library state during relink; rerunning the same focused target build completed and the tests passed.

Accepted for the parser/LSP/REPL composed boolean logical fact slice:

- Parser/type inference now propagates known comparison bool values through logical-not and non-short-circuit `&&` / `||` expressions.
- `!(1 < 2)` records a unary bool expression fact with `hasConstant=true`, `constantValue=false`, and an exact `ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_FALSE`.
- `(1 < 2) && (3 < 4)` records a full logical-expression bool fact with `hasConstant=true`, `constantValue=true`, and an exact `ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_TRUE` over the full expression range.
- Existing short-circuit facts keep priority: `true || false` still reports the short-circuit logical fact and skipped-RHS reachability fact through the existing parser/LSP query tests.
- LSP local query at the `&&` operator and REPL `:type` for the same expression both consume the shared parser facts; neither path duplicates composed boolean inference.
- The constant AST evaluator was extracted to `type_inference_constant_eval.c/.h`, keeping `type_inference_semantic_facts.c` as the fact-emission boundary and reducing it to 867 lines.
- WSL gcc, WSL clang, and Windows MSVC focused parser/LSP/REPL tests and adjacent expression/local-query regressions passed after the change, including the isolated post-modularization rerun. Existing warnings remain in unrelated dirty core/runtime/parser/LSP/library/CLI areas. Full repository green is still not claimed because unrelated active core/value-type runtime work remains in the dirty checkout.

Accepted for the LSP local reachability hover cause slice:

- Local semantic hover now preserves the concrete reachability reason, not only `unreachable`.
- Rich hover maps the same `Reachability: unreachable after return` line into the stable `reachability` role for VSCode-side structured panels.
- Constant-false loop bodies now reuse the same fact/query path: `semantic_analyzer_reachability.c` records a false-condition logical fact and `ZR_SEMANTIC_REACHABILITY_CONDITION_FALSE` for the loop body, and local hover/rich hover displays the concrete reason.
- Focused GCC and clang LSP local hover/query checks pass after the change.
- This slice does not claim complete control-flow visualization, code actions, or resolution of the separate inline-struct runtime assertion.

Accepted for the LSP completion semantic fact detail slice:

- Local variable completion detail now reuses initializer semantic facts from `SZrSemanticContext` instead of recomputing numeric or logical inference in the completion path.
- Numeric initializer facts can surface `range 3..3` in completion detail for `var sum = 1 + 2;`.
- Deterministic logical initializer facts can surface `logical true` and `short-circuits` in completion detail for `var const flag = true || false;`.
- The fact formatting lives in `lsp_completion_semantic_facts.c`; `lsp_interface_support.c` remains the metadata orchestration point and does not take on the new fact consumer responsibility.
- Focused GCC, clang, and MSVC smoke validation passed after the change.
- This slice does not claim completion resolve protocol redesign, signature-help semantic facts, all symbol kinds, or complete VSCode UI rendering.

Accepted for the LSP stdio identifier expression-statement inline-value slice:

- Expression statements now materialize their root expression fact while function-local bindings are still visible to type inference.
- `textDocument/inlineValue` can surface `range 5..5` for `var seed = 2; seed + 3;` on the `seed + 3` expression range.
- The stdio layer only recognizes a conservative single-line statement range and chooses the shared local semantic query position; it does not evaluate runtime debugger values or synthesize numeric facts itself.
- Focused WSL clang, WSL gcc, and Windows MSVC local-query plus stdio smoke validation passed after the change.

Accepted for the LSP stdio multi-line return inline-value slice:

- `textDocument/inlineValue` now surfaces a single fact-backed inline text over multi-line return expressions such as `return 1 +\n 2;`.
- Stdio maps source offsets to LSP multi-line ranges and query positions, then still delegates numeric/logical inference to `ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt`.
- Continuation lines after expression operators are no longer treated as separate line-start expression statements, avoiding duplicate inline values for the right-hand literal.
- Focused WSL gcc, WSL clang, and Windows MSVC stdio inline-value smoke validation passed after the change.

Accepted for the LSP stdio multi-line initializer inline-value slice:

- `textDocument/inlineValue` now keeps semantic initializer text attached to the local variable name when the initializer expression starts on the next line after `=`.
- `var sum =\n 1 + 2;` returns both the existing runtime variable lookup for `sum` and the compile-time semantic text containing `range 3..3` on the same `sum` range.
- The continuation expression no longer emits a duplicate inline value once the semantic text is anchored to the declaration name.
- Stdio only maps the initializer query position and output range; numeric/logical inference remains in the shared local semantic query.
- Focused WSL gcc, WSL clang, and Windows MSVC stdio smoke validation passed after the change.

Accepted for the LSP stdio return-next-line inline-value slice:

- `textDocument/inlineValue` now anchors `return\n 1 + 2;` semantic text to the actual expression range, not to the newline after `return`.
- The scanner emits one `range 3..3` fact-backed inline value for this shape instead of a return-range value plus a duplicate continuation expression value.
- Stdio only skips leading whitespace and chooses the local semantic query position; parser/type inference remains authoritative for the numeric/logical fact.
- Focused WSL gcc, WSL clang, and Windows MSVC stdio smoke validation passed after the change.

Accepted for the LSP stdio unary expression-statement inline-value slice:

- `textDocument/inlineValue` now surfaces shared semantic facts for line-start unary expression statements such as `!true;` and `-42;`.
- `!true;` returns a fact-backed inline text containing `logical false` on the unary expression range; `-42;` returns `range -42..-42` on the unary numeric expression range.
- Stdio only recognizes `!` and `-` as conservative expression-statement starts and chooses the shared local semantic query position; it does not duplicate unary logical or numeric inference.
- Focused WSL gcc and WSL clang registered stdio smokes passed; Windows MSVC `zr_vm_language_server_stdio` build and dedicated inline-value node smoke passed. The MSVC lsp-smoke tree did not register matching CTest entries.

Accepted for the LSP stdio call/member expression-statement inline-value slice:

- `textDocument/inlineValue` now surfaces shared expression payload facts for simple line-start call/member expression statements.
- `pick(42);` returns a fact-backed inline text containing `call pick args=1` on the call expression range; `seed.value;` returns `member value` on the member expression range.
- Stdio only locates the statement range and chooses the call target/member query token; parser/type inference/local query remain authoritative for the payload.
- Focused WSL gcc, WSL clang, and Windows MSVC stdio inline-value smoke validation passed after the change.

Accepted for the LSP stdio continuation-range expression-statement inline-value slice:

- `textDocument/inlineValue` now handles a request range that starts on the continuation line of a simple operator-split expression statement.
- `1 +\n 2;` returns a fact-backed inline text containing `range 3..3` over the full multi-line expression range when the request only includes the second line.
- Stdio only recovers the owner expression-statement range and chooses the shared local semantic query position; parser/type inference/local query remain authoritative for the fact.
- Focused WSL gcc, WSL clang, and Windows MSVC stdio inline-value smoke validation passed after the change.

Accepted for the parser/type-inference assignment write-reference fact slice:

- Registered identifier assignment targets now emit `ZR_SEMANTIC_REFERENCE_WRITE` instead of being misclassified as ordinary read references.
- The assignment-specific path preserves the left identifier expression fact and numeric range fact emission, so existing expression/numeric consumers still see the left-hand token.
- The write fact carries the declaration range, symbol id, and type id from the registered binding, giving LSP/REPL/Debug a token-stable write relationship without rescanning the AST.
- Focused WSL gcc, WSL clang, and Windows MSVC reference/expression fact tests passed after the change. The first focused Windows RED run failed with `Expected 3 Was 2`, proving the regression test caught the old read classification.
- This earlier slice did not claim property/index write reference facts at the time; the later member-write slice below covers assignment-target classification but still does not claim member declaration resolution, complete LSP UI rendering, or broader Debug/REPL reference-display parity.

Accepted for the LSP local assignment write-reference hover coverage slice:

- `tests/language_server/test_lsp_local_semantic_hover.c` now validates `ExpressionAt`, plain hover, and rich hover for the assignment target in `var seed = 1; seed = 3;`.
- The focused hover test confirms `Reference: write`, `Symbol: seed`, and `Declared at: 1:5` in plain hover, plus stable `reference`, `symbol`, and `declaration` rich-hover roles.
- WSL gcc, WSL clang, and Windows MSVC focused local semantic hover/query tests passed. The existing query test `LSP Local Reference Query Returns Write Fact` continued to pass in the same matrix.
- This slice was a coverage/documentation slice over the current LSP semantic analyzer write-reference path; the later member-write slice below covers property/index assignment-target classification but still does not claim full VSCode UI rendering or Debug/REPL reference-display parity.

Accepted for the parser/LSP/REPL member write-reference fact slice:

- `EZrSemanticReferenceKind` now has `ZR_SEMANTIC_REFERENCE_MEMBER_WRITE`, and local query/hover formatting maps it to `member write`.
- `ZrParser_AssignmentType_Infer` records `ZR_SEMANTIC_REFERENCE_MEMBER_WRITE` for non-identifier assignment targets after left-side inference succeeds. The fact range is the final member/index target token, `isResolved=false`, and symbol/type ids remain invalid.
- Parser coverage validates both property and computed-index targets: `seed.value = 3;` records `value`, and `seed[index] = 4;` records the computed index expression range.
- LSP local query validates `seed.value = 3;` at the `value` token, and hover/rich hover validates `Reference: member write` plus `Symbol: value`.
- REPL `:type seed.value = 3` validates the same fact through `tests/cli/repl_type_call_member_smoke.js`, printing `Reference: member write value` without a synthetic declaration location.
- WSL gcc, WSL clang, and Windows MSVC isolated focused builds passed `zr_vm_reference_fact_emission_test`, `zr_vm_language_server_local_semantic_query_test`, `zr_vm_language_server_local_semantic_hover_test`, and the direct REPL smoke across the relevant slices.
- This earlier member-write slice did not claim member-read facts, member declaration resolution, or full Debug reference-display parity.

Accepted for the parser/LSP/REPL member access-reference fact slice:

- `type_inference_record_member_access_reference_fact` now records unresolved `ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS` for successful non-computed dot-member reads such as `seed.value`.
- Parser coverage validates the `value` token range, unresolved state, invalid symbol/type ids, and member name payload in `zr_vm_reference_fact_emission_test`.
- `ZrParser_SemanticFacts_FindReferenceAtPosition` now chooses the best matching reference fact by range width and reference priority, so write/member-write facts win over read/member-access facts on the same token range.
- LSP local reference query validates `seed.value` at `value` returns `ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS`, while the existing member-write query for `seed.value = 3` still returns `ZR_SEMANTIC_REFERENCE_MEMBER_WRITE`.
- REPL `:type seed.value` validates the same fact through `tests/cli/repl_type_call_member_smoke.js`, printing `Reference: member value` alongside `Member: value`.
- WSL gcc, WSL clang, and Windows MSVC focused parser/LSP/REPL tests passed after the change. Existing const-qualifier warnings remain in the current dirty checkout baseline.
- This slice did not claim computed member-read facts at the time; the later computed member-access slice below covers unresolved computed member read classification while still not claiming member declaration resolution, hover/rich-hover coverage for member-access facts, or full Debug reference-display parity.

Accepted for the parser/LSP/REPL computed member access-reference fact slice:

- Computed member reads now emit unresolved `ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS` without hiding the computed index expression's own facts.
- `seed[index];` records a wide member-access fact on the member expression range, and `type_inference_record_member_access_reference_fact` materializes the index expression before appending the member fact.
- Reference lookup keeps the index-variable contract explicit: querying the `index` token returns resolved `ZR_SEMANTIC_REFERENCE_READ`, while querying `[` returns unresolved `ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS`.
- REPL `:type seed[index]` validates the same shared facts through `tests/cli/repl_type_call_member_smoke.js`, printing `Member: index` and `Reference: member index`.
- RED WSL gcc focused parser coverage failed as expected after adding `test_computed_member_access_records_member_reference_without_hiding_index_read`: the bracket query returned `ZR_SEMANTIC_REFERENCE_READ` instead of `ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS`.
- GREEN WSL gcc, WSL clang, and Windows MSVC focused validation passed `zr_vm_reference_fact_emission_test`, `zr_vm_language_server_local_semantic_query_test`, and the direct `tests/cli/repl_type_call_member_smoke.js` path.
- This slice still does not claim member declaration resolution, hover/rich-hover coverage for computed member-access facts, full Debug reference display, or whole-repository green. The later LSP hover slice below covers bracket-position computed member-access hover/rich-hover.

Accepted for the LSP computed member access-reference hover slice:

- `tests/language_server/test_lsp_computed_member_hover.c` validates `seed[index]` at the `[` position through `ExpressionAt`, plain hover, and rich hover.
- Local semantic query now bridges wide computed member-access reference facts back to member expression payloads, so the bracket query exposes both the reference classification and `Member: index`.
- `GetHover` lets local fact hover win at non-identifier positions after signature help, preserving normal identifier hover while allowing punctuation positions with semantic facts to render.
- Plain hover includes `Reference: member access`, `Symbol: index`, and `Member: index`; rich hover exposes stable `reference`, `symbol`, and `member` roles for VSCode-side structured panels.
- RED WSL gcc focused target first failed with `hasMember=0` and `hover=(nil)`, proving the old path found the reference but lost the member payload and hover fallback.
- GREEN WSL gcc, WSL clang, and Windows MSVC focused validation passed `zr_vm_language_server_computed_member_hover_test`, `zr_vm_language_server_local_semantic_query_test`, `zr_vm_language_server_local_semantic_hover_test`, and `zr_vm_reference_fact_emission_test`.
- This slice still does not claim member declaration resolution, full Debug reference display, or whole-repository green.

Accepted for the LSP stdio computed member inline-value reference slice:

- `tests/language_server/stdio_inline_value_semantic_smoke.js` now validates `seed[index];` through `textDocument/inlineValue`.
- Stdio inline values continue to query `ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt`; they now also format the returned `SZrSemanticReferenceFact` as compact `reference ...` text.
- Computed member expression statements now return fact-backed inline text containing both `member index` and `reference member access` on the expression range.
- RED WSL gcc smoke first failed with `text:"member index"` and no reference segment, proving the missing behavior was in the inline-value consumer rather than parser/type-inference fact emission.
- GREEN WSL gcc, WSL clang, and Windows MSVC dedicated stdio inline-value smokes passed after the change. Existing MSVC core/parser/library warnings remain part of the current dirty checkout baseline.
- This slice still does not claim member declaration resolution, full Debug reference display, runtime debugger value display, or whole-repository green.

Accepted for the LSP stdio aggregate expression-statement inline-value slice:

- `tests/language_server/stdio_inline_value_semantic_smoke.js` now validates `[1 + 2];` and `[true || false];` through `textDocument/inlineValue`.
- `textDocument/inlineValue` now handles simple line-start array-literal aggregate expression statements.
- `[1 + 2];` returns `range 3..3` on the aggregate expression range, and `[true || false];` returns `logical true, short-circuits` on the aggregate expression range.
- RED WSL gcc smoke first failed with `values=[]`, proving the scanner did not accept aggregate expression statement starts.
- GREEN WSL gcc, WSL clang, and Windows MSVC dedicated stdio inline-value smokes and registered CTest passed after the change.
- Stdio only extends the conservative start scanner with `[`: parser/type inference/local query remain the fact source, with no arbitrary aggregate parsing, runtime debugger value-display claim, or whole-repository green claim.

Accepted for the LSP stdio continuation-only initializer inline-value slice:

- `tests/language_server/stdio_inline_value_semantic_smoke.js` now validates a multi-line initializer when `textDocument/inlineValue` is requested only for the continuation expression line.
- Stdio inline values still delegate fact lookup to `ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt`; the new scanner path only recovers the owning `var name =` line and uses the initializer expression as the query position.
- The inline fact remains anchored to the declaration-name range, matching the existing debugger-facing lookup plus compile-time fact contract for initializer inline values.
- RED WSL gcc smoke first failed with `values=[]`, proving the fact was lost because the owner declaration line was outside the request range.
- GREEN WSL gcc, WSL clang, and Windows MSVC dedicated stdio inline-value smokes passed after the change.
- The inline-value semantic text/query bridge was split into `stdio_inline_value_semantic_text.c/.h`; `stdio_inline_value.c` is back under the large-file threshold at 855 lines. This slice still does not claim parser fact changes, complete VSCode adapter UI, full Debug reference display, or whole-repository green.

Accepted for the REPL expression fact kind/exact constant slice:

- `tests/cli/repl_type_expression_fact_smoke.js` validates that `:type` now prints expression fact kind/exactness for literal, binary, and string-literal queries.
- REPL `:type 42` prints `Expression: literal exact` and `Constant: 42`; `:type 1 + 2` prints `Expression: binary exact` and `Constant: 3`; `:type "zr"` prints `Constant: "zr"`.
- `ZrCli_ReplSemanticFacts_WriteExpression` now consumes the shared `SZrSemanticExpressionFact` payload instead of limiting `:type` output to call/member details.
- `type_inference_node_integer_value` now folds safe int64 unary `+`/`-` and binary `+`/`-`/`*` expression constants, while overflow keeps the expression fact non-constant.
- RED WSL gcc first proved the consumer gap by showing only type/numeric output, then the parser producer gap by showing `Expression: binary exact` without `Constant: 3`.
- GREEN WSL gcc, WSL clang, and Windows MSVC focused validation passed `zr_vm_expression_fact_emission_test`, the new direct REPL smoke, adjacent REPL type smokes, and the registered `cli_repl_type_expression_fact_smoke` CTest path where applicable.
- This slice still does not claim division/modulo folding, unsigned expression constants, Debug expression fact consumption, or whole-repository green; escaped string constant display is covered by the follow-up REPL display slice.

Accepted for the REPL escaped string constant display slice:

- `tests/cli/repl_type_expression_fact_smoke.js` now validates `:type "a\"b\\c\n\t"` as a string literal whose decoded payload contains an embedded quote, backslash, newline, and tab.
- REPL `:type` now keeps that constant on one display line as `Constant: "a\"b\\c\n\t"` instead of printing the embedded quote and decoded control characters raw.
- `repl_write_escaped_string_constant` formats `SZrSemanticExpressionFact` string constants by `ZrCore_String_GetByteLength`, escaping quote, backslash, common control characters, and other low control bytes.
- RED WSL gcc first failed because the old output contained raw `Constant: "a"b\c` followed by a decoded newline/tab split.
- GREEN WSL gcc, WSL clang, and Windows MSVC focused validation passed `zr_vm_expression_fact_emission_test`, the direct REPL expression-fact smoke, and adjacent REPL type smokes.
- This slice only changes REPL `:type` string-constant presentation; the later LSP escaped-string hover slice covers LSP string-constant presentation. It does not claim Debug expression fact consumption, persistent REPL scope, or whole-repository green.

Accepted for the LSP expression fact hover slice:

- `tests/language_server/test_lsp_expression_fact_hover.c` validates `1 + 2` through `ExpressionAt`, local hover building, and public `textDocument/hover`.
- Hover over the binary operator now includes `Expression: binary exact` and `Constant: 3`, alongside existing type and numeric range facts.
- The same focused test now validates string literal expression facts whose decoded payload contains quote, backslash, newline, and tab characters.
- Rich hover now exposes the same payload with stable `expression` and `constant` roles so VSCode-side structured panels do not have to parse generic detail text.
- `lsp_local_semantic_expression_text.c/.h` owns expression fact kind/exactness/constant formatting so `lsp_local_semantic_query.c` remains the query/hover orchestration boundary.
- RED WSL gcc first showed the underlying query already had `kind=binary`, `exact`, `hasConst=1`, and `const=3`, while hover displayed only type/numeric range text.
- GREEN WSL gcc and WSL clang focused validation passed `zr_vm_language_server_expression_fact_hover_test`, `zr_vm_language_server_local_semantic_query_test`, `zr_vm_language_server_local_semantic_hover_test`, and `zr_vm_language_server_computed_member_hover_test`.
- RED for the role refinement first showed plain/public hover already had the expression text while rich-hover sections still lacked `expression` / `constant` roles.
- GREEN Windows MSVC focused validation passed `zr_vm_language_server_expression_fact_hover_test`, `zr_vm_language_server_local_semantic_query_test`, `zr_vm_language_server_local_semantic_hover_test`, and `zr_vm_language_server_computed_member_hover_test`.
- This slice still does not claim Debug expression fact consumption or whole-repository green.

Accepted for the LSP escaped string constant hover slice:

- `tests/language_server/test_lsp_expression_fact_hover.c` validates `return "a\"b\\c\n\t";` through `ExpressionAt`, local hover building, public `textDocument/hover`, and rich hover.
- The decoded shared fact payload is still verified by byte length as `a"b\c` plus newline and tab, proving the test covers display formatting rather than changing parser fact emission.
- Local hover, public hover, and rich-hover `constant` role now keep the payload on one escaped display line as `Constant: "a\"b\\c\n\t"` instead of printing raw embedded quotes or decoded control characters.
- `lsp_local_semantic_expression_text.c` now formats string constants through a byte-length escaped writer, covering quote, backslash, common control characters, and other low control bytes.
- RED WSL gcc first failed because the old hover text contained raw `Constant: "a"b\c` followed by a decoded newline/tab split.
- GREEN WSL gcc, WSL clang, and Windows MSVC focused validation passed `zr_vm_language_server_expression_fact_hover_test`, `zr_vm_language_server_local_semantic_query_test`, `zr_vm_language_server_local_semantic_hover_test`, `zr_vm_language_server_computed_member_hover_test`, and `zr_vm_language_server_lsp_interface_test`.
- This slice only changes LSP expression fact string-constant presentation; it does not change parser/type-inference fact emission, REPL display, Debug expression fact consumption, or whole-repository green.

Accepted for the LSP completion initializer expression fact detail slice:

- `tests/language_server/test_lsp_inlay_semantic_facts.c` now validates local-variable completion details for initializer expression facts, including `expression binary exact`, folded integer `constant 3`, and the existing numeric `range 3..3`.
- The same focused test validates escaped string constants in completion details: `var label = "a\"b\\c\n\t";` displays one-line `constant "a\"b\\c\n\t"` and rejects raw embedded-quote output such as `constant "a"b`.
- `lsp_completion_semantic_facts.c` now materializes initializer facts through the shared semantic analyzer when needed, reads `SZrSemanticExpressionFact` from `SZrSemanticContext`, and appends expression kind/exactness/constants before existing numeric/logical/ownership detail text.
- RED WSL gcc first failed because `sum` completion detail contained `Semantic facts: range 3..3, unsigned 3..3` without expression kind or constant payload; the escaped-string RED then failed because `label` completion detail contained `expression literal exact` without the escaped string constant.
- The inlay hint numeric fact assertion was corrected after debugger evidence showed both signed and unsigned numeric facts were present; the stale exact expected label ignored the legitimate `unsigned 3..3` suffix. No inlay production code changed.
- GREEN WSL gcc, WSL clang, and Windows MSVC focused validation passed `zr_vm_language_server_inlay_semantic_facts_test` and `zr_vm_language_server_lsp_interface_test`.
- This slice only claims local variable initializer completion fact detail. It does not claim full completion resolve UI design, signature-help expression fact parity, Debug fact consumption, or whole-repository green.

Accepted for the LSP signature-help argument expression fact docs slice:

- `tests/language_server/test_lsp_inlay_semantic_facts.c` now validates signature-help parameter documentation for argument expression facts, including `expression binary exact`, folded integer `constant 3`, and the existing numeric `range 3..3` for `pick(1 + 2, true || false)`.
- `lsp_signature_semantic_facts.c` now materializes argument facts through the shared semantic analyzer when needed, reads `SZrSemanticExpressionFact` from `SZrSemanticContext`, and appends expression kind/exactness/constants before existing numeric/logical/ownership parameter documentation.
- Existing signature-help docs still surface logical short-circuit facts and ownership violations through the same `Argument semantic facts:` text, so the new expression payload is additive rather than replacing those details.
- RED WSL gcc first failed because numeric argument docs contained only `Argument semantic facts: range 3..3, unsigned 3..3`, without `expression binary exact` or `constant 3`.
- GREEN WSL gcc, WSL clang, and Windows MSVC focused validation passed `zr_vm_language_server_inlay_semantic_facts_test` and `zr_vm_language_server_lsp_interface_test`.
- This slice only claims signature-help argument expression fact documentation. It does not claim full completion resolve UI design, Debug fact consumption, or whole-repository green.

Accepted for the REPL prior function call reference slice:

- `tests/cli/repl_type_call_reference_smoke.js` validates a same-session prior function declaration followed by `:type pick(42)`.
- REPL `:type` now replays prior function declarations into its temporary type environment before inferring the queried expression, so `pick(42)` reports `Type: int` instead of unresolved `Type: object`.
- The same query prints the parser/type-inference shared call expression payload (`Call: pick args=1`) and resolved reference payload (`Reference: call pick`, plus `Declared at:`).
- RED WSL gcc first failed because `func pick(value: int): int { ... }` produced `[closure]`, but the follow-up `:type pick(42)` still showed only `Type: object` and `Call: pick args=1`.
- GREEN WSL gcc, WSL clang, and Windows MSVC focused validation passed the new direct REPL smoke, adjacent REPL type smokes, and the registered `cli_repl_type_call_reference_smoke` CTest.
- This slice only claims non-generic prior function signature replay for REPL `:type`; it does not claim complete generic function parity, complete Debug parser fact reuse, or whole-repository green.

Accepted for the Debug conditional-expression safe-evaluate slice:

- `tests/debug/test_debug_expression_diagnostics.c` now validates `?:` in paused-frame safe evaluate / conditional-breakpoint expressions.
- `true ? 1 : missingLocal` returns the selected true branch without resolving `missingLocal`; `false ? missingLocal : 2` does the symmetric false-branch case.
- The skipped branch is still parsed for syntax, preserving the safe-evaluate grammar contract while avoiding data lookup and calculations on unreachable branch paths.
- RED WSL gcc focused target first failed after adding the regression: `Expected TRUE Was FALSE` in `test_debug_condition_evaluates_selected_ternary_branch_without_resolving_skipped_branch`.
- GREEN WSL gcc and WSL clang focused validation passed `zr_vm_debug_expression_diagnostics_test` with 16 tests/0 failures; Windows MSVC focused validation also passed the same 16 tests/0 failures.
- The Debug safe-evaluate helper split moved diagnostic/right-operand support into `debug_eval_diagnostics.c` and `debug_eval_internal.h`, reducing `debug_eval.c` to 845 lines. This slice still does not claim complete language expression parity, shared parser fact consumption inside Debug, or whole-repository green.

Accepted for the Debug conditional branch diagnostics slice:

- `tests/debug/test_debug_expression_diagnostics.c` now validates missing consequent and missing alternate branches in paused-frame safe evaluate / conditional-breakpoint expressions.
- `true ? : 2` reports `Missing consequent expression in conditional expression`; `true ? 1 :` reports `Missing alternate expression in conditional expression`. Both include cause, conditional-breakpoint context, and suggestion text.
- RED WSL gcc focused target first failed after adding the regressions with the old generic diagnostics: `Invalid expression after '?'` for the missing consequent case, and `Missing expression after ':'` for the missing alternate case.
- GREEN implementation adds branch-specific diagnostic helpers in `debug_eval_diagnostics.c` and local non-consuming lookahead checks in `debug_eval.c` before the generic right-operand parser runs.
- GREEN WSL gcc, WSL clang, and Windows MSVC focused validation passed `zr_vm_debug_expression_diagnostics_test` with 18 tests/0 failures. This slice does not claim complete language expression parity, shared parser fact consumption inside Debug, or whole-repository green.

Accepted for the REPL/LSP conditional branch fact consumer slice:

- `tests/cli/repl_type_conditional_branch_smoke.js` validates that `:type true ? 1 : 2` displays the conditional expression fact, selected numeric range, constant-condition logical value, and skipped-branch reachability without executing the expression.
- `tests/language_server/test_lsp_local_semantic_query.c` now characterizes the same conditional branch facts through `ExpressionAt` at the skipped alternate branch position.
- RED WSL gcc first failed because REPL `:type` printed the conditional expression, selected range, and reachability but omitted `Logical value: true` from the nested condition node.
- GREEN implementation keeps fact production unchanged and extends `repl_semantic_facts.c` to traverse the queried expression tree for logical facts, matching the existing reachability/reference traversal pattern.
- GREEN WSL gcc, WSL clang, and Windows MSVC focused validation passed the direct REPL smoke, registered `cli_repl_type_conditional_branch_smoke` CTest, and `zr_vm_language_server_local_semantic_query_test`. This slice does not claim complete REPL/LSP parity, arbitrary expression-language coverage, or whole-repository green.

Accepted for the REPL nested numeric fact display slice:

- `tests/cli/repl_type_nested_numeric_smoke.js` validates that `:type [1 + 2]` displays the array expression fact and the nested element expression's numeric range without executing the aggregate expression.
- RED WSL gcc first failed because REPL `:type` printed `Type: int[1]<int>` and `Expression: array exact`, but no nested `Numeric range: 3..3`.
- GREEN implementation adds `repl_semantic_expression_walk.c/.h` as a small prunable expression AST walker and uses it from `repl_semantic_facts.c` to print expression-wide numeric facts while pruning literal children after a structural numeric fact is emitted.
- The root `:type 1 + 2` and conditional branch numeric displays still pass through the same writer, and adjacent expression, conditional, and reachability smokes stayed green.
- GREEN WSL gcc, WSL clang, and Windows MSVC focused validation passed the direct REPL smoke, registered `cli_repl_type_nested_numeric_smoke` CTest, and adjacent REPL fact smokes. This slice does not claim new parser numeric inference, complete REPL/LSP local-query parity, or whole-repository green.

Accepted for the REPL nested expression fact display slice:

- `tests/cli/repl_type_nested_expression_fact_smoke.js` validates that `:type [1 + 2]` displays the array expression fact plus the nested element expression's `Expression: binary exact` and folded `Constant: 3`.
- The same smoke rejects leaf constants `1` and `2`, so the display exposes structural nested facts without dumping every literal fact.
- RED WSL gcc first failed because REPL `:type` printed the aggregate expression fact and nested numeric range, but no nested expression fact or folded constant.
- GREEN implementation keeps fact production unchanged and extends `repl_semantic_facts.c` with an expression-wide writer that reuses `repl_semantic_expression_walk.c/.h`; array/object facts descend, while binary facts print and prune their children.
- GREEN WSL gcc, WSL clang, and Windows MSVC focused validation passed the direct REPL smoke, registered `cli_repl_type_nested_expression_fact_smoke` CTest, and adjacent REPL fact smokes. This slice does not claim new parser expression fact production, complete REPL/LSP local-query parity, or whole-repository green.

Accepted for the REPL nested ownership fact display slice:

- `tests/cli/repl_type_nested_ownership_smoke.js` validates that `:type [%borrow(owner)]` after `var owner: %unique int;` displays the aggregate borrowed type, nested ownership builtin expression fact, and nested `Ownership: borrow %borrowed`.
- RED WSL gcc first failed because REPL `:type` printed the aggregate borrowed type and nested expression fact, but no nested ownership fact.
- GREEN implementation keeps fact production unchanged and adds expression-wide ownership display through `repl_semantic_fact_walkers.c`.
- The expression-tree numeric and expression visitors also moved from `repl_semantic_facts.c` into `repl_semantic_fact_walkers.c`, keeping the formatter focused on single-fact presentation while all walker-backed fact consumers share one module.
- GREEN WSL gcc, WSL clang, and Windows MSVC focused validation passed the direct REPL smoke, registered `cli_repl_type_nested_ownership_smoke` CTest, and adjacent REPL fact smokes. This slice does not claim new parser ownership inference, complete REPL/LSP local-query parity, or whole-repository green.

Accepted for the REPL aggregate-start expression execution slice:

- `tests/cli/repl_expression_aggregate_smoke.js` validates that ordinary REPL input `[1 + 2][0]` is wrapped and executed as a bare expression, printing `3`.
- The smoke rejects the old parser path where line-start `[` was treated as a statement form that required `;`, rejects accidental `:type` analysis fallback, and rejects `SET_BY_INDEX` runtime failure while constructing the array literal.
- RED WSL gcc first failed with `Expected ';'` / `期望 ';'`, proving the REPL expression wrapper still excluded aggregate-start expressions. After allowing `[`, the same smoke failed with `SET_BY_INDEX: receiver must be an object or array`, proving the lower compiler/runtime array literal path was also broken for element initialization.
- GREEN implementation allows `[` through the REPL bare-expression scanner, compiles array-literal element expressions into temporary slots after the receiver slot, and refreshes logical frame slots for `CREATE_ARRAY`, `CREATE_OBJECT`, and `SET_BY_INDEX` dispatch.
- `tests/parser/test_instruction_execution.c` now asserts `CREATE_ARRAY With Elements Execution` returns an array with `length == 3` and indices `0..2 == 1,2,3`, so the lower array literal write path is no longer only logged.
- GREEN WSL gcc, WSL clang, and Windows MSVC focused validation passed the direct REPL smoke and registered `cli_repl_expression_aggregate_smoke` CTest. WSL gcc/clang focused instruction output also showed `Result: [1, 2, 3]` for `CREATE_ARRAY With Elements Execution`. This slice does not claim array value stringification, AOT/value-type runtime behavior, complete REPL expression-language parity, or whole-repository green.

Accepted for the REPL array result display slice:

- `tests/cli/repl_expression_array_display_smoke.js` validates that ordinary REPL input `[1 + 2]` is wrapped and executed as a bare expression, printing `[3]`.
- RED WSL gcc first failed with an abort at `zr_vm_core/src/zr_vm_core/value.c:358` in `ZrCore_Value_ConvertToString`, proving array construction already reached result display but shared value stringification had no array branch.
- GREEN implementation keeps REPL on the normal execution/output path and adds `ZR_VALUE_TYPE_ARRAY` handling to `ZrCore_Value_ConvertToString` by reusing the existing first-level array debug preview.
- GREEN WSL gcc, WSL clang, and Windows MSVC focused validation passed the direct REPL smoke and registered `cli_repl_expression_array_display_smoke` CTest. This slice does not claim arbitrary nested/cyclic array formatting, AOT/value-type runtime behavior, complete REPL expression-language parity, or whole-repository green.

Accepted for the REPL object-start expression execution slice:

- `tests/cli/repl_expression_object_smoke.js` validates that ordinary REPL input `{a: 1 + 2}.a` is wrapped and executed as a bare expression, printing `3`, and that `{[1 + 2]: 4}[3]` preserves computed-key object literal execution, printing `4`.
- RED WSL gcc first failed with `Expected ';'` / `期望 ';'`, proving the REPL expression wrapper still excluded line-start object literals even though the parenthesized forms already executed.
- GREEN implementation moves REPL input scanning into `repl_input_scan.c/.h` and adds a conservative object-literal start detector: empty objects, identifier/string keys followed by `:`, and computed keys `[...]` followed by `:` enter the internal expression wrapper, while ordinary `{ var ... }` / `{ if ... }` blocks remain on the parser statement path.
- GREEN WSL gcc, WSL clang, and Windows MSVC focused validation passed direct object/aggregate/array REPL smokes and registered CTest `cli_repl_expression_(aggregate|array_display|object)_smoke`. This slice does not claim complete block/object ambiguity resolution, persistent REPL runtime scope, AOT/value-type behavior, or whole-repository green.

Accepted for the LSP stdio object aggregate inline-value slice:

- `tests/language_server/stdio_inline_value_semantic_smoke.js` now validates that a computed-key object aggregate expression statement such as `{[1 + 2]: 4};` exposes the computed-key numeric fact as `range 3..3` on the object expression range.
- The same smoke now validates a multi-line ordinary-key object expression statement `{\n  a: 1 + 2\n};`; stdio recognizes the object-literal-looking start across line breaks, queries the nested `1 + 2` operator through `ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt`, and returns `range 3..3` over the multi-line object expression range.
- The smoke also covers same-line ordinary-key object expression statements such as `{a: 1 + 2};`. A first probe expected the range to include the closing brace and semicolon, but the existing local query correctly returned the object expression range ending at the closing brace, so the assertion was narrowed to the real expression range.
- RED WSL gcc first failed with `textDocument/inlineValue must expose computed-key numeric facts for object expression statements; values=[]`, proving the old stdio scanner did not treat `{` as a supported expression-statement start.
- RED follow-up WSL gcc failed with `textDocument/inlineValue must expose nested value facts for multi-line object expression statements`, returning only the computed-key case, proving object-literal start detection stopped at the first line and never reached the multi-line ordinary key/value shape.
- GREEN implementation keeps expression-statement scanning in `stdio_inline_value_scan.c/.h`. The scanner now owns identifier/keyword helpers, conservative object-literal start detection that can skip newline whitespace to the first key, balanced statement-end scanning that skips object literal braces, and query-offset selection; `stdio_inline_value.c` remains the request consumer and calls the shared local semantic query.
- GREEN WSL gcc, WSL clang, and Windows MSVC focused validation passed direct node execution and registered CTest `language_server_stdio_inline_value_semantic_smoke` after the change. The broader stdio smoke also now asserts the local `x` completion contains `range 20..20` without assuming it is the first semantic detail, because completion detail can legitimately include `expression ...` and `constant ...` before numeric facts. This slice does not claim full parser block/object ambiguity resolution, stdio-owned aggregate inference, complete VSCode UI parity, or whole-repository green.

Accepted for the LSP stdio moniker non-code token filter slice:

- `tests/language_server/stdio_smoke.js` now validates `textDocument/moniker` keeps returning a document-scoped `zr` identity for a code identifier, while identifiers inside `//` comments, `/* ... */` comments, and string literals return empty arrays.
- RED WSL gcc first failed with `textDocument/moniker must ignore identifiers inside comments and strings`, proving the old moniker handler treated comment/string words as source symbols.
- GREEN implementation adds a local scanner in `stdio_moniker.c` that recognizes line comments, block comments, string literals, and escaped string characters before creating the moniker. This is stdio request filtering only; it does not claim cross-document symbol identity or parser-token-stream replacement.
- GREEN WSL gcc, WSL clang, and Windows MSVC focused validation passed `tests/language_server/stdio_smoke.js` against `build/codex-semantic-wsl-gcc-debug`, `build/codex-semantic-wsl-clang-debug`, and `build\codex-semantic-msvc-debug`. This slice does not claim whole-repository green.

Accepted for the LSP stdio linked-editing fallback non-code token filter slice:

- `tests/language_server/stdio_smoke.js` now validates `textDocument/linkedEditingRange` still returns ranges for real code occurrences, while a document with one code occurrence of `linkedOnly` plus `// linkedOnly`, `"linkedOnly"`, and `/* linkedOnly */` returns `null` instead of synthesizing linked ranges from non-code text.
- RED WSL gcc first failed with `linkedEditingRange fallback must ignore identifiers inside comments and strings`, proving the fallback scanner treated comments and strings as editable symbol ranges.
- GREEN implementation adds a local comment/string scanner to `stdio_linked_editing.c` and uses it both for the requested token and for every fallback scan hit. This keeps semantic-reference results preferred and only constrains the document-scanning fallback.
- GREEN WSL gcc, WSL clang, and Windows MSVC focused validation passed `tests/language_server/stdio_smoke.js` against `build/codex-semantic-wsl-gcc-debug`, `build/codex-semantic-wsl-clang-debug`, and `build\codex-semantic-msvc-debug`. This slice does not claim cross-file rename, parser-token-stream replacement, or whole-repository green.

Accepted for the LSP shared semantic query document-highlight non-code token filter slice:

- `tests/language_server/stdio_smoke.js` now validates `textDocument/documentHighlight` keeps returning highlights for real code identifiers, while the same word inside `//` comments, `/* ... */` comments, and string literals returns empty arrays.
- RED WSL gcc first failed with `documentHighlight must ignore identifiers inside comments and strings`, proving the old raw identifier fallback could resolve non-code text as a real local symbol.
- GREEN implementation adds `zr_vm_language_server/src/zr_vm_language_server/interface/lsp_source_spans.c` and uses it from `lsp_semantic_query.c` before raw identifier fallback extraction. `%import("...")` import-literal navigation remains covered by the existing AST/import-chain path.
- GREEN WSL gcc, WSL clang, and Windows MSVC focused validation passed `tests/language_server/stdio_smoke.js` against `build/codex-semantic-wsl-gcc-debug`, `build/codex-semantic-wsl-clang-debug`, and `build\codex-semantic-msvc-debug`; WSL gcc `zr_vm_language_server_lsp_interface_test` also passed to protect import-literal semantic query behavior. This slice does not claim parser-token-stream replacement or whole-repository green.

Accepted for the LSP super-constructor non-code token filter slice:

- `tests/language_server/test_lsp_interface.c` now validates `textDocument/definition`, `textDocument/references`, and `textDocument/documentHighlight` keep resolving real `super(seed)` constructor calls while returning no locations/highlights for `super` text inside constructor comments and string literals.
- RED WSL gcc first failed with `Definition on comment super text returned 1 locations (first=5:8-10:0)`, proving the old constructor fallback treated body comment text as a constructor navigation target.
- GREEN implementation updates `lsp_super_navigation.c` so raw `super` token scanning, live cursor token checks, and constructor declaration fallback all reject non-code spans via the shared lexical span helper. It also updates `lsp_semantic_query.c` to guard the final local-symbol fallback with the same live-buffer code-span check after import/metadata-specific paths have already had their chance.
- GREEN WSL gcc, WSL clang, and Windows MSVC focused validation passed `zr_vm_language_server_lsp_interface_test` in `build/codex-semantic-wsl-gcc-debug`, `build/codex-semantic-wsl-clang-debug`, and `build\codex-semantic-msvc-debug`. Existing import-literal semantic query coverage still passed, so `%import("...")` string-literal navigation remains on the AST/import-chain path. This slice does not claim parser-token-stream replacement, stdio smoke parity, or whole-repository green.

Accepted for the LSP completion non-code token prefix filter slice:

- `tests/language_server/test_lsp_interface.c` now validates `textDocument/completion` keeps listing language directives and meta methods for real source `%` / `@` prefixes, while `@constructor` in a line comment and `%compileTime` in a string literal return empty completion lists.
- RED WSL gcc first failed with `Comment @ text should not list meta-method completions`, proving the old token-prefix completion path treated comment text as a source-code trigger.
- GREEN implementation adds `ZrLanguageServer_Lsp_IsCursorOffsetInCodeSpan` to `lsp_source_spans.c` for cursor-between-characters requests and uses it in `lsp_interface.c` before invoking token-prefix, import-chain, receiver, or generic completion providers.
- GREEN WSL gcc, WSL clang, and Windows MSVC focused validation passed `zr_vm_language_server_lsp_interface_test` in `build/codex-semantic-wsl-gcc-debug`, `build/codex-semantic-wsl-clang-debug`, and `build\codex-semantic-msvc-debug`. This slice does not claim parser-token-stream replacement, import-string path completion, stdio smoke parity, or whole-repository green.

Accepted for the LSP meta-method hover non-code token filter slice:

- `tests/language_server/test_lsp_interface.c` now validates `textDocument/hover` still describes a real source `@constructor` meta method and still produces structured rich-hover sections for it, while `@constructor` inside a line comment or string literal does not render meta-method documentation.
- RED WSL gcc first failed with `Hover on comment @constructor text returned meta-method documentation`, proving the old raw meta-method hover scanner treated comment text as a source token.
- GREEN implementation updates `lsp_token_metadata.c` so `ZrLanguageServer_Lsp_TryGetMetaMethodHover` rejects raw `@...` tokens whose token start is outside a code span via the shared lexical span helper.
- GREEN WSL gcc, WSL clang, and Windows MSVC focused validation passed `zr_vm_language_server_lsp_interface_test` in `build/codex-semantic-wsl-gcc-debug`, `build/codex-semantic-wsl-clang-debug`, and `build\codex-semantic-msvc-debug`. This slice does not claim parser-token-stream replacement, directive hover parity, stdio smoke parity, or whole-repository green.

Accepted for the LSP semantic-token template string filter slice:

- `tests/language_server/test_lsp_interface.c` now validates semantic tokens still classify a real source `%import` directive as `keyword`, while `%import(...)`, `@constructor`, and `#trace#` inside a backtick string are not classified as `keyword`, `metaMethod`, or `decorator`.
- RED WSL gcc first failed with `Semantic tokens classified directive, meta-method, or decorator text inside a backtick string`, proving the old semantic-token scanner skipped ordinary strings/comments but not template strings.
- GREEN implementation updates `lsp_semantic_tokens.c` so the existing string-skip branch treats backtick/template strings the same way it treats double-quoted and single-quoted strings.
- GREEN WSL gcc, WSL clang, and Windows MSVC focused validation passed `zr_vm_language_server_lsp_interface_test` in `build/codex-semantic-wsl-gcc-debug`, `build/codex-semantic-wsl-clang-debug`, and `build\codex-semantic-msvc-debug`. This slice does not claim parser-token-stream replacement, broader semantic-token UI redesign, stdio smoke parity, or whole-repository green.

Accepted for the LSP stdio inline-value block-comment scanner slice:

- `tests/language_server/stdio_inline_value_semantic_smoke.js` now validates `textDocument/inlineValue` keeps real source inline values while returning no values for variable-looking text inside multi-line block-comment bodies, indented single-line block comments, and zero-column single-line block comments.
- RED WSL gcc first failed with `textDocument/inlineValue must ignore variable-looking text inside block comments`, proving the old stdio raw scanner could emit a debugger variable lookup for comment text.
- RED follow-up WSL gcc failed with `textDocument/inlineValue must ignore zero-column block-comment variables`, proving a closed `/* ... */` at column zero still needed explicit skip-state handling.
- GREEN implementation updates `stdio_inline_value.c` so line scanning maintains block-comment state across all lines, including lines before the requested range, and skips line/block comment spans before variable lookup or local semantic fact queries.
- GREEN WSL gcc, WSL clang, and Windows MSVC focused validation passed direct `tests/language_server/stdio_inline_value_semantic_smoke.js` against `build/codex-semantic-wsl-gcc-debug`, `build/codex-semantic-wsl-clang-debug`, and `build\codex-semantic-msvc-debug`. This slice does not claim parser-token-stream replacement, debugger runtime value parity, or whole-repository green.

Accepted for the LSP stdio inline-value string-literal scanner slice:

- `tests/language_server/stdio_inline_value_semantic_smoke.js` now validates `textDocument/inlineValue` keeps real source inline values while returning no values for variable-looking text inside double-quoted, single-quoted, and backtick/template string literals.
- RED WSL gcc first failed with `textDocument/inlineValue must ignore variable-looking text inside double-quoted strings`, proving the old stdio raw scanner could emit a debugger variable lookup for string literal text.
- GREEN implementation moves raw line code-span detection into `stdio_inline_value_scan.c/.h`, skips double/single/backtick string literals with escape handling, and keeps line/block comment state filtering in the same scanner boundary.
- `stdio_inline_value.c` remains the request consumer and dropped from 926 to 853 lines after the scanner extraction; `stdio_inline_value_scan.c` now owns the raw span and expression-statement scanning responsibilities.
- GREEN WSL gcc, WSL clang, and Windows MSVC focused validation passed direct `tests/language_server/stdio_inline_value_semantic_smoke.js` against `build/codex-semantic-wsl-gcc-debug`, `build/codex-semantic-wsl-clang-debug`, and `build\codex-semantic-msvc-debug`. This slice does not claim parser-token-stream replacement, debugger runtime value parity, or whole-repository green.

Accepted for the Debug variable/evaluate child-shape metadata slice:

- `tests/debug/test_debug_variable_child_shape.c` validates per-value `namedVariables` / `indexedVariables` in `variables` responses and the same metadata in `evaluate("zr")`.
- `debug_child_shape.c` centralizes the runtime snapshot child-shape rules: visible object fields plus synthetic entries such as `$prototype` count as named children, direct arrays/index windows count indexed children, and hidden `__zr_` fields stay excluded.
- `debug_protocol.c` now serializes child-shape metadata on value previews, stack argument previews, and evaluate results so a future VS Code/DAP adapter can identify expandable named/indexed children without probing every handle first.
- RED WSL gcc focused protocol test first failed with `Expected 36 Was 0`, proving the aggregate expansion count existed while the per-value field was absent.
- GREEN WSL gcc and WSL clang focused validation passed `zr_vm_debug_variable_child_shape_test` with 1 test/0 failures; Windows MSVC focused validation also passed the same test.
- This is runtime Debug snapshot metadata, not parser semantic fact reuse; complete Debug reference display and whole-repository green remain outside this slice.

Accepted for the Debug runtime semantic-summary slice:

- `ZrDebugValuePreview` and `ZrDebugEvaluateResult` now carry `semantic_summary`, and `zrdbg/1` serializes it as `semanticSummary` for stack argument previews, `variables`, and `evaluate`.
- Runtime values are summarized as `logical true/false`, `integer <value>`, `number <value>`, `expandable <type>, named N, indexed M`, and optional `ownership <kind>` metadata. Index-window evaluate results report the window as indexed data.
- `tests/debug/test_debug_variable_child_shape.c` covers protocol `semanticSummary` for `variables` on `zr`, `evaluate("zr")`, `evaluate("1 + 2")`, and `evaluate("true || missingLocal")`.
- RED WSL gcc first failed on the added `semanticSummary` assertions because the protocol returned no summary field.
- GREEN WSL gcc, WSL clang, and Windows MSVC focused validation passed `debug_variable_child_shape` and `debug_expression_diagnostics`.
- This is adapter-facing Debug runtime data inference; it does not claim complete Debug parser fact reuse, complete Debug expression-language parity, or whole-repository green.

Accepted for the Debug evaluate parser semantic fact-summary slice:

- `zr_vm_debug` now links the parser module and keeps the bridge in `debug_semantic_facts.c`, not in the safe evaluator or protocol router.
- Successful `ZrDebug_Evaluate` results keep the runtime value summary and append parser/type-inference facts when the same expression source can be parsed and inferred. Covered cases now include `evaluate("1 + 2")`, which reports `integer 3, expression binary exact, constant 3, range 3..3, unsigned range 3..3`; `evaluate("true || false")`, which reports runtime `logical true` plus parser-owned `short-circuits` and `unreachable because short-circuit skips evaluation`; `evaluate("true ? 1 : 2")`, which reports constant-condition skipped-branch reachability; a string literal containing quote/backslash/newline/tab bytes, whose parser-owned `constant` summary is escaped as one line for Debug adapters; `evaluate("zr[1]")`, which reports parser-owned `reference member access` in `semanticSummary` while runtime `referenceSummary` still reports the actual read path as `global zr, index access`; paused-frame `evaluate("inside + 1")`, which reports parser-owned `reference read inside` after Debug seeds visible frame slots into the temporary parser type environment; and `evaluate("zr")`, which reports parser-owned `reference read zr` after Debug seeds stable globals into the same temporary parser type environment. The direct Debug semantic-summary bridge also seeds compiled entry-function top-level callable bindings and child function signature metadata, so `pick(1 + 2)` can reuse parser/type-inference to report `call pick args=1`, `reference call pick`, `expression binary exact`, `constant 3`, `range 3..3`, and `unsigned range 3..3` without enabling safe-evaluate function-call execution. Member expression payloads are also included from the same expression facts: `seed[index]` reports `member index`, `reference member access index`, and the index token's `reference read index` without adding a debugger-only member classifier.
- RED WSL gcc first failed in `zr_vm_debug_variable_child_shape_test` with `FAIL:integer 3`, proving the old protocol response stopped at the runtime value summary and did not expose expression/numeric facts.
- A later RED WSL gcc extension failed with `FAIL:logical true, expression binary exact, constant true` after adding `short-circuits` and skipped-branch reachability assertions, proving Debug still consumed only runtime boolean plus root expression facts.
- The string-constant extension first failed in WSL gcc with `FAIL:string value, expression literal exact, constant "a"b\c\n\x09"`, proving `debug_semantic_facts.c` formatted decoded string payloads directly instead of escaping parser-owned constants for the single-line Debug summary.
- The parser-reference extension first failed in WSL gcc with `FAIL:integer 98, expression member exact, expression identifier exact, expression literal exact, constant 1, range 1..1`, proving `debug_semantic_facts.c` walked expression/numeric/logical/reachability facts but skipped existing `SZrSemanticReferenceFact` entries.
- The paused-frame reference extension first failed in WSL gcc with `FAIL:integer 8, expression binary exact, expression identifier exact, expression literal exact, constant 1, range 1..1`, proving the temporary parser fact pass had no current-frame `inside` binding and therefore could not emit a resolved read-reference fact.
- The stable-global reference extension first failed in WSL gcc with `FAIL:expandable object, named 36, indexed 0, expression identifier exact`, proving the temporary parser fact pass inferred the global expression shape but had no `zr` binding and therefore could not emit a resolved global read-reference fact.
- The compiled callable replay extension first failed in WSL gcc with `FAIL:expression member exact, expression identifier exact`, after the test confirmed the compiled entry function had top-level callable bindings and child function signature metadata. That proved Debug parsed the `pick(1 + 2)` expression but did not seed ordinary callable signatures into the temporary parser type environment, so no parser-owned `ZR_SEMANTIC_REFERENCE_CALL` or folded argument facts were emitted.
- The call/member expression-payload extension first failed in WSL gcc with `FAIL:expression member exact, reference call pick, ...` for `pick(1 + 2)` and `FAIL:expression member exact, reference member access index, ...` for `seed[index]`, proving the Debug semantic-summary walker already reached parser expression/reference facts but did not format `hasCallInfo` / `hasMemberInfo` payloads.
- The unsigned-range extension first failed in WSL gcc with `FAIL:integer 3, expression binary exact, constant 3, range 3..3, ...`, proving `debug_semantic_facts.c` consumed the signed range and folded constant but dropped the existing `SZrSemanticNumericFact.hasUnsignedRange` payload that REPL/LSP already preserve.
- GREEN WSL gcc, WSL clang, and Windows MSVC focused validation passed `zr_vm_debug_variable_child_shape_test` and `zr_vm_debug_expression_diagnostics_test` in earlier semantic-summary slices. The current unsigned-range extension passed direct `zr_vm_debug_expression_diagnostics_test` runs in `build/codex-semantic-wsl-gcc-debug`, `build/codex-semantic-wsl-clang-debug`, and `build\codex-semantic-msvc-debug` with 27 tests/0 failures.
- Windows MSVC initially failed to link the focused diagnostics test with unresolved `zr_debug_append_expression_semantic_facts` because `build\codex-semantic-msvc-debug` is shared-only and the internal Debug semantic-summary helper was not exported from the DLL. `debug_internal.h` now marks that existing internal test helper with `ZR_DEBUG_API`; it remains out of public `debug.h`.
- This slice is parser fact consumption for successful evaluate/semantic-summary expressions only. It now includes limited paused-frame local/argument type-environment replay from visible frame slots, stable Debug global replay for `loadedModules`, `zr`, and active `error`, compiled entry top-level callable signature replay for ordinary source functions, parser-owned call/member expression payload formatting, and Debug display of parser-owned unsigned numeric ranges. It does not claim safe-evaluate function-call execution, imported/user-defined global replay beyond stable Debug globals, generic callable parity, write-reference parity, resolved member declaration resolution, index-window bound facts, or whole-repository green.

Accepted for the Debug runtime reference-summary slice:

- `ZrDebugValuePreview` and `ZrDebugEvaluateResult` now carry `reference_summary`, and `zrdbg/1` serializes it as `referenceSummary` for stack argument previews, `variables`, and `evaluate`.
- `debug_reference_summary.c` centralizes runtime reference-source formatting and identifier-to-scope resolution. Stable top-level scope references use labels such as `argument`, `local`, `closure`, and `global`; `zr_debug_evaluate_expression` fills the field for every identifier that safe evaluate actually resolves with `skip_evaluation=false`.
- `tests/debug/test_debug_variable_child_shape.c` covers protocol `referenceSummary` for `variables` on `zr`, `evaluate("zr")`, simple `evaluate("inside")`, compound `evaluate("inside + 1")`, index-window `evaluate("zr[1..3]")`, and normal indexed `evaluate("zr[1]")`; it also rejects false attribution from skipped paths with `true || inside` and `false ? inside : 2`. `tests/debug/test_debug_expression_diagnostics.c` now covers selected-branch accumulation with `zr ? loadedModules : missingLocal`, proving the condition and selected branch are reported while the skipped branch is not.
- `zr_debug_try_evaluate_index_window` asks the safe evaluator for a reference buffer only on the base expression, copies that base attribution to the evaluate result when a window is produced, and evaluates numeric range bounds without a reference buffer.
- `debug_eval.c` appends postfix reference suffixes only after safe member/index resolution succeeds, so `evaluate("zr[1]")` reports `global zr, index access` and successful member reads append `member <name>` without claiming skipped or failed member/index paths.
- RED WSL gcc first failed on the added `referenceSummary` assertions because the protocol returned no reference-summary field. The compound-expression extension then failed with an empty `referenceSummary` for `evaluate("inside + 1")`, proving the old simple-identifier-only evaluate path did not cover computable compound reads. The skipped-path refinement failed with `Expected '' Was 'local inside'`, proving post-hoc text scanning misattributed identifiers in branches that safe evaluate parsed but skipped. The index-window extension then failed with an empty `referenceSummary` for `evaluate("zr[1..3]")`, proving the window result dropped the base expression attribution. The ordinary-index extension then failed with `FAIL:global zr`, proving `evaluate("zr[1]")` kept the base reference but did not expose the index access.
- The selected-branch accumulation extension first failed in WSL gcc with `FAIL:global zr` after adding `zr ? loadedModules : missingLocal`, proving the runtime reference summary stopped at the condition read and did not include the selected branch read.
- GREEN WSL gcc, WSL clang, and Windows MSVC focused validation passed `debug_variable_child_shape` plus `debug_expression_diagnostics` for earlier reference-summary slices; the ordinary-index extension passed `zr_vm_debug_variable_child_shape_test` with 2 tests/0 failures in WSL gcc, WSL clang, and Windows MSVC focused builds, and the current selected-branch accumulation extension passed `zr_vm_debug_expression_diagnostics_test` with 28 tests/0 failures in `build/codex-semantic-wsl-gcc-debug`, `build/codex-semantic-wsl-clang-debug`, and `build\codex-semantic-msvc-debug`.
- This remains adapter-facing runtime snapshot metadata; it only reports identifiers actually read through safe evaluate or stable scope entries, and expanded object children, member declarations, and complete Debug parser reference-fact reuse remain follow-up work.

Accepted for the parser/LSP missing control-condition diagnostic slice:

- Empty `if ()` and `while ()` conditions now emit structured parser diagnostics with code `missing_condition`, statement-specific problem text such as `Missing condition inside 'if'`, a concrete cause, and a suggestion to add a boolean expression between the parentheses.
- `parse_if_expression` and `parse_while_loop` detect the empty condition immediately after `(`, then report through `ZrParser_DiagnosticBuilder_BuildMissingCondition` rather than falling through to a generic expected-token diagnostic.
- LSP interface coverage validates both `if ()` and `while ()`; stdio `publishDiagnostics` coverage validates that JSON clients receive the stable code, message, and suggestion for `if ()`.
- RED WSL gcc first failed after adding `test_lsp_get_missing_condition_parser_diagnostics`, proving the old path did not expose the structured diagnostic.
- GREEN WSL gcc, WSL clang, and Windows MSVC focused validation passed `zr_vm_language_server_lsp_interface_test` plus the broader `tests/language_server/stdio_smoke.js` path in the isolated semantic builds.
- This slice improves parser/LSP diagnostic expressiveness only; it does not touch core runtime, AOT/value-type lowering, Debug evaluator semantics, or whole-repository green status.

Accepted for the parser/LSP switch condition close diagnostic slice:

- `tests/language_server/test_lsp_parser_diagnostics.c` now also validates missing `)` after a `switch` control condition through the focused `zr_vm_language_server_parser_diagnostics_test` target.
- `switch (choice { () { return 1; } }` returns code `missing_condition_close`, problem text `Missing ')' after 'switch' condition`, and a suggestion to insert `)` after the condition expression before the block.
- RED WSL gcc first failed after adding the focused `switch` regression: the focused target reported `expected switch condition missing-close diagnostic to name the concrete parenthesis problem`, proving `parse_switch_expression` still fell through generic close-token expectation for this condition family.
- GREEN implementation keeps the existing structured condition-close diagnostic builder/reporting path and changes `parse_switch_expression` to report after parsing the control expression instead of generic `)` expectation.
- GREEN WSL gcc focused validation passed `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN WSL clang focused validation passed `cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN Windows MSVC focused validation passed after importing VsDevCmd, verifying `cl.exe` at MSVC 14.44.35207, then building `build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_parser_diagnostics_test` and running `build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe`.
- This slice improves parser/LSP diagnostic expressiveness for `switch` condition delimiter edits only; it does not touch core runtime, AOT/value-type lowering behavior, Debug evaluator semantics, REPL execution, or whole-repository green status.

Accepted for the parser/LSP switch empty-condition diagnostic slice:

- `tests/language_server/test_lsp_parser_diagnostics.c` now also validates an empty `switch ()` control condition through the focused `zr_vm_language_server_parser_diagnostics_test` target.
- `switch () { () { return 1; } }` returns code `missing_condition`, problem text `Missing condition inside 'switch'`, and a suggestion to add a boolean expression between the parentheses.
- RED WSL gcc first failed after adding the focused empty-switch regression: the focused target reported `expected empty switch condition to publish a structured missing-condition diagnostic`, proving `parse_switch_expression` still fell through the expression parser for this condition family.
- GREEN implementation reuses the existing structured missing-condition reporter and changes `parse_switch_expression` to report immediately after `(` when the next token is `)`, matching `if` and `while`.
- GREEN WSL gcc focused validation passed `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN WSL clang focused validation passed `cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN Windows MSVC focused validation passed after importing VsDevCmd, verifying `cl.exe` at MSVC 14.44.35207, then building `build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_parser_diagnostics_test` and running `build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe`.
- This slice improves parser/LSP diagnostic expressiveness for empty `switch` condition edits only; it does not touch core runtime, AOT/value-type lowering behavior, Debug evaluator semantics, REPL execution, or whole-repository green status.

Accepted for the parser/LSP member/index structure diagnostic slice:

- `tests/language_server/test_lsp_parser_diagnostics.c` now validates both malformed computed member access and malformed dot-member access through the focused `zr_vm_language_server_parser_diagnostics_test` target.
- `value[0;` returns code `missing_index_close`, problem text `Missing closing ']' in index access`, and a suggestion to insert the closing bracket before continuing the member access.
- `value.` returns code `missing_member_name`, problem text `Missing member name after '.'`, and a suggestion to add a member name or remove the member access.
- The production parser/LSP path already emitted the structured `missing_member_name` diagnostic in the current checkout; this slice consolidated coverage into the smaller parser-diagnostics target instead of relying only on the oversized LSP interface suite.
- GREEN WSL gcc focused validation passed `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN WSL clang focused validation passed `cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test`. The clang build reconfigured and emitted existing warning noise before the focused executable passed both tests.
- GREEN Windows MSVC focused validation passed after importing VsDevCmd and verifying `cl.exe` at MSVC 14.44.35207, then building `build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_parser_diagnostics_test` and running `build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe`.
- This slice improves focused parser/LSP diagnostic coverage only; it does not touch core runtime, AOT/value-type lowering, Debug evaluator semantics, REPL execution, or whole-repository green status.

Accepted for the parser/LSP aggregate literal close diagnostic slice:

- `tests/language_server/test_lsp_parser_diagnostics.c` now validates missing close delimiters for array and object literals through the focused `zr_vm_language_server_parser_diagnostics_test` target.
- `return [1, 2` returns code `missing_array_close`, problem text `Missing closing ']' in array literal`, and a suggestion to insert the closing bracket after the last array element.
- `return {a: 1` returns code `missing_object_close`, problem text `Missing closing '}' in object literal`, and a suggestion to insert the closing brace after the last object property.
- RED WSL gcc first failed after adding each focused regression: the old parser path produced no matching `missing_array_close` or `missing_object_close` structured diagnostic and fell back to the generic close-token expectation path.
- GREEN implementation adds the two builder/reporting paths and changes `parse_array_literal` / `parse_object_literal` to report the structured close diagnostic before returning `NULL`, while keeping computed member/index access on the existing `missing_index_close` path.
- GREEN WSL gcc focused validation passed `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN WSL clang focused validation passed `cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test`; the build emitted existing warning noise before the focused executable reported all parser diagnostic tests completed.
- GREEN Windows MSVC focused validation passed after importing VsDevCmd and verifying `cl.exe` at MSVC 14.44.35207, then building `build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_parser_diagnostics_test` and running `build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe`.
- This slice improves parser/LSP diagnostic expressiveness for aggregate literal edits only; it does not touch core runtime, AOT/value-type lowering, Debug evaluator semantics, REPL execution, or whole-repository green status.

Accepted for the parser/LSP array literal separator diagnostic slice:

- `tests/language_server/test_lsp_parser_diagnostics.c` now validates an array literal with adjacent elements and no separator through the focused `zr_vm_language_server_parser_diagnostics_test` target.
- `return [1 2];` returns code `missing_array_element_separator`, problem text `Missing separator between array elements`, and a suggestion to insert `,` or `;` between the elements.
- RED WSL gcc first failed after adding the focused regression: the old parser path did not emit `missing_array_element_separator` and the focused target reported `expected missing-array-element-separator diagnostic to carry code, problem text, and suggestion`.
- GREEN implementation adds the builder/reporting path and changes `parse_array_literal` to detect an expression-start token after an element before falling back to `missing_array_close`, so LSP names the separator problem at the second element token.
- GREEN WSL gcc focused validation passed `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN WSL clang focused validation passed `cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test`; the build emitted existing warning noise before the focused executable reported all parser diagnostic tests completed.
- GREEN Windows MSVC focused validation passed after importing VsDevCmd, verifying `cl.exe` at MSVC 14.44.35207, then building `build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_parser_diagnostics_test` and running `build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe`.
- This slice improves parser/LSP diagnostic expressiveness for array-literal edit errors only; it does not touch core runtime, AOT/value-type lowering behavior, Debug evaluator semantics, REPL execution, or whole-repository green status.

Accepted for the parser/LSP statement semicolon diagnostic slice:

- `tests/language_server/test_lsp_parser_diagnostics.c` now validates missing `;` terminators after both return statements and expression statements through the focused `zr_vm_language_server_parser_diagnostics_test` target.
- `return 1\nvar next = 2;` returns code `missing_statement_semicolon`, problem text `Missing ';' after return statement`, and a suggestion to insert `;` after the return statement before the next statement.
- `1 + 2\nvar next = 3;` returns the same code with statement-specific problem text `Missing ';' after expression statement` and the matching expression-statement suggestion.
- RED WSL gcc first failed after adding the focused regression: the old parser path did not emit `missing_statement_semicolon` and the focused target reported `expected return statement missing-semicolon diagnostic to carry code, problem text, and suggestion`.
- GREEN implementation adds the builder/reporting path in the parser diagnostics layer and changes return/expression statement parsing to report the structured terminator diagnostic after a statement body is parsed but before the next statement token is consumed.
- GREEN WSL gcc focused validation passed `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN WSL clang focused validation passed `cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN Windows MSVC focused validation passed after importing VsDevCmd, verifying `cl.exe` at MSVC 14.44.35207, then building `build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_parser_diagnostics_test` and running `build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe`.
- This slice improves parser/LSP diagnostic expressiveness for statement terminator edits only; it does not touch core runtime, AOT/value-type lowering behavior, Debug evaluator semantics, REPL execution, or whole-repository green status.

Accepted for the parser/LSP variable declaration semicolon diagnostic slice:

- `tests/language_server/test_lsp_parser_diagnostics.c` now also validates missing `;` terminators after variable declarations through the focused `zr_vm_language_server_parser_diagnostics_test` target.
- `var seed = 1\nvar next = 2;` returns code `missing_statement_semicolon`, problem text `Missing ';' after variable declaration statement`, and a suggestion to insert `;` after the variable declaration statement before the next statement.
- RED WSL gcc first failed after adding the focused variable-declaration regression: the focused target reported `expected variable declaration missing-semicolon diagnostic to carry code, problem text, and suggestion`, proving `parse_variable_declaration` still exempted the following `var` declaration from structured terminator reporting.
- GREEN implementation reuses the existing structured statement-semicolon reporter and changes `parse_variable_declaration` to report for a missing terminator before the next non-postfix token, while keeping `.`, `[`, and `(` as expression continuations and preserving EOF.
- GREEN WSL gcc focused validation passed `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN WSL clang focused validation passed `cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN Windows MSVC focused validation passed after importing VsDevCmd, verifying `cl.exe` at MSVC 14.44.35207, then building `build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_parser_diagnostics_test` and running `build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe`.
- This slice improves parser/LSP diagnostic expressiveness for variable declaration terminator edits only; it does not touch core runtime, AOT/value-type lowering behavior, Debug evaluator semantics, REPL execution, or whole-repository green status.

Accepted for the parser/LSP module declaration semicolon diagnostic slice:

- `tests/language_server/test_lsp_parser_diagnostics.c` now also validates missing `;` terminators after module declarations through the focused `zr_vm_language_server_parser_diagnostics_test` target.
- `%module "main"\nvar next = 2;` returns code `missing_statement_semicolon`, problem text `Missing ';' after module declaration statement`, and a suggestion to insert `;` after the module declaration statement before the next statement.
- RED WSL gcc first failed after adding the focused module-declaration regression: the focused target reported `expected module declaration missing-semicolon diagnostic to carry code, problem text, and suggestion`, proving `parse_module_declaration` still used a raw semicolon expectation after the module path.
- GREEN implementation reuses the existing structured statement-semicolon reporter and changes `parse_module_declaration` to report before building the AST when the terminator is missing, while preserving the normal consume path when `;` is present.
- GREEN WSL gcc focused validation passed `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN WSL clang focused validation passed `cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN Windows MSVC focused validation passed after importing VsDevCmd, verifying `cl.exe` at MSVC 14.44.35207, then building `build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_parser_diagnostics_test` and running `build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe`.
- This slice improves parser/LSP diagnostic expressiveness for module declaration terminator edits only; it does not touch core runtime, AOT/value-type lowering behavior, Debug evaluator semantics, REPL execution, or whole-repository green status.

Accepted for the parser/LSP break/continue statement semicolon diagnostic slice:

- `tests/language_server/test_lsp_parser_diagnostics.c` now also validates missing `;` terminators after `break` and `continue` statements inside loop bodies through the focused `zr_vm_language_server_parser_diagnostics_test` target.
- `while (true) { break\n var next = 2; }` returns code `missing_statement_semicolon`, problem text `Missing ';' after break statement`, and a suggestion to insert `;` after the break statement before the next statement.
- `while (true) { continue\n var next = 2; }` returns the same code with statement-specific problem text `Missing ';' after continue statement` and the matching continue-statement suggestion.
- RED WSL gcc first failed after adding the focused break/continue regressions: the focused target reported `expected break statement missing-semicolon diagnostic to carry code, problem text, and suggestion`, proving the old parser path still fell through the generic terminator handling for this statement family.
- GREEN implementation keeps the existing structured semicolon diagnostic builder/reporting path and changes `parse_break_continue_statement` so a newline-started next statement is not consumed as a break/continue expression before reporting `missing_statement_semicolon`.
- GREEN WSL gcc focused validation passed `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN WSL clang focused validation passed `cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test`; the build emitted existing warning noise before the focused executable reported all parser diagnostic tests completed.
- GREEN Windows MSVC focused validation passed after importing VsDevCmd, verifying `cl.exe` at MSVC 14.44.35207, then building `build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_parser_diagnostics_test` and running `build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe`.
- This slice improves parser/LSP diagnostic expressiveness for loop-control statement terminator edits only; it does not touch core runtime, AOT/value-type lowering behavior, Debug evaluator semantics, REPL execution, or whole-repository green status.

Accepted for the parser/LSP throw/out statement semicolon diagnostic slice:

- `tests/language_server/test_lsp_parser_diagnostics.c` now also validates missing `;` terminators after `throw` and `out` statements through the focused `zr_vm_language_server_parser_diagnostics_test` target.
- `throw 1\nvar next = 2;` returns code `missing_statement_semicolon`, problem text `Missing ';' after throw statement`, and a suggestion to insert `;` after the throw statement before the next statement.
- `out 1\nvar next = 2;` returns the same code with statement-specific problem text `Missing ';' after out statement` and the matching out-statement suggestion.
- RED WSL gcc first failed after adding the focused throw/out regressions: the focused target reported `expected throw statement missing-semicolon diagnostic to carry code, problem text, and suggestion`, proving the old parser path still fell through generic terminator handling for this statement family.
- GREEN implementation keeps the existing structured semicolon diagnostic builder/reporting path and changes `parse_throw_statement` / `parse_out_statement` to report after expression parsing instead of generic semicolon expectation.
- GREEN WSL gcc focused validation passed `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN WSL clang focused validation passed `cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN Windows MSVC focused validation passed after importing VsDevCmd, verifying `cl.exe` at MSVC 14.44.35207, then building `build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_parser_diagnostics_test` and running `build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe`.
- This slice improves parser/LSP diagnostic expressiveness for throw/out statement terminator edits only; it does not touch core runtime, AOT/value-type lowering behavior, Debug evaluator semantics, REPL execution, or whole-repository green status.

Accepted for the parser/LSP `%using` declaration semicolon diagnostic slice:

- `tests/language_server/test_lsp_parser_diagnostics.c` now also validates a missing `;` terminator after a single-expression `%using` declaration through the focused `zr_vm_language_server_parser_diagnostics_test` target.
- `var resource = 1; %using resource\nvar next = 2;` returns code `missing_statement_semicolon`, problem text `Missing ';' after using statement`, and a suggestion to insert `;` after the using statement before the next statement.
- RED WSL gcc first failed after adding the focused `%using` regression: the focused target reported `expected using statement missing-semicolon diagnostic to carry code, problem text, and suggestion`, proving `parse_using_statement_body` still fell through the generic semicolon expectation for this statement family.
- GREEN implementation keeps the existing structured semicolon diagnostic builder/reporting path and changes the single-expression `%using` declaration terminator check to report after resource expression parsing instead of generic semicolon expectation.
- GREEN WSL gcc focused validation passed `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN WSL clang focused validation passed `cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN Windows MSVC focused validation passed after importing VsDevCmd, verifying `cl.exe` at MSVC 14.44.35207, then building `build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_parser_diagnostics_test` and running `build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe`.
- This slice improves parser/LSP diagnostic expressiveness for `%using` declaration terminator edits only; it does not touch core runtime, AOT/value-type lowering behavior, Debug evaluator semantics, REPL execution, or whole-repository green status.

Accepted for the parser/LSP interface method signature semicolon diagnostic slice:

- `tests/language_server/test_lsp_parser_diagnostics.c` now validates a missing `;` terminator after an interface method signature through the focused `zr_vm_language_server_parser_diagnostics_test` target.
- `interface Readable { read(value: int): int }` returns code `missing_statement_semicolon`, problem text `Missing ';' after interface method signature statement`, and a suggestion to insert `;` after the interface method signature statement before the interface closes.
- RED WSL gcc first failed after adding the focused interface-method-signature semicolon regression: the focused target reported `expected interface-method-signature missing-semicolon diagnostic to carry code, problem text, and suggestion`, proving `parse_interface_method_signature` still fell through generic semicolon expectation for this member family.
- GREEN implementation keeps the existing structured semicolon diagnostic builder/reporting path and changes `parse_interface_method_signature` to report after parameter, return type, and where-clause parsing instead of generic semicolon expectation.
- GREEN WSL gcc focused validation passed `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN WSL clang focused validation passed `cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN Windows MSVC focused validation passed after importing VsDevCmd, verifying `cl.exe` at MSVC 14.44.35207, then building `build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_parser_diagnostics_test` and running `build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe`.
- `parser_interface.c` remains below the large-file threshold after this narrow method-signature parser change.
- This slice improves parser/LSP diagnostic expressiveness for interface method signature terminator edits only; it does not touch interface fields/properties/meta signatures, constructors, struct/extern parameter-list families, core runtime, AOT/value-type lowering behavior, Debug evaluator semantics, REPL execution, or whole-repository green status.

Accepted for the parser/LSP interface meta signature semicolon diagnostic slice:

- `tests/language_server/test_lsp_parser_diagnostics.c` now validates a missing `;` terminator after an interface meta signature through the focused `zr_vm_language_server_parser_diagnostics_test` target.
- `interface Callable { @call(value: int): int }` returns code `missing_statement_semicolon`, problem text `Missing ';' after interface meta signature statement`, and a suggestion to insert `;` after the interface meta signature statement before the interface closes.
- RED WSL gcc first failed after adding the focused interface-meta-signature semicolon regression: the focused target reported `expected interface-meta-signature missing-semicolon diagnostic to carry code, problem text, and suggestion`, proving `parse_interface_meta_signature` still fell through generic semicolon expectation for this member family.
- First GREEN attempts still failed only this new case because the initial edit landed on neighboring field/property semicolon branches. The final implementation restores those branches and changes only `parse_interface_meta_signature`.
- GREEN implementation keeps the existing structured semicolon diagnostic builder/reporting path and changes `parse_interface_meta_signature` to report after parameter-list and return-type parsing instead of generic semicolon expectation.
- GREEN WSL gcc focused validation passed `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN WSL clang focused validation passed `cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN Windows MSVC focused validation passed after importing VsDevCmd, verifying `cl.exe` at MSVC 14.44.35207, then building `build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_parser_diagnostics_test` and running `build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe`.
- `parser_interface.c` remains below the large-file threshold after this narrow meta-signature parser change.
- This slice improves parser/LSP diagnostic expressiveness for interface meta signature terminator edits only; it does not touch interface fields/properties/method signatures, constructors, struct/extern parameter-list families, core runtime, AOT/value-type lowering behavior, Debug evaluator semantics, REPL execution, or whole-repository green status.

Accepted for the parser/LSP interface property signature semicolon diagnostic slice:

- `tests/language_server/test_lsp_parser_diagnostics.c` now validates a missing `;` terminator after an interface property signature through the focused `zr_vm_language_server_parser_diagnostics_test` target.
- `interface Sized { get length: int }` returns code `missing_statement_semicolon`, problem text `Missing ';' after interface property signature statement`, and a suggestion to insert `;` after the interface property signature statement before the interface closes.
- RED WSL gcc first failed after adding the focused interface-property-signature semicolon regression: the focused target reported `expected interface-property-signature missing-semicolon diagnostic to carry code, problem text, and suggestion`.
- First GREEN implementation changed `parse_interface_property_signature` to use the structured reporter, but the focused test still failed only the new case. Inspecting interface member dispatch showed bare `get` / `set` members were not routed to the property parser unless preceded by an access modifier or field-like prefix.
- Final GREEN keeps the structured property-signature terminator reporter and also routes line-start `get` / `set` members directly to `parse_interface_property_signature`, matching that parser's optional-access grammar.
- GREEN WSL gcc focused validation passed `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN WSL clang focused validation passed `cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN Windows MSVC focused validation passed after importing VsDevCmd, verifying `cl.exe` at MSVC 14.44.35207, then building `build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_parser_diagnostics_test` and running `build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe`.
- `parser_interface.c` remains below the large-file threshold after this narrow property-signature parser/dispatch change.
- This slice improves parser/LSP diagnostic expressiveness and dispatch robustness for interface property signature terminator edits only; it does not touch interface fields/method/meta signatures, constructors, struct/extern parameter-list families, core runtime, AOT/value-type lowering behavior, Debug evaluator semantics, REPL execution, or whole-repository green status.

Accepted for the parser/LSP interface field declaration semicolon diagnostic slice:

- `tests/language_server/test_lsp_parser_diagnostics.c` now validates a missing `;` terminator after an interface field declaration through the focused `zr_vm_language_server_parser_diagnostics_test` target.
- `interface Entity { var id: int }` returns code `missing_statement_semicolon`, problem text `Missing ';' after interface field declaration statement`, and a suggestion to insert `;` after the interface field declaration statement before the interface closes.
- RED WSL gcc first failed after adding the focused interface-field-declaration semicolon regression: the focused target reported `expected interface-field-declaration missing-semicolon diagnostic to carry code, problem text, and suggestion`.
- GREEN implementation keeps the existing structured semicolon diagnostic builder/reporting path and changes `parse_interface_field_declaration` to report after field name/type parsing instead of generic semicolon expectation.
- GREEN WSL gcc focused validation passed `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN WSL clang focused validation passed `cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN Windows MSVC focused validation passed after importing VsDevCmd, verifying `cl.exe` at MSVC 14.44.35207, then building `build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_parser_diagnostics_test` and running `build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe`.
- `parser_interface.c` remains below the large-file threshold after this narrow field-declaration parser change.
- This slice improves parser/LSP diagnostic expressiveness for interface field declaration terminator edits only; it does not touch interface method/property/meta signatures, constructors, struct/extern parameter-list families, core runtime, AOT/value-type lowering behavior, Debug evaluator semantics, REPL execution, or whole-repository green status.

Accepted for the parser/LSP class field declaration semicolon diagnostic slice:

- `tests/language_server/test_lsp_parser_diagnostics.c` now validates a missing `;` terminator after a class field declaration through the focused `zr_vm_language_server_parser_diagnostics_test` target.
- `class Entity { var id: int }` returns code `missing_statement_semicolon`, problem text `Missing ';' after class field declaration statement`, and a suggestion to insert `;` after the class field declaration statement before the class closes.
- RED WSL gcc first failed after adding the focused class-field semicolon regression: the focused target reported `expected class-field missing-semicolon diagnostic to carry code, problem text, and suggestion`.
- GREEN implementation keeps the existing structured semicolon diagnostic builder/reporting path and changes `parse_class_field` to report after field name/type/default/where parsing instead of generic semicolon expectation.
- GREEN WSL gcc focused validation passed `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN WSL clang focused validation passed `cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN Windows MSVC focused validation passed after importing VsDevCmd, verifying `cl.exe` at MSVC 14.44.35207, then building `build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_parser_diagnostics_test` and running `build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe`.
- `parser_class.c` remains below the large-file threshold after this narrow field-declaration parser change; the pre-existing class-method parameter-list close change in the same file remains untouched.
- This slice improves parser/LSP diagnostic expressiveness for class field declaration terminator edits only; it does not touch class methods/properties/meta functions, constructors, struct/extern parameter-list families, core runtime, AOT/value-type lowering behavior, Debug evaluator semantics, REPL execution, or whole-repository green status.

Accepted for the parser/LSP class getter/setter accessor semicolon diagnostic slice:

- `tests/language_server/test_lsp_parser_diagnostics.c` now validates missing `;` terminators after class getter/setter accessor signatures through the focused `zr_vm_language_server_parser_diagnostics_test` target.
- `class Sized { get length: int }` returns code `missing_statement_semicolon`, problem text `Missing ';' after class getter statement`, and a suggestion to insert `;` after the class getter statement before the class closes.
- `class Sized { set length(value: int) }` returns code `missing_statement_semicolon`, problem text `Missing ';' after class setter statement`, and a suggestion to insert `;` after the class setter statement before the class closes.
- RED WSL gcc first failed after adding the focused class-getter semicolon regression: the focused target reported `expected class-getter missing-semicolon diagnostic to carry code, problem text, and suggestion`.
- GREEN implementation keeps the existing structured semicolon diagnostic builder/reporting path, routes line-start `get` / `set` class members into the property parser, and changes `parse_property_get` / `parse_property_set` to report accessor-specific missing-semicolon diagnostics when an accessor signature is followed by neither `;` nor a `{ ... }` body.
- GREEN WSL gcc focused validation passed `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN WSL clang focused validation passed `cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN Windows MSVC focused validation passed after importing VsDevCmd, verifying `cl.exe` at MSVC 14.44.35207, then building `build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_parser_diagnostics_test` and running `build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe`.
- `parser_class.c` remains below the large-file threshold after this narrow accessor parser change; the pre-existing class-method parameter-list close change in the same file remains untouched.
- This slice improves parser/LSP diagnostic expressiveness for class getter/setter accessor terminator edits only; it does not touch constructors, class fields/methods/meta functions beyond class-member dispatch for `get` / `set`, struct/extern parameter-list families, core runtime, AOT/value-type lowering behavior, Debug evaluator semantics, REPL execution, or whole-repository green status.

Accepted for the parser/LSP class method/meta function semicolon diagnostic slice:

- `tests/language_server/test_lsp_parser_diagnostics.c` now validates missing `;` terminators after class method and class meta function signatures through the focused `zr_vm_language_server_parser_diagnostics_test` target.
- `class Box { func read(value: int): int }` returns code `missing_statement_semicolon`, problem text `Missing ';' after class method statement`, and a suggestion to insert `;` after the class method statement before the class closes.
- `class Callable { @call(value: int): int }` returns code `missing_statement_semicolon`, problem text `Missing ';' after class meta function statement`, and a suggestion to insert `;` after the class meta function statement before the class closes.
- RED WSL gcc first failed after adding the focused class-method semicolon regression: the focused target reported `expected class-method missing-semicolon diagnostic to carry code, problem text, and suggestion`.
- RED WSL gcc then failed after adding the focused class-meta-function semicolon regression: the focused target reported `expected class-meta-function missing-semicolon diagnostic to carry code, problem text, and suggestion`.
- GREEN implementation keeps the existing structured semicolon diagnostic builder/reporting path and changes `parse_class_method` / `parse_class_meta_function` to report method/meta-specific missing-semicolon diagnostics when a signature is followed by neither `;` nor a `{ ... }` body.
- GREEN WSL gcc focused validation passed `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN WSL clang focused validation passed `cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN Windows MSVC focused validation passed after importing VsDevCmd, verifying `cl.exe` at MSVC 14.44.35207, then building `build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_parser_diagnostics_test` and running `build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe`.
- `parser_class.c` remains below the large-file threshold after this narrow method/meta parser change; the pre-existing class-method parameter-list close change in the same file remains untouched.
- This slice improves parser/LSP diagnostic expressiveness for class method/meta function terminator edits only; it does not touch constructors, class fields/accessors, struct/extern parameter-list families, core runtime, AOT/value-type lowering behavior, Debug evaluator semantics, REPL execution, or whole-repository green status.

Accepted for the parser/LSP class declaration body-open diagnostic slice:

- `tests/language_server/test_lsp_declaration_parser_diagnostics.c` now validates missing declaration body openers through the new focused `zr_vm_language_server_declaration_parser_diagnostics_test` target instead of growing the already-large parser diagnostics test file.
- `class Box` returns code `missing_declaration_body_open`, problem text `Missing '{' to start class declaration body`, and a suggestion to insert `{` after the class declaration header.
- RED WSL gcc first failed after adding the new focused target and class declaration body-open regression: the focused target reported `expected class declaration missing-body-open diagnostic to carry code, problem text, and suggestion`.
- GREEN implementation adds the shared structured diagnostic builder/reporting path `ZrParser_DiagnosticBuilder_BuildMissingDeclarationBodyOpen` / `report_missing_declaration_body_open` and changes `parse_class_declaration` to report when a parsed class header is followed by neither `{` nor a valid body opener.
- GREEN WSL gcc focused validation passed `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_declaration_parser_diagnostics_test` followed by `./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_declaration_parser_diagnostics_test`; the existing `zr_vm_language_server_parser_diagnostics_test` target also passed in the same toolchain.
- GREEN WSL clang focused validation passed `cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test -j 8` followed by both focused executables.
- GREEN Windows MSVC focused validation passed after importing VsDevCmd, verifying `cl.exe` at MSVC 14.44.35207, then building `build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test` and running both focused executables. MSVC emitted existing AOT/runtime and parser/LSP warning noise, but both focused tests passed.
- `tests/language_server/test_lsp_parser_diagnostics.c` stays below the large-file threshold because declaration body-open coverage moved to the new focused declaration diagnostics target; `parser_class.c` remains below the large-file threshold after this narrow class declaration parser change.
- This slice improves parser/LSP diagnostic expressiveness for class declaration body opener edits only; it does not touch constructors, class members, struct/interface/extern body openers, core runtime, AOT/value-type lowering behavior, Debug evaluator semantics, REPL execution, or whole-repository green status.

Accepted for the parser/LSP interface declaration body-open diagnostic slice:

- `tests/language_server/test_lsp_declaration_parser_diagnostics.c` now extends the focused `zr_vm_language_server_declaration_parser_diagnostics_test` target with `interface Sized`, keeping declaration opener coverage out of the already-large general parser diagnostics test file.
- `interface Sized` returns code `missing_declaration_body_open`, problem text `Missing '{' to start interface declaration body`, and a suggestion to insert `{` after the interface declaration header.
- RED WSL gcc first failed after adding the interface regression: the focused target reported `expected interface declaration missing-body-open diagnostic to carry code, problem text, and suggestion`.
- GREEN implementation reuses `ZrParser_DiagnosticBuilder_BuildMissingDeclarationBodyOpen` / `report_missing_declaration_body_open` and changes `parse_interface_declaration` to report when a parsed interface header is followed by neither `{` nor a valid body opener.
- GREEN WSL gcc focused validation passed `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test -j 8` followed by both focused executables.
- GREEN WSL clang focused validation passed `cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test -j 8` followed by both focused executables.
- GREEN Windows MSVC focused validation passed after importing VsDevCmd, then building `build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test` and running both focused executables.
- `parser_interface.c` is 508 lines and `tests/language_server/test_lsp_declaration_parser_diagnostics.c` is 215 lines after this slice, so neither touched file crosses the large-file threshold.
- This slice improves parser/LSP diagnostic expressiveness for interface declaration body opener edits only; it does not touch constructor receiver handling, SemIR, AOT/value-type lowering behavior, Debug evaluator semantics, REPL execution, or whole-repository green status.

Accepted for the parser/LSP function declaration body-open diagnostic slice:

- `tests/language_server/test_lsp_declaration_parser_diagnostics.c` now extends the focused `zr_vm_language_server_declaration_parser_diagnostics_test` target with `func pick(): int`, keeping declaration opener coverage out of the already-large general parser diagnostics test file.
- `func pick(): int` returns code `missing_declaration_body_open`, problem text `Missing '{' to start function declaration body`, and a suggestion to insert `{` after the function declaration header.
- RED WSL gcc first failed after adding the function regression: the focused target reported `expected function declaration missing-body-open diagnostic to carry code, problem text, and suggestion`.
- GREEN implementation reuses `ZrParser_DiagnosticBuilder_BuildMissingDeclarationBodyOpen` / `report_missing_declaration_body_open` and changes `parse_function_declaration` to report after the parsed function header/where clauses when the current token is not `{`, before returning through the existing cleanup path.
- GREEN WSL gcc focused validation passed `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test -j 8` followed by both focused executables.
- GREEN WSL clang focused validation passed `cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test -j 8` followed by both focused executables.
- GREEN Windows MSVC focused validation passed after importing VsDevCmd, then building `build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test` and running both focused executables.
- `parser_declarations.c` is 287 lines and `tests/language_server/test_lsp_declaration_parser_diagnostics.c` is 227 lines after this slice, so neither touched file crosses the large-file threshold.
- This slice improves parser/LSP diagnostic expressiveness for function declaration body opener edits only; it does not touch statement dispatch probing, constructor receiver handling, SemIR, AOT/value-type lowering behavior, Debug evaluator semantics, REPL execution, or whole-repository green status.

Accepted for the parser/LSP enum declaration body-open diagnostic slice:

- `tests/language_server/test_lsp_declaration_parser_diagnostics.c` now extends the focused `zr_vm_language_server_declaration_parser_diagnostics_test` target with `enum Tone`, keeping declaration opener coverage out of the already-large general parser diagnostics test file.
- `enum Tone` returns code `missing_declaration_body_open`, problem text `Missing '{' to start enum declaration body`, and a suggestion to insert `{` after the enum declaration header.
- RED WSL gcc first failed after adding the enum regression: the focused target reported `expected enum declaration missing-body-open diagnostic to carry code, problem text, and suggestion`.
- GREEN implementation reuses `ZrParser_DiagnosticBuilder_BuildMissingDeclarationBodyOpen` / `report_missing_declaration_body_open` and changes `parse_enum_declaration` to report when a parsed enum header is followed by neither `{` nor a valid body opener.
- GREEN WSL gcc focused validation passed `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test -j 8` followed by both focused executables.
- GREEN WSL clang focused validation passed `cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test -j 8` followed by both focused executables.
- GREEN Windows MSVC focused validation passed after importing VsDevCmd, then building `build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test` and running both focused executables.
- `parser_extern.c` is already above the large-file threshold, so this slice only replaced the existing enum body-opener expected-token branch and did not add new helpers or responsibilities; broader enum/extern parser extraction remains a separate cleanup boundary.
- This slice improves parser/LSP diagnostic expressiveness for enum declaration body opener edits only; it does not touch extern block body openers, intermediate declarations, constructor receiver handling, SemIR, AOT/value-type lowering behavior, Debug evaluator semantics, REPL execution, or whole-repository green status.

Accepted for the parser/LSP extern block body-open diagnostic slice:

- `tests/language_server/test_lsp_declaration_parser_diagnostics.c` now extends the focused `zr_vm_language_server_declaration_parser_diagnostics_test` target with `%extern("fixture")`, keeping declaration opener coverage out of the already-large general parser diagnostics test file.
- `%extern("fixture")` returns code `missing_declaration_body_open`, problem text `Missing '{' to start extern block body`, and a suggestion to insert `{` after the extern block header.
- RED WSL gcc first failed after adding the extern block regression: the focused target reported `expected extern block missing-body-open diagnostic to carry code, problem text, and suggestion`.
- GREEN implementation reuses `ZrParser_DiagnosticBuilder_BuildMissingDeclarationBodyOpen` / `report_missing_declaration_body_open` and changes `parse_extern_block` to report only when the extern block header is followed by end-of-input or a stray `}` instead of `{`.
- The existing brace-less single extern member form remains supported because `parse_extern_block` still falls back to `parse_extern_member_declaration` when a member token follows `%extern("fixture")`.
- GREEN WSL gcc focused validation first passed `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_declaration_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_declaration_parser_diagnostics_test`.
- GREEN WSL gcc focused validation then passed `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test -j 8` followed by both focused executables.
- GREEN WSL clang focused validation passed `cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test -j 8` followed by both focused executables.
- GREEN Windows MSVC focused validation passed after importing VsDevCmd, then building `build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test` and running both focused executables.
- `parser_extern.c` is already above the large-file threshold, so this slice only adjusted the existing extern block fallback and did not add new helpers or responsibilities; broader extern parser extraction remains a separate cleanup boundary. `tests/language_server/test_lsp_declaration_parser_diagnostics.c` is 218 lines after this slice.
- This slice improves parser/LSP diagnostic expressiveness for extern block body opener edits only; it does not touch intermediate declarations, constructor receiver handling, SemIR, AOT/value-type lowering behavior, Debug evaluator semantics, REPL execution, or whole-repository green status.

Accepted for the parser/LSP test declaration body-open diagnostic slice:

- `tests/language_server/test_lsp_declaration_parser_diagnostics.c` now extends the focused `zr_vm_language_server_declaration_parser_diagnostics_test` target with `%test("smoke")`, keeping declaration opener coverage out of the already-large general parser diagnostics test file.
- `%test("smoke")` returns code `missing_declaration_body_open`, problem text `Missing '{' to start test declaration body`, and a suggestion to insert `{` after the test declaration header.
- RED WSL gcc first failed after adding the test declaration regression: the focused target reported `expected test declaration missing-body-open diagnostic to carry code, problem text, and suggestion`.
- GREEN implementation reuses `ZrParser_DiagnosticBuilder_BuildMissingDeclarationBodyOpen` / `report_missing_declaration_body_open` and changes `parse_test_declaration` to report before the generic block parser when the parsed `%test(...)` header is followed by something other than `{`.
- Legal `%test(...) { ... }` declarations still enter the existing `parse_block` path; this slice only changes the missing-body-opener recovery path.
- GREEN WSL gcc focused validation first passed `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_declaration_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_declaration_parser_diagnostics_test`.
- GREEN WSL gcc focused validation then passed `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test -j 8` followed by both focused executables.
- GREEN WSL clang focused validation passed `cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test -j 8` followed by both focused executables.
- GREEN Windows MSVC focused validation passed after importing VsDevCmd, then building `build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test` and running both focused executables.
- `parser_extern.c` remains an oversized parser file, so this slice only inserted a local guard before the existing test block parse and did not add new helpers or responsibilities; broader extern/directive parser extraction remains a separate cleanup boundary.
- This slice improves parser/LSP diagnostic expressiveness for test declaration body opener edits only; it does not touch constructor receiver handling, SemIR, AOT/value-type lowering behavior, Debug evaluator semantics, REPL execution, or whole-repository green status.

Accepted for the parser/LSP statement body-open diagnostic slice:

- `tests/language_server/test_lsp_statement_parser_diagnostics.c` now validates missing control-statement and `else` body openers through the new focused `zr_vm_language_server_statement_parser_diagnostics_test` target instead of growing the already-large general parser diagnostics test file.
- `if (ready)\nreturn 1;`, `while (ready)\nreturn 1;`, `for (;;)\nreturn 1;`, `for (var item in items)\nreturn item;`, `switch (choice)\nreturn 1;`, `switch (choice) { (1)\nreturn 1; }`, `switch (choice) { ()\nreturn 1; }`, `if (ready) { return 1; } else\nreturn 2;`, `try\nreturn 1;`, `try { throw 1; } catch (error)\nreturn 2;`, `try { return 1; } finally\nreturn 2;`, and `%using (resource)\nreturn resource;` return code `missing_statement_body_open`, statement-kind-specific problem text such as `Missing '{' to start if statement body`, `Missing '{' to start foreach statement body`, `Missing '{' to start switch case body`, `Missing '{' to start switch default body`, `Missing '{' to start catch statement body`, or `Missing '{' to start using statement body`, and suggestions to insert `{` after the matching statement header.
- RED WSL gcc first failed after adding the focused target and `if` regression: the focused target reported `expected if statement missing-body-open diagnostic to carry code, problem text, and suggestion`. A later RED run after adding the implementation but before fixture cleanup failed only the `for` assertion because `for (var i = 0; i < 3; i = i + 1)` currently interacts with the `for(var ...)` / `foreach` dispatch path before this body-opener check.
- GREEN implementation adds `ZrParser_DiagnosticBuilder_BuildMissingStatementBodyOpen` / `report_missing_statement_body_open` and changes `parse_if_expression`, `parse_while_loop`, and traditional `parse_for_loop` to report when a parsed header is followed by neither `{` nor a valid block opener. The `for` regression uses `for (;;)` to target body-opener behavior directly while leaving the existing `for(var ...)` dispatch nuance for a separate parser-recognition slice.
- Follow-up RED WSL gcc failed only after adding `switch (choice)\nreturn 1;`, reporting `expected switch statement missing-body-open diagnostic to carry code, problem text, and suggestion`. GREEN changes `parse_switch_expression` to reuse `report_missing_statement_body_open` after parsing and closing the control condition but before consuming the switch cases block opener.
- Follow-up RED WSL gcc failed first after adding `switch (choice) { (1)\nreturn 1; }` and `switch (choice) { ()\nreturn 1; }`, reporting `expected switch case missing-body-open diagnostic to carry code, problem text, and suggestion`. GREEN changes `parse_switch_expression` to reuse `report_missing_statement_body_open` before parsing normal/default case blocks; the missing-opener paths free the parsed switch expression, case value, and case array before returning.
- Follow-up RED WSL gcc failed only after adding `if (ready) { return 1; } else\nreturn 2;`, reporting `expected else statement missing-body-open diagnostic to carry code, problem text, and suggestion`. GREEN changes the plain `else` branch in `parse_if_expression` to reuse `report_missing_statement_body_open` before entering `parse_block`; `else if` still follows the existing nested-if path.
- Follow-up RED WSL gcc failed only after adding `for (var item in items)\nreturn item;`, reporting `expected foreach statement missing-body-open diagnostic to carry code, problem text, and suggestion`. GREEN changes `parse_foreach_loop` to reuse `report_missing_statement_body_open` after parsing and closing the foreach header but before entering `parse_block`; the missing-opener path also frees the parsed pattern, optional type, and iterable expression.
- Follow-up RED WSL gcc failed first after adding `try\nreturn 1;`, `try { throw 1; } catch (error)\nreturn 2;`, and `try { return 1; } finally\nreturn 2;`, reporting `expected try statement missing-body-open diagnostic to carry code, problem text, and suggestion`. GREEN changes `parse_try_catch_finally_statement` to reuse `report_missing_statement_body_open` before parsing try/catch/finally blocks; catch/finally missing-opener paths free already parsed try/catch AST pieces before returning.
- Follow-up RED WSL gcc failed only after adding `%using (resource)\nreturn resource;`, reporting `expected using statement missing-body-open diagnostic to carry code, problem text, and suggestion`. GREEN changes the block-scoped `using (...)` branch in `parse_using_statement_body` to reuse `report_missing_statement_body_open` before entering `parse_block`; the missing-opener path frees the parsed resource expression before returning.
- GREEN WSL gcc focused validation passed `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_statement_parser_diagnostics_test zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test -j 8` followed by all three focused executables.
- GREEN WSL clang focused validation passed `cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_statement_parser_diagnostics_test zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test -j 8` followed by all three focused executables; clang emitted existing unrelated warning noise before the focused executables passed.
- GREEN Windows MSVC focused validation passed after importing VsDevCmd, then building `build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_statement_parser_diagnostics_test zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test` and running all three focused executables. MSVC emitted existing unrelated warning noise, but all focused tests passed.
- `parser_statements.c` is already above the large-file threshold, so this slice only added local body-opener guards inside existing control-statement parse functions and moved the new assertions into a separate focused test file. Broader statement parser decomposition remains a separate cleanup boundary.
- This slice improves parser/LSP diagnostic expressiveness for if/while/for/foreach/switch/switch-case/switch-default/else/try/catch/finally/using body opener edits only; it does not touch constructor receiver handling, SemIR, AOT/value-type lowering behavior, Debug evaluator semantics, REPL execution, or whole-repository green status.

Accepted for the parser/LSP block-close diagnostic slice:

- `tests/language_server/test_lsp_statement_parser_diagnostics.c` now validates missing block close braces through the focused `zr_vm_language_server_statement_parser_diagnostics_test` target.
- `if (ready) { return 1;` returns code `missing_block_close`, problem text `Missing closing '}' for block`, and a suggestion to insert the closing brace before continuing.
- RED WSL gcc first failed after adding the focused block-close regression: the focused target reported `expected missing-block-close diagnostic to carry code, problem text, and suggestion`.
- GREEN implementation adds `ZrParser_DiagnosticBuilder_BuildMissingBlockClose` / `report_missing_block_close` and changes `parse_block` to report the structured close diagnostic when the input ends before a closing `}` appears; the missing-close path frees the parsed statement array before returning.
- GREEN WSL gcc focused validation passed `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_statement_parser_diagnostics_test zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test -j 8` followed by all three focused executables; an earlier statement-only GREEN run also passed after the parser guard was added.
- GREEN WSL clang focused validation passed `cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_statement_parser_diagnostics_test zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test -j 8` followed by all three focused executables; clang emitted existing unrelated warning noise before the focused executables passed.
- GREEN Windows MSVC focused validation passed after importing VsDevCmd, then building `build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_statement_parser_diagnostics_test zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test` and running all three focused executables. MSVC emitted existing unrelated warning noise, but all focused tests passed.
- `parser_statements.c` remains oversized; this slice only replaced the existing generic block close fallback inside `parse_block` and did not add another parser responsibility. Broader statement parser decomposition remains a separate cleanup boundary.
- This slice improves parser/LSP diagnostic expressiveness for missing block close braces only; it does not touch constructor receiver handling, SemIR, AOT/value-type lowering behavior, Debug evaluator semantics, REPL execution, or whole-repository green status.

Accepted for the parser/LSP catch-pattern-close diagnostic slice:

- `tests/language_server/test_lsp_statement_parser_diagnostics.c` now validates malformed catch headers through the focused `zr_vm_language_server_statement_parser_diagnostics_test` target.
- `try { throw 1; } catch (error { return 2; }` returns code `missing_catch_pattern_close`, problem text `Missing closing ')' in catch pattern`, and a suggestion to insert the closing parenthesis before the catch body.
- RED WSL gcc first failed after adding the focused catch-pattern-close regression: the focused target reported `expected missing-catch-pattern-close diagnostic to carry code, problem text, and suggestion`.
- GREEN implementation adds `ZrParser_DiagnosticBuilder_BuildMissingCatchPatternClose` / `report_missing_catch_pattern_close` and changes `parse_try_catch_finally_statement` to report the structured close diagnostic when the catch pattern reaches the catch body before `)`; the missing-close path frees the parsed catch pattern, any previous catch clauses, and the parsed try block before returning.
- GREEN WSL gcc focused validation passed `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_statement_parser_diagnostics_test zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test -j 8` followed by all three focused executables; an earlier statement-only GREEN run also passed after the parser guard was added.
- GREEN WSL clang focused validation passed `cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_statement_parser_diagnostics_test zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test -j 8` followed by all three focused executables; clang emitted existing unrelated warning noise before the focused executables passed.
- GREEN Windows MSVC focused validation passed after importing VsDevCmd, then building `build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_statement_parser_diagnostics_test zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test` and running all three focused executables. MSVC emitted existing unrelated warning noise, but all focused tests passed.
- `parser_statements.c` remains oversized; this slice only replaced a local catch-header expected-token fallback inside `parse_try_catch_finally_statement` and did not add a new parser responsibility. Broader statement parser decomposition remains a separate cleanup boundary.
- This slice improves parser/LSP diagnostic expressiveness for malformed catch pattern closes only; it does not touch constructor receiver handling, SemIR, AOT/value-type lowering behavior, Debug evaluator semantics, REPL execution, or whole-repository green status.

Accepted for the parser/LSP using-resource-close diagnostic slice:

- `tests/language_server/test_lsp_statement_parser_diagnostics.c` now validates malformed block-scoped using headers through the focused `zr_vm_language_server_statement_parser_diagnostics_test` target.
- `%using (resource { return resource; }` returns code `missing_using_resource_close`, problem text `Missing closing ')' in using resource`, and a suggestion to insert the closing parenthesis before the using body.
- RED WSL gcc first failed after adding the focused using-resource-close regression: the focused target reported `expected missing-using-resource-close diagnostic to carry code, problem text, and suggestion`.
- GREEN implementation adds `ZrParser_DiagnosticBuilder_BuildMissingUsingResourceClose` / `report_missing_using_resource_close` and changes `parse_using_statement_body` to report the structured close diagnostic when the using resource reaches the using body before `)`; the missing-close path frees the parsed resource expression before returning.
- GREEN WSL gcc focused validation passed `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_statement_parser_diagnostics_test zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test -j 8` followed by all three focused executables; an earlier statement-only GREEN run also passed after the parser guard was added.
- GREEN WSL clang focused validation passed `cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_statement_parser_diagnostics_test zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test -j 8` followed by all three focused executables; clang emitted existing unrelated warning noise before the focused executables passed.
- GREEN Windows MSVC focused validation passed after importing VsDevCmd, then building `build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_statement_parser_diagnostics_test zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test` and running all three focused executables. MSVC emitted existing unrelated warning noise, but all focused tests passed.
- `parser_statements.c` remains oversized; this slice only replaced a local block-scoped using-header expected-token fallback inside `parse_using_statement_body` and did not add a new parser responsibility. Broader statement parser decomposition remains a separate cleanup boundary.
- This slice improves parser/LSP diagnostic expressiveness for malformed block-scoped using resource closes only; it does not touch constructor receiver handling, SemIR, AOT/value-type lowering behavior, Debug evaluator semantics, REPL execution, or whole-repository green status.

Accepted for the parser/LSP foreach-header-close diagnostic slice:

- `tests/language_server/test_lsp_statement_parser_diagnostics.c` now validates malformed foreach headers through the focused `zr_vm_language_server_statement_parser_diagnostics_test` target.
- `for (var item in items { return item; }` returns code `missing_foreach_header_close`, problem text `Missing closing ')' in foreach header`, and a suggestion to insert the closing parenthesis after the iterable expression before the loop body.
- RED WSL gcc first failed after adding the focused foreach-header-close regression: the focused target reported `expected missing-foreach-header-close diagnostic to carry code, problem text, and suggestion`, while existing statement parser diagnostic cases passed.
- GREEN implementation adds `ZrParser_DiagnosticBuilder_BuildMissingForeachHeaderClose` / `report_missing_foreach_header_close` and changes `parse_foreach_loop` to report the structured close diagnostic when the iterable expression reaches the loop body before `)`; the missing-close path frees the parsed pattern, optional type, and iterable expression before returning.
- GREEN WSL gcc focused validation passed `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_statement_parser_diagnostics_test zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test -j 8` followed by all three focused executables; an earlier statement-only GREEN run also passed after the parser guard was added. GCC emitted existing unrelated warning noise during the first rebuild.
- GREEN WSL clang focused validation passed `cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_statement_parser_diagnostics_test zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test -j 8` followed by all three focused executables; clang emitted existing unrelated warning noise before the focused executables passed.
- GREEN Windows MSVC focused validation passed after importing VsDevCmd, then building `build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_statement_parser_diagnostics_test zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test` and running all three focused executables. MSVC emitted existing unrelated warning noise, but all focused tests passed.
- `parser_loops.c` remains below the large-file threshold at 141 lines; `parser_statements.c` remains oversized at 1277 lines, but this slice did not add another responsibility there. Broader statement parser decomposition remains a separate cleanup boundary.
- This slice improves parser/LSP diagnostic expressiveness for malformed foreach header closes only; it does not touch constructor receiver handling, SemIR, AOT/value-type lowering behavior, Debug evaluator semantics, REPL execution, or whole-repository green status.

Accepted for the parser/LSP foreach-in-keyword diagnostic slice:

- `tests/language_server/test_lsp_statement_parser_diagnostics.c` now validates foreach headers that omit the `in` separator through the focused `zr_vm_language_server_statement_parser_diagnostics_test` target.
- `for (var item items) { return item; }` returns code `missing_foreach_in_keyword`, problem text `Missing 'in' in foreach header`, and a suggestion to insert `in` between the foreach pattern and iterable expression.
- RED WSL gcc first failed after adding the focused foreach-in-keyword regression: the focused target reported `expected missing-foreach-in-keyword diagnostic to carry code, problem text, and suggestion`, while existing statement parser diagnostic cases passed.
- GREEN implementation adds `ZrParser_DiagnosticBuilder_BuildMissingForeachInKeyword` / `report_missing_foreach_in_keyword` and changes `parse_foreach_loop` to report the structured separator diagnostic when the parsed pattern is not followed by `in`; the missing-keyword path frees the parsed pattern and optional type before returning.
- GREEN WSL gcc focused validation passed `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_statement_parser_diagnostics_test zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test -j 8` followed by all three focused executables; an earlier statement-only GREEN run also passed after the parser guard was added. GCC emitted existing unrelated warning noise during the first rebuild.
- GREEN WSL clang focused validation passed `cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_statement_parser_diagnostics_test zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test -j 8` followed by all three focused executables; clang emitted existing unrelated warning noise before the focused executables passed.
- GREEN Windows MSVC focused validation passed after importing VsDevCmd, then building `build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_statement_parser_diagnostics_test zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test` and running all three focused executables. MSVC emitted existing unrelated warning noise, but all focused tests passed.
- `parser_loops.c` remains below the large-file threshold at 148 lines; `parser_statements.c` remains oversized at 1277 lines, but this slice did not add another responsibility there. Broader statement parser decomposition remains a separate cleanup boundary.
- This slice improves parser/LSP diagnostic expressiveness for malformed foreach headers missing `in` only; it does not touch constructor receiver handling, SemIR, AOT/value-type lowering behavior, Debug evaluator semantics, REPL execution, or whole-repository green status.

Accepted for the parser/LSP switch-case-header-close diagnostic slice:

- `tests/language_server/test_lsp_statement_parser_diagnostics.c` now validates malformed ordinary switch case headers through the focused `zr_vm_language_server_statement_parser_diagnostics_test` target.
- `switch (choice) { (1 { return 1; } }` returns code `missing_switch_case_header_close`, problem text `Missing closing ')' in switch case header`, and a suggestion to insert the closing parenthesis before the case body.
- RED WSL gcc first failed after adding the focused switch-case-header-close regression: the focused target reported `expected missing-switch-case-header-close diagnostic to carry code, problem text, and suggestion`.
- GREEN implementation adds `ZrParser_DiagnosticBuilder_BuildMissingSwitchCaseHeaderClose` / `report_missing_switch_case_header_close` and changes `parse_switch_expression` to report the structured close diagnostic when a normal switch case expression reaches the case body before `)`; the missing-close path frees the parsed switch expression, case value, and case array before returning.
- GREEN WSL gcc focused validation passed `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_statement_parser_diagnostics_test zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test -j 8` followed by all three focused executables.
- GREEN WSL clang focused validation passed `cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_statement_parser_diagnostics_test zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test -j 8` followed by all three focused executables; clang emitted existing unrelated warning noise before the focused executables passed.
- GREEN Windows MSVC focused validation passed after importing VsDevCmd, then building `build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_statement_parser_diagnostics_test zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test` and running all three focused executables. MSVC emitted existing unrelated warning noise, but all focused tests passed.
- `parser_statements.c` remains oversized; this slice only replaced a local switch-case-header expected-token fallback inside `parse_switch_expression` and did not add a new parser responsibility. Broader statement parser decomposition remains a separate cleanup boundary.
- This slice improves parser/LSP diagnostic expressiveness for malformed ordinary switch case headers only; it does not touch constructor receiver handling, SemIR, AOT/value-type lowering behavior, Debug evaluator semantics, REPL execution, or whole-repository green status.

Accepted for the parser/LSP switch-body-close diagnostic slice:

- `tests/language_server/test_lsp_statement_parser_diagnostics.c` now validates missing final switch body braces through the focused `zr_vm_language_server_statement_parser_diagnostics_test` target.
- `switch (choice) { (1) { return 1; }` returns code `missing_switch_body_close`, problem text `Missing closing '}' for switch body`, and a suggestion to insert the closing brace before continuing.
- RED WSL gcc first failed after adding the focused switch-body-close regression: the focused target reported `expected missing-switch-body-close diagnostic to carry code, problem text, and suggestion`.
- GREEN implementation adds `ZrParser_DiagnosticBuilder_BuildMissingSwitchBodyClose` / `report_missing_switch_body_close` and changes `parse_switch_expression` to report the structured close diagnostic when the switch body reaches end-of-input before the final `}`; the missing-close path frees the parsed switch expression, case array, and default case before returning.
- GREEN WSL gcc focused validation passed building `build/codex-semantic-wsl-gcc-debug` for `zr_vm_language_server_statement_parser_diagnostics_test`, `zr_vm_language_server_declaration_parser_diagnostics_test`, and `zr_vm_language_server_parser_diagnostics_test`, then running all three focused executables.
- GREEN WSL clang focused validation passed building `build/codex-semantic-wsl-clang-debug` for the same three focused parser/LSP diagnostic targets, then running all three focused executables. Clang emitted existing unrelated warning noise in compiler/type-inference/LSP files.
- GREEN Windows MSVC focused validation passed after importing VsDevCmd and building `build\codex-semantic-msvc-debug --config Debug` for the same three focused parser/LSP diagnostic targets, then running all three focused executables. MSVC emitted existing unrelated warning noise in compiler/type-inference files.
- `parser_statements.c` remains oversized; this slice only replaced a local switch-body expected-token fallback inside `parse_switch_expression` and did not add a new parser responsibility. Broader statement parser decomposition remains a separate cleanup boundary.
- This slice improves parser/LSP diagnostic expressiveness for missing final switch body braces only; it does not touch constructor receiver handling, SemIR, AOT/value-type lowering behavior, Debug evaluator semantics, REPL execution, or whole-repository green status.

Accepted for the parser/LSP grouped-expression close diagnostic slice:

- `tests/language_server/test_lsp_parser_diagnostics.c` now validates missing close delimiters for grouped expressions through the focused `zr_vm_language_server_parser_diagnostics_test` target.
- `return (1 + 2` returns code `missing_group_close`, problem text `Missing closing ')' in grouped expression`, and a suggestion to insert the closing parenthesis after the grouped expression.
- RED WSL gcc first failed after adding the focused regression: the old parser path did not emit `missing_group_close` and the focused target reported `expected missing-group-close diagnostic to carry code, problem text, and suggestion`.
- GREEN implementation adds the builder/reporting path and changes the non-lambda `(` primary-expression branch to report the structured close diagnostic before returning `NULL`, while keeping function calls on `missing_call_close`.
- GREEN WSL gcc focused validation passed `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN WSL clang focused validation passed `cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test`; the build emitted existing warning noise before the focused executable reported all parser diagnostic tests completed.
- GREEN Windows MSVC focused validation passed after importing VsDevCmd and verifying `cl.exe` at MSVC 14.44.35207, then building `build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_parser_diagnostics_test` and running `build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe`.
- This slice improves parser/LSP diagnostic expressiveness for grouped-expression edits only; it does not touch core runtime, AOT/value-type lowering, Debug evaluator semantics, REPL execution, or whole-repository green status.

Accepted for the parser/LSP function parameter-list close diagnostic slice:

- `tests/language_server/test_lsp_parser_diagnostics.c` now validates missing close delimiters for top-level `func name(...)` declaration parameter lists through the focused `zr_vm_language_server_parser_diagnostics_test` target.
- `func pick(value: int: int { return value; }` returns code `missing_parameter_list_close`, problem text `Missing closing ')' in function declaration parameters`, and a suggestion to insert `)` after the parameter list before continuing.
- RED WSL gcc first failed after adding the focused parameter-list regression: the focused target reported `expected missing-parameter-list-close diagnostic to carry code, problem text, and suggestion`, proving the old path still fell back before exposing a declaration-parameter-list-specific diagnostic.
- First GREEN attempt still failed only this new case. A WSL gdb trace then showed `report_missing_parameter_list_close` was called from the quiet `try_parse_prefixed_function_declaration` probe with structured callbacks suppressed, so the diagnostic was discarded before the fallback parse path continued.
- GREEN implementation adds the builder/reporting path, changes `parse_function_declaration` to report the missing parameter-list close before return-type/body parsing, and changes statement dispatch so explicit `func name...` declarations that fail the quiet probe are reparsed once with diagnostics enabled. Function calls remain on `missing_call_close`, and grouped expressions remain on `missing_group_close`.
- GREEN WSL gcc focused validation passed `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN WSL clang focused validation passed `cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test`; clang still emitted existing unrelated warning noise before the focused executable reported all parser diagnostic tests completed.
- GREEN Windows MSVC focused validation passed after importing VsDevCmd, verifying `cl.exe` at MSVC 14.44.35207, then building `build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_parser_diagnostics_test` and running `build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe`; MSVC still emitted existing unrelated warning noise.
- `parser_statements.c` is already above the large-file threshold. This slice made a narrow correction inside the existing function-declaration dispatch branch instead of splitting parser orchestration; the follow-up boundary is to extract declaration-start/probe handling out of `parser_statements.c` when the next broader parser-dispatch cleanup is in scope.
- This slice improves parser/LSP diagnostic expressiveness for top-level function declaration parameter-list edits only; it does not touch methods/interfaces/struct/extern parameter-list families, core runtime, AOT/value-type lowering behavior, Debug evaluator semantics, REPL execution, or whole-repository green status.

Accepted for the parser/LSP class method parameter-list close diagnostic slice:

- `tests/language_server/test_lsp_parser_diagnostics.c` now validates missing close delimiters for class method declaration parameter lists through the focused `zr_vm_language_server_parser_diagnostics_test` target.
- `class Box { func read(value: int: int { return value; } }` returns code `missing_parameter_list_close`, problem text `Missing closing ')' in function declaration parameters`, and a suggestion to insert `)` after the parameter list before continuing.
- RED WSL gcc first failed after adding the focused method-parameter regression: the focused target reported `expected missing-method-parameter-list-close diagnostic to carry code, problem text, and suggestion`, while the existing top-level function, call, group, object, array, conditional, and statement-semicolon parser diagnostic cases still passed.
- GREEN implementation changes `parse_class_method` so a method parameter list missing `)` reports the structured close diagnostic before return-type/body parsing, while keeping function calls on `missing_call_close` and grouped expressions on `missing_group_close`.
- GREEN WSL gcc focused validation passed `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN WSL clang focused validation passed `cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN Windows MSVC focused validation passed after importing VsDevCmd, verifying `cl.exe` at MSVC 14.44.35207, then building `build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_parser_diagnostics_test` and running `build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe`; MSVC still emitted existing unrelated AOT runtime warning noise.
- `parser_class.c` remains below the large-file threshold after this narrow method-parser change.
- This slice improves parser/LSP diagnostic expressiveness for class method parameter-list edits only; it does not touch constructors/interfaces/struct/extern parameter-list families, core runtime, AOT/value-type lowering behavior, Debug evaluator semantics, REPL execution, or whole-repository green status.

Accepted for the parser/LSP interface method parameter-list close diagnostic slice:

- `tests/language_server/test_lsp_parser_diagnostics.c` now validates missing close delimiters for interface method signature parameter lists through the focused `zr_vm_language_server_parser_diagnostics_test` target.
- `interface Readable { read(value: int: int; }` returns code `missing_parameter_list_close`, problem text `Missing closing ')' in function declaration parameters`, and a suggestion to insert `)` after the parameter list before continuing.
- RED WSL gcc first failed after adding the focused interface-method-parameter regression: the focused target reported `expected missing-interface-method-parameter-list-close diagnostic to carry code, problem text, and suggestion`, while the existing top-level function, class method, call, group, object, array, conditional, and statement-semicolon parser diagnostic cases still passed.
- GREEN implementation changes `parse_interface_method_signature` so an interface method signature parameter list missing `)` reports the structured close diagnostic before return-type/where/semicolon parsing, while keeping function calls on `missing_call_close` and grouped expressions on `missing_group_close`.
- GREEN WSL gcc focused validation passed `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN WSL clang focused validation passed `cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN Windows MSVC focused validation passed after importing VsDevCmd, verifying `cl.exe` at MSVC 14.44.35207, then building `build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_parser_diagnostics_test` and running `build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe`.
- `parser_interface.c` remains below the large-file threshold after this narrow method-signature parser change.
- This slice improves parser/LSP diagnostic expressiveness for interface method signature parameter-list edits only; it does not touch interface meta signatures, constructors, struct/extern parameter-list families, core runtime, AOT/value-type lowering behavior, Debug evaluator semantics, REPL execution, or whole-repository green status.

Accepted for the parser/LSP interface meta parameter-list close diagnostic slice:

- `tests/language_server/test_lsp_parser_diagnostics.c` now validates missing close delimiters for interface meta signature parameter lists through the focused `zr_vm_language_server_parser_diagnostics_test` target.
- `interface Callable { @call(value: int: int; }` returns code `missing_parameter_list_close`, problem text `Missing closing ')' in function declaration parameters`, and a suggestion to insert `)` after the parameter list before continuing.
- RED WSL gcc first failed after adding the focused interface-meta-parameter regression: the focused target reported `expected missing-interface-meta-parameter-list-close diagnostic to carry code, problem text, and suggestion`, while the existing top-level function, class method, interface method, call, group, object, array, conditional, and statement-semicolon parser diagnostic cases still passed.
- GREEN implementation changes `parse_interface_meta_signature` so an interface meta signature parameter list missing `)` reports the structured close diagnostic before return-type/semicolon parsing, while keeping function calls on `missing_call_close` and grouped expressions on `missing_group_close`.
- GREEN WSL gcc focused validation passed `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN WSL clang focused validation passed `cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN Windows MSVC focused validation passed after importing VsDevCmd, verifying `cl.exe` at MSVC 14.44.35207, then building `build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_parser_diagnostics_test` and running `build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe`.
- `parser_interface.c` remains below the large-file threshold after this narrow meta-signature parser change.
- This slice improves parser/LSP diagnostic expressiveness for interface meta signature parameter-list edits only; it does not touch class/struct meta signatures, constructors, struct/extern parameter-list families, core runtime, AOT/value-type lowering behavior, Debug evaluator semantics, REPL execution, or whole-repository green status.

Accepted for the parser/LSP extern function parameter-list close diagnostic slice:

- `tests/language_server/test_lsp_parser_diagnostics.c` now validates missing close delimiters for extern function declaration parameter lists through the focused `zr_vm_language_server_parser_diagnostics_test` target.
- `%extern("fixture") { NativeAdd(value: int: int; }` returns code `missing_parameter_list_close`, problem text `Missing closing ')' in function declaration parameters`, and a suggestion to insert `)` after the parameter list before continuing.
- RED WSL gcc first failed after adding the focused extern-function-parameter regression: the focused target reported `expected missing-extern-function-parameter-list-close diagnostic to carry code, problem text, and suggestion`, while the existing top-level function, class method, interface method/meta, call, group, object, array, conditional, and statement-semicolon parser diagnostic cases still passed.
- GREEN implementation changes `parse_extern_function_declaration` so an extern function parameter list missing `)` reports the structured close diagnostic before return-type/semicolon parsing, while keeping function calls on `missing_call_close` and grouped expressions on `missing_group_close`.
- GREEN WSL gcc focused validation passed `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN WSL clang focused validation passed `cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN Windows MSVC focused validation passed after importing VsDevCmd, verifying `cl.exe` at MSVC 14.44.35207, then building `build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_parser_diagnostics_test` and running `build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe`.
- `parser_extern.c` is near but below the large-file threshold; this slice made a narrow branch-local correction and leaves broader extern parser extraction for a later cleanup boundary.
- This slice improves parser/LSP diagnostic expressiveness for extern function parameter-list edits only; it does not touch extern delegates, extern block spec parentheses, test declarations, intermediate declarations, core runtime, AOT/value-type lowering behavior, Debug evaluator semantics, REPL execution, or whole-repository green status.

Accepted for the parser/LSP extern delegate parameter-list close diagnostic slice:

- `tests/language_server/test_lsp_parser_diagnostics.c` now validates missing close delimiters for extern delegate declaration parameter lists through the focused parser diagnostics harness.
- `%extern("fixture") { delegate Callback(value: int: int; }` returns code `missing_parameter_list_close`, problem text `Missing closing ')' in function declaration parameters`, and a suggestion to insert `)` after the parameter list before continuing.
- RED WSL gcc first failed after adding the focused extern-delegate-parameter regression: a normal target build was initially blocked before test execution by current dirty-tree AOT backend signature mismatches, so the RED proof compiled only the updated test harness against the previously built parser/LSP shared libraries and reported `expected missing-extern-delegate-parameter-list-close diagnostic to carry code, problem text, and suggestion`; existing parser diagnostic cases still passed.
- GREEN implementation extracts local extern parser close recovery into `consume_extern_parameter_list_close_or_report`, so extern function and extern delegate declarations share the structured parameter-list close diagnostic before return-type/semicolon parsing, while keeping function calls on `missing_call_close` and grouped expressions on `missing_group_close`.
- GREEN WSL gcc focused validation first passed by rebuilding `parser_extern.c.o` plus `libzr_vm_parser.so`, then compiling/running the focused parser diagnostics harness manually against `build/codex-semantic-wsl-gcc-debug/lib`; after the active AOT backend files compiled cleanly enough for the parser shared target, the normal `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` plus `./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test` path also passed.
- GREEN WSL clang focused validation passed `cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN Windows MSVC focused validation passed after importing VsDevCmd, verifying `cl.exe` at MSVC 14.44.35207, then building `build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_parser_diagnostics_test` and running `build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe`.
- `parser_extern.c` is already above the large-file threshold at this point; this slice avoided another copied branch by consolidating the extern declaration close recovery, and leaves broader extern parser responsibility extraction as the next cleanup boundary.
- This slice improves parser/LSP diagnostic expressiveness for extern delegate parameter-list edits only; it does not touch extern block spec parentheses, test declarations, intermediate declarations, core runtime, AOT/value-type lowering behavior, Debug evaluator semantics, REPL execution, or whole-repository green status.

Accepted for the parser/LSP computed object-key close diagnostic slice:

- `tests/language_server/test_lsp_parser_diagnostics.c` now validates missing close delimiters for computed object literal keys through the focused `zr_vm_language_server_parser_diagnostics_test` target.
- `return {[seed: 1}` returns code `missing_object_computed_key_close`, problem text `Missing closing ']' in computed object key`, and a suggestion to insert the closing bracket before `:`.
- RED WSL gcc first failed after adding the focused regression: the old parser path did not emit `missing_object_computed_key_close` and the focused target reported `expected missing-object-computed-key-close diagnostic to carry code, problem text, and suggestion`.
- GREEN implementation adds the builder/reporting path and changes both first-property and subsequent-property computed-key parse branches to report the structured close diagnostic before returning `NULL`, while keeping computed member/index access on `missing_index_close`.
- GREEN WSL gcc focused validation passed after refreshing the existing semantic build graph so active AOT source globs included the current untracked value-SemIR call file, then running `cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN WSL clang focused validation passed after refreshing `build/codex-semantic-wsl-clang-debug`, then running `cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8` followed by `./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test`.
- GREEN Windows MSVC focused validation passed after importing VsDevCmd, verifying `cl.exe` at MSVC 14.44.35207, refreshing `build\codex-semantic-msvc-debug`, then building `--target zr_vm_language_server_parser_diagnostics_test` and running `build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe`.
- This slice improves parser/LSP diagnostic expressiveness for computed object-key edits only; it does not touch core runtime, AOT/value-type lowering behavior, Debug evaluator semantics, REPL execution, or whole-repository green status.

Accepted for the LSP signature-help non-code cursor filter slice:

- `tests/language_server/test_lsp_interface.c` now validates `textDocument/signatureHelp` returns no help when the cursor is inside `/* pick(99) is prose */` embedded in a real call argument list, while a later real `pick(result)` argument still returns `pick(value: int): int`.
- RED WSL gcc first failed with `pick(value: int): int` for the comment cursor position, proving AST call-context matching alone treated comment text inside the call range as an active signature-help location.
- GREEN implementation updates `lsp_signature_help.c` to call `ZrLanguageServer_Lsp_IsCursorOffsetInCodeSpan` before AST call-context matching, reusing `lsp_source_spans.c` instead of adding another scanner.
- GREEN WSL gcc, WSL clang, and Windows MSVC focused validation passed `zr_vm_language_server_lsp_interface_test` in `build/codex-semantic-wsl-gcc-debug`, `build/codex-semantic-wsl-clang-debug`, and `build\codex-semantic-msvc-debug`, with no `Fail -` markers in the test output. This slice does not claim parser-token-stream replacement, broader signature-help UI redesign, or whole-repository green.

Accepted for the LSP receiver/member non-code definition filter slice:

- `tests/language_server/test_lsp_advanced_editor_features.c` now validates `textDocument/definition` returns no locations for `box.value` inside a line comment or string literal, while a real `return box.value;` still resolves through the existing receiver/member definition path.
- RED WSL gcc first failed with `definition resolved receiver-member text from a non-code span`, proving the raw receiver/member semantic query fallback could parse comment/string text and resolve it against the surrounding function scope.
- GREEN implementation updates `lsp_semantic_query.c` so the receiver/member fallback checks the live source span with the shared `lsp_source_spans.c` helper before calling the project/native member resolver.
- GREEN WSL gcc, WSL clang, and Windows MSVC focused validation passed `zr_vm_language_server_lsp_advanced_editor_features_test` in `build/codex-semantic-wsl-gcc-debug`, `build/codex-semantic-wsl-clang-debug`, and `build\codex-semantic-msvc-debug`, with no `Fail -` markers in the test output. This slice does not claim parser-token-stream replacement, broader member-resolution redesign, or whole-repository green.

Accepted for the LSP call-hierarchy non-code call mention filter slice:

- `tests/language_server/test_lsp_advanced_editor_features.c` now validates outgoing calls from `run` and incoming calls to `helper` ignore `helper(value)` text inside a line comment or string literal, while the existing real direct-call tests still cover normal call hierarchy behavior.
- RED WSL gcc first failed with `call hierarchy counted comment or string text as a call`, proving the incoming/outgoing raw call scans could count call-looking prose as a call edge.
- GREEN implementation updates `lsp_hierarchy.c` so both incoming and outgoing raw call scans call `lsp_editor_offset_is_code` before accepting a `name(...)` candidate.
- GREEN WSL gcc, WSL clang, and Windows MSVC focused validation passed `zr_vm_language_server_lsp_advanced_editor_features_test` in `build/codex-semantic-wsl-gcc-debug`, `build/codex-semantic-wsl-clang-debug`, and `build\codex-semantic-msvc-debug`, with no `Fail -` markers in the test output. This slice does not claim parser-backed call graph resolution or whole-repository green.

Accepted for the LSP CodeLens non-code `%test(...)` filter slice:

- `tests/language_server/test_lsp_advanced_editor_features.c` now validates CodeLens does not expose a `zr.runCurrentProject` command for `%test(...)` text inside a line comment, block comment, or string literal, while the existing real `%test("advanced")` test still covers normal run-lens behavior.
- RED WSL gcc first failed with `non-code %test text produced a run CodeLens`, proving the raw `%test(` marker scan could turn prose into a runnable command.
- GREEN implementation updates `lsp_editor_features.c` so the run-lens marker scan calls `lsp_editor_offset_is_code` before accepting a `%test(` candidate. No new CodeLens responsibility was added to the already-large editor-features file; future broader CodeLens work should extract a focused module.
- GREEN WSL gcc, WSL clang, and Windows MSVC focused validation passed `zr_vm_language_server_lsp_advanced_editor_features_test` in `build/codex-semantic-wsl-gcc-debug`, `build/codex-semantic-wsl-clang-debug`, and `build\codex-semantic-msvc-debug`, with no `Fail -` markers in the test output. Clang/MSVC reconfigured because of dirty-tree glob mismatch before passing. This slice does not claim parser-backed test discovery or whole-repository green.

Accepted for the parser/LSP traditional `for` header variable-initializer separator diagnostic slice:

- `tests/language_server/test_lsp_statement_parser_diagnostics.c` now validates `missing_for_header_separator` for expression initializer, condition, and variable initializer clauses through the focused `zr_vm_language_server_statement_parser_diagnostics_test` target.
- `for (var i = 0 i < 3; i = i + 1) { out i; }` returns code `missing_for_header_separator`, problem text `Missing ';' between for header clauses`, and a suggestion to insert `;` between the traditional `for` header clauses.
- RED WSL gcc first failed after adding the variable-initializer regression in `build-wsl-gcc`: the focused statement diagnostics target reported `expected missing-for-header-separator diagnostic after variable initializer clause`, while the existing statement diagnostic cases still passed.
- Root cause evidence: GDB breakpoints on `report_missing_for_header_separator` showed only the two existing expression-initializer/condition separator cases fired. Reading `parse_top_level_statement` showed the old top-level `for` dispatcher treated every `for (var ...)` as foreach, so the malformed traditional `for (var i = 0 ...)` path never reached `parse_for_loop`.
- GREEN implementation adds shared `parser_for_header_should_parse_foreach` dispatch logic for both block and top-level statements. The dispatcher now treats `in` as foreach, `=` or `;` as traditional `for`, and malformed `for (var pattern ...)` headers with no `in/=/;` as foreach so the existing `missing_foreach_in_keyword` diagnostic remains intact.
- GREEN implementation also adds `parse_variable_declaration_for_header`, so traditional `for` variable initializers do not emit a normal variable-declaration semicolon diagnostic before `parse_for_loop` can report `missing_for_header_separator`.
- GREEN WSL gcc focused validation passed in `build-wsl-gcc`: `cmake --build build-wsl-gcc --target zr_vm_language_server_statement_parser_diagnostics_test zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test -j 8`, followed by the three direct test binaries, all completed successfully.
- GREEN WSL clang focused validation passed in `build/codex-semantic-wsl-clang-debug`: the same three targets built and `zr_vm_language_server_statement_parser_diagnostics_test`, `zr_vm_language_server_declaration_parser_diagnostics_test`, and `zr_vm_language_server_parser_diagnostics_test` all completed successfully. Clang still reports an existing unused-function warning in `parser_types.c`.
- GREEN Windows MSVC focused validation passed in `build\codex-semantic-msvc-debug` after importing VsDevCmd and verifying `cl.exe` at `E:\Visual Studio\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe`; the same three targets built and all three Debug test executables completed successfully.
- `parser_statements.c` remains oversized. This slice removed duplicated manual lexer-state restoration from both `for` dispatch branches and replaced it with one local helper, but broader statement parser decomposition remains a separate cleanup boundary.
- This slice improves parser/LSP diagnostic expressiveness for traditional `for` header separator edits involving `var` initializers only; it does not touch constructor receiver handling, SemIR, AOT/value-type lowering behavior, Debug evaluator semantics, REPL execution, or whole-repository green status.

Remaining risks:

- Expression fact emission is now centralized for successful `ZrParser_ExpressionType_Infer` paths, including logical expressions, but per-kind semantic payloads such as call targets, member chains, unsigned/non-exact numeric ranges, conversion provenance, and interprocedural range propagation still need expansion.
- Member reference facts currently classify dot/computed member reads and member/index assignment target tokens, but they remain unresolved classification facts; resolving member tokens to declarations remains follow-up work.
- Debug data inference and full expression-language parity are not yet complete; current Debug slices include runtime `semanticSummary` / `referenceSummary` metadata, partial parser-backed expression/numeric/logical/reachability/reference summary reuse for successful `evaluate`, limited paused-frame local/argument, stable Debug global read-reference replay, compiled entry top-level callable call-reference replay, and safe evaluator diagnostic/logical/conditional-expression improvements, not full parser fact parity.
- REPL supports fresh bare expression execution including aggregate-start expressions such as `[1 + 2][0]`, array result display for simple first-level arrays such as `[1 + 2] -> [3]`, and line-start object literal expressions such as `{a: 1 + 2}.a` / `{[1 + 2]: 4}[3]`, plus shared `:type` display for current expression type, root and nested expression kind/exactness/constants, root and nested numeric/ownership facts, logical/reachability facts, conditional-branch nested logical/reachability facts, call/member payloads, unresolved dot/computed member-access and member-write reference classifications, with string constants escaped for quotes, backslashes, and common control bytes; it does not yet provide persistent interactive scope, complete local-query parity with LSP, arbitrary nested/cyclic array formatting guarantees, full block/object ambiguity resolution, or complete expression-language parity.
- LSP completion fact details currently cover local-variable initializer expression/numeric/logical/ownership facts, signature-help parameter docs cover argument expression/numeric/logical/ownership facts, and stdio inline values cover object aggregate facts where parser/type inference already exposes a stable local fact. Local hover, rich hover, local-variable completion detail, inline value, and signature-help parameter docs now surface expression kind/exactness/constants where covered, but full parser block/object ambiguity resolution, broader completion resolve UI, and richer expression fact roles such as conversion provenance, call chains, and member-chain resolution remain follow-up work.
- Debug and REPL are not yet fully connected to the parser shared local query layer for compile-time semantic fact reuse; the current Debug summary bridge and REPL reachability slices are direct semantic-context consumers, not full LSP local-query parity.
- Full repository green is still not claimed; this slice used focused WSL gcc direct relinks where the normal dirty GCC target path could be pulled into unrelated active core/value-type rebuild state.
- The active inline-struct/value-type runtime work still has an ownership normalization assertion in `zr_vm_value_type_runtime_test`; it must be resolved by the runtime slice before whole-repository acceptance.
