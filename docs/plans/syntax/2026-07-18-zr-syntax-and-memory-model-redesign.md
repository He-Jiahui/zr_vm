# ZR 语法、引用与内存模型重设计草案

> 状态：人工已确认，代表已经批准实施。
>
> 日期：2026-07-18
>
> 范围：语言表层语法、类型能力、引用与借用、所有权、只读访问、属性、连续内存、池化，以及支撑这些能力的语义层契约。

## 1. 文档目的

当前 ZR 已经分别引入或规划了 `%ref`、`%in`、`%out`、`%unique`、`%shared`、`%weak`、`%borrow`、`%loan`、`%using`、`const`、属性访问器、GC 逃逸与确定性清理等能力，但这些能力来自不同阶段，尚未形成统一语言模型。

本设计不以“删除 `%` 字符”为终点，而是先统一以下基础概念：

1. 什么是值，什么是可寻址位置，什么是引用。
2. 读、写、移动、拥有和逃逸分别由什么类型能力表达。
3. GC 对象、唯一所有者、共享所有者和非拥有引用如何组合。
4. 哪些错误必须静态拒绝，哪些检查仍然属于运行时。
5. 属性、连续内存和池化如何成为上述基础能力的普通使用者，而不是额外特例。

本设计优先给出理想语言模型。旧语法迁移、诊断切换和二进制格式升级由本目录的细化设计分别约束。

## 2. 设计状态与适用里程碑

本设计横跨现有《ZR 基础语言与运行时重建总计划》的多个阶段，不能作为一个实现批次直接落地：

- Phase 1：统一 Semantic IR、类型标识、位置表达和数据流事实。
- Phase 2：规范化类型、泛型约束、引用能力和静态诊断。
- Phase 3：值类型、字段布局、装箱和对象能力。
- Phase 5：所有权、借用、确定性析构和 `using` 收敛。
- Phase 7：GC 与 ownership world 的边界和根追踪协议。
- Phase 8：模块产物、LSP、调试信息和旧语法迁移。

后续实施计划必须按这些基础层拆分，不能先把新拼写接入 parser，再依赖旧 AST 字段和字符串类型名补语义。

## 3. 目标

### 3.1 语言目标

- 大部分普通业务代码继续使用自动 GC，不要求开发者管理每个对象。
- 性能敏感代码可以选择值类型、栈限制引用、连续内存和确定性所有权。
- 所有权和 readonly 能力足够轻量，能够在普通库代码中经常使用。
- 引用、借用、移动、确定赋值和逃逸尽量在编译期检查。
- 调用处清楚展示可能修改调用者状态的 `ref` 和 `out` 操作。
- 不要求用户书写 Rust 风格命名生命周期。
- 不为池、arena、特定容器和具体类型名增加专用语法分支。

### 3.2 性能目标

- 值类型和连续布局减少堆分配、指针追逐和 GC 压力。
- borrow、move 和 `ref struct` 规则不依赖运行时借用检查。
- `Span<T>` 等视图不分配、不拥有内存。
- 范围分析可以消除已证明安全的边界检查。
- ownership world 的纯所有权对象图不进入普通 GC tracing。
- 非原子共享所有权不默认支付跨线程原子计数成本。
- 精确类型布局和根信息可以被 VM、AOT 和 LSP 共同消费。

### 3.3 开发体验目标

- 每个关键字只有一个主要职责。
- 同一语义在参数、局部变量、字段和返回值位置使用相同类型表达。
- class 和 interface 使用相同属性声明结构。
- 编译器诊断使用源代码中的变量、借用起点和逃逸路径说明问题。
- 简单代码不需要理解 GC region、引用计数控制块或显式生命周期。

## 4. 非目标

- 第一版不实现完整 Rust trait/lifetime 语言。
- 第一版不承诺深度不可变对象图。
- 第一版不让 `Shared<T>` 自动处理强引用环。
- 第一版不复制 C# 全部 Span 隐式转换和重载优先级规则。
- 池化、arena 和 size class 不成为语言关键字。
- 本文不决定旧 `%xxx` 语法保留几个版本。
- 本文不重新设计异常、协程、构造器和析构器的全部表层语法，只定义它们与引用及所有权的边界。

## 5. 备选路线

### 5.1 路线 A：只删除 `%`

将 `%ref` 改成 `ref`、`%unique` 改成 `unique`，其余 AST、类型系统和运行时保持不变。

优点是改动较小。缺点是 `ownershipQualifier`、内建类型枚举、字符串类型名和参数 passing mode 仍然表达同一语义，`Borrow<T>` 与 `%borrow` 等重复概念继续存在。该路线只能改善外观，不能改善静态检查和后端优化。

### 5.2 路线 B：统一位置、引用和所有权能力（按此路线来）

采用 C# 风格的易读表层、Rust 风格的 move/borrow 静态约束，并保持 ZR 的 GC 与 ownership 双轨运行时。所有高层语法统一降低为 Type、Place、Owner 和 Region 语义。

该路线是本设计的推荐路线。

### 5.3 路线 C：完整 Rust 化

引入显式生命周期参数、生命周期约束、trait 化的所有权能力和更严格的借用语法。

该路线安全能力强，但会明显增加语言学习成本、泛型复杂度、错误诊断难度和实现周期，不符合“简单语法、低开发负担”的首要目标。

## 6. 统一基础模型

### 6.1 值与位置

语义层必须区分：

- `Value`：计算结果，可复制或移动，但不能直接作为写入目标。
- `Place`：可寻址存储位置，例如局部变量、字段、数组元素、解引用结果。
- `Ref`：指向 Place 的非拥有引用，携带读写能力和逃逸上界。
- `Owner`：负责目标对象生命周期的拥有句柄。

源代码中的局部变量、显式字段、索引和解引用不再由不同特例处理，而是先形成 Place，再执行统一操作；property本身lower为accessor call，只有ref getter经deref形成Place：

```text
Load(place)
Store(place, value)
Move(place)
BorrowShared(place)
BorrowMut(place)
Reborrow(reference)
Drop(place)
```

### 6.2 核心类型形态

语言表层保留以下核心形态：

```zr
T
ref T
ref readonly T
scoped ref T
Unique<T>
Shared<T>
Weak<T>
```

其中：

- `T` 的存储语义由类型类别决定：struct 是内联值，class 是 GC 引用，resource class 由 owner 持有。
- `ref T` 是可读写非拥有引用。
- `ref readonly T` 是只读非拥有引用。
- `scoped` 为引用添加不得逃出当前函数的上界，不改变读写能力。
- `Unique<T>`、`Shared<T>` 和 `Weak<T>` 是真正的规范化类型构造，不是 AST 旁路标志。

### 6.3 显式 statement terminator

目标语法不采用换行终止或 automatic semicolon insertion：

```zr
module app.render;

let memory = import("core.memory");
let value: int = 1;
value += 1;
return value;
```

规范规则：

