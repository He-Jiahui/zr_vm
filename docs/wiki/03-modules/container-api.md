---
related_code:
  - zr_vm_lib_container/include/zr_vm_lib_container/module.h
  - zr_vm_lib_container/include/zr_vm_lib_container/generational_pool.h
  - zr_vm_lib_container/src/zr_vm_lib_container/module.c
  - zr_vm_lib_container/src/zr_vm_lib_container/pooling.c
  - zr_vm_lib_container/src/zr_vm_lib_container/generational_pool.c
  - zr_vm_lib_iteration/src/zr_vm_lib_iteration/runtime/descriptor.c
implementation_files:
  - zr_vm_lib_container/src/zr_vm_lib_container/module.c
  - zr_vm_lib_container/src/zr_vm_lib_container/contiguous_view.c
  - zr_vm_lib_container/src/zr_vm_lib_container/pooling.c
  - zr_vm_lib_container/src/zr_vm_lib_container/generational_pool.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/library-and-builtins/index.md
tests:
  - tests/container/test_container_module.c
  - tests/container/test_generational_pool.c
  - tests/container/test_generational_pool_type_layout.c
doc_type: api-reference
---

# `zr.container` API 参考

`zr.container` 提供可变集合、连续视图和稳定槽池。集合对象是 managed object；元素的
copy/drop/GC scan 由 `T` 的 canonical `TypeLayout` 决定。所有 index API 都进行边界检查，
错误不会以越界指针形式暴露给 ZR。

## Array

```zr
let values: zr.container.Array<int> = init zr.container.Array<int>(4);
values.add(10);
values.insert(0, 5);
let first = values[0];
values.removeAt(1);
```

| 成员 | 签名 | 语义 |
| --- | --- | --- |
| 字段 | `length: int`、`capacity: int` | 当前元素数和已分配容量，只读观察。 |
| 构造 | `Array<T>(capacity?: int)` | 0 或指定初始容量；容量不足时增长。 |
| `span` | `span(): Span<T>` | 取得连续视图；视图存活期间不能让 backing storage 失效。 |
| `add` | `(value: T): null` | 追加元素。 |
| `insert` | `(index: int, value: T): null` | 在 `[0,length]` 插入并移动后续元素。 |
| `removeAt` | `(index: int): null` | 删除并移动后续元素。 |
| `clear` | `(): null` | drop 全部元素，保留或重置容量由实现策略决定。 |
| `contains` / `indexOf` | `(value: T): bool` / `int` | 使用 canonical equality；找不到返回 -1。 |
| `getIterator` | `(): zr.iteration.Enumerator<T>` | 迭代器在修改后不保证旧位置有效。 |
| meta get/set | `array[index]` / `array[index] = value` | 等价于 checked index access。 |

随机访问为摊销 O(1)，insert/remove 为 O(n)，contains/indexOf 为 O(n)。`span` 是借用 view，
不能跨越可能重新分配的 `add/insert` 或 owner 销毁；需要稳定身份时使用 `Pool<T>`。

## Map、Set、Pair

| 类型 | 约束/字段 | 成员 |
| --- | --- | --- |
| `Map<K,V>` | `count:int`；K 需 `IHashable`、`IEquatable<K>` | 构造、`containsKey(K):bool`、`remove(K):bool`、`clear():null`、`getIterator():Enumerator<Pair<K,V>>`、meta get/set。 |
| `Set<T>` | `count:int`；T 需 `IHashable`、`IEquatable<T>` | 构造、`add(T):bool`、`contains(T):bool`、`remove(T):bool`、`clear():null`、`getIterator():Enumerator<T>`。 |
| `Pair<K,V>` | `first:K`、`second:V` | 构造、`equals`、`compareTo`、`hashCode`；参与 equality/ordering/hash protocol。 |

Map 的 key lookup 先按 hash 定位，再按 equality 确认；修改 entries version 会使内部热查找
缓存失效。不要在迭代 Map 时改变 key 的 hash 结果。Set 的 `add/remove` 返回是否改变集合，
而不是元素本身。

```zr
let map = init zr.container.Map<string, int>();
map["ok"] = 1;
if (map.containsKey("ok")) {
    let value = map["ok"];
}
let set = init zr.container.Set<string>();
set.add("zr");
```

