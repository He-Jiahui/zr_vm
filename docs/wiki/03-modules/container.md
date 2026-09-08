---
related_code:
  - zr_vm_lib_container/include/zr_vm_lib_container/module.h
  - zr_vm_lib_container/include/zr_vm_lib_container/generational_pool.h
  - zr_vm_lib_container/src/zr_vm_lib_container/module.c
  - zr_vm_lib_container/src/zr_vm_lib_container/contiguous_view.c
  - zr_vm_lib_container/src/zr_vm_lib_container/pooling.c
  - zr_vm_lib_container/src/zr_vm_lib_container/generational_pool.c
  - zr_vm_lib_iteration/include/zr_vm_lib_iteration/module.h
implementation_files:
  - zr_vm_lib_container/src/zr_vm_lib_container/module.c
  - zr_vm_lib_container/src/zr_vm_lib_container/contiguous_view.c
  - zr_vm_lib_container/src/zr_vm_lib_container/pooling.c
  - zr_vm_lib_container/src/zr_vm_lib_container/generational_pool.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/plans/syntax/2026-07-19-09-generational-pool-handle-ref-struct-design.md
tests:
  - tests/container/test_generational_pool.c
  - tests/container/test_generational_pool_gc_stress.c
  - tests/parser/test_span_core.c
  - tests/parser/test_span_gc_cases.c
  - tests/library/test_official_provider_convergence.c
doc_type: module-detail
---

# `zr.container`

**状态：`current`；Runtime provider。`zr.container` descriptor 未设置独立 module version，
公开 contract 为 `zr.container:v1:container-span-protocols`；`zr.pooling` 是同仓库的独立
实验 provider，版本 `1.1.0`、contract `zr.pooling:v1:stable-slot-generational-pool`。**

容器 provider 负责拥有数据结构和非拥有视图。`Array<T>`、`Map<K,V>`、`Set<T>`、
`LinkedList<T>`、`Pair<A,B>` 是普通 GC 容器；`Span<T>`、`ReadOnlySpan<T>`、
`PoolHandle<T>`、`PoolRef<T>` 是带明确借用/稳定身份约束的特殊类型。

## 普通容器

| 类型 | 核心操作 | 复杂度/语义 |
| --- | --- | --- |
| `Array<T>` | `add`、`insert`、`removeAt`、`clear`、`contains`、`indexOf`、`getIterator`、`length`、`capacity`、索引读写、`span` | 连续 GC 存储；扩容可能移动 backing，但会更新所有 GC 引用 |
| `Map<K,V>` | 索引读写、`containsKey`、`remove`、`clear`、`getIterator`、`count` | hash key/value pair；rehash 不改变逻辑身份 |
| `Set<T>` | `add`、`contains`、`remove`、`clear`、`getIterator`、`count` | 基于 hash equality 去重 |
| `LinkedList<T>` | `addFirst`、`addLast`、`removeFirst`、`removeLast`、`remove`、`clear`、`getIterator`、`first`、`last`、`count` | 节点独立分配；迭代期间节点由 GC 保持可达 |
| `Pair<A,B>` | `first`、`second` | canonical struct；元素按各自 layout 存储 |

普通容器方法通过 descriptor 的 generic parameter 和 canonical TypeId 检查元素类型，
不会以字符串比较类型名。`Array<T>` 的 `span()` 只创建 view，不复制元素。

### 调用形状

