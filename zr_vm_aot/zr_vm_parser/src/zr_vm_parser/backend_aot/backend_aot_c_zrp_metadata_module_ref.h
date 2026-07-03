#ifndef ZR_VM_PARSER_BACKEND_AOT_C_ZRP_METADATA_MODULE_REF_H
#define ZR_VM_PARSER_BACKEND_AOT_C_ZRP_METADATA_MODULE_REF_H

#include "backend_aot_function_table.h"

#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/zrp_metadata.h"

typedef struct SZrAotCZrpSignatureBlobRemap SZrAotCZrpSignatureBlobRemap;

TZrBool backend_aot_c_zrp_module_ref_row_is_retained(
        const SZrZrpMetadataModuleRefRow *row,
        const SZrMetadataTokenRecord *tokenRecords,
        TZrUInt32 tokenRecordCount,
        const SZrZrpMetadataTypeSpecRow *typeSpecRows,
        TZrUInt32 typeSpecCount,
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
        const TZrByte *signatureBlobPool,
        TZrUInt32 signatureBlobPoolBytes,
        const SZrAotCZrpSignatureBlobRemap *signatureRemap);

TZrUInt32 backend_aot_c_zrp_count_retained_module_refs(
        const SZrZrpMetadataModuleRefRow *rows,
        TZrUInt32 count,
        const SZrMetadataTokenRecord *tokenRecords,
        TZrUInt32 tokenRecordCount,
        const SZrZrpMetadataTypeSpecRow *typeSpecRows,
        TZrUInt32 typeSpecCount,
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
        const TZrByte *signatureBlobPool,
        TZrUInt32 signatureBlobPoolBytes,
        const SZrAotCZrpSignatureBlobRemap *signatureRemap);

TZrMetadataToken backend_aot_c_zrp_compacted_module_ref_token(
        const SZrZrpMetadataModuleRefRow *rows,
        TZrUInt32 count,
        TZrUInt32 moduleRefIndex,
        const SZrMetadataTokenRecord *tokenRecords,
        TZrUInt32 tokenRecordCount,
        const SZrZrpMetadataTypeSpecRow *typeSpecRows,
        TZrUInt32 typeSpecCount,
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
        const TZrByte *signatureBlobPool,
        TZrUInt32 signatureBlobPoolBytes,
        const SZrAotCZrpSignatureBlobRemap *signatureRemap);

TZrBool backend_aot_c_zrp_remap_module_ref_token(
        TZrMetadataToken *token,
        const SZrZrpMetadataModuleRefRow *rows,
        TZrUInt32 count,
        const SZrMetadataTokenRecord *tokenRecords,
        TZrUInt32 tokenRecordCount,
        const SZrZrpMetadataTypeSpecRow *typeSpecRows,
        TZrUInt32 typeSpecCount,
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
        const TZrByte *signatureBlobPool,
        TZrUInt32 signatureBlobPoolBytes,
        const SZrAotCZrpSignatureBlobRemap *signatureRemap);

TZrBool backend_aot_c_zrp_remap_module_ref_tokens_in_record(
        SZrMetadataTokenRecord *record,
        const SZrZrpMetadataModuleRefRow *rows,
        TZrUInt32 count,
        const SZrMetadataTokenRecord *tokenRecords,
        TZrUInt32 tokenRecordCount,
        const SZrZrpMetadataTypeSpecRow *typeSpecRows,
        TZrUInt32 typeSpecCount,
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
        const TZrByte *signatureBlobPool,
        TZrUInt32 signatureBlobPoolBytes,
        const SZrAotCZrpSignatureBlobRemap *signatureRemap);

#endif
