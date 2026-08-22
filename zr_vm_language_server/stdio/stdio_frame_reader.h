#ifndef ZR_VM_LANGUAGE_SERVER_STDIO_FRAME_READER_H
#define ZR_VM_LANGUAGE_SERVER_STDIO_FRAME_READER_H

#include <stdio.h>

#include "zr_vm_language_server/conf.h"

typedef enum EZrStdioFrameReadStatus {
    ZR_STDIO_FRAME_READ_OK = 0,
    ZR_STDIO_FRAME_READ_EOF,
    ZR_STDIO_FRAME_READ_MALFORMED_HEADER,
    ZR_STDIO_FRAME_READ_PAYLOAD_TRUNCATED,
    ZR_STDIO_FRAME_READ_TOO_LARGE,
    ZR_STDIO_FRAME_READ_IO_ERROR,
} EZrStdioFrameReadStatus;

typedef struct SZrStdioFrameReaderLimits {
    TZrSize maxHeaderBytes;
    TZrSize maxHeaderCount;
    TZrSize maxMessageBytes;
} SZrStdioFrameReaderLimits;

void ZrLanguageServer_StdioFrameReader_DefaultLimits(SZrStdioFrameReaderLimits *outLimits);
EZrStdioFrameReadStatus ZrLanguageServer_StdioFrameReader_Read(
        FILE *input,
        const SZrStdioFrameReaderLimits *limits,
        char **outPayload,
        TZrSize *outLength);
const char *ZrLanguageServer_StdioFrameReader_StatusName(EZrStdioFrameReadStatus status);

#endif
