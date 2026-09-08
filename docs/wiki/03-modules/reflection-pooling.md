---
related_code:
  - zr_vm_core/include/zr_vm_core/reflection.h
  - zr_vm_core/include/zr_vm_core/type.h
  - zr_vm_core/src/zr_vm_core/reflection.c
  - zr_vm_lib_container/include/zr_vm_lib_container/generational_pool.h
  - zr_vm_lib_container/src/zr_vm_lib_container/generational_pool.c
  - zr_vm_lib_container/src/zr_vm_lib_container/pooling.c
  - zr_vm_library/include/zr_vm_library/native_registry.h
  - zr_vm_parser/src/zr_vm_parser/artifact_projection.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/reflection.c
  - zr_vm_lib_container/src/zr_vm_lib_container/generational_pool.c
  - zr_vm_lib_container/src/zr_vm_lib_container/pooling.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_reflection_contract.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/plans/syntax/2026-07-19-08-reflection-library-type-system-design.md
tests:
  - tests/parser/test_reflection_type_surface.c
  - tests/parser/test_reflection_type_stress.c
  - tests/module/test_reflection_dynamic_generic_instance.c
  - tests/container/test_generational_pool.c
  - tests/container/test_generational_pool_type_layout.c
doc_type: module-detail
---

# `zr.reflection` 与 `zr.pooling`

`zr.reflection` 当前为 `experimental` 的 contract-only surface；它不提供可被普通 native
loader 覆盖的空模块。`zr.pooling` 为 Runtime provider，版本 `1.1.0`，contract
`zr.pooling:v1:stable-slot-generational-pool`。

## Reflection

`zr.reflection` 是 contract-only provider：它拥有 `BUILTIN_TYPE_SURFACE`/`REFLECTION` role、
canonical type hierarchy 和 projection kind，但不能被普通 native loader 当作空模块 materialize。
运行时 reflection service 由 resolver 按 provider role 取得，避免 workspace 伪造
`zr.reflection` 覆盖官方身份。

反射可读取 TypeId、kind、generic arguments、字段/方法 metadata、source span 和 method
signature，并执行受限构造/成员调用。abstract/interface/resource/ref-like/open-generic 类型
在构造前拒绝。每个 runtime module 有非零 metadata generation；reload 后同名类型的 cache
key 不复用旧 generation。

## Pooling

`BufferPool`/`Pool<T>` 使用 GC-traced stable slab。`PoolHandle` 只保存 pool identity、slot
和 generation；`PoolRef`/`PoolReadRef` 是 guard lifetime 内的临时 projection。读 guard 可
并存，写 guard 互斥；recycle 立即废弃旧 generation，最后一个 guard 关闭才执行 pending drop。

pool 由 `SZrTypeLayout` 推导 size/alignment/copy/drop/scan。`GcFree` 不扫描，`GcMapped` 扫描
初始化 slot，`GcBarriered` 用 dirty card 记录写入。`deliver` 只有在元素完全初始化后才发布
handle；失败路径调用 `abortInitialize` 并清零 slot。

### Pooling 导出签名

| 类型 | 成员 |
| --- | --- |
| `BufferPool` | `init BufferPool()`；`rent<T>(length: int): PoolLease<T>` |
| `PoolLease<T>` | `span(): Span<T>`；`close(): null`；索引读/写 `lease[i]` |
| `Pool<T>` | `deliver(value: T): PoolHandle<T>`；`isLive(handle): bool`；`recycle(handle): bool`；`tryRead(handle, out ref): bool`；`tryBorrow(handle, out ref): bool` |
| `PoolHandle<T>` | `pool`、`slot`、`generation` 只读身份字段；不可脚本构造 |
| `PoolReadRef<T>` | `value: T` 只读投影；`close(): null` |
| `PoolRef<T>` | `value: ref T` 可写投影；`close(): null` |

