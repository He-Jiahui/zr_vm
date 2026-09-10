---
related_code:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/parser.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_class.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_struct.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_interface.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_union.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_property.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_extern.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/parser/parser_class.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_struct.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_interface.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_union.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_property.c
plan_sources:
  - user: 2026-09-10 继续完善 ZrVm Wiki，要求详细介绍语法规则、用例和实现机制
  - docs/zr_language_specification.md
  - tests/fixtures/projects/syntax_reference_v1/syntax_reference_v1.zrp
tests:
  - tests/parser/test_union.c
  - tests/parser/test_property_unified_ast.c
  - tests/parser/test_syntax_reference_v1.c
  - tests/fixtures/projects/import_basic/src/classes_full_smoke.zr
  - tests/fixtures/projects/syntax_reference_v1/src/model.zr
doc_type: language-reference
---

# 对象类型、成员与构造参考

本页说明 ZR 的 nominal type 声明和成员表面：`class`、`struct`、`ref struct`、
`interface`、`enum`、`union`、字段、方法、统一 property 以及构造表达式。这里的
“接受”表示 production parser 能构造对应 AST；对象布局、继承合法性、override、资源
drop、泛型实参和运行时 provider 可用性仍由 compiler 与 runtime 决定。

与 [类型表达式与参数契约](type-expression-reference.md) 相比，本页聚焦声明的外形和
成员边界；与 [可调用对象、调用与异步迭代](callable-async-iterator-reference.md) 相比，
本页聚焦成员 receiver 与 construction。native descriptor 如何把类型投影给 ZR，见
[Native Provider Descriptor 深度参考](../03-modules/native-provider-descriptor-reference.md)。

## 1. 总体语法

```ebnf
type-declaration  ::= class-declaration | struct-declaration
                    | interface-declaration | enum-declaration | union-declaration ;

decorated-access  ::= { decorator }, [ access ] ;
access            ::= "pub" | "pri" | "pro" ;
decorator          ::= "#", decorator-expression, "#" ;

class-declaration  ::= decorated-access, [ "abstract" | "final" ], "class", identifier,
                       [ generic-declaration ], [ ":", type-list ], [ where-clauses ], class-body ;
struct-declaration ::= decorated-access, [ "readonly" ], [ "ref" ], "struct", identifier,
                       [ generic-declaration ], [ where-clauses ], struct-body ;
interface-declaration ::= decorated-access, "interface", identifier,
                          [ generic-declaration ], [ ":", type-list ], [ where-clauses ], interface-body ;
enum-declaration   ::= decorated-access, "enum", identifier, [ ":", TypeRef ], enum-body ;
union-declaration  ::= decorated-access, "union", identifier,
                       [ generic-declaration ], [ where-clauses ], union-body ;
```

`decorator` 的实际源代码边界是 `#...#`，而不是 C#/Java 风格的裸 `@name`。在 union
variant 的起始位置，单独的 `@` 有另一个含义：它标记该 variant 是 `using` 的默认
variant，最多只能出现一次。因此不要把 `@` 当作通用 decorator 记法。

`pub`、`pri`、`pro` 由 parser 保存为 access modifier；未写时采用当前声明上下文的默认
访问级别。`abstract`、`final`、`virtual`、`override`、`shadow` 是结构化 modifier，是否可
组合、是否存在可重写目标由 semantic phase 判断。parser 只会在不允许它们的位置给出
立即的 shape diagnostic，例如字段不接受 `abstract/virtual/override/final/shadow`。

## 2. Class

### 2.1 声明头、继承与泛型

```zr
pub class CacheEntry<K, V>: Entry, Inspectable
where K: class, V: new() {
    // members
}
```

class 头的处理顺序是：前置 decorator、访问修饰符、`abstract/final`、`class`、名称、可选
generic declaration、冒号后的逗号分隔 type list、可选 `where`、成员体。冒号后立刻出现
`{` 时，parser 会保留一个空 inheritance list；这不等于 compiler 接受“空基类”作为有意义
的继承关系。

