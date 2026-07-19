# 05 property 统一 AST、显式字段与 ref-return property

> 状态：细化草案，等待人工确认。
>
> 硬依赖：[Canonical TypeRef/Place/CFG](./2026-07-18-01-canonical-type-place-cfg-artifact-design.md)、[readonly/borrow checker](./2026-07-18-02-reference-syntax-borrow-checker-design.md)、[receiver effect/TypeLayout](./2026-07-18-03-struct-ref-struct-span-layout-design.md)

## 1. 目标结果

把 class property、interface property、getter、setter、显式存储字段和 ref return 合并为一个语言模型：

- class、struct、resource class 和 interface 使用同一个 `PropertyDecl` AST。
- `get/set/init` 是同一 property 的 accessor，不再是按名称配对的独立成员。
- class field 的 `let/var` 与 local binding 语义一致；property 不声明或合成存储。
- concrete accessor 必须显式代理到预先声明的 field、其他 property 或 method。
- 普通 property access lower 为已解析 accessor call。
- ref-return property lower 为 Ref，再通过 dereference 形成 Place。
- readonly/writable receiver、borrow、escape、layout、artifact 和 reflection 都消费同一 property contract。

## 2. 当前问题

当前 `ast.h` 中：

- class 使用 `SZrClassProperty` 包裹一个 `PropertyGet` 或 `PropertySet` 节点。
- getter/setter 各自保存 name/type/body，需要 compiler 再按名字配对。
- interface 使用独立 `SZrInterfacePropertySignature`，只保存 `hasGet/hasSet`。
- LSP/type prototype 分别读取 getter、setter 或 interface property。
- 显式 field、init accessor、ref-return 和 accessor visibility 没有统一 contract。

这种结构容易产生 getter/setter 类型漂移、重复 member symbol、class/interface 行为差异和 string-based accessor lookup。

## 3. 表层语法

### 3.1 基本形式

```zr
pri var _size: int;
pri var _age: int;

pub property size: int {
    pub get {
        return _size;
    }

    pri set {
        _size = value;
    }
}

pub property age: int {
    pub get {
        return _age;
    }

    pub set {
        require(value >= 0);
        _age = value;
    }
}
```

分号是统一的 simple-declaration terminator：interface/abstract bodyless accessor和expression accessor必须显式写 `;`，换行不具有终止语义。block accessor由自己的 `}` 闭合，不要求尾随 `;`。concrete property 不允许 initializer；初始化必须写入显式 field。

### 3.2 Grammar

```text
PropertyDecl
  := Modifiers? "property" Identifier ":" Type PropertyBody ";"?

PropertyBody
  := "{" AccessorDecl+ "}"

AccessorDecl
  := AccessModifier? ("get" | "set" | "init") AccessorBody

AccessModifier
  := "pri" | "pro" | "pub"

AccessorBody
  := ";"
   | "=>" Expression ";"
   | Block
```

PropertyBody 的 `}` 已经闭合 declaration，尾随 `;` 可省略。Accessor Block 同样由 `}` 闭合，不能依赖 newline 终止内部 statement。`=> ref place;` 仍按 expression grammar 解析为 ref-return expression，不需要 AccessorBody 为它建立独立语法分支。

第一版不加入 indexed property 语法。需要索引的 API 使用 `get(index)`、`set(index, value)` 或已有 index operator；以后若引入 indexer，仍复用 PropertyDecl/AccessorDecl，而不是再创建第四套 AST。

### 3.3 字段与属性分离

```zr
pri var count: int;       // replaceable field
pri let id: Guid;         // initialized once, binding cannot be replaced
pub property size: int {  // property proxy
    pub get {
        return count;
    }
}
```

- `var/let` 只能声明 field/local/binding；field 与 local 采用同一 replaceability 语义。
- `property` 明确声明 property。
- class/struct field 使用 `pri/pro/pub` 控制访问；省略时采用容器定义的默认 member access。
- object literal key 不因名称为 `property/get/set/value` 自动改变语义。
- `property`、`value`、`get/set/init` 尽量使用上下文关键字，减少全局保留标识符；`field` 不再具有 accessor 特殊含义。

