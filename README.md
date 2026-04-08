# ZrVM

> 本页按 2026-04-06 当前仓库里的文档、fixtures、测试用例和已跑通的验证结果整理，目标是把 **zr 语言当前已经落地的能力** 收敛到一个根目录总览里。下面优先写“仓库里已经有直接证据”的特性；对仍然只有语法面、索引面或局部运行面证据的点，会单独标明边界。

## 当前已验证的能力范围

- 基本表达式、局部变量、全局变量、顶层函数、入口模块顶层 `return`
- `class / interface / struct / enum`
- 字段、成员方法、静态字段、静态方法、getter / setter 属性
- `lambda`、立即执行闭包、逃逸闭包
- 生命周期 / 调用 / 运算符 / 转换 / 下标类元方法
- `%module`、`%import`、`%test`、`%compileTime`、`%extern`、`%type`
- `%async / %await`、`zr.task`、`zr.coroutine`、`zr.thread`
- `%owned / %unique / %shared / %weak / %using / %upgrade / %release / %borrowed`
- `zr.system`、`zr.network`、`zr.container`、`zr.math`、`zr.ffi`
- FFI 外部函数 / struct / enum / delegate 声明，含 callback
- GC、字符串异常、`Error` 派生异常、`try / catch / finally`
- 固定长度数组、容器变长数组、`Map`、`Set`
- 泛型、`const` 泛型、`%in / %out / %ref` 参数模式、`%type` 反射元数据

## 1. 基本语言特性

### 1.1 算数运算、局部变量、全局变量、全局方法

```zr
%module "core_types";

pub var globalSeed: int = 5;

add(a: int, b: int): int {
    var total = a + b;
    var scaled = total * 3;
    var shifted = (scaled - 4) << 1;
    return shifted / 2;
}

return add(globalSeed, 3);
```

常见基础能力已经有仓库级证据：

- `+ - * / %`
- 位运算和位移
- `<int> expr` 这类显式转换
- 顶层 `var`
- 顶层函数
- 顶层 `return` 作为入口模块返回值

函数声明当前默认直接写 `name(...) { ... }`；`func name(...) { ... }` 仍兼容，但不是必需关键字。

### 1.2 入口写法

zr 当前大量项目 fixture 都直接把入口逻辑写在 `main.zr` 顶层，而不是强制要求 `main()`：

```zr
var core = %import("core_types");
var meta = %import("meta_surface");

return core.coreScore() + meta.metaScore();
```

## 2. 类型系统与面向对象

### 2.1 class、field、getter、setter、member method、static method

```zr
class Accumulator {
    pri var _value: int = 0;
    pub static var created: int = 0;

    pub @constructor(start: int) {
        this._value = start;
        Accumulator.created = Accumulator.created + 1;
    }

    pub get value: int {
        return this._value;
    }

    pub set value(next: int) {
        this._value = next;
    }

    pub pulse(delta: int): int {
        this.value = this.value + delta;
        return this.value;
    }

    pub static identity(value: int): int {
        return value;
    }
}

var counter = new Accumulator(10);
counter.value = counter.pulse(4);
return counter.value + Accumulator.identity(1);
```

这段覆盖了：

- `field`
- `getter / setter`
- 普通成员方法
- 静态字段
- 静态方法
- `class` 实例化 `new Type(...)`

### 2.2 interface

```zr
interface Measure<T> {
    read(): T;
}

class Meter: Measure<int> {
    pri var value: int = 0;

    pub @constructor(seed: int) {
        this.value = seed;
    }

    pub read(): int {
        return this.value;
    }
}
```

### 2.3 struct 值类型实例化

```zr
struct Vector2 {
    pub var x: int;
    pub var y: int;

    pub @constructor(x: int, y: int) {
        this.x = x;
        this.y = y;
    }
}

var point = $Vector2(8, 9);
return point.x + point.y;
```

当前仓库里的 `struct` 值类型写法重点是：

- 声明时和 `class` 类似
- 值构造使用 `$Type(...)`
- 适合做轻量值语义聚合

### 2.4 enum

```zr
enum Mode: int {
    Idle = 2;
    Warm = 5;
    Hot = 7;
}

var mode = Mode.Hot;
return <int> mode;
```

### 2.5 额外已验证的泛型 / const 泛型 / 约束

