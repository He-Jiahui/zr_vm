---
related_code:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/canonical_type.h
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_types.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_reference_syntax.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/parser/parser_types.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_reference_syntax.c
  - zr_vm_parser/src/zr_vm_parser/compiler
plan_sources:
  - user: 2026-09-10 继续细化 Wiki 的语言规则、用例与 C 接口说明
  - docs/plans/syntax/01-canonical-type-place-cfg-artifact/m1-type-graph.md
  - docs/plans/syntax/02-reference-syntax-borrow-checker/m1-syntax-canonical-contract.md
  - docs/plans/syntax/03-struct-ref-struct-span-layout/m3-ref-struct-restrictions.md
tests:
  - tests/parser/test_canonical_type_graph.c
  - tests/parser/test_reference_syntax_contract.c
  - tests/parser/test_reference_escape_closure_suspension.c
  - tests/parser/test_type_inference.c
  - tests/parser/test_syntax_reference_v1.c
doc_type: language-reference
---

# 类型表达式与参数契约参考

本页规定 ZR 源码中 `TypeRef` 的可写形状、参数传递契约以及从 AST 到 canonical
`TypeId` 的边界。它解决三个容易混淆的问题：类型注解不是运行时对象、`ref` 既会出现在
类型/参数位置也会出现在表达式位置、以及 parser 接受的形状并不等于所有权和 ABI 已经通过。

阅读顺序建议是：先用本页确认源代码是否是合法的类型表达式，再用[类型、布局与所有权](types-ownership.md)
确认 ownership/GC 后果，最后用[泛型、接口与协议](generics-protocols.md)或
[Parser/Compiler C API](../05-interop/parser-compiler-c-api.md)处理语义与宿主侧对象。

## 适用阶段

| 阶段 | 本页涉及的输入/产物 | 该阶段能保证什么 | 仍不能保证什么 |
| --- | --- | --- | --- |
| lexer | `fn`、`ref`、`in`、`out`、`[`、`<` 等 token | token 和 source range 可用 | 名称是否是类型、`>>` 是否代表两个闭合尖括号 |
| parser | `SZrType`、`SZrParameter`、generic AST | 类型结构、参数 source form、数组约束和错误词序 | 名称解析、layout、可赋值性、泛型约束是否满足 |
| semantic | canonical `TZrTypeId`、parameter contract、Place/Loan | identity、passing form、escape 上界和约束 | 某个 backend/动态库是否能承载该 ABI |
| backend/runtime | layout、GC scan、call frame、FFI contract | 经验证的具体布局和调用规则 | 文本名称本身可作为跨模块/跨版本 identity |

因此，`parse_type()` 成功不表示一个 `ref` 可以逃出函数，也不表示 `Shared<T>` 可跨线程传递。
这些是 Semantic/CFG/ownership 或 provider contract 的职责。

## 记号与总语法

下列 EBNF 是 production parser 的可读摘要。`identifier` 表示普通标识符；方括号是
文档中的“可选”，不是 ZR token；花括号表示重复。

```ebnf
TypeRef              = type-prefix, type-core, { array-suffix } ;
type-prefix          = ref-prefix | readonly-prefix | empty ;
ref-prefix           = [ "scoped" ], "ref", [ "readonly" ] ;
readonly-prefix      = "readonly" ;
type-core            = named-type | generic-type | tuple-type
                     | function-type | "(", TypeRef, ")" ;
named-type           = identifier ;
generic-type         = identifier, "<", generic-argument,
                       { ",", generic-argument }, ">" ;
generic-argument     = TypeRef | additive-expression ;
tuple-type           = "[", TypeRef, { ",", TypeRef }, "]" ;
function-type        = "fn", [ generic-declaration, where-clauses ],
                       "(", function-type-parameters, ")", "->", TypeRef ;
array-suffix         = "[", [ array-size-constraint ], "]" ;
array-size-constraint = nonnegative-integer
                      | nonnegative-integer, "..", [ nonnegative-integer ]
                      | expression ;
```

`generic-argument` 的第二分支是为了容纳 const/int 类实参。parser 只保留节点；一个表达式
能否用作某个泛型形参、是否必须在编译期求值，都由 semantic 阶段决定。

