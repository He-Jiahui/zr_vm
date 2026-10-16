#ifndef ZR_VM_DEBUG_BREAKPOINT_LOGPOINT_H
#define ZR_VM_DEBUG_BREAKPOINT_LOGPOINT_H

#include "debug_internal.h"

/**
 * @brief 将断点 logMessage 的文本和 `{expression}` 拼成单条输出事件。
 *
 * 插值沿用条件断点的只读表达式能力；每个插值失败会转成错误文本，
 * 格式化本身仍可成功。输出缓冲区由调用方提供且可能截断。
 */
ZR_DEBUG_API TZrBool zr_debug_breakpoint_logpoint_format(ZrDebugAgent *agent,
                                                         const TZrChar *logMessage,
                                                         TZrChar *outText,
                                                         TZrSize outTextSize);

#endif // ZR_VM_DEBUG_BREAKPOINT_LOGPOINT_H
