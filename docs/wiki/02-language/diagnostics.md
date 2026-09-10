---
related_code:
  - zr_vm_parser/include/zr_vm_parser/parser.h
  - zr_vm_parser/include/zr_vm_parser/location.h
  - zr_vm_parser/include/zr_vm_parser/diagnostic_builder.h
  - zr_vm_parser/include/zr_vm_parser/legacy_migration.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
  - tests/fixtures/projects/syntax_reference_v1/golden/diagnostics.json
  - tests/fixtures/projects/syntax_reference_v1/negative/function_delimiters.zr
  - tests/fixtures/projects/syntax_reference_v1/negative/legacy_percent_surface.zr
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/parser/parser_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c
  - zr_vm_parser/src/zr_vm_parser/parser.c
tests:
  - tests/parser/test_parser.c
  - tests/parser/test_syntax_reference_v1.c
  - tests/parser/test_semantic_query_diagnostics.c
  - tests/fixtures/projects/syntax_reference_v1/golden/diagnostics.json
  - tests/fixtures/projects/syntax_reference_v1/negative/function_delimiters.zr
  - tests/fixtures/projects/syntax_reference_v1/negative/legacy_percent_surface.zr
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/zr_language_specification.md
doc_type: language-reference
---

# 诊断与错误恢复

ZR parser 的错误输出有两条兼容路径：旧的文本 `TZrParserErrorCallback` 和结构化的 `TZrParserStructuredErrorCallback`。新工具、LSP、CLI 和迁移器应优先消费 `SZrStructuredDiagnostic`，因为只有它能稳定表达错误 code、原因、建议、source span、related information 和可应用 fix。

## 诊断数据结构

```c
typedef struct SZrStructuredDiagnostic {
    EZrStructuredDiagnosticSeverity severity;
    SZrFileRange location;
    SZrString *code;
    SZrString *message;
    SZrString *cause;
    SZrString *suggestion;
    SZrArray relatedInformation;
    SZrArray fixes;
    TZrUInt32 descriptorId;
    EZrDiagnosticNoFixReason noFixReason;
} SZrStructuredDiagnostic;
```

`location` 的 start/end 同时含 byte offset、1-based line/column 和 source name。`message` 面向当前用户，`code` 用于机器分类；`cause`/`suggestion` 用于编辑器提示。fix 记录 title、edit range、edit text 和 applicability；不能安全自动修改时应设置 `noFixReason`，而不是伪造一个空 edit。

## Parser 生命周期与 callback

```c
SZrParserState parser;
TZrPtr diagnosticUserData = ZR_NULL; /* host-owned sink, if needed */

ZrParser_State_Init(&parser, state, source, sourceLength, sourceName);
parser.structuredErrorCallback = onStructuredDiagnostic;
parser.errorUserData = diagnosticUserData;

SZrAstNode *ast = ZrParser_ParseWithState(&parser);
if (parser.hasFatalError || ast == ZR_NULL) {
    /* consume diagnostics; do not inspect a partial AST */
}
ZrParser_Ast_Free(state, ast);
ZrParser_State_Free(&parser);
```

上例中的 `diagnosticUserData`/`onStructuredDiagnostic` 是宿主自己的 sink 和函数；公共 parser 只定义 callback 签名。callback 在 parser state 有效期间同步调用，diagnostic 指针及其中的 string/array 借用当前回调窗口；若要跨线程或跨 generation 保存，使用 `ZrParser_StructuredDiagnostic_Copy`，并在同一 state/global 释放副本。

`hasError` 表示遇到普通语法错误，`hasFatalError` 表示 production parser 遇到被删除的 legacy syntax，不能再把返回的 AST 当作可编译输入。调用 `ZrParser_ParseTopLevelStatementWithState` 或 `ZrParser_ParseExpressionWithState` 做增量工具时，同样要检查当前 state 和 callback 结果。

## 常见 parser code

