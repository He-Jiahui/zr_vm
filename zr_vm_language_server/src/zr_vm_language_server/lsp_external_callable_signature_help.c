#include "lsp_external_callable_signature_help.h"

#include "semantic/lsp_external_callable_contract.h"
#include "semantic/lsp_semantic_query.h"
#include "zr_vm_parser/semantic_query.h"

#include <stdio.h>
#include <string.h>

static const TZrChar *external_signature_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }
    return value->shortStringLength < ZR_VM_LONG_STRING_FLAG
                   ? ZrCore_String_GetNativeStringShort(value)
                   : ZrCore_String_GetNativeString(value);
}

static SZrString *external_signature_parameter_documentation(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *argumentNode,
        const TZrChar *descriptorDocumentation) {
    SZrString *argumentDocumentation =
            argumentNode != ZR_NULL
                    ? ZrLanguageServer_Lsp_BuildSignatureArgumentSemanticFactDocumentation(
                              state, analyzer, argumentNode)
                    : ZR_NULL;
    const TZrChar *argumentText =
            external_signature_string_text(argumentDocumentation);
    SZrString *result;
    TZrChar *buffer;
    TZrSize descriptorLength;
    TZrSize argumentLength;
    TZrSize totalLength;

    if (descriptorDocumentation == ZR_NULL ||
        descriptorDocumentation[0] == '\0') {
        return argumentDocumentation;
    }
    if (argumentText == ZR_NULL || argumentText[0] == '\0') {
        return ZrCore_String_Create(
                state,
                (TZrNativeString)descriptorDocumentation,
                strlen(descriptorDocumentation));
    }

    descriptorLength = strlen(descriptorDocumentation);
    argumentLength = strlen(argumentText);
    totalLength = descriptorLength + 2U + argumentLength;
    buffer = (TZrChar *)ZrCore_Memory_RawMalloc(
            state->global, totalLength + 1U);
    if (buffer == ZR_NULL) {
        return ZR_NULL;
    }
    snprintf(buffer,
             totalLength + 1U,
             "%s\n\n%s",
             descriptorDocumentation,
             argumentText);
    result = ZrCore_String_Create(state, buffer, totalLength);
    ZrCore_Memory_RawFree(state->global, buffer, totalLength + 1U);
    return result;
}

static SZrLspSignatureInformation *external_signature_first(
        SZrLspSignatureHelp *help) {
    SZrLspSignatureInformation **signaturePtr;

    if (help == ZR_NULL || help->signatures.length == 0U) {
        return ZR_NULL;
    }
    signaturePtr = (SZrLspSignatureInformation **)ZrCore_Array_Get(
            &help->signatures, 0U);
    return signaturePtr != ZR_NULL ? *signaturePtr : ZR_NULL;
}

static TZrBool external_signature_append_parameters(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        const SZrLspExternalCallableContract *contract,
        SZrAstNodeArray *argumentNodes,
        SZrLspSignatureHelp *help) {
    SZrLspSignatureInformation *signature = external_signature_first(help);

    if (state == ZR_NULL || analyzer == ZR_NULL || contract == ZR_NULL ||
        signature == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0U; index < contract->parameterCount; index++) {
        SZrLspParameterInformation *parameter;
        TZrChar label[ZR_LSP_TEXT_BUFFER_LENGTH];
        SZrAstNode *argumentNode =
                argumentNodes != ZR_NULL && index < argumentNodes->count
                        ? argumentNodes->nodes[index]
                        : ZR_NULL;

        if (!ZrLanguageServer_LspExternalCallableContract_FormatParameter(
                    contract, index, label, sizeof(label))) {
            return ZR_FALSE;
        }
        parameter = (SZrLspParameterInformation *)ZrCore_Memory_RawMalloc(
                state->global, sizeof(SZrLspParameterInformation));
        if (parameter == ZR_NULL) {
            return ZR_FALSE;
        }
        memset(parameter, 0, sizeof(*parameter));
        parameter->label = ZrCore_String_Create(state, label, strlen(label));
        parameter->documentation = external_signature_parameter_documentation(
                state,
                analyzer,
                argumentNode,
                contract->parameters[index].documentation);
        if (parameter->label == ZR_NULL) {
            ZrCore_Memory_RawFree(
                    state->global, parameter, sizeof(*parameter));
            return ZR_FALSE;
        }
        ZrCore_Array_Push(state, &signature->parameters, &parameter);
    }
    return ZR_TRUE;
}