## 命名类型与模块 identity

最小的类型表达式是名称：

```zr
var count: i32 = 0;
var title: string = "ZrVm";
var point: Point;
```

parser 将 `i32`、`string` 和 `Point` 都保存为 `SZrType.name` 指向的标识符 AST。它不会仅靠
`Point` 这段文本断言这是哪个定义。Semantic 在绑定时把模块 identity、声明 token 和泛型实参
纳入 canonical identity；两个显示为 `Point` 的类型只要来自不同 module，就可以拥有不同
`TypeId`。

不要用以下策略绕过这一规则：

- 用 `strcmp(typeName, "Point")` 代替 canonical type 比较；
- 将 import spelling 拼到类型名上，假设它等价于 resolver 的 module key；
- 保存一个旧 metadata generation 中的 `TypeId`，再把它用在 reload 后的 layout 或 native call 中。

宿主想按名称建立已知类型时，应在有有效 `SZrSemanticContext` 的阶段调用
`ZrParser_CanonicalType_FromName` 或由编译入口完成绑定，而不是手工构造整数 ID。

## 泛型类型与内建 ownership 包装

```zr
var names: Array<string>;
var pair: Map<string, i32>;
var owner: Unique<FileHandle>;
var shared: Shared<Session>;
var weak: Weak<Session>;
```

泛型尖括号至少包含一个实参。parser 会把 `Base<...>` 生成 `ZR_AST_GENERIC_TYPE`，并在
`SZrType` 中保存 generic AST。相邻闭合尖括号由 type parser 的 closing-angle 逻辑处理，
因此嵌套类型可写成常见形式：

```zr
var table: Map<string, Array<Shared<Record>>>;
```

`Unique<T>`、`Shared<T>` 和 `Weak<T>` 是当前识别的首字母大写 ownership generic 名称。
parser 把它们标为隐式内建类型并记录 ownership qualifier；后续阶段仍要检查 `T` 是否适用于
该 owner kind、转移/升级是否合法以及是否允许跨 domain。旧的 `unique<T>`、`shared<T>`、
`weak<T>` 会得到迁移诊断，不应继续写入新代码。

下列拼写也被显式当作旧设计拒绝：

```zr
// 不要这样写：Borrow<T> / Loan<T> 已移除。
var view: Borrow<Buffer>;

// 不要在类型开头使用旧的百分号标记。
var oldStyle: %Buffer;
```

共享视图应写 `ref readonly Buffer`，可写借用应写 `ref Buffer`。把 owner generic 和借用类型
混为一谈会使 escape、Drop 与 FFI 的责任边界无法判定。

## tuple、数组与尺寸约束

类型上下文中的方括号表示 tuple，不表示数组 literal：

```zr
var coordinate: [i32, i32];
var mixed: [string, i32, bool];
```

数组是在类型 core 后追加后缀：

```zr
var dynamicValues: i32[];
var matrix: f64[][];
var fixedHeader: byte[16];
var atLeastOne: byte[1..];
var bounded: byte[4..64];
```

`[N]` 的 `N` 必须为非负整数；`[M..N]` 要求 `N >= M`；`[M..]` 表示只有下界。若方括号内
不是这两类整数形式，parser 记录一个大小表达式，例如 `T[Width * Height]`。大小表达式能否被
接受为常量、是否可用于 AOT layout，不由 syntax 先行决定。

| 源码 | parser 保存的意图 | 常见后续检查 |
| --- | --- | --- |
| `T[]` | 一个未约束维度 | 元素 type 的 layout/GC scan。 |
| `T[N]` | fixed/min/max 都是 `N` | `N` 非负，布局或构造边界。 |
| `T[M..]` | 最小长度 `M` | 运行时长度检查或 API contract。 |
| `T[M..N]` | 最小/最大长度 | `M <= N`，调用点长度。 |
| `T[expr]` | AST expression | 常量求值、generic const binding 或动态约束。 |

数组 literal `[1, 2]` 则属于表达式语法，不能据此推断它一定和 `[i32, i32]` 是同一种值布局。
要查看表达式一侧的规则，参见[表达式与运算符](expressions.md)。

## 函数类型

函数类型使用细箭头 `->`，而不是匿名函数体的胖箭头 `=>`：

