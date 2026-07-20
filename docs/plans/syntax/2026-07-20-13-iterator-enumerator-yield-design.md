# 13 普通 `fn`、Enumerator、`yield` 与异步迭代实施计划

> **For agentic workers:** implement this document by milestone and keep every public API tied to the reference ledger in section 13.

**Goal:** 复用普通函数、显式返回类型和既有 `for` / `async` 机制实现惰性迭代；只新增无法由普通语句表达的上下文关键字 `yield`，并优先保证零装箱、可栈化、确定性清理和跨挂起点静态检查。

**Architecture:** 生成器不是新的函数种类。包含`yield`的普通函数必须显式声明返回`zr.iteration.Iterator<T>`；同时包含`await`的函数写作`async fn`并显式返回`zr.iteration.AsyncIterator<T>`。四个公开迭代类型只属于`zr.iteration`N1 native descriptor；compiler把`yield`降低为CFG suspension edge和函数私有frame。

**Occam gate:** 不引入 `iterator` modifier、`generator` declaration、隐式 `T -> Iterator<T>` 返回改写、`yield break`、`yield from` 或第二套函数 AST。某项公共类型或函数若不能在第 13 节找到仓库内来源和测试依据，不进入第一版。

---

> 状态：设计第一版，等待按里程碑实施。
>
> 上游契约：[01 CFG/artifact](./2026-07-18-01-canonical-type-place-cfg-artifact-design.md)、[02 borrow checker](./2026-07-18-02-reference-syntax-borrow-checker-design.md)、[03 ref struct/Span/layout](./2026-07-18-03-struct-ref-struct-span-layout-design.md)、[04 Drop](./2026-07-18-04-resource-ownership-drop-gc-bridge-design.md)、[05 property](./2026-07-18-05-property-unified-ast-design.md)、[12 Task/Scheduler](./2026-07-20-12-async-task-job-scheduler-design.md)。

## 1. 必要实体与删除项

第一版只保留四个公开概念：

| 实体 | 必要性 | 不可由什么替代 |
| --- | --- | --- |
| `Iterable<T>` | repeatable source 的 canonical capability | 单次 cursor 无法表达“每次 `for` 创建新 cursor” |
| `Enumerator<T>` | 同步 single-pass cursor capability | 对 Array、Span、Map 写类型名分支不可扩展 |
| `Iterator<T>` | `yield` 状态机的 single-use、owned 返回载体 | 返回元素 `T` 会造成签名撒谎；普通 interface 擦除会妨碍专用 layout |
| `AsyncIterator<T>` | `await + yield` 状态机及异步清理载体 | `Task<Iterator<T>>` 只异步创建 cursor，不能表达每次推进都异步 |

`Iterator<T>` 是 compiler-backed opaque value，并实现 `Enumerator<T>` capability。它允许 artifact 保存函数专用 frame layout，调用方仍看到稳定 TypeRef。它不是 class，也不要求 GC allocation。

### 1.1 Native ModuleId唯一归属

`Iterable<T>`、`Enumerator<T>`、`Iterator<T>`和`AsyncIterator<T>`的Canonical TypeId全部以`zr.iteration`为owner。`zr.container`只让Array/Map等concrete type实现这些capability，不能复制协议TypeDef；`zr.task`只提供AsyncIterator所用的`Task<T>` ABI，不能拥有AsyncIterator。

`zr.iteration`属于第10章OfficialNative domain的N1 Runtime native module。公开opaque carrier/type role由`ZrLibModuleDescriptor`登记；每个含yield函数生成的frame是`.zri/.zro`私有artifact type，不创建`zr.generator`、`zr.iterator`或per-function public module。compiler按IteratorCarrier/AsyncIteratorCarrier/Enumerator capability id绑定，不比较type-name字符串。

第一版不增加：

- `iterator fn`、`async iterator fn` 或 iterator lambda modifier；
- `AsyncIterable<T>`、`AsyncEnumerator<T>` 两个同义层；异步 repeatable source 用普通 factory function 显式返回新的 `AsyncIterator<T>`；
- `close()` 同步协议；同步清理由既有 Drop capability承担；
- `yield ref`、`yield from`、`yield break`；
- 隐式并发 consumer、自动 channel 或 cancellation 语法；
- 旧 `{{ ... }}` generator expression 和 generator `out expr;`。

## 2. 语法与显式返回 TypeRef

### 2.1 同步迭代

`yield` 函数仍是普通 `FunctionDefinition`：

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

规则：

