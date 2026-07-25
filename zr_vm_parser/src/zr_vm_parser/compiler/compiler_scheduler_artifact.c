#include "compiler_internal.h"

#include "zr_vm_parser/canonical_type.h"

#include "type_inference_internal.h"

static TZrBool compiler_scheduler_artifact_fact_equal(
        const SZrFunctionSchedulerSourceFact *left,
        const SZrFunctionSchedulerSourceFact *right) {
    return left != ZR_NULL && right != ZR_NULL &&
           left->schedulerTypeId == right->schedulerTypeId &&
           left->scheduleMemberToken == right->scheduleMemberToken &&
           left->scheduleSignatureToken == right->scheduleSignatureToken &&
           left->scheduleSignatureHash == right->scheduleSignatureHash &&
           left->schedulerProtocolMask == right->schedulerProtocolMask &&
           left->contractRole == right->contractRole;
}

static TZrBool compiler_scheduler_artifact_append_fact(
        SZrCompilerState *cs,
        const SZrFunctionSchedulerSourceFact *fact) {
    SZrFunction *function;
    SZrFunctionSchedulerSourceFact *facts;
    TZrUInt32 nextLength;

    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->state->global == ZR_NULL ||
        cs->currentFunction == ZR_NULL || fact == ZR_NULL) {
        return ZR_FALSE;
    }
    function = cs->currentFunction;
    for (TZrUInt32 index = 0u; index < function->schedulerSourceFactLength; ++index) {
        if (compiler_scheduler_artifact_fact_equal(&function->schedulerSourceFacts[index], fact)) {
            return ZR_TRUE;
        }
    }
    if (function->schedulerSourceFactLength == UINT32_MAX) {
        return ZR_FALSE;
    }

    nextLength = function->schedulerSourceFactLength + 1u;
    facts = (SZrFunctionSchedulerSourceFact *)ZrCore_Memory_RawMallocWithType(
            cs->state->global,
            sizeof(SZrFunctionSchedulerSourceFact) * nextLength,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (facts == ZR_NULL) {
        return ZR_FALSE;
    }
    if (function->schedulerSourceFacts != ZR_NULL && function->schedulerSourceFactLength > 0u) {
        ZrCore_Memory_RawCopy(facts,
                              function->schedulerSourceFacts,
                              sizeof(SZrFunctionSchedulerSourceFact) * function->schedulerSourceFactLength);
        ZrCore_Memory_RawFreeWithType(
                cs->state->global,
                function->schedulerSourceFacts,
                sizeof(SZrFunctionSchedulerSourceFact) * function->schedulerSourceFactLength,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    facts[function->schedulerSourceFactLength] = *fact;
    function->schedulerSourceFacts = facts;
    function->schedulerSourceFactLength = nextLength;
    return ZR_TRUE;
}

TZrBool compiler_scheduler_artifact_record_resolved_call(
        SZrCompilerState *cs,
        const SZrInferredType *receiverType,
        const SZrTypeMemberInfo *memberInfo) {
    SZrFunctionSchedulerSourceFact fact;

    if (cs == ZR_NULL || receiverType == ZR_NULL || memberInfo == ZR_NULL ||
        memberInfo->contractRole != ZR_MEMBER_CONTRACT_ROLE_TASK_SCHEDULER_SCHEDULE) {
        return ZR_TRUE;
    }
    if (!inferred_type_implements_protocol_mask(
                cs,
                receiverType,
                ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_TASK_SCHEDULER))) {
        return ZR_TRUE;
    }
    if (memberInfo->metadataToken == 0u || memberInfo->signatureToken == 0u ||
        memberInfo->signatureHash == 0u || cs->semanticContext == ZR_NULL) {
        return ZR_TRUE;
    }

    ZrCore_Memory_RawSet(&fact, 0, sizeof(fact));
    fact.schedulerTypeId = ZrParser_CanonicalType_FromInferred(
            cs->semanticContext,
            receiverType);
    if (fact.schedulerTypeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_TRUE;
    }
    fact.scheduleMemberToken = memberInfo->metadataToken;
    fact.scheduleSignatureToken = memberInfo->signatureToken;
    fact.scheduleSignatureHash = memberInfo->signatureHash;
    fact.schedulerProtocolMask = ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_TASK_SCHEDULER);
    fact.contractRole = memberInfo->contractRole;
    return compiler_scheduler_artifact_append_fact(cs, &fact);
}
