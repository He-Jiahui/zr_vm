#include "interface/lsp_interface_internal.h"
#include "semantic/lsp_numeric_range_text.h"

#include "zr_vm_parser/semantic_facts.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* 参数文档合成只借用 VM 字符串的底层字节；短串与长串必须按其各自长度读取。 */
static void signature_fact_get_string_view(SZrString *value,
                                           TZrNativeString *text,
                                           TZrSize *length) {
    if (text == ZR_NULL || length == ZR_NULL) {
        return;
    }

    *text = ZR_NULL;
    *length = 0;
    if (value == ZR_NULL) {
        return;
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        *text = ZrCore_String_GetNativeStringShort(value);
        *length = value->shortStringLength;
    } else {
        *text = ZrCore_String_GetNativeString(value);
        *length = value->longStringLength;
    }
}

/* 事实文档使用有界缓冲区；写不下时由调用者放弃附加文档而保留签名帮助主体。 */
static TZrBool signature_fact_append_format(TZrChar *buffer,
                                            TZrSize bufferSize,
                                            TZrSize *used,
                                            const TZrChar *format,
                                            ...) {
    va_list args;
    int written;

    if (buffer == ZR_NULL || bufferSize == 0 || used == ZR_NULL || format == ZR_NULL ||
        *used >= bufferSize) {
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

/* 分隔各类已存在的参数事实，避免空事实在文档开头留下标点。 */
static TZrBool signature_fact_append_separator(TZrChar *buffer,
                                               TZrSize bufferSize,
                                               TZrSize *used) {
    if (used == ZR_NULL || *used == 0) {
        return ZR_TRUE;
    }

    return signature_fact_append_format(buffer, bufferSize, used, ", ");
}

/* 将 parser 的事实分类转成签名帮助的统一展示词汇；不对未知类别赋予具体语义。 */
static const TZrChar *signature_fact_expression_kind(EZrSemanticExpressionFactKind kind) {
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

/* 参数表达式的精度随文案一同展示，调用者可区分确定结果与近似分析。 */
static const TZrChar *signature_fact_exactness(EZrSemanticFactExactness exactness) {
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

/* 字符串常量可直接来自调用实参；转义控制字节以免破坏参数文档边界。 */
static TZrBool signature_fact_append_escaped_string_constant(TZrChar *buffer,
                                                             TZrSize bufferSize,
                                                             TZrSize *used,
                                                             SZrString *value) {
    TZrNativeString text;
    TZrSize length;
    TZrSize index;
    unsigned char byte;

    signature_fact_get_string_view(value, &text, &length);
    if (!signature_fact_append_format(buffer, bufferSize, used, "constant \"")) {
        return ZR_FALSE;
    }

    for (index = 0; text != ZR_NULL && index < length; index++) {
        byte = (unsigned char)text[index];
        switch (byte) {
            case '"':
                if (!signature_fact_append_format(buffer, bufferSize, used, "\\\"")) {
                    return ZR_FALSE;
                }
                break;
            case '\\':
                if (!signature_fact_append_format(buffer, bufferSize, used, "\\\\")) {
                    return ZR_FALSE;
                }
                break;
            case '\n':
                if (!signature_fact_append_format(buffer, bufferSize, used, "\\n")) {
                    return ZR_FALSE;
                }
                break;
            case '\r':
                if (!signature_fact_append_format(buffer, bufferSize, used, "\\r")) {
                    return ZR_FALSE;
                }
                break;
            case '\t':
                if (!signature_fact_append_format(buffer, bufferSize, used, "\\t")) {
                    return ZR_FALSE;
                }
                break;
            default:
                if (byte < 0x20u || byte == 0x7Fu) {
                    if (!signature_fact_append_format(buffer, bufferSize, used, "\\x%02X", (unsigned int)byte)) {
                        return ZR_FALSE;
                    }
                } else if (!signature_fact_append_format(buffer, bufferSize, used, "%c", (int)byte)) {
                    return ZR_FALSE;
                }
                break;
        }
    }

    return signature_fact_append_format(buffer, bufferSize, used, "\"");
}

/* 仅在 parser 明确给出常量值时附加常量文案；类型未知不制造伪精确信息。 */
static TZrBool signature_fact_append_expression_constant(TZrChar *buffer,
                                                         TZrSize bufferSize,
                                                         TZrSize *used,
                                                         const SZrSemanticExpressionFact *fact) {
    if (fact == ZR_NULL || !fact->hasConstant) {
        return ZR_TRUE;
    }

    switch (fact->valueKind) {
        case ZR_SEMANTIC_VALUE_KIND_BOOL:
            if (!signature_fact_append_separator(buffer, bufferSize, used)) {
                return ZR_FALSE;
            }
            return signature_fact_append_format(buffer,
                                                bufferSize,
                                                used,
                                                "constant %s",
                                                fact->constantValue.boolValue ? "true" : "false");
        case ZR_SEMANTIC_VALUE_KIND_INT64:
            if (!signature_fact_append_separator(buffer, bufferSize, used)) {
                return ZR_FALSE;
            }
            return signature_fact_append_format(buffer,
                                                bufferSize,
                                                used,
                                                "constant %lld",
                                                (long long)fact->constantValue.int64Value);
        case ZR_SEMANTIC_VALUE_KIND_UINT64:
            if (!signature_fact_append_separator(buffer, bufferSize, used)) {
                return ZR_FALSE;
            }
            return signature_fact_append_format(buffer,
                                                bufferSize,
                                                used,
                                                "constant %llu",
                                                (unsigned long long)fact->constantValue.uint64Value);
        case ZR_SEMANTIC_VALUE_KIND_DOUBLE:
            if (!signature_fact_append_separator(buffer, bufferSize, used)) {
                return ZR_FALSE;
            }
            return signature_fact_append_format(buffer,
                                                bufferSize,
                                                used,
                                                "constant %.17g",
                                                fact->constantValue.doubleValue);
        case ZR_SEMANTIC_VALUE_KIND_NULL:
            if (!signature_fact_append_separator(buffer, bufferSize, used)) {
                return ZR_FALSE;
            }
            return signature_fact_append_format(buffer, bufferSize, used, "constant null");
        case ZR_SEMANTIC_VALUE_KIND_STRING:
            if (!signature_fact_append_separator(buffer, bufferSize, used)) {
                return ZR_FALSE;
            }
            return signature_fact_append_escaped_string_constant(buffer,
                                                                 bufferSize,
                                                                 used,
                                                                 fact->constantValue.stringValue);
        case ZR_SEMANTIC_VALUE_KIND_UNKNOWN:
        default:
            return ZR_TRUE;
    }
}

/* 表达式类别、精度与常量组合成参数级摘要，供多个签名帮助入口复用。 */
static TZrBool signature_fact_append_expression_detail(TZrChar *buffer,
                                                       TZrSize bufferSize,
                                                       TZrSize *used,
                                                       const SZrSemanticExpressionFact *fact) {
    if (fact == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!signature_fact_append_separator(buffer, bufferSize, used) ||
        !signature_fact_append_format(buffer,
                                      bufferSize,
                                      used,
                                      "expression %s %s",
                                      signature_fact_expression_kind(fact->kind),
                                      signature_fact_exactness(fact->exactness)) ||
        !signature_fact_append_expression_constant(buffer, bufferSize, used, fact)) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

/* 数值区间沿用共享格式；无符号范围及溢出风险仍保留独立提示。 */
static TZrBool signature_fact_append_numeric_detail(TZrChar *buffer,
                                                    TZrSize bufferSize,
                                                    TZrSize *used,
                                                    const SZrSemanticNumericFact *fact) {
    if (fact == ZR_NULL) {
        return ZR_TRUE;
    }

    if (fact->hasRange) {
        if (!signature_fact_append_separator(buffer, bufferSize, used)) {
            return ZR_FALSE;
        }
        if (!ZrLanguageServer_LspNumericRangeText_AppendRange(
                buffer,
                bufferSize,
                used,
                fact,
                ZR_TRUE)) {
            return ZR_FALSE;
        }
    }

    if (fact->hasUnsignedRange) {
        if (!signature_fact_append_separator(buffer, bufferSize, used) ||
            !signature_fact_append_format(buffer,
                                          bufferSize,
                                          used,
                                          "unsigned %llu..%llu",
                                          (unsigned long long)fact->minUnsignedValue,
                                          (unsigned long long)fact->maxUnsignedValue)) {
            return ZR_FALSE;
        }
    }

    if (fact->mayOverflow) {
        if (!signature_fact_append_separator(buffer, bufferSize, used) ||
            !signature_fact_append_format(buffer, bufferSize, used, "may overflow")) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

/* 仅转述语义分析已知的逻辑值与短路性质，避免调用位置再做推断。 */
static TZrBool signature_fact_append_logical_detail(TZrChar *buffer,
                                                    TZrSize bufferSize,
                                                    TZrSize *used,
                                                    const SZrSemanticLogicalFact *fact) {
    if (fact == ZR_NULL) {
        return ZR_TRUE;
    }

    if (fact->hasKnownValue) {
        if (!signature_fact_append_separator(buffer, bufferSize, used) ||
            !signature_fact_append_format(buffer,
                                          bufferSize,
                                          used,
                                          "logical %s",
                                          fact->knownValue ? "true" : "false")) {
            return ZR_FALSE;
        }
    }

    if (fact->kind == ZR_SEMANTIC_LOGICAL_FACT_SHORT_CIRCUIT) {
        if (!signature_fact_append_separator(buffer, bufferSize, used) ||
            !signature_fact_append_format(buffer, bufferSize, used, "short-circuits")) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

/* 所有权诊断值由 VM 管理；本函数只为当前同步格式化借用其零结尾文本。 */
static const TZrChar *signature_fact_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        return ZrCore_String_GetNativeStringShort(value);
    }

    return ZrCore_String_GetNativeString(value);
}

/* 签名帮助只提升违规的所有权事实；普通借用和转移不被包装成错误。 */
static TZrBool signature_fact_append_ownership_detail(TZrChar *buffer,
                                                      TZrSize bufferSize,
                                                      TZrSize *used,
                                                      const SZrSemanticOwnershipFact *fact) {
    const TZrChar *message;

    if (fact == ZR_NULL || !fact->isViolation) {
        return ZR_TRUE;
    }

    message = signature_fact_string_text(fact->diagnosticMessage);
    if (!signature_fact_append_separator(buffer, bufferSize, used)) {
        return ZR_FALSE;
    }

    if (message != ZR_NULL && message[0] != '\0') {
        return signature_fact_append_format(buffer,
                                            bufferSize,
                                            used,
                                            "ownership violation: %s",
                                            message);
    }

    return signature_fact_append_format(buffer, bufferSize, used, "ownership violation");
}

/* 内建操作的展示名与语言内建函数对应，未知枚举不猜测名称。 */
static const TZrChar *signature_fact_ownership_intrinsic_operation(
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

/* 参数表达式若含内建所有权操作，同步展示其是否消费输入。 */
static TZrBool signature_fact_append_ownership_intrinsic_detail(
        TZrChar *buffer,
        TZrSize bufferSize,
        TZrSize *used,
        const SZrOwnershipIntrinsicFact *fact) {
    const TZrChar *operation;

    if (fact == ZR_NULL) {
        return ZR_TRUE;
    }

    operation = signature_fact_ownership_intrinsic_operation(fact->operation);
    if (operation == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!signature_fact_append_separator(buffer, bufferSize, used)) {
        return ZR_FALSE;
    }

    return signature_fact_append_format(
            buffer,
            bufferSize,
            used,
            "ownership intrinsic %s %s",
            operation,
            fact->consuming ? "consuming" : "non-consuming");
}

/** @brief 为当前调用实参生成语义事实文档，供普通、规范类型和外部调用签名复用。
 *  @pre argumentNode 属于 analyzer 当前 semanticContext 对应的 AST；返回 VM 字符串由 VM 管理。
 *  @return 无已记录事实或文案装不下时返回 NULL；签名帮助主体仍可继续构造。 */
SZrString *ZrLanguageServer_Lsp_BuildSignatureArgumentSemanticFactDocumentation(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *argumentNode) {
    const SZrSemanticExpressionFact *expressionFact;
    const SZrSemanticNumericFact *numericFact;
    const SZrSemanticLogicalFact *logicalFact;
    const SZrSemanticOwnershipFact *ownershipFact;
    const SZrOwnershipIntrinsicFact *ownershipIntrinsicFact;
    /* BUG: 长字符串实参会产生超过 256 字节的常量事实；拼接失败后返回 NULL，
     * 调用者仍给出签名却丢失该实参的整段语义事实。字符串常量由 parser 实际记录。 */
    TZrChar factBuffer[ZR_LSP_TEXT_BUFFER_LENGTH];
    TZrChar docBuffer[ZR_LSP_TEXT_BUFFER_LENGTH];
    TZrSize factLength = 0;
    TZrSize docLength = 0;

    if (state == ZR_NULL ||
        analyzer == ZR_NULL ||
        analyzer->semanticContext == ZR_NULL ||
        argumentNode == ZR_NULL) {
        return ZR_NULL;
    }

    /* 与补全一样只读取当前快照的 AST 节点事实；文档路径不得重新执行类型推断。 */
    expressionFact = ZrParser_SemanticFacts_FindExpressionByNode(analyzer->semanticContext, argumentNode);
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(analyzer->semanticContext, argumentNode);
    logicalFact = ZrParser_SemanticFacts_FindLogicalByNode(analyzer->semanticContext, argumentNode);
    /* TODO: 位置查找仅按 argumentNode 的起点选最窄覆盖事实，不保证 fact->node 是整个实参；
     * 用嵌套所有权表达式核查参数文档是否会误显示子节点诊断。 */
    ownershipFact = ZrParser_SemanticFacts_FindOwnershipAtPosition(analyzer->semanticContext,
                                                                   argumentNode->location);
    ownershipIntrinsicFact =
        ZrParser_SemanticFacts_FindOwnershipIntrinsicByNode(
                analyzer->semanticContext, argumentNode);

    factBuffer[0] = '\0';
    if (!signature_fact_append_expression_detail(factBuffer, sizeof(factBuffer), &factLength, expressionFact) ||
        !signature_fact_append_numeric_detail(factBuffer, sizeof(factBuffer), &factLength, numericFact) ||
        !signature_fact_append_logical_detail(factBuffer, sizeof(factBuffer), &factLength, logicalFact) ||
        !signature_fact_append_ownership_detail(factBuffer, sizeof(factBuffer), &factLength, ownershipFact) ||
        !signature_fact_append_ownership_intrinsic_detail(
                factBuffer,
                sizeof(factBuffer),
                &factLength,
                ownershipIntrinsicFact) ||
        factLength == 0) {
        return ZR_NULL;
    }

    docBuffer[0] = '\0';
    if (!signature_fact_append_format(docBuffer,
                                      sizeof(docBuffer),
                                      &docLength,
                                      "Argument semantic facts: %s",
                                      factBuffer) ||
        docLength == 0) {
        return ZR_NULL;
    }

    return ZrCore_String_Create(state, docBuffer, docLength);
}