- `fn range(...): Iterator<int>` 的返回类型就是调用结果，不存在 effective return type 或隐藏包装。
- `yield expression;` 的 expression 必须可赋给 `Iterator<T>` 的元素 `T`；`return;` 结束序列。
- `return expression;` 在包含 `yield` 的函数中非法。
- 调用只初始化 cold state machine；首次 `moveNext()` 才执行函数体。
- `yield` 只作用于当前普通函数，不能穿过 nested function 或 lambda 边界。
- lambda 同样显式写 `fn(...): Iterator<T> { ... }`，不增加 iterator lambda 语法。
- formatter 不根据函数体改变签名；返回类型不是 `Iterator<T>` 时，`yield` 直接诊断。

### 2.2 异步迭代

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

规则：

- `async fn` 仍是既有 suspension marker，但返回 TypeRef 明写 `AsyncIterator<Event>`；不先改写为 `Task<Event>`，也不隐藏第二层 carrier。
- 第 12 章的 async result carrier 因此限定为 compiler 注册的 `Task<T>` 或本章 `AsyncIterator<T>`，而不是“所有 async fn 都暗中包装 Task”。
- `await for` 是既有 `await` 与 `for` 的组合解析，不增加单个复合 token。
- 每次推进和异步清理都可能 pending；不能把同步 Enumerator 自动包装为 Task。
- 第一版禁止 `ref T`、`ref struct`、`Span<T>` 或 `PoolRef<T>` 作为 async iterator element，避免直接引用跨 `await` 或下一次推进失效。

## 3. Canonical protocol

标准协议位于`zr.iteration`native descriptor。下面是descriptor生成的interface projection，每个函数都有显式返回TypeRef；它不是标准库的ZR实现源码：

```zr
let task = import("zr.task");

interface Enumerator<T> {
    fn moveNext(): bool;
    property current: ref readonly T { pub get; }
}

interface Iterable<T> {
    fn getEnumerator(): Enumerator<T>;
}

interface AsyncIterator<T> {
    fn moveNext(): task.Task<bool>;
    property current: T { pub get; }
    fn close(): task.Task<void>;
}
```

`Iterator<T>` 是 compiler-known opaque value，满足 `Enumerator<T>` 和 Drop capability，不再声明一套重复成员。

协议约束：

- `current` 仅在下一次 `moveNext()` 或 Drop 前有效；`for (let value ...)` 按 `T` 的 Copy/move/borrow 规则建立局部绑定。
- 实际 `getEnumerator()` 返回的 concrete value 保留在 SemIR 和 artifact 中；generic constrained/static dispatch 不得为了统一接口而装箱。
- 同步 cursor 若持有资源，依赖既有 Drop；`for` 在 normal end、`break`、`return` 和 `throw` 的 cleanup edge 上 Drop 一次。
- `AsyncIterator.close(): Task<void>` 是唯一新增的显式清理函数，因为同步 Drop 无法执行异步 finally。
- API 名称和语义只能由第 13 节 reference ledger 锁定，不按字符串启发式匹配。

## 4. `for` 与 `await for` lowering

`for (let pattern in expression)` 固定执行：

1. RHS 只求值一次。
2. 若值满足 `Enumerator<T>`，直接取得 cursor；否则解析唯一 `getEnumerator(): E` 并证明 `E` 满足该 capability。
3. 建立 cleanup region，循环调用 `moveNext(): bool`。
4. true 分支读取一次 `current`，再执行普通 pattern binding。
5. 所有退出边对 cursor 执行一次 Drop。

`await for` 固定执行：

1. RHS 必须为 `AsyncIterator<T>`。
2. 每轮 await `moveNext(): Task<bool>`。
3. true 分支按值读取一次 `current`。
4. 所有退出边 await `close(): Task<void>`；cleanup 不被普通取消请求跳过。

Array、Span、Map 等由 descriptor 注册同一 capability。compiler 不允许“找不到协议后尝试数组 magic、`@call` 或成员名猜测”的 fallback。

解构沿用普通 binding：

```zr
for (let { key, value } in map.entries()) {
    consume(key, value);
}
```

`current` 只读取一次。move-only element 若无法从 `ref readonly current` 合法取得，binder 必须报错，不隐式复制。

## 5. Iterator frame、内存与 GC

compiler 为每个含 `yield` 的函数生成专用 frame：

```text
IteratorFrame<T> {
    stateId;
    current: MaybeInitialized<T>;
    capturedParameters;
    liveAcrossYieldLocals;
    cleanupState;
    faultState;
}
```

