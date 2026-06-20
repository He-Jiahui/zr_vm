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
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_expression_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_closure.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_lambda.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_task_effects.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_struct.c
  - zr_vm_core/src/zr_vm_core/ownership.c
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
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_expression_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_closure.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_lambda.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_task_effects.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_struct.c
  - zr_vm_core/src/zr_vm_core/ownership.c
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

- 推荐新写法使用 `Unique<T>` / `Shared<T>` / `Weak<T>` / `Borrow<T>` / `Loan<T>` 所有权泛型
- 推荐所有权状态转换使用零参数成员方法：`share()` / `weak()` / `borrow()` / `loan()` / `upgrade()` / `release()` / `detach()`
- `var field: %unique T`
- `var field: %shared T`
- `var field: %weak T` 用于打断共享强环
- 语句或 block 级 `using` 作为 lifetime fence；`%using` 作为兼容写法保留
- 字段级 legacy `%using` 语法已经移出 public surface
- owner 值跨入 plain GC 类型必须显式 `%detach` 或通过 bridge 完成

## 源级写法

当前推荐写法：

```zr
%owned class Bag {
    var value: Unique<Resource>;
    var cache: Shared<Cache>;
}

struct HandleBox {
    var handle: Unique<Resource>;
    var count: int;
}
```

迁移方向很直接：

- `%unique T` / `%shared T` / `%weak T` / `%borrow T` / `%loan T` 过渡期继续可用，并归一到所有权泛型语义
- 旧字段级 lifecycle 标记迁到字段类型本身
- block cleanup 继续使用语句级 `using`
- parser 对 legacy field-scoped `%using` 产出迁移诊断，不再把它当有效字段 surface

## Parser 与语义层

parser 现在把字段生命周期分成两个世界：

- 字段类型里的 ownership qualifier
  - `Unique<T>`
  - `Shared<T>`
  - `Weak<T>`
  - `Borrow<T>`
  - `Loan<T>`
  - `%unique`
  - `%shared`
  - `%weak`
  - `%borrow` / `%borrowed`
  - `%loan` / `%loaned`
  - 其他非 owner qualifier 仍只表示类型能力，不自动变成 owner teardown surface
- 语句级 `using` / `%using`
  - 继续表示 block / scope 级 close fence

这意味着字段语义不再依赖“字段上是否额外写了 `%using` 前缀”，而是直接依赖字段类型。

semantic analyzer 侧当前做两件事：

1. 始终为字段注册正常的 field symbol。
2. 当字段类型是 `Unique<T>` / `Shared<T>` / `Weak<T>` 或兼容 `%unique T` / `%shared T` / `%weak T` 时，登记 deterministic cleanup metadata。

所有权泛型路径不会把 `Unique` / `Shared` 等 wrapper 当普通用户类型存进 prototype metadata。字段 metadata 仍写入 inner type name，例如 `Unique<Resource>` 的字段类型名是 `Resource`，同时通过 `ownershipQualifier` 保存 owner kind。

`Unique<T>(value)`、`Shared<T>(value)`、`Weak<T>(value)`、`Borrow<T>(value)` 和 `Loan<T>(value)` 是内建构造 surface。parser 会把这些显式泛型调用改写为既有 ownership construct expression，compiler 继续发出原有 ownership builtin opcode，不引入新的 runtime owner layout。

`owner.share()`、`shared.weak()`、`owner.borrow()`、`owner.loan()`、`weak.upgrade()`、`owner.release()` 和 `owner.detach()` 是同一套所有权泛型的成员式操作 surface。type inference 按 receiver ownership qualifier 校验可用性并返回对应 wrapper 或 plain/null type；compiler 在 member-chain lowering 中直接发出 dedicated `OWN_*` opcode，不把这些名字当普通用户成员查找。

cleanup plan 里仍保留原来的区分：

- `ZR_DETERMINISTIC_CLEANUP_KIND_STRUCT_VALUE_FIELD`
- `ZR_DETERMINISTIC_CLEANUP_KIND_INSTANCE_FIELD`

因此 language server / semantic metadata 仍然能区分：

- struct value field teardown
- class instance field teardown

只是触发条件已经从“显式 field-scoped `%using`”改成“字段类型本身是 owner”。