```zr
interface IProducer<out T> where T: class, new() {
    next(): T;
}

class Matrix<T, const N: int> {
    pub var rows: Array<T>[N];

    pub @constructor() {
    }
}

identity<T>(value: T): T {
    return value;
}

var matrix = new Matrix<int, 4>();
return identity(matrix);
```

### 2.6 类型声明规范 v2：`%func`、箭头兼容、类型对象化与别名

当前类型位遵循统一顺序：`前缀 % 保留字修饰 + 主类型 + 后缀修饰`。主类型可以是命名类型、泛型类型、元组类型、分组类型、`%func(...) -> Type`。

```zr
var ownedName: %borrowed string;
var fixed: int[4];
var buckets: Map<string, Array<int>>;
var callbacks: %shared (%func(int)->string)[];
```

函数类型正式表面语法统一为 `%func(...) -> ...`；parser 在 `%func` 和 lambda 两处都兼容 `=>`，但规范写法、反射输出、文档示例统一使用 `->`：

```zr
var canonical: %func(int)->int = (x: int)->{
    return x;
};

var compat: %func(int)=>int = (x: int)=>{
    return x;
};
```

类型本身也可以对象化为 `Type` 值，并且可直接复用普通绑定模型做类型别名：

```zr
var funcType = %type(%func(int)->int);

var F = %func(int)->int;
var callback: F = (x: int)->{
    return x;
};

return funcType.kind == "function" ? callback(7) : 0;
```

补充规则：

- `func` 关键字继续兼容，但只是函数声明的可选显式写法；默认直接写 `name(...) { ... }`
- `%async run(): int` 与 `%async run(): %async int` 都兼容，但文档和反射优先展示显式 `%async T`
- 类型值别名必须是编译期可折叠且冻结的 `Type` 值；名字与正式类型声明冲突时直接报错

## 3. Lambda 与闭包

### 3.1 立即执行闭包

```zr
var immediateBase = 7;
var immediateScore = ((delta: int)->{
    return immediateBase + delta;
})(5);

return immediateScore;
```

### 3.2 函数退出后仍然存活的闭包引用

```zr
makeEscapingCallback(offset: int) {
    var callback = (value: int)->{
        return value + offset;
    };
    return callback;
}

var escaped = makeEscapingCallback(8);
return escaped(6);
```

### 3.3 逃逸到回调句柄的闭包

这个写法适合“函数退出后，闭包被事件系统 / FFI / 宿主持有”的场景：

```zr
%extern("ffi_fixture") {
    delegate Unary(value: f64): f64;

    #zr.ffi.entry("zr_ffi_apply_callback")#
    Apply(value: f64, cb: Unary): f64;
}

var ffi = %import("zr.ffi");
var captured = 3.0;

var cb = ffi.callback(Unary, (value)->{
    return value + captured;
});

return Apply(5.0, cb);
```

## 4. 元方法与运算符覆盖面

### 4.1 当前已索引 / 已实现的元方法种类

按当前编译器/LSP 元数据，仓库里已经能识别这些元方法名字：

- 生命周期：`@constructor`、`@destructor`、`@close`
- 算数 / 运算符：`@add`、`@sub`、`@mul`、`@div`、`@mod`、`@pow`、`@neg`
- 比较 / 位运算：`@compare`、`@shiftLeft`、`@shiftRight`、`@bitAnd`、`@bitOr`、`@bitXor`、`@bitNot`
- 转换：`@toBool`、`@toString`、`@toInt`、`@toUInt`、`@toFloat`
- 调用 / 访问：`@call`、`@getter`、`@setter`、`@getItem`、`@setItem`
- 装饰器钩子：`@decorate`

### 4.2 生命周期、调用、属性、下标、运算符示例