- `Iterator<T>` non-Copy、single-consumer；复制、重入、Drop 后推进必须静态拒绝，只有动态边界保留最小状态检查。
- body 可见且不逃逸时，frame 内联到 caller stack，并允许 scalar replacement。
- 跨 module、逃逸或 frame 过大时，使用 typed pool-backed frame；这不创建普通 GC class。
- frame 含 GC handles 时，`TypeLayout` 提供精确 pointer map。pool frame 只扫描当前 state 已初始化且 live 的 slots；`GcFree` frame 不登记 root。
- `current` 在下一次推进前 Drop/overwrite；结束、fault 和提前退出共享同一 cleanup CFG。
- benchmark 必须分别报告 stack、pool 和 async pending 路径，不能只给总耗时。

这套模型直接服务本计划的目标：连续 frame layout、减少 GC object、精确扫描和池化复用；不以新增 source-level ownership 类型换取实现便利。

## 6. Suspension 与 borrow

每个 `yield` 在 borrow checking 前形成：

```text
YieldValue(value, nextState)
YieldSuspend(stateId, livePlaces, cleanupState)
YieldResume(stateId)
```

第一版禁止下列值跨 `yield` 存活：

- direct `ref`；
- `ref struct`、`Span<T>`、`PoolRef<T>`；
- lock guard、native pin 或任意 `scoped` borrow。

高性能 Span 迭代使用手写 `ref struct` enumerator 并实现 `Enumerator<T>`，不使用 `yield`。这样热循环可以保留 direct reference，又不引入 self-referential frame 或 Pin 语法。

owner 值可以 move 进 frame。normal exhaustion、consumer early exit、fault 和 Drop 必须释放 exactly once；fault 在触发它的 `moveNext()` 调用点传播，之后 state 固定为 closed。

## 7. 控制流限制

- `yield` 可位于普通 block、`if`、`switch`、loop 和被 `try/finally` 包围的 try body。
- 第一版禁止 `yield` 直接位于 `catch` 或 `finally` body，避免 cleanup 再次挂起。
- 禁止在 destructor/Drop body、property accessor、native callback 和 comptime code 中 `yield`。
- `break` / `continue` 保持普通 loop 语义；`return;` 结束整个 iterator。
- body 出现 `yield` 但返回 TypeRef 不是 `Iterator<T>` 或 `AsyncIterator<T>` 时，报“显式返回 carrier 不匹配”，不推断签名。
- `comptime fn`、native/bodyless declaration 和 abstract function 不能含 `yield`。

## 8. Semantic IR、artifact 与 LSP

SemIR 只新增 suspension 和 protocol operation，不新增函数种类：

```text
YieldValue
YieldSuspend
YieldResume
IteratorComplete
IteratorFault
AsyncIteratorClose
EnumeratorAcquire
EnumeratorMoveNext
EnumeratorCurrent
EnumeratorDrop
```

- AST 仍使用普通 `FunctionDefinition` 和 `YieldStatement`。
- `.zri` 保存 yield CFG、frame liveness/layout、cleanup state 和 source map。
- `.zro` 保存显式`zr.iteration.Iterator<T>`/`AsyncIterator<T>` return TypeId、owner ModuleId、element TypeId、frame ABI/hash和capability requirements；不保存“表面返回T、有效返回Iterator<T>”双签名。
- LSP hover 直接显示 `fn(...): Iterator<T>`；completion 只建议 `yield`，不建议 `iterator fn`。
- debugger 在 `yield` 处显示 logical frame/local，step resume 到下一 statement。

## 9. 旧语法迁移

- `{{ statements; out expression; }}` 机械迁移为显式返回 `Iterator<T>` 的 local/named ordinary function，并把 generator `out expression;` 改为 `yield expression;`。
- 无法证明 element type、capture lifetime 或 escape 时，migration 输出 requires-review，不发明隐式类型。
- `out` parameter 及普通 out binding 不参与全文替换。
- `iterator fn` 只可能出现在早期草案或 negative fixture；若曾进入 fixture，迁移为 `fn(...): Iterator<T>`。
- current parser/formatter 永远不生成旧 generator 或 `iterator` modifier。

## 10. 分层里程碑

### M1 Enumerator capability 与 ordinary `for`

统一 Array、Span 和 container protocol resolution、constrained call、Drop cleanup 与 destructuring。没有 concrete type-name 分派且热路径零装箱后晋级。

### M2 普通函数 + `yield`

复用 FunctionDefinition parser，实现 YieldStatement、显式 carrier 校验、frame layout、stack/pool path、fault 和 owner cleanup。

### M3 `AsyncIterator<T>`

