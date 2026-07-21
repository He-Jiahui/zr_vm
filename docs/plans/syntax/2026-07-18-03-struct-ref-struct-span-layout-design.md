# 03 struct/ref struct、receiver effect、Span 与 layout

> 状态：细化草案，等待人工确认。
>
> 硬依赖：[Canonical TypeRef/Place/CFG](./2026-07-18-01-canonical-type-place-cfg-artifact-design.md)、[引用语法与 borrow checker](./2026-07-18-02-reference-syntax-borrow-checker-design.md)

## 1. 目标结果

建立可预测的值类型和连续内存基础：

- `struct` 是真正内联布局，不再依赖 object wrapper 承载普通执行语义。
- `readonly struct` 提供构造后只读字段与 readonly receiver。
- `ref struct` 可以安全包含 ref，并通过静态逃逸规则保持 stack/ref-like 限制。
- `const fn` 与普通 `fn` 对 struct/class/interface 使用统一 receiver effect。
- `Span<T>` 和 `ReadOnlySpan<T>` 成为普通 ref-like 标准库类型，而不是 compiler 按名字识别的特殊容器。
- TypeLayout 成为 VM、AOT、GC、FFI、artifact 和 reflection 的唯一布局来源。

## 2. 类型类别

```zr
struct Point { ... }
readonly struct Vec2 { ... }
ref struct Enumerator<T> { ... }
readonly ref struct Span<T> { ... }
readonly ref struct ReadOnlySpan<T> { ... }
class Node { ... }
resource class Texture { ... }
```

| 类别 | 默认存储 | 可否包含 ref | 可否进入 GC heap | 生命周期 |
|---|---|---:|---:|---|
| `struct` | inline | 否 | 可作为字段/数组元素 | 跟随容器 |
| `readonly struct` | inline | 否 | 可 | 跟随容器 |
| `ref struct` | stack/ref-like storage | 是 | 否 | 受 region 限制 |
| `readonly ref struct` | stack/ref-like storage | 是 | 否 | 受 region 限制 |
| `class` | GC heap | 不直接包含语言 ref | 是 | GC |
| `resource class` | ownership heap | 通过允许的 owner/bridge | 否 | Unique/Shared |

## 3. struct 语义

### 3.1 声明与字段

```zr
struct Point {
    var x: float;
    var y: float;

    const fn lengthSquared(): float {
        return x * x + y * y;
    }

    fn translate(dx: float, dy: float): void {
        x += dx;
        y += dy;
    }
}
```

- struct value 直接包含字段存储。
- `var` field 可在 writable receiver 上修改。
- `let` field 在构造完成后不能重新赋值。
- struct 局部、字段、参数和返回值均使用同一 TypeLayout。
- writable method 调用要求 addressable writable Place；不能在纯 rvalue 上调用后丢弃修改。
- const method 可在 Place、value、`in` 或 `ref readonly` receiver 上调用。

### 3.2 显式值构造

规范表层只接受：

```zr
let point = init Point(1, 2);
let pair = init Pair<int>(left, right);
let span = init Span<int>(ref values[0], values.length);
```

```text
StructInitExpression
  := "init" TypeRef "(" ArgumentList? ")"
```

`init` 是上下文关键字。parser 在 `init` 后进入 TypeRef parser，因此 target 可以是 qualified type、constructed generic、type alias，以及具有显式 value-construction constraint 的泛型参数；target 不能是 ordinary value expression、运行时 Type object、prototype 变量、call/index/conditional result 或加括号规避后的 expression。

语义边界固定为：

```text
init S(args)  -> @constructor only
S(args)       -> ordinary call / @call only
new C(args)   -> GC class construction only
own R(args)   -> resource construction only
```

四条路径之间不能 fallback。即使某个 struct Type object 同时提供 `@call`，`init S(...)` 也只绑定 constructor；`S(...)` 也不能因为 callable 绑定失败而尝试 constructor。`init` target 的合法性由 TypeDef 的 `valueConstructible` capability/constructor set 判定，shared binder/runtime 不比较具体类型名字。

argument list 使用统一 callable argument contract 和 overload resolution，但结果写入初始化中的 receiver Place。命名 argument 对应 constructor parameter，不是 aggregate field assignment。`init S()` 必须选择可访问的零参数 constructor 或规范允许的 synthesized default constructor；它不能在显式 constructor 存在时静默 raw-zero。第一版不引入 `S { field: value }` 或 `init S { ... }` aggregate literal。

