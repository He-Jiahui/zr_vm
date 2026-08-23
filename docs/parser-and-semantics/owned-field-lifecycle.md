---
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_system.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_types.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_switch_patterns.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
  - zr_vm_parser/include/zr_vm_parser/diagnostic_builder.h
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_class.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_struct.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_flow.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_call.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_using_plugin_guard_escape.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_using_plugin_guard_escape_expression.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_using_plugin_guard_escape_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_using_plugin_guard_escape_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_using_plugin_guard_escape.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_closure.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_lambda.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_task_effects.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_struct.c
  - zr_vm_core/src/zr_vm_core/ownership.c
  - zr_vm_core/src/zr_vm_core/ownership_resource.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c
  - zr_vm_language_server/src/zr_vm_language_server/incremental_parser.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_support.c
  - zr_vm_core/src/zr_vm_core/module/module_prototype.c
  - zr_vm_core/src/zr_vm_core/object/object.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_system.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_types.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
  - zr_vm_parser/include/zr_vm_parser/diagnostic_builder.h
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_class.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_struct.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_call.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_closure.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_lambda.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_task_effects.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_struct.c
  - zr_vm_core/src/zr_vm_core/ownership.c
  - zr_vm_core/src/zr_vm_core/ownership_resource.c
  - zr_vm_language_server/src/zr_vm_language_server/incremental_parser.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_support.c
  - zr_vm_core/src/zr_vm_core/module/module_prototype.c
  - zr_vm_core/src/zr_vm_core/object/object.c
plan_sources:
  - user: 2026-05-16 Rust-First using / Ownership 语义收敛计划
  - .codex/plans/Rust-First using  Ownership 语义收敛计划.md
  - user: 2026-04-08 Rust-First Ownership / GC 分层设计
  - .codex/plans/Rust-First Ownership  GC 分层设计.md
  - user: 2026-06-17 using 泛型所有权关键字改造
  - docs/plans/using/01-ownership-as-generics.md
tests:
  - tests/parser/test_type_inference.c
  - tests/parser/test_parser.c
  - tests/parser/test_compiler_features.c
  - tests/parser/test_resource_unique_drop.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/module/test_module_system.c
  - tests/fixtures/reference/core_semantics/ownership_using_resource_lifecycle/generic_session_lifecycle_pass.zr
  - tests/fixtures/projects/classes/src/math.zr
  - tests/acceptance/2026-06-17-ownership-generics-p1.md
doc_type: module-detail
---

# Owned Field Lifecycle

## 目标

ownership world 里的字段生命周期现在直接写在字段类型上，而不是再额外叠一层字段级 `%using` 语法。

当前面向用户的规则是：

- 持有型字段只使用 `Unique<T>` / `Shared<T>` / `Weak<T>`
- 借用只使用 `ref T` / `ref readonly T`，并以 lexical block 限定 lifetime
- 所有权转换只使用 `own T(...)`、`share(owner)`、`degrade(shared)`、`wake(weak)`、`intoGc(owner)` 和 `drop(owner)`
- 语句或 block 级 `using` 只负责可关闭资源的 deterministic cleanup，不再承担 borrow/loan 兼容 lowering
- 字段级 legacy `%using` 语法已经移出 public surface
- owner 值跨入 plain GC 类型必须显式 `intoGc(owner)` 或通过 bridge 完成

## 源级写法

当前推荐写法：

```zr
resource class Bag {
    var value: Unique<Resource>;
    var cache: Shared<Cache>;
}

struct HandleBox {
    var handle: Unique<Resource>;
    var count: int;
}
```

迁移方向很直接：

- `%unique/%shared/%weak/%borrow/%loan` 和 `Borrow<T>/Loan<T>` 均为已删除语法；parser 只产出迁移错误，不构建可编译兼容 AST
- 旧字段级 lifecycle 标记迁到字段类型本身
- block cleanup 继续使用语句级 `using`
- parser 对 legacy field-scoped `%using` 产出迁移诊断，不再把它当有效字段 surface

## Parser 与语义层

parser 现在把字段生命周期分成两个世界：

