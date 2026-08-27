#include "compiler_internal.h"
#include "compiler_extern_decorator_diagnostics.h"

typedef enum EZrExternStructDecoratorTarget {
    ZR_EXTERN_STRUCT_DECORATOR_DECLARATION = 0,
    ZR_EXTERN_STRUCT_DECORATOR_FIELD = 1
} EZrExternStructDecoratorTarget;

typedef struct SZrExternStructDecoratorRule {
    const TZrChar *leafName;
    EZrExternStructDecoratorTarget target;
} SZrExternStructDecoratorRule;

static const SZrExternStructDecoratorRule kExternStructDecoratorRules[] = {
        {"kind", ZR_EXTERN_STRUCT_DECORATOR_DECLARATION},
        {"pack", ZR_EXTERN_STRUCT_DECORATOR_DECLARATION},
        {"align", ZR_EXTERN_STRUCT_DECORATOR_DECLARATION},
        {"offset", ZR_EXTERN_STRUCT_DECORATOR_FIELD},
        {"charset", ZR_EXTERN_STRUCT_DECORATOR_FIELD},
};

static const TZrChar *const kExternStructKindValues[] = {
        "struct", "union"};
static const TZrChar *const kExternStructCharsetValues[] = {
        "utf8", "utf16", "ansi"};

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
    for (TZrSize index = 0U; index < allowedValueCount; index++) {
        if (allowedValues[index] != ZR_NULL &&
            strcmp(text, allowedValues[index]) == 0) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool compiler_extern_struct_integer_is_power_of_two(TZrInt64 value) {
    TZrUInt64 unsignedValue;

    if (value <= 0 || (TZrUInt64)value > UINT32_MAX) {
        return ZR_FALSE;
    }
    unsignedValue = (TZrUInt64)value;
    return (unsignedValue & (unsignedValue - 1U)) == 0U;
}

static TZrBool compiler_extern_struct_arguments_valid(
        const SZrExternStructDecoratorRule *rule,
        SZrFunctionCall *call) {
    SZrString *stringValue = ZR_NULL;
    TZrInt64 integerValue = 0;

    if (rule == ZR_NULL || rule->leafName == ZR_NULL || call == ZR_NULL) {
        return ZR_FALSE;
    }
    if (strcmp(rule->leafName, "kind") == 0) {
        return extern_compiler_extract_string_argument(call, &stringValue) &&
               compiler_extern_string_in_set(
                       stringValue,
                       kExternStructKindValues,
                       ZR_ARRAY_COUNT(kExternStructKindValues));
    }
    if (strcmp(rule->leafName, "pack") == 0 ||
        strcmp(rule->leafName, "align") == 0) {
        return extern_compiler_extract_int_argument(call, &integerValue) &&
               compiler_extern_struct_integer_is_power_of_two(integerValue);
    }
    if (strcmp(rule->leafName, "offset") == 0) {
        return extern_compiler_extract_int_argument(call, &integerValue) &&
               integerValue >= 0 && (TZrUInt64)integerValue <= UINT32_MAX;
    }
    if (strcmp(rule->leafName, "charset") == 0) {
        return extern_compiler_extract_string_argument(call, &stringValue) &&
               compiler_extern_string_in_set(
                       stringValue,
                       kExternStructCharsetValues,
                       ZR_ARRAY_COUNT(kExternStructCharsetValues));
    }
    return ZR_FALSE;
}

static TZrBool compiler_extern_validate_struct_decorator_array(
        SZrCompilerState *cs,
        SZrAstNodeArray *decorators,
        EZrExternStructDecoratorTarget target) {
    TZrChar message[ZR_PARSER_ERROR_BUFFER_LENGTH];
    const TZrChar *targetText = target == ZR_EXTERN_STRUCT_DECORATOR_FIELD
                                       ? "extern struct fields"
                                       : "extern struct declarations";
    const TZrChar *cause = target == ZR_EXTERN_STRUCT_DECORATOR_FIELD
                                   ? "The decorator is not a valid canonical zr.ffi directive for this extern struct field."
                                   : "The decorator is not a valid canonical zr.ffi directive for this extern struct declaration.";
    const TZrChar *suggestion = target == ZR_EXTERN_STRUCT_DECORATOR_FIELD
                                        ? "Use zr.ffi.offset with a nonnegative integer or zr.ffi.charset with a supported charset."
                                        : "Use zr.ffi.kind, zr.ffi.pack, or zr.ffi.align with a supported canonical value.";

    if (cs == ZR_NULL) {
        return ZR_FALSE;
    }
    if (decorators == ZR_NULL) {
        return ZR_TRUE;
    }
    for (TZrSize decoratorIndex = 0U;
         decoratorIndex < decorators->count;
         decoratorIndex++) {
        SZrAstNode *decoratorNode = decorators->nodes[decoratorIndex];
        const SZrExternStructDecoratorRule *matchedRule = ZR_NULL;
        SZrFunctionCall *call = ZR_NULL;

        if (decoratorNode == ZR_NULL) {
            continue;
        }
        for (TZrSize ruleIndex = 0U;
             ruleIndex < ZR_ARRAY_COUNT(kExternStructDecoratorRules);
             ruleIndex++) {
            SZrFunctionCall *candidateCall = ZR_NULL;
            const SZrExternStructDecoratorRule *rule =
                    &kExternStructDecoratorRules[ruleIndex];

            if (rule->target == target &&
                extern_compiler_match_decorator_path(
                        decoratorNode,
                        rule->leafName,
                        ZR_TRUE,
                        &candidateCall)) {
                matchedRule = rule;
                call = candidateCall;
                break;
            }
        }
        if (matchedRule == ZR_NULL) {
            snprintf(
                    message,
                    sizeof(message),
                    "Decorator is not valid on %s",
                    targetText);
            return compiler_extern_report_invalid_decorator(
                    cs, decoratorNode, message, cause, suggestion);
        }
        if (!compiler_extern_struct_arguments_valid(matchedRule, call)) {
            snprintf(
                    message,
                    sizeof(message),
                    "zr.ffi.%s has invalid arguments for this %s",
                    matchedRule->leafName,
                    target == ZR_EXTERN_STRUCT_DECORATOR_FIELD
                            ? "extern struct field"
                            : "extern struct declaration");
            return compiler_extern_report_invalid_decorator(
                    cs, decoratorNode, message, cause, suggestion);
        }
    }
    return ZR_TRUE;
}

TZrBool ZrParser_Compiler_ValidateExternStructDecorators(
        SZrCompilerState *cs,
        SZrAstNode *declaration) {
    SZrStructDeclaration *structure;

    if (cs == ZR_NULL || declaration == ZR_NULL ||
        declaration->type != ZR_AST_STRUCT_DECLARATION) {
        return ZR_FALSE;
    }
    structure = &declaration->data.structDeclaration;
    if (!compiler_extern_validate_struct_decorator_array(
                cs,
                structure->decorators,
                ZR_EXTERN_STRUCT_DECORATOR_DECLARATION)) {
        return ZR_FALSE;
    }
    if (structure->members == ZR_NULL) {
        return ZR_TRUE;
    }
    for (TZrSize memberIndex = 0U;
         memberIndex < structure->members->count;
         memberIndex++) {
        SZrAstNode *member = structure->members->nodes[memberIndex];

        if (member != ZR_NULL && member->type == ZR_AST_STRUCT_FIELD &&
            !compiler_extern_validate_struct_decorator_array(
                    cs,
                    member->data.structField.decorators,
                    ZR_EXTERN_STRUCT_DECORATOR_FIELD)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}