```zr
class LifecycleHandle {
    pub static var closeCount: int = 0;
    pub static var destroyCount: int = 0;

    pub @constructor() {
    }

    pub @close() {
        LifecycleHandle.closeCount = LifecycleHandle.closeCount + 1;
    }

    pub @destructor() {
        LifecycleHandle.destroyCount = LifecycleHandle.destroyCount + 1;
    }
}

class Meter {
    pri var raw: int = 0;

    pub @constructor(start: int) {
        this.raw = start;
    }

    pub get value: int {
        return this.raw + 1;
    }

    pub set value(next: int) {
        this.raw = next + 2;
    }

    pub @call(delta: int): int {
        this.value = this.value + delta;
        return this.value;
    }
}

struct Vector2 {
    pub var x: int;
    pub var y: int;

    pub @constructor(x: int, y: int) {
        this.x = x;
        this.y = y;
    }

    pub static @add(left: Vector2, right: Vector2): Vector2 {
        return $Vector2(left.x + right.x, left.y + right.y);
    }

    pub static @compare(left: Vector2, right: Vector2): int {
        return (left.x + left.y) - (right.x + right.y);
    }

    pub @toString(): string {
        return "Vector2";
    }

    pub @getItem(index: int): int {
        return index == 0 ? this.x : this.y;
    }

    pub @setItem(index: int, value: int): void {
        if (index == 0) {
            this.x = value;
        } else {
            this.y = value;
        }
    }
}

var meter = new Meter(4);
var a = meter.value;
meter.value = 6;
var b = meter(3);
var v = $Vector2(1, 2);
v[0] = 8;
return a + b + v[0];
```

说明：

- 源码里的属性写法是 `get name` / `set name`；底层元数据面对应 getter / setter slot
- `@call` 已有直接运行时证据
- `@close` 会参与 `%using` 的自动释放路径

## 5. 模块、反射、编译期与 `%` 保留字

### 5.1 模块与导入

```zr
%module "core_types";

var system = %import("zr.system");
var math = %import("zr.math");
var container = %import("zr.container");
```

### 5.2 `%type` 反射 / 元数据

```zr
class User {
    pub var id: int = 1;
}

var info = %type(User);
var members = info.members;
var meta = info.metadata;
return members.id[0];
```

在当前仓库里，`%type(...)` 已经被用于：

- runtime decorator 元数据读取
- compile-time decorator 元数据读取
- 参数 / 成员 / 类型反射

### 5.3 `%compileTime` 编译时变量、函数、代码块

```zr
%compileTime var MAX_SIZE = 100;

%compileTime validateArraySize(size: int): bool {
    return size > 0;
}

%compileTime {
    var internalSeed = 5;
}

var fixed: int[MAX_SIZE];
var validated: int[validateArraySize(50) ? 50 : 10];
```

当前仓库已经有直接证据表明 `%compileTime` 支持：

- 编译期变量
- 编译期函数
- 编译期递归
- 编译期代码块
- 编译期表达式直接投影到运行时初始化
- 与命名参数 / 默认参数联动

### 5.4 `%test` 分阶段测试

一个模块里可以拆多个命名 `%test`，把不同语义域分阶段验证：

```zr
%test("core_types_score") {
    return coreScore();
}

%test("core_projection_score") {
    return coreProjection;
}

%test("exception_score") {
    return exceptionScore();
}
```

这也是当前仓库很多 fixture 的组织方式：基础类型、元方法、装饰器、并发、异常分别独立测试。

### 5.5 `%` 保留字现状总表

| 关键字 | 当前状态 | 最小写法 |
| --- | --- | --- |
| `%module` | 已验证 | `%module "core_types";` |
| `%import` | 已验证 | `var system = %import("zr.system");` |
| `%test` | 已验证 | `%test("core") { return 1; }` |
| `%compileTime` | 已验证 | `%compileTime var MAX = 4;` |
| `%extern` | 已验证 | `%extern("ffi_fixture") { Add(lhs:i32, rhs:i32): i32; }` |
| `%type` | 已验证 | `var meta = %type(User).metadata;`, `var funcType = %type(%func(int)->int);` |
| `%async` | 已验证 | `%async run(): %async int { ... }`，兼容 `%async run(): int { ... }` |
| `%await` | 已验证 | `return %await task;` |
| `%owned` | 已验证 | `%owned class PointSet {}` |
| `%unique` | 已验证 | `return %unique new PointSet();` |
| `%shared` | 已验证 | `var shared = %shared(owner);` |
| `%weak` | 已验证 | `var weak = %weak(shared);` |
| `%using` | 已验证 | `var owner = %using new Holder();` |
| `%upgrade` | 已验证 | `var upgraded = %upgrade(weak);` |
| `%release` | 已验证 | `var released = %release(shared);` |
| `%in` | 已验证 | `keep(%in value: int): int { ... }` |
| `%out` | 已验证 | `fill(%out value: int): void { ... }` |
| `%ref` | 已验证 | `swap(%ref left: T, %ref right: T)` |
| `%borrowed` | 已验证 | `%async f(value: %borrowed string): %async string { ... }` |
| `%mutex` | 当前拒绝 | 当前测试明确要求报错 |
| `%atomic` | 当前拒绝 | 当前测试明确要求报错 |