- 字段类型里的 ownership qualifier
  - `Unique<T>`
  - `Shared<T>`
  - `Weak<T>`
  - `ref readonly T`（只允许非逃逸引用位置）
  - `ref T`（只允许非逃逸引用位置）
  - 其他非 owner qualifier 仍只表示类型能力，不自动变成 owner teardown surface
- 语句级 `using`
  - 继续表示 block / scope 级 close fence

这意味着字段语义不再依赖“字段上是否额外写了 `%using` 前缀”，而是直接依赖字段类型。

semantic analyzer 侧当前做两件事：

1. 始终为字段注册正常的 field symbol。
2. 当字段类型是 `Unique<T>` / `Shared<T>` / `Weak<T>` 时，登记 deterministic cleanup metadata。

所有权泛型路径不会把 `Unique` / `Shared` 等 wrapper 当普通用户类型存进 prototype metadata。字段 metadata 仍写入 inner type name，例如 `Unique<Resource>` 的字段类型名是 `Resource`，同时通过 `ownershipQualifier` 保存 owner kind。

源码使用 `own T(...)` 创建 unique owner，通过 `share(owner)` 建立 shared owner，通过 `degrade(shared)` 建立 weak handle。`Unique<T>(value)`、`Shared<T>(value)`、`Weak<T>(value)`、`Borrow<T>(value)` 和 `Loan<T>(value)` 均不再是源码构造 surface。

所有权控制只允许 `share(owner)`、`degrade(shared)`、`wake(weak)`、`intoGc(owner)` 和 `drop(owner)` 五个 reserved intrinsic。readonly/mutable view 分别写作 `var view: ref readonly T = ref owner` 与 `var view: ref T = ref owner`。`.` / `?.` 只访问目标的真实成员；旧 `borrow()`、`loan()`、`release()`、`detach()` 和 ownership member-call 拼写会在 type inference/compiler 边界直接失败并给出新语法建议，不再降级成 `OWN_*` 操作。

cleanup plan 里仍保留原来的区分：

- `ZR_DETERMINISTIC_CLEANUP_KIND_STRUCT_VALUE_FIELD`
- `ZR_DETERMINISTIC_CLEANUP_KIND_INSTANCE_FIELD`

因此 language server / semantic metadata 仍然能区分：

- struct value field teardown
- class instance field teardown

只是触发条件已经从“显式 field-scoped `%using`”改成“字段类型本身是 owner”。

## Rust-First Ownership Boundary

`using` 现在只表达 scope cleanup fence，不负责推断 owner，也不在值逃逸时自动把 owner 变成 weak。owner 状态仍由类型和显式操作表达：

- `Unique<T>` 表示唯一 owner，可建立 `ref` view、传给 `share(owner)` 或通过 `intoGc(owner)` 返回 GC world
- `Shared<T>` 表示引用计数 owner，可通过 `degrade(shared)` 显式创建 `Weak<T>`
- `Weak<T>` 只能从允许 weak 的 owner 显式创建；可用 `wake(weak)` 显式恢复 nullable shared owner，也可直接通过 `.` / `?.` 访问目标
- `ref readonly T` 和 `ref T` 不能逃逸到 return、持有字段、闭包、全局或跨 async/thread 边界

当前 surface 写作 `var view: ref readonly T = ref owner`、`var view: ref T = ref owner`、`share(owner)`、`degrade(shared)`、`wake(weak)`、`intoGc(owner)` 和 `drop(owner)`。旧百分号调用与旧 ownership member-call 只保留定向失败诊断。

旧 `%unique/%shared/%weak/%borrow/%loan T` 类型语法会发出 `legacy_syntax_removed` error；该错误会阻断生产编译。LSP 可以基于同一诊断提供迁移编辑，但不会把旧源码当成有效程序。

