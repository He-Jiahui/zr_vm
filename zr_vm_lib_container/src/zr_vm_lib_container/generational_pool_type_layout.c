#include "generational_pool_internal.h"

#include "zr_vm_core/state.h"

#include <string.h>

static TZrBool zr_pool_canonical_layout_is_admissible(
        const SZrTypeLayout *layout,
        const SZrTypeLayoutRegistryView *registry,
        TZrUInt32 depth) {
    TZrBool rootCanRawCopy;

    if (layout == ZR_NULL || !ZrCore_TypeLayout_Validate(layout) ||
        depth > ZR_TYPE_LAYOUT_MAX_NESTING_DEPTH ||
        layout->copyKind == (TZrUInt8)ZR_TYPE_LAYOUT_COPY_KIND_MOVE_ONLY) {
        return ZR_FALSE;
    }
    if ((layout->kind == (TZrUInt8)ZR_TYPE_LAYOUT_KIND_VALUE ||
         layout->ownershipFieldCount != 0u) &&
        layout->dropKind == (TZrUInt8)ZR_TYPE_LAYOUT_DROP_KIND_NONE) {
        return ZR_FALSE;
    }
    rootCanRawCopy = ZrCore_TypeLayout_CanRawCopy(layout);
    for (TZrUInt32 index = 0u; index < layout->fieldCount; index++) {
        const SZrTypeLayoutField *field = &layout->fields[index];
        const SZrTypeLayout *nestedLayout;

        if ((field->flags & ZR_TYPE_LAYOUT_FIELD_FLAG_NESTED_LAYOUT) == 0u) {
            continue;
        }
        if (registry == ZR_NULL || registry->layouts == ZR_NULL ||
            field->typeLayoutIndex >= registry->count) {
            return ZR_FALSE;
        }
        nestedLayout = registry->layouts[field->typeLayoutIndex];
        if (nestedLayout == ZR_NULL || nestedLayout->byteSize != field->byteSize ||
            !zr_pool_canonical_layout_is_admissible(
                    nestedLayout, registry, depth + 1u) ||
            (rootCanRawCopy && !ZrCore_TypeLayout_CanRawCopy(nestedLayout)) ||
            nestedLayout->gcScanKind > layout->gcScanKind ||
            (nestedLayout->gcScanKind !=
                     (TZrUInt8)ZR_TYPE_LAYOUT_GC_SCAN_FREE &&
             layout->gcFieldOffsets != ZR_NULL) ||
            (nestedLayout->dropKind !=
                     (TZrUInt8)ZR_TYPE_LAYOUT_DROP_KIND_NONE &&
             layout->dropKind ==
                     (TZrUInt8)ZR_TYPE_LAYOUT_DROP_KIND_NONE)) {
            return ZR_FALSE;
        }
    }
    return (TZrBool)(
            layout->kind == (TZrUInt8)ZR_TYPE_LAYOUT_KIND_VALUE ||
            rootCanRawCopy ||
            layout->copyKind ==
                    (TZrUInt8)ZR_TYPE_LAYOUT_COPY_KIND_FIELDWISE);
}

static TZrBool zr_pool_canonical_layout_requires_state(
        const SZrTypeLayout *layout,
        const SZrTypeLayoutRegistryView *registry,
        TZrUInt32 depth) {
    if (layout == ZR_NULL || depth > ZR_TYPE_LAYOUT_MAX_NESTING_DEPTH) {
        return ZR_TRUE;
    }
    if (layout->kind == (TZrUInt8)ZR_TYPE_LAYOUT_KIND_VALUE) {
        return ZR_TRUE;
    }
    for (TZrUInt32 index = 0u; index < layout->fieldCount; index++) {
        const SZrTypeLayoutField *field = &layout->fields[index];

        if ((field->flags & ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT) != 0u) {
            return ZR_TRUE;
        }
        if ((field->flags & ZR_TYPE_LAYOUT_FIELD_FLAG_NESTED_LAYOUT) != 0u) {
            const SZrTypeLayout *nestedLayout;

            if (registry == ZR_NULL || registry->layouts == ZR_NULL ||
                field->typeLayoutIndex >= registry->count) {
                return ZR_TRUE;
            }
            nestedLayout = registry->layouts[field->typeLayoutIndex];
            if (zr_pool_canonical_layout_requires_state(
                        nestedLayout, registry, depth + 1u)) {
                return ZR_TRUE;
            }
        }
    }
    return ZR_FALSE;
}

