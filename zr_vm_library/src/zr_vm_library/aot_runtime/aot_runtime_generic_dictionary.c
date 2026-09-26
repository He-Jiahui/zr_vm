#include "zr_vm_library/aot_runtime.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_core/type_layout.h"

static const SZrAotGenericSlot *aot_runtime_generic_dictionary_slot(
        const SZrAotGenericDictionary *dictionary,
        TZrUInt32 slotIndex) {
    if (dictionary == ZR_NULL ||
        dictionary->slots == ZR_NULL ||
        slotIndex >= dictionary->slotCount) {
        return ZR_NULL;
    }

    return &dictionary->slots[slotIndex];
}

/* 生成器把 resolvedSlots 放在模块静态存储区；这里借用其可写槽位，不取得字典所有权。 */
static SZrAotGenericResolvedSlot *aot_runtime_generic_dictionary_cache_slot(
        const SZrAotGenericDictionary *dictionary,
        TZrUInt32 slotIndex) {
    if (dictionary == ZR_NULL ||
        dictionary->resolvedSlots == ZR_NULL ||
        slotIndex >= dictionary->slotCount) {
        return ZR_NULL;
    }

    return &dictionary->resolvedSlots[slotIndex];
}

/* 静态布局可直接返回；动态泛型槽必须以当前模块的 metadataRuntime 解析 typeLayoutId。 */
static const SZrTypeLayout *aot_runtime_generic_dictionary_resolve_type_layout(
        const SZrAotGenericSlot *slot,
        SZrMetadataRuntime *metadataRuntime) {
    if (slot == ZR_NULL) {
        return ZR_NULL;
    }

    if (slot->staticTypeLayout != ZR_NULL) {
        return slot->staticTypeLayout;
    }

    return ZrCore_MetadataRuntime_ResolveTypeLayout(metadataRuntime, slot->typeLayoutId);
}

/* 生成方法访问泛型布局的单一入口；缓存结果被后续调用复用。 */
/* TODO: 生成器使用静态 resolvedSlots，命中时只核对 slot/kind，未核对 metadataRuntime。
 * 若同一库在多个运行时复用且同一 id 对应不同布局，可能返回旧运行时的指针；
 * 需补双 global/双 metadataRuntime 装载用例并核对缓存作用域。并发写同一静态槽也需压力验证。 */
const struct SZrTypeLayout *ZrLibrary_AotRuntime_GenericSlot_TypeLayout(
        SZrState *state,
        const SZrAotGenericDictionary *dictionary,
        SZrMetadataRuntime *metadataRuntime,
        TZrUInt32 slotIndex) {
    const SZrAotGenericSlot *slot = aot_runtime_generic_dictionary_slot(dictionary, slotIndex);
    SZrAotGenericResolvedSlot *cache = aot_runtime_generic_dictionary_cache_slot(dictionary, slotIndex);
    const SZrTypeLayout *layout;

    (void)state;

    if (slot == ZR_NULL || slot->kind != (TZrUInt32)ZR_AOT_GENERIC_SLOT_TYPE_LAYOUT) {
        return ZR_NULL;
    }

    if (cache != ZR_NULL &&
        cache->isResolved &&
        cache->kind == (TZrUInt32)ZR_AOT_GENERIC_SLOT_TYPE_LAYOUT &&
        cache->value.typeLayout != ZR_NULL) {
        return cache->value.typeLayout;
    }

    layout = aot_runtime_generic_dictionary_resolve_type_layout(slot, metadataRuntime);
    if (layout == ZR_NULL) {
        return ZR_NULL;
    }

    if (cache != ZR_NULL) {
        cache->kind = (TZrUInt32)ZR_AOT_GENERIC_SLOT_TYPE_LAYOUT;
        cache->isResolved = (TZrUInt8)1u;
        cache->reserved0 = 0u;
        cache->reserved1 = 0u;
        cache->reserved2 = 0u;
        cache->value.typeLayout = layout;
    }

    return layout;
}

