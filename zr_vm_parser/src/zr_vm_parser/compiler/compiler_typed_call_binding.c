#include "compiler_typed_call_binding.h"

#include "zr_vm_parser/canonical_type.h"

void compiler_typed_call_use_generic_dispatch(SZrFunction *function, TZrUInt32 instructionIndex) {
    TZrInstruction *instruction;
    if (instructionIndex >= function->instructionsLength) return;
    instruction = &function->instructionsList[instructionIndex];
    switch ((EZrInstructionCode)instruction->instruction.operationCode) {
        case ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL):
            instruction->instruction.operationCode = ZR_INSTRUCTION_ENUM(FUNCTION_CALL); break;
        case ZR_INSTRUCTION_ENUM(KNOWN_VM_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_TAIL_CALL):
            instruction->instruction.operationCode = ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL); break;
        case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_CALL_NO_ARGS):
            instruction->instruction.operationCode = ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS); break;
        case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_TAIL_CALL_NO_ARGS):
            instruction->instruction.operationCode = ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS); break;
        default: break;
    }
}

TZrBool compiler_register_typed_callable_parameter(SZrCompilerState *compiler,
        SZrString *name, SZrType *type) {
    SZrResolvedCallSignature signature;
    SZrFunctionType *functionType;
    TZrBool valid = ZR_FALSE;
    if (type == ZR_NULL || type->name == ZR_NULL || type->name->type != ZR_AST_FUNCTION_TYPE ||
        type->dimensions != 0) return ZR_TRUE;
    functionType = &type->name->data.functionType;
    if (functionType->generic != ZR_NULL || functionType->args != ZR_NULL) return ZR_FALSE;
    ZrParser_InferredType_Init(compiler->state, &signature.returnType, ZR_VALUE_TYPE_OBJECT);
    ZrCore_Array_Construct(&signature.parameterTypes);
    ZrCore_Array_Construct(&signature.parameterPassingModes);
    if (functionType->returnType != ZR_NULL &&
        !ZrParser_AstTypeToInferredType_Convert(compiler, functionType->returnType, &signature.returnType)) goto cleanup;
    ZrCore_Array_Init(compiler->state, &signature.parameterTypes, sizeof(SZrInferredType),
            functionType->params == ZR_NULL ? 0u : functionType->params->count);
    ZrCore_Array_Init(compiler->state, &signature.parameterPassingModes, sizeof(EZrParameterPassingMode),
            functionType->params == ZR_NULL ? 0u : functionType->params->count);
    for (TZrSize index = 0u; functionType->params != ZR_NULL && index < functionType->params->count; ++index) {
        SZrAstNode *parameterNode = functionType->params->nodes[index];
        SZrInferredType parameterType;
        EZrParameterPassingMode mode;
        if (parameterNode == ZR_NULL || parameterNode->type != ZR_AST_PARAMETER) goto cleanup;
        mode = parameterNode->data.parameter.passingMode;
        ZrParser_InferredType_Init(compiler->state, &parameterType, ZR_VALUE_TYPE_OBJECT);
        if (!ZrParser_AstTypeToInferredType_Convert(compiler, parameterNode->data.parameter.typeInfo, &parameterType)) {
            ZrParser_InferredType_Free(compiler->state, &parameterType);
            goto cleanup;
        }
        ZrCore_Array_Push(compiler->state, &signature.parameterTypes, &parameterType);
        ZrCore_Array_Push(compiler->state, &signature.parameterPassingModes, &mode);
    }
    valid = ZrParser_TypeEnvironment_RegisterCallableValueFunction(compiler->state, compiler->typeEnv,
            name, &signature.returnType, &signature.parameterTypes, ZR_NULL,
            &signature.parameterPassingModes, ZR_NULL);
cleanup:
    for (TZrSize index = 0u; index < signature.parameterTypes.length; ++index)
        ZrParser_InferredType_Free(compiler->state, ZrCore_Array_Get(&signature.parameterTypes, index));
    if (signature.parameterTypes.isValid) ZrCore_Array_Free(compiler->state, &signature.parameterTypes);
    if (signature.parameterPassingModes.isValid) ZrCore_Array_Free(compiler->state, &signature.parameterPassingModes);
    ZrParser_InferredType_Free(compiler->state, &signature.returnType);
    return valid;
}

