#include "type_inference_internal.h"

#include <limits.h>
#include <stdio.h>

static TZrUInt32 member_resolution_min_argument_count(const SZrTypeMemberInfo *memberInfo) {
    if (memberInfo == ZR_NULL) {
        return 0;
    }

    if (memberInfo->minArgumentCount != ZR_MEMBER_PARAMETER_COUNT_UNKNOWN) {
        return memberInfo->minArgumentCount;
    }

    if (memberInfo->parameterCount == ZR_MEMBER_PARAMETER_COUNT_UNKNOWN) {
        return 0;
    }

    if (memberInfo->parameterHasDefaultValues.length > 0) {
        TZrUInt32 minArgumentCount = memberInfo->parameterCount;
        while (minArgumentCount > 0) {
            TZrBool *hasDefaultPtr =
                    (TZrBool *)ZrCore_Array_Get((SZrArray *)&memberInfo->parameterHasDefaultValues,
                                                (TZrSize)minArgumentCount - 1U);
            if (hasDefaultPtr == ZR_NULL || !*hasDefaultPtr) {
                break;
            }
            minArgumentCount--;
        }
        return minArgumentCount;
    }

    return memberInfo->parameterCount;
}

static TZrBool member_resolution_infer_call_arguments(SZrCompilerState *cs,
                                                      SZrFunctionCall *call,
                                                      SZrArray *argTypes) {
    TZrSize argCount;

    if (cs == ZR_NULL || argTypes == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Construct(argTypes);
    argCount = (call != ZR_NULL && call->args != ZR_NULL) ? call->args->count : 0;
    if (argCount == 0) {
        return ZR_TRUE;
    }

    ZrCore_Array_Init(cs->state, argTypes, sizeof(SZrInferredType), argCount);
    for (TZrSize index = 0; index < argCount; index++) {
        SZrAstNode *argNode = call->args->nodes[index];
        SZrInferredType argType;

        if (argNode == ZR_NULL) {
            free_inferred_type_array(cs->state, argTypes);
            ZrCore_Array_Construct(argTypes);
            return ZR_FALSE;
        }

        ZrParser_InferredType_Init(cs->state, &argType, ZR_VALUE_TYPE_OBJECT);
        if (!ZrParser_ExpressionType_Infer(cs, argNode, &argType)) {
            ZrParser_InferredType_Free(cs->state, &argType);
            free_inferred_type_array(cs->state, argTypes);
            ZrCore_Array_Construct(argTypes);
            return ZR_FALSE;
        }
        ZrCore_Array_Push(cs->state, argTypes, &argType);
    }

    return ZR_TRUE;
}

static TZrBool member_resolution_name_at(const SZrTypeMemberInfo *memberInfo,
                                         TZrSize index,
                                         SZrString **outName) {
    SZrString **namePtr;

    if (outName != ZR_NULL) {
        *outName = ZR_NULL;
    }
    if (memberInfo == ZR_NULL || outName == ZR_NULL || index >= memberInfo->parameterNames.length) {
        return ZR_FALSE;
    }

    namePtr = (SZrString **)ZrCore_Array_Get((SZrArray *)&memberInfo->parameterNames, index);
    if (namePtr == ZR_NULL || *namePtr == ZR_NULL) {
        return ZR_FALSE;
    }

    *outName = *namePtr;
    return ZR_TRUE;
}

static TZrBool member_resolution_build_argument_slots(SZrCompilerState *cs,
                                                      const SZrTypeMemberInfo *memberInfo,
                                                      SZrFunctionCall *call,
                                                      const SZrArray *callArgTypes,
                                                      SZrArray *orderedArgTypes,
                                                      TZrBool *outMismatch) {
    TZrSize argCount;
    TZrSize slotCount;
    TZrUInt32 minArgumentCount;
    TZrBool *provided = ZR_NULL;
    TZrBool mismatch = ZR_FALSE;

    if (outMismatch != ZR_NULL) {
        *outMismatch = ZR_FALSE;
    }
    if (cs == ZR_NULL || memberInfo == ZR_NULL || callArgTypes == ZR_NULL || orderedArgTypes == ZR_NULL) {
        return ZR_FALSE;
    }

    argCount = (call != ZR_NULL && call->args != ZR_NULL) ? call->args->count : 0;
    slotCount = memberInfo->parameterCount != ZR_MEMBER_PARAMETER_COUNT_UNKNOWN
                        ? memberInfo->parameterCount
                        : memberInfo->parameterTypes.length;
    if (memberInfo->parameterTypes.length > slotCount) {
        slotCount = memberInfo->parameterTypes.length;
    }
    if (memberInfo->parameterNames.length > slotCount) {
        slotCount = memberInfo->parameterNames.length;
    }
    minArgumentCount = member_resolution_min_argument_count(memberInfo);

    ZrCore_Array_Construct(orderedArgTypes);
    if (slotCount == 0) {
        if (argCount != 0) {
            if (outMismatch != ZR_NULL) {
                *outMismatch = ZR_TRUE;
            }
            return ZR_TRUE;
        }
        return ZR_TRUE;
    }

    ZrCore_Array_Init(cs->state, orderedArgTypes, sizeof(SZrInferredType), slotCount);
    provided = (TZrBool *)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                          sizeof(TZrBool) * slotCount,
                                                          ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (provided == ZR_NULL) {
        free_inferred_type_array(cs->state, orderedArgTypes);
        ZrCore_Array_Construct(orderedArgTypes);
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < slotCount; index++) {
        SZrInferredType emptyType;
        provided[index] = ZR_FALSE;
        ZrParser_InferredType_Init(cs->state, &emptyType, ZR_VALUE_TYPE_OBJECT);
        ZrCore_Array_Push(cs->state, orderedArgTypes, &emptyType);
    }

    if (call != ZR_NULL && call->hasNamedArgs && call->argNames != ZR_NULL) {
        TZrSize positionalCount = 0;

        for (TZrSize index = 0; index < argCount && index < call->argNames->length; index++) {
            SZrString **namePtr = (SZrString **)ZrCore_Array_Get(call->argNames, index);
            if (namePtr != ZR_NULL && *namePtr == ZR_NULL) {
                positionalCount++;
                continue;
            }
            break;
        }

        if (positionalCount > slotCount) {
            mismatch = ZR_TRUE;
            goto cleanup;
        }

        for (TZrSize index = 0; index < positionalCount; index++) {
            SZrInferredType *sourceType = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)callArgTypes, index);
            SZrInferredType *slotType = (SZrInferredType *)ZrCore_Array_Get(orderedArgTypes, index);
            if (sourceType == ZR_NULL || slotType == ZR_NULL) {
                goto error;
            }
            ZrParser_InferredType_Free(cs->state, slotType);
            ZrParser_InferredType_Copy(cs->state, slotType, sourceType);
            provided[index] = ZR_TRUE;
        }

        for (TZrSize index = positionalCount; index < argCount && index < call->argNames->length; index++) {
            SZrString **namePtr = (SZrString **)ZrCore_Array_Get(call->argNames, index);
            TZrBool matched = ZR_FALSE;

            if (namePtr == ZR_NULL || *namePtr == ZR_NULL) {
                mismatch = ZR_TRUE;
                goto cleanup;
            }

            for (TZrSize paramIndex = 0; paramIndex < slotCount; paramIndex++) {
                SZrString *paramName = ZR_NULL;
                SZrInferredType *sourceType;
                SZrInferredType *slotType;

                if (!member_resolution_name_at(memberInfo, paramIndex, &paramName) ||
                    !ZrCore_String_Equal(paramName, *namePtr)) {
                    continue;
                }
                if (provided[paramIndex]) {
                    mismatch = ZR_TRUE;
                    goto cleanup;
                }

                sourceType = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)callArgTypes, index);
                slotType = (SZrInferredType *)ZrCore_Array_Get(orderedArgTypes, paramIndex);
                if (sourceType == ZR_NULL || slotType == ZR_NULL) {
                    goto error;
                }
                ZrParser_InferredType_Free(cs->state, slotType);
                ZrParser_InferredType_Copy(cs->state, slotType, sourceType);
                provided[paramIndex] = ZR_TRUE;
                matched = ZR_TRUE;
                break;
            }

            if (!matched) {
                mismatch = ZR_TRUE;
                goto cleanup;
            }
        }
    } else {
        if (argCount > slotCount) {
            mismatch = ZR_TRUE;
            goto cleanup;
        }
        for (TZrSize index = 0; index < argCount; index++) {
            SZrInferredType *sourceType = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)callArgTypes, index);
            SZrInferredType *slotType = (SZrInferredType *)ZrCore_Array_Get(orderedArgTypes, index);
            if (sourceType == ZR_NULL || slotType == ZR_NULL) {
                goto error;
            }
            ZrParser_InferredType_Free(cs->state, slotType);
            ZrParser_InferredType_Copy(cs->state, slotType, sourceType);
            provided[index] = ZR_TRUE;
        }
    }

    for (TZrSize index = 0; index < slotCount; index++) {
        if (!provided[index] && index < minArgumentCount) {
            mismatch = ZR_TRUE;
            goto cleanup;
        }
        if (!provided[index] && index < memberInfo->parameterTypes.length) {
            SZrInferredType *paramType =
                    (SZrInferredType *)ZrCore_Array_Get((SZrArray *)&memberInfo->parameterTypes, index);
            SZrInferredType *slotType = (SZrInferredType *)ZrCore_Array_Get(orderedArgTypes, index);
            if (paramType != ZR_NULL && slotType != ZR_NULL) {
                ZrParser_InferredType_Free(cs->state, slotType);
                ZrParser_InferredType_Copy(cs->state, slotType, paramType);
            }
        }
    }

