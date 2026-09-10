---
related_code:
  - docs/zr_language_specification.md
  - zr_vm_parser/include/zr_vm_parser/lexer.h
  - zr_vm_parser/include/zr_vm_parser/parser.h
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expressions.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_declarations.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_types.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/lexer.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expressions.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_declarations.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_types.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/zr_language_specification.md
tests:
  - tests/parser/test_parser.c
  - tests/parser/test_syntax_reference_v1.c
  - tests/fixtures/projects/syntax_reference_v1
doc_type: language-reference
---

# ZR 语法总表

本页是面向查表的语法索引。文中的 EBNF 只描述 parser 当前能够建立的 AST 形状；泛型约束、
所有权、调用 effect、provider 能力和后端可执行性由 semantic/compiler 阶段继续验证。
因此“解析成功”不是“程序一定可以编译或运行”的同义词。

## 记号约定

| 写法 | 含义 |
| --- | --- |
| `"token"` | 必须出现的关键字或标点。 |
| `name`、`expr`、`TypeRef` | 语法变量。 |
| `[x]` | 可选一次。 |
| `{x}` | 重复零次或多次；不是 ZR 源码中的花括号。 |
| `x | y` | 二选一。 |
| `/* ... */` | 注释，不能当作语法 token。 |

源码需要显式分号；换行不会自动插入分号。块由 `{` 和 `}` 界定，字符串和模板中的换行
仍属于 literal 内容，除非 lexer 报告未闭合错误。

## 源文件和模块

```ebnf
source-file       = [module-declaration], {top-level-item} ;
module-declaration = "module", module-path, ";" ;
module-path       = identifier, {".", identifier} ;
top-level-item    = import-declaration
                   | using-declaration
                   | attribute
                   | type-declaration
                   | function-declaration
                   | binding-declaration
                   | statement ;
import-declaration = "import", "(", string-literal, ")" ;
```

`import` 在当前 parser 中只接受圆括号包裹的字符串 literal，例如
`import("zr.system.fs")`。不接受裸标识符、字符串拼接或运行时变量。模块名必须由
ASCII 风格 identifier 以点连接；相对路径、alias、workspace/package 解析在 Library
resolver 阶段完成，不能由字符串拼接替代。

```zr
module app.main;
let fs = import("zr.system.fs");
let helper = import("./helper");
```

以下写法会在 parser 阶段失败：

```zr
let name = "zr.system.fs";
let fs = import(name);       // import 参数不是 string literal
let fs = import "zr.system.fs"; // 缺少括号
```

## 标识符、关键字和可选词

标识符首字符必须满足 lexer 的 identifier-start 规则，后续字符可包含数字和下划线。关键字
由 lexer 直接产生 token，包括 `module`、`struct`、`class`、`interface`、`enum`、`union`、
`fn`、`let`、`var`、`using`、`if`、`else`、`while`、`for`、`switch`、`try`、`catch`、
`finally`、`return`、`throw`、`break`、`continue`、`yield`、`async`、`await`、`comptime`、
`native`、`extern`、`ref`、`typeid`、`typeof`、`share`、`degrade`、`wake`、`intoGc`、`drop`、
`test` 和 `intermediate` 等。`import`、`init`、`own`、`readonly`、`scoped`、`resource` 等
词在部分语境按 contextual identifier 处理，最终由 parser 分支决定是否具有特殊含义。

关键字区分大小写。普通 identifier 不能重定义为同一 scope 中已占用的声明；遮蔽规则和
访问级别由 binder 再验证。旧的 `out` 模板插值形式和 `{{ ... }}` 输出语法已经移除，
不要把历史文档中的示例复制到新代码。

## 声明

### 绑定

```ebnf
binding-declaration = ["pub" | "private" | "protected"],
                      ("let" | "var"), pattern, [":", TypeRef],
                      ["=", expr], ";" ;
pattern             = identifier
                    | "_"
                    | array-pattern
                    | object-pattern ;
array-pattern       = "[", [pattern, {",", pattern}], "]" ;
object-pattern      = "{", [identifier, {",", identifier}], "}" ;
```

