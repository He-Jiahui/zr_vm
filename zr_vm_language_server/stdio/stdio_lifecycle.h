#ifndef ZR_VM_LANGUAGE_SERVER_STDIO_LIFECYCLE_H
#define ZR_VM_LANGUAGE_SERVER_STDIO_LIFECYCLE_H

#include "zr_vm_language_server/conf.h"

/** @brief 普通请求先于 initialize 到达时，主循环使用的协议错误码。 */
#define ZR_LSP_JSON_RPC_SERVER_NOT_INITIALIZED_CODE (-32002)

/** @brief stdio 会话从创建到 exit 的单向状态；请求守卫按当前状态选择错误响应。 */
typedef enum EZrStdioLifecycleState {
    ZR_STDIO_LIFECYCLE_NEW = 0,
    ZR_STDIO_LIFECYCLE_INITIALIZING,
    ZR_STDIO_LIFECYCLE_RUNNING,
    ZR_STDIO_LIFECYCLE_SHUTDOWN,
    ZR_STDIO_LIFECYCLE_EXITED,
} EZrStdioLifecycleState;

/** @brief 主分发循环持有的会话状态；initialized 标志记录通知是否按序到达。 */
typedef struct SZrStdioLifecycle {
    EZrStdioLifecycleState state;
    /* TODO: 该标志目前仅在测试中读取；需确认它是否是有意保留的外部可观测状态。 */
    TZrBool initializedNotificationReceived;
} SZrStdioLifecycle;

/** @brief 初始化新会话的请求门禁，供服务器构造路径调用。 */
void ZrLanguageServer_StdioLifecycle_Init(SZrStdioLifecycle *lifecycle);
/** @brief 成功发送 initialize 响应后进入可处理请求的阶段。 */
TZrBool ZrLanguageServer_StdioLifecycle_BeginInitialize(SZrStdioLifecycle *lifecycle);
/** @brief 接受 initialized 通知，只有 INITIALIZING 阶段能进入 RUNNING。 */
void ZrLanguageServer_StdioLifecycle_MarkInitialized(SZrStdioLifecycle *lifecycle);
/** @brief 为普通请求与 setTrace 通知提供共享状态门禁。 */
TZrBool ZrLanguageServer_StdioLifecycle_CanProcessRequest(const SZrStdioLifecycle *lifecycle);
/** @brief 判断 initialize 是否仍可重试，特别用于响应发送失败的场景。 */
TZrBool ZrLanguageServer_StdioLifecycle_IsNew(const SZrStdioLifecycle *lifecycle);
/** @brief shutdown 响应成功发送后停止接受普通请求。 */
TZrBool ZrLanguageServer_StdioLifecycle_BeginShutdown(SZrStdioLifecycle *lifecycle);
/** @brief 消费 exit 通知并给进程退出码；只有先完成 shutdown 才返回零。 */
int ZrLanguageServer_StdioLifecycle_Exit(SZrStdioLifecycle *lifecycle);
/** @brief 供主循环结束时判断会话是否已完成 shutdown。 */
TZrBool ZrLanguageServer_StdioLifecycle_IsShutdown(const SZrStdioLifecycle *lifecycle);

#endif