## Rust-First Ownership Boundary

`using` 现在只表达 scope cleanup fence，不负责推断 owner，也不在值逃逸时自动把 owner 变成 weak。owner 状态仍由类型和显式操作表达：

- `Unique<T>` 表示唯一 owner，可显式 borrow、loan、share 或 `%detach`
- `Shared<T>` 表示引用计数 owner，可显式创建 `Weak<T>`
- `Weak<T>` 只能从允许 weak 的 owner 显式创建；不能隐式传给 `Borrow<T>`，访问目标前必须通过 `%upgrade` 或 nullable check
- `Borrow<T>` 和 `Loan<T>` 不能逃逸到 return、字段、闭包、全局或跨 async/thread 边界

新 surface 下推荐写作 `owner.borrow()`、`owner.loan()`、`owner.share()`、`owner.detach()`、`weak.upgrade()` 和 `owner.release()`；旧 `%borrow/%loan/%shared/%detach/%upgrade/%release` 调用形态只作为迁移/兼容语义继续存在。

旧 `%unique/%shared/%weak/%borrow/%loan T` 类型语法仍作为迁移糖兼容，但 parser 会发出 `legacy_ownership_type_syntax` warning，并建议改写为 `Unique<T>` / `Shared<T>` / `Weak<T>` / `Borrow<T>` / `Loan<T>`。该 warning 不会阻断 AST 构建或 LSP 语义分析。

类型系统不再允许 `%unique T` 或 `%shared T` 隐式流入 plain `T`。plain 类型属于普通 GC world；owner 对象跨过去必须显式 `%detach` 或通过运行时 bridge，之后恢复普通 GC tracing/barrier。这样 ownership world 的零 GC 路径是可证明的：owner 对象不会被普通 GC 提前回收，也不会因为一次普通赋值、字段写入或函数调用参数传递悄悄离开 owner graph。赋值/变量初始化、字段赋值表达式和调用参数路径会优先给出 owner-to-plain 专用诊断：`Owned value cannot flow into a plain GC value implicitly`，而不是把它降级成普通类型不匹配或 overload 失败；普通泛型外壳里的 ownership 实参也会递归检查，`Box<Unique<T>>` / `Box<Shared<T>>` 不能仅因外层 canonical name 相同而流入 plain `Box<T>`。

插件 guard 的 `.share()` 是当前明确允许的 plain-to-owner bridge。guard-scoped module handle 本身保持 plain GC object；`p.share()` / `m.share()` 通过 `ZrCore_Ownership_SharePlainValue` 建立 ownership control、把对象加入 GC ignore registry 并返回 `Shared<PluginModule>`，但不会消费或清空原 guard binding。释放该 shared handle 后，强引用计数归零并撤销 GC ignore，plain guard binding 仍只在 guard scope 内有效。该路径用于把插件生命周期显式延长到 guard 外，不能替代普通 owner-to-plain 赋值规则。默认 scoped guard 生命周期也已接到同一 bridge：guard 命中后 compiler 生成隐藏 `Shared` owner，并在 scope cleanup 中发出 `OWN_RELEASE`，可见 guard binding 仍保持 plain module 值。native registry 会通过 core ownership strong-ref observer 追踪这些显式/隐藏 shared owner，并在最后一个 owner 释放后让 `ZrLibrary_NativeRegistry_GetModuleRefCount(global, name)` 回到 0。