分层节点：

```text
Syntax AST
  StructInitExpression(typeSyntax, arguments, sourceRange)

Bound expression
  BoundValueConstruct(typeId, constructorId, boundArguments, resultTypeId)

Semantic IR
  ValueConstruct(destinationPlaceId, typeId, constructorId, arguments)
```

现有 `SZrConstructExpression` 以 `isNew`/`isUsing` 布尔值混合 `$`、`new` 和 ownership 的形态必须拆分；不能只把 token `$` 替换成 `init` 后继续复用含糊节点。`Call`、`ValueConstruct`、`GcNew`、`OwnConstruct` 必须是可独立查询的 bound/SemIR kind。

lowering 采用 destination-first：

- local 初始化直接使用 LocalPlace。
- field/array element 初始化直接使用投影 Place，并先验证可写、未初始化和 overlap/borrow facts。
- return/argument temporary 由 lowering 创建明确 temporary Place；大值可使用 hidden return destination。
- constructor 内按字段更新 definite-initialization bitmap；成功后一次 commit 整个 Place。
- 抛错只清理已经初始化的字段，并按声明逆序执行 drop glue。
- inline struct 构造不分配 GC object、不创建 prototype wrapper、不执行运行时 prototype-kind 测试。
- ref struct 构造复用同一 ValueConstruct，随后由 region checker 验证其字段来源和结果 escape。

旧 `$Type(...)` 仅在 target 可静态绑定为 TypeRef 时迁移为 `init Type(...)`。旧 `$proto(...)` 的 target 若是运行时 `zr.reflection.Type`，必须标记为 migration `requiresReview` 并迁为 `reflection.requireConstructible(type).createInstance(...constructionArgs)`；其第一版签名为 `const fn createInstance(...constructionArgs: object): object`，调用端把 `object[]` 用 `...constructionArgs` 展开。只有 target 无法绑定为 Type/ConstructibleType contract 时才标记 `blocked`。该调用不得生成 `StructInitExpression` 或重新进入静态 constructor lowering。

### 3.3 不支持具体 struct 继承

推荐取消具体 struct 继承，只允许：

- struct 实现一个或多个 interface。
- struct 通过字段组合复用布局和行为。
- 泛型约束按 capability/interface 约束。

原因：具体值类型继承会引入 base slicing、动态大小、字段偏移前缀、copy/drop 分派和数组元素大小不稳定，直接破坏“编译期精确布局”和连续数组。C# struct 与 Rust struct 都不采用具体值继承。

现有 `.codex/plans/ZR 基础语言与运行时重建总计划.md` 中的单一 struct 继承方向应由本设计替代；class 的单继承不受影响。

### 3.4 Copy、move 与 Drop

TypeDef 具有结构 capability：

```text
CopyKind: bitwise | fieldwise | moveOnly
DropKind: none | fieldwise | customThenFields
```

规则：

- instance field的`let/var`与local binding一致：`let`完成初始化后不可替换，`var`可在writable receiver下替换；二者都占用显式layout slot。
- `let`是浅binding immutability；class handle target可通过其他writable capability修改，但field自身不能指向新handle。
- 所有字段 bitwise-copy 且无自定义 drop 时，struct 可 bitwise copy。
- 字段需要 retain/copy glue 时使用 fieldwise copy。
- 包含 Unique、move-only field 或自定义 drop 的 struct 默认 move-only。
- readonly 不蕴含 Copy；copy/move 与可变性是独立维度。
- move-only struct 按值赋值、传参、返回会 move，源 Place 失效。
- field drop 按声明逆序执行。
- 部分构造失败只 drop 已完成初始化的字段。

第一版不允许对带 custom drop 或 owner field 的 struct 做任意 partial move；可以完整 move，或只借用字段。以后若开放 partial move，必须由 Place initialization/drop bitmap 支撑。

## 4. readonly struct

```zr
readonly struct Vec2 {
    let x: float;
    let y: float;

    fn length(): float {
        return sqrt(x * x + y * y);
    }
}
```

规则：

