#include "semantic/lsp_local_semantic_expression_text.h"

#include "zr_vm_core/string.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* 各 fact 格式化器共用这一容量边界；失败即让上层放弃整个 hover，不发布半截 Markdown。 */
static TZrBool expression_text_append_format(TZrChar *buffer,
                                             TZrSize bufferSize,
                                             TZrSize *used,
                                             const TZrChar *format,
                                             ...) {
    va_list args;
    int written;

    if (buffer == ZR_NULL || used == ZR_NULL || format == ZR_NULL || *used >= bufferSize) {
        return ZR_FALSE;
    }

    va_start(args, format);
    written = vsnprintf(buffer + *used, bufferSize - *used, format, args);
    va_end(args);

    if (written < 0 || (TZrSize)written >= bufferSize - *used) {
        buffer[bufferSize - 1u] = '\0';
        return ZR_FALSE;
    }

    *used += (TZrSize)written;
    return ZR_TRUE;
}

/* parser 的常量字符串借自语义快照；这里只取得短/长字符串统一视图，不转移所有权。 */
static const TZrChar *expression_text_string_value(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    return value->shortStringLength < ZR_VM_LONG_STRING_FLAG
               ? ZrCore_String_GetNativeStringShort(value)
               : ZrCore_String_GetNativeString(value);
}

/* 常量以可读的源码式转义附于事实摘要，避免控制字节破坏编辑器中的 hover 布局。 */
static TZrBool expression_text_append_escaped_string_constant(TZrChar *buffer,
                                                              TZrSize bufferSize,
                                                              TZrSize *used,
                                                              SZrString *value) {
    const TZrChar *text = expression_text_string_value(value);
    TZrSize length = ZrCore_String_GetByteLength(value);
    TZrSize index;
    unsigned char byte;

    if (!expression_text_append_format(buffer, bufferSize, used, "\n\nConstant: \"")) {
        return ZR_FALSE;
    }

    for (index = 0; text != ZR_NULL && index < length; ++index) {
        byte = (unsigned char)text[index];
        switch (byte) {
            case '"':
                if (!expression_text_append_format(buffer, bufferSize, used, "\\\"")) {
                    return ZR_FALSE;
                }
                break;
            case '\\':
                if (!expression_text_append_format(buffer, bufferSize, used, "\\\\")) {
                    return ZR_FALSE;
                }
                break;
            case '\n':
                if (!expression_text_append_format(buffer, bufferSize, used, "\\n")) {
                    return ZR_FALSE;
                }
                break;
            case '\t':
                if (!expression_text_append_format(buffer, bufferSize, used, "\\t")) {
                    return ZR_FALSE;
                }
                break;
            case '\r':
                if (!expression_text_append_format(buffer, bufferSize, used, "\\r")) {
                    return ZR_FALSE;
                }
                break;
            case '\b':
                if (!expression_text_append_format(buffer, bufferSize, used, "\\b")) {
                    return ZR_FALSE;
                }
                break;
            case '\f':
                if (!expression_text_append_format(buffer, bufferSize, used, "\\f")) {
                    return ZR_FALSE;
                }
                break;
            default:
                if (byte < 0x20u || byte == 0x7fu) {
                    if (!expression_text_append_format(buffer,
                                                       bufferSize,
                                                       used,
                                                       "\\x%02X",
                                                       (unsigned int)byte)) {
                        return ZR_FALSE;
                    }
                } else if (!expression_text_append_format(buffer, bufferSize, used, "%c", (int)byte)) {
                    return ZR_FALSE;
                }
                break;
        }
    }

    return expression_text_append_format(buffer, bufferSize, used, "\"");
}

/* 展示稳定的事实类别，供独立 hover 和已有符号 hover 的附录使用。 */
static const TZrChar *expression_text_kind(EZrSemanticExpressionFactKind kind) {
    switch (kind) {
        case ZR_SEMANTIC_EXPRESSION_FACT_LITERAL:
            return "literal";
        case ZR_SEMANTIC_EXPRESSION_FACT_IDENTIFIER:
            return "identifier";
        case ZR_SEMANTIC_EXPRESSION_FACT_BINARY:
            return "binary";
        case ZR_SEMANTIC_EXPRESSION_FACT_UNARY:
            return "unary";
        case ZR_SEMANTIC_EXPRESSION_FACT_CALL:
            return "call";
        case ZR_SEMANTIC_EXPRESSION_FACT_MEMBER:
            return "member";
        case ZR_SEMANTIC_EXPRESSION_FACT_ASSIGNMENT:
            return "assignment";
        case ZR_SEMANTIC_EXPRESSION_FACT_CONDITIONAL:
            return "conditional";
        case ZR_SEMANTIC_EXPRESSION_FACT_ARRAY:
            return "array";
        case ZR_SEMANTIC_EXPRESSION_FACT_OBJECT:
            return "object";
        case ZR_SEMANTIC_EXPRESSION_FACT_LAMBDA:
            return "lambda";
        case ZR_SEMANTIC_EXPRESSION_FACT_OWNERSHIP_BUILTIN:
            return "ownership builtin";
        case ZR_SEMANTIC_EXPRESSION_FACT_CONVERSION:
            return "conversion";
        case ZR_SEMANTIC_EXPRESSION_FACT_ERROR:
            return "error";
        case ZR_SEMANTIC_EXPRESSION_FACT_UNKNOWN:
        default:
            return "unknown";
    }
}