- newline/comment只属于 trivia；parser不得根据行边界、`}` 或 EOF 推断缺失的 `;`。
- module declaration、module import binding、field/local binding、expression/assignment statement、`return`、`throw`、`break`、`continue`、bodyless function/interface accessor declaration和expression accessor必须显式以 `;` 结束。
- type/function/constructor/destructor/property等由 `{ ... }` 自己闭合的 declaration可以省略尾随 `;`；parser至多接受一个尾随 `;` 作为declaration terminator，formatter默认删除。该token附着于declaration，不开放任意重复empty statement。
- compound control-flow statement由完整`if/else`、`switch`、loop或`try/catch/finally` grammar闭合，不消费declaration semicolon；尤其不能在`}`与`else/catch/finally`之间插入`;`。
- block-bodied anonymous function如果属于外层 simple declaration，外层仍需终止：`let f = fn(): int { return 1; };`。
- concrete property不允许initializer或bodyless auto accessor；初始化语句属于显式field/constructor/init accessor body并按各自statement规则写`;`。
- enum/union variant及其他无独立 block的 member declaration以 `;` 分隔，不接受 comma或newline作为等价 terminator。
- `.zrs` 保留 semicolon token/range；Semantic IR和`.zro`不保存 source terminator。formatter输出显式分号，minifier可以删除全部非必要换行和空白。

缺失分号必须产生定向诊断和插入式修复；不能在报错后静默按换行继续编译。这样压缩源码可以安全折叠为单行，同时不会引入 JavaScript ASI 的 `return/await/yield` 行边界规则。

## 7. 函数、参数和引用语法

### 7.1 函数声明

建议函数声明统一要求 `fn`：

```zr
pub fn add(left: int, right: int): int {
    return left + right;
}

let operation: fn(int, int) -> int = add;
```

`fn` 同时用于函数声明和函数类型，替代 `%func`。这会增加少量字符，但可以消除关键字缺失函数声明造成的试探解析，并减少函数、泛型和表达式之间的回溯。

函数定义与 callable 类型刻意使用不同分隔符：

```zr
fn parse(input: string): Node;
let parser: fn(string) -> Node = parse;

fn createParser(config: Config): fn(string) -> Node;
let curry: fn(A) -> fn(B) -> R;
```

- 命名函数、局部函数、method、interface member、native extern block中的函数声明和匿名函数定义统一以 `:` 引出 return TypeRef。
- callable 类型表达式统一以 `->` 表示 parameter types 到 return TypeRef 的映射，并按右结合解析；`fn(A) -> fn(B) -> R` 等价于 `fn(A) -> (fn(B) -> R)`。
- `=>` 只引出 expression body，不表示 return type，也不是 `->` 的兼容拼写。
- `:` 在参数列表闭合后的函数定义上下文中是 return annotation delimiter；它与参数/变量/property 的 `name: Type` 同样表示“声明绑定到类型”，但 parser 状态不同，不需要符号表消歧。

匿名函数也遵守定义语法：

```zr
let increment = fn(value: int): int => value + 1;
let factory = fn(): fn(int) -> string => createFormatter();
let block = fn(value: int): int {
    return value * 2;
};
```

这样 `fn createParser(...): fn(string) -> Node` 的第一个 `:` 固定标记函数定义的返回类型边界，内层 `->` 只属于 callable TypeRef，不会形成连续箭头链的视觉结合负担。

### 7.2 参数模式

参数语法使用 `name: type` 的 ZR 既有方向，引用修饰进入类型位置：

```zr
fn inspect(value: in Data): void;
fn mutate(value: scoped ref Data): void;
fn select(left: ref Data, right: ref Data): ref Data;
fn create(result: out Data): bool;
```

规范语义为：

| 表层类型 | 规范语义 | 是否要求 Place | 是否可写 | 默认可否逃逸 |
|---|---|---:|---:|---:|
| `T` | 按值传递 | 否 | 对参数绑定否 | 不适用 |
| `in T` | `scoped ref readonly T` 参数简写 | 否，可创建临时值 | 否 | 否 |
| `ref T` | 可写位置引用 | 是 | 是 | 按 safe-context 推导 |
| `ref readonly T` | 只读位置引用 | 是 | 否 | 按 safe-context 推导 |
| `scoped ref T` | 函数内可写引用 | 是 | 是 | 否 |
| `out T` | 未初始化的 `scoped ref T` | 是 | 是 | 否 |

`out T` 进入函数时不能读取，所有正常返回路径必须完成赋值。异常路径不要求产生一个可观察的完整值。

### 7.3 调用点语法

```zr
inspect(data);
mutate(ref data);
create(out data);
```

- `ref` 和 `out` 在调用点必须显式出现。`ref T` 允许修改调用者存储；`ref readonly T` 虽然不能写，但显式 `ref` 表明调用传递的是具有位置身份且可能参与引用返回的别名。
- `in` 调用不要求调用点标记，以降低只读热路径的书写负担。
- rvalue 可以传给 `in`，编译器可以创建不可逃逸临时位置。
- rvalue 不能传给 `ref`、`ref readonly` 或 `out`。

### 7.4 局部引用和引用返回

```zr
var value = 10;
let writable: ref int = ref value;
let view: ref readonly int = ref value;

fn choose(left: ref int, right: ref int, useLeft: bool): ref int;
```

规则：

- `ref` 初始化只能引用 Place。
- 存在活动可写引用时，原 Place 及其重叠 Place 不得被另行读取、写入或移动。
- 存在活动只读引用时，允许其他只读引用，但不允许重叠写入或移动。
- 返回引用的 safe-context 不得长于其来源。
- `scoped` 引用不能返回、存入逃逸字段、被闭包捕获或跨越 `await/yield`。
- 普通开发者不书写生命周期名字；编译器通过 block、function、caller、heap/static 的逃逸格推导。

## 8. 可变性和 readonly

### 8.1 绑定、常量与访问能力分离

```zr
var count = 0;
let config = loadConfig();
const CacheLine = 64;

let view: ref readonly Data = ref data;
```

- `var`：绑定可以重新赋值。
- `let`：绑定不能重新赋值，但不自动把其持有的对象变成深度只读。
- `const`：值必须可在编译期求得。
- `readonly`：表示类型或访问路径的只读能力。

`let list: List` 仍可以通过其可写对象能力调用修改成员；`let` 只禁止把 `list` 绑定到另一个值。只读对象视图使用 `readonly List` 或由 `in/ref readonly` 产生。

从 readonly class/interface 能力读取或复制出的 handle 仍是 `readonly T`，不能通过一次局部赋值恢复成可写 `T`。从 readonly 到 writable 的能力提升必须来自另一个已存在的 writable owner/ref，不能通过 cast 或类型推断凭空产生。

### 8.2 destructuring binding

```zr
let { width, localHeight: height } = record;
var [first, second] = numbers;
```