Borrow/Loan 的赋值、字段写入和调用参数路径已经先做 conservative 逃逸检查：`Borrow<T>` 只允许写入 `Borrow<T>` 目标，`Loan<T>` 只允许写入 `Loan<T>` 或降级的 `Borrow<T>` 目标；流入 plain 或持有型目标会在普通类型不匹配前报告 `Borrowed value cannot escape its owner` / `Loaned value cannot escape its owner`。这条检查位于赋值/变量初始化共用的 type-inference 入口、源码级 assignment expression、字段赋值表达式以及函数调用参数兼容/overload 失败路径，并会递归进入普通泛型实参，因此 `Box<Borrow<T>>` / `Box<Loan<T>>` 不能隐藏借用或借出值。脚本级 `pub` / `pro` 导出变量还会在导出 metadata 写入前拒绝 `Borrow<T>` / `Loan<T>`，并递归拒绝 `Holder<Borrow<T>>` / `Holder<Loan<T>>` 这类嵌套 borrow/loan 泛型实参，诊断为 `Borrowed and loaned owners cannot escape through exported globals`；private 顶层临时值仍可用于已有 ownership opcode/runtime smoke 测试。闭包和嵌套函数捕获会在 external variable 分析中拒绝 `Borrow<T>` / `Loan<T>`，并递归拒绝 `Holder<Borrow<T>>` / `Holder<Loan<T>>` 这类嵌套 borrow/loan 泛型实参，诊断为 `Borrowed and loaned owners cannot escape through closure capture`。async/task effect 边界会拒绝 Borrow/Loan 形参、Borrow/Loan initializer 推断局部、显式 `Loan<T>` 局部、嵌套 `Holder<Borrow<T>>` / `Holder<Loan<T>>` 局部以及 `using` guard `else` 分支在 `%await` 后继续使用，并报告对应 `Borrowed binding ... cannot be used after an await boundary` / `Loaned binding ... cannot be used after an await boundary`。thread `Send` / `Sync` marker 检查会递归扫描泛型实参，因此 `Transfer<Borrow<T>>`、`Shared<Loan<T>>` 这类跨线程 wrapper 不能隐藏 borrowed/loaned owner。

插件 guard 也已有 compiler 侧基础逃逸拦截：`using (var p = %import(...))` block 内未 `share()` 的 guard handle、块内别名或 callable member reference 不能通过 return、throw、out、普通调用参数、控制流条件中的调用参数、switch case 表达式中的调用参数、对象字段、数组元素、`%type(...)` / prototype wrapper、decorator metadata、parameter default metadata、signature type metadata、foreach binding type metadata、generator 延迟输出或闭包/嵌套函数捕获越过 guard scope；try/catch/finally、generator/out、switch case value、type-query/prototype wrapper、declaration/member decorators、函数/方法/meta function parameter default/signature type、foreach binding type 和带表达式 break/continue 内的同类路径也会被扫描。typed/no-annotation `DynamicModule<T>` `%import` guard payload binding 也会在 block 前复用同一 scanner，拒绝 `return m` / `throw m` / `out m` 这类未提升 payload 逃逸；内层 block / `if` / loop 把插件值赋给外层局部时，外层局部会继续保留插件别名 taint，foreach body 也会按 nested region 恢复 loop-local alias，catch 内临时别名则按嵌套 region 恢复。nested callable body 捕获扫描会登记 parameter、varargs、裸局部声明和解构局部声明 shadow，child scope 同名 local/parameter/destructuring binding 不再被误判为外层 guard handle；真正从 guard binder 初始化 alias 再 return/throw/out 的路径仍拒绝。task-effect validation 还会把 `%import` guard binder、默认 `@Available(m: Module)` tuple payload binding，以及同一 using body 内由赋值、条件/逻辑/type-cast、`%type(...)`/prototype wrapper、array/object/key-value/unpack 容器 initializer 和普通 `=` assignment-expression initializer 传播出的 guard alias 登记为 guard-scoped plugin binding；generator body、template interpolation、type-query operand、prototype wrapper target 以及 construct target/args 内的读取都会共享同一 `%await` 边界，拒绝同一 using body 内 `%await` 后继续读取，并报告 `Plugin guard binding ... cannot be used after an await boundary`；父 context 已跨 `%await` 后继承到 nested function/lambda 的旧 binding 会以 `inheritedAfterAwait` 保留该事实，子体读取继承的 Borrow/Loan/affine/plugin guard binding 触发同一诊断，fresh child local/parameter 不受影响。普通成员值和成员调用结果按结果类型处理。guard 命中后的隐藏 scoped owner cleanup 已覆盖当前 block 生命周期，registry owner refcount API 已能观察 shared owner 生命周期；后续完整 region 分析会继续覆盖更完整的 async/跨区域/全局流动和 descriptor safe unload/cache invalidation 语义。

成员赋值左值也属于插件 guard 的字段/容器持久化边界。`box.handle = plugin` 或 `box[key] = plugin` 这类 primary/member access 写入未 `share()` 的 guard handle 时会报告 `plugin_type_escape ... through field/container`；只有写入 guard block 局部裸标识符的 `alias = plugin` 仍作为受控别名传播。

