---
related_code:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expressions.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_postfix_call.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_call_arguments.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_struct_init.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_ownership_intrinsic.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expressions.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_postfix_call.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_call_arguments.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_struct_init.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_ownership_intrinsic.c
plan_sources:
  - user: 2026-09-10 继续完善 ZrVm Wiki，要求详细介绍语法规则、用例和实现机制
  - docs/zr_language_specification.md
  - tests/fixtures/projects/syntax_reference_v1/syntax_reference_v1.zrp
tests:
  - tests/parser/test_parser.c
  - tests/parser/test_reference_syntax_contract.c
  - tests/parser/test_percent_syntax_cutover.c
  - tests/parser/test_syntax_reference_v1.c
  - tests/fixtures/projects/syntax_reference_v1/src/model.zr
  - tests/fixtures/projects/syntax_reference_v1/src/ownership.zr
doc_type: language-reference
---

# 表达式、构造与调用形状参考

本页把 ZR 的 expression surface 按 parser 的实际分层展开：primary、unary、postfix、
construct、argument、cast、type query、ownership intrinsic 与赋值。operator 优先级和元
方法映射见 [运算符、元方法与类型转换](operators-conversions.md)；函数类型、lambda、
async 和 call contract 见 [可调用对象、调用与异步迭代](callable-async-iterator-reference.md)。

## 1. 表达式分层与“能解析”的含义

production parser 大致按以下下降层次建立 AST：

```text
assignment
  -> conditional / logical-or / logical-and
  -> bitwise-or / xor / and
  -> equality / relational / shift / additive / multiplicative
  -> unary
  -> primary
  -> member / index / call postfix chain
```

其中的优先级只决定 AST 的 parent/child 形状。`left + right` 是否允许、`left.value`
是否可读、数组 index 是否在范围内、一个 argument marker 是否匹配 native ABI，全部是
type、Place/Value、contract、runtime 边界的后续问题。缺少右操作数时 parser 会就近报告
operator range，而不是把下一个语句悄悄吞进当前表达式。

## 2. Primary 表达式

```ebnf
primary ::= identifier | literal | "null" | "Infinity" | "-Infinity" | "NaN"
          | "(", expression, ")"
          | array-literal | object-literal | template-literal
          | lambda | construct | struct-init | type-query ;
```

常见字面量的 token、转义和 template 插值由 lexer 处理，详见[词法与字面量](lexical-structure.md)。
parser 会把标识符、布尔/整数/浮点/字符串/字符、`null` 和特殊数值变成不同的 AST node；
不要以“它们都在 primary 中”推断运行时 representation 相同。

```zr
var enabled = true;
var count = 42;
var title = "ZrVm";
var unknown = null;
var values = [1, 2, 3];
var record = {name: "zr", version: 1};
```

array/object literal 的 key、spread、字段顺序与目标类型协商属于 semantic construction；
若期望有固定 nominal layout，优先选择 `init StructType(...)` 或在 native descriptor 中
显式给出 constructor contract，而不是依赖一个对象 literal 的动态 shape。

## 3. Postfix：成员、索引和调用

primary 或 unary expression 可继续形成 member/index/call 链：

```zr
var code = project.modules[0].compile(options: flags).status;
var maybeName = user?.profile?.name;
```

每一段 postfix 都保存自己的 source range 和 AST edge，因此 semantic phase 可以分别给
`project.modules`、`[0]`、`compile(...)`、`.status` 建 Place 或 Value fact。普通 `.` 与
可选访问的 null/absence 行为不能仅从 parser 决定；type checker 必须确认 receiver 的
contract，再由 runtime 处理实际 `null` 值。

### 3.1 调用参数

```ebnf
argument-list ::= [ argument, { ",", argument }, [ "," ] ] ;
argument      ::= [ argument-name, ":" ], [ call-marker ], expression ;
argument-name ::= identifier ;
call-marker   ::= "ref" | "out" | "..." ;
```

例如：

