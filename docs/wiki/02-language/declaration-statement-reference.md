---
related_code:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/parser.h
  - zr_vm_parser/include/zr_vm_parser/lexer.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_declarations.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_loops.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_extern.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_property.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_struct.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_class.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_interface.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_union.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_types.c
  - zr_vm_parser/include/zr_vm_parser/compiler.h
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/parser/parser_declarations.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_loops.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_extern.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_property.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_struct.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_class.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_interface.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_union.c
plan_sources:
  - user: 2026-09-09 继续完善 ZrVm Wiki，要求详细介绍语法规则、用例和实现机制
  - docs/zr_language_specification.md
  - tests/fixtures/projects/syntax_reference_v1/syntax_reference_v1.zrp
tests:
  - tests/parser/test_parser.c
  - tests/parser/test_parser_extern.c
  - tests/parser/test_union.c
  - tests/parser/test_property_unified_ast.c
  - tests/parser/test_percent_syntax_cutover.c
  - tests/parser/test_syntax_reference_v1.c
doc_type: language-reference
---

# 声明与语句语法参考

本页是当前 ZR parser 的逐项语法参考。它描述的是 checkout 中真正被
`ZrParser_Parse` 接受的形式，并同时解释节点如何进入 `SZrAstNode`、语义检查和
compiler。示例中的空格和换行可以自由调整，但分号、括号和大括号是语法的一部分。
词法层的字符、数字、字符串和注释规则见[词法结构](lexical-structure.md)，类型和所有权
限定见[类型与所有权](types-ownership.md)。

## 1. 记法和解析边界

本页使用如下 EBNF 约定：`[]` 表示可选，`{}` 表示重复，`|` 表示选择，尖括号中的
名称是非终结符。`identifier` 代表当前 lexer 允许的普通标识符；`test` 在模块路径和
表达式的若干位置被保留为兼容的路径段。生产 parser 不执行自动插入分号（ASI）：会
主动检查语句末尾的 `;`，缺失时通过结构化 diagnostic 报告。

```
script          ::= [module-declaration] {top-level-statement} EOS
module-declaration ::= "module" dotted-identifier ";"
block           ::= "{" {statement} "}"
statement       ::= declaration | expression-statement | control-statement
                    | using-statement | return-statement | throw-statement
                    | break-continue-statement | yield-statement
```

解析器的三个公共粒度不同：

| 入口 | 输入游标 | 返回值 | 适用场景 |
| --- | --- | --- | --- |
| `ZrParser_Parse` | 新建 parser state | `ZR_AST_SCRIPT` | 编译完整源文件 |
| `ZrParser_ParseTopLevelStatementWithState` | 当前 token | 一个顶层声明/语句 | REPL、增量编辑、测试 |
| `ZrParser_ParseExpressionWithState` | 当前 token | 一个表达式节点 | LSP 查询和表达式片段 |

`ZrParser_ParseWithState` 在连续错误过多时会尝试恢复；如果命中已删除语法，
`hasFatalError` 会阻止 production AST 返回。只有显式 migration frontend 才可以打开
`enableLegacyMigrationParsing` 捕获这些节点用于修复建议。

## 2. 模块声明

### 2.1 语法

```
module-declaration ::= "module" module-segment {"." module-segment} ";"
module-segment    ::= identifier | "test"
```

示例：

~~~zr
module app.server;
module test.fixtures;
~~~

parser 将点分路径规范化为一个字符串节点并写入 `SZrScript.moduleName`。模块声明只能
出现在脚本开头；`module "app.server";`、`module(app.server);` 等字符串/括号形式属于
旧语法，会得到 `legacy_syntax_removed`，不会被当作当前模块名。缺少路径段、点后没有
段或缺少分号分别触发模块路径和语句分号诊断。

## 3. 变量、模式与函数声明

### 3.1 变量声明

```
variable-declaration ::= [access] ("var" | "let" | "const") pattern
                         [":" TypeRef] ["=" expression] ";"
pattern              ::= identifier | object-pattern | array-pattern
object-pattern       ::= "{" [pattern-entry {("," | ";") pattern-entry}] "}"
array-pattern        ::= "[" [identifier {"," identifier}] "]"
```