## 4. 统一 Syntax AST

建议概念结构：

```text
PropertyDeclSyntax
  decorators
  access
  isStatic
  modifierFlags
  name
  nameRange
  typeSyntax
  accessors[]

PropertyAccessorSyntax
  kind: get | set | init
  accessOverride
  bodyKind: bodylessContract | expression | block
  body/expression
  keywordRange
  fullRange
```

容器节点决定 property 位于 class、struct、resource class 或 interface，不再创建 `ClassProperty` 和 `InterfacePropertySignature` 两种互不兼容结构。

AST 不保存：

- storage field association pointer/id；显式关联属于 metadata，不属于 syntax AST。
- accessor method token。
- resolved property type。
- receiver effect 推导结果。
- override/interface target。

这些是 bound/semantic layer 的职责。

## 5. Bound property contract

```text
PropertySymbol
  propertySymbolId
  declaringTypeId
  name
  valueTypeId
  access
  isStatic
  getterAccessorId?
  setterAccessorId?
  initAccessorId?
  overridePropertyId?
  propertyFlags
```

Accessor 是 callable symbol：

```text
AccessorSymbol
  kind
  access
  receiverEffect
  exportsWritableRef
  return/parameter contract
  body
  sourceRange
```

property member lookup 返回 PropertySymbol，随后按使用上下文选择 getter/setter/ref-getter。禁止通过名称拼接 `get_name`、`set_name` 再做普通 method lookup。

## 6. Accessor 规则

### 6.1 基本合法性

- property 至少有一个 accessor。
- 每种 get/set/init 最多一个。
- `set` 与 `init` 第一版互斥；需要二者时使用 `set`。
- accessor 使用 `pri/pro/pub`；省略时继承 property access。
- accessor access 不能比 property access 更宽，只能相同或收窄。
- interface accessor 必须是 bodyless，除非未来明确支持 default interface implementation。
- abstract property accessor bodyless；concrete property accessor 必须是 expression 或 block，`get;`/`set;` 不生成存储。
- static property accessor receiver effect 为 none。

### 6.2 Receiver effect

默认：

- value getter：readonly receiver。
- `ref readonly T` getter：readonly receiver。
- `ref T` getter：记录独立的`exportsWritableRef` effect。普通class/readonly object capability调用时等价于要求writable receiver，因为结果可修改实例状态。
- `Span<T>`/`PoolRef<T>`这类TypeDef若显式声明为writable-ref view，可以在readonly receiver上导出其`let ref T`字段；readonly只禁止替换view自身字段，不降级它已经持有的referent capability。
- setter：writable receiver。
- init：initializing receiver，只允许构造/初始化阶段。

getter 不允许通过 readonly receiver 写 `var` field。需要 lazy cache 时应使用显式 method，或未来单独设计 mutable/cache field；property 不应在只读调用中隐藏写副作用。

### 6.3 value symbol

在 set/init body 内，`value` 是 compiler-bound 隐式参数：

- type 等于 property value type。
- set 中 initialized/readonly binding。
- init 中同样只读。
- 用户不能在同一 accessor 声明另一个 `value` binding。
- 在 get 或 property 外，`value` 是普通标识符。

## 7. 显式 field 与初始化

### 7.1 `let`/`var` field

```zr
class User {
    pri let _id: Guid;
    pri var _name: string;

    pub property id: Guid {
        pub get {
            return _id;
        }
    }

    pub property name: string {
        pub get {
            return _name;
        }

        pri set {
            _name = value;
        }
    }
}
```

- `let` field 在初始化完成后不能被替换。
- `var` field 可以在 writable receiver 下重新赋值。
- `let` 是浅 binding immutability：若 field 保存 class handle，可以修改目标对象，但不能让 field 指向另一个对象。
- value/struct `let` field 的 subfield write 也是对 field storage 的修改，除非通过字段内部显式 interior-mutability capability，否则拒绝。
- field 访问由 `pri/pro/pub`、receiver readonly capability、Place availability 和 active loans 共同检查。

