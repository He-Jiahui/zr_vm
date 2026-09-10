---
related_code:
  - zr_vm_lib_system/include/zr_vm_lib_system/module.h
  - zr_vm_lib_system/include/zr_vm_lib_system/fs_registry.h
  - zr_vm_lib_container/include/zr_vm_lib_container/module.h
  - zr_vm_lib_container/include/zr_vm_lib_container/generational_pool.h
  - zr_vm_core/include/zr_vm_core/reflection.h
  - zr_vm_core/include/zr_vm_core/type_layout.h
implementation_files:
  - zr_vm_lib_system/src/zr_vm_lib_system/module.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c
  - zr_vm_lib_container/src/zr_vm_lib_container/module.c
  - zr_vm_lib_container/src/zr_vm_lib_container/generational_pool.c
  - zr_vm_core/src/zr_vm_core/reflection.c
plan_sources:
  - user: 2026-09-10 继续细化 Wiki 的内置库接口和 C 调用方案
  - docs/library-and-builtins/index.md
  - docs/plans/syntax/2026-07-19-08-reflection-library-type-system-design.md
  - docs/plans/syntax/2026-07-19-09-generational-pool-handle-ref-struct-design.md
tests:
  - tests/system/test_system_fs_module.c
  - tests/container/test_container_module.c
  - tests/container/test_generational_pool.c
  - tests/container/test_generational_pool_type_layout.c
  - tests/module/test_reflection_dynamic_generic_instance.c
  - tests/parser/test_reflection_type_surface.c
doc_type: api-reference
---

# 系统、容器、反射与稳定槽池运行时参考

`zr.system`、`zr.container`、`zr.reflection` 与 `zr.pooling` 覆盖了 ZR 应用最常交汇的四条
运行时边界：外部资源、托管集合、类型/metadata 身份和可重用稳定存储。本页说明它们如何组合，
并给出 C 宿主/插件应遵守的注册、root、generation 与 handle 规则。单个 API 的完整签名表见
[系统 API](system-api.md)、[容器 API](container-api.md)和
[反射与稳定槽池 API](reflection-pooling-api.md)。

## 模块图与身份

```text
zr.system
  ├─ console / env / process / assembly
  ├─ fs (File, Folder, FileStream, IOException)
  └─ gc / vm / exception

zr.container
  ├─ Array / Map / Set / LinkedList / Span
  └─ container layout + iterator contracts

zr.reflection
  └─ TypeId / Type / member token / generic metadata projection

zr.pooling
  └─ Pool / PoolHandle / PoolRef / PoolReadRef / BufferPool
```

`zr.pooling` 由 container library 的独立 module descriptor 暴露；它不是把 `Array` 地址加上一个
整数索引的语法糖。system 的 path/stream、container 的 backing storage、reflection 的 token 和
pool handle 都有不同的生命周期模型：

| 值 | 稳定性 | 跨 reload / GC / container mutation 的规则 |
| --- | --- | --- |
| `File` / `FileStream` | managed resource wrapper | 用 `using`/`close` 限定；stream 关闭后对象可存在但 I/O 句柄不可再用。 |
| `Array<T>` / `Span<T>` | Array 可扩容，span 是 scoped view | span 不能穿过可能重分配、owner drop 或不允许的 await/escape。 |
| reflection TypeId/token | metadata identity + generation | cache 时必须保存 generation/signature/layout；reload 后重新 resolve/revalidate。 |
| `PoolHandle<T>` | `(poolId, slotIndex, generation)` 身份 token | 不含裸地址；slot 重用后旧 handle 仍失效。 |
| `PoolRef` / `PoolReadRef` | scoped guard | guard active 时才可访问投影；必须 close/release 后再 recycle/destroy。 |

## ZR 侧组合用法

### 资源输入、容器聚合与异常边界

文件 I/O 与集合操作最好由一个显式资源范围和错误边界包围：

```zr
let container = import("zr.container");

fn readLines(path: string): container.Array<string> {
    let fs = import("zr.system.fs");
    let values = init container.Array<string>();
    let file = init fs.File(path);

    using (let stream = file.open("r")) {
        try {
            let line = stream.readLine();
            while (line != null) {
                values.add(line);
                line = stream.readLine();
            }
        } finally {
            stream.flush();
        }
    }
    return values;
}
```

这里 Array 持有 string value，而 stream 是 scope-owned resource。不能从 `line` 的内部 native
buffer 或 `values.span()` 取得地址后跨过 `readLine`、`add`、`flush` 或任何可能触发 GC 的
调用保存它。`IOException` 的用户侧 catch 和 pending-control 展开规则见
[异常展开、finally 与清理](../02-language/exception-cleanup-runtime-reference.md)。

### `typeof`、`typeid` 与可构造类型

当前 syntax fixture 覆盖的反射形状如下：

