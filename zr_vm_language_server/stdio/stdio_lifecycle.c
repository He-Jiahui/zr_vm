#include "stdio_lifecycle.h"

/* 服务器构造时将请求门禁清到 NEW，避免复用未初始化的通知状态。 */
void ZrLanguageServer_StdioLifecycle_Init(SZrStdioLifecycle *lifecycle) {
    if (lifecycle == ZR_NULL) {
        return;
    }

    lifecycle->state = ZR_STDIO_LIFECYCLE_NEW;
    lifecycle->initializedNotificationReceived = ZR_FALSE;
}

/* 仅在响应成功写出后提交 initialize；发送失败允许客户端重试。 */
TZrBool ZrLanguageServer_StdioLifecycle_BeginInitialize(SZrStdioLifecycle *lifecycle) {
    if (lifecycle == ZR_NULL || lifecycle->state != ZR_STDIO_LIFECYCLE_NEW) {
        return ZR_FALSE;
    }

    lifecycle->state = ZR_STDIO_LIFECYCLE_INITIALIZING;
    return ZR_TRUE;
}

/* 早到或重复的 initialized 通知不改变会话状态。 */
void ZrLanguageServer_StdioLifecycle_MarkInitialized(SZrStdioLifecycle *lifecycle) {
    if (lifecycle == ZR_NULL || lifecycle->state != ZR_STDIO_LIFECYCLE_INITIALIZING) {
        return;
    }

    lifecycle->initializedNotificationReceived = ZR_TRUE;
    lifecycle->state = ZR_STDIO_LIFECYCLE_RUNNING;
}

/* INITIALIZING 与 RUNNING 均容许普通请求，具体请求仍受分发器自身约束。 */
TZrBool ZrLanguageServer_StdioLifecycle_CanProcessRequest(const SZrStdioLifecycle *lifecycle) {
    return lifecycle != ZR_NULL &&
           (lifecycle->state == ZR_STDIO_LIFECYCLE_INITIALIZING ||
            lifecycle->state == ZR_STDIO_LIFECYCLE_RUNNING);
}

/* initialize 失败或未开始时仍应保留 NEW，供错误响应与重试路径使用。 */
TZrBool ZrLanguageServer_StdioLifecycle_IsNew(const SZrStdioLifecycle *lifecycle) {
    return lifecycle != ZR_NULL && lifecycle->state == ZR_STDIO_LIFECYCLE_NEW;
}

/* 与 initialize 一样，只有 shutdown 响应已发送才推进状态。 */
TZrBool ZrLanguageServer_StdioLifecycle_BeginShutdown(SZrStdioLifecycle *lifecycle) {
    if (!ZrLanguageServer_StdioLifecycle_CanProcessRequest(lifecycle)) {
        return ZR_FALSE;
    }

    lifecycle->state = ZR_STDIO_LIFECYCLE_SHUTDOWN;
    return ZR_TRUE;
}

/* 有效会话的 exit 会终结状态；退出码反映此前是否完成 shutdown 握手。 */
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

/* 主循环退出时复核握手状态，不依赖 initialized 标志。 */
TZrBool ZrLanguageServer_StdioLifecycle_IsShutdown(const SZrStdioLifecycle *lifecycle) {
    return lifecycle != ZR_NULL && lifecycle->state == ZR_STDIO_LIFECYCLE_SHUTDOWN;
}
