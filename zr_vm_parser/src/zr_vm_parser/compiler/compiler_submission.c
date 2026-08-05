#include "compiler_internal.h"

typedef enum ECompilerSubmissionValuePolicy {
    COMPILER_SUBMISSION_VALUE_POLICY_VALUE = 0,
    COMPILER_SUBMISSION_VALUE_POLICY_UNIQUE_MOVE,
    COMPILER_SUBMISSION_VALUE_POLICY_SHARED_COPY,
    COMPILER_SUBMISSION_VALUE_POLICY_WEAK_COPY,
    COMPILER_SUBMISSION_VALUE_POLICY_REJECT
} ECompilerSubmissionValuePolicy;

static TZrBool compiler_submission_has_valid_range(const SZrFileRange *range) {
    return range != ZR_NULL && range->source != ZR_NULL &&
           range->start.line >= 0 && range->start.column >= 0 &&
           range->end.line >= 0 && range->end.column >= 0;
}

static ECompilerSubmissionValuePolicy compiler_submission_classify_value(
        const SZrInferredType *type) {
    if (type == ZR_NULL || type->referenceAccess != ZR_REFERENCE_ACCESS_NONE ||
        (type->protocolMask & ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_REF_LIKE)) != 0u) {
        return COMPILER_SUBMISSION_VALUE_POLICY_REJECT;
    }

    switch (type->ownershipQualifier) {
        case ZR_OWNERSHIP_QUALIFIER_NONE:
            return COMPILER_SUBMISSION_VALUE_POLICY_VALUE;
        case ZR_OWNERSHIP_QUALIFIER_UNIQUE:
            return COMPILER_SUBMISSION_VALUE_POLICY_UNIQUE_MOVE;
        case ZR_OWNERSHIP_QUALIFIER_SHARED:
            return COMPILER_SUBMISSION_VALUE_POLICY_SHARED_COPY;
        case ZR_OWNERSHIP_QUALIFIER_WEAK:
            return COMPILER_SUBMISSION_VALUE_POLICY_WEAK_COPY;
        case ZR_OWNERSHIP_QUALIFIER_BORROWED:
        case ZR_OWNERSHIP_QUALIFIER_LOANED:
        default:
            return COMPILER_SUBMISSION_VALUE_POLICY_REJECT;
    }
}

static TZrBool compiler_submission_type_can_persist(const SZrInferredType *type) {
    return compiler_submission_classify_value(type) != COMPILER_SUBMISSION_VALUE_POLICY_REJECT;
}

static TZrBool compiler_submission_ranges_equal(
        const SZrFileRange *left,
        const SZrFileRange *right) {
    return left != ZR_NULL && right != ZR_NULL &&
           left->source == right->source &&
           left->start.line == right->start.line &&
           left->start.column == right->start.column &&
           left->start.offset == right->start.offset &&
           left->end.line == right->end.line &&
           left->end.column == right->end.column &&
           left->end.offset == right->end.offset;
}

static TZrBool compiler_submission_bindings_conflict(
        const SZrParserSubmissionBinding *left,
        const SZrParserSubmissionBinding *right) {
    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_FALSE;
    }

    return (left->name != ZR_NULL && right->name != ZR_NULL &&
            ZrCore_String_Equal(left->name, right->name)) ||
           (left->symbolId == right->symbolId && left->typeId == right->typeId &&
            compiler_submission_ranges_equal(
                    &left->declarationRange, &right->declarationRange));
}

static const SZrParserSubmissionCallableSignature *compiler_submission_signature_for_binding(
        const SZrParserSubmissionContext *context,
        const SZrParserSubmissionBinding *binding) {
    if (context == ZR_NULL || binding == ZR_NULL ||
        binding->kind != ZR_PARSER_SUBMISSION_BINDING_KIND_CALLABLE ||
        binding->callableSignatureIndex >= context->callableSignatureCount ||
        context->callableSignatures == ZR_NULL) {
        return ZR_NULL;
    }

    return &context->callableSignatures[binding->callableSignatureIndex];
}

