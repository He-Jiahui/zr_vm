//
// Created by Auto on 2025/01/XX.
//

#include "compile_expression_internal.h"

void emit_constant_to_slot_local(SZrCompilerState *cs, TZrUInt32 slot, const SZrTypeValue *value,
                                        SZrFileRange location) {
    if (cs == ZR_NULL || value == ZR_NULL || cs->hasError) {
        return;
    }

    if (!ZrParser_Compiler_ValidateRuntimeProjectionValue(cs, value, location)) {
        return;
    }

    SZrTypeValue constantValue = *value;
    TZrUInt32 constantIndex = add_constant(cs, &constantValue);
    TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TZrUInt16)slot, (TZrInt32)constantIndex);
    emit_instruction(cs, inst);
}

static EZrOwnershipBuiltinKind resolve_construct_expression_builtin_kind(
        const SZrConstructExpression *constructExpr) {
    if (constructExpr == ZR_NULL) {
        return ZR_OWNERSHIP_BUILTIN_KIND_NONE;
    }

    if (constructExpr->builtinKind != ZR_OWNERSHIP_BUILTIN_KIND_NONE) {
        return constructExpr->builtinKind;
    }

    if (constructExpr->isUsing) {
        return ZR_OWNERSHIP_BUILTIN_KIND_USING;
    }

    switch (constructExpr->ownershipQualifier) {
        case ZR_OWNERSHIP_QUALIFIER_UNIQUE:
            return ZR_OWNERSHIP_BUILTIN_KIND_UNIQUE;
        case ZR_OWNERSHIP_QUALIFIER_SHARED:
            return ZR_OWNERSHIP_BUILTIN_KIND_SHARED;
        case ZR_OWNERSHIP_QUALIFIER_WEAK:
            return ZR_OWNERSHIP_BUILTIN_KIND_WEAK;
        case ZR_OWNERSHIP_QUALIFIER_NONE:
        case ZR_OWNERSHIP_QUALIFIER_BORROWED:
        default:
            return ZR_OWNERSHIP_BUILTIN_KIND_NONE;
    }
}

TZrBool construct_expression_is_ownership_builtin(const SZrConstructExpression *constructExpr) {
    return constructExpr != ZR_NULL &&
           !constructExpr->isNew &&
           resolve_construct_expression_builtin_kind(constructExpr) != ZR_OWNERSHIP_BUILTIN_KIND_NONE;
}

static EZrInstructionCode resolve_ownership_builtin_opcode(const SZrConstructExpression *constructExpr) {
    EZrOwnershipBuiltinKind builtinKind;

    if (constructExpr == ZR_NULL) {
        return ZR_INSTRUCTION_ENUM(ENUM_MAX);
    }

    builtinKind = resolve_construct_expression_builtin_kind(constructExpr);
    switch (builtinKind) {
        case ZR_OWNERSHIP_BUILTIN_KIND_UNIQUE:
            return ZR_INSTRUCTION_ENUM(OWN_UNIQUE);
        case ZR_OWNERSHIP_BUILTIN_KIND_SHARED:
            return ZR_INSTRUCTION_ENUM(OWN_SHARE);
        case ZR_OWNERSHIP_BUILTIN_KIND_WEAK:
            return ZR_INSTRUCTION_ENUM(OWN_WEAK);
        case ZR_OWNERSHIP_BUILTIN_KIND_USING:
            return ZR_INSTRUCTION_ENUM(OWN_USING);
        case ZR_OWNERSHIP_BUILTIN_KIND_UPGRADE:
            return ZR_INSTRUCTION_ENUM(OWN_UPGRADE);
        case ZR_OWNERSHIP_BUILTIN_KIND_RELEASE:
            return ZR_INSTRUCTION_ENUM(OWN_RELEASE);
        case ZR_OWNERSHIP_BUILTIN_KIND_NONE:
        default:
            return ZR_INSTRUCTION_ENUM(ENUM_MAX);
    }
}

