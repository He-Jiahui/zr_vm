# Semantic Inference Fact Layer

## Scope

- First-stage shared semantic fact, diagnostic, and local LSP query layer.
- Affected layers: parser semantic context, parser diagnostics, type inference fact emission, language-server semantic analysis, LSP hover/local query, stdio diagnostic JSON, tests, and parser/semantic docs.
- Debug and REPL are now partial consumers of the same diagnostic/expression direction; full shared fact reuse remains follow-up work.
- Second-stage Debug expression diagnostics now cover missing right operands for evaluate and conditional-breakpoint expressions, numeric semantic failures such as non-numeric operands and division by zero, composed comparison/logical evaluation for Debug conditions, and local member/index syntax failures such as missing member names or missing `]`.
- Third-stage REPL expression execution now covers fresh bare expression submissions and parser-backed missing-right-operand diagnostics. REPL `:type <expression>` now covers the first shared parser/type-inference fact consumption path for expression type, numeric/logical/reachability/ownership facts, call/member expression payload display, member-access reference display, and member-write reference display. This acceptance record still does not claim full Debug expression inference, full Debug data inference, persistent REPL scope, or full REPL/LSP shared local-query parity are complete.
- LSP local expression queries now expose numeric facts, logical short-circuit facts, unary logical-not facts, constant numeric-comparison logical facts, and composed comparison logical facts for operator-position queries; this acceptance record still does not claim full VSCode UI integration or complete float/unsigned/variable range propagation.
- LSP local expression queries now also return ownership violation facts at the relevant ownership builtin expression, including `%loan(...)` return-escape diagnostics.
- LSP local hover and rich hover now surface reachability causes for unreachable code, for example `unreachable after return`, instead of reducing reachability to a bare boolean state.
- LSP local hover and rich hover now also surface constant-false loop body causes, for example `unreachable because the condition is false` inside `while (constFalse)`.
- LSP local hover and rich hover now also surface call/member expression payloads when local query hits an existing parser expression fact payload, for example `Call: pick args=1` and `Member: value`. This acceptance record still does not claim call execution, member-chain type resolution, or UI synthesis for expressions without shared payload facts.
- Parser/type-inference reference facts now distinguish assignment writes from identifier reads for known local targets: `seed = 3;` records a `ZR_SEMANTIC_REFERENCE_WRITE` on the left `seed` token while preserving left expression/numeric facts.
- LSP local hover and rich hover now expose the same assignment write-reference fact for identifier assignment targets: hovering the left `seed` in `seed = 3;` shows `Reference: write`, symbol name, and declaration location.
- Parser/type-inference reference facts now also classify ordinary dot-member reads, computed member reads, and member/index assignment targets: `seed.value;` records unresolved `ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS` on `value`, `seed[index];` records unresolved `ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS` on the full member expression while preserving the `index` token's variable read fact, `seed.value = 3;` records unresolved `ZR_SEMANTIC_REFERENCE_MEMBER_WRITE` on `value`, and `seed[index] = 4;` records the write kind on the computed index expression. This does not claim member declaration resolution.
- LSP local query and REPL `:type` now surface ordinary and computed member-access reference facts for property reads; LSP local query, hover, rich hover, and REPL `:type` surface member-write reference facts for property assignment targets as `Reference: member write` with the member token name.
- Exact integer variable range propagation now covers the first local constant slice: `var seed = 2; seed + 3;` produces an exact `5..5` numeric fact in parser/type inference and through LSP local query. This acceptance record still does not claim unsigned or non-exact interval propagation.
- LSP inlay hints now consume initializer semantic facts for existing unannotated-local type hints, so `var sum = 1 + 2;` can surface `: int, range 3..3` without adding expression-level hint noise.
- LSP completion item details now consume initializer semantic facts for local variables through `lsp_completion_semantic_facts.c`, so completions can surface numeric ranges and deterministic logical facts without duplicating parser/type-inference rules. This acceptance record still does not claim full completion resolve UI design, signature-help fact rendering, or non-local/interprocedural completion facts.
- LSP stdio inline values now consume numeric/logical/reference facts for local initializers, simple single-line `return` expressions, literal/boolean expression statements, identifier-led arithmetic expression statements, unary `!`/`-` expression statements, call/member expression statements that start at the source line after indentation when shared expression/reference payload facts exist, and simple operator-split expression statements when the request range starts on the continuation line. This acceptance record still does not claim runtime debugger value display, source-synthesized call/member inference, or arbitrary multi-line expression range support.

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
  - `tests/cli/repl_type_ownership_smoke.js`
  - `tests/cli/repl_type_call_member_smoke.js`
  - `tests/cli/repl_type_logical_comparison_smoke.js`
  - `tests/cli/repl_type_reachability_smoke.js`
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
  - stdio `textDocument/inlineValue` for a request range starting on the continuation line of `1 +\n 2;` adds fact-backed inline text on the full expression range with `range 3..3`.
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
  - Debug evaluate `"text" + 1` reports a numeric operand type error with concrete cause and suggestion.
  - Debug evaluate `1 / 0` reports division by zero with concrete cause and suggestion.
  - `loan_escape` ownership diagnostic emits concrete message, cause, suggestion, and ownership fact for `return %loan(resource);`.
  - Source compile rejects parser-reported expression errors instead of letting recovered AST reach runtime execution.
  - LSP parser diagnostics for `1 +` expose `missing_right_operand`, concrete problem text, and suggestion.
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

