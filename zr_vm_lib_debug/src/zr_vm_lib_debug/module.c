//
// Built-in script-level debug native module.
//

#include "zr_vm_lib_debug/module.h"

#include "zr_vm_core/closure.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

#include <stdlib.h>
#include <string.h>

#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

#define ZR_DEBUG_SCRIPT_TRACEBACK_BUFFER_SIZE 8192U
#define ZR_DEBUG_SCRIPT_HOOK_MASK_TEXT_CAPACITY 8U

typedef struct ZrDebugScriptHookRecord {
    SZrState *state;
    SZrTypeValue hookValue;
    TZrChar maskText[ZR_DEBUG_SCRIPT_HOOK_MASK_TEXT_CAPACITY];
    TZrUInt32 count;
    struct ZrDebugScriptHookRecord *next;
} ZrDebugScriptHookRecord;

static TZrBool debug_traceback_callback(ZrLibCallContext *context, SZrTypeValue *result);
static TZrBool debug_getinfo_callback(ZrLibCallContext *context, SZrTypeValue *result);
static TZrBool debug_getlocal_callback(ZrLibCallContext *context, SZrTypeValue *result);
static TZrBool debug_setlocal_callback(ZrLibCallContext *context, SZrTypeValue *result);
static TZrBool debug_getupvalue_callback(ZrLibCallContext *context, SZrTypeValue *result);
static TZrBool debug_setupvalue_callback(ZrLibCallContext *context, SZrTypeValue *result);
static TZrBool debug_upvalueid_callback(ZrLibCallContext *context, SZrTypeValue *result);
static TZrBool debug_sethook_callback(ZrLibCallContext *context, SZrTypeValue *result);
static TZrBool debug_gethook_callback(ZrLibCallContext *context, SZrTypeValue *result);
static TZrBool debug_module_on_materialize(SZrState *state,
                                           SZrObjectModule *module,
                                           const ZrLibModuleDescriptor *descriptor);

static const ZrLibModuleDescriptor g_debug_module_descriptor;
static const ZrLibModuleDescriptor g_debug_sandboxed_module_descriptor;
static ZrDebugScriptHookRecord *g_debug_hook_records = ZR_NULL;

static const ZrLibFunctionDescriptor g_debug_functions[] = {
        {
                .name = "traceback",
                .minArgumentCount = 0,
                .maxArgumentCount = 2,
                .callback = debug_traceback_callback,
                .returnTypeName = "string",
                .documentation = "Return a formatted traceback for the active script stack.",
        },
        {
                .name = "getinfo",
                .minArgumentCount = 1,
                .maxArgumentCount = 2,
                .callback = debug_getinfo_callback,
                .returnTypeName = "object",
                .documentation = "Return debug information for a stack level or callable.",
        },
        {
                .name = "getlocal",
                .minArgumentCount = 2,
                .maxArgumentCount = 2,
                .callback = debug_getlocal_callback,
                .returnTypeName = "object",
                .documentation = "Return a local variable name and value from an active stack level.",
        },
        {
                .name = "setlocal",
                .minArgumentCount = 3,
                .maxArgumentCount = 3,
                .callback = debug_setlocal_callback,
                .returnTypeName = "string",
                .documentation = "Replace a local variable in an active stack level.",
        },
        {
                .name = "getupvalue",
                .minArgumentCount = 2,
                .maxArgumentCount = 2,
                .callback = debug_getupvalue_callback,
                .returnTypeName = "object",
                .documentation = "Return a closure upvalue name and value.",
        },
        {
                .name = "setupvalue",
                .minArgumentCount = 3,
                .maxArgumentCount = 3,
                .callback = debug_setupvalue_callback,
                .returnTypeName = "string",
                .documentation = "Replace a closure upvalue.",
        },
        {
                .name = "upvalueid",
                .minArgumentCount = 2,
                .maxArgumentCount = 2,
                .callback = debug_upvalueid_callback,
                .returnTypeName = "nativePointer",
                .documentation = "Return the stable cell identity for a closure upvalue.",
        },
        {
                .name = "sethook",
                .minArgumentCount = 0,
                .maxArgumentCount = 3,
                .callback = debug_sethook_callback,
                .returnTypeName = "null",
                .documentation = "Install or clear a script debug hook.",
        },
        {
                .name = "gethook",
                .minArgumentCount = 0,
                .maxArgumentCount = 0,
                .callback = debug_gethook_callback,
                .returnTypeName = "object",
                .documentation = "Return the current script debug hook state.",
        },
};

