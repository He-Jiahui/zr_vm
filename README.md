# ZR 语言 Wiki

ZR 是一门面向模块化应用、原生互操作与多后端执行的静态语言。它把日常代码保持在自动 GC 的易用模型中，同时提供值类型、引用、连续内存、确定性资源管理和 AOT 友好的类型布局，供性能敏感场景按需使用。

这份 Wiki 介绍 ZR 的语言表层、类型与内存模型、模块系统和标准能力。它面向编写 `.zr` 源码的开发者；更精确的语法、诊断和实现状态请参阅文末链接的语言规范、设计文档与测试。

## 内容

- [程序、模块与导入](#程序模块与导入)
- [绑定、类型与解构](#绑定类型与解构)
- [函数与 callable](#函数与-callable)
- [类型、构造与属性](#类型构造与属性)
- [表达式与控制流](#表达式与控制流)
- [泛型、接口与 union](#泛型接口与-union)
- [引用、参数传递与确定赋值](#引用参数传递与确定赋值)
- [所有权、资源与 GC](#所有权资源与-gc)
- [编译期执行、元数据与声明生成](#编译期执行元数据与声明生成)
- [异步、Task、Job 与调度](#异步taskjob-与调度)
- [迭代器与 yield](#迭代器与-yield)
- [测试与 TestManifest](#测试与-testmanifest)
- [反射、池化与 native FFI](#反射池化与-native-ffi)
- [书写约定与延伸阅读](#书写约定与延伸阅读)

## 语言概览

ZR 的核心概念可以概括为四类：

- **Value**：一次计算得到的值，可以复制或移动，但不能直接写入。
- **Place**：可寻址的存储位置，例如局部变量、字段、数组元素或解引用结果。
- **Ref**：指向 Place 的非拥有引用，带有可写性与逃逸范围。
- **Owner**：负责资源生命周期的句柄，例如 `Unique<T>`、`Shared<T>` 和 `Weak<T>`。

普通 `class` 由精确 GC 管理。需要确定性释放的句柄、文件、图形资源或 native 资源使用 `resource class`，并通过所有权类型管理。语言会在编译期检查引用、移动、借用冲突、`out` 参数初始化和 ref-like 值逃逸；无法静态证明的边界，例如数组索引、动态 cast 或 native 输入，保留运行时检查。

## 程序、模块与导入

每个源文件以模块声明开始。模块级绑定使用 `let` 或 `var`；静态导入由 `import(...)` 表达式创建只读模块命名空间。

```zr
module examples.hello;

let system = import("zr.system");
let localConfig = import(".config");
let workspaceRenderer = import("engine.render");
let nativeRenderer = import("native:engine.render");
let localSdk = import("file:///E:/sdk/physics.zrp");

pub fn main(): int {
    let message: string = "hello, ZR";
    return 0;
}
```

导入规则：

- `import(...)` 只接受字符串字面量，并在编译时形成静态模块依赖。
- `"app.graphics"` 和 `"app/graphics"` 都表示分段模块路径。
- `".config"`、`"..shared.types"` 使用前导点表示相对模块路径。
- `"@math"` 表示第三方包根；`"@math.matrix"` 与 `"@math/matrix"` 表示其子模块。
- 项目别名使用单段 `#identifier`，例如 `import("#lib/tool")`。
- `native:engine.render` 显式选择已注册的 native module；它可与 workspace 的 `engine.render` 同名存在，但两者是不同的模块身份。
- `file:` URI 只负责定位 `.zr`、`.zrp`、`.zrm` 或项目目录；导入后的模块身份仍由目标的 module/project/artifact 声明决定。不要把裸 Windows、POSIX 或 UNC 路径作为 import literal。
- `zr.*` 是官方核心模块保留根，不能由 workspace、包、alias 或自定义 native provider 覆盖。
- 需要由运行时路径决定模块时，使用 `loadModule` 或 `loadPlugin`，不要把动态路径传给静态 `import(...)`。

标准库使用 `zr.xxx` 命名空间。例如 `zr.container`、`zr.iteration`、`zr.math`、`zr.system`、`zr.task`、`zr.thread`、`zr.debug`、`zr.reflection`、`zr.pooling` 和 `zr.ffi`。`zr.compile`、`zr.compile.declaration` 与 `zr.testing` 分别只存在于编译期或测试期，不能作为普通运行时依赖导入。

## 绑定、类型与解构

`let` 创建初始化后不可重新绑定的名称，`var` 创建可重新赋值的名称。两者都可以保存值、GC 对象句柄或所有权句柄；`let` 不代表对象图深度不可变。

```zr
let enabled: bool = true;
var count: int = 0;
let ratio: float = 0.5;
let name: string = "zr";
let letter: char = 'z';

let values: int[] = [1, 2, 3];
let pair: [int, string] = [1, "one"];
```

ZR 支持对象和数组解构。右侧表达式只求值一次；对象别名使用 `local: field` 形式。基础解构不依赖默认值、rest pattern 或嵌套 pattern。

```zr
let { width, height } = rectangle;
let { localWidth: width, localHeight: height } = rectangle;
let [first, second] = pair;
```

数组解构要求至少具有所需数量的元素，多余元素会被忽略，元素不足则失败。

常用类型形态如下：

```zr
let points: Point[];
let fixed: int[16];
let range: int[1..8];
let openRange: int[4..];
let mapping: Map<string, Point>;
let callback: fn(int, int) -> int;
```

## 函数与 callable

函数声明使用 `fn`。定义中的返回类型由 `:` 引出，函数类型使用 `->`，匿名函数的 expression body 使用 `=>`。

```zr
pub fn add(left: int, right: int): int {
    return left + right;
}

let increment = fn(value: int): int => value + 1;
let formatter: fn(string) -> string;
let factory: fn() -> fn(int) -> string;
```

函数体可以是 block，也可以是 bodyless 声明。block 由 `}` 闭合；interface、abstract 或 `native extern` 中没有函数体的声明必须以 `;` 结束。

```zr
interface Reader<T> {
    fn read(): T;
}

let transform = fn(value: int): string {
    return value.toString();
};
```

普通 instance `fn` 可以修改 receiver。`const fn` 表示只读 receiver，适合查询、比较和只读计算；只读 receiver 不能调用需要可写 receiver 的成员。

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

## 类型、构造与属性

### 类、struct 与 resource class

ZR 使用不同类型类别表达存储与生命周期：

| 类型 | 存储与生命周期 | 适用场景 |
|---|---|---|
| `class` | GC heap | 常规对象、可形成对象图的数据模型 |
| `struct` | 内联布局 | 小型值、数据记录、数组元素 |
| `readonly struct` | 内联布局且构造后字段不可替换 | 不变值对象 |
| `ref struct` | stack/ref-like storage | `Span<T>`、短期零拷贝视图 |
| `resource class` | ownership world | 文件、套接字、GPU/native 资源 |
| `interface` | 行为契约 | 多态抽象与泛型约束 |
| `enum` / `union` | 离散值或代数数据类型 | 状态、结果和模式匹配 |

`struct` 的字段直接进入布局。它不支持具体 struct 继承，复用应使用 interface、组合或泛型。`ref struct` 可以包含 `ref`，但不能进入 GC heap、box、普通数组、closure 或 suspension frame。

```zr
struct Point {
    pub var x: float;
    pub var y: float;
}

readonly struct Size {
    pub let width: int;
    pub let height: int;
}

class Document {
    pri var title: string;
}
```

### 构造

构造形式由目标类型类别决定，四条路径不互相回退：

```zr
let point = init Point(1.0, 2.0);       // struct value construction
let document = new Document();          // GC class construction
let file: Unique<FileHandle> = own FileHandle(); // resource construction
let result = callback(argument);        // ordinary call or @call
```

`init TypeRef(...)` 只构造静态可解析、具备 value-construction capability 的值类型。`TypeRef(...)` 是普通调用；它不会因为调用失败自动改为构造。运行时类型对象的动态构造通过反射库的 `ConstructibleType.createInstance(...)` 完成。

### 字段与属性

字段与 property 是不同概念。字段保存存储槽，property 定义访问器；concrete property 不会隐式生成 backing field。

```zr
class Meter {
    pri var _value: int;

    pub property value: int {
        pub get => _value;

        pub set {
            require(value >= 0);
            _value = value;
        }
    }
}
```

一个 property 至少包含一个 `get`、`set` 或 `init` accessor。`set` 与 `init` 在基础模型中互斥。ref-return property 只有 getter，它返回 Ref；对该 Ref 解引用后才成为可读写的 Place。

## 表达式与控制流

ZR 使用常见的算术、比较、逻辑、成员访问、索引、条件与调用表达式。

```zr
let total = price * quantity;
let label = total > 0 ? "active" : "empty";
let item = items[index];
let next = service.load(id).name;
```

对象与数组字面量：

```zr
let numbers = [1, 2, 3];
let record = { name: "zr", version: 1 };
let computed = { [key]: value };
```

条件、循环和异常处理：

```zr
if (count > 0) {
    process(count);
} else if (count == 0) {
    reset();
} else {
    fail();
}

for (var index: int = 0; index < values.length; index += 1) {
    consume(values[index]);
}

for (var item in values) {
    consume(item);
}

try {
    run();
} catch (error: RuntimeError) {
    report(error);
} finally {
    cleanup();
}
```

`return`、`throw`、`break` 和 `continue` 都是显式语句。语句结束由分号决定，不依赖换行。

### enum、union 与 switch

`enum` 适合固定的离散值；`union` 适合带载荷的变体，并支持基础穷尽性与不可达分支检查。

```zr
enum Color: int {
    Red;
    Green;
    Blue;
}

union Shape {
    Empty;
    Circle(radius: float);
    Rect { width: float; height: float; }
    @Fallback(message: string);
}

fn area(shape: Shape): float {
    switch (shape) {
        (Shape.Empty) {
            return 0.0;
        }
        (Shape.Circle(radius)) {
            return 3.14159 * radius * radius;
        }
        (Shape.Rect { width, height }) {
            return width * height;
        }
    }
}
```

## 泛型、接口与 union

泛型类型参数使用 `<...>`，约束通过 `where` 表达。接口可以为类型参数声明协变 `out` 或逆变 `in`；这与函数参数的 `in`/`out` contract 是不同概念。

```zr
pub fn identity<T>(value: T): T {
    return value;
}

pub struct FixedBuffer<T, const N: int> {
    var data: T[N];
}

pub interface Source<out T> {
    fn read(): T;
}

pub interface Sink<in T> {
    fn write(value: T): void;
}

pub fn create<T>(): T where T: class, new {
    return new T();
}
```

常见约束包括 `class`、`struct`、`new`、interface/类型名，以及由标准库暴露的能力约束。泛型代码应依赖约束的公开 contract，而不是根据具体类型名称分派。

## 引用、参数传递与确定赋值

引用类型描述对一个 Place 的访问能力：

```zr
ref T
ref readonly T
scoped ref T
```

- `ref T` 是可读写的非拥有引用。
- `ref readonly T` 是只读的非拥有引用。
- `scoped ref T` 限制引用不得逃出当前函数或受限区域。

函数参数使用 value、`in`、`ref`、`ref readonly`、`scoped ref` 或 `out` contract：

```zr
fn inspect(value: in Data): void {
    log(value);
}

fn update(value: ref Data): void {
    value.reset();
}

fn observe(value: ref readonly Data): int {
    return value.count();
}

fn tryCreate(result: out Data): bool {
    result = init Data();
    return true;
}

var data = init Data();
update(ref data);

var created: Data;
if (tryCreate(out created)) {
    inspect(created);
}
```

规则：

- `ref` 与 `out` 调用参数必须显式写出 marker，编译器不会通过 overload 猜测。
- `ref` 和 `ref readonly` 只能传入可寻址 Place，不能传入 rvalue。
- `in` 可以接受 Place 或临时值；临时值被物化为函数作用域 Place。
- `out` 参数在调用前可视为未初始化，函数必须在所有正常返回路径上赋值；调用后只在正常返回边上保证已初始化。
- 活动可写引用期间，重叠 Place 不能被并行读取、写入、移动或再次可写借用。

## 所有权、资源与 GC

普通 `class` 属于 GC world，适合日常对象图。`resource class` 属于 ownership world，用于必须确定性释放的资源。

```zr
class Document {
    pub var text: string;
}

resource class FileHandle {
    pub fn write(data: in ReadOnlySpan<byte>): void {
        // write to the underlying resource
    }
}

let document = new Document();
let file: Unique<FileHandle> = own FileHandle();
```

所有权类型：

| 类型 | 语义 |
|---|---|
| `Unique<T>` | 唯一 owner，按值传递会 move，离开作用域自动 drop |
| `Shared<T>` | 同一隔离域内的共享 owner，复制会保留资源 |
| `Weak<T>` | 不延长资源存活；通过 `wake(weak)` 留存 owner，或用 `.` / `?.` 访问目标 |

```zr
let file: Unique<FileHandle> = own FileHandle();
let shared: Shared<FileHandle> = share(file);
let weak: Weak<FileHandle> = degrade(shared);

let active = wake(weak);
if (active != null) {
    active.write(bytes);
}

drop(shared);
```

`Unique<T>` move 后，原 Place 不能再读取、借用、释放或再次转换。`Shared<T>` 默认不隐含跨线程原子引用计数；跨线程共享应使用明确的并发能力。普通路径不依赖运行时 use-after-move、double-drop 或借用检查。

GC 与 ownership world 通过显式 bridge 交互。资源对象不能偷偷持有普通 GC 对象；需要跨越边界时使用受约束的 `Gc<T>` bridge 或明确的 pin/copy/marshalling contract。

## 编译期执行、元数据与声明生成

编译期代码复用普通 `fn`、`if`、`struct`、attribute 和 `init`，不引入独立 macro 语言。使用 `comptime fn` 计算类型化常量，使用 `comptime if` 裁剪声明或语句，使用 `comptime { ... }` 执行检查和诊断。

```zr
let compile = import("zr.compile");

pub comptime fn tableSize(count: usize): usize {
    return count.nextPowerOfTwo();
}

const vertexStride: usize = sizeOf<Vertex>();

comptime if (compile.build.feature("simd")) {
    fn transform(values: Span<float>): void {
        simdTransform(values);
    }
} else {
    fn transform(values: Span<float>): void {
        scalarTransform(values);
    }
}

comptime {
    compile.assert(vertexStride == 32, "Vertex layout changed");
}
```

`comptime if` 的所有分支都会先被解析，只有活动分支参与名字解析、类型检查、借用检查和 lowering。predicate 只能读取已声明的 build feature 与规范 target facts，不能读取环境变量、时钟、随机数、网络或文件系统。模块常量仍使用 `const`；compiler-only 临时值使用 `comptime` 函数或 block 内的普通 `let`，不使用 `comptime let`。

attribute 是带已注册 metadata role 的 `readonly struct`。字段定义其编译期参数模式，retention 决定数据保留在 source、artifact 或 runtime reflection 中。

```zr
let reflection = import("zr.reflection");

#zr.reflection.attributeUsage(
    targets: reflection.AttributeTargets.field
        | reflection.AttributeTargets.property,
    retention: reflection.AttributeRetention.runtime,
    repeatable: false,
    inherited: false
)#
pub readonly struct Range {
    pub let min: int;
    pub let max: int;
}

class Player {
    #Range(min: 0, max: 100)#
    pri var _health: int = 100;
}
```

`#zr.compile.conditional("feature")#` 可以标记直接静态绑定、返回 `void` 的函数。对应 feature 关闭时，调用以及全部实参 lowering 一并移除，但调用本身仍会完成名字、访问与类型检查。需要裁剪声明时使用 `comptime if`。

类型化声明生成也是普通 `comptime fn`：它读取只读 declaration view，并返回 `declaration.Patch`。Patch 只能以 `GeneratedField`、`GeneratedMethod`、`GeneratedProperty` 等类型化数据新增声明；它不能读取或修改 token/source、修改已有成员，也不能递归触发新的 transform。

```zr
let declaration = import("zr.compile.declaration");

#zr.compile.declarationTransform#
pub comptime fn deriveMarker(
    target: declaration.Struct
): declaration.Patch {
    let marker = init declaration.GeneratedField(
        name: "_generatedEquality",
        type: typeid(bool),
        visibility: declaration.Visibility.private,
        mutability: declaration.Mutability.let,
        initializer: init declaration.ConstantValue(boolValue: true)
    );
    return init declaration.Patch(
        target: target.symbolId,
        additions: [marker]
    );
}
```

## 异步、Task、Job 与调度

异步函数使用 `async fn` 与 `await`，但返回类型必须显式写出 `zr.task.Task<T>`。调用 async 函数会立即执行到完成、抛错或第一个未完成的 `await`，因此 `Task<T>` 是 hot completion，不是延迟工作描述。

```zr
let task = import("zr.task");

async fn readConfig(path: string): task.Task<Config> {
    let bytes = await readAll(path);
    return parseConfig(bytes);
}

fn loadConfig(path: string): task.Task<Config> {
    return readConfig(path);
}

let loader: fn(string) -> task.Task<Config> = loadConfig;
```

`await expr` 接受 `Task<T>` 并产生 `T`。`Task<void>` 使用 `return;`。Task 必须被 `await`、`return` 或存入可证明的 consumer；第一版没有 `detach` 或 fire-and-forget 逃逸口。`in/ref/out` 参数、ref return、`ref struct`、`Span<T>`、`PoolRef<T>`、lock guard 和活动借用不能跨越 `await`。

需要延迟执行或指定调度位置时，构造 cold、non-Copy 的 `Job<T>`，再交给同一 `Scheduler` contract：

```zr
let task = import("zr.task");
let thread = import("zr.thread");

let checksumJob: task.Job<int> = init task.Job<int>(
    fn(): int => calculateChecksum()
);

let localResult: task.Task<int> =
    task.currentScheduler.schedule(checksumJob);

let workers = new thread.ThreadScheduler(workerCount: 4);
let compression: task.Job<Buffer<byte>> = init task.Job<Buffer<byte>>(
    fn(): Buffer<byte> => compress(input)
);
let compressed: task.Task<Buffer<byte>> = workers.schedule(compression);
```

`Job<T>` 按值消费，不能复制或重复 schedule。`task.currentScheduler`、`task.yieldNow()` 和 `task.delay(...)` 提供协作式调度入口。`ThreadScheduler` 是 Scheduler 的 provider，不会创建第二套 Task 类型；跨 isolate 的 capture 和结果必须满足 `Send`，普通 GC 对象、直接引用和 native pointer 默认不能跨越该边界。

## 迭代器与 yield

`yield` 不创建新的函数种类。包含 `yield` 的普通函数必须显式返回 `zr.iteration.Iterator<T>`；调用时只创建 cold state machine，首次推进才执行函数体。

```zr
let iteration = import("zr.iteration");

fn range(end: int): iteration.Iterator<int> {
    for (var index: int = 0; index < end; index += 1) {
        yield index;
    }
}

let values: iteration.Iterator<int> = range(4);
for (let value in values) {
    consume(value);
}
```

`yield expression;` 的值必须可赋给 iterator element type。含 `yield` 的函数只能使用 `return;` 结束序列，不能 `return expression;`。`Iterator<T>` 是 single-use、non-Copy 的 carrier；`for` 在正常结束、`break`、`return`、`throw` 等退出边上都会执行 cursor 的 Drop cleanup。

异步迭代显式返回 `AsyncIterator<T>`，并使用 `await for` 消费：

```zr
let iteration = import("zr.iteration");

async fn readEvents(source: EventSource): iteration.AsyncIterator<Event> {
    while (await source.hasNext()) {
        yield await source.readNext();
    }
}

await for (let event in readEvents(source)) {
    consume(event);
}
```

同步协议以 `Iterable<T>` 和 `Enumerator<T>` 表示，后者提供 `moveNext(): bool` 与 `current: ref readonly T`。异步迭代器的每次推进返回 `Task<bool>`，并在所有退出路径上执行 `close(): Task<void>`。`ref`、`ref struct`、`Span<T>`、`PoolRef<T>`、lock guard 与 scoped borrow 不能跨越 `yield`；高性能连续内存遍历应使用手写 `ref struct` enumerator，而不是 generator frame。

## 测试与 TestManifest

测试是普通 module-scope function 加上 `zr.testing` metadata，而不是 `test` 关键字、匿名测试 block 或隐藏 `main`。test build 从已绑定的 function 和 metadata 生成 `TestManifest`，runner 负责发现、隔离和报告；production build 在完整 type-check 后裁掉 test roots。

```zr
let testing = import("zr.testing");

#zr.testing.test#
fn parsesEmptyInput(): void {
    testing.assert(parse("").isEmpty(), "expected empty syntax tree");
}

#zr.testing.test#
#zr.testing.case(1, 2, 3)#
#zr.testing.case(-1, 1, 0)#
fn adds(left: int, right: int, expected: int): void {
    testing.equal(left + right, expected);
}
```

参数化测试的每个 `case` 参数都必须是 comptime constant。异步测试沿用普通 async 规则，显式返回 `task.Task<void>`：

```zr
#zr.testing.test#
async fn loadsFixture(): task.Task<void> {
    let document = await loadFixture();
    testing.assert(document.isValid(), "invalid fixture");
}

#zr.testing.test#
#zr.testing.skip(reason: "requires external service")#
fn importsRemoteSchema(): void {
    importRemoteSchema();
}
```

`skip` 保留测试的发现与报告，但仍会 parse、bind 和 type-check body。第一版的断言库只提供 `testing.assert`、`testing.equal` 和 `testing.throws<E>`；异步异常检查使用普通 `try/catch` 与 `testing.assert`。测试函数不能是 generic、member、local、lambda、native、comptime 或 bodyless function，也不能有 `in/ref/out` 参数、ref return 或非 `void` / `Task<void>` 返回类型。

## 反射、池化与 native FFI

### 反射

反射位于 `zr.reflection`。`typeid(TypeRef)` 提供轻量静态类型身份；`typeof(expr)` 对表达式求值一次并返回真实运行时类型描述符。

```zr
let reflection = import("zr.reflection");

let id = typeid(Point);
let descriptor = typeof(value);
let pointType = reflection.resolve(id);
```

反射类型提供字段、property、方法、构造器、metadata 与 layout 查询。动态构造属于 `ConstructibleType.createInstance(...constructionArgs: object)` 的显式 object boundary，不会回退到 `init`、`new`、`own` 或普通调用。

### 池化

`zr.pooling` 提供连续池化内存。`PoolHandle<T>` 是可以长期保存的 generational weak identity；`PoolRef<T>` 是验证后短期持有的 direct ref view。

```zr
let handle = particlePool.deliver(init Particle());

var particle: PoolRef<Particle>;
if (particlePool.tryBorrow(handle, out particle)) {
    particle.value.position.x += 1.0;
}

particlePool.recycle(handle);
```

回收会立刻使旧 handle 失效，但 slot 必须等待活动 `PoolRef<T>` guard 结束后才能复用。`PoolRef<T>` 是 ref-like 值，不能保存到 heap、普通容器、closure 或 async frame。

### Native FFI

静态 native binding 使用 `native extern`。语言 callable contract 与 ABI `FfiSignature` 在绑定期形成，VM 与 AOT 共享同一份 contract。

```zr
native extern("zr_native_math") {
    #zr.ffi.entry("zr_add_i32")#
    #zr.ffi.callingConvention("c")#
    pub fn add(left: i32, right: i32): i32;
}
```

native block 中只包含 bodyless function、delegate 或 type mapping 声明。`in`、`ref` 与 `out` 同时参与语言借用规则和 ABI direction；GC reference、ref struct、Span、resource/owner 类型只有在存在明确 marshaller、pin 或 copy contract 时才能跨越 ABI。

## 书写约定与延伸阅读

ZR 不采用自动分号插入：局部/字段/模块绑定、表达式、赋值、`return`、`throw`、`break`、`continue` 与 bodyless declaration 都要显式写 `;`。由 `{ ... }` 闭合的 type、function、property 等声明通常不需要尾随分号。

新代码遵循以下约定：

- 使用 `module`、`import(...)`、`fn`、`async fn`、`await`、`comptime`、`yield`、`native extern` 与 `#zr.testing.test#`。
- 使用 `fn name(...): R` 定义函数，使用 `fn(A) -> R` 表达 callable 类型。
- 使用 `init T(...)`、`new C(...)` 与 `own R(...)` 分别构造值类型、GC class 和 resource class。
- 使用 `ref`、`out`、`in`、`scoped`、`readonly` 表达引用与参数 contract。
- 使用 `Unique<T>`、`Shared<T>`、`Weak<T>` 与 `share/degrade/wake/intoGc/drop` 管理确定性资源；`.` / `?.` 只访问对象目标。
- 为 async 函数、迭代函数和异步迭代函数显式写出 `Task<T>`、`Iterator<T>` 或 `AsyncIterator<T>` 返回 TypeRef。

深入阅读：

- [语言规范](docs/zr_language_specification.md)
- [语法与内存模型设计索引](docs/plans/syntax/README.md)
- [编译期执行、元数据与声明生成](docs/plans/syntax/2026-07-20-11-compile-time-attribute-decorator-typed-generation-design.md)
- [Task、Job 与调度](docs/plans/syntax/2026-07-20-12-async-task-job-scheduler-design.md)
- [迭代器与 yield](docs/plans/syntax/2026-07-20-13-iterator-enumerator-yield-design.md)
- [测试与 TestManifest](docs/plans/syntax/2026-07-20-14-test-function-harness-design.md)
- [模块与项目文档](docs/module-system/index.md)
- [AOT 计划](docs/plans/aot/index.md)
- [测试目录](tests/)

当 Wiki、语言规范、实现和测试的细节不一致时，以已确认的子设计、当前语言规范和已启用测试为准；未完成的语法能力不应仅凭示例视为已经可用于所有后端。
