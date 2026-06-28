#ifndef ZR_VM_PARSER_BACKEND_AOT_C_ZRP_METADATA_SIZE_H
#define ZR_VM_PARSER_BACKEND_AOT_C_ZRP_METADATA_SIZE_H

#include <stdio.h>

#include "zr_vm_parser/writer.h"

typedef struct SZrAotZrpMetadataSizeStats {
    unsigned long long zrpMetadataBytes;
    unsigned long long tokenRecordBytes;
    unsigned long long typeDefBytes;
    unsigned long long methodDefBytes;
    unsigned long long fieldDefBytes;
    unsigned long long genericParamBytes;
    unsigned long long genericParamConstraintBytes;
    unsigned long long typeSpecBytes;
    unsigned long long methodSpecBytes;
    unsigned long long moduleRefBytes;
    unsigned long long stringPoolBytes;
    unsigned long long signatureBlobPoolBytes;
    unsigned long long constantPoolBytes;
    unsigned long long definitionTableBytes;
    unsigned long long poolBytes;
    unsigned long long tokenRecordCount;
    unsigned long long typeDefCount;
    unsigned long long methodDefCount;
    unsigned long long fieldDefCount;
    unsigned long long genericParamCount;
    unsigned long long genericParamConstraintCount;
    unsigned long long typeSpecCount;
    unsigned long long methodSpecCount;
    unsigned long long moduleRefCount;
    unsigned long long stringPoolCount;
    unsigned long long signatureBlobPoolCount;
    unsigned long long constantPoolCount;
} SZrAotZrpMetadataSizeStats;

void backend_aot_collect_zrp_metadata_size_stats(const SZrAotWriterOptions *options,
                                                 SZrAotZrpMetadataSizeStats *stats);
void backend_aot_collect_zrp_metadata_size_stats_from_blob(const TZrByte *blob,
                                                           TZrSize blobLength,
                                                           SZrAotZrpMetadataSizeStats *stats);
void backend_aot_write_zrp_metadata_size_stats(FILE *file, const SZrAotZrpMetadataSizeStats *stats);
void backend_aot_write_code_stripping_zrp_metadata_size_deltas(
        FILE *file,
        const SZrAotZrpMetadataSizeStats *beforeStats,
        const SZrAotZrpMetadataSizeStats *afterStats);

#endif