static const TZrChar g_debug_type_hints_json[] =
        "{\n"
        "  \"schema\": \"zr.native.hints/v1\",\n"
        "  \"module\": \"debug\",\n"
        "  \"functions\": [\n"
        "    {\"name\":\"traceback\",\"signature\":\"traceback(msg: string = null, level: int = 1): string\"},\n"
        "    {\"name\":\"getinfo\",\"signature\":\"getinfo(levelOrFunction, what: string = \\\"nSlu\\\"): object\"},\n"
        "    {\"name\":\"getlocal\",\"signature\":\"getlocal(level: int, index: int): object\"},\n"
        "    {\"name\":\"setlocal\",\"signature\":\"setlocal(level: int, index: int, value): string\"},\n"
        "    {\"name\":\"getupvalue\",\"signature\":\"getupvalue(func: function, index: int): object\"},\n"
        "    {\"name\":\"setupvalue\",\"signature\":\"setupvalue(func: function, index: int, value): string\"},\n"
        "    {\"name\":\"upvalueid\",\"signature\":\"upvalueid(func: function, index: int): nativePointer\"},\n"
        "    {\"name\":\"sethook\",\"signature\":\"sethook(hook: function = null, mask: string|int = \\\"\\\", count: int = 0): null\"},\n"
        "    {\"name\":\"gethook\",\"signature\":\"gethook(): object\"}\n"
        "  ]\n"
        "}\n";

static const ZrLibModuleDescriptor g_debug_module_descriptor = {
        .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
        .moduleName = "debug",
        .constants = ZR_NULL,
        .constantCount = 0,
        .functions = g_debug_functions,
        .functionCount = ZR_ARRAY_COUNT(g_debug_functions),
        .types = ZR_NULL,
        .typeCount = 0,
        .typeHints = ZR_NULL,
        .typeHintCount = 0,
        .typeHintsJson = g_debug_type_hints_json,
        .documentation = "Trusted script-level debug library.",
        .moduleLinks = ZR_NULL,
        .moduleLinkCount = 0,
        .moduleVersion = "1.0.0",
        .minRuntimeAbi = ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
        .requiredCapabilities = 0,
        .onMaterialize = debug_module_on_materialize,
};

static const ZrLibModuleDescriptor g_debug_sandboxed_module_descriptor = {
        .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
        .moduleName = "debug",
        .constants = ZR_NULL,
        .constantCount = 0,
        .functions = g_debug_functions,
        .functionCount = ZR_ARRAY_COUNT(g_debug_functions),
        .types = ZR_NULL,
        .typeCount = 0,
        .typeHints = ZR_NULL,
        .typeHintCount = 0,
        .typeHintsJson = g_debug_type_hints_json,
        .documentation = "Read-only script-level debug library for sandboxed hosts.",
        .moduleLinks = ZR_NULL,
        .moduleLinkCount = 0,
        .moduleVersion = "1.0.0",
        .minRuntimeAbi = ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
        .requiredCapabilities = 0,
        .onMaterialize = debug_module_on_materialize,
};

static void debug_write_int_field(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrInt64 value) {
    SZrTypeValue fieldValue;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return;
    }

    ZrLib_Value_SetInt(state, &fieldValue, value);
    ZrLib_Object_SetFieldCString(state, object, fieldName, &fieldValue);
}

static void debug_write_bool_field(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrBool value) {
    SZrTypeValue fieldValue;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return;
    }

    ZrLib_Value_SetBool(state, &fieldValue, value);
    ZrLib_Object_SetFieldCString(state, object, fieldName, &fieldValue);
}

static void debug_write_string_field(SZrState *state,
                                     SZrObject *object,
                                     const TZrChar *fieldName,
                                     const TZrChar *value) {
    SZrTypeValue fieldValue;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || value == ZR_NULL) {
        return;
    }

    ZrLib_Value_SetString(state, &fieldValue, value);
    ZrLib_Object_SetFieldCString(state, object, fieldName, &fieldValue);
}

static void debug_write_value_field(SZrState *state,
                                    SZrObject *object,
                                    const TZrChar *fieldName,
                                    const SZrTypeValue *value) {
    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || value == ZR_NULL) {
        return;
    }

    ZrLib_Object_SetFieldCString(state, object, fieldName, value);
}

static TZrUInt32 debug_script_level_to_core(TZrInt64 level) {
    if (level <= 1) {
        return 0u;
    }
    return (TZrUInt32)(level - 1);
}

static TZrBool debug_is_null_value(const SZrTypeValue *value) {
    return (TZrBool)(value == ZR_NULL || value->type == ZR_VALUE_TYPE_NULL);
}

static const TZrChar *debug_string_text(SZrState *state, SZrString *stringObject) {
    ZR_UNUSED_PARAMETER(state);
    return stringObject != ZR_NULL ? ZrCore_String_GetNativeString(stringObject) : ZR_NULL;
}

