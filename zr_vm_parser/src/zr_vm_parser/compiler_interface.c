//
// Created by Auto on 2026/04/01.
//

#include "compiler_internal.h"

static void compiler_interface_append_parameter_type(SZrCompilerState *cs,
                                                     SZrArray *parameterTypes,
                                                     SZrType *typeInfo) {
    SZrInferredType paramType;

    if (cs == ZR_NULL || parameterTypes == ZR_NULL) {
        return;
    }

    if (typeInfo != ZR_NULL && ZrParser_AstTypeToInferredType_Convert(cs, typeInfo, &paramType)) {
        ZrCore_Array_Push(cs->state, parameterTypes, &paramType);
        return;
    }

    ZrParser_InferredType_Init(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
    ZrCore_Array_Push(cs->state, parameterTypes, &paramType);
}

static void compiler_interface_collect_parameter_types(SZrCompilerState *cs,
                                                       SZrArray *parameterTypes,
                                                       SZrAstNodeArray *params) {
    if (cs == ZR_NULL || parameterTypes == ZR_NULL || params == ZR_NULL || params->count == 0) {
        return;
    }

    ZrCore_Array_Init(cs->state, parameterTypes, sizeof(SZrInferredType), params->count);
    for (TZrSize paramIndex = 0; paramIndex < params->count; paramIndex++) {
        SZrAstNode *paramNode = params->nodes[paramIndex];
        if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
            continue;
        }

        compiler_interface_append_parameter_type(cs, parameterTypes, paramNode->data.parameter.typeInfo);
    }
}

static EZrMetaType compiler_interface_resolve_meta_type(SZrString *metaName) {
    TZrNativeString metaNameText;

    if (metaName == ZR_NULL) {
        return ZR_META_ENUM_MAX;
    }

    metaNameText = ZrCore_String_GetNativeStringShort(metaName);
    if (metaNameText == ZR_NULL) {
        return ZR_META_ENUM_MAX;
    }

    if (strcmp(metaNameText, "constructor") == 0) {
        return ZR_META_CONSTRUCTOR;
    }
    if (strcmp(metaNameText, "destructor") == 0) {
        return ZR_META_DESTRUCTOR;
    }
    if (strcmp(metaNameText, "add") == 0) {
        return ZR_META_ADD;
    }
    if (strcmp(metaNameText, "toString") == 0) {
        return ZR_META_TO_STRING;
    }

    return ZR_META_ENUM_MAX;
}

static void compiler_interface_init_member_defaults(SZrTypeMemberInfo *memberInfo) {
    if (memberInfo == ZR_NULL) {
        return;
    }

    memset(memberInfo, 0, sizeof(*memberInfo));
    memberInfo->accessModifier = ZR_ACCESS_PRIVATE;
    memberInfo->ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    memberInfo->receiverQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    ZrCore_Array_Construct(&memberInfo->parameterTypes);
    ZrCore_Array_Construct(&memberInfo->genericParameters);
    ZrCore_Array_Construct(&memberInfo->parameterPassingModes);
}

