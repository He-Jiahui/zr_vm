//
// Created by HeJiahui on 2025/6/20.
//

#ifndef ZR_VM_CORE_LOG_H
#define ZR_VM_CORE_LOG_H
#include "zr_vm_core/conf.h"

struct SZrState;

/** @brief 默认 sink 使用的标准输出或标准错误通道。 */
typedef enum EZrOutputChannel {
    ZR_OUTPUT_CHANNEL_STDOUT = 0,
    ZR_OUTPUT_CHANNEL_STDERR = 1
} EZrOutputChannel;

/** @brief 区分结果、帮助、元信息和诊断消息的消费语义。 */
typedef enum EZrOutputKind {
    ZR_OUTPUT_KIND_RESULT = 0,
    ZR_OUTPUT_KIND_HELP = 1,
    ZR_OUTPUT_KIND_META = 2,
    ZR_OUTPUT_KIND_DIAGNOSTIC = 3
} EZrOutputKind;

/** @brief 全局日志观察回调；message 仅在本次调用期间有效。 */
typedef void (*FZrLog)(struct SZrState *state,
                       EZrLogLevel level,
                       EZrOutputChannel channel,
                       EZrOutputKind kind,
                       TZrNativeString message);

/** @brief 串行写入默认 sink，再通知全局观察回调；允许 state 为空。 */
ZR_CORE_API void ZrCore_Log_Write(struct SZrState *state,
                                  EZrLogLevel level,
                                  EZrOutputChannel channel,
                                  EZrOutputKind kind,
                                  TZrNativeString message);

/** @brief 格式化带显式级别、通道和类别的消息。 */
ZR_CORE_API void ZrCore_Log_Printf(struct SZrState *state,
                                   EZrLogLevel level,
                                   EZrOutputChannel channel,
                                   EZrOutputKind kind,
                                   TZrNativeString format,
                                   ...);

/** @brief 输出命令或 REPL 结果到标准输出。 */
ZR_CORE_API void ZrCore_Log_Resultf(struct SZrState *state, TZrNativeString format, ...);

/** @brief 输出帮助文本到标准输出。 */
ZR_CORE_API void ZrCore_Log_Helpf(struct SZrState *state, TZrNativeString format, ...);

/** @brief 输出机器可读元信息到标准输出。 */
ZR_CORE_API void ZrCore_Log_Metaf(struct SZrState *state, TZrNativeString format, ...);

/** @brief 按显式级别和通道输出诊断信息。 */
ZR_CORE_API void ZrCore_Log_Diagnosticf(struct SZrState *state,
                                        EZrLogLevel level,
                                        EZrOutputChannel channel,
                                        TZrNativeString format,
                                        ...);

/** @brief 向标准错误输出普通错误。 */
ZR_CORE_API void ZrCore_Log_Error(struct SZrState *state, TZrNativeString format, ...);

/** @brief 向标准错误输出异常诊断。 */
ZR_CORE_API void ZrCore_Log_Exception(struct SZrState *state, TZrNativeString format, ...);

/** @brief 向标准错误输出致命诊断；本函数本身不会中止进程。 */
ZR_CORE_API void ZrCore_Log_Fatal(struct SZrState *state, TZrNativeString format, ...);

/** @brief 在日志锁内刷新默认标准输出和错误流。 */
ZR_CORE_API void ZrCore_Log_FlushDefaultSinks(void);

#endif //ZR_VM_CORE_LOG_H
