#include "debug_internal.h"

#include <ctype.h>

void zr_debug_copy_text(TZrChar *buffer, TZrSize bufferSize, const TZrChar *text) {
    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }
    if (text == ZR_NULL) {
        buffer[0] = '\0';
        return;
    }

    snprintf(buffer, bufferSize, "%s", text);
    buffer[bufferSize - 1] = '\0';
}

const TZrChar *zr_debug_string_native(SZrString *value) {
    return value != ZR_NULL ? ZrCore_String_GetNativeString(value) : ZR_NULL;
}

const TZrChar *zr_debug_function_name(SZrFunction *function) {
    const TZrChar *name = function != ZR_NULL ? zr_debug_string_native(function->functionName) : ZR_NULL;
    return name != ZR_NULL && name[0] != '\0' ? name : "<anonymous>";
}

const TZrChar *zr_debug_function_source(SZrFunction *function) {
    const TZrChar *source = function != ZR_NULL ? zr_debug_string_native(function->sourceCodeList) : ZR_NULL;
    return source != ZR_NULL ? source : "";
}

TZrUInt32 zr_debug_instruction_offset(SZrCallInfo *callInfo, SZrFunction *function) {
    if (callInfo == ZR_NULL || function == ZR_NULL || function->instructionsList == ZR_NULL ||
        callInfo->context.context.programCounter == ZR_NULL ||
        callInfo->context.context.programCounter < function->instructionsList) {
        return 0;
    }

    return (TZrUInt32)(callInfo->context.context.programCounter - function->instructionsList);
}

TZrUInt32 zr_debug_frame_depth(SZrState *state) {
    TZrUInt32 depth = 0;
    SZrCallInfo *callInfo;

    if (state == ZR_NULL) {
        return 0;
    }

    callInfo = state->callInfoList;
    while (callInfo != ZR_NULL) {
        if (ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, callInfo) != ZR_NULL) {
            depth++;
        }
        callInfo = callInfo->previous;
    }

    return depth;
}

const TZrChar *zr_debug_stop_reason_name(EZrDebugStopReason reason) {
    switch (reason) {
        case ZR_DEBUG_STOP_REASON_ENTRY:
            return "entry";
        case ZR_DEBUG_STOP_REASON_BREAKPOINT:
            return "breakpoint";
        case ZR_DEBUG_STOP_REASON_PAUSE:
            return "pause";
        case ZR_DEBUG_STOP_REASON_STEP:
            return "step";
        case ZR_DEBUG_STOP_REASON_EXCEPTION:
            return "exception";
        case ZR_DEBUG_STOP_REASON_TERMINATED:
            return "terminated";
        case ZR_DEBUG_STOP_REASON_NONE:
        default:
            return "none";
    }
}

const TZrChar *zr_debug_exception_filter_name(EZrDebugExceptionFilter filter) {
    switch (filter) {
        case ZR_DEBUG_EXCEPTION_FILTER_CAUGHT:
            return "caught";
        case ZR_DEBUG_EXCEPTION_FILTER_UNCAUGHT:
            return "uncaught";
        case ZR_DEBUG_EXCEPTION_FILTER_NONE:
        default:
            return "none";
    }
}

const TZrChar *zr_debug_scope_kind_name(EZrDebugScopeKind kind) {
    switch (kind) {
        case ZR_DEBUG_SCOPE_KIND_ARGUMENTS:
            return "Arguments";
        case ZR_DEBUG_SCOPE_KIND_LOCALS:
            return "Locals";
        case ZR_DEBUG_SCOPE_KIND_CLOSURES:
            return "Closures";
        case ZR_DEBUG_SCOPE_KIND_GLOBALS:
            return "Globals";
        case ZR_DEBUG_SCOPE_KIND_PROTOTYPE:
            return "Prototype";
        case ZR_DEBUG_SCOPE_KIND_STATICS:
            return "Statics";
        case ZR_DEBUG_SCOPE_KIND_EXCEPTION:
            return "Exception";
        default:
            return "Unknown";
    }
}

const TZrChar *zr_debug_value_type_name(EZrValueType type) {
    switch (type) {
        case ZR_VALUE_TYPE_NULL:
            return "null";
        case ZR_VALUE_TYPE_BOOL:
            return "bool";
        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64:
            return "int";
        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64:
            return "uint";
        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            return "float";
        case ZR_VALUE_TYPE_STRING:
            return "string";
        case ZR_VALUE_TYPE_OBJECT:
            return "object";
        case ZR_VALUE_TYPE_ARRAY:
            return "array";
        case ZR_VALUE_TYPE_FUNCTION:
            return "function";
        case ZR_VALUE_TYPE_CLOSURE:
            return "closure";
        case ZR_VALUE_TYPE_NATIVE_POINTER:
            return "native_pointer";
        case ZR_VALUE_TYPE_NATIVE_DATA:
            return "native_data";
        default:
            return "value";
    }
}

TZrUInt32 zr_debug_scope_id(TZrUInt32 frameId, EZrDebugScopeKind kind) {
    return frameId * 10u + (TZrUInt32)kind;
}

static void zr_debug_normalize_path_for_compare(const TZrChar *path, TZrChar *buffer, TZrSize bufferSize) {
    TZrSize writeIndex = 0;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    if (path == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; path[index] != '\0' && writeIndex + 1 < bufferSize; index++) {
        TZrChar current = path[index];
        if (current == '\\') {
            current = '/';
        }
#ifdef ZR_VM_PLATFORM_IS_WIN
        current = (TZrChar)tolower((unsigned char)current);
#endif
        buffer[writeIndex++] = current;
    }

    while (writeIndex > 1 && buffer[writeIndex - 1] == '/') {
        writeIndex--;
    }
    buffer[writeIndex] = '\0';
}