继承列表中的每项按普通 `TypeRef` 解析，因而数组、泛型实例或无效 reference form 在
语法层和 type canonicalization 层会走同一套规则。不要根据显示文本把 `Entry<T>` 和
另一个 module 的同名 `Entry<T>` 当作同一个基类型；module identity 是 canonical identity
的一部分。

### 2.2 成员分类

class body 在当前 token 上预扫描 decorator、access、`static` 和允许的 modifier，再选择
下列 AST 分支：

| 源码形状 | AST 类别 | 解析器立即负责的内容 | 后续阶段负责的内容 |
| --- | --- | --- | --- |
| `var` / `let` 字段 | `ZR_AST_CLASS_FIELD` | 名称、类型、初始化式、分号 | layout、初始化顺序、写入权限 |
| `fn` 方法 | `ZR_AST_CLASS_METHOD` | 参数、返回类型、body、receiver modifier | overload、override、call contract |
| `property` | `ZR_AST_PROPERTY_DECLARATION` | getter/setter block 和访问级别 | getter/setter type、可写性、metadata |
| `@name(...)` | `ZR_AST_CLASS_META_FUNCTION` | meta 名称、参数、可选 `super(...)`、body | `constructor` 等角色和生命周期含义 |
| 旧 split `get` / `set` | legacy migration 节点 | 仅 migration parser 收集 | 生产 parser 直接报 removed syntax |

字段语法如下：

```ebnf
class-field ::= { decorator }, [ access ], [ "static" ], ( "var" | "let" | "const" ),
                member-identifier, [ ":", TypeRef ], [ "=", expression ], ";" ;
```

`let` 是当前不可重新绑定字段的标准拼写。`var const field` 是迁移前形状，production parser
会要求改写为 `let field: Type = value;`。字段级 `using` 也被明确拒绝：资源所属关系必须
编码在字段的类型，例如 `Unique<File>` 或 `Shared<Socket>`，而不是通过一个会在成员声明
结束时失去作用域的 `using` 标记。

### 2.3 方法、receiver 与异步

```ebnf
class-method ::= { decorator }, [ access ], [ "static" ],
                 [ "abstract" | "virtual" | "override" | "final" | "shadow" ],
                 [ "const" ], [ "async" ], "fn", member-identifier,
                 [ generic-declaration ], "(", parameters, ")",
                 [ ":", TypeRef ], [ where-clauses ], block ;
```

`const fn` 在 class method AST 中表示 const receiver modifier，而不是“编译期常量函数”。
它不能与 `static` 合用，因为 static method 没有 receiver。`async fn` 的参数与返回值仍先
按普通函数语法构建，随后由 task semantic rule 验证其 `Task<T>` 返回契约、暂停点和跨
`await` 的借用限制。

下面的代码来自当前项目样例，展示 instance field、property、static property、继承与
meta constructor 的组合：

```zr
class BaseHero {
    pri var _hp: int = 0;

    pub @constructor(seed: int) {
        this._hp = seed;
    }

    pub property hp: int {
        get { return this._hp; }
        set { this._hp = value; }
    }
}

class BossHero: BaseHero {
    pub static var created: int = 0;

    pub @constructor(seed: int) super(seed) {
        BossHero.created = BossHero.created + 1;
    }
}
```

`@constructor` 在 parser 中是 meta function surface，不是普通 decorator。`super(seed)`
的语法被收在该 meta function 节点中；基类构造调用是否存在、次数是否正确、异常或 cleanup
顺序如何安排，必须由 compiler/runtime 的对象构造路径作出结论。

### 2.4 统一 property

```ebnf
property-declaration ::= { decorator }, [ access ], [ "static" ], "property", member-identifier,
                         ":", TypeRef, "{", { property-accessor }, "}" ;
property-accessor    ::= [ access ], ( "get", block | "set", block ) ;
```