## 6. 装饰器

### 6.1 运行时装饰器 `#decorator#`

```zr
markClass(target: %type Class): void {
    target.metadata.runtimeSerializable = true;
}

markFunction(target: %type Function): void {
    target.metadata.instrumented = true;
}

#markClass#
class User {
}

#markFunction#
decoratedBonus(): int {
    return %type(decoratedBonus).metadata.instrumented ? 1 : 0;
}
```

当前仓库对运行时装饰器已有 class / field / method / property / function 级使用证据。

### 6.2 编译时装饰器 `#decorator#`

```zr
%compileTime class Serializable {
    pri var marker: int = 15;

    @constructor(marker: int = 15) {
        this.marker = marker;
    }

    @decorate(target: %type Class): DecoratorPatch {
        return {
            metadata: {
                serializable: this.marker
            }
        };
    }
}

%compileTime class MarkParameter {
    @decorate(target: %type Parameter): DecoratorPatch {
        return { metadata: { parameterTag: 62 } };
    }
}

%compileTime markFunction(target: %type Function, bonus: int = 17): DecoratorPatch {
    return { metadata: { instrumented: bonus } };
}

#Serializable()#
class CompileTimeUser {
}

#markFunction#
decoratedBonus(#MarkParameter# value: int = 1): int {
    return %type(decoratedBonus).metadata.instrumented;
}
```

这部分覆盖了：

- 编译时 class decorator
- 编译时 function decorator
- 编译时 parameter decorator
- `@decorate(target: %type X): DecoratorPatch`

## 7. 容器、定长数组、native 库类型推断

### 7.1 定长数组

```zr
var fixed: int[4] = [1, 2, 3, 4];

summarizeFixed(values: int[4]): int {
    var total = 0;
    for (var item in values) {
        total = total + <int> item;
    }
    return total;
}

var runtimeFixed: int[];
makeRuntimeFixed(): int[]{
	var arr = new int[20];
	return arr;
}

return summarizeFixed(fixed);
```

### 7.2 编译期决定长度的数组

```zr
%compileTime var ROWS = 4;
var matrixRow: int[ROWS];
```

### 7.3 `zr.container` 变长数组、Map、Set

```zr
var container = %import("zr.container");

var list = new container.LinkedList<int>();
list.addLast(7);
list.addLast(8);

var arr = new container.Array<int>();
arr.add(6);
arr.add(11);

var left = new container.Pair<int, string>(1, "a");
var right = new container.Pair<int, string>(2, "b");

var seen = new container.Set<Pair<int, string>>();
seen.add(left);
seen.add(right);

var buckets = new container.Map<string, Array<int>>();
buckets["sum"] = arr;

var fetched: Array<int> = buckets["sum"];
return fetched[0] + list.first.value + seen.count;
```

这类写法同时体现了当前仓库已经具备的：

- native / builtin 模块导出类型能进入类型推断
- 泛型容器闭型在导入后可直接用 `new container.Map<string, Array<int>>()`
- `Array<int>` 这类 builtin 类型可在注解和推断之间来回流动

## 8. native call、system、network、ffi、gc

### 8.1 native call

直接调用 native 模块导出：

```zr
var math = %import("zr.math");
var direct = math.Matrix4x4.scale(2.0, 3.0, 4.0);
return direct.m11 + direct.m22;
```

按名字动态调用模块导出：

```zr
var system = %import("zr.system");
var byName = system.vm.callModuleExport("zr.math", "sqrt", [4.0]);
return byName;
```

### 8.2 system

```zr
var system = %import("zr.system");

system.console.printLine("hello");
system.gc.stop();
system.gc.step();
system.gc.collect();
system.gc.start();
```

### 8.3 network