static TZrBool compiler_submission_signature_is_valid(
        const SZrParserSubmissionBinding *binding,
        const SZrParserSubmissionCallableSignature *signature) {
    TZrSize parameterCount;

    if (binding == ZR_NULL || signature == ZR_NULL ||
        signature->symbolId != binding->symbolId || signature->typeId != binding->typeId ||
        !compiler_submission_has_valid_range(&signature->declarationRange) ||
        !compiler_submission_ranges_equal(
                &binding->declarationRange, &signature->declarationRange)) {
        return ZR_FALSE;
    }

    parameterCount = signature->parameterTypes.length;
    return (parameterCount == 0u ||
            (signature->parameterTypes.isValid && signature->parameterTypes.head != ZR_NULL &&
             signature->parameterTypes.elementSize == sizeof(SZrInferredType))) &&
           (signature->parameterPassingModes.length == 0u ||
            (signature->parameterPassingModes.isValid &&
             signature->parameterPassingModes.head != ZR_NULL &&
             signature->parameterPassingModes.elementSize == sizeof(EZrParameterPassingMode))) &&
           signature->parameterPassingModes.length == parameterCount;
}

static TZrBool compiler_submission_validate_context(
        const SZrParserSubmissionContext *context) {
    if (context == ZR_NULL ||
        context->moduleGeneration == 0u ||
        context->environmentGeneration == 0u ||
        context->cellGeneration == 0u ||
        (context->bindingCount > 0u && context->bindings == ZR_NULL)) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0u; index < context->bindingCount; index++) {
        const SZrParserSubmissionBinding *binding = &context->bindings[index];

        if (binding->name == ZR_NULL ||
            binding->symbolId == ZR_SEMANTIC_ID_INVALID ||
            binding->typeId == ZR_SEMANTIC_ID_INVALID ||
            binding->captureIndex != index ||
            !compiler_submission_type_can_persist(&binding->inferredType) ||
            binding->moduleGeneration != context->moduleGeneration ||
            binding->environmentGeneration != context->environmentGeneration ||
            binding->cellGeneration == 0u ||
            binding->cellGeneration >= context->cellGeneration ||
            !compiler_submission_has_valid_range(&binding->declarationRange)) {
            return ZR_FALSE;
        }

        if (binding->kind != ZR_PARSER_SUBMISSION_BINDING_KIND_VALUE &&
            binding->kind != ZR_PARSER_SUBMISSION_BINDING_KIND_CALLABLE) {
            return ZR_FALSE;
        }

        for (TZrSize priorIndex = 0u; priorIndex < index; priorIndex++) {
            if (compiler_submission_bindings_conflict(
                    binding, &context->bindings[priorIndex])) {
                return ZR_FALSE;
            }
        }

        if (binding->kind == ZR_PARSER_SUBMISSION_BINDING_KIND_CALLABLE &&
            !compiler_submission_signature_is_valid(
                    binding,
                    compiler_submission_signature_for_binding(context, binding))) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

TZrBool compiler_submission_seed_context(
        SZrCompilerState *cs,
        const SZrParserSubmissionContext *context) {
    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->typeEnv == ZR_NULL) {
        return ZR_FALSE;
    }
    if (context == ZR_NULL) {
        return ZR_TRUE;
    }
    if (!compiler_submission_validate_context(context)) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0u; index < context->bindingCount; index++) {
        const SZrParserSubmissionBinding *binding = &context->bindings[index];
        const SZrParserSubmissionCallableSignature *signature =
                compiler_submission_signature_for_binding(context, binding);
        SZrFunctionClosureVariable closureVariable;

        if ((binding->kind == ZR_PARSER_SUBMISSION_BINDING_KIND_CALLABLE &&
             (signature == ZR_NULL ||
              !ZrParser_TypeEnvironment_RegisterCanonicalFunction(
                      cs->state,
                      cs->typeEnv,
                      binding->name,
                      &signature->returnType,
                      &signature->parameterTypes,
                      &signature->parameterPassingModes,
                      binding->symbolId,
                      binding->typeId,
                      signature->declarationRange))) ||
            (binding->kind == ZR_PARSER_SUBMISSION_BINDING_KIND_VALUE &&
             !ZrParser_TypeEnvironment_RegisterClosureCapture(
                      cs->state,
                      cs->typeEnv,
                      binding->name,
                      &binding->inferredType,
                      binding->symbolId,
                      binding->typeId,
                      binding->declarationRange,
                      binding->captureIndex,
                      context->environmentGeneration))) {
            return ZR_FALSE;
        }

        ZrCore_Memory_RawSet(&closureVariable, 0, sizeof(closureVariable));
        closureVariable.name = binding->name;
        closureVariable.inStack = ZR_FALSE;
        closureVariable.index = binding->captureIndex;
        closureVariable.valueType = binding->inferredType.baseType;
        closureVariable.scopeDepth = 0u;
        closureVariable.escapeFlags = ZR_GARBAGE_COLLECT_ESCAPE_KIND_CLOSURE_CAPTURE;
        closureVariable.symbolId = binding->symbolId;
        closureVariable.typeId = binding->typeId;
        closureVariable.declarationStartLine = (TZrUInt32)binding->declarationRange.start.line;
        closureVariable.declarationStartColumn = (TZrUInt32)binding->declarationRange.start.column;
        closureVariable.declarationEndLine = (TZrUInt32)binding->declarationRange.end.line;
        closureVariable.declarationEndColumn = (TZrUInt32)binding->declarationRange.end.column;
        ZrCore_Array_Push(cs->state, &cs->closureVars, &closureVariable);
        cs->closureVarCount++;
    }

    return ZR_TRUE;
}

