#include "compiler_internal.h"

#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_core/artifact_schema.h"

#include "module_init_analysis.h"
#include "type_inference_internal.h"

#include <string.h>

typedef struct SZrSchedulerArtifactTransportFingerprint {
    TZrMetadataToken schedulerToken;
    TZrMetadataToken taskToken;
    TZrMetadataToken jobToken;
    TZrMetadataToken scheduleMemberToken;
    TZrMetadataToken scheduleSignatureToken;
    TZrUInt64 scheduleSignatureHash;
    TZrUInt64 schedulerModuleHash;
    TZrUInt64 taskModuleHash;
    TZrUInt64 jobModuleHash;
    TZrUInt32 abiVersion;
    TZrUInt32 policyMask;
    TZrUInt32 attachedRequirementFlags;
    TZrUInt32 isolatedRequirementFlags;
} SZrSchedulerArtifactTransportFingerprint;

typedef struct SZrSchedulerArtifactNativeProviderFingerprint {
    TZrUInt64 moduleSignatureHash;
    TZrUInt64 typeNameHash;
    TZrUInt32 schemaVersion;
    TZrUInt32 genericParameterCount;
} SZrSchedulerArtifactNativeProviderFingerprint;

static TZrUInt64 compiler_scheduler_artifact_hash_string(const SZrString *value) {
    const TZrChar *text;

    if (value == ZR_NULL) {
        return 0u;
    }
    text = ZrCore_String_GetNativeString((SZrString *)value);
    return text != ZR_NULL ? ZrCore_Artifact_HashBytes((const TZrByte *)text, strlen(text)) : 0u;
}

static TZrUInt32 compiler_scheduler_artifact_token_rid(TZrUInt64 hash) {
    TZrUInt32 rid = (TZrUInt32)(hash & ZR_METADATA_TOKEN_RID_MASK);
    return rid != 0u ? rid : 1u;
}

