---
related_code:
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expressions.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_postfix_call.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_call_arguments.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_literals.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_ownership_intrinsic.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_reserved_task.c
  - tests/fixtures/projects/syntax_reference_v1/src/object_model.zr
  - tests/fixtures/projects/syntax_reference_v1/src/ownership.zr
  - tests/fixtures/projects/syntax_reference_v1/src/anonymous_expression_arrow.zr
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expressions.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_postfix_call.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_call_arguments.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_literals.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_ownership_intrinsic.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_reserved_task.c
tests:
  - tests/parser/test_parser.c
  - tests/parser/test_syntax_reference_v1.c
  - tests/fixtures/projects/syntax_reference_v1/src/object_model.zr
  - tests/fixtures/projects/syntax_reference_v1/src/ownership.zr
  - tests/fixtures/projects/syntax_reference_v1/src/anonymous_expression_arrow.zr
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/zr_language_specification.md
doc_type: language-reference
---

# 表达式与运算符

表达式 parser 采用从低优先级到高优先级的递归下降层次。每一层先解析左操作数，再循环消费同层操作符；赋值和条件表达式在需要时递归右侧，因此赋值、`?:` 和一元操作具有右结合特征。AST 建成后，semantic phase 才决定数值提升、可调用性、Place/Value、borrow 和短路执行。

## 主表达式

当前主表达式入口可以识别：

- 标识符、`super` 和 `test` 兼容名称；
- `true`/`false`/`null`、整数、浮点、字符、字符串、模板字符串；
- `(...)` 分组、数组 `[ ... ]`、对象 `{ key: value }`；
- `fn ...`/`async fn ...` 匿名函数；
- `import("literal")`、`init`/`own`/`new` 构造、`typeid(...)`/`typeof(...)` 查询；
- `share(...)`、`degrade(...)`、`wake(...)`、`intoGc(...)`、`drop(...)` ownership intrinsic。

```zr
let n = 42;
let pair = [n, n + 1];
let record = { name: "zr", value: n };
let callback = fn(x: int): int => x * 2;
let canonical = typeid(Point);
let runtimeType = typeof(record);
```

对象 literal 的 key 可以是 identifier、字符串、ownership intrinsic 名称或 `[expr]` 计算键；每个 key 后必须有 `:` 和 value。数组/对象元素之间可用逗号或分号，允许尾随分隔符，但不能省略相邻元素的分隔符。`{{ ... }}` 和 `out` generator 形式已删除，应使用 iterator function 与 `yield`。

## postfix 链

主表达式解析完后，parser 会连续消费 postfix suffix，直到遇到不能继续的 token：

```ebnf
postfix = primary, {
            ".", member-name
          | "?.", member-name
          | "[", expr, "]"
          | ["<", TypeRef, {",", TypeRef}, ">"], "(", arguments, ")"
          | "?.", "(", arguments, ")"
          } ;
```

```zr
let direct = service.client.load(id).name;
let optional = maybeService?.client?.load(id)?.name;
let indexed = matrix[row][column];
let called = factory<int>(value);
let optionalCall = callback?.(value);
```

`.` 是直接成员访问，`?.` 是 optional member/call，`[...]` 是 computed access。`?.[` 被明确拒绝；如果需要可选索引，应先把容器保存到变量并显式判断。可选链在 runtime 中短路整个剩余 suffix：缺失 receiver 时，不执行后续成员读取、调用或参数表达式。

调用参数支持位置参数、命名参数和 source passing marker：

```zr
configure("fast", retries: 3, timeout: 10);
handle.tryBorrow(out borrowed);
```

调用 AST 同时记录 `args`、`argNames`、`argumentMarkers` 和 optional/direct access mode。named argument 只在普通 callable descriptor 中解析；ownership intrinsic 要求恰好一个位置参数，不能写 `share(owner: value)`。

## 构造和类型查询

```zr
var point: Point = init Point(1, 2);
var owned: Unique<FileHandle> = own FileHandle();
var object = new Document();
let id = typeid(Point);
let descriptor = typeof(object);
```

`init`、`own`、`new` 都先解析目标 TypeRef/identifier 与参数，再由 canonicalization 生成不同的 construction kind：值构造不转移 owner，`own` 创建资源 owner，`new` 走对象分配/初始化路径。`typeid(TypeRef)` 查询 canonical type identity；`typeof(expr)` 查询运行时 descriptor，二者的操作数类别不能互换。

