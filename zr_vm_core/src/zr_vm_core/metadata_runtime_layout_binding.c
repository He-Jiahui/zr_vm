#include "zr_vm_core/metadata_runtime.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"

static const SZrZrpMetadataTypeDefRow *metadata_runtime_find_type_def_row(SZrMetadataRuntime *runtime,
                                                                          TZrMetadataToken typeDefToken) {
    SZrZrpMetadataSectionView view;
    const SZrZrpMetadataTypeDefRow *rows;
    TZrUInt32 index;

    if (runtime == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(typeDefToken) != ZR_METADATA_TABLE_TYPE_DEF ||
        !ZrCore_MetadataRuntime_GetZrpSectionView(runtime, ZR_ZRP_METADATA_SECTION_TYPE_DEFS, &view) ||
        view.data == ZR_NULL ||
        view.elementSize != (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow)) {
        return ZR_NULL;
    }

    rows = (const SZrZrpMetadataTypeDefRow *)(const void *)view.data;
    for (index = 0u; index < view.count; ++index) {
        if (rows[index].token == typeDefToken) {
            return &rows[index];
        }
    }

    return ZR_NULL;
}

static const SZrZrpMetadataTypeSpecRow *metadata_runtime_find_type_spec_row(SZrMetadataRuntime *runtime,
                                                                            TZrMetadataToken typeSpecToken) {
    SZrZrpMetadataSectionView view;
    const SZrZrpMetadataTypeSpecRow *rows;
    TZrUInt32 index;

    if (runtime == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(typeSpecToken) != ZR_METADATA_TABLE_TYPE_SPEC ||
        !ZrCore_MetadataRuntime_GetZrpSectionView(runtime, ZR_ZRP_METADATA_SECTION_TYPE_SPECS, &view) ||
        view.data == ZR_NULL ||
        view.elementSize != (TZrUInt32)sizeof(SZrZrpMetadataTypeSpecRow)) {
        return ZR_NULL;
    }

    rows = (const SZrZrpMetadataTypeSpecRow *)(const void *)view.data;
    for (index = 0u; index < view.count; ++index) {
        if (rows[index].token == typeSpecToken) {
            return &rows[index];
        }
    }

    return ZR_NULL;
}

static const SZrZrpMetadataFieldDefRow *metadata_runtime_find_field_def_row(SZrMetadataRuntime *runtime,
                                                                            TZrMetadataToken fieldDefToken,
                                                                            TZrUInt32 *outRowIndex) {
    SZrZrpMetadataSectionView view;
    const SZrZrpMetadataFieldDefRow *rows;
    TZrUInt32 index;

    if (outRowIndex != ZR_NULL) {
        *outRowIndex = ~(TZrUInt32)0u;
    }
    if (runtime == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(fieldDefToken) != ZR_METADATA_TABLE_MEMBER_DEF ||
        !ZrCore_MetadataRuntime_GetZrpSectionView(runtime, ZR_ZRP_METADATA_SECTION_FIELD_DEFS, &view) ||
        view.data == ZR_NULL ||
        view.elementSize != (TZrUInt32)sizeof(SZrZrpMetadataFieldDefRow)) {
        return ZR_NULL;
    }

    rows = (const SZrZrpMetadataFieldDefRow *)(const void *)view.data;
    for (index = 0u; index < view.count; ++index) {
        if (rows[index].token == fieldDefToken) {
            if (outRowIndex != ZR_NULL) {
                *outRowIndex = index;
            }
            return &rows[index];
        }
    }

    return ZR_NULL;
}

static TZrBool metadata_runtime_type_def_contains_field_index(const SZrZrpMetadataTypeDefRow *typeDefRow,
                                                              TZrUInt32 fieldRowIndex) {
    TZrUInt32 firstFieldIndex;
    TZrUInt32 fieldCount;

    if (typeDefRow == ZR_NULL) {
        return ZR_FALSE;
    }

    firstFieldIndex = typeDefRow->firstFieldDefIndex;
    fieldCount = typeDefRow->fieldDefCount;
    return fieldRowIndex >= firstFieldIndex &&
           fieldRowIndex - firstFieldIndex < fieldCount;
}

