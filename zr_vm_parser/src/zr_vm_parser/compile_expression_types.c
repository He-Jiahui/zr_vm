//
// Created by Auto on 2025/01/XX.
//

#include "compile_expression_internal.h"
#include "type_inference_internal.h"

static TZrBool compile_inferred_type_is_task_handle(const SZrInferredType *type) {
    const TZrChar *typeName;

    if (type == ZR_NULL || type->typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (type->typeName->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        typeName = ZrCore_String_GetNativeStringShort(type->typeName);
    } else {
        typeName = ZrCore_String_GetNativeString(type->typeName);
    }

    if (typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    return strcmp(typeName, "Task") == 0 || strncmp(typeName, "Task<", 5) == 0;
}

static void compile_expression_ensure_imported_module_runtime_metadata(SZrCompilerState *cs, SZrString *moduleName) {
    if (cs == ZR_NULL || moduleName == ZR_NULL) {
        return;
    }

    if (find_compiler_type_prototype(cs, moduleName) == ZR_NULL) {
        ensure_import_module_compile_info(cs, moduleName);
    }
}

SZrAstNode *find_type_declaration(SZrCompilerState *cs, SZrString *typeName) {
    if (cs == ZR_NULL || cs->scriptAst == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }
    
    if (cs->scriptAst->type != ZR_AST_SCRIPT) {
        return ZR_NULL;
    }
    
    SZrScript *script = &cs->scriptAst->data.script;
    if (script->statements == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 遍历顶层语句，查找 struct / class / interface 声明
    for (TZrSize i = 0; i < script->statements->count; i++) {
        SZrAstNode *stmt = script->statements->nodes[i];
        if (stmt == ZR_NULL) {
            continue;
        }
        
        // 检查是否是 struct 声明
        if (stmt->type == ZR_AST_STRUCT_DECLARATION) {
            SZrIdentifier *structName = stmt->data.structDeclaration.name;
            if (structName != ZR_NULL && structName->name != ZR_NULL) {
                if (ZrCore_String_Equal(structName->name, typeName)) {
                    return stmt;
                }
            }
        }
        
        // 检查是否是 class 声明
        if (stmt->type == ZR_AST_CLASS_DECLARATION) {
            SZrIdentifier *className = stmt->data.classDeclaration.name;
            if (className != ZR_NULL && className->name != ZR_NULL) {
                if (ZrCore_String_Equal(className->name, typeName)) {
                    return stmt;
                }
            }
        }

        if (stmt->type == ZR_AST_INTERFACE_DECLARATION) {
            SZrIdentifier *interfaceName = stmt->data.interfaceDeclaration.name;
            if (interfaceName != ZR_NULL && interfaceName->name != ZR_NULL) {
                if (ZrCore_String_Equal(interfaceName->name, typeName)) {
                    return stmt;
                }
            }
        }
    }

    return ZR_NULL;
}

TZrBool find_current_type_field_metadata(SZrCompilerState *cs,
                                                SZrString *typeName,
                                                SZrString *memberName,
                                                TZrBool *outIsConst,
                                                TZrBool *outIsStatic) {
    SZrAstNodeArray *members = ZR_NULL;

    if (outIsConst != ZR_NULL) {
        *outIsConst = ZR_FALSE;
    }
    if (outIsStatic != ZR_NULL) {
        *outIsStatic = ZR_FALSE;
    }

    if (cs == ZR_NULL || cs->currentTypeNode == ZR_NULL || typeName == ZR_NULL || memberName == ZR_NULL ||
        cs->currentTypeName == ZR_NULL || !ZrCore_String_Equal(cs->currentTypeName, typeName)) {
        return ZR_FALSE;
    }

    if (cs->currentTypeNode->type == ZR_AST_CLASS_DECLARATION) {
        members = cs->currentTypeNode->data.classDeclaration.members;
    } else if (cs->currentTypeNode->type == ZR_AST_STRUCT_DECLARATION) {
        members = cs->currentTypeNode->data.structDeclaration.members;
    }

    if (members == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < members->count; index++) {
        SZrAstNode *member = members->nodes[index];

        if (member == ZR_NULL) {
            continue;
        }

        if (member->type == ZR_AST_CLASS_FIELD &&
            member->data.classField.name != ZR_NULL &&
            member->data.classField.name->name != ZR_NULL &&
            ZrCore_String_Equal(member->data.classField.name->name, memberName)) {
            if (outIsConst != ZR_NULL) {
                *outIsConst = member->data.classField.isConst;
            }
            if (outIsStatic != ZR_NULL) {
                *outIsStatic = member->data.classField.isStatic;
            }
            return ZR_TRUE;
        }

        if (member->type == ZR_AST_STRUCT_FIELD &&
            member->data.structField.name != ZR_NULL &&
            member->data.structField.name->name != ZR_NULL &&
            ZrCore_String_Equal(member->data.structField.name->name, memberName)) {
            if (outIsConst != ZR_NULL) {
                *outIsConst = member->data.structField.isConst;
            }
            if (outIsStatic != ZR_NULL) {
                *outIsStatic = member->data.structField.isStatic;
            }
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

TZrBool find_type_member_is_const(SZrCompilerState *cs, SZrString *typeName, SZrString *memberName) {
    TZrBool isConst = ZR_FALSE;

    if (cs == ZR_NULL || typeName == ZR_NULL || memberName == ZR_NULL) {
        return ZR_FALSE;
    }

    SZrTypeMemberInfo *memberInfo = find_compiler_type_member(cs, typeName, memberName);
    if (memberInfo != ZR_NULL &&
        (memberInfo->memberType == ZR_AST_STRUCT_FIELD || memberInfo->memberType == ZR_AST_CLASS_FIELD) &&
        memberInfo->isConst) {
        return ZR_TRUE;
    }

    if (find_current_type_field_metadata(cs, typeName, memberName, &isConst, ZR_NULL)) {
        return isConst;
    }

    return ZR_FALSE;
}

SZrTypePrototypeInfo *find_compiler_type_prototype(SZrCompilerState *cs, SZrString *typeName) {
    SZrTypePrototypeInfo *info;

    if (cs == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize i = 0; i < cs->typePrototypes.length; i++) {
        info = (SZrTypePrototypeInfo *)ZrCore_Array_Get(&cs->typePrototypes, i);
        if (info != ZR_NULL && info->name != ZR_NULL && ZrCore_String_Equal(info->name, typeName)) {
            return info;
        }
    }

    // Fall back to the in-progress prototype only when the type has not been
    // published into the stable prototype table yet.
    if (cs->currentTypePrototypeInfo != ZR_NULL && cs->currentTypePrototypeInfo->name != ZR_NULL &&
        ZrCore_String_Equal(cs->currentTypePrototypeInfo->name, typeName)) {
        return cs->currentTypePrototypeInfo;
    }

    if (ensure_generic_instance_type_prototype(cs, typeName)) {
        for (TZrSize i = 0; i < cs->typePrototypes.length; i++) {
            info = (SZrTypePrototypeInfo *)ZrCore_Array_Get(&cs->typePrototypes, i);
            if (info != ZR_NULL && info->name != ZR_NULL && ZrCore_String_Equal(info->name, typeName)) {
                return info;
            }
        }
    }

    return ZR_NULL;
}

static SZrTypeMemberInfo *find_compiler_type_member_recursive(SZrCompilerState *cs, SZrString *typeName,
                                                              SZrString *memberName, TZrUInt32 depth) {
    SZrTypePrototypeInfo *info;
    SZrArray membersSnapshot;
    SZrArray inheritsSnapshot;

    if (cs == ZR_NULL || typeName == ZR_NULL || memberName == ZR_NULL ||
        depth > ZR_PARSER_RECURSIVE_MEMBER_LOOKUP_MAX_DEPTH) {
        return ZR_NULL;
    }

    info = find_compiler_type_prototype(cs, typeName);
    if (info == ZR_NULL) {
        return ZR_NULL;
    }

    /* Generic prototype materialization may grow cs->typePrototypes, so keep shallow snapshots of the
     * member/inheritance arrays instead of dereferencing info after recursive lookups. */
    membersSnapshot = info->members;
    inheritsSnapshot = info->inherits;

    for (TZrSize i = 0; i < membersSnapshot.length; i++) {
        SZrTypeMemberInfo *memberInfo = (SZrTypeMemberInfo *)ZrCore_Array_Get(&membersSnapshot, i);
        if (memberInfo != ZR_NULL && memberInfo->name != ZR_NULL && ZrCore_String_Equal(memberInfo->name, memberName)) {
            return memberInfo;
        }
    }

    for (TZrSize i = 0; i < inheritsSnapshot.length; i++) {
        SZrString **inheritTypeNamePtr = (SZrString **)ZrCore_Array_Get(&inheritsSnapshot, i);
        if (inheritTypeNamePtr == ZR_NULL || *inheritTypeNamePtr == ZR_NULL) {
            continue;
        }

        if (ZrCore_String_Equal(*inheritTypeNamePtr, typeName)) {
            continue;
        }

        SZrTypeMemberInfo *inheritedMember = find_compiler_type_member_recursive(
            cs, *inheritTypeNamePtr, memberName, depth + 1);
        if (inheritedMember != ZR_NULL) {
            return inheritedMember;
        }
    }

    return ZR_NULL;
}

SZrTypeMemberInfo *find_compiler_type_member(SZrCompilerState *cs, SZrString *typeName, SZrString *memberName) {
    if (typeName == ZR_NULL || memberName == ZR_NULL) {
        return ZR_NULL;
    }

    return find_compiler_type_member_recursive(cs, typeName, memberName, 0);
}

static TZrBool type_name_is_registered_prototype(SZrCompilerState *cs, SZrString *typeName) {
    SZrTypePrototypeInfo *info = find_compiler_type_prototype(cs, typeName);
    return info != ZR_NULL && info->type != ZR_OBJECT_PROTOTYPE_TYPE_MODULE;
}

static const SZrTypeMemberInfo *find_compiler_type_meta_member_recursive(SZrCompilerState *cs,
                                                                         SZrString *typeName,
                                                                         EZrMetaType metaType,
                                                                         TZrUInt32 depth) {
    SZrTypePrototypeInfo *info;
    SZrArray membersSnapshot;
    SZrArray inheritsSnapshot;

    if (cs == ZR_NULL || typeName == ZR_NULL || metaType >= ZR_META_ENUM_MAX ||
        depth > ZR_PARSER_RECURSIVE_MEMBER_LOOKUP_MAX_DEPTH) {
        return ZR_NULL;
    }

    info = find_compiler_type_prototype(cs, typeName);
    if (info == ZR_NULL) {
        return ZR_NULL;
    }

    membersSnapshot = info->members;
    inheritsSnapshot = info->inherits;

    for (TZrSize i = 0; i < membersSnapshot.length; i++) {
        SZrTypeMemberInfo *memberInfo = (SZrTypeMemberInfo *)ZrCore_Array_Get(&membersSnapshot, i);
        if (memberInfo != ZR_NULL && memberInfo->isMetaMethod && memberInfo->metaType == metaType) {
            return memberInfo;
        }
    }

    for (TZrSize i = 0; i < inheritsSnapshot.length; i++) {
        SZrString **inheritTypeNamePtr = (SZrString **)ZrCore_Array_Get(&inheritsSnapshot, i);
        const SZrTypeMemberInfo *inheritedMember;

        if (inheritTypeNamePtr == ZR_NULL || *inheritTypeNamePtr == ZR_NULL) {
            continue;
        }

        if (ZrCore_String_Equal(*inheritTypeNamePtr, typeName)) {
            continue;
        }

        inheritedMember = find_compiler_type_meta_member_recursive(cs, *inheritTypeNamePtr, metaType, depth + 1);
        if (inheritedMember != ZR_NULL) {
            return inheritedMember;
        }
    }

    return ZR_NULL;
}

static const SZrTypeMemberInfo *find_compiler_type_meta_member(SZrCompilerState *cs,
                                                               SZrString *typeName,
                                                               EZrMetaType metaType) {
    if (typeName == ZR_NULL) {
        return ZR_NULL;
    }

    return find_compiler_type_meta_member_recursive(cs, typeName, metaType, 0);
}

static TZrBool type_has_constructor(SZrCompilerState *cs, SZrString *typeName) {
    SZrTypePrototypeInfo *info = find_compiler_type_prototype(cs, typeName);
    if (info == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize i = 0; i < info->members.length; i++) {
        SZrTypeMemberInfo *memberInfo = (SZrTypeMemberInfo *)ZrCore_Array_Get(&info->members, i);
        if (memberInfo != ZR_NULL && memberInfo->isMetaMethod && memberInfo->metaType == ZR_META_CONSTRUCTOR) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static SZrString *resolve_expression_type_name(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL) {
        return ZR_NULL;
    }

    SZrInferredType inferredType;
    ZrParser_InferredType_Init(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    if (!ZrParser_ExpressionType_Infer(cs, node, &inferredType)) {
        return ZR_NULL;
    }

    SZrString *typeName = inferredType.typeName;
    ZrParser_InferredType_Free(cs->state, &inferredType);
    return typeName;
}

static TZrBool resolve_member_segment_names(SZrCompilerState *cs,
                                            SZrAstNode *propertyNode,
                                            SZrString **outLookupName,
                                            SZrString **outResolvedTypeName) {
    if (outLookupName != ZR_NULL) {
        *outLookupName = ZR_NULL;
    }
    if (outResolvedTypeName != ZR_NULL) {
        *outResolvedTypeName = ZR_NULL;
    }
    if (cs == ZR_NULL || propertyNode == ZR_NULL || outLookupName == ZR_NULL || outResolvedTypeName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (propertyNode->type == ZR_AST_IDENTIFIER_LITERAL) {
        *outLookupName = propertyNode->data.identifier.name;
        *outResolvedTypeName = propertyNode->data.identifier.name;
        return *outLookupName != ZR_NULL;
    }

    if (propertyNode->type == ZR_AST_TYPE) {
        SZrType *propertyType = &propertyNode->data.type;
        SZrInferredType inferredType;

        if (propertyType->name == ZR_NULL) {
            return ZR_FALSE;
        }

        if (propertyType->name->type == ZR_AST_IDENTIFIER_LITERAL) {
            *outLookupName = propertyType->name->data.identifier.name;
        } else if (propertyType->name->type == ZR_AST_GENERIC_TYPE &&
                   propertyType->name->data.genericType.name != ZR_NULL) {
            *outLookupName = propertyType->name->data.genericType.name->name;
        }

        if (*outLookupName == ZR_NULL) {
            return ZR_FALSE;
        }

        ZrParser_InferredType_Init(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
        if (!ZrParser_AstTypeToInferredType_Convert(cs, propertyType, &inferredType)) {
            ZrParser_InferredType_Free(cs->state, &inferredType);
            return ZR_FALSE;
        }

        *outResolvedTypeName = inferredType.typeName;
        ZrParser_InferredType_Free(cs->state, &inferredType);
        return *outResolvedTypeName != ZR_NULL;
    }

    return ZR_FALSE;
}

static TZrBool resolve_primary_expression_root_type(SZrCompilerState *cs,
                                                    SZrPrimaryExpression *primary,
                                                    SZrString **outTypeName,
                                                    TZrBool *outIsTypeReference) {
    SZrString *currentTypeName = ZR_NULL;
    TZrBool currentIsTypeReference = ZR_FALSE;

    if (cs == ZR_NULL || primary == ZR_NULL || outTypeName == ZR_NULL || outIsTypeReference == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!resolve_expression_root_type(cs, primary->property, &currentTypeName, &currentIsTypeReference) ||
        currentTypeName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (primary->members != ZR_NULL) {
        for (TZrSize i = 0; i < primary->members->count; i++) {
            SZrAstNode *memberNode = primary->members->nodes[i];
            SZrMemberExpression *memberExpr;
            SZrString *memberLookupName;
            SZrString *memberResolvedTypeName;
            SZrTypeMemberInfo *memberInfo;
            SZrString *nextTypeName = ZR_NULL;
            SZrTypePrototypeInfo *currentTypeInfo = ZR_NULL;
            SZrTypePrototypeInfo *nextTypeInfo = ZR_NULL;

            if (memberNode == ZR_NULL) {
                continue;
            }
            if (memberNode->type != ZR_AST_MEMBER_EXPRESSION) {
                break;
            }

            memberExpr = &memberNode->data.memberExpression;
            if (memberExpr->computed || memberExpr->property == ZR_NULL || currentTypeName == ZR_NULL ||
                !resolve_member_segment_names(cs,
                                              memberExpr->property,
                                              &memberLookupName,
                                              &memberResolvedTypeName)) {
                break;
            }

            memberInfo = find_compiler_type_member(cs, currentTypeName, memberLookupName);
            if (memberInfo == ZR_NULL) {
                compile_expression_ensure_imported_module_runtime_metadata(cs, currentTypeName);
                memberInfo = find_compiler_type_member(cs, currentTypeName, memberLookupName);
            }
            if (memberInfo == ZR_NULL) {
                break;
            }

            currentTypeInfo = find_compiler_type_prototype(cs, currentTypeName);
            if ((memberInfo->memberType == ZR_AST_STRUCT_FIELD || memberInfo->memberType == ZR_AST_CLASS_FIELD) &&
                memberInfo->fieldTypeName != ZR_NULL) {
                if (memberExpr->property->type == ZR_AST_TYPE &&
                    currentTypeInfo != ZR_NULL &&
                    currentTypeInfo->type == ZR_OBJECT_PROTOTYPE_TYPE_MODULE &&
                    memberResolvedTypeName != ZR_NULL) {
                    nextTypeName = memberResolvedTypeName;
                } else {
                    nextTypeName = memberInfo->fieldTypeName;
                }
            } else if ((memberInfo->memberType == ZR_AST_STRUCT_METHOD ||
                        memberInfo->memberType == ZR_AST_CLASS_METHOD ||
                        memberInfo->memberType == ZR_AST_STRUCT_META_FUNCTION ||
                        memberInfo->memberType == ZR_AST_CLASS_META_FUNCTION) &&
                       memberInfo->returnTypeName != ZR_NULL) {
                nextTypeName = memberInfo->returnTypeName;
            }

            if (nextTypeName == ZR_NULL) {
                break;
            }

            currentTypeName = nextTypeName;
            nextTypeInfo = find_compiler_type_prototype(cs, currentTypeName);
            currentIsTypeReference =
                    nextTypeInfo != ZR_NULL && nextTypeInfo->type != ZR_OBJECT_PROTOTYPE_TYPE_MODULE;
        }
    }

    *outTypeName = currentTypeName;
    *outIsTypeReference = currentIsTypeReference;
    return currentTypeName != ZR_NULL;
}

TZrBool resolve_expression_root_type(SZrCompilerState *cs, SZrAstNode *node, SZrString **outTypeName,
                                          TZrBool *outIsTypeReference) {
    if (outTypeName == ZR_NULL || outIsTypeReference == ZR_NULL) {
        return ZR_FALSE;
    }

    *outTypeName = ZR_NULL;
    *outIsTypeReference = ZR_FALSE;
    if (cs == ZR_NULL || node == ZR_NULL) {
        return ZR_FALSE;
    }

    if (node->type == ZR_AST_IDENTIFIER_LITERAL) {
        SZrString *candidateType = node->data.identifier.name;
        if (find_compiler_type_prototype(cs, candidateType) != ZR_NULL) {
            *outTypeName = candidateType;
            *outIsTypeReference = ZR_TRUE;
            return ZR_TRUE;
        }
    }

    if (node->type == ZR_AST_PRIMARY_EXPRESSION) {
        return resolve_primary_expression_root_type(cs, &node->data.primaryExpression, outTypeName, outIsTypeReference);
    }

    *outTypeName = resolve_expression_type_name(cs, node);
    return *outTypeName != ZR_NULL;
}

SZrString *resolve_construct_target_type_name(SZrCompilerState *cs, SZrAstNode *target,
                                                     EZrObjectPrototypeType *outPrototypeType) {
    SZrString *typeName = ZR_NULL;
    TZrBool isTypeReference = ZR_FALSE;
    SZrTypePrototypeInfo *prototypeInfo = ZR_NULL;
    SZrAstNode *typeDecl = ZR_NULL;

    if (outPrototypeType != ZR_NULL) {
        *outPrototypeType = ZR_OBJECT_PROTOTYPE_TYPE_INVALID;
    }
    if (cs == ZR_NULL || target == ZR_NULL) {
        return ZR_NULL;
    }

    if (resolve_prototype_target_inference(cs, target, &prototypeInfo, &typeName)) {
        if (outPrototypeType != ZR_NULL && prototypeInfo != ZR_NULL) {
            *outPrototypeType = prototypeInfo->type;
        }
        return typeName;
    }
    if (cs->hasError) {
        return ZR_NULL;
    }

    if (target->type == ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION) {
        target = target->data.prototypeReferenceExpression.target;
        if (target == ZR_NULL) {
            return ZR_NULL;
        }
    }

    if (target->type == ZR_AST_TYPE) {
        SZrInferredType inferredType;
        ZrParser_InferredType_Init(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
        if (!ZrParser_AstTypeToInferredType_Convert(cs, &target->data.type, &inferredType)) {
            ZrParser_InferredType_Free(cs->state, &inferredType);
            return ZR_NULL;
        }
        typeName = inferredType.typeName;
        if (!ensure_generic_instance_type_prototype(cs, typeName) && cs->hasError) {
            ZrParser_InferredType_Free(cs->state, &inferredType);
            return ZR_NULL;
        }
        prototypeInfo = find_compiler_type_prototype(cs, typeName);
        ZrParser_InferredType_Free(cs->state, &inferredType);
        if (prototypeInfo != ZR_NULL) {
            if (outPrototypeType != ZR_NULL) {
                *outPrototypeType = prototypeInfo->type;
            }
            return typeName;
        }
    }

    if (!resolve_expression_root_type(cs, target, &typeName, &isTypeReference) || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    prototypeInfo = find_compiler_type_prototype(cs, typeName);
    if (prototypeInfo != ZR_NULL) {
        if (outPrototypeType != ZR_NULL) {
            *outPrototypeType = prototypeInfo->type;
        }
        return typeName;
    }

    typeDecl = find_type_declaration(cs, typeName);
    if (typeDecl != ZR_NULL) {
        if (outPrototypeType != ZR_NULL) {
            if (typeDecl->type == ZR_AST_STRUCT_DECLARATION) {
                *outPrototypeType = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT;
            } else if (typeDecl->type == ZR_AST_CLASS_DECLARATION) {
                *outPrototypeType = ZR_OBJECT_PROTOTYPE_TYPE_CLASS;
            }
        }
        return typeName;
    }

    return ZR_NULL;
}

static TZrBool emit_hidden_constructor_call(SZrCompilerState *cs,
                                            TZrUInt32 instanceSlot,
                                            SZrAstNodeArray *constructorArgs,
                                            SZrString *typeName,
                                            SZrFileRange location) {
    TZrUInt32 functionSlot;
    TZrUInt32 receiverSlot;
    TZrUInt32 argCount = 1;
    TZrUInt32 constructorMemberId;
    TZrBool syncStructReceiver = ZR_FALSE;

    if (cs == ZR_NULL || cs->hasError || typeName == ZR_NULL || !type_has_constructor(cs, typeName)) {
        return ZR_TRUE;
    }

    {
        SZrTypePrototypeInfo *prototypeInfo = find_compiler_type_prototype(cs, typeName);
        if (prototypeInfo != ZR_NULL) {
            syncStructReceiver = prototypeInfo->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT;
        } else {
            SZrAstNode *typeDecl = find_type_declaration(cs, typeName);
            syncStructReceiver = typeDecl != ZR_NULL && typeDecl->type == ZR_AST_STRUCT_DECLARATION;
        }
    }

    {
        SZrString *constructorName = ZrCore_String_CreateFromNative(cs->state, "__constructor");
        constructorMemberId = compiler_get_or_add_member_entry(cs, constructorName);
    }
    if (constructorMemberId == ZR_PARSER_MEMBER_ID_NONE) {
        return ZR_FALSE;
    }
    functionSlot = allocate_stack_slot(cs);
    emit_instruction(cs,
                     create_instruction_2(ZR_INSTRUCTION_ENUM(GET_MEMBER),
                                          (TZrUInt16)functionSlot,
                                          (TZrUInt16)instanceSlot,
                                          (TZrUInt16)constructorMemberId));

    receiverSlot = allocate_stack_slot(cs);
    emit_instruction(cs,
                     create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK),
                                          (TZrUInt16)receiverSlot,
                                          (TZrInt32)instanceSlot));

    if (constructorArgs != ZR_NULL) {
        for (TZrSize i = 0; i < constructorArgs->count; i++) {
            SZrAstNode *argNode = constructorArgs->nodes[i];
            if (argNode != ZR_NULL &&
                compile_expression_into_slot(cs, argNode, receiverSlot + 1 + (TZrUInt32)i) == ZR_PARSER_SLOT_NONE) {
                return ZR_FALSE;
            }
        }
        argCount += (TZrUInt32)constructorArgs->count;
    }

    emit_instruction(cs,
                     create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_CALL),
                                          (TZrUInt16)functionSlot,
                                          (TZrUInt16)functionSlot,
                                          (TZrUInt16)argCount));
    if (syncStructReceiver) {
        /* Struct constructors may mutate the bound receiver without returning it. */
        emit_instruction(cs,
                         create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK),
                                              (TZrUInt16)instanceSlot,
                                              (TZrInt32)receiverSlot));
    }
    collapse_stack_to_slot(cs, instanceSlot);
    if (cs->hasError) {
        ZrParser_Compiler_Error(cs, "Failed to invoke prototype constructor", location);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool emit_construct_seed_instance(SZrCompilerState *cs,
                                            TZrUInt32 destSlot,
                                            EZrObjectPrototypeType prototypeType,
                                            TZrUInt32 typeNameConstantIndex,
                                            SZrFileRange location) {
    if (cs == ZR_NULL || cs->hasError) {
        return ZR_FALSE;
    }

    emit_instruction(cs, create_instruction_0(ZR_INSTRUCTION_ENUM(CREATE_OBJECT), (TZrUInt16)destSlot));

    if (prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
        emit_instruction(cs,
                         create_instruction_2(ZR_INSTRUCTION_ENUM(TO_STRUCT),
                                              (TZrUInt16)destSlot,
                                              (TZrUInt16)destSlot,
                                              (TZrUInt16)typeNameConstantIndex));
        return ZR_TRUE;
    }

    if (prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_CLASS) {
        emit_instruction(cs,
                         create_instruction_2(ZR_INSTRUCTION_ENUM(TO_OBJECT),
                                              (TZrUInt16)destSlot,
                                              (TZrUInt16)destSlot,
                                              (TZrUInt16)typeNameConstantIndex));
        return ZR_TRUE;
    }

    if (prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_ENUM) {
        ZrParser_Compiler_Error(cs, "Enum construction is not implemented yet", location);
    } else {
        ZrParser_Compiler_Error(cs, "Unsupported construct target prototype kind", location);
    }
    return ZR_FALSE;
}

TZrUInt32 emit_shorthand_constructor_instance(SZrCompilerState *cs, const TZrChar *op, SZrString *typeName,
                                              SZrAstNodeArray *constructorArgs, SZrFileRange location) {
    TZrUInt32 destSlot;
    SZrTypePrototypeInfo *prototypeInfo;
    SZrAstNode *typeDecl;
    EZrObjectPrototypeType prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_INVALID;
    TZrBool allowValueConstruction = ZR_FALSE;
    TZrBool allowBoxedConstruction = ZR_FALSE;
    SZrTypeValue typeNameValue;
    TZrUInt32 typeNameConstantIndex;

    if (cs == ZR_NULL || op == ZR_NULL || typeName == ZR_NULL) {
        return ZR_PARSER_SLOT_NONE;
    }

    prototypeInfo = find_compiler_type_prototype(cs, typeName);
    if (prototypeInfo != ZR_NULL) {
        prototypeType = prototypeInfo->type;
        allowValueConstruction = prototypeInfo->allowValueConstruction;
        allowBoxedConstruction = prototypeInfo->allowBoxedConstruction;
    } else {
        typeDecl = find_type_declaration(cs, typeName);
        if (typeDecl != ZR_NULL) {
            if (typeDecl->type == ZR_AST_STRUCT_DECLARATION) {
                prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT;
            } else if (typeDecl->type == ZR_AST_CLASS_DECLARATION) {
                prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_CLASS;
            }
        }
        allowValueConstruction = prototypeType != ZR_OBJECT_PROTOTYPE_TYPE_INVALID &&
                                 prototypeType != ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE &&
                                 prototypeType != ZR_OBJECT_PROTOTYPE_TYPE_MODULE;
        allowBoxedConstruction = allowValueConstruction;
    }

    if (prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_INVALID ||
        prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE ||
        prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_MODULE) {
        ZrParser_Compiler_Error(cs, "Construct target must resolve to a registered constructible prototype", location);
        return ZR_PARSER_SLOT_NONE;
    }

    if (strcmp(op, "$") == 0 && !allowValueConstruction) {
        ZrParser_Compiler_Error(cs, "Prototype does not allow value construction", location);
        return ZR_PARSER_SLOT_NONE;
    }

    if (strcmp(op, "new") == 0 && !allowBoxedConstruction) {
        ZrParser_Compiler_Error(cs, "Prototype does not allow boxed construction", location);
        return ZR_PARSER_SLOT_NONE;
    }

    destSlot = allocate_stack_slot(cs);
    ZrCore_Value_InitAsRawObject(cs->state, &typeNameValue, ZR_CAST_RAW_OBJECT_AS_SUPER(typeName));
    typeNameValue.type = ZR_VALUE_TYPE_STRING;
    typeNameConstantIndex = add_constant(cs, &typeNameValue);

    if (prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_ENUM) {
        if (constructorArgs == ZR_NULL || constructorArgs->count != 1) {
            ZrParser_Compiler_Error(cs, "Enum construction requires exactly one underlying value argument", location);
            return ZR_PARSER_SLOT_NONE;
        }

        if (compile_expression_into_slot(cs, constructorArgs->nodes[0], destSlot) == ZR_PARSER_SLOT_NONE) {
            return ZR_PARSER_SLOT_NONE;
        }

        emit_instruction(cs,
                         create_instruction_2(ZR_INSTRUCTION_ENUM(TO_OBJECT),
                                              (TZrUInt16)destSlot,
                                              (TZrUInt16)destSlot,
                                              (TZrUInt16)typeNameConstantIndex));
        collapse_stack_to_slot(cs, destSlot);
        return destSlot;
    }

    if (!emit_construct_seed_instance(cs, destSlot, prototypeType, typeNameConstantIndex, location)) {
        return ZR_PARSER_SLOT_NONE;
    }

    if (!emit_hidden_constructor_call(cs, destSlot, constructorArgs, typeName, location)) {
        return ZR_PARSER_SLOT_NONE;
    }

    collapse_stack_to_slot(cs, destSlot);
    return destSlot;
}

SZrTypeMemberInfo *find_hidden_property_accessor_member(SZrCompilerState *cs, SZrString *typeName,
                                                               SZrString *propertyName, TZrBool isSetter) {
    if (cs == ZR_NULL || typeName == ZR_NULL || propertyName == ZR_NULL) {
        return ZR_NULL;
    }

    SZrString *accessorName = create_hidden_property_accessor_name(cs, propertyName, isSetter);
    if (accessorName == ZR_NULL) {
        return ZR_NULL;
    }

    return find_compiler_type_member(cs, typeName, accessorName);
}

TZrBool can_use_property_accessor(TZrBool rootIsTypeReference, SZrTypeMemberInfo *accessorMember) {
    if (accessorMember == ZR_NULL) {
        return ZR_FALSE;
    }

    if (rootIsTypeReference && !accessorMember->isStatic) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool member_call_requires_bound_receiver(const SZrTypeMemberInfo *memberInfo) {
    if (memberInfo == ZR_NULL || memberInfo->isStatic) {
        return ZR_FALSE;
    }

    return memberInfo->memberType == ZR_AST_STRUCT_METHOD || memberInfo->memberType == ZR_AST_CLASS_METHOD;
}

static TZrBool emit_argument_conversion_if_needed(SZrCompilerState *cs,
                                                  SZrAstNode *argNode,
                                                  TZrUInt32 argSlot,
                                                  const SZrInferredType *expectedType) {
    SZrInferredType actualType;
    EZrInstructionCode conversionOpcode;

    if (cs == ZR_NULL || argNode == ZR_NULL || expectedType == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(cs->state, &actualType, ZR_VALUE_TYPE_OBJECT);
    if (!ZrParser_ExpressionType_Infer(cs, argNode, &actualType)) {
        ZrParser_InferredType_Free(cs->state, &actualType);
        return ZR_FALSE;
    }

    conversionOpcode = ZrParser_InferredType_GetConversionOpcode(&actualType, expectedType);
    ZrParser_InferredType_Free(cs->state, &actualType);
    if (conversionOpcode == ZR_INSTRUCTION_ENUM(ENUM_MAX)) {
        return ZR_TRUE;
    }

    emit_type_conversion(cs, argSlot, argSlot, conversionOpcode);
    return ZR_TRUE;
}

static TZrBool compile_arguments_against_parameter_types(SZrCompilerState *cs,
                                                         SZrAstNodeArray *argsToCompile,
                                                         const SZrArray *parameterTypes,
                                                         const SZrArray *parameterPassingModes,
                                                         TZrUInt32 firstArgSlot) {
    if (cs == ZR_NULL || argsToCompile == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrSize index = 0; index < argsToCompile->count; index++) {
        SZrAstNode *argNode = argsToCompile->nodes[index];
        TZrUInt32 argSlot;
        SZrInferredType *expectedType;
        EZrParameterPassingMode *passingMode = ZR_NULL;

        if (argNode == ZR_NULL) {
            continue;
        }

        if (parameterPassingModes != ZR_NULL && index < parameterPassingModes->length) {
            passingMode = (EZrParameterPassingMode *)ZrCore_Array_Get((SZrArray *)parameterPassingModes, index);
            if (passingMode != ZR_NULL &&
                (*passingMode == ZR_PARAMETER_PASSING_MODE_OUT || *passingMode == ZR_PARAMETER_PASSING_MODE_REF) &&
                !compiler_expression_is_assignable_storage_location(argNode)) {
                ZrParser_Compiler_Error(cs,
                                        *passingMode == ZR_PARAMETER_PASSING_MODE_OUT ?
                                                "%out argument must be an assignable storage location" :
                                                "%ref argument must be an assignable storage location",
                                        argNode->location);
                return ZR_FALSE;
            }
        }

        argSlot = compile_expression_into_slot(cs, argNode, firstArgSlot + (TZrUInt32)index);
        if (argSlot == ZR_PARSER_SLOT_NONE || cs->hasError) {
            return ZR_FALSE;
        }

        if (parameterTypes == ZR_NULL || index >= parameterTypes->length) {
            continue;
        }

        expectedType = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)parameterTypes, index);
        if (expectedType != ZR_NULL &&
            !emit_argument_conversion_if_needed(cs, argNode, argSlot, expectedType)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static SZrAstNodeArray *member_call_parameter_list(const SZrTypeMemberInfo *memberInfo) {
    if (memberInfo == ZR_NULL || memberInfo->declarationNode == ZR_NULL) {
        return ZR_NULL;
    }

    switch (memberInfo->declarationNode->type) {
        case ZR_AST_STRUCT_METHOD:
            return memberInfo->declarationNode->data.structMethod.params;
        case ZR_AST_STRUCT_META_FUNCTION:
            return memberInfo->declarationNode->data.structMetaFunction.params;
        case ZR_AST_CLASS_METHOD:
            return memberInfo->declarationNode->data.classMethod.params;
        case ZR_AST_CLASS_META_FUNCTION:
            return memberInfo->declarationNode->data.classMetaFunction.params;
        default:
            return ZR_NULL;
    }
}

static SZrString *member_call_parameter_name_at(const SZrTypeMemberInfo *memberInfo, TZrSize index) {
    if (memberInfo == ZR_NULL || index >= memberInfo->parameterNames.length) {
        return ZR_NULL;
    }

    {
        SZrString **namePtr = (SZrString **)ZrCore_Array_Get((SZrArray *)&memberInfo->parameterNames, index);
        return namePtr != ZR_NULL ? *namePtr : ZR_NULL;
    }
}

static TZrBool member_call_parameter_has_default_at(const SZrTypeMemberInfo *memberInfo, TZrSize index) {
    if (memberInfo == ZR_NULL || index >= memberInfo->parameterHasDefaultValues.length) {
        return ZR_FALSE;
    }

    {
        TZrBool *hasDefaultPtr =
                (TZrBool *)ZrCore_Array_Get((SZrArray *)&memberInfo->parameterHasDefaultValues, index);
        return hasDefaultPtr != ZR_NULL ? *hasDefaultPtr : ZR_FALSE;
    }
}

#ifndef ZR_MEMBER_PARAMETER_COUNT_UNKNOWN
#define ZR_MEMBER_PARAMETER_COUNT_UNKNOWN ((TZrUInt32)-1)
#endif

static TZrUInt32 member_call_min_argument_count(const SZrTypeMemberInfo *memberInfo) {
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
        while (minArgumentCount > 0 &&
               member_call_parameter_has_default_at(memberInfo, (TZrSize)minArgumentCount - 1U)) {
            minArgumentCount--;
        }
        return minArgumentCount;
    }

    return memberInfo->parameterCount;
}

static const SZrTypeValue *member_call_parameter_default_value_at(const SZrTypeMemberInfo *memberInfo, TZrSize index) {
    if (memberInfo == ZR_NULL || index >= memberInfo->parameterDefaultValues.length) {
        return ZR_NULL;
    }

    return (const SZrTypeValue *)ZrCore_Array_Get((SZrArray *)&memberInfo->parameterDefaultValues, index);
}

static EZrParameterPassingMode member_call_parameter_passing_mode_at(const SZrArray *parameterPassingModes,
                                                                    TZrSize index) {
    if (parameterPassingModes == ZR_NULL || index >= parameterPassingModes->length) {
        return ZR_PARAMETER_PASSING_MODE_VALUE;
    }

    {
        EZrParameterPassingMode *passingMode =
                (EZrParameterPassingMode *)ZrCore_Array_Get((SZrArray *)parameterPassingModes, index);
        return passingMode != ZR_NULL ? *passingMode : ZR_PARAMETER_PASSING_MODE_VALUE;
    }
}

static TZrBool emit_default_constant_argument(SZrCompilerState *cs,
                                              TZrUInt32 targetSlot,
                                              const SZrTypeValue *defaultValue) {
    SZrTypeValue constantValue;
    TZrUInt32 constantIndex;

    if (cs == ZR_NULL || defaultValue == ZR_NULL) {
        return ZR_FALSE;
    }

    constantValue = *defaultValue;
    if (cs->stackSlotCount <= targetSlot) {
        cs->stackSlotCount = (TZrSize)targetSlot + 1;
        if (cs->maxStackSlotCount < cs->stackSlotCount) {
            cs->maxStackSlotCount = cs->stackSlotCount;
        }
    }
    constantIndex = add_constant(cs, &constantValue);
    emit_instruction(cs,
                     create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT),
                                          (TZrUInt16)targetSlot,
                                          (TZrInt32)constantIndex));
    return !cs->hasError;
}

static TZrBool compile_arguments_against_imported_member_metadata(SZrCompilerState *cs,
                                                                  SZrFunctionCall *call,
                                                                  const SZrTypeMemberInfo *memberInfo,
                                                                  const SZrResolvedCallSignature *resolvedSignature,
                                                                  SZrFileRange callLocation,
                                                                  TZrUInt32 firstArgSlot,
                                                                  TZrUInt32 *outArgCount) {
    const SZrArray *parameterTypes =
            resolvedSignature != ZR_NULL ? &resolvedSignature->parameterTypes
                                         : (memberInfo != ZR_NULL ? &memberInfo->parameterTypes : ZR_NULL);
    const SZrArray *parameterPassingModes =
            resolvedSignature != ZR_NULL ? &resolvedSignature->parameterPassingModes
                                         : (memberInfo != ZR_NULL ? &memberInfo->parameterPassingModes : ZR_NULL);
    TZrSize callArgCount = (call != ZR_NULL && call->args != ZR_NULL) ? call->args->count : 0;
    TZrSize slotCount = 0;
    TZrBool *provided = ZR_NULL;
    SZrAstNode **argumentNodes = ZR_NULL;
    TZrBool success = ZR_FALSE;
    TZrBool hasNamedArguments = ZR_FALSE;
    TZrBool requireTaskHandleArgument = ZR_FALSE;
    TZrUInt32 minArgumentCount = 0;
    TZrSize compiledSlotCount = 0;

    if (outArgCount != ZR_NULL) {
        *outArgCount = 0;
    }
    if (cs == ZR_NULL || memberInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    requireTaskHandleArgument = zr_string_equals_cstr(memberInfo->name, "__awaitTask");
    minArgumentCount = member_call_min_argument_count(memberInfo);

    if (memberInfo->parameterCount != ZR_MEMBER_PARAMETER_COUNT_UNKNOWN) {
        slotCount = memberInfo->parameterCount;
    }
    if (parameterTypes != ZR_NULL && parameterTypes->length > slotCount) {
        slotCount = parameterTypes->length;
    }
    if (memberInfo->parameterNames.length > slotCount) {
        slotCount = memberInfo->parameterNames.length;
    }
    if (memberInfo->parameterDefaultValues.length > slotCount) {
        slotCount = memberInfo->parameterDefaultValues.length;
    }
    if (memberInfo->parameterHasDefaultValues.length > slotCount) {
        slotCount = memberInfo->parameterHasDefaultValues.length;
    }

    if (call != ZR_NULL && call->argNames != ZR_NULL) {
        for (TZrSize index = 0; index < call->argNames->length && index < callArgCount; index++) {
            SZrString **namePtr = (SZrString **)ZrCore_Array_Get(call->argNames, index);
            if (namePtr != ZR_NULL && *namePtr != ZR_NULL) {
                hasNamedArguments = ZR_TRUE;
                break;
            }
        }
    }

    if (memberInfo->parameterCount == ZR_MEMBER_PARAMETER_COUNT_UNKNOWN) {
        if (hasNamedArguments && memberInfo->parameterNames.length == 0) {
            ZrParser_Compiler_Error(cs,
                                    "Imported member call does not expose parameter names for named arguments",
                                    callLocation);
            goto cleanup;
        }

        if (callArgCount > slotCount) {
            slotCount = callArgCount;
        }
    }

    if (slotCount == 0) {
        success = callArgCount == 0;
        goto cleanup;
    }

    provided = (TZrBool *)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                          sizeof(TZrBool) * slotCount,
                                                          ZR_MEMORY_NATIVE_TYPE_ARRAY);
    argumentNodes = (SZrAstNode **)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                   sizeof(SZrAstNode *) * slotCount,
                                                                   ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (provided == ZR_NULL || argumentNodes == ZR_NULL) {
        goto cleanup;
    }

    for (TZrSize index = 0; index < slotCount; index++) {
        provided[index] = ZR_FALSE;
        argumentNodes[index] = ZR_NULL;
    }

    if (call != ZR_NULL && call->argNames != ZR_NULL) {
        TZrSize positionalCount = 0;

        for (TZrSize index = 0; index < call->argNames->length && index < callArgCount; index++) {
            SZrString **namePtr = (SZrString **)ZrCore_Array_Get(call->argNames, index);
            if (namePtr != ZR_NULL && *namePtr == ZR_NULL) {
                positionalCount++;
                continue;
            }
            break;
        }

        if (positionalCount > slotCount) {
            ZrParser_Compiler_Error(cs, "Too many arguments for member call", callLocation);
            goto cleanup;
        }

        for (TZrSize index = 0; index < positionalCount; index++) {
            provided[index] = ZR_TRUE;
            argumentNodes[index] = call->args->nodes[index];
        }

        for (TZrSize index = positionalCount; index < callArgCount && index < call->argNames->length; index++) {
            SZrString **namePtr = (SZrString **)ZrCore_Array_Get(call->argNames, index);
            TZrBool matched = ZR_FALSE;

            if (namePtr == ZR_NULL || *namePtr == ZR_NULL) {
                ZrParser_Compiler_Error(cs, "Invalid member call argument shape", callLocation);
                goto cleanup;
            }

            for (TZrSize paramIndex = 0; paramIndex < slotCount; paramIndex++) {
                SZrString *paramName = member_call_parameter_name_at(memberInfo, paramIndex);
                if (paramName == ZR_NULL || !ZrCore_String_Equal(paramName, *namePtr)) {
                    continue;
                }

                if (provided[paramIndex]) {
                    ZrParser_Compiler_Error(cs, "Duplicate named member argument", call->args->nodes[index]->location);
                    goto cleanup;
                }

                provided[paramIndex] = ZR_TRUE;
                argumentNodes[paramIndex] = call->args->nodes[index];
                matched = ZR_TRUE;
                break;
            }

            if (!matched) {
                ZrParser_Compiler_Error(cs, "Unknown named member argument", call->args->nodes[index]->location);
                goto cleanup;
            }
        }
    } else if (callArgCount > 0) {
        if (callArgCount > slotCount) {
            ZrParser_Compiler_Error(cs, "Too many arguments for member call", callLocation);
            goto cleanup;
        }

        for (TZrSize index = 0; index < callArgCount; index++) {
            provided[index] = ZR_TRUE;
            argumentNodes[index] = call->args->nodes[index];
        }
    }

    compiledSlotCount = slotCount;
    while (compiledSlotCount > 0) {
        TZrSize trailingIndex = compiledSlotCount - 1U;
        EZrParameterPassingMode passingMode =
                member_call_parameter_passing_mode_at(parameterPassingModes, trailingIndex);
        if (argumentNodes[trailingIndex] != ZR_NULL ||
            member_call_parameter_has_default_at(memberInfo, trailingIndex) ||
            passingMode == ZR_PARAMETER_PASSING_MODE_OUT ||
            passingMode == ZR_PARAMETER_PASSING_MODE_REF ||
            trailingIndex < (TZrSize)minArgumentCount) {
            break;
        }
        compiledSlotCount--;
    }

    for (TZrSize index = 0; index < compiledSlotCount; index++) {
        SZrAstNode *argNode = argumentNodes[index];
        SZrInferredType *expectedType =
                (parameterTypes != ZR_NULL && index < parameterTypes->length)
                        ? (SZrInferredType *)ZrCore_Array_Get((SZrArray *)parameterTypes, index)
                        : ZR_NULL;
        EZrParameterPassingMode passingMode = member_call_parameter_passing_mode_at(parameterPassingModes, index);
        TZrUInt32 argSlot = firstArgSlot + (TZrUInt32)index;

        if (argNode != ZR_NULL) {
            if (requireTaskHandleArgument && index == 0) {
                SZrInferredType argType;

                ZrParser_InferredType_Init(cs->state, &argType, ZR_VALUE_TYPE_OBJECT);
                if (!ZrParser_ExpressionType_Infer(cs, argNode, &argType)) {
                    ZrParser_InferredType_Free(cs->state, &argType);
                    goto cleanup;
                }
                if (!compile_inferred_type_is_task_handle(&argType)) {
                    ZrParser_InferredType_Free(cs->state, &argType);
                    ZrParser_Compiler_Error(cs,
                                            "%await expects a zr.task.Task<T>; call .start() on the TaskRunner first",
                                            argNode->location);
                    goto cleanup;
                }
                ZrParser_InferredType_Free(cs->state, &argType);
            }

            if ((passingMode == ZR_PARAMETER_PASSING_MODE_OUT || passingMode == ZR_PARAMETER_PASSING_MODE_REF) &&
                !compiler_expression_is_assignable_storage_location(argNode)) {
                ZrParser_Compiler_Error(cs,
                                        passingMode == ZR_PARAMETER_PASSING_MODE_OUT
                                                ? "%out argument must be an assignable storage location"
                                                : "%ref argument must be an assignable storage location",
                                        argNode->location);
                goto cleanup;
            }

            if (compile_expression_into_slot(cs, argNode, argSlot) == ZR_PARSER_SLOT_NONE || cs->hasError) {
                goto cleanup;
            }
            if (expectedType != ZR_NULL && !emit_argument_conversion_if_needed(cs, argNode, argSlot, expectedType)) {
                goto cleanup;
            }
            continue;
        }

        if (passingMode == ZR_PARAMETER_PASSING_MODE_OUT || passingMode == ZR_PARAMETER_PASSING_MODE_REF) {
            ZrParser_Compiler_Error(cs,
                                    passingMode == ZR_PARAMETER_PASSING_MODE_OUT
                                            ? "Missing %out member argument"
                                            : "Missing %ref member argument",
                                    callLocation);
            goto cleanup;
        }

        if (member_call_parameter_has_default_at(memberInfo, index)) {
            const SZrTypeValue *defaultValue = member_call_parameter_default_value_at(memberInfo, index);
            if (defaultValue == ZR_NULL || !emit_default_constant_argument(cs, argSlot, defaultValue)) {
                goto cleanup;
            }
            continue;
        }

        ZrParser_Compiler_Error(cs, "Missing required member argument", callLocation);
        goto cleanup;
    }

    if (outArgCount != ZR_NULL) {
        *outArgCount = (TZrUInt32)compiledSlotCount;
    }
    success = ZR_TRUE;

cleanup:
    if (provided != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      provided,
                                      sizeof(TZrBool) * slotCount,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (argumentNodes != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      argumentNodes,
                                      sizeof(SZrAstNode *) * slotCount,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    return success;
}

static TZrBool compile_arguments_against_function_signature(SZrCompilerState *cs,
                                                            SZrAstNodeArray *argsToCompile,
                                                            const SZrFunctionTypeInfo *funcType,
                                                            TZrUInt32 firstArgSlot) {
    return compile_arguments_against_parameter_types(cs,
                                                     argsToCompile,
                                                     funcType != ZR_NULL ? &funcType->paramTypes : ZR_NULL,
                                                     funcType != ZR_NULL ? &funcType->parameterPassingModes : ZR_NULL,
                                                     firstArgSlot);
}

static TZrBool compile_arguments_against_member_signature(SZrCompilerState *cs,
                                                          SZrAstNodeArray *argsToCompile,
                                                          const SZrTypeMemberInfo *memberInfo,
                                                          TZrUInt32 firstArgSlot) {
    return compile_arguments_against_parameter_types(cs,
                                                     argsToCompile,
                                                     memberInfo != ZR_NULL ? &memberInfo->parameterTypes : ZR_NULL,
                                                     memberInfo != ZR_NULL ? &memberInfo->parameterPassingModes : ZR_NULL,
                                                     firstArgSlot);
}

static TZrBool compile_arguments_against_member_resolved_signature(SZrCompilerState *cs,
                                                                   SZrAstNodeArray *argsToCompile,
                                                                   const SZrResolvedCallSignature *resolvedSignature,
                                                                   TZrUInt32 firstArgSlot) {
    return compile_arguments_against_parameter_types(cs,
                                                     argsToCompile,
                                                     resolvedSignature != ZR_NULL ? &resolvedSignature->parameterTypes
                                                                                  : ZR_NULL,
                                                     resolvedSignature != ZR_NULL ? &resolvedSignature->parameterPassingModes
                                                                                  : ZR_NULL,
                                                     firstArgSlot);
}

static TZrBool compile_arguments_against_function_resolved_signature(SZrCompilerState *cs,
                                                                     SZrAstNodeArray *argsToCompile,
                                                                     const SZrResolvedCallSignature *resolvedSignature,
                                                                     TZrUInt32 firstArgSlot) {
    return compile_arguments_against_parameter_types(cs,
                                                     argsToCompile,
                                                     resolvedSignature != ZR_NULL ? &resolvedSignature->parameterTypes
                                                                                  : ZR_NULL,
                                                     resolvedSignature != ZR_NULL ? &resolvedSignature->parameterPassingModes
                                                                                  : ZR_NULL,
                                                     firstArgSlot);
}

void compile_primary_member_chain(SZrCompilerState *cs, SZrAstNode *propertyNode, SZrAstNodeArray *members,
                                         TZrSize memberStartIndex, TZrUInt32 *ioCurrentSlot,
                                         SZrString **ioRootTypeName, TZrBool *ioRootIsTypeReference,
                                         EZrOwnershipQualifier *ioRootOwnershipQualifier) {
    TZrUInt32 currentSlot;
    TZrUInt32 pendingReceiverSlot = ZR_PARSER_SLOT_NONE;
    SZrString *pendingCallResultTypeName = ZR_NULL;
    const SZrTypeMemberInfo *pendingCallMemberInfo = ZR_NULL;
    SZrString *rootTypeName = ioRootTypeName != ZR_NULL ? *ioRootTypeName : ZR_NULL;
    TZrBool rootIsTypeReference = ioRootIsTypeReference != ZR_NULL ? *ioRootIsTypeReference : ZR_FALSE;
    EZrOwnershipQualifier rootOwnershipQualifier =
            ioRootOwnershipQualifier != ZR_NULL ? *ioRootOwnershipQualifier : ZR_OWNERSHIP_QUALIFIER_NONE;

    if (cs == ZR_NULL || ioCurrentSlot == ZR_NULL || members == ZR_NULL || cs->hasError) {
        return;
    }

    currentSlot = *ioCurrentSlot;
    for (TZrSize i = memberStartIndex; i < members->count; i++) {
        SZrAstNode *member = members->nodes[i];
        if (member == ZR_NULL) {
            continue;
        }

        if (member->type == ZR_AST_MEMBER_EXPRESSION) {
            SZrMemberExpression *memberExpr = &member->data.memberExpression;
            SZrString *memberName = ZR_NULL;
            TZrBool isStaticMember = ZR_FALSE;
            TZrBool bindReceiverForCall = ZR_FALSE;
            SZrTypeMemberInfo *typeMember = ZR_NULL;
            SZrTypeMemberInfo *getterAccessor = ZR_NULL;
            TZrBool nextIsFunctionCall =
                    (i + 1 < members->count &&
                     members->nodes[i + 1] != ZR_NULL &&
                     members->nodes[i + 1]->type == ZR_AST_FUNCTION_CALL);

            if (!memberExpr->computed && memberExpr->property != ZR_NULL &&
                memberExpr->property->type == ZR_AST_IDENTIFIER_LITERAL) {
                memberName = memberExpr->property->data.identifier.name;
            }

            if (rootTypeName != ZR_NULL && memberName != ZR_NULL) {
                typeMember = find_compiler_type_member(cs, rootTypeName, memberName);
                if (typeMember != ZR_NULL && typeMember->isStatic) {
                    isStaticMember = ZR_TRUE;
                }
                bindReceiverForCall = member_call_requires_bound_receiver(typeMember);
                pendingCallResultTypeName =
                        nextIsFunctionCall && typeMember != ZR_NULL ? typeMember->returnTypeName : ZR_NULL;
                pendingCallMemberInfo = nextIsFunctionCall ? typeMember : ZR_NULL;

                getterAccessor = find_hidden_property_accessor_member(cs, rootTypeName, memberName, ZR_FALSE);
                if (!can_use_property_accessor(rootIsTypeReference, getterAccessor)) {
                    getterAccessor = ZR_NULL;
                }
            }

            if (memberExpr->property != ZR_NULL) {
                if (getterAccessor != ZR_NULL && memberName != ZR_NULL && !memberExpr->computed) {
                    if (!emit_property_getter_call(cs, currentSlot, memberName, getterAccessor->isStatic,
                                                   member->location)) {
                        return;
                    }
                    pendingReceiverSlot = ZR_PARSER_SLOT_NONE;
                    rootTypeName = getterAccessor->returnTypeName;
                    rootIsTypeReference = getterAccessor->isStatic &&
                                          rootTypeName != ZR_NULL &&
                                          type_name_is_registered_prototype(cs, rootTypeName);
                    rootOwnershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
                } else {
                    if (nextIsFunctionCall && typeMember != ZR_NULL && !typeMember->isStatic &&
                        !receiver_ownership_can_call_member_local(rootOwnershipQualifier,
                                                                  typeMember->receiverQualifier)) {
                        ZrParser_Compiler_Error(cs,
                                                receiver_ownership_call_error_local(rootOwnershipQualifier),
                                                members->nodes[i + 1]->location);
                        return;
                    }

                    if (nextIsFunctionCall && bindReceiverForCall) {
                        pendingReceiverSlot = allocate_stack_slot(cs);
                        emit_instruction(cs, create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK),
                                                                  (TZrUInt16)pendingReceiverSlot,
                                                                  (TZrInt32)currentSlot));
                    } else {
                        pendingReceiverSlot = ZR_PARSER_SLOT_NONE;
                    }

                    if (!memberExpr->computed) {
                        SZrString *memberSymbol = resolve_member_expression_symbol(cs, memberExpr);
                        TZrUInt32 memberId = compiler_get_or_add_member_entry(cs, memberSymbol);
                        if (memberId == ZR_PARSER_MEMBER_ID_NONE) {
                            ZrParser_Compiler_Error(cs, "Failed to register member access symbol", member->location);
                            return;
                        }

                        emit_instruction(cs,
                                         create_instruction_2(ZR_INSTRUCTION_ENUM(GET_MEMBER),
                                                              (TZrUInt16)currentSlot,
                                                              (TZrUInt16)currentSlot,
                                                              (TZrUInt16)memberId));
                    } else {
                        TZrUInt32 keyTargetSlot =
                                (pendingReceiverSlot != ZR_PARSER_SLOT_NONE) ? currentSlot + 2 : currentSlot + 1;
                        TZrUInt32 keySlot = compile_member_key_into_slot(cs, memberExpr, keyTargetSlot);
                        if (keySlot == ZR_PARSER_SLOT_NONE) {
                            return;
                        }

                        emit_instruction(cs,
                                         create_instruction_2(ZR_INSTRUCTION_ENUM(GET_BY_INDEX),
                                                              (TZrUInt16)currentSlot,
                                                              (TZrUInt16)currentSlot,
                                                              (TZrUInt16)keySlot));
                        if (pendingReceiverSlot == ZR_PARSER_SLOT_NONE) {
                            collapse_stack_to_slot(cs, currentSlot);
                        }
                    }

                    if (typeMember != ZR_NULL &&
                        (typeMember->memberType == ZR_AST_STRUCT_FIELD ||
                         typeMember->memberType == ZR_AST_CLASS_FIELD)) {
                        rootTypeName = typeMember->fieldTypeName;
                        rootOwnershipQualifier = typeMember->ownershipQualifier;
                        rootIsTypeReference = isStaticMember &&
                                              rootTypeName != ZR_NULL &&
                                              type_name_is_registered_prototype(cs, rootTypeName);
                    } else if (typeMember != ZR_NULL) {
                        rootTypeName = ZR_NULL;
                        rootOwnershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
                        rootIsTypeReference = ZR_FALSE;
                    } else if (!isStaticMember) {
                        rootTypeName = ZR_NULL;
                        rootOwnershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
                        rootIsTypeReference = ZR_FALSE;
                    }
                }
            }
        } else if (member->type == ZR_AST_FUNCTION_CALL) {
            SZrFunctionCall *call = &member->data.functionCall;
            SZrAstNodeArray *argsToCompile = call->args;
            SZrFunctionTypeInfo *resolvedFunctionType = ZR_NULL;
            const SZrTypeMemberInfo *activeCallMemberInfo = pendingCallMemberInfo;
            SZrAstNodeArray *memberParamList = ZR_NULL;
            SZrResolvedCallSignature resolvedFunctionSignature;
            SZrResolvedCallSignature resolvedMemberSignature;
            TZrBool hasResolvedFunctionSignature = ZR_FALSE;
            TZrBool hasResolvedMemberSignature = ZR_FALSE;
            TZrBool useMetaCallOpcode = ZR_FALSE;

            memset(&resolvedFunctionSignature, 0, sizeof(resolvedFunctionSignature));
            ZrParser_InferredType_Init(cs->state, &resolvedFunctionSignature.returnType, ZR_VALUE_TYPE_OBJECT);
            ZrCore_Array_Construct(&resolvedFunctionSignature.parameterTypes);
            ZrCore_Array_Construct(&resolvedFunctionSignature.parameterPassingModes);
            memset(&resolvedMemberSignature, 0, sizeof(resolvedMemberSignature));
            ZrParser_InferredType_Init(cs->state, &resolvedMemberSignature.returnType, ZR_VALUE_TYPE_OBJECT);
            ZrCore_Array_Construct(&resolvedMemberSignature.parameterTypes);
            ZrCore_Array_Construct(&resolvedMemberSignature.parameterPassingModes);

            if (rootIsTypeReference) {
                ZrParser_Compiler_Error(cs,
                                        "Prototype references are not callable; use $target(...) or new target(...)",
                                        member->location);
                free_resolved_call_signature(cs->state, &resolvedFunctionSignature);
                free_resolved_call_signature(cs->state, &resolvedMemberSignature);
                return;
            }

            if (i == memberStartIndex && propertyNode != ZR_NULL && propertyNode->type == ZR_AST_IDENTIFIER_LITERAL) {
                SZrString *funcName = propertyNode->data.identifier.name;
                TZrUInt32 childFuncIndex = find_child_function_index(cs, funcName);

                if (childFuncIndex != ZR_PARSER_INDEX_NONE) {
                    ZR_UNUSED_PARAMETER(ZrCore_Array_Get(&cs->childFunctions, childFuncIndex));
                }

                {
                    SZrAstNode *funcDecl = find_function_declaration(cs, funcName);
                    if (funcDecl != ZR_NULL && funcDecl->type == ZR_AST_FUNCTION_DECLARATION) {
                        SZrFunctionDeclaration *funcDeclData = &funcDecl->data.functionDeclaration;
                        if (funcDeclData->params != ZR_NULL) {
                            argsToCompile = match_named_arguments(cs, call, funcDeclData->params);
                        }
                    }
                }

                if (cs->typeEnv != ZR_NULL &&
                    resolve_best_function_overload(cs,
                                                   cs->typeEnv,
                                                   funcName,
                                                   call,
                                                   member->location,
                                                   &resolvedFunctionType,
                                                   &resolvedFunctionSignature)) {
                    hasResolvedFunctionSignature = ZR_TRUE;
                    ZR_UNUSED_PARAMETER(resolvedFunctionType);
                } else if (cs->compileTimeTypeEnv != ZR_NULL) {
                    hasResolvedFunctionSignature =
                            resolve_best_function_overload(cs,
                                                           cs->compileTimeTypeEnv,
                                                           funcName,
                                                           call,
                                                           member->location,
                                                           &resolvedFunctionType,
                                                           &resolvedFunctionSignature);
                }
            }

            if (activeCallMemberInfo == ZR_NULL &&
                !rootIsTypeReference && rootTypeName != ZR_NULL) {
                activeCallMemberInfo = find_compiler_type_meta_member(cs, rootTypeName, ZR_META_CALL);
                useMetaCallOpcode = activeCallMemberInfo != ZR_NULL;
            }

            if (activeCallMemberInfo != ZR_NULL) {
                memberParamList = member_call_parameter_list(activeCallMemberInfo);
                if (memberParamList != ZR_NULL) {
                    argsToCompile = match_named_arguments(cs, call, memberParamList);
                }
                hasResolvedMemberSignature = resolve_generic_member_call_signature(cs,
                                                                                   activeCallMemberInfo,
                                                                                   call,
                                                                                   &resolvedMemberSignature);
                if (!hasResolvedMemberSignature && cs->hasError) {
                    if (argsToCompile != call->args && argsToCompile != ZR_NULL) {
                        ZrParser_AstNodeArray_Free(cs->state, argsToCompile);
                    }
                    free_resolved_call_signature(cs->state, &resolvedFunctionSignature);
                    free_resolved_call_signature(cs->state, &resolvedMemberSignature);
                    return;
                }
            }

            TZrUInt32 argCount = 0;
            TZrUInt32 argBaseSlot = currentSlot + 1;
            TZrUInt32 compiledMemberArgCount = 0;
            if (pendingReceiverSlot != ZR_PARSER_SLOT_NONE) {
                argCount = 1;
                argBaseSlot = pendingReceiverSlot + 1;
            }

            if (argsToCompile != ZR_NULL) {
                if (activeCallMemberInfo != ZR_NULL) {
                    if (argsToCompile != call->args) {
                        compiledMemberArgCount = (TZrUInt32)argsToCompile->count;
                        if (!(hasResolvedMemberSignature
                                      ? compile_arguments_against_member_resolved_signature(cs,
                                                                                            argsToCompile,
                                                                                            &resolvedMemberSignature,
                                                                                            argBaseSlot)
                                      : compile_arguments_against_member_signature(cs,
                                                                                   argsToCompile,
                                                                                   activeCallMemberInfo,
                                                                                   argBaseSlot))) {
                            if (argsToCompile != call->args && argsToCompile != ZR_NULL) {
                                ZrParser_AstNodeArray_Free(cs->state, argsToCompile);
                            }
                            free_resolved_call_signature(cs->state, &resolvedFunctionSignature);
                            free_resolved_call_signature(cs->state, &resolvedMemberSignature);
                            return;
                        }
                    } else if (!compile_arguments_against_imported_member_metadata(cs,
                                                                                   call,
                                                                                   activeCallMemberInfo,
                                                                                   hasResolvedMemberSignature
                                                                                           ? &resolvedMemberSignature
                                                                                           : ZR_NULL,
                                                                                   member->location,
                                                                                   argBaseSlot,
                                                                                   &compiledMemberArgCount)) {
                        if (argsToCompile != call->args && argsToCompile != ZR_NULL) {
                            ZrParser_AstNodeArray_Free(cs->state, argsToCompile);
                        }
                        free_resolved_call_signature(cs->state, &resolvedFunctionSignature);
                        free_resolved_call_signature(cs->state, &resolvedMemberSignature);
                        return;
                    }
                    argCount += compiledMemberArgCount;
                } else if (resolvedFunctionType != ZR_NULL) {
                    if (!(hasResolvedFunctionSignature
                                  ? compile_arguments_against_function_resolved_signature(cs,
                                                                                          argsToCompile,
                                                                                          &resolvedFunctionSignature,
                                                                                          argBaseSlot)
                                  : compile_arguments_against_function_signature(cs,
                                                                                 argsToCompile,
                                                                                 resolvedFunctionType,
                                                                                 argBaseSlot))) {
                        if (argsToCompile != call->args && argsToCompile != ZR_NULL) {
                            ZrParser_AstNodeArray_Free(cs->state, argsToCompile);
                        }
                        free_resolved_call_signature(cs->state, &resolvedFunctionSignature);
                        free_resolved_call_signature(cs->state, &resolvedMemberSignature);
                        return;
                    }
                    argCount += (TZrUInt32)argsToCompile->count;
                } else {
                    for (TZrSize j = 0; j < argsToCompile->count; j++) {
                        SZrAstNode *argNode = argsToCompile->nodes[j];
                        if (argNode != ZR_NULL) {
                            TZrUInt32 argSlot =
                                    compile_expression_into_slot(cs, argNode, argBaseSlot + (TZrUInt32)j);
                            if (argSlot == ZR_PARSER_SLOT_NONE || cs->hasError) {
                                if (argsToCompile != call->args && argsToCompile != ZR_NULL) {
                                    ZrParser_AstNodeArray_Free(cs->state, argsToCompile);
                                }
                                free_resolved_call_signature(cs->state, &resolvedFunctionSignature);
                                free_resolved_call_signature(cs->state, &resolvedMemberSignature);
                                return;
                            }
                        }
                    }
                    argCount += (TZrUInt32)argsToCompile->count;
                }
            }

            {
                TZrBool useDynamicCallOpcode =
                        activeCallMemberInfo == ZR_NULL &&
                        resolvedFunctionType == ZR_NULL &&
                        !hasResolvedFunctionSignature;
                EZrInstructionCode callOpcode =
                        cs->isInTailCallContext
                                ? (useMetaCallOpcode
                                           ? ZR_INSTRUCTION_ENUM(META_TAIL_CALL)
                                           : (useDynamicCallOpcode
                                                      ? ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL)
                                                      : ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL)))
                                : (useMetaCallOpcode
                                           ? ZR_INSTRUCTION_ENUM(META_CALL)
                                           : (useDynamicCallOpcode
                                                      ? ZR_INSTRUCTION_ENUM(DYN_CALL)
                                                      : ZR_INSTRUCTION_ENUM(FUNCTION_CALL)));
                emit_instruction(cs,
                                 create_instruction_2(callOpcode,
                                                      (TZrUInt16)currentSlot,
                                                      (TZrUInt16)currentSlot,
                                                      (TZrUInt16)argCount));
            }
            collapse_stack_to_slot(cs, currentSlot);
            pendingReceiverSlot = ZR_PARSER_SLOT_NONE;
            if (hasResolvedMemberSignature) {
                rootTypeName = get_type_name_from_inferred_type(cs, &resolvedMemberSignature.returnType);
            } else if (hasResolvedFunctionSignature) {
                rootTypeName = get_type_name_from_inferred_type(cs, &resolvedFunctionSignature.returnType);
            } else {
                rootTypeName = activeCallMemberInfo != ZR_NULL ? activeCallMemberInfo->returnTypeName : pendingCallResultTypeName;
            }
            rootOwnershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
            pendingCallResultTypeName = ZR_NULL;
            pendingCallMemberInfo = ZR_NULL;
            rootIsTypeReference = ZR_FALSE;

            if (argsToCompile != call->args && argsToCompile != ZR_NULL) {
                ZrParser_AstNodeArray_Free(cs->state, argsToCompile);
            }
            free_resolved_call_signature(cs->state, &resolvedFunctionSignature);
            free_resolved_call_signature(cs->state, &resolvedMemberSignature);
        }
    }

    *ioCurrentSlot = currentSlot;
    if (ioRootTypeName != ZR_NULL) {
        *ioRootTypeName = rootTypeName;
    }
    if (ioRootIsTypeReference != ZR_NULL) {
        *ioRootIsTypeReference = rootIsTypeReference;
    }
    if (ioRootOwnershipQualifier != ZR_NULL) {
        *ioRootOwnershipQualifier = rootOwnershipQualifier;
    }
}