| 类型 | 构造/成员签名 | 返回值 |
| --- | --- | --- |
| `Array<T>` | `init Array<T>(capacity?: int)`；`add(value: T)`；`insert(index: int, value: T)`；`removeAt(index: int)`；`clear()`；`contains(value: T)`；`indexOf(value: T)`；`getIterator()`；`span()` | 修改操作返回 `null`；查询返回 `bool/int/Enumerator<T>`；索引读写分别为 `T`/`T` |
| `Map<K,V>` | `init Map<K,V>()`；索引 `map[key]`；`containsKey(key: K)`；`remove(key: K)`；`clear()`；`getIterator()` | 索引读写为 `V`；remove 为 `bool`；迭代器元素为 `Pair<K,V>` |
| `Set<T>` | `init Set<T>()`；`add(value: T)`；`contains(value: T)`；`remove(value: T)`；`clear()`；`getIterator()` | add/contains/remove 为 `bool`；迭代器元素为 `T` |
| `LinkedList<T>` | `init LinkedList<T>()`；`addFirst/Last(value: T)`；`removeFirst/Last()`；`remove(value: T)`；`clear()`；`getIterator()` | add 返回 `LinkedNode<T>`；removeFirst/Last 返回 `T`；remove 返回 `bool` |
| `Pair<K,V>` | `init Pair<K,V>(first: K, second: V)`；`equals(other: Pair<K,V>)`；`compareTo(other: Pair<K,V>)`；`hashCode()` | `bool`、`int`、`int`；同时满足 `IEquatable`、`IComparable`、`IHashable`。 |
| `LinkedNode<T>` | `init LinkedNode<T>(value: T)`；字段 `value`、`next`、`previous` | 由 LinkedList 创建和链接；不应手动篡改链接来绕过 list 的计数。 |
| `Span<T>` | `init Span<T>()`；`slice(start: int, length: int)`；`asReadOnly()`；索引读写 | 返回 `Span<T>`/`ReadOnlySpan<T>`；索引写入返回 `T` 兼容槽。构造器只产生空 view，通常应从 `Array.span()` 或 FFI/pool pin 获得 source。 |
| `ReadOnlySpan<T>` | `init ReadOnlySpan<T>()`；`slice(start: int, length: int)`；索引读取 | 返回 `ReadOnlySpan<T>`/`T`；没有 SET meta method。 |

`length`、`capacity`、`count`、`first` 和 `last` 是 descriptor 字段；不能通过同名 setter
绕过边界检查。索引、slice 和 map key 的错误均在 native callback 内转换为 VM exception。

## Span 和边界证明

```zr
let c = import("zr.container");
var a = c.Array<int>();
a.add(3); a.add(5);
var s = a.span();
s[1] = 8;
let r = s.slice(1, 1).asReadOnly();
```

`Span<T>` 是 inline、ref-like、mutable view；`ReadOnlySpan<T>` 只能读。两者都保存
source、signed start、signed length。合法索引必须满足 `0 <= i < length`；slice 必须
满足 `0 <= start`、`0 <= n`、`start <= length`、`n <= length - start`。最后一条采用
减法形式避免整数加法溢出。编译器只有在 SemIR 同时拥有上下界 proof bits 时才删除运行时
分支；动态访问在 VM 和 AOT 走同一检查。

view 的 source loan 由 `CONTIGUOUS_SOURCE_OWNER`/`CONTIGUOUS_SOURCE_NATIVE_PINNED`
协议和 resolved receiver Place 产生。view 活跃时禁止移动、drop、unpin 或复用 source；
最后一次使用之后按 NLL 规则释放 loan。view 不能存入 class/global/array、闭包捕获或
跨 `await`/`yield`。

## Pool 与稳定槽

`BufferPool.rent<T>(length)` 返回 affine `PoolLease<T>`；`close()` 清空 backing、归还
池并递增 generation，重复 close 幂等。`Pool<T>` 使用固定容量 slab 和
`PoolHandle<T>{pool identity, slot, generation}`。handle 是可存储的弱身份，不是裸引用。

`tryRead(handle, out PoolReadRef<T>)` 取得共享只读 guard，`tryBorrow(handle, out PoolRef<T>)`
取得可写 guard。读 guard 可并存，写 guard 排斥其他 guard；slot recycle 立即使旧 handle
失效，但物理 drop 延迟到最后一个 guard 关闭。代数耗尽的 slot 永不回绕，避免 ABA。

### 池化 API 速查

