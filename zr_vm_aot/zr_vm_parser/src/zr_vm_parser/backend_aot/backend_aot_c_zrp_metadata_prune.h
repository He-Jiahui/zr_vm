#ifndef ZR_VM_PARSER_BACKEND_AOT_C_ZRP_METADATA_PRUNE_H
#define ZR_VM_PARSER_BACKEND_AOT_C_ZRP_METADATA_PRUNE_H

#include "backend_aot_function_table.h"
#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_parser/writer.h"

typedef struct SZrAotCZrpMemberTokenRemapEntry {
    TZrMetadataToken sourceToken;
    TZrMetadataToken targetToken;
} SZrAotCZrpMemberTokenRemapEntry;

typedef struct SZrAotCZrpTypeDefTokenRemapEntry {
    TZrMetadataToken sourceToken;
    TZrMetadataToken targetToken;
} SZrAotCZrpTypeDefTokenRemapEntry;

typedef struct SZrAotCEmbeddedZrpMetadata {
    const TZrByte *blob;
    TZrSize length;
    TZrByte *ownedBlob;
    TZrBool hasTypeDefTokenRemap;
    const SZrAotCZrpTypeDefTokenRemapEntry *typeDefTokenRemapEntries;
    TZrUInt32 typeDefTokenRemapCount;
    SZrAotCZrpTypeDefTokenRemapEntry *ownedTypeDefTokenRemapEntries;
    const SZrAotCZrpMemberTokenRemapEntry *memberTokenRemapEntries;
    TZrUInt32 memberTokenRemapCount;
    SZrAotCZrpMemberTokenRemapEntry *ownedMemberTokenRemapEntries;
    const SZrAotManifestExportEntry *manifestExportEntries;
    TZrUInt32 manifestExportCount;
    SZrAotManifestExportEntry *ownedManifestExportEntries;
} SZrAotCEmbeddedZrpMetadata;

TZrBool backend_aot_c_prepare_embedded_zrp_metadata(const SZrAotWriterOptions *options,
                                                    TZrBool enableCodeStripping,
                                                    const SZrAotFunctionTable *functionTable,
                                                    SZrAotCEmbeddedZrpMetadata *outMetadata);
void backend_aot_c_release_embedded_zrp_metadata(SZrAotCEmbeddedZrpMetadata *metadata);

#endif
