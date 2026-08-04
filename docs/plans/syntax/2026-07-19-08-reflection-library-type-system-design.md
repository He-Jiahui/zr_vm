# 08 `zr.reflection` 独立反射库与运行时类型系统

> 状态：已补齐直接依赖、分层里程碑与晋级门，等待按里程碑实施。
>
> 语义硬依赖：[Canonical TypeRef/Place/CFG/artifact](./2026-07-18-01-canonical-type-place-cfg-artifact-design.md)、[struct/layout](./2026-07-18-03-struct-ref-struct-span-layout-design.md)、[resource ownership/GC bridge](./2026-07-18-04-resource-ownership-drop-gc-bridge-design.md)、[property](./2026-07-18-05-property-unified-ast-design.md)。
>
> 集成依赖：M4-M5 依赖 10R 的 ModuleIdentity/native descriptor substrate；M1-M3 不等待 10F FFI ABI。

## 1. 目标结果

反射作为第10章OfficialNative domain的N2 Runtime native模块`zr.reflection`自成一体。核心语言只提供取得类型身份和运行时类型的最小intrinsic，字段、属性、方法、metadata、动态构造和查询策略都由native descriptor中的库类型承载：

- `typeid(TypeRef)` 取得轻量、不可反射的静态类型身份。
- `typeof(expr)` 求值一次并取得值的真实运行时类型描述符。
- `zr.reflection.resolve(typeId)` 显式从类型身份进入反射系统。
- `TypeOf<T>` 继承非泛型根 `Type`，并按 declaration category 返回最精确子类。
- `getField/getProperty/getMethod/getMeta` 采用 C# 反射的成员分层，但使用确定的 ZR query contract。
- 动态构造只属于可构造反射类型，不重新进入 `init/new/own/@call` binder。
- source compile、binary import、native registration、VM、AOT、LSP 和 debugger 消费同一 metadata graph。

反射库不得成为 compiler 按 `TypeOf` 名字特判的入口。compiler/runtime 通过 TypeDef capability、metadata token 和注册的 reflection service 接入。

`zr.reflection`及其declaration/member/layout子模块都属于官方`zr.*`保留根，由单一reflection descriptor或其module links提供。下面的类型层级是descriptor生成的interface projection，不是可覆盖的ZR source实现；workspace/package/custom native provider不能声明同名ModuleId。Runtime/AOT按reflection capability id接入，不比较`zr.reflection`字符串。

## 2. 模块边界

第一版标准模块：

```text
zr.reflection
  Type
  TypeOf<T>
  TypeId<T> bridge
  resolve(...)
  MemberQuery
  MetaQuery
  reflection errors

zr.reflection.declaration
  ClassTypeOf<T>
  ConcreteClassTypeOf<T>
  InstanceClassTypeOf<T>
  StructTypeOf<T>
  InterfaceTypeOf<T>
  ResourceClassTypeOf<T>
  RefStructTypeOf<T>
  EnumTypeOf<T>

zr.reflection.member
  MemberInfo
  FieldInfo
  PropertyInfo
  MethodInfo
  ConstructorInfo
  ParameterInfo
  MetaInfo

zr.reflection.layout
  LayoutInfo
  FieldLayoutInfo
  GcMapInfo
  OwnershipInfo
```

使用普通静态 import binding：

```zr
let reflection = import("zr.reflection");
let declaration = import("zr.reflection.declaration");
let members = import("zr.reflection.member");
```

反射对象由 runtime service 创建并缓存。用户代码不能继承、伪造或直接 `new/init` 这些 descriptor 类型。

## 3. 类型层级

### 3.1 根类型

概念公开契约：