TZrBool compile_ownership_builtin_expression(SZrCompilerState *cs,
                                                    SZrConstructExpression *constructExpr,
                                                    SZrFileRange location) {
    EZrInstructionCode opcode;
    EZrOwnershipBuiltinKind builtinKind;
    TZrUInt32 resultSlot;
    TZrUInt32 argumentSlot;
    TZrBool shouldResetConsumedIdentifier = ZR_FALSE;

    if (cs == ZR_NULL || constructExpr == ZR_NULL || cs->hasError) {
        return ZR_FALSE;
    }

    opcode = resolve_ownership_builtin_opcode(constructExpr);
    builtinKind = resolve_construct_expression_builtin_kind(constructExpr);
    if (opcode == ZR_INSTRUCTION_ENUM(ENUM_MAX)) {
        ZrParser_Compiler_Error(cs, "Unsupported ownership builtin expression", location);
        return ZR_FALSE;
    }

    resultSlot = allocate_stack_slot(cs);

    if (builtinKind == ZR_OWNERSHIP_BUILTIN_KIND_RELEASE) {
        TZrUInt32 sourceSlot;

        if (constructExpr->target == ZR_NULL || constructExpr->target->type != ZR_AST_IDENTIFIER_LITERAL) {
            ZrParser_Compiler_Error(cs, "'%release' currently requires a local identifier binding", location);
            return ZR_FALSE;
        }

        sourceSlot = find_local_var(cs, constructExpr->target->data.identifier.name);
        if (sourceSlot == (TZrUInt32)-1) {
            ZrParser_Compiler_Error(cs, "'%release' currently only supports local identifier bindings", location);
            return ZR_FALSE;
        }

        emit_instruction(cs,
                         create_instruction_2(opcode,
                                              (TZrUInt16)resultSlot,
                                              (TZrUInt16)sourceSlot,
                                              0));
        collapse_stack_to_slot(cs, resultSlot);
        return ZR_TRUE;
    }

    argumentSlot = allocate_stack_slot(cs);

    shouldResetConsumedIdentifier =
            builtinKind == ZR_OWNERSHIP_BUILTIN_KIND_SHARED &&
            constructExpr->target != ZR_NULL &&
            constructExpr->target->type == ZR_AST_IDENTIFIER_LITERAL &&
            infer_expression_ownership_qualifier_local(cs, constructExpr->target) ==
                    ZR_OWNERSHIP_QUALIFIER_UNIQUE;

    if (compile_expression_into_slot(cs, constructExpr->target, argumentSlot) == (TZrUInt32)-1) {
        return ZR_FALSE;
    }

    emit_instruction(cs,
                     create_instruction_2(opcode,
                                          (TZrUInt16)resultSlot,
                                          (TZrUInt16)argumentSlot,
                                          0));
    collapse_stack_to_slot(cs, resultSlot);
    if (shouldResetConsumedIdentifier) {
        if (!emit_null_reset_to_identifier_binding_local(cs,
                                                         constructExpr->target->data.identifier.name,
                                                         location)) {
            ZrParser_Compiler_Error(cs,
                                    "Failed to reset consumed ownership source binding",
                                    location);
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool wrap_constructed_result_with_ownership_builtin(SZrCompilerState *cs,
                                                              SZrConstructExpression *constructExpr,
                                                              SZrFileRange location) {
    EZrInstructionCode opcode;
    TZrUInt32 resultSlot;
    TZrUInt32 argumentSlot;

    if (cs == ZR_NULL || constructExpr == ZR_NULL || cs->hasError || cs->stackSlotCount == 0) {
        return ZR_FALSE;
    }

    opcode = resolve_ownership_builtin_opcode(constructExpr);
    if (opcode == ZR_INSTRUCTION_ENUM(ENUM_MAX)) {
        ZrParser_Compiler_Error(cs, "Unsupported ownership construct wrapper", location);
        return ZR_FALSE;
    }

    resultSlot = (TZrUInt32)(cs->stackSlotCount - 1);
    argumentSlot = allocate_stack_slot(cs);
    emit_instruction(cs,
                     create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK),
                                          (TZrUInt16)argumentSlot,
                                          (TZrInt32)resultSlot));

    emit_instruction(cs,
                     create_instruction_2(opcode,
                                          (TZrUInt16)resultSlot,
                                          (TZrUInt16)argumentSlot,
                                          0));
    collapse_stack_to_slot(cs, resultSlot);
    return ZR_TRUE;
}

static TZrBool zr_string_equals_cstr_local(SZrString *value, const TZrChar *literal) {
    TZrNativeString nativeValue;

    if (value == ZR_NULL || literal == ZR_NULL) {
        return ZR_FALSE;
    }

    nativeValue = ZrCore_String_GetNativeString(value);
    return nativeValue != ZR_NULL && strcmp(nativeValue, literal) == 0;
}

TZrBool receiver_ownership_can_call_member_local(EZrOwnershipQualifier receiverQualifier,
                                                        EZrOwnershipQualifier memberQualifier) {
    switch (receiverQualifier) {
        case ZR_OWNERSHIP_QUALIFIER_NONE:
            return ZR_TRUE;
        case ZR_OWNERSHIP_QUALIFIER_WEAK:
            return memberQualifier == ZR_OWNERSHIP_QUALIFIER_WEAK ||
                   memberQualifier == ZR_OWNERSHIP_QUALIFIER_SHARED ||
                   memberQualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED;
        case ZR_OWNERSHIP_QUALIFIER_SHARED:
            return memberQualifier == ZR_OWNERSHIP_QUALIFIER_SHARED ||
                   memberQualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED;
        case ZR_OWNERSHIP_QUALIFIER_UNIQUE:
            return memberQualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED;
        case ZR_OWNERSHIP_QUALIFIER_BORROWED:
            return memberQualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED;
        default:
            return ZR_FALSE;
    }
}

const TZrChar *receiver_ownership_call_error_local(EZrOwnershipQualifier receiverQualifier) {
    switch (receiverQualifier) {
        case ZR_OWNERSHIP_QUALIFIER_WEAK:
            return "Weak-owned receivers can only call %weak, %shared, or %borrowed methods";
        case ZR_OWNERSHIP_QUALIFIER_SHARED:
            return "Shared-owned receivers can only call %shared or %borrowed methods";
        case ZR_OWNERSHIP_QUALIFIER_UNIQUE:
            return "Unique-owned receivers can only call %borrowed methods";
        case ZR_OWNERSHIP_QUALIFIER_BORROWED:
            return "Borrowed receivers can only call %borrowed methods";
        case ZR_OWNERSHIP_QUALIFIER_NONE:
        default:
            return "Receiver ownership qualifier is not compatible with this method";
    }
}

EZrOwnershipQualifier infer_expression_ownership_qualifier_local(SZrCompilerState *cs, SZrAstNode *node) {
    SZrInferredType inferredType;
    EZrOwnershipQualifier qualifier = ZR_OWNERSHIP_QUALIFIER_NONE;

    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return ZR_OWNERSHIP_QUALIFIER_NONE;
    }

    ZrParser_InferredType_Init(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    if (ZrParser_ExpressionType_Infer(cs, node, &inferredType)) {
        qualifier = inferredType.ownershipQualifier;
    }
    ZrParser_InferredType_Free(cs->state, &inferredType);
    return qualifier;
}

TZrBool emit_null_reset_to_identifier_binding_local(SZrCompilerState *cs,
                                                    SZrString *name,
                                                    SZrFileRange location) {
    SZrTypeValue nullValue;
    TZrUInt32 nullSlot;
    TZrUInt32 localVarIndex;
    TZrUInt32 closureVarIndex;

    if (cs == ZR_NULL || name == ZR_NULL || cs->hasError) {
        return ZR_FALSE;
    }

    nullSlot = allocate_stack_slot(cs);
    ZrCore_Value_ResetAsNull(&nullValue);
    emit_constant_to_slot_local(cs, nullSlot, &nullValue, location);

    localVarIndex = find_local_var(cs, name);
    if (localVarIndex != (TZrUInt32)-1) {
        emit_instruction(cs,
                         create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK),
                                              (TZrUInt16)localVarIndex,
                                              (TZrInt32)nullSlot));
        ZrParser_Compiler_TrimStackBy(cs, 1);
        return ZR_TRUE;
    }

    closureVarIndex = find_closure_var(cs, name);
    if (closureVarIndex != (TZrUInt32)-1) {
        SZrFunctionClosureVariable *closureVar =
                (SZrFunctionClosureVariable *)ZrCore_Array_Get(&cs->closureVars, closureVarIndex);
        if (closureVar != ZR_NULL && closureVar->inStack) {
            emit_instruction(cs,
                             create_instruction_2(ZR_INSTRUCTION_ENUM(SETUPVAL),
                                                  (TZrUInt16)nullSlot,
                                                  (TZrUInt16)closureVarIndex,
                                                  0));
        } else {
            emit_instruction(cs,
                             create_instruction_2(ZR_INSTRUCTION_ENUM(SET_CLOSURE),
                                                  (TZrUInt16)nullSlot,
                                                  (TZrUInt16)closureVarIndex,
                                                  0));
        }
        ZrParser_Compiler_TrimStackBy(cs, 1);
        return ZR_TRUE;
    }

    ZrParser_Compiler_TrimStackBy(cs, 1);
    return ZR_FALSE;
}

