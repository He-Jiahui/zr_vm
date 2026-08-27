#include "compiler_internal.h"
#include "compiler_extern_decorator_diagnostics.h"

typedef struct SZrExternCallableDecoratorRule {
    const TZrChar *leafName;
    TZrBool requireCall;
} SZrExternCallableDecoratorRule;

static const SZrExternCallableDecoratorRule kExternFunctionDecoratorRules[] = {
        {"entry", ZR_TRUE},
        {"callingConvention", ZR_TRUE},
        {"callconv", ZR_TRUE},
        {"charset", ZR_TRUE},
        {"errorPolicy", ZR_TRUE},
        {"cleanup", ZR_TRUE},
        {"callbackLifetime", ZR_TRUE},
        {"callbackThread", ZR_TRUE},
        {"callbackException", ZR_TRUE},
        {"platform", ZR_TRUE},
        {"requiredCapabilities", ZR_TRUE},
};

static const SZrExternCallableDecoratorRule kExternDelegateDecoratorRules[] = {
        {"callingConvention", ZR_TRUE},
        {"callconv", ZR_TRUE},
        {"charset", ZR_TRUE},
};

static const TZrChar *const kExternCallableAbiValues[] = {
        "c", "cdecl", "stdcall", "system"};
static const TZrChar *const kExternCallableCharsetValues[] = {
        "utf8", "utf16", "ansi"};
static const TZrChar *const kExternCallableErrorPolicyValues[] = {
        "returnCode", "lastError", "errno", "throws"};
static const TZrChar *const kExternCallableCleanupValues[] = {
        "caller", "callee", "registered"};
static const TZrChar *const kExternCallableLifetimeValues[] = {
        "call", "scoped", "static"};
static const TZrChar *const kExternCallableThreadValues[] = {
        "caller", "attach", "forbidden"};
static const TZrChar *const kExternCallableExceptionValues[] = {
        "abort", "returnDefault", "errorResult"};
static const TZrChar *const kExternCallablePlatformValues[] = {
        "any", "windows", "unix"};