```zr
pub abstract class Type {
    pub const property id: TypeId {
        pub get;
    }

    pub const property name: string {
        pub get;
    }

    pub const property qualifiedName: string {
        pub get;
    }

    pub const fn getField(name: string, query: MemberQuery = MemberQuery.default): FieldInfo?;
    pub const fn getFields(query: MemberQuery = MemberQuery.default): readonly FieldInfo[];
    pub const fn getProperty(name: string, query: MemberQuery = MemberQuery.default): PropertyInfo?;
    pub const fn getProperties(query: MemberQuery = MemberQuery.default): readonly PropertyInfo[];
    pub const fn getMethod(
        name: string,
        parameterTypes: readonly TypeId[] = [],
        query: MemberQuery = MemberQuery.default
    ): MethodInfo?;
    pub const fn getMethods(query: MemberQuery = MemberQuery.default): readonly MethodInfo[];
    pub const fn getMeta(name: string, query: MetaQuery = MetaQuery.default): MetaInfo?;
    pub const fn getMetas(query: MetaQuery = MetaQuery.default): readonly MetaInfo[];
}

pub abstract class TypeOf<T>: Type {
    pub const property representedTypeId: TypeId<T> {
        pub get;
    }
}
```

文档中的 bodyless accessor 表示标准库/native contract，不产生 backing field；它不是 concrete user property 的 auto-property 许可。

`Type` 是 erased 查询边界。`TypeOf<T>` 只在 compiler 能证明 descriptor 精确表示 `T` 时暴露；不能用 `TypeOf<Base>` 伪装运行时实际表示 `Derived` 的 descriptor。

### 3.2 declaration 类型

```zr
module zr.reflection.declaration;

let reflection = import("zr.reflection");

pub abstract class ClassTypeOf<T>: reflection.TypeOf<T>
    where T: class;

pub abstract class ConcreteClassTypeOf<T>: ClassTypeOf<T>
    where T: class;

pub abstract class InstanceClassTypeOf<T>: ConcreteClassTypeOf<T>
    where T: class, new;

pub abstract class StructTypeOf<T>: reflection.TypeOf<T>
    where T: struct;

pub abstract class InterfaceTypeOf<T>: reflection.TypeOf<T>
    where T: interface;

pub abstract class ResourceClassTypeOf<T>: reflection.TypeOf<T>
    where T: resource class;

pub abstract class RefStructTypeOf<T>: reflection.TypeOf<T>
    where T: ref struct;
```

分类规则：

- `ClassTypeOf<T>` 包括 abstract class 和尚未证明 concrete 的 class declaration。
- `ConcreteClassTypeOf<T>` 表示非 abstract、closed、至少有一个可见 public constructor 的 class。
- `InstanceClassTypeOf<T>` 进一步满足 C# 风格 `new` 约束，即存在 public 无参 constructor。
- `StructTypeOf<T>` 只表示可装箱的普通 struct/readonly struct；ref struct 使用独立类型。
- `InterfaceTypeOf<T>` 表示 interface 声明，不表示“某个 interface-typed value 的运行时对象”。
- resource class/ref struct 可以查询 metadata/layout，但不能通过普通 reflection construction 创建。

`new` 约束不改写成“任意 public constructor”。带参可构造与 public 无参可构造是两个不同 capability。

### 3.3 可构造能力

反射构造保持显式 object boundary：

```zr
pub interface ConstructibleType {
    pub const fn createInstance(...constructionArgs: object): object;
}
```

`ConcreteClassTypeOf<T>` 和 `StructTypeOf<T>` 实现该 capability；`InstanceClassTypeOf<T>` 继承它。规则：

- concrete class 返回 GC object，公开返回类型仍为 `object`。
- ordinary struct 使用 canonical TypeLayout 构造后显式 box 为 `object`。
- public constructors only，第一版只按位置绑定参数。
- `@call` 不进入候选集合。
- resource class、ref struct、interface、abstract class 和 open generic 拒绝。
- constructor throw 保留原 cause，并标记 reflection invocation boundary。

静态代码需要无装箱、destination-first value construction 时必须使用 `init T(...)`；需要普通 GC class allocation 时使用 `new T(...)`。反射 API 的 object 返回成本不能反向污染静态路径。

erased `Type` 不直接公开 `createInstance`。动态值先进行 capability check：

```zr
let reflection = import("zr.reflection");

let type: reflection.Type = loadRuntimeType();
let constructible = reflection.requireConstructible(type);
let value: object = constructible.createInstance(...constructionArgs);
```

