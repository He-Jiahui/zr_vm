#ifndef ZR_VM_DEBUG_BREAKPOINT_CONDITION_H
#define ZR_VM_DEBUG_BREAKPOINT_CONDITION_H

#include "debug_internal.h"

/**
 * @brief 在当前 trace 位置的首帧求值断点条件，给 hook 的命中判断返回布尔结果。
 *
 * 条件为空视为命中；求值只允许无副作用表达式，失败由调用方决定如何
 * 报告并跳过该断点。当前内部约定使用主帧 ID 1。
 */
ZR_DEBUG_API TZrBool zr_debug_breakpoint_condition_evaluate(ZrDebugAgent *agent,
                                                            const TZrChar *condition,
                                                            TZrBool *outSatisfied,
                                                            TZrChar *errorBuffer,
                                                            TZrSize errorBufferSize);

#endif // ZR_VM_DEBUG_BREAKPOINT_CONDITION_H
