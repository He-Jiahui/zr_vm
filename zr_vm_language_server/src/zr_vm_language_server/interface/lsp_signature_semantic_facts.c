#include "interface/lsp_interface_internal.h"
#include "semantic/semantic_analyzer_internal.h"

#include "zr_vm_parser/semantic_facts.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

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

    numericFact = ZrParser_SemanticFacts_FindNumericByNode(analyzer->semanticContext, argumentNode);
    logicalFact = ZrParser_SemanticFacts_FindLogicalByNode(analyzer->semanticContext, argumentNode);
    ownershipFact = ZrParser_SemanticFacts_FindOwnershipAtPosition(analyzer->semanticContext,
                                                                   argumentNode->location);

    factBuffer[0] = '\0';
    if (!signature_fact_append_numeric_detail(factBuffer, sizeof(factBuffer), &factLength, numericFact) ||
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
