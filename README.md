# ZrVM 语法介绍

更新日期: 2026-06-24

本文是 ZR 语言当前语法面的入口文档，面向正在编写 `.zr` 源码的人。内容按现行解析器、语言规范、module/import 规则、using 规则和已有测试入口整理。

需要先明确一个边界: 本文介绍的是当前推荐写法和解析器已经承认的语法表面，不等同于承诺所有语法都已经在解释器、二进制后端和 AOT 后端中完整闭环。遇到实现状态问题时，以仓库中的计划文档、测试和当前后端能力为准。

## 基本原则

ZR 的语法目前遵循这些规则:

- 顶层文件必须声明 `%module`。
- 导入使用 `%import`，项目内显式相对导入只使用前导点语法。
- 函数声明推荐直接写 `name(...) { ... }`，旧的 `func name(...)` 只作为兼容输入。
- 顶层测试使用 `%test("name") { ... }`。
- 资源和模式守卫使用 `using`，不要再使用字段级 `%using`。
- 所有权类型推荐使用 `Unique<T>`、`Shared<T>`、`Weak<T>`、`Borrow<T>`、`Loan<T>`。
- 函数类型推荐使用 `%func(...) -> T`，`=>` 只作为兼容输入。
- `%async`、`%await`、`%compileTime`、`%extern`、`%type` 是当前保留的编译器指令语法。

一个最小文件:

```zr
%module "examples.hello";

pub main(): int {
    var message: string = "hello";
    return 0;
}
```

## 文件与模块

每个 `.zr` 文件应以 `%module` 开头:

```zr
%module "app.main";
%module("app.main");
%module app.main;
```

三种形式都会被规范化为模块路径。旧的裸 `module app.main;` 不再是推荐语法，当前规则要求写 `%module`。

导入使用 `%import`:

```zr
%import("zr.system");
%import("app.math");
%import app.math;
```

项目内相对导入使用前导点:

```zr
%import(".local");
%import("..shared.types");
%import("...root.feature");
```

导入规则要点:

- `"zr.system"`、`"app.math"` 这类裸模块名保持绝对模块名。
- `".local"` 表示当前模块目录下的 `local`。
- `"..shared.types"` 表示上一级模块目录下的 `shared.types`。
- 更多前导点表示继续向上。
- `.zrp.pathAliases` 可提供 `@alias` 和 `@alias.foo.bar` 形式的别名导入。
- 不使用 `"./x"`、`"../x"`、裸 `"."`、裸 `".."`、`"@alias/x"`。
- 显式 `%module` 与 sourceRoot 推导出的模块路径不一致时，项目导入校验会报告不匹配。

## 声明

ZR 支持这些顶层声明:

```zr
%module "examples.declarations";

pub var version: int = 1;

pub add(left: int, right: int): int {
    return left + right;
}

pub struct Point {
    var x: float;
    var y: float;
}

pub class Counter {
    var value: int = 0;

    @constructor(start: int = 0) {
        this.value = start;
    }

    pub add(step: int): int {
        this.value += step;
        return this.value;
    }
}

pub interface Reader<out T> {
    read(): T;
}

pub enum Color: int {
    Red;
    Green;
    Blue;
}
```

可见性修饰符:

```zr
pub api(): void {}
pri helper(): void {}
pro inheritedHook(): void {}
```

未写可见性时默认私有。`class`、`struct`、`interface`、`enum`、`union` 以及成员声明都可以出现在模块中。`abstract`、`virtual`、`override`、`final`、`shadow` 是当前保留的声明修饰符，具体可用位置由声明种类决定。

## 变量

变量声明使用 `var`:

```zr
var count: int = 0;
var name = "zr";
var const limit: int = 10;
```

解构声明:

```zr
var [first, second] = pair;
var {width, height} = rect;
var {localWidth: width, localHeight: height} = rect;
```

语句通常需要分号。块声明、函数声明和类型声明本身不需要额外分号。

## 函数

函数声明推荐写法:

```zr
pub distance(a: Point, b: Point): float {
    var dx = a.x - b.x;
    var dy = a.y - b.y;
    return sqrt(dx * dx + dy * dy);
}
```

参数支持默认值、`const`、变参和传递模式:

```zr
pub log(message: string, level: int = 1): void {}
pub sum(...values: int[]): int { return 0; }
pub copy(%in src: Buffer, %out dst: Buffer): void {}
pub mutate(%ref item: Item): void {}
```

约束:

- `%out` 参数不能有默认值。
- `...args: T[]` 表示变参。
- `func name(...)` 是兼容写法，新代码应直接写 `name(...)`。

## 类型

常见类型写法:

```zr
var ok: bool = true;
var n: int = 1;
var ratio: float = 0.5;
var text: string = "hello";
var letter: char = 'z';
```

数组、固定数组、范围数组和元组类型:

```zr
var names: string[];
var bytes: uint8[16];
var window: int[1..8];
var openRange: int[4..];
var pair: [int, string];
```

泛型类型:

```zr
var points: List<Point>;
var byName: Map<string, Point>;
```