field 与 local 使用同一 `let/var` 语义，区别只在 storage region、member access 和构造期 definite assignment。compiler 不为 property 创建第二套隐藏可变性规则。

### 7.2 禁止隐式存储

concrete property：

- 不允许 bodyless `get;`/`set;`/`init;`。
- 不允许 property initializer。
- 不定义 contextual `field` expression。
- 不生成 synthetic backing FieldDef、名字、layout slot 或 GC root。
- 每个 accessor body 必须显式访问预先声明 field、另一个 property、method、index storage 或外部代理。

interface/abstract property 的 bodyless accessor 只形成 callable contract，不形成 storage。

### 7.3 构造与 init

```zr
class User {
    pri let _id: Guid;
    pri var _name: string;

    pub @constructor(id: Guid, name: string) {
        _id = id;
        _name = name;
    }

    pub property name: string {
        pub get {
            return _name;
        }

        pri init {
            _name = value;
        }
    }
}
```

- constructor 必须在发布 `this` 前初始化所有 required `let`/`var` fields。
- `init` accessor 只允许初始化阶段调用，但仍只能写显式 field。
- `let` field 可以在所属对象的合法 initialization phase 初始化一次；完成后 init 也不能再次替换它。
- partial construction cleanup 直接使用显式 field initialization bitmap 和 Drop plan。
- property 本身不出现在 TypeLayout；只有显式 fields 参与 size/align/GC/ownership maps。

### 7.4 显式关联 metadata

reflection 默认把 field 和 property 视为独立声明，不根据 `_name`/`name` 猜 backing 关系。若框架需要关联，使用用户 metadata/decorator 显式记录 FieldDef token；该 metadata 不改变 access、layout 或 setter 权限。

## 8. 普通 get/set lowering

### 8.1 Read

```zr
let x = object.size;
```

lower：

1. 计算 receiver 一次。
2. resolve property/getter。
3. 检查 access、receiver effect 和 callable contract。
4. 发出 typed accessor call。
5. 产生 Value。

property read 不是 field load，除非 optimizer 在语义验证后内联 accessor。

### 8.2 Write

```zr
object.size = 10;
```

lower：

1. 计算 receiver 一次。
2. 计算右值一次。
3. resolve setter/init 合法性。
4. 发出 typed accessor call，value 为参数。

### 8.3 Compound assignment

```zr
object.size += delta;
```

语义为单次 receiver evaluation 的 get-modify-set：

1. evaluate receiver/address once。
2. getter call。
3. evaluate delta。
4. checked/operator computation。
5. setter call。

getter 与 setter 之间的 receiver borrow/effects 必须由一个 lowering plan 管理，不能把源码简单复制成两次 `object.size` 导致副作用重复。

若 property 只有 ref T getter，可直接对返回 Place 执行 compound operation，不调用 setter。

## 9. ref-return property

### 9.1 声明

```zr
pub property first: ref readonly Item {
    pub get => ref items[0];
}

pub property current: ref Item {
    pub get {
        return ref currentItem;
    }
}
```

规则：

- property type 为 `ref T` 或 `ref readonly T`。
- getter 必须显式 `=> ref place` 或 `return ref place`。
- ref-return property 不允许 set/init；writable ref 本身已经提供对 referent 的写能力。
- 不存在 `object.current = ref other` 的 property rebind 语义。需要切换 ref-like view 时替换整个 `var ref struct`，或调用显式 `rebind(ref target)` method。
- 第一版不允许 bodyless/auto ref-return property；必须有显式 getter，使来源、guard 和 region 可检查。
- getter 返回表达式必须形成 Place/ref，不允许 materialize rvalue temporary。

### 9.2 使用

```zr
let item = object.first;                   // Load referent value
let view: ref readonly Item = ref object.first;
let writable: ref Item = ref object.current;
```

- value context 自动对 property ref result 执行 Load/Copy/Move 合法性检查。
- `ref propertyAccess` 保留引用身份。
- assignment 到 `object.current` 等价于通过返回的 `ref Item` Store，不调用 setter。
- `ref readonly` property 不能作为 assignment target。
- `object.current = ref other` 非法，不通过 setter 吸收或扩大 incoming ref lifetime。