- object/array destructuring同时支持`let`和`var`，并要求整个binding statement以`;`结束。
- RHS只求值一次；projection/getter按pattern从左到右求值。
- object alias方向固定为`localName: sourceField`；未列出的field忽略。
- array pattern要求source至少有N项，额外项忽略；不足N项在静态长度已知时编译失败，动态长度只执行一次前置shape check。
- `let/var`统一作用于全部leaf binding；Copy leaf复制，move-only rvalue leaf移动。
- 对owner/custom-drop lvalue的任意partial move若Place/drop bitmap不能证明安全则拒绝。
- 第一版不加入default、rest或nested pattern，避免隐藏动态shape检查和partial cleanup复杂度。

### 8.3 只读对象能力

`readonly T` 是浅层只读能力：禁止通过当前能力调用 setter、写字段或调用可变成员，但其他别名仍可能修改同一对象。第一版不宣称对象图深度不可变。

为使 readonly 能跨 struct、class 和 interface 保持一致，方法需要记录 receiver effect：

```zr
const fn length(): int;
fn push(value: T): void;
```

- `const fn` 使用只读 receiver。
- `fn` 可写 receiver，并允许修改实例状态。
- property setter 和 `init` accessor 同样要求相应的可写/构造期 receiver。
- interface 和函数元数据必须保存该 effect，虚调用不能在运行时猜测。

编译器可以帮助推断私有方法是否实际写入 receiver，但公开 ABI 和 interface 声明必须具有稳定、可序列化的 receiver effect。

### 8.4 readonly struct

```zr
readonly struct Vec2 {
    let x: float;
    let y: float;
}
```

- 实例字段必须在构造完成前确定赋值。
- 构造完成后不能修改实例字段。
- 实例方法默认获得只读 receiver。
- 对 readonly/in struct 调用只读方法不产生防御性复制。

## 9. struct、ref struct 与布局

### 9.1 类型类别

```zr
struct Point { ... }
readonly struct Vec2 { ... }
readonly ref struct Span<T> { ... }
readonly ref struct ReadOnlySpan<T> { ... }
class Node { ... }
resource class Texture { ... }
```

- `struct`：内联值类型，可以位于栈、对象字段、数组或其他聚合中。
- `readonly struct`：构造完成后不可通过该值修改字段。
- `ref struct`：可以包含引用字段，但受栈/作用域逃逸限制。
- `readonly ref struct`：组合上述两类约束。
- `class`：普通 GC 引用类型。
- `resource class`：确定性生命周期对象，只能由 ownership handle 持有。

### 9.2 struct 显式构造

struct、readonly struct、ref struct 和 readonly ref struct 统一使用显式值初始化表达式：

```zr
let point = init Point(10, 20);
let pair = init Pair<int>(left, right);
let span = init Span<int>(ref values[0], values.length);
```

概念 grammar：

```text
StructInitExpression
  := "init" TypeRef "(" ArgumentList? ")"
```

`init` 是表达式起始位置的上下文关键字；`fn init(...)`、`value.init(...)` 和普通 `init(...)` 调用仍可使用标识符 `init`。一旦匹配 `init TypeRef (`，parser 必须进入 TypeRef 上下文，因此 qualified type、generic type、type alias 和满足可构造约束的泛型参数不依赖表达式回溯或符号命名约定。

构造与调用严格分离：

| 表层形式 | 目标 | 规范语义 |
|---|---|---|
| `init S(...)` | value-constructible `TypeRef` | 解析 `@constructor`，原地构造 inline value |
| `new C(...)` | 普通 `class` | 分配并构造 GC 对象 |
| `own R(...)` | `resource class` | 构造并返回 `Unique<R>` |
| `expr(...)` | callable value | 普通调用或 `@call` |

必须满足：

- `init` 的 operand 是 TypeRef，不是任意表达式；不能写 `init (getPrototype())(...)` 或 `init runtimeType(...)`。
- `init` 只查询已绑定 TypeDef 的 value-constructible capability 和 constructor set，不触发 `@call`。
- 普通 `StructData(...)` 始终保持 call/`@call` 语义，不因名字恰好绑定到 struct 类型而回退到构造。
- `init S(...)` 与 `S(...)` 之间不存在任意方向的语义 fallback。
- argument list 绑定 constructor 参数；命名参数不是公开字段写入，也不能绕过访问控制、definite initialization 或构造器不变量。
- `init S()` 必须解析到可访问的零参数构造器，或按语言规则合成的默认构造器；不得在存在显式构造语义时静默退化为 raw zeroing。零值能力应由独立的 default/zero-initialization 契约表达。
- 第一版不增加 `S { field: value }` aggregate literal；它会与控制流 block 产生解析压力，并可能绕过 constructor/private field invariant。

旧 `$Type(...)` 在迁移期绑定为静态 TypeRef 后改写成 `init Type(...)`。旧 `$proto(...)` 若 target 是运行时 `zr.reflection.Type`，则排除在核心语法之外：migration 标记为 `requiresReview`，调用方先以 `reflection.requireConstructible(type)` 验证 capability，再调用 `constructible.createInstance(...constructionArgs)`。公开签名固定为 `const fn createInstance(...constructionArgs: object): object`；调用点的 `constructionArgs` 是 `object[]` 并通过 `...constructionArgs` 展开。该 API 可以在运行时校验 type category、arity、argument conversion 和 result type，但不能复用 `StructInitExpression`、伪装成静态 TypeRef，或让普通 compiler path 恢复动态 prototype fallback。

lowering 必须优先分配/选择 destination Place，再执行字段初始化和 constructor body：

```text
let point = init Point(10, 20)
  -> destination = LocalPlace(point)
  -> ValueConstruct(destination, Point.TypeId, constructorId, arguments)
  -> mark initialized fields incrementally
  -> commit destination as definitelyInitialized
```

这样 VM/AOT 可以直接在 local、field、array element、hidden return destination 或 caller-provided temporary 中构造，避免先创建 object/boxed value 再复制。构造抛错时 cleanup CFG 只 drop 已完成初始化的字段；ref struct 构造成功后再由 region/escape checker 验证引用来源。普通 inline struct 构造不得产生 GC allocation、prototype hash dispatch 或运行时“是不是 struct”检查。

### 9.3 ref struct 规则

```zr
readonly ref struct Span<T> {
    let data: ref T;
    let length: usize;
}
```

`ref struct`：

- 不得装箱为普通 object/interface。
- 不得作为普通 GC class 字段。
- 不得存入普通 GC 数组。
- 不得被闭包捕获。
- 不得跨越 `await` 或 `yield`。
- 不得逃逸到其内部引用来源之外。
- 可以作为局部变量、scoped 参数、返回值或其他 ref struct 字段。
- 第一版不能把 ref struct 作为无约束泛型实参；通用 ref-like 泛型约束另行设计，不能通过具体类型白名单放宽。