`let` 绑定不能被重新赋值，`var` 可以。带初始化表达式的绑定在声明点求值；没有显式类型
时由 semantic inference 推导。模式绑定的每个名称都进入当前 lexical scope，重复名称和
不可达模式会在 binder/flow 阶段诊断。

### 函数和 callable type

```ebnf
function-declaration = [visibility], ["async"], "fn", identifier,
                       [generic-parameters], "(", [parameters], ")",
                       [":", TypeRef], [where-clauses], block ;
function-type        = "fn", "(", [parameters], ")", "->", TypeRef ;
parameters            = parameter, {",", parameter}, [",", "...", identifier] ;
parameter             = [passing-mode], identifier, ":", TypeRef, ["=", expr] ;
passing-mode          = "in" | "ref" | "out" ;
```

函数声明的返回类型使用冒号；函数类型使用箭头。匿名表达式函数可以使用 `=>`：

```zr
fn add(a: int, b: int): int { return a + b; }
let twice: fn(int) -> int = (x: int) => x * 2;
```

`async fn` 必须显式返回 `zr.task.Task<T>`，不能依靠普通 `T` 推断为任务。`ref`/`out`
参数的可写性、逃逸和调用方 place 要求不在 parser 中决定。

### 类型声明

```ebnf
type-declaration = struct-declaration
                  | class-declaration
                  | interface-declaration
                  | enum-declaration
                  | union-declaration ;
struct-declaration = [visibility], ["resource"], "struct", identifier,
                     [generic-parameters], [extends-clause], [implements-clause],
                     "{", {field-or-method}, "}" ;
class-declaration  = [visibility], ["abstract"], "class", identifier,
                     [generic-parameters], [extends-clause], [implements-clause],
                     "{", {field-or-method}, "}" ;
interface-declaration = [visibility], "interface", identifier,
                        [generic-parameters], [extends-clause], "{", {method-signature}, "}" ;
enum-declaration   = [visibility], "enum", identifier, "{", enum-member, {",", enum-member}, [","], "}" ;
union-declaration  = [visibility], "union", identifier, "{", {field-or-method}, "}" ;
field-or-method    = field-declaration | function-declaration | property-declaration ;
field-declaration  = [visibility], ("let" | "var"), identifier, ":", TypeRef, ["=", expr], ";" ;
```

`virtual`、`override`、`final`、`shadow`、`abstract` 是 declaration modifier；它们的组合
由继承和 interface contract 检查。`resource` 类型会获得 Drop/cleanup 约束，不能当普通
可复制值隐式复制。

## 类型引用

```ebnf
TypeRef          = primitive-type
                 | qualified-type
                 | generic-type
                 | array-type
                 | tuple-type
                 | function-type
                 | ownership-type
                 | "typeof", "(", expr, ")"
                 | "typeid", "(", TypeRef, ")" ;
primitive-type   = "void" | "bool" | "char" | "wchar" | "string"
                 | "i8" | "i16" | "i32" | "i64" | "u8" | "u16" | "u32" | "u64"
                 | "int" | "uint" | "float" | "double" ;
qualified-type   = identifier, {".", identifier} ;
generic-type     = qualified-type, "<", TypeRef, {",", TypeRef}, ">" ;
array-type       = TypeRef, "[", "]" ;
tuple-type       = "(", TypeRef, {",", TypeRef}, ")" ;
ownership-type   = ("own" | "share" | "weak" | "readonly" | "scoped"), TypeRef ;
```

注意：`T?` 不是当前源语法中普通 TypeRef 的可选后缀。API 表格为了表达“可省略参数”或
“可能返回 null”会使用 `?` 记法；源代码应使用 descriptor 要求的默认值、`null` 和
`?.`/guard 结构。类型 identity 会保留泛型实参和 ownership qualifier，不能只按显示名称
比较。

