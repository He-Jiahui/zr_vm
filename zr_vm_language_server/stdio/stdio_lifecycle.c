#include "stdio_lifecycle.h"

void ZrLanguageServer_StdioLifecycle_Init(SZrStdioLifecycle *lifecycle) {
    if (lifecycle == ZR_NULL) {
        return;
    }

    lifecycle->state = ZR_STDIO_LIFECYCLE_NEW;
    lifecycle->initializedNotificationReceived = ZR_FALSE;
}

TZrBool ZrLanguageServer_StdioLifecycle_BeginInitialize(SZrStdioLifecycle *lifecycle) {
    if (lifecycle == ZR_NULL || lifecycle->state != ZR_STDIO_LIFECYCLE_NEW) {
        return ZR_FALSE;
    }

    lifecycle->state = ZR_STDIO_LIFECYCLE_INITIALIZING;
    return ZR_TRUE;
}

void ZrLanguageServer_StdioLifecycle_MarkInitialized(SZrStdioLifecycle *lifecycle) {
    if (lifecycle == ZR_NULL || lifecycle->state != ZR_STDIO_LIFECYCLE_INITIALIZING) {
        return;
    }

    lifecycle->initializedNotificationReceived = ZR_TRUE;
    lifecycle->state = ZR_STDIO_LIFECYCLE_RUNNING;
}

TZrBool ZrLanguageServer_StdioLifecycle_CanProcessRequest(const SZrStdioLifecycle *lifecycle) {
    return lifecycle != ZR_NULL &&
           (lifecycle->state == ZR_STDIO_LIFECYCLE_INITIALIZING ||
            lifecycle->state == ZR_STDIO_LIFECYCLE_RUNNING);
}

TZrBool ZrLanguageServer_StdioLifecycle_IsNew(const SZrStdioLifecycle *lifecycle) {
    return lifecycle != ZR_NULL && lifecycle->state == ZR_STDIO_LIFECYCLE_NEW;
}

TZrBool ZrLanguageServer_StdioLifecycle_BeginShutdown(SZrStdioLifecycle *lifecycle) {
    if (!ZrLanguageServer_StdioLifecycle_CanProcessRequest(lifecycle)) {
        return ZR_FALSE;
    }

    lifecycle->state = ZR_STDIO_LIFECYCLE_SHUTDOWN;
    return ZR_TRUE;
}

int ZrLanguageServer_StdioLifecycle_Exit(SZrStdioLifecycle *lifecycle) {
    int exitCode = 1;

    if (lifecycle != ZR_NULL) {
        if (lifecycle->state == ZR_STDIO_LIFECYCLE_SHUTDOWN) {
            exitCode = 0;
        }
        lifecycle->state = ZR_STDIO_LIFECYCLE_EXITED;
    }

    return exitCode;
}

TZrBool ZrLanguageServer_StdioLifecycle_IsShutdown(const SZrStdioLifecycle *lifecycle) {
    return lifecycle != ZR_NULL && lifecycle->state == ZR_STDIO_LIFECYCLE_SHUTDOWN;
}
