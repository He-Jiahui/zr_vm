#include "backend_aot_c_zrp_metadata_size.h"

#include "zr_vm_core/zrp_metadata.h"

#include <string.h>

static unsigned long long backend_aot_zrp_section_bytes(const SZrZrpMetadataSection *section) {
    if (section == ZR_NULL) {
        return 0u;
    }
    return (unsigned long long)section->byteLength;
}

static unsigned long long backend_aot_zrp_section_count(const SZrZrpMetadataSection *section) {
    if (section == ZR_NULL) {
        return 0u;
    }
    return (unsigned long long)section->count;
}

void backend_aot_collect_zrp_metadata_size_stats(const SZrAotWriterOptions *options,
                                                 SZrAotZrpMetadataSizeStats *stats) {
    backend_aot_collect_zrp_metadata_size_stats_from_blob(
            options != ZR_NULL ? options->embeddedModuleBlob : ZR_NULL,
            options != ZR_NULL ? options->embeddedModuleBlobLength : 0u,
            stats);
}

void backend_aot_collect_zrp_metadata_size_stats_from_blob(const TZrByte *blob,
                                                           TZrSize blobLength,
                                                           SZrAotZrpMetadataSizeStats *stats) {
    SZrZrpMetadataHeader header;

    if (stats == ZR_NULL) {
        return;
    }

    memset(stats, 0, sizeof(*stats));
    if (blob != ZR_NULL &&
        blobLength > 0u &&
        ZrCore_ZrpMetadata_ReadHeader(blob, blobLength, &header)) {
        stats->zrpMetadataBytes = (unsigned long long)blobLength;
        stats->tokenRecordBytes = backend_aot_zrp_section_bytes(&header.tokenRecords);
        stats->typeDefBytes = backend_aot_zrp_section_bytes(&header.typeDefs);
        stats->methodDefBytes = backend_aot_zrp_section_bytes(&header.methodDefs);
        stats->fieldDefBytes = backend_aot_zrp_section_bytes(&header.fieldDefs);
        stats->genericParamBytes = backend_aot_zrp_section_bytes(&header.genericParams);
        stats->genericParamConstraintBytes = backend_aot_zrp_section_bytes(&header.genericParamConstraints);
        stats->typeSpecBytes = backend_aot_zrp_section_bytes(&header.typeSpecs);
        stats->methodSpecBytes = backend_aot_zrp_section_bytes(&header.methodSpecs);
        stats->moduleRefBytes = backend_aot_zrp_section_bytes(&header.moduleRefs);
        stats->stringPoolBytes = backend_aot_zrp_section_bytes(&header.stringPool);
        stats->signatureBlobPoolBytes = backend_aot_zrp_section_bytes(&header.signatureBlobPool);
        stats->constantPoolBytes = backend_aot_zrp_section_bytes(&header.constantPool);
        stats->tokenRecordCount = backend_aot_zrp_section_count(&header.tokenRecords);
        stats->typeDefCount = backend_aot_zrp_section_count(&header.typeDefs);
        stats->methodDefCount = backend_aot_zrp_section_count(&header.methodDefs);
        stats->fieldDefCount = backend_aot_zrp_section_count(&header.fieldDefs);
        stats->genericParamCount = backend_aot_zrp_section_count(&header.genericParams);
        stats->genericParamConstraintCount = backend_aot_zrp_section_count(&header.genericParamConstraints);
        stats->typeSpecCount = backend_aot_zrp_section_count(&header.typeSpecs);
        stats->methodSpecCount = backend_aot_zrp_section_count(&header.methodSpecs);
        stats->moduleRefCount = backend_aot_zrp_section_count(&header.moduleRefs);
        stats->stringPoolCount = backend_aot_zrp_section_count(&header.stringPool);
        stats->signatureBlobPoolCount = backend_aot_zrp_section_count(&header.signatureBlobPool);
        stats->constantPoolCount = backend_aot_zrp_section_count(&header.constantPool);
    }

    stats->definitionTableBytes = stats->typeDefBytes +
                                  stats->methodDefBytes +
                                  stats->fieldDefBytes +
                                  stats->genericParamBytes +
                                  stats->genericParamConstraintBytes +
                                  stats->typeSpecBytes +
                                  stats->methodSpecBytes +
                                  stats->moduleRefBytes;
    stats->poolBytes = stats->stringPoolBytes + stats->signatureBlobPoolBytes + stats->constantPoolBytes;
}

