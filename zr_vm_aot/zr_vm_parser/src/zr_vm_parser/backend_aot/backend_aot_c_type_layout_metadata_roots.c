#include "backend_aot_c_type_layout_metadata_roots.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/zrp_metadata.h"

#include <string.h>

static void backend_aot_c_type_layout_metadata_roots_reset(SZrAotCTypeLayoutMetadataRoots *outRoots) {
    if (outRoots == ZR_NULL) {
        return;
    }
    memset(outRoots, 0, sizeof(*outRoots));
}

static TZrBool backend_aot_c_type_layout_metadata_roots_append(
        SZrAotCTypeLayoutMetadataRoots *outRoots,
        TZrUInt32 typeLayoutId) {
    if (outRoots == ZR_NULL || typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
        return ZR_FALSE;
    }
    for (TZrUInt32 rootIndex = 0u; rootIndex < outRoots->count; rootIndex++) {
        if (outRoots->typeLayoutIds[rootIndex] == typeLayoutId) {
            return ZR_TRUE;
        }
    }
    if (outRoots->count >= ZR_AOT_C_TYPE_LAYOUT_METADATA_ROOT_CAPACITY) {
        return ZR_FALSE;
    }
    outRoots->typeLayoutIds[outRoots->count] = typeLayoutId;
    outRoots->count++;
    return ZR_TRUE;
}