EZrLspExternalCallableSignatureStatus
ZrLanguageServer_LspExternalCallableSignatureHelp_Resolve(
        SZrState *state,
        SZrLspContext *context,
        SZrSemanticAnalyzer *analyzer,
        SZrString *uri,
        SZrFileRange calleeRange,
        SZrAstNodeArray *argumentNodes,
        TZrInt32 activeParameter,
        SZrLspSignatureHelp **result) {
    SZrLspSemanticQuery query;
    SZrLspRange lspRange;
    SZrLspExternalCallableContract contract;
    SZrParserSemanticCallQuery callQuery;
    TZrChar label[ZR_LSP_LONG_TEXT_BUFFER_LENGTH];
    EZrLspExternalCallableSignatureStatus status =
            ZR_LSP_EXTERNAL_CALLABLE_SIGNATURE_NOT_EXTERNAL;

    if (state == ZR_NULL || context == ZR_NULL || analyzer == ZR_NULL ||
        uri == ZR_NULL || result == ZR_NULL) {
        return ZR_LSP_EXTERNAL_CALLABLE_SIGNATURE_UNAVAILABLE;
    }

    ZrLanguageServer_LspSemanticQuery_Init(&query);
    lspRange = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(
            context, uri, calleeRange);
    if (!ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(
                state, context, uri, lspRange.start, &query) ||
        (query.sourceKind != ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_BUILTIN &&
         query.sourceKind !=
                 ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_DESCRIPTOR_PLUGIN)) {
        goto cleanup;
    }

    if (query.kind != ZR_LSP_SEMANTIC_QUERY_TARGET_IMPORTED_MEMBER &&
        query.kind !=
                ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_TYPE_MEMBER) {
        goto cleanup;
    }

    status = ZR_LSP_EXTERNAL_CALLABLE_SIGNATURE_UNAVAILABLE;
    if (query.resolvedMember.memberKind == ZR_LSP_METADATA_MEMBER_FUNCTION) {
        if (!ZrLanguageServer_LspExternalCallableContract_FromResolvedMember(
                    &query.resolvedMember, &contract)) {
            goto cleanup;
        }
    } else if (query.resolvedMember.memberKind ==
                       ZR_LSP_METADATA_MEMBER_METHOD) {
        if (query.resolvedMember.methodDescriptor != ZR_NULL &&
            query.resolvedMember.methodDescriptor->isStatic) {
            status = ZR_LSP_EXTERNAL_CALLABLE_SIGNATURE_NOT_EXTERNAL;
            goto cleanup;
        }
        if (analyzer->semanticContext == ZR_NULL ||
            !ZrParser_SemanticQuery_CallAt(
                    analyzer->semanticContext,
                    calleeRange,
                    ZR_NULL,
                    &callQuery) ||
            !ZrLanguageServer_LspExternalCallableContract_FromResolvedMethod(
                    &query.resolvedMember,
                    analyzer->semanticContext,
                    callQuery.callableTypeId,
                    &contract)) {
            goto cleanup;
        }
    } else {
        status = ZR_LSP_EXTERNAL_CALLABLE_SIGNATURE_NOT_EXTERNAL;
        goto cleanup;
    }

    if (!ZrLanguageServer_LspExternalCallableContract_Format(
                &contract, label, sizeof(label)) ||
        !ZrLanguageServer_LspSignatureHelp_PopulateFromLabel(
                state,
                analyzer,
                label,
                ZR_NULL,
                ZR_NULL,
                ZR_NULL,
                activeParameter,
                result) ||
        *result == ZR_NULL ||
        !external_signature_append_parameters(
                state, analyzer, &contract, argumentNodes, *result)) {
        if (*result != ZR_NULL) {
            ZrLanguageServer_LspSignatureHelp_Free(state, *result);
            *result = ZR_NULL;
        }
        goto cleanup;
    }
    status = ZR_LSP_EXTERNAL_CALLABLE_SIGNATURE_RESOLVED;

cleanup:
    ZrLanguageServer_LspSemanticQuery_Free(state, &query);
    return status;
}