static void backend_aot_write_zrp_metadata_size_stat(FILE *file,
                                                     const TZrChar *name,
                                                     unsigned long long bytes) {
    fprintf(file, "/* aot_size.%s = %llu */\n", name, bytes);
}

void backend_aot_write_zrp_metadata_size_stats(FILE *file, const SZrAotZrpMetadataSizeStats *stats) {
    if (file == ZR_NULL || stats == ZR_NULL) {
        return;
    }

    backend_aot_write_zrp_metadata_size_stat(file, "zrpMetadataBytes", stats->zrpMetadataBytes);
    backend_aot_write_zrp_metadata_size_stat(file, "zrpMetadataTokenRecordBytes", stats->tokenRecordBytes);
    backend_aot_write_zrp_metadata_size_stat(file,
                                             "zrpMetadataDefinitionTableBytes",
                                             stats->definitionTableBytes);
    backend_aot_write_zrp_metadata_size_stat(file, "zrpMetadataPoolBytes", stats->poolBytes);
    backend_aot_write_zrp_metadata_size_stat(file, "zrpMetadataSectionBytes.tokenRecords", stats->tokenRecordBytes);
    backend_aot_write_zrp_metadata_size_stat(file, "zrpMetadataSectionBytes.typeDefs", stats->typeDefBytes);
    backend_aot_write_zrp_metadata_size_stat(file, "zrpMetadataSectionBytes.methodDefs", stats->methodDefBytes);
    backend_aot_write_zrp_metadata_size_stat(file, "zrpMetadataSectionBytes.fieldDefs", stats->fieldDefBytes);
    backend_aot_write_zrp_metadata_size_stat(file, "zrpMetadataSectionBytes.genericParams", stats->genericParamBytes);
    backend_aot_write_zrp_metadata_size_stat(file,
                                             "zrpMetadataSectionBytes.genericParamConstraints",
                                             stats->genericParamConstraintBytes);
    backend_aot_write_zrp_metadata_size_stat(file, "zrpMetadataSectionBytes.typeSpecs", stats->typeSpecBytes);
    backend_aot_write_zrp_metadata_size_stat(file, "zrpMetadataSectionBytes.methodSpecs", stats->methodSpecBytes);
    backend_aot_write_zrp_metadata_size_stat(file, "zrpMetadataSectionBytes.moduleRefs", stats->moduleRefBytes);
    backend_aot_write_zrp_metadata_size_stat(file, "zrpMetadataSectionBytes.stringPool", stats->stringPoolBytes);
    backend_aot_write_zrp_metadata_size_stat(file,
                                             "zrpMetadataSectionBytes.signatureBlobPool",
                                             stats->signatureBlobPoolBytes);
    backend_aot_write_zrp_metadata_size_stat(file,
                                             "zrpMetadataSectionBytes.constantPool",
                                             stats->constantPoolBytes);
    backend_aot_write_zrp_metadata_size_stat(file, "zrpMetadataSectionCounts.tokenRecords", stats->tokenRecordCount);
    backend_aot_write_zrp_metadata_size_stat(file, "zrpMetadataSectionCounts.typeDefs", stats->typeDefCount);
    backend_aot_write_zrp_metadata_size_stat(file, "zrpMetadataSectionCounts.methodDefs", stats->methodDefCount);
    backend_aot_write_zrp_metadata_size_stat(file, "zrpMetadataSectionCounts.fieldDefs", stats->fieldDefCount);
    backend_aot_write_zrp_metadata_size_stat(file,
                                             "zrpMetadataSectionCounts.genericParams",
                                             stats->genericParamCount);
    backend_aot_write_zrp_metadata_size_stat(file,
                                             "zrpMetadataSectionCounts.genericParamConstraints",
                                             stats->genericParamConstraintCount);
    backend_aot_write_zrp_metadata_size_stat(file, "zrpMetadataSectionCounts.typeSpecs", stats->typeSpecCount);
    backend_aot_write_zrp_metadata_size_stat(file, "zrpMetadataSectionCounts.methodSpecs", stats->methodSpecCount);
    backend_aot_write_zrp_metadata_size_stat(file, "zrpMetadataSectionCounts.moduleRefs", stats->moduleRefCount);
    backend_aot_write_zrp_metadata_size_stat(file, "zrpMetadataSectionCounts.stringPool", stats->stringPoolCount);
    backend_aot_write_zrp_metadata_size_stat(file,
                                             "zrpMetadataSectionCounts.signatureBlobPool",
                                             stats->signatureBlobPoolCount);
    backend_aot_write_zrp_metadata_size_stat(file,
                                             "zrpMetadataSectionCounts.constantPool",
                                             stats->constantPoolCount);
}