`let` 和 `const` 在 AST 中都设置 `SZrVariableDeclaration.isConst`；当前迁移规则推荐
只写 `let` 表示不可重新赋值。`var const x` 被明确拒绝，不能把 `const` 当作 `var` 的
后缀。类型和值都可省略，因此下面三种形式分别表示推断、显式类型、延迟初始化：

~~~zr
var count = 0;
let limit: int = 10;
var socket: Socket;
let {host, port} = endpoint;
var [first, second] = pair;
~~~

对象模式的 key/value 可以不同名（`{remote: local}`），数组模式按位置绑定。parser 只
负责构造 `ZR_AST_VARIABLE_DECLARATION` 和模式树；是否允许未初始化变量、模式形状是否
匹配、所有权是否可移动由 compiler 的 `ValidateVariableDeclaration`、类型推断和 flow
阶段决定。赋值号后直接出现 `;` 或 EOS 会生成“缺少表达式”诊断。

### 3.2 普通函数

```
function-declaration ::= [decorator] [access] ["async"] "fn" identifier
                         [generic-declaration] "(" [parameter-list] ["," "..." parameter] ")"
                         [":" TypeRef] [where-clauses] block
parameter            ::= ["const"] identifier [":"]
                         ["in" | "out" | "ref" | "scoped"] TypeRef
                         ["=" expression]
```

当前函数声明强制使用 `fn` 和冒号返回类型分隔符：

~~~zr
pub fn clamp(value: int, low: int = 0, high: int = 100): int {
    if (value < low) { return low; }
    if (value > high) { return high; }
    return value;
}

fn fold(values: in Span<int>, ...rest: int): int {
    return 0;
}
~~~

`in`、`out`、`ref`、`scoped` 是参数的 source passing form，不是函数体内的普通变量
修饰词。源码通常写成 `value: out int`；passing mode 位于冒号之后、类型之前，
因此 `out value: int` 不是同一 grammar production。它们被存入 `SZrParameter.passingMode`、`sourcePassingForm` 和位置范围，供
canonical type、借用检查、native descriptor 和 AOT contract 共用。可变参数只能有一个，
并且必须放在参数列表最后。

`async` 是上下文识别的 identifier，而不是 lexer 的独立关键字；`async fn` 的
`SZrFunctionDeclaration.isAsync` 会被 compiler 转成 task effect。当前语义要求 async
函数显式返回 `zr.task.Task<T>`，遗漏或返回普通类型会在 task effect 检查阶段失败。
函数类型和匿名函数使用不同箭头，见[表达式与调用](expressions.md)和[运算符与转换](operators-conversions.md)。

### 3.3 泛型和 where 约束

```
generic-declaration ::= "<" generic-parameter {"," generic-parameter} ">"
generic-parameter   ::= identifier | "const" identifier ":" "int"
where-clauses       ::= "where" constraint {"," constraint}
constraint          ::= identifier ":" TypeRef {"+" TypeRef}
```

函数、class、interface、union 和函数类型可以带泛型。泛型参数节点记录约束、variance、
是否要求 class/struct/new/owner；`parse_optional_where_clauses` 只把语法附着到 generic
节点，真正的约束满足在 canonical type 和 type inference 阶段判断。空尖括号、重复参数
和 `where` 引用未知参数属于语义错误而不是 lexer 错误。

## 4. 类型声明

### 4.1 struct

```
struct-declaration ::= [decorator] [access] ["readonly"] ["ref"] "struct" identifier
                       [generic-declaration] [where-clauses] "{" {struct-member} "}"
struct-member      ::= field | method | property | meta-function
field              ::= [decorator] [access] ["static"] ("var" | "let" | "const") identifier
                       [":" TypeRef] ["=" expression] ";"
```

示例：

~~~zr
pub readonly struct Point {
    let x: int;
    let y: int;
    fn length(): float { return 0.0; }
    @toString(): string { return "Point"; }
}
~~~

`readonly struct` 和 `ref struct` 分别设置 `isReadonly`、`isRefLike`；它们会影响 inline
layout、借用 region 和 FFI lowering。字段的 `static`、`let`、装饰器和初始化表达式都在
`SZrStructField` 中保留。struct 当前不解析 class 风格的继承列表；实现文件会创建空的
`inherits` 数组，协议关系由语义层发布。

### 4.2 class

