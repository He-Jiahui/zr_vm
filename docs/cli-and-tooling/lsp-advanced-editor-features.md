---
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/conf.h
  - zr_vm_language_server/include/zr_vm_language_server/lsp_interface.h
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_source_spans.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_local_semantic_query.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_super_navigation.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_document_links.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_editor_features.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_code_actions.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_folding_ranges.c
  - zr_vm_language_server/stdio/stdio_editor_features.c
  - zr_vm_language_server/stdio/stdio_linked_editing.c
  - zr_vm_language_server/stdio/stdio_moniker.c
  - zr_vm_language_server/stdio/stdio_document_color.c
  - zr_vm_language_server/stdio/stdio_inline_value.c
  - zr_vm_language_server/stdio/stdio_inline_value_scan.h
  - zr_vm_language_server/stdio/stdio_inline_value_scan.c
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio_internal.h
  - zr_vm_language_server/CMakeLists.txt
  - zr_vm_parser/include/zr_vm_parser/diagnostic_builder.h
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_class.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_declarations.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_extern.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_interface.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_source_spans.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_local_semantic_query.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_super_navigation.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_document_links.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_editor_features.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_code_actions.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_folding_ranges.c
  - zr_vm_language_server/stdio/stdio_editor_features.c
  - zr_vm_language_server/stdio/stdio_linked_editing.c
  - zr_vm_language_server/stdio/stdio_moniker.c
  - zr_vm_language_server/stdio/stdio_document_color.c
  - zr_vm_language_server/stdio/stdio_inline_value.c
  - zr_vm_language_server/stdio/stdio_inline_value_scan.h
  - zr_vm_language_server/stdio/stdio_inline_value_scan.c
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_language_server/include/zr_vm_language_server/lsp_interface.h
  - zr_vm_language_server/include/zr_vm_language_server/conf.h
  - zr_vm_parser/include/zr_vm_parser/diagnostic_builder.h
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_class.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_declarations.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_extern.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_interface.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
plan_sources:
  - user: 2026-04-24 Zr LSP 现代能力对齐计划
tests:
  - tests/language_server/test_lsp_advanced_editor_features.c
  - tests/language_server/test_lsp_parser_diagnostics.c
  - tests/language_server/test_lsp_declaration_parser_diagnostics.c
  - tests/language_server/test_lsp_statement_parser_diagnostics.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_local_semantic_hover.c
  - tests/language_server/test_lsp_computed_member_hover.c
  - tests/language_server/stdio_smoke.js
  - tests/language_server/stdio_inline_value_semantic_smoke.js
  - tests/CMakeLists.txt
doc_type: module-detail
---

# LSP Advanced Editor Features

## 范围

这份文档记录 Zr language server 第一阶段现代 LSP 能力对齐：补齐编辑器高频交互能力，同时保持语义事实仍来自现有 `lsp_semantic_query`、project index、metadata provider 和 incremental parser。

本阶段实现的协议能力包括：

- `textDocument/codeAction`
- `textDocument/formatting`
- `textDocument/rangeFormatting`
- `textDocument/onTypeFormatting`
- `textDocument/foldingRange`
- `textDocument/selectionRange`
- `textDocument/linkedEditingRange`
- `textDocument/moniker`
- `textDocument/documentLink`
- `textDocument/declaration`
- `textDocument/typeDefinition`
- `textDocument/implementation`
- `textDocument/codeLens`
- `textDocument/prepareCallHierarchy`
- `callHierarchy/incomingCalls`
- `callHierarchy/outgoingCalls`
- `textDocument/prepareTypeHierarchy`
- `typeHierarchy/supertypes`
- `typeHierarchy/subtypes`
- `textDocument/diagnostic`
- `workspace/diagnostic`
- `completionItem/resolve`
- `textDocument/inlineValue`

## 设计边界

`lsp_editor_features.c` / `lsp_code_actions.c` 是新的公共接口实现文件，避免继续扩大 `lsp_interface.c`。它们只负责编辑器形态的结果建模：

- formatting 生成 `SZrLspTextEdit`，当前使用基于 brace/block 的保守缩进。
- code action 使用 `SZrLspCodeAction`，第一阶段提供 `source.organizeImports` 和缺失分号 `quickfix`，返回最小 `TextEdit`；缺失分号 quickfix 会先确认 trimmed statement start 位于 code span，避免 block comment 正文中的 `return ...` 文本触发编辑。
- folding / selection range 从当前文档文本结构生成轻量结构范围，包含 block、连续 import/comment region，以及 word -> line -> block selection chain。
- document link 扫描 `%import("...")` 字面量、`.zrp` 的 `source` / `binary` / `entry` 路径，以及 native virtual declaration 里的 module link；import 优先复用 definition 查询结果作为跳转目标。
- declaration / typeDefinition / implementation 当前复用 definition 查询，保证和已有导航结果一致。
- code lens 当前为函数/类型提供引用计数，并为 code span 中的 `%test(...)` 提供可运行入口；comment/string 中的 `%test(...)` 文本不会生成 run CodeLens。
- call hierarchy 当前实现 prepare 和同文件直接 incoming/outgoing calls；incoming/outgoing 的 raw call scan 会先过滤 line comment、block comment 和 string literal 中的 call-looking 文本，例如 `helper(value)`；type hierarchy 当前返回同文件直接继承/派生关系。
- pull diagnostics 复用现有 diagnostics 生成逻辑，不替代 `publishDiagnostics` 推送模型；`publishDiagnostics` / pull diagnostics 都保留 parser 结构化诊断 code/message/suggestion，例如空 `if ()` / `while ()` / `switch ()` 条件会报告 `missing_condition`，`if (ready { ... }`、`while (ready { ... }` 和 `switch (choice { ... }` 会报告 `missing_condition_close`，`func pick(value: int: int { ... }`、`class Box { func read(value: int: int { ... } }`、`interface Readable { read(value: int: int; }`、`interface Callable { @call(value: int: int; }`、`%extern("fixture") { NativeAdd(value: int: int; }` 和 `%extern("fixture") { delegate Callback(value: int: int; }` 会报告 `missing_parameter_list_close`，`return [1 2];` 会报告 `missing_array_element_separator`，`{a: 1 b: 2}` 会报告 `missing_object_property_separator`，`return 1\nvar next = 2;`、`var seed = 1\nvar next = 2;`、`%module "main"\nvar next = 2;`、`break\nvar next = 2;`、`continue\nvar next = 2;`、`throw 1\nvar next = 2;`、`out 1\nvar next = 2;` 和 `%using resource\nvar next = 2;` 会报告 `missing_statement_semicolon`，而不是只暴露 expected-token 信息。
- pull diagnostics 也保留 interface method signature terminator 的结构化诊断：`interface Readable { read(value: int): int }` 会报告 `missing_statement_semicolon`、`Missing ';' after interface method signature statement` 和补 `;` 的 suggestion，而不是只暴露 expected-token 信息。
- interface meta signature terminator 同样保留结构化诊断：`interface Callable { @call(value: int): int }` 会报告 `missing_statement_semicolon`、`Missing ';' after interface meta signature statement` 和补 `;` 的 suggestion。
- interface property signature terminator 同样保留结构化诊断：`interface Sized { get length: int }` 会报告 `missing_statement_semicolon`、`Missing ';' after interface property signature statement` 和补 `;` 的 suggestion。
- interface field declaration terminator 同样保留结构化诊断：`interface Entity { var id: int }` 会报告 `missing_statement_semicolon`、`Missing ';' after interface field declaration statement` 和补 `;` 的 suggestion。
- class field declaration terminator 同样保留结构化诊断：`class Entity { var id: int }` 会报告 `missing_statement_semicolon`、`Missing ';' after class field declaration statement` 和补 `;` 的 suggestion。
- class getter/setter accessor terminator 同样保留结构化诊断：`class Sized { get length: int }` 和 `class Sized { set length(value: int) }` 会报告 `missing_statement_semicolon`、对应的 class getter/setter 问题文本和补 `;` 的 suggestion，而不是只暴露 block/expected-token 信息。
- class method/meta function terminator 同样保留结构化诊断：`class Box { func read(value: int): int }` 和 `class Callable { @call(value: int): int }` 会报告 `missing_statement_semicolon`、对应的 class method/meta function 问题文本和补 `;` 的 suggestion，而不是只暴露 block/expected-token 信息。
- class member parameter-list close 同样保留结构化诊断：`class Sized { set length(value: int { return value; } }` 和 `class Callable { @call(value: int: int { return value; } }` 会报告 `missing_parameter_list_close`、declaration parameter list 缺少 closing `)` 的问题文本和补 `)` 的 suggestion，而不是只暴露 expected-token 信息。
- declaration body opener 同样保留结构化诊断：`class Box`、`interface Sized`、`func pick(): int`、`enum Tone`、`%extern("fixture")` 和 `%test("smoke")` 会报告 `missing_declaration_body_open`，分别给出 class/interface/function/enum/test declaration body 或 extern block body 的问题文本和补 `{` 的 suggestion，而不是只暴露 expected-token 信息。
- declaration body close 同样保留结构化诊断：`class Box { var id: int;`、`interface Sized { get length: int;`、`enum Tone { warm,`、`%extern("fixture") { NativeAdd(value: int): int;`、`func pick(): int { return 1;` 和 `%test("smoke") { return 1;` 会报告 `missing_declaration_body_close`、分别给出 class/interface/enum/extern/function/test declaration body 缺少 closing `}` 的问题文本和补 `}` 的 suggestion，而不是只暴露 expected-token 信息或过宽的 generic block-close 语义。
- extern spec close 同样保留结构化诊断：`%extern("fixture" { NativeAdd(value: int): int; }` 会报告 `missing_extern_spec_close`、`Missing closing ')' in extern block spec` 和在 extern block body 前补 `)` 的 suggestion，而不是只暴露 expected-token 信息。
- statement body opener 同样保留结构化诊断：`if (ready)\nreturn 1;`、`while (ready)\nreturn 1;`、`for (;;)\nreturn 1;`、`for (var item in items)\nreturn item;`、`switch (choice)\nreturn 1;`、`switch (choice) { (1)\nreturn 1; }`、`switch (choice) { ()\nreturn 1; }`、`if (ready) { return 1; } else\nreturn 2;`、`try\nreturn 1;`、`try { throw 1; } catch (error)\nreturn 2;`、`try { return 1; } finally\nreturn 2;` 和 `%using (resource)\nreturn resource;` 会报告 `missing_statement_body_open`，分别给出 if/while/for/foreach/switch/switch-case/switch-default/else/try/catch/finally/using statement body 的问题文本和补 `{` 的 suggestion，而不是只暴露 expected-token 信息。
- block close 同样保留结构化诊断：`if (ready) { return 1;` 会报告 `missing_block_close`、`Missing closing '}' for block` 和补 `}` 的 suggestion，而不是让通用 `parse_block` 只暴露 expected-token 信息。
- catch pattern close 同样保留结构化诊断：`try { throw 1; } catch (error { return 2; }` 会报告 `missing_catch_pattern_close`、`Missing closing ')' in catch pattern` 和在 catch body 前补 `)` 的 suggestion，而不是只暴露 expected-token 信息。
- using resource close 同样保留结构化诊断：`%using (resource { return resource; }` 会报告 `missing_using_resource_close`、`Missing closing ')' in using resource` 和在 using body 前补 `)` 的 suggestion，而不是只暴露 expected-token 信息。
- for/foreach header close 同样保留结构化诊断：`for (; ready; ready = false { return 1; }` 会报告 `missing_for_header_close`，`for (var item in items { return item; }` 会报告 `missing_foreach_header_close`，并给出补 `)` 后再进入 loop body 的 suggestion，而不是只暴露 expected-token 信息。
- for header separator 同样保留结构化诊断：`for (i = 0 i < 3; i = i + 1) { out i; }`、`for (i = 0; i < 3 i = i + 1) { out i; }` 和 `for (var i = 0 i < 3; i = i + 1) { out i; }` 会报告 `missing_for_header_separator`、`Missing ';' between for header clauses` 和在 clauses 之间补 `;` 的 suggestion；顶层和 block 内 `for (var ...)` 分流会用 header 内的 `=` / `;` / `in` 判断 traditional for 或 foreach，避免把 traditional for 变量初始化编辑误报成 foreach 缺 `in`。
- foreach `in` keyword 同样保留结构化诊断：`for (var item items) { return item; }` 会报告 `missing_foreach_in_keyword`、`Missing 'in' in foreach header` 和在 foreach pattern 与 iterable expression 之间补 `in` 的 suggestion，而不是只暴露 expected-token 信息。
- switch case header close 同样保留结构化诊断：`switch (choice) { (1 { return 1; } }` 会报告 `missing_switch_case_header_close`、`Missing closing ')' in switch case header` 和在 case body 前补 `)` 的 suggestion，而不是只暴露 expected-token 信息。
- switch body close 同样保留结构化诊断：`switch (choice) { (1) { return 1; }` 会报告 `missing_switch_body_close`、`Missing closing '}' for switch body` 和补 `}` 的 suggestion，而不是只暴露 expected-token 信息。
- completion resolve 通过 completion item `data` 复用原始 URI/position，按 label 回填 detail/documentation。

`stdio_editor_features.c` 是协议层 glue：

- 序列化 `TextEdit`、`WorkspaceEdit`、`CodeAction`、`FoldingRange`、`SelectionRange`、`DocumentLink`、`CodeLens` 和 pull diagnostic report。
- 处理新增 request，并在失败或空结果时返回符合 LSP 习惯的空数组/空 report。
- 保持 stdio request 分发文件只做 capability 广告和 method routing。