类型系统不允许 `Unique<T>` 或 `Shared<T>` 隐式流入 plain `T`。plain 类型属于普通 GC world；owner 对象跨过去必须显式 `intoGc(owner)` 或通过运行时 bridge，之后恢复普通 GC tracing/barrier。这样 ownership world 的零 GC 路径是可证明的：owner 对象不会被普通 GC 提前回收，也不会因为一次普通赋值、字段写入或函数调用参数传递悄悄离开 owner graph。赋值/变量初始化、字段赋值表达式和调用参数路径会优先给出 owner-to-plain 专用诊断：`Owned value cannot flow into a plain GC value implicitly`，而不是把它降级成普通类型不匹配或 overload 失败；普通泛型外壳里的 ownership 实参也会递归检查，`Box<Unique<T>>` / `Box<Shared<T>>` 不能仅因外层 canonical name 相同而流入 plain `Box<T>`。

插件 guard payload 是 guard-scoped plain GC object，不能通过 ownership promotion 逃逸；所有权控制只允许 reserved intrinsic 形式，而 `share(owner)` 不接受 plain module payload。`Module.share()` 只按普通 module member lookup 解释：真实 export 可以被调用，但该表达式不会产生 owner，也不会降低为 `OWN_SHARE`。guard 命中后 compiler 会在源语言不可见的位置创建隐藏 `Shared` owner，并在 scope cleanup 中发出 `OWN_RELEASE`，可见 binding 始终只是 guard 内的 module view。native registry 通过 core ownership strong-ref observer 追踪这个隐藏 owner，并在 scope cleanup 后让 `ZrLibrary_NativeRegistry_GetModuleRefCount(global, name)` 回到 0。

以下 Borrow/Loan 分析说明是破坏性切换前的内部实现记录。当前源码不能声明
`Borrow<T>` / `Loan<T>`，也不能调用 `%borrow/%loan`；对应 metadata、逃逸检查和
runtime opcode 只作为旧制品/迁移测试的兼容边界保留。新源码借用统一由
`ref`、`ref readonly` 与 `scoped ref` 的 place/region 检查表达。

插件 guard 也已有 compiler 侧基础逃逸拦截：`using` block 内的 guard-scoped module handle、块内别名或 callable member reference 不能通过 return、throw、out、普通调用参数、控制流条件中的调用参数、switch case 表达式中的调用参数、对象字段、数组元素、type query / prototype wrapper、decorator metadata、parameter default metadata、signature type metadata、foreach binding type metadata、generator 延迟输出或闭包/嵌套函数捕获越过 guard scope；try/catch/finally、generator/out、switch case value、type-query/prototype wrapper、declaration/member decorators、函数/方法/meta function parameter default/signature type、foreach binding type 和带表达式 break/continue 内的同类路径也会被扫描。typed/no-annotation `DynamicModule<T>` import guard payload binding 也会在 block 前复用同一 scanner，直接拒绝 `return m` / `throw m` / `out m` 这类 payload 逃逸；内层 block / `if` / loop 把插件值赋给外层局部时，外层局部会继续保留插件别名 taint。nested callable body 捕获扫描会登记 parameter、varargs、裸局部声明和解构局部声明 shadow，child scope 同名 binding 不会被误判为外层 guard handle。task-effect validation 同样拒绝 guard-scoped binding 跨 `await` 后继续读取。guard 命中后的隐藏 scoped owner cleanup 覆盖当前 block 生命周期，registry owner refcount API 可观察该 owner 生命周期；完整跨区域/全局流动分析仍是后续工作。

成员赋值左值也属于插件 guard 的字段/容器持久化边界。`box.handle = plugin` 或 `box[key] = plugin` 这类 primary/member access 写入 guard-scoped module handle 时会报告 `plugin_type_escape ... through field/container`；只有写入 guard block 局部裸标识符的 `alias = plugin` 仍作为受控别名传播。

block expression 的 task-effect alias 传播只取最后一条 expression statement 作为表达式结果；前面的声明或副作用语句仍按正常 task-effect traversal 校验，但不会把自己的 binding kind 作为 initializer 结果传播给外层 `var alias = { ... };`。这让 `{ var marker = 0; plugin; }` 与直接 `plugin`、条件/逻辑/容器 initializer 一样受到 await-boundary 约束，同时避免把无返回值 block 内的临时 statement 误当作外层 alias。

