#ifndef ZR_VM_LANGUAGE_SERVER_STDIO_SERVER_H
#define ZR_VM_LANGUAGE_SERVER_STDIO_SERVER_H

#include <stdio.h>

#include "zr_vm_core/global.h"

/** @brief stdio 会话的不透明句柄；New 创建，Free 结束读线程并释放内部状态。 */
typedef struct SZrStdioServer SZrStdioServer;

/** @brief 生命周期自检使用的故障注入阶段；常规启动应使用 NONE。 */
typedef enum EZrStdioServerFaultPoint {
    ZR_STDIO_SERVER_FAULT_NONE = 0,
    ZR_STDIO_SERVER_FAULT_AFTER_GLOBAL,
    ZR_STDIO_SERVER_FAULT_AFTER_CONTEXT,
    ZR_STDIO_SERVER_FAULT_AFTER_INPUT_INIT,
    ZR_STDIO_SERVER_FAULT_AFTER_READER_START,
} EZrStdioServerFaultPoint;

/** @brief 创建会话时借用输入流并选择可选故障阶段。
 * @note input 为 NULL 时使用 stdin；调用方负责保持流可读至读线程结束并自行关闭它。 */
typedef struct SZrStdioServerOptions {
    FILE *input;
    EZrStdioServerFaultPoint faultPoint;
} SZrStdioServerOptions;

/** @brief 建立 VM、LSP 上下文、请求注册表及输入队列；失败时回收已建资源。
 * @return 成功返回由调用方持有的会话；失败返回 NULL。 */
SZrStdioServer *ZrLanguageServer_StdioServer_New(const SZrStdioServerOptions *options);
/** @brief 启动单个输入线程；失败后仍须调用 Free 结束已建会话。 */
TZrBool ZrLanguageServer_StdioServer_Start(SZrStdioServer *server);
/** @brief 请求输入侧停止；正在阻塞的流读取须由调用方确保能够返回。 */
void ZrLanguageServer_StdioServer_Shutdown(SZrStdioServer *server);
/** @brief 等待读线程并按依赖顺序释放会话资源；不关闭 options.input。 */
void ZrLanguageServer_StdioServer_Free(SZrStdioServer *server);

#endif