- 所有 instance field 在构造结束前 definite initialized。
- 构造结束后 instance field 不可写。
- readonly struct 内的普通 `fn` receiver 自动规范化为 readonly；显式 `const fn` 允许但属于冗余拼写，formatter 可统一省略。
- readonly struct 中不能声明真正 writable receiver method。
- 对 `in/ref readonly` readonly struct 调用方法不生成 defensive copy。
- 若字段本身是 class/owner handle，readonly 只限制字段替换和通过该 capability 的 writable member；不宣称深度冻结对象图。

## 5. receiver effect

### 5.1 规范 effect

```text
ReceiverEffect
  none
  readonly
  writable

RefExportEffect
  none
  readonlyRef
  writableRef
```

- static/free function：none。
- normal class/struct instance `fn`：writable。
- `const fn`：readonly。
- readonly struct 的 instance method：readonly。
- property getter：readonly。
- property setter/init：writable/initializing。
- writable `ref T` getter另记`writableRef` export；普通readonly object capability不能调用，但TypeDef声明为writable-ref view的readonly ref struct可以导出其既有`let ref T` capability。

receiver/ref-export effect进入callable contract、override/interface校验、method group、reflection、LSP hover和artifact hash。

### 5.2 调用与转换

- writable receiver 可以调用 readonly 或 writable method。
- readonly receiver 只能调用 readonly method。
- method override 不得要求比 base/interface 更强的 receiver capability。
- 未绑定 method reference 必须保留 hidden receiver contract。
- 泛型 `T` 上的方法调用依据约束中的 receiver effect，不依据具体实例类型名。
- dynamic call 若无法证明 readonly 安全，只能调用标记为 readonly 的候选；不能延迟到运行时绕过 capability。

## 6. ref struct

### 6.1 允许的字段

ref struct 可以包含：

- 普通 value/struct。
- `ref T`、`ref readonly T`。
- 其他 ref struct，前提是 region 不被扩大。
- GC handle；其 stack/frame layout 必须包含精确 GC map。
- owner value；其 layout 必须包含 drop/ownership map。

允许 GC/owner 字段并不改变 ref struct 自身的 ref-like 存储限制。Span 等非拥有视图应由 API 设计保证不包含 owner。

### 6.2 禁止的存储和转换

ref struct 不得：

- 作为普通 class/resource class field。
- 作为普通 GC array element。
- 装箱为 object/interface 或 dynamic。
- 存入 module/global/static storage。
- 被逃逸 closure 捕获。
- 跨越 await/yield suspension point。
- 通过 reflection 创建 heap box。
- 通过 native ABI 作为未声明布局的 opaque object 保存。

允许：

- local 和 temporary。
- scoped/value parameter。
- safe-context 允许的返回值。
- 其他 ref struct 的 field。
- compiler 已证明生命周期安全的 inline call/aggregate。

### 6.3 泛型边界

第一阶段采用保守规则：

- ref struct 可以自身带普通类型参数，例如 `Span<T>`。
- ref struct 不能作为无约束泛型参数 `T` 的实参。
- 标准泛型容器不能存储 ref struct。
- 不在第一阶段引入 C# `allows ref struct`/anti-constraint 等放宽语法。

后续若需要通用算法接收任意 ref-like 类型，应先设计独立 capability constraint 和“泛型实现不会装箱/捕获/heap store”的完整验证，不能由具体 `Span` 白名单绕过。

### 6.4 返回与移动

- 移动 ref struct value 不移动其 referent，只转移视图/聚合值。
- move 后源是否失效由 CopyKind 决定；含 owner 的 ref struct 通常 move-only。
- 返回 ref struct 时，结果 escape 上界是全部内部 ref field 的最严格共同上界。
- 默认/空 ref struct 可以存在，但不得在无有效来源时解引用其 ref field。
- generational weak handle若只包含PoolId/slot/generation等scalar identity，应是ordinary readonly struct而非ref struct；验证后包含direct ref + guard的view才是ref struct。
- guarded ref struct可以move但通常因guard/drop field而不可copy；guard活跃期referent storage不能被物理drop/reuse。

## 7. TypeLayout

### 7.1 唯一布局来源

TypeLayout 必须统一服务：

- VM frame/aggregate 存储。
- AOT C/LLVM 类型生成。
- struct copy/move/drop glue。
- GC stack/object pointer map。
- ownership field teardown。
- FFI ABI 验证。
- reflection size/alignment/offset。
- artifact layout hash/version。