这样 interface/ref-struct descriptor 不会在普通调用路径上携带一个注定失败的方法。

## 4. `typeid`、`typeof` 与 `resolve`

### 4.1 `typeid(TypeRef)`

```zr
let pixelId: TypeId<Pixel> = typeid(Pixel);
let listId: TypeId<List<int>> = typeid(List<int>);
```

`typeid`：

- operand 总在 TypeRef context 解析，不求值 expression。
- 返回 opaque canonical type identity，可用于 equality、map key、dispatch、artifact reference 和 cache key。
- 不提供成员查询或动态构造。
- 单独使用不应 root 字段、方法、property 或 decorator metadata。
- raw integer/hash/address 不构成公共语义；跨 artifact 使用 canonical token/schema identity。

### 4.2 `typeof(expr)`

```zr
let runtimeType = typeof(createRenderer());
```

`typeof`：

- operand 是 expression，必须按源程序求值一次。
- 返回 descriptor 的 runtime identity 恒等于值的真实运行时 TypeId。
- 当 exact-type fact 足够时，静态返回最精确的 `TypeOf<T>` 子类。
- 当值可能是派生类、dynamic 或未收窄 union 时，静态返回 `Type`，但 runtime object 仍是精确 descriptor。
- nullable operand 必须先经过 non-null flow narrowing；不把 null 隐式变成 reflection exception 热路径。

示例：

```zr
let point = init Point(1, 2);
let pointType: declaration.StructTypeOf<Point> = typeof(point);

let service = new FileService();
let serviceType: declaration.ConcreteClassTypeOf<FileService> = typeof(service);

let base: Service = loadService();
let runtimeType: reflection.Type = typeof(base);
```

最后一例不能静态标为 `ClassTypeOf<Service>`：descriptor 可能实际表示 `FileService`，泛型实参必须表示 exact identity，而不是上界。

### 4.3 `resolve(typeid(T))`

没有实例时，通过库显式解析声明 descriptor：

```zr
let reflection = import("zr.reflection");
let declarations = import("zr.reflection.declaration");

let contract: declarations.InterfaceTypeOf<IRenderer> =
    reflection.resolve(typeid(IRenderer));

let point: declarations.StructTypeOf<Point> =
    reflection.resolve(typeid(Point));
```

generic `TypeId<T>` 和 canonical TypeDef category 共同决定结果静态类型；erased `TypeId` 只返回 `Type`。调用 `resolve` 才进入 metadata preservation boundary。

恒等关系：

```zr
typeof(point).id == typeid(Point);
reflection.resolve(typeid(Point)) === typeof(point);
```

同一 runtime generation 内，相同 TypeId 返回同一个 canonical descriptor identity。module reload/metadata generation 变化时 cache 按 generation 失效。

## 5. 成员查询

### 5.1 `MemberQuery`

C# `BindingFlags` 提供了成熟 precedent，但直接复制 bitmask 容易产生非法组合。ZR 第一版使用结构化 query：

```text
MemberQuery
  scope: declared | inherited | all
  access: public | protected | private | all
  storage: instance | static | all
  includeCompilerGenerated: bool = false
  includeMetaMethods: bool = false
```

默认值等价于 public、instance + static、包含合法 inherited public member、不包含 compiler-generated member 和 meta-method。

查询顺序稳定为：目标类型直接声明顺序，然后按 base chain 从近到远；interface 使用 canonical interface linearization。输出顺序进入 golden tests，不依赖 hash table iteration。

### 5.2 singular 与 plural

- `getField/getProperty` 按 name + query 查找唯一成员；无结果返回 null，多个同等候选报告 structured ambiguity。
- `getMethod` 必须结合 parameter TypeId vector、generic arity/query 完成 overload selection；仅名字不足以选中唯一 overload。
- `getFields/getProperties/getMethods` 返回 readonly ordered collection。
- 查询失败不通过空 `MemberInfo` sentinel 表示。

