---
related_code:
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/closure.h
  - zr_vm_core/include/zr_vm_core/call_info.h
  - zr_vm_core/include/zr_vm_core/call_binding.h
  - zr_vm_core/include/zr_vm_core/typed_call_binding.h
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_call.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_call_binding.c
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/closure.c
  - zr_vm_core/src/zr_vm_core/call_binding.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/closure.c
  - zr_vm_core/src/zr_vm_core/call_binding.c
  - zr_vm_core/src/zr_vm_core/typed_call_binding.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_call.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_call_binding.c
tests:
  - tests/function/test_named_arguments.c
  - tests/parser/test_typed_call_binding.c
  - tests/parser/test_call_binding_pipeline.c
  - tests/core/test_call_binding_runtime.c
  - tests/parser/test_reference_receiver_call_boundary.c
  - tests/parser/test_reference_escape_closure_suspension.c
  - zr_vm_aot/tests/parser/test_tail_call_pipeline.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/core-runtime/call-binding.md
  - docs/core-runtime/typed-call-binding.md
  - docs/plans/syntax/2026-07-18-02-reference-syntax-borrow-checker-design.md
doc_type: module-detail
---

# 函数、闭包与调用

**状态：`current`；跨后端 typed call/AOT call binding 为 `experimental`。**

## 函数声明和 callable 类型

```zr
fn add(left: int, right: int): int {
    return left + right;
}

let mapper: fn(int) -> int = fn(value: int): int => value * 2;
```

函数类型由返回 TypeRef、参数顺序/类型、passing mode、泛型参数和 varargs 状态组成。`fn(int) -> int` 是 callable type；`fn(value: int): int { ... }` 是匿名函数表达式。两者不要与旧的 `=>` callable type 或 `func` 语法混用。

## 参数绑定

普通 value 参数按声明顺序绑定。调用者可以使用命名参数、默认值和（声明允许时）可变参数：

```zr
fn connect(host: string, port: int = 80, secure: bool = false): void { }

connect(host: "localhost", secure: true);
```

编译器先收集参数名和 declaration order，再检查：重复命名、未知名称、位置参数出现在命名参数之后、缺少必需参数、默认值类型和 varargs 尾部。默认表达式只在调用点缺省时求值；求值顺序和异常传播仍由 call lowering 保证。

`in`、`ref`、`ref readonly`、`scoped ref`、`out` 是类型/参数 contract 的一部分：

```zr
fn inspect(value: in Data): void { }
fn mutate(value: ref Data): void { }
fn create(value: out Data): void { value = init Data(); }
```

`ref/out` 实参必须显式写 marker 并指向 Place；`in` 可以物化临时值；`out` 在所有正常返回边上必须完成赋值。调用 contract 不会通过 overload 猜测 passing mode。

## 调用类别

| 类别 | 典型源码 | 编译期信息 | 运行时目标 |
|---|---|---|---|
| direct | `add(1, 2)` | function identity/signature | VM function 或 AOT thunk |
| member | `object.run()` | receiver type + member descriptor | field/method/property accessor |
| virtual/interface | `shape.draw()` | prototype/interface slot | receiver descriptor + dispatch slot |
| typed function value | `callback(x)` | required callable signature | 当前 value 的 VM/native/AOT callable |
| meta/dynamic | `@call(target, args)` 或未静态解析调用 | metadata/runtime contract | runtime resolver，可能 deopt |
| property get/set | `object.value`, `object.value = x` | property/accessor identity | getter/setter hidden callable |
| tail call | `return next(args)` | tail-position + cleanup facts | frame reuse或显式回退 |

`.` 和 `?.` 只访问 receiver target。ownership intrinsic（`share`、`degrade`、`wake`、`intoGc`、`drop`）由独立语义节点处理；如果目标存在同名真实成员，仍按普通 member dispatch。

## 编译阶段

compiler 对每个 call site 依次完成：

1. 解析 callable expression 和 receiver/argument evaluation order。
2. 依据 canonical TypeId、SymbolId、PlaceId 做 overload、generic inference、named/default mapping 和 passing-mode checks。
3. 形成 `SZrCallBindingContract`：binding kind、target/signature/owner metadata tokens、signature hash、module signature hash、layout version/hash、operation 和 dispatch slot。
4. 把语义 call 降成 Semantic IR，再选择 `FUNCTION_CALL`、`META_CALL`、`CALL_TYPED`、property opcode 或后续 ExecBC quickening。
5. 把可持久化字段写入 `.zri/.zro`；绝不写入进程函数指针或 closure 地址。

## `SZrCallInfo` 与 frame