static const SZrTypeLayout *metadata_runtime_find_type_layout_cache_by_token(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken typeToken,
        TZrUInt32 *outTypeLayoutId) {
    TZrUInt32 index;

    if (runtime == ZR_NULL || typeToken == 0u) {
        return ZR_NULL;
    }
    for (index = 0u; index < ZR_METADATA_RUNTIME_TYPE_LAYOUT_CACHE_CAPACITY; index++) {
        if (runtime->typeLayoutCacheTokens[index] == typeToken &&
            runtime->typeLayoutCacheLayouts[index] != ZR_NULL) {
            if (outTypeLayoutId != ZR_NULL) {
                *outTypeLayoutId = runtime->typeLayoutCacheIds[index];
            }
            return runtime->typeLayoutCacheLayouts[index];
        }
    }
    return ZR_NULL;
}

static TZrMetadataToken metadata_runtime_find_type_layout_cache_by_id(
        SZrMetadataRuntime *runtime,
        TZrUInt32 typeLayoutId) {
    TZrUInt32 index;

    if (runtime == ZR_NULL || typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
        return 0u;
    }
    for (index = 0u; index < ZR_METADATA_RUNTIME_TYPE_LAYOUT_CACHE_CAPACITY; index++) {
        if (runtime->typeLayoutCacheIds[index] == typeLayoutId &&
            runtime->typeLayoutCacheTokens[index] != 0u &&
            runtime->typeLayoutCacheLayouts[index] != ZR_NULL) {
            return runtime->typeLayoutCacheTokens[index];
        }
    }
    return 0u;
}

static void metadata_runtime_store_type_layout_cache(SZrMetadataRuntime *runtime,
                                                     TZrMetadataToken typeToken,
                                                     TZrUInt32 typeLayoutId,
                                                     const SZrTypeLayout *typeLayout) {
    TZrUInt32 index;

    if (runtime == ZR_NULL ||
        typeToken == 0u ||
        typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
        typeLayout == ZR_NULL) {
        return;
    }

    for (index = 0u; index < ZR_METADATA_RUNTIME_TYPE_LAYOUT_CACHE_CAPACITY; index++) {
        if (runtime->typeLayoutCacheTokens[index] == typeToken ||
            runtime->typeLayoutCacheTokens[index] == 0u) {
            runtime->typeLayoutCacheTokens[index] = typeToken;
            runtime->typeLayoutCacheIds[index] = typeLayoutId;
            runtime->typeLayoutCacheLayouts[index] = typeLayout;
            return;
        }
    }

    index = runtime->typeLayoutCacheNextIndex % ZR_METADATA_RUNTIME_TYPE_LAYOUT_CACHE_CAPACITY;
    runtime->typeLayoutCacheTokens[index] = typeToken;
    runtime->typeLayoutCacheIds[index] = typeLayoutId;
    runtime->typeLayoutCacheLayouts[index] = typeLayout;
    runtime->typeLayoutCacheNextIndex =
            (index + 1u) % ZR_METADATA_RUNTIME_TYPE_LAYOUT_CACHE_CAPACITY;
}

static TZrBool metadata_runtime_is_layout_type_token(TZrMetadataToken typeToken) {
    TZrUInt32 table = ZR_METADATA_TOKEN_TABLE(typeToken);
    return (TZrBool)(table == ZR_METADATA_TABLE_TYPE_DEF ||
                     table == ZR_METADATA_TABLE_TYPE_SPEC);
}

