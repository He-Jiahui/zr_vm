#include "lsp_canonical_signature_help.h"

#include "zr_vm_parser/semantic_query.h"

#include <stdio.h>
#include <string.h>

static const TZrChar *canonical_signature_help_passing_prefix(
        EZrCanonicalPassingForm passingForm) {
    switch (passingForm) {
        case ZR_CANONICAL_PASSING_IN: return "in ";
        case ZR_CANONICAL_PASSING_REF: return "ref ";
        case ZR_CANONICAL_PASSING_REF_READONLY: return "ref readonly ";
        case ZR_CANONICAL_PASSING_OUT: return "out ";
        case ZR_CANONICAL_PASSING_VALUE:
        default: return "";
    }
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
        TZrChar typeLabel[ZR_LSP_TEXT_BUFFER_LENGTH];
        TZrChar parameterLabel[ZR_LSP_TEXT_BUFFER_LENGTH];
        int written;

        if (contract == ZR_NULL ||
            !ZrParser_CanonicalType_Format(
                    analyzer->semanticContext,
                    contract->typeId,
                    typeLabel,
                    sizeof(typeLabel))) {
            return ZR_FALSE;
        }
        written = snprintf(
                parameterLabel,
                sizeof(parameterLabel),
                "%s%s",
                canonical_signature_help_passing_prefix(contract->passingForm),
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