| code/类别 | 触发条件 | 推荐修复 |
| --- | --- | --- |
| `function_definition_return_delimiter` | 函数声明使用 `->`/`=>` 代替返回类型 `:`。 | 写 `fn name(...): Type { ... }`。fixture 中的负例会验证 range。 |
| `legacy_migration_required` / `legacy_syntax_removed` | `%module`、`%owned`、旧 `func`、`$Type(...)`、双大括号 generator 等旧表面。 | 按 suggestion 迁移到当前语法；不要关闭 production fatal 检查。 |
| missing statement semicolon | 简单语句、`return`、`throw`、`yield` 或 bodyless declaration 缺 `;`。 | 在上一个 token 结束处插入分号。builder 通常提供 machine-applicable fix。 |
| missing condition/body/close | `if/while/for/switch` 括号或 body brace 缺失。 | 根据 location 补 `)`/`{`/`}`；先修外层 delimiter。 |
| `missing_right_operand` | 二元操作符后没有 expression。 | 补右操作数或删除多余操作符。 |
| import path not constant | `import` 的括号内不是 string literal。 | 改为 `import("literal.path")`，动态加载走 runtime API。 |
| `ownership_intrinsic_call_required` / arity mismatch | `share`/`drop` 等被当作值、零参数、多参数或 named argument。 | 传恰好一个位置 owner expression。 |
| `using_binder_invalid` / `using_else_without_guard` | using pattern 不能绑定，或无 guard 却有 `else`。 | 使用 `using (let name = expr) { ... } else { ... }` 或移除 else。 |

完整 code 文本由 `diagnostic_builder.h` 与 `parser_diagnostics.c` 维护；表格只列长期稳定的迁移/结构类别，不应在工具中用 message 字符串做唯一判断。

## 诊断渲染

CLI 常见的人类可读形状如下（列号和 caret 以真实 source range 为准）：

```text
src/main.zr:1:35: error[function_definition_return_delimiter]
  fn invalid(value: int) -> int {
                                  ^
  cause: function declarations use ':' before the return TypeRef
  help: write fn invalid(value: int): int { ... }
```

编辑器应同时展示 `code`、主 range、relatedInformation 和可用 fix；不要把 `cause` 或 `suggestion` 拼回 source 再次解析。source name 为空时仍应显示 line/column，不能用 offset 代替用户位置。

## 错误恢复边界

parser 在可恢复位置会跳到匹配的 `;`、`)`、`]`、`}` 或 `EOS`，以便收集后续 diagnostic；但 production parser 遇到 legacy syntax 会设置 fatal 标记，并拒绝把部分 AST 送入 semantic lowering。嵌套模板插值、function parameter、switch case 和 using pattern 的恢复通常以其局部 delimiter 为界，外层工具不应假设每个错误都能继续到文件末尾。

恢复后得到的 AST 只适合错误显示或迁移 preview。编译、写 `.zri/.zro`、执行或 LSP semantic snapshot 前必须确认 `hasError == false`（以及 `hasFatalError == false`），并释放失败路径上的 AST/diagnostic 所有权。

## 迁移器与 production parser

迁移器可以将 `enableLegacyMigrationParsing` 设为 true，捕获 `%module`、旧 property、旧 return delimiter 等结构并生成带 placeholder 的 fix；普通 parser 默认 false。迁移输出的 fix applicability 可能是 `HAS_PLACEHOLDERS` 或 `MAYBE_INCORRECT`，宿主必须让用户确认后再写回文件。不要为了让旧 fixture 通过而在 production parser 中重新启用 legacy AST。

## C API 速查

| API | 作用 |
| --- | --- |
| `ZrParser_State_Init/Free` | 初始化/释放 lexer、source range 和 callback 状态。 |
| `ZrParser_ParseWithState` | 解析完整 source，返回 caller-owned AST。 |
| `ZrParser_ParseTopLevelStatementWithState` | 增量解析一个顶层 statement。 |
| `ZrParser_ParseExpressionWithState` | 增量解析一个完整 expression。 |
| `ZrParser_StructuredDiagnostic_Init/Free/Copy` | 初始化、释放或跨窗口复制 diagnostic。 |
| `ZrParser_DiagnosticBuilder_Build*` | 为 parser/semantic 工具构造带 code/cause/suggestion/fix 的 diagnostic。 |
| `ZrParser_Ast_Free` | 在同一 state/global 释放 AST。 |

更完整的 parser/compiler 调用顺序和 writer/artifact 入口见 [Parser/Compiler C API](../05-interop/c-api-parser.md)。
