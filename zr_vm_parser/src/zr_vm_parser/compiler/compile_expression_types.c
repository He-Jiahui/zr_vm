//
// Created by Auto on 2025/01/XX.
//

#include "compile_expression_internal.h"
#include "type_inference_internal.h"

static SZrTypePrototypeInfo *find_registered_type_prototype_exact_only(SZrCompilerState *cs,
                                                                       SZrString *typeName) {
    if (cs == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < cs->typePrototypes.length; index++) {
        SZrTypePrototypeInfo *info = (SZrTypePrototypeInfo *)ZrCore_Array_Get(&cs->typePrototypes, index);
        if (info != ZR_NULL && info->name != ZR_NULL && ZrCore_String_Equal(info->name, typeName)) {
            return info;
        }
    }

    if (cs->currentTypePrototypeInfo != ZR_NULL &&
        cs->currentTypePrototypeInfo->name != ZR_NULL &&
        ZrCore_String_Equal(cs->currentTypePrototypeInfo->name, typeName)) {
        return cs->currentTypePrototypeInfo;
    }

    return ZR_NULL;
}

static const TZrChar *compile_expression_find_top_level_last_dot(const TZrChar *typeNameText) {
    const TZrChar *lastDot = ZR_NULL;
    TZrInt32 genericDepth = 0;

    if (typeNameText == ZR_NULL) {
        return ZR_NULL;
    }

    for (const TZrChar *cursor = typeNameText; *cursor != '\0'; cursor++) {
        if (*cursor == '<') {
            genericDepth++;
            continue;
        }
        if (*cursor == '>') {
            if (genericDepth > 0) {
                genericDepth--;
            }
            continue;
        }
        if (*cursor == '.' && genericDepth == 0) {
            lastDot = cursor;
        }
    }

    return lastDot;
}

static SZrString *compile_expression_create_trimmed_type_name_segment(SZrCompilerState *cs,
                                                                      const TZrChar *source,
                                                                      TZrSize start,
                                                                      TZrSize end) {
    if (cs == ZR_NULL || source == ZR_NULL || end <= start) {
        return ZR_NULL;
    }

    while (start < end && (source[start] == ' ' || source[start] == '\t' || source[start] == '\r' || source[start] == '\n')) {
        start++;
    }
    while (end > start &&
           (source[end - 1] == ' ' || source[end - 1] == '\t' || source[end - 1] == '\r' || source[end - 1] == '\n')) {
        end--;
    }

    if (end <= start) {
        return ZR_NULL;
    }

    return ZrCore_String_Create(cs->state, (TZrNativeString)(source + start), end - start);
}

static SZrString *compile_expression_extract_member_lookup_name_from_type_text(SZrCompilerState *cs,
                                                                               const TZrChar *memberTypeText) {
    TZrSize memberTypeLength;
    TZrSize rootEnd;

    if (cs == ZR_NULL || memberTypeText == ZR_NULL || memberTypeText[0] == '\0') {
        return ZR_NULL;
    }

    memberTypeLength = strlen(memberTypeText);
    rootEnd = memberTypeLength;
    for (TZrSize index = 0; index < memberTypeLength; index++) {
        if (memberTypeText[index] == '<') {
            rootEnd = index;
            break;
        }
    }

    return compile_expression_create_trimmed_type_name_segment(cs, memberTypeText, 0, rootEnd);
}

static SZrString *compile_expression_build_canonical_module_member_type_name(SZrCompilerState *cs,
                                                                             SZrString *canonicalMemberTypeName,
                                                                             SZrString *requestedMemberTypeName) {
    const TZrChar *canonicalText;
    const TZrChar *requestedText;
    const TZrChar *requestedGenericStart;
    const TZrChar *canonicalGenericStart;
    TZrSize canonicalBaseLength;
    TZrChar buffer[ZR_PARSER_TYPE_NAME_BUFFER_LENGTH];
    TZrInt32 written;

    if (cs == ZR_NULL || canonicalMemberTypeName == ZR_NULL) {
        return ZR_NULL;
    }

    canonicalText = ZrCore_String_GetNativeString(canonicalMemberTypeName);
    if (canonicalText == ZR_NULL) {
        return ZR_NULL;
    }

    requestedText = requestedMemberTypeName != ZR_NULL ? ZrCore_String_GetNativeString(requestedMemberTypeName) : ZR_NULL;
    requestedGenericStart = requestedText != ZR_NULL ? strchr(requestedText, '<') : ZR_NULL;
    if (requestedGenericStart == ZR_NULL) {
        return canonicalMemberTypeName;
    }

    canonicalGenericStart = strchr(canonicalText, '<');
    canonicalBaseLength = canonicalGenericStart != ZR_NULL ? (TZrSize)(canonicalGenericStart - canonicalText)
                                                           : strlen(canonicalText);
    written = snprintf(buffer,
                       sizeof(buffer),
                       "%.*s%s",
                       (int)canonicalBaseLength,
                       canonicalText,
                       requestedGenericStart);
    if (written <= 0 || (TZrSize)written >= sizeof(buffer)) {
        return ZR_NULL;
    }

    return ZrCore_String_Create(cs->state, buffer, (TZrSize)written);
}