static TZrBool zr_debug_normalized_path_is_module_like(const TZrChar *path) {
    if (path == ZR_NULL || path[0] == '\0') {
        return ZR_FALSE;
    }

    return strchr(path, '/') == ZR_NULL ? ZR_TRUE : ZR_FALSE;
}

static void zr_debug_normalized_path_copy_stem(const TZrChar *path, TZrChar *buffer, TZrSize bufferSize) {
    const TZrChar *nameStart;
    TZrSize length;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    if (path == ZR_NULL || path[0] == '\0') {
        return;
    }

    nameStart = strrchr(path, '/');
    nameStart = nameStart != ZR_NULL ? nameStart + 1 : path;
    length = strlen(nameStart);
    if (length >= 3 && strcmp(nameStart + length - 3, ".zr") == 0) {
        length -= 3;
    } else if (length >= 4 &&
               (strcmp(nameStart + length - 4, ".zri") == 0 || strcmp(nameStart + length - 4, ".zro") == 0)) {
        length -= 4;
    }
    if (length >= bufferSize) {
        length = bufferSize - 1u;
    }

    memcpy(buffer, nameStart, length);
    buffer[length] = '\0';
}

static TZrBool zr_debug_source_paths_equal(const TZrChar *left, const TZrChar *right) {
    TZrChar normalizedLeft[ZR_DEBUG_TEXT_CAPACITY];
    TZrChar normalizedRight[ZR_DEBUG_TEXT_CAPACITY];
    TZrChar leftStem[ZR_DEBUG_NAME_CAPACITY];
    TZrChar rightStem[ZR_DEBUG_NAME_CAPACITY];

    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_debug_normalize_path_for_compare(left, normalizedLeft, sizeof(normalizedLeft));
    zr_debug_normalize_path_for_compare(right, normalizedRight, sizeof(normalizedRight));
    if (strcmp(normalizedLeft, normalizedRight) == 0) {
        return ZR_TRUE;
    }

    if (!zr_debug_normalized_path_is_module_like(normalizedLeft) &&
        !zr_debug_normalized_path_is_module_like(normalizedRight)) {
        return ZR_FALSE;
    }

    zr_debug_normalized_path_copy_stem(normalizedLeft, leftStem, sizeof(leftStem));
    zr_debug_normalized_path_copy_stem(normalizedRight, rightStem, sizeof(rightStem));
    return (TZrBool)(leftStem[0] != '\0' && strcmp(leftStem, rightStem) == 0);
}

static TZrBool zr_debug_breakpoint_matches_module(const ZrDebugAgent *agent, const ZrDebugBreakpoint *breakpoint) {
    if (agent == ZR_NULL || breakpoint == ZR_NULL || breakpoint->module_name[0] == '\0') {
        return ZR_TRUE;
    }

    return strcmp(agent->moduleName, breakpoint->module_name) == 0;
}

