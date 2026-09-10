---
related_code:
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_loops.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_switch_patterns.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_yield.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_property.c
  - tests/fixtures/projects/syntax_reference_v1/src/iterators.zr
  - tests/fixtures/projects/syntax_reference_v1/src/ownership.zr
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_loops.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_switch_patterns.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_yield.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg.c
tests:
  - tests/parser/test_parser.c
  - tests/parser/test_syntax_reference_v1.c
  - tests/parser/test_place_cfg_graph.c
  - tests/fixtures/projects/syntax_reference_v1/golden/coverage.json
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/zr_language_specification.md
doc_type: language-reference
---

# 控制流与资源清理

控制结构先在 parser 中形成 AST，再由 CFG builder 划分 basic block、边和 cleanup region。`return`、`throw`、`break`、`continue`、`yield`、异常处理和 `using` 都是显式控制转移；backend 不应从 token 文本重新猜测它们的边界。

## Block 与 if

```ebnf
if-expression = "if", "(", expr, ")", block,
                [ "else", ( if-expression | block ) ] ;
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

条件括号和两个 body brace 都是必需的；`else if` 只是递归的 `if` 节点，不是特殊 token。AST
类型名仍叫 `ZR_AST_IF_EXPRESSION`，但 production statement 入口会把顶层/块内节点标记为
`isStatement`。普通源码不要把 `if (...) { ... }` 当作值表达式；需要产生值时使用
`?:`，而 compile-time/内部 lowering 若构造 expression-mode 节点，必须遵守对应 backend
contract。

## while

```zr
while (queue.length > 0) {
    consume(queue[0]);
}
```

`while` 需要非空的括号条件和 block。CFG 通常形成 header -> body -> back-edge，并把 `break` 连接到 loop exit、`continue` 连接到 header；条件表达式中的副作用仍按普通 expression 规则处理。

## C 风格 for

```ebnf
for-statement = "for", "(",
                [ variable-declaration | expr ], ";",
                [ expr ], ";",
                [ expr ], ")", block ;
```

```zr
for (var i: int = 0; i < values.length; i += 1) {
    consume(values[i]);
}
```

初始化、条件和步进都可省略，但两个分号仍需保留：

```zr
for (; ready; ) {
    poll();
}
```

`parse_for_loop` 会分别建立 `init`、`cond`、`step` AST；变量声明在头部不额外要求语句分号，分隔符由 for parser 消费。不要把 `for (item in values)` 当成 C 风格 for，它会被前瞻逻辑分派到 foreach parser。

## foreach 与解构

```ebnf
foreach-statement = "for", "(", ("var" | "let"), binding-pattern,
                    [ ":", TypeRef ], "in", expr, ")", block ;
```

```zr
for (let item in values) {
    consume(item);
}

for (var [key, value] in entries) {
    index(key, value);
}
```

`let` 在 foreach header 中表示只读/const iteration binding，`var` 表示可写 binding。pattern 可以是 identifier、对象解构或数组解构，`in` 后必须是 expression。迭代器协议、借用和元素类型由 semantic/provider 决定；parser 只记录 pattern、可选 TypeRef、容器 expression 和 body。

## switch 与模式

ZR 的 switch case 头部不是 `case` 关键字，而是括号包裹的值/variant pattern；空括号表示 default：

```ebnf
switch-statement = "switch", "(", expr, ")", "{",
                   { "(", ( expr | variant-pattern ), ")", block },
                   [ "(", ")", block ], "}" ;
```

```zr
switch (state) {
    (Ready) {
        start();
    }
    (Busy(value)) {
        observe(value);
    }
    () {
        fallback();
    }
}
```

union variant 的 move pattern 可以写成 `Variant(move value)` 或 `Variant { field: move local }`；struct payload pattern 会被 parser 以 object-like AST 保存。switch 节点同时保存 cases、可选 defaultCase、匹配表达式和 statement/expression 标记。variant 穷尽性、重复分支、不可达性和 move binding 合法性在 semantic/CFG 阶段检查。

## break、continue、return

```zr
for (var i: int = 0; i < 10; i += 1) {
    if (i == 3) { continue; }
    if (i == 8) { break; }
}

