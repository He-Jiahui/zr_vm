#include "compile_expression_internal.h"

#include "zr_vm_parser/bound_expression.h"

static EZrCanonicalCallSiteMarker value_construct_call_site_marker(
        const SZrStructInitExpression *expression,
        TZrSize index) {
    const SZrCallArgumentSyntax *syntax;

    if (expression == ZR_NULL || expression->argumentMarkers == ZR_NULL ||
        index >= expression->argumentMarkers->length) {
        return ZR_CANONICAL_CALL_SITE_NONE;
    }
    syntax = (const SZrCallArgumentSyntax *)ZrCore_Array_Get(
            expression->argumentMarkers, index);
    if (syntax == ZR_NULL) {
        return ZR_CANONICAL_CALL_SITE_NONE;
    }
    switch (syntax->marker) {
        case ZR_CALL_ARGUMENT_MARKER_REF:
            return ZR_CANONICAL_CALL_SITE_REF;
        case ZR_CALL_ARGUMENT_MARKER_OUT:
            return ZR_CANONICAL_CALL_SITE_OUT;
        case ZR_CALL_ARGUMENT_MARKER_NONE:
        default:
            return ZR_CANONICAL_CALL_SITE_NONE;
    }
}

static SZrString *value_construct_argument_name(
        const SZrStructInitExpression *expression,
        TZrSize index) {
    SZrString **name;

    if (expression == ZR_NULL || expression->argNames == ZR_NULL ||
        index >= expression->argNames->length) {
        return ZR_NULL;
    }
    name = (SZrString **)ZrCore_Array_Get(expression->argNames, index);
    return name != ZR_NULL ? *name : ZR_NULL;
}

static const TZrChar *value_construct_resolution_message(
        EZrValueConstructorResolution resolution) {
    switch (resolution) {
        case ZR_VALUE_CONSTRUCTOR_NOT_CONSTRUCTIBLE:
            return "init target is not a value-constructible struct type";
        case ZR_VALUE_CONSTRUCTOR_NO_MATCH:
            return "No struct constructor matches init arguments";
        case ZR_VALUE_CONSTRUCTOR_INACCESSIBLE:
            return "Matching struct constructor is not accessible";
        case ZR_VALUE_CONSTRUCTOR_AMBIGUOUS:
            return "Struct init constructor is ambiguous";
        case ZR_VALUE_CONSTRUCTOR_INVALID_ARGUMENTS:
        case ZR_VALUE_CONSTRUCTOR_RESOLVED:
        default:
            return "Invalid struct init arguments";
    }
}

static const SZrTypeMemberInfo *value_construct_find_constructor(
        SZrCompilerState *cs,
        SZrString *typeName,
        TZrSymbolId constructorId) {
    SZrTypePrototypeInfo *prototype = find_compiler_type_prototype(cs, typeName);

    if (prototype == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrSize index = 0U; index < prototype->members.length; index++) {
        const SZrTypeMemberInfo *member =
                (const SZrTypeMemberInfo *)ZrCore_Array_Get(
                        &prototype->members, index);
        if (member != ZR_NULL && member->isMetaMethod &&
            member->metaType == ZR_META_CONSTRUCTOR &&
            member->symbolId == constructorId) {
            return member;
        }
    }
    return ZR_NULL;
}

static TZrBool value_construct_ensure_slot(
        SZrCompilerState *cs,
        TZrUInt32 stackSlot) {
    while (cs != ZR_NULL && cs->stackSlotCount <= stackSlot) {
        if (allocate_stack_slot(cs) == ZR_PARSER_SLOT_NONE) {
            return ZR_FALSE;
        }
    }
    return cs != ZR_NULL;
}

static TZrBool value_construct_parameter_has_default(
        const SZrTypeMemberInfo *member,
        TZrSize index) {
    const TZrBool *hasDefault;

    if (member == ZR_NULL || index >= member->parameterHasDefaultValues.length) {
        return ZR_FALSE;
    }
    hasDefault = (const TZrBool *)ZrCore_Array_Get(
            (SZrArray *)&member->parameterHasDefaultValues, index);
    return hasDefault != ZR_NULL ? *hasDefault : ZR_FALSE;
}