`stdio_linked_editing.c` 为同文档 identifier 编辑返回 linked ranges。它优先使用语义引用；当语义引用不足以形成至少两个范围时，才退回轻量文档扫描。fallback 扫描现在会跳过 line comment、block comment 和 string literal，避免把注释或字面量里的普通单词加入 linked-editing rename 范围。

`stdio_moniker.c` 在 stdio 层为源码 identifier 返回 document-scoped moniker。它仍然是轻量 token 级能力，不做跨文件符号索引；在生成 moniker 前会跳过 line comment、block comment 和 string literal 中的候选词，避免把注释或字面量里的普通单词暴露成可索引符号。

`stdio_inline_completion.c` 为 `textDocument/inlineCompletion` 提供轻量 keyword prefix 补全。它现在会先确认已输入 prefix 位于 code span，再生成 `return` / `func` / `class` 等 inline completion item；line comment、block comment、double/single quoted string 或 backtick/template string 中的同样 prefix 都返回空数组，避免编辑器在非代码文本中插入源码 keyword。

`stdio_document_color.c` 为 `textDocument/documentColor` / `colorPresentation` 提供轻量 hex color literal 支持。它仍会识别真实源码字符串中的 `"#336699"` 这类颜色字面量并返回 presentation edit，但 line comment 和 block comment 中的 `#RRGGBB` / `#RRGGBBAA` 文本不再生成 color entry；`colorPresentation` 也会先确认请求 range 对应同一组非注释 color literal，否则返回空数组，避免注释说明变成可编辑颜色 UI。

`lsp_source_spans.c` 提供 shared LSP lexical span helper，用于识别 raw document offset 是否位于 line comment、block comment 或 string literal。它也提供 cursor-offset 形式，用于 completion 这类光标位于字符之间的 request。`lsp_semantic_query.c` 在 raw identifier fallback、receiver/member semantic query fallback 和最终 local-symbol fallback 前使用它，避免 `documentHighlight` / hover / definition / references 等语义查询把注释或字面量里的普通单词或 `receiver.member` 文本解析成真实 symbol/member；`lsp_interface.c` 在 completion 进入 token-prefix、import-chain、receiver、generic completion provider 前使用 cursor code-span check，避免 `// @constructor` 或 `"%compileTime"` 触发源码补全；`lsp_token_metadata.c` 在 meta-method hover 的 raw `@...` token scan 后检查 token start code span，避免 comment/string 中的 `@constructor` 渲染成 meta-method hover；`%import("...")` string literal 仍走已有 AST/import-chain 语义路径。

`lsp_hierarchy.c` 为 call hierarchy 的 incoming/outgoing calls 做同文件轻量 call scan。它现在在统计 `name(...)` 候选前复用 editor code-span check，保留真实源码调用，同时避免 `// helper(value)` 或 `"helper(value)"` 这类说明文本被算作调用边。

`lsp_editor_features.c` 仍承载 CodeLens 编排：symbol reference count lens 走 reference 查询，`%test(...)` run lens 是轻量 raw marker scan。run lens 现在在接受 `%test(` marker 前复用 editor code-span check，保留真实 test block，同时避免 comment/string 中的 `%test(...)` 说明文本生成可运行命令。

`lsp_super_navigation.c` 为 `super(...)` 和 constructor declaration 提供 definition / references / documentHighlight 的 constructor-aware navigation。它现在复用 shared lexical span helper：raw `super` token scan 只接受 code span，constructor declaration fallback 也只在 cursor 位于 code span 时返回当前 constructor。这样保留真实 `super(seed)` 到 base constructor 的跳转，同时避免 constructor body 里的 `// super` 或 `"super"` 被当成 constructor target。

`lsp_semantic_tokens.c` 为 source semantic tokens 做轻量文本扫描，补充 keyword/directive、decorator、meta-method 和 import-chain member token。该扫描现在和其它 raw-token LSP feature 保持一致：跳过 line comment、block comment、double/single quoted string 和 backtick/template string 后才生成 token entry。真实 source `%import`、`@constructor`、`#decorator#` 和 import-chain member 仍会被标记；template string 里的 `%import(...)`、`@constructor`、`#trace#` 不会被当成 source token 上色。

`lsp_signature_help.c` 仍以 AST call context 和 semantic signature resolution 为主；进入 AST call-context 匹配前会复用 `lsp_source_spans.c` 的 cursor code-span 判定。真实 call argument 仍返回 signature help，但 call argument list 里的 comment/string 正文不会因为落在 call AST range 内而触发签名。

`stdio_inline_value.c` 是 stdio 层的 inline value consumer。它继续返回调试器可用的 `InlineValueVariableLookup`，同时对 local initializer、单行 `return` 表达式，以及缩进后位于行首的简单 expression statement 调用 `ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt`。当 parser/type inference 已证明 numeric/logical fact 时，stdio 会追加 `InlineValueText`，例如变量名范围上的 `range 20..20` / `logical true, short-circuits`，return 表达式范围上的 `range 3..3`，或 `1 + 2;` / `true || false;` / `seed + 3;` statement expression 范围上的 `range 3..3` / `logical true, short-circuits` / `range 5..5`。`stdio_inline_value_scan.c/.h` 现在拥有 expression-statement 起点、语句结束、query token 和 raw line code-span 的轻量扫描，避免继续扩大 request consumer。简单 aggregate statement 只复用同一路径：`[1 + 2];` / `[true || false];` 会在 aggregate expression range 上显示 nested element/operator fact 对应的 `range 3..3` / `logical true, short-circuits`；当源码中对象字面量以计算键形式消歧，例如前面已有普通语句后的 `{[1 + 2]: 4};`，stdio 会把 `{` 当作保守 expression-statement start，并在对象表达式范围上显示 computed-key 的 `range 3..3`。当 editor request 只从 operator-split expression statement 的 continuation line 开始，例如 `1 +\n 2;` 的第二行，stdio 会回溯 owner statement 并仍返回覆盖完整表达式的 fact-backed inline value。当 shared expression fact 已有 call/member payload 时，同一路径也能在 `pick(42);` / `seed.value;` statement expression 范围上显示 `call pick args=1` / `member value`；当 local semantic query 同时返回 reference fact 时，stdio 会追加 `reference ...`，例如 `seed[index];` 显示 `member index, reference member access`。逐行 raw scan 现在会维护 block comment 状态并跳过 `//` / `/* ... */` 内容以及 double/single/backtick string literal，所以 comment 或 string 正文中的 `var ghost = 1;` 不会生成 `InlineValueVariableLookup` 或 fact-backed text。这条路径复用语义事实层，不在 stdio request handler 内重新实现数值范围、变量范围传播、短路推断、reference 分类、array/object element 推断或 call/member payload 推断。

`lsp_local_semantic_query.c` 也是 hover/rich-hover 的局部事实 formatter。它在传统 symbol hover 之外追加 numeric/logical/reachability/reference/ownership facts，并且现在直接渲染 parser expression fact 的 kind/exactness/constant 与 call/member payload：`1 + 2` 显示 `Expression: binary exact` 和 `Constant: 3`，`pick(42)` 显示 `Call: pick args=1`，`seed.value` 显示 `Member: value`。对 `seed[index]` 这类 computed member，`[` 位置会用 member-access reference fact 找回 member payload，所以 hover/rich-hover 可以同时显示 `Reference: member access`、`Symbol: index` 和 `Member: index`。`lsp_interface.c` 的 rich-hover section parser 把这些 label 映射成稳定 `expression`、`constant`、`reference`、`symbol`、`call`、`member` roles，方便 VSCode 端结构化展示而不需要重新解析源码。

## Capability 广告

`initialize` 现在会额外声明：

- `completionProvider.resolveProvider = true`
- `codeActionProvider.codeActionKinds = ["quickfix", "source.organizeImports"]`
- `documentFormattingProvider = true`
- `documentRangeFormattingProvider = true`
- `documentOnTypeFormattingProvider.firstTriggerCharacter = "}"`
- `foldingRangeProvider = true`
- `selectionRangeProvider = true`
- `linkedEditingRangeProvider = true`
- `monikerProvider = true`
- `inlineCompletionProvider = true`
- `documentLinkProvider.resolveProvider = false`
- `declarationProvider = true`
- `typeDefinitionProvider = true`
- `implementationProvider = true`
- `codeLensProvider.resolveProvider = false`
- `callHierarchyProvider = true`
- `typeHierarchyProvider = true`
- `diagnosticProvider.interFileDependencies = true`
- `diagnosticProvider.workspaceDiagnostics = true`

VS Code desktop/native stdio 模式会自动消费这些 standard providers；extension 侧不需要为 formatting、links、CodeLens 或 organize imports 增加专有命令。

## 当前限制

- formatting 仍是保守文本缩进器，不是 AST pretty-printer；它不会重排表达式、参数列表或注释。
- code action 第一阶段稳定输出 organize imports 和缺失分号 quickfix；缺失分号 quickfix 仍是轻量文本能力，只做 code-span 过滤和 line-comment 前插入，不声明 AST-backed rewrite。后续可按 diagnostics code/data 增加缺失 import、未解析成员等修复。
- declaration / typeDefinition / implementation 目前复用 definition 查询；后续如果 class/interface/extern 语义拆分，需要在统一语义查询层细化，不要在 stdio 层分叉。
- workspace diagnostic 会为当前增量解析器已知文档返回 full report；document pull diagnostics 复用现有单文档诊断。
- linked editing 仍优先依赖语义 references；fallback 是文档级 token 扫描，只用于语义 references 不足时的保守体验，不声明声明解析或跨文件 rename。
- moniker 目前只提供文档内稳定 identity，不声明跨文档解析或 workspace symbol 绑定；comment/string 过滤来自 stdio 层轻量 scanner，不替代 parser token stream。
- CodeLens 的 `%test(...)` run command 仍是轻量 marker scan，不声明 parser-backed test discovery；code-span guard 只阻止 comment/string 中的 `%test(...)` 文本生成 run command，不改变真实 test block 的 CodeLens。
- `documentHighlight` 仍由 shared semantic query/reference graph 驱动；新增 lexical span helper 只约束 raw identifier fallback，避免把 comment/string 中的普通文本提升为 symbol target，不替代 parser token stream 或 import-literal AST navigation。
- receiver/member navigation 仍走 shared semantic query 和 existing member resolver；code-span guard 只阻止 comment/string 中的 `box.value` 这类 raw text 触发 definition/hover/references，不替代 parser token stream，不改变真实 `return box.value;` 的 member definition 解析。
- call hierarchy 仍是同文件 lightweight raw call scan，不声明 parser-backed call graph；code-span guard 只阻止 comment/string 中的 `helper(value)` 这类说明文本成为 incoming/outgoing edge，不改变真实 `return helper(value);` 的直接调用关系。
- `super(...)` constructor navigation 仍是专用 LSP fallback；它只在 code span 中解析 raw `super` token 或 constructor declaration fallback，comment/string filtering 复用 shared lexical span helper，不声明完整 parser token-stream replacement。
- `textDocument/completion` 会在 request cursor 不位于 code span 时返回空 completion list，避免 comment/string 中的 `%` / `@` 触发 directive 或 meta-method completions；这只是 completion request 入口过滤，不声明 parser token-stream replacement 或 import-string path completion。
- `textDocument/inlineCompletion` 仍是 stdio 层 keyword-prefix 轻量能力，不执行 parser completion 或 symbol inference；它只在 prefix 字节位于 code span 时返回 keyword item，comment/string/template 中返回空数组，不声明 parser token-stream replacement。
- `textDocument/documentColor` / `colorPresentation` 仍是 stdio 层 hex literal 扫描能力，不执行 parser color typing；它只过滤 line/block comment spans，不把注释中的颜色样例暴露为 editor color entry 或 presentation edit。
- meta-method `textDocument/hover` 仍是 token-metadata 辅助能力，只在 code span 中解释 `@constructor` / `@add` 等 token；comment/string 中的相同文本不会生成 meta-method hover，不声明完整 parser token-stream replacement。
- inline value 的语义 fact-backed 文本目前覆盖 local initializer、单行 return 表达式，以及缩进后行首的简单 expression statement 的 numeric/logical/reference facts，包括 identifier-led 算术表达式如 `seed + 3;`、unary 起始、call/member 起始、computed member 起始、array literal aggregate 起始如 `[1 + 2];`，以及由 computed key 消歧的 object literal aggregate 起始如 `{[1 + 2]: 4};`。多行支持仍是保守的：已覆盖多行 return、return-next-line、multi-line initializer，以及 request range 从 continuation line 开始的简单 operator-split expression statement；call/member statement 只在共享 expression/reference payload fact 已存在时显示 `call ... args=N`、`member ...` 或 `reference ...`，array/object aggregate statement 只显示共享 nested element/operator facts，不会由 stdio 自己解析源码来伪造展示。raw line scanner 会过滤 line/block comments 和 double/single/backtick string literal，但这仍是 stdio request 边界保护，不声明 parser token-stream replacement。普通 `{a: ...};` 行首源码语句仍受 block/object 歧义限制，后续需要 parser 级消歧后才能声明 inline-value parity。运行时 debugger value 展示和更复杂的控制流 fact 仍应通过共享 local semantic query 继续扩展。
- hover/rich-hover 的 call/member 展示只消费已经存在的 expression/reference payload facts，不执行调用、不解析成员链类型，也不为没有共享 payload 的表达式合成 UI 文本；后续更复杂的 call/member UI 仍应先扩展共享 fact 层。

## 验证证据

新增覆盖：

- `tests/language_server/test_lsp_advanced_editor_features.c`
  - formatting full-document edit
  - folding range 和 selection range
  - `%import(...)` document link
  - organize imports code action
  - missing semicolon quickfix keeps insertion before line comments and skips block-comment bodies
  - reference count 和 `%test(...)` CodeLens command
  - call hierarchy / type hierarchy prepare item