cleanup:
    if (outMismatch != ZR_NULL) {
        *outMismatch = mismatch;
    }
    if (provided != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      provided,
                                      sizeof(TZrBool) * slotCount,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (mismatch) {
        free_inferred_type_array(cs->state, orderedArgTypes);
        ZrCore_Array_Construct(orderedArgTypes);
    }
    return ZR_TRUE;

error:
    if (provided != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      provided,
                                      sizeof(TZrBool) * slotCount,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    free_inferred_type_array(cs->state, orderedArgTypes);
    ZrCore_Array_Construct(orderedArgTypes);
    return ZR_FALSE;
}

static TZrInt32 member_resolution_score_candidate(SZrCompilerState *cs,
                                                  const SZrTypeMemberInfo *memberInfo,
                                                  const SZrResolvedCallSignature *resolvedSignature,
                                                  const SZrArray *orderedArgTypes) {
    const SZrArray *parameterTypes =
            resolvedSignature != ZR_NULL && resolvedSignature->parameterTypes.length > 0
                    ? &resolvedSignature->parameterTypes
                    : (memberInfo != ZR_NULL ? &memberInfo->parameterTypes : ZR_NULL);

    if (cs == ZR_NULL || memberInfo == ZR_NULL || parameterTypes == ZR_NULL) {
        return -1;
    }

    if (parameterTypes->length != orderedArgTypes->length) {
        return -1;
    }

    {
        TZrInt32 score = 0;
        for (TZrSize index = 0; index < orderedArgTypes->length; index++) {
            SZrInferredType *argType = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)orderedArgTypes, index);
            SZrInferredType *paramType = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)parameterTypes, index);

            if (argType == ZR_NULL || paramType == ZR_NULL) {
                return -1;
            }
            if (ZrParser_InferredType_Equal(argType, paramType)) {
                continue;
            }
            if (!ZrParser_InferredType_IsCompatible(argType, paramType) &&
                !inferred_type_can_use_named_constraint_fallback(cs, argType, paramType)) {
                return -1;
            }
            score += 1;
        }
        return score;
    }
}