显式 function-call generic arguments 也属于同一插件 guard 边界。compile-time scanner 会在普通调用参数前扫描 `SZrFunctionCall.genericArguments`，按 signature metadata 处理类型实参、嵌套泛型/tuple/function type、array-size expression，并把 const-expression 泛型实参交给表达式边界扫描；return/throw/break/continue/out 这类 flow expression 会先跑 side-effect metadata scan，避免 `return sink<math.Vector>()` 在检查 guard-scoped `math` 之前先落到 unresolved call。task-effect validator 同步扫描 function-call generic arguments，并验证 type metadata root identifier，因此 `await` 后的 `sink<plugin + 0>()` 或 `sink<plugin.Vector>()` 会和普通表达式读取一样触发 `Plugin guard binding 'plugin' cannot be used after an await boundary`。

Union variant payload 解构也遵循同一条默认借用边界。`switch` case 和 `using` variant guard 从 `Unique<T>` / `Shared<T>` owner payload 绑定出的局部变量会先通过 `OWN_VIEW_SHARED` 形成非消费 `ref readonly T` view，不能作为 `share(...)`、`intoGc(...)` 或 `drop(...)` 的 owner operand；声明为 `ref` / `ref readonly` 的 payload 保持对应引用模式。`Weak<T>` payload 保持 weak，需要显式 `wake(weak)` 取得 nullable `Shared<T>` 后再访问目标。块式 `using` 已支持显式 `move` pattern 来承担真正转移 union payload owner 的语义：`using (var [move handle]: Resource.Open = resource)` 或 `using (var {move handle}: Resource.Open = resource)` 会保留 payload 的声明 owner 类型，并清空 matched inline union payload。`switch` case pattern 也支持同一 transfer 语义：tuple 写 `(Open(move handle))`，struct 写 `(Open { handle: move h })` 或 `(Open { move handle })`，其中 struct switch pattern 方向是 `field: local`。这些显式 move 路径都会清空 matched inline union payload，避免后续 active-variant drop 再释放该 owner。

弱引用也保持显式：只有 `degrade(shared)` 能创建 `Weak<T>`，编译器不会在逃逸、存储或容器插入时自动降级。`wake(weak)` 显式尝试留存并返回 nullable `Shared<T>`；weak receiver 的 `.` 与 `?.` 始终按目标对象成员访问解释，不承担所有权操作。目标已失效时，`.` 抛出可捕获的 `NullReferenceError`，`?.` 返回 `null` 并短路后续 suffix 与调用参数求值。`Shared<T>` 不引入 cycle collector；需要形成父子或图结构时，反向边应显式存为 `Weak<T>`。

## Prototype Metadata 与 Runtime 恢复

编译器继续把字段生命周期信息序列化进 prototype metadata，关键位包括：

- `ownershipQualifier`
- `ownershipBuiltinKind`（语句级 `using` cleanup plan 用于记录 owner cleanup 选择）
- `callsClose`
- `callsDestructor`
- `declarationOrder`

对当前 direct owner field 来说：

- `reservedRemovedUsingManaged` 只是必须保持为零的 ABI tombstone，不代表 public surface
- `ownershipQualifier` 成为恢复 managed-field 行为的主入口

module prototype materialization 恢复 managed field 时，当前以 `ownershipQualifier != NONE` 为主判据，因此 direct owner field 可以稳定恢复为 runtime managed-field table。

这让后续行为继续统一：

- field teardown
- owner field override / replace
- struct value cleanup
- class instance cleanup

## Resource owner fields

`resource class` fields reuse the canonical managed-field metadata, but Drop is now deterministic
for direct `Unique<Resource>` owners:

- construction marks only fields that were successfully initialized;
- partial-construction unwind skips the resource custom destructor;
- initialized owner fields drop in reverse declaration order;
- full Drop executes the resource custom destructor first, then fields;
- the lifecycle state makes reentrant/repeated Drop idempotent.

The runtime does not reconstruct this behavior from a field name or source syntax. It consumes the
prototype resource modifier, `ownershipQualifier`, managed-field presence, and declaration order.
See `resource-unique-drop.md` for the complete construction and VM/AOT contract.

## Stable Shared/Weak resource controls