```zr
class Meter {
    pri var stored: int = 0;

    pub property value: int {
        pub get { return this.stored; }
        pri set { this.stored = value; }
    }
}
```

`get` 和 `set` 访问级别可分别指定。setter 中的 `value` 是 property 约定的输入位置，
但 getter/setter 的返回规则、访问扩张是否允许、是否能在 interface 上提供实现，都不是
parser 的职责。当前 production parser 采用一个 `property name: Type { ... }` 节点；旧的
分离 `get name` / `set name` 形式仅由 migration frontend 读取并生成迁移诊断，不能作为
新代码的兼容写法。

## 3. Struct 与 `ref struct`

### 3.1 语法表面

```zr
ref struct SyntaxReferenceView {
    var value: int;
}

pub readonly struct SyntaxReferenceLabel {
    var text: string;
}

struct SyntaxReferencePoint {
    var x: int;
    var y: int;

    pub @constructor(x: int, y: int) {
        this.x = x;
        this.y = y;
    }
}
```

struct body 支持字段、方法、meta function 与统一 property，成员的 parser shape 与 class
相近。`ref struct`、`readonly struct` 是 declaration modifiers，分别改变后续 type/layout
与访问约束的输入事实；它们不等于 C 中的某个固定 struct packing，也不允许 native host
根据源码关键词猜测内存地址稳定性。

值形态通常使用 `init TypeRef(...)` 建立构造 AST：

```zr
var point: SyntaxReferencePoint = init SyntaxReferencePoint(1, 2);
var view: Span<int> = init Span<int>();
```

`init` 后必须接 `TypeRef` 与参数括号。named argument、`ref`/`out` marker 和 ordinary call
一样先由 argument parser 保留，是否与 constructor contract 匹配由 call binding 验证。参见
[表达式与构造参考](expression-construction-reference.md)。

### 3.2 resource class 与 ownership

```zr
resource class SyntaxReferenceOwned {
    var value: int;
    pub @constructor(value: int) { this.value = value; }
}

fn makeOwned(): int {
    var owner: Unique<SyntaxReferenceOwned> = own SyntaxReferenceOwned(7);
    var boxed: GcBox<SyntaxReferenceOwned> = intoGc(owner);
    return boxed.read();
}
```

`resource class` 是 class declaration 的资源入口。`own` construction、`Unique<T>` 以及
`intoGc(owner)` 的组合由 ownership flow、resource drop 与 GC bridge 共同检查。语法存在
并不代表任意资源类型都能进入 GC，也不代表 `owner` 在 `intoGc` 后仍能读取；这些都是 move
和 ownership intrinsic 的语义问题，详见 [模式、所有权与资源配方](patterns-ownership-recipes.md)。

## 4. Interface

interface 用于声明 contract surface。其成员可包含字段签名、方法签名、property 签名和
meta signature，但 ordinary interface body 不把 method implementation 当作默认行为。

```ebnf
interface-declaration ::= decorated-access, "interface", identifier,
                          [ generic-declaration ], [ ":", type-list ], [ where-clauses ],
                          "{", { interface-member }, "}" ;
interface-method      ::= { decorator }, [ access ], [ "static" ], [ modifiers ],
                          [ "const" ], [ "async" ], "fn", identifier,
                          [ generic-declaration ], "(", parameters, ")", [ ":", TypeRef ], ";" ;
```

签名末尾的 `;` 很重要：class/struct method 的 `block` 与 interface method 的 contract
signature 不是可互换的 production。interface generic parameter 的 variance、约束和继承
列表仍走 canonical type 规则；是否可实现、是否冲突、dispatch slot 如何形成，应由 compiler
和 native descriptor 合同作出决定。

## 5. Enum 与 Union

### 5.1 Enum