| 类型/成员 | 精确形状 | 语义 |
| --- | --- | --- |
| `BufferPool` | `init BufferPool()`；`rent<T>(length: int): PoolLease<T>` | 从 GC-traced backing 池取得一次性 owner lease；`available`、`nextGeneration`、`returnCount`、`reuseCount` 是运行时统计字段，应用不应改写。 |
| `PoolLease<T>` | `span(): Span<T>`；`close(): null`；索引 GET/SET | lease 是 `CONTIGUOUS_SOURCE_OWNER`；`close` 归还 backing 并递增 generation，重复调用幂等；SET 返回 `null`。 |
| `Pool<T>` | `init Pool<T>()`；`deliver(value: T): PoolHandle<T>`；`isLive(handle: PoolHandle<T>): bool`；`recycle(handle: PoolHandle<T>): bool` | `deliver` 初始化并发布稳定槽；`isLive` 校验 pool/slot/generation 三元组；`recycle` 使 identity 失效，活跃 guard 关闭后才回收。 |
| `Pool<T>` | `tryRead(handle: PoolHandle<T>, view: out PoolReadRef<T>): bool`；`tryBorrow(handle: PoolHandle<T>, view: out PoolRef<T>): bool` | 通过 `out` 写入 scoped guard；失败只返回 `false`，不会写入半初始化 view。 |
| `PoolHandle<T>` | `poolId: uint`；`slotIndex: uint`；`generation: uint` | 不可构造的弱身份值；字段共同参与完整性校验，不保存元素地址，旧 generation 永不回绕。 |
| `PoolRef<T>` / `PoolReadRef<T>` | `value`（可写/只读 ref）；`close(): null` | move-only ref-like guard；必须 close，离开作用域也会走 close meta；不能存入 GC heap 或跨 suspension。 |
| `STABLE_SLOT_CONTRACT_HASH` | `uint` 常量 | 当前 StableSlotSource 能力契约哈希，供宿主在 provider 组合前比对。 |

池从 canonical `SZrTypeLayout` 推导大小、对齐、copy/drop 和 GC scan class。`GcFree`、
`GcMapped`、`GcBarriered` 分别表示无需扫描、逐 slot 扫描和带 dirty card 的扫描路径。
初始化失败先执行 `abortInitialize` 再发布 handle，绝不暴露半初始化元素。

## C 入口和调试计数器

```c
const ZrLibModuleDescriptor *d = ZrVmLibContainer_GetModuleDescriptor();
const ZrLibModuleDescriptor *p = ZrVmLibContainer_GetPoolingModuleDescriptor();
ZrVmLibContainer_Register(global);
ZrVmLibContainer_Debug_ResetHotMapLookupStats();
ZrVmLibContainerDebugHotMapLookupStats s =
    ZrVmLibContainer_Debug_GetHotMapLookupStats();
```

池化 C API 的生命周期和返回状态如下：

| C 符号 | 作用 |
| --- | --- |
| `ZrPool_Create` / `ZrPool_CreateFromTypeLayout` | 以显式 `SZrPoolTypeLayout` 或 canonical `SZrTypeLayout` 创建 pool；layout、registry、visitor 和 context 在 pool 存续期间必须有效。 |
| `ZrPool_Deliver` | 初始化一个 live slot 并写出 `SZrPoolHandle`；初始化失败会调用 `abortInitialize`。 |
| `ZrPool_Validate` / `ZrPool_Recycle` | 校验或退休完整 pool/slot/generation 身份；recycle 遇到活跃 guard 时延迟物理 drop。 |
| `ZrPool_TryRead` / `ZrPool_TryBorrow` | 获取只读/可写 `SZrPoolGuard`；冲突返回 `ZR_POOL_STATUS_BORROW_CONFLICT`。 |
| `ZrPoolGuard_Value` / `ZrPoolGuard_ReadOnlyValue` / `ZrPoolGuard_Release` | 访问 guard 投影并恰好释放一次；释放后指针立即失效。 |
| `ZrPool_Scan` / `ZrPool_TraceGcValues` | 按 GC scan kind 扫描槽并返回 scanned slot/byte 计数。 |
| `ZrPool_GetStats` / `ZrPool_StatusName` | 读取统计快照或把 `EZrPoolStatus` 转成稳定名称。 |
| `ZrPool_Destroy` | 销毁 pool；仍有 live slot 或 guard 时返回 `ZR_POOL_STATUS_POOL_BUSY`。 |

`EZrPoolStatus` 区分 wrong pool、stale handle、retired entity、generation exhausted、
construction failure 和 pool destroyed，宿主应按状态决定重试、重新 deliver 或报告错误。
debug 计数器只观测 Map 热路径，不改变缓存失效逻辑。模块共享库仍使用 v1 descriptor
入口；pool layout 注册必须在其引用的 global、registry 和 callback 生命周期内保持有效。