`<Type> expr` 是显式类型转换表达式。parser 使用前瞻区分它与泛型调用：只有成功解析 TypeRef、读到 `>` 且后面确实能开始一个表达式时才建立 cast AST。旧 `$Type(...)` 构造和把 `<...>` 当作普通调用的歧义形式会产生迁移诊断。

## Ownership intrinsic

```zr
let sharedValue = share(owner);
let weakValue = degrade(sharedValue);
let upgraded = wake(weakValue);
let gcValue = intoGc(owner);
drop(gcValue);
```

这些名称是 lexer 保留 token，不是可引用的一等函数。每个 intrinsic 必须带一个位置参数，parser 会拒绝零参数、多个参数和命名参数。真正的 owner/borrow 合法性、生命周期和 GC bridge 由 semantic flow 与 runtime contract 检查；即使语法通过，也可能因为 active loan、跨 await 或错误 owner qualifier 而失败。

## 运算符优先级

下表从高到低排列；同一行的二元运算在 parser 中左结合，赋值和条件表达式右结合。

| 层级 | 运算符 | 解析函数/说明 |
| --- | --- | --- |
| 最高 | `.` `?.` `[]` `()` `<T>(...)` | postfix member/index/call 链。 |
|  | `typeid(...)` `typeof(...)` `await` `ref` `init` `new` `own` | 专用/一元前缀。 |
|  | `!` `~` unary `+` unary `-` | `parse_unary_expression`，右递归。 |
|  | `*` `/` `%` | multiplicative。 |
|  | `+` `-` | additive。 |
|  | `<<` `>>` | shift。 |
|  | `<` `>` `<=` `>=` | relational。 |
|  | `==` `!=` | equality。 |
|  | `&` | bitwise and。 |
|  | `^` | bitwise xor。 |
|  | `|` | bitwise or。 |
|  | `&&` | logical and，短路。 |
|  | `||` | logical or，短路。 |
|  | `?:` | conditional，右结合。 |
| 最低 | `=` `+=` `-=` `*=` `/=` `%=` | assignment，右结合，左侧必须是可写 Place。 |

例如：

```zr
let value = a + b * c;
let flag = ready && count > 0 || fallback;
let label = ok ? "yes" : "no";
total += step * 2;
```

解析结果相当于 `a + (b * c)`、`(ready && (count > 0)) || fallback` 和 `ok ? "yes" : "no"`。如果可读性比依赖优先级更重要，使用括号；括号表达式本身仍会在 AST 中保留 source range，便于 diagnostic 和 LSP。

## 短路与求值顺序

`&&` 和 `||` 的右操作数只有在需要时求值；`?:` 只求值选中的分支。普通二元运算、函数参数和 object/array literal 元素按 source 顺序生成 evaluation nodes，但具体副作用/exception contract 由 backend 遵循 SemIR。optional chain 还会跳过其后所有 postfix 参数：

```zr
let result = maybe?.compute(expensive());
```

当 `maybe` 缺失时，`compute` 与 `expensive()` 都不应执行。不要依赖 parser AST 的节点排列来观察 runtime 副作用；请使用 VM/AOT equivalence 测试验证 backend 行为。

## 赋值、Place 与引用

赋值左侧可以是变量、直接/可选成员或 computed member，但必须在 semantic phase 解析成可写 Place：

```zr
counter.value = 1;
items[index] += 1;
ref target = ref source;
```

`ref` expression 产生借用/引用 view，不会复制对象。`let` 绑定、readonly receiver、`in` 参数和 property getter 可能使 Place 只读；parser 不提前判断这些语义，错误会在 canonical type/loan analysis 中报告。

## 解析失败的常见形状

```zr
// 错误：缺少右操作数
let value = left + ;

// 错误：条件表达式缺少冒号和 alternate
let label = ok ? "yes";

// 错误：ownership intrinsic 不是可赋值的函数值
let op = share;
```

parser 会保留操作符 source range，并给出 `missing_right_operand`、条件表达式缺分支或 `ownership_intrinsic_call_required` 等结构化 diagnostic。详见[诊断与错误恢复](diagnostics.md)。
