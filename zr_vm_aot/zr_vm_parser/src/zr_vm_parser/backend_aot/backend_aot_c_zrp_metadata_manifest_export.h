#ifndef ZR_VM_PARSER_BACKEND_AOT_C_ZRP_METADATA_MANIFEST_EXPORT_H
#define ZR_VM_PARSER_BACKEND_AOT_C_ZRP_METADATA_MANIFEST_EXPORT_H

#include "backend_aot_function_table.h"
#include "backend_aot_c_zrp_metadata_prune.h"
#include "backend_aot_c_zrp_metadata_string_pool.h"

#include "zr_vm_core/zrp_metadata.h"

TZrBool backend_aot_c_zrp_manifest_export_row_is_retained(
        const SZrZrpMetadataManifestExportRow *row,
        const SZrMetadataTokenRecord *tokenRecords,
        TZrUInt32 tokenRecordCount,
        const SZrZrpMetadataTypeDefRow *typeRows,
        TZrUInt32 typeCount,
        const SZrZrpMetadataMethodDefRow *methodRows,
        TZrUInt32 methodCount,
        const SZrZrpMetadataFieldDefRow *fieldRows,
        TZrUInt32 fieldCount,
        const SZrZrpMetadataGenericParamRow *genericParamRows,
        TZrUInt32 genericParamCount,
        const SZrZrpMetadataGenericParamConstraintRow *genericParamConstraintRows,
        TZrUInt32 genericParamConstraintCount,
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 retainedMethodDefCount);

TZrBool backend_aot_c_zrp_copy_manifest_exports(
        TZrByte *targetBlob,
        const SZrZrpMetadataHeader *targetHeader,
        const SZrZrpMetadataManifestExportRow *rows,
        TZrUInt32 count,
        const SZrMetadataTokenRecord *tokenRecords,
        TZrUInt32 tokenRecordCount,
        const SZrZrpMetadataTypeDefRow *typeRows,
        TZrUInt32 typeCount,
        const SZrZrpMetadataMethodDefRow *methodRows,
        TZrUInt32 methodCount,
        const SZrZrpMetadataFieldDefRow *fieldRows,
        TZrUInt32 fieldCount,
        const SZrZrpMetadataGenericParamRow *genericParamRows,
        TZrUInt32 genericParamCount,
        const SZrZrpMetadataGenericParamConstraintRow *genericParamConstraintRows,
        TZrUInt32 genericParamConstraintCount,
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 retainedMethodDefCount,
        const SZrAotCZrpStringPoolRemap *stringRemap);

TZrBool backend_aot_c_zrp_publish_manifest_export_declarations(
        SZrAotCEmbeddedZrpMetadata *metadata,
        const SZrAotManifestExportDeclaration *declarations,
        TZrUInt32 declarationCount);

#endif
