//
// Created by Auto on 2025/01/XX.
//

#include "compiler_internal.h"
#include "compiler_attribute_binding.h"

#include "zr_vm_parser/ffi_contract.h"

static const TZrChar *kCompilerEnumRuntimeValueTypeFieldName = "__zr_enumValueTypeName";
static const TZrChar *kCompilerEnumRuntimeMembersFieldName = "__zr_enumMembers";

typedef struct SZrExternStaticDecoratorRule {
    const TZrChar *leafName;
    TZrBool requireCall;
} SZrExternStaticDecoratorRule;

static TZrBool compiler_decorators_validate_static_rules(
        SZrCompilerState *cs,
        SZrAstNodeArray *decorators,
        const SZrExternStaticDecoratorRule *rules,
        TZrSize ruleCount) {
    if (cs == ZR_NULL) {
        return ZR_FALSE;
    }
    if (decorators == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrSize decoratorIndex = 0;
         decoratorIndex < decorators->count;
         decoratorIndex++) {
        SZrAstNode *decoratorNode = decorators->nodes[decoratorIndex];
        TZrBool matched = ZR_FALSE;

        if (decoratorNode == ZR_NULL) {
            continue;
        }
        for (TZrSize ruleIndex = 0; ruleIndex < ruleCount; ruleIndex++) {
            if (extern_compiler_match_decorator_path(
                        decoratorNode,
                        rules[ruleIndex].leafName,
                        rules[ruleIndex].requireCall,
                        ZR_NULL)) {
                matched = ZR_TRUE;
                break;
            }
        }
        if (!matched) {
            ZrParser_Compiler_Error(
                    cs,
                    "decorator.runtime_removed: native extern accepts only canonical zr.ffi static directives",
                    decoratorNode->location);
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static void compiler_enum_init_member_defaults(SZrTypeMemberInfo *memberInfo) {
    if (memberInfo == ZR_NULL) {
        return;
    }

    memset(memberInfo, 0, sizeof(*memberInfo));
    memberInfo->minArgumentCount = ZR_MEMBER_PARAMETER_COUNT_UNKNOWN;
    memberInfo->memberType = ZR_AST_CLASS_FIELD;
    memberInfo->accessModifier = ZR_ACCESS_PUBLIC;
    memberInfo->isStatic = ZR_TRUE;
    memberInfo->isConst = ZR_TRUE;
    memberInfo->metaType = ZR_META_ENUM_MAX;
    memberInfo->virtualSlotIndex = (TZrUInt32)-1;
    memberInfo->interfaceContractSlot = (TZrUInt32)-1;
    memberInfo->propertyIdentity = (TZrUInt32)-1;
    ZrCore_Array_Construct(&memberInfo->parameterTypes);
    ZrCore_Array_Construct(&memberInfo->parameterNames);
    ZrCore_Array_Construct(&memberInfo->parameterHasDefaultValues);
    ZrCore_Array_Construct(&memberInfo->parameterDefaultValues);
    ZrCore_Array_Construct(&memberInfo->genericParameters);
    ZrCore_Array_Construct(&memberInfo->parameterPassingModes);
    ZrCore_Array_Construct(&memberInfo->decorators);
    ZrCore_Value_ResetAsNull(&memberInfo->decoratorMetadataValue);
}

static const SZrExternStaticDecoratorRule kExternFunctionDecoratorRules[] = {
        {"entry", ZR_TRUE},
        {"callingConvention", ZR_TRUE},
        {"callconv", ZR_TRUE},
        {"charset", ZR_TRUE},
        {"errorPolicy", ZR_TRUE},
        {"cleanup", ZR_TRUE},
        {"callbackLifetime", ZR_TRUE},
        {"callbackThread", ZR_TRUE},
        {"callbackException", ZR_TRUE},
        {"platform", ZR_TRUE},
        {"requiredCapabilities", ZR_TRUE},
};

static const SZrExternStaticDecoratorRule kExternDelegateDecoratorRules[] = {
        {"callingConvention", ZR_TRUE},
        {"callconv", ZR_TRUE},
        {"charset", ZR_TRUE},
};

static const SZrExternStaticDecoratorRule kExternStructDecoratorRules[] = {
        {"kind", ZR_TRUE},
        {"pack", ZR_TRUE},
        {"align", ZR_TRUE},
};

static const SZrExternStaticDecoratorRule kExternStructFieldDecoratorRules[] = {
        {"offset", ZR_TRUE},
        {"charset", ZR_TRUE},
};

static const SZrExternStaticDecoratorRule kExternEnumDecoratorRules[] = {
        {"underlying", ZR_TRUE},
};

static const SZrExternStaticDecoratorRule kExternEnumMemberDecoratorRules[] = {
        {"value", ZR_TRUE},
};

static const SZrExternStaticDecoratorRule kExternParameterDecoratorRules[] = {
        {"in", ZR_FALSE},
        {"out", ZR_FALSE},
        {"inout", ZR_FALSE},
        {"charset", ZR_TRUE},
};

static TZrBool compiler_extern_validate_parameter_decorators(
        SZrCompilerState *cs,
        SZrAstNodeArray *params,
        SZrParameter *args) {
    if (params != ZR_NULL) {
        for (TZrSize index = 0; index < params->count; index++) {
            SZrAstNode *parameterNode = params->nodes[index];

            if (parameterNode == ZR_NULL || parameterNode->type != ZR_AST_PARAMETER) {
                continue;
            }
            if (!compiler_decorators_validate_static_rules(
                        cs,
                        parameterNode->data.parameter.decorators,
                        kExternParameterDecoratorRules,
                        ZR_ARRAY_COUNT(kExternParameterDecoratorRules))) {
                return ZR_FALSE;
            }
        }
    }

    return args == ZR_NULL ||
           compiler_decorators_validate_static_rules(
                   cs,
                   args->decorators,
                   kExternParameterDecoratorRules,
                   ZR_ARRAY_COUNT(kExternParameterDecoratorRules));
}

static TZrBool compiler_extern_validate_declaration_decorators(
        SZrCompilerState *cs,
        SZrAstNode *declaration) {
    if (cs == ZR_NULL || declaration == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (declaration->type) {
        case ZR_AST_EXTERN_FUNCTION_DECLARATION: {
            SZrExternFunctionDeclaration *function =
                    &declaration->data.externFunctionDeclaration;
            return compiler_decorators_validate_static_rules(
                           cs,
                           function->decorators,
                           kExternFunctionDecoratorRules,
                           ZR_ARRAY_COUNT(kExternFunctionDecoratorRules)) &&
                   compiler_extern_validate_parameter_decorators(
                           cs, function->params, function->args);
        }
        case ZR_AST_EXTERN_DELEGATE_DECLARATION: {
            SZrExternDelegateDeclaration *delegate =
                    &declaration->data.externDelegateDeclaration;
            return compiler_decorators_validate_static_rules(
                           cs,
                           delegate->decorators,
                           kExternDelegateDecoratorRules,
                           ZR_ARRAY_COUNT(kExternDelegateDecoratorRules)) &&
                   compiler_extern_validate_parameter_decorators(
                           cs, delegate->params, delegate->args);
        }
        case ZR_AST_STRUCT_DECLARATION: {
            SZrStructDeclaration *structDecl = &declaration->data.structDeclaration;

            if (!compiler_decorators_validate_static_rules(
                        cs,
                        structDecl->decorators,
                        kExternStructDecoratorRules,
                        ZR_ARRAY_COUNT(kExternStructDecoratorRules))) {
                return ZR_FALSE;
            }
            if (structDecl->members != ZR_NULL) {
                for (TZrSize index = 0; index < structDecl->members->count; index++) {
                    SZrAstNode *member = structDecl->members->nodes[index];

                    if (member != ZR_NULL && member->type == ZR_AST_STRUCT_FIELD &&
                        !compiler_decorators_validate_static_rules(
                                cs,
                                member->data.structField.decorators,
                                kExternStructFieldDecoratorRules,
                                ZR_ARRAY_COUNT(kExternStructFieldDecoratorRules))) {
                        return ZR_FALSE;
                    }
                }
            }
            return ZR_TRUE;
        }
        case ZR_AST_ENUM_DECLARATION: {
            SZrEnumDeclaration *enumDecl = &declaration->data.enumDeclaration;

            if (!compiler_decorators_validate_static_rules(
                        cs,
                        enumDecl->decorators,
                        kExternEnumDecoratorRules,
                        ZR_ARRAY_COUNT(kExternEnumDecoratorRules))) {
                return ZR_FALSE;
            }
            if (enumDecl->members != ZR_NULL) {
                for (TZrSize index = 0; index < enumDecl->members->count; index++) {
                    SZrAstNode *member = enumDecl->members->nodes[index];

                    if (member != ZR_NULL && member->type == ZR_AST_ENUM_MEMBER &&
                        !compiler_decorators_validate_static_rules(
                                cs,
                                member->data.enumMember.decorators,
                                kExternEnumMemberDecoratorRules,
                                ZR_ARRAY_COUNT(kExternEnumMemberDecoratorRules))) {
                        return ZR_FALSE;
                    }
                }
            }
            return ZR_TRUE;
        }
        default:
            return ZR_TRUE;
    }
}

TZrBool extern_compiler_has_registered_type(SZrCompilerState *cs, SZrString *typeName) {
    if (cs == ZR_NULL || typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < cs->typePrototypes.length; index++) {
        SZrTypePrototypeInfo *info = (SZrTypePrototypeInfo *)ZrCore_Array_Get(&cs->typePrototypes, index);
        if (info != ZR_NULL && info->name != ZR_NULL && ZrCore_String_Equal(info->name, typeName)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static SZrString *compiler_enum_resolve_underlying_type_name(SZrCompilerState *cs,
                                                             const SZrEnumDeclaration *enumDecl) {
    SZrString *underlyingName = ZR_NULL;

    if (cs == ZR_NULL || enumDecl == ZR_NULL) {
        return ZR_NULL;
    }

    if (enumDecl->baseType != ZR_NULL) {
        underlyingName = extract_type_name_string(cs, enumDecl->baseType);
    }
    if (underlyingName == ZR_NULL) {
        underlyingName = extern_compiler_decorators_get_string_arg(enumDecl->decorators, "underlying");
    }
    if (underlyingName == ZR_NULL) {
        underlyingName = ZrCore_String_CreateFromNative(cs->state, "int");
    }

    return underlyingName;
}

static TZrBool compiler_enum_underlying_supports_auto_increment(SZrString *underlyingName) {
    if (underlyingName == ZR_NULL) {
        return ZR_TRUE;
    }

    return extern_compiler_string_equals(underlyingName, "int") ||
           extern_compiler_string_equals(underlyingName, "i8") ||
           extern_compiler_string_equals(underlyingName, "u8") ||
           extern_compiler_string_equals(underlyingName, "i16") ||
           extern_compiler_string_equals(underlyingName, "u16") ||
           extern_compiler_string_equals(underlyingName, "i32") ||
           extern_compiler_string_equals(underlyingName, "u32") ||
           extern_compiler_string_equals(underlyingName, "i64") ||
           extern_compiler_string_equals(underlyingName, "u64");
}

static TZrBool compiler_enum_init_member_runtime_value(SZrCompilerState *cs,
                                                       SZrAstNode *memberNode,
                                                       SZrString *underlyingName,
                                                       TZrInt64 *nextAutoValue,
                                                       SZrTypeValue *outValue) {
    SZrAstNode *valueNode;
    TZrInt64 explicitIntValue = 0;

    if (cs == ZR_NULL || memberNode == ZR_NULL || nextAutoValue == ZR_NULL || outValue == ZR_NULL ||
        memberNode->type != ZR_AST_ENUM_MEMBER) {
        return ZR_FALSE;
    }

    valueNode = memberNode->data.enumMember.value;
    if (valueNode == ZR_NULL &&
        extern_compiler_decorators_get_int_arg(memberNode->data.enumMember.decorators, "value", &explicitIntValue)) {
        ZrCore_Value_InitAsInt(cs->state, outValue, explicitIntValue);
        *nextAutoValue = explicitIntValue + 1;
        return ZR_TRUE;
    }

    if (valueNode == ZR_NULL) {
        if (!compiler_enum_underlying_supports_auto_increment(underlyingName)) {
            ZrParser_Compiler_Error(cs,
                                    "Enum auto values currently require an integer underlying type",
                                    memberNode->location);
            return ZR_FALSE;
        }

        ZrCore_Value_InitAsInt(cs->state, outValue, *nextAutoValue);
        (*nextAutoValue)++;
        return ZR_TRUE;
    }

    switch (valueNode->type) {
        case ZR_AST_INTEGER_LITERAL:
            explicitIntValue = valueNode->data.integerLiteral.value;
            ZrCore_Value_InitAsInt(cs->state, outValue, explicitIntValue);
            *nextAutoValue = explicitIntValue + 1;
            return ZR_TRUE;
        case ZR_AST_FLOAT_LITERAL:
            ZrCore_Value_InitAsFloat(cs->state, outValue, valueNode->data.floatLiteral.value);
            return ZR_TRUE;
        case ZR_AST_BOOLEAN_LITERAL:
            outValue->type = ZR_VALUE_TYPE_BOOL;
            outValue->value.nativeObject.nativeBool = valueNode->data.booleanLiteral.value ? ZR_TRUE : ZR_FALSE;
            outValue->isGarbageCollectable = ZR_FALSE;
            outValue->isNative = ZR_TRUE;
            outValue->ownershipKind = ZR_OWNERSHIP_VALUE_KIND_NONE;
            outValue->ownershipControl = ZR_NULL;
            outValue->ownershipWeakRef = ZR_NULL;
            return ZR_TRUE;
        case ZR_AST_STRING_LITERAL:
            if (valueNode->data.stringLiteral.value == ZR_NULL) {
                break;
            }
            ZrCore_Value_InitAsRawObject(cs->state,
                                         outValue,
                                         ZR_CAST_RAW_OBJECT_AS_SUPER(valueNode->data.stringLiteral.value));
            outValue->type = ZR_VALUE_TYPE_STRING;
            return ZR_TRUE;
        case ZR_AST_CHAR_LITERAL:
            ZrCore_Value_InitAsInt(cs->state, outValue, (TZrInt64)valueNode->data.charLiteral.value);
            return ZR_TRUE;
        default:
            break;
    }

    ZrParser_Compiler_Error(cs,
                            "Enum member values currently require literal constants or integer decorator overrides",
                            memberNode->location);
    return ZR_FALSE;
}

static TZrBool compiler_enum_fill_prototype_info(SZrCompilerState *cs,
                                                  SZrAstNode *declarationNode,
                                                  SZrTypePrototypeInfo *info,
                                                  TZrBool isExternDeclaration) {
    SZrEnumDeclaration *enumDecl;
    SZrString *underlyingName;
    SZrObject *metadataObject;
    SZrObject *membersObject;
    SZrTypeValue metadataValue;
    SZrTypeValue membersValue;
    SZrTypeValue underlyingValue;
    TZrInt64 nextAutoValue = 0;
    ZrExternCompilerTempRoot metadataRoot = {0};
    ZrExternCompilerTempRoot membersRoot = {0};

    if (cs == ZR_NULL || declarationNode == ZR_NULL || info == ZR_NULL ||
        declarationNode->type != ZR_AST_ENUM_DECLARATION || cs->hasError) {
        return ZR_FALSE;
    }

    enumDecl = &declarationNode->data.enumDeclaration;
    if (enumDecl->name == ZR_NULL || enumDecl->name->name == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(info, 0, sizeof(*info));
    info->name = enumDecl->name->name;
    info->type = ZR_OBJECT_PROTOTYPE_TYPE_ENUM;
    info->accessModifier = enumDecl->accessModifier;
    info->isImportedNative = ZR_FALSE;
    info->allowValueConstruction = ZR_TRUE;
    info->allowBoxedConstruction = ZR_TRUE;
    ZrCore_Array_Init(cs->state, &info->inherits, sizeof(SZrString *), 1);
    ZrCore_Array_Init(cs->state, &info->implements, sizeof(SZrString *), 1);
    ZrCore_Array_Init(cs->state, &info->decorators, sizeof(SZrTypeDecoratorInfo), ZR_PARSER_INITIAL_CAPACITY_TINY);
    ZrCore_Array_Init(cs->state, &info->members, sizeof(SZrTypeMemberInfo), ZR_PARSER_INITIAL_CAPACITY_TINY);

    underlyingName = compiler_enum_resolve_underlying_type_name(cs, enumDecl);
    if (underlyingName == ZR_NULL) {
        return ZR_FALSE;
    }
    info->enumValueTypeName = underlyingName;

    if (!extern_compiler_temp_root_begin(cs, &metadataRoot) ||
        !extern_compiler_temp_root_begin(cs, &membersRoot)) {
        if (metadataRoot.active) {
            extern_compiler_temp_root_end(&metadataRoot);
        }
        if (membersRoot.active) {
            extern_compiler_temp_root_end(&membersRoot);
        }
        return ZR_FALSE;
    }

    metadataObject = extern_compiler_new_object_constant(cs);
    membersObject = extern_compiler_new_object_constant(cs);
    if (metadataObject == ZR_NULL || membersObject == ZR_NULL) {
        extern_compiler_temp_root_end(&membersRoot);
        extern_compiler_temp_root_end(&metadataRoot);
        return ZR_FALSE;
    }
    extern_compiler_temp_root_set_object(&metadataRoot, metadataObject, ZR_VALUE_TYPE_OBJECT);
    extern_compiler_temp_root_set_object(&membersRoot, membersObject, ZR_VALUE_TYPE_OBJECT);

    ZrCore_Value_InitAsRawObject(cs->state, &underlyingValue, ZR_CAST_RAW_OBJECT_AS_SUPER(underlyingName));
    underlyingValue.type = ZR_VALUE_TYPE_STRING;
    if (!extern_compiler_set_object_field(cs,
                                          metadataObject,
                                          kCompilerEnumRuntimeValueTypeFieldName,
                                          &underlyingValue)) {
        extern_compiler_temp_root_end(&membersRoot);
        extern_compiler_temp_root_end(&metadataRoot);
        return ZR_FALSE;
    }

    if (enumDecl->members != ZR_NULL) {
        for (TZrSize index = 0; index < enumDecl->members->count; index++) {
            SZrAstNode *memberNode = enumDecl->members->nodes[index];
            SZrTypeMemberInfo memberInfo;
            SZrTypeValue memberValue;
            const TZrChar *memberNameText;

            if (memberNode == ZR_NULL || memberNode->type != ZR_AST_ENUM_MEMBER ||
                memberNode->data.enumMember.name == ZR_NULL || memberNode->data.enumMember.name->name == ZR_NULL) {
                continue;
            }

            compiler_enum_init_member_defaults(&memberInfo);
            memberInfo.declarationNode = memberNode;
            memberInfo.name = memberNode->data.enumMember.name->name;
            memberInfo.fieldTypeName = info->name;

            if (isExternDeclaration) {
                if (!compiler_decorators_validate_static_rules(
                            cs,
                            memberNode->data.enumMember.decorators,
                            kExternEnumMemberDecoratorRules,
                            ZR_ARRAY_COUNT(kExternEnumMemberDecoratorRules))) {
                    extern_compiler_temp_root_end(&membersRoot);
                    extern_compiler_temp_root_end(&metadataRoot);
                    return ZR_FALSE;
                }
            } else {
                if (!ZrParser_CompileTime_ApplyMemberDecorators(
                            cs,
                            memberNode,
                            memberNode->data.enumMember.decorators,
                            &memberInfo) ||
                    !ZrParser_Metadata_ApplyMemberAttributes(
                            cs,
                            memberNode->data.enumMember.decorators,
                            ZR_PARSER_ATTRIBUTE_TARGET_FIELD,
                            &memberInfo,
                            memberNode->location)) {
                    extern_compiler_temp_root_end(&membersRoot);
                    extern_compiler_temp_root_end(&metadataRoot);
                    return ZR_FALSE;
                }
            }

            if (!compiler_enum_init_member_runtime_value(cs,
                                                         memberNode,
                                                         underlyingName,
                                                         &nextAutoValue,
                                                         &memberValue)) {
                extern_compiler_temp_root_end(&membersRoot);
                extern_compiler_temp_root_end(&metadataRoot);
                return ZR_FALSE;
            }

            ZrCore_Array_Push(cs->state, &info->members, &memberInfo);

            memberNameText = ZrCore_String_GetNativeString(memberNode->data.enumMember.name->name);
            if (memberNameText == ZR_NULL ||
                !extern_compiler_set_object_field(cs, membersObject, memberNameText, &memberValue)) {
                extern_compiler_temp_root_end(&membersRoot);
                extern_compiler_temp_root_end(&metadataRoot);
                return ZR_FALSE;
            }
        }
    }

    ZrCore_Value_InitAsRawObject(cs->state, &membersValue, ZR_CAST_RAW_OBJECT_AS_SUPER(membersObject));
    membersValue.type = ZR_VALUE_TYPE_OBJECT;
    if (!extern_compiler_set_object_field(cs,
                                          metadataObject,
                                          kCompilerEnumRuntimeMembersFieldName,
                                          &membersValue)) {
        extern_compiler_temp_root_end(&membersRoot);
        extern_compiler_temp_root_end(&metadataRoot);
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(cs->state, &metadataValue, ZR_CAST_RAW_OBJECT_AS_SUPER(metadataObject));
    metadataValue.type = ZR_VALUE_TYPE_OBJECT;
    info->decoratorMetadataValue = metadataValue;
    info->hasDecoratorMetadata = ZR_TRUE;

    extern_compiler_temp_root_end(&membersRoot);
    extern_compiler_temp_root_end(&metadataRoot);
    return ZR_TRUE;
}

void extern_compiler_register_struct_prototype(SZrCompilerState *cs, SZrAstNode *declarationNode) {
    SZrStructDeclaration *structDecl;
    SZrTypePrototypeInfo info;
    TZrUInt32 currentOffset = 0;

    if (cs == ZR_NULL || declarationNode == ZR_NULL || declarationNode->type != ZR_AST_STRUCT_DECLARATION || cs->hasError) {
        return;
    }

    structDecl = &declarationNode->data.structDeclaration;
    if (structDecl->name == ZR_NULL || structDecl->name->name == ZR_NULL ||
        extern_compiler_has_registered_type(cs, structDecl->name->name)) {
        return;
    }

    memset(&info, 0, sizeof(info));
    info.name = structDecl->name->name;
    info.type = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT;
    info.accessModifier = structDecl->accessModifier;
    info.isImportedNative = ZR_FALSE;
    info.allowValueConstruction = ZR_TRUE;
    info.allowBoxedConstruction = ZR_TRUE;
    ZrCore_Array_Init(cs->state, &info.inherits, sizeof(SZrString *), ZR_PARSER_INITIAL_CAPACITY_PAIR);
    ZrCore_Array_Init(cs->state, &info.implements, sizeof(SZrString *), ZR_PARSER_INITIAL_CAPACITY_PAIR);
    ZrCore_Array_Init(cs->state, &info.decorators, sizeof(SZrTypeDecoratorInfo), ZR_PARSER_INITIAL_CAPACITY_TINY);
    ZrCore_Array_Init(cs->state, &info.members, sizeof(SZrTypeMemberInfo), ZR_PARSER_INITIAL_CAPACITY_SMALL);

    if (structDecl->members != ZR_NULL) {
        for (TZrSize index = 0; index < structDecl->members->count; index++) {
            SZrAstNode *member = structDecl->members->nodes[index];
            SZrTypeMemberInfo memberInfo;
            TZrInt64 explicitOffset = 0;

            if (member == ZR_NULL || member->type != ZR_AST_STRUCT_FIELD ||
                member->data.structField.name == ZR_NULL || member->data.structField.name->name == ZR_NULL) {
                continue;
            }

            memset(&memberInfo, 0, sizeof(memberInfo));
            memberInfo.minArgumentCount = ZR_MEMBER_PARAMETER_COUNT_UNKNOWN;
            memberInfo.memberType = ZR_AST_STRUCT_FIELD;
            memberInfo.name = member->data.structField.name->name;
            memberInfo.accessModifier = member->data.structField.access;
            memberInfo.isStatic = member->data.structField.isStatic;
            memberInfo.isConst = member->data.structField.isConst;
            memberInfo.fieldType = member->data.structField.typeInfo;
            memberInfo.fieldTypeName = extract_type_name_string(cs, member->data.structField.typeInfo);
            memberInfo.fieldSize = calculate_type_size(cs, member->data.structField.typeInfo);
            if (memberInfo.fieldSize == 0) {
                memberInfo.fieldSize = ZR_ALIGN_SIZE;
            }

            if (extern_compiler_decorators_get_int_arg(member->data.structField.decorators, "offset", &explicitOffset)) {
                memberInfo.fieldOffset = (TZrUInt32)explicitOffset;
                currentOffset = memberInfo.fieldOffset + memberInfo.fieldSize;
            } else {
                currentOffset = align_offset(currentOffset, get_type_alignment(cs, member->data.structField.typeInfo));
                memberInfo.fieldOffset = currentOffset;
                currentOffset += memberInfo.fieldSize;
            }

            ZrCore_Array_Push(cs->state, &info.members, &memberInfo);
        }
    }

    ZrCore_Array_Push(cs->state, &cs->typePrototypes, &info);
    if (cs->typeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_RegisterType(cs->state, cs->typeEnv, info.name);
    }
    if (cs->compileTimeTypeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_RegisterType(cs->state, cs->compileTimeTypeEnv, info.name);
    }
}

void extern_compiler_register_enum_prototype(SZrCompilerState *cs, SZrAstNode *declarationNode) {
    SZrTypePrototypeInfo info;
    SZrEnumDeclaration *enumDecl;

    if (cs == ZR_NULL || declarationNode == ZR_NULL || declarationNode->type != ZR_AST_ENUM_DECLARATION || cs->hasError) {
        return;
    }

    enumDecl = &declarationNode->data.enumDeclaration;
    if (enumDecl->name == ZR_NULL || enumDecl->name->name == ZR_NULL ||
        extern_compiler_has_registered_type(cs, enumDecl->name->name)) {
        return;
    }

    if (!compiler_enum_fill_prototype_info(cs, declarationNode, &info, ZR_TRUE)) {
        return;
    }

    ZrCore_Array_Push(cs->state, &cs->typePrototypes, &info);
    if (cs->typeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_RegisterType(cs->state, cs->typeEnv, info.name);
    }
    if (cs->compileTimeTypeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_RegisterType(cs->state, cs->compileTimeTypeEnv, info.name);
    }
}

void compile_enum_declaration(SZrCompilerState *cs, SZrAstNode *node) {
    SZrEnumDeclaration *enumDecl;
    SZrTypePrototypeInfo info;
    TZrBool decoratorsValid;

    if (cs == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_ENUM_DECLARATION || cs->hasError) {
        return;
    }

    enumDecl = &node->data.enumDeclaration;
    if (enumDecl->name == ZR_NULL || enumDecl->name->name == ZR_NULL ||
        extern_compiler_has_registered_type(cs, enumDecl->name->name)) {
        return;
    }

    if (!compiler_enum_fill_prototype_info(cs, node, &info, ZR_FALSE)) {
        return;
    }

    decoratorsValid = ZrParser_Compiler_ApplyCompileTimeTypeDecorators(
            cs, node, enumDecl->decorators, &info);
    if (decoratorsValid) {
        decoratorsValid = ZrParser_Metadata_ApplyTypeAttributes(
                cs, enumDecl->decorators, &info, node->location);
    }
    if (!decoratorsValid) {
        return;
    }

    ZrCore_Array_Push(cs->state, &cs->typePrototypes, &info);
    if (cs->typeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_RegisterType(cs->state, cs->typeEnv, info.name);
    }
    if (cs->compileTimeTypeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_RegisterType(cs->state, cs->compileTimeTypeEnv, info.name);
    }
}

void compiler_register_extern_block_bindings(SZrCompilerState *cs, SZrExternBlock *externBlock) {
    if (cs == ZR_NULL || externBlock == ZR_NULL || externBlock->declarations == ZR_NULL || cs->hasError) {
        return;
    }

    for (TZrSize index = 0; index < externBlock->declarations->count; index++) {
        SZrAstNode *declaration = externBlock->declarations->nodes[index];

        if (declaration != ZR_NULL &&
            !compiler_extern_validate_declaration_decorators(cs, declaration)) {
            return;
        }
    }

    for (TZrSize index = 0; index < externBlock->declarations->count; index++) {
        SZrAstNode *declaration = externBlock->declarations->nodes[index];
        if (declaration == ZR_NULL) {
            continue;
        }

        if (declaration->type == ZR_AST_STRUCT_DECLARATION) {
            extern_compiler_register_struct_prototype(cs, declaration);
        } else if (declaration->type == ZR_AST_ENUM_DECLARATION) {
            extern_compiler_register_enum_prototype(cs, declaration);
        }
    }

    for (TZrSize index = 0; index < externBlock->declarations->count; index++) {
        SZrAstNode *declaration = externBlock->declarations->nodes[index];
        if (declaration == ZR_NULL) {
            continue;
        }

        switch (declaration->type) {
            case ZR_AST_EXTERN_FUNCTION_DECLARATION:
                compiler_register_extern_function_type_binding_to_env(
                        cs,
                        declaration,
                        cs->typeEnv,
                        &declaration->data.externFunctionDeclaration);
                compiler_register_extern_function_type_binding_to_env(
                        cs,
                        declaration,
                        cs->compileTimeTypeEnv,
                        &declaration->data.externFunctionDeclaration);
                break;
            case ZR_AST_EXTERN_DELEGATE_DECLARATION:
                if (declaration->data.externDelegateDeclaration.name != ZR_NULL) {
                    SZrString *delegateName = declaration->data.externDelegateDeclaration.name->name;
                    compiler_register_named_value_binding_to_env(cs, cs->typeEnv, delegateName, ZR_NULL);
                    compiler_register_named_value_binding_to_env(cs,
                                                                 cs->compileTimeTypeEnv,
                                                                 delegateName,
                                                                 ZR_NULL);
                }
                break;
            case ZR_AST_STRUCT_DECLARATION:
                if (declaration->data.structDeclaration.name != ZR_NULL) {
                    SZrString *structName = declaration->data.structDeclaration.name->name;
                    compiler_register_named_value_binding_to_env(cs, cs->typeEnv, structName, ZR_NULL);
                    compiler_register_named_value_binding_to_env(cs, cs->compileTimeTypeEnv, structName, ZR_NULL);
                }
                break;
            case ZR_AST_ENUM_DECLARATION:
                if (declaration->data.enumDeclaration.name != ZR_NULL) {
                    SZrString *enumName = declaration->data.enumDeclaration.name->name;
                    compiler_register_named_value_binding_to_env(cs, cs->typeEnv, enumName, ZR_NULL);
                    compiler_register_named_value_binding_to_env(cs, cs->compileTimeTypeEnv, enumName, ZR_NULL);
                }
                break;
            default:
                break;
        }
    }
}

void ZrParser_Compiler_PredeclareExternBindings(SZrCompilerState *cs, SZrAstNodeArray *statements) {
    if (cs == ZR_NULL || statements == ZR_NULL || cs->hasError || cs->externBindingsPredeclared) {
        return;
    }

    for (TZrSize index = 0; index < statements->count; index++) {
        SZrAstNode *statement = statements->nodes[index];
        if (statement == ZR_NULL || statement->type != ZR_AST_EXTERN_BLOCK) {
            continue;
        }
        compiler_register_extern_block_bindings(cs, &statement->data.externBlock);
        if (cs->hasError) {
            return;
        }
    }

    cs->externBindingsPredeclared = ZR_TRUE;
}

static TZrUInt64 compiler_native_import_module_id(
        const SZrCompilerState *cs,
        const SZrNativeImportContract *contract) {
    const TZrChar *identity = ZR_NULL;
    TZrUInt64 hash = ZR_FFI_CONTRACT_FNV_OFFSET;

    if (cs != ZR_NULL && cs->currentModuleKey != ZR_NULL) {
        identity = ZrCore_String_GetNativeString(cs->currentModuleKey);
    } else if (cs != ZR_NULL && cs->scriptAst != ZR_NULL &&
               cs->scriptAst->type == ZR_AST_SCRIPT &&
               cs->scriptAst->data.script.moduleName != ZR_NULL) {
        const SZrAstNode *module = cs->scriptAst->data.script.moduleName;

        if (module->type == ZR_AST_MODULE_DECLARATION &&
            module->data.moduleDeclaration.name != ZR_NULL &&
            module->data.moduleDeclaration.name->type == ZR_AST_STRING_LITERAL &&
            module->data.moduleDeclaration.name->data.stringLiteral.value !=
                    ZR_NULL) {
            identity = ZrCore_String_GetNativeString(
                    module->data.moduleDeclaration.name->data.stringLiteral.value);
        }
    }
    if (identity == ZR_NULL && contract != ZR_NULL) {
        identity = contract->sourceMapping.document;
    }
    if (identity == ZR_NULL) {
        identity = "<unknown>";
    }
    for (TZrSize index = 0u;; index++) {
        hash = ZrCommon_FfiContract_HashByte(
                hash, (TZrUInt8)identity[index]);
        if (identity[index] == '\0') {
            break;
        }
    }
    return hash;
}

static TZrBool compiler_append_native_import_contract(
        SZrCompilerState *cs,
        const SZrExternBlock *externBlock,
        const SZrAstNode *declaration,
        TZrUInt32 *outContractIndex) {
    SZrNativeImportContract contract;
    SZrFfiContractDiagnostic diagnostic;
    SZrNativeImportContract *contracts;
    TZrUInt32 oldCount;
    EZrFfiContractStatus status;

    if (outContractIndex != ZR_NULL) {
        *outContractIndex = ZR_PARSER_SLOT_NONE;
    }
    if (cs == ZR_NULL || cs->currentFunction == ZR_NULL ||
        outContractIndex == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Memory_RawSet(&diagnostic, 0, sizeof(diagnostic));
    status = ZrParser_FfiContract_Build(
            cs->semanticContext,
            externBlock,
            declaration,
            &contract,
            &diagnostic);
    if (status != ZR_FFI_CONTRACT_STATUS_OK) {
        ZrParser_Compiler_Error(
                cs,
                "native extern declaration cannot form a canonical FFI contract",
                diagnostic.sourceRange.source != ZR_NULL
                        ? diagnostic.sourceRange
                        : declaration->location);
        return ZR_FALSE;
    }
    contract.declaringModuleId = compiler_native_import_module_id(cs, &contract);

    oldCount = cs->currentFunction->nativeImportContractLength;
    contracts = (SZrNativeImportContract *)ZrCore_Memory_RawMallocWithType(
            cs->state->global,
            sizeof(SZrNativeImportContract) * (oldCount + 1u),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (contracts == ZR_NULL) {
        ZrParser_Compiler_Error(
                cs, "failed to allocate native import contract table", declaration->location);
        return ZR_FALSE;
    }
    if (oldCount > 0u && cs->currentFunction->nativeImportContracts != ZR_NULL) {
        ZrCore_Memory_RawCopy(
                contracts,
                cs->currentFunction->nativeImportContracts,
                sizeof(SZrNativeImportContract) * oldCount);
        ZrCore_Memory_RawFreeWithType(
                cs->state->global,
                cs->currentFunction->nativeImportContracts,
                sizeof(SZrNativeImportContract) * oldCount,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    contracts[oldCount] = contract;
    cs->currentFunction->nativeImportContracts = contracts;
    cs->currentFunction->nativeImportContractLength = oldCount + 1u;
    *outContractIndex = oldCount;
    return ZR_TRUE;
}

void compile_extern_block_declaration(SZrCompilerState *cs, SZrAstNode *node) {
    SZrExternBlock *externBlock;
    SZrString *libraryName;
    SZrString *ffiModuleName;
    SZrString *loadLibraryName;
    SZrString *getSymbolName;
    TZrUInt32 ffiModuleSlot = ZR_PARSER_SLOT_NONE;
    TZrUInt32 librarySlot = ZR_PARSER_SLOT_NONE;

    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }

    if (node->type != ZR_AST_EXTERN_BLOCK) {
        ZrParser_Compiler_Error(cs, "Expected extern block declaration", node->location);
        return;
    }

    externBlock = &node->data.externBlock;
    if (externBlock->libraryName == ZR_NULL || externBlock->libraryName->type != ZR_AST_STRING_LITERAL) {
        ZrParser_Compiler_Error(cs, "extern block requires a string library specifier", node->location);
        return;
    }

    libraryName = externBlock->libraryName->data.stringLiteral.value;
    ffiModuleName = ZrCore_String_CreateFromNative(cs->state, "zr.ffi");
    loadLibraryName = ZrCore_String_CreateFromNative(cs->state, "loadLibrary");
    getSymbolName = ZrCore_String_CreateFromNative(
            cs->state,
            "getContractSymbol");
    if (libraryName == ZR_NULL || ffiModuleName == ZR_NULL || loadLibraryName == ZR_NULL || getSymbolName == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "failed to allocate extern ffi helper strings", node->location);
        return;
    }

    if (!cs->externBindingsPredeclared) {
        compiler_register_extern_block_bindings(cs, externBlock);
        if (cs->hasError) {
            return;
        }
    }

    if (externBlock->declarations == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < externBlock->declarations->count; index++) {
        SZrAstNode *declaration = externBlock->declarations->nodes[index];
        SZrString *bindingName = ZR_NULL;

        if (declaration == ZR_NULL) {
            continue;
        }

        switch (declaration->type) {
            case ZR_AST_EXTERN_DELEGATE_DECLARATION: {
                ZrExternCompilerTempRoot descriptorRoot = {0};
                if (declaration->data.externDelegateDeclaration.name != ZR_NULL) {
                    bindingName = declaration->data.externDelegateDeclaration.name->name;
                }
                if (bindingName != ZR_NULL &&
                    extern_compiler_temp_root_begin(cs, &descriptorRoot) &&
                    extern_compiler_build_delegate_descriptor_value(
                            cs,
                            externBlock,
                            declaration,
                            ZR_TRUE,
                            extern_compiler_temp_root_value(&descriptorRoot))) {
                    TZrUInt32 localSlot = allocate_local_var(cs, bindingName);
                    emit_constant_to_slot(cs, localSlot, extern_compiler_temp_root_value(&descriptorRoot));
                    ZrParser_Compiler_TrimStackToSlot(cs, localSlot);
                }
                if (descriptorRoot.active) {
                    extern_compiler_temp_root_end(&descriptorRoot);
                }
                break;
            }
            case ZR_AST_STRUCT_DECLARATION: {
                ZrExternCompilerTempRoot descriptorRoot = {0};
                if (declaration->data.structDeclaration.name != ZR_NULL) {
                    bindingName = declaration->data.structDeclaration.name->name;
                }
                if (bindingName != ZR_NULL &&
                    extern_compiler_temp_root_begin(cs, &descriptorRoot) &&
                    extern_compiler_build_struct_descriptor_value(
                            cs,
                            externBlock,
                            declaration,
                            extern_compiler_temp_root_value(&descriptorRoot))) {
                    TZrUInt32 localSlot = allocate_local_var(cs, bindingName);
                    emit_constant_to_slot(cs, localSlot, extern_compiler_temp_root_value(&descriptorRoot));
                    ZrParser_Compiler_TrimStackToSlot(cs, localSlot);
                }
                if (descriptorRoot.active) {
                    extern_compiler_temp_root_end(&descriptorRoot);
                }
                break;
            }
            case ZR_AST_ENUM_DECLARATION: {
                ZrExternCompilerTempRoot descriptorRoot = {0};
                if (declaration->data.enumDeclaration.name != ZR_NULL) {
                    bindingName = declaration->data.enumDeclaration.name->name;
                }
                if (bindingName != ZR_NULL &&
                    extern_compiler_temp_root_begin(cs, &descriptorRoot) &&
                    extern_compiler_build_enum_descriptor_value(
                            cs, declaration, extern_compiler_temp_root_value(&descriptorRoot))) {
                    TZrUInt32 localSlot = allocate_local_var(cs, bindingName);
                    emit_constant_to_slot(cs, localSlot, extern_compiler_temp_root_value(&descriptorRoot));
                    ZrParser_Compiler_TrimStackToSlot(cs, localSlot);
                }
                if (descriptorRoot.active) {
                    extern_compiler_temp_root_end(&descriptorRoot);
                }
                break;
            }
            case ZR_AST_EXTERN_FUNCTION_DECLARATION: {
                SZrExternFunctionDeclaration *functionDecl = &declaration->data.externFunctionDeclaration;
                TZrUInt32 localSlot;
                SZrTypeValue symbolArguments[2];
                TZrUInt32 symbolArgumentCount = 0u;
                TZrUInt32 nativeContractIndex = ZR_PARSER_SLOT_NONE;

                if (functionDecl->name == ZR_NULL || functionDecl->name->name == ZR_NULL) {
                    ZrParser_Compiler_Error(cs, "extern function declaration is missing a name", declaration->location);
                    return;
                }
                if (!compiler_append_native_import_contract(
                            cs,
                            externBlock,
                            declaration,
                            &nativeContractIndex)) {
                    return;
                }
                ZrCore_Value_InitAsInt(
                        cs->state,
                        &symbolArguments[0],
                        (TZrInt64)nativeContractIndex);
                symbolArgumentCount = 1u;

                if (ffiModuleSlot == ZR_PARSER_SLOT_NONE) {
                    SZrString *hiddenFfiName = create_hidden_extern_local_name(cs, "ffi");
                    SZrString *hiddenLibraryName = create_hidden_extern_local_name(cs, "library");
                    SZrTypeValue loadArguments[1];
                    if (hiddenFfiName == ZR_NULL || hiddenLibraryName == ZR_NULL) {
                        ZrParser_Compiler_Error(cs, "failed to allocate hidden extern locals", declaration->location);
                        return;
                    }

                    ffiModuleSlot = allocate_local_var(cs, hiddenFfiName);
                    if (!extern_compiler_emit_import_module_to_local(cs, ffiModuleName, ffiModuleSlot, declaration->location)) {
                        return;
                    }

                    librarySlot = allocate_local_var(cs, hiddenLibraryName);
                    ZrCore_Value_InitAsRawObject(cs->state, &loadArguments[0], ZR_CAST_RAW_OBJECT_AS_SUPER(libraryName));
                    loadArguments[0].type = ZR_VALUE_TYPE_STRING;
                    if (!extern_compiler_emit_module_function_call_to_local(cs,
                                                                           ffiModuleSlot,
                                                                           loadLibraryName,
                                                                           loadArguments,
                                                                           1,
                                                                           librarySlot,
                                                                           declaration->location)) {
                        return;
                    }
                }

                localSlot = allocate_local_var(cs, functionDecl->name->name);
                if (!extern_compiler_emit_method_call_to_local(cs,
                                                               librarySlot,
                                                               getSymbolName,
                                                               symbolArguments,
                                                               symbolArgumentCount,
                                                               localSlot,
                                                               declaration->location)) {
                    return;
                }
                break;
            }
            default:
                break;
        }

        if (cs->hasError) {
            return;
        }
    }
}

void ZrParser_Compiler_CompileExternBlock(SZrCompilerState *cs, SZrAstNode *node) {
    compile_extern_block_declaration(cs, node);
}

// 编译元函数（@constructor, @destructor 等）
