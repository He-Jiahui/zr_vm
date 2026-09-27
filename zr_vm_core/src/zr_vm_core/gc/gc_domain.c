#include "gc/gc_domain_internal.h"

#include "zr_vm_core/global.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/raw_object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/value.h"

#define ZR_GC_DOMAIN_ROOT_INITIAL_CAPACITY ((TZrSize)8u)
/* 无效槽索引同时用于句柄与对象上的 ownership root 标记；槽代数负责识别复用。 */
#define ZR_GC_DOMAIN_ROOT_NONE (~(TZrUInt32)0u)

/* 失败输出也保持可安全 Release 的空句柄形态，避免旧槽信息被误认为本次创建成功。 */
static void gc_domain_reset_handle(SZrGcRootHandle *handle) {
    if (handle == ZR_NULL) {
        return;
    }
    handle->domain.id = 0u;
    handle->domain.generation = 0u;
    handle->slotIndex = ZR_GC_DOMAIN_ROOT_NONE;
    handle->slotGeneration = 0u;
}

/* 域编号与代数须同时匹配；事务或句柄不得凭历史域编号重新进入已销毁的域。 */
static TZrBool gc_domain_identity_matches(
        const SZrGcDomain *domain,
        SZrGcDomainIdentity identity) {
    return domain != ZR_NULL && domain->active &&
           domain->identity.id == identity.id &&
           domain->identity.generation == identity.generation;
}

/* 对象戳记是跨域引用检查的依据，调用方仍须保证对象指针本身可解引用。 */
static TZrBool gc_domain_object_matches(
        const SZrGcDomain *domain,
        const SZrRawObject *object) {
    return domain != ZR_NULL && object != ZR_NULL && domain->active &&
           object->gcDomainId == domain->identity.id &&
           object->gcDomainGeneration == domain->identity.generation;
}

