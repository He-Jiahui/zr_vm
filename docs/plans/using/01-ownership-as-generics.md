# Using 01：Unique、Shared、Weak 与 Resource

## 类型模型

```text
resource class R
Unique<R>       move-only strong owner
Shared<R>       non-atomic shared owner, first-version thread-local
Weak<R>         non-owning identity, upgrade is checked
AtomicShared<R> future/independent thread-safe owner
ref R           scoped mutable borrow
ref readonly R  scoped readonly borrow
```

`Unique/Shared/Weak`不是按类型名触发的泛型特例，而是Canonical TypeRef中的registered owner constructor。标准库API与runtime capability实现操作。

## 操作

- `own R(...)`产生`Unique<R>`，不能对普通class使用。
- move在Place availability中静态跟踪；move后使用是编译错误。
- `share(owner)`消耗Unique并产生Shared；第一次share才允许创建control block。
- `degrade(shared)`产生Weak；`wake(weak)`返回nullable Shared并保留runtime check。
- `drop(owner)`提前结束owner lifetime；scope exit生成Drop glue。
- `intoGc(owner)`显式消费`Unique<T>`、生成`GcBox<T>`并放弃确定性释放承诺。
- resource保存GC对象必须使用`Gc<T>` root handle。

## 限制

第一版Shared非原子且不能跨线程；强引用环需要Weak打断或后续cycle policy。borrow不增加owner数量，也不能比强源活得更久。PoolHandle/PoolRef属于`zr.pooling`，不伪装成通用owner。

## 完成记录

[2026-06-20 ownership runtime baseline](./01-ownership/2026-06-20-ownership-runtime-baseline.md) 仅证明已有runtime入口；Canonical owner TypeRef、Place move和新surface仍需收敛。

## 实施阶段与验收

| 阶段 | 交付 | 必须覆盖 |
|---|---|---|
| O1 canonical owner types | Unique/Shared/Weak TypeNode、constraint与TypeId roundtrip | nested generic、nullable/union、非法T类别 |
| O2 Place availability | move/drop状态、branch join、projection overlap | use-after-move、maybeMoved、active loan |
| O3 runtime protocol | construct/share/weak/upgrade/drop glue | allocation failure、weak失效、retain/release配对 |
| O4 thread/GC bridge | Shared/AtomicShared能力与Gc/GcBox root | illegal cross-thread、direct GC field、bridge failure |
| O5 consumer parity | VM/AOT/artifact/reflection/LSP共同contract | source/binary hash与diagnostic一致 |

证据入口：`tests/parser/test_aot_c_ownership_contracts.c`、`tests/parser/test_dataflow_engine.c`、`tests/language_server/test_ownership_diagnostics.c`、`tests/acceptance/2026-06-17-ownership-generics-p1.md`。目标用例还必须覆盖loop/throw/finally、partial construction、Shared环与Weak打断策略。

退出条件：owner语义不依赖变量置null或类型名字符串；每个scope/exception edge的Drop可审计；Shared线程策略被类型系统执行；reflection不能直接构造resource；四backend行为一致。