第一版不复制 C# 后续对 interface 和泛型 anti-constraint 的所有放宽，先保持规则简单且可验证。

### 9.4 内存布局

- `struct` 字段内联，布局在编译期确定。
- 普通布局至少保证同一目标 ABI 下可查询大小、对齐和字段偏移。
- native interop 使用显式稳定布局契约，不能依赖普通 struct 的跨版本偶然布局。
- `T[]` 保证元素连续存储。
- 固定长度内联数组和显式 `align/packed/extern layout` 语法另行设计，不在本文内锁定拼写。
- AOT、VM 和反射必须消费同一个 TypeLayout，不得各自重新计算字段偏移。

## 10. 所有权模型

### 10.1 删除名义借用包装

以下概念从最终源语言删除：

```text
Borrow<T>
Loan<T>
%borrow
%borrowed
%loan
%loaned
```

它们分别由 `ref readonly T`、`ref T`、region 和借用数据流表达。loan 是 checker 状态，不是需要分配或传递的运行时对象。

### 10.2 owner 类型

```zr
resource class Texture { ... }

let texture: Unique<Texture> = own Texture(...);
let shared: Shared<Texture> = texture.share();
let weak: Weak<Texture> = shared.weak();

if let Some(value) = weak.upgrade() {
    use(value);
}

drop(shared);
```

规范语义：

- `own T(...)` 只构造 ownership-capable 类型，并返回 `Unique<T>`。
- `Unique<T>` 不可复制；按值赋值、传参和返回执行 move。
- move 后源 Place 进入 moved 状态，除重新赋值外不可使用。
- `Unique<T>.share()` 消耗 unique，返回 `Shared<T>`。
- `Shared<T>` 复制增加 strong count，drop 减少 strong count。
- `Shared<T>.weak()` 创建 `Weak<T>`，不增加 strong count。
- `Weak<T>.upgrade()` 返回 `Option<Shared<T>>`，不使用隐式 nullable。
- `drop(value)` 是标准 intrinsic，用于可选的提前确定性释放；正常作用域退出自动 drop。
- final owner drop 执行一次且仅一次资源清理，不通过运行时“源变量置 null”模拟语义正确性。

### 10.3 Shared 的线程语义

推荐第一版：

- `Shared<T>` 使用非原子引用计数，只能在线程或 actor 隔离域内使用。
- 跨线程共享使用后续显式类型 `AtomicShared<T>`。
- 编译器通过 `Send/Sync` 或等价 capability 检查跨线程移动和共享。

这样普通共享对象不默认承担原子加减成本，并且成本在类型上可见。

### 10.4 强引用环

- `Shared<T>` 不提供 cycle collector。
- 长期图结构必须使用 `Weak<T>` 打断反向边。
- 编译器必须诊断过程内可以证明的强环，并对高风险字段给出 lint。
- 编译器不能声称通过局部分析消灭全部跨模块动态环。
- GC 继续负责普通 class 的循环对象图；Shared 不替代 GC。

### 10.5 GC world 与 ownership world

推荐保持两个明确生命周期世界：

- 普通 `class` 通过 `new` 创建，由精确 GC 管理。
- `resource class` 通过 `own` 创建，由 `Unique/Shared` 管理。
- 纯 ownership graph 不参与普通 GC liveness tracing。
- 跨世界引用必须经过规范化 bridge/handle，禁止在共享基础路径里通过具体类名或隐藏 registry 特判。
- `.intoGc()` 第一版只消耗 `Unique<T>` 并生成 `GcBox<T>`；它表示主动放弃确定性释放，不承诺零拷贝或地址不变。

采用严格边界：resource class 直接字段不能静默保存普通 GC handle；需要保存时使用显式 `Gc<T>` bridge。这样运行时能够准确登记根，并使“是否需要 GC 扫描”在类型上可见。

### 10.6 Drop、close 与 using

- owner 在作用域结束自动 drop，不需要 `using owner`。
- `drop(value)` 只表示提前消耗并结束 owner 生命周期。
- `using` 只保留一个职责：管理实现 Close/Dispose 协议但不由 owner 自动管理的资源作用域。
- close 与最终对象销毁是不同协议，不能由同一个 `%release` 模糊处理。
- 异常退出、早返回和正常退出必须生成同一套 cleanup plan。
- pattern matching 和插件装载不再复用 `using`。

## 11. 属性设计

### 11.1 统一声明

字段和属性具有明确语法边界：

```zr
pri var count: int;
pri let id: Guid;

pub property size: int {
    pub get {
        return count;
    }

    pri set {
        count = value;
    }
}

pri var _age: int;

pub property age: int {
    pub get {
        return _age;
    }

    pub set {
        require(value >= 0);
        _age = value;
    }
}

pub property first: ref readonly Item {
    pub get => ref items[0];
}
```

- `var/let` 声明字段，并与 local 采用相同 replaceability：`let` 初始化后不可替换，`var` 可替换。
- field 与 accessor 都使用 `pri/pro/pub`；accessor 省略时继承 property access，且不能比 property 更宽。
- `property` 声明属性，避免 class field、interface accessor 与对象字面量之间的解析歧义。
- class 与 interface 使用同一个 PropertyDecl AST；interface accessor 只有签名，没有实现体。
- concrete property 必须显式访问预先声明 field、method 或代理目标；不生成 backing field，不定义 contextual `field`。
- `value` 只在 setter/init accessor 内表示输入值。
- 支持 `get`、`set`、`init`、accessor 可见性和 ref return。
- `ref T`/`ref readonly T` property 是 getter-only；不存在 ref setter 或 `property = ref other`。
- writable ref getter记录独立`exportsWritableRef` effect；ordinary readonly object不能调用，但`Span<T>`/`PoolRef<T>`等显式writable-ref view可在自身field不可替换的同时导出可写referent。
- setter 要求 writable receiver；getter 默认只读。

### 11.2 代理和优化契约

- property 不拥有 storage slot；显式 field 独立进入 TypeLayout、GC/ownership map 和 reflection。
- 调用点只依赖 property/accessor 契约，不依赖 field 命名或隐式关联。
- 同模块且 accessor 可见时，VM/AOT 可以内联 getter/setter，但不能跳过访问控制或副作用。
- ref-return property 返回真实 Place 的引用，必须经过与普通 ref return 相同的逃逸检查。

## 12. 连续内存、Span 和池化

### 12.1 连续存储模型

推荐以三种角色组合连续内存能力：

- `T[]`：拥有元素存储的 GC 数组，元素连续。
- `Buffer<T>` 或 owner-backed array：拥有确定性释放的连续存储。
- `Span<T>` / `ReadOnlySpan<T>`：不拥有内存的 ref struct 视图。

`Span<T>` 的规范形态是 `ref T + length`，创建和切片不分配。`ReadOnlySpan<T>` 使用 `ref readonly T + length`。

第一版建议数组到 Span 使用显式 `.span()` 或构造，暂不引入 C# 14 全套一等 Span 隐式转换，以避免重载解析和二义性膨胀。