TZrBool compiler_submission_append_declared_capture(
        SZrCompilerState *cs,
        SZrString *name,
        TZrUInt32 *outCaptureIndex) {
    const SZrTypeBinding *binding;
    SZrFunctionClosureVariable closureVariable;
    TZrUInt32 captureIndex;

    if (outCaptureIndex != ZR_NULL) {
        *outCaptureIndex = ZR_PARSER_INDEX_NONE;
    }
    if (cs == ZR_NULL || cs->submissionContext == ZR_NULL) {
        return ZR_TRUE;
    }
    if (cs->state == ZR_NULL || cs->typeEnv == ZR_NULL || name == ZR_NULL ||
        cs->closureVars.length > UINT32_MAX ||
        cs->submissionDeclaredCaptureIndices.elementSize != sizeof(TZrUInt32)) {
        return ZR_FALSE;
    }

    binding = ZrParser_TypeEnvironment_FindVariableBinding(cs->typeEnv, name);
    if (binding == ZR_NULL || binding->symbolId == ZR_SEMANTIC_ID_INVALID ||
        binding->typeId == ZR_SEMANTIC_ID_INVALID || !binding->hasDeclarationRange) {
        return ZR_FALSE;
    }

    captureIndex = (TZrUInt32)cs->closureVars.length;
    ZrCore_Memory_RawSet(&closureVariable, 0, sizeof(closureVariable));
    closureVariable.name = binding->name;
    closureVariable.inStack = ZR_FALSE;
    closureVariable.index = captureIndex;
    closureVariable.valueType = binding->type.baseType;
    closureVariable.scopeDepth = 0u;
    closureVariable.escapeFlags = ZR_GARBAGE_COLLECT_ESCAPE_KIND_CLOSURE_CAPTURE;
    closureVariable.symbolId = binding->symbolId;
    closureVariable.typeId = binding->typeId;
    closureVariable.declarationStartLine = (TZrUInt32)binding->declarationRange.start.line;
    closureVariable.declarationStartColumn = (TZrUInt32)binding->declarationRange.start.column;
    closureVariable.declarationEndLine = (TZrUInt32)binding->declarationRange.end.line;
    closureVariable.declarationEndColumn = (TZrUInt32)binding->declarationRange.end.column;
    ZrCore_Array_Push(cs->state, &cs->closureVars, &closureVariable);
    ZrCore_Array_Push(cs->state, &cs->submissionDeclaredCaptureIndices, &captureIndex);
    cs->closureVarCount++;

    if (outCaptureIndex != ZR_NULL) {
        *outCaptureIndex = captureIndex;
    }
    return ZR_TRUE;
}