```zr
var transform: fn(i32) -> i32;
var compare: fn(left: string, right: string) -> i32;
var sink: fn(... string) -> void;
```

完整摘要如下：

```ebnf
function-type-parameters = [ function-type-parameter,
                              { ",", function-type-parameter },
                              [ ",", variadic-parameter ] ] ;
function-type-parameter  = [ "const" ], [ identifier, ":" ],
                           [ parameter-source-form ], TypeRef,
                           [ "=", expression ] ;
variadic-parameter       = "...", [ "const" ], [ identifier, ":" ],
                           [ parameter-source-form ], TypeRef ;
parameter-source-form    = "in" | "out" | ref-prefix ;
```

`fn(...) => T` 会被识别为已移除的 function-type 写法，并提示改为 `fn(...) -> T`。反过来，
匿名函数的表达式 body 必须用 `=>`，详见[可调用对象、调用与异步迭代](callable-async-iterator-reference.md)。

函数类型的形状进入 canonical function type：参数 type、passing form、return type、receiver
effect 和 effect flags 都会参与其语义表示。因此，`fn(ref i32) -> void` 与
`fn(i32) -> void` 不是可按字符串替换的同一种 callable。

## 参数 source form 与 type prefix 的区别

在声明参数中，`in`、`out`、`ref`、`ref readonly`、`scoped ref` 和
`scoped ref readonly` 是**参数传递契约**。它们出现在可选 `name:` 后、TypeRef 前：

```zr
fn copy(source: in Buffer, destination: out Buffer): void {
    destination = source;
}

fn normalize(value: ref Vector3): void {
    value = value.normalized();
}

fn inspect(value: ref readonly Vector3): f32 {
    return value.length();
}

fn transient(value: scoped ref readonly Span<byte>): void {
    // scoped 使借用的可逃逸范围更严格；不是跨调用长期 owner。
}
```

`in` 和 `out` 不是一般 TypeRef prefix；它们由参数 parser 消费并写入 `SZrParameter` 的
`passingMode` / `sourcePassingForm`。调用处相应的显式 marker 只有 `ref` 和 `out`：

```zr
normalize(ref vector);
copy(source, out target);
```

调用方是否提供了可写 Place、所有路径是否初始化 `out`、以及 caller/callee 的方向是否匹配，
都在 semantic/flow 阶段验证。

### 词序与禁止嵌套

下列词序是有意严格的：

```zr
// 合法
var writable: ref Buffer;
var readOnly: ref readonly Buffer;
var temporary: scoped ref readonly Buffer;
var capability: readonly Buffer;

// 非法：readonly 必须跟在 ref 后。
var wrongOrder: readonly ref Buffer;

// 非法：不要写嵌套引用；对表达式重新借用。
var nested: ref ref Buffer;
```

`scoped` 只能修饰 `ref` 或 `ref readonly`，不能单独作为 type keyword。AST 中
`SZrType.isScopedReference` 表示 lexical-region 限制；公共头文件明确说明该标志本身不参与
`TypeId` identity。它仍会影响 loan/escape 规则，所以不能因为 `TypeId` 相同就把 scoped view
延长到函数返回、closure 或 `await` 之后。

`readonly T` 是 capability view，不等于 `ref readonly T`：前者修饰对象/值类型的只读能力，
后者是引用访问模式。parser 会拒绝重复 `readonly`，也会拒绝把 `readonly` capability 套在
function type 上。

## 泛型形参与 where 约束

声明可引入 type 形参或 const 整数形参：

```zr
fn identity<T>(value: T): T {
    return value;
}

struct Window<const Width: i32> {
    var pixels: byte[Width];
}
```

interface generic parameter 才允许 `in` / `out` variance。`where` 子句可重复，每个子句选择
已声明的 generic name 并用逗号列出约束：

```zr
fn make<T>(value: T): T
where T: class, new() {
    return value;
}
```

当前 parser 能记录的 constraint 类别包括 `class`、`struct`、`new()`、`owner`、指定 owner
qualifier，以及普通 TypeRef。它会报告未知 generic 名、互相冲突的 ownership constraint，或
不允许位置上的 variance。是否有匹配 constructor、protocol witness 或 concrete layout，留给
semantic binding 决定。

