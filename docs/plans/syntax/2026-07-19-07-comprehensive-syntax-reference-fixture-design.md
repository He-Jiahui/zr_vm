# 07 ZR 目标语法全覆盖参考工程实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 建立一个单一、多模块、可运行的ZR参考工程，用正例、负例和产物golden覆盖本目录01-06、08-10已确认的全部目标语法与语义边界。

**Architecture:** `syntax_reference_v1` 是一项完整的像素渲染任务，不是互不相干的语法片段集合。源码层覆盖声明、类型、构造、调用、借用、布局、属性、所有权和反射；同一份 bound/SemIR facts 再由 VM、AOT、artifact、LSP 和 migration fixture 共同验证。

**Tech Stack:** ZR source/project fixture、zr_vm parser/compiler/SemIR、VM、AOT C/LLVM、`.zrs/.zri/.zro`、LSP、migration frontend、Unity/CMake project tests。

---

> 状态：设计参考第一版，等待人工修改与确认；不是当前编译器已经支持的语法声明。
>
> 硬依赖：[01-10子设计索引](./README.md)。任何代码块与已确认子设计冲突时，以对应细化设计为准，并同步修订本文。

## 1. “覆盖所有语法”的范围

本文中的“所有”采用可验证边界，不等于保留当前 parser 接受的每一种历史拼写：

1. 覆盖01-06、08-10定义的目标语言规则，包括Canonical TypeRef、Place/CFG facts、函数与引用、struct/ref struct/Span、resource ownership、property、reflection、pooling、native/FFI、module/package、迁移、artifact和LSP。
2. 覆盖与这些规则组合时不可缺少的稳定基础语法：`module/import`、`let/var/const`、class/interface/enum/union/generic、数组/tuple、普通调用、成员访问、`if/if let/switch/while/for`、异常、async、native extern、test、decorator 和 meta method。
3. 旧 `%xxx`、`$Type(...)` 和 `$proto(...)` 只进入 legacy input 或 compile-fail fixture，不进入 current-pass 源码。
4. `intermediate` 裸指令块、`{{ ... }}` generator、动态 object/prototype 拼接构造不属于核心语言参考工程；它们需要单独决定是工具 IR、库能力还是删除项，不能借本 fixture 自动晋级为目标语法。
5. `using` 只保留 Close/Dispose 作用域这一语义，但 04 尚未冻结其完整 statement grammar；在 grammar 冻结前只列入 coverage manifest 的 `surfacePending`，不在 current-pass 文件中猜测拼写。owner 生命周期由 `Unique<T>` 自动 Drop 和 `drop(value)` 覆盖。
6. 自定义 Drop body 第一版沿用当前 `@destructor` 声明作为过渡表层，语义一律绑定为 04 的不可抛错、不可挂起 `DropContract`。若后续改名，只允许替换声明表层，不得改变 cleanup CFG 和 artifact contract。

因此，本计划既提供完整设计样例，也显式暴露尚未冻结的旧语言表面，避免“示例里没写”被误认为已经删除或已经批准。

### 1.1 当前完整规范逐章处置

| 当前规范表面                                                                | 本参考工程处置                                                                                             | 状态                  |
| --------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------- | --------------------- |
| keyword、identifier、comment、整数/浮点/字符串/字符/bool/null literal       | 在 parser/doc-test lexical matrix 覆盖，运行用例选取代表值                                                 | inherited-current     |
| arithmetic/comparison/logical/bitwise/assignment/member/index/call operator | 在 model/algorithms/main 和 meta-method 驱动测试覆盖                                                       | inherited-current     |
| module/import                                                               | `module name;` + `let alias = import("path");`，返回ModuleNamespace object并记录静态依赖               | target-current        |
| let/var/const 与类型推断                                                    | 绑定可变性、字段可变性、readonly capability 分开覆盖                                                       | target-current        |
| object/array destructuring                                                  | 保留当前结构化 binding AST；补充 parser/Place projection case                                              | inherited-current     |
| named/local/member/interface/async function与native declaration             | 强制`fn` 和定义/声明 return `:`                                                                        | target-current        |
| anonymous function、variadic、spread                                        | 匿名定义使用`:`/`=>`；variadic/spread由 reflection用例覆盖                                             | target-current        |
| struct/class/interface/enum/union/generic                                   | current-pass 多模块源码覆盖                                                                                | target-current        |
| object literal、array literal、tuple                                        | current-pass/control-flow源码及 parser golden覆盖                                                          | inherited-current     |
| if/if let/switch/while/for/break/continue/return                            | main/ownership/algorithms覆盖                                                                              | inherited-current     |
| try/catch/finally/throw                                                     | main与cleanup CFG golden覆盖                                                                               | inherited-current     |
| test/comptime/async/await/native extern                                     | effects/host/native_ffi和await负例覆盖                                                                     | target-current        |
| decorator`#...#`                                                          | 保留现有 decorator AST 与 target metadata；单独 parser/runtime case，不与`%` 迁移混合                    | inherited-current     |
| meta method                                                                 | `@constructor/@call/@add/@destructor`在正例中覆盖，其余 meta kind由现有 meta-surface fixture继续逐项驱动 | inherited-current     |
| property getter/setter                                                      | 统一`property` AST + 显式`pri/pro/pub let|var` field；ref property getter-only                          | target-current        |
| value/class/resource/dynamic construction                                   | 分别使用`init/new/own/createInstance`                                                                    | target-current        |
| `%type`                                                                   | 拆为`typeof(expr)` 与 `typeid(TypeRef)`                                                                | target-current        |
| cast`<Type>expr`                                                          | 现有拼写与 generic/comparison解析压力较大，本轮不冻结；保留现有测试并进入后续 expression-syntax审查        | surfacePending        |
| fixed/ranged array`T[N]`、`T[min..max]`                                 | layout/range语义保留，声明拼写在 layout扩展计划确认后再晋级reference fixture                               | surfacePending        |
| Close/Dispose`using`                                                      | 语义保留，statement grammar等待冻结                                                                        | surfacePending        |
| `{{ ... }}` generator                                                     | 不进入核心reference；需要独立 generator/coroutine设计决定替代语法                                          | excluded-from-core-v1 |
| `intermediate` source block                                               | 移到工具/调试IR边界，不作为用户语言current-pass                                                            | excluded-from-core-v1 |
| 旧`%xxx` 与 `$...`                                                      | 只进入migration/negative fixture                                                                           | legacy-only           |

`surfacePending` 不是默认批准，也不是遗漏。第 12 节要求这些项在“全语法 reference v1”正式晋级前逐项变为 current、legacy-only 或 excluded；不能长期保持含糊状态。

## 2. 第一版已锁定规则

| 主题                                        | 规范写法                                               | 不允许的 fallback                                      |
| ------------------------------------------- | ------------------------------------------------------ | ------------------------------------------------------ |
| 命名/member/interface/async函数定义与native函数声明 | `fn name(args): ReturnType`                  | 不接受定义/声明位置`-> ReturnType`                   |
| 匿名函数定义                                | `fn(args): ReturnType => expression` 或 block        | `=>` 不表示 return type                              |
| callable TypeRef                            | `fn(A) -> R`，`->` 右结合                          | 类型位置不接受`fn(A): R`                             |
| 可写/只读引用                               | `ref T`、`ref readonly T`                          | 不产生 Borrow/Loan runtime wrapper                     |
| 参数 contract                               | `in T`、`out T`、`scoped ref T`                  | 调用解析不能猜测省略的`ref/out`                      |
| value struct 构造                           | `init TypeRef(args)`                                 | 不查询`@call`，不接受 runtime Type value             |
| GC class 构造                               | `new ClassType(args)`                                | 不构造 struct/resource class                           |
| resource 构造                               | `own ResourceType(args)`                             | 不把普通 class 强化为 Unique                           |
| 普通调用                                    | `expr(args)`                                         | callable/`@call` 失败后不尝试 constructor            |
| 动态反射构造                                | `requireConstructible(type).createInstance(...constructionArgs)` | 不进入`init/new/own/@call` binder       |
| property                                    | `property name: T { pri/pro/pub get/set/init }`      | 不拼接成员、不生成backing field；ref property无setter |
| pool identity/access                        | `PoolHandle<T>` -> `tryBorrow` -> `PoolRef<T>`       | handle不伪装ref struct，direct ref不逐次检查generation |
| 提前释放                                    | `drop(owner)`                                        | 不保留`%release/%detach` 隐式状态切换                |
| simple declaration/statement终止            | 显式`;`                                              | newline、`}`、EOF均不自动终止                        |
| braced declaration终止                      | `}`，可选一个尾随`;`                               | formatter默认不输出；不开放重复empty statement         |
| compound control-flow终止                   | 完整block/branch chain                                 | 不消费declaration`;`，branch之间禁止插入`;`        |
| static module import                        | `let alias = import("module.path");`                 | 不接受裸import、dynamic path或local/conditional import |