```zr
copy(target: out destination, source: ref sourceView);
emit(prefix: "count", values: ...items);
```

parser 的重要边界如下：

| 规则 | parser 行为 | 语义阶段继续验证 |
| --- | --- | --- |
| named argument | 识别 `name:` 并保留 names array | 名称存在、重复、默认值与重载选择 |
| 顺序 | 一旦遇到 named argument，后续 positional argument 报错 | 与参数绑定的最终顺序 |
| `ref` / `out` | 保留 call-site marker | 可寻址 Place、passing mode、初始化/别名规则 |
| `...` | 保留 spread marker | operand 可展开、元素类型、varargs/collection contract |
| trailing comma | 允许在 `)` 前 | 不改变 argument count |

argument parser 不会根据目标函数“猜” marker。也就是说，`ref value` 可以形成 AST，但如果
目标实际需要普通 value、`value` 不是可写 Place，或 binding 来自 stale provider generation，
compiler/runtime 必须拒绝该调用。

## 4. 结构值与对象构造

### 4.1 `init TypeRef(...)`

`init` 是 struct/value construction 的明确语法：

```ebnf
struct-init ::= "init", TypeRef, "(", argument-list, ")" ;
```

```zr
struct Point {
    var x: int;
    var y: int;
}

var point: Point = init Point(1, 2);
var span: Span<int> = init Span<int>();
```

parser 在 `init` 后要求一个 `TypeRef`，并且必须看到 `(`。它会在
`ZR_AST_STRUCT_INIT_EXPRESSION` 中保存 type、arguments、argument names 和 markers，
并记录 `hasNamedArgs`。`init Point`、`init Point;`、`init (Point)` 不能替代该 production。
实际 constructor 是否存在、是否接受 named arguments、返回的是 inline value 还是需要
boxing，取决于 type layout 与 constructor contract。

### 4.2 `new TypeRef(...)`

```zr
var boss = new BossHero(30);
```

`new` 进入 general construct parser，并在 AST 中携带目标 type、参数、ownership qualifier
和 builtin kind。它适用于当前 object/class construction surface；不能把 `new` 当作 C++ 的
任意 placement-new，也不能据此绕过 ZR 的 allocator、GC 或 resource ownership。class
constructor 的 meta role、基类 `super(...)` 与调用顺序由 compiler 和 object runtime 决定。

### 4.3 ownership construction

资源对象应通过显式 owner 类型与 construction 配合：

```zr
resource class FileHandle { /* ... */ }

var handle: Unique<FileHandle> = own FileHandle(/* ... */);
```

`own` construction 和普通 `new` 的主要区别不是 parser token 的数量，而是后续 flow 得到
的 owner/move/drop 事实。把 `own FileHandle(...)` 赋给普通共享引用、复制 `Unique<T>` 或
让其穿过不允许的 `await`，都会在 ownership 或 task semantic 阶段而非“构造括号”阶段失败。
完整资源生命周期见[模式、所有权与资源配方](patterns-ownership-recipes.md)。

## 5. Unary、reference、cast 与 type query

### 5.1 常规 unary

```zr
var sign = -count;
var masked = ~bits;
var missing = !enabled;
```

`!`、`~`、一元 `+`、一元 `-` 右结合，并各自形成 `ZR_AST_UNARY_EXPRESSION`。operator 的
结果类型、数值 promotion、overflow 与 meta-method fallback 不在 unary parser 中决定。

### 5.2 `ref` 和 `out` 的位置

`ref` 在 expression 一元位置构造 reference expression；在 call argument 前则构造
call-site marker。两者都不自动产生“可跨越任何 scope 的 C 指针”：compiler 必须建立
borrow region、Place 信息与 escape 上界。`out` 是 call boundary marker，不是普通可求值
的 unary expression；它要求 callee contract 与 caller 的可写未初始化目标配合。

### 5.3 角括号 cast

```zr
var narrow = <i32>wide;
```

