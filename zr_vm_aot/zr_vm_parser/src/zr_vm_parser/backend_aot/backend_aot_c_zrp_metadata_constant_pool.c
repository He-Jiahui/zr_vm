#include "backend_aot_c_zrp_metadata_constant_pool.h"

#include "backend_aot_c_zrp_metadata_type_def.h"

#include <stdlib.h>
#include <string.h>

TZrBool backend_aot_c_zrp_constant_pool_remap_init(SZrAotCZrpConstantPoolRemap *remap,
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

    remap->entries = (SZrAotCZrpConstantPoolRemapEntry *)malloc(
            (TZrSize)capacity * sizeof(SZrAotCZrpConstantPoolRemapEntry));
    if (remap->entries == ZR_NULL) {
        memset(remap, 0, sizeof(*remap));
        return ZR_FALSE;
    }
    memset(remap->entries, 0, (TZrSize)capacity * sizeof(SZrAotCZrpConstantPoolRemapEntry));
    return ZR_TRUE;
}

void backend_aot_c_zrp_constant_pool_remap_destroy(SZrAotCZrpConstantPoolRemap *remap) {
    if (remap == ZR_NULL) {
        return;
    }

    if (remap->entries != ZR_NULL) {
        free(remap->entries);
    }
    memset(remap, 0, sizeof(*remap));
}