static void backend_aot_write_code_stripping_zrp_metadata_delta_stat(FILE *file,
                                                                     const TZrChar *name,
                                                                     unsigned long long bytesBefore,
                                                                     unsigned long long bytesAfter) {
    unsigned long long bytesRemoved = bytesBefore >= bytesAfter ? bytesBefore - bytesAfter : 0u;

    if (file == ZR_NULL || name == ZR_NULL) {
        return;
    }

    fprintf(file, "/* code_stripping.%sBefore = %llu */\n", name, bytesBefore);
    fprintf(file, "/* code_stripping.%sAfter = %llu */\n", name, bytesAfter);
    fprintf(file, "/* code_stripping.%sRemoved = %llu */\n", name, bytesRemoved);
}

static void backend_aot_write_code_stripping_zrp_metadata_section_delta_stats(
        FILE *file,
        const SZrAotZrpMetadataSizeStats *beforeStats,
        const SZrAotZrpMetadataSizeStats *afterStats) {
    backend_aot_write_code_stripping_zrp_metadata_delta_stat(file,
                                                             "zrpMetadataSectionBytes.tokenRecords",
                                                             beforeStats->tokenRecordBytes,
                                                             afterStats->tokenRecordBytes);
    backend_aot_write_code_stripping_zrp_metadata_delta_stat(file,
                                                             "zrpMetadataSectionBytes.typeDefs",
                                                             beforeStats->typeDefBytes,
                                                             afterStats->typeDefBytes);
    backend_aot_write_code_stripping_zrp_metadata_delta_stat(file,
                                                             "zrpMetadataSectionBytes.methodDefs",
                                                             beforeStats->methodDefBytes,
                                                             afterStats->methodDefBytes);
    backend_aot_write_code_stripping_zrp_metadata_delta_stat(file,
                                                             "zrpMetadataSectionBytes.fieldDefs",
                                                             beforeStats->fieldDefBytes,
                                                             afterStats->fieldDefBytes);
    backend_aot_write_code_stripping_zrp_metadata_delta_stat(file,
                                                             "zrpMetadataSectionBytes.genericParams",
                                                             beforeStats->genericParamBytes,
                                                             afterStats->genericParamBytes);
    backend_aot_write_code_stripping_zrp_metadata_delta_stat(
            file,
            "zrpMetadataSectionBytes.genericParamConstraints",
            beforeStats->genericParamConstraintBytes,
            afterStats->genericParamConstraintBytes);
    backend_aot_write_code_stripping_zrp_metadata_delta_stat(file,
                                                             "zrpMetadataSectionBytes.typeSpecs",
                                                             beforeStats->typeSpecBytes,
                                                             afterStats->typeSpecBytes);
    backend_aot_write_code_stripping_zrp_metadata_delta_stat(file,
                                                             "zrpMetadataSectionBytes.methodSpecs",
                                                             beforeStats->methodSpecBytes,
                                                             afterStats->methodSpecBytes);
    backend_aot_write_code_stripping_zrp_metadata_delta_stat(file,
                                                             "zrpMetadataSectionBytes.moduleRefs",
                                                             beforeStats->moduleRefBytes,
                                                             afterStats->moduleRefBytes);
    backend_aot_write_code_stripping_zrp_metadata_delta_stat(file,
                                                             "zrpMetadataSectionBytes.stringPool",
                                                             beforeStats->stringPoolBytes,
                                                             afterStats->stringPoolBytes);
    backend_aot_write_code_stripping_zrp_metadata_delta_stat(file,
                                                             "zrpMetadataSectionBytes.signatureBlobPool",
                                                             beforeStats->signatureBlobPoolBytes,
                                                             afterStats->signatureBlobPoolBytes);
    backend_aot_write_code_stripping_zrp_metadata_delta_stat(file,
                                                             "zrpMetadataSectionBytes.constantPool",
                                                             beforeStats->constantPoolBytes,
                                                             afterStats->constantPoolBytes);
}

