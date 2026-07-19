# ZR 语法、引用与内存模型

更新日期：2026-07-19

本仓库正在将 ZR 收敛为一套以规范类型、可寻址位置和静态数据流为核心的语言契约。目标是让普通业务代码继续采用自动 GC，同时让性能敏感代码能够显式使用值类型、引用、连续内存和确定性资源管理，而不把这些能力实现为互不相干的语法特例。

> 状态：`docs/plans/syntax/` 中的总设计已批准，细化设计按依赖顺序实施。本文描述目标语法和迁移方向；它不表示所有示例均已在解释器、VM、AOT、artifact writer 和 LSP 中完成闭环。当前可执行行为仍以语言规范、已启用测试和对应里程碑状态为准。

## 设计入口

- [语法重设计子计划索引](docs/plans/syntax/README.md)
- [总设计：ZR 语法、引用与内存模型重设计](docs/plans/syntax/2026-07-18-zr-syntax-and-memory-model-redesign.md)
- [Canonical TypeRef、Place IR、CFG facts 与 artifact schema](docs/plans/syntax/2026-07-18-01-canonical-type-place-cfg-artifact-design.md)
- [`fn/ref/in/out/scoped/readonly` 与 borrow checker](docs/plans/syntax/2026-07-18-02-reference-syntax-borrow-checker-design.md)

完整路线还包括值布局、资源所有权、属性、迁移/LSP、全语法 fixture、反射、池化以及 native FFI、模块和包解析。它们的依赖关系与晋级门由子计划索引统一维护。

## 核心模型

语言语义按以下层次形成，后续层不得反向定义前一层的含义：

```text
Source
  -> Syntax AST
  -> Bound declarations and symbols
  -> Canonical Type graph
  -> Semantic IR, CFG, and Place graph
  -> Flow facts and public contracts
  -> ExecIR / ExecBC
  -> VM or AOT
```

其中：

- **Value** 是一次计算的结果，可以复制或移动，但不是写入目标。
- **Place** 是可寻址存储位置，例如局部变量、字段、索引或解引用结果。
- **Ref** 是指向 Place 的非拥有引用，带有读写与逃逸约束。
- **Owner** 是负责资源生命周期的拥有句柄。

AST 只保存语法结构和源位置；规范类型、借用状态、确定赋值、移动、逃逸和可达性由 Canonical Type、Place、CFG 与 flow facts 统一表达。VM、AOT、LSP、反射和 artifact writer 消费同一份稳定契约或其投影。

## 目标语法速览

### 模块与导入

模块声明和静态导入使用普通语言语法。每个简单声明必须显式以分号结束；换行只是 trivia，不触发自动分号插入。

```zr
module app.render;

let reflection = import("zr.reflection");
let config = import(".config");
let matrix = import("@math.matrix");
```

- `import(...)` 只接受字符串字面量，产生只读 `ModuleNamespace` 并记录静态依赖。
- `.` 和 `/` 都可用作模块路径分段符；`.` 与 `..` 前缀表示相对模块路径。
- 第三方包根使用单段 `@identifier`，例如 `@math` 或 `@math.matrix`。
- 动态路径不属于静态 import 语法，应使用 `loadModule` 或 `loadPlugin`。

### 函数与 callable 类型

函数声明使用 `fn`，返回类型由 `:` 引出；callable 类型使用 `->`；`=>` 只引出匿名函数的 expression body。

```zr
pub fn add(left: int, right: int): int {
    return left + right;
}

let increment = fn(value: int): int => value + 1;
let transform: fn(int) -> int;
let factory: fn() -> fn(string) -> int;
```

普通 instance `fn` 可以修改 receiver；`const fn` 表示 readonly receiver。块体声明由 `}` 闭合，通常不需要额外分号；bodyless 声明必须以 `;` 结束。作为 `let` initializer 的 block-bodied 匿名函数仍须写成 `};`，因为结束的是外层绑定。

### 值、引用与参数契约

`T`、`ref T`、`ref readonly T`、`scoped ref T`、`Unique<T>`、`Shared<T>` 与 `Weak<T>` 是规范化类型能力。`in`、`ref` 与 `out` 是参数 contract，不是任意位置可声明的局部类型。

```zr
fn clear(value: ref int): void {
    value = 0;
}

fn tryCreate(result: out int): bool {
    result = 1;
    return true;
}

var counter: int = 5;
clear(ref counter);

var created: int;
if (tryCreate(out created)) {
    use(created);
}
```