static const TZrChar *debug_namewhat_text(EZrDebugNameWhat nameWhat) {
    switch (nameWhat) {
        case ZR_DEBUG_NAMEWHAT_GLOBAL:
            return "global";
        case ZR_DEBUG_NAMEWHAT_LOCAL:
            return "local";
        case ZR_DEBUG_NAMEWHAT_FIELD:
            return "field";
        case ZR_DEBUG_NAMEWHAT_METHOD:
            return "method";
        case ZR_DEBUG_NAMEWHAT_UPVALUE:
            return "upvalue";
        case ZR_DEBUG_NAMEWHAT_UNKNOWN:
        default:
            return "";
    }
}

static EZrDebugInfoType debug_parse_what(const TZrChar *whatText) {
    EZrDebugInfoType type = 0;
    const TZrChar *cursor;

    if (whatText == ZR_NULL || whatText[0] == '\0') {
        return (EZrDebugInfoType)(ZR_DEBUG_INFO_FUNCTION_NAME |
                                  ZR_DEBUG_INFO_SOURCE_FILE |
                                  ZR_DEBUG_INFO_LINE_NUMBER |
                                  ZR_DEBUG_INFO_CLOSURE);
    }

    for (cursor = whatText; *cursor != '\0'; cursor++) {
        switch (*cursor) {
            case 'n':
                type = (EZrDebugInfoType)(type | ZR_DEBUG_INFO_FUNCTION_NAME);
                break;
            case 'S':
                type = (EZrDebugInfoType)(type | ZR_DEBUG_INFO_SOURCE_FILE);
                break;
            case 'l':
                type = (EZrDebugInfoType)(type | ZR_DEBUG_INFO_LINE_NUMBER);
                break;
            case 'u':
                type = (EZrDebugInfoType)(type | ZR_DEBUG_INFO_CLOSURE);
                break;
            case 't':
                type = (EZrDebugInfoType)(type | ZR_DEBUG_INFO_TAIL_CALL);
                break;
            case 'r':
                type = (EZrDebugInfoType)(type | ZR_DEBUG_INFO_RETURN_VALUE);
                break;
            case 'f':
                type = (EZrDebugInfoType)(type | ZR_DEBUG_INFO_PUSH_FUNCTION);
                break;
            default:
                break;
        }
    }

    return type;
}

static void debug_mask_to_text(TZrUInt32 mask, TZrChar *buffer, TZrSize bufferSize) {
    TZrSize offset = 0;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    if ((mask & ZR_DEBUG_HOOK_MASK_CALL) != 0 && offset + 1u < bufferSize) {
        buffer[offset++] = 'c';
    }
    if ((mask & ZR_DEBUG_HOOK_MASK_RETURN) != 0 && offset + 1u < bufferSize) {
        buffer[offset++] = 'r';
    }
    if ((mask & ZR_DEBUG_HOOK_MASK_LINE) != 0 && offset + 1u < bufferSize) {
        buffer[offset++] = 'l';
    }
    buffer[offset] = '\0';
}

static TZrUInt32 debug_parse_hook_mask_text(const TZrChar *maskText) {
    TZrUInt32 mask = 0u;
    const TZrChar *cursor;

    if (maskText == ZR_NULL) {
        return 0u;
    }

    for (cursor = maskText; *cursor != '\0'; cursor++) {
        switch (*cursor) {
            case 'c':
                mask |= ZR_DEBUG_HOOK_MASK_CALL;
                break;
            case 'r':
                mask |= ZR_DEBUG_HOOK_MASK_RETURN;
                break;
            case 'l':
                mask |= ZR_DEBUG_HOOK_MASK_LINE;
                break;
            default:
                break;
        }
    }

    return mask;
}

static TZrBool debug_context_allows_writes(const ZrLibCallContext *context) {
    return (TZrBool)(context != ZR_NULL && context->moduleDescriptor == &g_debug_module_descriptor);
}

static TZrBool debug_require_write_api(const ZrLibCallContext *context) {
    if (debug_context_allows_writes(context)) {
        return ZR_TRUE;
    }

    ZrCore_Debug_RunError(context != ZR_NULL ? context->state : ZR_NULL, "debug write API is disabled");
    return ZR_FALSE;
}

static ZrDebugScriptHookRecord *debug_find_hook_record(SZrState *state) {
    ZrDebugScriptHookRecord *record;

    for (record = g_debug_hook_records; record != ZR_NULL; record = record->next) {
        if (record->state == state) {
            return record;
        }
    }

    return ZR_NULL;
}

