#include "semantic/lsp_local_semantic_expression_text.h"

#include "zr_vm_core/string.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

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

static const TZrChar *expression_text_string_value(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    return value->shortStringLength < ZR_VM_LONG_STRING_FLAG
               ? ZrCore_String_GetNativeStringShort(value)
               : ZrCore_String_GetNativeString(value);
}

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