```zr
var network = %import("zr.network");

var listener = network.tcp.listen("127.0.0.1", 0);
var client = network.tcp.connect("127.0.0.1", listener.port());
var server = listener.accept(3000);

var wrotePing = client.write("ping");
var readPing = server.read(16, 3000);

var socket = network.udp.bind("127.0.0.1", 0);
var sentEcho = socket.send("127.0.0.1", socket.port(), "echo");
var packet = socket.receive(16, 3000);

server.close();
client.close();
listener.close();
socket.close();
```

### 8.4 FFI 调用

最小 `%extern` 函数声明：

```zr
%extern("ffi_fixture") {
    #zr.ffi.entry("zr_ffi_add_i32")#
    Add(lhs: i32, rhs: i32): i32;
}

return Add(7, 5);
```

带 `delegate` 的 callback 例子：

```zr
%extern("ffi_fixture") {
    delegate Unary(value: f64): f64;

    #zr.ffi.entry("zr_ffi_apply_callback")#
    Apply(value: f64, cb: Unary): f64;
}

var ffi = %import("zr.ffi");
var cb = ffi.callback(Unary, (value)->{
    return value * 2.0;
});

return Apply(5.0, cb);
```

当前 `%extern` 公开面还有：

- extern `struct`
- extern `enum`
- extern `delegate`
- `#zr.ffi.callconv(...)#`
- `#zr.ffi.charset(...)#`
- `#zr.ffi.pack(...)#`
- `#zr.ffi.align(...)#`
- `#zr.ffi.offset(...)#`
- `#zr.ffi.value(...)#`
- `#zr.ffi.underlying(...)#`
- 参数级 `#zr.ffi.in# / #zr.ffi.out# / #zr.ffi.inout#`

## 9. ownership、弱引用、协程、任务、多线程

### 9.1 `%using`、`%shared`、`%weak`、`%upgrade`、`%release`

```zr
class Holder {
}

var owner = %using new Holder();
var shared = %shared(owner);
var weak = %weak(shared);
var upgraded = %upgrade(weak);
var releasedShared = %release(shared);
var releasedUpgrade = %release(upgraded);
var after = %upgrade(weak);

if (owner == null && releasedShared == null && releasedUpgrade == null && after == null) {
    return 1;
}
return 0;
```

### 9.2 `%owned`、`%unique`、`%borrowed`

```zr
%owned class PointSet {
    pub @constructor() {
    }
}

takeFromPool(): %unique PointSet {
    return %unique new PointSet();
}

%async valid(value: %borrowed string): %async string {
    return value;
}
```

### 9.3 task 与 `%async / %await`

当前 parser 已兼容两种 async 返回标注：

- `%async run(): int`
- `%async run(): %async int`

两种写法都会归一到同一个底层 runner 语义；用户面写法和展示优先使用显式 `%async T`，`TaskRunner<T>` 只作为实现名出现。

```zr
%async addOne(value: int): %async int {
    return value + 1;
}

%async run(): int {
    var task = addOne(9).start();
    return %await task;
}

return %await run().start();
```

### 9.4 coroutine 手动 pump

```zr
var coroutine = %import("zr.coroutine");

%async addOne(value: int): %async int {
    return value + 1;
}

coroutine.coroutineScheduler.setAutoCoroutine(false);
var task = coroutine.start(addOne(6));
coroutine.coroutineScheduler.pump();
return %await task;
```

### 9.5 多线程与 `zr.thread`

```zr
var thread = %import("zr.thread");

%async addOne(value: int): %async int {
    return value + 1;
}

%async run(): int {
    var worker = thread.spawnThread();
    var task = worker.start(addOne(4));
    return %await task;
}

return %await run().start();
```

### 9.6 跨线程句柄：`Channel<T>`、`Shared<T>`、`WeakShared<T>`

```zr
var thread = %import("zr.thread");

%async run(): int {
    var worker = thread.spawnThread();
    var channel = new thread.Channel<int>();
    var shared = new thread.Shared<int>(7);
    var weak = shared.downgrade();

    %async readWeak(): %async int {
        var upgraded = weak.upgrade();
        if (upgraded == null) {
            return 0;
        }
        channel.send(upgraded.load());
        return 1;
    }

    var task = worker.start(readWeak());
    if (%await task != 1) {
        return 0;
    }
    return channel.recv();
}

return %await run().start();
```

