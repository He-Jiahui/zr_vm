#ifndef ZR_VM_REFLECTION_GENERIC_ARGUMENT_INTERNAL_H
#define ZR_VM_REFLECTION_GENERIC_ARGUMENT_INTERNAL_H

#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_core/reflection.h"

TZrBool ZrCore_Reflection_ValidateGenericTypeArgument(
        SZrMetadataRuntime *runtime,
        const SZrReflectionGenericTypeArgument *argument,
        TZrUInt32 depth);

TZrBool ZrCore_Reflection_GenericTypeArgumentMatchesSignatureNode(
        SZrMetadataRuntime *runtime,
        const SZrZrpMetadataPoolSliceView *candidateBlob,
        const SZrMetadataRuntimeSignatureTypeNodeView *candidateNode,
        TZrMetadataToken resolvedCandidateToken,
        const SZrReflectionGenericTypeArgument *argument,
        TZrUInt32 depth);

#endif