`Shared<Resource>` and `Weak<Resource>` fields use one stable process-local control block rather
than linked weak stack slots. Shared field copy increments the strong count, Weak field copy
increments the explicit weak count, and reverse-order resource field teardown releases each
stored handle exactly once. Releasing the final strong handle marks the control dead before the
resource custom Drop body runs, so a Weak back-reference cannot resurrect the resource during
Drop. Explicit Weak fields keep the dead control alive until their own teardown.

The compiler warns on process-local resource `Shared<Self>` and reciprocal Shared field edges with
the structured `resource_shared_strong_cycle` diagnostic. A Weak reverse edge is the intended way
to break the ownership cycle. See `resource-shared-weak.md` for control counts, isolation-domain
rules, cleanup mirrors, and the current nullable-upgrade compatibility boundary.

## 当前边界

这轮收敛后的边界是：

- `Unique<T>` / `Shared<T>` / `Weak<T>` 与 `ref readonly T` / `ref T` 是唯一生产 ownership/reference surface
- `share(owner)` / `degrade(shared)` / `wake(weak)` / `intoGc(owner)` / `drop(owner)` 已接入 type inference、compiler lowering 和 runtime 验证
- direct `%unique/%shared/%weak/%borrow/%loan` type syntax、`Borrow<T>/Loan<T>` 以及 `.borrow()/.loan()/.release()/.detach()` 均为硬错误
- 语句级 `using` 是唯一写法，`%using` 只产生 `legacy_syntax_removed` 诊断
- field-scoped `%using` 只剩迁移诊断，不再是语言设计目标
- `using` cleanup plan 已记录 owner qualifier 与 release builtin metadata；`Unique<T>` / `Shared<T>` 在正常 scope 退出、`return` 和跨出 using 的 `break` 路径已落到 `OWN_RELEASE`
- lexical block 内的 `ref readonly` / `ref` binding 在 scope-end 结束 view；`using(ref owner)` 被拒绝，不再进入旧 `Loan<T>` source-slot 归还 lowering
- `ref readonly T` / `ref T` 赋值、变量初始化、字段赋值表达式和调用参数逃逸诊断已接入；只读 view 只能流向只读 view，mutable view 可以在同一有效 region 内流向 mutable 或 readonly view
- `ref readonly T` / `ref T` 脚本级 `pub/pro var` 导出全局逃逸诊断已接入，并会递归拒绝普通泛型外壳中嵌套的 reference 实参；private 顶层临时 owner/reference 仅保留给受控 runtime smoke path
- `ref readonly T` / `ref T` 闭包捕获逃逸诊断已接入；闭包和嵌套函数不能保存直接或嵌套在普通泛型实参里的 reference view
- `ref readonly T` / `ref T` async/task await-boundary 基础诊断已接入；显式 mutable view、嵌套 reference typed local 和 `using` guard `else` 分支在 `await` 后不能继续使用已越界 view；`import(...)` guard binder、默认 `@Available(m: Module)` payload binding，以及由赋值、条件/逻辑/type-cast、`type(...)`/prototype wrapper、array/object/key-value/unpack 容器 initializer、construct/decorator expression 和普通 `=` assignment-expression initializer 传播出的 guard alias 也不能在同一 guard body 内跨 `await` 后继续使用；父 context 已跨 `await` 后继承到 nested function/lambda 的旧 reference/affine/plugin guard binding 也会触发同一诊断，fresh child local/parameter 不受影响
- `ref readonly T` / `ref T` / `Shared<T>` / `Weak<T>` 嵌入 thread `Send` / `Sync` 泛型实参时的基础拒绝已接入；thread marker 检查会递归扫描 `elementTypes`
- 普通泛型外壳中的 reference view、`Unique<T>` 和 `Shared<T>` 实参会递归参与赋值和类型兼容性检查，不能隐式流入 plain 泛型目标
- `Unique<T>` / `Shared<T>` 赋值/变量初始化、字段赋值表达式和调用参数进入 plain `T` 会触发 owner-to-plain 专用诊断，要求显式 `intoGc(owner)` bridge
- `where T: owner` 已接入 generic constraint metadata 和调用约束检查；更细粒度的 `unique/shared/weak` 约束仍是后续扩展
- plugin guard 的 return/throw/out/callable-member-return/call-argument/constructor-argument/switch-case-call-argument/object-field/array-element/template-interpolation/generator-out/closure-capture/type-query/prototype-wrapper/decorator/parameter-default/signature-type/foreach-binding-type 基础 `plugin_type_escape` 已接入；typed/no-annotation `DynamicModule<T>` import guard payload binding 复用同一 scanner，并保留内层 region 赋给外层局部的插件 alias taint。条件、for step、表达式语句和变量 initializer 中的 assignment expression 也会传播 guard alias taint；nested callable shadow 与跨 await 使用均有专门检查。guard payload 不存在 public `Module.share()` 逃逸 API，guard 命中后的 hidden scoped shared owner 已登记 scope `OWN_RELEASE` cleanup；registry owner refcount API、descriptor safe unload/cache invalidation 与 `plugin_unknown_export` 稳定诊断已接入，更完整 async/跨区域/全局流动的 region 逃逸检查仍是后续工作
- block expression initializer 的 task-effect alias 传播已按最后一条 expression statement 收口；`var alias = { var marker = 0; plugin; };` 会把 `alias` 登记为 plugin guard binding，跨 `await` 后读取会触发同一 plugin guard await-boundary 诊断
- 成员赋值左值 `box.handle = plugin` / `box[key] = plugin` 已按 field/container 持久化边界处理；guard-scoped module handle 写入成员存储会报告 `plugin_type_escape ... through field/container`，局部裸标识符 alias 传播不受影响
- 显式 function-call generic arguments 已进入 compile-time `plugin_type_escape` 与 task-effect await-boundary 扫描；`return sink<math.Vector>()` 先报 `plugin_type_escape ... through signature type`，`await` 后 `sink<plugin + 0>()` 或 `sink<plugin.Vector>()` 先报 plugin guard await-boundary 诊断
- union variant payload 的 `Unique<T>` / `Shared<T>` 默认解构会通过 `OWN_VIEW_SHARED` 形成 `ref readonly T`；块式 `using` 和 `switch` case 显式 move 解构会跳过 view lowering、保留声明 owner 类型并清空 matched payload

