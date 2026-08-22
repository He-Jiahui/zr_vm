#ifndef ZR_VM_LANGUAGE_SERVER_STDIO_SERVER_H
#define ZR_VM_LANGUAGE_SERVER_STDIO_SERVER_H

#include <stdio.h>

#include "zr_vm_core/global.h"

typedef struct SZrStdioServer SZrStdioServer;

typedef enum EZrStdioServerFaultPoint {
    ZR_STDIO_SERVER_FAULT_NONE = 0,
    ZR_STDIO_SERVER_FAULT_AFTER_GLOBAL,
    ZR_STDIO_SERVER_FAULT_AFTER_CONTEXT,
    ZR_STDIO_SERVER_FAULT_AFTER_INPUT_INIT,
    ZR_STDIO_SERVER_FAULT_AFTER_READER_START,
} EZrStdioServerFaultPoint;

typedef struct SZrStdioServerOptions {
    FILE *input;
    EZrStdioServerFaultPoint faultPoint;
} SZrStdioServerOptions;

SZrStdioServer *ZrLanguageServer_StdioServer_New(const SZrStdioServerOptions *options);
TZrBool ZrLanguageServer_StdioServer_Start(SZrStdioServer *server);
void ZrLanguageServer_StdioServer_Shutdown(SZrStdioServer *server);
void ZrLanguageServer_StdioServer_Free(SZrStdioServer *server);

#endif
