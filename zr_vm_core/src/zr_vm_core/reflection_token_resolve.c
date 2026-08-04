#include "zr_vm_core/reflection.h"

#include "zr_vm_core/memory.h"
#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_core/value.h"

static void reflection_clear_resolved_token(SZrReflectionResolvedToken *resolved) {
    if (resolved != ZR_NULL) {
        ZrCore_Memory_RawSet(resolved, 0, sizeof(*resolved));
    }
}

static void reflection_clear_resolved_generic_argument(SZrReflectionResolvedGenericArgument *argument) {
    if (argument != ZR_NULL) {
        ZrCore_Memory_RawSet(argument, 0, sizeof(*argument));
    }
}

static void reflection_clear_resolved_generic_parameter(SZrReflectionResolvedGenericParameter *parameter) {
    if (parameter != ZR_NULL) {
        ZrCore_Memory_RawSet(parameter, 0, sizeof(*parameter));
    }
}

static void reflection_clear_resolved_generic_parameter_constraint(
        SZrReflectionResolvedGenericParameterConstraint *constraint) {
    if (constraint != ZR_NULL) {
        ZrCore_Memory_RawSet(constraint, 0, sizeof(*constraint));
    }
}

static void reflection_clear_resolved_method_spec_generic_argument(
        SZrReflectionResolvedMethodSpecGenericArgument *argument) {
    if (argument != ZR_NULL) {
        ZrCore_Memory_RawSet(argument, 0, sizeof(*argument));
    }
}

static void reflection_fill_generic_parameter(
        const SZrMetadataRuntimeGenericParamView *view,
        SZrReflectionResolvedGenericParameter *parameter) {
    parameter->ownerToken = view->ownerToken;
    parameter->ownerRecord = view->ownerRecord;
    parameter->genericParamRow = view->genericParamRow;
    parameter->genericParamIndex = view->genericParamIndex;
    parameter->parameterIndex = view->parameterIndex;
    parameter->nameStringOffset = view->nameStringOffset;
    parameter->firstConstraintIndex = view->firstConstraintIndex;
    parameter->constraintCount = view->constraintCount;
    parameter->flags = view->flags;
}

static void reflection_fill_method_signature(SZrMetadataRuntime *runtime,
                                             TZrMetadataToken methodToken,
                                             SZrReflectionResolvedToken *resolved) {
    const SZrMetadataTokenRecord *signatureRecord =
            ZrCore_MetadataRuntime_ResolveSignatureRecord(runtime, methodToken);

    if (signatureRecord == ZR_NULL) {
        return;
    }

    resolved->methodSignatureToken = signatureRecord->token;
    resolved->methodSignatureRecord = signatureRecord;
    resolved->methodSignatureHash = signatureRecord->signatureHash;
}

static void reflection_fill_method_binding(SZrMetadataRuntime *runtime,
                                           TZrMetadataToken methodToken,
                                           SZrReflectionResolvedToken *resolved) {
    SZrMetadataRuntimeMethodBindingView view;

    if (!ZrCore_MetadataRuntime_ReadMethodBindingView(runtime, methodToken, &view)) {
        return;
    }

    resolved->methodFunctionIndex = view.functionIndex;
    resolved->methodInfo = view.methodInfo;
    resolved->methodFunctionPointer = view.functionPointer;
    resolved->methodInvoker = view.invoker;
}

static TZrBool reflection_resolve_type_def_token(SZrMetadataRuntime *runtime,
                                                 TZrMetadataToken token,
                                                 SZrReflectionResolvedToken *resolved) {
    SZrMetadataRuntimeTypeDefLayoutBindingView view;

    if (!ZrCore_MetadataRuntime_ReadTypeDefLayoutBindingView(runtime, token, &view)) {
        return ZR_FALSE;
    }

    resolved->kind = ZR_REFLECTION_RESOLVED_TOKEN_TYPE;
    resolved->token = token;
    resolved->record = view.typeRecord;
    resolved->typeDefRow = view.typeDefRow;
    resolved->typeLayoutId = view.typeLayoutId;
    resolved->cTypeId = view.cTypeId;
    resolved->typeLayout = view.typeLayout;
    return ZR_TRUE;
}

