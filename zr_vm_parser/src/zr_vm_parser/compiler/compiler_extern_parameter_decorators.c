#include "compiler_internal.h"
#include "compiler_extern_decorator_diagnostics.h"
#include "compiler_extern_parameter_decorators.h"

static const TZrChar *const kExternParameterDirectionNames[] = {
        "in", "out", "inout"};

static TZrBool compiler_extern_parameter_charset_supported(
        SZrString *value) {
    return extern_compiler_string_equals(value, "utf8") ||
           extern_compiler_string_equals(value, "utf16") ||
           extern_compiler_string_equals(value, "ansi");
}

static TZrBool compiler_extern_report_invalid_parameter_decorator(
        SZrCompilerState *cs,
        SZrAstNode *decorator,
        const TZrChar *message) {
    return compiler_extern_report_invalid_decorator(
            cs,
            decorator,
            message,
            "The decorator is not a valid canonical zr.ffi directive for this extern parameter.",
            "Use at most one direction decorator or a supported charset with the required argument shape.");
}

static TZrBool compiler_extern_validate_parameter_decorator(
        SZrCompilerState *cs,
        SZrAstNode *decorator,
        TZrSize *directionCount) {
    TZrChar message[ZR_PARSER_ERROR_BUFFER_LENGTH];

    if (decorator == ZR_NULL) {
        return ZR_TRUE;
    }
    for (TZrSize index = 0U;
         index < ZR_ARRAY_COUNT(kExternParameterDirectionNames);
         index++) {
        const TZrChar *direction = kExternParameterDirectionNames[index];
        SZrFunctionCall *call = ZR_NULL;

        if (extern_compiler_match_decorator_path(
                    decorator, direction, ZR_FALSE, ZR_NULL)) {
            (*directionCount)++;
            if (*directionCount > 1U) {
                return compiler_extern_report_invalid_parameter_decorator(
                        cs,
                        decorator,
                        "Extern parameters may specify only one of zr.ffi.in/out/inout");
            }
            return ZR_TRUE;
        }
        if (extern_compiler_match_decorator_path(
                    decorator, direction, ZR_TRUE, &call)) {
            snprintf(
                    message,
                    sizeof(message),
                    "zr.ffi.%s has invalid arguments for this extern parameter",
                    direction);
            return compiler_extern_report_invalid_parameter_decorator(
                    cs, decorator, message);
        }
    }

    {
        SZrFunctionCall *call = ZR_NULL;
        SZrString *charset = ZR_NULL;

        if (extern_compiler_match_decorator_path(
                    decorator, "charset", ZR_TRUE, &call)) {
            if (!extern_compiler_extract_string_argument(call, &charset) ||
                !compiler_extern_parameter_charset_supported(charset)) {
                return compiler_extern_report_invalid_parameter_decorator(
                        cs,
                        decorator,
                        "zr.ffi.charset has invalid arguments for this extern parameter");
            }
            return ZR_TRUE;
        }
        if (extern_compiler_match_decorator_path(
                    decorator, "charset", ZR_FALSE, ZR_NULL)) {
            return compiler_extern_report_invalid_parameter_decorator(
                    cs,
                    decorator,
                    "zr.ffi.charset has invalid arguments for this extern parameter");
        }
    }

    return compiler_extern_report_invalid_parameter_decorator(
            cs,
            decorator,
            "Decorator is not valid on extern parameters");
}

TZrBool compiler_extern_validate_parameter_decorator_array(
        SZrCompilerState *cs,
        SZrAstNodeArray *decorators) {
    TZrSize directionCount = 0U;

    if (cs == ZR_NULL) {
        return ZR_FALSE;
    }
    if (decorators == ZR_NULL) {
        return ZR_TRUE;
    }
    for (TZrSize index = 0U; index < decorators->count; index++) {
        if (!compiler_extern_validate_parameter_decorator(
                    cs, decorators->nodes[index], &directionCount)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool ZrParser_Compiler_ValidateExternParameterDecorators(
        SZrCompilerState *cs,
        SZrAstNode *parameter) {
    if (cs == ZR_NULL || parameter == ZR_NULL ||
        parameter->type != ZR_AST_PARAMETER) {
        return ZR_FALSE;
    }
    return compiler_extern_validate_parameter_decorator_array(
            cs, parameter->data.parameter.decorators);
}
