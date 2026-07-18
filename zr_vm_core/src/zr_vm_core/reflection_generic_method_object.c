#include "zr_vm_core/reflection.h"

#include "reflection_object_internal.h"
#include "zr_vm_core/metadata_runtime.h"

static TZrBool generic_method_object_revalidate(
        SZrMetadataRuntime *runtime,
        const SZrReflectionResolvedGenericMethodSpec *input,
        SZrReflectionResolvedGenericMethodSpec *outResolved) {
    if (runtime == ZR_NULL || input == ZR_NULL || outResolved == ZR_NULL ||
        input->requestedArguments == ZR_NULL || input->genericArgumentCount == 0u ||
        !ZrCore_Reflection_ResolveConstructedGenericMethod(
                runtime,
                input->genericMethodToken,
                input->requestedArguments,
                input->genericArgumentCount,
                outResolved)) {
        return ZR_FALSE;
    }
    return (TZrBool)(
            outResolved->methodSpecToken == input->methodSpecToken &&
            outResolved->methodSpecRecord == input->methodSpecRecord &&
            outResolved->genericMethodToken == input->genericMethodToken &&
            outResolved->genericMethodRecord == input->genericMethodRecord &&
            outResolved->genericSignatureHash == input->genericSignatureHash &&
            outResolved->genericArgumentCount == input->genericArgumentCount &&
            outResolved->genericArgumentListBlobOffset ==
                    input->genericArgumentListBlobOffset);
}

SZrObject *ZrCore_Reflection_BuildConstructedGenericMethodObject(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        const SZrReflectionResolvedGenericMethodSpec *methodSpec) {
    SZrReflectionResolvedGenericMethodSpec resolved;
    SZrObject *definitionObject;
    SZrObject *methodObject = ZR_NULL;
    SZrObject *result = ZR_NULL;
    const SZrTypeValue *nameValue;
    TZrBool definitionPinned = ZR_FALSE;
    TZrBool methodPinned = ZR_FALSE;

    if (state == ZR_NULL ||
        !generic_method_object_revalidate(runtime, methodSpec, &resolved)) {
        return ZR_NULL;
    }

    definitionObject = ZrCore_Reflection_BuildGenericMethodDefinitionObject(
            state, runtime, resolved.genericMethodToken);
    if (definitionObject == ZR_NULL ||
        !ZrCore_Reflection_ObjectPinRaw(
                state,
                ZR_CAST_RAW_OBJECT_AS_SUPER(definitionObject),
                &definitionPinned)) {
        return ZR_NULL;
    }

    methodObject = ZrCore_Reflection_BuildMethodSpecGenericContextObject(
            state, runtime, resolved.methodSpecToken);
    if (methodObject == ZR_NULL ||
        !ZrCore_Reflection_ObjectPinRaw(
                state,
                ZR_CAST_RAW_OBJECT_AS_SUPER(methodObject),
                &methodPinned)) {
        goto cleanup;
    }

    nameValue = ZrCore_Reflection_ObjectGetFieldValue(
            state, definitionObject, "name");
    if (nameValue == ZR_NULL || nameValue->type != ZR_VALUE_TYPE_STRING ||
        !ZrCore_Reflection_ObjectSetString(
                state, methodObject, "kind", "constructedGenericMethod") ||
        !ZrCore_Reflection_ObjectSetFieldValue(
                state, methodObject, "name", nameValue) ||
        !ZrCore_Reflection_ObjectSetBool(
                state, methodObject, "isGenericMethodDefinition", ZR_FALSE) ||
        !ZrCore_Reflection_ObjectSetObject(
                state,
                methodObject,
                "genericMethodDefinition",
                definitionObject,
                ZR_VALUE_TYPE_OBJECT)) {
        goto cleanup;
    }
    result = methodObject;

cleanup:
    ZrCore_Reflection_ObjectUnpinRaw(
            state->global,
            methodObject != ZR_NULL
                    ? ZR_CAST_RAW_OBJECT_AS_SUPER(methodObject)
                    : ZR_NULL,
            methodPinned);
    ZrCore_Reflection_ObjectUnpinRaw(
            state->global,
            ZR_CAST_RAW_OBJECT_AS_SUPER(definitionObject),
            definitionPinned);
    return result;
}
