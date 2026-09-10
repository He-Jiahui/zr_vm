---
related_code:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_declarations.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_types.c
  - zr_vm_core/include/zr_vm_core/type.h
  - zr_vm_core/include/zr_vm_core/canonical_consumer.h
  - zr_vm_lib_container/src/zr_vm_lib_container/module.c
  - zr_vm_lib_iteration/src/zr_vm_lib_iteration/runtime/descriptor.c
  - zr_vm_lib_task/src/zr_vm_lib_task/runtime/runtime.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_types.c
  - zr_vm_core/src/zr_vm_core/type.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/plans/syntax/README.md
tests:
  - tests/parser/test_generic_type_parser.c
  - tests/module/test_reflection_dynamic_generic_instance.c
  - tests/container/test_container_module.c
  - tests/fixtures/projects/syntax_reference_v1/src/generics_and_protocols.zr
doc_type: language-reference
---

# 泛型、接口与协议

泛型在 ZR 中不是文本替换。parser 先记录参数和约束，binder 为每个实例生成 canonical
TypeId，layout/协议检查随后决定能否构造、调用和跨线程传递。native descriptor 也必须发布
相同的泛型参数、约束和 passing mode，否则 registry 会拒绝 contract。

## 声明泛型参数

```ebnf
generic-parameters = "<", generic-parameter, {",", generic-parameter}, [","], ">" ;
generic-parameter  = ["in" | "out"], ["const"], identifier, [":", TypeRef] ;
where-clauses      = {"where", identifier, ":", constraint, {",", constraint}} ;
constraint          = "class" | "struct" | "new", "(", ")"
                    | "owner" | "unique" | "shared" | "weak" | TypeRef ;
```

当前 parser 把普通约束放在声明头之后的 `where` 子句；`<...>` 内的冒号只属于 `const`
整数参数的 TypeRef。推荐在公共 API 中让 `where` 紧邻声明头，使 descriptor 文档和源码一致：

```zr
struct Box<T> where T: IEquatable<T> {
    pub let value: T;
}

fn same<T>(left: in T, right: in T): bool where T: IEquatable<T> {
    return left.equals(right);
}
```

约束可为 `class`、`struct`、`new()`、`owner`、精确 owner qualifier 或已注册的 TypeRef。
未知泛型参数和相互冲突的 owner constraint 会在 parser 阶段报告；具体 TypeRef、构造性和
实参匹配会在 binding 阶段拒绝，而不是生成一个“宽松 any”实例。精确 grammar、AST、canonical
identity 和 C 实例表见[泛型参数、约束与实例化参考](generic-parameter-instantiation-reference.md)。

## interface 与 implements

```zr
interface Iterable<T> {
    fn getEnumerator(): zr.iteration.Enumerator<T>;
}

struct Numbers implements Iterable<int> {
    fn getEnumerator(): zr.iteration.Enumerator<int> { ... }
}
```

interface 只声明可观察的 method/property contract；`class`/`struct` 使用 `implements` 满足
它。继承使用 `extends`，一个 class 的 base class 和多个 interface 的组合在 canonical
type graph 中保持顺序和 identity。`virtual` 方法提供可覆写槽，`override` 必须精确匹配
参数 passing mode、泛型实参和返回类型；`shadow` 才表示有意隐藏而不是覆写。

下列条件会拒绝声明：

- `override` 没有对应的 virtual/abstract base member；
- interface method 缺少实现，且类型不是 abstract；
- 同名 overload 只有 passing mode 不同，导致调用选择不唯一；
- 一个 type 通过不同路径继承同一 interface 但 contract 不一致；
- generic parameter 的约束不能证明所调用的 member 存在。

## canonical identity

canonical TypeId 至少包括 module identity、qualified name、generic argument list、ownership
qualifier 和 projection kind。下面三个名字不是同一个类型：

```text
zr.container.Array<int>
zr.container.Array<float>
share zr.container.Array<int>
```

反射、AOT metadata、LSP 和 native call binding 都消费该 identity。字符串比较只适用于展示；
需要比较类型时使用 `typeid(T)`/reflection API 或 compiler 传入的 TypeId。

## generic method 和推断

调用点可以显式传入实参，也可以从 value/expected type 推断：