static TZrMetadataToken metadata_runtime_resolve_registration_type_layout_token(
        SZrMetadataRuntime *runtime,
        TZrUInt32 typeLayoutId) {
    const SZrTypeLayout *typeLayout;
    TZrMetadataToken typeToken;

    if (runtime == ZR_NULL ||
        runtime->codeRegistration == ZR_NULL ||
        runtime->codeRegistration->typeLayoutTokens == ZR_NULL ||
        typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
        typeLayoutId >= runtime->typeLayoutTokenCount) {
        return 0u;
    }

    typeToken = runtime->codeRegistration->typeLayoutTokens[typeLayoutId];
    if (typeToken == 0u || !metadata_runtime_is_layout_type_token(typeToken)) {
        return 0u;
    }

    typeLayout = ZrCore_MetadataRuntime_ResolveTypeLayout(runtime, typeLayoutId);
    if (typeLayout == ZR_NULL) {
        return 0u;
    }

    metadata_runtime_store_type_layout_cache(runtime, typeToken, typeLayoutId, typeLayout);
    return typeToken;
}

TZrBool ZrCore_MetadataRuntime_ReadTypeDefLayoutBindingView(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken typeDefToken,
        SZrMetadataRuntimeTypeDefLayoutBindingView *outView) {
    const SZrMetadataTokenRecord *typeRecord;
    const SZrZrpMetadataTypeDefRow *typeDefRow;
    const SZrTypeLayout *typeLayout;

    if (outView != ZR_NULL) {
        ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
    }
    if (runtime == ZR_NULL ||
        outView == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(typeDefToken) != ZR_METADATA_TABLE_TYPE_DEF) {
        return ZR_FALSE;
    }

    typeRecord = ZrCore_MetadataRuntime_ResolveTypeRecord(runtime, typeDefToken);
    typeDefRow = metadata_runtime_find_type_def_row(runtime, typeDefToken);
    if (typeRecord == ZR_NULL ||
        typeDefRow == ZR_NULL ||
        typeDefRow->typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
        return ZR_FALSE;
    }

    typeLayout = ZrCore_MetadataRuntime_ResolveTypeLayout(runtime, typeDefRow->typeLayoutId);

    outView->typeDefToken = typeDefToken;
    outView->typeRecord = typeRecord;
    outView->typeDefRow = typeDefRow;
    outView->typeLayoutId = typeDefRow->typeLayoutId;
    outView->cTypeId = typeDefRow->typeLayoutId;
    outView->layoutVersion = typeRecord->layoutVersion;
    outView->layoutHash = typeRecord->layoutHash;
    outView->typeLayout = typeLayout;
    return ZR_TRUE;
}

TZrBool ZrCore_MetadataRuntime_ReadTypeSpecLayoutBindingView(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken typeSpecToken,
        SZrMetadataRuntimeTypeSpecLayoutBindingView *outView) {
    const SZrMetadataTokenRecord *typeRecord;
    const SZrMetadataTokenRecord *signatureRecord;
    const SZrZrpMetadataTypeSpecRow *typeSpecRow;
    const SZrTypeLayout *typeLayout;
    SZrMetadataRuntimeTypeSpecGenericBindingView genericBindingView;

    if (outView != ZR_NULL) {
        ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
    }
    if (runtime == ZR_NULL ||
        outView == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(typeSpecToken) != ZR_METADATA_TABLE_TYPE_SPEC) {
        return ZR_FALSE;
    }

    typeRecord = ZrCore_MetadataRuntime_ResolveTypeRecord(runtime, typeSpecToken);
    signatureRecord = ZrCore_MetadataRuntime_ResolveSignatureRecord(runtime, typeSpecToken);
    typeSpecRow = metadata_runtime_find_type_spec_row(runtime, typeSpecToken);
    if (typeRecord == ZR_NULL ||
        signatureRecord == ZR_NULL ||
        typeSpecRow == ZR_NULL ||
        typeSpecRow->typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
        typeSpecRow->signatureBlobOffset != signatureRecord->signatureBlobOffset ||
        typeSpecRow->signatureBlobLength != signatureRecord->signatureBlobLength ||
        typeSpecRow->signatureHash != signatureRecord->signatureHash ||
        typeSpecRow->signatureHash != typeRecord->signatureHash ||
        !ZrCore_MetadataRuntime_ReadTypeSpecGenericBindingView(runtime,
                                                               typeSpecToken,
                                                               &genericBindingView)) {
        return ZR_FALSE;
    }

    typeLayout = ZrCore_MetadataRuntime_ResolveTypeLayout(runtime, typeSpecRow->typeLayoutId);
    if (typeLayout == ZR_NULL) {
        return ZR_FALSE;
    }

    outView->typeSpecToken = typeSpecToken;
    outView->typeRecord = typeRecord;
    outView->typeSpecRow = typeSpecRow;
    outView->genericBindingView = genericBindingView;
    outView->typeLayoutId = typeSpecRow->typeLayoutId;
    outView->cTypeId = typeSpecRow->typeLayoutId;
    outView->signatureHash = typeSpecRow->signatureHash;
    outView->typeLayout = typeLayout;
    return ZR_TRUE;
}

