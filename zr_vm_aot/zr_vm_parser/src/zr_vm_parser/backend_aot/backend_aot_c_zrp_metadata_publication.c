#include "backend_aot_c_zrp_metadata_publication.h"

#include "zr_vm_core/zrp_metadata.h"

#include <stdio.h>

TZrBool backend_aot_c_publish_compacted_zrp_metadata(const SZrAotWriterOptions *options,
                                                     const SZrAotCEmbeddedZrpMetadata *metadata) {
    const TZrChar *path;
    SZrZrpMetadataHeader header;
    FILE *file;
    size_t written;
    int closeResult;

    path = options != ZR_NULL ? options->compactedZrpMetadataOutputPath : ZR_NULL;
    if (path == ZR_NULL || path[0] == '\0') {
        return ZR_TRUE;
    }
    if (metadata == ZR_NULL ||
        metadata->blob == ZR_NULL ||
        metadata->length == 0u ||
        !ZrCore_ZrpMetadata_ReadHeader(metadata->blob, metadata->length, &header) ||
        !ZrCore_ZrpMetadata_ValidateDefinitionTables(metadata->blob, metadata->length, &header)) {
        remove(path);
        return ZR_FALSE;
    }

    file = fopen(path, "wb");
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }

    written = fwrite(metadata->blob, 1u, (size_t)metadata->length, file);
    closeResult = fclose(file);
    if (written != (size_t)metadata->length || closeResult != 0) {
        remove(path);
        return ZR_FALSE;
    }

    return ZR_TRUE;
}
