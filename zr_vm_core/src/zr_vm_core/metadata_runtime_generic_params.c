#include "zr_vm_core/metadata_runtime.h"

#include "zr_vm_core/memory.h"

typedef struct SZrMetadataRuntimeGenericOwnerRange {
    const SZrMetadataTokenRecord *ownerRecord;
    TZrUInt32 firstGenericParamIndex;
    TZrUInt32 genericParamCount;
} SZrMetadataRuntimeGenericOwnerRange;

static const SZrZrpMetadataTypeDefRow *metadata_runtime_generic_find_type_def_row(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken typeDefToken) {
    SZrZrpMetadataSectionView view;
    const SZrZrpMetadataTypeDefRow *rows;

    if (runtime == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(typeDefToken) != ZR_METADATA_TABLE_TYPE_DEF ||
        !ZrCore_MetadataRuntime_GetZrpSectionView(runtime, ZR_ZRP_METADATA_SECTION_TYPE_DEFS, &view) ||
        view.data == ZR_NULL ||
        view.elementSize != (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow)) {
        return ZR_NULL;
    }

    rows = (const SZrZrpMetadataTypeDefRow *)(const void *)view.data;
    for (TZrUInt32 index = 0u; index < view.count; index++) {
        if (rows[index].token == typeDefToken) {
            return &rows[index];
        }
    }
    return ZR_NULL;
}

static const SZrZrpMetadataMethodDefRow *metadata_runtime_generic_find_method_def_row(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken methodDefToken) {
    SZrZrpMetadataSectionView view;
    const SZrZrpMetadataMethodDefRow *rows;

    if (runtime == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(methodDefToken) != ZR_METADATA_TABLE_MEMBER_DEF ||
        !ZrCore_MetadataRuntime_GetZrpSectionView(runtime, ZR_ZRP_METADATA_SECTION_METHOD_DEFS, &view) ||
        view.data == ZR_NULL ||
        view.elementSize != (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow)) {
        return ZR_NULL;
    }

    rows = (const SZrZrpMetadataMethodDefRow *)(const void *)view.data;
    for (TZrUInt32 index = 0u; index < view.count; index++) {
        if (rows[index].token == methodDefToken) {
            return &rows[index];
        }
    }
    return ZR_NULL;
}

