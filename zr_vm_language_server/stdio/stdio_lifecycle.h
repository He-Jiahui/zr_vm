#ifndef ZR_VM_LANGUAGE_SERVER_STDIO_LIFECYCLE_H
#define ZR_VM_LANGUAGE_SERVER_STDIO_LIFECYCLE_H

#include "zr_vm_language_server/conf.h"

#define ZR_LSP_JSON_RPC_SERVER_NOT_INITIALIZED_CODE (-32002)

typedef enum EZrStdioLifecycleState {
    ZR_STDIO_LIFECYCLE_NEW = 0,
    ZR_STDIO_LIFECYCLE_INITIALIZING,
    ZR_STDIO_LIFECYCLE_RUNNING,
    ZR_STDIO_LIFECYCLE_SHUTDOWN,
    ZR_STDIO_LIFECYCLE_EXITED,
} EZrStdioLifecycleState;

typedef struct SZrStdioLifecycle {
    EZrStdioLifecycleState state;
    TZrBool initializedNotificationReceived;
} SZrStdioLifecycle;

void ZrLanguageServer_StdioLifecycle_Init(SZrStdioLifecycle *lifecycle);
TZrBool ZrLanguageServer_StdioLifecycle_BeginInitialize(SZrStdioLifecycle *lifecycle);
void ZrLanguageServer_StdioLifecycle_MarkInitialized(SZrStdioLifecycle *lifecycle);
TZrBool ZrLanguageServer_StdioLifecycle_CanProcessRequest(const SZrStdioLifecycle *lifecycle);
TZrBool ZrLanguageServer_StdioLifecycle_IsNew(const SZrStdioLifecycle *lifecycle);
TZrBool ZrLanguageServer_StdioLifecycle_BeginShutdown(SZrStdioLifecycle *lifecycle);
int ZrLanguageServer_StdioLifecycle_Exit(SZrStdioLifecycle *lifecycle);
TZrBool ZrLanguageServer_StdioLifecycle_IsShutdown(const SZrStdioLifecycle *lifecycle);

#endif