static void backend_aot_write_code_stripping_zrp_metadata_section_count_delta_stats(
        FILE *file,
        const SZrAotZrpMetadataSizeStats *beforeStats,
        const SZrAotZrpMetadataSizeStats *afterStats) {
    backend_aot_write_code_stripping_zrp_metadata_delta_stat(file,
                                                             "zrpMetadataSectionCounts.tokenRecords",
                                                             beforeStats->tokenRecordCount,
                                                             afterStats->tokenRecordCount);
    backend_aot_write_code_stripping_zrp_metadata_delta_stat(file,
                                                             "zrpMetadataSectionCounts.typeDefs",
                                                             beforeStats->typeDefCount,
                                                             afterStats->typeDefCount);
    backend_aot_write_code_stripping_zrp_metadata_delta_stat(file,
                                                             "zrpMetadataSectionCounts.methodDefs",
                                                             beforeStats->methodDefCount,
                                                             afterStats->methodDefCount);
    backend_aot_write_code_stripping_zrp_metadata_delta_stat(file,
                                                             "zrpMetadataSectionCounts.fieldDefs",
                                                             beforeStats->fieldDefCount,
                                                             afterStats->fieldDefCount);
    backend_aot_write_code_stripping_zrp_metadata_delta_stat(file,
                                                             "zrpMetadataSectionCounts.genericParams",
                                                             beforeStats->genericParamCount,
                                                             afterStats->genericParamCount);
    backend_aot_write_code_stripping_zrp_metadata_delta_stat(
            file,
            "zrpMetadataSectionCounts.genericParamConstraints",
            beforeStats->genericParamConstraintCount,
            afterStats->genericParamConstraintCount);
    backend_aot_write_code_stripping_zrp_metadata_delta_stat(file,
                                                             "zrpMetadataSectionCounts.typeSpecs",
                                                             beforeStats->typeSpecCount,
                                                             afterStats->typeSpecCount);
    backend_aot_write_code_stripping_zrp_metadata_delta_stat(file,
                                                             "zrpMetadataSectionCounts.methodSpecs",
                                                             beforeStats->methodSpecCount,
                                                             afterStats->methodSpecCount);
    backend_aot_write_code_stripping_zrp_metadata_delta_stat(file,
                                                             "zrpMetadataSectionCounts.moduleRefs",
                                                             beforeStats->moduleRefCount,
                                                             afterStats->moduleRefCount);
    backend_aot_write_code_stripping_zrp_metadata_delta_stat(file,
                                                             "zrpMetadataSectionCounts.stringPool",
                                                             beforeStats->stringPoolCount,
                                                             afterStats->stringPoolCount);
    backend_aot_write_code_stripping_zrp_metadata_delta_stat(file,
                                                             "zrpMetadataSectionCounts.signatureBlobPool",
                                                             beforeStats->signatureBlobPoolCount,
                                                             afterStats->signatureBlobPoolCount);
    backend_aot_write_code_stripping_zrp_metadata_delta_stat(file,
                                                             "zrpMetadataSectionCounts.constantPool",
                                                             beforeStats->constantPoolCount,
                                                             afterStats->constantPoolCount);
}

void backend_aot_write_code_stripping_zrp_metadata_size_deltas(
        FILE *file,
        const SZrAotZrpMetadataSizeStats *beforeStats,
        const SZrAotZrpMetadataSizeStats *afterStats) {
    if (file == ZR_NULL || beforeStats == ZR_NULL || afterStats == ZR_NULL) {
        return;
    }

    backend_aot_write_code_stripping_zrp_metadata_delta_stat(file,
                                                             "zrpMetadataBytes",
                                                             beforeStats->zrpMetadataBytes,
                                                             afterStats->zrpMetadataBytes);
    backend_aot_write_code_stripping_zrp_metadata_delta_stat(file,
                                                             "zrpMetadataTokenRecordBytes",
                                                             beforeStats->tokenRecordBytes,
                                                             afterStats->tokenRecordBytes);
    backend_aot_write_code_stripping_zrp_metadata_delta_stat(file,
                                                             "zrpMetadataDefinitionTableBytes",
                                                             beforeStats->definitionTableBytes,
                                                             afterStats->definitionTableBytes);
    backend_aot_write_code_stripping_zrp_metadata_delta_stat(file,
                                                             "zrpMetadataPoolBytes",
                                                             beforeStats->poolBytes,
                                                             afterStats->poolBytes);
    backend_aot_write_code_stripping_zrp_metadata_section_delta_stats(file, beforeStats, afterStats);
    backend_aot_write_code_stripping_zrp_metadata_section_count_delta_stats(file, beforeStats, afterStats);
}
