#include "interface/lsp_interface_internal.h"
#include "semantic/semantic_analyzer_internal.h"

#include "zr_vm_parser/semantic_facts.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

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

static TZrBool signature_fact_is_float_numeric_fact(const SZrSemanticNumericFact *fact) {
    return fact != ZR_NULL &&
           (fact->sourceType == ZR_VALUE_TYPE_FLOAT ||
            fact->sourceType == ZR_VALUE_TYPE_DOUBLE ||
            fact->targetType == ZR_VALUE_TYPE_FLOAT ||
            fact->targetType == ZR_VALUE_TYPE_DOUBLE);
}

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

static TZrBool signature_fact_append_separator(TZrChar *buffer,
                                               TZrSize bufferSize,
                                               TZrSize *used) {
    if (used == ZR_NULL || *used == 0) {
        return ZR_TRUE;
    }

    return signature_fact_append_format(buffer, bufferSize, used, ", ");
}

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
        if (signature_fact_is_float_numeric_fact(fact)) {
            if (!signature_fact_append_format(buffer,
                                              bufferSize,
                                              used,
                                              "range %.17g..%.17g",
                                              fact->minDoubleValue,
                                              fact->maxDoubleValue)) {
                return ZR_FALSE;
            }
        } else if (!signature_fact_append_format(buffer,
                                                 bufferSize,
                                                 used,
                                                 "range %lld..%lld",
                                                 (long long)fact->minValue,
                                                 (long long)fact->maxValue)) {
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

static const TZrChar *signature_fact_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        return ZrCore_String_GetNativeStringShort(value);
    }

    return ZrCore_String_GetNativeString(value);
}

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

static void signature_fact_materialize_argument(SZrState *state,
                                                SZrSemanticAnalyzer *analyzer,
                                                SZrAstNode *argumentNode) {
    SZrInferredType inferredType;

    if (state == ZR_NULL ||
        analyzer == ZR_NULL ||
        analyzer->semanticContext == ZR_NULL ||
        analyzer->compilerState == ZR_NULL ||
        argumentNode == ZR_NULL) {
        return;
    }

    if (ZrParser_SemanticFacts_FindExpressionByNode(analyzer->semanticContext, argumentNode) != ZR_NULL) {
        return;
    }

    ZrParser_InferredType_Init(state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    (void)ZrLanguageServer_SemanticAnalyzer_InferExactExpressionType(state,
                                                                     analyzer,
                                                                     argumentNode,
                                                                     &inferredType);
    ZrParser_InferredType_Free(state, &inferredType);
}

SZrString *ZrLanguageServer_Lsp_BuildSignatureArgumentSemanticFactDocumentation(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *argumentNode) {
    const SZrSemanticExpressionFact *expressionFact;
    const SZrSemanticNumericFact *numericFact;
    const SZrSemanticLogicalFact *logicalFact;
    const SZrSemanticOwnershipFact *ownershipFact;
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

    signature_fact_materialize_argument(state, analyzer, argumentNode);

    expressionFact = ZrParser_SemanticFacts_FindExpressionByNode(analyzer->semanticContext, argumentNode);
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(analyzer->semanticContext, argumentNode);
    logicalFact = ZrParser_SemanticFacts_FindLogicalByNode(analyzer->semanticContext, argumentNode);
    ownershipFact = ZrParser_SemanticFacts_FindOwnershipAtPosition(analyzer->semanticContext,
                                                                   argumentNode->location);

    factBuffer[0] = '\0';
    if (!signature_fact_append_expression_detail(factBuffer, sizeof(factBuffer), &factLength, expressionFact) ||
        !signature_fact_append_numeric_detail(factBuffer, sizeof(factBuffer), &factLength, numericFact) ||
        !signature_fact_append_logical_detail(factBuffer, sizeof(factBuffer), &factLength, logicalFact) ||
        !signature_fact_append_ownership_detail(factBuffer, sizeof(factBuffer), &factLength, ownershipFact) ||
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
