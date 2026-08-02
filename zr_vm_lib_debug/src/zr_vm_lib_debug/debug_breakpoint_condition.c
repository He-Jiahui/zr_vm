#include "debug_breakpoint_condition.h"

static TZrBool zr_debug_breakpoint_condition_value_truthy(const SZrTypeValue *value) {
    if (value == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (value->type) {
        case ZR_VALUE_TYPE_NULL:
            return ZR_FALSE;
        case ZR_VALUE_TYPE_BOOL:
            return value->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE;
        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64:
        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64:
            return value->value.nativeObject.nativeInt64 != 0 ? ZR_TRUE : ZR_FALSE;
        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            return value->value.nativeObject.nativeDouble != 0.0 ? ZR_TRUE : ZR_FALSE;
        case ZR_VALUE_TYPE_STRING:
            return value->value.object != ZR_NULL ? ZR_TRUE : ZR_FALSE;
        default:
            return value->value.object != ZR_NULL ? ZR_TRUE : ZR_FALSE;
    }
}

TZrBool zr_debug_breakpoint_condition_evaluate(ZrDebugAgent *agent,
                                               const TZrChar *condition,
                                               TZrBool *outSatisfied,
                                               TZrChar *errorBuffer,
                                               TZrSize errorBufferSize) {
    SZrTypeValue value;

    if (outSatisfied != ZR_NULL) {
        *outSatisfied = ZR_FALSE;
    }
    if (errorBuffer != ZR_NULL && errorBufferSize > 0u) {
        errorBuffer[0] = '\0';
    }
    if (agent == ZR_NULL || outSatisfied == ZR_NULL) {
        zr_debug_copy_text(errorBuffer, errorBufferSize, "invalid breakpoint condition request");
        return ZR_FALSE;
    }
    if (condition == ZR_NULL || condition[0] == '\0') {
        *outSatisfied = ZR_TRUE;
        return ZR_TRUE;
    }

    memset(&value, 0, sizeof(value));
    if (!zr_debug_evaluate_expression_with_capabilities(agent,
                                                        1u,
                                                        condition,
                                                        ZR_DEBUG_EVALUATION_EFFECT_NONE,
                                                        ZR_FALSE,
                                                        &value,
                                                        errorBuffer,
                                                        errorBufferSize,
                                                        ZR_NULL,
                                                        0u)) {
        return ZR_FALSE;
    }

    *outSatisfied = zr_debug_breakpoint_condition_value_truthy(&value);
    return ZR_TRUE;
}