/* 将推断精度连同表达式类别呈现，防止近似结果被读作确定结论。 */
static const TZrChar *expression_text_exactness(EZrSemanticFactExactness exactness) {
    switch (exactness) {
        case ZR_SEMANTIC_FACT_EXACT:
            return "exact";
        case ZR_SEMANTIC_FACT_APPROXIMATE:
            return "approximate";
        case ZR_SEMANTIC_FACT_UNKNOWN:
        default:
            return "unknown";
    }
}

/* 仅将已知 intrinsic 操作映射到 UI 名称；未知枚举值不生成误导性说明。 */
static const TZrChar *expression_text_ownership_intrinsic_operation(
        EZrOwnershipIntrinsicOperation operation) {
    switch (operation) {
        case ZR_OWNERSHIP_INTRINSIC_SHARE:
            return "share";
        case ZR_OWNERSHIP_INTRINSIC_DEGRADE:
            return "degrade";
        case ZR_OWNERSHIP_INTRINSIC_WAKE:
            return "wake";
        case ZR_OWNERSHIP_INTRINSIC_INTO_GC:
            return "intoGc";
        case ZR_OWNERSHIP_INTRINSIC_DROP:
            return "drop";
        default:
            return ZR_NULL;
    }
}

/* 只有 parser 明示 hasConstant 时才展示值，不能把未知值当成可求值的常量。 */
static TZrBool expression_text_append_constant(TZrChar *buffer,
                                               TZrSize bufferSize,
                                               TZrSize *used,
                                               const SZrSemanticExpressionFact *fact) {
    if (fact == ZR_NULL || !fact->hasConstant) {
        return ZR_TRUE;
    }

    switch (fact->valueKind) {
        case ZR_SEMANTIC_VALUE_KIND_BOOL:
            return expression_text_append_format(buffer,
                                                 bufferSize,
                                                 used,
                                                 "\n\nConstant: %s",
                                                 fact->constantValue.boolValue ? "true" : "false");
        case ZR_SEMANTIC_VALUE_KIND_INT64:
            return expression_text_append_format(buffer,
                                                 bufferSize,
                                                 used,
                                                 "\n\nConstant: %lld",
                                                 (long long)fact->constantValue.int64Value);
        case ZR_SEMANTIC_VALUE_KIND_UINT64:
            return expression_text_append_format(buffer,
                                                 bufferSize,
                                                 used,
                                                 "\n\nConstant: %llu",
                                                 (unsigned long long)fact->constantValue.uint64Value);
        case ZR_SEMANTIC_VALUE_KIND_DOUBLE:
            return expression_text_append_format(buffer,
                                                 bufferSize,
                                                 used,
                                                 "\n\nConstant: %.17g",
                                                 fact->constantValue.doubleValue);
        case ZR_SEMANTIC_VALUE_KIND_STRING:
            return expression_text_append_escaped_string_constant(buffer,
                                                                 bufferSize,
                                                                 used,
                                                                 fact->constantValue.stringValue);
        case ZR_SEMANTIC_VALUE_KIND_NULL:
            return expression_text_append_format(buffer, bufferSize, used, "\n\nConstant: null");
        case ZR_SEMANTIC_VALUE_KIND_UNKNOWN:
        default:
            return ZR_TRUE;
    }
}

/* 局部查询的 expressionFact 由 analyzer 借出；本层只格式化，调用结束后不保存指针。 */
TZrBool ZrLanguageServer_LspLocalSemanticExpressionText_AppendHover(
    TZrChar *buffer,
    TZrSize bufferSize,
    TZrSize *used,
    const SZrSemanticExpressionFact *fact) {
    if (fact == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!expression_text_append_format(buffer,
                                       bufferSize,
                                       used,
                                       "\n\nExpression: %s %s",
                                       expression_text_kind(fact->kind),
                                       expression_text_exactness(fact->exactness))) {
        return ZR_FALSE;
    }

    return expression_text_append_constant(buffer, bufferSize, used, fact);
}

/* 在同一事实附录里显示所有权操作及 place，供调用点 hover 解释语义动作。 */
TZrBool ZrLanguageServer_LspLocalSemanticExpressionText_AppendOwnershipIntrinsicHover(
        TZrChar *buffer,
        TZrSize bufferSize,
        TZrSize *used,
        const SZrOwnershipIntrinsicFact *fact) {
    const TZrChar *operation;

    if (fact == ZR_NULL) {
        return ZR_TRUE;
    }

    operation = expression_text_ownership_intrinsic_operation(fact->operation);
    if (operation == ZR_NULL) {
        return ZR_TRUE;
    }

    return expression_text_append_format(
            buffer,
            bufferSize,
            used,
            "\n\nOwnership intrinsic: %s (%s, place %u)",
            operation,
            fact->consuming ? "consuming" : "non-consuming",
            (unsigned int)fact->placeId);
}
