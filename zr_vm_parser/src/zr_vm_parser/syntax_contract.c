#include "zr_vm_parser/syntax_contract.h"

#include "zr_vm_parser/semantic.h"

#include <string.h>

TZrBool ZrParser_SyntaxParameter_Normalize(
        SZrSemanticContext *context,
        const SZrParameter *parameter,
        TZrTypeId valueTypeId,
        SZrCanonicalParameterContract *outContract) {
    EZrCanonicalRefAccess access = ZR_CANONICAL_REF_WRITABLE;

    if (context == ZR_NULL || parameter == ZR_NULL || outContract == ZR_NULL ||
        valueTypeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }

    memset(outContract, 0, sizeof(*outContract));
    outContract->typeId = valueTypeId;
    outContract->passingForm = ZR_CANONICAL_PASSING_VALUE;
    outContract->escapeUpperBound = ZR_CANONICAL_ESCAPE_FUNCTION;
    outContract->entryInitialization = ZR_CANONICAL_ENTRY_INITIALIZED;
    outContract->exitInitialization = ZR_CANONICAL_EXIT_UNCHANGED;
    outContract->acceptsTemporary = ZR_TRUE;
    outContract->callSiteMarker = ZR_CANONICAL_CALL_SITE_NONE;

    switch (parameter->sourcePassingForm) {
        case ZR_PARAMETER_SOURCE_VALUE:
            return ZR_TRUE;
        case ZR_PARAMETER_SOURCE_IN:
            access = ZR_CANONICAL_REF_READONLY;
            outContract->passingForm = ZR_CANONICAL_PASSING_IN;
            break;
        case ZR_PARAMETER_SOURCE_REF:
            outContract->passingForm = ZR_CANONICAL_PASSING_REF;
            outContract->escapeUpperBound = ZR_CANONICAL_ESCAPE_CALLER;
            outContract->acceptsTemporary = ZR_FALSE;
            outContract->callSiteMarker = ZR_CANONICAL_CALL_SITE_REF;
            break;
        case ZR_PARAMETER_SOURCE_REF_READONLY:
            access = ZR_CANONICAL_REF_READONLY;
            outContract->passingForm = ZR_CANONICAL_PASSING_REF_READONLY;
            outContract->escapeUpperBound = ZR_CANONICAL_ESCAPE_CALLER;
            outContract->acceptsTemporary = ZR_FALSE;
            outContract->callSiteMarker = ZR_CANONICAL_CALL_SITE_REF;
            break;
        case ZR_PARAMETER_SOURCE_SCOPED_REF:
            outContract->passingForm = ZR_CANONICAL_PASSING_REF;
            outContract->escapeUpperBound = ZR_CANONICAL_ESCAPE_BLOCK;
            outContract->acceptsTemporary = ZR_FALSE;
            outContract->callSiteMarker = ZR_CANONICAL_CALL_SITE_REF;
            break;
        case ZR_PARAMETER_SOURCE_SCOPED_REF_READONLY:
            access = ZR_CANONICAL_REF_READONLY;
            outContract->passingForm = ZR_CANONICAL_PASSING_REF_READONLY;
            outContract->escapeUpperBound = ZR_CANONICAL_ESCAPE_BLOCK;
            outContract->acceptsTemporary = ZR_FALSE;
            outContract->callSiteMarker = ZR_CANONICAL_CALL_SITE_REF;
            break;
        case ZR_PARAMETER_SOURCE_OUT:
            outContract->passingForm = ZR_CANONICAL_PASSING_OUT;
            outContract->entryInitialization = ZR_CANONICAL_ENTRY_UNINITIALIZED;
            outContract->exitInitialization = ZR_CANONICAL_EXIT_DEFINITELY_INITIALIZED;
            outContract->acceptsTemporary = ZR_FALSE;
            outContract->callSiteMarker = ZR_CANONICAL_CALL_SITE_OUT;
            break;
        default:
            return ZR_FALSE;
    }

    outContract->typeId = ZrParser_CanonicalType_InternRef(context, valueTypeId, access);
    return outContract->typeId != ZR_SEMANTIC_ID_INVALID;
}

TZrTypeId ZrParser_SyntaxCallable_Intern(
        SZrSemanticContext *context,
        const SZrAstNodeArray *parameters,
        const TZrTypeId *parameterTypeIds,
        TZrTypeId returnTypeId,
        EZrCanonicalReceiverEffect receiverEffect,
        TZrUInt32 effectFlags) {
    SZrArray contracts;
    TZrTypeId result;

    if (context == ZR_NULL || returnTypeId == ZR_SEMANTIC_ID_INVALID ||
        (parameters != ZR_NULL && parameters->count > 0u && parameterTypeIds == ZR_NULL)) {
        return ZR_SEMANTIC_ID_INVALID;
    }

    ZrCore_Array_Init(context->state, &contracts, sizeof(SZrCanonicalParameterContract),
                      parameters != ZR_NULL && parameters->count > 0u ? parameters->count : 1u);
    if (parameters != ZR_NULL) {
        for (TZrSize index = 0u; index < parameters->count; index++) {
            SZrCanonicalParameterContract contract;
            SZrAstNode *node = parameters->nodes[index];
            if (node == ZR_NULL || node->type != ZR_AST_PARAMETER ||
                !ZrParser_SyntaxParameter_Normalize(
                        context, &node->data.parameter, parameterTypeIds[index], &contract)) {
                ZrCore_Array_Free(context->state, &contracts);
                return ZR_SEMANTIC_ID_INVALID;
            }
            ZrCore_Array_Push(context->state, &contracts, &contract);
        }
    }

    result = ZrParser_CanonicalType_InternFunction(
            context,
            (const SZrCanonicalParameterContract *)contracts.head,
            contracts.length,
            returnTypeId,
            receiverEffect,
            effectFlags);
    ZrCore_Array_Free(context->state, &contracts);
    return result;
}