```zr
let reflection = import("zr.reflection");

struct Point {
    pub var x: int;
}

fn create(typeValue: reflection.Type): object {
    let constructible = reflection.requireConstructible(typeValue);
    return constructible.createInstance(7);
}

fn pointType(): reflection.Type {
    return reflection.resolve(typeid(Point));
}

fn runtimeType(value: object): reflection.TypeOf {
    return typeof(value);
}
```

`typeid(Point)` 是编译期可定位的 type identity；`typeof(value)` 是值的运行时投影；
`reflection.resolve` 和 `requireConstructible` 还会检查当前 metadata、category、constructor
contract 和 layout 是否可用。interface、abstract/open generic、resource/ref-like 等类型不能因
为具有名称就被反射构造。若应用支持 reload，任何长寿命 cache 都必须同时保存 canonical id、
signature hash、metadata generation 和 type layout id。

### Pool handle 与 scoped borrow

`PoolHandle<T>` 是稳定身份，`PoolRef<T>` 是短暂可写投影。fixture 中的可执行表面是：

```zr
let pooling = import("zr.pooling");

fn borrow(
    handle: pooling.PoolHandle<int>,
    borrowed: out pooling.PoolRef<int>
): bool {
    return handle.tryBorrow(out borrowed);
}

fn recycle(handle: pooling.PoolHandle<int>): bool {
    return handle.recycle();
}
```

调用方只有在 `borrow` 成功后才能读写 `borrowed.value`，并且需要在相同 pool 的作用域中结束
该 guard。read guard 可以并存；write guard 与其它 read/write guard 冲突。将 `PoolRef` 存到
heap、closure、Task 或另一个线程域中，会违反其 ref-like/loan 语义；要长期保存身份，只保存
handle 并在使用点重新 validate/borrow。

## C 侧 module 注册

system/container 的 C library 入口分别在它们的 `module.h` 中。它们注册的是 descriptor 和
native callback contract，不是把 C 结构地址直接变成可访问 ZR object：

```c
#include "zr_vm_lib_system/module.h"
#include "zr_vm_lib_container/module.h"

static TZrBool register_runtime_libraries(SZrGlobalState *global) {
    if (!ZrVmLibSystem_Register(global)) {
        return ZR_FALSE;
    }
    if (!ZrVmLibContainer_Register(global)) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}
```

`ZrVmLibSystem_GetModuleDescriptor`、`ZrVmLibContainer_GetModuleDescriptor` 和
`ZrVmLibContainer_GetPoolingModuleDescriptor` 用于读取当前 descriptor；shared-library 构建还
暴露版本化 `ZrVm_GetNativeModule_v1` 入口。实际宿主必须在 core global/state 建立、基础 provider
注册和 ABI/version 检查的既定顺序内调用它们。若注册失败，应先读取当前 registry/global 的
last-error 诊断，不能继续导入同名 module，也不能用另一个手写 descriptor 覆盖失败原因。

## C 侧 Pool API

stable slab pool 有独立的、非脚本专用 C API。它以 `SZrPoolTypeLayout` 描述元素，支持
`GC_FREE`、`GC_MAPPED` 与 `GC_BARRIERED` scan 方式，并返回明确的 `EZrPoolStatus`。

| API | 成功后的责任 | 失败时不可做的事 |
| --- | --- | --- |
| `ZrPool_Create` | 取得 `SZrPool *`，最终调用 `ZrPool_Destroy(&pool)` | 不把 null/未初始化 pool 当可用对象。 |
| `ZrPool_CreateFromTypeLayout` | 借用 state、layout、registry、visitor；这些依赖须活到 pool 销毁 | 不在 registry/layout unload 后继续 scan。 |
| `ZrPool_Deliver` | 初始化成功才发布 `SZrPoolHandle` | construction failure 后不泄露半初始化 handle。 |
| `ZrPool_Validate` | 每次外来 handle 使用前验证 pool/id/generation | 不根据 slotIndex 直接解引用。 |
| `ZrPool_TryRead` / `TryBorrow` | 成功后持有 active `SZrPoolGuard` | 冲突、stale、wrong-pool 状态不能当作可重试的 live pointer。 |
| `ZrPoolGuard_Release` | 结束一次 guard，之后 `value` 不可读写 | 不 double-release 或 release 后保留 `value` 地址。 |
| `ZrPool_Recycle` | 使本 generation 无效，按 layout drop | 有活动 guard 时不能强制回收。 |
| `ZrPool_Scan` / `TraceGcValues` | 使用匹配的 visitor/scan contract | 不把 GcFree 元素按 object 数组扫描。 |

一个 C 调用顺序的骨架如下；`layout` 和 `config` 必须由调用方根据 element ABI 完整初始化：