四种静态构造/调用和一种动态反射边界必须在 AST、binder 与 SemIR 中保持独立：

```text
init S(args)                              -> ValueConstruct
new C(args)                               -> GcNew
own R(args)                               -> OwnConstruct
callable(args)                            -> Call / MetaCall(@call)
requireConstructible(type).createInstance(...args) -> ordinary resolved API call -> reflection runtime boundary
```

`ReflectionCreateInstance` 是 runtime/service operation，不是第五种源码 ConstructExpression。源码 AST 仍是普通 member call 加 spread argument；这样 reflection 不会污染静态 TypeRef parser。

### 2.1 分号与换行

本reference fixture必须能够删除全部非字面量换行后保持完全相同的tokenization、AST和语义：

```zr
module syntax.reference.sample;
let math = import("core.math");
let value: int = 1;
value += 2;
```

- newline只属于trivia，不是terminator，也不触发ASI。
- field/local/module/import binding、expression/assignment、return/throw/break/continue、bodyless function/accessor和expression accessor必须写 `;`。
- class/struct/interface/resource class/enum/union/function/property等braced declaration自己的`{ ... }`已提供闭合边界，可以省略一个尾随`;`；formatter默认省略。
- `if/else`、`switch`、loop、`try/catch/finally`由完整compound-statement grammar闭合，不消费declaration semicolon，branch之间不能插入`;`。
- `let callback = fn(): int { return 1; };` 仍需要最后的 `;`，因为braced anonymous function只是initializer，外层let declaration尚未结束。
- 缺失分号在newline、`}`和EOF前都产生同一`missing_statement_terminator`诊断；formatter/code action插入 `;`，不能通过保留换行修复。

### 2.2 ModuleNamespace import binding

```text
ModuleDeclaration
  := "module" QualifiedModuleName ";"

ModuleImportBinding
  := "let" Identifier "=" ImportExpression ";"

ImportExpression
  := "import" "(" StringLiteral ")"
```

第一版限定：

- import binding只能位于module scope且必须使用immutable `let`。
- ImportExpression是专用AST节点，不是名字为`import`的普通CallExpression。
- string literal在compile时canonicalize为ModuleId；source/binary/native provider由统一resolver选择。
- binding既产生runtime readonly ModuleNamespace object，也产生compile-time namespace fact；`alias.Type`可进入TypeRef，`alias.member`可进入value expression。
- module object可以作为普通readonly object读取或传递，但复制出的普通value alias不继承TypeRef namespace资格。
- dynamic path、conditional/local import使用`loadModule/loadPlugin`及显式Result/union，不使用ImportExpression。

## 3. `zr.reflection` 第一版契约

### 3.1 公开签名

```zr
let reflection = import("zr.reflection");
let declarations = import("zr.reflection.declaration");

// TypeOf<T>: Type
// ClassTypeOf<T>: TypeOf<T> where T: class
// ConcreteClassTypeOf<T>: ClassTypeOf<T> where T: class
// InstanceClassTypeOf<T>: ConcreteClassTypeOf<T> where T: class, new
// StructTypeOf<T>: TypeOf<T> where T: struct
// InterfaceTypeOf<T>: TypeOf<T> where T: interface

interface ConstructibleType {
    pub const fn createInstance(...constructionArgs: object): object;
}
```

该声明固定以下含义：

- 声明侧 `...constructionArgs: object` 是 value-only variadic parameter；每个元素的公开参数类型为 `object`。
- 函数体/runtime service 看到一个只读参数序列；实现可以使用 argument vector 或 `ReadOnlySpan<object>`，不要求直接调用时先分配新的 `object[]`。
- 调用侧已有 `object[]` 时写 `...constructionArgs` 展开。展开表达式只求值一次，元素保持从左到右顺序。
- 直接调用 `type.createInstance(1, "name")` 与数组展开调用使用同一个 binder。值类型参数按 reflection boundary 的装箱规则进入 `object` 参数序列。
- 第一版只支持位置参数；不支持 named argument、`ref/out` constructor parameter 或把 spread element 当作字段 initializer。
- `createInstance` 只存在于 `ConstructibleType` capability；`ConcreteClassTypeOf<T>`、`InstanceClassTypeOf<T>`和普通`StructTypeOf<T>`实现它。
- `Type`/`TypeOf<T>`、interface/resource/ref-struct descriptor不携带注定失败的构造方法。
- `createInstance` 是 readonly receiver API。内部 constructor cache 不构成对公开 descriptor capability 的可观察写入。

规范调用：

```zr
let constructionArgs: object[] = [12, 34, 56, 255];
let pixelType = reflection.resolve(typeid(model.Pixel));
let boxedPixel: object = pixelType.createInstance(...constructionArgs);
```

### 3.2 可构造类别

| runtime type category                    | 第一版结果                     | 规则                                        |
| ---------------------------------------- | ------------------------------ | ------------------------------------------- |
| concrete`class`                        | GC object，以`object` 返回   | 绑定可见 public constructor                 |
| `struct` / `readonly struct`         | boxed value，以`object` 返回 | 使用同一 TypeLayout 和 constructor metadata |
| `ref struct` / `readonly ref struct` | 拒绝                           | 不能 heap box，不能绕过 region              |
| `resource class`                       | 拒绝                           | 必须通过静态`own TypeRef(...)` 建立 owner |
| interface / abstract class               | 拒绝                           | 无 concrete allocation target               |
| open generic                             | 拒绝                           | 必须先形成 closed Type descriptor           |

`@call` 永远不进入候选集合。即使某个 Type object 同时公开 `@call`，`createInstance` 也只查询 constructor metadata。

### 3.3 Binder、错误和缓存

第一版 binder 采用以下确定顺序：

1. 验证 type category、closed generic、可实例化和 metadata preservation。
2. 枚举 public instance constructors，并按 arity 过滤。
3. 对每个参数执行 identity、nullable/reference compatibility 和语言已批准的安全隐式转换。
4. exact match 优先于 conversion match；无唯一最佳候选时报 `reflection.constructor_ambiguous`。
5. 无候选时报 `reflection.constructor_not_found`；不合法类型时报 `reflection.type_not_constructible`。
6. constructor body 抛错时保留原始 cause，并以 `reflection.constructor_threw` 标记 reflection invocation boundary。
7. partial struct/resource-like field cleanup 仍由 constructor 的 cleanup plan 执行；resource class 在进入 constructor 前已经被类别检查拒绝。

缓存 key 至少包含：

```text
ReflectionConstructorCacheKey
  target TypeId
  argument runtime TypeId vector, including null marker
  accessibility mode = public
  module/runtime generation
```

缓存只保存 constructor/binder plan，不缓存构造结果。module reload、metadata generation 或 constructor contract hash 改变时必须失效。

### 3.4 `typeof` 与 `typeid`

本参考工程固定迁移后的职责：

```zr
let reflectionType: reflection.Type = typeof(value);
let pixelTypeId: TypeId<Pixel> = typeid(Pixel);
```

- `typeof(expr)` 求值 expression一次并返回真实runtime descriptor；只有exact-type fact充分时才暴露最精确`TypeOf<T>`子类，否则静态返回`Type`。
- `typeid(TypeRef)` 在类型上下文解析静态 TypeRef，返回稳定轻量 `TypeId`，不创建descriptor或root成员metadata。
- `reflection.resolve(typeid(T))` 在无实例时显式进入反射系统，并按declaration category返回精确descriptor。
- `typeid(Pixel).createInstance(...)` 非法；TypeId不是reflection descriptor。
- `typeof(window)` 返回RefStructTypeOf并且不实现ConstructibleType；不能调用createInstance。

## 4. 单一参考工程结构

计划创建：

```text
tests/fixtures/projects/syntax_reference_v1/
  syntax_reference_v1.zrp
  src/
    host.zr
    model.zr
    object_model.zr
    ownership.zr
    algorithms.zr
    effects.zr
    reflection.zr
    pooling.zr
    modules.zr
    native_ffi.zr
    nested/
      imports.zr
    main.zr
  packages/
    fixturedep/
      fixturedep.zrp
      src/
        index.zr
        tool.zr
  artifacts/
    fixturedep.zrm
  surface/
    lexical_and_literals.zr
    destructuring_and_literals.zr
    decorators_and_meta.zr
  negative/
    function_delimiters.zr
    reference_and_escape.zr
    construction_boundaries.zr
    reflection_boundaries.zr
    ownership_boundaries.zr
    property_boundaries.zr
    pooling_boundaries.zr
    module_package_boundaries.zr
    native_ffi_boundaries.zr
    legacy_percent_surface.zr
  golden/
    coverage.json
    diagnostics.json
    lsp.json
    syntax.zrs
    semantic.zri
    public.zro.manifest.json
```