TZrBool ZrCore_MetadataRuntime_ReadFieldDefLayoutBindingView(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldDefToken,
        SZrMetadataRuntimeFieldDefLayoutBindingView *outView) {
    const SZrMetadataTokenRecord *fieldRecord;
    const SZrMetadataTokenRecord *ownerTypeRecord;
    const SZrZrpMetadataFieldDefRow *fieldDefRow;
    const SZrZrpMetadataTypeDefRow *ownerTypeDefRow;
    const SZrTypeLayout *fieldTypeLayout;
    const SZrTypeLayout *ownerTypeLayout;
    TZrUInt32 fieldRowIndex = ~(TZrUInt32)0u;

    if (outView != ZR_NULL) {
        ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
    }
    if (runtime == ZR_NULL ||
        outView == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(fieldDefToken) != ZR_METADATA_TABLE_MEMBER_DEF) {
        return ZR_FALSE;
    }

    fieldRecord = ZrCore_MetadataRuntime_ResolveFieldRecord(runtime, fieldDefToken);
    fieldDefRow = metadata_runtime_find_field_def_row(runtime, fieldDefToken, &fieldRowIndex);
    if (fieldRecord == ZR_NULL ||
        fieldDefRow == ZR_NULL ||
        fieldDefRow->ownerTypeToken == 0u ||
        ZR_METADATA_TOKEN_TABLE(fieldDefRow->ownerTypeToken) != ZR_METADATA_TABLE_TYPE_DEF ||
        fieldDefRow->typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
        return ZR_FALSE;
    }

    ownerTypeRecord = ZrCore_MetadataRuntime_ResolveTypeRecord(runtime, fieldDefRow->ownerTypeToken);
    ownerTypeDefRow = metadata_runtime_find_type_def_row(runtime, fieldDefRow->ownerTypeToken);
    if (ownerTypeRecord == ZR_NULL ||
        ownerTypeDefRow == ZR_NULL ||
        ownerTypeDefRow->typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
        !metadata_runtime_type_def_contains_field_index(ownerTypeDefRow, fieldRowIndex)) {
        return ZR_FALSE;
    }

    fieldTypeLayout = ZrCore_MetadataRuntime_ResolveTypeLayout(runtime, fieldDefRow->typeLayoutId);
    ownerTypeLayout = ZrCore_MetadataRuntime_ResolveTypeLayout(runtime, ownerTypeDefRow->typeLayoutId);
    if (fieldTypeLayout == ZR_NULL || ownerTypeLayout == ZR_NULL) {
        return ZR_FALSE;
    }

    outView->fieldDefToken = fieldDefToken;
    outView->fieldRecord = fieldRecord;
    outView->fieldDefRow = fieldDefRow;
    outView->ownerTypeToken = fieldDefRow->ownerTypeToken;
    outView->ownerTypeRecord = ownerTypeRecord;
    outView->ownerTypeDefRow = ownerTypeDefRow;
    outView->byteOffset = fieldDefRow->byteOffset;
    outView->fieldTypeLayoutId = fieldDefRow->typeLayoutId;
    outView->ownerTypeLayoutId = ownerTypeDefRow->typeLayoutId;
    outView->fieldTypeLayout = fieldTypeLayout;
    outView->ownerTypeLayout = ownerTypeLayout;
    return ZR_TRUE;
}