函数类型使用 `%func`:

```zr
var transform: %func(int, int) -> int;
var predicate: %func(string) -> bool;
var FunctionAlias = %func(int) -> int;
```

`%func(...) -> T` 是规范写法。`%func(...) => T` 仍可作为兼容输入读取，但文档和格式化输出应使用 `->`。

异步类型:

```zr
var job: %async int;
```

所有权类型推荐使用大写泛型:

```zr
var owner: Unique<Resource>;
var shared: Shared<Resource>;
var weak: Weak<Resource>;
var borrowed: Borrow<Resource>;
var loaned: Loan<Resource>;
```

旧式 `%unique T`、`%shared T`、`%weak T`、`%borrow T`、`%loan T` 属于迁移兼容表面，不应作为新代码的主要写法。旧的小写泛型 `unique<T>`、`shared<T>` 等已经不是当前推荐语法。

## 泛型与约束

声明可以带类型参数和编译期常量参数:

```zr
pub identity<T>(value: T): T {
    return value;
}

pub struct FixedBuffer<T, const N: int> {
    var data: T[N];
}
```

接口支持变型标记:

```zr
pub interface Source<out T> {
    read(): T;
}

pub interface Sink<in T> {
    write(value: T): void;
}
```

`in` 和 `out` 只用于接口类型参数，不用于普通 `class`、`struct` 或函数泛型参数。

`where` 约束:

```zr
pub make<T>(): T where T: class, new() {
    return new T();
}

pub cloneOwner<T>(value: T): T where T: owner {
    return value;
}

pub useUnique<T>(value: T): void where T: unique {}
pub useShared<T>(value: T): void where T: shared {}
```

当前约束表面包含 `class`、`struct`、`new()`、`owner`、`unique`、`shared`、`weak`，也可以使用类型名作为约束。

## 表达式

常见表达式:

```zr
var a = 1 + 2 * 3;
var b = (a > 3) ? a : 0;
var c = <TargetType>value;
var item = list[index];
var next = object.method(arg).field;
```

对象、数组和生成器块:

```zr
var array = [1, 2, 3];
var object = { name: "zr", value: 1 };
var computed = { [key]: value };

var generated = {{
    out 1;
    out 2;
}};
```

构造:

```zr
var p = $Point(x: 1.0, y: 2.0);
var counter = new Counter(0);
```

约定:

- `struct` 值构造使用 `$Type(...)`。
- `class` 实例构造使用 `new Type(...)`。
- 字符串使用双引号，字符使用单引号，模板字符串使用反引号。

## 控制流

条件:

```zr
if (count > 0) {
    log("positive");
} else if (count == 0) {
    log("zero");
} else {
    log("negative");
}
```

循环:

```zr
while (running) {
    tick();
}

for (var i: int = 0; i < 10; i += 1) {
    step(i);
}

for (;;) {
    break;
}

for (var item in items) {
    visit(item);
}
```

异常:

```zr
try {
    risky();
} catch (e: RuntimeError) {
    handle(e);
} finally {
    cleanup();
}

throw error;
```

`break` 和 `continue` 可在当前语法中携带可选表达式，但普通循环中建议只使用无表达式形式，避免和后续语义约束冲突。

## switch 与 union

普通 `switch`:

```zr
switch (code) {
    (0) {
        return "ok";
    }
    (1) {
        return "retry";
    }
    () {
        return "unknown";
    }
}
```

`union` 声明:

```zr
pub union Shape {
    Empty;
    Circle(radius: float);
    Rect { width: float; height: float; }
    @Fallback(message: string);
}
```

变体形式:

- `Empty;` 是无载荷变体。
- `Circle(radius: float);` 是元组式载荷。
- `Rect { width: float; height: float; }` 是结构式载荷。
- `@Fallback(...)` 表示默认变体，一个 union 中只能有一个默认变体。

Union switch:

```zr
pub area(shape: Shape): float {
    switch (shape) {
        (Shape.Empty) {
            return 0.0;
        }
        (Shape.Circle(r)) {
            return 3.14159 * r * r;
        }
        (Shape.Rect { width: w, height: h }) {
            return w * h;
        }
    }
}
```

当目标 union 类型已知时，分支中也可以使用未限定的变体名。没有默认分支时，编译器可以对已知 union 做基础穷尽性检查，并报告重复变体等不可达分支。

## using

`using` 是当前资源释放、所有权借用和 union/plugin 守卫的统一语法。

资源作用域:

```zr
using owner;

using (owner) {
    owner.work();
}
```

所有权成员方法:

```zr
var owner: Unique<Resource> = %unique(new Resource());
var shared = owner.share();
var weak = shared.weak();

using (owner.loan()) {
    owner.work();
}
```

常见所有权操作:

```zr
var owner = %unique(new Resource());
var shared = %shared(owner);
var weak = %weak(shared);
var borrow = %borrow(owner);
var loan = %loan(owner);
var raw = %detach(owner);
var restored = %upgrade(weak);
%release(owner);
```

限制:

- `%unique new Type(...)` 是当前支持的 `new` 所有权构造表面。
- `%shared new Type(...)`、`%weak new Type(...)` 等不是当前推荐构造方式。
- `%borrowed` 和 `%loaned` 用在类型或接收者位置，不作为普通表达式使用。

Union 守卫:

```zr
using (var [value]: Option.Some = maybeValue) {
    use(value);
} else {
    handleNone();
}

using (var {width, height}: Shape.Rect = shape) {
    draw(width, height);
} else {
    skip();
}
```

无块简写:

```zr
using [value] = maybeValue;
using var [value] = maybeValue;
using {width, h: height} = shape;
```

插件或动态模块守卫:

```zr
using (var plugin = %import("render.vulkan")) {
    plugin.createDevice();
} else {
    fallback();
}
```

旧的 `using (var Variant(x) = value)` 不是当前推荐写法。请使用 `using (var [x]: Union.Variant = value)` 或结构式解构写法。

## 异步

异步函数使用 `%async`:

```zr
%async fetchCount(): int {
    var value = %await loadCount();
    return value;
}
```

规则:

- `%async f(): T` 表示函数结果会被任务类型包裹。
- 如果返回类型已经是任务类型，编译器不会重复包裹。
- `%await expr` 会等待一个任务表达式并返回其结果。

## 编译期语法

`%compileTime` 可用于编译期变量、声明、块或表达式:

```zr
%compileTime var tableSize: int = 256;

%compileTime {
    var generated = buildTable(tableSize);
}

%compileTime class GeneratedType {
    var id: int;
}
```

`%type` 可用于类型对象和类型注解相关场景:

```zr
var IntType = %type(int);
var FnType = %type(%func(int) -> int);
```

类型字面量也可以作为表达式出现:

```zr
var Handler = %func(string) -> bool;
```

## 外部接口

外部库绑定使用 `%extern`:

```zr
%extern("native_math") {
    #zr.ffi.entry("sin")#
    sin(value: double): double;

    delegate Callback(value: int): void;

    struct NativePoint {
        var x: float;
        var y: float;
    }

    enum NativeCode: int {
        Ok;
        Error;
    }
}
```

`%extern` 块内当前主要用于函数签名、delegate、struct 和 enum。FFI 相关属性通过装饰器表达，例如入口名、调用约定、布局等。

## 装饰器与元方法

装饰器使用 `#...#`:

```zr
#route("/health")#
pub health(): int {
    return 200;
}

#zr.ffi.callconv("cdecl")#
nativeCall(value: int): int;
```

装饰器可以放在函数、类型、成员、参数和 extern 成员等声明位置。具体语义由编译期和后端实现决定。

元方法使用 `@name` 声明名:

```zr
pub class Box {
    var value: int;

    @constructor(value: int) {
        this.value = value;
    }

    @destructor() {
        cleanup();
    }

    @call(input: int): int {
        return this.value + input;
    }

    @getItem(index: int): int {
        return this.value;
    }
}
```

当前语法保留了构造、析构、关闭、调用、索引访问、装饰器钩子以及一组运算符/转换相关元方法名。请把这些视为声明表面，具体运算符分派能力以当前测试和后端实现为准。

## 测试

测试块使用 `%test`:

```zr
%test("counter adds values") {
    var counter = new Counter(0);
    assert(counter.add(2) == 2);
}
```

裸 `test("name") { ... }` 是兼容输入，新代码应使用 `%test`。

## 当前不推荐或已移除的写法

不要在新代码中使用这些写法:

```zr
module old.name;              // 使用 %module
func add(a: int, b: int): int // 直接写 add(...)
using (var Some(x) = value)   // 使用 using (var [x]: Option.Some = value)
%using var field: T;          // 字段所有权写进字段类型
unique<T>                     // 使用 Unique<T>
shared<T>                     // 使用 Shared<T>
%func(int) => int             // 文档和输出使用 %func(int) -> int
```

迁移建议:

- 字段级 `%using` 改为 `var field: Unique<T>` 或 `var field: Shared<T>`。
- 旧所有权类型语法改为大写泛型所有权类型。
- 旧 `func` 声明改为直接函数声明。
- 旧 union 守卫改为“类型注解 + 解构”的 `using` 写法。
- 函数类型统一输出为 `->`。

## 证据入口

继续核对语法时，优先看这些文件和目录:

- `docs/zr_language_specification.md`
- `docs/module-system/index.md`
- `docs/plans/using/index.md`
- `docs/plans/aot/index.md`
- `docs/reference-alignment/core-semantics-matrix.md`
- `zr_vm_parser/include/zr_vm_parser/lexer.h`
- `zr_vm_parser/include/zr_vm_parser/ast.h`
- `zr_vm_parser/src/zr_vm_parser/parser/*.c`
- `tests/parser/`
- `tests/fixtures/reference/core_semantics/`
- `tests/fixtures/projects/using_*`

如果文档、解析器和测试之间出现冲突，先以解析器和已启用测试为准，再更新规范或计划文档。