block expression 的 task-effect alias 传播只取最后一条 expression statement 作为表达式结果；前面的声明或副作用语句仍按正常 task-effect traversal 校验，但不会把自己的 binding kind 作为 initializer 结果传播给外层 `var alias = { ... };`。这让 `{ var marker = 0; plugin; }` 与直接 `plugin`、条件/逻辑/容器 initializer 一样受到 await-boundary 约束，同时避免把无返回值 block 内的临时 statement 误当作外层 alias。

显式 function-call generic arguments 也属于同一插件 guard 边界。compile-time scanner 会在普通调用参数前扫描 `SZrFunctionCall.genericArguments`，按 signature metadata 处理类型实参、嵌套泛型/tuple/function type、array-size expression，并把 const-expression 泛型实参交给表达式边界扫描；return/throw/break/continue/out 这类 flow expression 会先跑 side-effect metadata scan，避免 `return sink<math.Vector>()` 在检查 guard-scoped `math` 之前先落到 unresolved call。task-effect validator 同步扫描 function-call generic arguments，并验证 type metadata root identifier，因此 `%await` 后的 `sink<plugin + 0>()` 或 `sink<plugin.Vector>()` 会和普通表达式读取一样触发 `Plugin guard binding 'plugin' cannot be used after an await boundary`。

Union variant payload 解构也遵循同一条默认借用边界。`switch` case 和 `using` variant guard 从 `Unique<T>` / `Shared<T>` / `Loan<T>` payload 绑定出的局部变量会先通过 `OWN_BORROW` 降级为 `Borrow<T>`，不能直接 `%release` / `%detach`；`Weak<T>` payload 保持 weak，仍需要显式 `%upgrade` 后访问。块式 `using` 已支持显式 `move` pattern 来承担真正转移 union payload owner 的语义：`using (var [move handle]: Resource.Open = resource)` 或 `using (var {move handle}: Resource.Open = resource)` 会保留 payload 的声明 owner 类型，并清空 matched inline union payload。`switch` case pattern 也支持同一 transfer 语义：tuple 写 `(Open(move handle))`，struct 写 `(Open { handle: move h })` 或 `(Open { move handle })`，其中 struct switch pattern 方向是 `field: local`。这些显式 move 路径都会清空 matched inline union payload，避免后续 active-variant drop 再释放该 owner。

弱引用也保持显式：`%weak` 不是逃逸时的自动降级，而是 store 点、构造点或容器插入点的显式设计选择。`%weak` receiver 只能直接调用 `%weak` 方法；普通 `%borrowed`/`%shared` target 方法需要先把 weak handle 升级成 checked owner。`%shared` 不引入 cycle collector；需要形成父子或图结构时，反向边应写成 `%weak`。

## Prototype Metadata 与 Runtime 恢复

编译器继续把字段生命周期信息序列化进 prototype metadata，关键位包括：

- `ownershipQualifier`
- `ownershipBuiltinKind`（语句级 `using` cleanup plan 用于记录 owner cleanup 选择）
- `callsClose`
- `callsDestructor`
- `declarationOrder`

对当前 direct owner field 来说：

- `isUsingManaged` 不再代表 public surface
- `ownershipQualifier` 成为恢复 managed-field 行为的主入口

module prototype materialization 恢复 managed field 时，当前以 `ownershipQualifier != NONE` 为主判据，因此 direct owner field 可以稳定恢复为 runtime managed-field table。

这让后续行为继续统一：

- field teardown
- owner field override / replace
- struct value cleanup
- class instance cleanup

## 当前边界

这轮收敛后的边界是：