static TZrBool reflection_resolve_type_spec_token(SZrMetadataRuntime *runtime,
                                                  TZrMetadataToken token,
                                                  SZrReflectionResolvedToken *resolved) {
    SZrMetadataRuntimeTypeSpecLayoutBindingView view;

    if (!ZrCore_MetadataRuntime_ReadTypeSpecLayoutBindingView(runtime, token, &view)) {
        return ZR_FALSE;
    }

    resolved->kind = ZR_REFLECTION_RESOLVED_TOKEN_TYPE;
    resolved->token = token;
    resolved->record = view.typeRecord;
    resolved->typeSpecRow = view.typeSpecRow;
    resolved->genericSignatureToken = view.genericBindingView.signatureView.signatureToken;
    resolved->genericSignatureHash = view.genericBindingView.signatureView.signatureHash;
    resolved->genericBaseToken = view.genericBindingView.baseToken;
    resolved->genericBaseRecord = view.genericBindingView.baseRecord;
    resolved->genericArgumentCount = view.genericBindingView.signatureView.argumentCount;
    resolved->genericArgumentListBlobOffset = view.genericBindingView.signatureView.argumentListBlobOffset;
    resolved->typeLayoutId = view.typeLayoutId;
    resolved->cTypeId = view.cTypeId;
    resolved->typeLayout = view.typeLayout;
    return ZR_TRUE;
}

static TZrBool reflection_resolve_type_record_token(SZrMetadataRuntime *runtime,
                                                    TZrMetadataToken token,
                                                    SZrReflectionResolvedToken *resolved) {
    const SZrMetadataTokenRecord *record = ZrCore_MetadataRuntime_ResolveTypeRecord(runtime, token);

    if (record == ZR_NULL) {
        return ZR_FALSE;
    }

    resolved->kind = ZR_REFLECTION_RESOLVED_TOKEN_TYPE;
    resolved->token = token;
    resolved->record = record;
    return ZR_TRUE;
}

static TZrBool reflection_resolve_field_token(SZrMetadataRuntime *runtime,
                                              TZrMetadataToken token,
                                              SZrReflectionResolvedToken *resolved) {
    SZrMetadataRuntimeFieldDefLayoutBindingView view;

    if (!ZrCore_MetadataRuntime_ReadFieldDefLayoutBindingView(runtime, token, &view)) {
        return ZR_FALSE;
    }

    resolved->kind = ZR_REFLECTION_RESOLVED_TOKEN_FIELD;
    resolved->token = token;
    resolved->record = view.fieldRecord;
    resolved->fieldDefRow = view.fieldDefRow;
    resolved->ownerTypeToken = view.ownerTypeToken;
    resolved->ownerTypeRecord = view.ownerTypeRecord;
    resolved->ownerTypeDefRow = view.ownerTypeDefRow;
    resolved->fieldTypeToken = ZrCore_MetadataRuntime_ResolveTypeLayoutToken(runtime, view.fieldTypeLayoutId);
    if (resolved->fieldTypeToken != 0u) {
        resolved->fieldTypeRecord = ZrCore_MetadataRuntime_ResolveTypeRecord(runtime, resolved->fieldTypeToken);
    }
    resolved->byteOffset = view.byteOffset;
    resolved->fieldTypeLayoutId = view.fieldTypeLayoutId;
    resolved->ownerTypeLayoutId = view.ownerTypeLayoutId;
    resolved->fieldTypeLayout = view.fieldTypeLayout;
    resolved->ownerTypeLayout = view.ownerTypeLayout;
    return ZR_TRUE;
}