项目 manifest：

```json
{
  "manifestVersion": 2,
  "name": "syntax_reference_v1",
  "version": "1.0.0",
  "kind": "library",
  "source": "src",
  "binary": "bin",
  "entry": "main",
  "aliases": {
    "#fixture": "syntax/reference"
  },
  "package": {
    "name": "@syntaxref",
    "exports": {
      ".": "main",
      "./model": "model"
    }
  },
  "dependencies": {
    "@fixturedep": {
      "version": "1.0.0",
      "path": "packages/fixturedep/fixturedep.zrp"
    }
  },
  "supportMultithread": true,
  "autoCoroutine": true
}
```

`src` 是完整项目 current-pass；`surface` 是仍然属于 current language、但不适合塞进运行 checksum 的 parser/runtime focused case；`negative` 每个 case 必须通过 manifest 指定唯一预期诊断；`golden` 不保存机器相关绝对路径。

## 5. 规范正例

以下代码共同组成一个“构建像素、借用连续窗口、创建渲染资源、再通过反射重建配置”的完整用例。代码块是目标源码，不是省略号式伪代码；所有simple declaration/statement均使用显式 `;`，换行可被minifier全部移除。

### 5.1 `host.zr`

```zr
module syntax.reference.host;

native extern("syntax_reference_host") {
    #zr.ffi.callingConvention("c")#
    #zr.ffi.entry("syntax_require")#
    pub fn require(condition: bool, message: string): void;

    #zr.ffi.entry("syntax_print")#
    pub fn print(value: in string): void;

    #zr.ffi.entry("syntax_release_texture")#
    pub fn releaseTexture(handle: int): void;
}
```

覆盖 `module`、`native extern`、FFI metadata、命名函数 `:` return delimiter 和 `in` 参数；binder必须生成Canonical CallableContract和FfiSignature。

### 5.2 `model.zr`

```zr
module syntax.reference.model;

let host = import("syntax.reference.host");

enum PixelFormat: int {
    Rgba8 = 0;
    Bgra8 = 1;
}

union DecodeResult<T> {
    Ok(value: T);
    Error(message: string);
}

readonly struct Extent {
    let width: int;
    let height: int;

    pub @constructor(width: int, height: int) {
        host.require(width > 0, "width must be positive");
        host.require(height > 0, "height must be positive");
        this.width = width;
        this.height = height;
    }

    fn area(): int {
        return width * height;
    }
}

struct Pair<TLeft, TRight> {
    let left: TLeft;
    let right: TRight;

    pub @constructor(left: TLeft, right: TRight) {
        this.left = left;
        this.right = right;
    }
}

struct Pixel {
    var red: int;
    var green: int;
    var blue: int;
    var alpha: int;

    pub @constructor(red: int, green: int, blue: int, alpha: int) {
        this.red = red;
        this.green = green;
        this.blue = blue;
        this.alpha = alpha;
    }

    pub static @call(gray: int): Pixel {
        return init Pixel(gray, gray, gray, 255);
    }

    pub static @add(left: Pixel, right: Pixel): Pixel {
        return init Pixel(
            left.red + right.red,
            left.green + right.green,
            left.blue + right.blue,
            left.alpha + right.alpha
        );
    }

    pub const fn checksum(): int {
        return red + green + blue + alpha;
    }

    pub fn clear(): void {
        red = 0;
        green = 0;
        blue = 0;
        alpha = 0;
    }
}

ref struct PixelWindow {
    pri let cells: Span<Pixel>;

    pub @constructor(cells: Span<Pixel>) {
        this.cells = cells;
    }

    pub property length: usize {
        pub get => cells.length();
    }

    pub property first: ref Pixel {
        pub get => ref cells[0];
    }

    pub property firstReadonly: ref readonly Pixel {
        pub get => ref cells[0];
    }
}
```

关键区分：

```zr
let constructed: Pixel = init Pixel(1, 2, 3, 255);
let called: Pixel = Pixel(8);
```

第一行只解析 `@constructor`；第二行只解析 `@call`。二者即使返回同一类型，也不能共享 bound kind 或互相 fallback。

### 5.3 `object_model.zr`

```zr
module syntax.reference.object_model;

let host = import("syntax.reference.host");
let model = import("syntax.reference.model");

interface Named {
    pub property name: string {
        pub get;
    }

    pub const fn describe(): string;
}

class RenderSettings: Named {
    pri let _name: string;
    pri var _samples: int;
    pri var _z: int;

    pub property name: string {
        pub get {
            return _name;
        }
    }

    pub property samples: int {
        pub get {
            return _samples;
        }

        pri set {
            host.require(value > 0, "samples must be positive");
            _samples = value;
        }
    }

    pub property z: int {
        pub get => _z;
        pri set => this._z = value;
    }

    pub @constructor(name: string, samples: int) {
        this._name = name;
        this.samples = samples;
        this._z = 0;
    }

    pub const fn describe(): string {
        return name;
    }

    pub fn resetSamples(): void {
        samples = 1;
    }
}

class PixelBuffer {
    pri let pixels: model.Pixel[];

    pub @constructor(pixels: model.Pixel[]) {
        host.require(pixels.length > 0, "pixel buffer cannot be empty");
        this.pixels = pixels;
    }

    pub property length: usize {
        pub get => pixels.length;
    }

    pub property first: ref model.Pixel {
        pub get => ref pixels[0];
    }

    pub property firstReadonly: ref readonly model.Pixel {
        pub get => ref pixels[0];
    }
}
```

该文件覆盖 interface/concrete property同一AST、显式`pri let/var` field、field/local一致的replaceability、`value`隐式参数、pri setter、value getter、getter-only ref/ref readonly property和receiver effect。没有auto accessor、property initializer、contextual `field`或synthetic backing field。

### 5.4 `ownership.zr`

```zr
module syntax.reference.ownership;

let host = import("syntax.reference.host");
let model = import("syntax.reference.model");

class Document {
    pri let _title: string;

    pub property title: string {
        pub get {
            return _title;
        }
    }

    pub @constructor(title: string) {
        this._title = title;
    }
}

resource class Texture {
    pri let handle: int;
    pri let extent: model.Extent;

    pub @constructor(handle: int, extent: model.Extent) {
        this.handle = handle;
        this.extent = extent;
    }

    pub const fn id(): int {
        return handle;
    }

    pub const fn pixelCount(): int {
        return extent.area();
    }

    @destructor() {
        host.releaseTexture(handle);
    }
}

resource class RenderRequest {
    pri let document: Gc<Document>;

    pub @constructor(document: Gc<Document>) {
        this.document = document;
    }
}

pub fn readTexture(texture: in Texture): int {
    return texture.id() + texture.pixelCount();
}

pub fn shareTexture(texture: Unique<Texture>): Shared<Texture> {
    return texture.share();
}

pub fn weakChecksum(texture: Shared<Texture>): int {
    let weak: Weak<Texture> = texture.weak();
    if let Some(live) = weak.upgrade() {
        return readTexture(live);
    }
    return 0;
}

pub fn boxTexture(texture: Unique<Texture>): GcBox<Texture> {
    return texture.intoGc();
}
```

`RenderRequest` 只接受已经建立的 `Gc<Document>`，避免在本计划中发明尚未冻结的 root-handle factory 拼写。这个限制不会削弱 bridge contract 的覆盖：字段类别、constructor contract、GC map 和 artifact metadata 都必须被验证。

### 5.5 `algorithms.zr`

```zr
module syntax.reference.algorithms;

let model = import("syntax.reference.model");
let objects = import("syntax.reference.object_model");

pub fn inspect(value: in model.Pixel): int {
    return value.checksum();
}

pub fn mutate(value: scoped ref model.Pixel): void {
    value.red += 1;
}

pub fn initialize(result: out model.Pixel): bool {
    result = init model.Pixel(1, 2, 3, 255);
    return true;
}

pub fn swap(left: ref model.Pixel, right: ref model.Pixel): void {
    let temporary: model.Pixel = left;
    left = right;
    right = temporary;
}

pub fn choose(
    left: ref model.Pixel,
    right: ref model.Pixel,
    useLeft: bool
): ref model.Pixel {
    if (useLeft) {
        return ref left;
    }
    return ref right;
}

pub fn makeScaler(scale: int): fn(model.Pixel) -> model.Pixel {
    return fn(value: model.Pixel): model.Pixel => init model.Pixel(
        value.red * scale,
        value.green * scale,
        value.blue * scale,
        value.alpha
    );
}

pub fn makeScalerFactory(): fn(int) -> fn(model.Pixel) -> model.Pixel {
    return fn(scale: int): fn(model.Pixel) -> model.Pixel => makeScaler(scale);
}

pub fn readonlySettings(settings: in objects.RenderSettings): int {
    return settings.samples;
}

pub fn nllExample(): int {
    var pixel: model.Pixel = init model.Pixel(1, 2, 3, 4);
    let view: ref readonly model.Pixel = ref pixel;
    let before: int = view.checksum();
    pixel.red = 10;
    return before + pixel.red;
}

pub fn spanChecksum(values: ReadOnlySpan<model.Pixel>): int {
    var result: int = 0;
    for (var index: usize = 0; index < values.length(); index += 1) {
        result += values[index].checksum();
    }
    return result;
}

pub fn clearFirst(window: ref model.PixelWindow): void {
    window.first.clear();
}
```