- `Unique<T>` / `Shared<T>` / `Weak<T>` / `Borrow<T>` / `Loan<T>` 是所有权新 surface
- `share()` / `weak()` / `borrow()` / `loan()` / `upgrade()` / `release()` / `detach()` 成员式所有权 API 已作为新 surface 接入 type inference、compiler lowering 和 runtime 验证
- direct `%unique/%shared/%weak/%borrow/%loan` type syntax 继续作为兼容 surface
- 语句级 `using` 是推荐写法，`%using` 继续兼容
- field-scoped `%using` 只剩迁移诊断，不再是语言设计目标
- `using` cleanup plan 已记录 owner qualifier 与 release builtin metadata；`Unique<T>` / `Shared<T>` 在正常 scope 退出、`return` 和跨出 using 的 `break` 路径已落到 `OWN_RELEASE`
- `Borrow<T>` / `Loan<T>` 的直连本地 owner scope-end 已接入；`Borrow<T>` scope 退出结束借用，`Loan<T>(owner)` 和 `owner.loan()` scope 退出都会通过 `OWN_RETURN_LOAN` 把 loaned value 归还 source owner
- `Borrow<T>` / `Loan<T>` 赋值/变量初始化、字段赋值表达式和调用参数逃逸诊断已接入；Borrow 只允许流向 Borrow 目标，Loan 只允许流向 Loan 或 Borrow 目标
- `Borrow<T>` / `Loan<T>` 脚本级 `pub/pro var` 导出全局逃逸诊断已接入，并会递归拒绝 `Holder<Borrow<T>>` / `Holder<Loan<T>>` 这类嵌套 borrow/loan 泛型实参；private 顶层临时 owner/borrow/loan 仍保留给当前 runtime smoke path
- `Borrow<T>` / `Loan<T>` 闭包捕获逃逸诊断已接入；闭包和嵌套函数不能保存直接或嵌套在普通泛型实参里的借用/借出值
- `Borrow<T>` / `Loan<T>` async/task await-boundary 基础诊断已接入；显式 `Loan<T>` 局部、嵌套 Borrow/Loan typed local 和 `using` guard `else` 分支在 `%await` 后不能继续使用 Borrow/Loan；`%import` guard binder、默认 `@Available(m: Module)` payload binding，以及由赋值、条件/逻辑/type-cast、`%type(...)`/prototype wrapper、array/object/key-value/unpack 容器 initializer、construct/decorator expression 和普通 `=` assignment-expression initializer 传播出的 guard alias 也不能在同一 guard body 内跨 `%await` 后继续使用；父 context 已跨 `%await` 后继承到 nested function/lambda 的旧 Borrow/Loan/affine/plugin guard binding 也会触发同一诊断，fresh child local/parameter 不受影响
- `Borrow<T>` / `Loan<T>` / `Shared<T>` / `Weak<T>` 嵌入 thread `Send` / `Sync` 泛型实参时的基础拒绝已接入；thread marker 检查会递归扫描 `elementTypes`
- 普通泛型外壳中的 `Borrow<T>` / `Loan<T>` / `Unique<T>` / `Shared<T>` 实参会递归参与赋值和类型兼容性检查，不能隐式流入 plain 泛型目标
- `Unique<T>` / `Shared<T>` 赋值/变量初始化、字段赋值表达式和调用参数进入 plain `T` 会触发 owner-to-plain 专用诊断，要求显式 detach/bridge
- `where T: owner` 已接入 generic constraint metadata 和调用约束检查；更细粒度的 `unique/shared/weak` 约束仍是后续扩展
- plugin guard 的 return/throw/out/callable-member-return/call-argument/constructor-argument/switch-case-call-argument/object-field/array-element/template-interpolation/generator-out/closure-capture/type-query/prototype-wrapper/decorator/parameter-default/signature-type/foreach-binding-type 基础 `plugin_type_escape` 已接入，并覆盖 try/catch/finally、generator/out、switch case value、template string interpolation、type-query/prototype wrapper、declaration/member decorators、函数/方法/meta function parameter default/signature type、foreach binding type 和带表达式 break/continue 中的同类路径；typed/no-annotation `DynamicModule<T>` `%import` guard payload binding 也已复用同一 scanner，并会保留内层 region 赋给外层局部的插件 alias taint；条件、for step、表达式语句和变量 initializer 中的 assignment expression 也会传播 guard alias taint，因此 `if (alias = plugin)` / `var ok = (alias = plugin)` 后再返回 `alias` 会被拒绝；scanner 已拆为 orchestration、expression recursion、statement boundary 和 internal header，并在 nested callable body capture 中维护 shadowed-name stack，避免 child scope 同名 parameter/local/destructuring binding 被误判为外层 guard capture；task-effect validation 已拒绝 guard binder、默认 `@Available(m: Module)` payload binding、赋值/条件/逻辑/type-cast/`%type(...)`/prototype wrapper、array/object/key-value/unpack 容器 initializer、assignment-expression initializer 传播出的 guard alias 跨 await 使用，并会遍历 generator body、template string interpolation、container initializer、type-query operand、prototype wrapper target、construct target/args 和 decorator expression；block-local decorated function parsing 已接入，因此 `#plugin.Decorate(plugin)# func nested()` 在 using body 内也会被同一 await-boundary 检查覆盖；父 context 已跨 `%await` 后继承到 nested function/lambda 的旧 binding 会用 `inheritedAfterAwait` 继续触发该诊断，fresh child local/parameter 不受影响；guard-scoped module `.share()` 已能显式产生 `Shared` owner 并由 `%release` 管理；guard 命中后的 hidden scoped shared owner 已登记 scope `OWN_RELEASE` cleanup；registry owner refcount API 已接入；descriptor safe unload/cache invalidation 已接入；LSP 未知导出成员诊断已使用 `plugin_unknown_export` 稳定 code，更完整 async/跨区域/全局流动的 region 逃逸检查仍是后续工作
- block expression initializer 的 task-effect alias 传播已按最后一条 expression statement 收口；`var alias = { var marker = 0; plugin; };` 会把 `alias` 登记为 plugin guard binding，跨 `%await` 后读取会触发同一 plugin guard await-boundary 诊断
- 成员赋值左值 `box.handle = plugin` / `box[key] = plugin` 已按 field/container 持久化边界处理；未 `share()` 的 guard handle 写入成员存储会报告 `plugin_type_escape ... through field/container`，局部裸标识符 alias 传播不受影响
- 显式 function-call generic arguments 已进入 compile-time `plugin_type_escape` 与 task-effect await-boundary 扫描；`return sink<math.Vector>()` 先报 `plugin_type_escape ... through signature type`，`%await` 后 `sink<plugin + 0>()` 或 `sink<plugin.Vector>()` 先报 plugin guard await-boundary 诊断
- union variant payload 的 `Unique<T>` / `Shared<T>` / `Loan<T>` 默认解构已降级为 `Borrow<T>`，并通过 `OWN_BORROW` 形成运行时借用别名；块式 `using` 和 `switch` case 显式 move 解构已能保留声明 owner 类型并清空 matched payload