static TZrBool compile_inferred_type_is_task_handle(SZrCompilerState *cs, const SZrInferredType *type) {
    if (cs == ZR_NULL || type == ZR_NULL) {
        return ZR_FALSE;
    }

    return inferred_type_implements_protocol_mask(cs,
                                                  type,
                                                  ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_TASK_HANDLE));
}

static void compile_expression_ensure_imported_module_runtime_metadata(SZrCompilerState *cs, SZrString *moduleName) {
    if (cs == ZR_NULL || moduleName == ZR_NULL) {
        return;
    }

    ensure_builtin_reflection_compile_type(cs, moduleName);
    if (find_compiler_type_prototype(cs, moduleName) == ZR_NULL) {
        ensure_import_module_compile_info(cs, moduleName);
    }
}

static SZrString *compile_expression_build_qualified_module_member_field_type_name(SZrCompilerState *cs,
                                                                                   SZrString *moduleTypeName,
                                                                                   SZrString *memberTypeName) {
    TZrNativeString moduleTypeText;
    TZrNativeString memberTypeText;
    const TZrChar *memberGenericStart;
    const TZrChar *memberRootDot;
    TZrChar buffer[ZR_PARSER_TYPE_NAME_BUFFER_LENGTH];
    TZrInt32 written;

    if (cs == ZR_NULL || moduleTypeName == ZR_NULL || memberTypeName == ZR_NULL) {
        return ZR_NULL;
    }

    moduleTypeText = ZrCore_String_GetNativeString(moduleTypeName);
    memberTypeText = ZrCore_String_GetNativeString(memberTypeName);
    memberGenericStart = memberTypeText != ZR_NULL ? strchr(memberTypeText, '<') : ZR_NULL;
    memberRootDot = memberTypeText != ZR_NULL
                            ? memchr(memberTypeText,
                                     '.',
                                     memberGenericStart != ZR_NULL ? (size_t)(memberGenericStart - memberTypeText)
                                                                   : strlen(memberTypeText))
                            : ZR_NULL;
    if (moduleTypeText == ZR_NULL || memberTypeText == ZR_NULL || memberRootDot != ZR_NULL) {
        return ZR_NULL;
    }

    written = snprintf(buffer, sizeof(buffer), "%s.%s", moduleTypeText, memberTypeText);
    if (written <= 0 || (TZrSize)written >= sizeof(buffer)) {
        return ZR_NULL;
    }

    return ZrCore_String_Create(cs->state, buffer, (TZrSize)written);
}

static SZrAstNode *find_type_declaration_in_array(SZrAstNodeArray *declarations, SZrString *typeName) {
    if (declarations == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < declarations->count; index++) {
        SZrAstNode *declaration = declarations->nodes[index];

        if (declaration == ZR_NULL) {
            continue;
        }

        switch (declaration->type) {
            case ZR_AST_STRUCT_DECLARATION:
                if (declaration->data.structDeclaration.name != ZR_NULL &&
                    declaration->data.structDeclaration.name->name != ZR_NULL &&
                    ZrCore_String_Equal(declaration->data.structDeclaration.name->name, typeName)) {
                    return declaration;
                }
                break;

            case ZR_AST_CLASS_DECLARATION:
                if (declaration->data.classDeclaration.name != ZR_NULL &&
                    declaration->data.classDeclaration.name->name != ZR_NULL &&
                    ZrCore_String_Equal(declaration->data.classDeclaration.name->name, typeName)) {
                    return declaration;
                }
                break;

            case ZR_AST_INTERFACE_DECLARATION:
                if (declaration->data.interfaceDeclaration.name != ZR_NULL &&
                    declaration->data.interfaceDeclaration.name->name != ZR_NULL &&
                    ZrCore_String_Equal(declaration->data.interfaceDeclaration.name->name, typeName)) {
                    return declaration;
                }
                break;

            case ZR_AST_ENUM_DECLARATION:
                if (declaration->data.enumDeclaration.name != ZR_NULL &&
                    declaration->data.enumDeclaration.name->name != ZR_NULL &&
                    ZrCore_String_Equal(declaration->data.enumDeclaration.name->name, typeName)) {
                    return declaration;
                }
                break;

            case ZR_AST_EXTERN_DELEGATE_DECLARATION:
                if (declaration->data.externDelegateDeclaration.name != ZR_NULL &&
                    declaration->data.externDelegateDeclaration.name->name != ZR_NULL &&
                    ZrCore_String_Equal(declaration->data.externDelegateDeclaration.name->name, typeName)) {
                    return declaration;
                }
                break;

            default:
                break;
        }
    }

    return ZR_NULL;
}