### 12.2 边界检查

- 越界访问的语言语义仍然必须产生确定错误，不能为性能直接删除。
- 编译器可通过常量范围、循环归纳变量、slice 长度关系和支配条件证明安全后消除检查。
- Span 的来源、长度和 slice 范围进入 Semantic IR，不能退化为无法证明关系的普通整数。
- unsafe/native 边界绕过检查必须具有单独显式能力，不由普通 `ref` 自动获得。

### 12.3 池化

池化位于标准库 `zr.pooling`，不引入 `pool` 关键字。Buffer/Span lease 与实体 weak handle 是两种相关但不同的 API。

Buffer lease：

```zr
var lease = pool.rent<int>(1024);
var cells: Span<int> = lease.span();

process(cells);
// lease 离开作用域，缓冲区自动归还池
```

实体 handle：

```zr
let handle: PoolHandle<Particle> =
    particlePool.deliver(init Particle(...));

var particle: PoolRef<Particle>;
if (particlePool.tryBorrow(handle, out particle)) {
    particle.value.position.x += 1.0;
}

particlePool.recycle(handle);
```

规范契约：

- `rent` 返回 `Unique<PoolLease<T>>` 或语义等价 owner。
- lease 的 drop 把存储归还池，而不是释放底层 arena。
- 从 lease 取得的 Span 不能比 lease 活得更久。
- 存在活动 Span borrow 时，不允许 move/drop lease。
- `PoolHandle<T>` 是普通 readonly struct，保存 `PoolId + slotIndex + generation`，不包含 GC handle/ref，可长期流转。
- `PoolRef<T>`/`PoolReadRef<T>` 是 readonly ref struct；`tryBorrow/tryRead(handle, out view): bool` 单次验证 handle 后初始化 direct ref + guard。
- 第一版不使用`Option<PoolRef<T>>`，因为ref struct不能作为普通generic argument；Try/out失败路径产生default不可解引用view且guard drop为no-op。
- recycle 立即把 Live entity标记为Retired，使新 borrow失败；slot实际Drop/reuse延迟到active guards归零。
- 旧 generation handle 即使 slot复用也不能重新生效；回绕前必须retire slot或使用足够宽identity。
- PoolRef的`value: ref T`是getter-only；修改referent写`poolRef.value = replacement`，切换实体则替换整个`var PoolRef`。
- inline struct slab由TypeLayout计算`GcFree/GcMapped/GcBarriered`；只有`GcFree`整段可NoScan。
- ordinary class pooling不会自动消除per-object GC header/tracing；真正连续化优先使用fixed-layout struct slot。
- 池按 size class、线程域或 arena 分配属于标准库和运行时实现策略。

逐个 `own class` 分配影响的是 ownership heap/arena fragmentation，不是栈碎片。需要 resource identity 时可让 pool 成为唯一 owner，handle只借用；reclamation阶段执行确定Drop。

## 13. 关键字与 `%xxx` 收敛

### 13.1 收敛原则

- 语法结构使用普通或上下文关键字。
- 类型能力使用类型构造。
- 运行时转换和 ownership 操作使用普通方法或 intrinsic。
- 纯静态 borrow/loan 状态不出现在运行时 API。
- 一个关键字不能同时承担模块导入、动态插件加载、模式匹配和资源作用域等多个职责。

### 13.2 完整映射

| 当前形式 | 推荐形式 | 处理方式 |
|---|---|---|
| `%async` | `async` | 上下文关键字 |
| `%await` | `await` | 上下文关键字 |
| `%borrow` | `ref readonly` / 自动 shared borrow | 删除内建表达式 |
| `%borrowed` | `ref readonly T` | 删除装饰类型 |
| `%compileTime` | `comptime` | 上下文关键字 |
| `%detach` | `.intoGc()` | 显式跨世界转换 |
| `%extern` | `native extern("library") { ... }` | 静态 native 声明；绑定期生成 FfiSignature |
| `%func` | `fn` | 函数声明和函数类型关键字 |
| `%import` | `let alias = import("module.path");` | 返回 ModuleNamespace object的静态导入绑定 |
| `%in` | `in T` | 参数类型简写 |
| `%loan` | `ref` / 自动 mutable borrow | 删除内建表达式 |
| `%loaned` | `ref T` | 删除装饰类型 |
| `%module` | `module` | 模块声明 |
| `%out` | `out T` | 参数类型简写 |
| `%owned` | `resource class` | 类型生命周期类别 |
| `%ref` | `ref T` | 真正引用类型 |
| `%release` | `drop(value)` | 提前确定性释放 intrinsic |
| `%shared` | `Shared<T>` / `.share()` | owner 类型和转换方法 |
| `%test` | `test` | 上下文关键字 |
| `%type` | `typeof(expr)` / `typeid(T)` | 拆分值类型查询和类型标识 |
| `%unique` | `Unique<T>` / `own T(...)` | owner 类型和构造表达式 |
| `%upgrade` | `.upgrade()` | Weak 的普通方法 |
| `%using` | `using` | 只用于 Close/Dispose 作用域 |
| `%weak` | `Weak<T>` / `.weak()` | owner 类型和转换方法 |
| `$Type(args)` | `init Type(args)` | 仅在 target 可静态绑定为 value-constructible TypeRef 时自动迁移 |
| `$proto(args)` | `reflection.requireConstructible(type).createInstance(...constructionArgs)` | 动态构造移出核心语法，需绑定为 `zr.reflection.Type` 并人工确认 capability、参数数组和装箱 |

### 13.3 module、import 与动态加载

```zr
module app.render;

let memory = import("core.memory");
let localMath = import(".math.vector");
let tools = import("#lib/tool");
let matrix = import("@math.matrix");
let vector: memory.Vector = init memory.Vector(1, 2);
memory.flush();
```

- `module`不是 braced declaration，因此必须以 `;` 结束。
- `import("...")` 是专用 ImportExpression，不是可 shadow/override 的普通函数；第一版只接受一个 string literal。
- `let alias = import("...");` 第一版只允许 module scope immutable binding。binding返回只读 ModuleNamespace object，同时建立 ModuleId、artifact dependency和type namespace alias。
- imported value/function通过 `alias.member` 访问；imported type可在TypeRef中写 `alias.Type`，但这种资格只属于原始 import binding，`let other = alias;` 不会让 `other.Type` 成为合法TypeRef。
- 重复导入同一 canonical ModuleId返回同一environment内的module object并只初始化一次；source/binary/native provider仍经过统一resolver。
- module literal先解析为结构化ModuleSpecifier，再解析为Canonical ModuleId/ModuleIdentity。`core.math`与`core/math`、`.math.vector`与`./math/vector`、`..math.vector`与`../math/vector`分别等价。
- `.zrp` workspace alias只允许单段`#identifier`，例如`"#lib": "core/lib"`；`#lib/tool`在segment边界展开。
- 第三方package root只允许单段`@identifier`。`@math`是root/default entry，`@math.matrix`与`@math/matrix`是同一个exported submodule；第一版不允许package name包含`.`或`/`。
- 显式`.zrm` locator由assembly manifest解析entry；`.zrm`内部依赖仍保存Canonical ModuleId，不能把容器member path当模块身份。
- 动态插件加载使用普通 API，例如 `loadPlugin("render.vulkan")`，返回显式结果 union。
- runtime path、conditional/local loading使用 `loadModule/loadPlugin`；不能写 `import(path)`，也不能把 import expression藏在conditional或普通call argument中。
- 单分支 union 解构使用 `if let`，多分支使用 `switch/match`，不再使用 `using` 充当模式守卫。