```
class-declaration ::= [decorator] [access] ["abstract" | "final"] "class" identifier
                      [generic-declaration] [":" TypeRef {"," TypeRef}] [where-clauses]
                      "{" {class-member} "}"
class-member      ::= field | method | property | meta-function
```

示例：

~~~zr
pub abstract class Reader: Disposable {
    var position: int = 0;
    virtual fn read(count: int): Span<byte> { return []; }
    property length: int { get => position; }
}
~~~

class 的 `modifierFlags` 记录 `abstract`/`final`，成员再记录 `virtual`、`override`、
`shadow` 等 flag。`isOwned` 不是 `owned class` 的旧语法开关，而是后续语义投影的字段；
当前源码中声明初值为 false。构造、继承循环、override 槽位和布局一致性由 compiler 和
prototype builder 检查。

### 4.3 interface

```
interface-declaration ::= [access] "interface" identifier [generic-declaration]
                          [":" TypeRef {"," TypeRef}] [where-clauses]
                          "{" {interface-member} "}"
interface-member     ::= field-signature | method-signature
                          | property-signature | meta-signature
method-signature      ::= [access] ["const"] "fn" member-name "(" [parameter-list] ")"
                          [":" TypeRef] ";"
property-signature    ::= [access] "property" identifier ":" TypeRef
                          "{" ["get;"] ["set;"] "}"
```

interface 方法签名必须带 `fn`，返回类型仍使用 `:`。`SZrInterfacePropertySignature` 只
记录 `hasGet`/`hasSet`；实现类的 accessor body 不属于 interface AST。接口 meta signature
用 `@name(...)` 语法表达协议所需的元操作，但实现是否可调用由 canonical protocol mask
决定。

### 4.4 enum

```
enum-declaration ::= [decorator] [access] "enum" identifier [":" ("int" | "uint" | "float" | "string" | "bool")]
                    "{" enum-member {("," | ";") enum-member} [("," | ";")] "}"
enum-member      ::= [decorator] identifier ["=" expression]
```

~~~zr
enum Status: int {
    Pending = 0,
    Ready,
    Failed = 9,
}
~~~

enum member 的 value 可省略；具体的自动编号和底层值校验由 compiler 完成。extern enum
还可以通过装饰器提供 native 名称和 ABI 映射，见[Native Module 编写](../05-interop/native-module-authoring.md)。

### 4.5 union

```
union-declaration ::= [decorator] [access] "union" identifier [generic-declaration]
                     [where-clauses] "{" union-variant {union-variant} "}"
union-variant      ::= [decorator] ["@"] identifier
                       ["(" union-field-list ")" | "{" union-field-list "}"] ["," | ";"]
union-field        ::= [decorator] identifier ":" TypeRef
```

~~~zr
union Result<T> {
    @ok(value: T)
    error(code: int, message: string)
}
~~~

没有 payload 的 variant 是 unit；圆括号是 tuple payload，大括号是 struct payload。
variant 前的 `@` 只能有一个，用于标记 default-using variant；重复标记会产生
`union_default_variant_duplicate`。switch/using 的模式 parser 会复用这些 variant 节点，
因此字段顺序和字段名会直接影响后续 pattern shape 检查。

## 5. 属性、meta 方法和装饰器

### 5.1 统一 property

```
property-declaration ::= [decorator] [access] ["static"] [member-modifier]
                        "property" identifier ":" TypeRef "{" accessor {accessor} "}"
accessor             ::= [access] ("get" | "set" | "init")
                        (";" | "=>" ["ref"] expression ";" | block)
```

~~~zr
property value: int {
    get => backing;
    set { backing = value; }
}
~~~

getter/setter/init 的 body 可以是空声明、表达式 body 或 block body；表达式 accessor 的
`=> ref` 会设置 `isReferenceResult`。parser 统一生成 `ZR_AST_PROPERTY_DECLARATION` 和
`ZR_AST_PROPERTY_ACCESSOR`，旧的分离 `get value: T`/`set value: T` 形式只用于 migration
诊断。`ZrParser_Compiler_BindPropertyDeclaration` 再把 accessor 投影成 getter/setter
method descriptor、reference access 和布局槽位。

### 5.2 meta 方法