static TZrBool reflection_resolve_method_token(SZrMetadataRuntime *runtime,
                                               TZrMetadataToken token,
                                               SZrReflectionResolvedToken *resolved) {
    const SZrMetadataTokenRecord *record = ZrCore_MetadataRuntime_ResolveMethodRecord(runtime, token);

    if (record == ZR_NULL) {
        return ZR_FALSE;
    }

    resolved->kind = ZR_REFLECTION_RESOLVED_TOKEN_METHOD;
    resolved->token = token;
    resolved->record = record;
    resolved->methodToken = token;
    resolved->methodRecord = record;
    reflection_fill_method_signature(runtime, token, resolved);
    reflection_fill_method_binding(runtime, token, resolved);
    return ZR_TRUE;
}

static TZrBool reflection_resolve_method_spec_token(SZrMetadataRuntime *runtime,
                                                    TZrMetadataToken token,
                                                    SZrReflectionResolvedToken *resolved) {
    SZrMetadataRuntimeMethodSpecSignatureView view;

    if (!ZrCore_MetadataRuntime_ReadMethodSpecSignatureView(runtime, token, &view)) {
        return ZR_FALSE;
    }

    resolved->kind = ZR_REFLECTION_RESOLVED_TOKEN_METHOD;
    resolved->token = token;
    resolved->record = view.methodSpecRecord;
    resolved->methodToken = view.methodToken;
    resolved->methodRecord = view.methodRecord;
    resolved->methodSignatureToken = view.methodSpecToken;
    resolved->methodSignatureRecord = view.methodSpecRecord;
    resolved->methodSignatureHash = view.signatureHash;
    reflection_fill_method_binding(runtime, view.methodToken, resolved);
    resolved->genericSignatureHash = view.signatureHash;
    resolved->genericArgumentCount = view.argumentCount;
    resolved->genericArgumentListBlobOffset = view.argumentListBlobOffset;
    return ZR_TRUE;
}

