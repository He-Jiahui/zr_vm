#include "lsp_canonical_signature_help.h"

#include "zr_vm_parser/semantic_query.h"

#include <stdio.h>
#include <string.h>

static const TZrChar *canonical_signature_help_passing_prefix(
        const SZrCanonicalParameterContract *contract) {
    if (contract == ZR_NULL) {
        return "";
    }
    switch (contract->passingForm) {
        case ZR_CANONICAL_PASSING_IN: return "in ";
        case ZR_CANONICAL_PASSING_REF:
            return contract->escapeUpperBound == ZR_CANONICAL_ESCAPE_FUNCTION
                           ? "scoped ref "
                           : "ref ";
        case ZR_CANONICAL_PASSING_REF_READONLY:
            return contract->escapeUpperBound == ZR_CANONICAL_ESCAPE_FUNCTION
                           ? "scoped ref readonly "
                           : "ref readonly ";
        case ZR_CANONICAL_PASSING_OUT: return "out ";
        case ZR_CANONICAL_PASSING_VALUE:
        default: return "";
    }
}

static TZrTypeId canonical_signature_help_parameter_value_type_id(
        const SZrSemanticContext *context,
        const SZrCanonicalParameterContract *contract) {
    const SZrCanonicalTypeNode *type;

    if (context == ZR_NULL || contract == ZR_NULL) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    if (contract->passingForm == ZR_CANONICAL_PASSING_VALUE) {
        return contract->typeId;
    }
    type = ZrParser_CanonicalType_Find(context, contract->typeId);
    return type != ZR_NULL && type->kind == ZR_CANONICAL_TYPE_REF
                   ? type->data.refType.pointeeTypeId
                   : ZR_SEMANTIC_ID_INVALID;
}

static TZrBool canonical_signature_help_append_parameters(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        const SZrCanonicalTypeNode *functionType,
        SZrAstNodeArray *argumentNodes,
        SZrLspSignatureHelp *help) {
    SZrLspSignatureInformation **signaturePtr;
    SZrLspSignatureInformation *signature;
    TZrSize index;

    if (state == ZR_NULL || analyzer == ZR_NULL ||
        analyzer->semanticContext == ZR_NULL || functionType == ZR_NULL ||
        functionType->kind != ZR_CANONICAL_TYPE_FUNCTION || help == ZR_NULL ||
        help->signatures.length == 0) {
        return ZR_FALSE;
    }

    signaturePtr = (SZrLspSignatureInformation **)ZrCore_Array_Get(
            &help->signatures,
            0);
    signature = signaturePtr != ZR_NULL ? *signaturePtr : ZR_NULL;
    if (signature == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0;
         index < functionType->data.function.parameterContracts.length;
         index++) {
        const SZrCanonicalParameterContract *contract =
                (const SZrCanonicalParameterContract *)ZrCore_Array_Get(
                        (SZrArray *)&functionType->data.function.parameterContracts,
                        index);
        SZrLspParameterInformation *parameter;
        TZrTypeId valueTypeId;
        TZrChar typeLabel[ZR_LSP_TEXT_BUFFER_LENGTH];
        TZrChar parameterLabel[ZR_LSP_TEXT_BUFFER_LENGTH];
        int written;

        valueTypeId = canonical_signature_help_parameter_value_type_id(
                analyzer->semanticContext,
                contract);
        if (valueTypeId == ZR_SEMANTIC_ID_INVALID ||
            !ZrParser_CanonicalType_Format(
                    analyzer->semanticContext,
                    valueTypeId,
                    typeLabel,
                    sizeof(typeLabel))) {
            return ZR_FALSE;
        }
        written = snprintf(
                parameterLabel,
                sizeof(parameterLabel),
                "%s%s",
                canonical_signature_help_passing_prefix(contract),
                typeLabel);
        if (written < 0 || (TZrSize)written >= sizeof(parameterLabel)) {
            return ZR_FALSE;
        }

        parameter = (SZrLspParameterInformation *)ZrCore_Memory_RawMalloc(
                state->global,
                sizeof(SZrLspParameterInformation));
        if (parameter == ZR_NULL) {
            return ZR_FALSE;
        }
        memset(parameter, 0, sizeof(*parameter));
        parameter->label = ZrCore_String_Create(
                state,
                parameterLabel,
                (TZrSize)written);
        if (parameter->label == ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, parameter, sizeof(*parameter));
            return ZR_FALSE;
        }
        parameter->documentation =
                argumentNodes != ZR_NULL && index < argumentNodes->count
                        ? ZrLanguageServer_Lsp_BuildSignatureArgumentSemanticFactDocumentation(
                                  state,
                                  analyzer,
                                  argumentNodes->nodes[index])
                        : ZR_NULL;
        ZrCore_Array_Push(state, &signature->parameters, &parameter);
    }

    return ZR_TRUE;
}