## 语句和控制流

```ebnf
statement      = block | binding-declaration | expression-statement
               | if-statement | while-statement | for-statement
               | switch-statement | try-statement | using-statement
               | return-statement | throw-statement | jump-statement ;
block          = "{", {statement}, "}" ;
if-statement    = "if", "(", expr, ")", statement, ["else", statement] ;
while-statement = "while", "(", expr, ")", statement ;
for-statement   = "for", "(", for-init, ";", [expr], ";", [expr], ")", statement
               | "for", "(", ("let" | "var"), pattern, "in", expr, ")", statement ;
switch-statement = "switch", "(", expr, ")", "{", {switch-arm}, "}" ;
switch-arm      = "(", pattern, ")", block | "(", ")", block ;
try-statement   = "try", block, {"catch", "(", parameters, ")", block}, ["finally", block] ;
using-statement = "using", "(", using-resource, ")", statement, ["else", statement] ;
return-statement = "return", [expr], ";" ;
throw-statement  = "throw", expr, ";" ;
jump-statement   = ("break" | "continue"), [identifier], ";"
                 | "yield", expr, ";" ;
```

`switch` 的空 `()` arm 是 default；它不是省略括号的 `else`。C 风格 `for` 的三个槽位
都必须保留两个分号，foreach 形式使用 `in`。`try` 可以有多个 `catch`，匹配顺序由异常
类型关系决定；`finally` 无论正常返回还是 pending control 都要执行。

## 表达式优先级

从高到低可按下表理解；同一行通常从左到右结合，赋值类从右到左：

| 级别 | 形式 |
| --- | --- |
| 1 | literal、identifier、`init Type(...)`、数组/对象 literal、`(expr)` |
| 2 | postfix call `f(...)`、index `a[i]`、member `a.b`、optional member `a?.b` |
| 3 | unary `+ - ! ~`、ownership intrinsic `share/degrade/wake/intoGc/drop` |
| 4 | `* / %` |
| 5 | `+ -` |
| 6 | shifts |
| 7 | relational `< <= > >=` |
| 8 | equality `== !=` |
| 9 | bitwise `& ^ |` |
| 10 | logical `&& ||` |
| 11 | conditional `?:` |
| 12 | assignment `= += -= *= /=` |

调用、索引和 property 链在 parser 中先形成 postfix AST，再由 semantic phase 选择普通
函数、native binding、meta method 或 property reference。短路逻辑只对 `&&` 和 `||` 保证
右侧按需求值；其它运算符的求值顺序以 compiler 生成的 call/CFG 为准。

## 字面量

- 整数支持十进制、八进制和 `0x` 十六进制；超出目标类型范围在 literal typing 阶段报错。
- 浮点支持小数点和指数，`f/F`、`d/D` 后缀用于指定精度；`NaN`/`Inf` 由 math provider
  的常量提供，不是 lexer 的普通 identifier literal。
- 字符使用单引号，字符串使用双引号；反斜杠转义在 lexer 中解码并保留 source range。
- 反引号模板保留插值边界，插值表达式仍按普通 expression 解析；未闭合模板会把错误
  定位到开始 token。

## 解析阶段的恢复和诊断

parser 在 `;`、块结束 `}`、catch/finally 边界和顶层 declaration 边界尝试同步。诊断包含
code、severity、location、message 和可选 fixes；恢复后 AST 节点会带 invalid/recovered 标记。
宿主不应把 recovered AST 当作可编译输入，也不应在 parser state 释放后保存未复制的 range
或 message 指针。

完整可执行正负样例的入口说明见[构建与验证](../04-tools/build-validation.md)；源码 fixture
位于仓库的 `tests/fixtures/projects/syntax_reference_v1`。另见[诊断与错误恢复](diagnostics.md)。解析 API 的 C 调用顺序见
[Parser/Compiler C API](../05-interop/c-api-parser.md)。