static TZrBool compiler_extern_string_in_set(
        SZrString *value,
        const TZrChar *const *allowedValues,
        TZrSize allowedValueCount) {
    const TZrChar *text;

    if (value == ZR_NULL || allowedValues == ZR_NULL) {
        return ZR_FALSE;
    }
    text = ZrCore_String_GetNativeString(value);
    if (text == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0; index < allowedValueCount; index++) {
        if (allowedValues[index] != ZR_NULL &&
            strcmp(text, allowedValues[index]) == 0) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool compiler_extern_callable_arguments_valid(
        const TZrChar *leafName,
        SZrFunctionCall *call) {
    SZrString *stringValue = ZR_NULL;
    TZrInt64 integerValue = 0;

    if (leafName == ZR_NULL || call == ZR_NULL) {
        return ZR_FALSE;
    }
    if (strcmp(leafName, "requiredCapabilities") == 0) {
        return extern_compiler_extract_int_argument(call, &integerValue) &&
               integerValue >= 0;
    }
    if (!extern_compiler_extract_string_argument(call, &stringValue)) {
        return ZR_FALSE;
    }
    if (strcmp(leafName, "entry") == 0) {
        return ZR_TRUE;
    }
    if (strcmp(leafName, "callingConvention") == 0 ||
        strcmp(leafName, "callconv") == 0) {
        return compiler_extern_string_in_set(
                stringValue,
                kExternCallableAbiValues,
                ZR_ARRAY_COUNT(kExternCallableAbiValues));
    }
    if (strcmp(leafName, "charset") == 0) {
        return compiler_extern_string_in_set(
                stringValue,
                kExternCallableCharsetValues,
                ZR_ARRAY_COUNT(kExternCallableCharsetValues));
    }
    if (strcmp(leafName, "errorPolicy") == 0) {
        return compiler_extern_string_in_set(
                stringValue,
                kExternCallableErrorPolicyValues,
                ZR_ARRAY_COUNT(kExternCallableErrorPolicyValues));
    }
    if (strcmp(leafName, "cleanup") == 0) {
        return compiler_extern_string_in_set(
                stringValue,
                kExternCallableCleanupValues,
                ZR_ARRAY_COUNT(kExternCallableCleanupValues));
    }
    if (strcmp(leafName, "callbackLifetime") == 0) {
        return compiler_extern_string_in_set(
                stringValue,
                kExternCallableLifetimeValues,
                ZR_ARRAY_COUNT(kExternCallableLifetimeValues));
    }
    if (strcmp(leafName, "callbackThread") == 0) {
        return compiler_extern_string_in_set(
                stringValue,
                kExternCallableThreadValues,
                ZR_ARRAY_COUNT(kExternCallableThreadValues));
    }
    if (strcmp(leafName, "callbackException") == 0) {
        return compiler_extern_string_in_set(
                stringValue,
                kExternCallableExceptionValues,
                ZR_ARRAY_COUNT(kExternCallableExceptionValues));
    }
    if (strcmp(leafName, "platform") == 0) {
        return compiler_extern_string_in_set(
                stringValue,
                kExternCallablePlatformValues,
                ZR_ARRAY_COUNT(kExternCallablePlatformValues));
    }
    return ZR_FALSE;
}

static TZrBool compiler_extern_validate_callable_rules(
        SZrCompilerState *cs,
        SZrAstNodeArray *decorators,
        const SZrExternCallableDecoratorRule *rules,
        TZrSize ruleCount) {
    TZrChar message[ZR_PARSER_ERROR_BUFFER_LENGTH];

    if (cs == ZR_NULL) {
        return ZR_FALSE;
    }
    if (decorators == ZR_NULL) {
        return ZR_TRUE;
    }
    for (TZrSize decoratorIndex = 0;
         decoratorIndex < decorators->count;
         decoratorIndex++) {
        SZrAstNode *decoratorNode = decorators->nodes[decoratorIndex];
        const SZrExternCallableDecoratorRule *matchedRule = ZR_NULL;
        SZrFunctionCall *call = ZR_NULL;

        if (decoratorNode == ZR_NULL) {
            continue;
        }
        for (TZrSize ruleIndex = 0; ruleIndex < ruleCount; ruleIndex++) {
            SZrFunctionCall *candidateCall = ZR_NULL;

            if (extern_compiler_match_decorator_path(
                        decoratorNode,
                        rules[ruleIndex].leafName,
                        rules[ruleIndex].requireCall,
                        &candidateCall)) {
                matchedRule = &rules[ruleIndex];
                call = candidateCall;
                break;
            }
        }
        if (matchedRule == ZR_NULL) {
            return compiler_extern_report_invalid_decorator(
                    cs,
                    decoratorNode,
                    "Decorator is not valid on extern callable declarations",
                    "The decorator is not a valid canonical zr.ffi directive for this extern callable declaration.",
                    "Use a supported zr.ffi callable decorator with the required argument shape and value.");
        }
        if (!compiler_extern_callable_arguments_valid(
                    matchedRule->leafName, call)) {
            snprintf(message,
                     sizeof(message),
                     "zr.ffi.%s has invalid arguments for this extern callable declaration",
                     matchedRule->leafName);
            return compiler_extern_report_invalid_decorator(
                    cs,
                    decoratorNode,
                    message,
                    "The decorator is not a valid canonical zr.ffi directive for this extern callable declaration.",
                    "Use a supported zr.ffi callable decorator with the required argument shape and value.");
        }
    }
    return ZR_TRUE;
}

TZrBool ZrParser_Compiler_ValidateExternCallableDecorators(
        SZrCompilerState *cs,
        SZrAstNode *declaration) {
    if (cs == ZR_NULL || declaration == ZR_NULL) {
        return ZR_FALSE;
    }
    switch (declaration->type) {
        case ZR_AST_EXTERN_FUNCTION_DECLARATION:
            return compiler_extern_validate_callable_rules(
                    cs,
                    declaration->data.externFunctionDeclaration.decorators,
                    kExternFunctionDecoratorRules,
                    ZR_ARRAY_COUNT(kExternFunctionDecoratorRules));
        case ZR_AST_EXTERN_DELEGATE_DECLARATION:
            return compiler_extern_validate_callable_rules(
                    cs,
                    declaration->data.externDelegateDeclaration.decorators,
                    kExternDelegateDecoratorRules,
                    ZR_ARRAY_COUNT(kExternDelegateDecoratorRules));
        default:
            return ZR_FALSE;
    }
}