void compile_interface_declaration(SZrCompilerState *cs, SZrAstNode *node) {
    SZrInterfaceDeclaration *interfaceDecl;
    SZrString *typeName;
    SZrString *oldTypeName;
    SZrTypePrototypeInfo *oldTypePrototypeInfo;
    SZrTypePrototypeInfo info;

    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }

    if (node->type != ZR_AST_INTERFACE_DECLARATION) {
        ZrParser_Statement_Compile(cs, node);
        return;
    }

    interfaceDecl = &node->data.interfaceDeclaration;
    if (interfaceDecl->name == ZR_NULL || interfaceDecl->name->name == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Interface declaration must have a valid name", node->location);
        return;
    }

    typeName = interfaceDecl->name->name;
    oldTypeName = cs->currentTypeName;
    oldTypePrototypeInfo = cs->currentTypePrototypeInfo;
    cs->currentTypeName = typeName;

    memset(&info, 0, sizeof(info));
    info.name = typeName;
    info.type = ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE;
    info.accessModifier = interfaceDecl->accessModifier;
    info.isImportedNative = ZR_FALSE;
    info.allowValueConstruction = ZR_FALSE;
    info.allowBoxedConstruction = ZR_FALSE;

    ZrCore_Array_Init(cs->state, &info.inherits, sizeof(SZrString *), 4);
    ZrCore_Array_Init(cs->state, &info.implements, sizeof(SZrString *), 1);
    ZrCore_Array_Init(cs->state,
                      &info.genericParameters,
                      sizeof(SZrTypeGenericParameterInfo),
                      interfaceDecl->generic != ZR_NULL && interfaceDecl->generic->params != ZR_NULL
                              ? interfaceDecl->generic->params->count
                              : 1);
    ZrCore_Array_Init(cs->state, &info.members, sizeof(SZrTypeMemberInfo), 8);
    compiler_collect_generic_parameter_info(cs, &info.genericParameters, interfaceDecl->generic);

    if (interfaceDecl->inherits != ZR_NULL && interfaceDecl->inherits->count > 0) {
        for (TZrSize index = 0; index < interfaceDecl->inherits->count; index++) {
            SZrAstNode *inheritType = interfaceDecl->inherits->nodes[index];
            SZrString *inheritTypeName =
                    inheritType != ZR_NULL && inheritType->type == ZR_AST_TYPE
                            ? extract_type_name_string(cs, &inheritType->data.type)
                            : ZR_NULL;
            if (inheritTypeName == ZR_NULL) {
                continue;
            }

            if (info.extendsTypeName == ZR_NULL) {
                info.extendsTypeName = inheritTypeName;
            }
            ZrCore_Array_Push(cs->state, &info.inherits, &inheritTypeName);
        }
    }

    cs->currentTypePrototypeInfo = &info;
    if (interfaceDecl->members != ZR_NULL && interfaceDecl->members->count > 0) {
        for (TZrSize index = 0; index < interfaceDecl->members->count; index++) {
            SZrAstNode *member = interfaceDecl->members->nodes[index];

            if (member == ZR_NULL) {
                continue;
            }

            switch (member->type) {
                case ZR_AST_INTERFACE_FIELD_DECLARATION: {
                    SZrInterfaceFieldDeclaration *field = &member->data.interfaceFieldDeclaration;
                    SZrTypeMemberInfo memberInfo;

                    compiler_interface_init_member_defaults(&memberInfo);
                    memberInfo.memberType = ZR_AST_CLASS_FIELD;
                    memberInfo.declarationNode = member;
                    memberInfo.accessModifier = field->access;
                    memberInfo.isConst = field->isConst;
                    memberInfo.name = field->name != ZR_NULL ? field->name->name : ZR_NULL;
                    memberInfo.fieldType = field->typeInfo;
                    memberInfo.fieldTypeName = field->typeInfo != ZR_NULL
                                                       ? extract_type_name_string(cs, field->typeInfo)
                                                       : ZrCore_String_CreateFromNative(cs->state, "object");
                    memberInfo.ownershipQualifier = field->typeInfo != ZR_NULL
                                                            ? field->typeInfo->ownershipQualifier
                                                            : ZR_OWNERSHIP_QUALIFIER_NONE;
                    memberInfo.fieldSize = field->typeInfo != ZR_NULL ? calculate_type_size(cs, field->typeInfo) : 0;
                    if (memberInfo.name != ZR_NULL) {
                        ZrCore_Array_Push(cs->state, &info.members, &memberInfo);
                    }
                    break;
                }
                case ZR_AST_INTERFACE_METHOD_SIGNATURE: {
                    SZrInterfaceMethodSignature *method = &member->data.interfaceMethodSignature;
                    SZrTypeMemberInfo memberInfo;

                    compiler_interface_init_member_defaults(&memberInfo);
                    memberInfo.memberType = ZR_AST_CLASS_METHOD;
                    memberInfo.declarationNode = member;
                    memberInfo.accessModifier = method->access;
                    memberInfo.name = method->name != ZR_NULL ? method->name->name : ZR_NULL;
                    memberInfo.returnTypeName =
                            method->returnType != ZR_NULL ? extract_type_name_string(cs, method->returnType) : ZR_NULL;
                    ZrCore_Array_Init(cs->state,
                                      &memberInfo.genericParameters,
                                      sizeof(SZrTypeGenericParameterInfo),
                                      method->generic != ZR_NULL && method->generic->params != ZR_NULL
                                              ? method->generic->params->count
                                              : 1);
                    compiler_collect_generic_parameter_info(cs, &memberInfo.genericParameters, method->generic);
                    compiler_interface_collect_parameter_types(cs, &memberInfo.parameterTypes, method->params);
                    compiler_collect_parameter_passing_modes(cs->state,
                                                             &memberInfo.parameterPassingModes,
                                                             method->params);
                    memberInfo.parameterCount = (TZrUInt32)memberInfo.parameterTypes.length;
                    if (memberInfo.name != ZR_NULL) {
                        ZrCore_Array_Push(cs->state, &info.members, &memberInfo);
                    }
                    break;
                }
                case ZR_AST_INTERFACE_PROPERTY_SIGNATURE: {
                    SZrInterfacePropertySignature *property = &member->data.interfacePropertySignature;
                    if (property->name == ZR_NULL || property->name->name == ZR_NULL) {
                        break;
                    }

                    if (property->hasGet) {
                        SZrTypeMemberInfo getterInfo;
                        compiler_interface_init_member_defaults(&getterInfo);
                        getterInfo.memberType = ZR_AST_CLASS_METHOD;
                        getterInfo.declarationNode = member;
                        getterInfo.accessModifier = property->access;
                        getterInfo.name =
                                compiler_create_hidden_property_accessor_name(cs, property->name->name, ZR_FALSE);
                        getterInfo.returnTypeName =
                                property->typeInfo != ZR_NULL ? extract_type_name_string(cs, property->typeInfo)
                                                              : ZrCore_String_CreateFromNative(cs->state, "object");
                        if (getterInfo.name != ZR_NULL) {
                            ZrCore_Array_Push(cs->state, &info.members, &getterInfo);
                        }
                    }

                    if (property->hasSet) {
                        SZrTypeMemberInfo setterInfo;
                        compiler_interface_init_member_defaults(&setterInfo);
                        setterInfo.memberType = ZR_AST_CLASS_METHOD;
                        setterInfo.declarationNode = member;
                        setterInfo.accessModifier = property->access;
                        setterInfo.name =
                                compiler_create_hidden_property_accessor_name(cs, property->name->name, ZR_TRUE);
                        ZrCore_Array_Init(cs->state, &setterInfo.parameterTypes, sizeof(SZrInferredType), 1);
                        compiler_interface_append_parameter_type(cs, &setterInfo.parameterTypes, property->typeInfo);
                        {
                            EZrParameterPassingMode passingMode = ZR_PARAMETER_PASSING_MODE_VALUE;
                            ZrCore_Array_Init(cs->state,
                                              &setterInfo.parameterPassingModes,
                                              sizeof(EZrParameterPassingMode),
                                              1);
                            ZrCore_Array_Push(cs->state, &setterInfo.parameterPassingModes, &passingMode);
                        }
                        setterInfo.parameterCount = (TZrUInt32)setterInfo.parameterTypes.length;
                        if (setterInfo.name != ZR_NULL) {
                            ZrCore_Array_Push(cs->state, &info.members, &setterInfo);
                        }
                    }
                    break;
                }
                case ZR_AST_INTERFACE_META_SIGNATURE: {
                    SZrInterfaceMetaSignature *metaSignature = &member->data.interfaceMetaSignature;
                    SZrTypeMemberInfo memberInfo;

                    compiler_interface_init_member_defaults(&memberInfo);
                    memberInfo.memberType = ZR_AST_CLASS_META_FUNCTION;
                    memberInfo.declarationNode = member;
                    memberInfo.accessModifier = metaSignature->access;
                    memberInfo.name = metaSignature->meta != ZR_NULL ? metaSignature->meta->name : ZR_NULL;
                    memberInfo.metaType = compiler_interface_resolve_meta_type(memberInfo.name);
                    memberInfo.isMetaMethod = memberInfo.metaType != ZR_META_ENUM_MAX;
                    memberInfo.returnTypeName = metaSignature->returnType != ZR_NULL
                                                        ? extract_type_name_string(cs, metaSignature->returnType)
                                                        : ZR_NULL;
                    compiler_interface_collect_parameter_types(cs, &memberInfo.parameterTypes, metaSignature->params);
                    compiler_collect_parameter_passing_modes(cs->state,
                                                             &memberInfo.parameterPassingModes,
                                                             metaSignature->params);
                    memberInfo.parameterCount = (TZrUInt32)memberInfo.parameterTypes.length;
                    if (memberInfo.name != ZR_NULL) {
                        ZrCore_Array_Push(cs->state, &info.members, &memberInfo);
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }

    ZrCore_Array_Push(cs->state, &cs->typePrototypes, &info);
    if (cs->typeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_RegisterType(cs->state, cs->typeEnv, typeName);
    }
    if (!cs->hasError) {
        compiler_validate_interface_variance_rules(cs, node);
    }

    cs->currentTypeName = oldTypeName;
    cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
}
