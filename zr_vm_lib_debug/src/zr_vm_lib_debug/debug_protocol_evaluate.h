#ifndef ZR_VM_DEBUG_PROTOCOL_EVALUATE_H
#define ZR_VM_DEBUG_PROTOCOL_EVALUATE_H

#include "debug_internal.h"

/** @brief 将调试协议的 evaluate 上下文映射为允许的副作用能力位；未知上下文默认无能力。 */
TZrUInt32 zr_debug_protocol_evaluate_allowed_effect_flags(const cJSON *contextItem);
/** @brief 对暂停线程求值并生成所有权交给调用方的 JSON 结果。 */
ZR_DEBUG_API cJSON *zr_debug_protocol_make_evaluate_result(ZrDebugAgent *agent,
                                                           TZrUInt32 threadId,
                                                           TZrUInt32 frameId,
                                                           const TZrChar *expression,
                                                           TZrUInt32 allowedEffectFlags,
                                                           TZrChar *errorBuffer,
                                                           TZrSize errorBufferSize);
/** @brief 与普通结果相同，但把失败种类与结构化诊断写入 outFailure。 */
cJSON *zr_debug_protocol_make_evaluate_result_detailed(
                                               ZrDebugAgent *agent,
                                               TZrUInt32 threadId,
                                               TZrUInt32 frameId,
                                               const TZrChar *expression,
                                               TZrUInt32 allowedEffectFlags,
                                               ZrDebugEvaluateFailure *outFailure,
                                               TZrChar *errorBuffer,
                                               TZrSize errorBufferSize);
/** @brief 将失败详情转换为协议 error.data；无失败时返回 NULL，非空结果所有权交给调用方。 */
ZR_DEBUG_API cJSON *zr_debug_protocol_make_evaluate_failure_data(const ZrDebugEvaluateFailure *failure);

#endif // ZR_VM_DEBUG_PROTOCOL_EVALUATE_H