## 10. 异常、抛出、捕获

### 10.1 抛出非 `Error` 类型异常

当前仓库已经验证：直接 `throw "boom"` 这类字符串异常会被装箱成可捕获异常对象。

```zr
try {
    throw "boom";
} catch (e) {
    return e.message == "boom" ? 1 : 0;
}
```

### 10.2 抛出 `Error` 派生异常

异常模块当前走 `zr.system.exception`：

```zr
var exception = %import("zr.system.exception");

try {
    throw $exception.RuntimeError("typed");
} catch (e: RuntimeError) {
    return 1;
} catch (e: Error) {
    return 2;
}
```

### 10.3 `try / catch / finally`

```zr
var marker = 0;

try {
    try {
        throw "boom";
    } finally {
        marker = 2;
    }
} catch (e) {
    return marker + 3;
}

return 0;
```

当前异常测试已经覆盖：

- `throw` 字符串
- `throw RuntimeError`
- `catch` 顺序分派
- `finally` 在正常返回 / 异常路径下都执行
- `%test` 内异常语义与普通运行一致

## 11. 参数模式

```zr
swap<T>(%ref left: T, %ref right: T): T {
    var temp = left;
    left = right;
    right = temp;
    return left;
}

fill(%out value: int): void {
    value = 4;
}

keep(%in value: int): int {
    return value + 1;
}
```

这部分已经在当前 gauntlet fixture 里出现。

## 12. 额外已出现的特性

除了你点名的能力，当前仓库里还能明确看到这些已经接入的语言面：

- 泛型类、泛型接口、泛型函数
- `const` 泛型与定长数组尺寸联动
- `where` 约束
- `pub / pri` 可见性
- `for (var item in collection)` 迭代
- 命名参数、默认参数、编译期命名参数投影
- `%type(...)` 反射读取成员与 metadata
- field-scoped `%using var field: Type;` 生命周期标记

field-scoped `%using` 例子：

```zr
struct HandleBox {
    %using var handle: %unique Resource;
}

class Holder {
    %using var resource: %shared Resource;
}
```

## 13. 当前边界与重点验证结论

这部分很重要，避免把“已有语法面”误写成“100% 运行时闭环”：

1. 当前异常模块路径是 `zr.system.exception`，不是 `zr.exception`。
2. 当前文档 / LSP /编译器索引里的生命周期元方法标准名是 `@destructor`，不是 `@deconstructor`。
3. REPL 探针里 `@call` 已有直接正例；`@constructor`、`@destructor` 也已有直接正例。
4. `@add`、`@sub`、`@mul` 等算数元方法当前已确认“名字面可索引、声明面可写”，但 `@add` 直连 `+` 运算符的运行时路径仍需要更强的回归覆盖，README 这里只给声明写法，不把它表述成“已完全打通的运算符重载体系”。
5. 定长数组当前有强证据的是“字面量长度、编译期表达式长度、`const` 泛型长度”；没有足够证据证明“运行期才决定的定长数组长度”已经是稳定正向特性。
6. `%mutex` 和 `%atomic` 当前不是可用正特性；现有任务/线程测试明确要求它们被拒绝。
7. `@close` 与 `%using` 的资源释放路径已有直接文档和运行时证据；这比泛化声称“所有析构语义都完整落地”要更稳妥。

## 14. 证据入口

如果后续继续补 README，可优先从这些文件继续向下核对：

- `docs/zr_language_specification.md`
- `docs/library-and-builtins/zr-task-runtime.md`
- `docs/library-and-builtins/zr-coroutine-runtime.md`
- `docs/library-and-builtins/zr-thread-runtime.md`
- `docs/library-and-builtins/zr-system-submodules.md`
- `docs/parser-and-semantics/ffi-extern-declarations.md`
- `docs/parser-and-semantics/owned-field-lifecycle.md`
- `docs/parser-and-semantics/ownership-builtins-semir-aot.md`
- `tests/fixtures/projects/language_feature_gauntlet/src/*.zr`
- `tests/projects/test_language_feature_gauntlet.c`
- `tests/exceptions/test_exceptions.c`
- `tests/task/test_task_runtime.c`
- `tests/thread/test_thread_runtime.c`