static const SZrFunctionTypeInfo *compiler_submission_find_callable_by_declaration(
        const SZrTypeEnvironment *typeEnv,
        const SZrAstNode *declarationNode) {
    if (typeEnv == ZR_NULL || declarationNode == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0u; index < typeEnv->functionReturnTypes.length; index++) {
        const SZrFunctionTypeInfo *const *candidate =
                (const SZrFunctionTypeInfo *const *)ZrCore_Array_Get(
                        (SZrArray *)&typeEnv->functionReturnTypes, index);

        if (candidate != ZR_NULL && *candidate != ZR_NULL &&
            (*candidate)->declarationNode == declarationNode) {
            return *candidate;
        }
    }
    return ZR_NULL;
}

static const SZrFunctionTypeInfo *compiler_submission_find_callable_by_capture(
        const SZrTypeEnvironment *typeEnv,
        const SZrFunctionClosureVariable *capture) {
    if (typeEnv == ZR_NULL || capture == ZR_NULL || capture->name == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0u; index < typeEnv->functionReturnTypes.length; index++) {
        const SZrFunctionTypeInfo *const *candidate =
                (const SZrFunctionTypeInfo *const *)ZrCore_Array_Get(
                        (SZrArray *)&typeEnv->functionReturnTypes, index);

        if (candidate == ZR_NULL || *candidate == ZR_NULL || (*candidate)->name == ZR_NULL ||
            !ZrCore_String_Equal((*candidate)->name, capture->name) ||
            (*candidate)->symbolId != capture->symbolId || (*candidate)->typeId != capture->typeId ||
            !(*candidate)->hasDeclarationRange ||
            (TZrUInt32)(*candidate)->declarationRange.start.line !=
                    capture->declarationStartLine ||
            (TZrUInt32)(*candidate)->declarationRange.start.column !=
                    capture->declarationStartColumn ||
            (TZrUInt32)(*candidate)->declarationRange.end.line !=
                    capture->declarationEndLine ||
            (TZrUInt32)(*candidate)->declarationRange.end.column !=
                    capture->declarationEndColumn) {
            continue;
        }
        return *candidate;
    }
    return ZR_NULL;
}

