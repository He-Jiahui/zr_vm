---
related_code:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_loops.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_switch_patterns.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_control_flow.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_scope.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_loops.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_switch_patterns.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_control_flow.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_scope.c
plan_sources:
  - user: 2026-09-10 继续细化 Wiki 的语言规则、用例与 C 接口说明
  - docs/plans/syntax/01-canonical-type-place-cfg-artifact/m2-place-cfg.md
  - docs/plans/syntax/02-reference-syntax-borrow-checker/m2-place-out-definite-assignment.md
  - docs/plans/syntax/13-iterator-enumerator-yield/m2-yield-syntax-semir.md
tests:
  - tests/parser/test_cfg_typed_catch_flow.c
  - tests/parser/test_cfg_typed_catch_loop_flow.c
  - tests/parser/test_reference_place_out_flow.c
  - tests/parser/test_compiler_features.c
  - tests/iterator/test_yield_syntax.c
doc_type: language-reference
---

# 控制流、模式与清理参考

本页定义 ZR 的 block、分支、循环、`switch`、异常、`using`、模式绑定和 abrupt control
(`return`/`throw`/`break`/`continue`/`yield`)。核心原则是：parser 负责建立结构化 AST；
semantic/CFG 负责检查到达性、类型、模式、初始化与 loan；compiler 负责让每一条离开 scope 的
路径都执行正确的 cleanup。

对简单语法速查可先看[控制流与资源清理](control-flow.md)。本页补充 production parser 的
精确 block 形状、特殊 pattern 以及实现层的 cleanup/control-flow 模型。

## 控制流的共同模型

```text
source block
    -> AST block / branch / loop / handler node
    -> CFG basic blocks + normal, exceptional, cleanup, suspend edges
    -> typed Place/Loan/initialization facts
    -> instruction labels + scope cleanup registrations
    -> VM or AOT execution
```

一段源码“可解析”不等于其 control transfer 有目标。例如 parser 可以构造
`break value;` 的 optional expression，但 semantic 仍须验证当前有可中断结构、该结构是否
允许携带值、以及 value 的 type/ownership 是否可沿目标边传递。

## block 与 `if`

```ebnf
block       = "{", { statement }, "}" ;
if-statement = "if", "(", expression, ")", block,
               [ "else", ( if-statement | block ) ] ;
```

```zr
if (ready) {
    start();
} else if (retryable) {
    retry();
} else {
    fail();
}
```

condition 必须写在圆括号中，then/else body 必须是 block；不存在“单条语句不加花括号”的
隐式形式。parser 使用 `ZR_AST_IF_EXPRESSION`，但 statement entry 将 `isStatement` 设为真；
不要因为 AST 名称含 `EXPRESSION` 就假定它总能在任意 value position 产出值。是否有可合并的
分支结果由后续 type/control-flow 分析决定。

每个 block 形成 scope 边界：局部绑定、loan、`using` guard、owner cleanup 与 diagnostic
range 都按该边界管理。分支后的变量可用性必须经过 definite-assignment 合并，不能因为两个
分支都“看起来赋了值”就跳过 CFG 检查。

## 循环

### `while`

```ebnf
while-statement = "while", "(", expression, ")", block ;
```

```zr
while (cursor.moveNext()) {
    consume(cursor.current());
}
```

body 必须为 block。condition 的 bool-like 合法性、循环是否可达、在循环回边上哪些变量仍被
初始化，以及 mutable loan 是否跨回边，全部由 semantic/CFG 判断。

### 三段式 `for`

```ebnf
for-statement = "for", "(", [ for-init ], ";", [ expression ], ";",
                [ expression ], ")", block ;
for-init      = variable-declaration-for-header | expression ;
```

```zr
for (var index: int = 0; index < count; index += 1) {
    visit(index);
}
```

init、condition 和 step 各自可省略，但两个分号不能省略。header 内的变量声明由专用 parser
入口读取，因此不需要把普通 statement 的结尾分号再复制一遍；外部文字仍要保留两个 header
separator。loop body 是 block，init binding 的可见范围与 cleanup 都由 loop scope 管理。

### `foreach`