`nllExample` 要求 shared loan 在最后一次 `view` 使用后结束，因而后续 field store 合法。`makeScalerFactory` 同时锁定函数定义 `:` 和右结合 callable `->`。

### 5.6 `effects.zr`

```zr
module syntax.reference.effects;

let model = import("syntax.reference.model");

pub comptime fn defaultAlpha(): int {
    return 255;
}

async fn ready(value: model.Pixel): model.Pixel {
    return value;
}

pub async fn checksumAsync(value: model.Pixel): int {
    let completed: model.Pixel = await ready(value);
    return completed.checksum();
}

test("target syntax function delimiters") {
    let mapper: fn(int) -> int = fn(value: int): int => value + 1;
    return mapper(1) - 2;
}
```

该文件只在没有活跃 ref/ref struct 的位置挂起；跨 `await` 的非法借用进入负例。

### 5.7 `reflection.zr`

```zr
module syntax.reference.reflection;

let host = import("syntax.reference.host");
let model = import("syntax.reference.model");
let objects = import("syntax.reference.object_model");
let reflect = import("zr.reflection");
let declarations = import("zr.reflection.declaration");

pub fn constructPixelByReflection(seed: in model.Pixel): object {
    let runtimeType: declarations.StructTypeOf<model.Pixel> = typeof(seed);
    host.require(runtimeType.id == typeid(model.Pixel), "runtime type mismatch");
    let reflectionType: declarations.StructTypeOf<model.Pixel> =
        reflect.resolve(typeid(model.Pixel));
    let constructionArgs: object[] = [12, 34, 56, 255];
    return reflectionType.createInstance(...constructionArgs);
}

pub fn constructSettingsByReflection(seed: in objects.RenderSettings): object {
    let runtimeType: reflect.Type = typeof(seed);
    host.require(runtimeType.id == typeid(objects.RenderSettings), "runtime type mismatch");
    let reflectionType: declarations.ConcreteClassTypeOf<objects.RenderSettings> =
        reflect.resolve(typeid(objects.RenderSettings));
    return reflectionType.createInstance("reflected", 4);
}

pub fn pixelTypeIdentity(): TypeId {
    return typeid(model.Pixel);
}
```

第一个函数覆盖exact struct `typeof`、`resolve(typeid(T))`和参数数组展开；第二个覆盖erased runtime `Type`、`ConcreteClassTypeOf<T>`和direct variadic call；第三个证明TypeId与reflection descriptor的职责分离。

### 5.8 `pooling.zr`

```zr
module syntax.reference.pooling;

let model = import("syntax.reference.model");
let pooling = import("zr.pooling");

pub fn deliver(
    pool: pooling.Pool<model.Pixel>,
    value: model.Pixel
): pooling.PoolHandle<model.Pixel> {
    return pool.deliver(value);
}

pub fn incrementRed(
    pool: pooling.Pool<model.Pixel>,
    handle: pooling.PoolHandle<model.Pixel>
): bool {
    var pixel: pooling.PoolRef<model.Pixel>;
    if (pool.tryBorrow(handle, out pixel)) {
        pixel.value.red += 1;
        return true;
    }
    return false;
}

pub fn recycleWhileBorrowed(
    pool: pooling.Pool<model.Pixel>,
    handle: pooling.PoolHandle<model.Pixel>
): bool {
    var pixel: pooling.PoolRef<model.Pixel>;
    if (pool.tryBorrow(handle, out pixel)) {
        let retired: bool = pool.recycle(handle);
        pixel.value.green += 1;
        return retired;
    }
    return false;
}
```

该文件固定二阶段语义：`PoolHandle<T>`只传递weak generational identity；`tryBorrow(handle, out view): bool`验证一次并初始化包含direct ref + guard的`PoolRef<T>`；recycle立即使后续borrow失败，但已经取得的PoolRef在guard结束前仍可零重复检查地访问旧实体。第一版不用`Option<PoolRef<T>>`，避免提前开放ref struct generic anti-constraint。

### 5.9 `modules.zr` 与 `nested/imports.zr`

`modules.zr`：

```zr
module syntax.reference.modules;

let modelDot = import("syntax.reference.model");
let modelSlash = import("syntax/reference/model");
let localDot = import(".model");
let localSlash = import("./model");
let aliasDot = import("#fixture.model");
let aliasSlash = import("#fixture/model");
let packageRoot = import("@fixturedep");
let packageToolDot = import("@fixturedep.tool");
let packageToolSlash = import("@fixturedep/tool");
let assemblyRoot = import("artifacts/fixturedep.zrm");

pub fn moduleChecksum(): int {
    let absoluteValue: modelDot.Pixel = init modelSlash.Pixel(1, 2, 3, 4);
    let localValue: localDot.Pixel = init localSlash.Pixel(2, 3, 4, 5);
    let aliasValue: aliasDot.Pixel = init aliasSlash.Pixel(3, 4, 5, 6);
    return absoluteValue.checksum()
        + localValue.checksum()
        + aliasValue.checksum()
        + packageRoot.value()
        + packageToolDot.value()
        + packageToolSlash.value()
        + assemblyRoot.value();
}
```

`nested/imports.zr`：

```zr
module syntax.reference.nested.imports;

let parentDot = import("..model");
let parentSlash = import("../model");

pub fn parentChecksum(): int {
    let value: parentDot.Pixel = init parentSlash.Pixel(2, 3, 4, 5);
    return value.checksum();
}
```

package fixture清单：

```json
{
  "manifestVersion": 2,
  "name": "syntax-reference-dependency",
  "version": "1.0.0",
  "description": "Package and assembly resolver fixture.",
  "license": "MIT",
  "kind": "library",
  "source": "src",
  "entry": "index",
  "package": {
    "name": "@fixturedep",
    "exports": {
      ".": "index",
      "./tool": "tool"
    }
  },
  "assembly": {
    "name": "fixturedep",
    "output": "../../artifacts/fixturedep.zrm"
  }
}
```

`packages/fixturedep/src/index.zr`：

```zr
module fixturedep.index;

pub fn value(): int {
    return 11;
}
```

`packages/fixturedep/src/tool.zr`：

```zr
module fixturedep.tool;

pub fn value(): int {
    return 13;
}
```

这些import pair必须分别规范化为相同Canonical ModuleId；LSP hover保留原spelling但显示同一ModuleIdentity。package fixture将`@fixturedep`限制为单段包名并只export`.`与`./tool`。测试先把同一package构建为`artifacts/fixturedep.zrm`，再证明package/source与assembly entry具有相同public contract hash；二者的provider/artifact identity仍可区分。

### 5.10 `native_ffi.zr`

```zr
module syntax.reference.native_ffi;

native extern("syntax_reference_native") {
    #zr.ffi.callingConvention("c")#
    #zr.ffi.entry("syntax_sum_i32")#
    pub fn sum(values: in i32, count: usize): i32;

    #zr.ffi.entry("syntax_try_read")#
    pub fn tryRead(index: usize, value: out i32): bool;

    #zr.ffi.entry("syntax_exchange")#
    pub fn exchange(value: ref i32, replacement: i32): void;
}
```

该文件锁定 native block、entry metadata、calling convention以及`in/ref/out`到borrow facts和FfiSignature direction的双重投影。正例只使用可直接分类的scalar；Span/pointer/custom marshaller进入独立能力测试。

### 5.11 `main.zr`