仓库内部仍保留少量 legacy metadata 位用于兼容已存在的编译产物结构，但新源码路径不再依赖它们表达字段 owner 生命周期。

## 验证覆盖

当前已对齐的验证包括：

- parser
  - 直接 owner field 可解析
  - legacy field-scoped `%using` 报迁移诊断
  - legacy `%unique/%shared/%weak/%borrow/%loan T` type syntax 报 `legacy_ownership_type_syntax` warning，同时继续构建 AST
- compiler / prototype metadata
  - direct `%unique/%shared` field 写入 ownership metadata
  - `Unique<Resource>` / `Shared<Resource>` field 写入 inner type name 与 ownership metadata
  - `Unique<T>(value)` 等构造调用发出对应 ownership builtin opcode
  - `share()` / `weak()` / `borrow()` / `loan()` / `upgrade()` / `release()` / `detach()` 成员调用发出 dedicated ownership opcode family
  - `generic_session_lifecycle_pass.zr` 真实 reference fixture 会组合 `Unique<T>` / `Shared<T>` / `Borrow<T>` / `Loan<T>` / `Weak<T>`、`detach()`、`upgrade()` 和 `release()`，compiler integration 读取该 `.zr` 后检查 ownership opcode family 并执行返回 mask `63`
  - legacy field-scoped `%using` 不再作为新 surface 写入