static ZrDebugScriptHookRecord *debug_get_or_create_hook_record(SZrState *state) {
    ZrDebugScriptHookRecord *record;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    record = debug_find_hook_record(state);
    if (record != ZR_NULL) {
        return record;
    }

    record = (ZrDebugScriptHookRecord *)calloc(1, sizeof(*record));
    if (record == ZR_NULL) {
        return ZR_NULL;
    }

    record->state = state;
    ZrCore_Value_ResetAsNull(&record->hookValue);
    record->maskText[0] = '\0';
    record->next = g_debug_hook_records;
    g_debug_hook_records = record;
    return record;
}

static void debug_store_module_hook_value(SZrState *state, const SZrTypeValue *hookValue) {
    SZrObjectModule *module;
    SZrTypeValue nullValue;

    if (state == ZR_NULL) {
        return;
    }

    module = ZrLib_Module_GetLoaded(state, "debug");
    if (module == ZR_NULL) {
        return;
    }

    if (hookValue != ZR_NULL) {
        ZrLib_Object_SetFieldCString(state, &module->super, "__hook", hookValue);
        return;
    }

    ZrLib_Value_SetNull(&nullValue);
    ZrLib_Object_SetFieldCString(state, &module->super, "__hook", &nullValue);
}

static void debug_clear_script_hook(SZrState *state) {
    ZrDebugScriptHookRecord *record = debug_get_or_create_hook_record(state);

    if (state == ZR_NULL) {
        return;
    }

    ZrCore_Debug_SetHook(state, ZR_NULL, 0u, 0u);
    if (record != ZR_NULL) {
        ZrCore_Value_ResetAsNull(&record->hookValue);
        record->maskText[0] = '\0';
        record->count = 0u;
    }
    debug_store_module_hook_value(state, ZR_NULL);
}

static const TZrChar *debug_hook_event_name(EZrDebugHookEvent event) {
    switch (event) {
        case ZR_DEBUG_HOOK_EVENT_CALL:
            return "call";
        case ZR_DEBUG_HOOK_EVENT_RETURN:
            return "return";
        case ZR_DEBUG_HOOK_EVENT_LINE:
            return "line";
        case ZR_DEBUG_HOOK_EVENT_COUNT:
            return "count";
        case ZR_DEBUG_HOOK_EVENT_MAX:
        default:
            return "unknown";
    }
}

static void debug_script_hook_trampoline(SZrState *state, SZrDebugInfo *debugInfo) {
    ZrDebugScriptHookRecord *record;
    SZrTypeValue arguments[2];
    SZrTypeValue ignoredResult;

    if (state == ZR_NULL || debugInfo == ZR_NULL) {
        return;
    }

    record = debug_find_hook_record(state);
    if (record == ZR_NULL || debug_is_null_value(&record->hookValue)) {
        return;
    }

    ZrLib_Value_SetString(state, &arguments[0], debug_hook_event_name(debugInfo->event));
    ZrLib_Value_SetInt(state, &arguments[1], (TZrInt64)debugInfo->currentLine);
    ZrLib_Value_SetNull(&ignoredResult);
    (void)ZrLib_CallValue(state, &record->hookValue, ZR_NULL, arguments, 2, &ignoredResult);
}