### 9.3 Region

- 返回 class field ref：ref 绑定到 receiver GC handle 的存活和 managed interior-ref 表示。
- 返回 struct field ref：receiver 必须是 ref/addressable Place，结果不长于 receiver。
- 返回 ref struct/Span element ref：结果不长于 view 的 base region。
- 返回 static field ref：可为 static region。
- getter 返回 local/temporary ref：编译期拒绝。

GC compact 下，class field ref 使用 base handle + offset 或等价可更新表示；AOT 只有在 pin/escape 证明下才能降为裸 pointer。

## 10. Interface、override 与继承

### 10.1 Interface

```zr
interface Sized {
    pub property size: int {
        pub get;
    }
}
```

interface PropertyDecl 与 concrete property 使用同一 AST/PropertySymbol。bodyless accessor 形成 required callable contract。

### 10.2 实现兼容

- property name/static/value type/ref kind 必须匹配。
- implementation 必须提供 interface 要求的每个 accessor。
- implementation 可以额外提供更窄访问 accessor，但不能让 interface 要求不可达。
- getter receiver effect 不得从 readonly 加强为 writable。
- setter/init receiver effect 和 value contract 必须匹配。
- `ref T` 与 `ref readonly T` 第一版按不变类型匹配，不做返回 ref capability 协变。

### 10.3 Override

- override 绑定 PropertySymbol，不分别按 getter/setter 名字搜索。
- accessor virtual slot 分别存在，但共享 property identity。
- derived property 不能只隐藏一半 accessor 而让另一半意外继承；override 声明必须明确完整 property contract。
- shadow 与 override 诊断基于 PropertySymbol/token。

## 11. Semantic IR 与 Place

普通 property：

```text
PropertyGet(propertyId, receiver) -> value
PropertySet(propertyId, receiver, value)
PropertyInit(propertyId, receiver, value)
```

这些可以直接规范化为 resolved callable symbol call；保留 propertyId 有助于 source/debug/reflection。

ref-return property：

```text
PropertyRefGet(propertyId, receiver) -> refValue
Deref(refValue) -> place
```

accessor 内显式 field access 使用普通 field Place，不新增 runtime property storage opcode。

## 12. Artifact 与 reflection

新增/规范 `PropertyDef` metadata：

```text
property token
owner TypeDef token
name string id
property signature token
flags: static/abstract/virtual/override/ref-return
getter/setter/init method tokens
source/debug ranges
```

property signature 包含：

- value/ref TypeRef。
- getter/setter/init contract。
- receiver effects。
- writable-ref export effect/view capability。
- access visibility。

reflection：

- 能查询 accessors、可见性、ref kind、receiver effect 和显式 user metadata；不报告 auto/backing field，因为语言不生成它们。
- 普通 set/get 通过 accessor 调用；optimizer 只有在语义验证后才能内联为显式 field access。
- ref getter reflection 调用返回受管理 Ref handle，不装箱为普通 value。
- minimal metadata stripping 不能删除仍被 dynamic/reflection/property dispatch 使用的 accessor；显式 field 按自身 reflection/layout root 规则保留。

## 13. LSP 行为

- completion 把 property 显示为一个成员，不重复 getter/setter。
- hover 展示 canonical form、accessors、ref kind、receiver effect。
- signature/definition 跳到 property name；可提供“转到 getter/setter”附加目标。
- semantic tokens 对 `value` 使用 contextual parameter 分类；普通 `field` 只按名字解析结果分类。
- code action 支持生成缺失 interface accessor、生成显式 `pri let/var _name` 和代理 accessor，但不会创建隐藏 field。
- rename property 只更新 property uses；显式 field 拥有独立 SymbolId，除非用户选择关联 rename action。

LSP 必须通过 PropertySymbol/SemanticQuery 工作，不重新扫描 getter/setter AST 名称。

## 14. 里程碑

### M1 Unified AST/Symbol

class/struct/resource/interface property 全部进入 PropertyDecl/PropertySymbol。