/* sizeof 泛型槽与 TypeLayout 共用缓存，结果依赖解析时的 metadataRuntime。 */
/* TODO: 静态缓存未区分不同 metadataRuntime，可能沿用上次实例的 byteSize；
 * 需用同一生成模块在两个运行时中解析不同布局，核对结果和并发访问。 */
TZrBool ZrLibrary_AotRuntime_GenericSlot_TryGetSizeOf(
        SZrState *state,
        const SZrAotGenericDictionary *dictionary,
        SZrMetadataRuntime *metadataRuntime,
        TZrUInt32 slotIndex,
        TZrSize *outSize) {
    const SZrAotGenericSlot *slot = aot_runtime_generic_dictionary_slot(dictionary, slotIndex);
    SZrAotGenericResolvedSlot *cache = aot_runtime_generic_dictionary_cache_slot(dictionary, slotIndex);
    const SZrTypeLayout *layout;

    (void)state;

    if (outSize != ZR_NULL) {
        *outSize = 0u;
    }

    if (slot == ZR_NULL ||
        slot->kind != (TZrUInt32)ZR_AOT_GENERIC_SLOT_SIZEOF ||
        outSize == ZR_NULL) {
        return ZR_FALSE;
    }

    if (cache != ZR_NULL &&
        cache->isResolved &&
        cache->kind == (TZrUInt32)ZR_AOT_GENERIC_SLOT_SIZEOF) {
        *outSize = cache->value.sizeOfValue;
        return ZR_TRUE;
    }

    layout = aot_runtime_generic_dictionary_resolve_type_layout(slot, metadataRuntime);
    if (layout == ZR_NULL) {
        return ZR_FALSE;
    }

    *outSize = (TZrSize)layout->byteSize;
    if (cache != ZR_NULL) {
        cache->kind = (TZrUInt32)ZR_AOT_GENERIC_SLOT_SIZEOF;
        cache->isResolved = (TZrUInt8)1u;
        cache->reserved0 = 0u;
        cache->reserved1 = 0u;
        cache->reserved2 = 0u;
        cache->value.sizeOfValue = *outSize;
    }

    return ZR_TRUE;
}

/* 泛型方法槽目前只从生成注册表的 staticMethod 取 thunk；不借用 metadataFunction 的动态绑定。 */
/* TODO: metadataFunction 参数未参与解析，需核对方法泛型在模块版本变化时是否允许
 * 复用静态 thunk；同一生成字典的并发缓存写也尚无同步契约或压力测试。 */
FZrAotEntryThunk ZrLibrary_AotRuntime_GenericSlot_Method(
        SZrState *state,
        const SZrAotGenericDictionary *dictionary,
        const SZrFunction *metadataFunction,
        TZrUInt32 slotIndex) {
    const SZrAotGenericSlot *slot = aot_runtime_generic_dictionary_slot(dictionary, slotIndex);
    SZrAotGenericResolvedSlot *cache = aot_runtime_generic_dictionary_cache_slot(dictionary, slotIndex);
    FZrAotEntryThunk method;

    (void)state;
    (void)metadataFunction;

    if (slot == ZR_NULL || slot->kind != (TZrUInt32)ZR_AOT_GENERIC_SLOT_METHOD) {
        return ZR_NULL;
    }

    if (cache != ZR_NULL &&
        cache->isResolved &&
        cache->kind == (TZrUInt32)ZR_AOT_GENERIC_SLOT_METHOD &&
        cache->value.method != ZR_NULL) {
        return cache->value.method;
    }

    method = slot->staticMethod;
    if (method == ZR_NULL) {
        return ZR_NULL;
    }

    if (cache != ZR_NULL) {
        cache->kind = (TZrUInt32)ZR_AOT_GENERIC_SLOT_METHOD;
        cache->isResolved = (TZrUInt8)1u;
        cache->reserved0 = 0u;
        cache->reserved1 = 0u;
        cache->reserved2 = 0u;
        cache->value.method = method;
    }

    return method;
}