SZrAstNode *find_type_declaration(SZrCompilerState *cs, SZrString *typeName) {
    SZrAstNode *match;

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

    match = find_type_declaration_in_array(script->statements, typeName);
    if (match != ZR_NULL) {
        return match;
    }

    for (TZrSize index = 0; index < script->statements->count; index++) {
        SZrAstNode *statement = script->statements->nodes[index];

        if (statement == ZR_NULL) {
            continue;
        }

        if (statement->type == ZR_AST_EXTERN_BLOCK &&
            statement->data.externBlock.declarations != ZR_NULL) {
            match = find_type_declaration_in_array(statement->data.externBlock.declarations, typeName);
            if (match != ZR_NULL) {
                return match;
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
    const TZrChar *typeNameText;
    const TZrChar *lastDot;

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

    for (TZrSize index = 0; index < cs->typeValueAliases.length; index++) {
        SZrTypeBinding *binding = (SZrTypeBinding *)ZrCore_Array_Get(&cs->typeValueAliases, index);
        if (binding != ZR_NULL &&
            binding->name != ZR_NULL &&
            binding->type.typeName != ZR_NULL &&
            ZrCore_String_Equal(binding->name, typeName) &&
            !ZrCore_String_Equal(binding->type.typeName, typeName)) {
            return find_compiler_type_prototype(cs, binding->type.typeName);
        }
    }

    if (ensure_generic_instance_type_prototype(cs, typeName)) {
        for (TZrSize i = 0; i < cs->typePrototypes.length; i++) {
            info = (SZrTypePrototypeInfo *)ZrCore_Array_Get(&cs->typePrototypes, i);
            if (info != ZR_NULL && info->name != ZR_NULL && ZrCore_String_Equal(info->name, typeName)) {
                return info;
            }
        }
    }

    typeNameText = ZrCore_String_GetNativeString(typeName);
    lastDot = compile_expression_find_top_level_last_dot(typeNameText);
    if (lastDot != ZR_NULL && lastDot > typeNameText && lastDot[1] != '\0') {
        SZrString *moduleName =
                ZrCore_String_Create(cs->state, (TZrNativeString)typeNameText, (TZrSize)(lastDot - typeNameText));
        SZrString *memberTypeName = ZrCore_String_CreateFromNative(cs->state, (TZrNativeString)(lastDot + 1));
        SZrString *memberName =
                compile_expression_extract_member_lookup_name_from_type_text(cs, lastDot + 1);
        SZrTypePrototypeInfo *moduleInfo = ZR_NULL;

        if (moduleName != ZR_NULL && memberName != ZR_NULL && memberTypeName != ZR_NULL) {
            for (TZrSize i = 0; i < cs->typePrototypes.length; i++) {
                info = (SZrTypePrototypeInfo *)ZrCore_Array_Get(&cs->typePrototypes, i);
                if (info != ZR_NULL && info->name != ZR_NULL && ZrCore_String_Equal(info->name, moduleName)) {
                    moduleInfo = info;
                    break;
                }
            }
            if (moduleInfo == ZR_NULL &&
                cs->currentTypePrototypeInfo != ZR_NULL &&
                cs->currentTypePrototypeInfo->name != ZR_NULL &&
                ZrCore_String_Equal(cs->currentTypePrototypeInfo->name, moduleName)) {
                moduleInfo = cs->currentTypePrototypeInfo;
            }
            if (moduleInfo == ZR_NULL && cs->typeEnv != ZR_NULL) {
                SZrInferredType aliasType;

                ZrParser_InferredType_Init(cs->state, &aliasType, ZR_VALUE_TYPE_OBJECT);
                if (ZrParser_TypeEnvironment_LookupVariable(cs->state, cs->typeEnv, moduleName, &aliasType) &&
                    aliasType.typeName != ZR_NULL &&
                    !ZrCore_String_Equal(aliasType.typeName, moduleName)) {
                    moduleInfo = find_compiler_type_prototype(cs, aliasType.typeName);
                    if (moduleInfo == ZR_NULL && ensure_import_module_compile_info(cs, aliasType.typeName)) {
                        moduleInfo = find_compiler_type_prototype(cs, aliasType.typeName);
                    }
                }
                ZrParser_InferredType_Free(cs->state, &aliasType);
            }
            if (moduleInfo == ZR_NULL && ensure_import_module_compile_info(cs, moduleName)) {
                for (TZrSize i = 0; i < cs->typePrototypes.length; i++) {
                    info = (SZrTypePrototypeInfo *)ZrCore_Array_Get(&cs->typePrototypes, i);
                    if (info != ZR_NULL && info->name != ZR_NULL && ZrCore_String_Equal(info->name, moduleName)) {
                        moduleInfo = info;
                        break;
                    }
                }
            }
        }

        if (moduleInfo != ZR_NULL && moduleInfo->type == ZR_OBJECT_PROTOTYPE_TYPE_MODULE) {
            for (TZrSize memberIndex = 0; memberIndex < moduleInfo->members.length; memberIndex++) {
                SZrTypeMemberInfo *memberInfo =
                        (SZrTypeMemberInfo *)ZrCore_Array_Get(&moduleInfo->members, memberIndex);
                if (memberInfo != ZR_NULL && memberInfo->name != ZR_NULL &&
                    memberInfo->fieldTypeName != ZR_NULL &&
                    ZrCore_String_Equal(memberInfo->name, memberName)) {
                    SZrString *candidateTypeName =
                            compile_expression_build_canonical_module_member_type_name(cs,
                                                                                       memberInfo->fieldTypeName,
                                                                                       memberTypeName);
                    if (candidateTypeName != ZR_NULL && !ZrCore_String_Equal(candidateTypeName, typeName)) {
                        SZrTypePrototypeInfo *candidateExact =
                                find_registered_type_prototype_exact_only(cs, candidateTypeName);
                        if (candidateExact != ZR_NULL) {
                            return candidateExact;
                        }
                        if (ZrCore_String_GetNativeString(candidateTypeName) != ZR_NULL &&
                            strchr(ZrCore_String_GetNativeString(candidateTypeName), '<') != ZR_NULL) {
                            return find_compiler_type_prototype(cs, candidateTypeName);
                        }
                    }
                    if (!ZrCore_String_Equal(memberInfo->fieldTypeName, typeName)) {
                        SZrTypePrototypeInfo *fieldExact =
                                find_registered_type_prototype_exact_only(cs, memberInfo->fieldTypeName);
                        if (fieldExact != ZR_NULL) {
                            return fieldExact;
                        }
                    }
                    return ZR_NULL;
                }
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

        *outResolvedTypeName = extract_type_name_string(cs, propertyType);
        return *outResolvedTypeName != ZR_NULL;
    }

    return ZR_FALSE;
}

static SZrString *compile_expression_build_qualified_module_member_type_name(SZrCompilerState *cs,
                                                                             SZrString *moduleTypeName,
                                                                             SZrString *memberResolvedTypeName) {
    TZrNativeString moduleTypeText;
    TZrNativeString memberTypeText;
    const TZrChar *memberGenericStart;
    const TZrChar *memberRootDot;
    TZrChar buffer[ZR_PARSER_TYPE_NAME_BUFFER_LENGTH];
    TZrInt32 written;

    if (cs == ZR_NULL || moduleTypeName == ZR_NULL || memberResolvedTypeName == ZR_NULL) {
        return ZR_NULL;
    }

    moduleTypeText = ZrCore_String_GetNativeString(moduleTypeName);
    memberTypeText = ZrCore_String_GetNativeString(memberResolvedTypeName);
    memberGenericStart = memberTypeText != ZR_NULL ? strchr(memberTypeText, '<') : ZR_NULL;
    memberRootDot = memberTypeText != ZR_NULL
                            ? memchr(memberTypeText,
                                     '.',
                                     memberGenericStart != ZR_NULL ? (size_t)(memberGenericStart - memberTypeText)
                                                                   : strlen(memberTypeText))
                            : ZR_NULL;
    if (moduleTypeText == ZR_NULL || memberTypeText == ZR_NULL || memberRootDot != ZR_NULL) {
        return ZR_NULL;
    }

    written = snprintf(buffer, sizeof(buffer), "%s.%s", moduleTypeText, memberTypeText);
    if (written <= 0 || (TZrSize)written >= sizeof(buffer)) {
        return ZR_NULL;
    }

    return ZrCore_String_Create(cs->state, buffer, (TZrSize)written);
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
                    SZrString *canonicalMemberTypeName =
                            compile_expression_build_canonical_module_member_type_name(cs,
                                                                                        memberInfo->fieldTypeName,
                                                                                        memberResolvedTypeName);
                    nextTypeName = canonicalMemberTypeName != ZR_NULL ? canonicalMemberTypeName
                                                                      : memberInfo->fieldTypeName;
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
        SZrTypePrototypeInfo *prototype = find_compiler_type_prototype(cs, candidateType);
        if (prototype != ZR_NULL) {
            *outTypeName = candidateType;
            *outIsTypeReference = prototype->type != ZR_OBJECT_PROTOTYPE_TYPE_MODULE;
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
                                 prototypeType != ZR_OBJECT_PROTOTYPE_TYPE_MODULE &&
                                 (typeDecl == ZR_NULL || typeDecl->type != ZR_AST_CLASS_DECLARATION ||
                                  (typeDecl->data.classDeclaration.modifierFlags & ZR_DECLARATION_MODIFIER_ABSTRACT) == 0);
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
        if (prototypeInfo != ZR_NULL &&
            (prototypeInfo->modifierFlags & ZR_DECLARATION_MODIFIER_ABSTRACT) != 0) {
            ZrParser_Compiler_Error(cs, "Abstract classes cannot be constructed", location);
        } else if (typeDecl != ZR_NULL && typeDecl->type == ZR_AST_CLASS_DECLARATION &&
                   (typeDecl->data.classDeclaration.modifierFlags & ZR_DECLARATION_MODIFIER_ABSTRACT) != 0) {
            ZrParser_Compiler_Error(cs, "Abstract classes cannot be constructed", location);
        } else {
            ZrParser_Compiler_Error(cs, "Prototype does not allow boxed construction", location);
        }
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

static TZrUInt32 primary_member_chain_direct_local_slot(SZrCompilerState *cs, SZrAstNode *propertyNode) {
    if (cs == ZR_NULL || propertyNode == ZR_NULL || propertyNode->type != ZR_AST_IDENTIFIER_LITERAL ||
        propertyNode->data.identifier.name == ZR_NULL) {
        return ZR_PARSER_SLOT_NONE;
    }

    return find_local_var(cs, propertyNode->data.identifier.name);
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

    requireTaskHandleArgument = memberInfo->contractRole == ZR_MEMBER_CONTRACT_ROLE_TASK_AWAIT;
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
                if (!compile_inferred_type_is_task_handle(cs, &argType)) {
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

static TZrBool resolved_function_call_uses_meta_call_opcode(const SZrFunctionTypeInfo *resolvedFunctionType) {
    return resolvedFunctionType != ZR_NULL &&
           resolvedFunctionType->declarationNode != ZR_NULL &&
           resolvedFunctionType->declarationNode->type == ZR_AST_EXTERN_FUNCTION_DECLARATION;
}

static TZrBool resolved_function_call_skips_native_boundary_conversion(SZrCompilerState *cs,
                                                                       const SZrFunctionTypeInfo *resolvedFunctionType,
                                                                       SZrAstNode *argNode,
                                                                       TZrSize parameterIndex,
                                                                       const SZrInferredType *expectedType) {
    SZrInferredType actualType;
    TZrBool shouldSkip = ZR_FALSE;

    if (cs == ZR_NULL || resolvedFunctionType == ZR_NULL || argNode == ZR_NULL || expectedType == ZR_NULL ||
        !resolved_function_call_uses_meta_call_opcode(resolvedFunctionType)) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(cs->state, &actualType, ZR_VALUE_TYPE_OBJECT);
    if (ZrParser_ExpressionType_Infer(cs, argNode, &actualType)) {
        shouldSkip = ffi_function_call_argument_is_native_boundary_compatible(cs,
                                                                              resolvedFunctionType,
                                                                              parameterIndex,
                                                                              &actualType,
                                                                              expectedType);
    }
    ZrParser_InferredType_Free(cs->state, &actualType);
    return shouldSkip;
}

static TZrBool compile_arguments_against_function_resolved_signature(SZrCompilerState *cs,
                                                                     SZrAstNodeArray *argsToCompile,
                                                                     const SZrFunctionTypeInfo *resolvedFunctionType,
                                                                     const SZrResolvedCallSignature *resolvedSignature,
                                                                     TZrUInt32 firstArgSlot) {
    const SZrArray *parameterTypes =
            resolvedSignature != ZR_NULL ? &resolvedSignature->parameterTypes : ZR_NULL;
    const SZrArray *parameterPassingModes =
            resolvedSignature != ZR_NULL ? &resolvedSignature->parameterPassingModes : ZR_NULL;

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
                                        *passingMode == ZR_PARAMETER_PASSING_MODE_OUT
                                                ? "%out argument must be an assignable storage location"
                                                : "%ref argument must be an assignable storage location",
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
        if (expectedType == ZR_NULL ||
            resolved_function_call_skips_native_boundary_conversion(cs,
                                                                    resolvedFunctionType,
                                                                    argNode,
                                                                    index,
                                                                    expectedType)) {
            continue;
        }

        if (!emit_argument_conversion_if_needed(cs, argNode, argSlot, expectedType)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

void compile_primary_member_chain(SZrCompilerState *cs, SZrAstNode *propertyNode, SZrAstNodeArray *members,
                                         TZrSize memberStartIndex, TZrUInt32 *ioCurrentSlot,
                                         SZrString **ioRootTypeName, TZrBool *ioRootIsTypeReference,
                                         EZrOwnershipQualifier *ioRootOwnershipQualifier,
                                         TZrBool rootUsesSuperLookup,
                                         TZrUInt32 superReceiverSlot) {
    TZrUInt32 currentSlot;
    TZrUInt32 pendingReceiverSlot = ZR_PARSER_SLOT_NONE;
    TZrUInt32 pendingReceiverWritebackSlot = ZR_PARSER_SLOT_NONE;
    SZrString *pendingCallResultTypeName = ZR_NULL;
    const SZrTypeMemberInfo *pendingCallMemberInfo = ZR_NULL;
    SZrString *rootTypeName = ioRootTypeName != ZR_NULL ? *ioRootTypeName : ZR_NULL;
    TZrBool rootIsTypeReference = ioRootIsTypeReference != ZR_NULL ? *ioRootIsTypeReference : ZR_FALSE;
    EZrOwnershipQualifier rootOwnershipQualifier =
            ioRootOwnershipQualifier != ZR_NULL ? *ioRootOwnershipQualifier : ZR_OWNERSHIP_QUALIFIER_NONE;
    TZrBool superLookupActive = rootUsesSuperLookup ? ZR_TRUE : ZR_FALSE;

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
            SZrString *declaredFieldTypeName = ZR_NULL;
            EZrOwnershipQualifier declaredFieldOwnershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
            TZrBool declaredFieldMatch = ZR_FALSE;
            TZrBool nextIsFunctionCall =
                    (i + 1 < members->count &&
                     members->nodes[i + 1] != ZR_NULL &&
                     members->nodes[i + 1]->type == ZR_AST_FUNCTION_CALL);
            TZrBool memberUsesSuperLookup = superLookupActive && i == memberStartIndex;

            if (!memberExpr->computed && memberExpr->property != ZR_NULL &&
                memberExpr->property->type == ZR_AST_IDENTIFIER_LITERAL) {
                memberName = memberExpr->property->data.identifier.name;
            }

            if (rootTypeName != ZR_NULL && memberName != ZR_NULL) {
                typeMember = find_compiler_type_member(cs, rootTypeName, memberName);
                if (typeMember != ZR_NULL && typeMember->isStatic) {
                    isStaticMember = ZR_TRUE;
                }
                declaredFieldMatch = resolve_declared_field_member_access(cs,
                                                                          rootTypeName,
                                                                          memberName,
                                                                          &declaredFieldTypeName,
                                                                          &isStaticMember,
                                                                          &declaredFieldOwnershipQualifier);
                bindReceiverForCall = member_call_requires_bound_receiver(typeMember);
                if (memberUsesSuperLookup &&
                    typeMember != ZR_NULL &&
                    typeMember->isMetaMethod &&
                    typeMember->metaType != ZR_META_CONSTRUCTOR) {
                    bindReceiverForCall = ZR_TRUE;
                }
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
                    if (memberUsesSuperLookup) {
                        if (!emit_super_accessor_call_from_prototype(cs,
                                                                     currentSlot,
                                                                     superReceiverSlot,
                                                                     getterAccessor->name,
                                                                     ZR_NULL,
                                                                     0,
                                                                     member->location)) {
                            return;
                        }
                    } else {
                        if (!emit_property_getter_call(cs, currentSlot, memberName, getterAccessor->isStatic,
                                                       member->location)) {
                            return;
                        }
                    }
                    pendingReceiverSlot = ZR_PARSER_SLOT_NONE;
                    pendingReceiverWritebackSlot = ZR_PARSER_SLOT_NONE;
                    rootTypeName = getterAccessor->returnTypeName;
                    rootIsTypeReference = getterAccessor->isStatic &&
                                          rootTypeName != ZR_NULL &&
                                          type_name_is_registered_prototype(cs, rootTypeName);
                    rootOwnershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
                    superLookupActive = ZR_FALSE;
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
                        TZrUInt32 receiverSourceSlot = currentSlot;
                        pendingReceiverSlot = allocate_stack_slot(cs);

                        pendingReceiverWritebackSlot = ZR_PARSER_SLOT_NONE;
                        if (memberUsesSuperLookup) {
                            receiverSourceSlot = superReceiverSlot;
                        } else if (typeMember != ZR_NULL &&
                            typeMember->memberType == ZR_AST_STRUCT_METHOD &&
                            i == memberStartIndex) {
                            TZrUInt32 directLocalSlot = primary_member_chain_direct_local_slot(cs, propertyNode);
                            if (directLocalSlot != ZR_PARSER_SLOT_NONE) {
                                receiverSourceSlot = directLocalSlot;
                                pendingReceiverWritebackSlot = directLocalSlot;
                            }
                        }

                        emit_instruction(cs, create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK),
                                                                  (TZrUInt16)pendingReceiverSlot,
                                                                  (TZrInt32)receiverSourceSlot));
                    } else {
                        pendingReceiverSlot = ZR_PARSER_SLOT_NONE;
                        pendingReceiverWritebackSlot = ZR_PARSER_SLOT_NONE;
                    }

                    if (!memberExpr->computed) {
                        SZrString *memberSymbol = resolve_member_expression_symbol(cs, memberExpr);
                        if (memberUsesSuperLookup &&
                            typeMember != ZR_NULL &&
                            typeMember->isMetaMethod &&
                            typeMember->metaType != ZR_META_CONSTRUCTOR) {
                            if (!nextIsFunctionCall) {
                                ZrParser_Compiler_Error(cs,
                                                        "super meta members must be invoked as calls",
                                                        member->location);
                                return;
                            }
                            if (!emit_member_function_constant_to_slot(cs, currentSlot, typeMember, member->location)) {
                                return;
                            }
                        } else {
                            TZrUInt32 memberId = compiler_get_or_add_member_entry(cs, memberSymbol);
                            TZrBool canEmitMemberSlot = declaredFieldMatch;
                            if (memberId == ZR_PARSER_MEMBER_ID_NONE) {
                                ZrParser_Compiler_Error(cs,
                                                        "Failed to register member access symbol",
                                                        member->location);
                                return;
                            }

                            if (canEmitMemberSlot) {
                                if (!emit_member_slot_get(cs,
                                                          currentSlot,
                                                          currentSlot,
                                                          memberId,
                                                          member->location)) {
                                    return;
                                }
                            } else {
                                emit_instruction(cs,
                                                 create_instruction_2(ZR_INSTRUCTION_ENUM(GET_MEMBER),
                                                                      (TZrUInt16)currentSlot,
                                                                      (TZrUInt16)currentSlot,
                                                                      (TZrUInt16)memberId));
                            }
                        }
                    } else {
                        if (memberUsesSuperLookup) {
                            ZrParser_Compiler_Error(cs,
                                                    "super only supports direct member names, not computed member access",
                                                    member->location);
                            return;
                        }
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

                    if (declaredFieldMatch) {
                        rootTypeName = typeMember != ZR_NULL ? typeMember->fieldTypeName : declaredFieldTypeName;
                        rootOwnershipQualifier = typeMember != ZR_NULL ? typeMember->ownershipQualifier
                                                                       : declaredFieldOwnershipQualifier;
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
                    superLookupActive = ZR_FALSE;
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
            SZrInferredType contractReturnType;
            TZrBool hasResolvedFunctionSignature = ZR_FALSE;
            TZrBool hasResolvedMemberSignature = ZR_FALSE;
            TZrBool hasContractReturnType = ZR_FALSE;
            TZrBool useMetaCallOpcode = ZR_FALSE;

            memset(&resolvedFunctionSignature, 0, sizeof(resolvedFunctionSignature));
            ZrParser_InferredType_Init(cs->state, &resolvedFunctionSignature.returnType, ZR_VALUE_TYPE_OBJECT);
            ZrCore_Array_Construct(&resolvedFunctionSignature.parameterTypes);
            ZrCore_Array_Construct(&resolvedFunctionSignature.parameterPassingModes);
            memset(&resolvedMemberSignature, 0, sizeof(resolvedMemberSignature));
            ZrParser_InferredType_Init(cs->state, &resolvedMemberSignature.returnType, ZR_VALUE_TYPE_OBJECT);
            ZrCore_Array_Construct(&resolvedMemberSignature.parameterTypes);
            ZrCore_Array_Construct(&resolvedMemberSignature.parameterPassingModes);
            ZrParser_InferredType_Init(cs->state, &contractReturnType, ZR_VALUE_TYPE_OBJECT);

            if (rootIsTypeReference) {
                ZrParser_Compiler_Error(cs,
                                        "Prototype references are not callable; use $target(...) or new target(...)",
                                        member->location);
                free_resolved_call_signature(cs->state, &resolvedFunctionSignature);
                free_resolved_call_signature(cs->state, &resolvedMemberSignature);
                ZrParser_InferredType_Free(cs->state, &contractReturnType);
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
                    ZrParser_InferredType_Free(cs->state, &contractReturnType);
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
                            ZrParser_InferredType_Free(cs->state, &contractReturnType);
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
                        ZrParser_InferredType_Free(cs->state, &contractReturnType);
                        return;
                    }
                    argCount += compiledMemberArgCount;
                } else if (resolvedFunctionType != ZR_NULL) {
                    if (!(hasResolvedFunctionSignature
                                  ? compile_arguments_against_function_resolved_signature(cs,
                                                                                          argsToCompile,
                                                                                          resolvedFunctionType,
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
                        ZrParser_InferredType_Free(cs->state, &contractReturnType);
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
                                ZrParser_InferredType_Free(cs->state, &contractReturnType);
                                return;
                            }
                        }
                    }
                    argCount += (TZrUInt32)argsToCompile->count;
                }
            }

            {
                TZrBool useResolvedFunctionMetaCallOpcode =
                        activeCallMemberInfo == ZR_NULL &&
                        resolved_function_call_uses_meta_call_opcode(resolvedFunctionType);
                TZrBool useDynamicCallOpcode =
                        activeCallMemberInfo == ZR_NULL &&
                        !useResolvedFunctionMetaCallOpcode &&
                        resolvedFunctionType == ZR_NULL &&
                        !hasResolvedFunctionSignature;
                TZrBool emitMetaCallOpcode = useMetaCallOpcode || useResolvedFunctionMetaCallOpcode;
                EZrInstructionCode callOpcode =
                        cs->isInTailCallContext
                                ? (emitMetaCallOpcode
                                           ? ZR_INSTRUCTION_ENUM(META_TAIL_CALL)
                                           : (useDynamicCallOpcode
                                                      ? ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL)
                                                      : ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL)))
                                : (emitMetaCallOpcode
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
            if (pendingReceiverSlot != ZR_PARSER_SLOT_NONE &&
                pendingReceiverWritebackSlot != ZR_PARSER_SLOT_NONE) {
                emit_instruction(cs,
                                 create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK),
                                                      (TZrUInt16)pendingReceiverWritebackSlot,
                                                      (TZrInt32)pendingReceiverSlot));
            }
            collapse_stack_to_slot(cs, currentSlot);
            pendingReceiverSlot = ZR_PARSER_SLOT_NONE;
            pendingReceiverWritebackSlot = ZR_PARSER_SLOT_NONE;
            if (activeCallMemberInfo != ZR_NULL) {
                hasContractReturnType =
                        infer_member_call_contract_return_type(cs, activeCallMemberInfo, call, &contractReturnType);
            }
            if (hasResolvedMemberSignature) {
                rootTypeName = get_type_name_from_inferred_type(cs, &resolvedMemberSignature.returnType);
            } else if (hasResolvedFunctionSignature) {
                rootTypeName = get_type_name_from_inferred_type(cs, &resolvedFunctionSignature.returnType);
            } else if (hasContractReturnType) {
                rootTypeName = get_type_name_from_inferred_type(cs, &contractReturnType);
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
            ZrParser_InferredType_Free(cs->state, &contractReturnType);
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