static TZrBool zr_pool_canonical_initialize(
        void *destination,
        const void *source,
        void *context) {
    SZrPool *pool = (SZrPool *)context;

    if (pool == ZR_NULL || !pool->hasCanonicalLayout ||
        destination == ZR_NULL || source == ZR_NULL ||
        !ZrCore_TypeLayout_InitializeStorageWithRegistry(
                pool->canonicalState,
                &pool->canonicalLayout,
                &pool->canonicalRegistry,
                destination)) {
        return ZR_FALSE;
    }
    if (ZrCore_TypeLayout_CopyInlineWithRegistry(
                pool->canonicalState,
                &pool->canonicalLayout,
                &pool->canonicalRegistry,
                destination,
                source) &&
        (pool->canonicalState == ZR_NULL ||
         pool->canonicalState->threadStatus == ZR_THREAD_STATUS_FINE)) {
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

static void zr_pool_canonical_drop(void *element, void *context) {
    SZrPool *pool = (SZrPool *)context;

    if (pool == ZR_NULL || !pool->hasCanonicalLayout || element == ZR_NULL) {
        return;
    }
    (void)ZrCore_TypeLayout_DropInlineWithRegistry(
            pool->canonicalState,
            &pool->canonicalLayout,
            &pool->canonicalRegistry,
            element);
}

static void zr_pool_canonical_scan(void *element, void *context) {
    SZrPool *pool = (SZrPool *)context;

    if (pool == ZR_NULL || !pool->hasCanonicalLayout || element == ZR_NULL ||
        pool->canonicalGcVisitor == ZR_NULL) {
        return;
    }
    (void)ZrCore_TypeLayout_VisitGcValuesWithRegistry(
            pool->canonicalState,
            &pool->canonicalLayout,
            &pool->canonicalRegistry,
            element,
            pool->canonicalGcVisitor,
            pool->canonicalGcVisitorUserData);
}

EZrPoolStatus ZrPool_CreateFromTypeLayout(
        struct SZrState *state,
        const SZrTypeLayout *layout,
        const SZrTypeLayoutRegistryView *registry,
        FZrTypeLayoutGcValueVisitor visitor,
        TZrPtr visitorUserData,
        const SZrPoolConfig *config,
        SZrPool **outPool) {
    SZrPoolTypeLayout poolLayout;
    SZrPool *pool;
    EZrPoolStatus status;

    if (outPool == ZR_NULL) {
        return ZR_POOL_STATUS_INVALID_ARGUMENT;
    }
    *outPool = ZR_NULL;
    if (layout == ZR_NULL || !ZrCore_TypeLayout_Validate(layout) ||
        layout->byteSize == 0u || layout->byteAlign == 0u ||
        layout->gcScanKind > (TZrUInt8)ZR_TYPE_LAYOUT_GC_SCAN_BARRIERED ||
        (layout->gcScanKind != (TZrUInt8)ZR_TYPE_LAYOUT_GC_SCAN_FREE &&
         visitor == ZR_NULL) ||
        (registry != ZR_NULL && registry->count != 0u &&
         registry->layouts == ZR_NULL) ||
        !zr_pool_canonical_layout_is_admissible(layout, registry, 0u) ||
        ((state == ZR_NULL ||
          (config != ZR_NULL &&
           config->concurrencyMode == ZR_POOL_CONCURRENCY_CONCURRENT)) &&
         zr_pool_canonical_layout_requires_state(layout, registry, 0u))) {
        return ZR_POOL_STATUS_INVALID_ARGUMENT;
    }

    memset(&poolLayout, 0, sizeof(poolLayout));
    poolLayout.elementSize = layout->byteSize;
    poolLayout.elementAlignment = layout->byteAlign;
    poolLayout.gcScanKind = (EZrPoolGcScanKind)layout->gcScanKind;
    poolLayout.initialize = zr_pool_canonical_initialize;
    poolLayout.abortInitialize = zr_pool_canonical_drop;
    poolLayout.drop = layout->dropKind != (TZrUInt8)ZR_TYPE_LAYOUT_DROP_KIND_NONE
                              ? zr_pool_canonical_drop
                              : ZR_NULL;
    poolLayout.scan = layout->gcScanKind !=
                                      (TZrUInt8)ZR_TYPE_LAYOUT_GC_SCAN_FREE
                              ? zr_pool_canonical_scan
                              : ZR_NULL;
    status = ZrPool_Create(&poolLayout, config, &pool);
    if (status != ZR_POOL_STATUS_OK) {
        return status;
    }
    pool->canonicalLayout = *layout;
    if (registry != ZR_NULL) {
        pool->canonicalRegistry = *registry;
    } else {
        memset(&pool->canonicalRegistry, 0, sizeof(pool->canonicalRegistry));
    }
    pool->canonicalState = state;
    pool->canonicalGcVisitor = visitor;
    pool->canonicalGcVisitorUserData = visitorUserData;
    pool->hasCanonicalLayout = ZR_TRUE;
    pool->layout.context = pool;
    *outPool = pool;
    return ZR_POOL_STATUS_OK;
}