禁止 parser、AOT writer、GC 和 reflection 分别重新计算字段偏移。

### 7.2 布局类别

现有 value/struct/union 建议扩展或通过 flags 表达：

```text
scalar
struct
union
refStruct
classReference
resourceReference
arrayReference
```

Layout 至少包含：

```text
byteSize
byteAlign
field list(typeId, offset, size, flags)
copyKind
dropKind
gc reference map
gc scan kind: free | mapped | barriered
ownership field map
ref field map
blittable
layoutVersion
layoutHash
```

### 7.3 顺序与 padding

推荐默认：

- struct 字段按声明顺序布局。
- 每个字段 offset 向上对齐到字段 alignment。
- struct alignment 为字段最大 alignment。
- struct size 向上补齐到 struct alignment。
- empty struct size 为 1、alignment 为 1，避免零大小地址/数组 stride 特例。
- size/offset 运算使用 checked arithmetic；溢出或超过实现上限时编译期报错。
- generic instance 在类型参数全部确定后生成独立 layout。

第一版不允许 optimizer 重排字段。以后若增加 `layout(auto)`，必须显式改变 layout hash，且不能用于 native/public stable layout。

### 7.4 稳定性

- 同一 compiler target、schema 和 TypeDef layout version 下，布局必须确定。
- 跨模块通过 layout hash/version 验证。
- 跨编译器版本/native ABI 稳定需要显式 extern/stable layout contract。
- 普通 struct 不承诺跨平台相同 padding。
- AOT C struct 只是目标布局载体，C compiler 的自然布局不能反过来决定 ZR layout；必要时生成 padding/static assertions。

### 7.5 Ref layout

语言 ref 不应默认序列化为裸 native pointer。运行时可采用：

- stack/local ref：frame base + offset 或受 VM 管理的 address descriptor。
- GC interior ref：GC handle/base object + field/element offset，使 compact 后可更新。
- native pinned ref：显式 pinned handle + pointer。

具体表示可按 backend 优化，但 TypeRef/escape 语义不可改变。AOT 只有在证明 base 不移动且 region 有效时才能降为裸 pointer。

## 8. Span 与 ReadOnlySpan

### 8.1 规范形态

```zr
readonly ref struct Span<T> {
    let data: ref T;
    let length: usize;
}

readonly ref struct ReadOnlySpan<T> {
    let data: ref readonly T;
    let length: usize;
}
```

Span struct 自身 readonly，表示 `data/length` 不可被替换；`Span<T>.data` 仍是 writable ref，因此索引结果可以写。

### 8.2 核心 API 契约

```text
Span<T>.length() -> usize
Span<T>.get(index) -> ref T
Span<T>.slice(start, length) -> Span<T>
Span<T>.readonly() -> ReadOnlySpan<T>

ReadOnlySpan<T>.length() -> usize
ReadOnlySpan<T>.get(index) -> ref readonly T
ReadOnlySpan<T>.slice(start, length) -> ReadOnlySpan<T>
```

实际表层可以通过 indexer/property/operator 暴露；语义必须降低为相同 ref return 和 bounds contract。

### 8.3 来源与生命周期

Span 可以来自：

- `T[]` 的 `.span()`。
- stack/inline buffer 的安全范围。
- owner-backed Buffer/PoolLease 的 borrow。
- native pinned memory 的显式 handle。
- 另一个 Span 的 slice/reborrow。

Span escape 不得宽于来源：

- array Span 需要 base GC handle 保活。
- owner Span 活跃时阻止 owner move/drop/reuse。
- stack buffer Span 不能返回到 caller。
- native Span 不能活过 pin/handle。

### 8.4 空值与边界

- default Span 合法，length 为 0。
- 空 Span 的 data ref 可以是不可解引用 sentinel；只要 length 为 0，不允许执行 load/store。
- index 必须满足 `0 <= index < length`。
- slice 必须满足 `start <= length` 且 `sliceLength <= length - start`，使用防溢出形式检查。
- one-past-end ref 不作为普通安全 ref 暴露。

### 8.5 转换与 overload

