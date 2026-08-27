#include "compiler_internal.h"
#include "compiler_extern_decorator_diagnostics.h"

typedef enum EZrExternEnumDecoratorTarget {
    ZR_EXTERN_ENUM_DECORATOR_DECLARATION = 0,
    ZR_EXTERN_ENUM_DECORATOR_MEMBER = 1
} EZrExternEnumDecoratorTarget;

static const TZrChar *const kExternEnumUnderlyingValues[] = {
        "i8", "u8", "i16", "u16", "i32", "u32", "i64", "u64"};

static TZrBool compiler_extern_enum_underlying_is_supported(
        SZrString *value) {
    const TZrChar *text;

    if (value == ZR_NULL) {
        return ZR_FALSE;
    }
    text = ZrCore_String_GetNativeString(value);
    if (text == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0U;
         index < ZR_ARRAY_COUNT(kExternEnumUnderlyingValues);
         index++) {
        if (strcmp(text, kExternEnumUnderlyingValues[index]) == 0) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool compiler_extern_enum_arguments_valid(
        EZrExternEnumDecoratorTarget target,
        SZrFunctionCall *call) {
    if (target == ZR_EXTERN_ENUM_DECORATOR_MEMBER) {
        TZrInt64 value = 0;
        return extern_compiler_extract_int_argument(call, &value);
    }

    SZrString *underlying = ZR_NULL;
    return extern_compiler_extract_string_argument(call, &underlying) &&
           compiler_extern_enum_underlying_is_supported(underlying);
}

static TZrBool compiler_extern_validate_enum_decorator_array(
        SZrCompilerState *cs,
        SZrAstNodeArray *decorators,
        EZrExternEnumDecoratorTarget target) {
    const TZrChar *leafName = target == ZR_EXTERN_ENUM_DECORATOR_MEMBER
                                     ? "value"
                                     : "underlying";
    const TZrChar *targetText = target == ZR_EXTERN_ENUM_DECORATOR_MEMBER
                                       ? "extern enum members"
                                       : "extern enum declarations";
    const TZrChar *cause = target == ZR_EXTERN_ENUM_DECORATOR_MEMBER
                                  ? "The decorator is not a valid canonical zr.ffi directive for this extern enum member."
                                  : "The decorator is not a valid canonical zr.ffi directive for this extern enum declaration.";
    const TZrChar *suggestion = target == ZR_EXTERN_ENUM_DECORATOR_MEMBER
                                       ? "Use zr.ffi.value with exactly one integer literal."
                                       : "Use zr.ffi.underlying with one supported fixed-width integer type name.";
    TZrChar message[ZR_PARSER_ERROR_BUFFER_LENGTH];

    if (cs == ZR_NULL) {
        return ZR_FALSE;
    }
    if (decorators == ZR_NULL) {
        return ZR_TRUE;
    }
    for (TZrSize index = 0U; index < decorators->count; index++) {
        SZrAstNode *decoratorNode = decorators->nodes[index];
        SZrFunctionCall *call = ZR_NULL;

        if (decoratorNode == ZR_NULL) {
            continue;
        }
        if (!extern_compiler_match_decorator_path(
                    decoratorNode, leafName, ZR_TRUE, &call)) {
            snprintf(
                    message,
                    sizeof(message),
                    "Decorator is not valid on %s",
                    targetText);
            return compiler_extern_report_invalid_decorator(
                    cs, decoratorNode, message, cause, suggestion);
        }
        if (!compiler_extern_enum_arguments_valid(target, call)) {
            snprintf(
                    message,
                    sizeof(message),
                    "zr.ffi.%s has invalid arguments for this %s",
                    leafName,
                    target == ZR_EXTERN_ENUM_DECORATOR_MEMBER
                            ? "extern enum member"
                            : "extern enum declaration");
            return compiler_extern_report_invalid_decorator(
                    cs, decoratorNode, message, cause, suggestion);
        }
    }
    return ZR_TRUE;
}

TZrBool ZrParser_Compiler_ValidateExternEnumDecorators(
        SZrCompilerState *cs,
        SZrAstNode *declaration) {
    SZrEnumDeclaration *enumeration;

    if (cs == ZR_NULL || declaration == ZR_NULL ||
        declaration->type != ZR_AST_ENUM_DECLARATION) {
        return ZR_FALSE;
    }
    enumeration = &declaration->data.enumDeclaration;
    if (!compiler_extern_validate_enum_decorator_array(
                cs,
                enumeration->decorators,
                ZR_EXTERN_ENUM_DECORATOR_DECLARATION)) {
        return ZR_FALSE;
    }
    if (enumeration->members == ZR_NULL) {
        return ZR_TRUE;
    }
    for (TZrSize memberIndex = 0U;
         memberIndex < enumeration->members->count;
         memberIndex++) {
        SZrAstNode *member = enumeration->members->nodes[memberIndex];

        if (member != ZR_NULL && member->type == ZR_AST_ENUM_MEMBER &&
            !compiler_extern_validate_enum_decorator_array(
                    cs,
                    member->data.enumMember.decorators,
                    ZR_EXTERN_ENUM_DECORATOR_MEMBER)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}