- `tests/language_server/stdio_smoke.js`
  - initialize capability wire shape
  - formatting / folding / selection / documentLink / codeAction / CodeLens / hierarchy request arrays
  - declaration provider
  - pull diagnostics full report
  - `publishDiagnostics` parser diagnostic code/message/suggestion, including `missing_condition` for empty `if ()`
  - `textDocument/documentHighlight` returns code-token highlights and ignores raw fallback identifiers inside line comments, block comments, and string literals
  - `textDocument/linkedEditingRange` returns code-token edit ranges and does not use comment/string matches to synthesize fallback ranges
  - `textDocument/moniker` returns code-token identity and ignores identifiers inside line comments, block comments, and string literals
  - `textDocument/inlineCompletion` expands real code keyword prefixes and ignores the same prefixes inside line comments, block comments, and string literals
  - `textDocument/documentColor` exposes real hex color literals and ignores hex colors inside line/block comments; `colorPresentation` returns edits only for ranges matching those exposed literals
  - inline value variable lookup 和 semantic fact-backed inline text
- `tests/language_server/test_lsp_interface.c`
  - parser-backed LSP diagnostics for empty `if ()` / `while ()` conditions expose `missing_condition`
  - statement-specific message and concrete suggestion text
  - `textDocument/definition`, `textDocument/references`, and `textDocument/documentHighlight` keep resolving real `super(...)` constructor calls, but return no locations/highlights for `super` text inside constructor comments or string literals
  - `textDocument/completion` keeps directive/meta-method completions for real `%` / `@` code prefixes, but returns an empty list for the same prefixes inside comments or string literals
  - `textDocument/hover` keeps meta-method hover for real `@constructor` declarations, but does not render meta-method documentation for `@constructor` text inside comments or string literals
  - `textDocument/semanticTokens/full` keeps classifying real source `%import` directives, but does not classify `%import(...)`, `@constructor`, or `#trace#` text inside backtick/template strings as keyword, meta-method, or decorator tokens
- `tests/language_server/stdio_inline_value_semantic_smoke.js`
  - identifier-led expression statement `seed + 3;`
  - fact-backed inline text `range 5..5` on the expression range
  - call/member expression statements `pick(42);` / `seed.value;`
  - fact-backed inline text `call pick args=1` / `member value` on the expression range
  - computed member expression statement `seed[index];`
  - fact-backed inline text `member index, reference member access` on the expression range
  - aggregate expression statements `[1 + 2];` / `[true || false];`
  - fact-backed inline text `range 3..3` / `logical true, short-circuits` on the aggregate expression range
  - computed-key object aggregate statement `{[1 + 2]: 4};`
  - fact-backed inline text `range 3..3` on the object expression range
  - continuation-only request range for `1 +` / `2;`
  - fact-backed inline text `range 3..3` over the full multi-line expression range
  - variable-looking text inside multi-line, indented single-line, and zero-column block comments returns no inline values
  - variable-looking text inside double-quoted, single-quoted, and backtick/template strings returns no inline values
- `tests/language_server/test_lsp_local_semantic_hover.c`
  - call/member expression payload hover text
  - rich-hover `call` / `member` roles
- `tests/language_server/test_lsp_computed_member_hover.c`
  - computed member-access hover at `[` includes `Reference: member access`, `Symbol: index`, and `Member: index`
  - rich-hover `reference` / `symbol` / `member` roles for the same bracket-position query
- `tests/language_server/test_lsp_expression_fact_hover.c`
  - local hover over `1 + 2` includes `Expression: binary exact` and `Constant: 3`
  - public `textDocument/hover` uses the same local semantic expression fact payload
  - rich-hover exposes stable `expression` / `constant` roles for the same payload

2026-04-24 验证命令：

```sh
cmake -S . -B build/codex-wsl-gcc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DBUILD_TESTS=ON
cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_lsp_advanced_editor_features_test zr_vm_language_server_stdio -j 8
ctest --test-dir build/codex-wsl-gcc-debug -R 'language_server|language_server_stdio_smoke' --output-on-failure
cmake -S . -B build/codex-wsl-clang-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DBUILD_TESTS=ON
cmake --build build/codex-wsl-clang-debug -j 8
ctest --test-dir build/codex-wsl-clang-debug -R 'language_server|language_server_stdio_smoke' --output-on-failure
```

2026-06-04 inline value semantic fact 聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-wsl-gcc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_stdio -j 8 && ctest --test-dir build/codex-wsl-gcc-debug -R language_server_stdio_smoke --output-on-failure"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-wsl-clang-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ && cmake --build build/codex-wsl-clang-debug --target zr_vm_language_server_stdio -j 8 && ctest --test-dir build/codex-wsl-clang-debug -R language_server_stdio_smoke --output-on-failure"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake -S . -B build\codex-msvc-lsp-smoke -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTS=OFF -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF; cmake --build build\codex-msvc-lsp-smoke --config Debug --target zr_vm_language_server_shared zr_vm_language_server_stdio --parallel 8; cmake -S . -B build\codex-msvc-cli-debug -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTS=OFF -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF; cmake --build build\codex-msvc-cli-debug --config Debug --target zr_vm_cli_executable --parallel 8; .\build\codex-msvc-cli-debug\bin\Debug\zr_vm_cli.exe .\tests\fixtures\projects\hello_world\hello_world.zrp
```

结果：WSL gcc 和 WSL clang 的 `language_server_stdio_smoke` 均通过，覆盖 `textDocument/inlineValue` 对 `var x = 20;` 输出 `range 20..20`，对 `var flag = true || false;` 输出 `logical true, short-circuits`。MSVC smoke 构建 `zr_vm_language_server_shared`、`zr_vm_language_server_stdio` 和 CLI 通过，`hello_world.zrp` 输出 `hello world`。

2026-06-04 return expression inline value semantic fact 聚焦验证：

```powershell
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && node tests/language_server/stdio_smoke.js build/codex-wsl-clang-debug/bin/zr_vm_language_server_stdio build/codex-wsl-clang-debug/bin/zr_vm_cli"
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_language_server_stdio zr_vm_cli_executable -j 4 && node tests/language_server/stdio_smoke.js build/codex-wsl-clang-debug/bin/zr_vm_language_server_stdio build/codex-wsl-clang-debug/bin/zr_vm_cli"
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_stdio zr_vm_cli_executable -j 4 && node tests/language_server/stdio_smoke.js build/codex-wsl-gcc-debug/bin/zr_vm_language_server_stdio build/codex-wsl-gcc-debug/bin/zr_vm_cli"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-msvc-lsp-smoke --config Debug --target zr_vm_language_server_shared zr_vm_language_server_stdio --parallel 4; node tests\language_server\stdio_smoke.js build\codex-msvc-lsp-smoke\bin\Debug\zr_vm_language_server_stdio.exe build\codex-msvc-cli-debug\bin\Debug\zr_vm_cli.exe
```

RED：新增 `return 1 + 2;` 的 `textDocument/inlineValue` 断言后，旧 server 返回响应但没有 `range 3..3` inline text。GREEN：`stdio_inline_value.c` 对 `return` 表达式范围调用同一 local semantic query，并在表达式范围 `1 + 2` 上返回 `InlineValueText`。WSL clang、WSL gcc 和 MSVC stdio smoke 均通过；gcc/clang/MSVC 仍有既有 core/parser/LSP warning，本轮没有运行全仓库 ctest。

2026-06-04 expression-statement inline value semantic fact 聚焦验证：

```powershell
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_language_server_stdio zr_vm_cli_executable -j 4 > /tmp/zr_inline_expr_clang_build.out 2>&1; status=$?; tail -80 /tmp/zr_inline_expr_clang_build.out; exit $status"
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && node tests/language_server/stdio_smoke.js build/codex-wsl-clang-debug/bin/zr_vm_language_server_stdio build/codex-wsl-clang-debug/bin/zr_vm_cli"
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_stdio zr_vm_cli_executable -j 4 > /tmp/zr_inline_expr_gcc_build.out 2>&1; status=$?; tail -80 /tmp/zr_inline_expr_gcc_build.out; exit $status"
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && node tests/language_server/stdio_smoke.js build/codex-wsl-gcc-debug/bin/zr_vm_language_server_stdio build/codex-wsl-gcc-debug/bin/zr_vm_cli"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-msvc-lsp-smoke --config Debug --target zr_vm_language_server_shared zr_vm_language_server_stdio --parallel 4; node tests\language_server\stdio_smoke.js build\codex-msvc-lsp-smoke\bin\Debug\zr_vm_language_server_stdio.exe build\codex-msvc-cli-debug\bin\Debug\zr_vm_cli.exe
```

RED：新增 `1 + 2;` / `true || false;` 的 `textDocument/inlineValue` 断言后，旧 server 返回响应但没有 expression-statement 范围上的 `range 3..3` 或 `logical true, short-circuits` inline text。GREEN：`stdio_inline_value.c` 对缩进后行首的简单 literal/boolean expression statement 复用同一 local semantic query，在表达式范围上返回 fact-backed `InlineValueText`。WSL clang、WSL gcc 和 MSVC stdio smoke 均通过；gcc/MSVC 仍有既有 core/parser/CLI warning，本轮没有运行全仓库 ctest。

2026-06-04 call/member expression-statement inline value semantic payload 聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_stdio -j 8 && node tests/language_server/stdio_inline_value_semantic_smoke.js build/codex-wsl-gcc-debug/bin/zr_vm_language_server_stdio && ctest --test-dir build/codex-wsl-gcc-debug -R '^language_server_stdio(_inline_value_semantic)?_smoke$' --output-on-failure"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_language_server_stdio -j 8 && node tests/language_server/stdio_inline_value_semantic_smoke.js build/codex-wsl-clang-debug/bin/zr_vm_language_server_stdio && ctest --test-dir build/codex-wsl-clang-debug -R '^language_server_stdio(_inline_value_semantic)?_smoke$' --output-on-failure"
cmake --build build\codex-msvc-lsp-smoke --config Debug --target zr_vm_language_server_stdio --parallel 8; if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }; node tests\language_server\stdio_inline_value_semantic_smoke.js build\codex-msvc-lsp-smoke\bin\Debug\zr_vm_language_server_stdio.exe
node tests\language_server\stdio_smoke.js build\codex-msvc-lsp-smoke\bin\Debug\zr_vm_language_server_stdio.exe build\codex-msvc-cli-debug\bin\Debug\zr_vm_cli.exe
```

RED：新增 `pick(42);` / `seed.value;` 的 `textDocument/inlineValue` 断言后，旧 server 返回响应但没有 call/member payload inline text。GREEN：`stdio_inline_value.c` 格式化共享 `SZrSemanticExpressionFact` 的 call/member payload，分别输出 `call pick args=1` 和 `member value`。WSL gcc、WSL clang registered stdio smokes 均通过；Windows MSVC dedicated inline-value smoke 和 broader stdio smoke 通过。本轮仍不声明全仓库 ctest 绿色。

2026-06-04 continuation-range expression-statement inline value 聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_stdio -j 8 && node tests/language_server/stdio_inline_value_semantic_smoke.js build/codex-wsl-gcc-debug/bin/zr_vm_language_server_stdio && ctest --test-dir build/codex-wsl-gcc-debug -R '^language_server_stdio(_inline_value_semantic)?_smoke$' --output-on-failure"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_language_server_stdio -j 8 && node tests/language_server/stdio_inline_value_semantic_smoke.js build/codex-wsl-clang-debug/bin/zr_vm_language_server_stdio && ctest --test-dir build/codex-wsl-clang-debug -R '^language_server_stdio(_inline_value_semantic)?_smoke$' --output-on-failure"
cmake --build build\codex-msvc-lsp-smoke --config Debug --target zr_vm_language_server_stdio --parallel 8
node tests\language_server\stdio_inline_value_semantic_smoke.js build\codex-msvc-lsp-smoke\bin\Debug\zr_vm_language_server_stdio.exe
node tests\language_server\stdio_smoke.js build\codex-msvc-lsp-smoke\bin\Debug\zr_vm_language_server_stdio.exe build\codex-msvc-cli-debug\bin\Debug\zr_vm_cli.exe
```

RED：新增只请求 `1 +\n 2;` 第二行的 inline-value 断言后，旧 server 返回空数组。GREEN：`stdio_inline_value.c` 在 request 从 continuation line 开始且 owner line 不在请求范围内时回溯到前一行 expression statement，返回完整表达式范围上的 `range 3..3`。WSL gcc、WSL clang registered stdio smokes 均通过；Windows MSVC dedicated inline-value smoke 和 broader stdio smoke 通过。本轮仍不声明全仓库 ctest 绿色。

2026-06-05 computed-member inline value reference fact 聚焦验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_stdio -j 6 && node tests/language_server/stdio_inline_value_semantic_smoke.js ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_stdio'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_stdio -j 6 && node tests/language_server/stdio_inline_value_semantic_smoke.js ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_stdio'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
where.exe cl
cmake --build build\agent-msvc-tests --config Debug --target zr_vm_language_server_stdio --parallel 6
node tests\language_server\stdio_inline_value_semantic_smoke.js build\agent-msvc-tests\bin\Debug\zr_vm_language_server_stdio.exe
```

RED：新增 `seed[index];` 的 `textDocument/inlineValue` reference 断言后，旧 server 返回 `text:"member index"`，缺少 `reference member access`。GREEN：`stdio_inline_value.c` 继续通过 `ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt` 取事实，并把 `SZrSemanticReferenceFact` 格式化为 compact `reference ...` inline text；computed member expression statement 现在返回 `member index, reference member access`。WSL gcc、WSL clang 和 Windows MSVC dedicated inline-value smoke 均通过；MSVC 仍有当前 dirty checkout 的既有 core/parser/library warning，本轮仍不声明全仓库绿色。