static TZrBool member_resolution_member_is_callable(const SZrTypeMemberInfo *memberInfo) {
    if (memberInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    return memberInfo->memberType == ZR_AST_CLASS_METHOD ||
           memberInfo->memberType == ZR_AST_STRUCT_METHOD ||
           memberInfo->memberType == ZR_AST_CLASS_META_FUNCTION ||
           memberInfo->memberType == ZR_AST_STRUCT_META_FUNCTION ||
           memberInfo->memberType == ZR_AST_INTERFACE_METHOD_SIGNATURE ||
           memberInfo->memberType == ZR_AST_INTERFACE_META_SIGNATURE ||
           memberInfo->compiledFunction != ZR_NULL ||
           memberInfo->returnTypeName != ZR_NULL ||
           memberInfo->parameterTypes.length > 0;
}

static void member_resolution_capture_first_mismatch(SZrCompilerState *cs,
                                                     const SZrArray *parameterTypes,
                                                     const SZrArray *orderedArgTypes,
                                                     SZrInferredType *outExpectedType,
                                                     SZrInferredType *outActualType,
                                                     TZrBool *ioHasMismatch) {
    if (cs == ZR_NULL ||
        parameterTypes == ZR_NULL ||
        orderedArgTypes == ZR_NULL ||
        outExpectedType == ZR_NULL ||
        outActualType == ZR_NULL ||
        ioHasMismatch == ZR_NULL ||
        *ioHasMismatch ||
        parameterTypes->length != orderedArgTypes->length) {
        return;
    }

    for (TZrSize index = 0; index < orderedArgTypes->length; index++) {
        SZrInferredType *argType = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)orderedArgTypes, index);
        SZrInferredType *paramType = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)parameterTypes, index);

        if (argType == ZR_NULL || paramType == ZR_NULL) {
            continue;
        }
        if (ZrParser_InferredType_Equal(argType, paramType) ||
            ZrParser_InferredType_IsCompatible(argType, paramType) ||
            inferred_type_can_use_named_constraint_fallback(cs, argType, paramType)) {
            continue;
        }

        ZrParser_InferredType_Copy(cs->state, outExpectedType, paramType);
        ZrParser_InferredType_Copy(cs->state, outActualType, argType);
        *ioHasMismatch = ZR_TRUE;
        return;
    }
}