## LinkedList 和 LinkedNode

`LinkedList<T>` 暴露 `count`、`first`、`last`；`LinkedNode<T>` 暴露 `value`、`next`、
`previous`。方法为 `addFirst`、`addLast`、`removeFirst`、`removeLast`、`remove(value)`、
`clear` 和 `getIterator`。首尾插入/删除为 O(1)，按值删除和查找为 O(n)。节点属于 list
owner，不应被保存到另一个 list；删除节点后读取其 next/previous 属于失效访问。

## Span 与 ReadOnlySpan

| 类型 | 字段 | 方法/操作 |
| --- | --- | --- |
| `Span<T>` | `source`、`start`、`length` | `slice(start,length): Span<T>`、`asReadOnly(): ReadOnlySpan<T>`、meta get/set。 |
| `ReadOnlySpan<T>` | `source`、`start`、`length` | `slice(start,length): ReadOnlySpan<T>`、meta get。 |

span 的 start/length 都在构造和 slice 时检查；slice 不复制元素，只改变 view 边界。source
必须保持存活且 layout 不变。只读 span 可共享，写 span 需要可写 loan；把 span 存入长期
对象或跨 `await` 会触发 borrow escape 诊断。

## Pooling

容器模块同时注册 `zr.pooling`。它把 managed layout 放入 stable slab，并用
`(poolId, slotIndex, generation)` 组成句柄：

| 类型/方法 | 签名 | 规则 |
| --- | --- | --- |
| `BufferPool` | `rent<T>(length:int): PoolLease<T>` | 取得可写连续租约。 |
| `PoolLease<T>` | `span(): Span<T>`、`close(): null`、`lease[i]` | close 幂等；close 后 view 失效。 |
| `Pool<T>` | `deliver(value:T): PoolHandle<T>` | 完整初始化后发布句柄。 |
| `Pool<T>` | `isLive(handle): bool` | 同时检查 pool、slot、generation。 |
| `Pool<T>` | `recycle(handle): bool` | 标记旧 generation 失效；活动 guard 结束后 drop。 |
| `Pool<T>` | `tryRead(handle,out PoolReadRef<T>): bool` | 取得只读 guard。 |
| `Pool<T>` | `tryBorrow(handle,out PoolRef<T>): bool` | 取得可写 guard。 |
| `PoolRef` / `PoolReadRef` | `value`、`close()` | scoped projection；必须在同一 pool release。 |

generation 单调递增且不回绕；slot 重用后旧句柄仍无效。读 guard 可并存，写 guard 互斥，
guard 未 close 前不能 recycle。GC scanner 根据 `GcFree/GcMapped/GcBarriered` layout 选择
扫描路径；初始化失败必须清零 slot 并 abort，不得发布半成品句柄。

## C 侧布局和调试

native provider 可通过 `ZrLib_Array_New`、`ZrLib_Array_PushValue`、`ZrLib_Array_Get` 构造
通用数组，但不能直接改 hidden `__zr_items` 字段。需要高性能 inline 元素时使用
`ZrLib_CallContext_InlineArgumentView` 和可信 `SZrTypeLayout`，并在 safepoint 后重新获取
borrowed view。Pool 的 C API（`ZrPool_Create`、`Deliver`、`TryRead/TryBorrow`、`Recycle`、
`Guard_Release`、`Destroy`）返回明确 `EZrPoolStatus`；遇到 `HANDLE_STALE` 或 `WRONG_POOL`
不得把句柄当新对象重试。

## 性能和失效清单

- 高频 Map lookup 使用内部 4-slot hot cache，但任何 entries version 改变都会失效；不要
  依赖缓存地址稳定性。
- Array 扩容可能搬迁 inline storage；持有 span 时避免触发扩容。
- Map/Set 的 key 需要稳定 hash；修改参与 hash 的字段后先 remove 再重新 add。
- LinkedNode 不拥有独立生命周期；list clear 会一次性 drop 节点。
- Pool handle 只是一份身份 token，不是可解引用的裸指针；每次使用都应 validate。

迭代协议详见[迭代 API](iteration-api.md)，所有权规则详见[类型、布局与所有权](../02-language/types-ownership.md)。