- array/Buffer 到 Span 第一版使用显式 `.span()`，不加入大范围隐式转换。
- `Span<T>` 到 `ReadOnlySpan<T>` 可以作为唯一的 capability weakening 转换；实现应通过 TypeDef 的 `readonlyViewOf` 元数据表达，不比较类型名。
- overload resolution 中 exact type 优先于 readonly-view conversion。
- ReadOnlySpan 不能转回 Span，除非来源处仍持有独立 writable capability；不提供 cast 强化。

### 8.6 边界检查消除

Semantic IR 为 Span 保留：

- length ValueId。
- index/range facts。
- slice 与 parent 的长度关系。
- base/region identity。

optimizer 只有在 CFG 支配条件和 numeric facts 证明安全时删除 bounds check。未证明时 VM/AOT 都必须保留同一错误语义。

## 9. `zr.pooling` 与 Buffer 协作

```zr
var lease = pool.rent<int>(1024);
var cells: Span<int> = lease.span();
process(cells);
```

- rent 返回 owner-backed lease。
- `span()` 对 lease 建立 borrow。
- active Span 期间 lease 不能 move、drop 或归还池。
- lease drop 将 buffer 返回池。
- pool size class/arena/thread-local 策略不进入 Span TypeId 和语言语法。
- 需要跨 await 的缓冲区必须 move owner 进入 frame，并在 await 前结束 Span borrow；Span 自身不能跨 await。

实体pool采用二阶段handle：

```zr
let handle: PoolHandle<Particle> = pool.deliver(init Particle(...));

var particle: PoolRef<Particle>;
if (pool.tryBorrow(handle, out particle)) {
    particle.value.position.x += 1.0;
}

pool.recycle(handle);
```

- `PoolHandle<T>`是ordinary readonly struct，保存PoolId + slotIndex + generation且不含GC/ref字段。
- `PoolRef<T>`/`PoolReadRef<T>`是readonly ref struct，保存direct ref + borrow guard。
- `tryBorrow/tryRead(handle, out view): bool`验证一次identity并acquire guard；成功后的field/index/property ref access不重复检查generation。
- 第一版不用`Option<PoolRef<T>>`，因为ref struct不能作为普通generic argument；Try/out失败时初始化default不可解引用view。
- recycle立即使handle失效并将slot标为Retired；active guard归零后才Drop/reuse。
- ref property是getter-only；切换实体时替换整个`var PoolRef`，不通过ref setter重绑裸ref。
- active slab不因扩容移动；moving GC使用base handle + slot offset，native/owner slab使用稳定allocation/pin contract。

## 10. Artifact 与 ABI

### 10.1 TypeDef/Layout

artifact 保存：

- readonly/ref-like/value flags。
- value-constructible capability 与公开 constructor contract/token。
- field TypeId、offset、flags。
- copy/drop kind。
- GC scan kind、GC/ownership/ref maps。
- receiver effect。
- layout version/hash。

### 10.2 Public ref-like signature

跨 ZR module 的 Span/ref struct 参数和返回值允许，但 caller/provider 必须验证：

- TypeRef signature hash。
- layout hash/version。
- ref-like flag。
- callable escape contract。
- target ABI lowering kind。

native FFI 不自动采用 ZR ref struct ABI。需要显式 extern layout/marshaller contract。

### 10.3 常量与反射

- ref struct 不进入普通 heap constant pool。
- reflection 可以查询 ref-like、layout 和字段，但不能装箱实例。
- reflection 可以查询 constructor metadata；runtime `zr.reflection.Type` 必须先取得 `ConstructibleType` capability才能调用`createInstance(...constructionArgs)`，不能伪装成 `init TypeRef(...)`。第一版反射构造不支持 ref struct 或 resource class；普通 struct 结果必须装箱为 `object`。
- debugger 从活跃 frame/layout 读取 ref struct，不创建长期保存的 object wrapper。

## 11. 里程碑

### M1 Struct layout、copy 分类与通用 map 表示

覆盖 `init TypeRef(...)` 的 syntax/binding/ValueConstruct、primitive、nested struct、union、array element、generic instance，以及供后续消费者填充的通用 field-slot/map 表示。M1 只定义 field kind、offset、TypeId、capability id 和可为空的 GC/ownership/ref map entry，不要求 resource/owner 的 drop、release、retain 或 GC bridge 语义已经成立。