每个活动调用由 `SZrCallInfo` 链表示。它保存 function base/top、metadata function、前后 frame、program counter/native continuation、expected return count、return destination、argument source frame、yield/tail/native flags 和 debug frame generation。栈增长/重定位后，`SZrFunctionStackAnchor` 以相对 offset 重建指针，避免把旧地址当作稳定引用。

普通 VM frame 在 `SZrState.stackBase..stackTop` 中分配；inline struct 参数可能直接占用多个 byte slots。native call 使用 `ZrLibCallContext`，其中 `argumentValues`/`argumentValuePointers` 是当前调用视图，`functionBaseAnchor` 和 `stackBasePointer` 用于 GC/重定位安全。

## 闭包捕获

编译器为逃逸的局部创建 `SZrClosureValue`。变量仍在栈上时，closure value 指向栈槽并挂入 open-value list；离开作用域、yield、await 或确定需要逃逸时，`ZrCore_Closure_CloseStackValue` 把值复制到 managed closed storage。`SZrClosure` 保存 VM function 和 capture slots；`SZrClosureNative` 保存 native callback、descriptor、generation 和 captures。

捕获分析记录 scope depth、escape flags、SymbolId/TypeId 和 declaration range。跨 `await`/`yield` 的 ref、ref struct、Span、活动借用和 lock guard 会在 compiler 阶段拒绝；可复制值或 GC-safe owner 则提升到 closure frame。

## Call Binding cache

`SZrFunctionCallSiteCacheEntry`（定义在 `function.h`）把指令索引映射到 contract/cache。linker 验证 token、signature hash、module hash、owner layout、relocation kind 和 generation，成功后发布 VM/native/AOT witness。缓存包含：

- **contract**：持久化、可编码的 64-byte字段集合。
- **target**：运行时 tagged witness（VM function、native callback、AOT thunk/invoker）。
- **generation**：模块/函数图替换后递增，旧 witness 立即失效。
- **callableObject**：GC traced context，不序列化。

typed function value 的 contract 没有静态 target，`signatureToken`/hash 必须存在，relocation 为 `NONE`。每次实际 callable 可能不同，`ZrCore_CallBinding_PrepareTypedCall` 重新核对结构签名；不能用上一次 witness 替换当前 value。

## 泛型与方法

open generic declaration 保留 generic parameter/constraint；调用时形成 closed TypeId/MethodSpec。约束、方差、owner/ref contract 都参与 canonical signature。AOT 可按 layout specialization 或 dictionary sharing 实例化，解释器则把 generic context 放入 `SZrCallInfo.interpreterGenericContext`。

## property 与 receiver

property getter/setter 是独立 accessor symbol。普通 property get/set 直接 lower 为 `META_GET`/`META_SET`（或等价 typed accessor contract），不会先取字段再拼 helper call。inline struct receiver 要保留 receiver source provenance，setter 在原 Place 写回；静态 accessor 不注入额外 receiver。

## tail call

尾调用只有在没有活动异常处理器、`using` cleanup、借用或必须保留的 frame state 时才可复用 frame。若无法证明，compiler 保留普通 call path。深尾递归测试不仅检查结果，还检查 `callInfo` 链有界；native callback、debug hook 和 AOT observation 可能强制回退。

## C API 速查

| 入口 | 用途 |
|---|---|
| `ZrCore_CallBinding_CheckContract` | 检查 token/operation/slot 等静态字段。 |
| `ZrCore_CallBinding_CompareContracts` | 比较期望与实际结构签名。 |
| `ZrCore_CallBinding_Resolve` | 从 candidate 集合中解析目标并写入 binding。 |
| `ZrCore_CallBinding_Validate` | 按 generation 验证已缓存目标。 |
| `ZrCore_CallBinding_Invalidate` | 清除 runtime witness。 |
| `ZrCore_CallBinding_AdvanceGeneration` | 函数图 reload 后递增 generation。 |
| `ZrCore_CallBinding_LinkFunction` | 为 function graph 建立 callsite map。 |
| `ZrCore_CallBinding_PrepareMember` | receiver member call 的预解析。 |
| `ZrCore_CallBinding_PrepareTypedCall` | 检查 typed callable value 并建立 witness。 |
| `ZrCore_CallInfo_Extend` | 为新 VM/native frame 扩展 call-info 链。 |
| `ZrCore_Closure_New` / `ZrCore_ClosureNative_New` | 创建 VM/native closure。 |

失败状态包括 `MISSING_CONTRACT`、`TARGET_NOT_FOUND`、`SIGNATURE_MISMATCH`、`MODULE_MISMATCH`、`LAYOUT_MISMATCH`、`STALE_GENERATION`、`TARGET_KIND_MISMATCH` 和 `INVALID_RELOCATION`；宿主应读取 `SZrCallBindingDiagnostic`，不要只打印布尔失败。
