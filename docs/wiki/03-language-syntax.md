---
related_code:
  - README.md
  - docs/zr_language_specification.md
  - zr_vm_parser/include/zr_vm_parser/lexer.h
  - zr_vm_parser/include/zr_vm_parser/parser.h
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/syntax_contract.h
  - zr_vm_parser/src/zr_vm_parser/parser.c
  - zr_vm_language_server_extension/syntaxes/zr.tmLanguage.json
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/lexer.c
  - zr_vm_parser/include/zr_vm_parser/syntax_contract.h
tests:
  - tests/parser/test_parser.c
  - tests/parser/test_syntax_reference_v1.c
  - tests/fixtures/projects/syntax_reference_v1/syntax_reference_v1.zrp
  - zr_vm_language_server_extension/test/syntaxGrammar.test.js
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/zr_language_specification.md
  - docs/plans/syntax/README.md
doc_type: module-detail
---

# 语言语法参考

**状态：`current`；个别高级能力的跨后端状态见[状态矩阵](06-reference/status-matrix.md)。**

本页描述生产 parser 接受的 ZR 表层。换行是 trivia，**不会自动插入分号**；简单声明、绑定、表达式、赋值、`return`、`throw`、`break`、`continue` 和无函数体声明必须写 `;`。

## 源文件与模块

```ebnf
source          = [ module-declaration ], { top-level-declaration } ;
module-declaration = "module", qualified-name, ";" ;
qualified-name  = identifier, { ".", identifier } ;
```

示例：

```zr
module app.main;

let system = import("zr.system");
let version: string = "0.0.1";
```

`module` 声明的语义名称只允许用 `.` 分段，例如 `app.main`。`import(...)` 是一个专用静态导入表达式，只接受一个字符串 literal；它通常绑定到 module-scope `let`。导入 literal 内部可以承载 builtin、relative、alias、package、registered-native 或 file locator 形式，其中路径形式可包含 `/`；这不改变 `module` 声明本身的点分名称规则。详见[模块与产物](07-modules-projects-artifacts.md)。

`module` 声明是可选的。省略时，project resolver 使用当前源文件的规范路径和 manifest
上下文推导 module identity；同一工程内不应让显式名称与推导名称冲突。独立 parser 只把
缺省值保留为“未声明”，不会在 AST 阶段猜测宿主路径。

## 词法规则

### 标识符与关键字

标识符由字母或 `_` 开头，后续可包含字母、数字和 `_`。当前 lexer 直接保留为 token 的拼写是：`module`、`struct`、`class`、`abstract`、`virtual`、`override`、`final`、`shadow`、`interface`、`enum`、`union`、`fn`、`ref`、`typeid`、`typeof`、`share`、`degrade`、`wake`、`intoGc`、`drop`、`test`、`intermediate`、`var`、`let`、`using`、`pub`、`pri`、`pro`、`if`、`else`、`switch`、`while`、`for`、`break`、`continue`、`return`、`yield`、`super`、`new`、`set`、`get`、`static`、`const`、`in`、`out`、`throw`、`try`、`catch`、`finally`、`Infinity`、`NegativeInfinity`、`NaN`、`true`、`false` 和 `null`。

`import`、`init`、`own`、`readonly`、`scoped`、`async`、`await`、`comptime`、`native`、`extern`、`resource` 是**上下文拼写**：lexer 将它们读作 identifier，parser 只在相应语法位置赋予专门含义。为避免未来语法扩展造成歧义，公共 API 名称不应使用这些拼写。

`%` 只保留为 modulo/modulo-assignment 运算符；任何 `%module`、`%import`、`%owned` 等形式都走 `legacy_syntax_removed` 诊断，不产生生产 AST。

### Literal

```zr
let nil = null;
let enabled = true;
let count = 42;
let ratio = 3.14;
let letter = 'Z';
let text = "zr\nvm";
let bytes = [0x01, 0x02, 0xFF];
```

字符串使用 UTF-8 字节序列；字符 literal 必须表示一个合法字符。数字的默认类型由上下文和类型推断决定，跨 ABI 时应写明确宽度（如 `i32`、`i64`、`u64`、`float`）。

## 声明

### 绑定

```zr
let immutableBinding: int = 1;
var mutableBinding: int = 2;
mutableBinding += 1;
```

`let` 禁止重新绑定名称，但其对象内容不自动深度 immutable；`var` 允许重新赋值。字段也必须显式使用 `let`/`var`，property 不会隐式创建 backing field。

### 函数与 callable

