#ifndef ZR_VM_PARSER_BACKEND_AOT_C_ZRP_METADATA_MEMBER_TOKEN_H
#define ZR_VM_PARSER_BACKEND_AOT_C_ZRP_METADATA_MEMBER_TOKEN_H

#include "backend_aot_c_zrp_metadata_prune.h"

#include "zr_vm_core/zrp_metadata.h"

TZrBool backend_aot_c_zrp_member_token_remap_build(
        SZrAotCEmbeddedZrpMetadata *metadata,
        const SZrZrpMetadataMethodDefRow *methodRows,
        TZrUInt32 methodCount,
        const SZrZrpMetadataFieldDefRow *fieldRows,
        TZrUInt32 fieldCount,
        const SZrZrpMetadataTypeDefRow *typeRows,
        TZrUInt32 typeCount,
        const SZrMetadataTokenRecord *tokenRecords,
        TZrUInt32 tokenRecordCount,
        const SZrZrpMetadataGenericParamRow *genericParamRows,
        TZrUInt32 genericParamCount,
        const SZrZrpMetadataGenericParamConstraintRow *genericParamConstraintRows,
        TZrUInt32 genericParamConstraintCount,
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 retainedMethodDefCount,
        TZrUInt32 retainedFieldDefCount);
void backend_aot_c_zrp_member_token_remap_destroy(SZrAotCEmbeddedZrpMetadata *metadata);
TZrBool backend_aot_c_embedded_zrp_metadata_remap_member_token(const SZrAotCEmbeddedZrpMetadata *metadata,
                                                               TZrMetadataToken *token);
TZrBool backend_aot_c_embedded_zrp_metadata_remap_type_def_token(const SZrAotCEmbeddedZrpMetadata *metadata,
                                                                 TZrMetadataToken *token);
TZrBool backend_aot_c_zrp_manifest_export_table_build(
        SZrAotCEmbeddedZrpMetadata *metadata,
        const SZrAotManifestExportDeclaration *declarations,
        TZrUInt32 declarationCount);
void backend_aot_c_zrp_manifest_export_table_destroy(SZrAotCEmbeddedZrpMetadata *metadata);

#endif