第一版不允许 query 绕过 module/access control。需要 non-public reflection 时调用点必须持有明确 capability；不能只传字符串 `private` 获得权限。

## 6. Field、Property、Method 与 Meta

### 6.1 `FieldInfo`

至少公开：

```text
name, declaringType, fieldType, access, isStatic, isLet, isVar,
offset/layout, gcKind, ownershipKind, source, metadataToken
```

- `let` field reflection 不提供替换写入。
- `var` field 的 reflective set 仍检查 readonly receiver、access、type conversion 和 active loan。
- ordinary field get/set 返回/接收 object 时遵守装箱规则。
- ref-like/native direct access 必须通过专门的 scoped Ref handle，不伪装为 object。

### 6.2 `PropertyInfo`

至少公开：

```text
name, propertyType, access, getter, setter,
returnsRef, returnsRefReadonly, receiverEffect, refExportEffect,
source, metadataToken
```

property 没有编译器自动 backing field。reflection 不推测 `_name` 与 `name` 的关系；显式 field 和 property 是两个独立 member，除非用户 metadata 明确声明关联。

ref-return property 只允许 getter。reflection 的 ref getter 返回受 region/guard 管理的 scoped Ref handle，不装箱为普通 object。

### 6.3 `MethodInfo`

至少公开 callable signature、parameter passing、return TypeRef、receiver effect、generic parameters、access、static/virtual/override、source 和 token。`invoke`：

- 使用普通 argument vector/spread contract。
- 保留 `ref/in/out/scoped`，不能把它们全部抹平成 object[] 后失去写回/逃逸语义。
- 第一版通用 object invocation 只接受 value-only callable；by-ref invocation 需要专用 typed/scoped API。
- method throw 保留 cause，并标记 reflection invocation boundary。

### 6.4 `getMeta`

`getMeta/getMetas` 查询：

- retention为runtime的`AttributeData` record；参数按11的compile-constant schema读取。
- 用户自定义 key/value metadata。
- source location/documentation identity。
- compiler-generated、deprecated等已明确retained的声明标记。test-only/compile-tool标记只在对应artifact和权限下可见。

不通过 `getMeta` 返回：

- layout/offset/align：使用 `layout` API。
- ownership/drop/GC map：使用 `ownership/layout` API。
- declaration transform handler、DeclarationPatch、IR/code block和compile heap：它们是compiler/debug provenance，不是runtime reflection metadata。
- `@call/@constructor` 元方法：使用 `getMethod` 且显式 `includeMetaMethods`。

## 7. Metadata graph 与 artifact

reflection 不维护第二套类型事实。规范来源是 Canonical Type graph + ModuleMetadataGraph：

```text
TypeDef/TypeSpec/TypeId
MemberDef tokens
PropertyDef/accessor tokens
Callable contracts
TypeLayout/GC/ownership maps
AttributeData/Meta records
Source/debug records
Preservation flags
```

`.zri/.zro`：

- 同一 schema writer/reader 对称保存 mandatory public metadata。
- 被裁剪 section 必须有显式 preservation state，不能在 runtime 假装“成员不存在”。
- TypeId、member token、layout hash、contract hash 和 metadata generation 交叉验证。
- unknown mandatory kind、超限 count、断裂 owner/member reference、伪造 category 必须拒绝加载。
- 带DeclarationTransform role的comptime function及其Patch data不会进入runtime metadata graph；reflection只观察expansion后的最终member与retained AttributeData。AttributeData的schema统一来自带AttributeUsage role的普通`readonly struct`，不需要`attribute`关键字或runtime attribute object。

native registration 必须提交同形 metadata descriptor；不能只注册 type name 后由 reflection 猜字段。

## 8. AOT、裁剪与性能

- `typeid(T)` alone 不保留 member tables。
- `typeof(expr).id` 可优化为 runtime TypeId load，不必 materialize完整 wrapper。
- 静态 `getField("x")` 可只 root 已解析 field；动态 name/query 需要显式 broad preservation policy。
- descriptor cache 只缓存 immutable metadata view、binder plan，不缓存实例。
- member lookup cache key 包含 TypeId、query、name/signature 和 metadata generation。
- normal field/property/method call 不经过 reflection binder。
- reflection struct construction 的 box 和 object conversion 必须单独计入 benchmark。