static TZrBool value_construct_emit_default_argument(
        SZrCompilerState *cs,
        const SZrTypeMemberInfo *member,
        TZrSize parameterIndex,
        TZrUInt32 targetSlot) {
    const SZrTypeValue *value;
    SZrTypeValue constantValue;
    TZrUInt32 constantIndex;

    if (cs == ZR_NULL || member == ZR_NULL ||
        parameterIndex >= member->parameterDefaultValues.length ||
        !value_construct_parameter_has_default(member, parameterIndex) ||
        !value_construct_ensure_slot(cs, targetSlot)) {
        return ZR_FALSE;
    }
    value = (const SZrTypeValue *)ZrCore_Array_Get(
            (SZrArray *)&member->parameterDefaultValues, parameterIndex);
    if (value == ZR_NULL) {
        return ZR_FALSE;
    }
    constantValue = *value;
    constantIndex = add_constant(cs, &constantValue);
    emit_instruction(
            cs,
            create_instruction_1(
                    ZR_INSTRUCTION_ENUM(GET_CONSTANT),
                    (TZrUInt16)targetSlot,
                    (TZrInt32)constantIndex));
    return !cs->hasError;
}

static TZrBool value_construct_emit_explicit_constructor(
        SZrCompilerState *cs,
        SZrAstNode *node,
        const SZrStructInitExpression *expression,
        SZrString *typeName,
        TZrTypeId typeId,
        TZrUInt32 targetSlot,
        TZrPlaceId destinationPlaceId,
        const SZrBoundValueConstruct *bound) {
    const SZrTypeMemberInfo *member;
    SZrTypePrototypeInfo *prototype;
    SZrTypeValue typeNameValue;
    TZrUInt32 functionSlot;
    TZrUInt32 typeNameConstantIndex;
    TZrUInt32 constructorMemberId;
    TZrUInt32 *argumentSlots = ZR_NULL;
    TZrUInt32 *sourceArgumentSlots = ZR_NULL;
    TZrBool *provided = ZR_NULL;
    TZrSize parameterCount;
    TZrBool success = ZR_FALSE;

    if (cs == ZR_NULL || node == ZR_NULL || expression == ZR_NULL ||
        typeName == ZR_NULL || bound == ZR_NULL || targetSlot == 0U) {
        return ZR_FALSE;
    }
    member = value_construct_find_constructor(cs, typeName, bound->constructorId);
    if (member == ZR_NULL) {
        ZrParser_Compiler_Error(
                cs, "Resolved struct constructor declaration is unavailable", node->location);
        return ZR_FALSE;
    }
    prototype = find_compiler_type_prototype(cs, typeName);
    if (member->compiledFunction == ZR_NULL) {
        if (prototype == ZR_NULL || !prototype->isNativeRuntime ||
            prototype->type != ZR_OBJECT_PROTOTYPE_TYPE_CLASS) {
            ZrParser_Compiler_Error(
                    cs, "Resolved native constructor target is unavailable", node->location);
            return ZR_FALSE;
        }
        ZrCore_Value_InitAsRawObject(
                cs->state, &typeNameValue, ZR_CAST_RAW_OBJECT_AS_SUPER(typeName));
        typeNameValue.type = ZR_VALUE_TYPE_STRING;
        typeNameConstantIndex = add_constant(cs, &typeNameValue);
        emit_instruction(
                cs,
                create_instruction_0(ZR_INSTRUCTION_ENUM(CREATE_OBJECT), (TZrUInt16)targetSlot));
        emit_instruction(
                cs,
                create_instruction_2(
                        ZR_INSTRUCTION_ENUM(TO_OBJECT),
                        (TZrUInt16)targetSlot,
                        (TZrUInt16)targetSlot,
                        (TZrUInt16)typeNameConstantIndex));
    }
    functionSlot = targetSlot - 1U;
    parameterCount = member->parameterCount;
    if (parameterCount > 0U) {
        argumentSlots = (TZrUInt32 *)ZrCore_Memory_RawMallocWithType(
                cs->state->global,
                sizeof(TZrUInt32) * parameterCount,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
        provided = (TZrBool *)ZrCore_Memory_RawMallocWithType(
                cs->state->global,
                sizeof(TZrBool) * parameterCount,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
        if (argumentSlots == ZR_NULL || provided == ZR_NULL) {
            goto cleanup;
        }
        memset(argumentSlots, 0, sizeof(TZrUInt32) * parameterCount);
        memset(provided, 0, sizeof(TZrBool) * parameterCount);
    }
    if (bound->arguments.length > 0U) {
        sourceArgumentSlots = (TZrUInt32 *)ZrCore_Memory_RawMallocWithType(
                cs->state->global,
                sizeof(TZrUInt32) * bound->arguments.length,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
        if (sourceArgumentSlots == ZR_NULL) {
            goto cleanup;
        }
    }

    for (TZrSize index = 0U; index < bound->arguments.length; index++) {
        const SZrBoundValueConstructArgument *argument =
                (const SZrBoundValueConstructArgument *)ZrCore_Array_Get(
                        (SZrArray *)&bound->arguments, index);
        SZrAstNode *argumentNode;
        TZrUInt32 sourceSlot;

        if (argument == ZR_NULL || argument->parameterIndex >= parameterCount ||
            expression->args == ZR_NULL ||
            argument->sourceIndex >= expression->args->count) {
            goto cleanup;
        }
        argumentNode = expression->args->nodes[argument->sourceIndex];
        sourceSlot = targetSlot + 1U + (TZrUInt32)parameterCount + argument->sourceIndex;
        if (argumentNode == ZR_NULL || !value_construct_ensure_slot(cs, sourceSlot) ||
            compile_expression_into_slot(cs, argumentNode, sourceSlot) == ZR_PARSER_SLOT_NONE) {
            goto cleanup;
        }
        sourceArgumentSlots[index] = sourceSlot;
    }
    for (TZrSize index = 0U; index < bound->arguments.length; index++) {
        const SZrBoundValueConstructArgument *argument =
                (const SZrBoundValueConstructArgument *)ZrCore_Array_Get(
                        (SZrArray *)&bound->arguments, index);
        const SZrInferredType *expectedType;
        TZrUInt32 argumentSlot;

        if (argument == ZR_NULL || argument->parameterIndex >= parameterCount) {
            goto cleanup;
        }
        argumentSlot = targetSlot + 1U + argument->parameterIndex;
        emit_instruction(
                cs,
                create_instruction_1(
                        ZR_INSTRUCTION_ENUM(SET_STACK),
                        (TZrUInt16)argumentSlot,
                        (TZrInt32)sourceArgumentSlots[index]));
        expectedType = argument->parameterIndex < member->parameterTypes.length
                               ? (const SZrInferredType *)ZrCore_Array_Get(
                                         (SZrArray *)&member->parameterTypes,
                                         argument->parameterIndex)
                               : ZR_NULL;
        if (expectedType != ZR_NULL &&
            !compiler_register_stack_slot_type_hint(cs, argumentSlot, expectedType)) {
            goto cleanup;
        }
        argumentSlots[argument->parameterIndex] = argumentSlot;
        provided[argument->parameterIndex] = ZR_TRUE;
    }
    for (TZrSize index = 0U; index < parameterCount; index++) {
        TZrUInt32 argumentSlot = targetSlot + 1U + (TZrUInt32)index;
        const SZrInferredType *expectedType =
                index < member->parameterTypes.length
                        ? (const SZrInferredType *)ZrCore_Array_Get(
                                  (SZrArray *)&member->parameterTypes, index)
                        : ZR_NULL;
        if (!provided[index]) {
            if (!value_construct_emit_default_argument(
                        cs, member, index, argumentSlot)) {
                ZrParser_Compiler_Error(
                        cs, "Missing struct constructor argument", node->location);
                goto cleanup;
            }
            argumentSlots[index] = argumentSlot;
        }
        if (expectedType != ZR_NULL &&
            !compiler_register_stack_slot_type_hint(cs, argumentSlot, expectedType)) {
            goto cleanup;
        }
    }

    if (member->compiledFunction != ZR_NULL) {
        if (!emit_member_function_constant_to_slot(
                    cs, functionSlot, member, node->location)) {
            goto cleanup;
        }
    } else {
        constructorMemberId = compiler_get_or_add_member_entry_for_type_member(
                cs, member->name, member, 0U);
        if (constructorMemberId == ZR_PARSER_MEMBER_ID_NONE) {
            ZrParser_Compiler_Error(
                    cs, "Failed to bind resolved native constructor", node->location);
            goto cleanup;
        }
        emit_instruction(
                cs,
                create_instruction_2(
                        ZR_INSTRUCTION_ENUM(GET_MEMBER),
                        (TZrUInt16)functionSlot,
                        (TZrUInt16)targetSlot,
                        (TZrUInt16)constructorMemberId));
    }
    emit_instruction(
            cs,
            create_instruction_1(
                    ZR_INSTRUCTION_ENUM(SET_STACK),
                    (TZrUInt16)targetSlot,
                    (TZrInt32)targetSlot));
    if (!compiler_semantic_ir_lower_value_construct_to_place(
                cs,
                targetSlot,
                destinationPlaceId,
                typeId,
                bound->constructorId,
                argumentSlots,
                parameterCount,
                node->location)) {
        ZrParser_Compiler_Error(
                cs, "Failed to lower struct init Semantic IR", node->location);
        goto cleanup;
    }
    emit_instruction(
            cs,
            create_instruction_2(
                    ZR_INSTRUCTION_ENUM(FUNCTION_CALL),
                    (TZrUInt16)functionSlot,
                    (TZrUInt16)functionSlot,
                    (TZrUInt16)(parameterCount + 1U)));
    collapse_stack_to_slot(cs, targetSlot);
    cs->lastExpressionSlot = targetSlot;
    success = !cs->hasError;

cleanup:
    if (argumentSlots != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                cs->state->global,
                argumentSlots,
                sizeof(TZrUInt32) * parameterCount,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (provided != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                cs->state->global,
                provided,
                sizeof(TZrBool) * parameterCount,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (sourceArgumentSlots != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                cs->state->global,
                sourceArgumentSlots,
                sizeof(TZrUInt32) * bound->arguments.length,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    return success;
}

TZrUInt32 compile_struct_init_expression_into_slot_and_place(
        SZrCompilerState *cs,
        SZrAstNode *node,
        TZrUInt32 targetSlot,
        TZrPlaceId destinationPlaceId) {
    SZrStructInitExpression *expression;
    SZrInferredType resultType;
    SZrString *typeName;
    TZrTypeId typeId;
    SZrBoundValueConstructArgumentInput *inputs = ZR_NULL;
    SZrBoundValueConstruct bound;
    EZrValueConstructorResolution resolution;
    TZrSize argumentCount = 0U;
    TZrBool resultTypeInitialized = ZR_FALSE;
    TZrUInt32 result = ZR_PARSER_SLOT_NONE;

    if (cs == ZR_NULL || node == ZR_NULL || targetSlot == ZR_PARSER_SLOT_NONE ||
        node->type != ZR_AST_STRUCT_INIT_EXPRESSION || cs->hasError) {
        return ZR_PARSER_SLOT_NONE;
    }
    expression = &node->data.structInitExpression;
    if (expression->typeInfo == ZR_NULL || cs->semanticContext == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Struct init requires a static type", node->location);
        return ZR_PARSER_SLOT_NONE;
    }

    ZrParser_InferredType_Init(cs->state, &resultType, ZR_VALUE_TYPE_OBJECT);
    resultTypeInitialized = ZR_TRUE;
    if (!ZrParser_AstTypeToInferredType_Convert(
                cs, expression->typeInfo, &resultType)) {
        goto cleanup;
    }
    typeName = get_type_name_from_inferred_type(cs, &resultType);
    typeId = ZrParser_CanonicalType_FromInferred(
            cs->semanticContext, &resultType);
    if (typeName == ZR_NULL || typeId == ZR_SEMANTIC_ID_INVALID ||
        !note_inline_struct_result_slot(cs, targetSlot, typeName)) {
        ZrParser_Compiler_Error(cs, "Failed to resolve struct init type", node->location);
        goto cleanup;
    }

    argumentCount = expression->args != ZR_NULL ? expression->args->count : 0U;
    if (argumentCount > 0U) {
        inputs = (SZrBoundValueConstructArgumentInput *)ZrCore_Memory_RawMallocWithType(
                cs->state->global,
                sizeof(SZrBoundValueConstructArgumentInput) * argumentCount,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
        if (inputs == ZR_NULL) {
            ZrParser_Compiler_Error(cs, "Failed to allocate struct init binding", node->location);
            goto cleanup;
        }
        memset(inputs, 0, sizeof(SZrBoundValueConstructArgumentInput) * argumentCount);
        for (TZrSize index = 0U; index < argumentCount; index++) {
            SZrInferredType argumentType;
            SZrAstNode *argumentNode = expression->args->nodes[index];

            if (argumentNode == ZR_NULL) {
                ZrParser_Compiler_Error(cs, "Struct init argument is null", node->location);
                goto cleanup;
            }
            ZrParser_InferredType_Init(cs->state, &argumentType, ZR_VALUE_TYPE_OBJECT);
            if (!ZrParser_ExpressionType_Infer(cs, argumentNode, &argumentType)) {
                ZrParser_InferredType_Free(cs->state, &argumentType);
                goto cleanup;
            }
            inputs[index].typeId = ZrParser_CanonicalType_FromInferred(
                    cs->semanticContext, &argumentType);
            ZrParser_InferredType_Free(cs->state, &argumentType);
            if (inputs[index].typeId == ZR_SEMANTIC_ID_INVALID) {
                ZrParser_Compiler_Error(
                        cs, "Failed to resolve struct init argument type", argumentNode->location);
                goto cleanup;
            }
            inputs[index].name = value_construct_argument_name(expression, index);
            inputs[index].callSiteMarker = value_construct_call_site_marker(expression, index);
            inputs[index].sourceRange = argumentNode->location;
        }
    }

    ZrParser_BoundValueConstruct_Init(cs->state, &bound);
    resolution = ZrParser_BoundValueConstruct_Bind(
            cs->semanticContext,
            typeId,
            inputs,
            argumentCount,
            node->location,
            &bound);
    if (resolution != ZR_VALUE_CONSTRUCTOR_RESOLVED) {
        ZrParser_Compiler_Error(
                cs, value_construct_resolution_message(resolution), node->location);
        ZrParser_BoundValueConstruct_Free(cs->state, &bound);
        goto cleanup;
    }
    if (bound.constructorId != ZR_CANONICAL_SYNTHESIZED_DEFAULT_CONSTRUCTOR_ID) {
        if (!value_construct_emit_explicit_constructor(
                    cs,
                    node,
                    expression,
                    typeName,
                    typeId,
                    targetSlot,
                    destinationPlaceId,
                    &bound)) {
            if (!cs->hasError) {
                ZrParser_Compiler_Error(
                        cs, "Failed to invoke struct init constructor", node->location);
            }
            ZrParser_BoundValueConstruct_Free(cs->state, &bound);
            goto cleanup;
        }
        ZrParser_BoundValueConstruct_Free(cs->state, &bound);
        result = targetSlot;
        goto cleanup;
    }
    if (!compiler_semantic_ir_lower_value_construct_to_place(
                cs,
                targetSlot,
                destinationPlaceId,
                typeId,
                bound.constructorId,
                ZR_NULL,
                0U,
                node->location)) {
        ZrParser_Compiler_Error(
                cs, "Failed to lower struct init Semantic IR", node->location);
        ZrParser_BoundValueConstruct_Free(cs->state, &bound);
        goto cleanup;
    }
    ZrParser_BoundValueConstruct_Free(cs->state, &bound);

    emit_instruction(
            cs,
            create_instruction_1(
                    ZR_INSTRUCTION_ENUM(SET_STACK),
                    (TZrUInt16)targetSlot,
                    (TZrInt32)targetSlot));
    collapse_stack_to_slot(cs, targetSlot);
    cs->lastExpressionSlot = targetSlot;
    result = targetSlot;

cleanup:
    if (inputs != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                cs->state->global,
                inputs,
                sizeof(SZrBoundValueConstructArgumentInput) * argumentCount,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (resultTypeInitialized) {
        ZrParser_InferredType_Free(cs->state, &resultType);
    }
    return result;
}

TZrUInt32 compile_struct_init_expression_into_slot(
        SZrCompilerState *cs,
        SZrAstNode *node,
        TZrUInt32 targetSlot) {
    return compile_struct_init_expression_into_slot_and_place(
            cs, node, targetSlot, ZR_PLACE_ID_INVALID);
}

void compile_struct_init_expression(SZrCompilerState *cs, SZrAstNode *node) {
    TZrUInt32 functionSlot;
    TZrUInt32 targetSlot;

    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    functionSlot = allocate_stack_slot(cs);
    targetSlot = allocate_stack_slot(cs);
    if (functionSlot == ZR_PARSER_SLOT_NONE || targetSlot == ZR_PARSER_SLOT_NONE ||
        targetSlot != functionSlot + 1U) {
        return;
    }
    (void)compile_struct_init_expression_into_slot(cs, node, targetSlot);
}