static TZrBool member_resolution_scan_direct_members(SZrCompilerState *cs,
                                                     SZrString *typeName,
                                                     SZrString *memberName,
                                                     SZrFunctionCall *call,
                                                     const SZrArray *callArgTypes,
                                                     TZrUInt32 depth,
                                                     SZrTypeMemberInfo **ioBestMember,
                                                     TZrInt32 *ioBestScore,
                                                     TZrBool *ioHasTie,
                                                     TZrBool *ioSawCandidate,
                                                     TZrBool *ioSawTypedCandidate,
                                                     TZrBool *ioHasFirstMismatch,
                                                     SZrInferredType *ioFirstExpectedType,
                                                     SZrInferredType *ioFirstActualType,
                                                     TZrBool *ioHasGenericDiagnostic,
                                                     TZrChar *ioGenericDiagnostic,
                                                     TZrSize genericDiagnosticSize) {
    SZrTypePrototypeInfo *info;
    SZrArray membersSnapshot;
    SZrArray implementsSnapshot;
    SZrArray inheritsSnapshot;

    if (cs == ZR_NULL || typeName == ZR_NULL || memberName == ZR_NULL || ioBestMember == ZR_NULL ||
        ioBestScore == ZR_NULL || ioHasTie == ZR_NULL || ioSawCandidate == ZR_NULL ||
        ioSawTypedCandidate == ZR_NULL || ioHasFirstMismatch == ZR_NULL ||
        ioFirstExpectedType == ZR_NULL || ioFirstActualType == ZR_NULL ||
        ioHasGenericDiagnostic == ZR_NULL || ioGenericDiagnostic == ZR_NULL ||
        depth > ZR_PARSER_RECURSIVE_MEMBER_LOOKUP_MAX_DEPTH) {
        return ZR_FALSE;
    }

    info = find_compiler_type_prototype_inference(cs, typeName);
    if (info == ZR_NULL) {
        return ZR_TRUE;
    }

    membersSnapshot = info->members;
    implementsSnapshot = info->implements;
    inheritsSnapshot = info->inherits;

    for (TZrSize index = 0; index < membersSnapshot.length; index++) {
        SZrTypeMemberInfo *memberInfo = (SZrTypeMemberInfo *)ZrCore_Array_Get(&membersSnapshot, index);
        SZrResolvedCallSignature resolvedSignature;
        SZrArray orderedArgTypes;
        TZrBool mismatch = ZR_FALSE;
        TZrInt32 score;

        if (memberInfo == ZR_NULL ||
            memberInfo->name == ZR_NULL ||
            !ZrCore_String_Equal(memberInfo->name, memberName) ||
            !member_resolution_member_is_callable(memberInfo)) {
            continue;
        }

        *ioSawCandidate = ZR_TRUE;
        if (memberInfo->parameterTypes.length > 0) {
            *ioSawTypedCandidate = ZR_TRUE;
        }
        ZrCore_Array_Construct(&orderedArgTypes);
        if (!member_resolution_build_argument_slots(cs,
                                                    memberInfo,
                                                    call,
                                                    callArgTypes,
                                                    &orderedArgTypes,
                                                    &mismatch)) {
            return ZR_FALSE;
        }
        if (mismatch) {
            continue;
        }

        ZrCore_Memory_RawSet(&resolvedSignature, 0, sizeof(resolvedSignature));
        ZrParser_InferredType_Init(cs->state, &resolvedSignature.returnType, ZR_VALUE_TYPE_OBJECT);
        ZrCore_Array_Construct(&resolvedSignature.parameterTypes);
        ZrCore_Array_Construct(&resolvedSignature.parameterPassingModes);
        {
            TZrChar diagnostic[ZR_PARSER_DETAIL_BUFFER_LENGTH];
            EZrGenericCallResolveStatus genericStatus;

            diagnostic[0] = '\0';
            genericStatus = resolve_generic_member_call_signature_detailed(cs,
                                                                           memberInfo,
                                                                           call,
                                                                           &resolvedSignature,
                                                                           diagnostic,
                                                                           sizeof(diagnostic));
            if (genericStatus != ZR_GENERIC_CALL_RESOLVE_OK) {
                if (!*ioHasGenericDiagnostic && diagnostic[0] != '\0' && genericDiagnosticSize > 0) {
                    snprintf(ioGenericDiagnostic, genericDiagnosticSize, "%s", diagnostic);
                    *ioHasGenericDiagnostic = ZR_TRUE;
                }
                free_resolved_call_signature(cs->state, &resolvedSignature);
                free_inferred_type_array(cs->state, &orderedArgTypes);
                continue;
            }
        }
        {
            score = member_resolution_score_candidate(cs, memberInfo, &resolvedSignature, &orderedArgTypes);
            if (score < 0 && resolvedSignature.parameterTypes.length > 0) {
                *ioSawTypedCandidate = ZR_TRUE;
                member_resolution_capture_first_mismatch(cs,
                                                         &resolvedSignature.parameterTypes,
                                                         &orderedArgTypes,
                                                         ioFirstExpectedType,
                                                         ioFirstActualType,
                                                         ioHasFirstMismatch);
            }
        }
        free_resolved_call_signature(cs->state, &resolvedSignature);
        free_inferred_type_array(cs->state, &orderedArgTypes);

        if (score < 0) {
            continue;
        }
        if (*ioBestMember == ZR_NULL || score < *ioBestScore) {
            *ioBestMember = memberInfo;
            *ioBestScore = score;
            *ioHasTie = ZR_FALSE;
        } else if (score == *ioBestScore) {
            *ioHasTie = ZR_TRUE;
        }
    }

    if (*ioBestMember != ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrSize index = 0; index < implementsSnapshot.length; index++) {
        SZrString **implementedTypeNamePtr = (SZrString **)ZrCore_Array_Get(&implementsSnapshot, index);
        if (implementedTypeNamePtr == ZR_NULL || *implementedTypeNamePtr == ZR_NULL ||
            ZrCore_String_Equal(*implementedTypeNamePtr, typeName)) {
            continue;
        }
        if (!member_resolution_scan_direct_members(cs,
                                                   *implementedTypeNamePtr,
                                                   memberName,
                                                   call,
                                                   callArgTypes,
                                                   depth + 1,
                                                   ioBestMember,
                                                   ioBestScore,
                                                   ioHasTie,
                                                   ioSawCandidate,
                                                   ioSawTypedCandidate,
                                                   ioHasFirstMismatch,
                                                   ioFirstExpectedType,
                                                   ioFirstActualType,
                                                   ioHasGenericDiagnostic,
                                                   ioGenericDiagnostic,
                                                   genericDiagnosticSize)) {
            return ZR_FALSE;
        }
        if (*ioBestMember != ZR_NULL) {
            return ZR_TRUE;
        }
    }

    for (TZrSize index = 0; index < inheritsSnapshot.length; index++) {
        SZrString **inheritTypeNamePtr = (SZrString **)ZrCore_Array_Get(&inheritsSnapshot, index);
        if (inheritTypeNamePtr == ZR_NULL || *inheritTypeNamePtr == ZR_NULL ||
            ZrCore_String_Equal(*inheritTypeNamePtr, typeName)) {
            continue;
        }
        if (!member_resolution_scan_direct_members(cs,
                                                   *inheritTypeNamePtr,
                                                   memberName,
                                                   call,
                                                   callArgTypes,
                                                   depth + 1,
                                                   ioBestMember,
                                                   ioBestScore,
                                                   ioHasTie,
                                                   ioSawCandidate,
                                                   ioSawTypedCandidate,
                                                   ioHasFirstMismatch,
                                                   ioFirstExpectedType,
                                                   ioFirstActualType,
                                                   ioHasGenericDiagnostic,
                                                   ioGenericDiagnostic,
                                                   genericDiagnosticSize)) {
            return ZR_FALSE;
        }
        if (*ioBestMember != ZR_NULL) {
            return ZR_TRUE;
        }
    }

    return ZR_TRUE;
}