const SZrTypeLayout *ZrCore_MetadataRuntime_ResolveTypeTokenLayout(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken typeToken,
        TZrUInt32 *outTypeLayoutId) {
    const SZrTypeLayout *typeLayout = ZR_NULL;
    TZrUInt32 typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;

    if (outTypeLayoutId != ZR_NULL) {
        *outTypeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    }
    if (runtime == ZR_NULL || typeToken == 0u) {
        return ZR_NULL;
    }

    typeLayout = metadata_runtime_find_type_layout_cache_by_token(runtime, typeToken, &typeLayoutId);
    if (typeLayout != ZR_NULL) {
        if (outTypeLayoutId != ZR_NULL) {
            *outTypeLayoutId = typeLayoutId;
        }
        return typeLayout;
    }

    switch (ZR_METADATA_TOKEN_TABLE(typeToken)) {
        case ZR_METADATA_TABLE_TYPE_DEF: {
            SZrMetadataRuntimeTypeDefLayoutBindingView view;
            if (!ZrCore_MetadataRuntime_ReadTypeDefLayoutBindingView(runtime, typeToken, &view) ||
                view.typeLayout == ZR_NULL) {
                return ZR_NULL;
            }
            typeLayoutId = view.typeLayoutId;
            typeLayout = view.typeLayout;
            break;
        }

        case ZR_METADATA_TABLE_TYPE_SPEC: {
            SZrMetadataRuntimeTypeSpecLayoutBindingView view;
            if (!ZrCore_MetadataRuntime_ReadTypeSpecLayoutBindingView(runtime, typeToken, &view)) {
                return ZR_NULL;
            }
            typeLayoutId = view.typeLayoutId;
            typeLayout = view.typeLayout;
            break;
        }

        default:
            return ZR_NULL;
    }

    metadata_runtime_store_type_layout_cache(runtime, typeToken, typeLayoutId, typeLayout);
    if (outTypeLayoutId != ZR_NULL) {
        *outTypeLayoutId = typeLayoutId;
    }
    return typeLayout;
}

static TZrMetadataToken metadata_runtime_find_type_def_token_for_layout_id(
        SZrMetadataRuntime *runtime,
        TZrUInt32 typeLayoutId,
        const SZrTypeLayout **outTypeLayout) {
    SZrZrpMetadataSectionView sectionView;
    const SZrZrpMetadataTypeDefRow *rows;
    TZrUInt32 index;

    if (outTypeLayout != ZR_NULL) {
        *outTypeLayout = ZR_NULL;
    }
    if (runtime == ZR_NULL ||
        !ZrCore_MetadataRuntime_GetZrpSectionView(runtime, ZR_ZRP_METADATA_SECTION_TYPE_DEFS, &sectionView) ||
        sectionView.data == ZR_NULL ||
        sectionView.elementSize != (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow)) {
        return 0u;
    }

    rows = (const SZrZrpMetadataTypeDefRow *)sectionView.data;
    for (index = 0u; index < sectionView.count; index++) {
        SZrMetadataRuntimeTypeDefLayoutBindingView bindingView;
        if (rows[index].typeLayoutId != typeLayoutId) {
            continue;
        }
        if (ZrCore_MetadataRuntime_ReadTypeDefLayoutBindingView(runtime, rows[index].token, &bindingView) &&
            bindingView.typeLayoutId == typeLayoutId &&
            bindingView.typeLayout != ZR_NULL) {
            if (outTypeLayout != ZR_NULL) {
                *outTypeLayout = bindingView.typeLayout;
            }
            return bindingView.typeDefToken;
        }
    }
    return 0u;
}