`tryRead/tryBorrow` 的第二参数是 `out` guard；成功后调用方必须在使用完投影后 close。
`isLive` 同时比较 pool identity、slot 和 generation，不能只比较整数 slot。`PoolHandle` 的
`generation` 单调递增且不回绕，旧句柄即使 slot 重用也保持失效。

### Core C reflection 入口

contract-only provider 的运行时操作由 core reflection API 完成：
`ZrCore_Reflection_TypeOfValue`、`BuildTypeIdObject`、`ReadTypeIdObject`、
`ResolveTypeIdObject`、`QueryMembers`、`GetMember`、`RequireConstructible`、
`CreateInstance`、`ResolveToken`、`InvokeMethodToken`、
`ResolveConstructedGenericType` 和 `RevalidateDynamicGenericTypeInstance`。查询/构造状态
分别通过 `EZrReflectionQueryStatus`、`EZrReflectionConstructionStatus` 返回；metadata
generation 变化后必须重新解析 token 和 layout。

## 反射与池的交界

runtime-only 字段不会出现在 reflection；`POOL_REF_PROJECTION` 被投影为 getter-only 或
可写 property，而不是暴露隐藏 provider method。ref-like 类型不能经反射构造。LSP、binary
artifact、AOT 和 runtime 必须消费相同 layout id/hash，缺失或跨 registry 的 layout 失败关闭。

## C 调用接口

反射是 core service，pool 是可独立使用的 C API。两者都返回明确状态，不把 stale handle
当作普通空指针：

| 入口 | 作用 | 结果 |
| --- | --- | --- |
| `ZrCore_Reflection_TypeOfValue` / `BuildTypeIdObject` / `ReadTypeIdObject` / `ResolveTypeIdObject` | 在 state 中建立、读取和解析 TypeId | `TZrBool` 或 GC 对象；对象由 state/GC 管理 |
| `ZrCore_Reflection_QueryMembers` / `GetMember` | 按 `SZrReflectionMemberQuery` 查询并取得成员 token | `TZrBool`；状态写入 `EZrReflectionQueryStatus` |
| `ZrCore_Reflection_RequireConstructible` / `CreateInstance` | 检查构造能力并调用构造函数 | `TZrBool`；区分 not-constructible、not-found、ambiguous、constructor-threw |
| `ZrCore_Reflection_ResolveToken` / `InvokeMethodToken` | 把 metadata token 解析为当前 generation 的成员并调用 | `TZrBool`；旧 generation 必须重新解析 |
| `ZrCore_Reflection_ResolveConstructedGenericType` / `RevalidateDynamicGenericTypeInstance` | 解析和重新校验动态泛型实例 | `TZrBool`；失败不得缓存旧 layout |
| `ZrPool_Create` / `ZrPool_CreateFromTypeLayout` | 创建裸布局或 `SZrTypeLayout` 驱动的 pool | `EZrPoolStatus`；成功时写 `SZrPool*` |
| `ZrPool_Deliver` / `Recycle` / `Validate` | 发布、回收和校验稳定句柄 | `EZrPoolStatus`；可返回 `HANDLE_STALE/WRONG_POOL/ENTITY_RETIRED` |
| `ZrPool_TryRead` / `TryBorrow` / `ZrPoolGuard_Release` | 获取读/写 guard 并结束借用 | `EZrPoolStatus`；guard 结束前不能 recycle |
| `ZrPool_Scan` / `TraceGcValues` / `GetStats` | 执行 GC 扫描或读取计数器 | `EZrPoolStatus`；扫描结果写入 out 参数 |
| `ZrPool_Destroy` / `ZrPool_StatusName` | 销毁 pool、格式化状态名 | destroy 返回状态；状态名为静态字符串 |

`SZrPoolHandle` 必须按 `(poolId, slotIndex, generation)` 三元组验证；`SZrPoolGuard` 是
调用方持有的活动借用，必须在同一 pool 上 release。`ZrPool_CreateFromTypeLayout` 的
state、layout registry 和 visitor 都是借用值，调用方须保证它们在 pool 销毁前有效。
