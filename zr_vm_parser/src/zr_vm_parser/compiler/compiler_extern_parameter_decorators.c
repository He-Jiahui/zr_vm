#include "compiler_internal.h"
#include "compiler_extern_decorator_diagnostics.h"
#include "compiler_extern_parameter_decorators.h"

/* extern 参数方向的唯一规范集合；数组校验借它跨多个装饰器统计互斥性。 */
static const TZrChar *const kExternParameterDirectionNames[] = {
        "in", "out", "inout"};

/** @brief 限制 FFI 参数 charset 为后端声明的可表示编码集合。 */
static TZrBool compiler_extern_parameter_charset_supported(
        SZrString *value) {
    return extern_compiler_string_equals(value, "utf8") ||
           extern_compiler_string_equals(value, "utf16") ||
           extern_compiler_string_equals(value, "ansi");
}

/** @brief 把参数装饰器非法形态归入统一的结构化诊断，供编译器与 LSP 查询。 */
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

/**
 * @brief 验证单个参数装饰器的规范路径与参数形态，未知指令一律拒绝。
 * @pre directionCount 由同一参数的数组入口维护，跨装饰器累计 in/out/inout 的互斥计数。
 * @return 空装饰器与合法指令返回 true；非法形态发布统一诊断后返回 false。
 */
static TZrBool compiler_extern_validate_parameter_decorator(
        SZrCompilerState *cs,
        SZrAstNode *decorator,
        TZrSize *directionCount) {
    TZrChar message[ZR_PARSER_ERROR_BUFFER_LENGTH];

    if (decorator == ZR_NULL) {
        return ZR_TRUE;
    }
    /* 方向是无参标记，同一参数至多一个；带调用后缀的同名指令也要诊断。 */
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

    /* charset 必须是单个受支持的字符串实参；裸路径和其他指令走统一拒绝路径。 */
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

/**
 * @brief 按参数声明顺序验证全部 FFI 装饰器，维持跨装饰器的方向互斥约束。
 * @pre cs 是可报告诊断的编译器状态；decorators 可为空，代表参数没有附加约束。
 * @return 第一个无效指令即返回 false；成功表示这一数组整体符合规范。
 */
TZrBool compiler_extern_validate_parameter_decorator_array(
        SZrCompilerState *cs,
        SZrAstNodeArray *decorators) {
    TZrSize directionCount = 0U;
    /* TODO: 数组只累计方向指令；charset 逐项校验且可重复，需确定不同 charset 是否应冲突报错。 */
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

/**
 * @brief 为编译器及 LSP 提供单个 extern 参数的同一规范校验入口。
 * @pre parameter 必须为 ZR_AST_PARAMETER；调用者决定是否处于 extern 函数或 delegate 上下文。
 * @return 非法节点或装饰器返回 false；装饰器为空时返回 true。
 */
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
