#include "backend_aot_c_zrp_metadata_string_pool.h"

#include "backend_aot_c_zrp_metadata_remap.h"

#include <stdlib.h>
#include <string.h>

TZrBool backend_aot_c_zrp_string_pool_remap_init(SZrAotCZrpStringPoolRemap *remap,
                                                 TZrUInt32 capacity,
                                                 TZrUInt32 sourceByteLength) {
    if (remap == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(remap, 0, sizeof(*remap));
    remap->capacity = capacity;
    remap->sourceByteLength = sourceByteLength;
    if (capacity == 0u) {
        return ZR_TRUE;
    }

    remap->entries = (SZrAotCZrpStringPoolRemapEntry *)malloc(
            (TZrSize)capacity * sizeof(SZrAotCZrpStringPoolRemapEntry));
    if (remap->entries == ZR_NULL) {
        memset(remap, 0, sizeof(*remap));
        return ZR_FALSE;
    }
    memset(remap->entries, 0, (TZrSize)capacity * sizeof(SZrAotCZrpStringPoolRemapEntry));
    return ZR_TRUE;
}

void backend_aot_c_zrp_string_pool_remap_destroy(SZrAotCZrpStringPoolRemap *remap) {
    if (remap == ZR_NULL) {
        return;
    }

    if (remap->entries != ZR_NULL) {
        free(remap->entries);
    }
    memset(remap, 0, sizeof(*remap));
}

TZrBool backend_aot_c_zrp_string_pool_remap_is_identity(const SZrAotCZrpStringPoolRemap *remap) {
    if (remap == ZR_NULL || remap->byteLength != remap->sourceByteLength) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < remap->count; index++) {
        if (remap->entries[index].oldOffset != remap->entries[index].newOffset) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool backend_aot_c_zrp_string_pool_slice_length(const TZrByte *sourceStringPool,
                                                          TZrUInt32 sourceStringPoolBytes,
                                                          TZrUInt32 oldOffset,
                                                          TZrUInt32 *outByteLength) {
    if (outByteLength == ZR_NULL) {
        return ZR_FALSE;
    }
    *outByteLength = 0u;
    if (sourceStringPool == ZR_NULL || oldOffset >= sourceStringPoolBytes) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = oldOffset; index < sourceStringPoolBytes; index++) {
        if (sourceStringPool[index] == 0u) {
            *outByteLength = index - oldOffset + 1u;
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool backend_aot_c_zrp_string_pool_remap_add(SZrAotCZrpStringPoolRemap *remap,
                                                       const TZrByte *sourceStringPool,
                                                       TZrUInt32 sourceStringPoolBytes,
                                                       TZrUInt32 oldOffset,
                                                       TZrUInt32 byteLength) {
    if (remap == ZR_NULL) {
        return ZR_FALSE;
    }
    if (byteLength == 0u) {
        return ZR_TRUE;
    }

    for (TZrUInt32 index = 0u; index < remap->count; index++) {
        if (remap->entries[index].oldOffset == oldOffset) {
            return remap->entries[index].byteLength == byteLength;
        }
    }

    if (remap->entries == ZR_NULL || remap->count >= remap->capacity) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < remap->count; index++) {
        const SZrAotCZrpStringPoolRemapEntry *entry = &remap->entries[index];
        if (entry->byteLength == byteLength &&
            oldOffset <= sourceStringPoolBytes &&
            byteLength <= (TZrUInt32)(sourceStringPoolBytes - oldOffset) &&
            entry->oldOffset <= sourceStringPoolBytes &&
            entry->byteLength <= (TZrUInt32)(sourceStringPoolBytes - entry->oldOffset) &&
            memcmp(sourceStringPool + oldOffset, sourceStringPool + entry->oldOffset, byteLength) == 0) {
            remap->entries[remap->count].oldOffset = oldOffset;
            remap->entries[remap->count].byteLength = byteLength;
            remap->entries[remap->count].newOffset = entry->newOffset;
            remap->count++;
            return ZR_TRUE;
        }
    }

    if (byteLength > (TZrUInt32)(0xFFFFFFFFu - remap->byteLength)) {
        return ZR_FALSE;
    }
    remap->entries[remap->count].oldOffset = oldOffset;
    remap->entries[remap->count].byteLength = byteLength;
    remap->entries[remap->count].newOffset = remap->byteLength;
    remap->count++;
    remap->byteLength += byteLength;
    return ZR_TRUE;
}

static TZrBool backend_aot_c_zrp_add_string_offset(SZrAotCZrpStringPoolRemap *remap,
                                                   const TZrByte *sourceStringPool,
                                                   TZrUInt32 sourceStringPoolBytes,
                                                   TZrUInt32 oldOffset) {
    TZrUInt32 byteLength;

    if (sourceStringPoolBytes == 0u) {
        return ZR_TRUE;
    }
    if (!backend_aot_c_zrp_string_pool_slice_length(sourceStringPool,
                                                    sourceStringPoolBytes,
                                                    oldOffset,
                                                    &byteLength)) {
        return ZR_FALSE;
    }

    return backend_aot_c_zrp_string_pool_remap_add(remap,
                                                  sourceStringPool,
                                                  sourceStringPoolBytes,
                                                  oldOffset,
                                                  byteLength);
}

static TZrBool backend_aot_c_zrp_string_pool_inputs_are_valid(
        const TZrByte *sourceStringPool,
        TZrUInt32 sourceStringPoolBytes,
        const SZrZrpMetadataTypeDefRow *typeRows,
        TZrUInt32 typeCount,
        const SZrZrpMetadataMethodDefRow *methodRows,
        TZrUInt32 methodCount,
        const SZrZrpMetadataFieldDefRow *fieldRows,
        TZrUInt32 fieldCount,
        const SZrZrpMetadataGenericParamRow *genericParamRows,
        TZrUInt32 genericParamCount,
        const SZrZrpMetadataModuleRefRow *moduleRefRows,
        TZrUInt32 moduleRefCount) {
    if (sourceStringPoolBytes != 0u && sourceStringPool == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrBool)((typeCount == 0u || typeRows != ZR_NULL) &&
                     (methodCount == 0u || methodRows != ZR_NULL) &&
                     (fieldCount == 0u || fieldRows != ZR_NULL) &&
                     (genericParamCount == 0u || genericParamRows != ZR_NULL) &&
                     (moduleRefCount == 0u || moduleRefRows != ZR_NULL));
}

TZrBool backend_aot_c_zrp_build_string_pool_remap(
        SZrAotCZrpStringPoolRemap *stringRemap,
        const TZrByte *sourceStringPool,
        TZrUInt32 sourceStringPoolBytes,
        const SZrZrpMetadataTypeDefRow *typeRows,
        TZrUInt32 typeCount,
        const SZrZrpMetadataMethodDefRow *methodRows,
        TZrUInt32 methodCount,
        const SZrZrpMetadataFieldDefRow *fieldRows,
        TZrUInt32 fieldCount,
        const SZrZrpMetadataGenericParamRow *genericParamRows,
        TZrUInt32 genericParamCount,
        const SZrZrpMetadataModuleRefRow *moduleRefRows,
        TZrUInt32 moduleRefCount,
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 retainedMethodDefCount) {
    if (stringRemap == ZR_NULL ||
        !backend_aot_c_zrp_string_pool_inputs_are_valid(sourceStringPool,
                                                        sourceStringPoolBytes,
                                                        typeRows,
                                                        typeCount,
                                                        methodRows,
                                                        methodCount,
                                                        fieldRows,
                                                        fieldCount,
                                                        genericParamRows,
                                                        genericParamCount,
                                                        moduleRefRows,
                                                        moduleRefCount)) {
        return ZR_FALSE;
    }
    if (sourceStringPoolBytes == 0u) {
        return ZR_TRUE;
    }

    for (TZrUInt32 index = 0u; index < typeCount; index++) {
        if (!backend_aot_c_zrp_add_string_offset(stringRemap,
                                                 sourceStringPool,
                                                 sourceStringPoolBytes,
                                                 typeRows[index].nameStringOffset) ||
            !backend_aot_c_zrp_add_string_offset(stringRemap,
                                                 sourceStringPool,
                                                 sourceStringPoolBytes,
                                                 typeRows[index].namespaceStringOffset)) {
            return ZR_FALSE;
        }
    }

    for (TZrUInt32 index = 0u; index < methodCount; index++) {
        if (backend_aot_c_zrp_method_def_row_is_retained(&methodRows[index], functionTable) &&
            !backend_aot_c_zrp_add_string_offset(stringRemap,
                                                 sourceStringPool,
                                                 sourceStringPoolBytes,
                                                 methodRows[index].nameStringOffset)) {
            return ZR_FALSE;
        }
    }

    for (TZrUInt32 index = 0u; index < fieldCount; index++) {
        if (!backend_aot_c_zrp_add_string_offset(stringRemap,
                                                 sourceStringPool,
                                                 sourceStringPoolBytes,
                                                 fieldRows[index].nameStringOffset)) {
            return ZR_FALSE;
        }
    }

    for (TZrUInt32 index = 0u; index < genericParamCount; index++) {
        if (backend_aot_c_zrp_generic_param_row_is_retained(&genericParamRows[index],
                                                            methodRows,
                                                            methodCount,
                                                            fieldRows,
                                                            fieldCount,
                                                            functionTable,
                                                            retainedMethodDefCount) &&
            !backend_aot_c_zrp_add_string_offset(stringRemap,
                                                 sourceStringPool,
                                                 sourceStringPoolBytes,
                                                 genericParamRows[index].nameStringOffset)) {
            return ZR_FALSE;
        }
    }

    for (TZrUInt32 index = 0u; index < moduleRefCount; index++) {
        if (!backend_aot_c_zrp_add_string_offset(stringRemap,
                                                 sourceStringPool,
                                                 sourceStringPoolBytes,
                                                 moduleRefRows[index].nameStringOffset) ||
            !backend_aot_c_zrp_add_string_offset(stringRemap,
                                                 sourceStringPool,
                                                 sourceStringPoolBytes,
                                                 moduleRefRows[index].versionStringOffset)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

void backend_aot_c_zrp_copy_string_pool(TZrByte *targetBlob,
                                        const TZrByte *sourceBlob,
                                        const SZrZrpMetadataHeader *sourceHeader,
                                        const SZrZrpMetadataHeader *targetHeader,
                                        const SZrAotCZrpStringPoolRemap *stringRemap) {
    TZrByte *targetPool;
    const TZrByte *sourcePool;

    if (targetBlob == ZR_NULL ||
        sourceBlob == ZR_NULL ||
        sourceHeader == ZR_NULL ||
        targetHeader == ZR_NULL ||
        stringRemap == ZR_NULL ||
        targetHeader->stringPool.byteLength == 0u) {
        return;
    }

    targetPool = targetBlob + targetHeader->stringPool.offset;
    sourcePool = sourceBlob + sourceHeader->stringPool.offset;
    for (TZrUInt32 index = 0u; index < stringRemap->count; index++) {
        memcpy(targetPool + stringRemap->entries[index].newOffset,
               sourcePool + stringRemap->entries[index].oldOffset,
               stringRemap->entries[index].byteLength);
    }
}

static TZrBool backend_aot_c_zrp_remap_string_offset(TZrUInt32 *stringOffset,
                                                     const SZrAotCZrpStringPoolRemap *remap) {
    if (stringOffset == ZR_NULL) {
        return ZR_FALSE;
    }
    if (remap == ZR_NULL || remap->sourceByteLength == 0u) {
        return ZR_TRUE;
    }

    for (TZrUInt32 index = 0u; index < remap->count; index++) {
        if (remap->entries[index].oldOffset == *stringOffset) {
            *stringOffset = remap->entries[index].newOffset;
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

TZrBool backend_aot_c_zrp_remap_type_def_string_offsets(SZrZrpMetadataTypeDefRow *row,
                                                        const SZrAotCZrpStringPoolRemap *stringRemap) {
    return (TZrBool)(row != ZR_NULL &&
                     backend_aot_c_zrp_remap_string_offset(&row->nameStringOffset, stringRemap) &&
                     backend_aot_c_zrp_remap_string_offset(&row->namespaceStringOffset, stringRemap));
}

TZrBool backend_aot_c_zrp_remap_method_def_string_offsets(SZrZrpMetadataMethodDefRow *row,
                                                          const SZrAotCZrpStringPoolRemap *stringRemap) {
    return (TZrBool)(row != ZR_NULL &&
                     backend_aot_c_zrp_remap_string_offset(&row->nameStringOffset, stringRemap));
}

TZrBool backend_aot_c_zrp_remap_field_def_string_offsets(SZrZrpMetadataFieldDefRow *row,
                                                         const SZrAotCZrpStringPoolRemap *stringRemap) {
    return (TZrBool)(row != ZR_NULL &&
                     backend_aot_c_zrp_remap_string_offset(&row->nameStringOffset, stringRemap));
}

TZrBool backend_aot_c_zrp_remap_generic_param_string_offsets(SZrZrpMetadataGenericParamRow *row,
                                                            const SZrAotCZrpStringPoolRemap *stringRemap) {
    return (TZrBool)(row != ZR_NULL &&
                     backend_aot_c_zrp_remap_string_offset(&row->nameStringOffset, stringRemap));
}

TZrBool backend_aot_c_zrp_remap_module_ref_string_offsets(SZrZrpMetadataModuleRefRow *row,
                                                          const SZrAotCZrpStringPoolRemap *stringRemap) {
    return (TZrBool)(row != ZR_NULL &&
                     backend_aot_c_zrp_remap_string_offset(&row->nameStringOffset, stringRemap) &&
                     backend_aot_c_zrp_remap_string_offset(&row->versionStringOffset, stringRemap));
}