晋级门：VM/AOT/layout registry/artifact 对 size/align/offset/hash 和通用 map 编解码一致；`init` 直接写 destination Place；无 object wrapper 参与普通 struct construction/field access；普通 call 与 constructor 无 fallback。owner/resource map 的语义填充、field teardown 和异常清理属于 04 的 promotion gate；08 的 reflection projection 只消费已冻结布局，不反向成为 M1 的前置验收条件。

状态记录：[M1 Struct layout、copy 分类与通用 map 表示](./03-struct-ref-struct-span-layout/m1-struct-layout-copy-maps.md)。

### M2 Receiver effect

覆盖 class/struct/readonly struct/interface/override/method reference/property accessor。

晋级门：每种 receiver capability 调用矩阵和 artifact roundtrip 完整；readonly 调用不产生 defensive copy。

状态：已完成（2026-07-21 03:12 +08:00）。class/struct/readonly struct/interface/override/
method reference/property accessor 共用 canonical receiver effect；readonly inline receiver 通过
borrowed frame alias 执行，VM/AOT 栈迁移、artifact patch 35 roundtrip/validation 与三工具链
focused matrix 均已通过。

状态记录：[M2 Receiver effect](./03-struct-ref-struct-span-layout/m2-receiver-effect.md)。

### M3 ref struct restrictions

覆盖 local、parameter、return、field、array、box、closure、async、native。

晋级门：全部非法 storage/escape 路径有静态诊断；GC frame map 正确扫描合法 GC fields。

状态：已完成（2026-07-21 05:05 +08:00）。`ref struct` / `readonly ref struct` 已进入
AST 与 canonical capability；storage/boxing/native 限制和 reference escape 共同覆盖
field、array、global、generic、closure、await/yield 与返回来源。合法 GC/owner/ref field
分别进入精确 frame map，并在 GCC、Clang、MSVC 的 9 目标、253 项 focused matrix 中通过。

状态记录：[M3 ref struct restrictions](./03-struct-ref-struct-span-layout/m3-ref-struct-restrictions.md)。

### M4 Span core

覆盖 array/owner/native 来源、index、slice、readonly conversion、default/empty。

晋级门：生命周期、bounds、VM/AOT 等价和 check elimination 基准全部通过。

状态：已完成（2026-07-21 10:19 +08:00）。`Span<T>` / `ReadOnlySpan<T>` 已作为
protocol/role 驱动的普通 ref-like TypeDef 进入 array view、index、slice、readonly weakening
和 default/empty 路径；structured view/bounds/source-loan facts、moving-GC array source、
owner/native lifecycle conflict、check elimination 与 strict AOT shared-library 均通过
GCC、Clang、MSVC 的 9 目标、每套 322 项矩阵。

状态记录：[M4 Span core](./03-struct-ref-struct-span-layout/m4-span-core.md)。

### M5 Buffer/pool/FFI integration

覆盖 PoolLease borrow、异常清理、pin/unpin、跨模块 ABI。

晋级门：GC/池压力下无 use-after-return、double-return 或 stale pointer；native 地址稳定契约通过。

状态：已完成（2026-07-21 13:30 +08:00）。`zr.pooling` 已提供结构化
`BufferPool` / `PoolLease<T>` single-return 与 generation/reuse 合同；真实 owner/native
provider 共用 M4 的 Place/loan/view facts，异常展开、moving GC、pin/unpin 和动态调用物理
frame window 均已闭环。公开 ref-like ABI 由 canonical consumer 精确校验 TypeRef hash、
type flags、layout version/hash、callable escape flags 和 lowering kind，并拒绝 native direct。

状态记录：[M5 Buffer/pool/FFI integration](./03-struct-ref-struct-span-layout/m5-buffer-pool-ffi.md)。

## 12. 测试矩阵

### Construction

- `init` 的 zero/one/many/named arguments、qualified/generic/alias TypeRef 与深层嵌套 TypeRef。
- `init` 作为 contextual keyword；`fn init`、`value.init()`、`init()` 不误解析。
- struct 同时具有 `@constructor`/`@call`，分别验证 `init S()` 与 `S()` 绑定唯一。
- type/value shadowing、import alias、泛型 value-construction constraint 和 inaccessible/ambiguous constructor。
- 拒绝 `init runtimeProto(...)`、`init (expr)(...)`、`init Class(...)`、`new Struct(...)` 和 `own Struct(...)`。
- local/field/array/return destination 原地构造；constructor throw 的 partial cleanup/drop order。
- VM/AOT/artifact roundtrip 保持同一 TypeId/ConstructorId/ValueConstruct，热循环无 prototype dispatch、boxing 和多余 temporary copy。