static TZrMetadataToken metadata_runtime_find_type_spec_token_for_layout_id(
        SZrMetadataRuntime *runtime,
        TZrUInt32 typeLayoutId,
        const SZrTypeLayout **outTypeLayout) {
    SZrZrpMetadataSectionView sectionView;
    const SZrZrpMetadataTypeSpecRow *rows;
    TZrUInt32 index;

    if (outTypeLayout != ZR_NULL) {
        *outTypeLayout = ZR_NULL;
    }
    if (runtime == ZR_NULL ||
        !ZrCore_MetadataRuntime_GetZrpSectionView(runtime, ZR_ZRP_METADATA_SECTION_TYPE_SPECS, &sectionView) ||
        sectionView.data == ZR_NULL ||
        sectionView.elementSize != (TZrUInt32)sizeof(SZrZrpMetadataTypeSpecRow)) {
        return 0u;
    }

    rows = (const SZrZrpMetadataTypeSpecRow *)sectionView.data;
    for (index = 0u; index < sectionView.count; index++) {
        SZrMetadataRuntimeTypeSpecLayoutBindingView bindingView;
        if (rows[index].typeLayoutId != typeLayoutId) {
            continue;
        }
        if (ZrCore_MetadataRuntime_ReadTypeSpecLayoutBindingView(runtime, rows[index].token, &bindingView) &&
            bindingView.typeLayoutId == typeLayoutId &&
            bindingView.typeLayout != ZR_NULL) {
            if (outTypeLayout != ZR_NULL) {
                *outTypeLayout = bindingView.typeLayout;
            }
            return bindingView.typeSpecToken;
        }
    }
    return 0u;
}

TZrMetadataToken ZrCore_MetadataRuntime_ResolveTypeLayoutToken(
        SZrMetadataRuntime *runtime,
        TZrUInt32 typeLayoutId) {
    const SZrTypeLayout *typeLayout = ZR_NULL;
    TZrMetadataToken typeToken;

    if (runtime == ZR_NULL || typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
        return 0u;
    }
    typeToken = metadata_runtime_find_type_layout_cache_by_id(runtime, typeLayoutId);
    if (typeToken != 0u) {
        return typeToken;
    }

    typeToken = metadata_runtime_resolve_registration_type_layout_token(runtime, typeLayoutId);
    if (typeToken != 0u) {
        return typeToken;
    }

    typeToken = metadata_runtime_find_type_def_token_for_layout_id(runtime, typeLayoutId, &typeLayout);
    if (typeToken == 0u) {
        typeToken = metadata_runtime_find_type_spec_token_for_layout_id(runtime, typeLayoutId, &typeLayout);
    }
    if (typeToken == 0u || typeLayout == ZR_NULL) {
        return 0u;
    }

    metadata_runtime_store_type_layout_cache(runtime, typeToken, typeLayoutId, typeLayout);
    return typeToken;
}

TZrMetadataToken ZrCore_MetadataRuntime_ResolveCTypeIdToken(
        SZrMetadataRuntime *runtime,
        TZrUInt32 cTypeId) {
    TZrMetadataToken typeToken;

    if (runtime == ZR_NULL || cTypeId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
        return 0u;
    }

    typeToken = metadata_runtime_find_type_layout_cache_by_id(runtime, cTypeId);
    if (typeToken != 0u) {
        return typeToken;
    }

    typeToken = metadata_runtime_resolve_registration_type_layout_token(runtime, cTypeId);
    if (typeToken != 0u) {
        return typeToken;
    }

    return ZrCore_MetadataRuntime_ResolveTypeLayoutToken(runtime, cTypeId);
}