```c
SZrPool *pool = ZR_NULL;
SZrPoolHandle handle;
SZrPoolGuard guard;
EZrPoolStatus status;

status = ZrPool_Create(&layout, &config, &pool);
if (status != ZR_POOL_STATUS_OK) {
    return status;
}

status = ZrPool_Deliver(pool, &sourceValue, &handle);
if (status == ZR_POOL_STATUS_OK) {
    status = ZrPool_TryRead(pool, handle, &guard);
    if (status == ZR_POOL_STATUS_OK) {
        const void *value = ZrPoolGuard_ReadOnlyValue(&guard);
        consume_value(value);
        status = ZrPoolGuard_Release(&guard);
    }
}
if (status == ZR_POOL_STATUS_OK) {
    status = ZrPool_Recycle(pool, handle);
}
(void)ZrPool_Destroy(&pool);
return status;
```

`EZrPoolStatus` 特别区分 `WRONG_POOL`、`HANDLE_STALE`、`ENTITY_RETIRED`、`BORROW_CONFLICT`、
`GENERATION_EXHAUSTED`、`POOL_BUSY` 和 `POOL_DESTROYED`。日志应保留
`ZrPool_StatusName(status)`，而不是把所有非 OK 简化为“not found”；这会显著缩短 ABA、lease
泄漏和销毁时序问题的定位时间。

## C 侧 reflection query 和构造

反射 C API 接受 managed type descriptor/object，返回的 members、instance 或 value 也属于
当前 `SZrState`。下面是查询 public/all default members 的基本形状：

```c
SZrReflectionMemberQuery query;
SZrObject *members = ZR_NULL;
EZrReflectionQueryStatus queryStatus;

ZrCore_Reflection_MemberQueryInitDefault(&query);
if (!ZrCore_Reflection_QueryMembers(state,
                                    typeDescriptor,
                                    ZR_REFLECTION_MEMBER_KIND_ANY,
                                    &query,
                                    &members,
                                    &queryStatus)) {
    /* queryStatus: NOT_FOUND, AMBIGUOUS, ACCESS_DENIED, ... */
    return ZR_FALSE;
}
```

`GetMember` 额外接收名称、member kind 和参数 TypeId 列表，用于 overload resolution；
`RequireConstructible` 与 `CreateInstance` 使用单独的 `EZrReflectionConstructionStatus`，它能
区分不可构造、没有匹配 constructor、constructor 歧义、constructor 抛错和非法实参。不要用
空 `SZrTypeValue` 代替 status：constructor 可能已执行并抛出了当前异常。

对 token、generic instance 和 reload 的正确顺序为：

```text
TypeId / metadata token
  -> ResolveTypeIdObject or ResolveToken
  -> check signature hash + metadata generation + type layout id
  -> QueryMembers / CreateInstance / InvokeMethodToken
  -> reload notification
  -> discard cache or RevalidateDynamicGenericTypeInstance
```

`ZrCore_Reflection_ResolveConstructedGenericType` 返回的动态实例含 route、generic signature、
requested arguments、layout id 和 borrowed layout pointer；cache 前必须复制稳定 identity，不能
长期保存 borrowed `typeLayout` 或 metadata record 指针。对于 field token，read/write helper 的
`inlineStorage` 和 byte-size 参数必须匹配当前 layout；错误的 layout id 应 fail closed，而不是
按旧 byte offset 写内存。

## 托管值、root 与交叉模块调用

从 system/container/reflection/pooling 任何一条路径得到的 object、array、string、descriptor 或
exception 都是 state-owned。C 代码在下一次会分配、调用 provider、执行 reflection、进入 native
callback 或触发 GC 前，必须把仍要使用的 managed 值保存在可扫描的 slot/root 中。不要把
`SZrObject *`、Array backing address、reflection member object 或 pool guard 的 value pointer
放进未注册的 C 局部变量后再调用 VM。

同样，传给 native callback 的 inline argument span 只是调用期间的 view；pool guard 和 span
有相同的“地址不是长期身份”原则。详细 root 形式、pin 和 callback write-back 规则见
[Native Call Context](../05-interop/native-call-context-reference.md) 与
[GC、异常与执行安全](../05-interop/core-gc-exception-api.md)。

## 选型和故障排查

| 目标 | 应使用 | 不应使用 |
| --- | --- | --- |
| 短生命周期、顺序集合 | `Array<T>` | 为普通元素身份强行创建 pool handle。 |
| 连续批处理但不跨异步边界 | `Span<T>` / `BufferPool` lease | 保存 backing address 后触发 Array 扩容。 |
| 稳定身份、slot 重用和 ABA 防护 | `PoolHandle<T>` + scoped guard | 只保存 `slotIndex` 或 `void *`。 |
| 按类型/metadata 查询成员 | reflection TypeId/token API | 由显示名称或隐藏字段名推断布局。 |
| 文件/stream 生命周期 | `using` + provider FileStream | 指望 finalizer 负责所有 flush/close。 |
| native host 扩展 | module descriptor + call context/root API | 直接篡改 managed object/collection internals。 |

高频 Map 查询、reflection member query 和 pool scan 都有缓存/统计入口，但缓存永远从属于
generation、entries version、layout 和 owner identity。性能优化应先建立这些一致性前提；否则
一次 reload、GC 或 slot recycle 就会把“命中缓存”变成错误对象访问。