```zr
fn add(left: int, right: int): int {
    return left + right;
}

let increment: fn(int) -> int = fn(value: int): int => value + 1;
```

参数 passing mode 写在 TypeRef 中：

```zr
fn read(value: in int): int { return value; }
fn update(value: ref int): void { value += 1; }
fn inspect(value: ref readonly int): int { return value; }
fn produce(value: out int): void { value = 7; }
```

函数、方法、lambda 和 native callable 都可以形成 callable value；完整调用绑定规则见[函数与调用](05-functions-and-calls.md)。

### 类型声明

```zr
struct Point { var x: float; var y: float; }
readonly struct Size { let width: int; let height: int; }
class Document { pri var text: string; }
resource class FileHandle { pub fn close(): void; }
interface Printable { fn print(): void; }
enum Color: int { Red; Green; Blue; }
union Result { Ok(value: int); Error(message: string); }
```

访问修饰符（如 `pub`、`pro`、`pri`）控制 module/type/member 可见性；接口和 bodyless/native 声明以 `;` 结束。

### property

```zr
class Counter {
    pri var stored: int = 0;

    pub property value: int {
        pub get { return stored; }
        pri set { stored = value; }
    }
}
```

property 由一个可见的 property symbol 和 getter/setter/init accessor symbols 组成；存储必须由显式 field 提供。`ref` return property 产生 Place/reference contract，而不是复制值。

## 表达式

支持字面量、名称、数组/对象构造、成员访问、索引、调用、算术/比较/逻辑、条件、类型转换、ownership intrinsic、`typeof/typeid` 和 `await` 等。

```zr
let sum = left + right;
let label = total > 0 ? "active" : "empty";
let first = values[0];
let name = service.load(id).name;
let optionalName = maybeService?.load(id)?.name;
let answer = callback?.(input);
```

可选调用的规范写法是 `callable?.(args)`；对象目标使用 `receiver?.member` 或 `receiver?.method(args)`。缺失目标会跳过完整剩余 postfix suffix 和参数求值；普通 `.` 在 null/absent 上抛出 `NullReferenceError`。

## 语句与控制流

```zr
if (ready) { start(); } else { reset(); }

for (var i: int = 0; i < values.length; i += 1) {
    consume(values[i]);
}

for (let item in values) {
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

`return`、`throw`、`break`、`continue` 都是显式控制转移；异常处理器和 `using`/Drop cleanup 会在这些边上生成统一 cleanup CFG。`switch` 可以匹配 enum/union variant，并由 CFG/dataflow 做穷尽性与不可达检查。

## 泛型、引用与异步语法

```zr
fn identity<T>(value: T): T { return value; }
fn tryGet<T>(key: string, value: out T): bool { value = default(T); return true; }

async fn load(): zr.task.Task<string> {
    return await readText("config");
}

fn range(end: int): zr.iteration.Iterator<int> {
    for (var i: int = 0; i < end; i += 1) { yield i; }
}
```

`async fn` 必须显式返回 `Task<T>`；含 `yield` 的普通函数必须返回 `Iterator<T>`，异步迭代器返回 `AsyncIterator<T>`。`ref`、`out`、`ref struct`、Span 和活动 borrow 不能跨 `await`/`yield`。

## 编译期、attribute 与测试

```zr
const stride: usize = sizeOf<Vertex>();

let compile = import("zr.compile");
comptime if (compile.build.feature("simd")) {
    const selectedWidth: int = 4;
}

let testing = import("zr.testing");

#zr.testing.test#
fn comparesIntegers(): void {
    testing.assert(1 < 2, "expected ordered integers");
}
```

`comptime` 代码只读取规范 build facts 和编译期导入；attribute 是带 metadata role 的只读结构。测试是普通 module-scope function 加 `zr.testing` metadata，而不是独立 `test` 关键字。

编译期 callable、阶段和预算见 [`zr.compile`](03-modules/compile.md)；声明 transform 的
不可变 view 与 typed Patch 见 [`zr.compile.declaration`](03-modules/compile-declaration.md)。

## 删除的旧形式（`migration`）

生产 parser 拒绝：`%module`、`%import`、`%extern`、`%compileTime`、`%test`、`%owned`、`func`、旧 return delimiter、`$Type(...)`、旧 generator `out`、用户 `intermediate` 声明，以及把 `share/weak/upgrade/intoGc` 当作隐式 ownership member 的形式。迁移工具可识别它们并输出结构化 edit，但这些 token 不会抵达 semantic lowering。