/* 根表扩容先建新表再替换旧表，使分配失败时所有已有根仍然有效。 */
static TZrBool gc_domain_grow_roots(SZrGcDomain *domain) {
    TZrSize newCapacity;
    TZrSize newBytes;
    SZrGcDomainRootSlot *newRoots;

    if (domain == ZR_NULL || domain->global == ZR_NULL) {
        return ZR_FALSE;
    }
    newCapacity = domain->rootCapacity == 0u
                          ? ZR_GC_DOMAIN_ROOT_INITIAL_CAPACITY
                          : domain->rootCapacity * 2u;
    /* TODO: 容量翻倍和字节乘法未检查溢出；核对 TZrSize 上界及根槽最大数量后限制分配。 */
    newBytes = newCapacity * sizeof(SZrGcDomainRootSlot);
    newRoots = (SZrGcDomainRootSlot *)ZrCore_Memory_RawMallocWithType(
            domain->global, newBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (newRoots == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Memory_RawSet(newRoots, 0, newBytes);
    if (domain->roots != ZR_NULL && domain->rootCapacity > 0u) {
        TZrSize oldBytes = domain->rootCapacity * sizeof(SZrGcDomainRootSlot);
        ZrCore_Memory_RawCopy(newRoots, domain->roots, oldBytes);
        ZrCore_Memory_RawFreeWithType(
                domain->global, domain->roots, oldBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    domain->roots = newRoots;
    domain->rootCapacity = newCapacity;
    return ZR_TRUE;
}

/* 普通句柄与所有权系统共用根表，但分别计数；空闲槽仅在旧代数失效后复用。 */
static TZrBool gc_domain_allocate_root(
        SZrGcDomain *domain,
        SZrRawObject *target,
        EZrGcDomainRootKind kind,
        TZrUInt32 *outIndex,
        TZrUInt32 *outGeneration) {
    TZrSize index;
    SZrGcDomainRootSlot *slot;

    if (domain == ZR_NULL || target == ZR_NULL ||
        !gc_domain_object_matches(domain, target) ||
        outIndex == ZR_NULL || outGeneration == ZR_NULL) {
        return ZR_FALSE;
    }
    for (index = 0u; index < domain->rootLength; index++) {
        if (domain->roots[index].kind == ZR_GC_DOMAIN_ROOT_KIND_FREE) {
            break;
        }
    }
    if (index == domain->rootLength) {
        if (domain->rootLength == domain->rootCapacity && !gc_domain_grow_roots(domain)) {
            return ZR_FALSE;
        }
        domain->rootLength++;
    }
    slot = &domain->roots[index];
    if (slot->generation == 0u) {
        slot->generation = 1u;
    }
    slot->target = target;
    slot->retainCount = 1u;
    slot->kind = kind;
    domain->activeRootCount++;
    if (kind == ZR_GC_DOMAIN_ROOT_KIND_OWNERSHIP) {
        domain->ownershipRootCount++;
    }
    *outIndex = (TZrUInt32)index;
    /* TODO: slotIndex 为 32 位，rootLength 为 TZrSize；核查超出 UINT32_MAX 时的拒绝边界。 */
    *outGeneration = slot->generation;
    return ZR_TRUE;
}

/* 调用方须持有域锁；域代数与槽代数双重校验隔离域销毁和槽复用后的陈旧句柄。 */
static SZrGcDomainRootSlot *gc_domain_resolve_slot(
        const SZrState *state,
        const SZrGcRootHandle *handle,
        EZrGcDomainRootKind expectedKind) {
    SZrGcDomain *domain;
    SZrGcDomainRootSlot *slot;

    if (state == ZR_NULL || handle == ZR_NULL || state->gcDomain == ZR_NULL) {
        return ZR_NULL;
    }
    domain = state->gcDomain;
    if (!gc_domain_identity_matches(domain, handle->domain) ||
        handle->slotIndex >= domain->rootLength) {
        return ZR_NULL;
    }
    slot = &domain->roots[handle->slotIndex];
    if (slot->kind != expectedKind || slot->generation != handle->slotGeneration ||
        slot->retainCount == 0u || slot->target == ZR_NULL) {
        return ZR_NULL;
    }
    return slot;
}

/* clone 先递减共享保留数；最后一个引用释放时才撤销 GC 根并推进槽代数。 */
static void gc_domain_release_slot(SZrGcDomain *domain, SZrGcDomainRootSlot *slot) {
    if (domain == ZR_NULL || slot == ZR_NULL || slot->kind == ZR_GC_DOMAIN_ROOT_KIND_FREE) {
        return;
    }
    if (slot->retainCount > 1u) {
        slot->retainCount--;
        return;
    }
    if (slot->kind == ZR_GC_DOMAIN_ROOT_KIND_OWNERSHIP && domain->ownershipRootCount > 0u) {
        domain->ownershipRootCount--;
    }
    if (domain->activeRootCount > 0u) {
        domain->activeRootCount--;
    }
    slot->target = ZR_NULL;
    slot->retainCount = 0u;
    slot->kind = ZR_GC_DOMAIN_ROOT_KIND_FREE;
    slot->generation++;
    if (slot->generation == 0u) {
        slot->generation = 1u;
    }
}

/* BUG: global.c 忽略域分配失败；后续注册表路径将无域分配的空对象交给 Object_Init，存在空指针访问。 */
SZrGcDomain *ZrCore_GcDomain_New(
        SZrGlobalState *global,
        SZrGarbageCollector *collector) {
    SZrGcDomain *domain;

    if (global == ZR_NULL || collector == ZR_NULL) {
        return ZR_NULL;
    }
    domain = (SZrGcDomain *)ZrCore_Memory_RawMallocWithType(
            global, sizeof(SZrGcDomain), ZR_MEMORY_NATIVE_TYPE_MANAGER);
    if (domain == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_Memory_RawSet(domain, 0, sizeof(SZrGcDomain));
    domain->global = global;
    domain->collector = collector;
    domain->identity.id = global->cacheIdentity != 0u ? global->cacheIdentity : 1u;
    domain->identity.generation = 1u;
    domain->active = ZR_TRUE;
    if (!ZrCore_GcDomain_CoordinationInit(domain)) {
        ZrCore_Memory_RawFreeWithType(
                global, domain, sizeof(SZrGcDomain), ZR_MEMORY_NATIVE_TYPE_MANAGER);
        return ZR_NULL;
    }
    return domain;
}

/* global 析构先撤下主 state，再使代际身份失效并释放域持有的两张表。 */
void ZrCore_GcDomain_Free(SZrGcDomain *domain) {
    SZrGlobalState *global;

    if (domain == ZR_NULL || domain->global == ZR_NULL) {
        return;
    }
    global = domain->global;
    ZrCore_GcDomain_Lock(domain);
    domain->active = ZR_FALSE;
    domain->identity.generation++;
    domain->pauseRequested = ZR_FALSE;
    domain->collectorState = ZR_NULL;
    domain->pauseDepth = 0u;
    ZrCore_GcDomain_Broadcast(domain);
    ZrCore_GcDomain_Unlock(domain);
    if (domain->roots != ZR_NULL && domain->rootCapacity > 0u) {
        ZrCore_Memory_RawFreeWithType(
                global,
                domain->roots,
                domain->rootCapacity * sizeof(SZrGcDomainRootSlot),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (domain->mutators != ZR_NULL && domain->mutatorCapacity > 0u) {
        ZrCore_Memory_RawFreeWithType(
                global,
                domain->mutators,
                domain->mutatorCapacity * sizeof(SZrGcDomainMutatorRecord),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    ZrCore_GcDomain_CoordinationFree(domain);
    ZrCore_Memory_RawFreeWithType(
            global, domain, sizeof(SZrGcDomain), ZR_MEMORY_NATIVE_TYPE_MANAGER);
}

/* 先登记 mutator 再暴露 state->gcDomain，确保安全点扫描能看到已附着 state。 */
void ZrCore_GcDomain_AttachState(SZrGcDomain *domain, SZrState *state) {
    if (domain == ZR_NULL || state == ZR_NULL || !domain->active) {
        return;
    }
    if (!ZrCore_GcDomain_RegisterMutator(domain, state)) {
        /* BUG: mutator 表扩容失败使 state 无域；global.c 不检查，后续注册表路径把空对象交给 Object_Init。 */
        return;
    }
    state->gcDomain = domain;
    ZrCore_GcDomain_Lock(domain);
    if (domain->attachedState == ZR_NULL) {
        domain->attachedState = state;
    }
    ZrCore_GcDomain_Unlock(domain);
    (void)ZrCore_GcDomain_AssignObject(domain, &state->super);
}

/* 解除 mutator 登记后再清除 state 链接；调用前所有执行与原生作用域须已退出。 */
void ZrCore_GcDomain_DetachState(SZrGcDomain *domain, SZrState *state) {
    if (domain == ZR_NULL || state == ZR_NULL) {
        return;
    }
    ZrCore_GcDomain_UnregisterMutator(domain, state);
    ZrCore_GcDomain_Lock(domain);
    if (domain->attachedState == state) {
        domain->attachedState = domain->mutatorLength > 0u
                                        ? domain->mutators[0].state
                                        : ZR_NULL;
    }
    ZrCore_GcDomain_Unlock(domain);
    if (state->gcDomain == domain) {
        state->gcDomain = ZR_NULL;
    }
}

/* 新对象及附着 state 的 super 使用同一域戳记，供写屏障和跨域传输预检。 */
TZrBool ZrCore_GcDomain_AssignObject(SZrGcDomain *domain, SZrRawObject *object) {
    if (domain == ZR_NULL || object == ZR_NULL || !domain->active) {
        return ZR_FALSE;
    }
    object->gcDomainId = domain->identity.id;
    object->gcDomainGeneration = domain->identity.generation;
    return ZR_TRUE;
}

SZrGcDomainIdentity ZrCore_GcDomain_GetIdentity(const SZrState *state) {
    SZrGcDomainIdentity identity = {0u, 0u};
    if (state != ZR_NULL && state->gcDomain != ZR_NULL && state->gcDomain->active) {
        identity = state->gcDomain->identity;
    }
    return identity;
}

TZrBool ZrCore_GcDomain_IdentityEquals(
        SZrGcDomainIdentity left,
        SZrGcDomainIdentity right) {
    return left.id == right.id && left.generation == right.generation;
}

TZrBool ZrCore_GcDomain_IdentityIsCurrent(
        const SZrState *state,
        SZrGcDomainIdentity identity) {
    return state != ZR_NULL && gc_domain_identity_matches(state->gcDomain, identity);
}

TZrBool ZrCore_GcDomain_GetObjectIdentity(
        const SZrRawObject *object,
        SZrGcDomainIdentity *outIdentity) {
    if (outIdentity != ZR_NULL) {
        outIdentity->id = 0u;
        outIdentity->generation = 0u;
    }
    if (object == ZR_NULL || outIdentity == ZR_NULL ||
        object->gcDomainId == 0u || object->gcDomainGeneration == 0u) {
        return ZR_FALSE;
    }
    outIdentity->id = object->gcDomainId;
    outIdentity->generation = object->gcDomainGeneration;
    return ZR_TRUE;
}

TZrBool ZrCore_GcDomain_ObjectBelongsToState(
        const SZrState *state,
        const SZrRawObject *object) {
    return state != ZR_NULL && gc_domain_object_matches(state->gcDomain, object);
}

TZrBool ZrCore_GcDomain_ValidateWrite(
        const SZrState *state,
        const SZrRawObject *owner,
        const SZrRawObject *target) {
    if (state == ZR_NULL || owner == ZR_NULL ||
        !gc_domain_object_matches(state->gcDomain, owner)) {
        return ZR_FALSE;
    }
    return target == ZR_NULL || gc_domain_object_matches(state->gcDomain, target);
}

TZrBool ZrCore_GcDomain_ValidateValueWrite(
        const SZrState *state,
        const SZrRawObject *owner,
        const SZrTypeValue *value) {
    if (state == ZR_NULL || owner == ZR_NULL ||
        !gc_domain_object_matches(state->gcDomain, owner)) {
        return ZR_FALSE;
    }
    if (value == ZR_NULL || !value->isGarbageCollectable ||
        value->value.object == ZR_NULL) {
        return ZR_TRUE;
    }
    return gc_domain_object_matches(state->gcDomain, value->value.object);
}

/* task/checkpoint/跨域图重建通过句柄持有对象，输出槽的释放责任转给调用方。 */
TZrBool ZrCore_GcRootHandle_Create(
        SZrState *state,
        SZrRawObject *target,
        SZrGcRootHandle *outHandle) {
    TZrUInt32 index;
    TZrUInt32 generation;

    if (outHandle == ZR_NULL) {
        return ZR_FALSE;
    }
    gc_domain_reset_handle(outHandle);
    if (state == ZR_NULL || state->gcDomain == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_GcDomain_Lock(state->gcDomain);
    if (!gc_domain_allocate_root(state->gcDomain, target,
                                 ZR_GC_DOMAIN_ROOT_KIND_HANDLE,
                                 &index, &generation)) {
        ZrCore_GcDomain_Unlock(state->gcDomain);
        return ZR_FALSE;
    }
    outHandle->domain = state->gcDomain->identity;
    outHandle->slotIndex = index;
    outHandle->slotGeneration = generation;
    ZrCore_GcDomain_Unlock(state->gcDomain);
    return ZR_TRUE;
}

/* clone 持有同一根槽的一份份额，不另建根；后续 Update 才按需分离。 */
TZrBool ZrCore_GcRootHandle_Clone(
        SZrState *state,
        const SZrGcRootHandle *source,
        SZrGcRootHandle *outHandle) {
    SZrGcDomainRootSlot *slot;

    if (source == ZR_NULL || outHandle == ZR_NULL || outHandle == source) {
        return ZR_FALSE;
    }
    if (state == ZR_NULL || state->gcDomain == ZR_NULL) {
        gc_domain_reset_handle(outHandle);
        return ZR_FALSE;
    }
    ZrCore_GcDomain_Lock(state->gcDomain);
    slot = gc_domain_resolve_slot(state, source, ZR_GC_DOMAIN_ROOT_KIND_HANDLE);
    if (slot == ZR_NULL || slot->retainCount == ~(TZrUInt32)0u) {
        ZrCore_GcDomain_Unlock(state->gcDomain);
        gc_domain_reset_handle(outHandle);
        return ZR_FALSE;
    }
    slot->retainCount++;
    *outHandle = *source;
    ZrCore_GcDomain_Unlock(state->gcDomain);
    return ZR_TRUE;
}

/* 对共享句柄采用写时分离，避免改换一个调用方的根时影响其他 clone。 */
TZrBool ZrCore_GcRootHandle_Update(
        SZrState *state,
        SZrGcRootHandle *handle,
        SZrRawObject *target) {
    SZrGcDomain *domain;
    SZrGcDomainRootSlot *slot;
    TZrUInt32 oldIndex;

    if (state == ZR_NULL || state->gcDomain == ZR_NULL) {
        return ZR_FALSE;
    }
    domain = state->gcDomain;
    ZrCore_GcDomain_Lock(domain);
    slot = gc_domain_resolve_slot(
            state, handle, ZR_GC_DOMAIN_ROOT_KIND_HANDLE);
    if (slot == ZR_NULL || !gc_domain_object_matches(domain, target)) {
        ZrCore_GcDomain_Unlock(domain);
        return ZR_FALSE;
    }
    if (slot->retainCount > 1u) {
        TZrUInt32 index;
        TZrUInt32 generation;

        oldIndex = handle->slotIndex;
        if (!gc_domain_allocate_root(domain,
                                     target,
                                     ZR_GC_DOMAIN_ROOT_KIND_HANDLE,
                                     &index,
                                     &generation)) {
            ZrCore_GcDomain_Unlock(domain);
            return ZR_FALSE;
        }
        domain->roots[oldIndex].retainCount--;
        handle->slotIndex = index;
        handle->slotGeneration = generation;
        ZrCore_GcDomain_Unlock(domain);
        return ZR_TRUE;
    }
    slot->target = target;
    ZrCore_GcDomain_Unlock(domain);
    return ZR_TRUE;
}

/* 只在锁内验证槽位；返回的是借用对象指针，调用方跨 GC 边界应重新解析句柄。 */
TZrBool ZrCore_GcRootHandle_Resolve(
        const SZrState *state,
        const SZrGcRootHandle *handle,
        SZrRawObject **outTarget) {
    SZrGcDomainRootSlot *slot;
    if (outTarget != ZR_NULL) {
        *outTarget = ZR_NULL;
    }
    if (outTarget == ZR_NULL) {
        return ZR_FALSE;
    }
    if (state == ZR_NULL || state->gcDomain == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_GcDomain_Lock(state->gcDomain);
    slot = gc_domain_resolve_slot(state, handle, ZR_GC_DOMAIN_ROOT_KIND_HANDLE);
    if (slot == ZR_NULL || !gc_domain_object_matches(state->gcDomain, slot->target)) {
        ZrCore_GcDomain_Unlock(state->gcDomain);
        return ZR_FALSE;
    }
    *outTarget = slot->target;
    ZrCore_GcDomain_Unlock(state->gcDomain);
    return ZR_TRUE;
}

void ZrCore_GcRootHandle_Release(SZrState *state, SZrGcRootHandle *handle) {
    SZrGcDomainRootSlot *slot = ZR_NULL;
    if (state != ZR_NULL && state->gcDomain != ZR_NULL) {
        ZrCore_GcDomain_Lock(state->gcDomain);
        slot = gc_domain_resolve_slot(
                state, handle, ZR_GC_DOMAIN_ROOT_KIND_HANDLE);
    }
    if (slot != ZR_NULL) {
        gc_domain_release_slot(state->gcDomain, slot);
    }
    if (state != ZR_NULL && state->gcDomain != ZR_NULL) {
        ZrCore_GcDomain_Unlock(state->gcDomain);
    }
    gc_domain_reset_handle(handle);
}

TZrSize ZrCore_GcDomain_GetRootCount(const SZrState *state) {
    TZrSize count = 0u;
    if (state != ZR_NULL && state->gcDomain != ZR_NULL) {
        ZrCore_GcDomain_Lock(state->gcDomain);
        count = state->gcDomain->activeRootCount;
        ZrCore_GcDomain_Unlock(state->gcDomain);
    }
    return count;
}

TZrSize ZrCore_GcDomain_GetOwnershipRootCount(const SZrState *state) {
    TZrSize count = 0u;
    if (state != ZR_NULL && state->gcDomain != ZR_NULL) {
        ZrCore_GcDomain_Lock(state->gcDomain);
        count = state->gcDomain->ownershipRootCount;
        ZrCore_GcDomain_Unlock(state->gcDomain);
    }
    return count;
}

/* unique/resource 的根与普通句柄根分开统计，并把槽身份回写对象供释放时核对。 */
TZrBool ZrCore_GcDomain_RegisterOwnershipRoot(
        SZrState *state,
        SZrRawObject *object) {
    SZrGcDomain *domain;
    TZrUInt32 index;
    TZrUInt32 generation;

    if (state == ZR_NULL || object == ZR_NULL || state->gcDomain == ZR_NULL) {
        return ZR_FALSE;
    }
    domain = state->gcDomain;
    ZrCore_GcDomain_Lock(domain);
    if (object->ownershipRootIndex < domain->rootLength) {
        SZrGcDomainRootSlot *existing = &domain->roots[object->ownershipRootIndex];
        if (existing->kind == ZR_GC_DOMAIN_ROOT_KIND_OWNERSHIP &&
            existing->generation == object->ownershipRootGeneration &&
            existing->target == object) {
            ZrCore_GcDomain_Unlock(domain);
            return ZR_TRUE;
        }
    }
    if (!gc_domain_allocate_root(domain, object,
                                 ZR_GC_DOMAIN_ROOT_KIND_OWNERSHIP,
                                 &index, &generation)) {
        ZrCore_GcDomain_Unlock(domain);
        return ZR_FALSE;
    }
    object->ownershipRootIndex = index;
    object->ownershipRootGeneration = generation;
    ZrCore_GcDomain_Unlock(domain);
    return ZR_TRUE;
}

/* 只撤销仍由该对象持有的同代槽，防止迟到的释放清掉已复用根。 */
void ZrCore_GcDomain_UnregisterOwnershipRoot(
        SZrState *state,
        SZrRawObject *object) {
    SZrGcDomain *domain;
    SZrGcDomainRootSlot *slot;

    if (state == ZR_NULL || object == ZR_NULL || state->gcDomain == ZR_NULL) {
        return;
    }
    domain = state->gcDomain;
    ZrCore_GcDomain_Lock(domain);
    if (object->ownershipRootIndex >= domain->rootLength) {
        ZrCore_GcDomain_Unlock(domain);
        return;
    }
    slot = &domain->roots[object->ownershipRootIndex];
    if (slot->kind != ZR_GC_DOMAIN_ROOT_KIND_OWNERSHIP ||
        slot->generation != object->ownershipRootGeneration ||
        slot->target != object) {
        ZrCore_GcDomain_Unlock(domain);
        return;
    }
    gc_domain_release_slot(domain, slot);
    object->ownershipRootIndex = ZR_GC_DOMAIN_ROOT_NONE;
    object->ownershipRootGeneration = 0u;
    ZrCore_GcDomain_Unlock(domain);
}

TZrBool ZrCore_GcDomain_IsOwnershipRoot(
        const SZrState *state,
        const SZrRawObject *object) {
    SZrGcDomain *domain;
    const SZrGcDomainRootSlot *slot;

    if (state == ZR_NULL || object == ZR_NULL || state->gcDomain == ZR_NULL) {
        return ZR_FALSE;
    }
    domain = state->gcDomain;
    ZrCore_GcDomain_Lock(domain);
    if (object->ownershipRootIndex >= domain->rootLength) {
        ZrCore_GcDomain_Unlock(domain);
        return ZR_FALSE;
    }
    slot = &domain->roots[object->ownershipRootIndex];
    {
        TZrBool result = slot->kind == ZR_GC_DOMAIN_ROOT_KIND_OWNERSHIP &&
                         slot->generation == object->ownershipRootGeneration &&
                         slot->target == object && gc_domain_object_matches(domain, object);
        ZrCore_GcDomain_Unlock(domain);
        return result;
    }
}