TZrBool ZrCore_Reflection_ResolveToken(SZrMetadataRuntime *runtime,
                                       TZrMetadataToken token,
                                       SZrReflectionResolvedToken *outResolved) {
    TZrUInt32 table = ZR_METADATA_TOKEN_TABLE(token);

    reflection_clear_resolved_token(outResolved);
    if (runtime == ZR_NULL || token == 0u || outResolved == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (table) {
        case ZR_METADATA_TABLE_TYPE_DEF:
            return reflection_resolve_type_def_token(runtime, token, outResolved);
        case ZR_METADATA_TABLE_TYPE_SPEC:
            return reflection_resolve_type_spec_token(runtime, token, outResolved);
        case ZR_METADATA_TABLE_TYPE_REF:
            return reflection_resolve_type_record_token(runtime, token, outResolved);
        case ZR_METADATA_TABLE_MEMBER_DEF:
            if (reflection_resolve_field_token(runtime, token, outResolved)) {
                return ZR_TRUE;
            }
            reflection_clear_resolved_token(outResolved);
            return reflection_resolve_method_token(runtime, token, outResolved);
        case ZR_METADATA_TABLE_MEMBER_REF:
            return reflection_resolve_method_token(runtime, token, outResolved);
        case ZR_METADATA_TABLE_SIGNATURE:
            return reflection_resolve_method_spec_token(runtime, token, outResolved);
        default:
            return ZR_FALSE;
    }
}

static TZrBool reflection_resolve_invokable_method(SZrMetadataRuntime *runtime,
                                                   TZrMetadataToken methodToken,
                                                   SZrReflectionResolvedToken *outResolved) {
    if (!ZrCore_Reflection_ResolveToken(runtime, methodToken, outResolved)) {
        return ZR_FALSE;
    }
    return outResolved->kind == ZR_REFLECTION_RESOLVED_TOKEN_METHOD && outResolved->methodInfo != ZR_NULL &&
           outResolved->methodFunctionPointer != ZR_NULL && outResolved->methodInvoker != ZR_NULL;
}

static TZrBool reflection_signature_base_type_accepts_argument(TZrUInt16 baseType, const struct SZrTypeValue *argument) {
    if (baseType >= (TZrUInt16)ZR_VALUE_TYPE_ENUM_MAX) {
        return ZR_FALSE;
    }
    if (baseType == (TZrUInt16)ZR_VALUE_TYPE_NULL || baseType == (TZrUInt16)ZR_VALUE_TYPE_UNKNOWN) {
        return ZR_TRUE;
    }
    return (TZrBool)(argument != ZR_NULL && argument->type == (EZrValueType)baseType);
}

static TZrBool reflection_signature_accepts_fixed_argument_types(const SZrAotSignature *signature,
                                                                 const struct SZrTypeValue *args) {
    TZrUInt32 index;

    if (signature == ZR_NULL || (signature->parameterCount > 0u && args == ZR_NULL)) {
        return ZR_FALSE;
    }

    for (index = 0u; index < signature->parameterCount; index++) {
        if (signature->parameterTypes[index].passingMode !=
            (TZrUInt8)ZR_AOT_PARAMETER_PASSING_VALUE ||
            !reflection_signature_base_type_accepts_argument(
                    signature->parameterTypes[index].baseType, &args[index])) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool reflection_method_signature_is_value_only(
        const SZrAotMethodInfo *methodInfo) {
    const SZrAotSignature *signature;

    if (methodInfo == ZR_NULL || methodInfo->signature == ZR_NULL) {
        return ZR_FALSE;
    }
    signature = methodInfo->signature;
    if (signature->parameterCount > 0u && signature->parameterTypes == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrUInt32 index = 0u; index < signature->parameterCount; index++) {
        if (signature->parameterTypes[index].passingMode !=
            (TZrUInt8)ZR_AOT_PARAMETER_PASSING_VALUE) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool reflection_method_signature_accepts_arguments(const SZrAotMethodInfo *methodInfo,
                                                             struct SZrTypeValue *args,
                                                             TZrUInt32 argCount) {
    const SZrAotSignature *signature;

    if (methodInfo == ZR_NULL || methodInfo->signature == ZR_NULL || (argCount > 0u && args == ZR_NULL)) {
        return ZR_FALSE;
    }

    signature = methodInfo->signature;
    if ((signature->parameterCount > 0u && signature->parameterTypes == ZR_NULL) ||
        (signature->hasReturnValue && signature->returnType == ZR_NULL)) {
        return ZR_FALSE;
    }
    if (signature->hasVarArgs) {
        if (argCount < signature->parameterCount) {
            return ZR_FALSE;
        }
    } else if (argCount != signature->parameterCount) {
        return ZR_FALSE;
    }
    return reflection_signature_accepts_fixed_argument_types(signature, args);
}

static TZrBool reflection_method_signature_accepts_return_value(const SZrAotMethodInfo *methodInfo,
                                                                const struct SZrTypeValue *outReturn) {
    const SZrAotSignature *signature;

    if (methodInfo == ZR_NULL || methodInfo->signature == ZR_NULL) {
        return ZR_FALSE;
    }

    signature = methodInfo->signature;
    if (!signature->hasReturnValue) {
        return ZR_TRUE;
    }
    return reflection_signature_base_type_accepts_argument(signature->returnType->baseType, outReturn);
}

static void reflection_method_prepare_return_value(const SZrAotMethodInfo *methodInfo,
                                                   struct SZrTypeValue *outReturn) {
    if (methodInfo == ZR_NULL ||
        methodInfo->signature == ZR_NULL ||
        !methodInfo->signature->hasReturnValue ||
        outReturn == ZR_NULL) {
        return;
    }

    ZrCore_Value_ResetAsNull(outReturn);
}

static void reflection_method_finish_return_value(const SZrAotMethodInfo *methodInfo,
                                                  struct SZrTypeValue *outReturn) {
    if (methodInfo == ZR_NULL ||
        methodInfo->signature == ZR_NULL ||
        methodInfo->signature->hasReturnValue ||
        outReturn == ZR_NULL) {
        return;
    }

    ZrCore_Value_ResetAsNull(outReturn);
}

static TZrBool reflection_dispatch_invokable_method(struct SZrState *state,
                                                    const SZrReflectionResolvedToken *resolved,
                                                    struct SZrTypeValue *self,
                                                    struct SZrTypeValue *args,
                                                    struct SZrTypeValue *outReturn) {
    resolved->methodInvoker(state, resolved->methodFunctionPointer, resolved->methodInfo, self, args, outReturn);
    return ZR_TRUE;
}

TZrBool ZrCore_Reflection_InvokeMethodToken(struct SZrState *state,
                                            SZrMetadataRuntime *runtime,
                                            TZrMetadataToken methodToken,
                                            struct SZrTypeValue *self,
                                            struct SZrTypeValue *args,
                                            struct SZrTypeValue *outReturn) {
    SZrReflectionResolvedToken resolved;

    if (state == ZR_NULL || runtime == ZR_NULL || methodToken == 0u || outReturn == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!reflection_resolve_invokable_method(runtime, methodToken, &resolved)) {
        return ZR_FALSE;
    }
    if (!reflection_method_signature_is_value_only(resolved.methodInfo)) {
        return ZR_FALSE;
    }

    return reflection_dispatch_invokable_method(state, &resolved, self, args, outReturn);
}

TZrBool ZrCore_Reflection_InvokeMethodTokenWithArgCount(struct SZrState *state,
                                                        SZrMetadataRuntime *runtime,
                                                        TZrMetadataToken methodToken,
                                                        struct SZrTypeValue *self,
                                                        struct SZrTypeValue *args,
                                                        TZrUInt32 argCount,
                                                        struct SZrTypeValue *outReturn) {
    SZrReflectionResolvedToken resolved;

    if (state == ZR_NULL || runtime == ZR_NULL || methodToken == 0u || outReturn == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!reflection_resolve_invokable_method(runtime, methodToken, &resolved)) {
        return ZR_FALSE;
    }
    if (!reflection_method_signature_accepts_arguments(resolved.methodInfo, args, argCount)) {
        return ZR_FALSE;
    }

    reflection_method_prepare_return_value(resolved.methodInfo, outReturn);
    if (!reflection_dispatch_invokable_method(state, &resolved, self, args, outReturn)) {
        return ZR_FALSE;
    }
    reflection_method_finish_return_value(resolved.methodInfo, outReturn);
    return reflection_method_signature_accepts_return_value(resolved.methodInfo, outReturn);
}

TZrBool ZrCore_Reflection_ResolveTypeSpecGenericArgument(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken typeSpecToken,
        TZrUInt32 argumentIndex,
        SZrReflectionResolvedGenericArgument *outArgument) {
    SZrMetadataRuntimeTypeSpecGenericArgumentView view;

    reflection_clear_resolved_generic_argument(outArgument);
    if (runtime == ZR_NULL || typeSpecToken == 0u || outArgument == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!ZrCore_MetadataRuntime_ReadTypeSpecGenericArgumentView(runtime, typeSpecToken, argumentIndex, &view)) {
        return ZR_FALSE;
    }

    outArgument->typeSpecToken = view.bindingView.signatureView.typeSpecToken;
    outArgument->genericSignatureToken = view.bindingView.signatureView.signatureToken;
    outArgument->genericSignatureHash = view.bindingView.signatureView.signatureHash;
    outArgument->genericBaseToken = view.bindingView.baseToken;
    outArgument->genericBaseRecord = view.bindingView.baseRecord;
    outArgument->argumentIndex = view.argumentIndex;
    outArgument->argumentNodeKind = (TZrUInt32)view.argumentNode.node;
    outArgument->argumentPayload0 = view.argumentNode.payload0;
    outArgument->argumentPayload1 = view.argumentNode.payload1;
    outArgument->argumentToken = view.argumentToken;
    outArgument->argumentRecord = view.argumentRecord;
    return ZR_TRUE;
}

TZrBool ZrCore_Reflection_ResolveGenericParameter(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken ownerToken,
        TZrUInt32 parameterIndex,
        SZrReflectionResolvedGenericParameter *outParameter) {
    SZrMetadataRuntimeGenericParamView view;

    reflection_clear_resolved_generic_parameter(outParameter);
    if (runtime == ZR_NULL || ownerToken == 0u || outParameter == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!ZrCore_MetadataRuntime_ReadGenericParamView(runtime, ownerToken, parameterIndex, &view)) {
        return ZR_FALSE;
    }

    reflection_fill_generic_parameter(&view, outParameter);
    return ZR_TRUE;
}

TZrBool ZrCore_Reflection_ResolveGenericParameterConstraint(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken ownerToken,
        TZrUInt32 parameterIndex,
        TZrUInt32 constraintIndex,
        SZrReflectionResolvedGenericParameterConstraint *outConstraint) {
    SZrMetadataRuntimeGenericParamConstraintView view;

    reflection_clear_resolved_generic_parameter_constraint(outConstraint);
    if (runtime == ZR_NULL || ownerToken == 0u || outConstraint == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!ZrCore_MetadataRuntime_ReadGenericParamConstraintView(runtime,
                                                               ownerToken,
                                                               parameterIndex,
                                                               constraintIndex,
                                                               &view)) {
        return ZR_FALSE;
    }

    reflection_fill_generic_parameter(&view.genericParamView, &outConstraint->genericParameter);
    outConstraint->constraintRow = view.constraintRow;
    outConstraint->constraintIndex = view.constraintIndex;
    outConstraint->constraintTypeToken = view.constraintTypeToken;
    outConstraint->constraintTypeRecord = view.constraintTypeRecord;
    outConstraint->signatureBlobData = view.signatureBlob.data;
    outConstraint->signatureBlobByteLength = view.signatureBlob.byteLength;
    outConstraint->signatureBlobOffset = view.constraintRow->signatureBlobOffset;
    outConstraint->signatureBlobLength = view.constraintRow->signatureBlobLength;
    return ZR_TRUE;
}

TZrBool ZrCore_Reflection_ResolveMethodSpecGenericArgument(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken methodSpecToken,
        TZrUInt32 argumentIndex,
        SZrReflectionResolvedMethodSpecGenericArgument *outArgument) {
    SZrMetadataRuntimeMethodSpecGenericArgumentView view;

    reflection_clear_resolved_method_spec_generic_argument(outArgument);
    if (runtime == ZR_NULL || methodSpecToken == 0u || outArgument == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!ZrCore_MetadataRuntime_ReadMethodSpecGenericArgumentView(runtime,
                                                                 methodSpecToken,
                                                                 argumentIndex,
                                                                 &view)) {
        return ZR_FALSE;
    }

    outArgument->methodSpecToken = view.signatureView.methodSpecToken;
    outArgument->methodToken = view.signatureView.methodToken;
    outArgument->methodRecord = view.signatureView.methodRecord;
    outArgument->genericSignatureHash = view.signatureView.signatureHash;
    outArgument->argumentIndex = view.argumentIndex;
    outArgument->argumentNodeKind = (TZrUInt32)view.argumentNode.node;
    outArgument->argumentPayload0 = view.argumentNode.payload0;
    outArgument->argumentPayload1 = view.argumentNode.payload1;
    outArgument->argumentToken = view.argumentToken;
    outArgument->argumentRecord = view.argumentRecord;
    return ZR_TRUE;
}