meta 方法以 `@` 开头，存入 `SZrStructMetaFunction`、`SZrClassMetaFunction` 或 interface
meta signature。可用的运行时 meta 类型由 `zr_meta_conf.h` 定义，包括 constructor、
destructor、算术、比较、转换、`CALL`、getter/setter、index、`CLOSE` 等。普通方法名可以
包含 `test` 等上下文标识符，但不能借此绕过访问和 contract 检查。

装饰器使用 `#...#`：

~~~zr
#zr.testing.test# fn smoke(): void { }
#zr.compile.declaration.transform# fn make_patch(view: DeclarationView): DeclarationPatch { }
~~~

装饰器 parser 只建立表达式节点；attribute role、target、retention、重复性和 provider
phase 由 descriptor 或 compile-tool contract 验证。未知 role 不会自动变成运行时语义。

## 6. native extern 和编译期声明

### 6.1 Native extern

```
extern-block ::= "native" "extern" "(" string-literal ")"
                 ("{" {extern-member} "}" | extern-member)
extern-member ::= [decorator] [access] "fn" identifier "(" [parameter-list] ")"
                  [":" TypeRef] ";"
                | "delegate" identifier "(" [parameter-list] ")" [":" TypeRef] ";"
                | struct-declaration | enum-declaration
```

~~~zr
native extern("libcrypto") {
    pub fn hash(data: Span<byte>): Span<byte>;
    delegate Callback(value: int): void;
}
~~~

library spec 必须是字符串 literal，extern member 的 `fn` 不能省略。parser 会将函数、
delegate、struct、enum 分别映射到 `ZR_AST_EXTERN_*` 或普通类型 AST；compiler 的
`CompileExternBlock` 和 FFI contract 再核对 calling convention、decorator 和参数 passing
mode。它不会在 parser 阶段加载动态库。

### 6.2 comptime

```
comptime-declaration ::= "comptime" (function-declaration | if-expression | block | expression)
```

~~~zr
comptime {
    let feature = 1;
}
comptime if (feature == 1) { fn fast(): int { return 1; } }
~~~

`SZrCompileTimeDeclaration` 记录 declaration type、selected branch、是否 conditional
pruning 和 build-facts 是否已求值。当前 parser 拒绝 `comptime var`、`comptime class` 和
`comptime struct` 旧形式；声明变换应使用 role 标记的 `comptime fn` 返回
`DeclarationPatch`。执行入口是 `ZrParser_CompileTimeDeclaration_Execute`，预算、effect
和运行时投影由 compile-time contract 负责。

## 7. 控制语句

### 7.1 块和表达式语句

```
block               ::= "{" {statement} "}"
expression-statement ::= expression ";"
```

块总是创建 `SZrBlock`，其中 `isStatement` 在普通代码中为 true；函数、循环、分支和
accessor 都复用同一个 block parser。单独的对象 literal 可能与块开头冲突，parser 会用
前瞻判断 `{key: value}` 是对象还是块。

### 7.2 if、while

```
if-statement    ::= "if" "(" expression ")" block ["else" (if-statement | block)]
while-statement ::= "while" "(" expression ")" block
```

~~~zr
if (ready) { start(); } else if (retry) { wait(); } else { fail(); }
while (queue.length() > 0) { consume(queue.pop()); }
~~~

parser 将 if 保存为 `SZrIfExpression`，并设置 `isStatement`；同一个节点类型也可被
compile-time pruning 复用。条件为空、缺少右括号或缺少 `{` 都是结构诊断。condition 的
truthiness、分支类型合并和不可达代码由 CFG/semantic flow 计算。

### 7.3 for 和 foreach

```
for-statement     ::= "for" "(" [variable-declaration-no-semicolon | expression] ";"
                      [expression] ";" [expression] ")" block
foreach-statement ::= "for" "(" ("var" | "let") pattern [":" TypeRef] "in" expression ")" block
```

~~~zr
for (var i: int = 0; i < 10; i += 1) { use(i); }
for (let item: Item in items) { use(item); }
for (var {key, value} in table) { use(key, value); }
~~~

C 风格 `for` 的 init 可以是变量声明或表达式，三个槽位允许为空；header 内的变量声明
不消费分号，外层 `for` parser 消费分隔符。foreach 的 `let` 会设置
`SZrForeachLoop.isConst`，`in` 右侧是完整表达式，模式可以是 identifier、对象解构或
数组解构。迭代器协议、元素类型和 cardinality 是语义层工作，不是 parser 的隐式转换。