### Layout

- empty、single field、padding、nested、large alignment、overflow。
- generic/union/array/tuple/owner/GC field maps。
- declaration order、layout hash drift、module mismatch。
- bitwise/fieldwise/move-only copy 和 drop order。

### Receiver

- lvalue/rvalue、readonly/writable、class/struct/interface。
- override effect weakening/strengthening。
- generic constrained call、dynamic call boundary。
- readonly struct 无 defensive copy。

### ref struct

- legal local/param/return/nested ref struct。
- illegal GC/resource field、array、box、capture、global、await/yield。
- internal GC handle/root map、owner field/drop map。
- move/copy/return region。

### Span

- default/empty/one/max length。
- negative/overflow/start-at-end/slice-to-end。
- array/owner/stack/native source。
- writable/read-only index result。
- owner drop/move/reuse conflict。
- GC compact while array Span active。

### Performance/stress

- 大数组顺序访问和 bounds-check elimination。
- nested struct 数组 cache locality。
- 高频 slice 不分配。
- pool rent/span/return 热循环。
- 大量 frame ref maps 的 GC pause/scan 成本。

## 13. 参考依据

- .NET `Span<T>` 实际 `ref T + length` 布局：`lua/runtime/src/libraries/System.Private.CoreLib/src/System/Span.cs`、`ReadOnlySpan.cs`。
- C# span safety/ref-like 限制：`lua/csharplang/proposals/csharp-7.2/span-safety.md`。
- C# ref field/readonly/scoped：`lua/csharplang/proposals/csharp-11.0/low-level-struct-improvements.md`、`csharp-7.2/readonly-struct.md`、`csharp-8.0/readonly-instance-members.md`。
- Roslyn Span/ref field/readonly tests：`lua/roslyn/src/Compilers/CSharp/Test/Semantic/Semantics/SpanStackSafetyTests.cs`、`RefFieldTests.cs`、`ReadOnlyStructsTests.cs`。
- C# struct construction/default initialization：`lua/csharplang/proposals/csharp-10.0/parameterless-struct-constructors.md`、`lua/roslyn/src/Compilers/CSharp/Test/Semantic/Semantics/StructConstructorTests.cs`、`lua/roslyn/src/Compilers/CSharp/Test/Emit/CodeGen/CodeGenConstructorInitTests.cs`。
- Rust aggregate move/drop/borrow 边界：`lua/rust/tests/ui/moves`、`lua/rust/tests/ui/drop`、`lua/rust/tests/ui/borrowck`。
- Rust struct literal 的 block ambiguity 与 call/literal 分离负例：`lua/rust/compiler/rustc_parse/src/parser/mod.rs`、`lua/rust/tests/ui/parser/struct-literals-in-invalid-places.rs`、`lua/rust/tests/ui/empty/empty-struct-braces-expr.rs`。
- CPython `type_call` 提供“类型作为 callable 统一构造”的动态对照：`lua/cpython/Objects/typeobject.c`。ZR 为保留 `@call` 且让 inline layout/constructor 静态可优化，刻意选择独立 `init TypeRef(...)`。
- CPython pool/arena 分层：`lua/cpython/Include/internal/pycore_obmalloc.h`、`lua/cpython/Objects/obmalloc.c`。
- ZR 当前 layout：`zr_vm_core/include/zr_vm_core/type_layout.h`、`tests/core/test_type_layout_inline_copy.c`、`tests/core/test_type_layout_metadata_contracts.c`。

ZR 的刻意差异是：Span 仍是普通标准库 TypeDef，但 ref-like/capability/layout 是通用元数据；第一版不开放 ref struct 作为无约束泛型实参，也不保留具体 struct 继承。值构造使用 `init TypeRef(...)`，不采用 C# `new`、Rust brace literal 或 CPython “call type to construct”的表层统一；这样 `@call`、GC allocation、resource ownership 和 inline value initialization 各有唯一入口。