## 9. LSP 与诊断

LSP：

- hover 显示 precise descriptor type，例如 `StructTypeOf<Point>`、`Type`。
- completion 只展示目标 descriptor 静态层级合法的方法。
- `typeid(T)` 不建议 `getField/createInstance`。
- erased `Type.createInstance` 提供 `requireConstructible` code action，不插入 unsafe cast。
- reflection query 可跳转到 Field/Property/Method declaration；source 被裁剪时明确显示 binary metadata。

诊断至少包括：

```text
reflection.null_operand
reflection.metadata_not_preserved
reflection.member_not_found
reflection.member_ambiguous
reflection.access_denied
reflection.type_not_constructible
reflection.constructor_not_found
reflection.constructor_ambiguous
reflection.constructor_threw
reflection.byref_invoke_requires_typed_api
```

## 10. 迁移规则

- `%type(expr)` -> `typeof(expr)`。
- 旧静态类型身份查询 -> `typeid(TypeRef)`。
- 旧 runtime `$proto(args)`：若 target 可绑定为 `Type`，生成 `requireConstructible(target).createInstance(...constructionArgs)` 的 `requiresReview` edit。
- 静态 `$Struct(args)` -> `init Struct(args)`；静态 class/resource 分别进入 `new/own`。
- migration 绝不能把任意 runtime Type expression 改写成 TypeRef，或把 reflection call lower 为 ConstructExpression。
- 旧 `ReflectionTypeInstance` 术语全部迁为 `Type` + `ConstructibleType` capability；不保留平行兼容类。

## 11. 分层里程碑与晋级门

### M1 TypeId 与 descriptor category

目标：建立 `Type`/`TypeOf<T>`、class/struct/interface/resource class/ref struct category 和 `typeid/typeof/resolve` 的单一 canonical identity，不接入成员 binder。

依赖：01、03、04。晋级门：静态 identity、运行时 exact type、nullable/base/interface fallback、resource/ref-struct 分类和错误类别均有 parser/type-inference/runtime 单测；descriptor 由 TypeId/category capability 驱动，不存在类型名分支。

### M2 Member descriptor 与 property/callable contract

目标：接入 FieldInfo、PropertyInfo、MethodInfo、MemberQuery、访问控制、稳定顺序和 value-only/by-ref invocation 边界。

依赖：M1、05，以及 03 已冻结的 receiver/layout contract。晋级门：field let/var、concrete/ref-return property、overload/generic method、declared/inherited/access/query ambiguity 全矩阵通过；ref getter 和 `in/ref/out` 不被 object invocation 擦除。

状态（2026-08-04）：已晋级。公开 callable/parameter descriptor 已保留七种 passing mode；AOT ABI 15 把 ExecIR 的 `value/in/ref/ref readonly/scoped ref/scoped ref readonly/out` 投影到版本化 MethodInfo signature。通用 object invocation 只接受显式 `value`，unknown、损坏值和全部 by-ref mode 在 invoker 调度前失败。field/property/member query 的访问、继承、稳定顺序、overload/ambiguity 与 ref getter 证据由 reflection surface、token resolve 和 module reflection 矩阵覆盖；三编译器聚焦验收见 `tests/acceptance/2026-08-04-syntax-08-m2-reflection-invocation-boundary.md`。M3-M5 不因本状态自动晋级。

### M3 Metadata graph、artifact 与 preservation

目标：让 source、binary 与 runtime descriptor 共用 TypeDef/member/layout/ownership/meta record schema，并建立裁剪/preservation 状态和损坏输入拒绝。

依赖：M2、04 的 ownership/GC map 语义。晋级门：`.zri/.zro` writer/reader 对称，source/native/binary contract hash 一致；unknown mandatory kind、断裂 token、超限表、伪造 category 和 stripped metadata 均有负例。08 只提供 immutable retained metadata contract，11 在其上定义 compile-time attribute/patch，不形成反向依赖。

