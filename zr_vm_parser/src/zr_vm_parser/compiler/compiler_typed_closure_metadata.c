#include "compiler_internal.h"

static TZrBool compiler_typed_closure_binding_matches_source(
        const SZrFunctionClosureVariable *capture,
        const SZrTypeBinding *binding) {
    if (capture == ZR_NULL || binding == ZR_NULL ||
        capture->symbolId == ZR_SEMANTIC_ID_INVALID ||
        capture->typeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }

    if (binding->symbolId != capture->symbolId || binding->typeId != capture->typeId) {
        return ZR_FALSE;
    }
    if (!binding->hasDeclarationRange) {
        return ZR_TRUE;
    }
    return binding->declarationRange.start.line >= 0 &&
           binding->declarationRange.start.column >= 0 &&
           binding->declarationRange.end.line >= 0 &&
           binding->declarationRange.end.column >= 0 &&
           (TZrUInt32)binding->declarationRange.start.line == capture->declarationStartLine &&
           (TZrUInt32)binding->declarationRange.start.column == capture->declarationStartColumn &&
           (TZrUInt32)binding->declarationRange.end.line == capture->declarationEndLine &&
           (TZrUInt32)binding->declarationRange.end.column == capture->declarationEndColumn;
}

static TZrBool compiler_typed_closure_callable_matches_source(
        const SZrFunctionClosureVariable *capture,
        const SZrFunctionTypeInfo *callable) {
    if (capture == ZR_NULL || callable == ZR_NULL ||
        capture->symbolId == ZR_SEMANTIC_ID_INVALID ||
        capture->typeId == ZR_SEMANTIC_ID_INVALID ||
        callable->symbolId != capture->symbolId ||
        callable->typeId != capture->typeId ||
        !callable->hasDeclarationRange) {
        return ZR_FALSE;
    }

    return (TZrBool)(
            (TZrUInt32)callable->declarationRange.start.line == capture->declarationStartLine &&
            (TZrUInt32)callable->declarationRange.start.column == capture->declarationStartColumn &&
            (TZrUInt32)callable->declarationRange.end.line == capture->declarationEndLine &&
            (TZrUInt32)callable->declarationRange.end.column == capture->declarationEndColumn);
}

static const SZrFunctionTypeInfo *compiler_find_typed_closure_callable(
        const SZrTypeEnvironment *typeEnv,
        const SZrFunctionClosureVariable *capture) {
    if (typeEnv == ZR_NULL || capture == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0u; index < typeEnv->functionReturnTypes.length; index++) {
        const SZrFunctionTypeInfo *const *candidate =
                (const SZrFunctionTypeInfo *const *)ZrCore_Array_Get(
                        (SZrArray *)&typeEnv->functionReturnTypes, index);

        if (candidate != ZR_NULL &&
            compiler_typed_closure_callable_matches_source(capture, *candidate)) {
            return *candidate;
        }
    }
    return ZR_NULL;
}

TZrBool compiler_build_typed_closure_bindings(SZrCompilerState *cs,
                                              SZrFunctionTypedClosureBinding **outBindings,
                                              TZrUInt32 *outCount) {
    SZrFunctionTypedClosureBinding *bindings;
    TZrUInt32 bindingCount = 0u;
    TZrUInt32 captureCount;

    if (outBindings == ZR_NULL || outCount == ZR_NULL) {
        return ZR_FALSE;
    }

    *outBindings = ZR_NULL;
    *outCount = 0u;
    if (cs == ZR_NULL || cs->closureVars.length == 0u || cs->typeEnv == ZR_NULL) {
        return ZR_TRUE;
    }

    captureCount = (TZrUInt32)cs->closureVars.length;
    bindings = (SZrFunctionTypedClosureBinding *)ZrCore_Memory_RawMallocWithType(
            cs->state->global,
            sizeof(SZrFunctionTypedClosureBinding) * captureCount,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (bindings == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 captureIndex = 0u; captureIndex < captureCount; captureIndex++) {
        const SZrFunctionClosureVariable *capture =
                (const SZrFunctionClosureVariable *)ZrCore_Array_Get(&cs->closureVars, captureIndex);
        const SZrTypeBinding *binding;
        const SZrFunctionTypeInfo *callable;
        SZrFunctionTypedClosureBinding *destination;

        if (capture == ZR_NULL || capture->name == ZR_NULL) {
            continue;
        }

        binding = ZrParser_TypeEnvironment_FindVariableBinding(cs->typeEnv, capture->name);
        callable = binding == ZR_NULL
                           ? compiler_find_typed_closure_callable(cs->typeEnv, capture)
                           : ZR_NULL;
        if (!compiler_typed_closure_binding_matches_source(capture, binding) && callable == ZR_NULL) {
            continue;
        }

        destination = &bindings[bindingCount++];
        ZrCore_Memory_RawSet(destination, 0, sizeof(*destination));
        destination->captureIndex = captureIndex;
        if (binding != ZR_NULL) {
            compiler_typed_type_ref_from_inferred(&destination->type, &binding->type);
        } else {
            SZrInferredType functionType;

            ZrParser_InferredType_Init(cs->state, &functionType, ZR_VALUE_TYPE_FUNCTION);
            compiler_typed_type_ref_from_inferred(&destination->type, &functionType);
            ZrParser_InferredType_Free(cs->state, &functionType);
        }
        destination->symbolId = capture->symbolId;
        destination->typeId = capture->typeId;
        destination->declarationStartLine = capture->declarationStartLine;
        destination->declarationStartColumn = capture->declarationStartColumn;
        destination->declarationEndLine = capture->declarationEndLine;
        destination->declarationEndColumn = capture->declarationEndColumn;
    }

    if (bindingCount == 0u) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      bindings,
                                      sizeof(SZrFunctionTypedClosureBinding) * captureCount,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        return ZR_TRUE;
    }

    *outBindings = bindings;
    *outCount = bindingCount;
    return ZR_TRUE;
}
