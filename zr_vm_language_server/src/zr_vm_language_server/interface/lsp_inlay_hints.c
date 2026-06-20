#include "interface/lsp_interface_internal.h"
#include "semantic/semantic_analyzer_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static int lsp_inlay_compare_position(SZrLspPosition left, SZrLspPosition right) {
    if (left.line < right.line) {
        return -1;
    }
    if (left.line > right.line) {
        return 1;
    }
    if (left.character < right.character) {
        return -1;
    }
    if (left.character > right.character) {
        return 1;
    }
    return 0;
}

static TZrBool lsp_inlay_position_in_range(SZrLspPosition position, SZrLspRange range) {
    return lsp_inlay_compare_position(position, range.start) >= 0 &&
           lsp_inlay_compare_position(position, range.end) <= 0;
}

static TZrBool lsp_inlay_append_format(TZrChar *buffer,
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

static TZrBool lsp_inlay_symbol_has_exact_type_text(SZrState *state,
                                                    SZrSymbol *symbol,
                                                    TZrChar *buffer,
                                                    TZrSize bufferSize,
                                                    const TZrChar **outTypeText) {
    if (outTypeText != ZR_NULL) {
        *outTypeText = ZR_NULL;
    }
    if (state == ZR_NULL ||
        symbol == ZR_NULL ||
        buffer == ZR_NULL ||
        bufferSize == 0 ||
        outTypeText == ZR_NULL ||
        symbol->typeInfo == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_IsPreciseInferredType(symbol->typeInfo)) {
        return ZR_FALSE;
    }

    *outTypeText = ZrParser_TypeNameString_Get(state, symbol->typeInfo, buffer, bufferSize);
    return *outTypeText != ZR_NULL && (*outTypeText)[0] != '\0';
}

static TZrBool lsp_inlay_append_hint(SZrState *state,
                                     SZrArray *result,
                                     SZrLspPosition position,
                                     TZrInt32 kind,
                                     const TZrChar *labelText) {
    TZrChar labelBuffer[ZR_LSP_LONG_TEXT_BUFFER_LENGTH];
    int labelLength;
    SZrLspInlayHint *hint;

    if (state == ZR_NULL || result == ZR_NULL || labelText == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspInlayHint *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }

    labelLength = snprintf(labelBuffer, sizeof(labelBuffer), ": %s", labelText);
    if (labelLength <= 0 || (TZrSize)labelLength >= sizeof(labelBuffer)) {
        return ZR_FALSE;
    }

    hint = (SZrLspInlayHint *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspInlayHint));
    if (hint == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(hint, 0, sizeof(*hint));
    hint->position = position;
    hint->kind = kind;
    hint->paddingLeft = ZR_TRUE;
    hint->paddingRight = ZR_FALSE;
    hint->label = ZrCore_String_Create(state, labelBuffer, (TZrSize)labelLength);
    if (hint->label == ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, hint, sizeof(SZrLspInlayHint));
        return ZR_FALSE;
    }

    ZrCore_Array_Push(state, result, &hint);
    return ZR_TRUE;
}

static TZrBool lsp_inlay_is_float_numeric_fact(const SZrSemanticNumericFact *fact) {
    return fact != ZR_NULL &&
           (fact->sourceType == ZR_VALUE_TYPE_FLOAT ||
            fact->sourceType == ZR_VALUE_TYPE_DOUBLE ||
            fact->targetType == ZR_VALUE_TYPE_FLOAT ||
            fact->targetType == ZR_VALUE_TYPE_DOUBLE);
}