```zr
module syntax.reference.main;

let host = import("syntax.reference.host");
let model = import("syntax.reference.model");
let objects = import("syntax.reference.object_model");
let owners = import("syntax.reference.ownership");
let algorithms = import("syntax.reference.algorithms");
let effects = import("syntax.reference.effects");
let reflection = import("syntax.reference.reflection");
let pooled = import("syntax.reference.pooling");
let modules = import("syntax.reference.modules");
let nestedImports = import("syntax.reference.nested.imports");
let pooling = import("zr.pooling");

pub fn decode(code: int): model.DecodeResult<model.Pixel> {
    if (code < 0) {
        return model.DecodeResult<model.Pixel>.Error("negative code");
    }
    return model.DecodeResult<model.Pixel>.Ok(
        init model.Pixel(code, code, code, 255)
    );
}

pub fn main(): int {
    let extent: model.Extent = init model.Extent(4, 4);
    let pair: model.Pair<int, string> = init model.Pair<int, string>(4, "pixels");
    var first: model.Pixel = init model.Pixel(1, 2, 3, effects.defaultAlpha());
    var second: model.Pixel = model.Pixel(8);

    algorithms.swap(ref first, ref second);
    algorithms.mutate(ref first);

    var initialized: model.Pixel;
    let initializedOk: bool = algorithms.initialize(out initialized);
    host.require(initializedOk, "out initialization failed");

    let selected: ref model.Pixel = algorithms.choose(ref first, ref second, true);
    selected.green += 1;

    let pixels: model.Pixel[] = [first, second, initialized];
    let buffer: objects.PixelBuffer = new objects.PixelBuffer(pixels);
    var window: model.PixelWindow = init model.PixelWindow(pixels.span());
    algorithms.clearFirst(ref window);

    let particlePool = new pooling.Pool<model.Pixel>();
    let pooledHandle: pooling.PoolHandle<model.Pixel> =
        pooled.deliver(particlePool, first);
    host.require(
        pooled.incrementRed(particlePool, pooledHandle),
        "initial pool borrow failed"
    );
    host.require(
        pooled.recycleWhileBorrowed(particlePool, pooledHandle),
        "pool retire failed"
    );
    host.require(
        !pooled.incrementRed(particlePool, pooledHandle),
        "stale pool handle became valid again"
    );

    let scalerFactory: fn(int) -> fn(model.Pixel) -> model.Pixel =
        algorithms.makeScalerFactory();
    let mapper: fn(model.Pixel) -> model.Pixel = scalerFactory(2);
    let mapped: model.Pixel = mapper(buffer.firstReadonly);
    buffer.first = mapped;

    let settings: objects.RenderSettings = new objects.RenderSettings("reference", 4);
    settings.samples += 1;
    let readonlySampleCount: int = algorithms.readonlySettings(settings);

    let unique: Unique<owners.Texture> = own owners.Texture(7, extent);
    let shared: Shared<owners.Texture> = unique.share();
    let weak: Weak<owners.Texture> = shared.weak();
    var ownerChecksum: int = 0;
    if let Some(live) = weak.upgrade() {
        ownerChecksum = owners.readTexture(live);
    }
    drop(shared);

    let gcOwned: Unique<owners.Texture> = own owners.Texture(8, extent);
    let boxedResource: GcBox<owners.Texture> = owners.boxTexture(gcOwned);

    let boxedPixel: object = reflection.constructPixelByReflection(first);
    let boxedSettings: object = reflection.constructSettingsByReflection(settings);
    host.require(boxedPixel != null, "boxed pixel missing");
    host.require(boxedSettings != null, "boxed settings missing");
    host.require(
        reflection.pixelTypeIdentity() == typeid(model.Pixel),
        "type identity mismatch"
    );

    var flowChecksum: int = 0;
    switch (decode(pair.left)) {
        (model.DecodeResult<model.Pixel>.Ok(value)) {
            flowChecksum += value.checksum();
        }
        (model.DecodeResult<model.Pixel>.Error(message)) {
            host.print(message);
        }
    }

    var loopIndex: int = 0;
    while (loopIndex < 2) {
        loopIndex += 1;
        if (loopIndex == 1) {
            continue;
        }
        break;
    }

    try {
        host.require(buffer.length > 0, "buffer empty");
    } catch (error) {
        host.print(error.toString());
        throw error;
    } finally {
        host.print("syntax_reference_v1");
    }

    let spanValue: int = algorithms.spanChecksum(pixels.span());
    let summary: [int, bool] = [
        extent.area()
            + readonlySampleCount
            + ownerChecksum
            + flowChecksum
            + spanValue
            + algorithms.inspect(first)
            + algorithms.nllExample()
            + modules.moduleChecksum()
            + nestedImports.parentChecksum(),
        typeof(boxedResource) != null,
    ];
    host.require(summary[1], "GC bridge failed");
    return summary[0];
}
```

`main` 的返回值必须在 interp、binary-first、AOT C 和 AOT LLVM 下完全一致；测试不得依赖地址、时间、随机数或 finalizer 调度时机。

### 5.12 `surface` focused files

`surface/lexical_and_literals.zr`：

```zr
module syntax.reference.surface.lexical;

pub fn lexicalChecksum(): int {
    let decimal = 123;
    let hexadecimal = 0x2A;
    let octal = 077;
    let floating = 3.5;
    let scientific = 1.5e2;
    let text = "line\nvalue";
    let character = 'Z';
    let nullable: object = null;
    let arithmetic = ((decimal + hexadecimal) * 2 - octal) / 2;
    let bits = (arithmetic << 1) ^ 0x0F;
    let logic = nullable == null && text != "" && character == 'Z';
    let selected = logic ? bits : 0;
    return selected % 257 + <int>floating + <int>scientific;
}
```

此文件中的 `<int>expr` 只作为当前 cast surface 的保留测试，不代表第 1.1 节已批准它成为最终拼写；coverage manifest 将两个 cast 标为 `surfacePending`，其余 lexical/operator case 标为 `inherited-current`。

`surface/destructuring_and_literals.zr`：

```zr
module syntax.reference.surface.destructuring;

pub fn destructuringChecksum(): int {
    let record = {
        width: 4,
        height: 5,
    };
    let { width, localHeight: height } = record;

    let numbers: int[] = [1, 2, 3];
    var [first, second] = numbers;
    first += 1;

    let tuple: [int, string, bool] = [width + localHeight, "surface", true];
    if (tuple[2]) {
        return tuple[0] + first + second;
    }
    return 0;
}
```

destructuring固定规则：RHS只求值一次；object entry别名方向是`localName: sourceField`；`let/var`统一作用于全部leaf binding。array pattern要求source至少有N项，额外项忽略，少于N项在静态长度已知时编译失败、动态长度时只执行一次前置shape check。第一版不开放default、rest或nested pattern；Copy leaf复制，move-only rvalue leaf按Place/Drop facts移动，无法安全表达的owner partial move静态拒绝。

`surface/decorators_and_meta.zr`：

```zr
module syntax.reference.surface.decorators;

#serializable#
class DecoratedCounter {
    #range(min: 0, max: 255)#
    pub var value: int;

    pub @constructor(value: int) {
        this.value = value;
    }

    pub @toInt(): int {
        return value;
    }

    pub @toString(): string {
        return value.toString();
    }

    pub @toBool(): bool {
        return value != 0;
    }
}
```

`#serializable#` 与 `#range(...)#` 必须来自测试 host 注册的标准 decorator metadata，fixture 不允许用未解析 decorator 名称蒙混 parser pass。其余 arithmetic/index/conversion meta kinds继续由 meta registry生成的 focused matrix逐项断言；本文件验证 decorator target、argument、class/field metadata和转换语法在新函数/property模型下不会漂移。

## 6. Coverage manifest

`golden/coverage.json` 必须按稳定 feature id 记录覆盖位置，禁止只写“由 main 覆盖”。最小矩阵如下：