## 14. 规范化语义类型

语义层不能继续同时依赖 AST `ownershipQualifier`、`ownershipBuiltinKind` 和字符串类型名。规范模型分为结构类型身份、使用点约束和 callable contract：

```text
Canonical TypeNode
  kind: nominal | function | ref | owner | readonlyView | tuple | union | array | ...
  structural child TypeIds

SemanticValueType
  typeId
  escapeUpperBound

CallableValueContract
  typeId
  passingForm: value | in | ref | refReadonly | out
  entry/exit initialization
  escapeUpperBound
  call-site marker
```

这些字段是示意性的语义信息，不要求最终 C 结构按同一扁平布局实现。关键要求是：

- 所有层消费同一个规范化 TypeId/TypeRef。
- 不通过类型名字符串判断 `Span`、`Unique` 或其他内建类型。
- 标准库类型通过 capability、layout 和 protocol 注册成为普通消费者。
- parser 只保存语法来源；semantic layer 决定规范类型和能力。

## 15. 数据流与静态检查

### 15.1 Place 状态

每个 Place 或可区分投影分维度跟踪：

```text
Initialization: uninitialized | initialized | maybeInitialized
Availability: available | moved | maybeMoved | dropped
Borrowing: sharedLoanSet + optionalMutableLoan
Escape: block | function | caller | heapStatic | unknown
```

字段级 partial move 只在布局和析构协议允许时开放；第一版可以保守禁止复杂对象的 partial move，但不能默默退化成运行时 null 检查。

### 15.2 Region 与逃逸

推荐使用不暴露给普通源代码的逃逸格：

```text
temporary/block < function < caller < heap/static
```

检查范围必须覆盖：

- 局部变量和字段投影。
- 参数与引用返回。
- 闭包捕获。
- async/yield suspension point。
- 数组、容器和对象字段存储。
- 模块导出、全局变量和 native handle。
- owner 与 GC bridge。

### 15.3 运行时检查边界

必须静态完成：

- use-after-move。
- double-drop。
- 读未初始化 `out`。
- 正常返回前未写 `out`。
- 写入活动 readonly borrow。
- 活动 mutable borrow 的别名访问。
- ref struct/borrow 非法逃逸。
- borrow 跨 await/yield。

仍可能需要运行时完成：

- 无法静态证明安全的数组/Span 边界检查。
- null、dynamic cast 和外部输入校验。
- Weak upgrade 是否成功。
- Shared strong count 变化。
- native/unsafe 契约保护。

## 16. GC 与运行时性能契约

- GC class 使用精确类型描述和根图，不依赖保守扫描猜测引用。
- GC 可继续采用 region、分代、remembered set、并发 major 标记和选择性 compact。
- ownership 对象的生命周期不由 GC 决定。
- ownership 到 GC 的 bridge 必须成为显式根并参与写屏障契约。
- `ref struct` 和普通 borrow 不进入堆对象图，也不需要运行时借用表。
- `PoolHandle<T>` 若只含scalar identity则不进入GC reference graph；`PoolRef<T>`只存在于active frame/ref-like aggregate。
- pool/slab长期存活不能自动跳过mark；只有TypeLayout `GcFree`可NoScan，`GcMapped/GcBarriered`仍参与precise map、barrier/card与remembered set。
- VM 与 AOT 对 owner move/drop、ref load/store 和 property accessor 使用相同 SemIR 操作。
- 优化器可以做标量替换、栈分配、检查消除和 accessor 内联，但这些优化不能改变可观察生命周期与错误语义。

## 17. 诊断要求

诊断必须指出能力冲突，而不是只报告“类型不兼容”。示例形态：

```text
ZR2104: cannot write `buffer[0]` while readonly borrow `view` is active
  borrow begins here: line 12
  write occurs here: line 15
  borrow is last used here: line 18
```

至少需要稳定覆盖：

- use-after-move，并指出 move 发生点。
- borrow 与写入冲突，并指出 borrow 存活区间。
- 引用返回逃逸，并指出返回值依赖的较短 region。
- ref struct 被装箱、捕获或跨 suspension point。
- `out` 未赋值和赋值前读取。
- readonly receiver 调用 mut member。
- resource/GC world 非法直接交叉。
- Shared 强环的确定错误或风险 lint。
- 旧 `%xxx` 写法的定向迁移提示。
- 裸 `StructData(...)` 不可调用时提示“普通调用不会构造 struct”，并在无 `@call` 歧义且 target 可静态绑定时建议 `init StructData(...)`。
- `init` target 不是 TypeRef、不是 value-constructible 类型、构造器不可访问或 arity/type 不匹配。
- `new StructType(...)`、`own StructType(...)`、`init ClassType(...)` 和 `init ResourceType(...)` 指出正确的生命周期构造形式。
- 旧 `$proto(...)` 明确提示 runtime Type object 构造已移出核心语法；target 可绑定为 `zr.reflection.Type` 时建议 `requireConstructible(target).createInstance(...constructionArgs)` 的 requiresReview edit，不提供把任意 expression 自动改成 `init` 的修复。

LSP 必须复用相同 semantic facts 提供 hover、类型展示、move 后不可用高亮和借用范围提示，不能重新实现一套简化规则。

## 18. 验证矩阵

### 18.1 Parser 与诊断

- 所有新关键字在合法上下文中解析，在普通标识符上下文中按上下文关键字规则工作。
- `ref T` 覆盖参数、局部、字段、返回值、泛型参数和 property。
- 非 Place 的 `ref/out` 调用产生精确诊断。
- 旧 `%xxx` 在迁移期产生可操作诊断，而不是普通 token error。
- property 在 class/interface 中形成同一 AST 结构。
- 命名/局部/member/interface/async/anonymous function definition与native extern block declaration使用 `:`；FunctionTypeSyntax使用 `->`；anonymous expression body使用 `=>`。
- 返回 callable、右结合嵌套 callable、definition/type 半输入与 `:`/`->`/`=>` 错位均有定向诊断和精确 token range。
- `init TypeRef(...)` 覆盖 qualified/generic/alias/ref-like TypeRef、零参数/多参数/命名参数以及上下文关键字边界。
- `StructData(...)`、`init StructData(...)`、`new ClassData(...)` 和 `own ResourceData(...)` 形成四种互不 fallback 的 AST/绑定路径。

