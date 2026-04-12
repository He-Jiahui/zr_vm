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

static TZrBool compiler_append_callsite_cache_entry(SZrCompilerState *cs,
                                                    EZrFunctionCallSiteCacheKind kind,
                                                    TZrUInt32 instructionIndex,
                                                    TZrUInt32 memberEntryIndex,
                                                    TZrUInt16 *outCacheIndex,
                                                    SZrFileRange location) {
    SZrFunctionCallSiteCacheEntry *newEntries;
    TZrSize oldCount;
    TZrSize newCount;
    TZrSize copyBytes;

    if (outCacheIndex != ZR_NULL) {
        *outCacheIndex = 0;
    }

    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->state->global == ZR_NULL || cs->currentFunction == ZR_NULL) {
        return ZR_FALSE;
    }

    oldCount = (TZrSize)cs->currentFunction->callSiteCacheLength;
    newCount = oldCount + 1u;
    if (newCount - 1u > 0xFFFFu) {
        ZrParser_Compiler_Error(cs, "Too many callsite caches for member-slot emission", location);
        return ZR_FALSE;
    }

    newEntries = (SZrFunctionCallSiteCacheEntry *)ZrCore_Memory_RawMallocWithType(
            cs->state->global,
            sizeof(SZrFunctionCallSiteCacheEntry) * newCount,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (newEntries == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Failed to allocate member-slot callsite cache", location);
        return ZR_FALSE;
    }

    copyBytes = sizeof(SZrFunctionCallSiteCacheEntry) * oldCount;
    if (cs->currentFunction->callSiteCaches != ZR_NULL && copyBytes > 0) {
        memcpy(newEntries, cs->currentFunction->callSiteCaches, copyBytes);
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      cs->currentFunction->callSiteCaches,
                                      sizeof(SZrFunctionCallSiteCacheEntry) * oldCount,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }

    memset(&newEntries[oldCount], 0, sizeof(SZrFunctionCallSiteCacheEntry));
    newEntries[oldCount].kind = (TZrUInt32)kind;
    newEntries[oldCount].instructionIndex = instructionIndex;
    newEntries[oldCount].memberEntryIndex = memberEntryIndex;
    newEntries[oldCount].deoptId = ZR_RUNTIME_SEMIR_DEOPT_ID_NONE;
    newEntries[oldCount].argumentCount = 0;

    cs->currentFunction->callSiteCaches = newEntries;
    cs->currentFunction->callSiteCacheLength = (TZrUInt32)newCount;

    if (outCacheIndex != ZR_NULL) {
        *outCacheIndex = (TZrUInt16)oldCount;
    }
    return ZR_TRUE;
}

static TZrBool emit_member_slot_instruction(SZrCompilerState *cs,
                                            EZrInstructionCode opcode,
                                            TZrUInt32 destinationOrValueSlot,
                                            TZrUInt32 receiverSlot,
                                            TZrUInt32 memberEntryIndex,
                                            SZrFileRange location) {
    TZrUInt16 cacheIndex = 0;
    EZrFunctionCallSiteCacheKind cacheKind;
    TZrInstruction instruction;

    if (cs == ZR_NULL || cs->hasError) {
        return ZR_FALSE;
    }

    cacheKind = opcode == ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT)
                        ? ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET
                        : ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET;
    if (!compiler_append_callsite_cache_entry(cs,
                                              cacheKind,
                                              (TZrUInt32)cs->instructionCount,
                                              memberEntryIndex,
                                              &cacheIndex,
                                              location)) {
        return ZR_FALSE;
    }

    instruction = create_instruction_2(opcode,
                                       ZR_COMPILE_SLOT_U16(destinationOrValueSlot),
                                       ZR_COMPILE_SLOT_U16(receiverSlot),
                                       cacheIndex);
    emit_instruction(cs, instruction);
    return !cs->hasError;
}

TZrBool emit_member_slot_get(SZrCompilerState *cs,
                             TZrUInt32 destinationSlot,
                             TZrUInt32 receiverSlot,
                             TZrUInt32 memberEntryIndex,
                             SZrFileRange location) {
    return emit_member_slot_instruction(cs,
                                        ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT),
                                        destinationSlot,
                                        receiverSlot,
                                        memberEntryIndex,
                                        location);
}

TZrBool emit_member_slot_set(SZrCompilerState *cs,
                             TZrUInt32 valueSlot,
                             TZrUInt32 receiverSlot,
                             TZrUInt32 memberEntryIndex,
                             SZrFileRange location) {
    return emit_member_slot_instruction(cs,
                                        ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT),
                                        valueSlot,
                                        receiverSlot,
                                        memberEntryIndex,
                                        location);
}