TZrBool find_compiler_type_member_call_inference(SZrCompilerState *cs,
                                                 SZrString *typeName,
                                                 SZrString *memberName,
                                                 SZrFunctionCall *call,
                                                 SZrFileRange location,
                                                 SZrTypeMemberInfo **outMember) {
    SZrArray callArgTypes;
    SZrTypeMemberInfo *bestMember = ZR_NULL;
    TZrInt32 bestScore = INT_MAX;
    TZrBool hasTie = ZR_FALSE;
    TZrBool sawCandidate = ZR_FALSE;
    TZrBool sawTypedCandidate = ZR_FALSE;
    TZrBool hasFirstMismatch = ZR_FALSE;
    TZrBool hasGenericDiagnostic = ZR_FALSE;
    TZrChar genericDiagnostic[ZR_PARSER_DETAIL_BUFFER_LENGTH];
    SZrInferredType firstExpectedType;
    SZrInferredType firstActualType;

    if (outMember != ZR_NULL) {
        *outMember = ZR_NULL;
    }
    if (cs == ZR_NULL || typeName == ZR_NULL || memberName == ZR_NULL || outMember == ZR_NULL) {
        return ZR_FALSE;
    }

    genericDiagnostic[0] = '\0';
    ZrParser_InferredType_Init(cs->state, &firstExpectedType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(cs->state, &firstActualType, ZR_VALUE_TYPE_OBJECT);
    if (!member_resolution_infer_call_arguments(cs, call, &callArgTypes)) {
        ZrParser_InferredType_Free(cs->state, &firstExpectedType);
        ZrParser_InferredType_Free(cs->state, &firstActualType);
        return ZR_FALSE;
    }

    if (!member_resolution_scan_direct_members(cs,
                                               typeName,
                                               memberName,
                                               call,
                                               &callArgTypes,
                                               0,
                                               &bestMember,
                                               &bestScore,
                                               &hasTie,
                                               &sawCandidate,
                                               &sawTypedCandidate,
                                               &hasFirstMismatch,
                                               &firstExpectedType,
                                               &firstActualType,
                                               &hasGenericDiagnostic,
                                               genericDiagnostic,
                                               sizeof(genericDiagnostic))) {
        free_inferred_type_array(cs->state, &callArgTypes);
        ZrParser_InferredType_Free(cs->state, &firstExpectedType);
        ZrParser_InferredType_Free(cs->state, &firstActualType);
        return ZR_FALSE;
    }
    free_inferred_type_array(cs->state, &callArgTypes);

    if (bestMember == ZR_NULL) {
        if (sawTypedCandidate) {
            if (hasGenericDiagnostic) {
                ZrParser_Compiler_Error(cs, genericDiagnostic, location);
            } else if (hasFirstMismatch) {
                ZrParser_TypeError_Report(cs,
                                          "Argument type mismatch",
                                          &firstExpectedType,
                                          &firstActualType,
                                          location);
            }
            ZrParser_InferredType_Free(cs->state, &firstExpectedType);
            ZrParser_InferredType_Free(cs->state, &firstActualType);
            return ZR_FALSE;
        }
        bestMember = find_compiler_type_member_inference(cs, typeName, memberName);
        *outMember = bestMember;
        ZrParser_InferredType_Free(cs->state, &firstExpectedType);
        ZrParser_InferredType_Free(cs->state, &firstActualType);
        return ZR_TRUE;
    }

    if (hasTie) {
        TZrChar errorMessage[ZR_PARSER_ERROR_BUFFER_LENGTH];
        snprintf(errorMessage,
                 sizeof(errorMessage),
                 "Ambiguous overload for member '%s'",
                 ZrCore_String_GetNativeString(memberName));
        ZrParser_Compiler_Error(cs, errorMessage, location);
        ZrParser_InferredType_Free(cs->state, &firstExpectedType);
        ZrParser_InferredType_Free(cs->state, &firstActualType);
        return ZR_FALSE;
    }

    *outMember = bestMember;
    ZrParser_InferredType_Free(cs->state, &firstExpectedType);
    ZrParser_InferredType_Free(cs->state, &firstActualType);
    return ZR_TRUE;
}