2026-06-05 moniker non-code token 过滤聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_stdio -j 6 && node tests/language_server/stdio_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_stdio"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_stdio -j 6 && node tests/language_server/stdio_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_stdio"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_stdio --parallel 6; node tests\language_server\stdio_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_stdio.exe
```

RED：新增 comment/string/block-comment 位置的 `textDocument/moniker` 断言后，旧 server 会为注释或字面量里的普通单词返回 `zr` moniker。GREEN：`stdio_moniker.c` 在生成 document-scoped identity 前用轻量 scanner 跳过 line comment、block comment 和 string literal，保留真实 code token 的 moniker。WSL gcc、WSL clang 和 Windows MSVC focused stdio smoke 均通过；本轮不声明全仓库 ctest 绿色。

2026-06-05 linked-editing fallback non-code token 过滤聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_stdio -j 6 && node tests/language_server/stdio_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_stdio"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_stdio -j 6 && node tests/language_server/stdio_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_stdio"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_stdio --parallel 6; node tests\language_server\stdio_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_stdio.exe
```

RED：新增 `linkedOnly` 只有一个真实 code token、但 comment/string/block-comment 中有同名文本的 `textDocument/linkedEditingRange` 断言后，旧 fallback 返回 linked ranges，证明它会把非代码文本当成可编辑符号。GREEN：`stdio_linked_editing.c` 在 fallback 入口和每个扫描命中点跳过 line comment、block comment 和 string literal；当只有一个真实 code token 时返回 `null`。WSL gcc、WSL clang 和 Windows MSVC focused stdio smoke 均通过；本轮不声明全仓库 ctest 绿色。

2026-06-05 documentHighlight non-code token 过滤聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && node tests/language_server/stdio_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_stdio"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_stdio -j 6 && node tests/language_server/stdio_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_stdio"
wsl bash -lc "cd /mnt/e/Git/zr_vm && build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_lsp_interface_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_stdio -j 6 && node tests/language_server/stdio_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_stdio"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_stdio --parallel 6; node tests\language_server\stdio_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_stdio.exe
```

RED：新增 comment/string/block-comment 位置的 `textDocument/documentHighlight` 断言后，旧 shared semantic query 会把 `highlightOnly` 注释或字面量文本解析成真实 local symbol highlights。GREEN：新增 `lsp_source_spans.c` shared lexical span helper，`lsp_semantic_query.c` 在 raw identifier fallback 前拒绝 line comment、block comment 和 string literal offset；import literal navigation 仍通过 existing AST/import-chain path。WSL gcc、WSL clang 和 Windows MSVC focused stdio smoke 均通过，WSL gcc `zr_vm_language_server_lsp_interface_test` 也通过；本轮不声明全仓库 ctest 绿色。

2026-06-05 super constructor navigation non-code token 过滤聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_lsp_interface_test -j 6 && build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_lsp_interface_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_lsp_interface_test -j 6 && build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_lsp_interface_test"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_lsp_interface_test --parallel 6; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_lsp_interface_test.exe
```

RED：新增 `tests/language_server/test_lsp_interface.c` 的 `LSP Super Constructor Navigation Ignores Non-Code Tokens` 后，旧 WSL gcc build 在 comment `super` 上返回一个 constructor definition location，证明 `super` fallback 会把 constructor body 中的非代码文本当成 navigation target。GREEN：`lsp_super_navigation.c` 对 raw `super` token scan、live cursor token check、constructor declaration fallback 都复用 shared lexical span helper；`lsp_semantic_query.c` 同时在最终 local-symbol fallback 前检查 live buffer code span，避免 constructor body comment/string 继续经通用 symbol fallback 解析。真实 `super(seed)` definition/references/highlights 和 import-literal semantic query tests 继续通过。WSL gcc、WSL clang 和 Windows MSVC focused `zr_vm_language_server_lsp_interface_test` 均通过；本轮不声明全仓库 ctest 绿色。

2026-06-05 completion non-code token prefix 过滤聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_lsp_interface_test -j 6 && build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_lsp_interface_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_lsp_interface_test -j 6 && build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_lsp_interface_test"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_lsp_interface_test --parallel 6; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_lsp_interface_test.exe
```

RED：新增 `tests/language_server/test_lsp_interface.c` 的 `LSP Completion Ignores Token Prefixes In Non Code Text` 后，旧 WSL gcc build 在 comment `@constructor` 上返回 meta-method completion，证明 token-prefix completion 会把注释文本当成 source trigger。GREEN：`lsp_source_spans.c` 增加 cursor-offset code-span helper，`lsp_interface.c` 在 completion request 进入 token-prefix、import-chain、receiver 和 generic completion providers 前过滤 non-code cursor。真实 `%` / `@` directive/meta-method completion 继续通过；comment/string 中的 prefix 返回空 completion list。WSL gcc、WSL clang 和 Windows MSVC focused `zr_vm_language_server_lsp_interface_test` 均通过；本轮不声明全仓库 ctest 绿色。

2026-06-05 meta-method hover non-code token 过滤聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_lsp_interface_test -j 6 && build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_lsp_interface_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_lsp_interface_test -j 6 && build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_lsp_interface_test"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_lsp_interface_test --parallel 6; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_lsp_interface_test.exe
```

RED：新增 `tests/language_server/test_lsp_interface.c` 的 `LSP Hover Ignores Meta Methods In Non Code Text` 后，旧 WSL gcc build 在 comment `@constructor` 上返回 meta-method hover documentation，证明 meta-method hover 的 raw token scan 会把注释文本当成 source token。GREEN：`lsp_token_metadata.c` 在 `@...` token scan 后复用 shared lexical span helper 检查 token start，只让 code span 中的 meta-method token 生成 hover。真实 `@constructor` declaration hover 与 rich-hover 结构化 section 继续通过；comment/string 中的 `@constructor` 不再渲染 meta-method hover。WSL gcc、WSL clang 和 Windows MSVC focused `zr_vm_language_server_lsp_interface_test` 均通过；本轮不声明全仓库 ctest 绿色。

2026-06-05 semantic-token template string 过滤聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_lsp_interface_test -j 8 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_lsp_interface_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_lsp_interface_test -j 8 && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_lsp_interface_test"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_lsp_interface_test --parallel 8; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_lsp_interface_test.exe
```

RED：新增 `tests/language_server/test_lsp_interface.c` 的 `LSP Semantic Tokens Ignore Template String Tokens` 后，旧 WSL gcc build 报告 `Semantic tokens classified directive, meta-method, or decorator text inside a backtick string`，证明 semantic-token scanner 会把 template string 文本当成 source token。GREEN：`lsp_semantic_tokens.c` 的 string-skip branch 现在同时跳过 backtick/template string；真实 `%import` keyword token 继续生成，而 template string 中的 `%import(...)`、`@constructor`、`#trace#` 不再生成 keyword/metaMethod/decorator token。WSL gcc、WSL clang 和 Windows MSVC focused `zr_vm_language_server_lsp_interface_test` 均通过；本轮不声明全仓库 ctest 绿色。

2026-06-05 inline-completion non-code prefix 过滤聚焦验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_stdio zr_vm_cli_executable -j 8 && ctest --test-dir build/codex-semantic-wsl-gcc-debug -R language_server_stdio_smoke --output-on-failure'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_stdio zr_vm_cli_executable -j 8 && ctest --test-dir build/codex-semantic-wsl-clang-debug -R language_server_stdio_smoke --output-on-failure'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_stdio zr_vm_cli_executable --parallel 8; ctest --test-dir build\codex-semantic-msvc-debug -C Debug -R language_server_stdio_smoke --output-on-failure
```

RED：扩展 `tests/language_server/stdio_smoke.js` 后，旧 WSL gcc stdio server 在 `// ret` 中返回 `return` inline completion，证明 keyword prefix scanner 会把 line comment 文本当成 code。GREEN：`stdio_inline_completion.c` 在生成 keyword item 前检查 prefix 是否位于 code span；真实 `ret` / `retu` code prefix 继续补全为 `return `，line comment、string literal 和 block comment 中的同样 prefix 返回空数组。WSL gcc、WSL clang 和 Windows MSVC focused `language_server_stdio_smoke` 均通过；本轮不声明全仓库 ctest 绿色。

2026-06-05 documentColor comment color 过滤聚焦验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_stdio zr_vm_cli_executable -j 8 && ctest --test-dir build/codex-semantic-wsl-gcc-debug -R language_server_stdio_smoke --output-on-failure'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_stdio zr_vm_cli_executable -j 8 && ctest --test-dir build/codex-semantic-wsl-clang-debug -R language_server_stdio_smoke --output-on-failure'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_stdio zr_vm_cli_executable --parallel 8; ctest --test-dir build\codex-semantic-msvc-debug -C Debug -R language_server_stdio_smoke --output-on-failure
```

RED：扩展 `tests/language_server/stdio_smoke.js` 后，旧 WSL gcc stdio server 为 `// "#112233"` / `/* "#445566" ... */` 返回 color entries，证明 `documentColor` raw scanner 会把注释样例当成源码颜色。GREEN：`stdio_document_color.c` 在 hex literal 扫描外增加 line/block comment 状态，同时保留真实字符串字面量 `"#336699"` 的 color entry 和 `colorPresentation` edit。WSL gcc、WSL clang 和 Windows MSVC focused `language_server_stdio_smoke` 均通过；本轮不声明全仓库 ctest 绿色。

2026-06-05 colorPresentation comment range 过滤聚焦验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_stdio zr_vm_cli_executable -j 8 && ctest --test-dir build/codex-semantic-wsl-gcc-debug -R language_server_stdio_smoke --output-on-failure'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_stdio zr_vm_cli_executable -j 8 && ctest --test-dir build/codex-semantic-wsl-clang-debug -R language_server_stdio_smoke --output-on-failure'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_stdio zr_vm_cli_executable --parallel 8; ctest --test-dir build\codex-semantic-msvc-debug -C Debug -R language_server_stdio_smoke --output-on-failure
```

RED：扩展 `tests/language_server/stdio_smoke.js` 后，旧 WSL gcc stdio server 对 line comment 中 `#112233` 的 range 仍返回 `colorPresentation` edit，证明 presentation request 没有复用 `documentColor` 的非注释扫描边界。GREEN：`stdio_document_color.c` 在生成 presentation 前先确认 request range 匹配同一文档中的有效非注释 color literal；真实字符串字面量 `"#336699"` 仍返回 `#336699` edit，注释 range 返回空数组。WSL gcc、WSL clang 和 Windows MSVC focused `language_server_stdio_smoke` 均通过；本轮不声明全仓库 ctest 绿色。

2026-06-05 semicolon quickfix block-comment 过滤聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && ninja -C build/codex-semantic-wsl-gcc-debug -j2 zr_vm_language_server_lsp_advanced_editor_features_test && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_lsp_advanced_editor_features_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && ninja -C build/codex-semantic-wsl-clang-debug -j2 zr_vm_language_server_lsp_advanced_editor_features_test && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_lsp_advanced_editor_features_test"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_lsp_advanced_editor_features_test --parallel 6; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_lsp_advanced_editor_features_test.exe
```

RED：新增 `tests/language_server/test_lsp_advanced_editor_features.c` 的 block-comment semicolon quickfix 断言后，旧 WSL gcc focused executable 失败：`semicolon quick fix was offered inside a block comment`。GREEN：`lsp_code_actions.c` 在生成缺失分号 quickfix 前复用 `lsp_editor_offset_is_code` 检查 trimmed statement start，保留真实 `return answer // note` 在 line comment 前插入分号，同时不再为 block comment 正文中的 `return answer` 提供 semicolon edit。WSL gcc、WSL clang 和 Windows MSVC focused `zr_vm_language_server_lsp_advanced_editor_features_test` 均通过；本轮不声明全仓库 ctest 绿色。

2026-06-05 inlineValue block-comment scanner 过滤聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && ninja -C build/codex-semantic-wsl-gcc-debug -j2 zr_vm_language_server_stdio && node tests/language_server/stdio_inline_value_semantic_smoke.js ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_stdio"
wsl bash -lc "cd /mnt/e/Git/zr_vm && ninja -C build/codex-semantic-wsl-clang-debug -j2 zr_vm_language_server_stdio && node tests/language_server/stdio_inline_value_semantic_smoke.js ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_stdio"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_stdio --parallel 6; node tests\language_server\stdio_inline_value_semantic_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_stdio.exe
```

RED：新增 block-comment inline-value 断言后，旧 WSL gcc stdio server 先对 multi-line block comment 正文中的 `var ghost = 1;` 返回 `InlineValueVariableLookup`；随后 zero-column single-line `/* var topGhost = 3; */` 也失败，错误为 `textDocument/inlineValue must ignore zero-column block-comment variables`。GREEN：`stdio_inline_value.c` 的逐行 scanner 维护 block comment 状态，并在 request range 之前的行也更新该状态；line/block comment 内容不再进入 variable lookup 或 semantic fact 查询。WSL gcc、WSL clang 和 Windows MSVC dedicated inline-value smoke 均通过；本轮不声明全仓库 ctest 绿色。

2026-06-05 inlineValue string-literal scanner 过滤聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && ninja -C build/codex-semantic-wsl-gcc-debug -j2 zr_vm_language_server_stdio && node tests/language_server/stdio_inline_value_semantic_smoke.js ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_stdio"
wsl bash -lc "cd /mnt/e/Git/zr_vm && ninja -C build/codex-semantic-wsl-clang-debug -j2 zr_vm_language_server_stdio && node tests/language_server/stdio_inline_value_semantic_smoke.js ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_stdio"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_stdio --parallel 6; node tests\language_server\stdio_inline_value_semantic_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_stdio.exe
```