static TZrBool backend_aot_c_type_layout_zrp_section_view(const TZrByte *metadataBlob,
                                                          TZrSize metadataBlobLength,
                                                          EZrZrpMetadataSectionKind sectionKind,
                                                          TZrUInt32 expectedElementSize,
                                                          SZrZrpMetadataSectionView *outView) {
    SZrZrpMetadataHeader header;

    if (outView != ZR_NULL) {
        memset(outView, 0, sizeof(*outView));
    }
    if (metadataBlob == ZR_NULL ||
        metadataBlobLength == 0u ||
        outView == ZR_NULL ||
        !ZrCore_ZrpMetadata_ReadHeader(metadataBlob, metadataBlobLength, &header) ||
        !ZrCore_ZrpMetadata_GetSectionView(metadataBlob, metadataBlobLength, &header, sectionKind, outView) ||
        outView->elementSize != expectedElementSize) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool backend_aot_c_type_layout_metadata_type_def_token_root(
        const TZrByte *metadataBlob,
        TZrSize metadataBlobLength,
        TZrMetadataToken typeToken,
        SZrAotCTypeLayoutMetadataRoots *outRoots) {
    SZrZrpMetadataSectionView view;

    if (!backend_aot_c_type_layout_zrp_section_view(metadataBlob,
                                                    metadataBlobLength,
                                                    ZR_ZRP_METADATA_SECTION_TYPE_DEFS,
                                                    (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow),
                                                    &view)) {
        return ZR_FALSE;
    }

    for (TZrUInt32 rowIndex = 0u; rowIndex < view.count; rowIndex++) {
        const SZrZrpMetadataTypeDefRow *row =
                &((const SZrZrpMetadataTypeDefRow *)(const void *)view.data)[rowIndex];

        if (row->token != typeToken) {
            continue;
        }
        if (outRoots->count > 0u ||
            !backend_aot_c_type_layout_metadata_roots_append(outRoots, row->typeLayoutId)) {
            return ZR_FALSE;
        }
    }

    return (TZrBool)(outRoots->count > 0u);
}

static TZrBool backend_aot_c_type_layout_metadata_type_spec_token_root(
        const TZrByte *metadataBlob,
        TZrSize metadataBlobLength,
        TZrMetadataToken typeToken,
        SZrAotCTypeLayoutMetadataRoots *outRoots) {
    SZrZrpMetadataSectionView view;

    if (!backend_aot_c_type_layout_zrp_section_view(metadataBlob,
                                                    metadataBlobLength,
                                                    ZR_ZRP_METADATA_SECTION_TYPE_SPECS,
                                                    (TZrUInt32)sizeof(SZrZrpMetadataTypeSpecRow),
                                                    &view)) {
        return ZR_FALSE;
    }

    for (TZrUInt32 rowIndex = 0u; rowIndex < view.count; rowIndex++) {
        const SZrZrpMetadataTypeSpecRow *row =
                &((const SZrZrpMetadataTypeSpecRow *)(const void *)view.data)[rowIndex];

        if (row->token != typeToken) {
            continue;
        }
        if (outRoots->count > 0u ||
            !backend_aot_c_type_layout_metadata_roots_append(outRoots, row->typeLayoutId)) {
            return ZR_FALSE;
        }
    }

    return (TZrBool)(outRoots->count > 0u);
}

static TZrBool backend_aot_c_type_layout_metadata_type_ref_token_root(
        const TZrByte *metadataBlob,
        TZrSize metadataBlobLength,
        TZrMetadataToken typeToken,
        SZrAotCTypeLayoutMetadataRoots *outRoots) {
    SZrZrpMetadataSectionView view;
    TZrMetadataToken targetTypeToken = 0u;

    if (!backend_aot_c_type_layout_zrp_section_view(metadataBlob,
                                                    metadataBlobLength,
                                                    ZR_ZRP_METADATA_SECTION_TOKEN_RECORDS,
                                                    (TZrUInt32)sizeof(SZrMetadataTokenRecord),
                                                    &view)) {
        return ZR_FALSE;
    }

    for (TZrUInt32 rowIndex = 0u; rowIndex < view.count; rowIndex++) {
        const SZrMetadataTokenRecord *record =
                &((const SZrMetadataTokenRecord *)(const void *)view.data)[rowIndex];

        if (record->token != typeToken) {
            continue;
        }
        if (targetTypeToken != 0u ||
            record->targetMetadataToken == 0u ||
            ZR_METADATA_TOKEN_TABLE(record->targetMetadataToken) != ZR_METADATA_TABLE_TYPE_DEF) {
            return ZR_FALSE;
        }
        targetTypeToken = record->targetMetadataToken;
    }

    if (targetTypeToken == 0u) {
        return ZR_FALSE;
    }
    return backend_aot_c_type_layout_metadata_type_def_token_root(metadataBlob,
                                                                  metadataBlobLength,
                                                                  targetTypeToken,
                                                                  outRoots);
}

TZrBool backend_aot_c_type_layout_metadata_type_token_roots(
        const TZrByte *metadataBlob,
        TZrSize metadataBlobLength,
        TZrMetadataToken typeToken,
        SZrAotCTypeLayoutMetadataRoots *outRoots) {
    backend_aot_c_type_layout_metadata_roots_reset(outRoots);
    if (typeToken == 0u || outRoots == ZR_NULL) {
        return ZR_FALSE;
    }
    if (ZR_METADATA_TOKEN_TABLE(typeToken) == ZR_METADATA_TABLE_TYPE_DEF) {
        return backend_aot_c_type_layout_metadata_type_def_token_root(metadataBlob,
                                                                      metadataBlobLength,
                                                                      typeToken,
                                                                      outRoots);
    }
    if (ZR_METADATA_TOKEN_TABLE(typeToken) == ZR_METADATA_TABLE_TYPE_REF) {
        return backend_aot_c_type_layout_metadata_type_ref_token_root(metadataBlob,
                                                                      metadataBlobLength,
                                                                      typeToken,
                                                                      outRoots);
    }
    if (ZR_METADATA_TOKEN_TABLE(typeToken) == ZR_METADATA_TABLE_TYPE_SPEC) {
        return backend_aot_c_type_layout_metadata_type_spec_token_root(metadataBlob,
                                                                       metadataBlobLength,
                                                                       typeToken,
                                                                       outRoots);
    }
    return ZR_FALSE;
}

static const SZrZrpMetadataTypeDefRow *backend_aot_c_type_layout_metadata_find_owner_type_def(
        const TZrByte *metadataBlob,
        TZrSize metadataBlobLength,
        const SZrZrpMetadataFieldDefRow *fieldRow,
        TZrUInt32 fieldRowIndex) {
    SZrZrpMetadataSectionView view;
    const SZrZrpMetadataTypeDefRow *matchedTypeDef = ZR_NULL;

    if (fieldRow == ZR_NULL ||
        !backend_aot_c_type_layout_zrp_section_view(metadataBlob,
                                                    metadataBlobLength,
                                                    ZR_ZRP_METADATA_SECTION_TYPE_DEFS,
                                                    (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow),
                                                    &view)) {
        return ZR_NULL;
    }

    for (TZrUInt32 rowIndex = 0u; rowIndex < view.count; rowIndex++) {
        const SZrZrpMetadataTypeDefRow *row =
                &((const SZrZrpMetadataTypeDefRow *)(const void *)view.data)[rowIndex];

        if (row->token != fieldRow->ownerTypeToken ||
            fieldRowIndex < row->firstFieldDefIndex ||
            fieldRowIndex - row->firstFieldDefIndex >= row->fieldDefCount) {
            continue;
        }
        if (matchedTypeDef != ZR_NULL) {
            return ZR_NULL;
        }
        matchedTypeDef = row;
    }

    return matchedTypeDef;
}

TZrBool backend_aot_c_type_layout_metadata_field_token_roots(
        const TZrByte *metadataBlob,
        TZrSize metadataBlobLength,
        TZrMetadataToken fieldToken,
        SZrAotCTypeLayoutMetadataRoots *outRoots) {
    SZrZrpMetadataSectionView view;

    backend_aot_c_type_layout_metadata_roots_reset(outRoots);
    if (fieldToken == 0u ||
        outRoots == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(fieldToken) != ZR_METADATA_TABLE_MEMBER_DEF ||
        !backend_aot_c_type_layout_zrp_section_view(metadataBlob,
                                                    metadataBlobLength,
                                                    ZR_ZRP_METADATA_SECTION_FIELD_DEFS,
                                                    (TZrUInt32)sizeof(SZrZrpMetadataFieldDefRow),
                                                    &view)) {
        return ZR_FALSE;
    }

    for (TZrUInt32 rowIndex = 0u; rowIndex < view.count; rowIndex++) {
        const SZrZrpMetadataFieldDefRow *fieldRow =
                &((const SZrZrpMetadataFieldDefRow *)(const void *)view.data)[rowIndex];
        const SZrZrpMetadataTypeDefRow *ownerRow;

        if (fieldRow->token != fieldToken) {
            continue;
        }
        if (outRoots->count > 0u) {
            return ZR_FALSE;
        }
        ownerRow = backend_aot_c_type_layout_metadata_find_owner_type_def(metadataBlob,
                                                                          metadataBlobLength,
                                                                          fieldRow,
                                                                          rowIndex);
        if (ownerRow == ZR_NULL ||
            !backend_aot_c_type_layout_metadata_roots_append(outRoots, ownerRow->typeLayoutId) ||
            !backend_aot_c_type_layout_metadata_roots_append(outRoots, fieldRow->typeLayoutId)) {
            return ZR_FALSE;
        }
    }

    return (TZrBool)(outRoots->count > 0u);
}