static TZrBool metadata_runtime_generic_owner_range(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken ownerToken,
        SZrMetadataRuntimeGenericOwnerRange *outRange) {
    const SZrMetadataTokenRecord *ownerRecord;

    if (outRange != ZR_NULL) {
        ZrCore_Memory_RawSet(outRange, 0, sizeof(*outRange));
    }
    if (runtime == ZR_NULL || ownerToken == 0u || outRange == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (ZR_METADATA_TOKEN_TABLE(ownerToken)) {
        case ZR_METADATA_TABLE_TYPE_DEF: {
            const SZrZrpMetadataTypeDefRow *row =
                    metadata_runtime_generic_find_type_def_row(runtime, ownerToken);
            ownerRecord = ZrCore_MetadataRuntime_ResolveTypeRecord(runtime, ownerToken);
            if (row == ZR_NULL || ownerRecord == ZR_NULL) {
                return ZR_FALSE;
            }
            outRange->ownerRecord = ownerRecord;
            outRange->firstGenericParamIndex = row->firstGenericParamIndex;
            outRange->genericParamCount = row->genericParamCount;
            return ZR_TRUE;
        }

        case ZR_METADATA_TABLE_MEMBER_DEF: {
            const SZrZrpMetadataMethodDefRow *row =
                    metadata_runtime_generic_find_method_def_row(runtime, ownerToken);
            ownerRecord = ZrCore_MetadataRuntime_ResolveMethodRecord(runtime, ownerToken);
            if (row == ZR_NULL || ownerRecord == ZR_NULL) {
                return ZR_FALSE;
            }
            outRange->ownerRecord = ownerRecord;
            outRange->firstGenericParamIndex = row->firstGenericParamIndex;
            outRange->genericParamCount = row->genericParamCount;
            return ZR_TRUE;
        }

        default:
            return ZR_FALSE;
    }
}

static TZrBool metadata_runtime_generic_param_index_in_owner_range(
        const SZrMetadataRuntimeGenericOwnerRange *range,
        TZrUInt32 genericParamIndex) {
    if (range == ZR_NULL) {
        return ZR_FALSE;
    }
    return (TZrBool)(genericParamIndex >= range->firstGenericParamIndex &&
                     genericParamIndex - range->firstGenericParamIndex < range->genericParamCount);
}

static const SZrZrpMetadataGenericParamRow *metadata_runtime_find_generic_param_row(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken ownerToken,
        TZrUInt32 parameterIndex,
        const SZrMetadataRuntimeGenericOwnerRange *ownerRange,
        TZrUInt32 *outGenericParamIndex) {
    SZrZrpMetadataSectionView view;
    const SZrZrpMetadataGenericParamRow *rows;

    if (outGenericParamIndex != ZR_NULL) {
        *outGenericParamIndex = ~(TZrUInt32)0u;
    }
    if (runtime == ZR_NULL ||
        !ZrCore_MetadataRuntime_GetZrpSectionView(runtime, ZR_ZRP_METADATA_SECTION_GENERIC_PARAMS, &view) ||
        view.data == ZR_NULL ||
        view.elementSize != (TZrUInt32)sizeof(SZrZrpMetadataGenericParamRow)) {
        return ZR_NULL;
    }

    rows = (const SZrZrpMetadataGenericParamRow *)(const void *)view.data;
    for (TZrUInt32 index = 0u; index < view.count; index++) {
        if (rows[index].ownerToken == ownerToken &&
            rows[index].parameterIndex == parameterIndex &&
            metadata_runtime_generic_param_index_in_owner_range(ownerRange, index)) {
            if (outGenericParamIndex != ZR_NULL) {
                *outGenericParamIndex = index;
            }
            return &rows[index];
        }
    }
    return ZR_NULL;
}

TZrBool ZrCore_MetadataRuntime_ReadGenericParamView(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken ownerToken,
        TZrUInt32 parameterIndex,
        SZrMetadataRuntimeGenericParamView *outView) {
    SZrMetadataRuntimeGenericOwnerRange ownerRange;
    const SZrZrpMetadataGenericParamRow *row;
    TZrUInt32 genericParamIndex = ~(TZrUInt32)0u;

    if (outView != ZR_NULL) {
        ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
    }
    if (outView == ZR_NULL ||
        !metadata_runtime_generic_owner_range(runtime, ownerToken, &ownerRange)) {
        return ZR_FALSE;
    }

    row = metadata_runtime_find_generic_param_row(runtime,
                                                  ownerToken,
                                                  parameterIndex,
                                                  &ownerRange,
                                                  &genericParamIndex);
    if (row == ZR_NULL) {
        return ZR_FALSE;
    }

    outView->ownerToken = ownerToken;
    outView->ownerRecord = ownerRange.ownerRecord;
    outView->genericParamRow = row;
    outView->genericParamIndex = genericParamIndex;
    outView->parameterIndex = row->parameterIndex;
    outView->nameStringOffset = row->nameStringOffset;
    outView->firstConstraintIndex = row->firstConstraintIndex;
    outView->constraintCount = row->constraintCount;
    outView->flags = row->flags;
    return ZR_TRUE;
}

static TZrBool metadata_runtime_generic_get_constraint_blob(
        SZrMetadataRuntime *runtime,
        const SZrZrpMetadataGenericParamConstraintRow *row,
        SZrZrpMetadataPoolSliceView *outBlob) {
    if (outBlob != ZR_NULL) {
        ZrCore_Memory_RawSet(outBlob, 0, sizeof(*outBlob));
    }
    if (runtime == ZR_NULL || row == ZR_NULL || outBlob == ZR_NULL) {
        return ZR_FALSE;
    }
    if (row->signatureBlobLength == 0u) {
        return ZR_TRUE;
    }

    if (!ZrCore_ZrpMetadata_GetPoolSlice(runtime->zrpMetadataBuffer,
                                         runtime->zrpMetadataBufferLength,
                                         &runtime->zrpMetadataHeader,
                                         ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL,
                                         row->signatureBlobOffset,
                                         row->signatureBlobLength,
                                         outBlob) ||
        !ZrCore_ZrpMetadata_ValidateSignatureBlob(outBlob->data, outBlob->byteLength)) {
        ZrCore_Memory_RawSet(outBlob, 0, sizeof(*outBlob));
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrCore_MetadataRuntime_ReadGenericParamConstraintView(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken ownerToken,
        TZrUInt32 parameterIndex,
        TZrUInt32 constraintIndex,
        SZrMetadataRuntimeGenericParamConstraintView *outView) {
    SZrMetadataRuntimeGenericParamView genericParamView;
    SZrZrpMetadataSectionView view;
    const SZrZrpMetadataGenericParamConstraintRow *rows;
    const SZrZrpMetadataGenericParamConstraintRow *row;
    const SZrMetadataTokenRecord *constraintTypeRecord;
    TZrUInt32 absoluteConstraintIndex;
    SZrZrpMetadataPoolSliceView signatureBlob;

    if (outView != ZR_NULL) {
        ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
    }
    if (outView == ZR_NULL ||
        !ZrCore_MetadataRuntime_ReadGenericParamView(runtime,
                                                     ownerToken,
                                                     parameterIndex,
                                                     &genericParamView) ||
        constraintIndex >= genericParamView.constraintCount ||
        !ZrCore_MetadataRuntime_GetZrpSectionView(runtime,
                                                  ZR_ZRP_METADATA_SECTION_GENERIC_PARAM_CONSTRAINTS,
                                                  &view) ||
        view.data == ZR_NULL ||
        view.elementSize != (TZrUInt32)sizeof(SZrZrpMetadataGenericParamConstraintRow)) {
        return ZR_FALSE;
    }

    absoluteConstraintIndex = genericParamView.firstConstraintIndex + constraintIndex;
    if (absoluteConstraintIndex < genericParamView.firstConstraintIndex ||
        absoluteConstraintIndex >= view.count) {
        return ZR_FALSE;
    }

    rows = (const SZrZrpMetadataGenericParamConstraintRow *)(const void *)view.data;
    row = &rows[absoluteConstraintIndex];
    if (row->genericParamIndex != genericParamView.genericParamIndex) {
        return ZR_FALSE;
    }

    constraintTypeRecord = ZrCore_MetadataRuntime_ResolveTypeRecord(runtime, row->constraintTypeToken);
    if (constraintTypeRecord == ZR_NULL ||
        !metadata_runtime_generic_get_constraint_blob(runtime, row, &signatureBlob)) {
        return ZR_FALSE;
    }

    outView->genericParamView = genericParamView;
    outView->constraintRow = row;
    outView->constraintIndex = constraintIndex;
    outView->constraintTypeToken = row->constraintTypeToken;
    outView->constraintTypeRecord = constraintTypeRecord;
    outView->signatureBlob = signatureBlob;
    return ZR_TRUE;
}