static TZrBool zr_debug_breakpoint_matches_function(const ZrDebugBreakpoint *breakpoint, SZrFunction *function) {
    if (breakpoint == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    if (breakpoint->source_file[0] != '\0' &&
        !zr_debug_source_paths_equal(zr_debug_function_source(function), breakpoint->source_file)) {
        return ZR_FALSE;
    }

    if (breakpoint->function_name[0] != '\0' &&
        strcmp(zr_debug_function_name(function), breakpoint->function_name) != 0) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static SZrFunction *zr_debug_function_from_constant(const SZrTypeValue *constant) {
    SZrRawObject *rawObject;

    if (constant == ZR_NULL || constant->value.object == ZR_NULL ||
        (constant->type != ZR_VALUE_TYPE_FUNCTION && constant->type != ZR_VALUE_TYPE_CLOSURE)) {
        return ZR_NULL;
    }

    rawObject = constant->value.object;
    if (rawObject->type == ZR_RAW_OBJECT_TYPE_FUNCTION) {
        return ZR_CAST(SZrFunction *, rawObject);
    }

    if (rawObject->type == ZR_RAW_OBJECT_TYPE_CLOSURE && !constant->isNative) {
        SZrClosure *closure = ZR_CAST(SZrClosure *, rawObject);
        return closure != ZR_NULL ? closure->function : ZR_NULL;
    }

    return ZR_NULL;
}

static TZrUInt32 zr_debug_function_line_span(const SZrFunction *function) {
    if (function == ZR_NULL ||
        function->lineInSourceStart == 0 ||
        function->lineInSourceEnd == 0 ||
        function->lineInSourceEnd < function->lineInSourceStart) {
        return UINT32_MAX;
    }

    return function->lineInSourceEnd - function->lineInSourceStart;
}

static TZrBool zr_debug_line_breakpoint_candidate_is_better(const ZrDebugBreakpoint *current,
                                                            const ZrDebugBreakpoint *candidate,
                                                            const SZrFunction *entryFunction) {
    TZrUInt32 currentSpan;
    TZrUInt32 candidateSpan;

    if (candidate == ZR_NULL || !candidate->resolved || candidate->resolved_function == ZR_NULL) {
        return ZR_FALSE;
    }
    if (current == ZR_NULL || !current->resolved || current->resolved_function == ZR_NULL) {
        return ZR_TRUE;
    }

    if (current->resolved_function == entryFunction && candidate->resolved_function != entryFunction) {
        return ZR_TRUE;
    }
    if (current->resolved_function != entryFunction && candidate->resolved_function == entryFunction) {
        return ZR_FALSE;
    }

    currentSpan = zr_debug_function_line_span(current->resolved_function);
    candidateSpan = zr_debug_function_line_span(candidate->resolved_function);
    if (candidateSpan < currentSpan) {
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool zr_debug_breakpoint_resolve_function_entry(SZrFunction *function,
                                                          TZrUInt32 *outInstructionIndex,
                                                          TZrUInt32 *outLine) {
    TZrUInt32 index;

    if (outInstructionIndex != ZR_NULL) {
        *outInstructionIndex = 0;
    }
    if (outLine != ZR_NULL) {
        *outLine = 0;
    }
    if (function == ZR_NULL) {
        return ZR_FALSE;
    }

    if (function->executionLocationInfoList != ZR_NULL && function->executionLocationInfoLength > 0) {
        for (index = 0; index < function->executionLocationInfoLength; index++) {
            const SZrFunctionExecutionLocationInfo *location = &function->executionLocationInfoList[index];
            if (location->lineInSource == 0 && index + 1u < function->executionLocationInfoLength) {
                continue;
            }
            if (outInstructionIndex != ZR_NULL) {
                *outInstructionIndex = (TZrUInt32)location->currentInstructionOffset;
            }
            if (outLine != ZR_NULL) {
                *outLine = location->lineInSource != 0 ? location->lineInSource : function->lineInSourceStart;
            }
            return ZR_TRUE;
        }
    }

    if (outLine != ZR_NULL) {
        *outLine = function->lineInSourceStart;
    }
    return function->instructionsLength > 0 ? ZR_TRUE : ZR_FALSE;
}

static void zr_debug_resolve_breakpoint_recursive(ZrDebugBreakpoint *breakpoint, SZrFunction *function) {
    TZrUInt32 index;

    if (breakpoint == ZR_NULL || function == ZR_NULL || breakpoint->resolved) {
        return;
    }

    if (breakpoint->kind == ZR_DEBUG_BREAKPOINT_KIND_LINE && function->childFunctionList != ZR_NULL) {
        for (index = 0; index < function->childFunctionLength; index++) {
            zr_debug_resolve_breakpoint_recursive(breakpoint, &function->childFunctionList[index]);
        }
        if (breakpoint->resolved) {
            return;
        }
    }

    if (zr_debug_breakpoint_matches_function(breakpoint, function)) {
        if (breakpoint->kind == ZR_DEBUG_BREAKPOINT_KIND_FUNCTION) {
            TZrUInt32 entryLine = 0;
            if (zr_debug_breakpoint_resolve_function_entry(function,
                                                           &breakpoint->resolved_instruction_index,
                                                           &entryLine)) {
                breakpoint->resolved_function = function;
                breakpoint->resolved = ZR_TRUE;
                if (breakpoint->line == 0) {
                    breakpoint->line = entryLine;
                }
            }
        } else if (breakpoint->line > 0 &&
                   function->executionLocationInfoList != ZR_NULL &&
                   function->executionLocationInfoLength > 0) {
            for (index = 0; index < function->executionLocationInfoLength; index++) {
                if (function->executionLocationInfoList[index].lineInSource == breakpoint->line) {
                    breakpoint->resolved_function = function;
                    breakpoint->resolved_instruction_index =
                            (TZrUInt32)function->executionLocationInfoList[index].currentInstructionOffset;
                    breakpoint->resolved = ZR_TRUE;
                    break;
                }
            }
        }
    }

    if (breakpoint->kind != ZR_DEBUG_BREAKPOINT_KIND_LINE && function->childFunctionList != ZR_NULL) {
        for (index = 0; index < function->childFunctionLength; index++) {
            zr_debug_resolve_breakpoint_recursive(breakpoint, &function->childFunctionList[index]);
        }
    }
}

static void zr_debug_refine_line_breakpoint_with_constant_roots(ZrDebugBreakpoint *breakpoint, SZrFunction *entryFunction) {
    TZrUInt32 constantIndex;

    if (breakpoint == ZR_NULL || entryFunction == ZR_NULL || breakpoint->kind != ZR_DEBUG_BREAKPOINT_KIND_LINE ||
        entryFunction->constantValueList == ZR_NULL || entryFunction->constantValueLength == 0) {
        return;
    }

    for (constantIndex = 0; constantIndex < entryFunction->constantValueLength; constantIndex++) {
        SZrFunction *candidateFunction =
                zr_debug_function_from_constant(&entryFunction->constantValueList[constantIndex]);
        ZrDebugBreakpoint candidate;

        if (candidateFunction == ZR_NULL || candidateFunction == entryFunction) {
            continue;
        }

        candidate = *breakpoint;
        candidate.resolved = ZR_FALSE;
        candidate.resolved_function = ZR_NULL;
        candidate.resolved_instruction_index = 0;
        zr_debug_resolve_breakpoint_recursive(&candidate, candidateFunction);
        if (zr_debug_line_breakpoint_candidate_is_better(breakpoint, &candidate, entryFunction)) {
            *breakpoint = candidate;
        }
    }
}

static TZrBool zr_debug_breakpoint_try_resolve_with_function_root(ZrDebugBreakpoint *breakpoint,
                                                                  SZrFunction *rootFunction) {
    ZrDebugBreakpoint candidate;

    if (breakpoint == ZR_NULL || rootFunction == ZR_NULL || breakpoint->resolved) {
        return ZR_FALSE;
    }

    candidate = *breakpoint;
    candidate.resolved = ZR_FALSE;
    candidate.resolved_function = ZR_NULL;
    candidate.resolved_instruction_index = 0;
    zr_debug_resolve_breakpoint_recursive(&candidate, rootFunction);
    zr_debug_refine_line_breakpoint_with_constant_roots(&candidate, rootFunction);
    if (!candidate.resolved) {
        return ZR_FALSE;
    }

    breakpoint->resolved_function = candidate.resolved_function;
    breakpoint->resolved_instruction_index = candidate.resolved_instruction_index;
    breakpoint->resolved = ZR_TRUE;
    breakpoint->line = candidate.line;
    return ZR_TRUE;
}

static void zr_debug_agent_try_resolve_pending_breakpoints_for_function(ZrDebugAgent *agent, SZrFunction *function) {
    TZrSize index;

    if (agent == ZR_NULL || function == ZR_NULL) {
        return;
    }

    for (index = 0; index < agent->breakpointCount; index++) {
        ZrDebugBreakpoint *breakpoint = &agent->breakpoints[index];

        if (breakpoint->resolved || !zr_debug_breakpoint_matches_module(agent, breakpoint)) {
            continue;
        }

        if (zr_debug_breakpoint_try_resolve_with_function_root(breakpoint, function)) {
            zr_debug_agent_emit_breakpoint_resolved(agent, breakpoint);
        }
    }
}

static TZrBool zr_debug_parse_uint_text(const TZrChar *text, TZrUInt32 *outValue) {
    const TZrChar *cursor = text;
    TZrUInt32 value = 0;

    if (outValue != ZR_NULL) {
        *outValue = 0;
    }
    if (cursor == ZR_NULL || *cursor == '\0') {
        return ZR_FALSE;
    }

    while (*cursor == ' ' || *cursor == '\t') {
        cursor++;
    }
    if (*cursor == '\0') {
        return ZR_FALSE;
    }

    while (*cursor >= '0' && *cursor <= '9') {
        value = value * 10u + (TZrUInt32)(*cursor - '0');
        cursor++;
    }
    while (*cursor == ' ' || *cursor == '\t') {
        cursor++;
    }
    if (*cursor != '\0') {
        return ZR_FALSE;
    }

    if (outValue != ZR_NULL) {
        *outValue = value;
    }
    return ZR_TRUE;
}

static TZrBool zr_debug_hit_condition_satisfied(const TZrChar *expression, TZrUInt32 hitCount) {
    TZrUInt32 threshold = 0;

    if (expression == ZR_NULL || expression[0] == '\0') {
        return ZR_TRUE;
    }

    if (expression[0] == '%' && zr_debug_parse_uint_text(expression + 1, &threshold) && threshold > 0) {
        return (TZrBool)((hitCount % threshold) == 0);
    }
    if (strncmp(expression, ">=", 2) == 0 && zr_debug_parse_uint_text(expression + 2, &threshold)) {
        return (TZrBool)(hitCount >= threshold);
    }
    if (strncmp(expression, "<=", 2) == 0 && zr_debug_parse_uint_text(expression + 2, &threshold)) {
        return (TZrBool)(hitCount <= threshold);
    }
    if (strncmp(expression, "==", 2) == 0 && zr_debug_parse_uint_text(expression + 2, &threshold)) {
        return (TZrBool)(hitCount == threshold);
    }
    if (expression[0] == '>' && zr_debug_parse_uint_text(expression + 1, &threshold)) {
        return (TZrBool)(hitCount > threshold);
    }
    if (expression[0] == '<' && zr_debug_parse_uint_text(expression + 1, &threshold)) {
        return (TZrBool)(hitCount < threshold);
    }
    if (zr_debug_parse_uint_text(expression, &threshold)) {
        return (TZrBool)(hitCount == threshold);
    }

    return ZR_FALSE;
}

static TZrBool zr_debug_value_truthy(const SZrTypeValue *value) {
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

static TZrBool zr_debug_breakpoint_condition_satisfied(ZrDebugAgent *agent, const ZrDebugBreakpoint *breakpoint) {
    SZrTypeValue value;
    TZrChar error[256];

    if (agent == ZR_NULL || breakpoint == ZR_NULL || breakpoint->condition[0] == '\0') {
        return ZR_TRUE;
    }

    memset(&value, 0, sizeof(value));
    if (!zr_debug_evaluate_expression(agent, 1, breakpoint->condition, &value, error, sizeof(error))) {
        zr_debug_agent_emit_output(agent, "stderr", error);
        zr_debug_agent_emit_output(agent, "stderr", "\n");
        return ZR_FALSE;
    }

    return zr_debug_value_truthy(&value);
}

static void zr_debug_append_text_segment(TZrChar *buffer, TZrSize bufferSize, const TZrChar *text) {
    TZrSize length;

    if (buffer == ZR_NULL || bufferSize == 0 || text == ZR_NULL) {
        return;
    }

    length = strlen(buffer);
    if (length >= bufferSize - 1u) {
        return;
    }

    snprintf(buffer + length, bufferSize - length, "%s", text);
    buffer[bufferSize - 1u] = '\0';
}

static void zr_debug_breakpoint_emit_logpoint(ZrDebugAgent *agent, const ZrDebugBreakpoint *breakpoint) {
    TZrChar output[ZR_DEBUG_TEXT_CAPACITY];
    const TZrChar *cursor;

    if (agent == ZR_NULL || breakpoint == ZR_NULL || breakpoint->log_message[0] == '\0') {
        return;
    }

    output[0] = '\0';
    cursor = breakpoint->log_message;
    while (*cursor != '\0') {
        const TZrChar *openBrace = strchr(cursor, '{');
        if (openBrace == ZR_NULL) {
            zr_debug_append_text_segment(output, sizeof(output), cursor);
            break;
        }

        if (openBrace > cursor) {
            TZrChar chunk[ZR_DEBUG_TEXT_CAPACITY];
            TZrSize length = (TZrSize)(openBrace - cursor);
            if (length >= sizeof(chunk)) {
                length = sizeof(chunk) - 1u;
            }
            memcpy(chunk, cursor, length);
            chunk[length] = '\0';
            zr_debug_append_text_segment(output, sizeof(output), chunk);
        }

        {
            const TZrChar *closeBrace = strchr(openBrace + 1, '}');
            if (closeBrace == ZR_NULL) {
                zr_debug_append_text_segment(output, sizeof(output), openBrace);
                break;
            }

            TZrChar expression[ZR_DEBUG_TEXT_CAPACITY];
            SZrTypeValue value;
            TZrChar valueText[ZR_DEBUG_TEXT_CAPACITY];
            TZrChar error[256];
            TZrSize expressionLength = (TZrSize)(closeBrace - (openBrace + 1));

            if (expressionLength >= sizeof(expression)) {
                expressionLength = sizeof(expression) - 1u;
            }
            memcpy(expression, openBrace + 1, expressionLength);
            expression[expressionLength] = '\0';

            memset(&value, 0, sizeof(value));
            if (zr_debug_evaluate_expression(agent, 1, expression, &value, error, sizeof(error))) {
                zr_debug_format_value_text_safe(agent->state, &value, valueText, sizeof(valueText));
                zr_debug_append_text_segment(output, sizeof(output), valueText);
            } else {
                snprintf(valueText, sizeof(valueText), "<error:%.*s>", (int)(sizeof(valueText) - 9u), error);
                zr_debug_append_text_segment(output, sizeof(output), valueText);
            }

            cursor = closeBrace + 1;
        }
    }

    zr_debug_append_text_segment(output, sizeof(output), "\n");
    zr_debug_agent_emit_output(agent, "console", output);
}

static TZrBool zr_debug_agent_should_stop_for_breakpoint(ZrDebugAgent *agent,
                                                         SZrFunction *function,
                                                         TZrUInt32 instructionIndex,
                                                         TZrUInt32 sourceLine) {
    TZrSize index;
    TZrBool shouldStop = ZR_FALSE;

    if (agent == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < agent->breakpointCount; index++) {
        ZrDebugBreakpoint *breakpoint = &agent->breakpoints[index];

        if (!breakpoint->resolved ||
            !zr_debug_breakpoint_matches_module(agent, breakpoint)) {
            continue;
        }
        if (breakpoint->resolved_function != function ||
            breakpoint->resolved_instruction_index != instructionIndex) {
            if (breakpoint->kind != ZR_DEBUG_BREAKPOINT_KIND_LINE ||
                breakpoint->resolved_function == function ||
                breakpoint->line == 0 ||
                breakpoint->line != sourceLine ||
                !zr_debug_breakpoint_matches_function(breakpoint, function)) {
                continue;
            }

            breakpoint->resolved_function = function;
            breakpoint->resolved_instruction_index = instructionIndex;
        }

        breakpoint->hit_count++;
        if (!zr_debug_hit_condition_satisfied(breakpoint->hit_condition, breakpoint->hit_count)) {
            continue;
        }
        if (!zr_debug_breakpoint_condition_satisfied(agent, breakpoint)) {
            continue;
        }
        if (breakpoint->log_message[0] != '\0') {
            zr_debug_breakpoint_emit_logpoint(agent, breakpoint);
            continue;
        }

        shouldStop = ZR_TRUE;
    }

    return shouldStop;
}

static TZrBool zr_debug_agent_should_stop_for_step(ZrDebugAgent *agent,
                                                   SZrFunction *function,
                                                   TZrUInt32 instructionIndex,
                                                   TZrUInt32 sourceLine) {
    TZrUInt32 currentDepth;
    TZrBool advancedPosition;

    if (agent == ZR_NULL) {
        return ZR_FALSE;
    }

    currentDepth = zr_debug_frame_depth(agent->state);
    advancedPosition =
            (TZrBool)(function != agent->resumeFunction ||
                      sourceLine != agent->lastStopEvent.line ||
                      (sourceLine == 0 && instructionIndex != agent->resumeInstruction));
    switch (agent->runMode) {
        case ZR_DEBUG_RUN_MODE_STEP_INTO:
            return advancedPosition;

        case ZR_DEBUG_RUN_MODE_STEP_OVER:
            return (TZrBool)(advancedPosition && currentDepth <= agent->stepDepth);

        case ZR_DEBUG_RUN_MODE_STEP_OUT:
            return (TZrBool)(currentDepth < agent->stepDepth);

        case ZR_DEBUG_RUN_MODE_RUNNING:
        case ZR_DEBUG_RUN_MODE_PAUSED:
        default:
            return ZR_FALSE;
    }
}

static TZrBool zr_debug_agent_should_stop_for_caught_exception(ZrDebugAgent *agent, const TZrInstruction *programCounter) {
    if (agent == ZR_NULL || agent->state == ZR_NULL || !agent->exceptionBreakOnCaught || !agent->state->hasCurrentException ||
        programCounter == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrBool)(programCounter->instruction.operationCode == ZR_INSTRUCTION_ENUM(CATCH));
}

static TZrDebugSignal zr_debug_agent_trace_observer(SZrState *state,
                                                    SZrFunction *function,
                                                    const TZrInstruction *programCounter,
                                                    TZrUInt32 instructionOffset,
                                                    TZrUInt32 sourceLine,
                                                    TZrPtr userData) {
    ZrDebugAgent *agent = (ZrDebugAgent *)userData;
    EZrDebugStopReason reason = ZR_DEBUG_STOP_REASON_NONE;
    EZrDebugExceptionFilter exceptionFilter = ZR_DEBUG_EXCEPTION_FILTER_NONE;

    ZR_UNUSED_PARAMETER(state);

    if (agent == ZR_NULL || function == ZR_NULL) {
        return ZR_DEBUG_SIGNAL_NONE;
    }

    zr_debug_agent_poll_messages(agent, 0, ZR_NULL);
    zr_debug_agent_try_resolve_pending_breakpoints_for_function(agent, function);

    if (zr_debug_bool_load(&agent->exceptionStopPending)) {
        reason = ZR_DEBUG_STOP_REASON_EXCEPTION;
        exceptionFilter = ZR_DEBUG_EXCEPTION_FILTER_UNCAUGHT;
        zr_debug_bool_store(&agent->exceptionStopPending, ZR_FALSE);
    } else if (zr_debug_agent_should_stop_for_caught_exception(agent, programCounter)) {
        reason = ZR_DEBUG_STOP_REASON_EXCEPTION;
        exceptionFilter = ZR_DEBUG_EXCEPTION_FILTER_CAUGHT;
    } else if (zr_debug_bool_load(&agent->pauseRequested)) {
        reason = ZR_DEBUG_STOP_REASON_PAUSE;
        zr_debug_bool_store(&agent->pauseRequested, ZR_FALSE);
    } else if (agent->startStopPending) {
        if (!agent->hasClient && !agent->config.wait_for_client) {
            agent->startStopPending = ZR_FALSE;
            return ZR_DEBUG_SIGNAL_NONE;
        }
        reason = ZR_DEBUG_STOP_REASON_ENTRY;
        agent->startStopPending = ZR_FALSE;
    } else if (zr_debug_agent_should_stop_for_breakpoint(agent, function, instructionOffset, sourceLine)) {
        reason = ZR_DEBUG_STOP_REASON_BREAKPOINT;
    } else if (zr_debug_agent_should_stop_for_step(agent, function, instructionOffset, sourceLine)) {
        reason = ZR_DEBUG_STOP_REASON_STEP;
    }

    if (reason == ZR_DEBUG_STOP_REASON_NONE) {
        return ZR_DEBUG_SIGNAL_NONE;
    }

    zr_debug_agent_fill_stop_event(agent, reason, exceptionFilter, function, instructionOffset, sourceLine);
    zr_debug_agent_pause_loop(agent);
    return ZR_DEBUG_SIGNAL_NONE;
}

TZrBool ZrDebug_AgentStart(SZrState *state,
                           SZrFunction *entryFunction,
                           const TZrChar *moduleName,
                           const ZrDebugAgentConfig *config,
                           ZrDebugAgent **outAgent,
                           TZrChar *errorBuffer,
                           TZrSize errorBufferSize) {
    ZrDebugAgent *agent;
    SZrNetworkEndpoint endpoint;
    ZrDebugAgentConfig effectiveConfig;

    if (outAgent != ZR_NULL) {
        *outAgent = ZR_NULL;
    }
    if (state == ZR_NULL || entryFunction == ZR_NULL || outAgent == ZR_NULL) {
        zr_debug_copy_text(errorBuffer, errorBufferSize, "debug agent requires state, entryFunction, and outAgent");
        return ZR_FALSE;
    }

    memset(&effectiveConfig, 0, sizeof(effectiveConfig));
    if (config != ZR_NULL) {
        effectiveConfig = *config;
    }
    if (effectiveConfig.address == ZR_NULL || effectiveConfig.address[0] == '\0') {
        effectiveConfig.address = "127.0.0.1:0";
    }

    agent = (ZrDebugAgent *)calloc(1, sizeof(*agent));
    if (agent == ZR_NULL) {
        zr_debug_copy_text(errorBuffer, errorBufferSize, "failed to allocate debug agent");
        return ZR_FALSE;
    }

    if (!ZrNetwork_ParseEndpoint(effectiveConfig.address, &endpoint, errorBuffer, errorBufferSize) ||
        !ZrNetwork_ListenerOpenLoopback(&endpoint, &agent->listener, errorBuffer, errorBufferSize) ||
        !ZrNetwork_FormatEndpoint(&agent->listener.endpoint, agent->endpoint, sizeof(agent->endpoint))) {
        ZrDebug_AgentStop(agent);
        return ZR_FALSE;
    }

    agent->state = state;
    agent->entryFunction = entryFunction;
    agent->config = effectiveConfig;
    agent->runMode = ZR_DEBUG_RUN_MODE_RUNNING;
    agent->clientDisconnectContinue = ZR_TRUE;
    agent->waitForClientPending = effectiveConfig.wait_for_client;
    agent->startStopPending = (TZrBool)(effectiveConfig.suspend_on_start || effectiveConfig.wait_for_client);
    agent->exceptionBreakOnCaught = ZR_FALSE;
    agent->exceptionBreakOnUncaught = effectiveConfig.stop_on_uncaught_exception;
    agent->previousDebugHookSignal = state->debugHookSignal;
    zr_debug_copy_text(agent->moduleName, sizeof(agent->moduleName), moduleName != ZR_NULL ? moduleName : "");
    state->debugHookSignal |= ZR_DEBUG_HOOK_MASK_LINE;
    ZrCore_Debug_SetTraceObserver(state, zr_debug_agent_trace_observer, agent);
    *outAgent = agent;
    return ZR_TRUE;
}

void ZrDebug_AgentStop(ZrDebugAgent *agent) {
    if (agent == ZR_NULL) {
        return;
    }

    if (agent->state != ZR_NULL && agent->state->debugTraceUserData == agent) {
        ZrCore_Debug_SetTraceObserver(agent->state, ZR_NULL, ZR_NULL);
        agent->state->debugHookSignal = agent->previousDebugHookSignal;
    }

    zr_debug_agent_close_client(agent);
    ZrNetwork_ListenerClose(&agent->listener);
    if (agent->breakpoints != ZR_NULL) {
        free(agent->breakpoints);
        agent->breakpoints = ZR_NULL;
    }
    zr_debug_variable_handles_clear(agent);
    free(agent);
}

TZrBool ZrDebug_AgentGetEndpoint(ZrDebugAgent *agent, TZrChar *buffer, TZrSize bufferSize) {
    if (agent == ZR_NULL || buffer == ZR_NULL || bufferSize == 0 || agent->endpoint[0] == '\0') {
        return ZR_FALSE;
    }

    zr_debug_copy_text(buffer, bufferSize, agent->endpoint);
    return ZR_TRUE;
}

static void zr_debug_copy_breakpoint_from_existing(ZrDebugBreakpoint *destination, const ZrDebugBreakpoint *source) {
    if (destination == ZR_NULL || source == ZR_NULL) {
        return;
    }

    *destination = *source;
}

static void zr_debug_copy_breakpoint_from_spec(ZrDebugAgent *agent,
                                               ZrDebugBreakpoint *destination,
                                               const ZrDebugBreakpointSpec *spec,
                                               EZrDebugBreakpointKind fallbackKind) {
    if (destination == ZR_NULL || spec == ZR_NULL) {
        return;
    }

    memset(destination, 0, sizeof(*destination));
    destination->kind = spec->kind != 0 ? spec->kind : fallbackKind;
    zr_debug_copy_text(destination->module_name,
                       sizeof(destination->module_name),
                       spec->module_name != ZR_NULL ? spec->module_name : (agent != ZR_NULL ? agent->moduleName : ""));
    zr_debug_copy_text(destination->source_file,
                       sizeof(destination->source_file),
                       spec->source_file != ZR_NULL ? spec->source_file : "");
    zr_debug_copy_text(destination->function_name,
                       sizeof(destination->function_name),
                       spec->function_name != ZR_NULL ? spec->function_name : "");
    zr_debug_copy_text(destination->condition,
                       sizeof(destination->condition),
                       spec->condition != ZR_NULL ? spec->condition : "");
    zr_debug_copy_text(destination->hit_condition,
                       sizeof(destination->hit_condition),
                       spec->hit_condition != ZR_NULL ? spec->hit_condition : "");
    zr_debug_copy_text(destination->log_message,
                       sizeof(destination->log_message),
                       spec->log_message != ZR_NULL ? spec->log_message : "");
    destination->line = spec->line;
}

static void zr_debug_recount_breakpoints(ZrDebugAgent *agent) {
    TZrSize index;

    if (agent == ZR_NULL) {
        return;
    }

    agent->lineBreakpointCount = 0;
    agent->functionBreakpointCount = 0;
    for (index = 0; index < agent->breakpointCount; index++) {
        if (agent->breakpoints[index].kind == ZR_DEBUG_BREAKPOINT_KIND_FUNCTION) {
            agent->functionBreakpointCount++;
        } else {
            agent->lineBreakpointCount++;
        }
    }
}

static TZrBool zr_debug_replace_breakpoints_of_kind(ZrDebugAgent *agent,
                                                    EZrDebugBreakpointKind kind,
                                                    const ZrDebugBreakpointSpec *specs,
                                                    TZrSize count) {
    TZrSize index;
    TZrSize preservedCount = 0;
    TZrSize newCount;
    ZrDebugBreakpoint *breakpoints = ZR_NULL;

    if (agent == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < agent->breakpointCount; index++) {
        if (agent->breakpoints[index].kind != kind) {
            preservedCount++;
        }
    }

    newCount = preservedCount + count;
    if (newCount > 0) {
        breakpoints = (ZrDebugBreakpoint *)calloc(newCount, sizeof(*breakpoints));
        if (breakpoints == ZR_NULL) {
            return ZR_FALSE;
        }
    }

    preservedCount = 0;
    for (index = 0; index < agent->breakpointCount; index++) {
        if (agent->breakpoints[index].kind != kind) {
            zr_debug_copy_breakpoint_from_existing(&breakpoints[preservedCount++], &agent->breakpoints[index]);
        }
    }

    for (index = 0; index < count; index++) {
        zr_debug_copy_breakpoint_from_spec(agent, &breakpoints[preservedCount + index], &specs[index], kind);
        zr_debug_resolve_breakpoint_recursive(&breakpoints[preservedCount + index], agent->entryFunction);
        zr_debug_refine_line_breakpoint_with_constant_roots(&breakpoints[preservedCount + index], agent->entryFunction);
    }

    if (agent->breakpoints != ZR_NULL) {
        free(agent->breakpoints);
    }

    agent->breakpoints = breakpoints;
    agent->breakpointCount = newCount;
    zr_debug_recount_breakpoints(agent);
    return ZR_TRUE;
}

TZrBool ZrDebug_SetBreakpoints(ZrDebugAgent *agent, const ZrDebugBreakpointSpec *specs, TZrSize count) {
    return zr_debug_replace_breakpoints_of_kind(agent, ZR_DEBUG_BREAKPOINT_KIND_LINE, specs, count);
}

TZrBool ZrDebug_SetFunctionBreakpoints(ZrDebugAgent *agent,
                                       const ZrDebugBreakpointSpec *specs,
                                       TZrSize count) {
    return zr_debug_replace_breakpoints_of_kind(agent, ZR_DEBUG_BREAKPOINT_KIND_FUNCTION, specs, count);
}

void ZrDebug_SetExceptionBreakpoints(ZrDebugAgent *agent, TZrBool caught, TZrBool uncaught) {
    if (agent == ZR_NULL) {
        return;
    }

    agent->exceptionBreakOnCaught = caught ? ZR_TRUE : ZR_FALSE;
    agent->exceptionBreakOnUncaught = uncaught ? ZR_TRUE : ZR_FALSE;
}

void ZrDebug_Continue(ZrDebugAgent *agent) {
    if (agent == ZR_NULL) {
        return;
    }

    zr_debug_variable_handles_clear(agent);
    zr_debug_bool_store(&agent->pauseRequested, ZR_FALSE);
    agent->runMode = ZR_DEBUG_RUN_MODE_RUNNING;
}

void ZrDebug_Pause(ZrDebugAgent *agent) {
    if (agent == ZR_NULL) {
        return;
    }

    zr_debug_bool_store(&agent->pauseRequested, ZR_TRUE);
}

static void zr_debug_begin_step(ZrDebugAgent *agent, EZrDebugRunMode runMode) {
    SZrCallInfo *callInfo;
    SZrFunction *function;

    if (agent == ZR_NULL || agent->state == ZR_NULL) {
        return;
    }

    callInfo = agent->state->callInfoList;
    function = ZrCore_Closure_GetMetadataFunctionFromCallInfo(agent->state, callInfo);
    agent->resumeFunction = function;
    agent->resumeInstruction = zr_debug_instruction_offset(callInfo, function);
    agent->stepDepth = zr_debug_frame_depth(agent->state);
    zr_debug_variable_handles_clear(agent);
    zr_debug_bool_store(&agent->pauseRequested, ZR_FALSE);
    agent->runMode = runMode;
}

void ZrDebug_StepInto(ZrDebugAgent *agent) {
    zr_debug_begin_step(agent, ZR_DEBUG_RUN_MODE_STEP_INTO);
}

void ZrDebug_StepOver(ZrDebugAgent *agent) {
    zr_debug_begin_step(agent, ZR_DEBUG_RUN_MODE_STEP_OVER);
}

void ZrDebug_StepOut(ZrDebugAgent *agent) {
    zr_debug_begin_step(agent, ZR_DEBUG_RUN_MODE_STEP_OUT);
}

void ZrDebug_NotifyException(ZrDebugAgent *agent) {
    SZrCallInfo *callInfo;
    SZrFunction *function;
    TZrUInt32 instructionIndex;
    TZrUInt32 sourceLine;
    ZrDebugFrameSnapshot frame;

    if (agent == ZR_NULL || agent->state == ZR_NULL || !agent->exceptionBreakOnUncaught) {
        return;
    }
    if (!agent->hasClient && !agent->config.wait_for_client) {
        return;
    }

    if (zr_debug_exception_read_frame(agent, 1, &frame)) {
        zr_debug_agent_fill_stop_event(agent,
                                       ZR_DEBUG_STOP_REASON_EXCEPTION,
                                       ZR_DEBUG_EXCEPTION_FILTER_UNCAUGHT,
                                       ZR_NULL,
                                       frame.instruction_index,
                                       frame.line);
        zr_debug_copy_text(agent->lastStopEvent.source_file, sizeof(agent->lastStopEvent.source_file), frame.source_file);
        zr_debug_copy_text(agent->lastStopEvent.function_name,
                           sizeof(agent->lastStopEvent.function_name),
                           frame.function_name);
        zr_debug_agent_pause_loop(agent);
        return;
    }

    callInfo = agent->state->callInfoList;
    function = ZrCore_Closure_GetMetadataFunctionFromCallInfo(agent->state, callInfo);
    instructionIndex = zr_debug_instruction_offset(callInfo, function);
    sourceLine = zr_debug_call_info_line(agent, callInfo, function, ZR_TRUE);
    zr_debug_agent_fill_stop_event(agent,
                                   ZR_DEBUG_STOP_REASON_EXCEPTION,
                                   ZR_DEBUG_EXCEPTION_FILTER_UNCAUGHT,
                                   function,
                                   instructionIndex,
                                   sourceLine);
    zr_debug_agent_pause_loop(agent);
}