- `ref` 调用参数必须是可寻址 Place。
- `out` 调用参数必须是可写 Place；调用前视为未初始化，只有正常返回路径保证初始化完成。
- `in` 可接受临时值，编译器会创建函数作用域临时 Place。
- `scoped ref` 不能逃出当前函数；`ref readonly` 保留位置身份但不能经由该引用写入。
- 借用、move、readonly 冲突、`ref struct` 逃逸和 `out` 确定赋值必须静态检查；数组边界、Weak upgrade、动态 cast 与 native 输入等无法证明的状态仍保留运行时检查。

### 值构造、类型与所有权

`struct` 值构造使用静态 TypeRef 的 `init TypeRef(...)`。普通 `expr(...)` 只表示 call 或 `@call`；旧 `$proto(...)` 动态原型构造不再属于核心语法。

```zr
struct Point {
    pub var x: float;
    pub var y: float;
}

let origin = init Point(0.0, 0.0);
```

具体类型能力来自规范 TypeRef 与 TypeLayout，而不是字符串类型名或 AST 旁路标记：

- `struct` 是内联值；`ref struct` 具有不能逃逸或被装箱的 ref-like 限制。
- 普通 `class` 使用 GC；resource class 通过 `Unique<T>`、`Shared<T>`、`Weak<T>` 进入确定性生命周期模型。
- `Span<T>` 等连续视图不分配也不拥有内存。
- 只有 Canonical TypeLayout 证明为 `GcFree` 的闭合类型或 slab 才能标记为 `NoScan`；其他布局仍需精确 GC pointer map、write barrier 和 remembered set。

### 属性、反射、池化与 native FFI

- concrete property 不隐式生成 backing field；存储必须先以 `pri`、`pro` 或 `pub` 的 `let/var` field 显式声明。
- ref-return property 只有 getter；getter 产生 Ref，解引用后才形成可写 Place。
- 反射 API 位于 `zr.reflection`。`typeid(TypeRef)` 提供轻量身份，`typeof(expr)` 返回运行时精确 descriptor；动态构造通过 `ConstructibleType.createInstance(...constructionArgs: object)` 的显式 capability boundary 完成。
- 池化 API 位于 `zr.pooling`。`PoolHandle<T>` 是可长期保存的 generational weak identity，`tryBorrow(handle, out PoolRef<T>)` 负责一次验证并初始化 scoped direct ref。
- 静态 native 声明使用 `native extern("library") { ... }`。语言 callable contract 与 ABI `FfiSignature` 在绑定期一次形成，并由 VM 与 AOT 共享。

```zr
native extern("native_math") {
    fn sin(value: float): float;
}
```

## 迁移规则

新语法不是简单删除 `%` 前缀，而是将旧 AST 中重复的 passing mode、ownership qualifier、内建类型枚举和字符串特判收敛为统一的 Type、Place、Owner 和 Region 语义。

- 旧 `%xxx` 只允许存在于 migration frontend、负例和历史文档中，不保留长期双轨语义。
- 新代码使用 `fn`、`ref`、`out`、`scoped`、`readonly`、`module`、`import(...)`、`init TypeRef(...)` 与 `native extern(...)` 等目标写法。
- 不以具体类型名触发共享基础设施特例。`Span<T>`、`Unique<T>`、池化类型和反射类型均通过 capability、token、layout 或稳定 contract 接入。
- `.zro` 只保存跨模块所需的稳定类型、布局与公开契约；局部 flow facts 默认进入 `.zri` 与调试 sidecar。

在迁移完成前，正式语言规范仍描述当前可执行语法。若本文、计划文档、解析器与测试出现冲突，按以下顺序判断：已确认子设计、总设计、当前语言规范、已启用测试与实现状态。

## 实施与验证

每个语法里程碑必须先验证下层基础，再验证父层、项目与 CLI；上层 smoke 不能替代 Type、Place、CFG、layout 或 runtime contract 的基础测试。完成某项能力至少需要覆盖：

- 成功、边界、失败和诊断范围；
- VM 与 AOT 的等价行为；
- 受影响的 `.zrs`、`.zri`、`.zro` 产物；
- LSP 查询或明确限定该阶段只提供 query facts；
- 不依赖 concrete type-name dispatch 或临时拼写特判。

建议从 `docs/plans/syntax/README.md` 选择当前里程碑，再结合对应测试、fixture 与实现状态验证实际可用范围。