TZrBool compiler_submission_append_declared_callable(
        SZrCompilerState *cs,
        SZrAstNode *declarationNode,
        TZrUInt32 *outCaptureIndex) {
    const SZrFunctionTypeInfo *functionInfo;
    SZrFunctionClosureVariable closureVariable;
    TZrUInt32 captureIndex;

    if (outCaptureIndex != ZR_NULL) {
        *outCaptureIndex = ZR_PARSER_INDEX_NONE;
    }
    if (cs == ZR_NULL || cs->submissionContext == ZR_NULL) {
        return ZR_TRUE;
    }
    if (cs->state == ZR_NULL || cs->typeEnv == ZR_NULL || declarationNode == ZR_NULL ||
        cs->closureVars.length > UINT32_MAX ||
        cs->submissionDeclaredCaptureIndices.elementSize != sizeof(TZrUInt32)) {
        return ZR_FALSE;
    }

    functionInfo = compiler_submission_find_callable_by_declaration(cs->typeEnv, declarationNode);
    if (functionInfo == ZR_NULL || functionInfo->name == ZR_NULL ||
        functionInfo->symbolId == ZR_SEMANTIC_ID_INVALID ||
        functionInfo->typeId == ZR_SEMANTIC_ID_INVALID ||
        !functionInfo->hasDeclarationRange) {
        return ZR_FALSE;
    }

    captureIndex = (TZrUInt32)cs->closureVars.length;
    ZrCore_Memory_RawSet(&closureVariable, 0, sizeof(closureVariable));
    closureVariable.name = functionInfo->name;
    closureVariable.inStack = ZR_FALSE;
    closureVariable.index = captureIndex;
    closureVariable.valueType = ZR_VALUE_TYPE_FUNCTION;
    closureVariable.scopeDepth = 0u;
    closureVariable.escapeFlags = ZR_GARBAGE_COLLECT_ESCAPE_KIND_CLOSURE_CAPTURE;
    closureVariable.symbolId = functionInfo->symbolId;
    closureVariable.typeId = functionInfo->typeId;
    closureVariable.declarationStartLine = (TZrUInt32)functionInfo->declarationRange.start.line;
    closureVariable.declarationStartColumn = (TZrUInt32)functionInfo->declarationRange.start.column;
    closureVariable.declarationEndLine = (TZrUInt32)functionInfo->declarationRange.end.line;
    closureVariable.declarationEndColumn = (TZrUInt32)functionInfo->declarationRange.end.column;
    ZrCore_Array_Push(cs->state, &cs->closureVars, &closureVariable);
    ZrCore_Array_Push(cs->state, &cs->submissionDeclaredCaptureIndices, &captureIndex);
    cs->closureVarCount++;

    if (outCaptureIndex != ZR_NULL) {
        *outCaptureIndex = captureIndex;
    }
    return ZR_TRUE;
}

static TZrBool compiler_submission_copy_callable_signature(
        SZrCompilerState *cs,
        const SZrFunctionTypeInfo *source,
        SZrParserSubmissionCallableSignature *destination) {
    if (cs == ZR_NULL || cs->state == ZR_NULL || source == ZR_NULL ||
        destination == ZR_NULL || source->symbolId == ZR_SEMANTIC_ID_INVALID ||
        source->typeId == ZR_SEMANTIC_ID_INVALID || !source->hasDeclarationRange ||
        source->parameterPassingModes.length != source->paramTypes.length) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(destination, 0, sizeof(*destination));
    destination->symbolId = source->symbolId;
    destination->typeId = source->typeId;
    destination->declarationRange = source->declarationRange;
    ZrParser_InferredType_Copy(cs->state, &destination->returnType, &source->returnType);
    ZrCore_Array_Construct(&destination->parameterTypes);
    ZrCore_Array_Construct(&destination->parameterPassingModes);

    if (source->paramTypes.length > 0u) {
        ZrCore_Array_Init(
                cs->state,
                &destination->parameterTypes,
                sizeof(SZrInferredType),
                source->paramTypes.length);
        ZrCore_Array_Init(
                cs->state,
                &destination->parameterPassingModes,
                sizeof(EZrParameterPassingMode),
                source->parameterPassingModes.length);
    }
    for (TZrSize index = 0u; index < source->paramTypes.length; index++) {
        const SZrInferredType *parameterType = (const SZrInferredType *)ZrCore_Array_Get(
                (SZrArray *)&source->paramTypes, index);
        const EZrParameterPassingMode *passingMode = (const EZrParameterPassingMode *)ZrCore_Array_Get(
                (SZrArray *)&source->parameterPassingModes, index);
        SZrInferredType copiedType;
        EZrParameterPassingMode copiedPassingMode;

        if (parameterType == ZR_NULL || passingMode == ZR_NULL) {
            return ZR_FALSE;
        }
        ZrParser_InferredType_Copy(cs->state, &copiedType, parameterType);
        copiedPassingMode = *passingMode;
        ZrCore_Array_Push(cs->state, &destination->parameterTypes, &copiedType);
        ZrCore_Array_Push(cs->state, &destination->parameterPassingModes, &copiedPassingMode);
    }
    return ZR_TRUE;
}