fn read(): int {
    if (ready) { return value; }
    return 0;
}

fn borrowResult(): ref int {
    return ref storage;
}
```

这几类语句都需要分号。`return ref expr;` 会在 AST 中设置 `isReferenceReturn` 和 source location；返回的 Place/lifetime 是否满足函数 contract 由 borrow analysis 决定。`break`/`continue` 的可选 expression 若被 parser 读取，也必须经过后续 loop/value contract 验证；不要把它当作通用 `goto`。

## yield 与迭代器

```zr
fn values(start: int): Iterator<int> {
    yield start;
    yield start + 1;
}
```

`yield expr;` 是独立 statement，必须显式分号。当前 parser 拒绝旧的 `out` generator statement 和 `{{ ... }}` 双大括号形式，并建议使用显式 `Iterator<T>` 返回 carrier。compiler 会把 yield 点变成可恢复的 iterator state；借用值不能跨 yield 保持活动，具体规则由 flow analysis 与 provider contract 给出。

## throw、try/catch/finally

```ebnf
try-statement = "try", block,
                { "catch", "(", parameters, ")", block },
                [ "finally", block ] ;
```

```zr
try {
    run();
} catch (error: RuntimeError) {
    report(error);
} catch (fallback) {
    recover(fallback);
} finally {
    closeResources();
}

throw error;
```

可以有多个 catch；每个 catch 的参数列表使用普通 parameter grammar，括号和 body 都必须闭合。`finally` 最多一个，且没有参数。parser 建立 try block、catch clause array 和 finally block；CFG builder 为正常边、异常边和 finally cleanup 边分别建模，确保 `return`/`break`/`throw` 离开 try 时仍执行 finally。

## using：Drop、守卫和作用域

最简单的 block-scoped resource：

```zr
using (openFile("data.txt")) {
    readAll();
}
```

也可以绑定资源，或用 union/import guard：

```zr
using (let file = openFile("data.txt")) {
    readAll(file);
}

using let [first, second] = acquirePair();

using (let plugin = import("native:engine.render")) {
    plugin.render();
} else {
    log("plugin unavailable");
}
```

形式化地说，当前 parser 接受三类形状：

```ebnf
using-statement = "using", (
    "(", [ ("let" | "var"), guard-pattern, [":", TypeRef], "=" ], expr, ")",
    block, ["else", block]
  | ["let" | "var"], destructuring-pattern, [":", TypeRef], "=", expr, ";"
  | expr, ";"
) ;
guard-pattern = identifier | array-pattern | object-pattern ;
```

第二种“不带 block”形状在 production parser 中只把数组/对象解构当作 guard（例如
`using [value] = option;` 或 `using var {ok: result} = packet;`）；若要绑定普通 identifier
并使用 `else`，请使用第一种括号形状 `using (let name = expr) { ... } else { ... }`。

规则要点：

1. 括号形式 `using (expr) { ... }` 建立 block-scoped cleanup；离开 body 的正常/异常/显式控制边都会触发 Drop/close contract。
2. `using (let name = expr)` 和不带 block 的数组/对象 pattern 形成 guard binding；只有 guard 才能跟 `else { ... }`。
3. guard pattern 可选 `: UnionType` 注解；`move` 标记只在 switch/using pattern 的绑定位置有特殊含义。
4. 无 guard 的 `using (expr) ... else ...` 会产生 `using_else_without_guard`，parser 不会把它解释成普通 if。

`using` AST 保存 resource、body、是否 block-scoped、guard kind、pattern、guard TypeRef 和 elseBody。后续 cleanup lowering 把这些信息转换为统一 cleanup CFG；不要手动在每个 return 前插入 `drop` 来替代 using，因为这样容易遗漏异常和 break/continue 边。