RED：新增 double/single/backtick string literal inline-value 断言后，旧 WSL gcc stdio server 对 `"var stringGhost = 4;"` 返回 `InlineValueVariableLookup`，错误为 `textDocument/inlineValue must ignore variable-looking text inside double-quoted strings`。GREEN：raw line code-span helper 移入 `stdio_inline_value_scan.c/.h`，同时跳过 double/single/backtick strings 和已有 line/block comments；`stdio_inline_value.c` 只保留 request orchestration，并从 926 行降到 853 行。WSL gcc、WSL clang 和 Windows MSVC dedicated inline-value smoke 均通过；本轮仍不声明全仓库绿色。

2026-06-05 continuation-only initializer inline value 聚焦验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm; node tests/language_server/stdio_inline_value_semantic_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_stdio'
wsl bash -lc 'cd /mnt/e/Git/zr_vm; cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_stdio -j 6 && node tests/language_server/stdio_inline_value_semantic_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_stdio'
wsl bash -lc 'cd /mnt/e/Git/zr_vm; cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_stdio -j 6 && node tests/language_server/stdio_inline_value_semantic_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_stdio'
& "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Invoke-VsDevCommand.ps1" cmake --build build\codex-msvc-debug --target zr_vm_language_server_stdio --config Debug -j 6
node tests\language_server\stdio_inline_value_semantic_smoke.js build\codex-msvc-debug\bin\Debug\zr_vm_language_server_stdio.exe
```

RED：只请求 `var sum =\n  1 + 2;` continuation 行时，旧 WSL gcc stdio server 返回空 inline-value 数组。GREEN：`stdio_inline_value.c` 对 continuation-only initializer request 回溯到 `var sum =` owner，并保持 semantic fact 锚定在 `sum` 名字 range；semantic text formatter/query bridge 已抽出到 `stdio_inline_value_semantic_text.c/.h`，避免继续膨胀 scanner。WSL gcc、WSL clang 和 Windows MSVC dedicated inline-value smoke 均通过；本轮仍不声明全仓库绿色。

2026-06-05 aggregate expression-statement inline value 聚焦验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_stdio -j 6 && node tests/language_server/stdio_inline_value_semantic_smoke.js ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_stdio'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && ctest --test-dir build/codex-semantic-wsl-gcc-debug -R "^language_server_stdio_inline_value_semantic_smoke$" --output-on-failure'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_stdio -j 6 && node tests/language_server/stdio_inline_value_semantic_smoke.js ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_stdio && ctest --test-dir build/codex-semantic-wsl-clang-debug -R "^language_server_stdio_inline_value_semantic_smoke$" --output-on-failure'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
where.exe cl
cmake --build build\codex-semantic-msvc-debug --target zr_vm_language_server_stdio --config Debug --parallel 6
node tests\language_server\stdio_inline_value_semantic_smoke.js .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_stdio.exe
ctest --test-dir build\codex-semantic-msvc-debug -C Debug -R "^language_server_stdio_inline_value_semantic_smoke$" --output-on-failure
```

RED：新增 `[1 + 2];` / `[true || false];` 的 aggregate expression-statement inline-value 断言后，旧 WSL gcc stdio server 返回 `values=[]`。GREEN：`stdio_inline_value.c` 只把 `[` 纳入缩进后行首的保守 expression-statement start 集合，range/query 仍复用 shared local semantic query；`[1 + 2];` 在 aggregate range 上显示 `range 3..3`，`[true || false];` 显示 `logical true, short-circuits`。WSL gcc、WSL clang 和 Windows MSVC dedicated inline-value smoke 与 registered CTest 均通过；本轮没有修改 parser/type inference/core runtime，也不声明全仓库绿色。