### 7.4 switch 和 union pattern

```
switch-statement ::= "switch" "(" expression ")" "{"
                     {"(" pattern-or-expression ")" block}
                     ["(" ")" block] "}"
```

~~~zr
switch (result) {
    (ok(value)) { return value; }
    (error(code, message)) { log(code, message); }
    () { return 0; }
}
~~~

空 `()` 是 default case，普通 case 的值可为常量表达式、union tuple move pattern 或
struct payload pattern。每个 case 都生成 `SZrSwitchCase.value` 和 block；default 生成
`SZrSwitchDefault`。`isUnionExhaustive` 在后续 CFG/union analysis 设置，parser 不根据
case 数量自行判定穷尽性。case 缺少括号或 body 会被拒绝，不能写成 C/Java 的 `case:`。

### 7.5 try、catch、finally

```
try-statement ::= "try" block {"catch" "(" [parameter-list] ")" block}
                 ["finally" block]
catch-parameter ::= ["const"] identifier [":" TypeRef]
```

~~~zr
try {
    read();
} catch (error: IOError) {
    report(error);
} catch (error: Exception) {
    recover(error);
} finally {
    close();
}
~~~

catch 参数复用 parameter-list 节点，可带类型和 passing mode，但不支持默认值语义。多个
catch 按源码顺序保存在 `catchClauses`；finally 可选。compiler 会把 abrupt edge、清理和
异常 handler 写入 CFG，确保 return/break/throw 都先经过 finally。

### 7.6 using、资源和 guard

```
using-block  ::= "using" "(" expression ")" block
using-guard  ::= "using" "(" ("let" | "var") guard-pattern
                 [":" TypeRef] "=" expression ")" block ["else" block]
using-short  ::= "using" ("let" | "var") (array-pattern | object-pattern)
                 [":" TypeRef] "=" expression ";"
```

~~~zr
using (new File("input.txt")) {
    consume();
}

using (let plugin = import("zr.pluginprobe")) {
    plugin.run();
} else {
    fallback();
}

using let [head, tail] = pair;
~~~

第一种形式是 drop/resource scope；第二种是 pattern 或 plugin guard，只有 guard 才允许
`else`。无块形式目前只在数组/对象 pattern 通过 lookahead 识别，`using let name = expr;`
作为普通短式不会被当作 no-block guard；需要 block 时应写括号形式。AST 的
`guardKind` 分为 DROP、PATTERN、PLUGIN，`resource`、`pattern`、`guardTypeInfo` 和
`elseBody` 分开保存。资源所有权迁移、drop 顺序、import plugin 生命周期由 semantic flow
和 library registry 完成。

### 7.7 return、break、continue、throw、yield

```
return-statement        ::= "return" ["ref"] [expression] ";"
break-continue-statement ::= ("break" | "continue") [expression] ";"
throw-statement         ::= "throw" expression ";"
yield-statement         ::= "yield" expression ";"
```

`return ref expr;` 设置 `isReferenceReturn`，必须通过 reference escape 检查。break 和
continue 的可选 expression 由 CFG loop result 规则解释；跨行没有分号仍然会报错。yield
是当前 generator/iterator 载体的独立节点，值不能是悬空借用；旧的 `out` statement 和
`{{ ... }}` 双大括号 generator 在 production parser 中报告迁移诊断，应该改写为返回
显式 iterator 并使用 `yield`。

## 8. 表达式入口与类型查询

表达式 parser 支持 assignment、logical、binary、conditional、unary、lambda、call、
member/index、array/object literal、`new`/`using` construct、`typeid(TypeRef)`、
`typeof(expr)`、ownership intrinsic 和 `fn(...)` 匿名函数。优先级和每个运算符的
meta dispatch 见[运算符、元方法与类型转换](operators-conversions.md)。几个容易混淆的
形式如下：

~~~zr
let callback: fn(int) -> int;
let transform = fn(value: int): int => value;
let canonical = typeid(Point);
let runtimeType = typeof(value);
let owner = new Resource();
let shared = share(owner);
~~~