仓库内部仍保留少量 legacy metadata 位用于兼容已存在的编译产物结构，但新源码路径不再依赖它们表达字段 owner 生命周期。

## 验证覆盖

当前已对齐的验证包括：

- parser
  - 直接 owner field 可解析
  - legacy field-scoped `%using` 报迁移诊断
  - legacy `%unique/%shared/%weak/%borrow/%loan T` type syntax 报 `legacy_syntax_removed` error，不进入生产编译
- compiler / prototype metadata
  - direct `Unique<T>/Shared<T>` field 写入 ownership metadata
  - `Unique<Resource>` / `Shared<Resource>` field 写入 inner type name 与 ownership metadata
  - `own T(...)`、`share(owner)`、`degrade(shared)`、`wake(weak)`、`intoGc(owner)` 和 `drop(owner)` 发出对应 ownership opcode family
  - `.borrow()/.loan()/.release()/.detach()` 在 lowering 前失败
  - `generic_session_lifecycle_pass.zr` 真实 reference fixture 会组合 `Unique<T>` / `Shared<T>` / `ref readonly T` / `ref T` / `Weak<T>`、`intoGc(owner)`、`wake(weak)` 和 `drop(owner)`，compiler integration 读取该 `.zr` 后检查 ownership opcode family 并执行生命周期断言
  - legacy field-scoped `%using` 不再作为新 surface 写入