```zr
let a = same<int>(1, 1);
let b = same(2, 2); // 从两个 in 参数推断 T=int
```

推断不会跨越不透明 callable、动态 `value` 或 nullable 未决分支。多个候选同样可行时，
compiler 报 ambiguous overload；不会按运行时值随机选择。native descriptor 的
`genericParameters` 和 `constraintTypeNames` 必须足够让 resolver 在没有函数体的情况下
完成同样的检查。

## 容器约束实例

`zr.container.Map<K,V>` 和 `Set<T>` 要求 key/element 满足 `IHashable` 与
`IEquatable<T>`。这不是文档建议，而是 descriptor 的正式约束：

```zr
let counts: zr.container.Map<string, int> = init zr.container.Map<string, int>();
counts["ok"] = 1;
```

如果自定义类型只实现 `equals` 而没有 `hashCode`，构造 Map 时会在 type binding 阶段失败。
`Pair<K,V>` 通过协议提供 equality/ordering/hash；`Array<T>` 不要求 T 可 hash，但其
`contains`/`indexOf` 仍依赖 canonical equality。

## iteration 与 task 协议

`zr.iteration.Iterable<T>` 返回 `Enumerator<T>`；枚举器有 readonly `current` 和
`moveNext(): bool`。异步生产者实现 `AsyncIterator<T>`，`moveNext` 返回
`zr.task.Task<bool>`，并提供 `close`。泛型实参必须在同步/异步协议之间保持一致，不能把
`Enumerator<T>` 隐式当成 `AsyncIterator<T>`。

`zr.task.Scheduler.schedule` 接受 `zr.task.Job<T>`，返回 `zr.task.Task<T>`。任务的 T 由
Job 的 callable 返回值决定；`async fn` 产生的嵌套 Task 会由 provider 的 bridge 规则展开，
不会在脚本层自动把任意 callable 当成可调度工作。

## Send/Sync 协议

`zr.thread` 以 marker interface `Send` 和 `Sync` 约束跨 worker 值：

- `Send`：值可移动到另一个执行域，原 owner 不再使用；
- `Sync`：多个线程可通过共享引用访问，具体操作仍受对象方法的锁语义约束。

`Channel<T>`、`Shared<T>` 等 descriptor 只在 T 满足声明的协议时可实例化。`Lock<T>` 和
`SharedLock<T>` 是 affine/scoped view，不能跨 `await` 或线程迁移；这类限制由 flow/loan
检查实现，不能靠在运行时捕获死锁来替代。

## native descriptor 中的泛型

C 侧使用 `ZrLibGenericParameterDescriptor` 发布名称、文档和约束数组；类型 descriptor
使用 `genericParameters`，函数/method 使用各自的 generic 参数字段。contract hash 会把
参数顺序、约束、passing mode 和 dispatch flags 纳入计算。新增约束、改变 `T` 的 owner 或
改变 interface relation 都是 public contract 变化，必须同步更新 module version/hash、AOT
registration、LSP cache 和文档。

## 反射动态实例

反射可以从 `SZrReflectionGenericTypeArgument` 构造动态 generic instance，参数 kind 包括
primitive、type token、array、tuple、ownership、nullable 和 union。解析结果带
`genericSignatureHash`、`typeLayoutId` 和 metadata generation；缓存前必须保存这些字段，
reload 后调用 `RevalidateDynamicGenericTypeInstance`。旧 generation 的实例不能继续写入
新 layout。

## 诊断和调试清单

| 诊断 | 原因 | 修复方向 |
| --- | --- | --- |
| `generic arity` | 实参数量不符 | 补齐或删除显式实参 |
| `constraint unsatisfied` | T 不满足 interface | 实现协议或换类型 |
| `ambiguous overload` | 候选同样精确 | 指定 `<T>` 或显式转换 |
| `override mismatch` | 签名/passing mode 不同 | 与 base descriptor 对齐 |
| `layout mismatch` | 动态实例使用旧 layout | 重新 resolve/revalidate |
| `thread transfer rejected` | T 非 Send/Sync | 使用 transfer/clone 或保留在原 domain |

参阅[类型、布局与所有权](types-ownership.md)、[迭代 API](../03-modules/iteration-api.md)
和[线程 API](../03-modules/thread-api.md)。