parser 对 `<Type> expression` 使用可回退的前瞻：先在抑制诊断的临时 cursor 中尝试解析
TypeRef，只有紧随其后的 token 能开启 expression 时才提交为 type-cast AST；否则恢复 cursor，
避免把泛型/关系表达式误吞为 cast。这个机制只解决 grammar 歧义，绝不承诺转换是安全、无
损、可空或 native ABI 兼容。显式/隐式转换规则见 [运算符、元方法与类型转换](operators-conversions.md)。

### 5.4 `typeid` 与 `typeof`

```zr
var pointIdentity = typeid(Point);
var runtimeType = typeof(value);
```

`typeid(...)` 要求 TypeRef，并构造 canonical identity query；`typeof(...)` 要求 expression，
并构造 runtime descriptor query。两者不是文本反射：`typeid` 的实参会参与 canonical type
解析，`typeof` 的 operand 会正常 type-check/evaluate。对应的 reflection provider 是否注册、
是否保留 metadata、AOT 是否裁剪描述信息，是独立的运行时/产物问题。

## 6. Ownership intrinsics

当前 parser 为以下保留名称建立专门的 `ZR_AST_OWNERSHIP_INTRINSIC_EXPRESSION`：

| intrinsic | 一个位置参数的 parser 契约 | 后续语义问题 |
| --- | --- | --- |
| `share(owner)` | 只能一个 positional expression | 是否能从 owner 形成 shared view |
| `degrade(owner)` | 只能一个 positional expression | 是否形成 weak/degraded reference |
| `wake(owner)` | 只能一个 positional expression | 目标是否仍有效、是否能升级 |
| `intoGc(owner)` | 只能一个 positional expression | 所有权到 GC domain 的合法 bridge |
| `drop(owner)` | 只能一个 positional expression | 是否立即 consume、drop 是否有效 |

它们不能当作普通函数值传递，也不允许 named argument、零参数或多参数：parser 会产生
`ownership_intrinsic_call_required` 或 `ownership_intrinsic_arity_mismatch`，并附带需要用户
决策的 no-fix reason。下面是合法的表面形状：

```zr
var sharedHandle = share(owner);
var weakHandle = degrade(sharedHandle);
var ownerAgain = wake(weakHandle);
var managed = intoGc(ownerAgain);
drop(managed);
```

这段代码并不保证每一步都能通过；例如 `wake` 的动态成功性、`drop` 后的 use、跨 domain
传输和 alias 数量必须由具体 provider/type contract 验证。

## 7. 赋值与副作用边界

assignment parser 将左、右 expression 以及 operator 存入 AST。它不会在语法阶段断言左边是
可写 Place，因此下面的形状可能先解析、后在 semantic phase 被拒绝：

```zr
(a + b) = 1;
readonlyView.field = 1;
```

compound assignment、member write、index write、property setter 和 `ref/out` write-back 都
必须用同一份 canonical call/property contract 判断。native callback 在 C 层写回 argument
时也不能跳过这个模型，参见 [Native Callback 实战配方](../05-interop/native-callback-recipes.md)。

## 8. 宿主观察 AST 的方式

宿主通常不手工解释 parser AST 以执行表达式，而是交给 compiler/VM。若工具确实只需分析
形状，使用 parser API 并让 AST 的 owner 保持有效：

```c
SZrAstNode *expression = ZrParser_ParseExpressionWithState(&parser);

/* Inspect node type/range while parser state and SZrState are alive. */
if (expression != ZR_NULL) {
    ZrParser_Ast_Free(state, expression);
}
```

不要从 `ZR_AST_CALL_EXPRESSION` 的内部 array 直接提取裸 `SZrString *` 后长期缓存，也不要
根据 AST 的 `new`/`init` token 直接调用 C allocator。正确的 C 执行/调用路径见
[Parser/Compiler 深度 C API](../05-interop/parser-compiler-c-api.md)、[Core 宿主状态、执行与预算](../05-interop/core-runtime-host-api.md)
和 [Native Registry 调用绑定](../03-modules/native-registry-call-binding.md)。