第 12 章 Task/Scheduler ABI 稳定后实现 `async fn(...): AsyncIterator<T>`、`await for` 和 async close。不得复制 completion queue 或 exception transport。

### M4 artifact、AOT、debug、LSP 与迁移

完成 ABI roundtrip、VM/AOT 等价、logical stepping、legacy migration 和 comprehensive reference fixture。

## 11. 实施任务

### Task 1: canonical protocol 与 `for`

**Files:**

- Create: `zr_vm_lib_iteration/CMakeLists.txt`
- Create: `zr_vm_lib_iteration/include/zr_vm_lib_iteration/module.h`
- Create: `zr_vm_lib_iteration/src/zr_vm_lib_iteration/module.c`
- Create: `zr_vm_lib_iteration/src/zr_vm_lib_iteration/runtime/descriptor.c`
- Create: `zr_vm_parser/include/zr_vm_parser/iteration_contract.h`
- Create: `zr_vm_parser/src/zr_vm_parser/compiler/enumerator_binding.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_flow.c`
- Test: `tests/iterator/test_enumerator_protocol.c`

- [ ] 先用单一`zr.iteration`descriptor登记四个public TypeId/role，并覆盖builtin/plugin注册的contract hash一致性。
- [ ] 覆盖direct Enumerator、Iterable factory、ref struct constrained call、invalid shape和Drop cleanup。
- [ ] 删除 Array/type-name fallback。
- [ ] 用 allocation/instruction assertions 证明 Array/Span hot loop 无 box/heap allocation。

### Task 2: YieldStatement 与 SemIR

**Files:**

- Modify: `zr_vm_parser/include/zr_vm_parser/ast.h`
- Create: `zr_vm_parser/src/zr_vm_parser/parser_yield.c`
- Create: `zr_vm_parser/src/zr_vm_parser/compiler/iterator_semantic.c`
- Modify: `zr_vm_parser/include/zr_vm_parser/semantic_ir.h`
- Test: `tests/iterator/test_yield_syntax.c`
- Test: `tests/iterator/test_iterator_semantic_ir.c`

- [ ] 断言 AST 仍为 FunctionDefinition；不得出现 IteratorFunctionSyntax。
- [ ] 覆盖显式 carrier、`yield` 位置、`return`、nested function 边界和 parser recovery。
- [ ] 每个 `yield` 先形成 CFG suspension facts，再运行 borrow checker。

### Task 3: frame、runtime 与 cleanup

**Files:**

- Create: `zr_vm_core/include/zr_vm_core/iterator_runtime.h`
- Create: `zr_vm_core/src/zr_vm_core/iterator/frame.c`
- Create: `zr_vm_core/src/zr_vm_core/iterator/dispatch.c`
- Test: `tests/iterator/test_iterator_runtime.c`
- Test: `tests/iterator/test_iterator_gc_drop.c`

- [ ] 覆盖 0/1/多 yield、nested loop、fault、early exit、Drop 和 reentrancy。
- [ ] 每个 state 强制 GC，验证 pointer map 与 owner Drop exactly once。
- [ ] 用 counter 分别证明 stack/no-allocation 和 typed pool reuse 路径。

### Task 4: async iterator

**Files:**

- Create: `zr_vm_parser/src/zr_vm_parser/compiler/async_iterator_semantic.c`
- Create: `zr_vm_core/src/zr_vm_core/iterator/async_iterator.c`
- Test: `tests/iterator/test_async_iterator.c`
- Project: `tests/fixtures/projects/async_iterator_reference/`

- [ ] 覆盖 completed/pending moveNext、await/yield 交错、async close、fault 和 concurrent moveNext rejection。
- [ ] 复用 Task/Scheduler/FrameLayout。
- [ ] VM/AOT 比较 values、states、cleanup 和 allocation counters。

### Task 5: artifact、工具与迁移

**Files:**

- Modify: `zr_vm_core/src/zr_vm_core/artifact_*.c`
- Modify: `zr_vm_debugger/`
- Modify: `zr_vm_language_server/`
- Test: `tests/artifact/test_iterator_contract_roundtrip.c`
- Test: `tests/language_server/test_lsp_iterator_facts.c`

- [ ] 验证 source/binary 显式 signature 和 frame compatibility。
- [ ] 添加 yield stepping、hover、borrow 和 second-consume diagnostics。
- [ ] legacy migration fixture 幂等，current fixture 不含旧表层。

## 12. 晋级门

必须同时满足：