2026-06-05 computed-key object aggregate expression-statement inline value 聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && node tests/language_server/stdio_inline_value_semantic_smoke.js ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_stdio"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_stdio -j 8 && ctest --test-dir build/codex-semantic-wsl-gcc-debug -R '^language_server_stdio_inline_value_semantic_smoke$' --output-on-failure"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_stdio -j 8 && ctest --test-dir build/codex-semantic-wsl-clang-debug -R '^language_server_stdio_inline_value_semantic_smoke$' --output-on-failure"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
where.exe cl
cmake --build build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_stdio --parallel 8
ctest --test-dir build\codex-semantic-msvc-debug -C Debug -R "^language_server_stdio_inline_value_semantic_smoke$" --output-on-failure
```

RED：扩展 `tests/language_server/stdio_inline_value_semantic_smoke.js` 后，旧 WSL gcc stdio server 对 `{[1 + 2]: 4};` 返回 `values=[]`，证明 inline-value scanner 不接受 `{` 起始的 object aggregate expression statement。GREEN：`stdio_inline_value_scan.c/.h` 接管 expression-statement 起点、balanced statement-end 和 query token 定位，保守接受空对象、普通 key 后跟 `:`、以及 computed key `[...]` 后跟 `:`；当前 smoke 锁住 parser 已能稳定提供 local fact 的 computed-key object form，并在对象表达式 range 上显示 `range 3..3`。普通 `{a: ...};` 行首源码语句仍受 block/object 歧义限制，不在本 slice 声明。随后 broader stdio smoke 的 completion assertion 改为检查 detail 中包含 `range 20..20`，兼容 completion detail 现在先显示 `expression ...` / `constant ...` 再显示 numeric fact 的合法顺序；WSL gcc、WSL clang 和 Windows MSVC registered stdio smoke pair 均通过。`stdio_inline_value.c` 降到大文件阈值以下；本轮仍不声明全仓库绿色。

2026-06-05 expression fact hover 聚焦验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm; cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_expression_fact_hover_test -j 6 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_expression_fact_hover_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm; cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_expression_fact_hover_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_computed_member_hover_test -j 6 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_expression_fact_hover_test && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_query_test && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_hover_test && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_computed_member_hover_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm; cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_expression_fact_hover_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_computed_member_hover_test -j 6 && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_expression_fact_hover_test && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_query_test && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_hover_test && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_computed_member_hover_test'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
cmake --build build\codex-semantic-msvc-debug --target zr_vm_language_server_expression_fact_hover_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_computed_member_hover_test --config Debug -j 6
.\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_expression_fact_hover_test.exe
.\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_local_semantic_query_test.exe
.\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_computed_member_hover_test.exe
```

RED：新增 `tests/language_server/test_lsp_expression_fact_hover.c` 后，WSL gcc local query 已经拿到 `kind=binary`、`exact`、`hasConstant=1`、`const=3`，但 hover 只显示 `Type: int`、`Numeric range: 3..3` 和 unsigned range，没有 `Expression:` / `Constant:`。GREEN：`lsp_local_semantic_expression_text.c/.h` 将 expression fact kind/exactness/constant 格式化为 hover text，`lsp_local_semantic_query.c` 只作为 query/hover 编排层调用它。WSL gcc 和 WSL clang focused expression/local-query/local-hover/computed-member hover targets 均通过；Windows MSVC focused expression-hover、local-query、computed-member hover targets 通过。当前 `build\codex-semantic-msvc-debug` 的 `zr_vm_language_server_local_semantic_hover_test.exe` 仍会以 `0xc0000005` 崩在 `zr_vm_parser.dll` 偏移 `0x1396a0`，事件日志显示同一崩溃在本 slice 前已存在，因此本轮不把该既有 Windows local-hover suite 当作绿色。

2026-06-05 expression fact rich-hover role 聚焦验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_expression_fact_hover_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_computed_member_hover_test -j 6 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_expression_fact_hover_test && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_query_test && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_hover_test && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_computed_member_hover_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_expression_fact_hover_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_computed_member_hover_test -j 6 && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_expression_fact_hover_test && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_query_test && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_hover_test && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_computed_member_hover_test'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_expression_fact_hover_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_computed_member_hover_test --parallel 6; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_expression_fact_hover_test.exe; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_local_semantic_query_test.exe; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_local_semantic_hover_test.exe; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_computed_member_hover_test.exe
```

RED：扩展 `tests/language_server/test_lsp_expression_fact_hover.c` 后，plain/public hover 已显示 `Expression: binary exact` 和 `Constant: 3`，但 rich-hover 没有稳定 `expression` / `constant` roles。GREEN：`lsp_interface.c` 的既有 label-to-role table 将 `Expression` 映射为 `expression`、`Constant` 映射为 `constant`；没有改变 parser/type-inference fact emission 或 hover formatter。WSL gcc、WSL clang 和 Windows MSVC focused expression/query/local-hover/computed-member hover targets 均通过；本轮仍不声明全仓库绿色。

2026-06-05 parser/LSP missing condition diagnostic 聚焦验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_lsp_interface_test zr_vm_language_server_stdio zr_vm_cli_executable -j 8 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_lsp_interface_test && node tests/language_server/stdio_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_stdio build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_lsp_interface_test zr_vm_language_server_stdio zr_vm_cli_executable -j 8 && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_lsp_interface_test && node tests/language_server/stdio_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_stdio build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_lsp_interface_test zr_vm_language_server_stdio zr_vm_cli_executable --parallel 8; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_lsp_interface_test.exe; node tests\language_server\stdio_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_stdio.exe build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe
```

RED：新增 `if ()` / `while ()` 的 LSP diagnostic 断言后，旧 parser/LSP 路径没有 `missing_condition` code、具体问题文本和建议。GREEN：parser 在 `if` / `while` 的 `(` 之后立即识别空条件，并通过 `ZrParser_DiagnosticBuilder_BuildMissingCondition` 产生结构化诊断；LSP interface 和 stdio JSON 都保留 `missing_condition`、`Missing condition inside ...` 与 `Add a boolean expression between '(' and ')'` 建议。后续 focused parser-diagnostics suite 又覆盖 `if (ready { ... }` / `while (ready { ... }` 的 `missing_condition_close`，并加入 `switch` 条件家族：`switch (choice { ... }` 在条件表达式后报告 `Missing ')' after 'switch' condition`，`switch ()` 在 `(` 后立即报告 `Missing condition inside 'switch'`，都不再走 generic expected-token fallback。WSL gcc、WSL clang 和 Windows MSVC 聚焦 parser diagnostics / LSP interface / stdio smoke 路径均通过；本轮仍不声明全仓库绿色。

2026-06-06 parser/LSP grouped-expression close diagnostic 聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8 && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_parser_diagnostics_test --parallel 8; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe
```

RED：新增 `return (1 + 2` 的 focused parser-diagnostics 断言后，旧 WSL gcc 路径没有 `missing_group_close` code、具体问题文本和建议。GREEN：parser 在非 lambda `(` primary-expression 分支读完 grouped expression 后，如果没有看到 `)`，通过 `ZrParser_DiagnosticBuilder_BuildMissingGroupClose` 产生结构化诊断；LSP diagnostics 保留 `missing_group_close`、`Missing closing ')' in grouped expression` 与 `Insert ')' after the grouped expression before continuing` 建议。WSL gcc、WSL clang 和 Windows MSVC focused `zr_vm_language_server_parser_diagnostics_test` 均通过；本轮仍不声明全仓库绿色。

2026-06-07 parser/LSP function parameter-list close diagnostic 聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8 && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_parser_diagnostics_test --parallel 8; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe
```

RED：新增 `func pick(value: int: int { return value; }` 的 focused parser-diagnostics 断言后，旧 WSL gcc 路径没有 `missing_parameter_list_close` code、具体问题文本和建议。第一次实现后仍只失败该用例；WSL gdb 证明 structured reporter 在 quiet `func` declaration probe 中被调用，但 probe 暂时关闭了 structured callback，导致诊断被丢弃。GREEN：parser 为 top-level function declaration parameter list 增加 `missing_parameter_list_close` builder/reporter，并在 explicit `func name...` quiet probe 失败后用真实 parser path 重跑一次以保留诊断；LSP diagnostics 保留 `missing_parameter_list_close`、`Missing closing ')' in function declaration parameters` 与 `Insert ')' after the parameter list before continuing` 建议。WSL gcc、WSL clang 和 Windows MSVC focused `zr_vm_language_server_parser_diagnostics_test` 均通过；clang/MSVC 仍有当前 dirty tree 中既有 warning noise，本轮仍不声明全仓库绿色。

2026-06-07 parser/LSP class method parameter-list close diagnostic 聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8 && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-semantic-msvc-debug --target zr_vm_language_server_parser_diagnostics_test --config Debug --parallel 8; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe
```

RED：新增 `class Box { func read(value: int: int { return value; } }` 的 focused parser-diagnostics 断言后，旧 WSL gcc 路径只失败该 method case，没有 `missing_parameter_list_close` code、具体问题文本和建议。GREEN：`parse_class_method` 在读完 method parameter list 后，如果没有看到 `)`，会通过既有 `missing_parameter_list_close` reporter 报告结构化诊断并停止进入 return-type/body 解析；function calls 仍保留 `missing_call_close`，grouped expressions 仍保留 `missing_group_close`。WSL gcc、WSL clang 和 Windows MSVC focused `zr_vm_language_server_parser_diagnostics_test` 均通过；MSVC 仍有当前 dirty tree 中既有 AOT runtime warning noise，本轮仍不声明全仓库绿色。

2026-06-11 parser/LSP class setter/meta parameter-list close diagnostic 聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake -S . -B build-wsl-gcc && cmake --build build-wsl-gcc --target zr_vm_language_server_class_member_parser_diagnostics_test -j 8 && ./build-wsl-gcc/bin/zr_vm_language_server_class_member_parser_diagnostics_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_class_member_parser_diagnostics_test -j 8 && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_class_member_parser_diagnostics_test"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_class_member_parser_diagnostics_test --parallel 8; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_class_member_parser_diagnostics_test.exe
```

RED：新增 focused target `zr_vm_language_server_class_member_parser_diagnostics_test` 后，第一次 WSL gcc 运行先证明新 target 需要刷新 `build-wsl-gcc` CMake graph；刷新后 `class Sized { set length(value: int { return value; } }` 旧路径只失败 class-setter parameter-list close case，没有 `missing_parameter_list_close` code、具体问题文本和建议。GREEN：`parse_property_set` 在读完 setter 参数后如果没有看到 `)`，复用既有 `missing_parameter_list_close` reporter 并停止进入 accessor body；随后同一 focused target 加入 `class Callable { @call(value: int: int { return value; } }`，RED 只失败 class-meta case，GREEN：`parse_class_meta_function` 在 primary parameter list 缺少 `)` 时复用同一 structured reporter 并释放已解析 params。新 target 避免继续扩大接近大文件阈值的 `test_lsp_parser_diagnostics.c`。WSL gcc focused target 已通过；完整 focused matrix 见 acceptance 记录。本轮仍不声明全仓库绿色，也不触碰 constructor receiver、SemIR、AOT/value-type、Debug 或 REPL 路径。

2026-06-07 parser/LSP interface method parameter-list close diagnostic 聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8 && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-semantic-msvc-debug --target zr_vm_language_server_parser_diagnostics_test --config Debug --parallel 8; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe
```

RED：新增 `interface Readable { read(value: int: int; }` 的 focused parser-diagnostics 断言后，旧 WSL gcc 路径只失败该 interface method signature case，没有 `missing_parameter_list_close` code、具体问题文本和建议。GREEN：`parse_interface_method_signature` 在读完 signature parameter list 后，如果没有看到 `)`，会通过既有 `missing_parameter_list_close` reporter 报告结构化诊断并停止进入 return-type/where/semicolon 解析；function calls 仍保留 `missing_call_close`，grouped expressions 仍保留 `missing_group_close`。WSL gcc、WSL clang 和 Windows MSVC focused `zr_vm_language_server_parser_diagnostics_test` 均通过；本轮仍不声明全仓库绿色。

2026-06-07 parser/LSP interface meta parameter-list close diagnostic 聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8 && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-semantic-msvc-debug --target zr_vm_language_server_parser_diagnostics_test --config Debug --parallel 8; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe
```

RED：新增 `interface Callable { @call(value: int: int; }` 的 focused parser-diagnostics 断言后，旧 WSL gcc 路径只失败该 interface meta signature case，没有 `missing_parameter_list_close` code、具体问题文本和建议。GREEN：`parse_interface_meta_signature` 在读完 meta signature parameter list 后，如果没有看到 `)`，会通过既有 `missing_parameter_list_close` reporter 报告结构化诊断并停止进入 return-type/semicolon 解析；function calls 仍保留 `missing_call_close`，grouped expressions 仍保留 `missing_group_close`。WSL gcc、WSL clang 和 Windows MSVC focused `zr_vm_language_server_parser_diagnostics_test` 均通过；本轮仍不声明全仓库绿色。

2026-06-07 parser/LSP extern function parameter-list close diagnostic 聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8 && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-semantic-msvc-debug --target zr_vm_language_server_parser_diagnostics_test --config Debug --parallel 8; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe
```

RED：新增 `%extern("fixture") { NativeAdd(value: int: int; }` 的 focused parser-diagnostics 断言后，旧 WSL gcc 路径只失败该 extern function case，没有 `missing_parameter_list_close` code、具体问题文本和建议。GREEN：`parse_extern_function_declaration` 在读完 extern function parameter list 后，如果没有看到 `)`，会通过既有 `missing_parameter_list_close` reporter 报告结构化诊断并停止进入 return-type/semicolon 解析；function calls 仍保留 `missing_call_close`，grouped expressions 仍保留 `missing_group_close`。WSL gcc、WSL clang 和 Windows MSVC focused `zr_vm_language_server_parser_diagnostics_test` 均通过；本轮仍不声明全仓库绿色。

2026-06-07 parser/LSP extern delegate parameter-list close diagnostic 聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && gcc ... tests/language_server/test_lsp_parser_diagnostics.c ... -lzr_vm_language_server -lzr_vm_parser -lzr_vm_core -o build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test_manual_red && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test_manual_red"
wsl bash -lc "cd /mnt/e/Git/zr_vm && ninja -C build/codex-semantic-wsl-gcc-debug zr_vm_parser/CMakeFiles/zr_vm_parser_shared.dir/src/zr_vm_parser/parser/parser_extern.c.o && ninja -C build/codex-semantic-wsl-gcc-debug lib/libzr_vm_parser.so && gcc ... tests/language_server/test_lsp_parser_diagnostics.c ... -lzr_vm_language_server -lzr_vm_parser -lzr_vm_core -o build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test_manual_green && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test_manual_green"
```

RED：新增 `%extern("fixture") { delegate Callback(value: int: int; }` 的 focused parser-diagnostics 断言后，旧 WSL gcc parser/LSP library 路径只失败该 extern delegate case，没有 `missing_parameter_list_close` code、具体问题文本和建议；一次正常 `cmake --build ... --target zr_vm_language_server_parser_diagnostics_test` 先被当前 dirty tree 中活跃 AOT backend signature mismatch 阻断，未执行测试，因此 RED 使用只重编 test harness、复用既有 parser/LSP shared libraries 的方式证明旧行为。GREEN：`parser_extern.c` 抽出本地 `consume_extern_parameter_list_close_or_report`，让 extern function 与 extern delegate declarations 共享同一个 `missing_parameter_list_close` recovery path，delegate parameter list 缺少 `)` 时会在 return-type/semicolon 解析前报告结构化诊断；function calls 仍保留 `missing_call_close`，grouped expressions 仍保留 `missing_group_close`。随后 WSL gcc、WSL clang 和 Windows MSVC normal focused `zr_vm_language_server_parser_diagnostics_test` 均通过；本轮仍不声明全仓库绿色。

2026-06-06 parser/LSP computed object-key close diagnostic 聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-semantic-wsl-gcc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-semantic-wsl-clang-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8 && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake -S . -B build\codex-semantic-msvc-debug -G "Visual Studio 17 2022" -A x64 -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF; cmake --build build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_parser_diagnostics_test --parallel 8; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe
```

RED：新增 `return {[seed: 1}` 的 focused parser-diagnostics 断言后，旧 WSL gcc 路径没有 `missing_object_computed_key_close` code、具体问题文本和建议。GREEN：parser 在 object literal computed-key 分支读完 key expression 后，如果没有看到 `]`，通过 `ZrParser_DiagnosticBuilder_BuildMissingObjectComputedKeyClose` 产生结构化诊断；LSP diagnostics 保留 `missing_object_computed_key_close`、`Missing closing ']' in computed object key` 与 `Insert ']' after the computed key expression before ':'` 建议。WSL gcc、WSL clang 和 Windows MSVC focused `zr_vm_language_server_parser_diagnostics_test` 均通过；由于当前 dirty tree 中 AOT backend 新文件使用 CMake glob，验证前刷新了对应 build graph。本轮仍不声明全仓库绿色。

2026-06-06 parser/LSP array literal separator diagnostic 聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test"
```

RED：新增 `return [1 2];` 的 focused parser-diagnostics 断言后，旧 WSL gcc 路径没有 `missing_array_element_separator` code、具体问题文本和建议，而是退化到 array literal closing bracket 方向。GREEN：parser 在 array literal 读完一个元素后，如果下一个 token 仍可作为表达式起点且没有 `,` / `;` 分隔，通过 `ZrParser_DiagnosticBuilder_BuildMissingArrayElementSeparator` 产生结构化诊断；LSP diagnostics 保留 `missing_array_element_separator`、`Missing separator between array elements` 与 `Insert ',' or ';' between array elements` 建议。WSL gcc、WSL clang 和 Windows MSVC focused `zr_vm_language_server_parser_diagnostics_test` 均通过；本轮仍不声明全仓库绿色。

2026-06-06 parser/LSP object property separator diagnostic 聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test"
```

RED：新增 `{a: 1 b: 2}` 的 focused parser-diagnostics 断言后，旧 WSL gcc 路径没有 `missing_object_property_separator` code、具体问题文本和建议，而是退化到 object literal closing brace 方向。GREEN：parser 在 object literal 读完一个属性 value 后，如果下一个 token 仍可作为 property key 起点且没有 `,` / `;` 分隔，通过 `ZrParser_DiagnosticBuilder_BuildMissingObjectPropertySeparator` 产生结构化诊断；LSP diagnostics 保留 `missing_object_property_separator`、`Missing separator between object properties` 与 `Insert ',' or ';' between object properties` 建议。WSL gcc、WSL clang 和 Windows MSVC focused `zr_vm_language_server_parser_diagnostics_test` 均通过；本轮仍不声明全仓库绿色。

2026-06-06 parser/LSP statement semicolon diagnostic 聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8 && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_parser_diagnostics_test --parallel 8; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe
```

RED：新增 `return 1\nvar next = 2;` / `1 + 2\nvar next = 3;` 的 focused parser-diagnostics 断言后，旧 WSL gcc 路径没有 `missing_statement_semicolon` code、具体问题文本和建议。GREEN：parser 在 return / expression statement 成功解析 body 后，如果当前 token 不是 `;`，通过 `ZrParser_DiagnosticBuilder_BuildMissingStatementSemicolon` 产生 statement-specific 结构化诊断；LSP diagnostics 保留 `missing_statement_semicolon`、`Missing ';' after return statement` / `Missing ';' after expression statement` 与补 `;` 的建议。后续同一 focused suite 又加入 loop-control 语句：`break\nvar next = 2;` 和 `continue\nvar next = 2;` 会先识别下一行已开始新 statement，不再把 `var next` 当作 loop-control expression，而是分别报告 `Missing ';' after break statement` / `Missing ';' after continue statement`。后续继续把 `throw 1\nvar next = 2;` 和 `out 1\nvar next = 2;` 接到同一路径：`parse_throw_statement` / `parse_out_statement` 在表达式 body 解析完成后报告 `Missing ';' after throw statement` / `Missing ';' after out statement`，而不是走 generic expected-token fallback。本轮又加入单表达式 `%using` declaration：`%using resource\nvar next = 2;` 在 resource 表达式解析完成后报告 `Missing ';' after using statement`，避免 `parse_using_statement_body` 的 generic semicolon expectation 淹没真实修复建议。WSL gcc、WSL clang 和 Windows MSVC focused `zr_vm_language_server_parser_diagnostics_test` 均通过；本轮仍不声明全仓库绿色。

2026-06-07 parser/LSP interface method signature semicolon diagnostic 聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8 && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-semantic-msvc-debug --target zr_vm_language_server_parser_diagnostics_test --config Debug --parallel 8; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe
```

RED：新增 `interface Readable { read(value: int): int }` 的 focused parser-diagnostics 断言后，旧 WSL gcc 路径只失败该 interface-method-signature semicolon case，没有 `missing_statement_semicolon` code、具体问题文本和建议。GREEN：`parse_interface_method_signature` 在读完参数、返回类型和 where clause 后，如果当前 token 不是 `;`，复用现有 structured statement terminator reporter 并传入 `interface method signature` statement kind；LSP diagnostics 保留 `missing_statement_semicolon`、`Missing ';' after interface method signature statement` 和补 `;` 的建议。WSL gcc、WSL clang 和 Windows MSVC focused `zr_vm_language_server_parser_diagnostics_test` 均通过；本轮仍不声明全仓库绿色，也不触碰 AOT/value-type/core runtime、Debug 或 REPL 路径。

2026-06-07 parser/LSP interface meta signature semicolon diagnostic 聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8 && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-semantic-msvc-debug --target zr_vm_language_server_parser_diagnostics_test --config Debug --parallel 8; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe
```

RED：新增 `interface Callable { @call(value: int): int }` 的 focused parser-diagnostics 断言后，旧 WSL gcc 路径只失败该 interface-meta-signature semicolon case，没有 `missing_statement_semicolon` code、具体问题文本和建议。GREEN：`parse_interface_meta_signature` 在读完参数列表和返回类型后，如果当前 token 不是 `;`，复用现有 structured statement terminator reporter 并传入 `interface meta signature` statement kind；LSP diagnostics 保留 `missing_statement_semicolon`、`Missing ';' after interface meta signature statement` 和补 `;` 的建议。第一次 GREEN 尝试误改了 property signature semicolon branch，focused target 仍只失败新 meta-signature case；最终修正为恢复 property branch 并只修改 meta-signature terminator。WSL gcc、WSL clang 和 Windows MSVC focused `zr_vm_language_server_parser_diagnostics_test` 均通过；本轮仍不声明全仓库绿色，也不触碰 AOT/value-type/core runtime、Debug 或 REPL 路径。

2026-06-07 parser/LSP interface property signature semicolon diagnostic 聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8 && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-semantic-msvc-debug --target zr_vm_language_server_parser_diagnostics_test --config Debug --parallel 8; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe
```

RED：新增 `interface Sized { get length: int }` 的 focused parser-diagnostics 断言后，旧 WSL gcc 路径只失败该 interface-property-signature semicolon case，没有 `missing_statement_semicolon` code、具体问题文本和建议。GREEN：`parse_interface_property_signature` 在读完 property name/type 后，如果当前 token 不是 `;`，复用现有 structured statement terminator reporter 并传入 `interface property signature` statement kind；同时 interface member dispatch 直接接受 line-start `get` / `set`，让无访问修饰符 property signature 进入 property parser，而不是落入 unknown-member fallback。第一次 GREEN 尝试只改了 terminator 后仍失败，证明 fixture 暴露了 dispatch gap；补上 direct `get` / `set` dispatch 后 WSL gcc focused target 通过。WSL clang 和 Windows MSVC focused validation 同步覆盖本 slice；本轮仍不声明全仓库绿色，也不触碰 AOT/value-type/core runtime、Debug 或 REPL 路径。

2026-06-07 parser/LSP interface field declaration semicolon diagnostic 聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8 && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-semantic-msvc-debug --target zr_vm_language_server_parser_diagnostics_test --config Debug --parallel 8; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe
```

RED：新增 `interface Entity { var id: int }` 的 focused parser-diagnostics 断言后，旧 WSL gcc 路径只失败该 interface-field-declaration semicolon case，没有 `missing_statement_semicolon` code、具体问题文本和建议。GREEN：`parse_interface_field_declaration` 在读完 field name/type 后，如果当前 token 不是 `;`，复用现有 structured statement terminator reporter 并传入 `interface field declaration` statement kind；LSP diagnostics 保留 `missing_statement_semicolon`、`Missing ';' after interface field declaration statement` 和补 `;` 的建议。WSL gcc、WSL clang 和 Windows MSVC focused `zr_vm_language_server_parser_diagnostics_test` 均通过；本轮仍不声明全仓库绿色，也不触碰 AOT/value-type/core runtime、Debug 或 REPL 路径。

2026-06-07 parser/LSP class field declaration semicolon diagnostic 聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8 && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-semantic-msvc-debug --target zr_vm_language_server_parser_diagnostics_test --config Debug --parallel 8; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe
```

RED：新增 `class Entity { var id: int }` 的 focused parser-diagnostics 断言后，旧 WSL gcc 路径只失败该 class-field semicolon case，没有 `missing_statement_semicolon` code、具体问题文本和建议。GREEN：`parse_class_field` 在读完 field name/type/default/where 后，如果当前 token 不是 `;`，复用现有 structured statement terminator reporter 并传入 `class field declaration` statement kind；LSP diagnostics 保留 `missing_statement_semicolon`、`Missing ';' after class field declaration statement` 和补 `;` 的建议。该 slice 只触碰 class field terminator，不触碰 constructor receiver、SemIR、AOT/value-type、Debug 或 REPL 路径；WSL gcc、WSL clang 和 Windows MSVC focused `zr_vm_language_server_parser_diagnostics_test` 均通过，本轮仍不声明全仓库绿色。

2026-06-07 parser/LSP class getter/setter accessor semicolon diagnostic 聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8 && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-semantic-msvc-debug --target zr_vm_language_server_parser_diagnostics_test --config Debug --parallel 8; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe
```

RED：新增 `class Sized { get length: int }` 的 focused parser-diagnostics 断言后，旧 WSL gcc 路径只失败该 class-getter semicolon case，没有 `missing_statement_semicolon` code、具体问题文本和建议；同一 slice 也加入 `class Sized { set length(value: int) }` 覆盖。GREEN：class member dispatch 会把 line-start `get` / `set` 送入 property parser，`parse_property_get` / `parse_property_set` 在 accessor signature 后如果既不是 `;` 也不是 `{ ... }` body，就复用 structured statement terminator reporter 并分别传入 `class getter` / `class setter` statement kind；合法 body accessor 仍走原 block 解析。WSL gcc、WSL clang 和 Windows MSVC focused `zr_vm_language_server_parser_diagnostics_test` 均通过；本轮仍不声明全仓库绿色，也不触碰 constructor receiver、SemIR、AOT/value-type、Debug 或 REPL 路径。

2026-06-07 parser/LSP class method/meta function semicolon diagnostic 聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_parser_diagnostics_test -j 8 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_parser_diagnostics_test -j 8 && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-semantic-msvc-debug --target zr_vm_language_server_parser_diagnostics_test --config Debug --parallel 8; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe
```

RED：新增 `class Box { func read(value: int): int }` 的 focused parser-diagnostics 断言后，旧 WSL gcc 路径只失败该 class-method semicolon case；随后新增 `class Callable { @call(value: int): int }` 也确认旧路径只失败 class-meta-function semicolon case。GREEN：`parse_class_method` / `parse_class_meta_function` 在 signature 后如果既不是 `;` 也不是 `{ ... }` body，就复用 structured statement terminator reporter 并分别传入 `class method` / `class meta function` statement kind；合法 body method/meta 仍走原 block 解析。WSL gcc、WSL clang 和 Windows MSVC focused `zr_vm_language_server_parser_diagnostics_test` 均通过；本轮仍不声明全仓库绿色，也不触碰 constructor receiver、SemIR、AOT/value-type、Debug 或 REPL 路径。

2026-06-07 parser/LSP class/interface/function/enum/extern/test declaration body-open diagnostic 聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test -j 8 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_declaration_parser_diagnostics_test && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test -j 8 && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_declaration_parser_diagnostics_test && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-semantic-msvc-debug --target zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test --config Debug --parallel 8; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_declaration_parser_diagnostics_test.exe; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe
```

RED：新增独立 focused target `zr_vm_language_server_declaration_parser_diagnostics_test` 和 `class Box` 断言后，旧 WSL gcc 路径只失败该 class declaration body-open case，没有 `missing_declaration_body_open` code、具体问题文本和建议；随后扩展 `interface Sized` 时，RED 只失败 interface declaration body-open case；再扩展 `func pick(): int` 时，RED 只失败 function declaration body-open case；继续扩展 `enum Tone` 时，RED 只失败 enum declaration body-open case；扩展 `%extern("fixture")` 时，RED 只失败 extern block body-open case；扩展 `%test("smoke")` 时，RED 只失败 test declaration body-open case。GREEN：新增 `ZrParser_DiagnosticBuilder_BuildMissingDeclarationBodyOpen` / `report_missing_declaration_body_open`，并让 `parse_class_declaration`、`parse_interface_declaration`、`parse_function_declaration`、`parse_enum_declaration`、`parse_extern_block` 和 `parse_test_declaration` 在 header 后未看到 `{` 时报告 `missing_declaration_body_open`，其中 extern block 仍保留 brace-less single extern member fallback，合法 `%test(...) { ... }` 仍走原 block parser；同时保留旧 `zr_vm_language_server_parser_diagnostics_test` 作为回归。WSL gcc、WSL clang 和 Windows MSVC focused declaration/parser diagnostics targets 均通过；本轮仍不声明全仓库绿色，也不触碰 constructor receiver、SemIR、AOT/value-type、Debug 或 REPL 路径。MSVC 输出包含既有 AOT/runtime 与 parser/LSP warning，本轮未新增失败。

同日补充 class/interface declaration body-close RED 时，`class Box { var id: int;` 旧 WSL gcc 路径只失败 class declaration body-close case，没有 `missing_declaration_body_close` code、具体问题文本和建议；随后同一 focused test 扩展 `interface Sized { get length: int;`。GREEN：新增 `ZrParser_DiagnosticBuilder_BuildMissingDeclarationBodyClose` / `report_missing_declaration_body_close`，并让 `parse_class_declaration` 和 `parse_interface_declaration` 只在已看到 body-opening `{` 且输入结束前仍未看到 closing `}` 时报告 `missing_declaration_body_close`，避免 `class Box` / `interface Sized` opener 缺失场景被误报为 close 缺失。WSL gcc `build-wsl-gcc` focused declaration diagnostics target 先通过；随后 WSL gcc、WSL clang 和 Windows MSVC focused statement/declaration/parser diagnostics targets 均通过。本轮仍不声明全仓库绿色，也不触碰 constructor receiver、SemIR、AOT/value-type、Debug 或 REPL 路径。

同日继续补充 enum/extern declaration body-close RED 时，`enum Tone { warm,` 旧 WSL gcc 路径只失败 enum declaration body-close case，没有 `missing_declaration_body_close` code、具体问题文本和建议；同一 focused test 也加入 `%extern("fixture") { NativeAdd(value: int): int;`。GREEN：`parse_enum_declaration` 和 `parse_extern_block` 记录真实 body-opening `{` 位置，并在输入结束前仍未看到 closing `}` 时复用 `report_missing_declaration_body_close`，同时保留 `enum Tone` / `%extern("fixture")` opener 缺失场景的 `missing_declaration_body_open`。`parser_extern.c` 已超过大文件阈值，本 slice 只替换既有局部 expected-token fallback，extern parser 责任拆分仍是后续 cleanup boundary。WSL gcc `build-wsl-gcc` focused declaration diagnostics target 先通过；随后 WSL gcc、WSL clang 和 Windows MSVC focused statement/declaration/parser diagnostics targets 均通过。本轮仍不声明全仓库绿色，也不触碰 constructor receiver、SemIR、AOT/value-type、Debug 或 REPL 路径。

同日继续补充 function/test declaration body-close RED 时，`func pick(): int { return 1;` 旧 WSL gcc 路径只失败 function declaration body-close case，因为该路径只暴露 generic `missing_block_close`，没有 declaration-specific `missing_declaration_body_close` code、具体问题文本和建议；同一 focused test 也加入 `%test("smoke") { return 1;`。GREEN：`parse_block` 保持原 generic block-close 入口，同时抽出 `parse_declaration_body_block` 让 `parse_function_declaration` 和 `parse_test_declaration` 在输入结束前仍未看到 closing `}` 时复用 `report_missing_declaration_body_close`，普通 statement block、lambda block、method body 和 control-flow block 仍保留 `missing_block_close`。`parser_statements.c` 已 1446 行，本 slice 只抽出 declaration-aware block wrapper，statement parser 责任拆分仍是后续 cleanup boundary；`parser_extern.c` 仍保持最小 `%test` body 调用点改动。WSL gcc `build-wsl-gcc` focused declaration diagnostics target 先通过；随后 WSL gcc、WSL clang 和 Windows MSVC focused statement/declaration/parser diagnostics targets 均通过。本轮仍不声明全仓库绿色，也不触碰 constructor receiver、SemIR、AOT/value-type、Debug 或 REPL 路径。

同日补充 `%extern("fixture" { NativeAdd(value: int): int; }` RED 时，旧路径只失败 extern spec close case；GREEN 新增 `ZrParser_DiagnosticBuilder_BuildMissingExternSpecClose` / `report_missing_extern_spec_close`，并让 `parse_extern_block` 在 extern spec 到达 extern block body 前仍未看到 `)` 时报告 `missing_extern_spec_close` 且释放已解析 library spec literal。WSL gcc、WSL clang 和 Windows MSVC focused statement/declaration/parser diagnostics targets 均通过；Clang/MSVC 仍只输出既有 warning。

2026-06-07 parser/LSP if/while/for/foreach/switch/switch-case/switch-default/else/try/catch/finally/using statement body-open/block-close/catch-pattern-close/using-resource-close diagnostic 聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_statement_parser_diagnostics_test zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test -j 8 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_statement_parser_diagnostics_test && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_declaration_parser_diagnostics_test && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_statement_parser_diagnostics_test zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test -j 8 && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_statement_parser_diagnostics_test && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_declaration_parser_diagnostics_test && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; cmake --build build\codex-semantic-msvc-debug --target zr_vm_language_server_statement_parser_diagnostics_test zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test --config Debug --parallel 8; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_statement_parser_diagnostics_test.exe; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_declaration_parser_diagnostics_test.exe; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe
```

RED：新增独立 focused target `zr_vm_language_server_statement_parser_diagnostics_test` 和 `if (ready)\nreturn 1;` 断言后，旧 WSL gcc 路径只失败 if statement body-open case，没有 `missing_statement_body_open` code、具体问题文本和建议。GREEN：新增 `ZrParser_DiagnosticBuilder_BuildMissingStatementBodyOpen` / `report_missing_statement_body_open`，并让 `parse_if_expression`、`parse_while_loop` 和传统 `parse_for_loop` 在 header 后未看到 `{` 时报告 `missing_statement_body_open`；`for` 覆盖使用 `for (;;)\nreturn 1;`，避免当前 `for (var ...)` / `foreach` 分流实现干扰这个 body-opener slice。后续扩展 `switch (choice)\nreturn 1;` RED 时，旧路径只失败 switch statement body-open case；GREEN 让 `parse_switch_expression` 在 header 后未看到 `{` 时复用同一结构化诊断。继续扩展 `switch (choice) { (1)\nreturn 1; }` 和 `switch (choice) { ()\nreturn 1; }` RED 时，旧路径先失败 switch case statement body-open case；GREEN 让 `parse_switch_expression` 在普通 case/default case header 后未看到 `{` 时复用同一结构化诊断，并清理已解析的 switch expression、case value 和 case array。再扩展 `if (ready) { return 1; } else\nreturn 2;` RED 时，旧路径只失败 else statement body-open case；GREEN 让 `parse_if_expression` 的 plain `else` branch 在进入 block parser 前复用同一结构化诊断，同时保留 `else if` 的 nested-if path。继续扩展 `for (var item in items)\nreturn item;` RED 时，旧路径只失败 foreach statement body-open case；GREEN 让 `parse_foreach_loop` 在 header 后未看到 `{` 时复用同一结构化诊断，并清理已解析的 pattern/type/iterable expression。继续扩展 `try\nreturn 1;`、`try { throw 1; } catch (error)\nreturn 2;` 和 `try { return 1; } finally\nreturn 2;` RED 时，旧路径先失败 try statement body-open case；GREEN 让 `parse_try_catch_finally_statement` 在 try/catch/finally header 后未看到 `{` 时复用同一结构化诊断，并清理已解析的 try/catch AST pieces。继续扩展 `%using (resource)\nreturn resource;` RED 时，旧路径只失败 using statement body-open case；GREEN 让 `parse_using_statement_body` 的 block-scoped `using (...)` branch 在 header 后未看到 `{` 时复用同一结构化诊断，并清理已解析 resource。继续扩展 `if (ready) { return 1;` RED 时，旧路径只失败 missing block close case；GREEN 新增 `ZrParser_DiagnosticBuilder_BuildMissingBlockClose` / `report_missing_block_close`，并让 `parse_block` 在输入结束前仍未看到 `}` 时报告 `missing_block_close` 且释放已解析 statement array。继续扩展 `try { throw 1; } catch (error { return 2; }` RED 时，旧路径只失败 catch pattern close case；GREEN 新增 `ZrParser_DiagnosticBuilder_BuildMissingCatchPatternClose` / `report_missing_catch_pattern_close`，并让 `parse_try_catch_finally_statement` 在 catch pattern 到达 catch body 前仍未看到 `)` 时报告 `missing_catch_pattern_close` 且释放已解析 catch pattern、已有 catch clauses 和 try block。WSL gcc、WSL clang 和 Windows MSVC focused statement/declaration/parser diagnostics targets 均通过；本轮仍不声明全仓库绿色，也不触碰 constructor receiver、SemIR、AOT/value-type、Debug 或 REPL 路径。Clang/MSVC 输出包含既有 warning，本轮未新增失败。

同日补充 `%using (resource { return resource; }` RED 时，旧路径只失败 using resource close case；GREEN 新增 `ZrParser_DiagnosticBuilder_BuildMissingUsingResourceClose` / `report_missing_using_resource_close`，并让 `parse_using_statement_body` 在 using resource 到达 using body 前仍未看到 `)` 时报告 `missing_using_resource_close` 且释放已解析 resource。WSL gcc、WSL clang 和 Windows MSVC focused statement/declaration/parser diagnostics targets 均通过；Clang/MSVC 仍只输出既有 warning。

同日补充 `switch (choice) { (1 { return 1; } }` RED 时，旧路径只失败 switch case header close case；GREEN 新增 `ZrParser_DiagnosticBuilder_BuildMissingSwitchCaseHeaderClose` / `report_missing_switch_case_header_close`，并让 `parse_switch_expression` 在普通 switch case expression 到达 case body 前仍未看到 `)` 时报告 `missing_switch_case_header_close` 且释放已解析 switch expression、case value 和 case array。WSL gcc、WSL clang 和 Windows MSVC focused statement/declaration/parser diagnostics targets 均通过；Clang/MSVC 仍只输出既有 warning。

同日补充 `switch (choice) { (1) { return 1; }` RED 时，旧路径只失败 switch body close case；GREEN 新增 `ZrParser_DiagnosticBuilder_BuildMissingSwitchBodyClose` / `report_missing_switch_body_close`，并让 `parse_switch_expression` 在 switch body 到达输入结束前仍未看到最终 `}` 时报告 `missing_switch_body_close` 且释放已解析 switch expression、case array 和 default case。WSL gcc statement-only focused 验证已通过；完整 statement/declaration/parser diagnostics 三平台 focused 结果记录在 acceptance evidence。

同日补充 `for (var item in items { return item; }` RED 时，旧路径只失败 foreach header close case；GREEN 新增 `ZrParser_DiagnosticBuilder_BuildMissingForeachHeaderClose` / `report_missing_foreach_header_close`，并让 `parse_foreach_loop` 在 iterable expression 到达 loop body 前仍未看到 `)` 时报告 `missing_foreach_header_close` 且释放已解析 pattern、type 和 iterable expression。WSL gcc statement-only focused 验证已通过；完整 statement/declaration/parser diagnostics 三平台 focused 结果记录在 acceptance evidence。

同日补充 `for (var item items) { return item; }` RED 时，旧路径只失败 foreach `in` keyword case；GREEN 新增 `ZrParser_DiagnosticBuilder_BuildMissingForeachInKeyword` / `report_missing_foreach_in_keyword`，并让 `parse_foreach_loop` 在 pattern 后未看到 `in` 时报告 `missing_foreach_in_keyword` 且释放已解析 pattern/type。WSL gcc statement-only focused 验证已通过；完整 statement/declaration/parser diagnostics 三平台 focused 结果记录在 acceptance evidence。

同日补充 `for (var i = 0 i < 3; i = i + 1) { out i; }` RED 时，旧顶层 statement 分流把所有 `for (var ...)` 当作 foreach，因此新增用例没有得到 `missing_for_header_separator`。GREEN 新增共享的 `parser_for_header_should_parse_foreach` 分流 helper，让顶层和 block 内 `for` 都按 header 内 token 判断：看到 `in` 走 foreach，看到 `=` 或 `;` 走 traditional for，没有 `in/=/;` 的 `for (var pattern ...)` 仍保留 malformed foreach 以维持 `missing_foreach_in_keyword`。同时 `parse_variable_declaration_for_header` 让 traditional for 变量初始化由 `parse_for_loop` 报告 `missing_for_header_separator`，不再泄露普通变量声明缺分号诊断。WSL gcc statement-only focused 验证已通过；完整三平台 focused 结果记录在 acceptance evidence。

2026-06-05 signatureHelp call-comment code-span 过滤聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && ninja -C build/codex-semantic-wsl-gcc-debug -j2 zr_vm_language_server_lsp_interface_test && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_lsp_interface_test"
wsl bash -lc 'cd /mnt/e/Git/zr_vm && ninja -C build/codex-semantic-wsl-clang-debug -j2 zr_vm_language_server_lsp_interface_test && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_lsp_interface_test > /tmp/zr_lsp_interface_clang.out && if grep -q "Fail -" /tmp/zr_lsp_interface_clang.out; then cat /tmp/zr_lsp_interface_clang.out; exit 1; fi; tail -40 /tmp/zr_lsp_interface_clang.out'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_lsp_interface_test --parallel 6; if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }; $out = Join-Path $env:TEMP 'zr_lsp_interface_msvc.out'; & .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_lsp_interface_test.exe *> $out; if (Select-String -Path $out -Pattern 'Fail -' -Quiet) { Get-Content $out; exit 1 }; Get-Content $out -Tail 40
```

RED：新增 `/* pick(99) is prose */` 位于 call argument list 内的 `textDocument/signatureHelp` 断言后，旧 WSL gcc LSP interface 输出 `pick(value: int): int`，证明 AST call range 内的 comment 正文仍会触发签名。GREEN：`lsp_signature_help.c` 在 call-context AST 匹配前调用 `ZrLanguageServer_Lsp_IsCursorOffsetInCodeSpan`；comment/string 正文返回空 signature help，真实 `pick(result)` argument 仍返回 `pick(value: int): int`。WSL gcc、WSL clang 和 Windows MSVC focused `zr_vm_language_server_lsp_interface_test` 均通过且输出无 `Fail -`；本轮不声明全仓库绿色。

2026-06-05 receiver/member definition non-code 过滤聚焦验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && ninja -C build/codex-semantic-wsl-gcc-debug -j2 zr_vm_language_server_lsp_advanced_editor_features_test && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_lsp_advanced_editor_features_test > /tmp/zr_lsp_adv_gcc.out; status=$?; if grep -q "Fail -" /tmp/zr_lsp_adv_gcc.out; then cat /tmp/zr_lsp_adv_gcc.out; exit 1; fi; tail -40 /tmp/zr_lsp_adv_gcc.out; exit $status'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && ninja -C build/codex-semantic-wsl-clang-debug -j2 zr_vm_language_server_lsp_advanced_editor_features_test && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_lsp_advanced_editor_features_test > /tmp/zr_lsp_adv_clang.out; status=$?; if grep -q "Fail -" /tmp/zr_lsp_adv_clang.out; then cat /tmp/zr_lsp_adv_clang.out; exit 1; fi; tail -40 /tmp/zr_lsp_adv_clang.out; exit $status'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_lsp_advanced_editor_features_test --parallel 6; if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }; $out = Join-Path $env:TEMP 'zr_lsp_adv_msvc.out'; & .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_lsp_advanced_editor_features_test.exe *> $out; $status = $LASTEXITCODE; if (Select-String -Path $out -Pattern 'Fail -' -Quiet) { Get-Content $out; exit 1 }; Get-Content $out -Tail 40; exit $status
```

RED：新增 `// box.value` 和 `"box.value ..."` 的 `textDocument/definition` 断言后，旧 WSL gcc focused advanced-editor target 失败并报告 `definition resolved receiver-member text from a non-code span`，证明 receiver/member fallback 会把 comment/string 正文中的 `box.value` 解析成真实 member target。GREEN：`lsp_semantic_query.c` 在 receiver/member semantic query fallback 入口调用 shared code-span check；非代码 span 返回无 definition，真实 `return box.value;` 仍能跳转到 `value` member。WSL gcc、WSL clang 和 Windows MSVC focused `zr_vm_language_server_lsp_advanced_editor_features_test` 均通过且输出无 `Fail -`；本轮不声明全仓库绿色。

2026-06-05 call hierarchy non-code call mention 过滤聚焦验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && ninja -C build/codex-semantic-wsl-gcc-debug -j2 zr_vm_language_server_lsp_advanced_editor_features_test && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_lsp_advanced_editor_features_test > /tmp/zr_lsp_adv_hierarchy_gcc.out; status=$?; if grep -q "Fail -" /tmp/zr_lsp_adv_hierarchy_gcc.out; then cat /tmp/zr_lsp_adv_hierarchy_gcc.out; exit 1; fi; tail -40 /tmp/zr_lsp_adv_hierarchy_gcc.out; exit $status'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && ninja -C build/codex-semantic-wsl-clang-debug -j2 zr_vm_language_server_lsp_advanced_editor_features_test && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_lsp_advanced_editor_features_test > /tmp/zr_lsp_adv_hierarchy_clang.out; status=$?; if grep -q "Fail -" /tmp/zr_lsp_adv_hierarchy_clang.out; then cat /tmp/zr_lsp_adv_hierarchy_clang.out; exit 1; fi; tail -40 /tmp/zr_lsp_adv_hierarchy_clang.out; exit $status'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_lsp_advanced_editor_features_test --parallel 6; if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }; $out = Join-Path $env:TEMP 'zr_lsp_adv_hierarchy_msvc.out'; & .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_lsp_advanced_editor_features_test.exe *> $out; $status = $LASTEXITCODE; if (Select-String -Path $out -Pattern 'Fail -' -Quiet) { Get-Content $out; exit 1 }; Get-Content $out -Tail 40; exit $status
```

RED：新增 `// helper(value)` 和 `"helper(value) ..."` 的 call hierarchy 断言后，旧 WSL gcc focused advanced-editor target 失败并报告 `call hierarchy counted comment or string text as a call`，证明 incoming/outgoing raw call scan 会把 comment/string 正文中的 `helper(value)` 统计成调用边。GREEN：`lsp_hierarchy.c` 在 incoming 和 outgoing raw call scan 计数前调用 `lsp_editor_offset_is_code`；非代码 span 不再生成 call edge，已有真实直接调用测试仍通过。WSL gcc、WSL clang 和 Windows MSVC focused `zr_vm_language_server_lsp_advanced_editor_features_test` 均通过且输出无 `Fail -`；本轮不声明全仓库绿色。

2026-06-05 CodeLens `%test(...)` non-code 过滤聚焦验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && ninja -C build/codex-semantic-wsl-gcc-debug -j2 zr_vm_language_server_lsp_advanced_editor_features_test && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_lsp_advanced_editor_features_test > /tmp/zr_lsp_adv_codelens_gcc.out; status=$?; if grep -q "Fail -" /tmp/zr_lsp_adv_codelens_gcc.out; then cat /tmp/zr_lsp_adv_codelens_gcc.out; exit 1; fi; tail -45 /tmp/zr_lsp_adv_codelens_gcc.out; exit $status'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && ninja -C build/codex-semantic-wsl-clang-debug -j2 zr_vm_language_server_lsp_advanced_editor_features_test && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_lsp_advanced_editor_features_test > /tmp/zr_lsp_adv_codelens_clang.out; status=$?; if grep -q "Fail -" /tmp/zr_lsp_adv_codelens_clang.out; then cat /tmp/zr_lsp_adv_codelens_clang.out; exit 1; fi; tail -45 /tmp/zr_lsp_adv_codelens_clang.out; exit $status'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_lsp_advanced_editor_features_test --parallel 6; if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }; $out = Join-Path $env:TEMP 'zr_lsp_adv_codelens_msvc.out'; & .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_lsp_advanced_editor_features_test.exe *> $out; $status = $LASTEXITCODE; if (Select-String -Path $out -Pattern 'Fail -' -Quiet) { Get-Content $out; exit 1 }; Get-Content $out -Tail 45; exit $status
```

RED：新增 comment/string 中 `%test(...)` 的 CodeLens 断言后，旧 WSL gcc focused advanced-editor target 失败并报告 `non-code %test text produced a run CodeLens`，证明 raw `%test(` marker scan 会把说明文本暴露成可运行命令。GREEN：`lsp_editor_features.c` 在生成 run CodeLens 前调用 `lsp_editor_offset_is_code`；真实 `%test("advanced")` 仍生成 `zr.runCurrentProject`，非代码 `%test(...)` 不再生成 run lens。WSL gcc、WSL clang 和 Windows MSVC focused `zr_vm_language_server_lsp_advanced_editor_features_test` 均通过且输出无 `Fail -`；Clang/MSVC 因 dirty tree glob mismatch 重新配置后通过，本轮不声明全仓库绿色。
