#include "zr_vm_core/reflection.h"

#include "reflection_generic_argument_internal.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/metadata_runtime.h"

static void reflection_clear_resolved_generic_method_spec(
        SZrReflectionResolvedGenericMethodSpec *methodSpec) {
    if (methodSpec != ZR_NULL) {
        ZrCore_Memory_RawSet(methodSpec, 0, sizeof(*methodSpec));
    }
}

static TZrBool reflection_method_spec_matches_request(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken methodSpecToken,
        const SZrMetadataRuntimeGenericOwnerView *ownerView,
        const SZrReflectionGenericTypeArgument *arguments,
        TZrUInt32 argumentCount,
        SZrMetadataRuntimeMethodSpecSignatureView *outView) {
    SZrMetadataRuntimeMethodSpecSignatureView view;
    TZrUInt32 index;

    if (!ZrCore_MetadataRuntime_ReadMethodSpecSignatureView(runtime, methodSpecToken, &view) ||
        view.methodToken != ownerView->ownerToken ||
        view.methodRecord != ownerView->ownerRecord ||
        view.argumentCount != argumentCount) {
        return ZR_FALSE;
    }

    for (index = 0u; index < argumentCount; ++index) {
        SZrMetadataRuntimeMethodSpecGenericArgumentView argumentView;
        if (!ZrCore_MetadataRuntime_ReadMethodSpecGenericArgumentView(
                    runtime, methodSpecToken, index, &argumentView) ||
            !ZrCore_Reflection_GenericTypeArgumentMatchesSignatureNode(
                    runtime,
                    &argumentView.signatureView.blob,
                    &argumentView.argumentNode,
                    argumentView.argumentToken,
                    &arguments[index],
                    0u)) {
            return ZR_FALSE;
        }
    }

    *outView = view;
    return ZR_TRUE;
}

static TZrBool reflection_resolve_generic_method_from_records(
        SZrMetadataRuntime *runtime,
        const SZrMetadataTokenRecord *records,
        TZrUInt32 recordCount,
        const SZrMetadataRuntimeGenericOwnerView *ownerView,
        const SZrReflectionGenericTypeArgument *arguments,
        TZrUInt32 argumentCount,
        SZrReflectionResolvedGenericMethodSpec *outMethodSpec) {
    TZrUInt32 index;

    if (records == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0u; index < recordCount; ++index) {
        SZrMetadataRuntimeMethodSpecSignatureView view;
        if (ZR_METADATA_TOKEN_TABLE(records[index].token) != ZR_METADATA_TABLE_SIGNATURE ||
            !reflection_method_spec_matches_request(runtime,
                                                    records[index].token,
                                                    ownerView,
                                                    arguments,
                                                    argumentCount,
                                                    &view)) {
            continue;
        }

        outMethodSpec->methodSpecToken = view.methodSpecToken;
        outMethodSpec->methodSpecRecord = view.methodSpecRecord;
        outMethodSpec->genericMethodToken = view.methodToken;
        outMethodSpec->genericMethodRecord = view.methodRecord;
        outMethodSpec->genericSignatureHash = view.signatureHash;
        outMethodSpec->genericArgumentCount = view.argumentCount;
        outMethodSpec->genericArgumentListBlobOffset = view.argumentListBlobOffset;
        outMethodSpec->requestedArguments = arguments;
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

TZrBool ZrCore_Reflection_ResolveConstructedGenericMethod(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken genericMethodToken,
        const SZrReflectionGenericTypeArgument *arguments,
        TZrUInt32 argumentCount,
        SZrReflectionResolvedGenericMethodSpec *outMethodSpec) {
    SZrMetadataRuntimeGenericOwnerView ownerView;
    SZrFunction *metadataFunction;
    TZrUInt32 index;

    reflection_clear_resolved_generic_method_spec(outMethodSpec);
    if (runtime == ZR_NULL || outMethodSpec == ZR_NULL || arguments == ZR_NULL || argumentCount == 0u ||
        ZR_METADATA_TOKEN_TABLE(genericMethodToken) != ZR_METADATA_TABLE_MEMBER_DEF ||
        !ZrCore_MetadataRuntime_ReadGenericOwnerView(runtime, genericMethodToken, &ownerView) ||
        ownerView.methodDefRow == ZR_NULL || ownerView.genericParamCount != argumentCount) {
        return ZR_FALSE;
    }

    for (index = 0u; index < argumentCount; ++index) {
        if (!ZrCore_Reflection_ValidateGenericTypeArgument(runtime, &arguments[index], 0u)) {
            return ZR_FALSE;
        }
    }

    metadataFunction = runtime->metadataFunction;
    if (metadataFunction == ZR_NULL) {
        return ZR_FALSE;
    }
    if (reflection_resolve_generic_method_from_records(
                runtime,
                metadataFunction->metadataTokenRecords,
                metadataFunction->metadataTokenRecordLength,
                &ownerView,
                arguments,
                argumentCount,
                outMethodSpec)) {
        return ZR_TRUE;
    }
    return reflection_resolve_generic_method_from_records(
            runtime,
            metadataFunction->moduleMetadataTokenRecords,
            metadataFunction->moduleMetadataTokenRecordLength,
            &ownerView,
            arguments,
            argumentCount,
            outMethodSpec);
}
