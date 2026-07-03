#ifndef ZR_VM_PARSER_BACKEND_AOT_C_ZRP_METADATA_CONSTANT_POOL_H
#define ZR_VM_PARSER_BACKEND_AOT_C_ZRP_METADATA_CONSTANT_POOL_H

#include "backend_aot_function_table.h"

#include "zr_vm_core/zrp_metadata.h"

typedef struct SZrAotCZrpConstantPoolRemapEntry {
    TZrUInt32 oldOffset;
    TZrUInt32 byteLength;
    TZrUInt32 newOffset;
} SZrAotCZrpConstantPoolRemapEntry;

typedef struct SZrAotCZrpConstantPoolRemap {
    SZrAotCZrpConstantPoolRemapEntry *entries;
    TZrUInt32 count;
    TZrUInt32 capacity;
    TZrUInt32 byteLength;
    TZrUInt32 sourceByteLength;
} SZrAotCZrpConstantPoolRemap;

TZrBool backend_aot_c_zrp_constant_pool_remap_init(SZrAotCZrpConstantPoolRemap *remap,
                                                   TZrUInt32 capacity,
                                                   TZrUInt32 sourceByteLength);
void backend_aot_c_zrp_constant_pool_remap_destroy(SZrAotCZrpConstantPoolRemap *remap);
TZrBool backend_aot_c_zrp_constant_pool_remap_is_identity(const SZrAotCZrpConstantPoolRemap *remap);

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
                                                    TZrUInt32 retainedMethodDefCount);

TZrBool backend_aot_c_zrp_copy_constant_pool(TZrByte *targetBlob,
                                             const TZrByte *sourceBlob,
                                             const SZrZrpMetadataHeader *sourceHeader,
                                             const SZrZrpMetadataHeader *targetHeader,
                                             const SZrAotCZrpConstantPoolRemap *constantPoolRemap);

TZrBool backend_aot_c_zrp_remap_field_def_default_value_constant_pool_slice(
        SZrZrpMetadataFieldDefRow *row,
        const SZrAotCZrpConstantPoolRemap *constantPoolRemap);

#endif