晋级门：旧 getter/setter 配对路径停止作为语义来源；parser recovery、duplicate accessor 和 type mismatch 测试完整。

### M2 显式 field/init

member `let/var`、accessor visibility、显式 field proxy、init phase、layout/token 完整。

晋级门：构造路径、partial construction、reflection、artifact roundtrip 完整。

### M3 Access lowering/receiver effect

get/set/init、compound assignment、virtual/interface/static 全部 typed lowering。

晋级门：receiver 单次求值、副作用顺序、readonly/writable 调用矩阵完整；VM/AOT 一致。

### M4 Ref-return

ref/ref readonly getter、Place lowering、region、managed interior ref 完整。

晋级门：struct/class/ref struct/static/native 边界和所有逃逸负例完整。

### M5 LSP/reflection/migration

hover/completion/rename/code action、PropertyDef reflection 和旧属性迁移完整。

晋级门：source/binary/LSP/reflection 对 property identity 和 contract 输出一致。

## 15. 测试矩阵

### Syntax/AST

- get/set/init 组合、重复/缺失 accessor。
- bodyless contract/expression/block body，concrete bodyless negative。
- visibility/static/virtual/override/abstract。
- malformed body、半输入、contextual `value` 和普通标识符 `field`。
- class/struct/resource/interface 同构 AST。

### Semantics

- getter/setter 类型一致。
- accessor visibility narrowing。
- member `let` 初始化后不可替换、`var` 可替换、class handle 的浅 binding immutability。
- readonly getter/writable setter/ref getter receiver effect。
- constructor/init phase 与 definite assignment。
- override/interface contract。
- generic property、nullable/owner/ref-like value type。

### Lowering/runtime

- get/set/init、compound assignment 单次求值。
- 显式 field layout/copy/drop/GC map，property 无 storage slot。
- virtual/interface dispatch。
- accessor throw 与 cleanup。
- optimizer inline 前后行为一致。

### Ref return

- field/array/Span/static ref。
- local/temp escape 拒绝。
- readonly/write conflict。
- GC compact/interior ref。
- owner borrow 活跃期 move/drop conflict。
- ref property声明 set/init、`property = ref other` 和 guard/source不匹配全部拒绝。

### Artifact/LSP

- PropertyDef/MethodDef token 关联；FieldDef 独立存在，只有显式 user metadata 才记录关联。
- source/binary reflection 一致。
- metadata strip roots。
- hover/completion/rename/definition/code action。

### Stress

- 大量 properties/accessors 的 symbol/token/layout 构建。
- 深继承 override/interface property lookup。
- 高频 property access inline/devirtualization。
- LSP 大文件增量修改 property body/contract。

## 16. 参考依据

- C# field-backed property反例/对照：`lua/csharplang/proposals/csharp-14.0/field-keyword.md`；ZR 刻意不采用隐式 `field` storage。
- C# ref return/ref field：`lua/csharplang/proposals/csharp-11.0/low-level-struct-improvements.md`。
- Roslyn property/ref tests：`lua/roslyn/src/Compilers/CSharp/Test/Semantic/Semantics/RefLocalsAndReturnsTests.cs`、`RefFieldTests.cs` 及 property 相关 compiler tests。
- CPython descriptor/property separation：`lua/cpython/Objects/descrobject.c`、`lua/cpython/Objects/typeobject.c`。
- Rust Place/ref return/borrow 边界：`lua/rust/compiler/rustc_middle/src/mir/syntax.rs`、`lua/rust/tests/ui/borrowck`。
- ZR 当前 property split：`zr_vm_parser/include/zr_vm_parser/ast.h`、`parser_class.c`、`parser_interface.c`、`compiler_class.c`、`semantic_type_prototypes.c`。

ZR 刻意采用显式 `property` 关键字、显式 `pri/pro/pub let|var` field 和统一 AST；不复制 C# auto/field-backed storage，也不复制 Python 的完全动态 descriptor replacement。property 在 shared compiler path 中不能通过 getter/setter 命名约定或 synthetic field 特判实现。