static TZrBool lsp_inlay_append_numeric_fact_detail(TZrChar *buffer,
                                                    TZrSize bufferSize,
                                                    TZrSize *used,
                                                    const SZrSemanticNumericFact *fact) {
    if (fact == ZR_NULL) {
        return ZR_TRUE;
    }

    if (fact->hasRange) {
        if (lsp_inlay_is_float_numeric_fact(fact)) {
            if (!lsp_inlay_append_format(buffer,
                                         bufferSize,
                                         used,
                                         ", range %.17g..%.17g",
                                         fact->minDoubleValue,
                                         fact->maxDoubleValue)) {
                return ZR_FALSE;
            }
        } else if (!lsp_inlay_append_format(buffer,
                                            bufferSize,
                                            used,
                                            ", range %lld..%lld",
                                            (long long)fact->minValue,
                                            (long long)fact->maxValue)) {
            return ZR_FALSE;
        }
    }

    if (fact->hasUnsignedRange &&
        !lsp_inlay_append_format(buffer,
                                 bufferSize,
                                 used,
                                 ", unsigned %llu..%llu",
                                 (unsigned long long)fact->minUnsignedValue,
                                 (unsigned long long)fact->maxUnsignedValue)) {
        return ZR_FALSE;
    }

    if (fact->mayOverflow &&
        !lsp_inlay_append_format(buffer, bufferSize, used, ", may overflow")) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool lsp_inlay_append_logical_fact_detail(TZrChar *buffer,
                                                    TZrSize bufferSize,
                                                    TZrSize *used,
                                                    const SZrSemanticLogicalFact *fact) {
    if (fact == ZR_NULL) {
        return ZR_TRUE;
    }

    if (fact->hasKnownValue &&
        !lsp_inlay_append_format(buffer,
                                 bufferSize,
                                 used,
                                 ", logical %s",
                                 fact->knownValue ? "true" : "false")) {
        return ZR_FALSE;
    }

    if (fact->kind == ZR_SEMANTIC_LOGICAL_FACT_SHORT_CIRCUIT &&
        !lsp_inlay_append_format(buffer, bufferSize, used, ", short-circuits")) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static void lsp_inlay_materialize_initializer_facts(SZrState *state,
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

static TZrBool lsp_inlay_build_label_text(SZrState *state,
                                          SZrSemanticAnalyzer *analyzer,
                                          SZrSymbol *symbol,
                                          const TZrChar *typeText,
                                          TZrChar *buffer,
                                          TZrSize bufferSize) {
    SZrAstNode *astNode;
    SZrAstNode *initializer;
    const SZrSemanticNumericFact *numericFact = ZR_NULL;
    const SZrSemanticLogicalFact *logicalFact = ZR_NULL;
    TZrSize used = 0;

    if (state == ZR_NULL ||
        symbol == ZR_NULL ||
        typeText == ZR_NULL ||
        buffer == ZR_NULL ||
        bufferSize == 0) {
        return ZR_FALSE;
    }

    if (!lsp_inlay_append_format(buffer, bufferSize, &used, "%s", typeText)) {
        return ZR_FALSE;
    }

    astNode = symbol->astNode;
    initializer = astNode != ZR_NULL && astNode->type == ZR_AST_VARIABLE_DECLARATION
                      ? astNode->data.variableDeclaration.value
                      : ZR_NULL;
    if (initializer == ZR_NULL || analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL) {
        return ZR_TRUE;
    }

    lsp_inlay_materialize_initializer_facts(state, analyzer, initializer);
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(analyzer->semanticContext, initializer);
    logicalFact = ZrParser_SemanticFacts_FindLogicalByNode(analyzer->semanticContext, initializer);

    return lsp_inlay_append_numeric_fact_detail(buffer, bufferSize, &used, numericFact) &&
           lsp_inlay_append_logical_fact_detail(buffer, bufferSize, &used, logicalFact);
}

static TZrBool lsp_inlay_try_append_symbol_hint(SZrState *state,
                                                SZrLspContext *context,
                                                SZrString *uri,
                                                SZrSemanticAnalyzer *analyzer,
                                                SZrSymbol *symbol,
                                                SZrLspRange range,
                                                SZrArray *result) {
    TZrChar typeBuffer[ZR_LSP_LONG_TEXT_BUFFER_LENGTH];
    TZrChar labelBuffer[ZR_LSP_LONG_TEXT_BUFFER_LENGTH];
    const TZrChar *typeText = ZR_NULL;
    SZrLspPosition position;
    SZrAstNode *astNode;

    if (!lsp_inlay_symbol_has_exact_type_text(state, symbol, typeBuffer, sizeof(typeBuffer), &typeText)) {
        return ZR_TRUE;
    }

    astNode = symbol != ZR_NULL ? symbol->astNode : ZR_NULL;
    if (astNode == ZR_NULL) {
        return ZR_TRUE;
    }

    switch (astNode->type) {
        case ZR_AST_VARIABLE_DECLARATION:
            if (astNode->data.variableDeclaration.typeInfo != ZR_NULL ||
                astNode->data.variableDeclaration.pattern == ZR_NULL ||
                astNode->data.variableDeclaration.pattern->type != ZR_AST_IDENTIFIER_LITERAL) {
                return ZR_TRUE;
            }
            position = ZrLanguageServer_Lsp_PositionFromFilePositionForDocument(context, uri, symbol->selectionRange.end);
            break;

        case ZR_AST_CLASS_FIELD:
            if (astNode->data.classField.typeInfo != ZR_NULL) {
                return ZR_TRUE;
            }
            position = ZrLanguageServer_Lsp_PositionFromFilePositionForDocument(context, uri, symbol->selectionRange.end);
            break;

        case ZR_AST_STRUCT_FIELD:
            if (astNode->data.structField.typeInfo != ZR_NULL) {
                return ZR_TRUE;
            }
            position = ZrLanguageServer_Lsp_PositionFromFilePositionForDocument(context, uri, symbol->selectionRange.end);
            break;

        case ZR_AST_FUNCTION_DECLARATION:
            if (astNode->data.functionDeclaration.returnType != ZR_NULL) {
                return ZR_TRUE;
            }
            position = astNode->data.functionDeclaration.body != ZR_NULL
                           ? ZrLanguageServer_Lsp_PositionFromFilePositionForDocument(
                                 context,
                                 uri,
                                 astNode->data.functionDeclaration.body->location.start)
                           : ZrLanguageServer_Lsp_PositionFromFilePositionForDocument(
                                 context,
                                 uri,
                                 symbol->selectionRange.end);
            break;

        case ZR_AST_CLASS_METHOD:
            if (astNode->data.classMethod.returnType != ZR_NULL) {
                return ZR_TRUE;
            }
            position = astNode->data.classMethod.body != ZR_NULL
                           ? ZrLanguageServer_Lsp_PositionFromFilePositionForDocument(
                                 context,
                                 uri,
                                 astNode->data.classMethod.body->location.start)
                           : ZrLanguageServer_Lsp_PositionFromFilePositionForDocument(
                                 context,
                                 uri,
                                 symbol->selectionRange.end);
            break;

        case ZR_AST_STRUCT_METHOD:
            if (astNode->data.structMethod.returnType != ZR_NULL) {
                return ZR_TRUE;
            }
            position = astNode->data.structMethod.body != ZR_NULL
                           ? ZrLanguageServer_Lsp_PositionFromFilePositionForDocument(
                                 context,
                                 uri,
                                 astNode->data.structMethod.body->location.start)
                           : ZrLanguageServer_Lsp_PositionFromFilePositionForDocument(
                                 context,
                                 uri,
                                 symbol->selectionRange.end);
            break;

        default:
            return ZR_TRUE;
    }

    if (!lsp_inlay_position_in_range(position, range)) {
        return ZR_TRUE;
    }

    if (!lsp_inlay_build_label_text(state, analyzer, symbol, typeText, labelBuffer, sizeof(labelBuffer))) {
        return ZR_FALSE;
    }

    return lsp_inlay_append_hint(state, result, position, ZR_LSP_INLAY_HINT_KIND_TYPE, labelBuffer);
}

TZrBool ZrLanguageServer_Lsp_GetInlayHints(SZrState *state,
                                           SZrLspContext *context,
                                           SZrString *uri,
                                           SZrLspRange range,
                                           SZrArray *result) {
    SZrSemanticAnalyzer *analyzer;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspInlayHint *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }

    analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL || analyzer->symbolTable == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize scopeIndex = 0; scopeIndex < analyzer->symbolTable->allScopes.length; scopeIndex++) {
        SZrSymbolScope **scopePtr =
            (SZrSymbolScope **)ZrCore_Array_Get(&analyzer->symbolTable->allScopes, scopeIndex);
        if (scopePtr == ZR_NULL || *scopePtr == ZR_NULL) {
            continue;
        }

        for (TZrSize symbolIndex = 0; symbolIndex < (*scopePtr)->symbols.length; symbolIndex++) {
            SZrSymbol **symbolPtr = (SZrSymbol **)ZrCore_Array_Get(&(*scopePtr)->symbols, symbolIndex);
            if (symbolPtr == ZR_NULL || *symbolPtr == ZR_NULL || (*symbolPtr)->location.source == ZR_NULL) {
                continue;
            }

            if (!ZrLanguageServer_Lsp_StringsEqual((*symbolPtr)->location.source, uri) &&
                !ZrLanguageServer_Lsp_UrisResolveToSameNativePath((*symbolPtr)->location.source, uri)) {
                continue;
            }

            if (!lsp_inlay_try_append_symbol_hint(state, context, uri, analyzer, *symbolPtr, range, result)) {
                ZrLanguageServer_Lsp_FreeInlayHints(state, result);
                return ZR_FALSE;
            }
        }
    }

    return ZR_TRUE;
}

void ZrLanguageServer_Lsp_FreeInlayHints(SZrState *state, SZrArray *result) {
    if (state == ZR_NULL || result == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < result->length; index++) {
        SZrLspInlayHint **hintPtr = (SZrLspInlayHint **)ZrCore_Array_Get(result, index);
        if (hintPtr != ZR_NULL && *hintPtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *hintPtr, sizeof(SZrLspInlayHint));
        }
    }

    ZrCore_Array_Free(state, result);
}
