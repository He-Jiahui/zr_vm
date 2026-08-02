#include "debug_protocol_evaluate.h"

TZrUInt32 zr_debug_protocol_evaluate_allowed_effect_flags(const cJSON *contextItem) {
    const TZrChar *context;

    if (!cJSON_IsString(contextItem) || contextItem->valuestring == ZR_NULL) {
        return ZR_DEBUG_EVALUATION_EFFECT_NONE;
    }

    context = contextItem->valuestring;
    if (strcmp(context, "watch") == 0) {
        return ZR_DEBUG_EVALUATION_EFFECT_PROPERTY_GETTER;
    }
    if (strcmp(context, "repl") == 0) {
        return ZR_DEBUG_EVALUATION_EFFECT_PROPERTY_GETTER |
               ZR_DEBUG_EVALUATION_EFFECT_ALLOCATION |
               ZR_DEBUG_EVALUATION_EFFECT_CALL |
               ZR_DEBUG_EVALUATION_EFFECT_NATIVE_CALL;
    }
    return ZR_DEBUG_EVALUATION_EFFECT_NONE;
}

cJSON *zr_debug_protocol_make_evaluate_result(ZrDebugAgent *agent,
                                               TZrUInt32 threadId,
                                               TZrUInt32 frameId,
                                               const TZrChar *expression,
                                               TZrUInt32 allowedEffectFlags,
                                               TZrChar *errorBuffer,
                                               TZrSize errorBufferSize) {
    ZrDebugEvaluateResult evaluateResult;
    cJSON *result;
    SZrState *previousState = ZR_NULL;
    TZrUInt32 previousThreadId = 0u;
    TZrUInt32 resolvedThreadId = 0u;

    memset(&evaluateResult, 0, sizeof(evaluateResult));
    if (zr_debug_agent_begin_thread_access(agent,
                                           threadId,
                                           &resolvedThreadId,
                                           &previousState,
                                           &previousThreadId) == ZR_NULL) {
        zr_debug_copy_text(errorBuffer, errorBufferSize, "unknown threadId");
        return ZR_NULL;
    }
    if (!ZrDebug_EvaluateWithCapabilities(agent,
                                           frameId,
                                           expression,
                                           allowedEffectFlags,
                                           &evaluateResult,
                                           errorBuffer,
                                           errorBufferSize)) {
        zr_debug_agent_end_thread_access(agent, previousState, previousThreadId);
        return ZR_NULL;
    }

    result = cJSON_CreateObject();
    if (result == ZR_NULL) {
        zr_debug_agent_end_thread_access(agent, previousState, previousThreadId);
        zr_debug_copy_text(errorBuffer, errorBufferSize, "failed to allocate evaluate result");
        return ZR_NULL;
    }

    cJSON_AddNumberToObject(result, "threadId", resolvedThreadId);
    cJSON_AddNumberToObject(result, "stateId", (double)evaluateResult.state_id);
    cJSON_AddBoolToObject(result, "hasCanonicalType", evaluateResult.has_canonical_type ? 1 : 0);
    if (evaluateResult.has_canonical_type) {
        cJSON_AddNumberToObject(result, "canonicalTypeId", (double)evaluateResult.canonical_type_id);
    }
    cJSON_AddStringToObject(result, "type", evaluateResult.type_name);
    cJSON_AddStringToObject(result, "value", evaluateResult.value_text);
    cJSON_AddStringToObject(result, "semanticSummary", evaluateResult.semantic_summary);
    cJSON_AddStringToObject(result, "referenceSummary", evaluateResult.reference_summary);
    cJSON_AddNumberToObject(result, "variablesReference", evaluateResult.variables_reference);
    cJSON_AddNumberToObject(result, "namedVariables", (double)evaluateResult.named_variables);
    cJSON_AddNumberToObject(result, "indexedVariables", (double)evaluateResult.indexed_variables);
    zr_debug_agent_end_thread_access(agent, previousState, previousThreadId);
    return result;
}
