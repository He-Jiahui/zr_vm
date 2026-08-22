#include "zr_vm_language_server_stdio_internal.h"

static char *stdio_read_all_text(const TZrChar *nativePath, size_t *outLength) {
    FILE *file;
    long rawSize;
    size_t readLength;
    char *buffer;

    if (nativePath == ZR_NULL || outLength == NULL) {
        return NULL;
    }

    *outLength = 0;
    file = fopen(nativePath, "rb");
    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    rawSize = ftell(file);
    if (rawSize < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    buffer = (char *)malloc((size_t)rawSize + 1);
    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }

    readLength = rawSize > 0 ? fread(buffer, 1, (size_t)rawSize, file) : 0;
    fclose(file);
    if (rawSize > 0 && readLength != (size_t)rawSize) {
        free(buffer);
        return NULL;
    }

    buffer[readLength] = '\0';
    *outLength = readLength;
    return buffer;
}

char *read_document_text_from_uri(SZrString *uri, size_t *outLength) {
    TZrChar nativePath[ZR_VM_PATH_LENGTH_MAX];

    if (!ZrLanguageServer_LspUri_FileToNativePath(uri, nativePath, sizeof(nativePath))) {
        if (outLength != NULL) {
            *outLength = 0;
        }
        return NULL;
    }

    return stdio_read_all_text(nativePath, outLength);
}