| feature group                                           | current-pass 证据             | compile-fail 证据                       | 核心 consumer                 |
| ------------------------------------------------------- | ----------------------------- | --------------------------------------- | ----------------------------- |
| function definition`:`                                | host/model/algorithms/effects | definition 使用`->`                   | parser, AST, formatter, LSP   |
| callable`->` / right associativity                    | algorithms/effects            | type 使用`:`                          | TypeRef, artifact signature   |
| anonymous`:` + `=>` body                            | algorithms/effects            | definition body 使用`->`              | parser, closure lowering      |
| `in/ref/out/scoped/readonly`                          | algorithms/main               | marker 缺失、逃逸、冲突                 | Place/CFG/borrow checker      |
| `init/new/own/call`                                   | model/object/ownership/main   | category 交叉误用                       | distinct bound/SemIR kinds    |
| reflection Type/TypeOf/createInstance                   | reflection/main               | invalid capability、ambiguous/no ctor   | metadata/runtime binder       |
| struct/readonly/ref struct                              | model/main                    | boxing/heap field/await escape          | TypeLayout/region checker     |
| Span/continuous array                                   | model/algorithms/main         | bounds/owner-drop conflict              | range facts, VM/AOT checks    |
| receiver effect                                         | model/object/ownership        | readonly 调 writable member             | callable contract/dispatch    |
| Unique/Shared/Weak/Drop                                 | ownership/main                | use-after-move、Shared mutable          | CFG cleanup/runtime owner ops |
| Gc/GcBox bridge                                         | ownership/main                | direct cross-world field                | GC map/barrier/finalization   |
| property AST/explicit field/ref                         | object/model/main             | concrete auto/ref setter                | PropertySymbol/Place/lowering |
| PoolHandle/PoolRef/retirement                           | pooling/main                  | heap/suspend/ref setter/fake NoScan      | TypeLayout/guard/GC runtime   |
| enum/union/generic/array/tuple                          | model/main                    | invalid generic/variant                 | TypeRef/layout/CFG            |
| lexical/operator/object literal/destructuring/decorator | surface focused files         | invalid literal/decorator/range         | lexer/parser/meta dispatch    |
| module/import/native extern/test/comptime/async         | all modules/effects/native_ffi| legacy`%`、invalid FFI与await escape  | project/compiler/LSP          |
| ModuleSpecifier/#alias/@package/.zrm                    | modules/nested/package fixture| invalid root/export/path/package name   | resolver/artifact/LSP/debug   |
| Canonical CallableContract/FfiSignature                 | host/native_ffi               | ABI/layout/direction/marshaller冲突     | semantic/VM/AOT/artifact      |
| explicit semicolon/no ASI                               | every current source file     | newline/`}`/EOF missing terminator    | lexer/parser/formatter/LSP    |
| ModuleNamespace import object                           | every imported module         | standalone/dynamic/local/mutable import | resolver/artifact/runtime/LSP |
| control flow/exception cleanup                          | main                          | out/drop on partial paths               | CFG facts/cleanup blocks      |
| `%xxx` migration                                      | none in src                   | legacy_percent_surface                  | migration/LSP code action     |

`coverage.json` 的每项必须包含：

```json
{
  "feature": "reflection.create_instance.spread",
  "source": "src/reflection.zr",
  "expect": "pass",
  "ast": "CallExpression(MemberAccess, SpreadArgument)",
  "semantic": "ConstructibleType.createInstance(object...): object",
  "consumers": ["vm", "aot_c", "aot_llvm", "artifact", "lsp"]
}
```

## 7. 负例与定向诊断

每个负例文件使用独立 case range 和一个 primary diagnostic。以下 case 不得被宽泛的 `unexpected token` 代替：

| case                                         | 必须拒绝的源码形态                           | 诊断要点                                   |
| -------------------------------------------- | -------------------------------------------- | ------------------------------------------ |
| definition arrow                             | `fn parse(value: int) -> int`              | 函数定义 return type 使用`:`             |
| callable colon                               | `let parser: fn(int): int`                 | callable TypeRef 使用`->`                |
| anonymous arrow                              | `fn(value: int) -> int => value`           | 匿名定义 return type 使用`:`             |
| newline termination                          | `let value = 1` 后直接换行                 | newline不结束声明，插入`;`               |
| block-close termination                      | `return value` 后直接 `}`                | `}`不替代simple statement的 `;`        |
| EOF termination                              | 文件以未终止call结束                         | EOF前仍要求`;`                           |
| control-flow separator                       | `if { } ; else { }`                        | branch之间不允许declaration semicolon      |
| import standalone                            | `import("core.math");`                     | import必须绑定module-scope immutable alias |
| import dynamic                               | `let m = import(path);`                    | ImportExpression只接受string literal       |
| import local/mutable                         | function内import或`var m = import(...)`    | static import只能module-scope`let`       |
| invalid package root/segment                | `import("@@math")`、`import("@1math")`、`import("@math//matrix")` | root须为单段`@identifier`且segment非空 |
| package export denied                       | `import("@fixturedep/internal")`          | 子模块未在package exports公开             |
| unknown alias                               | `import("#missing/tool")`                 | 指向缺失`.zrp` alias entry              |
| relative root escape                        | 超出workspace/package root的`../../../x`    | relative module不能越过解析根             |
| native definition arrow                     | native block内`fn f() -> int;`             | native declaration return使用`:`        |
| invalid FFI direction/layout                | `out`缺marshaller的ref-like/GC类型            | 指向参数与FfiSignature映射失败             |
| invalid FFI ABI metadata                    | 未注册calling convention或冲突metadata        | binding期拒绝，不推迟到首次调用            |
| missing ref marker                           | `swap(left, right)`                        | 参数要求显式`ref`                        |
| missing out marker                           | `initialize(result)`                       | 参数要求显式`out`                        |
| invalid local out                            | `var slot: out Pixel`                      | `out` 只允许 parameter type              |
| duplicated ref                               | `ref ref Pixel`                            | canonical ref 只有一层                     |
| shared/mutable conflict                      | shared view 活跃时写原 Place                 | 指向 loan origin 和冲突 write              |
| scoped escape                                | 返回/捕获`scoped ref`                      | 指出 destination region 过宽               |
| await escape                                 | ref/ref struct 跨`await`                   | 指出 suspension point                      |
| out incomplete                               | normal return 前未写 out                     | 指向缺失赋值路径                           |
| moved owner                                  | `unique.share()` 后再读 unique             | use-after-move                             |
| Shared writable                              | 经 Shared 调 writable`fn`                  | 只能形成 readonly borrow                   |
| wrong static construct                       | `new Pixel(...)` / `own Pixel(...)`      | 分别建议`init Pixel(...)`                |
| wrong value construct                        | `init RenderSettings(...)`                 | class 使用`new`                          |
| runtime init target                          | `init reflectionType(...)`                 | `init` operand 必须是 TypeRef            |
| ordinary call fallback                       | `Extent(1, 2)`                             | 普通 call 不会构造 struct，建议`init`    |
| legacy static`$` | `$Pixel(1, 2, 3, 4)`  | migration edit 为`init Pixel(...)`         |                                            |
| legacy dynamic`$` | `$runtimeType(1, 2)` | requiresReview，建议 createInstance 参数数组 |                                            |
| ref struct reflection                        | `typeof(window).createInstance()`          | RefStructTypeOf不实现ConstructibleType     |
| resource reflection                          | `typeofResource.createInstance()`          | ResourceClassTypeOf不实现ConstructibleType |
| erased reflection construct                  | `type.createInstance()`                    | Type必须先`requireConstructible`           |
| reflection ambiguity                         | 两个 equally-good constructor                | `reflection.constructor_ambiguous`       |
| reflection no match                          | arity/type 无候选                            | `reflection.constructor_not_found`       |
| property duplicate                           | 两个 get 或两个 set                          | accessor 重复                              |
| property set+init                            | 同一 property 同时 set/init                  | 第一版互斥                                 |
| concrete auto property                       | `property x: int { get; set; }`            | concrete accessor必须显式body/field        |
| auto ref property                            | `property item: ref Pixel { get; }`        | ref getter 必须显式来源                    |
| ref property setter                          | ref-return property 声明 set                 | ref property 不允许 set/init               |
| pool ref heap store                          | PoolRef作为class field/array element         | direct ref view必须保持ref-like            |
| pool ref suspension                          | PoolRef跨`await/yield`                     | 保存PoolHandle，恢复后重新borrow            |
| stale pool handle assumption                 | recycle后required borrow旧generation         | stale handle不能再次生效                    |
| fake pool NoScan                             | 含class field的T强制声明GcFree               | gcScanKind由Canonical TypeLayout计算        |
| short array destructuring                    | `[a,b,c]`绑定动态/静态长度2                 | shape check失败且不产生partial binding      |
| readonly receiver write                      | `in RenderSettings` 调 reset               | writable receiver 不可用                   |
| direct GC in resource                        | resource field 为`Document`                | 使用`Gc<Document>`                       |
| direct owner in class                        | class field 为`Unique<Texture>`            | 使用`GcBox<Texture>` 或重构生命周期      |

`legacy_percent_surface.zr` 至少包含 `%module/%import/%func/%async/%await/%extern/%test/%compileTime/%in/%ref/%out/%owned/%unique/%shared/%weak/%release/%detach/%type/%using` 和 `$` 两类构造。migration expected edits 必须逐 AST role 生成，不能全文字符串替换。

## 8. AST、SemIR 与 artifact 断言

### 8.1 Syntax AST

必须区分：

```text
FunctionDefinition(returnDelimiter = colon)
AnonymousFunctionExpression(returnDelimiter = colon, bodyKind = expressionArrow)
FunctionTypeSyntax(arrow = thinArrow)
ModuleDeclaration(qualifiedName, semicolonRange)
ModuleImportBinding(binding, ImportExpression(stringLiteral), semicolonRange)
StructInitExpression(typeSyntax, arguments)
GcNewExpression(typeSyntax, arguments)
OwnConstructExpression(typeSyntax, arguments)
CallExpression(callee, arguments)
SpreadArgument(expression)
PropertyDeclSyntax(accessors)
RefExpression(placeExpression)
SimpleStatement(terminatorRange)
```