```ebnf
foreach-statement = "for", "(", ( "let" | "var" ), pattern,
                    [ ":", TypeRef ], "in", expression, ")", block ;
pattern           = identifier | array-destructuring | object-destructuring ;
```

```zr
for (let item in items) {
    render(item);
}

for (var [name, score] in standings) {
    update(name, score);
}

for (let {id: recordId, title} in records) {
    index(recordId, title);
}
```

`let`/`var` 直接决定 foreach binding 的 AST `isConst` 标志；可选 TypeRef 是对 element/pattern
的约束，而不是对 iterable 本身的类型断言。迭代协议、解构字段是否存在、元素能否 move 或借用，
在 iterator binding 和 flow 阶段验证。对 `Iterator` 的运行时协议参见[可调用对象、调用与异步迭代](callable-async-iterator-reference.md)。

## `switch` 与模式 case

ZR 当前 switch 头和每个 case 均使用圆括号，default 用空括号：

```ebnf
switch-statement = "switch", "(", expression, ")", "{",
                   { "(", case-pattern, ")", block },
                   [ "(", ")", block ], "}" ;
```

```zr
switch (status) {
    (Status.ready) {
        start();
    }
    (Status.failed) {
        recover();
    }
    () {
        reportUnknown(status);
    }
}
```

这不是 C/Java 风格的 `case value:` 语法。parser 为每个普通分支创建 `ZR_AST_SWITCH_CASE`，
空括号 block 创建 `ZR_AST_SWITCH_DEFAULT`，外层是 `ZR_AST_SWITCH_EXPRESSION` 且在 statement
位置标记为 statement。

### variant payload 与 move binding

switch parser 会尝试识别 union/variant payload pattern。tuple 和 object 样式的绑定允许在
binding identifier 前加 `move`：

```zr
switch (result) {
    (Result.ok(move value)) {
        consume(value);
    }
    (Result.error { message: move text }) {
        show(text);
    }
    () {
        show("unexpected result");
    }
}
```

这里的 `move` 是 pattern binder 的语义标记，不是任意 expression prefix。parser 仅在 case
header 的可识别 variant/payload 形状中标记 `SZrIdentifier.isMoveBinding`；semantic 仍要证明
matched variant、payload field、owner transfer 和剩余分支的 exhaustiveness/合法性。没有
move binding 的相似 call/object 形状会回退为普通 case expression，而不是被强行当模式解释。

## 直接控制转移

```ebnf
return-statement   = "return", [ "ref" ], [ expression ], ";" ;
break-statement    = "break", [ expression ], ";" ;
continue-statement = "continue", [ expression ], ";" ;
throw-statement    = "throw", expression, ";" ;
yield-statement    = "yield", expression, ";" ;
```

```zr
fn first(values: Array<int>): int {
    if (values.length() == 0) { return 0; }
    return values[0];
}

while (true) {
    if (cancelled()) { break; }
    if (skipped()) { continue; }
    process();
}
```

要点：

- `return` 的 expression 可省略；`return ref expr;` 额外记录 reference-return 意图，后续
  必须验证 return contract 和 source region，不能返回局部借用。
- parser 允许 `break`/`continue` 后跟 expression，因此需要由当前 loop/switch 的 semantic
  contract 决定该 value 是否有意义；不要把它当跨语言可移植特性。
- `throw` 必须有 expression 和分号；异常类型、catch 匹配和 native error normalization 属于
  runtime/semantic 层。
- `yield` 必须有 expression 和分号；`yield;` 与 `iterator fn` modifier 已被测试明确拒绝。

`return`、`throw`、`break`、`continue` 和 `yield` 都可能穿过嵌套 cleanup scope。它们不应由
源码作者手工在每个出口调用 `drop` 来补救。

## `try`、`catch`、`finally`

```ebnf
try-statement   = "try", block, { "catch", "(", catch-parameters, ")", block },
                  [ "finally", block ] ;
catch-parameters = parameter, { ",", parameter } ;
```

```zr
try {
    writeFile(path, content);
} catch (error: IOError) {
    report(error);
} finally {
    audit(path);
}
```

production parser 允许零个或多个 catch，然后可选 finally；它保存每个 catch 的 parameter
array 和 block。仅从 parser 看，`try {}` 没有 catch/finally 也可以形成 AST；是否有语义价值、
异常是否可达、catch 类型是否存在、handler 是否遮蔽后续 handler，都由 CFG/type checking
判断。