TZrBool backend_aot_c_zrp_constant_pool_remap_is_identity(const SZrAotCZrpConstantPoolRemap *remap) {
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

static TZrBool backend_aot_c_zrp_constant_pool_range_is_valid(const SZrAotCZrpConstantPoolRemap *remap,
                                                              TZrUInt32 oldOffset,
                                                              TZrUInt32 byteLength) {
    if (byteLength == 0u) {
        return oldOffset == 0u;
    }
    if (remap == ZR_NULL || oldOffset >= remap->sourceByteLength) {
        return ZR_FALSE;
    }
    return byteLength <= remap->sourceByteLength - oldOffset;
}

static TZrBool backend_aot_c_zrp_constant_pool_remap_add(SZrAotCZrpConstantPoolRemap *remap,
                                                         TZrUInt32 oldOffset,
                                                         TZrUInt32 byteLength) {
    if (remap == ZR_NULL || !backend_aot_c_zrp_constant_pool_range_is_valid(remap, oldOffset, byteLength)) {
        return ZR_FALSE;
    }
    if (byteLength == 0u) {
        return ZR_TRUE;
    }

    for (TZrUInt32 index = 0u; index < remap->count; index++) {
        if (remap->entries[index].oldOffset == oldOffset && remap->entries[index].byteLength == byteLength) {
            return ZR_TRUE;
        }
    }
    if (remap->entries == ZR_NULL ||
        remap->count >= remap->capacity ||
        byteLength > (TZrUInt32)(0xFFFFFFFFu - remap->byteLength)) {
        return ZR_FALSE;
    }

    remap->entries[remap->count].oldOffset = oldOffset;
    remap->entries[remap->count].byteLength = byteLength;
    remap->entries[remap->count].newOffset = remap->byteLength;
    remap->count++;
    remap->byteLength += byteLength;
    return ZR_TRUE;
}

TZrBool backend_aot_c_zrp_build_constant_pool_remap(SZrAotCZrpConstantPoolRemap *constantPoolRemap,
                                                    const SZrZrpMetadataFieldDefRow *fieldRows,
                                                    TZrUInt32 fieldCount,
                                                    const SZrZrpMetadataTypeDefRow *typeRows,
                                                    TZrUInt32 typeCount,
                                                    const SZrMetadataTokenRecord *tokenRecords,
                                                    TZrUInt32 tokenRecordCount,
                                                    const SZrZrpMetadataMethodDefRow *methodRows,
                                                    TZrUInt32 methodCount,
                                                    const SZrZrpMetadataGenericParamRow *genericParamRows,
                                                    TZrUInt32 genericParamCount,
                                                    const SZrZrpMetadataGenericParamConstraintRow *genericParamConstraintRows,
                                                    TZrUInt32 genericParamConstraintCount,
                                                    const SZrAotFunctionTable *functionTable,
                                                    TZrUInt32 retainedMethodDefCount) {
    if (constantPoolRemap == ZR_NULL || (fieldCount != 0u && fieldRows == ZR_NULL)) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < fieldCount; index++) {
        if (!backend_aot_c_zrp_field_def_row_is_retained(&fieldRows[index],
                                                         typeRows,
                                                         typeCount,
                                                         tokenRecords,
                                                         tokenRecordCount,
                                                         methodRows,
                                                         methodCount,
                                                         fieldRows,
                                                         fieldCount,
                                                         genericParamRows,
                                                         genericParamCount,
                                                         genericParamConstraintRows,
                                                         genericParamConstraintCount,
                                                         functionTable,
                                                         retainedMethodDefCount)) {
            continue;
        }
        if (!backend_aot_c_zrp_constant_pool_remap_add(
                    constantPoolRemap,
                    fieldRows[index].defaultValueConstantPoolOffset,
                    fieldRows[index].defaultValueConstantPoolLength)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool backend_aot_c_zrp_constant_pool_remap_lookup(const SZrAotCZrpConstantPoolRemap *remap,
                                                            TZrUInt32 oldOffset,
                                                            TZrUInt32 byteLength,
                                                            TZrUInt32 *outNewOffset) {
    if (outNewOffset == ZR_NULL) {
        return ZR_FALSE;
    }
    if (byteLength == 0u) {
        if (oldOffset != 0u) {
            return ZR_FALSE;
        }
        *outNewOffset = 0u;
        return ZR_TRUE;
    }
    if (remap == ZR_NULL || remap->entries == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < remap->count; index++) {
        if (remap->entries[index].oldOffset == oldOffset && remap->entries[index].byteLength == byteLength) {
            *outNewOffset = remap->entries[index].newOffset;
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

TZrBool backend_aot_c_zrp_copy_constant_pool(TZrByte *targetBlob,
                                             const TZrByte *sourceBlob,
                                             const SZrZrpMetadataHeader *sourceHeader,
                                             const SZrZrpMetadataHeader *targetHeader,
                                             const SZrAotCZrpConstantPoolRemap *constantPoolRemap) {
    TZrByte *targetPool;
    const TZrByte *sourcePool;

    if (targetHeader == ZR_NULL || targetHeader->constantPool.byteLength == 0u) {
        return ZR_TRUE;
    }
    if (targetBlob == ZR_NULL ||
        sourceBlob == ZR_NULL ||
        sourceHeader == ZR_NULL ||
        constantPoolRemap == ZR_NULL ||
        targetHeader->constantPool.byteLength != constantPoolRemap->byteLength) {
        return ZR_FALSE;
    }

    targetPool = targetBlob + targetHeader->constantPool.offset;
    sourcePool = sourceBlob + sourceHeader->constantPool.offset;
    for (TZrUInt32 index = 0u; index < constantPoolRemap->count; index++) {
        const SZrAotCZrpConstantPoolRemapEntry *entry = &constantPoolRemap->entries[index];
        if (!backend_aot_c_zrp_constant_pool_range_is_valid(constantPoolRemap,
                                                            entry->oldOffset,
                                                            entry->byteLength) ||
            entry->newOffset > constantPoolRemap->byteLength ||
            entry->byteLength > constantPoolRemap->byteLength - entry->newOffset) {
            return ZR_FALSE;
        }
        memcpy(targetPool + entry->newOffset, sourcePool + entry->oldOffset, entry->byteLength);
    }

    return ZR_TRUE;
}

TZrBool backend_aot_c_zrp_remap_field_def_default_value_constant_pool_slice(
        SZrZrpMetadataFieldDefRow *row,
        const SZrAotCZrpConstantPoolRemap *constantPoolRemap) {
    TZrUInt32 newOffset;

    if (row == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!backend_aot_c_zrp_constant_pool_remap_lookup(constantPoolRemap,
                                                      row->defaultValueConstantPoolOffset,
                                                      row->defaultValueConstantPoolLength,
                                                      &newOffset)) {
        return ZR_FALSE;
    }

    row->defaultValueConstantPoolOffset = newOffset;
    return ZR_TRUE;
}