## 从 AST 到 canonical type

下面的映射不是 host 需要自己重复实现的转换算法，而是帮助调试 output 和读 C API 时理解
对象层次。

| 源码形状 | AST 保存 | canonical 层的主要接口 |
| --- | --- | --- |
| `i32`、`bool` | `SZrType.name` identifier | `ZrParser_CanonicalType_InternPrimitive`。 |
| `module.Type` / 名义类型 | identifier + resolved symbol | `ZrParser_CanonicalType_InternNominal`。 |
| `Box<T>` | `ZR_AST_GENERIC_TYPE` | `ZrParser_CanonicalType_InternGenericInstanceEx`。 |
| `T[]` | `dimensions` 与数组约束 | `ZrParser_CanonicalType_InternArray`。 |
| `[A, B]` | `ZR_AST_TUPLE_TYPE` | `ZrParser_CanonicalType_InternTuple`。 |
| `ref T` / `ref readonly T` | reference access | `ZrParser_CanonicalType_InternRef`。 |
| `readonly T` | readonly view flag | `ZrParser_CanonicalType_InternReadonlyView`。 |
| `Unique<T>` 等 | ownership qualifier | `ZrParser_CanonicalType_InternOwner`。 |
| `fn(...) -> R` | `ZR_AST_FUNCTION_TYPE` | `ZrParser_CanonicalType_InternFunction`。 |

canonicalizer 会把 `SZrCanonicalParameterContract` 放入 function type，而不是只保存一串
原始 token。对 native extern、AOT metadata 或 LSP 来说，这一点很关键：后端应消费已经
验证的 canonical contract，不能重新从源字符串猜 `ref`、`out` 或 generic 参数。

## C 侧读取和建立类型

对嵌入式工具，推荐让 parser/compiler 完成源代码转换。只有在编写 semantic 工具、fixture
或 provider compiler 时，才需要直接操作 canonical type API。最小的上下文边界如下：

```c
SZrSemanticContext *semantic = ZrParser_SemanticContext_New(state);
TZrTypeId intType = ZrParser_CanonicalType_InternPrimitive(
        semantic, ZR_VALUE_TYPE_INT32);
TZrTypeId readonlyInt = ZrParser_CanonicalType_InternRef(
        semantic, intType, ZR_CANONICAL_REF_READONLY);

/* Use readonlyInt only while semantic/context metadata remains valid. */
ZrParser_SemanticContext_Free(semantic);
```

上例只展示 API ownership 和阶段关系；primitive enum、ref access enum、module identity 和
function contract 的完整值应直接取自公共头文件。不要把这个片段理解为“任意 C 插件都应自己
intern ZR 类型”：普通 native callback 应使用 Library 的 binding/value helper，具体见
[Native callback 配方](../05-interop/native-callback-recipes.md)和
[Core 值与对象 API](../05-interop/core-value-object-api.md)。

`ZrParser_CanonicalType_Find` 可查询 context 内的 canonical node，
`ZrParser_CanonicalType_Format` 可生成诊断文本，
`ZrParser_CanonicalType_ValidateParameterContract` 用于检查参数契约。它们不会让 type 脱离
semantic context 成为永久、跨线程、跨 reload 的句柄。

## 合法/非法用例清单

```zr
// 合法：命名、泛型、tuple、数组后缀和 callable type。
var handler: fn(ref readonly Packet, out i32) -> bool;
var queue: Array<Shared<Job>>[];
var key: [string, i32];

// 合法：参数名可省略，契约仍然存在。
var predicate: fn(ref readonly Record) -> bool;

// 非法：function type 用错箭头。
var brokenCallable: fn(i32) => i32;

// 非法：scoped 不是独立类型修饰符。
var brokenScope: scoped Buffer;

// 非法：nested ref 不能表示更长借用。
var brokenRef: ref ref Buffer;
```

遇到失败时先看 diagnostic 的 source range：词法/括号/箭头问题是 parser 层；名称、约束、
owner、`out` definite assignment、跨 await escape 和 ABI layout 问题不是修正 token 后就能
解决的语法错误。更完整的错误传播路径见[异常、诊断与失败传播](error-handling.md)。
