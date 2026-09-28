#include "interface/lsp_interface_internal.h"
#include "semantic/lsp_numeric_range_text.h"

#include "zr_vm_parser/semantic_facts.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* 补全元数据拼接要同时兼容 VM 的短串和长串；返回的视图只在原字符串存活期间有效。 */
static void completion_fact_get_string_view(SZrString *value,
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

/* 各类事实共享固定容量的展示缓冲区；任一段装不下时让上层保留原补全描述。 */
static TZrBool completion_fact_append_format(TZrChar *buffer,
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

/* 只有先前已有事实时才插入分隔符，使独立事实可在同一详情字段组合。 */
static TZrBool completion_fact_append_separator(TZrChar *buffer,
                                                TZrSize bufferSize,
                                                TZrSize *used) {
    if (used != ZR_NULL &&
        *used > 0 &&
        !completion_fact_append_format(buffer, bufferSize, used, ", ")) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

/* 将 parser 的表达式分类投影为面向编辑器的稳定词汇；未知分类退回中性描述。 */
static const TZrChar *completion_fact_expression_kind(EZrSemanticExpressionFactKind kind) {
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

/* 在补全详情中保留事实精度，避免近似推断被读成已证实的值。 */
static const TZrChar *completion_fact_exactness(EZrSemanticFactExactness exactness) {
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

/* 字符串常量来自 parser 语义事实；展示前转义控制字节，避免补全详情失去可读边界。 */
static TZrBool completion_fact_append_escaped_string_constant(TZrChar *buffer,
                                                              TZrSize bufferSize,
                                                              TZrSize *used,
                                                              SZrString *value) {
    TZrNativeString text;
    TZrSize length;
    TZrSize index;
    unsigned char byte;

    completion_fact_get_string_view(value, &text, &length);
    if (!completion_fact_append_format(buffer, bufferSize, used, "constant \"")) {
        return ZR_FALSE;
    }

    for (index = 0; text != ZR_NULL && index < length; index++) {
        byte = (unsigned char)text[index];
        switch (byte) {
            case '"':
                if (!completion_fact_append_format(buffer, bufferSize, used, "\\\"")) {
                    return ZR_FALSE;
                }
                break;
            case '\\':
                if (!completion_fact_append_format(buffer, bufferSize, used, "\\\\")) {
                    return ZR_FALSE;
                }
                break;
            case '\n':
                if (!completion_fact_append_format(buffer, bufferSize, used, "\\n")) {
                    return ZR_FALSE;
                }
                break;
            case '\r':
                if (!completion_fact_append_format(buffer, bufferSize, used, "\\r")) {
                    return ZR_FALSE;
                }
                break;
            case '\t':
                if (!completion_fact_append_format(buffer, bufferSize, used, "\\t")) {
                    return ZR_FALSE;
                }
                break;
            default:
                if (byte < 0x20u || byte == 0x7Fu) {
                    if (!completion_fact_append_format(buffer, bufferSize, used, "\\x%02X", (unsigned int)byte)) {
                        return ZR_FALSE;
                    }
                } else if (!completion_fact_append_format(buffer, bufferSize, used, "%c", (int)byte)) {
                    return ZR_FALSE;
                }
                break;
        }
    }

    return completion_fact_append_format(buffer, bufferSize, used, "\"");
}

/* 仅把有确定常量值的事实补到表达式摘要，未知值不推断也不占用展示空间。 */
static TZrBool completion_fact_append_expression_constant(TZrChar *buffer,
                                                          TZrSize bufferSize,
                                                          TZrSize *used,
                                                          const SZrSemanticExpressionFact *fact) {
    if (fact == ZR_NULL || !fact->hasConstant) {
        return ZR_TRUE;
    }

    switch (fact->valueKind) {
        case ZR_SEMANTIC_VALUE_KIND_BOOL:
            if (!completion_fact_append_separator(buffer, bufferSize, used)) {
                return ZR_FALSE;
            }
            return completion_fact_append_format(buffer,
                                                 bufferSize,
                                                 used,
                                                 "constant %s",
                                                 fact->constantValue.boolValue ? "true" : "false");
        case ZR_SEMANTIC_VALUE_KIND_INT64:
            if (!completion_fact_append_separator(buffer, bufferSize, used)) {
                return ZR_FALSE;
            }
            return completion_fact_append_format(buffer,
                                                 bufferSize,
                                                 used,
                                                 "constant %lld",
                                                 (long long)fact->constantValue.int64Value);
        case ZR_SEMANTIC_VALUE_KIND_UINT64:
            if (!completion_fact_append_separator(buffer, bufferSize, used)) {
                return ZR_FALSE;
            }
            return completion_fact_append_format(buffer,
                                                 bufferSize,
                                                 used,
                                                 "constant %llu",
                                                 (unsigned long long)fact->constantValue.uint64Value);
        case ZR_SEMANTIC_VALUE_KIND_DOUBLE:
            if (!completion_fact_append_separator(buffer, bufferSize, used)) {
                return ZR_FALSE;
            }
            return completion_fact_append_format(buffer,
                                                 bufferSize,
                                                 used,
                                                 "constant %.17g",
                                                 fact->constantValue.doubleValue);
        case ZR_SEMANTIC_VALUE_KIND_NULL:
            if (!completion_fact_append_separator(buffer, bufferSize, used)) {
                return ZR_FALSE;
            }
            return completion_fact_append_format(buffer, bufferSize, used, "constant null");
        case ZR_SEMANTIC_VALUE_KIND_STRING:
            if (!completion_fact_append_separator(buffer, bufferSize, used)) {
                return ZR_FALSE;
            }
            return completion_fact_append_escaped_string_constant(buffer,
                                                                  bufferSize,
                                                                  used,
                                                                  fact->constantValue.stringValue);
        case ZR_SEMANTIC_VALUE_KIND_UNKNOWN:
        default:
            return ZR_TRUE;
    }
}

/* 补全以变量初始化表达式为锚点，先呈现类别和精度，再附可证明的常量。 */
static TZrBool completion_fact_append_expression_detail(TZrChar *buffer,
                                                        TZrSize bufferSize,
                                                        TZrSize *used,
                                                        const SZrSemanticExpressionFact *fact) {
    if (fact == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!completion_fact_append_separator(buffer, bufferSize, used) ||
        !completion_fact_append_format(buffer,
                                       bufferSize,
                                       used,
                                       "expression %s %s",
                                       completion_fact_expression_kind(fact->kind),
                                       completion_fact_exactness(fact->exactness)) ||
        !completion_fact_append_expression_constant(buffer, bufferSize, used, fact)) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

/* 数值摘要与 hover、签名帮助共用区间文案；溢出标记不能被区间展示掩盖。 */
static TZrBool completion_fact_append_numeric_detail(TZrChar *buffer,
                                                     TZrSize bufferSize,
                                                     TZrSize *used,
                                                     const SZrSemanticNumericFact *fact) {
    if (fact == ZR_NULL) {
        return ZR_TRUE;
    }

    if (fact->hasRange) {
        if (!completion_fact_append_separator(buffer, bufferSize, used)) {
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
        if (!completion_fact_append_separator(buffer, bufferSize, used)) {
            return ZR_FALSE;
        }
        if (!completion_fact_append_format(buffer,
                                           bufferSize,
                                           used,
                                           "unsigned %llu..%llu",
                                           (unsigned long long)fact->minUnsignedValue,
                                           (unsigned long long)fact->maxUnsignedValue)) {
            return ZR_FALSE;
        }
    }

    if (fact->mayOverflow) {
        if (!completion_fact_append_separator(buffer, bufferSize, used)) {
            return ZR_FALSE;
        }
        if (!completion_fact_append_format(buffer, bufferSize, used, "may overflow")) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

/* 逻辑事实只显示 parser 已记录的确定值或短路性质，不从表达式文本重新推断。 */
static TZrBool completion_fact_append_logical_detail(TZrChar *buffer,
                                                     TZrSize bufferSize,
                                                     TZrSize *used,
                                                     const SZrSemanticLogicalFact *fact) {
    if (fact == ZR_NULL) {
        return ZR_TRUE;
    }

    if (fact->hasKnownValue) {
        if (!completion_fact_append_separator(buffer, bufferSize, used)) {
            return ZR_FALSE;
        }
        if (!completion_fact_append_format(buffer,
                                           bufferSize,
                                           used,
                                           "logical %s",
                                           fact->knownValue ? "true" : "false")) {
            return ZR_FALSE;
        }
    }

    if (fact->kind == ZR_SEMANTIC_LOGICAL_FACT_SHORT_CIRCUIT) {
        if (!completion_fact_append_separator(buffer, bufferSize, used)) {
            return ZR_FALSE;
        }
        if (!completion_fact_append_format(buffer, bufferSize, used, "short-circuits")) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

/* 所有权诊断消息仍由 VM 字符串持有；这里只借用零结尾文本供同步格式化。 */
static const TZrChar *completion_fact_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        return ZrCore_String_GetNativeStringShort(value);
    }

    return ZrCore_String_GetNativeString(value);
}

/* 仅将违反所有权约束的事实提升到补全详情，普通转移事实不作为诊断显示。 */
static TZrBool completion_fact_append_ownership_detail(TZrChar *buffer,
                                                       TZrSize bufferSize,
                                                       TZrSize *used,
                                                       const SZrSemanticOwnershipFact *fact) {
    const TZrChar *message;

    if (fact == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!fact->isViolation) {
        return ZR_TRUE;
    }

    message = completion_fact_string_text(fact->diagnosticMessage);
    if (!completion_fact_append_separator(buffer, bufferSize, used)) {
        return ZR_FALSE;
    }

    if (message != ZR_NULL && message[0] != '\0') {
        return completion_fact_append_format(buffer,
                                             bufferSize,
                                             used,
                                             "ownership violation: %s",
                                             message);
    }

    return completion_fact_append_format(buffer, bufferSize, used, "ownership violation");
}

/* 将语义层的内建所有权操作名映射到用户可见术语；新增操作需同步此映射。 */
static const TZrChar *completion_fact_ownership_intrinsic_operation(
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

/* 内建所有权操作的消费语义由 parser 判定，补全仅转述该事实。 */
static TZrBool completion_fact_append_ownership_intrinsic_detail(
        TZrChar *buffer,
        TZrSize bufferSize,
        TZrSize *used,
        const SZrOwnershipIntrinsicFact *fact) {
    const TZrChar *operation;

    if (fact == ZR_NULL) {
        return ZR_TRUE;
    }

    operation = completion_fact_ownership_intrinsic_operation(fact->operation);
    if (operation == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!completion_fact_append_separator(buffer, bufferSize, used)) {
        return ZR_FALSE;
    }

    return completion_fact_append_format(
            buffer,
            bufferSize,
            used,
            "ownership intrinsic %s %s",
            operation,
            fact->consuming ? "consuming" : "non-consuming");
}

/* 在已有补全描述后加入事实摘要，同时避免重复调用元数据增强时重复追加。 */
/* @note 返回值可能是原 detail；新字符串由 VM 管理，调用方不得手工释放。 */
static SZrString *completion_fact_append_detail(SZrState *state,
                                                SZrString *detail,
                                                const TZrChar *factDetail) {
    TZrNativeString detailText;
    TZrSize detailLength;
    TZrSize factLength;
    TZrChar buffer[ZR_LSP_LONG_TEXT_BUFFER_LENGTH];
    TZrSize used = 0;

    if (state == ZR_NULL || factDetail == ZR_NULL || factDetail[0] == '\0') {
        return detail;
    }

    completion_fact_get_string_view(detail, &detailText, &detailLength);
    factLength = strlen(factDetail);
    if (detailText != ZR_NULL && detailLength >= factLength) {
        for (TZrSize index = 0; index + factLength <= detailLength; index++) {
            if (memcmp(detailText + index, factDetail, factLength) == 0) {
                return detail;
            }
        }
    }

    buffer[0] = '\0';
    if (detailText != ZR_NULL && detailLength > 0) {
        if (!completion_fact_append_format(buffer, sizeof(buffer), &used, "%.*s", (int)detailLength, detailText) ||
            !completion_fact_append_format(buffer, sizeof(buffer), &used, "\n")) {
            return detail;
        }
    }
    if (!completion_fact_append_format(buffer, sizeof(buffer), &used, "Semantic facts: %s", factDetail)) {
        return detail;
    }

    return ZrCore_String_Create(state, buffer, used);
}

/** @brief 为变量补全项附加其初始化表达式的语义事实，供编辑器详情面板展示。
 *  @pre item 仍归补全结果持有，调用方维持 analyzer 和 symbol 在本次同步读取期间有效。
 *  @note 只有当前 semanticContext 认识该初始化节点才有事实；无事实或格式化失败不阻断补全。 */
void ZrLanguageServer_Lsp_EnrichCompletionItemSemanticFacts(SZrState *state,
                                                            SZrSemanticAnalyzer *analyzer,
                                                            SZrSymbol *symbol,
                                                            SZrCompletionItem *item) {
    SZrAstNode *initializer;
    const SZrSemanticExpressionFact *expressionFact;
    const SZrSemanticNumericFact *numericFact;
    const SZrSemanticLogicalFact *logicalFact;
    const SZrSemanticOwnershipFact *ownershipFact;
    const SZrOwnershipIntrinsicFact *ownershipIntrinsicFact;
    /* BUG: 可达的长字符串字面量事实超过 256 字节时，转义追加失败，下面整段事实均被丢弃。
     * parser 在 type_inference_semantic_facts.c 将字符串字面量记录为常量；补全入口仍返回成功。 */
    TZrChar factDetail[ZR_LSP_TEXT_BUFFER_LENGTH];
    TZrSize used = 0;

    if (state == ZR_NULL ||
        analyzer == ZR_NULL ||
        analyzer->semanticContext == ZR_NULL ||
        symbol == ZR_NULL ||
        item == ZR_NULL ||
        symbol->astNode == ZR_NULL ||
        symbol->astNode->type != ZR_AST_VARIABLE_DECLARATION) {
        return;
    }

    initializer = symbol->astNode->data.variableDeclaration.value;
    if (initializer == ZR_NULL) {
        return;
    }

    /* 只读取 parser 已绑定到初始化 AST 节点的事实，避免补全在旧源码或同名符号上猜测。 */
    expressionFact = ZrParser_SemanticFacts_FindExpressionByNode(analyzer->semanticContext, initializer);
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(analyzer->semanticContext, initializer);
    logicalFact = ZrParser_SemanticFacts_FindLogicalByNode(analyzer->semanticContext, initializer);
    /* TODO: FindOwnershipAtPosition 只用 initializer 范围的 start 选最窄覆盖事实；
     * 核查复杂初始化式中子节点的所有权诊断是否会错归给变量补全项。 */
    ownershipFact = ZrParser_SemanticFacts_FindOwnershipAtPosition(analyzer->semanticContext, initializer->location);
    ownershipIntrinsicFact =
        ZrParser_SemanticFacts_FindOwnershipIntrinsicByNode(
                analyzer->semanticContext, initializer);

    factDetail[0] = '\0';
    if (!completion_fact_append_expression_detail(factDetail, sizeof(factDetail), &used, expressionFact) ||
        !completion_fact_append_numeric_detail(factDetail, sizeof(factDetail), &used, numericFact) ||
        !completion_fact_append_logical_detail(factDetail, sizeof(factDetail), &used, logicalFact) ||
        !completion_fact_append_ownership_detail(factDetail, sizeof(factDetail), &used, ownershipFact) ||
        !completion_fact_append_ownership_intrinsic_detail(
                factDetail, sizeof(factDetail), &used, ownershipIntrinsicFact) ||
        used == 0) {
        return;
    }

    item->detail = completion_fact_append_detail(state, item->detail, factDetail);
}
