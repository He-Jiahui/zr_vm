---
related_code:
  - zr_vm_parser/include/zr_vm_parser/lexer.h
  - zr_vm_parser/src/zr_vm_parser/lexer.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_literals.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
  - tests/fixtures/projects/syntax_reference_v1/surface/lexical_and_literals.zr
  - tests/fixtures/projects/syntax_reference_v1/src/semicolon_explicit.zr
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/lexer.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_literals.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
tests:
  - tests/parser/test_parser.c
  - tests/parser/test_syntax_reference_v1.c
  - tests/fixtures/projects/syntax_reference_v1/golden/coverage.json
  - tests/fixtures/projects/syntax_reference_v1/golden/diagnostics.json
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/zr_language_specification.md
doc_type: language-reference
---

# 词法与字面量

本页描述 lexer 在进入 parser 前如何把源文本切成 token。词法阶段只负责 token、字面量值和 source range；它不决定变量类型、所有权或调用重载。注释和空白会被丢弃，但行号、列号和字节偏移会保留在后续 diagnostic 的 range 中。

## 源字符与 trivia

生产 lexer 从 ASCII 字母或 `_` 开始识别名称，数字和操作符按单字节扫描。UTF-8 文本可以安全地出现在注释和字符串值中；非 ASCII 字符不要用作标识符首字符，除非宿主先经过与当前 lexer 一致的扩展。空格、制表符、回车和换行都是 whitespace。换行只更新 source location，**不会触发自动分号插入**。

```zr
// 行注释直到换行结束
/* 块注释可以跨行 */
let answer = 40 + 2; /* 注释不改变语句边界 */
```

`//` 和 `/* ... */` 在 lexer 中统一跳过。未闭合的块注释会读到文件末尾；parser 随后只能看到 `EOS`，因此应把它视为源文件词法错误，而不是依赖错误恢复继续执行。

## 标识符

```ebnf
identifier = ( "A".."Z" | "a".."z" | "_" ),
             { "A".."Z" | "a".."z" | "0".."9" | "_" } ;
```

标识符区分大小写，`value`、`Value` 和 `VALUE` 是三个不同名称。当前扫描器不会把连字符、空格或 `.` 包进名称；`app.main` 会被切成 `identifier`, `.`, `identifier`，由 module/import parser 再组合成路径。

### 直接关键字

以下拼写在 lexer 中直接产生专用 token，不能在普通名称位置当作变量名使用：

| 类别 | 拼写 |
| --- | --- |
| 类型/声明 | `module`, `struct`, `class`, `abstract`, `virtual`, `override`, `final`, `shadow`, `interface`, `enum`, `union`, `fn`, `ref` |
| 反射/所有权内建 | `typeid`, `typeof`, `share`, `degrade`, `wake`, `intoGc`, `drop` |
| 语句 | `var`, `let`, `using`, `if`, `else`, `switch`, `while`, `for`, `break`, `continue`, `return`, `yield`, `throw`, `try`, `catch`, `finally` |
| 可见性/成员 | `pub`, `pri`, `pro`, `static`, `const`, `get`, `set`, `in`, `out`, `new`, `super` |
| 工具/测试/常量 | `test`, `intermediate`, `Infinity`, `NegativeInfinity`, `NaN`, `true`, `false`, `null` |

`test` 在少数成员和 module path 位置会被 parser 规范化为普通名称 `test`；这不是把所有关键字都解禁。`share`、`degrade`、`wake`、`intoGc`、`drop` 是保留的 ownership intrinsic 名称，不能声明同名字段或函数。

### 上下文拼写

`import`、`init`、`own`、`readonly`、`scoped`、`async`、`await`、`comptime`、`native`、`extern`、`resource` 在 lexer 中仍是 `identifier`。parser 只在特定位置赋予它们语义：

```zr
let moduleValue = import("zr.system");
var point = init Point(1, 2);
var handle = own FileHandle();
async fn fetch(): Task<int> { return await next(); }
```

因此，关键字表和 parser 上下文必须同时看。把 `async` 误当成 lexer token、或在任意位置把 `import` 当作普通函数，都会得到与预期不同的 AST。公共 API 名称应避免复用这些保留拼写，以便未来语法扩展。

## 数字字面量

```ebnf
integer = decimal | octal | hexadecimal ;
decimal = digit, { digit } ;
octal = "0", octal-digit, { octal-digit } ;
hexadecimal = ("0x" | "0X"), hex-digit, { hex-digit } ;
float = digits, [ ".", digits ], [ ("e" | "E"), ["+"|"-"], digits ],
        [ "f" | "F" | "d" | "D" ] ;
```

实际扫描规则如下：