Accepted for the Debug conditional-expression safe-evaluate slice:

- `tests/debug/test_debug_expression_diagnostics.c` now validates `?:` in paused-frame safe evaluate / conditional-breakpoint expressions.
- `true ? 1 : missingLocal` returns the selected true branch without resolving `missingLocal`; `false ? missingLocal : 2` does the symmetric false-branch case.
- The skipped branch is still parsed for syntax, preserving the safe-evaluate grammar contract while avoiding data lookup and calculations on unreachable branch paths.
- RED WSL gcc focused target first failed after adding the regression: `Expected TRUE Was FALSE` in `test_debug_condition_evaluates_selected_ternary_branch_without_resolving_skipped_branch`.
- GREEN WSL gcc and WSL clang focused validation passed `zr_vm_debug_expression_diagnostics_test` with 16 tests/0 failures; Windows MSVC focused validation also passed the same 16 tests/0 failures.
- The Debug safe-evaluate helper split moved diagnostic/right-operand support into `debug_eval_diagnostics.c` and `debug_eval_internal.h`, reducing `debug_eval.c` to 845 lines. This slice still does not claim complete language expression parity, shared parser fact consumption inside Debug, or whole-repository green.

Accepted for the Debug variable/evaluate child-shape metadata slice:

- `tests/debug/test_debug_variable_child_shape.c` validates per-value `namedVariables` / `indexedVariables` in `variables` responses and the same metadata in `evaluate("zr")`.
- `debug_child_shape.c` centralizes the runtime snapshot child-shape rules: visible object fields plus synthetic entries such as `$prototype` count as named children, direct arrays/index windows count indexed children, and hidden `__zr_` fields stay excluded.
- `debug_protocol.c` now serializes child-shape metadata on value previews, stack argument previews, and evaluate results so a future VS Code/DAP adapter can identify expandable named/indexed children without probing every handle first.
- RED WSL gcc focused protocol test first failed with `Expected 36 Was 0`, proving the aggregate expansion count existed while the per-value field was absent.
- GREEN WSL gcc and WSL clang focused validation passed `zr_vm_debug_variable_child_shape_test` with 1 test/0 failures; Windows MSVC focused validation also passed the same test.
- This is runtime Debug snapshot metadata, not parser semantic fact reuse; complete Debug reference display and whole-repository green remain outside this slice.

Remaining risks:

- Expression fact emission is now centralized for successful `ZrParser_ExpressionType_Infer` paths, including logical expressions, but per-kind semantic payloads such as call targets, member chains, unsigned/non-exact numeric ranges, conversion provenance, and interprocedural range propagation still need expansion.
- Member reference facts currently classify dot/computed member reads and member/index assignment target tokens, but they remain unresolved classification facts; resolving member tokens to declarations remains follow-up work.
- Debug data inference and full expression-language parity are not yet complete; current Debug slices are safe evaluator diagnostic/logical/conditional-expression improvements, not full parser fact reuse.
- REPL supports fresh bare expression execution and shared `:type` display for current expression type, numeric/logical/reachability/ownership facts, call/member payloads, unresolved dot/computed member-access and member-write reference classifications, but not yet persistent interactive scope, complete local-query parity with LSP, or complete expression-language parity.
- LSP completion fact details currently cover initializer numeric/logical facts for local variable completions only; broader completion/signature consumers still need to be connected to the shared fact layer.
- Debug and REPL are not yet fully connected to the parser shared local query layer for compile-time semantic fact reuse; the new REPL reachability slice is a direct semantic-context consumer, not full LSP local-query parity.
- Full repository green is still not claimed; this slice used focused WSL gcc direct relinks where the normal dirty GCC target path could be pulled into unrelated active core/value-type rebuild state.
- The active inline-struct/value-type runtime work still has an ownership normalization assertion in `zr_vm_value_type_runtime_test`; it must be resolved by the runtime slice before whole-repository acceptance.