- language server semantic metadata
  - direct owner field 会登记 cleanup plan
  - `using` resource cleanup plan 会记录 owner qualifier 与 cleanup builtin kind
  - `using (owner)` owner 泛型路径会在正常 scope 退出、`return` 和跨出 using 的 `break` 路径发出 `OWN_RELEASE`
  - `using (Borrow<T>(owner))` 会在正常 scope 退出和 direct return 前清理借用
  - `using (Loan<T>(owner))` 和 `using (owner.loan())` 会在正常 scope 退出和 break 前通过 `OWN_RETURN_LOAN` 归还 source owner
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
  - Borrow/Loan 形参、Borrow/Loan initializer 推断局部、显式 `Loan<T>` 局部、嵌套 Borrow/Loan typed local 以及 `using` guard `else` 分支在 `%await` 后使用会触发 await-boundary 诊断
  - thread `Send` / `Sync` marker 会拒绝泛型实参里嵌套 Borrow/Loan/Shared/Weak 的线程库 wrapper
  - `using` 插件 guard 内未 `share()` 的绑定值、callable member reference 或块内别名通过 return、throw、out、普通/条件调用参数、switch case 调用参数、构造函数实参、对象字段、数组元素、模板字符串插值、`%type(...)`/prototype wrapper、decorator metadata、parameter default metadata、signature type metadata、generator 延迟输出或闭包捕获逃逸时会触发 `plugin_type_escape`；try/catch/finally、generator/out、switch case value、template string interpolation、type-query/prototype wrapper、declaration/member decorators、function/method/meta function parameter default/signature type 和带表达式 break/continue 中的同类逃逸路径也会触发该诊断；typed/no-annotation `DynamicModule<T>` `%import` guard payload binding 通过 `return m` / `throw m` / `out m`、switch case 调用实参、构造实参、模板插值、decorator metadata 或 signature type metadata 逃逸时也会触发同一诊断；内层 `if` / block / loop 中赋给外层局部后再 return/throw/out 的路径也会保留插件别名并被拒绝；条件、for step、表达式语句和变量 initializer 中的 `alias = plugin` 赋值表达式会传播同一插件别名；nested callable body 的同名 parameter/local/destructuring shadow 不触发外层 guard capture 误报，但从 guard binder 初始化 alias 后返回仍拒绝；guard binder、默认 `@Available(m: Module)` payload binding 和同一 body 内赋值/条件/逻辑/type-cast/`%type(...)`/prototype wrapper/assignment-expression initializer 传播出的 guard alias 在 guard body 内跨 `%await` 后读取会触发 plugin guard await-boundary 诊断，template interpolation 和 type-query/prototype wrapper 中的 await 后读取也会触发同一诊断；父 context 已跨 `%await` 后继承到 nested function/lambda 的旧 binding 会用 `inheritedAfterAwait` 继续触发该诊断，fresh child local/parameter 不受影响
  - 显式 function-call generic arguments 中引用 guard-scoped handle 会触发同一 compile-time/task-effect 边界：`return sink<math.Vector>()` 先报 `plugin_type_escape ... through signature type`，`%await` 后 `sink<plugin + 0>()` 或 `sink<plugin.Vector>()` 先报 plugin guard await-boundary 诊断
  - `using` 插件 guard 内的 module handle 可通过零参数 `.share()` 显式提升为 shared owner；plain source 不被消费，提升结果可经 `%release` 释放
  - `using` 插件 guard 命中后会生成隐藏 shared owner，并在正常 scope 退出、return、break、continue cleanup 路径发出 `OWN_RELEASE`
  - LSP incremental parser 对 warning-only parser diagnostics 保留当前 AST / analyzer 状态，`LSP Legacy Ownership Type Warning Preserves Current AST` 覆盖该路径
  - struct/class field cleanup kind 仍可区分
  - `%unique/%shared` 不能隐式流入 plain GC declaration
  - `%weak` 不能隐式传给 `%borrowed` 参数
- generic constraints
  - `where T: owner` 会接受 `Unique<T>` 等 owner 泛型实参
  - plain `T` 显式实参会触发 owner constraint 诊断
- type inference / receiver compatibility
  - `%weak` 不能隐式借用成 `%borrowed`
  - `%weak` receiver 不能直接调用 `%borrowed` 方法
- module runtime metadata
  - prototypeData roundtrip 后仍可恢复 managed field 表
- project fixture
  - `tests/fixtures/projects/classes/src/math.zr` 已迁到 direct owner field 语法
- reference fixture
  - `tests/fixtures/reference/core_semantics/ownership_using_resource_lifecycle/generic_session_lifecycle_pass.zr` 覆盖所有权泛型新 surface 的真实生命周期组合，manifest case 为 `ownership-generic-session-lifecycle-pass`