### M4 Runtime construction、cache、AOT 与 native module

目标：实现 `ConstructibleType.createInstance`、binder cache/generation invalidation、throw cleanup、boxed struct/class result，并通过 10R 注册唯一 `zr.reflection` ModuleIdentity。

依赖：M1-M3、10R。晋级门：direct/spread arguments、constructor no-match/ambiguity/throw、resource/ref struct/interface/abstract/open-generic 拒绝、GC compact、AOT 与 VM 等价全部通过；普通调用/构造不经过 reflection 慢路径。

### M5 LSP、迁移与压力验收

目标：接通 precise hover/completion/navigation、06A 的 `%type`/dynamic construction migration adapter，以及查询/cache/GC 压力测试。

依赖：M1-M4、06A。晋级门：LSP 只消费 canonical query；migration edit 对 machine/requiresReview/blocked 分类稳定且幂等；100k members、深继承、重复查询和 compact/throw 压力无陈旧 pointer 或对象泄漏。

### 整体晋级门

只有 M1-M5 各自通过、下列测试矩阵全部有直接证据、10R native identity 集成完成且不存在 concrete type-name dispatch 时，08 才可供 11、06B 和 07B 作为已晋级依赖。任何缺失层保持对应里程碑 open，不能用 LSP 或 end-to-end smoke 替代。

## 12. 测试矩阵

### Parser/type inference

- `typeof(expr)` 单次求值、nullable narrowing、exact allocation、base/interface/dynamic fallback。
- `typeid` 深泛型/array/union/callable/ref/owner TypeRef。
- `resolve(TypeId<T>)` 对 class/concrete/new/struct/interface/resource/ref struct 的精确结果。

### Member behavior

- declared/inherited、public/protected/private、static/instance、ordered collection。
- field let/var writeability。
- property accessor visibility、ref getter、无 synthetic backing field。
- overloaded/generic method exact/no-match/ambiguous。
- metadata 与 layout/ownership/meta-method query 分离。

### Runtime/artifact

- descriptor canonical identity 和 generation invalidation。
- source/native/binary metadata shape/hash 一致。
- constructor direct args/spread、box、throw cleanup、cache hit/invalidation。
- metadata stripped、corrupt token、oversized table、unknown mandatory kind。

### Boundary/stress

- null、empty query、long names、100k members、深继承/interface graph。
- repeated `typeof/resolve/getMethod/createInstance` cache pressure。
- GC compact 期间 descriptor/member cache 不持有实例或陈旧裸 pointer。
- reflection exceptions 不泄漏 partially constructed struct/class。

## 13. 参考依据与差异

- .NET `Type` 查询 API：`lua/runtime/src/libraries/System.Private.CoreLib/src/System/Type.cs`。
- .NET `MemberInfo/FieldInfo/PropertyInfo/MethodInfo`：`lua/runtime/src/libraries/System.Private.CoreLib/src/System/Reflection/`。
- .NET late-bound construction：`lua/runtime/src/libraries/System.Private.CoreLib/src/System/Activator.cs`。
- .NET reflection tests：`lua/runtime/src/libraries/System.Reflection.TypeExtensions/tests/`。
- JDK declaration/member lookup：`lua/jdk/src/java.base/share/classes/java/lang/Class.java`、`java/lang/reflect/Field.java`、`Method.java`、`Constructor.java`。
- JDK failure/access tests：`lua/jdk/test/jdk/java/lang/reflect/`。
- Rust identity/erasure boundary：`lua/rust/library/core/src/any.rs` 的 `Any/TypeId`。
- CPython dynamic descriptor precedent：`lua/cpython/Objects/typeobject.c`、`descrobject.c`。

ZR 刻意不复制 C# 的全部 BindingFlags/binder/culture API，也不采用 Java deprecated `Class.newInstance()`。共同核心是：类型身份与反射描述符分离，成员类别有独立 descriptor，动态构造位于显式慢边界，类型/layout/ownership metadata 只有一个真源。