`requireConstructible(type).createInstance(...constructionArgs)` 只能形成普通嵌套`CallExpression` + `SpreadArgument`，不能形成 ConstructExpression。

### 8.2 Bound/Semantic IR

参考工程至少断言：

- `init Pixel` -> `BoundValueConstruct` -> `ValueConstruct(destination, Pixel, ctor)`。
- `Pixel(8)` -> `BoundCall`/`MetaCall(@call)`，没有 constructor candidate。
- `new RenderSettings` -> `GcNew`。
- `own Texture` -> `OwnConstruct` + Unique result。
- `resolve/type member query/createInstance` -> registered reflection capability/service call；spread lowering生成object argument sequence，runtime service执行dynamic constructor binder。
- property read/write/ref access分别形成PropertyGet/Set/RefGet；accessor内预先绑定的显式field形成普通Place projection，没有backing field inference。
- ref property、array index和 Span index都产生可追踪 Place/Region。
- PoolHandle是ordinary value；`tryBorrow(handle, out view): bool`普通resolved call初始化ref-like value + guard，`value`形成PropertyRefGet/RefValue/Place，guard通过普通Drop/cleanup释放。
- `out` definite assignment、Unique availability 和 live loan set在 CFG join 处分别合并。
- try/finally、owner scope exit和partial construction统一进入 cleanup CFG。

### 8.3 `.zrs/.zri/.zro`

- `.zrs` 保存 token/range 和语法节点，不保存 runtime borrow 状态。
- `.zrs` 保存required/optional semicolon range、ImportExpression literal range和ModuleNamespace binding source range；newline仍只在trivia中。
- `.zri` 保存 TypeId、SymbolId、PlaceId、LoanId、Region、constructor binding、property lowering和 source map。
- `.zro` 保存公开callable/property/constructor contract、receiver effect、layout/drop/GC scan kind/maps、StableSlotSource contract hash和schema hash；不保存`%`、`$`、`init`等源码拼写，也不保存runtime PoolId/generation/borrow count。
- reflection-preserved constructor 进入 metadata roots；被 trim 的 constructor 不得在运行时偶然可见。
- source import 与 binary import 必须产生相同 public contract hash。
- `.zro` dependency table保存canonical ModuleId，不保存local alias拼写；runtime restore仍构造同一readonly ModuleNamespace object。

## 9. LSP 参考表现

- 函数定义 hover：`fn makeScaler(scale: int): fn(Pixel) -> Pixel`。
- callable binding hover：`fn(Pixel) -> Pixel`，不显示定义 delimiter。
- `init Pixel(` signature help只显示 `@constructor`；`Pixel(` 只显示 `@call`。
- `createInstance(` signature help固定显示 `const fn createInstance(...constructionArgs: object): object`。
- missing semicolon diagnostic在newline/`}`/EOF前提供同一个插入 `;` quick fix；formatter不会用换行替代terminator。
- hover import alias显示canonical ModuleId、provider kind和readonly ModuleNamespace；definition跳到module declaration，member navigation继续落到exported symbol。
- hover `...constructionArgs` 分别标识 `variadic parameter` 和 `spread argument: object[]`。
- property completion只显示一个 property member，可跳转到 getter/setter/init；ref-return hover显示 region和 writable/readonly ref kind。
- PoolHandle hover显示weak generational identity并建议`tryBorrow/tryRead`；PoolRef hover显示scoped direct ref + guard且禁止heap/suspension。
- borrow/move诊断同时高亮 origin、最后使用、冲突点或 move point。
- `$runtimeType(...)` code action生成`object[]`加`requireConstructible(runtimeType).createInstance(...constructionArgs)`的requiresReview edit；绝不生成`init runtimeType(...)`。
- semantic tokens把 `init`、`ref`、`in/out/scoped/readonly`、`resource class` 和 accessor context作为目标 token kind，不把普通同名标识符误标为 keyword。

## 10. VM/AOT 与性能验收

1. 普通 `init Pixel` 不允许 GC allocation、boxing、prototype hash lookup或 runtime type-category check。
2. `new`、`own` 和 `createInstance` 的分配成本必须分别归因，benchmark 不能混为同一 constructor 指标。
3. reflection 构造 struct 必须显式产生 box；这是 dynamic boundary 的已知成本，不得反向污染静态 `init`。
4. direct variadic reflection call允许使用 argument vector，避免强制先建 `object[]`；数组 spread在安全时借用现有连续参数存储，否则只 materialize 一次。
5. Span bounds check只有在 CFG range facts证明后才能删除；VM/AOT未证明时保持相同异常语义。
6. Unique hot path不创建 ref-count control block；第一次 `.share()` 才允许创建。
7. Shared retain/release、Weak upgrade和GcBox finalization在四 backend 上保持可观察语义一致。
8. reflection cache压力测试覆盖重复命中、不同 argument type vector、null marker、module generation失效和 constructor throw后的再次调用。
9. minifier删除全部trivia/newline后重新parse，AST/semantic hash必须与formatted source一致；分号token本身不进入runtime artifact。
10. repeated import返回同一environment内的module object并只执行一次module initialization；module cycle行为与artifact dependency graph一致。
11. parser stress生成至少100,000条由`;`分隔的single-line simple statement，并与pretty multiline版本比较AST/semantic hash、峰值内存和parse time；测试同时覆盖string/comment内部newline不被minifier破坏。
12. source-size报告分别记录formatted source、semicolon-preserving minified source、`.zrs`和`.zro`大小；不能把删除source map/debug metadata产生的缩小归因于semicolon grammar。
13. PoolHandle validate/reject、PoolRef hot field access、retire/deferred reuse分别计数；hot direct ref访问不能重复检查generation。
14. GcFree/GcMapped/GcBarriered slab分别报告allocation count、scan bytes、barrier/card和pause成本；只有GcFree结果可以归因于NoScan。

## 11. 实施任务

### Task 1: 建立 reference fixture 与 coverage manifest

**Files:**

- Create: `tests/fixtures/projects/syntax_reference_v1/syntax_reference_v1.zrp`
- Create: `tests/fixtures/projects/syntax_reference_v1/src/*.zr`
- Create: `tests/fixtures/projects/syntax_reference_v1/golden/coverage.json`
- Test: `tests/parser/test_syntax_reference_v1.c`

- [ ] **Step 1: 先加入只读取 fixture 并枚举 stable feature id 的失败测试**
- [ ] **Step 2: 运行 `cmake --build build-wsl-gcc --target zr_vm_syntax_reference_v1_test -j2`，确认因 fixture/feature id 缺失失败**
- [ ] **Step 3: 按第 4-6 节逐文件加入源码和 coverage manifest，不调整 production parser**
- [ ] **Step 3a: 对formatted与single-line minified版本断言相同AST/semantic hash，证明newline不参与终止**
- [ ] **Step 4: 重跑 focused target，确认 manifest 完整且 current/negative 文件没有交叉收集**
- [ ] **Step 5: 提交 `test: add target syntax reference fixture`**

### Task 2: 按基础层顺序开放 parser 与 semantic coverage

**Files:**

- Modify: parser AST/token/grammar 文件，以实施时模块清单为准
- Modify: canonical TypeRef、Place、CFG facts 和 borrow checker 文件
- Test: `tests/parser/test_syntax_reference_v1.c`
- Test: `tests/compiler/test_syntax_reference_semantics.c`

- [ ] **Step 1: 为一个尚未支持的 current-pass feature启用单一失败断言，记录 expected AST/bound kind**
- [ ] **Step 2: 运行 focused parser/semantic target，确认只失败该 feature**
- [ ] **Step 3: 从 TypeRef/Place/CFG foundation 实施最小通用能力，不加入 `Pixel/Span/Unique` 名字判断**
- [ ] **Step 4: 重跑当前 feature及全部已开放 feature，确认没有 AST kind或 contract 回退**
- [ ] **Step 5: 每完成一个独立 feature group提交一次，commit message使用 `feat(syntax): <feature id>`**

### Task 3: 实施 `zr.reflection` Type/TypeOf层级与ConstructibleType

**Files:**

- Modify: `zr_vm_core/include/zr_vm_core/reflection.h`
- Modify: `zr_vm_core/src/zr_vm_core/reflection*.c`
- Modify: reflection metadata/binder/runtime registration对应模块
- Test: `tests/core/test_reflection_create_instance.c`
- Test: `tests/compiler/test_syntax_reference_semantics.c`