- 所有函数声明和 callable 示例都有显式返回 TypeRef。
- Iterable/Enumerator/Iterator/AsyncIterator只有`zr.iteration`owner TypeDef；container/task/builtin没有同义re-export。
- builtin与descriptor plugin provider产生相同ModuleId、TypeId、role和public contract hash，N1等级不参与runtime dispatch。
- parser/AST 不存在 iterator function declaration。
- `for` 不存在 concrete type-name 特例。
- 每个 `yield` 在 borrow 前是 CFG suspension edge。
- 所有退出只 cleanup 一次。
- sync immediate-consume 路径可证明不分配 GC object。
- async iterator 只复用 Task ABI。
- 第 13 节所列公共 API 的来源路径和对应测试路径均存在。
- 旧 `{{...}}`、generator `out` 和 `iterator fn` 只存在 migration/negative fixture。

## 13. 公共 API reference ledger

“有 reference”是第一版准入条件：每个 public entity/function 至少有一个协议来源和一个行为/编译器测试来源；优先使用两种语言交叉验证。路径均为仓库内 `./lua/` 镜像。

`Iterator<T>`与`AsyncIterator<T>`必须各自保留独立行：前者的single-use同步cursor/Drop来源不能替代后者的pending推进与async close来源，反之亦然。

| ZR 实体或函数 | 协议/实现来源 | 行为/编译器测试来源 | 采纳边界 |
| --- | --- | --- | --- |
| `zr.iteration.Enumerator<T>`、`moveNext(): bool`、`current` | `lua/runtime/src/libraries/System.Private.CoreLib/src/System/Collections/Generic/IEnumerator.cs`；`lua/rust/library/core/src/iter/traits/iterator.rs` | `lua/roslyn/src/Compilers/CSharp/Test/Emit/CodeGen/CodeGenIterators.cs`；`lua/cpython/Lib/test/test_generators.py` | 保留 cursor 协议；不复制 .NET object boxing |
| `zr.iteration.Iterable<T>`、`getEnumerator(): Enumerator<T>` | `lua/runtime/src/libraries/System.Private.CoreLib/src/System/Collections/Generic/IEnumerable.cs`；`lua/rust/library/core/src/iter/traits/iterator.rs` | `lua/roslyn/src/Compilers/CSharp/Test/Semantic/Semantics/IteratorTests.cs` | 只负责 repeatable factory |
| `zr.iteration.Iterator<T>` opaque value | `lua/rust/library/core/src/iter/traits/iterator.rs`；`lua/cpython/Objects/genobject.c` | `lua/cpython/Lib/test/test_generators.py`；`lua/roslyn/src/Compilers/CSharp/Test/Emit/CodeGen/CodeGenIterators.cs` | single-use/cold；ZR增加显式TypeRef与stack/pool layout |
| `zr.iteration.AsyncIterator<T>`、`moveNext(): zr.task.Task<bool>`、`close(): zr.task.Task<void>` | `lua/runtime/src/libraries/System.Private.CoreLib/src/System/Collections/Generic/IAsyncEnumerator.cs`；`lua/rust/library/core/src/async_iter/async_iter.rs` | `lua/roslyn/src/Compilers/CSharp/Test/Emit/CodeGen/CodeGenAsyncIteratorTests.cs` | 合并async cursor层；不引入AsyncIterable或第二owner |
| `yield` suspension | `lua/cpython/Python/codegen.c`；`lua/cpython/Objects/genobject.c` | `lua/roslyn/src/Compilers/CSharp/Test/Semantic/Semantics/IteratorTests.cs`；`lua/roslyn/src/Compilers/CSharp/Test/Emit3/RefUnsafeInIteratorAndAsyncTests.cs` | 只新增 statement；不新增 function kind |
| Drop/early-exit cleanup | `lua/cpython/Objects/genobject.c`；`lua/runtime/src/libraries/System.Private.CoreLib/src/System/Collections/Generic/IEnumerator.cs` | `lua/cpython/Lib/test/test_generators.py`；`lua/roslyn/src/Compilers/CSharp/Test/Emit/CodeGen/CodeGenIterators.cs` | 同步复用 Drop，异步才保留 close |
| `range` / `readEvents` 示例形状 | `lua/cpython/Lib/test/test_generators.py`；`lua/runtime/src/libraries/System.Private.CoreLib/src/System/Collections/Generic/IAsyncEnumerable.cs` | `lua/roslyn/src/Compilers/CSharp/Test/Emit/CodeGen/CodeGenAsyncIteratorTests.cs` | 仅作为语法示例，不新增库函数 |

若实施时要增加 ledger 中没有的 public helper，必须先补充必要性说明、仓库内 reference 和 negative/behavior tests；否则使用现有普通函数、property、Drop、Task 或 typed data表达。
