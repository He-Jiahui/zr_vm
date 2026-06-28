#ifndef ZR_VM_PARSER_BACKEND_AOT_C_ZRP_METADATA_STRING_POOL_H
#define ZR_VM_PARSER_BACKEND_AOT_C_ZRP_METADATA_STRING_POOL_H

#include "backend_aot_function_table.h"

#include "zr_vm_core/zrp_metadata.h"

typedef struct SZrAotCZrpStringPoolRemapEntry {
    TZrUInt32 oldOffset;
    TZrUInt32 byteLength;
    TZrUInt32 newOffset;
} SZrAotCZrpStringPoolRemapEntry;

typedef struct SZrAotCZrpStringPoolRemap {
    SZrAotCZrpStringPoolRemapEntry *entries;
    TZrUInt32 count;
    TZrUInt32 capacity;
    TZrUInt32 byteLength;
    TZrUInt32 sourceByteLength;
} SZrAotCZrpStringPoolRemap;

TZrBool backend_aot_c_zrp_string_pool_remap_init(SZrAotCZrpStringPoolRemap *remap,
                                                 TZrUInt32 capacity,
                                                 TZrUInt32 sourceByteLength);
void backend_aot_c_zrp_string_pool_remap_destroy(SZrAotCZrpStringPoolRemap *remap);
TZrBool backend_aot_c_zrp_string_pool_remap_is_identity(const SZrAotCZrpStringPoolRemap *remap);

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
        TZrUInt32 retainedMethodDefCount);

void backend_aot_c_zrp_copy_string_pool(TZrByte *targetBlob,
                                        const TZrByte *sourceBlob,
                                        const SZrZrpMetadataHeader *sourceHeader,
                                        const SZrZrpMetadataHeader *targetHeader,
                                        const SZrAotCZrpStringPoolRemap *stringRemap);

TZrBool backend_aot_c_zrp_remap_type_def_string_offsets(SZrZrpMetadataTypeDefRow *row,
                                                        const SZrAotCZrpStringPoolRemap *stringRemap);
TZrBool backend_aot_c_zrp_remap_method_def_string_offsets(SZrZrpMetadataMethodDefRow *row,
                                                          const SZrAotCZrpStringPoolRemap *stringRemap);
TZrBool backend_aot_c_zrp_remap_field_def_string_offsets(SZrZrpMetadataFieldDefRow *row,
                                                         const SZrAotCZrpStringPoolRemap *stringRemap);
TZrBool backend_aot_c_zrp_remap_generic_param_string_offsets(SZrZrpMetadataGenericParamRow *row,
                                                            const SZrAotCZrpStringPoolRemap *stringRemap);
TZrBool backend_aot_c_zrp_remap_module_ref_string_offsets(SZrZrpMetadataModuleRefRow *row,
                                                          const SZrAotCZrpStringPoolRemap *stringRemap);

#endif