static TZrBool typed_value_type(SZrCompilerState *compiler, TZrTypeId typeId,
                               SZrInferredType *result) {
    const SZrCanonicalTypeNode *node = ZrParser_CanonicalType_Find(
            compiler->typeEnv->semanticContext, typeId);
    if (node == ZR_NULL) return ZR_FALSE;
    switch (node->kind) {
        case ZR_CANONICAL_TYPE_PRIMITIVE:
            result->baseType = node->data.primitive.valueType;
            return ZR_TRUE;
        case ZR_CANONICAL_TYPE_NOMINAL:
            result->baseType = ZR_VALUE_TYPE_OBJECT;
            result->typeName = node->data.nominal.name;
            return ZR_TRUE;
        case ZR_CANONICAL_TYPE_NULLABLE:
            if (!typed_value_type(compiler, node->data.target.targetTypeId, result)) return ZR_FALSE;
            result->isNullable = ZR_TRUE;
            return ZR_TRUE;
        case ZR_CANONICAL_TYPE_OWNER:
            if (!typed_value_type(compiler, node->data.owner.targetTypeId, result)) return ZR_FALSE;
            switch (node->data.owner.ownerKind) {
                case ZR_CANONICAL_OWNER_UNIQUE: result->ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_UNIQUE; break;
                case ZR_CANONICAL_OWNER_SHARED: result->ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_SHARED; break;
                case ZR_CANONICAL_OWNER_WEAK: result->ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_WEAK; break;
                default: return ZR_FALSE;
            }
            return ZR_TRUE;
        case ZR_CANONICAL_TYPE_ARRAY: {
            SZrInferredType element;
            if (node->data.array.rank != 1u ||
                node->data.array.storageKind != ZR_CANONICAL_ARRAY_STORAGE_MANAGED) return ZR_FALSE;
            ZrParser_InferredType_Init(compiler->state, &element, ZR_VALUE_TYPE_OBJECT);
            if (!typed_value_type(compiler, node->data.array.elementTypeId, &element)) {
                ZrParser_InferredType_Free(compiler->state, &element);
                return ZR_FALSE;
            }
            result->baseType = ZR_VALUE_TYPE_ARRAY;
            ZrCore_Array_Init(compiler->state, &result->elementTypes, sizeof(element), 1u);
            ZrCore_Array_Push(compiler->state, &result->elementTypes, &element);
            return ZR_TRUE;
        }
        default: return ZR_FALSE;
    }
}

TZrBool compiler_resolve_typed_callable_value(SZrCompilerState *compiler,
        SZrString *name, SZrResolvedCallSignature *signature, SZrFileRange location) {
    const SZrTypeBinding *binding;
    const SZrCanonicalTypeNode *node;
    if (compiler->typeEnv == ZR_NULL || compiler->typeEnv->semanticContext == ZR_NULL) return ZR_FALSE;
    binding = ZrParser_TypeEnvironment_FindVariableBinding(compiler->typeEnv, name);
    if (binding == ZR_NULL) return ZR_FALSE;
    node = ZrParser_CanonicalType_Find(compiler->typeEnv->semanticContext, binding->typeId);
    if (node == ZR_NULL || node->kind != ZR_CANONICAL_TYPE_FUNCTION) return ZR_FALSE;
    if (node->data.function.receiverEffect != ZR_CANONICAL_RECEIVER_NONE ||
        node->data.function.effectFlags != ZR_CANONICAL_CALLABLE_EFFECT_NONE ||
        !typed_value_type(compiler, node->data.function.returnTypeId, &signature->returnType)) goto unsupported;
    ZrCore_Array_Init(compiler->state, &signature->parameterTypes, sizeof(SZrInferredType),
            node->data.function.parameterContracts.length);
    ZrCore_Array_Init(compiler->state, &signature->parameterPassingModes, sizeof(EZrParameterPassingMode),
            node->data.function.parameterContracts.length);
    for (TZrSize index = 0u; index < node->data.function.parameterContracts.length; ++index) {
        const SZrCanonicalParameterContract *parameter = ZrCore_Array_Get(
                (SZrArray *)&node->data.function.parameterContracts, index);
        SZrInferredType type;
        EZrParameterPassingMode mode;
        switch (parameter->passingForm) {
            case ZR_CANONICAL_PASSING_VALUE: mode = ZR_PARAMETER_PASSING_MODE_VALUE; break;
            case ZR_CANONICAL_PASSING_IN: mode = ZR_PARAMETER_PASSING_MODE_IN; break;
            case ZR_CANONICAL_PASSING_REF: mode = ZR_PARAMETER_PASSING_MODE_REF; break;
            case ZR_CANONICAL_PASSING_OUT: mode = ZR_PARAMETER_PASSING_MODE_OUT; break;
            default: goto unsupported;
        }
        ZrParser_InferredType_Init(compiler->state, &type, ZR_VALUE_TYPE_OBJECT);
        if (!typed_value_type(compiler, parameter->typeId, &type)) {
            ZrParser_InferredType_Free(compiler->state, &type);
            goto unsupported;
        }
        ZrCore_Array_Push(compiler->state, &signature->parameterTypes, &type);
        ZrCore_Array_Push(compiler->state, &signature->parameterPassingModes, &mode);
    }
    return ZR_TRUE;
unsupported:
    ZrParser_Compiler_Error(compiler, "Typed callable signature cannot be represented by the runtime call contract", location);
    return ZR_FALSE;
}