TZrBool ZrLanguageServer_LspCanonicalSignatureHelp_Resolve(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrFileRange position,
        SZrAstNodeArray *argumentNodes,
        TZrInt32 activeParameter,
        SZrLspSignatureHelp **result) {
    SZrParserSemanticCallQuery query;
    const SZrCanonicalTypeNode *functionType;
    TZrChar label[ZR_LSP_LONG_TEXT_BUFFER_LENGTH];

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL ||
        result == ZR_NULL ||
        !ZrParser_SemanticQuery_CallAt(analyzer->semanticContext, position, ZR_NULL, &query) ||
        !ZrParser_SemanticQuery_FormatCall(
                analyzer->semanticContext, &query, label, sizeof(label))) {
        return ZR_FALSE;
    }
    functionType = ZrParser_CanonicalType_Find(
            analyzer->semanticContext,
            query.callableTypeId);
    if (functionType == ZR_NULL ||
        !ZrLanguageServer_LspSignatureHelp_PopulateFromLabel(state,
                                                             analyzer,
                                                             label,
                                                             ZR_NULL,
                                                             argumentNodes,
                                                             ZR_NULL,
                                                             activeParameter,
                                                             result) ||
        !canonical_signature_help_append_parameters(
                state,
                analyzer,
                functionType,
                argumentNodes,
                *result)) {
        if (result != ZR_NULL && *result != ZR_NULL) {
            ZrLanguageServer_LspSignatureHelp_Free(state, *result);
            *result = ZR_NULL;
        }
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_LspCanonicalSignatureHelp_TryGetResolvedCallReferenceRange(
        SZrSemanticAnalyzer *analyzer,
        SZrFileRange position,
        SZrFileRange *result) {
    SZrParserSemanticCallQuery query;

    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL ||
        result == ZR_NULL ||
        !ZrParser_SemanticQuery_CallAt(
                analyzer->semanticContext, position, ZR_NULL, &query) ||
        !query.hasResolvedTarget || query.reference == ZR_NULL ||
        !query.reference->isResolved) {
        return ZR_FALSE;
    }
    *result = query.reference->range;
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_LspCanonicalSignatureHelp_ResolveReceiverHover(
        SZrState *state,
        SZrLspContext *context,
        SZrSemanticAnalyzer *analyzer,
        SZrString *uri,
        SZrFileRange position,
        SZrLspHover **result) {
    SZrParserSemanticCallQuery query;
    const SZrCanonicalTypeNode *functionType;
    SZrLspHover *hover;
    SZrString *content;
    TZrChar label[ZR_LSP_LONG_TEXT_BUFFER_LENGTH];
    TZrChar markdown[ZR_LSP_LONG_TEXT_BUFFER_LENGTH];
    int written;

    if (state == ZR_NULL || context == ZR_NULL || analyzer == ZR_NULL ||
        analyzer->semanticContext == ZR_NULL || uri == ZR_NULL || result == ZR_NULL ||
        !ZrParser_SemanticQuery_CallAt(
                analyzer->semanticContext, position, ZR_NULL, &query) ||
        !query.hasResolvedTarget ||
        query.reference == ZR_NULL || !query.reference->isResolved ||
        position.start.offset < query.reference->range.start.offset ||
        position.start.offset > query.reference->range.end.offset ||
        !ZrParser_SemanticQuery_FormatCall(
                analyzer->semanticContext, &query, label, sizeof(label))) {
        return ZR_FALSE;
    }
    functionType = ZrParser_CanonicalType_Find(
            analyzer->semanticContext,
            query.callableTypeId);
    if (functionType == ZR_NULL ||
        functionType->kind != ZR_CANONICAL_TYPE_FUNCTION ||
        functionType->data.function.receiverEffect == ZR_CANONICAL_RECEIVER_NONE) {
        return ZR_FALSE;
    }

    written = snprintf(markdown, sizeof(markdown), "**call**\n\nSignature: %s", label);
    if (written < 0 || (TZrSize)written >= sizeof(markdown)) {
        return ZR_FALSE;
    }
    content = ZrCore_String_Create(state, markdown, (TZrSize)written);
    hover = (SZrLspHover *)ZrCore_Memory_RawMalloc(
            state->global,
            sizeof(SZrLspHover));
    if (content == ZR_NULL || hover == ZR_NULL) {
        if (hover != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, hover, sizeof(*hover));
        }
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state, &hover->contents, sizeof(SZrString *), 1U);
    ZrCore_Array_Push(state, &hover->contents, &content);
    hover->range = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(
            context,
            uri,
            query.reference->range);
    *result = hover;
    return ZR_TRUE;
}