`finally` 是控制流承诺，不是普通末尾 block：无论 try 正常返回、catch 返回、throw、break 或
continue，compiler 都要将 pending control 穿过 finally/cleanup 之后再转到真实目标。native
callback 抛出的错误也应归一到同一异常路径，详见[核心 GC 与异常 API](../05-interop/core-gc-exception-api.md)。

## `using`：资源 scope 与 guard pattern

最常用的资源 scope：

```zr
using (let file = open(path)) {
    file.write(content);
} // normal return, exception, break/continue all leave through cleanup
```

`using` 同时支持资源表达式、绑定、数组/对象解构 guard 和有限的 `else` 形状。实践中使用以下
规则可避免歧义：

```zr
using (let connection = connect(endpoint)) {
    send(connection);
} else {
    reportUnavailable(endpoint);
}

using [value] = option;
use(value);
```

| 形状 | parser 保存的意图 | `else` |
| --- | --- | --- |
| `using (expr) { block }` | block-scoped resource cleanup | 不把它当 guard；不能附 `else`。 |
| `using (let/var name = expr) { block }` | binder + guard/resource scope | guard 形状可附 `else`。 |
| `using [..] = expr;` / `using {..} = expr;` | 无 block 解构 guard | 支持规定的 guard 后续形状。 |
| 带 type annotation 的 guard | pattern + guard TypeRef | semantic 验证 match/type。 |

`using` AST 会保留 resource、body、guard kind、pattern、guard TypeRef 和 else body。它不是
简单 macro：一个实现需要同时处理 normal、exceptional 和 abrupt exit edge，且每种边都要
做相同的资源/loan cleanup。

## CFG 与 cleanup 的实现机制

compiler scope 维护 cleanup registration。进入 scope 时记录局部变量起点和 cleanup array；
注册 unique/shared/weak owner、显式 loan return 或有 close contract 的对象后，在退出 scope
或跳转到更外层目标时按**从内到外、每个 scope 内逆注册顺序**发射清理。

当前实现可见的低层指令意图包括：

| 编译器动作 | 对应实现意图 | 不能从中推断 |
| --- | --- | --- |
| `MARK_TO_BE_CLOSED` | 标记 scope-managed close | 任意对象都可按位关闭。 |
| `OWN_DROP` | owner/resource drop | 可以在 source 中重复 drop 后继续用 owner。 |
| `OWN_RETURN_LOAN` | 归还 borrower/loan | 借用可以跨任意 handler 或 suspension。 |
| `CLOSE_SCOPE` | 完成 scope cleanup | 可以跳过 finally/outer exception state。 |

这些是当前 compiler/VM 实现细节，不是给 native 插件直接拼 instruction 的稳定 ABI。C
宿主若要执行或报告异常，应走[嵌入式生命周期](../05-interop/embedding-lifecycle.md)和
[核心运行时宿主 API](../05-interop/core-runtime-host-api.md)，不要模拟 ZR 的 scope unwinding。

## 常见错误

| 表面错误 | 原因 | 修复方向 |
| --- | --- | --- |
| `if condition statement;` | branch body 不是 block | 使用 `{ ... }`。 |
| `for (...) statement;` | loop body 不是 block | 使用 `{ ... }`。 |
| `switch` 中写 `case x:` | 当前 grammar 使用 `(x) {}` | 改为括号 case header。 |
| `yield;` | yield 缺少 expression/分号 | 写 `yield value;`。 |
| `iterator fn` | 已移除的 modifier | 普通 `fn` 返回 `Iterator<T>` 并在 body 中 `yield`。 |
| `using (expr) { ... } else` | 非 guard resource scope 没有 else 语义 | 用绑定/解构 guard，或写显式 `if`。 |
| 从 `return ref` 返回局部 | source lifetime 不覆盖 caller | 返回 owned/value，或重设 API contract。 |
| 在 cleanup scope 后继续使用 moved owner | flow 已记录 drop/move | 缩短使用范围或转移为合法 shared owner。 |

更完整的语句 production 与 AST 字段可查[声明与语句完整参考](declaration-statement-reference.md)。