TZrBool compiler_submission_publish_result(
        SZrCompilerState *cs,
        SZrParserSubmissionResult *outResult) {
    SZrParserSubmissionResult result;
    TZrSize bindingCount;
    TZrSize callableSignatureCount = 0u;
    TZrSize nextCallableSignature = 0u;

    if (outResult == ZR_NULL) {
        return ZR_TRUE;
    }
    ZrCore_Memory_RawSet(outResult, 0, sizeof(*outResult));
    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->state->global == ZR_NULL ||
        cs->typeEnv == ZR_NULL || cs->submissionContext == ZR_NULL) {
        return ZR_FALSE;
    }

    bindingCount = cs->submissionDeclaredCaptureIndices.length;
    if (bindingCount == 0u) {
        return ZR_TRUE;
    }
    if (bindingCount > UINT32_MAX) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(&result, 0, sizeof(result));
    result.bindings = (SZrParserSubmissionBinding *)ZrCore_Memory_RawMallocWithType(
            cs->state->global,
            sizeof(*result.bindings) * bindingCount,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (result.bindings == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Memory_RawSet(result.bindings, 0, sizeof(*result.bindings) * bindingCount);
    result.bindingCount = bindingCount;

    for (TZrSize index = 0u; index < bindingCount; index++) {
        const TZrUInt32 *captureIndex = (const TZrUInt32 *)ZrCore_Array_Get(
                &cs->submissionDeclaredCaptureIndices, index);
        const SZrFunctionClosureVariable *capture;

        if (captureIndex == ZR_NULL || *captureIndex >= cs->closureVars.length) {
            ZrParser_SubmissionResult_Free(cs->state, &result);
            return ZR_FALSE;
        }
        capture = (const SZrFunctionClosureVariable *)ZrCore_Array_Get(
                &cs->closureVars, *captureIndex);
        if (capture == ZR_NULL || capture->name == ZR_NULL) {
            ZrParser_SubmissionResult_Free(cs->state, &result);
            return ZR_FALSE;
        }
        if (ZrParser_TypeEnvironment_FindVariableBinding(cs->typeEnv, capture->name) == ZR_NULL) {
            if (compiler_submission_find_callable_by_capture(cs->typeEnv, capture) == ZR_NULL) {
                ZrParser_SubmissionResult_Free(cs->state, &result);
                return ZR_FALSE;
            }
            callableSignatureCount++;
        }
    }

    if (callableSignatureCount > 0u) {
        result.callableSignatures =
                (SZrParserSubmissionCallableSignature *)ZrCore_Memory_RawMallocWithType(
                        cs->state->global,
                        sizeof(*result.callableSignatures) * callableSignatureCount,
                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (result.callableSignatures == ZR_NULL) {
            ZrParser_SubmissionResult_Free(cs->state, &result);
            return ZR_FALSE;
        }
        ZrCore_Memory_RawSet(
                result.callableSignatures,
                0,
                sizeof(*result.callableSignatures) * callableSignatureCount);
        result.callableSignatureCount = callableSignatureCount;
    }

    for (TZrSize index = 0u; index < bindingCount; index++) {
        const TZrUInt32 *captureIndex = (const TZrUInt32 *)ZrCore_Array_Get(
                &cs->submissionDeclaredCaptureIndices, index);
        const SZrFunctionClosureVariable *capture =
                (const SZrFunctionClosureVariable *)ZrCore_Array_Get(
                        &cs->closureVars, *captureIndex);
        const SZrTypeBinding *valueBinding =
                ZrParser_TypeEnvironment_FindVariableBinding(cs->typeEnv, capture->name);
        SZrParserSubmissionBinding *resultBinding = &result.bindings[index];

        resultBinding->name = capture->name;
        resultBinding->symbolId = capture->symbolId;
        resultBinding->typeId = capture->typeId;
        resultBinding->placeId = ZR_SEMANTIC_ID_INVALID;
        resultBinding->captureIndex = *captureIndex;
        resultBinding->moduleGeneration = cs->submissionContext->moduleGeneration;
        resultBinding->environmentGeneration = cs->submissionContext->environmentGeneration;
        resultBinding->cellGeneration = cs->submissionContext->cellGeneration;

        if (valueBinding != ZR_NULL && valueBinding->symbolId == capture->symbolId &&
            valueBinding->typeId == capture->typeId && valueBinding->hasDeclarationRange) {
            if (!compiler_submission_type_can_persist(&valueBinding->type)) {
                ZrParser_SubmissionResult_Free(cs->state, &result);
                return ZR_FALSE;
            }
            resultBinding->kind = ZR_PARSER_SUBMISSION_BINDING_KIND_VALUE;
            ZrParser_InferredType_Copy(cs->state, &resultBinding->inferredType, &valueBinding->type);
            resultBinding->placeId = valueBinding->placeId;
            resultBinding->declarationRange = valueBinding->declarationRange;
            resultBinding->callableSignatureIndex = ZR_PARSER_SUBMISSION_CALLABLE_SIGNATURE_NONE;
        } else {
            const SZrFunctionTypeInfo *functionInfo =
                    compiler_submission_find_callable_by_capture(cs->typeEnv, capture);

            if (functionInfo == ZR_NULL || nextCallableSignature >= result.callableSignatureCount ||
                !compiler_submission_copy_callable_signature(
                        cs,
                        functionInfo,
                        &result.callableSignatures[nextCallableSignature])) {
                ZrParser_SubmissionResult_Free(cs->state, &result);
                return ZR_FALSE;
            }
            resultBinding->kind = ZR_PARSER_SUBMISSION_BINDING_KIND_CALLABLE;
            ZrParser_InferredType_Init(
                    cs->state, &resultBinding->inferredType, ZR_VALUE_TYPE_FUNCTION);
            resultBinding->declarationRange = functionInfo->declarationRange;
            resultBinding->callableSignatureIndex = (TZrUInt32)nextCallableSignature++;
        }
    }

    *outResult = result;
    return ZR_TRUE;
}

void ZrParser_SubmissionResult_Free(
        SZrState *state,
        SZrParserSubmissionResult *result) {
    if (state == ZR_NULL || state->global == ZR_NULL || result == ZR_NULL) {
        return;
    }

    if (result->bindings != ZR_NULL) {
        for (TZrSize index = 0u; index < result->bindingCount; index++) {
            ZrParser_InferredType_Free(state, &result->bindings[index].inferredType);
        }
        ZrCore_Memory_RawFreeWithType(
                state->global,
                result->bindings,
                sizeof(*result->bindings) * result->bindingCount,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }

    if (result->callableSignatures != ZR_NULL) {
        for (TZrSize signatureIndex = 0u;
             signatureIndex < result->callableSignatureCount;
             signatureIndex++) {
            SZrParserSubmissionCallableSignature *signature =
                    &result->callableSignatures[signatureIndex];

            ZrParser_InferredType_Free(state, &signature->returnType);
            for (TZrSize parameterIndex = 0u;
                 parameterIndex < signature->parameterTypes.length;
                 parameterIndex++) {
                SZrInferredType *parameterType = (SZrInferredType *)ZrCore_Array_Get(
                        &signature->parameterTypes, parameterIndex);
                if (parameterType != ZR_NULL) {
                    ZrParser_InferredType_Free(state, parameterType);
                }
            }
            if (signature->parameterTypes.isValid && signature->parameterTypes.head != ZR_NULL) {
                ZrCore_Array_Free(state, &signature->parameterTypes);
            }
            if (signature->parameterPassingModes.isValid &&
                signature->parameterPassingModes.head != ZR_NULL) {
                ZrCore_Array_Free(state, &signature->parameterPassingModes);
            }
        }
        ZrCore_Memory_RawFreeWithType(
                state->global,
                result->callableSignatures,
                sizeof(*result->callableSignatures) * result->callableSignatureCount,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }

    result->bindings = ZR_NULL;
    result->bindingCount = 0u;
    result->callableSignatures = ZR_NULL;
    result->callableSignatureCount = 0u;
}