### 18.2 Semantic IR 与类型系统

- TypeRef 规范化不依赖具体类型名字符串。
- Place 投影覆盖 local、field、index、deref 和 property ref return。
- receiver effect 进入 overload resolution 和 interface dispatch。
- Region、move、definite assignment 和 cleanup plan 在 CFG 合流处正确合并。
- 泛型约束可以表达 value、ref-like、owner、copy、drop、send/sync 等 capability。
- struct init 在 bind 后携带 TypeId、ConstructorId、argument contracts 和 destination Place；不得保留 runtime `zr.reflection.Type` expression。

### 18.3 VM、AOT 与 artifact

- VM 与 AOT 对 ref load/store、move/drop、Weak upgrade 和 property accessor 行为一致。
- `.zrs/.zri/.zro` 保存规范类型、引用能力、owner kind、receiver effect 和布局信息。
- `.zrs` 保留 definition/type/body delimiter；`.zri/.zro` 的 canonical callable contract 不把 `:`/`->`/`=>` 计入类型身份。
- module loader 不从表层拼写恢复语义。
- 调试器能显示 owner 状态、ref 来源和 ref struct 内容。
- VM/AOT 对 struct init 直接写入 destination Place；普通 inline 构造无隐藏 GC allocation，部分构造失败 cleanup 顺序一致。

### 18.4 边界与负例

- 空 Span、单元素 Span、最大长度和 slice 边界。
- 嵌套 reborrow、条件分支借用、循环内借用和提前返回。
- partial initialization、异常退出和构造失败清理。
- constructor 与 `@call` 同时存在、同名 value/type shadowing、imported alias、runtime Type object 和反射边界。
- closure、async、generator、module/global/native 逃逸。
- Shared 最后一个 strong drop、Weak upgrade 成功/失败和重复 upgrade；引入 AtomicShared 后另测并发 upgrade。
- pool lease 正常退出、异常退出、Span 仍活动时 move/drop；PoolHandle stale/wrong-pool/generation-wrap和PoolRef active期间recycle/deferred reuse。
- GC 在 owner bridge 活动期间运行。

### 18.5 压力与性能

- 大量短命 GC 对象与纯 owner 对象图对比。
- 高频 Unique move/drop 不产生隐藏分配。
- Shared 热循环计数成本可测且不会误用原子路径。
- Span 热循环的边界检查消除和未消除路径均有基准。
- 大量 pool rent/return、handle validate/reject、retire/reuse、不同 size class 和跨线程误用；分别记录GcFree/GcMapped scan bytes。
- 深层对象图、Weak 图和资源析构链不发生 double free 或栈失控。
- 大量 inline struct 构造、嵌套构造和 constructor-return 热路径不退化为 prototype 动态分派或临时 boxing/copy。

## 19. 参考实现与设计依据

### 19.1 C# 与 .NET

- `ref struct`、safe-context 和逃逸限制：`lua/csharplang/proposals/csharp-7.2/span-safety.md`
- `ref` 字段与 `scoped`：`lua/csharplang/proposals/csharp-11.0/low-level-struct-improvements.md`
- `in` 与 `ref readonly` 的调用差异：`lua/csharplang/proposals/csharp-12.0/ref-readonly-parameters.md`
- readonly struct/member：`lua/csharplang/proposals/csharp-7.2/readonly-struct.md`、`csharp-8.0/readonly-instance-members.md`
- property auto/backing field对照（ZR不采用）：`lua/csharplang/proposals/csharp-14.0/field-keyword.md`
- Span 实际布局：`lua/runtime/src/libraries/System.Private.CoreLib/src/System/Span.cs`
- Roslyn 负例：`lua/roslyn/src/Compilers/CSharp/Test/Semantic/Semantics/RefEscapingTests.cs`、`RefLocalsAndReturnsTests.cs`、`SpanStackSafetyTests.cs`、`RefFieldTests.cs`
- C# struct object-creation 与参数构造测试：`lua/csharplang/proposals/csharp-10.0/parameterless-struct-constructors.md`、`lua/roslyn/src/Compilers/CSharp/Test/Semantic/Semantics/StructConstructorTests.cs`。
- C# method 与 function-pointer type 采用不同声明/类型结构：`lua/csharplang/proposals/csharp-9.0/function-pointers.md`、`lua/roslyn/src/Compilers/CSharp/Test/Syntax/Parsing/FunctionPointerTests.cs`。

采用的共同核心是：引用具有读写能力和逃逸范围，Span 是 ref-like 值类型，readonly receiver 避免无意义复制。ZR 不复制 C# 的历史重载兼容和全部隐式 Span 转换。

### 19.2 Rust

- 不可写共享引用：`lua/rust/tests/ui/borrowck/assignment-to-immutable-ref.rs`
- use-after-move：`lua/rust/tests/ui/borrowck/borrow-of-moved-value-in-for-loop-61108.rs`
- 栈变量借用逃逸：`lua/rust/tests/ui/borrowck/borrowck-borrow-from-stack-variable.rs`
- 其他边界来源：`lua/rust/tests/ui/borrowck`、`moves`、`drop`、`reborrow`、`pin`
- struct literal/block 解析冲突与负例：`lua/rust/compiler/rustc_parse/src/parser/mod.rs`、`lua/rust/tests/ui/parser/struct-literals-in-invalid-places.rs`、`lua/rust/tests/ui/empty/empty-struct-braces-expr.rs`。
- Rust declaration/function type 共用 `->` 的对照 parser：`lua/rust/compiler/rustc_parse/src/parser/ty.rs`。ZR 刻意让 definition 使用 `:`，避免返回 callable 时出现需要人工判断结合关系的连续箭头。

采用的共同核心是：move 后静态失效，共享借用与独占借用互斥，借用不得长于来源。ZR 刻意不要求普通用户书写命名生命周期，也不把所有 class 都改成所有权对象。

### 19.3 Lua、QuickJS 与 CPython

- Lua incremental/generational barrier：`lua/src/lgc.c`
- QuickJS 引用计数和 cycle removal 阶段：`lua/QuickJS-master/quickjs.c`
- CPython arena/pool/size class：`lua/cpython/Include/internal/pycore_obmalloc.h`、`lua/cpython/Objects/obmalloc.c`
- CPython 将 class construction 置于普通 callable/type-call protocol 的对照实现：`lua/cpython/Objects/typeobject.c` 中的 `type_call`。ZR 因为同时保留显式 `@call` 和静态值布局，刻意不采用“调用类型即构造”的统一动态入口。
- CPython 分离 function definition 和 standalone function type grammar：`lua/cpython/Grammar/python.gram`、`lua/cpython/Lib/test/test_type_comments.py`。ZR 在该语法分层基础上进一步分离 delimiter 职责。