static TZrBool compiler_scheduler_artifact_resolve_native_provider(
        const SZrTypePrototypeInfo *prototype,
        const SZrString *baseName,
        SZrFunctionArtifactSourceTypeIdentity *outProvider) {
    SZrSchedulerArtifactNativeProviderFingerprint fingerprint;
    TZrUInt64 signatureHash;

    if (prototype == ZR_NULL || !prototype->isNativeRuntime ||
        prototype->importModuleName == ZR_NULL || baseName == ZR_NULL ||
        outProvider == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Memory_RawSet(&fingerprint, 0, sizeof(fingerprint));
    fingerprint.moduleSignatureHash =
            compiler_scheduler_artifact_hash_string(prototype->importModuleName);
    fingerprint.typeNameHash = compiler_scheduler_artifact_hash_string(baseName);
    fingerprint.schemaVersion = 1u;
    fingerprint.genericParameterCount = (TZrUInt32)prototype->genericParameters.length;
    if (fingerprint.moduleSignatureHash == 0u || fingerprint.typeNameHash == 0u) {
        return ZR_FALSE;
    }
    signatureHash = ZrCore_Artifact_HashBytes(
            (const TZrByte *)&fingerprint, sizeof(fingerprint));
    if (signatureHash == 0u) {
        return ZR_FALSE;
    }
    outProvider->metadataToken = ZR_METADATA_TOKEN_MAKE(
            ZR_METADATA_TABLE_TYPE_DEF,
            compiler_scheduler_artifact_token_rid(
                    fingerprint.moduleSignatureHash ^ fingerprint.typeNameHash));
    outProvider->signatureToken = ZR_METADATA_TOKEN_MAKE(
            ZR_METADATA_TABLE_SIGNATURE,
            compiler_scheduler_artifact_token_rid(signatureHash));
    outProvider->signatureHash = signatureHash;
    outProvider->layoutVersion = fingerprint.schemaVersion;
    outProvider->layoutHash = ZrCore_Artifact_HashBytes(
            (const TZrByte *)&fingerprint.typeNameHash,
            sizeof(fingerprint.typeNameHash));
    outProvider->moduleSignatureHash = fingerprint.moduleSignatureHash;
    return outProvider->layoutHash != 0u;
}

static TZrBool compiler_scheduler_artifact_provider_equal(
        const SZrFunctionArtifactSourceTypeIdentity *left,
        const SZrFunctionArtifactSourceTypeIdentity *right) {
    return left != ZR_NULL && right != ZR_NULL &&
           left->metadataToken == right->metadataToken &&
           left->signatureToken == right->signatureToken &&
           left->signatureHash == right->signatureHash &&
           left->layoutVersion == right->layoutVersion &&
           left->layoutHash == right->layoutHash &&
           left->moduleSignatureHash == right->moduleSignatureHash;
}

static SZrString *compiler_scheduler_artifact_base_type_name(
        SZrCompilerState *cs,
        SZrString *typeName) {
    const TZrChar *text;
    const TZrChar *genericStart;

    if (cs == ZR_NULL || cs->state == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }
    text = ZrCore_String_GetNativeString(typeName);
    if (text == ZR_NULL) {
        return ZR_NULL;
    }
    genericStart = strchr(text, '<');
    if (genericStart == ZR_NULL) {
        return typeName;
    }
    return ZrCore_String_Create(
            cs->state, (TZrNativeString)text, (TZrSize)(genericStart - text));
}

static TZrBool compiler_scheduler_artifact_resolve_provider(
        SZrCompilerState *cs,
        const SZrInferredType *type,
        SZrFunctionArtifactSourceTypeIdentity *outProvider) {
    SZrTypePrototypeInfo *prototype;
    const SZrParserModuleInitSummary *summary;
    SZrString *baseName;

    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->state->global == ZR_NULL ||
        type == ZR_NULL || type->typeName == ZR_NULL || outProvider == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Memory_RawSet(outProvider, 0, sizeof(*outProvider));
    prototype = find_compiler_type_prototype_inference(cs, type->typeName);
    if (prototype == ZR_NULL || !prototype->isImportedNative ||
        prototype->importModuleName == ZR_NULL) {
        return ZR_FALSE;
    }
    baseName = compiler_scheduler_artifact_base_type_name(cs, prototype->name);
    if (baseName == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!ZrParser_ModuleInitAnalysis_EnsureSummary(cs, prototype->importModuleName)) {
        return compiler_scheduler_artifact_resolve_native_provider(
                prototype, baseName, outProvider);
    }
    summary = ZrParser_ModuleInitAnalysis_FindSummary(
            cs->state->global, prototype->importModuleName);
    if (summary == ZR_NULL || !summary->typeDefs.isValid) {
        return compiler_scheduler_artifact_resolve_native_provider(
                prototype, baseName, outProvider);
    }
    for (TZrSize index = 0u; index < summary->typeDefs.length; ++index) {
        const SZrModuleInitTypeDefInfo *typeDef =
                (const SZrModuleInitTypeDefInfo *)ZrCore_Array_Get(
                        (SZrArray *)&summary->typeDefs, index);
        if (typeDef == ZR_NULL || typeDef->name == ZR_NULL ||
            !ZrCore_String_Equal(typeDef->name, baseName)) {
            continue;
        }
        if (typeDef->metadataToken == 0u || typeDef->signatureToken == 0u ||
            typeDef->signatureHash == 0u || typeDef->layoutVersion == 0u ||
            typeDef->layoutHash == 0u || summary->moduleSignatureHash == 0u) {
            return ZR_FALSE;
        }
        outProvider->metadataToken = typeDef->metadataToken;
        outProvider->signatureToken = typeDef->signatureToken;
        outProvider->signatureHash = typeDef->signatureHash;
        outProvider->layoutVersion = typeDef->layoutVersion;
        outProvider->layoutHash = typeDef->layoutHash;
        outProvider->moduleSignatureHash = summary->moduleSignatureHash;
        return ZR_TRUE;
    }
    return compiler_scheduler_artifact_resolve_native_provider(
            prototype, baseName, outProvider);
}

static TZrUInt64 compiler_scheduler_artifact_transport_hash(
        const SZrFunctionSchedulerSourceFact *fact) {
    SZrSchedulerArtifactTransportFingerprint fingerprint;

    if (fact == ZR_NULL) {
        return 0u;
    }
    ZrCore_Memory_RawSet(&fingerprint, 0, sizeof(fingerprint));
    fingerprint.schedulerToken = fact->schedulerProvider.metadataToken;
    fingerprint.taskToken = fact->taskProvider.metadataToken;
    fingerprint.jobToken = fact->jobProvider.metadataToken;
    fingerprint.scheduleMemberToken = fact->scheduleMemberToken;
    fingerprint.scheduleSignatureToken = fact->scheduleSignatureToken;
    fingerprint.scheduleSignatureHash = fact->scheduleSignatureHash;
    fingerprint.schedulerModuleHash = fact->schedulerProvider.moduleSignatureHash;
    fingerprint.taskModuleHash = fact->taskProvider.moduleSignatureHash;
    fingerprint.jobModuleHash = fact->jobProvider.moduleSignatureHash;
    fingerprint.abiVersion = fact->schedulerAbiVersion;
    fingerprint.policyMask = fact->schedulerPolicyMask;
    fingerprint.attachedRequirementFlags = fact->attachedRequirementFlags;
    fingerprint.isolatedRequirementFlags = fact->isolatedRequirementFlags;
    return ZrCore_Artifact_HashBytes((const TZrByte *)&fingerprint, sizeof(fingerprint));
}

static TZrBool compiler_scheduler_artifact_fact_equal(
        const SZrFunctionSchedulerSourceFact *left,
        const SZrFunctionSchedulerSourceFact *right) {
    return left != ZR_NULL && right != ZR_NULL &&
           left->schedulerTypeId == right->schedulerTypeId &&
           left->taskTypeId == right->taskTypeId &&
           left->jobTypeId == right->jobTypeId &&
           left->schedulerAbiVersion == right->schedulerAbiVersion &&
           left->scheduleMemberToken == right->scheduleMemberToken &&
           left->scheduleSignatureToken == right->scheduleSignatureToken &&
           left->scheduleSignatureHash == right->scheduleSignatureHash &&
           left->schedulerProtocolMask == right->schedulerProtocolMask &&
           left->contractRole == right->contractRole &&
           left->schedulerPolicyMask == right->schedulerPolicyMask &&
           left->attachedRequirementFlags == right->attachedRequirementFlags &&
           left->isolatedRequirementFlags == right->isolatedRequirementFlags &&
           left->transportContractHash == right->transportContractHash &&
           left->schedulerContractHash == right->schedulerContractHash &&
           compiler_scheduler_artifact_provider_equal(
                   &left->schedulerProvider, &right->schedulerProvider) &&
           compiler_scheduler_artifact_provider_equal(
                   &left->taskProvider, &right->taskProvider) &&
           compiler_scheduler_artifact_provider_equal(
                   &left->jobProvider, &right->jobProvider);
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
        const SZrTypeMemberInfo *memberInfo,
        const SZrResolvedCallSignature *resolvedSignature) {
    SZrFunctionSchedulerSourceFact fact;

    if (cs == ZR_NULL || receiverType == ZR_NULL || memberInfo == ZR_NULL ||
        resolvedSignature == ZR_NULL ||
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
    fact.schedulerAbiVersion = 1u;
    fact.schedulerPolicyMask = ZR_ARTIFACT_SCHEDULER_POLICY_ISOLATED_DOMAIN;
    fact.isolatedRequirementFlags = ZR_ARTIFACT_SCHEDULER_REQUIREMENT_SEND;
    {
        fact.taskTypeId = ZrParser_CanonicalType_FromInferred(
                cs->semanticContext, &resolvedSignature->returnType);
        (void)compiler_scheduler_artifact_resolve_provider(
                cs, &resolvedSignature->returnType, &fact.taskProvider);
    }
    if (resolvedSignature->parameterTypes.isValid &&
        resolvedSignature->parameterTypes.length == 1u) {
        const SZrInferredType *jobType = (const SZrInferredType *)ZrCore_Array_Get(
                (SZrArray *)&resolvedSignature->parameterTypes, 0u);
        if (jobType != ZR_NULL) {
            fact.jobTypeId = ZrParser_CanonicalType_FromInferred(
                    cs->semanticContext, jobType);
            (void)compiler_scheduler_artifact_resolve_provider(
                    cs, jobType, &fact.jobProvider);
        }
    }
    (void)compiler_scheduler_artifact_resolve_provider(
            cs, receiverType, &fact.schedulerProvider);
    fact.schedulerContractHash = fact.scheduleSignatureHash;
    fact.transportContractHash = compiler_scheduler_artifact_transport_hash(&fact);
    return compiler_scheduler_artifact_append_fact(cs, &fact);
}
