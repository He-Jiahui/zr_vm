---
related_code:
  - zr_vm_core/include/zr_vm_core/reflection.h
  - zr_vm_core/include/zr_vm_core/metadata_token.h
  - zr_vm_core/include/zr_vm_core/type_layout.h
  - zr_vm_core/src/zr_vm_core/reflection.c
  - zr_vm_lib_container/include/zr_vm_lib_container/generational_pool.h
  - zr_vm_lib_container/src/zr_vm_lib_container/generational_pool.c
  - zr_vm_lib_container/src/zr_vm_lib_container/pooling.c
  - zr_vm_library/include/zr_vm_library/native_registry.h
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
doc_type: api-reference
---

# 反射与稳定槽池 API

zr.reflection 是 contract-only provider，负责把 canonical type/metadata 投影给 runtime、
AOT 和工具；zr.pooling 是可 materialize 的 Runtime provider，使用 stable slab 和 generation
handle。两者都强调“身份 + generation + layout”而不是裸地址。

## TypeId 和 type descriptor

~~~zr
let typeObject = typeid(zr.container.Array<int>);
let actualType = typeof(value);
~~~

C 层核心入口：

| 函数 | 结果 |
| --- | --- |
| ZrCore_Reflection_TypeOfValue | 从 SZrTypeValue 构建运行时 type descriptor。 |
| ZrCore_Reflection_BuildTypeLiteralObject | 从类型名建立 type literal object。 |
| BuildTypeIdObject / ReadTypeIdObject | 建立/读取带 identity、signature hash、generation 的 TypeId。 |
| ResolveTypeIdObject | 在当前 metadata registry 解析 TypeId。 |
| IsTypeIdObject / IsReflectionObject | 类型检查。 |
| FormatObject | 生成 bounded 调试文本。 |

SZrReflectionTypeIdentity 包含 canonicalTypeId、metadata token、signatureHash、metadataGeneration
和 category（class、struct、interface、resource、ref struct、enum 等）。reload 后 generation
变化，即使 canonical name 相同也不能复用旧 token/layout。

## 成员查询

~~~c
SZrReflectionMemberQuery query;
ZrCore_Reflection_MemberQueryInitDefault(&query);
query.scope = ZR_REFLECTION_MEMBER_SCOPE_ALL;
query.access = ZR_REFLECTION_MEMBER_ACCESS_PUBLIC;
SZrObject *members = ZR_NULL;
EZrReflectionQueryStatus status;
if (!ZrCore_Reflection_QueryMembers(state, typeDescriptor,
                                    ZR_REFLECTION_MEMBER_KIND_ANY,
                                    &query, &members, &status)) {
    /* inspect status */
}
~~~

query 可选择 declared/inherited/all、public/protected/private/all、instance/static/all，
并控制 compiler-generated、meta methods 和 non-public capability。GetMember 额外接受名称、
参数 TypeId 列表和数量，用于 overload resolution。

| status | 含义 |
| --- | --- |
| OK | 唯一匹配。 |
| NOT_FOUND | 名称/签名不存在。 |
| AMBIGUOUS | 多个候选同样精确。 |
| ACCESS_DENIED | capability 不足。 |
| INVALID_ARGUMENT | query/type/name 非法。 |
| METADATA_NOT_PRESERVED | artifact 未保留需要的 metadata。 |

member cache 只缓存当前 metadata generation；调试统计由
ZrCore_Reflection_DebugResetMemberCacheStats/DebugGetMemberCacheStats 读取。

## 构造和调用 token

RequireConstructible 先拒绝 abstract/interface/resource/ref-like/open-generic 等不可构造
类型；CreateInstance 再按 constructor signature 做 arity/type conversion。状态包括
TYPE_NOT_CONSTRUCTIBLE、CONSTRUCTOR_NOT_FOUND、CONSTRUCTOR_AMBIGUOUS、CONSTRUCTOR_THREW 和
INVALID_ARGUMENT。constructor 抛错时保留原始 exception，不把失败伪装成 null instance。