这些实现说明：GC 屏障、引用计数环处理和池化分配器属于不同运行时问题。ZR 不应试图用一个 `%shared` 或 `%using` 表层特性同时解决三者。

## 20. 与现有计划的关系

本设计已经人工确认，将替代现有计划中的以下设计方向：

- 不再保留 `%borrow/%loan/%borrowed/%loaned` 作为表层类型或表达式。
- 不再以“把源变量运行时置 null”作为 move 正确性的基础。
- 不再让 `%using` 兼任 owner 构造、pattern guard 和插件 guard。
- 不再通过 ownership 类型名字符串和散落枚举决定编译行为。
- 不再把 `%owned` 的同一实例同时允许普通 `new` 和 owner 构造；改为 `class` 与 `resource class` 显式区分。
- 不再用隐藏 GC ignore registry 自动猜测跨世界生命周期；跨世界通过规范 bridge 表达。
- 不再使用 `$` 原型表达式承担 struct 构造；静态值构造改为 `init TypeRef(...)`，运行时 prototype 构造退出核心语法。

以上内容作为新的设计基线；既有计划只有在与本设计一致时才继续有效，冲突部分应标记为 superseded。

## 21. 已确认默认值

以下项目按本设计和子设计采用明确默认值；后续如需改变，必须先修改对应设计和 artifact/测试契约：

1. 函数声明是否强制 `fn`。
   结论：强制，删除关键字缺失函数声明和 `%func` 双轨。
2. 所有权类型声明使用 `resource class` 还是 `owned class`。
   结论：`resource class`，因为它表达类型职责，而不是某个变量当前恰好被拥有。
3. `Shared<T>` 是否默认原子计数。
   结论：非原子且不可跨线程；跨线程使用显式 `AtomicShared<T>`。
4. resource class 是否允许直接保存 GC 引用。
   结论：必须通过显式 `Gc<T>` bridge，保持 ownership graph 可跳过 tracing。
5. `.intoGc()` 是零拷贝转换还是生成 GC box。
   结论：第一版只允许 `Unique<T>.intoGc()` 生成 `GcBox<T>`，不承诺地址不变或零拷贝。
6. 普通 struct 的布局稳定级别。
   结论：字段按声明顺序布局，同 target/schema 内可查询且一致；跨版本/native 稳定布局必须显式声明。
7. `readonly T` 是否进入第一版。
   结论：进入，作为 class/interface 的浅层只读能力；否则 `in` 对引用对象只能保护 handle 绑定，无法控制 setter 和 writable method。
8. 旧 `%xxx` 是否一次性删除。
   结论：语义一次性切换，parser 保留一个有期限的迁移诊断阶段，不长期双轨执行。
9. 函数返回类型使用 `:` 还是 `->`。
   结论：命名/匿名函数定义使用 `:`，callable 类型表达式使用 `->`，`=>` 只用于 expression body。函数定义返回 callable 时写 `fn make(...): fn(A) -> R`。
10. struct 实例化使用什么显式标识。
   结论：使用 `init TypeRef(...)`；`init` 只解析静态 TypeRef 和 `@constructor`，普通 `expr(...)` 只执行 call/`@call`。旧 `$proto(...)` 的动态原型构造排除在核心语法之外。
11. 运行时反射类型如何实例化。
   结论：反射位于 `zr.reflection`。runtime `Type` 先通过 `requireConstructible` 取得 `ConstructibleType` capability，再调用 `createInstance(...constructionArgs: object): object`。调用端把 `object[]` 作为 `...constructionArgs` 展开；只按位置绑定 public constructor，不解析 `@call`。普通 class 返回 GC object，struct 返回 boxed object；resource class、ref struct、interface、abstract type 和 open generic 必须拒绝。
12. 换行是否可以终止声明或语句。
   结论：不可以。simple declaration/statement必须显式使用 `;`；由 `{ ... }` 闭合的declaration可以省略一个尾随`;`，compound control-flow statement由自身grammar闭合且不消费declaration semicolon。formatter不在braced declaration后输出多余分号。
13. import是裸声明还是返回module object的表达式。
   结论：使用module-scope `let alias = import("module.path");`。ImportExpression只接受string literal，返回只读ModuleNamespace object并同时建立静态依赖/type namespace；动态路径使用显式loader API。
14. property 是否自动声明 backing field，以及 ref setter 是否存在。
   结论：都不存在。concrete property 必须显式代理预先声明的 `pri/pro/pub let|var` field或其他目标；ref-return property是getter-only，写referent通过writable ref getter完成，切换目标则替换整个ref-like view。
15. 对象池交付实体还是handle，以及GC如何跳过pool storage。
   结论：`zr.pooling.PoolHandle<T>` 是长期generational weak identity，`PoolRef<T>` 是一次验证后的scoped direct ref。recycle立即使handle失效、延迟slot复用；只有TypeLayout证明`GcFree`的slab可以NoScan。
16. native extern、模块alias与第三方包如何表示。
   结论：静态native声明使用`native extern("library") { ... }`并绑定为Canonical CallableContract + FfiSignature。workspace alias使用单段`#identifier`；package使用单段`@identifier`。`@math`是package root，`@math.matrix`与`@math/matrix`等价，组织作用域命名推迟到独立语法。

## 22. 子设计文档

总索引：[ZR 语法重设计子计划索引](./README.md)。十份细化设计为：

1. [Canonical TypeRef、Place IR、CFG facts 与 artifact schema](./2026-07-18-01-canonical-type-place-cfg-artifact-design.md)。
2. [`fn/ref/in/out/scoped/readonly` 与 borrow checker](./2026-07-18-02-reference-syntax-borrow-checker-design.md)。
3. [struct/ref struct、receiver effect、Span 与 layout](./2026-07-18-03-struct-ref-struct-span-layout-design.md)。
4. [resource class、Unique/Shared/Weak、Drop 与 GC bridge](./2026-07-18-04-resource-ownership-drop-gc-bridge-design.md)。
5. [property 统一 AST、显式字段与 ref-return property](./2026-07-18-05-property-unified-ast-design.md)。
6. [`%xxx` 迁移、LSP、文档与全项目 fixture](./2026-07-18-06-percent-migration-lsp-fixtures-design.md)。
7. [目标语法全覆盖参考工程](./2026-07-19-07-comprehensive-syntax-reference-fixture-design.md)。
8. [`zr.reflection` 独立反射库与运行时类型系统](./2026-07-19-08-reflection-library-type-system-design.md)。
9. [generational `PoolHandle<T>`、`PoolRef<T>` 与连续池化内存](./2026-07-19-09-generational-pool-handle-ref-struct-design.md)。
10. [Native extern、内建库、模块与包解析](./2026-07-19-10-native-ffi-module-package-design.md)。

每一步都必须先验证共享语义基础，禁止通过具体类型名、表层拼写或单个测试用例特判完成。
