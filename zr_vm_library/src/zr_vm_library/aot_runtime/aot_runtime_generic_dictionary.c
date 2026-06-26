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