ResolveToken/InvokeMethodToken 将 metadata token 解析为当前 generation 的 method/field
layout；field read/write helper 支持 nested path 和 primitive specialization。token 是 borrowed
metadata reference，不能跨 module unload 保存。

## 动态泛型实例

SZrReflectionGenericTypeArgument 支持 primitive、type token、array、tuple、ownership、
nullable 和 union。ResolveConstructedGenericType 返回 route（AOT 或 interpreter deopt）、
generic signature hash、typeLayoutId 和 layout pointer；RevalidateDynamicGenericTypeInstance
在 reload/registry 改变后重新检查。动态实例 cache key 必须包含所有实参和 generation。

## Pool handle

~~~zr
let pool = init zr.pooling.Pool<int>();
let handle = pool.deliver(42);
if (pool.isLive(handle)) {
    let view = pool.tryRead(handle);
    let value = view.value;
    view.close();
}
pool.recycle(handle);
~~~

| 类型/操作 | 语义 |
| --- | --- |
| BufferPool.rent<T>(length) | 分配/复用连续 PoolLease<T>。 |
| PoolLease.span/close/index | scoped 可写 view；close 幂等。 |
| Pool<T>.deliver(value) | 完整初始化后发布三元组 handle。 |
| isLive(handle) | 检查 pool identity、slot index、generation。 |
| recycle(handle) | 使旧 generation 失效；活动 guard 结束后 drop。 |
| tryRead(handle,out PoolReadRef) | 只读 guard；可并存。 |
| tryBorrow(handle,out PoolRef) | 可写 guard；与其它 guard 互斥。 |
| PoolRef/PoolReadRef.value/close | scoped projection，必须在同一 pool release。 |

generation 单调递增且不回绕，slot 重用不会让旧 handle 复活。Pool<T> layout 来源可以是
SZrTypeLayout；GcFree 不扫描，GcMapped 按字段扫描，GcBarriered 记录 dirty card。
deliver 失败必须 abortInitialize/清零，不能发布半初始化值。

## Pool C API

| 函数族 | 作用/返回 |
| --- | --- |
| ZrPool_Create / CreateFromTypeLayout | 创建 pool；返回 EZrPoolStatus 和 SZrPool*。 |
| ZrPool_Deliver | 发布值并写 SZrPoolHandle。 |
| ZrPool_Recycle / Validate | 回收/验证 handle；可返回 HANDLE_STALE、WRONG_POOL、ENTITY_RETIRED。 |
| ZrPool_TryRead / TryBorrow | 取得 guard。 |
| ZrPoolGuard_Release | 结束借用；同一 guard 只能 release 一次。 |
| ZrPool_Scan / TraceGcValues / GetStats | GC scan 或读取统计。 |
| ZrPool_Destroy / StatusName | 销毁 pool、取得静态状态名。 |

SZrPoolHandle 必须按 (poolId, slotIndex, generation) 完整验证。state、layout registry 和
visitor 是 borrowed；调用方须保证它们在 pool 销毁前有效。recycle 前若有任何 guard 未 release，
应返回 busy/active-borrow status，而不是强行释放。

## 反射和池化边界

runtime-only 字段不出现在 reflection；POOL_REF_PROJECTION 投影为 getter-only 或可写 property，
不会暴露隐藏 provider method。ref-like 类型不能通过反射构造；metadata/layout 缺失或跨 registry
时 fail-closed。LSP、artifact、AOT 和 runtime 必须使用同一 layout id/hash。

## 使用顺序

~~~text
resolve TypeId -> check generation/layout -> query token
  -> construct/invoke or acquire pool guard
  -> finish read/write -> release guard
  -> invalidate caches on reload
~~~

调试器或插件若缓存 token/handle，必须同时缓存 generation 和 owner identity；收到 reload/
invalidate 事件立即丢弃。更多类型 identity 规则见泛型、接口与协议页。
