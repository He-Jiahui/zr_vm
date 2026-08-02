#ifndef ZR_VM_DEBUG_PROTOCOL_EVALUATE_H
#define ZR_VM_DEBUG_PROTOCOL_EVALUATE_H

#include "debug_internal.h"

TZrUInt32 zr_debug_protocol_evaluate_allowed_effect_flags(const cJSON *contextItem);
cJSON *zr_debug_protocol_make_evaluate_result(ZrDebugAgent *agent,
                                               TZrUInt32 threadId,
                                               TZrUInt32 frameId,
                                               const TZrChar *expression,
                                               TZrUInt32 allowedEffectFlags,
                                               TZrChar *errorBuffer,
                                               TZrSize errorBufferSize);

#endif // ZR_VM_DEBUG_PROTOCOL_EVALUATE_H
