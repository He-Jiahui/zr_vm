#include "module/module_internal.h"
#include "zr_vm_core/gc.h"

/* 物化时保存声明接口槽与实现描述符的稳定配对，引用归 receiver 的 GC 图持有。 */
static TZrBool interface_dispatch_append(SZrState *state, SZrObjectPrototype *receiver,
        SZrObjectPrototype *interfacePrototype, TZrUInt32 slot,
        SZrObjectPrototype *implementation, TZrUInt32 descriptorIndex) {
    TZrSize oldBytes = receiver->interfaceDispatchCount * sizeof(SZrInterfaceDispatchEntry);
    SZrInterfaceDispatchEntry *entries;
    for (TZrUInt32 index = 0u; index < receiver->interfaceDispatchCount; ++index) {
        const SZrInterfaceDispatchEntry *entry = &receiver->interfaceDispatchEntries[index];
        if (entry->interfacePrototype == interfacePrototype && entry->interfaceSlot == slot &&
            entry->implementationPrototype == implementation && entry->descriptorIndex == descriptorIndex)
            return ZR_TRUE;
    }
    entries = ZrCore_Memory_RawMalloc(state->global, oldBytes + sizeof(*entries));
    if (entries == ZR_NULL) return ZR_FALSE;
    if (oldBytes != 0u) memcpy(entries, receiver->interfaceDispatchEntries, oldBytes);
    entries[receiver->interfaceDispatchCount].interfacePrototype = interfacePrototype;
    entries[receiver->interfaceDispatchCount].implementationPrototype = implementation;
    entries[receiver->interfaceDispatchCount].interfaceSlot = slot;
    entries[receiver->interfaceDispatchCount].descriptorIndex = descriptorIndex;
    entries[receiver->interfaceDispatchCount].implementationLayoutGeneration = implementation->layoutGeneration;
    if (oldBytes != 0u) ZrCore_Memory_RawFree(state->global, receiver->interfaceDispatchEntries, oldBytes);
    receiver->interfaceDispatchEntries = entries;
    ++receiver->interfaceDispatchCount;
    ZrCore_RawObject_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(receiver), ZR_CAST_RAW_OBJECT_AS_SUPER(interfacePrototype));
    ZrCore_RawObject_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(receiver), ZR_CAST_RAW_OBJECT_AS_SUPER(implementation));
    return ZR_TRUE;
}

/* 原型构造期建立接口分派表；运行时仅以接口身份和槽位寻找已绑定实现。 */
TZrBool zr_module_bind_interface_dispatch(SZrState *state, SZrObjectPrototype *receiver,
                                         SZrObjectPrototype *interfacePrototype) {
    if (receiver == ZR_NULL || interfacePrototype == ZR_NULL ||
        interfacePrototype->type != ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE) return ZR_FALSE;
    for (TZrUInt32 index = 0u; index < interfacePrototype->memberDescriptorCount; ++index) {
        const SZrMemberDescriptor *required = &interfacePrototype->memberDescriptors[index];
        SZrObjectPrototype *implementation = receiver;
        TZrBool matched = ZR_FALSE;
        if (required->isStatic || required->kind != ZR_MEMBER_DESCRIPTOR_KIND_METHOD) continue;
        /* 名称匹配发生在原型物化期；调用时只能依据接口身份、槽和描述符分派。
         * BUG: 同名重载会为同一接口槽追加多个实现，调用端取首项后若签名不合即失败，
         * 不会继续尝试正确重载；编译器原本按签名区分这些成员。 */
        while (implementation != ZR_NULL && !matched) {
            for (TZrUInt32 member = 0u; member < implementation->memberDescriptorCount; ++member) {
                const SZrMemberDescriptor *candidate = &implementation->memberDescriptors[member];
                if (candidate->isStatic || candidate->methodFunction == ZR_NULL ||
                    candidate->name == ZR_NULL || required->name == ZR_NULL ||
                    !ZrCore_String_Equal(candidate->name, required->name)) continue;
                if (!interface_dispatch_append(state, receiver, interfacePrototype,
                        required->interfaceContractSlot, implementation, member)) return ZR_FALSE;
                matched = ZR_TRUE;
            }
            implementation = implementation->superPrototype;
        }
    }
    if (interfacePrototype->superPrototype != ZR_NULL &&
        interfacePrototype->superPrototype->type == ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE)
        return zr_module_bind_interface_dispatch(state, receiver, interfacePrototype->superPrototype);
    return ZR_TRUE;
}
