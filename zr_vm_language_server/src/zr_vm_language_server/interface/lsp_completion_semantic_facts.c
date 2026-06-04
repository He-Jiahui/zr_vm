#include "interface/lsp_interface_internal.h"
#include "semantic/semantic_analyzer_internal.h"

#include "zr_vm_parser/semantic_facts.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

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

static TZrBool completion_fact_is_float_numeric_fact(const SZrSemanticNumericFact *fact) {
    return fact != ZR_NULL &&
           (fact->sourceType == ZR_VALUE_TYPE_FLOAT ||
            fact->sourceType == ZR_VALUE_TYPE_DOUBLE ||
            fact->targetType == ZR_VALUE_TYPE_FLOAT ||
            fact->targetType == ZR_VALUE_TYPE_DOUBLE);
}

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

static TZrBool completion_fact_append_numeric_detail(TZrChar *buffer,
                                                     TZrSize bufferSize,
                                                     TZrSize *used,
                                                     const SZrSemanticNumericFact *fact) {
    if (fact == ZR_NULL) {
        return ZR_TRUE;
    }

    if (fact->hasRange) {
        if (completion_fact_is_float_numeric_fact(fact)) {
            if (!completion_fact_append_format(buffer,
                                               bufferSize,
                                               used,
                                               "range %.17g..%.17g",
                                               fact->minDoubleValue,
                                               fact->maxDoubleValue)) {
                return ZR_FALSE;
            }
        } else if (!completion_fact_append_format(buffer,
                                                  bufferSize,
                                                  used,
                                                  "range %lld..%lld",
                                                  (long long)fact->minValue,
                                                  (long long)fact->maxValue)) {
            return ZR_FALSE;
        }
    }

    if (fact->hasUnsignedRange) {
        if (*used > 0 &&
            !completion_fact_append_format(buffer, bufferSize, used, ", ")) {
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
        if (*used > 0 &&
            !completion_fact_append_format(buffer, bufferSize, used, ", ")) {
            return ZR_FALSE;
        }
        if (!completion_fact_append_format(buffer, bufferSize, used, "may overflow")) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool completion_fact_append_logical_detail(TZrChar *buffer,
                                                     TZrSize bufferSize,
                                                     TZrSize *used,
                                                     const SZrSemanticLogicalFact *fact) {
    if (fact == ZR_NULL) {
        return ZR_TRUE;
    }

    if (fact->hasKnownValue) {
        if (*used > 0 &&
            !completion_fact_append_format(buffer, bufferSize, used, ", ")) {
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
        if (*used > 0 &&
            !completion_fact_append_format(buffer, bufferSize, used, ", ")) {
            return ZR_FALSE;
        }
        if (!completion_fact_append_format(buffer, bufferSize, used, "short-circuits")) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static const TZrChar *completion_fact_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        return ZrCore_String_GetNativeStringShort(value);
    }

    return ZrCore_String_GetNativeString(value);
}

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
    if (*used > 0 &&
        !completion_fact_append_format(buffer, bufferSize, used, ", ")) {
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

static void completion_fact_materialize_initializer(SZrState *state,
                                                    SZrSemanticAnalyzer *analyzer,
                                                    SZrAstNode *initializer) {
    SZrInferredType inferredType;

    if (state == ZR_NULL ||
        analyzer == ZR_NULL ||
        initializer == ZR_NULL ||
        analyzer->semanticContext == ZR_NULL) {
        return;
    }

    if (ZrParser_SemanticFacts_FindExpressionByNode(analyzer->semanticContext, initializer) != ZR_NULL) {
        return;
    }

    ZrParser_InferredType_Init(state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    (void)ZrLanguageServer_SemanticAnalyzer_InferExactExpressionType(state, analyzer, initializer, &inferredType);
    ZrParser_InferredType_Free(state, &inferredType);
}

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

void ZrLanguageServer_Lsp_EnrichCompletionItemSemanticFacts(SZrState *state,
                                                            SZrSemanticAnalyzer *analyzer,
                                                            SZrSymbol *symbol,
                                                            SZrCompletionItem *item) {
    SZrAstNode *initializer;
    const SZrSemanticNumericFact *numericFact;
    const SZrSemanticLogicalFact *logicalFact;
    const SZrSemanticOwnershipFact *ownershipFact;
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

    completion_fact_materialize_initializer(state, analyzer, initializer);
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(analyzer->semanticContext, initializer);
    logicalFact = ZrParser_SemanticFacts_FindLogicalByNode(analyzer->semanticContext, initializer);
    ownershipFact = ZrParser_SemanticFacts_FindOwnershipAtPosition(analyzer->semanticContext, initializer->location);

    factDetail[0] = '\0';
    if (!completion_fact_append_numeric_detail(factDetail, sizeof(factDetail), &used, numericFact) ||
        !completion_fact_append_logical_detail(factDetail, sizeof(factDetail), &used, logicalFact) ||
        !completion_fact_append_ownership_detail(factDetail, sizeof(factDetail), &used, ownershipFact) ||
        used == 0) {
        return;
    }

    item->detail = completion_fact_append_detail(state, item->detail, factDetail);
}