| 写法 | 示例 | 结果 |
| --- | --- | --- |
| 十进制整数 | `0`, `42`, `9001` | `ZR_TK_INTEGER`，以十进制解析。 |
| 八进制整数 | `077` | `ZR_TK_INTEGER`，以八进制解析；`08` 不应当用作八进制写法。 |
| 十六进制整数 | `0x2a`, `0XFF` | `ZR_TK_INTEGER`，使用 `strtoull(..., 16)`。 |
| 小数 | `3.14`, `1.` | `ZR_TK_FLOAT`。 |
| 指数 | `6.02e23`, `1e-3` | `ZR_TK_FLOAT`。 |
| 类型后缀 | `1.0f`, `2d` | `ZR_TK_FLOAT`；后缀影响 literal metadata，最终类型仍由语义阶段决定。 |

负数不是独立 token：`-7` 会先产生 `-`，再由 parser 建立 unary expression。十六进制路径不会消费小数点或指数；不要写带 `_` 分隔符的数字，当前 lexer 没有数字分隔符规则。跨 C ABI 传递时，建议在 TypeRef 或 API descriptor 中明确 `i32`、`i64`、`u64` 等宽度。

## 字符串、字符和模板

普通字符串使用双引号，字符使用单引号；两者都不允许未转义的换行或文件结束：

```zr
let text = "line\\nnext\\tcolumn";
let quote = '\\'';
let unicode = "\\u4e2d";
let byte = "\\x7f";
```

支持的常用转义包括 `\n`、`\t`、`\r`、`\b`、`\f`、`\"`、`\'`、`\\`、`\uXXXX` 和 `\xXX`。当前实现把 `\uXXXX`/`\xXX` 转换为单个内部字符单元；需要完整 UTF-8 编码时，应通过标准库或宿主 API 处理，不要假定一个 code unit 等于一个 Unicode scalar。

模板字符串使用反引号，可以跨行，`${...}` 插值由 parser 在 lexer 产生的原始模板 token 上二次拆分：

```zr
let name = "ZrVm";
let greeting = `hello ${name},
line two`;
```

模板中的静态片段成为字符串 AST 节点，插值片段会调用嵌套 parser 解析为 expression。嵌套表达式必须在自身范围内完整结束；未闭合 `}`、反引号或字符串会使整个模板 literal 失败。反引号和反斜杠可用 `\``、`\\` 转义。

特殊常量 `true`、`false`、`null`、`Infinity`、`NegativeInfinity` 和 `NaN` 由 lexer 直接分类；它们不是可重新绑定的普通变量。

## 操作符与标点

lexer 先识别最长的多字符操作符，再识别单字符操作符。常见 token 包括：

```text
=>  ->  ==  !=  <=  >=  <<  >>  &&  ||  +=  -=  *=  /=  %=
?.  ?   :   .   ,   ;   ( )  [ ]  { }  + - * / % & | ^ ! ~
```

`=>` 只用于匿名函数 expression body；函数声明返回类型使用 `:`，function type 使用 `->`。`?.[` 被 parser 明确拒绝，必须拆成普通索引或先检查对象。`%` 保留为取模和取模赋值；它不再是 module/import/ownership 指令前缀。

## 分号和语句边界

简单声明、绑定、表达式语句、`return`、`throw`、`break`、`continue`、`yield` 和无函数体声明需要显式 `;`：

```zr
var count: int = 0;
count += 1;
return count;
```

块、class/struct/interface/union body 和 `switch` case body 由 `{}` 结束，不额外要求块后分号。`if`、`while`、`for`、`foreach`、`try` 和 `using (...) {}` 的控制结构也以 body brace 作为边界。换行不会把两行自动拼成合法语句：

```zr
// 错误：下一行不是隐式分号
var first = 1
var second = 2;
```

parser 会在发现下一 token 或 `EOS` 时发出 `missing_statement_semicolon` 类错误；具体 code、行列和修复建议见[诊断与错误恢复](diagnostics.md)。

## 词法错误排查

1. 先确认引号、反引号、块注释是否闭合。
2. 再检查数字是否混用了八进制/十六进制规则，是否把 `08` 当作八进制。
3. 检查上下文拼写所在位置：`import("...")`、`init Type(...)`、`async fn` 和 `native extern("...")` 不能任意改成普通调用。
4. 检查语句结尾是否有 `;`，以及 `?.` 后面是否为成员名或 `(`。

最小复现可直接使用 fixture 中的 `tests/fixtures/projects/syntax_reference_v1/surface/lexical_and_literals.zr` 和 `tests/fixtures/projects/syntax_reference_v1/src/semicolon_explicit.zr`。这些路径由 front matter 的 `related_code` 记录，在网页中作为源码证据显示，不是运行时模块导入路径。