static SZrClosure *debug_read_vm_closure_argument(const ZrLibCallContext *context, TZrSize index) {
    SZrTypeValue *value = ZR_NULL;

    if (!ZrLib_CallContext_ReadFunction(context, index, &value) || value == ZR_NULL ||
        value->type != ZR_VALUE_TYPE_CLOSURE || value->isNative || value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_VM_CLOSURE(context->state, value->value.object);
}

static SZrObject *debug_make_named_value_object(SZrState *state,
                                                const TZrChar *name,
                                                const SZrTypeValue *value) {
    SZrObject *object;

    if (state == ZR_NULL || name == ZR_NULL || value == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZrLib_Object_New(state);
    if (object == ZR_NULL) {
        return ZR_NULL;
    }

    debug_write_string_field(state, object, "name", name);
    debug_write_value_field(state, object, "value", value);
    return object;
}

static SZrObject *debug_make_stack_info_object(SZrState *state,
                                               const SZrDebugInfo *info,
                                               EZrDebugInfoType type,
                                               const SZrTypeValue *functionValue) {
    SZrObject *object;

    if (state == ZR_NULL || info == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZrLib_Object_New(state);
    if (object == ZR_NULL) {
        return ZR_NULL;
    }

    if ((type & ZR_DEBUG_INFO_FUNCTION_NAME) != 0) {
        if (info->name != ZR_NULL) {
            debug_write_string_field(state, object, "name", info->name);
        }
        debug_write_string_field(state, object, "namewhat", debug_namewhat_text(info->nameWhat));
    }
    if ((type & ZR_DEBUG_INFO_SOURCE_FILE) != 0) {
        if (info->source != ZR_NULL) {
            debug_write_string_field(state, object, "source", info->source);
            debug_write_string_field(state, object, "short_src", info->source);
        }
        debug_write_int_field(state, object, "linedefined", (TZrInt64)info->definedLineStart);
        debug_write_int_field(state, object, "lastlinedefined", (TZrInt64)info->definedLineEnd);
        debug_write_bool_field(state, object, "isnative", info->isNative);
        debug_write_string_field(state, object, "what", info->isNative ? "native" : "script");
    }
    if ((type & ZR_DEBUG_INFO_LINE_NUMBER) != 0) {
        debug_write_int_field(state, object, "currentline", (TZrInt64)info->currentLine);
    }
    if ((type & ZR_DEBUG_INFO_CLOSURE) != 0) {
        debug_write_int_field(state, object, "nups", (TZrInt64)info->closureValuesCount);
        debug_write_int_field(state, object, "nparams", (TZrInt64)info->parametersCount);
        debug_write_bool_field(state, object, "isvararg", info->hasVariableParameters);
    }
    if ((type & ZR_DEBUG_INFO_TAIL_CALL) != 0) {
        debug_write_bool_field(state, object, "istailcall", info->isTailCall);
    }
    if ((type & ZR_DEBUG_INFO_RETURN_VALUE) != 0) {
        debug_write_int_field(state, object, "transferstart", (TZrInt64)info->transferStart);
        debug_write_int_field(state, object, "transfercount", (TZrInt64)info->transferCount);
    }
    if ((type & ZR_DEBUG_INFO_PUSH_FUNCTION) != 0 && functionValue != ZR_NULL) {
        debug_write_value_field(state, object, "func", functionValue);
    }

    return object;
}

static SZrObject *debug_make_function_info_object(SZrState *state,
                                                  const SZrTypeValue *functionValue,
                                                  SZrFunction *function,
                                                  EZrDebugInfoType type) {
    SZrDebugInfo info;
    const TZrChar *name;
    const TZrChar *source;

    if (state == ZR_NULL || function == ZR_NULL) {
        return ZR_NULL;
    }

    memset(&info, 0, sizeof(info));
    name = debug_string_text(state, function->functionName);
    source = debug_string_text(state, function->sourceCodeList);
    info.name = (TZrNativeString)name;
    info.nameWhat = ZR_DEBUG_NAMEWHAT_UNKNOWN;
    info.scope = ZR_DEBUG_SCOPE_FUNCTION;
    info.source = (TZrNativeString)source;
    info.sourceLength = source != ZR_NULL ? strlen(source) : 0u;
    info.definedLineStart = function->lineInSourceStart;
    info.definedLineEnd = function->lineInSourceEnd;
    info.closureValuesCount = function->closureValueLength;
    info.parametersCount = function->parameterCount;
    info.hasVariableParameters = function->hasVariableArguments;
    info.isNative = functionValue != ZR_NULL ? functionValue->isNative : ZR_FALSE;
    return debug_make_stack_info_object(state, &info, type, functionValue);
}

static TZrBool debug_read_optional_string_argument(const ZrLibCallContext *context,
                                                   TZrSize index,
                                                   const TZrChar **outText) {
    SZrTypeValue *value;
    SZrString *stringValue = ZR_NULL;

    if (outText == ZR_NULL) {
        return ZR_FALSE;
    }

    *outText = ZR_NULL;
    value = ZrLib_CallContext_Argument(context, index);
    if (debug_is_null_value(value)) {
        return ZR_TRUE;
    }

    if (!ZrLib_CallContext_ReadString(context, index, &stringValue) || stringValue == ZR_NULL) {
        return ZR_FALSE;
    }

    *outText = ZrCore_String_GetNativeString(stringValue);
    return ZR_TRUE;
}

static TZrBool debug_traceback_callback(ZrLibCallContext *context, SZrTypeValue *result) {
    const TZrChar *prefix = ZR_NULL;
    TZrInt64 level = 1;
    TZrChar buffer[ZR_DEBUG_SCRIPT_TRACEBACK_BUFFER_SIZE];

    if (context == ZR_NULL || result == ZR_NULL || context->state == ZR_NULL ||
        !ZrLib_CallContext_CheckArity(context, 0, 2)) {
        return ZR_FALSE;
    }

    if (!debug_read_optional_string_argument(context, 0, &prefix)) {
        return ZR_FALSE;
    }
    if (context->argumentCount > 1 && !ZrLib_CallContext_ReadInt(context, 1, &level)) {
        return ZR_FALSE;
    }

    memset(buffer, 0, sizeof(buffer));
    (void)ZrCore_Debug_Traceback(context->state,
                                 (TZrNativeString)prefix,
                                 debug_script_level_to_core(level),
                                 0u,
                                 buffer,
                                 sizeof(buffer));
    ZrLib_Value_SetString(context->state, result, buffer);
    return ZR_TRUE;
}

static TZrBool debug_getinfo_callback(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrTypeValue *target;
    const TZrChar *whatText = ZR_NULL;
    EZrDebugInfoType type;
    SZrObject *object;

    if (context == ZR_NULL || result == ZR_NULL || context->state == ZR_NULL ||
        !ZrLib_CallContext_CheckArity(context, 1, 2)) {
        return ZR_FALSE;
    }

    target = ZrLib_CallContext_Argument(context, 0);
    if (target == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!debug_read_optional_string_argument(context, 1, &whatText)) {
        return ZR_FALSE;
    }
    type = debug_parse_what(whatText);

    if (ZR_VALUE_IS_TYPE_INT(target->type) || target->type == ZR_VALUE_TYPE_FLOAT) {
        TZrInt64 level = 0;
        SZrDebugActivation activation;
        SZrDebugInfo info;
        SZrTypeValue functionValue;
        TZrBool hasFunctionValue = ZR_FALSE;
        TZrStackValuePointer savedStackTop = context->state->stackTop.valuePointer;

        if (!ZrLib_CallContext_ReadInt(context, 0, &level)) {
            return ZR_FALSE;
        }

        if (!ZrCore_Debug_GetStack(context->state, debug_script_level_to_core(level), &activation)) {
            ZrLib_Value_SetNull(result);
            return ZR_TRUE;
        }

        ZrCore_Value_ResetAsNull(&functionValue);
        if (!ZrCore_Debug_GetInfo(context->state, &activation, type, &info)) {
            context->state->stackTop.valuePointer = savedStackTop;
            ZrLib_Value_SetNull(result);
            return ZR_TRUE;
        }
        if ((type & ZR_DEBUG_INFO_PUSH_FUNCTION) != 0 &&
            context->state->stackTop.valuePointer > savedStackTop) {
            ZrCore_Value_Copy(context->state, &functionValue, ZrCore_Stack_GetValue(savedStackTop));
            hasFunctionValue = ZR_TRUE;
            context->state->stackTop.valuePointer = savedStackTop;
        }

        object = debug_make_stack_info_object(context->state, &info, type, hasFunctionValue ? &functionValue : ZR_NULL);
        if (object == ZR_NULL) {
            return ZR_FALSE;
        }
        ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
        return ZR_TRUE;
    }

    {
        SZrFunction *function = ZrCore_Closure_GetMetadataFunctionFromValue(context->state, target);
        if (function == ZR_NULL) {
            ZrLib_Value_SetNull(result);
            return ZR_TRUE;
        }

        object = debug_make_function_info_object(context->state, target, function, type);
        if (object == ZR_NULL) {
            return ZR_FALSE;
        }
        ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
        return ZR_TRUE;
    }
}

static TZrBool debug_getlocal_callback(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrInt64 level = 0;
    TZrInt64 index = 0;
    SZrDebugActivation activation;
    SZrTypeValue value;
    TZrNativeString name;
    SZrObject *object;

    if (context == ZR_NULL || result == ZR_NULL || context->state == ZR_NULL ||
        !ZrLib_CallContext_CheckArity(context, 2, 2) ||
        !ZrLib_CallContext_ReadInt(context, 0, &level) ||
        !ZrLib_CallContext_ReadInt(context, 1, &index)) {
        return ZR_FALSE;
    }

    if (!ZrCore_Debug_GetStack(context->state, debug_script_level_to_core(level), &activation)) {
        ZrLib_Value_SetNull(result);
        return ZR_TRUE;
    }

    ZrCore_Value_ResetAsNull(&value);
    name = ZrCore_Debug_GetLocal(context->state, &activation, (TZrInt32)index, &value);
    object = debug_make_named_value_object(context->state, name, &value);
    if (object == ZR_NULL) {
        ZrLib_Value_SetNull(result);
        return ZR_TRUE;
    }

    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

static TZrBool debug_setlocal_callback(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrInt64 level = 0;
    TZrInt64 index = 0;
    SZrTypeValue *value;
    SZrDebugActivation activation;
    TZrNativeString name;

    if (!debug_require_write_api(context)) {
        return ZR_FALSE;
    }
    if (context == ZR_NULL || result == ZR_NULL || context->state == ZR_NULL ||
        !ZrLib_CallContext_CheckArity(context, 3, 3) ||
        !ZrLib_CallContext_ReadInt(context, 0, &level) ||
        !ZrLib_CallContext_ReadInt(context, 1, &index)) {
        return ZR_FALSE;
    }

    value = ZrLib_CallContext_Argument(context, 2);
    if (value == ZR_NULL || !ZrCore_Debug_GetStack(context->state, debug_script_level_to_core(level), &activation)) {
        ZrLib_Value_SetNull(result);
        return ZR_TRUE;
    }

    name = ZrCore_Debug_SetLocal(context->state, &activation, (TZrInt32)index, value);
    if (name == ZR_NULL) {
        ZrLib_Value_SetNull(result);
        return ZR_TRUE;
    }

    ZrLib_Value_SetString(context->state, result, name);
    return ZR_TRUE;
}

static TZrBool debug_getupvalue_callback(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrClosure *closure;
    TZrInt64 index = 0;
    SZrTypeValue value;
    TZrNativeString name;
    SZrObject *object;

    if (context == ZR_NULL || result == ZR_NULL || context->state == ZR_NULL ||
        !ZrLib_CallContext_CheckArity(context, 2, 2)) {
        return ZR_FALSE;
    }

    closure = debug_read_vm_closure_argument(context, 0);
    if (!ZrLib_CallContext_ReadInt(context, 1, &index) || closure == ZR_NULL) {
        ZrLib_Value_SetNull(result);
        return ZR_TRUE;
    }

    ZrCore_Value_ResetAsNull(&value);
    name = ZrCore_Debug_GetUpvalue(context->state, closure, (TZrInt32)index, &value);
    object = debug_make_named_value_object(context->state, name, &value);
    if (object == ZR_NULL) {
        ZrLib_Value_SetNull(result);
        return ZR_TRUE;
    }

    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

static TZrBool debug_setupvalue_callback(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrClosure *closure;
    TZrInt64 index = 0;
    SZrTypeValue *value;
    TZrNativeString name;

    if (!debug_require_write_api(context)) {
        return ZR_FALSE;
    }
    if (context == ZR_NULL || result == ZR_NULL || context->state == ZR_NULL ||
        !ZrLib_CallContext_CheckArity(context, 3, 3)) {
        return ZR_FALSE;
    }

    closure = debug_read_vm_closure_argument(context, 0);
    value = ZrLib_CallContext_Argument(context, 2);
    if (!ZrLib_CallContext_ReadInt(context, 1, &index) || closure == ZR_NULL || value == ZR_NULL) {
        ZrLib_Value_SetNull(result);
        return ZR_TRUE;
    }

    name = ZrCore_Debug_SetUpvalue(context->state, closure, (TZrInt32)index, value);
    if (name == ZR_NULL) {
        ZrLib_Value_SetNull(result);
        return ZR_TRUE;
    }

    ZrLib_Value_SetString(context->state, result, name);
    return ZR_TRUE;
}

static TZrBool debug_upvalueid_callback(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrClosure *closure;
    TZrInt64 index = 0;
    TZrPtr pointer;

    if (context == ZR_NULL || result == ZR_NULL || context->state == ZR_NULL ||
        !ZrLib_CallContext_CheckArity(context, 2, 2)) {
        return ZR_FALSE;
    }

    closure = debug_read_vm_closure_argument(context, 0);
    if (!ZrLib_CallContext_ReadInt(context, 1, &index) || closure == ZR_NULL) {
        ZrLib_Value_SetNull(result);
        return ZR_TRUE;
    }

    pointer = ZrCore_Debug_GetUpvalueId(context->state, closure, (TZrInt32)index);
    if (pointer == ZR_NULL) {
        ZrLib_Value_SetNull(result);
        return ZR_TRUE;
    }

    ZrLib_Value_SetNativePointer(context->state, result, pointer);
    return ZR_TRUE;
}

static TZrBool debug_sethook_callback(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrTypeValue *hookValue;
    SZrTypeValue *maskValue;
    TZrUInt32 mask = 0u;
    TZrInt64 count = 0;
    TZrChar maskText[ZR_DEBUG_SCRIPT_HOOK_MASK_TEXT_CAPACITY];
    ZrDebugScriptHookRecord *record;

    if (!debug_require_write_api(context)) {
        return ZR_FALSE;
    }
    if (context == ZR_NULL || result == ZR_NULL || context->state == ZR_NULL ||
        !ZrLib_CallContext_CheckArity(context, 0, 3)) {
        return ZR_FALSE;
    }

    hookValue = ZrLib_CallContext_Argument(context, 0);
    if (debug_is_null_value(hookValue)) {
        debug_clear_script_hook(context->state);
        ZrLib_Value_SetNull(result);
        return ZR_TRUE;
    }

    if (!ZrLib_CallContext_ReadFunction(context, 0, &hookValue)) {
        return ZR_FALSE;
    }

    maskText[0] = '\0';
    maskValue = ZrLib_CallContext_Argument(context, 1);
    if (!debug_is_null_value(maskValue)) {
        if (maskValue->type == ZR_VALUE_TYPE_STRING) {
            SZrString *maskString = ZR_NULL;
            const TZrChar *sourceMask;

            if (!ZrLib_CallContext_ReadString(context, 1, &maskString)) {
                return ZR_FALSE;
            }
            sourceMask = maskString != ZR_NULL ? ZrCore_String_GetNativeString(maskString) : ZR_NULL;
            mask = debug_parse_hook_mask_text(sourceMask);
            debug_mask_to_text(mask, maskText, sizeof(maskText));
        } else if (ZR_VALUE_IS_TYPE_INT(maskValue->type)) {
            TZrInt64 rawMask = 0;
            if (!ZrLib_CallContext_ReadInt(context, 1, &rawMask)) {
                return ZR_FALSE;
            }
            mask = (TZrUInt32)rawMask;
            debug_mask_to_text(mask, maskText, sizeof(maskText));
        } else {
            ZrLib_CallContext_RaiseTypeError(context, 1, "string or int");
        }
    }

    if (context->argumentCount > 2 && !ZrLib_CallContext_ReadInt(context, 2, &count)) {
        return ZR_FALSE;
    }
    if (count < 0) {
        count = 0;
    }
    if (count > 0) {
        mask |= ZR_DEBUG_HOOK_MASK_COUNT;
    }

    if (mask == 0u) {
        debug_clear_script_hook(context->state);
        ZrLib_Value_SetNull(result);
        return ZR_TRUE;
    }

    record = debug_get_or_create_hook_record(context->state);
    if (record == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_Copy(context->state, &record->hookValue, hookValue);
    memcpy(record->maskText, maskText, sizeof(record->maskText));
    record->maskText[sizeof(record->maskText) - 1u] = '\0';
    record->count = (TZrUInt32)count;
    debug_store_module_hook_value(context->state, hookValue);
    ZrCore_Debug_SetHook(context->state, debug_script_hook_trampoline, mask, (TZrUInt32)count);
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

static TZrBool debug_gethook_callback(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrDebugScriptHookRecord *record;
    SZrObject *object;

    if (context == ZR_NULL || result == ZR_NULL || context->state == ZR_NULL ||
        !ZrLib_CallContext_CheckArity(context, 0, 0)) {
        return ZR_FALSE;
    }

    object = ZrLib_Object_New(context->state);
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }

    record = debug_find_hook_record(context->state);
    if (record == ZR_NULL || ZrCore_Debug_GetHook(context->state) != debug_script_hook_trampoline ||
        debug_is_null_value(&record->hookValue)) {
        SZrTypeValue nullValue;

        ZrLib_Value_SetNull(&nullValue);
        debug_write_value_field(context->state, object, "hook", &nullValue);
        debug_write_string_field(context->state, object, "mask", "");
        debug_write_int_field(context->state, object, "count", 0);
    } else {
        debug_write_value_field(context->state, object, "hook", &record->hookValue);
        debug_write_string_field(context->state, object, "mask", record->maskText);
        debug_write_int_field(context->state, object, "count", (TZrInt64)record->count);
    }

    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

static TZrBool debug_module_on_materialize(SZrState *state,
                                           SZrObjectModule *module,
                                           const ZrLibModuleDescriptor *descriptor) {
    SZrTypeValue writeEnabledValue;
    SZrTypeValue nullValue;

    if (state == ZR_NULL || module == ZR_NULL || descriptor == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetBool(state, &writeEnabledValue, descriptor == &g_debug_module_descriptor);
    ZrLib_Object_SetFieldCString(state, &module->super, "__writeEnabled", &writeEnabledValue);
    ZrLib_Value_SetNull(&nullValue);
    ZrLib_Object_SetFieldCString(state, &module->super, "__hook", &nullValue);
    return ZR_TRUE;
}

const ZrLibModuleDescriptor *ZrVmLibDebug_GetModuleDescriptor(void) {
    return &g_debug_module_descriptor;
}

const ZrLibModuleDescriptor *ZrVmLibDebug_GetSandboxedModuleDescriptor(void) {
    return &g_debug_sandboxed_module_descriptor;
}

TZrBool ZrVmLibDebug_Register(SZrGlobalState *global) {
    return ZrLibrary_NativeRegistry_RegisterModule(global, &g_debug_module_descriptor);
}

TZrBool ZrVmLibDebug_RegisterSandboxed(SZrGlobalState *global) {
    return ZrLibrary_NativeRegistry_RegisterModule(global, &g_debug_sandboxed_module_descriptor);
}

#if defined(ZR_LIBRARY_TYPE_SHARED)
const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void) {
    return ZrVmLibDebug_GetModuleDescriptor();
}
#endif