- [ ] **Step 1: 写失败测试，锁定TypeOf<T>: Type、Class/Concrete/Instance/Struct/Interface精确分类和`typeof/typeid/resolve`职责**
- [ ] **Step 2: 写失败测试，锁定direct args、`object[]` spread、class result、boxed struct result、`@call`不参与，以及ref struct/resource/interface/abstract/open generic拒绝**
- [ ] **Step 3: 运行 focused reflection target，确认新测试按缺失 API/contract失败**
- [ ] **Step 4: 注册`zr.reflection`模块、member/meta query和`ConstructibleType.createInstance(...constructionArgs: object): object`，复用通用call spread和constructor metadata**
- [ ] **Step 5: 实施按 TypeId + argument runtime TypeIds + generation键控的 binder cache与失效**
- [ ] **Step 6: 运行 focused reflection、GC和ownership regression，确认 constructor失败时清理且 cache不保存对象**
- [ ] **Step 7: 提交 `feat(reflection): add createInstance variadic constructor binding`**

### Task 4: 实施 `zr.pooling` generational handle 与 guarded ref

**Files:**

- Create/Modify: `zr.pooling`标准库与runtime registration模块
- Modify: canonical TypeLayout `GcScanKind`/StableSlotSource contract
- Test: `tests/core/test_pooling_generational_handle.c`
- Test: `tests/projects/syntax_reference_v1/src/pooling.zr`

- [ ] **Step 1: 写失败测试锁定PoolId/slot/generation、wrong/stale/double recycle和ABA永久失效**
- [ ] **Step 2: 写失败测试锁定Try/out default view、reader/writer guard、recycle立即retire、active guard归零后Drop/reuse**
- [ ] **Step 3: 实施ordinary PoolHandle与ref-like PoolRef/PoolReadRef，不增加pool专用AST/opcode或type-name dispatch**
- [ ] **Step 4: 接通GcFree/GcMapped/GcBarriered slab、compact base+offset ref、barrier/card与resource Drop**
- [ ] **Step 5: 运行hot borrow/direct field/recycle churn和GC scan bytes基准，确认field访问不重复generation check**

### Task 5: 接通 VM、AOT、artifact 与 LSP

**Files:**

- Modify: VM/AOT lowering对应模块
- Modify: `.zrs/.zri/.zro` writer/reader和schema tests
- Modify: `zr_vm_language_server` semantic query/display模块
- Test: `tests/cmake/run_projects_suite.cmake`
- Test: `tests/language_server/test_lsp_syntax_reference_v1.c`

- [ ] **Step 1: 注册 interp、binary-first、aot_c和aot_llvm四条项目 case，先确认未接线时失败**
- [ ] **Step 2: 让四 backend消费同一 bound/SemIR contract并断言相同 checksum**
- [ ] **Step 3: 生成并 roundtrip `.zrs/.zri/.zro` golden，断言 source/binary public contract parity**
- [ ] **Step 4: 加入第 9 节 hover/signature/semantic-token/diagnostic/code-action golden**
- [ ] **Step 5: 运行 focused project、artifact和LSP targets，确认无 concrete type-name dispatch**
- [ ] **Step 6: 提交 `test(syntax): validate reference project across consumers`**

### Task 6: 收口 negative、migration 与文档

**Files:**

- Create: `tests/fixtures/projects/syntax_reference_v1/negative/*.zr`
- Create: `tests/fixtures/projects/syntax_reference_v1/golden/diagnostics.json`
- Modify: migration fixture/expected edit集合
- Modify: `docs/zr_language_specification.md`
- Modify: `docs/plans/syntax/README.md`

- [ ] **Step 1: 按第 7 节逐 case加入 primary diagnostic断言，先确认旧 generic diagnostic不满足 golden**
- [ ] **Step 2: 实施定向诊断和 migration AST edit，验证二次迁移幂等**
- [ ] **Step 3: 把 current文档代码块纳入 doc-test，把 legacy代码块标记为 migration input**
- [ ] **Step 4: 运行仓库 `%xxx`/`$` inventory，确认剩余命中只在 legacy、negative或历史计划排除项**
- [ ] **Step 5: 运行完整 parser/compiler/project/artifact/LSP/migration矩阵并记录准确通过数量**
- [ ] **Step 6: 提交 `docs(syntax): publish syntax reference v1`**

## 12. 晋级门

本文只有在以下条件全部满足后才能从“设计参考”改为“current reference”：

- 第 6 节 coverage manifest 的每个 feature id都有 current或negative证据，且没有未解释的 `surfacePending`。
- 正例项目在 interp、binary-first、AOT C、AOT LLVM输出相同结果。
- 所有负例具有稳定 primary diagnostic、精确 range和关联 origin range。
- reflection Type/TypeOf精确分类、member/meta query、direct args与数组spread、成功与失败、缓存与失效、GC compact与constructor throw均有测试。
- PoolHandle stale/ABA、PoolRef guard、retire/deferred reuse、GcFree NoScan和GcMapped/barrier均有测试。
- `.zrs/.zri/.zro` roundtrip和source/binary import contract一致。
- formatted/minified source在去除全部非必要newline后仍有相同AST/semantic hash；缺失任何required semicolon都会稳定失败。
- 每个static import都采用module-scope `let alias = import("literal");`，返回ModuleNamespace object且artifact dependency可roundtrip。
- LSP不从源码字符串重新推断 TypeRef、constructor、property、borrow或owner状态。
- shared foundations中没有按`Pixel`、`Span`、`Unique`、`TypeOf`、`PoolHandle`名字分支；reflection/pooling通过注册的runtime capability/service contract接入。
- 自定义 Drop和`using`的最终表层若仍未冻结，必须明确保留为未晋级项，不能把其他覆盖通过冒充“全语法完成”。

## 13. 参考依据

动态反射构造主要参考：

- .NET `Activator.CreateInstance(Type, params object?[]? args)`：`lua/runtime/src/libraries/System.Private.CoreLib/src/System/Activator.cs`。
- .NET constructor选择与调用：`lua/runtime/src/coreclr/System.Private.CoreLib/src/System/RuntimeType.CoreCLR.cs`、`lua/runtime/src/libraries/System.Private.CoreLib/src/System/Reflection/RuntimeConstructorInfo.cs`。
- .NET constructor cache和ref-like拒绝：`lua/runtime/src/coreclr/System.Private.CoreLib/src/System/RuntimeType.ActivatorCache.cs`、`lua/runtime/src/libraries/System.Runtime/tests/System.Runtime.Tests/System/ActivatorTests.cs`。
- Java显式 constructor reflection边界：`lua/jdk/src/java.base/share/classes/java/lang/reflect/Constructor.java`及`lua/jdk/test/jdk/java/lang/reflect`。

显式分号和module object import参考：

- JDK parser对local declaration、expression、return/throw/break/continue、package/import统一调用`accept(SEMI)`：`lua/jdk/src/jdk.compiler/share/classes/com/sun/tools/javac/parser/JavacParser.java`。ZR采用其“simple statement显式终止、block自行闭合”的核心，但允许braced declaration尾随分号作为可省略兼容形式。
- QuickJS的`js_parse_expect_semi`会在newline、`}`和EOF执行automatic insertion，相关`yield/await`边界测试位于`lua/QuickJS-master/tests/test_language.js::test_parse_semicolon`。ZR刻意拒绝该行为，三种位置都报告缺失`;`。
- Rust parser的`expect_semi`和missing-semicolon recovery位于`lua/rust/compiler/rustc_parse/src/parser/diagnostics.rs`，UI负例位于`lua/rust/tests/ui/parser/missing-semicolon.rs`。ZR复用精确插入诊断，但不采用“有无分号改变block tail value”的表达式语义。
- CPython从`sys.modules`缓存并返回module object的实现位于`lua/cpython/Python/import.c::PyImport_ImportModuleLevelObject`，循环与重复导入测试位于`lua/cpython/Lib/test/test_import`。QuickJS使用readonly module namespace exotic object，实现在`lua/QuickJS-master/quickjs.c`的`JS_CLASS_MODULE_NS`路径。ZR采用“静态依赖 + readonly namespace object”的共同核心。
- 仓库既有`%import`计划已经建立专用ImportExpression、string-literal路径、内部resolver和module object result：`.codex/plans/%import Reserved Syntax Migration Plan.md`。本设计只移除`%`并把表达式限制在module-scope immutable binding，不退化为普通函数名特判。

类型、借用、布局与所有权继续采用 01-06 已记录的 C#/.NET、Rust、CPython、QuickJS、Lua和JDK证据。ZR 的刻意差异是：静态 value/class/resource构造分别使用 `init/new/own`，运行时反射构造是显式普通 API，既不采用 CPython 的“调用 type 即构造”，也不允许 reflection绕过ref-like和ownership边界。

池化另外采用CPython arena/pool/size-class、.NET managed byref/Span和Rust pin/borrow/drop的共同核心：weak identity与active direct ref分离，地址/slot复用受guard保护。ZR的差异是PoolHandle可长期存储而PoolRef保持ref-like；通过一次validate换取hot access零重复generation检查。
