#include "debug_internal.h"

static const TZrChar *zr_debug_reference_scope_label(EZrDebugScopeKind scopeKind) {
    switch (scopeKind) {
        case ZR_DEBUG_SCOPE_KIND_ARGUMENTS:
            return "argument";
        case ZR_DEBUG_SCOPE_KIND_LOCALS:
            return "local";
        case ZR_DEBUG_SCOPE_KIND_CLOSURES:
            return "closure";
        case ZR_DEBUG_SCOPE_KIND_GLOBALS:
            return "global";
        case ZR_DEBUG_SCOPE_KIND_PROTOTYPE:
            return "prototype";
        case ZR_DEBUG_SCOPE_KIND_STATICS:
            return "static";
        case ZR_DEBUG_SCOPE_KIND_EXCEPTION:
            return "exception";
        default:
            return "";
    }
}

void zr_debug_reference_summary_from_scope(EZrDebugScopeKind scopeKind,
                                           const TZrChar *name,
                                           TZrChar *buffer,
                                           TZrSize bufferSize) {
    const TZrChar *label;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }
    buffer[0] = '\0';
    if (name == ZR_NULL || name[0] == '\0') {
        return;
    }

    label = zr_debug_reference_scope_label(scopeKind);
    if (label[0] == '\0') {
        return;
    }

    snprintf(buffer, bufferSize, "%s %s", label, name);
    buffer[bufferSize - 1u] = '\0';
}

TZrBool zr_debug_identifier_reference_summary(ZrDebugAgent *agent,
                                              TZrUInt32 frameId,
                                              const TZrChar *name,
                                              TZrChar *buffer,
                                              TZrSize bufferSize) {
    SZrFunction *function = ZR_NULL;
    SZrCallInfo *callInfo;
    TZrUInt32 pc;
    TZrUInt32 slotIndex;

    if (buffer != ZR_NULL && bufferSize > 0) {
        buffer[0] = '\0';
    }
    if (agent == ZR_NULL || agent->state == ZR_NULL || name == ZR_NULL || name[0] == '\0' ||
        buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    callInfo = zr_debug_find_call_info_by_frame_id(agent, frameId == 0 ? 1u : frameId, &function);
    if (callInfo != ZR_NULL && function != ZR_NULL) {
        if (zr_debug_closure_capture_value(agent, function, callInfo, name) != ZR_NULL) {
            zr_debug_reference_summary_from_scope(ZR_DEBUG_SCOPE_KIND_CLOSURES,
                                                  name,
                                                  buffer,
                                                  bufferSize);
            return buffer[0] != '\0' ? ZR_TRUE : ZR_FALSE;
        }

        pc = zr_debug_instruction_offset(callInfo, function);
        for (slotIndex = 0; slotIndex < function->stackSize; slotIndex++) {
            SZrString *localName = ZrCore_Function_GetLocalVariableName(function, slotIndex, pc);
            if (localName != ZR_NULL && strcmp(zr_debug_string_native(localName), name) == 0) {
                zr_debug_reference_summary_from_scope(slotIndex < function->parameterCount
                                                              ? ZR_DEBUG_SCOPE_KIND_ARGUMENTS
                                                              : ZR_DEBUG_SCOPE_KIND_LOCALS,
                                                      name,
                                                      buffer,
                                                      bufferSize);
                return buffer[0] != '\0' ? ZR_TRUE : ZR_FALSE;
            }
        }

    }

    if (strcmp(name, "zr") == 0 ||
        strcmp(name, "loadedModules") == 0 ||
        (strcmp(name, "error") == 0 && agent->state->hasCurrentException)) {
        zr_debug_reference_summary_from_scope(ZR_DEBUG_SCOPE_KIND_GLOBALS, name, buffer, bufferSize);
        return buffer[0] != '\0' ? ZR_TRUE : ZR_FALSE;
    }

    return ZR_FALSE;
}