```ebnf
enum-declaration ::= decorated-access, "enum", identifier, [ ":", TypeRef ],
                     "{", { enum-member }, "}" ;
enum-member      ::= { decorator }, identifier, [ "=", expression ], [ "," | ";" ] ;
```

```zr
pub enum Color: int {
    Red = 1,
    Green = 2;
    Blue
}
```

parser 允许成员由逗号、分号或 body close 结束，也允许基础 TypeRef。基础类型的可行性、值
表达式是否可常量求值和重复值策略留给后续阶段。不要以 parser 接受 `Color: string` 为依据
推断所有后端都支持 string-backed enum。

### 5.2 Union variant

```ebnf
union-declaration ::= decorated-access, "union", identifier,
                      [ generic-declaration ], [ where-clauses ],
                      "{", { union-variant }, "}" ;
union-variant     ::= { decorator }, [ "@" ], identifier,
                      [ "(", [ named-type { ",", named-type } ], ")"
                      | "{", [ named-type { ";" | ",", named-type } ], "}" ],
                      [ ";" | "," ] ;
named-type        ::= identifier, ":", TypeRef ;
```

```zr
union Reply<T> {
    None;
    Value(value: T);
    Error { message: string; code: int; }
}
```

tuple variant 和 struct variant 的 field 都使用 `name: Type`，不是 `Type name`。struct
variant field 用 `;` 或 `,` 分隔。若 variant 前写 `@`，parser 设置 `isDefaultUsingVariant`；
同一 union 中第二个 `@` 会产生 `union_default_variant_duplicate`。模式匹配、exhaustiveness、
payload move binding 与 `using` 自动选择规则在 [控制流、模式与清理参考](control-flow-pattern-cleanup-reference.md)
中展开。

## 6. 从源码到 AST、布局和调用

parser 的职责是把每个声明表示成 `SZrAstNode` 并保存 source range、decorator、access、
generic、member/variant array。下一阶段大致按下列顺序消费：

```text
class/struct/interface/enum/union AST
  -> symbol declaration and canonical TypeId
  -> member/property/constructor contract construction
  -> layout, ownership, virtual/interface dispatch and metadata facts
  -> ExecBC or AOT lowering
  -> runtime object/prototype/native provider binding
```

这条链解释两个常见误解：

1. parser 已创建 `ZR_AST_CLASS_DECLARATION` 不表示 class 已经有可执行 layout。
2. native provider descriptor 中出现一个类型，不表示 source class 可以以同名名称替代它；
   resolver、module identity、contract hash 和 provider generation 都必须一致。

## 7. C parser/host 入口

宿主只想检查对象声明的语法时，可使用完整 script parser，随后在 AST 中查找对应节点：

```c
#include <string.h>

#include "zr_vm_parser/parser.h"

static SZrAstNode *parse_object_source(
        SZrState *state,
        const TZrChar *source,
        SZrString *sourceName) {
    SZrParserState parser;
    SZrAstNode *script;

    ZrParser_State_Init(&parser, state, source, strlen(source), sourceName);
    script = ZrParser_ParseWithState(&parser);

    /* Read diagnostics / AST while parser and state still belong to this owner. */
    ZrParser_State_Free(&parser);
    return script;
}
```

调用方在处理完 script 后调用 `ZrParser_Ast_Free(state, script)`。不要在
`ZrParser_State_Free` 或 `SZrState` 已释放后保留 AST、identifier string、成员数组或
source range 中借用的 source 地址；跨线程或跨 revision 使用时应通过 semantic/LSP 的复制
和 snapshot API，而不是复制内部节点指针。

要真正 materialize object、操作字段或注册 native type，请继续阅读 [Core 值/对象 API](../05-interop/core-value-object-api.md)、[Native Module 编写](../05-interop/native-module-authoring.md)
和 [Native Provider Descriptor 深度参考](../03-modules/native-provider-descriptor-reference.md)。