函数类型必须使用 `fn(params) -> ReturnType`，声明和 lambda 的返回类型仍使用 `:`。
`T?` 不是 parser 的通用 TypeRef 后缀；API 表格中的问号只是可选/可空记法。可选成员
访问使用 `?.` token，和普通 `.` member access 的 null 行为不同。

## 9. AST 映射与 C 调用流程

### 9.1 节点分层

`EZrAstNodeType` 将节点分为顶层、声明、成员、表达式、字面量、复合 literal、语句和
循环。声明节点中的 `SZrType` 记录 ownership qualifier、reference access、数组维度、
readonly view 和固定大小约束；函数/方法节点中的 parameter 数组记录 passing mode 和
generic constraint。不要把 AST 节点地址作为跨编译单元的稳定 ID，后续 canonical type
和 metadata token 才是可持久化 identity。

### 9.2 最小 C 解析示例

~~~c
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/ast.h"

SZrAstNode *parse_source(SZrState *state, const TZrChar *source, TZrSize length,
                         SZrString *sourceName) {
    SZrAstNode *ast = ZrParser_Parse(state, source, length, sourceName);
    if (ast == ZR_NULL) {
        /* 读取 state/diagnostic 回调中的结构化错误。 */
        return ZR_NULL;
    }
    /* ast->type == ZR_AST_SCRIPT；使用完后必须由调用方释放。 */
    return ast;
}

void release_source_ast(SZrState *state, SZrAstNode *ast) {
    ZrParser_Ast_Free(state, ast);
}
~~~

需要 token 级控制时，先初始化 `SZrParserState`，设置 `errorCallback` 或
`structuredErrorCallback`，再调用 `ZrParser_State_SeekToTokenStart`、
`ZrParser_ParseTopLevelStatementWithState`；所有返回的 AST 都由调用方通过
`ZrParser_Ast_Free` 释放。`ZrParser_State_Free` 只释放 lexer/parser storage，不替代 AST
释放。

### 9.3 从 AST 到执行

```
source -> Lexer token -> Parser AST -> type/canonical facts -> CFG/flow
       -> Semantic IR -> compiler function/instructions -> VM/AOT/native dispatch
```

`ZrParser_Compiler_Compile` 消费完整 AST；`ZrParser_Expression_Compile` 和
`ZrParser_Statement_Compile` 是测试及分阶段接线用的低层入口。变量、property、extern、
call-binding 和 ownership 的语义检查失败时，compiler 可以保留 parser 已构建的节点，
但不会发布可执行函数。详细的 semantic IR、Place/Loan 和 cleanup 规则见[语义与执行模型](semantics-implementation.md)。

## 10. 诊断、恢复与迁移矩阵

| 输入 | parser 行为 | 下一步 |
| --- | --- | --- |
| 缺少 `;` | `missing_statement_semicolon`，当前节点失败或恢复到下一个 token | 补显式分号 |
| 缺少 `)`/`}` | 结构化位置诊断，释放已分配的子节点 | 补闭合符号 |
| 重复 modifier | 记录 parser error，仍保留已见 flag 供诊断 | 删除重复 modifier |
| `func`、无 `fn`、箭头返回类型 | `legacy_syntax_removed`，production 返回 NULL | 改成 `fn ...: Type` |
| `%...`、`$Type(...)`、旧 ownership 类型 | migration diagnostic；不是普通 modulo/identifier | 按 suggestion 迁移 |
| 无 guard 的 `using ... else` | `using_else_without_guard` | 加 `let/var` guard 或删除 else |
| union 多个 `@` default variant | `union_default_variant_duplicate` | 只保留一个 default |
| 语法正确但类型/所有权不一致 | parser 成功，compiler/flow 失败 | 查看 semantic diagnostic |

迁移前端若需要继续拿到旧节点，只能显式开启 parser state 的 migration 开关，并且不应
把返回 AST 送入 production compiler。这样保证旧语法的修复工具和当前执行器不会共享
一套含糊的语义。

## 11. 验证覆盖

声明、循环、属性、extern、union、分号切换和最小 fixture 的覆盖分别由本页 front matter
列出的 parser 测试验证。新增语法时应同时更新 `docs/zr_language_specification.md`、
`tests/fixtures/projects/syntax_reference_v1/golden/coverage.json` 和本页的 EBNF；只改
示例而不增加 parser/semantic 负例，会使网页文档与实际接受范围逐渐漂移。