- language server semantic metadata
  - direct owner field 会登记 cleanup plan
  - `using` resource cleanup plan 会记录 owner qualifier 与 cleanup builtin kind
  - `using (owner)` owner 泛型路径会在正常 scope 退出、`return` 和跨出 using 的 `break` 路径发出 `OWN_RELEASE`
  - lexical block 内的 `ref readonly` / `ref` binding 会按 region 结束 view
  - `using(ref owner)`、`using(Loan<T>(owner))` 和 `using(owner.loan())` 均被拒绝，旧 source-slot resolver 已从生产 compiler 删除
  - Borrow/Loan 写入 plain/持有型目标会触发专用逃逸诊断
  - Unique/Shared 写入 plain 目标会触发 owner-to-plain 专用诊断
  - Borrow/Loan 写入 plain 字段会触发专用逃逸诊断
  - Unique/Shared 写入 plain 字段会触发 owner-to-plain 专用诊断
  - Borrow/Loan 作为 plain 参数传入会触发专用逃逸诊断
  - Unique/Shared 作为 plain 参数传入会触发 owner-to-plain 专用诊断
  - Borrow/Loan/Unique/Shared 藏在普通泛型实参中写入 plain 泛型目标时会触发对应逃逸诊断
  - Borrow/Loan 作为脚本级 `pub/pro var` 导出全局类型会触发 `Borrowed and loaned owners cannot escape through exported globals`
  - Borrow/Loan 藏在导出全局普通泛型实参中也会触发同一 exported-global 逃逸诊断
  - Borrow/Loan 或嵌套在普通泛型实参中的 Borrow/Loan 被闭包或嵌套函数捕获会触发 `Borrowed and loaned owners cannot escape through closure capture`
  - Borrow/Loan 形参、Borrow/Loan initializer 推断局部、显式 `Loan<T>` 局部、嵌套 Borrow/Loan typed local 以及 `using` guard `else` 分支在 `await` 后使用会触发 await-boundary 诊断
  - thread `Send` / `Sync` marker 会拒绝泛型实参里嵌套 Borrow/Loan/Shared/Weak 的线程库 wrapper
  - `using` 插件 guard 内的 binding、callable member reference 或块内别名通过 return、throw、out、调用参数、持久化容器、metadata、generator 延迟输出或闭包捕获逃逸时会触发 `plugin_type_escape`；typed/no-annotation `DynamicModule<T>` import guard payload 复用同一检查。内层 region 的 alias taint、nested callable shadow 和 await boundary 均由同一 guard-scoped fact 处理；不存在 source-level promotion escape hatch
  - 显式 function-call generic arguments 中引用 guard-scoped handle 会触发同一 compile-time/task-effect 边界：`return sink<math.Vector>()` 先报 `plugin_type_escape ... through signature type`，`await` 后 `sink<plugin + 0>()` 或 `sink<plugin.Vector>()` 先报 plugin guard await-boundary 诊断
  - `using` 插件 guard 内的 module handle 不存在公开 ownership promotion；`Module.share()` 是普通成员分派且从不发出 `OWN_SHARE`，`share(modulePayload)` 则因 plain operand 被拒绝
  - `using` 插件 guard 命中后会生成隐藏 shared owner，并在正常 scope 退出、return、break、continue cleanup 路径发出 `OWN_RELEASE`
  - LSP incremental parser 对 warning-only parser diagnostics 保留当前 AST / analyzer 状态，`LSP Legacy Ownership Type Warning Preserves Current AST` 覆盖该路径
  - struct/class field cleanup kind 仍可区分
  - `Unique<T>` / `Shared<T>` 不能隐式流入 plain GC declaration
  - `Weak<T>` 不能隐式传给 `ref readonly T` / `ref T` 参数
- generic constraints
  - `where T: owner` 会接受 `Unique<T>` 等 owner 泛型实参
  - plain `T` 显式实参会触发 owner constraint 诊断
- type inference / receiver compatibility
  - `Weak<T>` 不能隐式借用成 `ref readonly T` / `ref T`
  - weak receiver 的 `.` / `?.` 只查询目标成员；失效时分别抛 `NullReferenceError` 或短路为 `null`
- module runtime metadata
  - prototypeData roundtrip 后仍可恢复 managed field 表
- project fixture
  - `tests/fixtures/projects/classes/src/math.zr` 已迁到 direct owner field 语法
- reference fixture
  - `tests/fixtures/reference/core_semantics/ownership_using_resource_lifecycle/generic_session_lifecycle_pass.zr` 覆盖所有权泛型新 surface 的真实生命周期组合，manifest case 为 `ownership-generic-session-lifecycle-pass`