static TZrBool has_compile_time_variable_binding_local(SZrCompilerState *cs, SZrString *name) {
    if (cs == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < cs->compileTimeVariables.length; index++) {
        SZrCompileTimeVariable **varPtr =
                (SZrCompileTimeVariable **)ZrCore_Array_Get(&cs->compileTimeVariables, index);
        if (varPtr != ZR_NULL && *varPtr != ZR_NULL && (*varPtr)->name != ZR_NULL &&
            ZrCore_String_Equal((*varPtr)->name, name)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool has_compile_time_function_binding_local(SZrCompilerState *cs, SZrString *name) {
    if (cs == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < cs->compileTimeFunctions.length; index++) {
        SZrCompileTimeFunction **funcPtr =
                (SZrCompileTimeFunction **)ZrCore_Array_Get(&cs->compileTimeFunctions, index);
        if (funcPtr != ZR_NULL && *funcPtr != ZR_NULL && (*funcPtr)->name != ZR_NULL &&
            ZrCore_String_Equal((*funcPtr)->name, name)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool is_compile_time_projection_candidate(SZrCompilerState *cs, SZrString *rootName) {
    if (cs == ZR_NULL || rootName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (zr_string_equals_cstr_local(rootName, "Assert") ||
        zr_string_equals_cstr_local(rootName, "FatalError")) {
        return ZR_TRUE;
    }

    return has_compile_time_variable_binding_local(cs, rootName) ||
           has_compile_time_function_binding_local(cs, rootName);
}

static TZrBool primary_root_is_compile_time_projection_candidate(SZrCompilerState *cs, SZrAstNode *rootNode) {
    if (rootNode == ZR_NULL) {
        return ZR_FALSE;
    }

    if (rootNode->type == ZR_AST_IMPORT_EXPRESSION) {
        return ZR_TRUE;
    }

    if (rootNode->type != ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_FALSE;
    }

    return is_compile_time_projection_candidate(cs, rootNode->data.identifier.name);
}

TZrBool try_emit_compile_time_function_call(SZrCompilerState *cs, SZrAstNode *node) {
    SZrPrimaryExpression *primary;
    SZrTypeValue compileTimeValue;
    TZrUInt32 destSlot;

    if (cs == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_FALSE;
    }

    primary = &node->data.primaryExpression;
    if (primary->property == ZR_NULL || primary->members == ZR_NULL || primary->members->count == 0) {
        return ZR_FALSE;
    }

    if (primary->property->type == ZR_AST_IMPORT_EXPRESSION) {
        SZrAstNode *tailMember = primary->members->nodes[primary->members->count - 1];
        if (primary->members->count < 2 || tailMember == ZR_NULL ||
            tailMember->type != ZR_AST_FUNCTION_CALL) {
            return ZR_FALSE;
        }
    } else if (primary->property->type != ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_FALSE;
    }

    if (!primary_root_is_compile_time_projection_candidate(cs, primary->property)) {
        return ZR_FALSE;
    }

    if (!ZrParser_Compiler_EvaluateCompileTimeExpression(cs, node, &compileTimeValue)) {
        return cs->hasError || cs->hasCompileTimeError || cs->hasFatalError;
    }

    destSlot = allocate_stack_slot(cs);
    emit_constant_to_slot_local(cs, destSlot, &compileTimeValue, node->location);
    return ZR_TRUE;
}

// 类型转换辅助函数
void emit_type_conversion(SZrCompilerState *cs, TZrUInt32 destSlot, TZrUInt32 srcSlot, EZrInstructionCode conversionOpcode) {
    if (cs == ZR_NULL || cs->hasError) {
        return;
    }
    // 类型转换指令格式: operandExtra = destSlot, operand1[0] = srcSlot, operand1[1] = 0
    TZrInstruction convInst = create_instruction_2(conversionOpcode, (TZrUInt16)destSlot, (TZrUInt16)srcSlot, 0);
    emit_instruction(cs, convInst);
}

// 带原型信息的类型转换辅助函数（用于 TO_STRUCT 和 TO_OBJECT）
void emit_type_conversion_with_prototype(SZrCompilerState *cs, TZrUInt32 destSlot, TZrUInt32 srcSlot, 
                                                EZrInstructionCode conversionOpcode, TZrUInt32 prototypeConstantIndex) {
    if (cs == ZR_NULL || cs->hasError) {
        return;
    }
    // 类型转换指令格式: operandExtra = destSlot, operand1[0] = srcSlot, operand1[1] = prototypeConstantIndex
    TZrInstruction convInst = create_instruction_2(conversionOpcode, (TZrUInt16)destSlot, (TZrUInt16)srcSlot, (TZrUInt16)prototypeConstantIndex);
    emit_instruction(cs, convInst);
}

EZrValueType binary_expression_effective_type_after_conversion(EZrValueType originalType,
                                                                      EZrInstructionCode conversionOpcode) {
    switch (conversionOpcode) {
        case ZR_INSTRUCTION_ENUM(TO_BOOL):
            return ZR_VALUE_TYPE_BOOL;
        case ZR_INSTRUCTION_ENUM(TO_INT):
            return ZR_VALUE_TYPE_INT64;
        case ZR_INSTRUCTION_ENUM(TO_UINT):
            return ZR_VALUE_TYPE_UINT64;
        case ZR_INSTRUCTION_ENUM(TO_FLOAT):
            return ZR_VALUE_TYPE_DOUBLE;
        case ZR_INSTRUCTION_ENUM(TO_STRING):
            return ZR_VALUE_TYPE_STRING;
        default:
            return originalType;
    }
}

TZrBool binary_expression_type_is_float_like(EZrValueType type) {
    return (TZrBool)(type == ZR_VALUE_TYPE_FLOAT || type == ZR_VALUE_TYPE_DOUBLE);
}

void update_identifier_assignment_type_environment(SZrCompilerState *cs,
                                                          SZrString *name,
                                                          SZrAstNode *valueExpression) {
    SZrInferredType inferredType;

    if (cs == ZR_NULL || name == ZR_NULL || valueExpression == ZR_NULL || cs->typeEnv == ZR_NULL) {
        return;
    }

    ZrParser_InferredType_Init(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    if (ZrParser_ExpressionType_Infer(cs, valueExpression, &inferredType)) {
        ZrParser_TypeEnvironment_RegisterVariable(cs->state, cs->typeEnv, name, &inferredType);
    }
    ZrParser_InferredType_Free(cs->state, &inferredType);
}

// 在脚本 AST 中查找类型定义（struct 或 class）
// 返回找到的 AST 节点，如果未找到返回 ZR_NULL

SZrString *create_hidden_property_accessor_name(SZrCompilerState *cs, SZrString *propertyName,
                                                TZrBool isSetter) {
    if (cs == ZR_NULL || propertyName == ZR_NULL) {
        return ZR_NULL;
    }

    const TZrChar *prefix = isSetter ? "__set_" : "__get_";
    TZrNativeString propertyNameString = ZrCore_String_GetNativeString(propertyName);
    if (propertyNameString == ZR_NULL) {
        return ZR_NULL;
    }

    TZrSize prefixLength = strlen(prefix);
    TZrSize propertyNameLength = propertyName->shortStringLength < ZR_VM_LONG_STRING_FLAG
                                         ? propertyName->shortStringLength
                                         : propertyName->longStringLength;
    TZrSize bufferSize = prefixLength + propertyNameLength + 1;
    TZrChar *buffer = (TZrChar *)ZrCore_Memory_RawMalloc(cs->state->global, bufferSize);
    if (buffer == ZR_NULL) {
        return ZR_NULL;
    }

    memcpy(buffer, prefix, prefixLength);
    memcpy(buffer + prefixLength, propertyNameString, propertyNameLength);
    buffer[bufferSize - 1] = '\0';

    SZrString *result = ZrCore_String_CreateFromNative(cs->state, buffer);
    ZrCore_Memory_RawFree(cs->state->global, buffer, bufferSize);
    return result;
}

// 辅助函数：查找类型成员信息（检查字段是否是 const）

void collapse_stack_to_slot(SZrCompilerState *cs, TZrUInt32 slot) {
    if (cs == ZR_NULL) {
        return;
    }

    ZrParser_Compiler_TrimStackToSlot(cs, slot);
}

TZrUInt32 normalize_top_result_to_slot(SZrCompilerState *cs, TZrUInt32 targetSlot) {
    if (cs == ZR_NULL || cs->hasError || cs->stackSlotCount == 0) {
        return (TZrUInt32)-1;
    }

    TZrUInt32 resultSlot = (TZrUInt32)(cs->stackSlotCount - 1);
    if (resultSlot != targetSlot) {
        TZrInstruction copyInst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TZrUInt16)targetSlot, (TZrInt32)resultSlot);
        emit_instruction(cs, copyInst);
    }

    collapse_stack_to_slot(cs, targetSlot);
    return targetSlot;
}

void compile_expression_non_tail(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }

    TZrBool oldTailCallContext = cs->isInTailCallContext;
    cs->isInTailCallContext = ZR_FALSE;
    ZrParser_Expression_Compile(cs, node);
    cs->isInTailCallContext = oldTailCallContext;
}

TZrUInt32 emit_string_constant(SZrCompilerState *cs, SZrString *value) {
    if (cs == ZR_NULL || value == ZR_NULL || cs->hasError) {
        return (TZrUInt32)-1;
    }

    SZrTypeValue constantValue;
    ZrCore_Value_InitAsRawObject(cs->state, &constantValue, ZR_CAST_RAW_OBJECT_AS_SUPER(value));
    constantValue.type = ZR_VALUE_TYPE_STRING;

    TZrUInt32 constantIndex = add_constant(cs, &constantValue);
    TZrUInt32 slot = allocate_stack_slot(cs);
    TZrInstruction inst =
            create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), ZR_COMPILE_SLOT_U16(slot), (TZrInt32)constantIndex);
    emit_instruction(cs, inst);
    return slot;
}

SZrString *resolve_member_expression_symbol(SZrCompilerState *cs, SZrMemberExpression *memberExpr) {
    SZrInferredType inferredType;

    if (cs == ZR_NULL || memberExpr == ZR_NULL || memberExpr->property == ZR_NULL || memberExpr->computed) {
        return ZR_NULL;
    }

    if (memberExpr->property->type == ZR_AST_IDENTIFIER_LITERAL) {
        return memberExpr->property->data.identifier.name;
    }

    if (memberExpr->property->type != ZR_AST_TYPE) {
        return ZR_NULL;
    }

    ZrParser_InferredType_Init(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    if (!ZrParser_AstTypeToInferredType_Convert(cs, &memberExpr->property->data.type, &inferredType)) {
        ZrParser_InferredType_Free(cs->state, &inferredType);
        return ZR_NULL;
    }

    {
        SZrString *resolvedName = inferredType.typeName;
        ZrParser_InferredType_Free(cs->state, &inferredType);
        return resolvedName;
    }
}

TZrUInt32 compile_expression_into_slot(SZrCompilerState *cs, SZrAstNode *node, TZrUInt32 targetSlot) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return (TZrUInt32)-1;
    }

    compile_expression_non_tail(cs, node);
    if (cs->hasError) {
        return (TZrUInt32)-1;
    }

    return normalize_top_result_to_slot(cs, targetSlot);
}

TZrBool emit_property_getter_call(SZrCompilerState *cs, TZrUInt32 currentSlot, SZrString *propertyName,
                                       TZrBool isStatic, SZrFileRange location) {
    TZrUInt32 memberId;
    TZrInstruction metaGetInst;
    TZrUInt8 memberFlags = isStatic ? ZR_FUNCTION_MEMBER_ENTRY_FLAG_STATIC_ACCESSOR : 0;

    if (cs == ZR_NULL || propertyName == ZR_NULL || cs->hasError) {
        return ZR_FALSE;
    }

    SZrString *accessorName = create_hidden_property_accessor_name(cs, propertyName, ZR_FALSE);
    if (accessorName == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Failed to create property getter accessor name", location);
        return ZR_FALSE;
    }

    memberId = compiler_get_or_add_member_entry_with_flags(cs, accessorName, memberFlags);
    if (memberId == (TZrUInt32)-1) {
        ZrParser_Compiler_Error(cs, "Failed to register property getter member symbol", location);
        return ZR_FALSE;
    }

    metaGetInst = create_instruction_2(ZR_INSTRUCTION_ENUM(META_GET),
                                       (TZrUInt16)currentSlot,
                                       (TZrUInt16)currentSlot,
                                       (TZrUInt16)memberId);
    emit_instruction(cs, metaGetInst);
    collapse_stack_to_slot(cs, currentSlot);
    return ZR_TRUE;
}

TZrUInt32 emit_property_setter_call(SZrCompilerState *cs, TZrUInt32 objectSlot, SZrString *propertyName, TZrBool isStatic,
                                         TZrUInt32 assignedValueSlot, SZrFileRange location) {
    TZrUInt32 memberId;
    TZrInstruction metaSetInst;
    TZrUInt8 memberFlags = isStatic ? ZR_FUNCTION_MEMBER_ENTRY_FLAG_STATIC_ACCESSOR : 0;

    if (cs == ZR_NULL || propertyName == ZR_NULL || cs->hasError) {
        return (TZrUInt32)-1;
    }

    SZrString *accessorName = create_hidden_property_accessor_name(cs, propertyName, ZR_TRUE);
    if (accessorName == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Failed to create property setter accessor name", location);
        return (TZrUInt32)-1;
    }

    memberId = compiler_get_or_add_member_entry_with_flags(cs, accessorName, memberFlags);
    if (memberId == (TZrUInt32)-1) {
        ZrParser_Compiler_Error(cs, "Failed to register property setter member symbol", location);
        return (TZrUInt32)-1;
    }

    metaSetInst = create_instruction_2(ZR_INSTRUCTION_ENUM(META_SET),
                                       (TZrUInt16)objectSlot,
                                       (TZrUInt16)assignedValueSlot,
                                       (TZrUInt16)memberId);
    emit_instruction(cs, metaSetInst);
    collapse_stack_to_slot(cs, objectSlot);
    return objectSlot;
}

TZrUInt32 compile_member_key_into_slot(SZrCompilerState *cs, SZrMemberExpression *memberExpr, TZrUInt32 targetSlot) {
    if (cs == ZR_NULL || memberExpr == ZR_NULL || memberExpr->property == ZR_NULL || cs->hasError) {
        return (TZrUInt32)-1;
    }

    if (!memberExpr->computed) {
        SZrString *fieldName = resolve_member_expression_symbol(cs, memberExpr);
        if (fieldName == ZR_NULL || emit_string_constant(cs, fieldName) == (TZrUInt32)-1) {
            return (TZrUInt32)-1;
        }
        return normalize_top_result_to_slot(cs, targetSlot);
    }

    return compile_expression_into_slot(cs, memberExpr->property, targetSlot);
}

TZrBool expression_uses_dynamic_object_access(SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_PRIMARY_EXPRESSION: {
            SZrPrimaryExpression *primary = &node->data.primaryExpression;
            if (primary->members != ZR_NULL && primary->members->count > 0) {
                return ZR_TRUE;
            }
            return expression_uses_dynamic_object_access(primary->property);
        }
        case ZR_AST_MEMBER_EXPRESSION:
        case ZR_AST_FUNCTION_CALL:
            return ZR_TRUE;
        case ZR_AST_IMPORT_EXPRESSION:
            return ZR_FALSE;
        case ZR_AST_TYPE_QUERY_EXPRESSION:
            return expression_uses_dynamic_object_access(node->data.typeQueryExpression.operand);
        case ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION:
            return expression_uses_dynamic_object_access(node->data.prototypeReferenceExpression.target);
        case ZR_AST_CONSTRUCT_EXPRESSION:
            return ZR_TRUE;
        case ZR_AST_BINARY_EXPRESSION:
            return expression_uses_dynamic_object_access(node->data.binaryExpression.left) ||
                   expression_uses_dynamic_object_access(node->data.binaryExpression.right);
        case ZR_AST_UNARY_EXPRESSION:
            return expression_uses_dynamic_object_access(node->data.unaryExpression.argument);
        case ZR_AST_CONDITIONAL_EXPRESSION:
            return expression_uses_dynamic_object_access(node->data.conditionalExpression.test) ||
                   expression_uses_dynamic_object_access(node->data.conditionalExpression.consequent) ||
                   expression_uses_dynamic_object_access(node->data.conditionalExpression.alternate);
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            return expression_uses_dynamic_object_access(node->data.assignmentExpression.left) ||
                   expression_uses_dynamic_object_access(node->data.assignmentExpression.right);
        default:
            return ZR_FALSE;
    }
}