TZrBool resolve_declared_field_member_access(SZrCompilerState *cs,
                                             SZrString *ownerTypeName,
                                             SZrString *memberName,
                                             SZrString **outFieldTypeName,
                                             TZrBool *outIsStatic,
                                             EZrOwnershipQualifier *outOwnershipQualifier) {
    SZrTypeMemberInfo *memberInfo;
    SZrAstNode *typeDecl;
    SZrAstNodeArray *members = ZR_NULL;

    if (outFieldTypeName != ZR_NULL) {
        *outFieldTypeName = ZR_NULL;
    }
    if (outIsStatic != ZR_NULL) {
        *outIsStatic = ZR_FALSE;
    }
    if (outOwnershipQualifier != ZR_NULL) {
        *outOwnershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    }
    if (cs == ZR_NULL || ownerTypeName == ZR_NULL || memberName == ZR_NULL) {
        return ZR_FALSE;
    }

    memberInfo = find_compiler_type_member(cs, ownerTypeName, memberName);
    if (memberInfo != ZR_NULL &&
        (memberInfo->memberType == ZR_AST_STRUCT_FIELD || memberInfo->memberType == ZR_AST_CLASS_FIELD)) {
        if (outFieldTypeName != ZR_NULL) {
            *outFieldTypeName = memberInfo->fieldTypeName;
        }
        if (outIsStatic != ZR_NULL) {
            *outIsStatic = memberInfo->isStatic;
        }
        if (outOwnershipQualifier != ZR_NULL) {
            *outOwnershipQualifier = memberInfo->ownershipQualifier;
        }
        return ZR_TRUE;
    }

    typeDecl = find_type_declaration(cs, ownerTypeName);
    if (typeDecl == ZR_NULL) {
        return ZR_FALSE;
    }

    if (typeDecl->type == ZR_AST_CLASS_DECLARATION) {
        members = typeDecl->data.classDeclaration.members;
    } else if (typeDecl->type == ZR_AST_STRUCT_DECLARATION) {
        members = typeDecl->data.structDeclaration.members;
    }
    if (members == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < members->count; index++) {
        SZrAstNode *memberNode = members->nodes[index];

        if (memberNode == ZR_NULL) {
            continue;
        }

        if (memberNode->type == ZR_AST_CLASS_FIELD) {
            SZrClassField *field = &memberNode->data.classField;
            if (field->name == ZR_NULL || field->name->name == ZR_NULL ||
                !ZrCore_String_Equal(field->name->name, memberName)) {
                continue;
            }
            if (outFieldTypeName != ZR_NULL && field->typeInfo != ZR_NULL) {
                *outFieldTypeName = extract_type_name_string(cs, field->typeInfo);
            }
            if (outIsStatic != ZR_NULL) {
                *outIsStatic = field->isStatic;
            }
            return ZR_TRUE;
        }

        if (memberNode->type == ZR_AST_STRUCT_FIELD) {
            SZrStructField *field = &memberNode->data.structField;
            if (field->name == ZR_NULL || field->name->name == ZR_NULL ||
                !ZrCore_String_Equal(field->name->name, memberName)) {
                continue;
            }
            if (outFieldTypeName != ZR_NULL && field->typeInfo != ZR_NULL) {
                *outFieldTypeName = extract_type_name_string(cs, field->typeInfo);
            }
            if (outIsStatic != ZR_NULL) {
                *outIsStatic = field->isStatic;
            }
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static EZrOwnershipBuiltinKind resolve_construct_expression_builtin_kind(
        const SZrConstructExpression *constructExpr) {
    if (constructExpr == ZR_NULL) {
        return ZR_OWNERSHIP_BUILTIN_KIND_NONE;
    }

    if (constructExpr->builtinKind != ZR_OWNERSHIP_BUILTIN_KIND_NONE) {
        return constructExpr->builtinKind;
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
        case ZR_OWNERSHIP_QUALIFIER_LOANED:
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
        case ZR_OWNERSHIP_BUILTIN_KIND_BORROW:
            return ZR_INSTRUCTION_ENUM(OWN_BORROW);
        case ZR_OWNERSHIP_BUILTIN_KIND_LOAN:
            return ZR_INSTRUCTION_ENUM(OWN_LOAN);
        case ZR_OWNERSHIP_BUILTIN_KIND_SHARED:
            return ZR_INSTRUCTION_ENUM(OWN_SHARE);
        case ZR_OWNERSHIP_BUILTIN_KIND_WEAK:
            return ZR_INSTRUCTION_ENUM(OWN_WEAK);
        case ZR_OWNERSHIP_BUILTIN_KIND_DETACH:
            return ZR_INSTRUCTION_ENUM(OWN_DETACH);
        case ZR_OWNERSHIP_BUILTIN_KIND_UPGRADE:
            return ZR_INSTRUCTION_ENUM(OWN_UPGRADE);
        case ZR_OWNERSHIP_BUILTIN_KIND_RELEASE:
            return ZR_INSTRUCTION_ENUM(OWN_RELEASE);
        case ZR_OWNERSHIP_BUILTIN_KIND_NONE:
        default:
            return ZR_INSTRUCTION_ENUM(ENUM_MAX);
    }
}

static const TZrChar *compile_ownership_builtin_operand_error_message(EZrOwnershipBuiltinKind builtinKind) {
    switch (builtinKind) {
        case ZR_OWNERSHIP_BUILTIN_KIND_SHARED:
            return "'%shared' requires a %unique owner";
        case ZR_OWNERSHIP_BUILTIN_KIND_WEAK:
            return "'%weak' requires a %shared owner";
        case ZR_OWNERSHIP_BUILTIN_KIND_LOAN:
            return "'%loan' requires a %unique owner";
        case ZR_OWNERSHIP_BUILTIN_KIND_UPGRADE:
            return "'%upgrade' requires a %weak owner";
        case ZR_OWNERSHIP_BUILTIN_KIND_RELEASE:
            return "'%release' requires a %unique or %shared owner";
        case ZR_OWNERSHIP_BUILTIN_KIND_DETACH:
            return "'%detach' requires a %unique or %shared owner";
        case ZR_OWNERSHIP_BUILTIN_KIND_NONE:
        case ZR_OWNERSHIP_BUILTIN_KIND_UNIQUE:
        case ZR_OWNERSHIP_BUILTIN_KIND_BORROW:
        default:
            return ZR_NULL;
    }
}

static TZrBool compile_ownership_builtin_operand_matches_qualifier(EZrOwnershipBuiltinKind builtinKind,
                                                                   EZrOwnershipQualifier qualifier) {
    switch (builtinKind) {
        case ZR_OWNERSHIP_BUILTIN_KIND_SHARED:
        case ZR_OWNERSHIP_BUILTIN_KIND_LOAN:
            return qualifier == ZR_OWNERSHIP_QUALIFIER_UNIQUE;
        case ZR_OWNERSHIP_BUILTIN_KIND_WEAK:
            return qualifier == ZR_OWNERSHIP_QUALIFIER_SHARED;
        case ZR_OWNERSHIP_BUILTIN_KIND_UPGRADE:
            return qualifier == ZR_OWNERSHIP_QUALIFIER_WEAK;
        case ZR_OWNERSHIP_BUILTIN_KIND_RELEASE:
        case ZR_OWNERSHIP_BUILTIN_KIND_DETACH:
            return qualifier == ZR_OWNERSHIP_QUALIFIER_UNIQUE ||
                   qualifier == ZR_OWNERSHIP_QUALIFIER_SHARED;
        case ZR_OWNERSHIP_BUILTIN_KIND_NONE:
        case ZR_OWNERSHIP_BUILTIN_KIND_UNIQUE:
        case ZR_OWNERSHIP_BUILTIN_KIND_BORROW:
        default:
            return ZR_TRUE;
    }
}

static TZrBool validate_ownership_builtin_operand_expression(SZrCompilerState *cs,
                                                             const SZrConstructExpression *constructExpr,
                                                             SZrFileRange errorLocation) {
    EZrOwnershipBuiltinKind builtinKind;
    SZrInferredType inferredType;
    const TZrChar *errorMessage;
    TZrBool success;

    if (cs == ZR_NULL || constructExpr == ZR_NULL) {
        return ZR_FALSE;
    }

    builtinKind = resolve_construct_expression_builtin_kind(constructExpr);
    errorMessage = compile_ownership_builtin_operand_error_message(builtinKind);
    if (errorMessage == ZR_NULL) {
        return ZR_TRUE;
    }

    ZrParser_InferredType_Init(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    success = ZrParser_ExpressionType_Infer(cs, constructExpr->target, &inferredType);
    if (!success) {
        ZrParser_InferredType_Free(cs->state, &inferredType);
        return ZR_FALSE;
    }

    if (!compile_ownership_builtin_operand_matches_qualifier(builtinKind, inferredType.ownershipQualifier)) {
        ZrParser_InferredType_Free(cs->state, &inferredType);
        ZrParser_Compiler_Error(cs, errorMessage, errorLocation);
        return ZR_FALSE;
    }

    ZrParser_InferredType_Free(cs->state, &inferredType);
    return ZR_TRUE;
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

    if (!validate_ownership_builtin_operand_expression(cs,
                                                       constructExpr,
                                                       constructExpr->target != ZR_NULL ? constructExpr->target->location : location)) {
        return ZR_FALSE;
    }

    resultSlot = allocate_stack_slot(cs);

    if (builtinKind == ZR_OWNERSHIP_BUILTIN_KIND_RELEASE ||
        builtinKind == ZR_OWNERSHIP_BUILTIN_KIND_DETACH) {
        TZrUInt32 sourceSlot;

        if (constructExpr->target == ZR_NULL || constructExpr->target->type != ZR_AST_IDENTIFIER_LITERAL) {
            ZrParser_Compiler_Error(cs,
                                    builtinKind == ZR_OWNERSHIP_BUILTIN_KIND_RELEASE
                                            ? "'%release' currently requires a local identifier binding"
                                            : "'%detach' currently requires a local identifier binding",
                                    location);
            return ZR_FALSE;
        }

        sourceSlot = find_local_var(cs, constructExpr->target->data.identifier.name);
        if (sourceSlot == ZR_PARSER_SLOT_NONE) {
            ZrParser_Compiler_Error(cs,
                                    builtinKind == ZR_OWNERSHIP_BUILTIN_KIND_RELEASE
                                            ? "'%release' currently only supports local identifier bindings"
                                            : "'%detach' currently only supports local identifier bindings",
                                    location);
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

    shouldResetConsumedIdentifier =
            (builtinKind == ZR_OWNERSHIP_BUILTIN_KIND_SHARED ||
             builtinKind == ZR_OWNERSHIP_BUILTIN_KIND_LOAN) &&
            constructExpr->target != ZR_NULL &&
            constructExpr->target->type == ZR_AST_IDENTIFIER_LITERAL &&
            infer_expression_ownership_qualifier_local(cs, constructExpr->target) ==
                    ZR_OWNERSHIP_QUALIFIER_UNIQUE;

    if (shouldResetConsumedIdentifier) {
        argumentSlot = find_local_var(cs, constructExpr->target->data.identifier.name);
        if (argumentSlot == ZR_PARSER_SLOT_NONE) {
            ZrParser_Compiler_Error(cs,
                                    "Failed to resolve consumed ownership source binding",
                                    location);
            return ZR_FALSE;
        }
    } else {
        argumentSlot = allocate_stack_slot(cs);
        if (compile_expression_into_slot(cs, constructExpr->target, argumentSlot) == ZR_PARSER_SLOT_NONE) {
            return ZR_FALSE;
        }
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

TZrBool compiler_is_super_identifier_node(SZrAstNode *node) {
    return node != ZR_NULL &&
           node->type == ZR_AST_IDENTIFIER_LITERAL &&
           zr_string_equals_cstr_local(node->data.identifier.name, "super");
}

TZrBool compiler_resolve_super_member_context(SZrCompilerState *cs,
                                              SZrFileRange location,
                                              SZrString **outSuperTypeName,
                                              TZrUInt32 *outReceiverSlot,
                                              EZrOwnershipQualifier *outReceiverOwnershipQualifier) {
    SZrString *thisName;
    TZrUInt32 receiverSlot = ZR_PARSER_SLOT_NONE;
    EZrOwnershipQualifier receiverQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;

    if (outSuperTypeName != ZR_NULL) {
        *outSuperTypeName = ZR_NULL;
    }
    if (outReceiverSlot != ZR_NULL) {
        *outReceiverSlot = ZR_PARSER_SLOT_NONE;
    }
    if (outReceiverOwnershipQualifier != ZR_NULL) {
        *outReceiverOwnershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    }

    if (cs == ZR_NULL) {
        return ZR_FALSE;
    }

    if (cs->currentTypePrototypeInfo == ZR_NULL || cs->currentTypePrototypeInfo->extendsTypeName == ZR_NULL) {
        ZrParser_Compiler_Error(cs,
                                "super.member requires a direct base class",
                                location);
        return ZR_FALSE;
    }

    if (cs->currentFunctionNode == ZR_NULL) {
        ZrParser_Compiler_Error(cs,
                                "super.member is only valid inside instance methods, property accessors, or non-constructor meta methods",
                                location);
        return ZR_FALSE;
    }

    switch (cs->currentFunctionNode->type) {
        case ZR_AST_CLASS_METHOD:
            if (cs->currentFunctionNode->data.classMethod.isStatic) {
                ZrParser_Compiler_Error(cs,
                                        "super.member is not available inside static methods",
                                        location);
                return ZR_FALSE;
            }
            receiverQualifier =
                    get_implicit_this_ownership_qualifier(get_member_receiver_qualifier(cs->currentFunctionNode));
            break;

        case ZR_AST_CLASS_PROPERTY:
            if (cs->currentFunctionNode->data.classProperty.isStatic) {
                ZrParser_Compiler_Error(cs,
                                        "super.member is not available inside static property accessors",
                                        location);
                return ZR_FALSE;
            }
            break;

        case ZR_AST_CLASS_META_FUNCTION:
            if (cs->currentFunctionNode->data.classMetaFunction.isStatic) {
                ZrParser_Compiler_Error(cs,
                                        "super.member is not available inside static meta methods",
                                        location);
                return ZR_FALSE;
            }
            if (cs->currentFunctionNode->data.classMetaFunction.meta != ZR_NULL &&
                zr_string_equals_cstr_local(cs->currentFunctionNode->data.classMetaFunction.meta->name,
                                            "constructor")) {
                ZrParser_Compiler_Error(cs,
                                        "super.member is not available inside constructors; use super(...) for constructor chaining",
                                        location);
                return ZR_FALSE;
            }
            break;

        default:
            ZrParser_Compiler_Error(cs,
                                    "super.member is only valid inside instance methods, property accessors, or non-constructor meta methods",
                                    location);
            return ZR_FALSE;
    }

    thisName = ZrCore_String_CreateFromNative(cs->state, "this");
    if (thisName == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Failed to allocate implicit receiver binding for super.member", location);
        return ZR_FALSE;
    }

    receiverSlot = find_local_var(cs, thisName);
    if (receiverSlot == ZR_PARSER_SLOT_NONE) {
        ZrParser_Compiler_Error(cs,
                                "super.member requires an implicit instance receiver",
                                location);
        return ZR_FALSE;
    }

    if (outSuperTypeName != ZR_NULL) {
        *outSuperTypeName = cs->currentTypePrototypeInfo->extendsTypeName;
    }
    if (outReceiverSlot != ZR_NULL) {
        *outReceiverSlot = receiverSlot;
    }
    if (outReceiverOwnershipQualifier != ZR_NULL) {
        *outReceiverOwnershipQualifier = receiverQualifier;
    }
    return ZR_TRUE;
}

TZrBool emit_member_function_constant_to_slot(SZrCompilerState *cs,
                                              TZrUInt32 targetSlot,
                                              const SZrTypeMemberInfo *memberInfo,
                                              SZrFileRange location) {
    SZrTypeValue functionValue;
    TZrUInt32 constantIndex;

    if (cs == ZR_NULL || memberInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    if (memberInfo->compiledFunction == ZR_NULL) {
        ZrParser_Compiler_Error(cs,
                                "Failed to bind base meta member for super dispatch",
                                location);
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(cs->state,
                                 &functionValue,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(memberInfo->compiledFunction));
    constantIndex = add_constant(cs, &functionValue);
    emit_instruction(cs,
                     create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT),
                                          (TZrUInt16)targetSlot,
                                          (TZrInt32)constantIndex));
    return !cs->hasError;
}

TZrBool emit_super_accessor_call_from_prototype(SZrCompilerState *cs,
                                                TZrUInt32 prototypeSlot,
                                                TZrUInt32 receiverSlot,
                                                SZrString *accessorName,
                                                const TZrUInt32 *argumentSlots,
                                                TZrUInt32 argumentCount,
                                                SZrFileRange location) {
    TZrUInt32 memberId;
    TZrUInt32 receiverArgSlot;

    if (cs == ZR_NULL || accessorName == ZR_NULL || cs->hasError) {
        return ZR_FALSE;
    }

    memberId = compiler_get_or_add_member_entry(cs, accessorName);
    if (memberId == ZR_PARSER_MEMBER_ID_NONE) {
        ZrParser_Compiler_Error(cs,
                                "Failed to register base accessor symbol for super dispatch",
                                location);
        return ZR_FALSE;
    }

    emit_instruction(cs,
                     create_instruction_2(ZR_INSTRUCTION_ENUM(GET_MEMBER),
                                          (TZrUInt16)prototypeSlot,
                                          (TZrUInt16)prototypeSlot,
                                          (TZrUInt16)memberId));

    receiverArgSlot = allocate_stack_slot(cs);
    emit_instruction(cs,
                     create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK),
                                          (TZrUInt16)receiverArgSlot,
                                          (TZrInt32)receiverSlot));

    for (TZrUInt32 index = 0; index < argumentCount; index++) {
        TZrUInt32 copiedArgSlot = allocate_stack_slot(cs);
        emit_instruction(cs,
                         create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK),
                                              (TZrUInt16)copiedArgSlot,
                                              (TZrInt32)argumentSlots[index]));
    }

    emit_instruction(cs,
                     create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_CALL),
                                          (TZrUInt16)prototypeSlot,
                                          (TZrUInt16)prototypeSlot,
                                          (TZrUInt16)(argumentCount + 1)));
    collapse_stack_to_slot(cs, prototypeSlot);
    return !cs->hasError;
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
        case ZR_OWNERSHIP_QUALIFIER_LOANED:
            return memberQualifier == ZR_OWNERSHIP_QUALIFIER_LOANED ||
                   memberQualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED;
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
        case ZR_OWNERSHIP_QUALIFIER_LOANED:
            return "Loaned receivers can only call %loaned or %borrowed methods";
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
    if (localVarIndex != ZR_PARSER_SLOT_NONE) {
        emit_instruction(cs,
                         create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK),
                                              (TZrUInt16)localVarIndex,
                                              (TZrInt32)nullSlot));
        ZrParser_Compiler_TrimStackBy(cs, 1);
        return ZR_TRUE;
    }

    closureVarIndex = find_closure_var(cs, name);
    if (closureVarIndex != ZR_PARSER_INDEX_NONE) {
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

    if (cs->isCompilingCompileTimeRuntimeSupport) {
        return ZR_FALSE;
    }

    if (zr_string_equals_cstr_local(rootName, "Assert") ||
        zr_string_equals_cstr_local(rootName, "FatalError")) {
        return ZR_TRUE;
    }

    return has_compile_time_variable_binding_local(cs, rootName) ||
           has_compile_time_function_binding_local(cs, rootName);
}

static TZrBool import_expression_targets_module(SZrAstNode *rootNode, const TZrChar *moduleName) {
    SZrAstNode *modulePathNode;

    if (rootNode == ZR_NULL || moduleName == ZR_NULL || rootNode->type != ZR_AST_IMPORT_EXPRESSION) {
        return ZR_FALSE;
    }

    modulePathNode = rootNode->data.importExpression.modulePath;
    return modulePathNode != ZR_NULL &&
           modulePathNode->type == ZR_AST_STRING_LITERAL &&
           modulePathNode->data.stringLiteral.value != ZR_NULL &&
           zr_string_equals_cstr_local(modulePathNode->data.stringLiteral.value, moduleName);
}

static TZrBool primary_root_is_compile_time_projection_candidate(SZrCompilerState *cs, SZrAstNode *rootNode) {
    if (rootNode == ZR_NULL) {
        return ZR_FALSE;
    }

    if (rootNode->type == ZR_AST_IMPORT_EXPRESSION) {
        if (import_expression_targets_module(rootNode, "zr.task")) {
            return ZR_FALSE;
        }
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

static TZrBool compiler_type_is_integral(EZrValueType type) {
    return (TZrBool)(ZR_VALUE_IS_TYPE_SIGNED_INT(type) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(type));
}

static TZrBool compiler_type_is_numeric_like_for_arithmetic(EZrValueType type) {
    return (TZrBool)(ZR_VALUE_IS_TYPE_NUMBER(type) || type == ZR_VALUE_TYPE_BOOL);
}

static TZrBool compiler_operands_can_use_numeric_fast_path(EZrValueType leftType, EZrValueType rightType) {
    return (TZrBool)(compiler_type_is_numeric_like_for_arithmetic(leftType) &&
                     compiler_type_is_numeric_like_for_arithmetic(rightType));
}

static TZrBool compiler_type_prefers_unsigned(EZrValueType resultType, EZrValueType leftType, EZrValueType rightType) {
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(resultType)) {
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_SIGNED_INT(resultType)) {
        return ZR_FALSE;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftType) && !ZR_VALUE_IS_TYPE_SIGNED_INT(rightType)) {
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightType) && !ZR_VALUE_IS_TYPE_SIGNED_INT(leftType)) {
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

TZrBool compiler_try_infer_expression_base_type(SZrCompilerState *cs, SZrAstNode *expression, EZrValueType *outType) {
    SZrInferredType inferredType;
    TZrBool success;

    if (outType != ZR_NULL) {
        *outType = ZR_VALUE_TYPE_OBJECT;
    }
    if (cs == ZR_NULL || expression == ZR_NULL || outType == ZR_NULL || cs->typeEnv == ZR_NULL ||
        expression_uses_dynamic_object_access(expression)) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    success = ZrParser_ExpressionType_Infer(cs, expression, &inferredType);
    if (success) {
        *outType = inferredType.baseType;
    }
    ZrParser_InferredType_Free(cs->state, &inferredType);
    return success;
}

EZrInstructionCode compiler_select_binary_arithmetic_opcode(const TZrChar *op,
                                                            TZrBool hasTypeInfo,
                                                            EZrValueType resultType,
                                                            EZrValueType leftType,
                                                            EZrValueType rightType) {
    TZrBool prefersUnsigned;
    TZrBool integralOperands;

    if (op == ZR_NULL) {
        return ZR_INSTRUCTION_ENUM(ENUM_MAX);
    }

    prefersUnsigned = compiler_type_prefers_unsigned(resultType, leftType, rightType);
    integralOperands = (TZrBool)(compiler_type_is_integral(resultType) || compiler_type_is_integral(leftType) ||
                                 compiler_type_is_integral(rightType));

    if (strcmp(op, "+") == 0) {
        if (!hasTypeInfo) {
            return ZR_INSTRUCTION_ENUM(ADD);
        }
        if (resultType == ZR_VALUE_TYPE_STRING || leftType == ZR_VALUE_TYPE_STRING || rightType == ZR_VALUE_TYPE_STRING) {
            return ZR_INSTRUCTION_ENUM(ADD_STRING);
        }
        if (!compiler_operands_can_use_numeric_fast_path(leftType, rightType)) {
            return ZR_INSTRUCTION_ENUM(ADD);
        }
        if (binary_expression_type_is_float_like(resultType) || binary_expression_type_is_float_like(leftType) ||
            binary_expression_type_is_float_like(rightType)) {
            return ZR_INSTRUCTION_ENUM(ADD_FLOAT);
        }
        if (!integralOperands) {
            return ZR_INSTRUCTION_ENUM(ADD);
        }
        return prefersUnsigned ? ZR_INSTRUCTION_ENUM(ADD_UNSIGNED) : ZR_INSTRUCTION_ENUM(ADD_SIGNED);
    }

    if (strcmp(op, "-") == 0) {
        if (!hasTypeInfo) {
            return ZR_INSTRUCTION_ENUM(SUB);
        }
        if (!compiler_operands_can_use_numeric_fast_path(leftType, rightType)) {
            return ZR_INSTRUCTION_ENUM(SUB);
        }
        if (binary_expression_type_is_float_like(resultType) || binary_expression_type_is_float_like(leftType) ||
            binary_expression_type_is_float_like(rightType)) {
            return ZR_INSTRUCTION_ENUM(SUB_FLOAT);
        }
        if (!integralOperands) {
            return ZR_INSTRUCTION_ENUM(SUB);
        }
        return prefersUnsigned ? ZR_INSTRUCTION_ENUM(SUB_UNSIGNED) : ZR_INSTRUCTION_ENUM(SUB_SIGNED);
    }

    if (strcmp(op, "*") == 0) {
        if (!hasTypeInfo) {
            return ZR_INSTRUCTION_ENUM(MUL);
        }
        if (!compiler_operands_can_use_numeric_fast_path(leftType, rightType)) {
            return ZR_INSTRUCTION_ENUM(MUL);
        }
        if (binary_expression_type_is_float_like(resultType) || binary_expression_type_is_float_like(leftType) ||
            binary_expression_type_is_float_like(rightType)) {
            return ZR_INSTRUCTION_ENUM(MUL_FLOAT);
        }
        if (!integralOperands) {
            return ZR_INSTRUCTION_ENUM(MUL);
        }
        return prefersUnsigned ? ZR_INSTRUCTION_ENUM(MUL_UNSIGNED) : ZR_INSTRUCTION_ENUM(MUL_SIGNED);
    }

    if (strcmp(op, "/") == 0) {
        if (!hasTypeInfo) {
            return ZR_INSTRUCTION_ENUM(DIV);
        }
        if (!compiler_operands_can_use_numeric_fast_path(leftType, rightType)) {
            return ZR_INSTRUCTION_ENUM(DIV);
        }
        if (binary_expression_type_is_float_like(resultType) || binary_expression_type_is_float_like(leftType) ||
            binary_expression_type_is_float_like(rightType)) {
            return ZR_INSTRUCTION_ENUM(DIV_FLOAT);
        }
        if (!integralOperands) {
            return ZR_INSTRUCTION_ENUM(DIV);
        }
        return prefersUnsigned ? ZR_INSTRUCTION_ENUM(DIV_UNSIGNED) : ZR_INSTRUCTION_ENUM(DIV_SIGNED);
    }

    if (strcmp(op, "%") == 0) {
        if (!hasTypeInfo) {
            return ZR_INSTRUCTION_ENUM(MOD);
        }
        if (!compiler_operands_can_use_numeric_fast_path(leftType, rightType)) {
            return ZR_INSTRUCTION_ENUM(MOD);
        }
        if (binary_expression_type_is_float_like(resultType) || binary_expression_type_is_float_like(leftType) ||
            binary_expression_type_is_float_like(rightType)) {
            return ZR_INSTRUCTION_ENUM(MOD_FLOAT);
        }
        if (!integralOperands) {
            return ZR_INSTRUCTION_ENUM(MOD);
        }
        return prefersUnsigned ? ZR_INSTRUCTION_ENUM(MOD_UNSIGNED) : ZR_INSTRUCTION_ENUM(MOD_SIGNED);
    }

    if (strcmp(op, "**") == 0) {
        if (!hasTypeInfo) {
            return ZR_INSTRUCTION_ENUM(POW);
        }
        if (!compiler_operands_can_use_numeric_fast_path(leftType, rightType)) {
            return ZR_INSTRUCTION_ENUM(POW);
        }
        if (binary_expression_type_is_float_like(resultType) || binary_expression_type_is_float_like(leftType) ||
            binary_expression_type_is_float_like(rightType)) {
            return ZR_INSTRUCTION_ENUM(POW_FLOAT);
        }
        if (!integralOperands) {
            return ZR_INSTRUCTION_ENUM(POW);
        }
        return prefersUnsigned ? ZR_INSTRUCTION_ENUM(POW_UNSIGNED) : ZR_INSTRUCTION_ENUM(POW_SIGNED);
    }

    return ZR_INSTRUCTION_ENUM(ENUM_MAX);
}

EZrInstructionCode compiler_select_binary_equality_opcode(TZrBool isNotEqual,
                                                          TZrBool hasTypeInfo,
                                                          EZrValueType leftType,
                                                          EZrValueType rightType) {
    if (!hasTypeInfo) {
        return isNotEqual ? ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL) : ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL);
    }

    if (leftType == ZR_VALUE_TYPE_BOOL && rightType == ZR_VALUE_TYPE_BOOL) {
        return isNotEqual ? ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_BOOL)
                          : ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_BOOL);
    }

    if (leftType == ZR_VALUE_TYPE_STRING && rightType == ZR_VALUE_TYPE_STRING) {
        return isNotEqual ? ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_STRING)
                          : ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_STRING);
    }

    if (binary_expression_type_is_float_like(leftType) || binary_expression_type_is_float_like(rightType)) {
        return isNotEqual ? ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_FLOAT)
                          : ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_FLOAT);
    }

    if (compiler_type_is_integral(leftType) && compiler_type_is_integral(rightType)) {
        if (compiler_type_prefers_unsigned(ZR_VALUE_TYPE_OBJECT, leftType, rightType)) {
            return isNotEqual ? ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_UNSIGNED)
                              : ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_UNSIGNED);
        }
        return isNotEqual ? ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_SIGNED)
                          : ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_SIGNED);
    }

    return isNotEqual ? ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL) : ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL);
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
        return ZR_PARSER_SLOT_NONE;
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
        return ZR_PARSER_SLOT_NONE;
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
        return ZR_PARSER_SLOT_NONE;
    }

    compile_expression_non_tail(cs, node);
    if (cs->hasError) {
        return ZR_PARSER_SLOT_NONE;
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
    if (memberId == ZR_PARSER_MEMBER_ID_NONE) {
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
        return ZR_PARSER_SLOT_NONE;
    }

    SZrString *accessorName = create_hidden_property_accessor_name(cs, propertyName, ZR_TRUE);
    if (accessorName == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Failed to create property setter accessor name", location);
        return ZR_PARSER_SLOT_NONE;
    }

    memberId = compiler_get_or_add_member_entry_with_flags(cs, accessorName, memberFlags);
    if (memberId == ZR_PARSER_MEMBER_ID_NONE) {
        ZrParser_Compiler_Error(cs, "Failed to register property setter member symbol", location);
        return ZR_PARSER_SLOT_NONE;
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
        return ZR_PARSER_SLOT_NONE;
    }

    if (!memberExpr->computed) {
        SZrString *fieldName = resolve_member_expression_symbol(cs, memberExpr);
        if (fieldName == ZR_NULL || emit_string_constant(cs, fieldName) == ZR_PARSER_SLOT_NONE) {
            return ZR_PARSER_SLOT_NONE;
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


