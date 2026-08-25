//
// Created by Auto on 2026/04/01.
//

#include "compiler_internal.h"
#include "compiler_parameter_metadata.h"

#include <stddef.h>

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
                                                       SZrAstNodeArray *params,
                                                       SZrAstNode *functionNode) {
    SZrAstNode *previousFunctionNode;

    if (cs == ZR_NULL || parameterTypes == ZR_NULL) {
        return;
    }

    previousFunctionNode = cs->currentFunctionNode;
    if (functionNode != ZR_NULL) {
        cs->currentFunctionNode = functionNode;
    }

    if (params == ZR_NULL || params->count == 0) {
        cs->currentFunctionNode = previousFunctionNode;
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

    cs->currentFunctionNode = previousFunctionNode;
}

static void compiler_interface_init_member_defaults(SZrTypeMemberInfo *memberInfo) {
    if (memberInfo == ZR_NULL) {
        return;
    }

    memset(memberInfo, 0, sizeof(*memberInfo));
    memberInfo->minArgumentCount = ZR_MEMBER_PARAMETER_COUNT_UNKNOWN;
    memberInfo->accessModifier = ZR_ACCESS_PRIVATE;
    memberInfo->modifierFlags = ZR_DECLARATION_MODIFIER_ABSTRACT | ZR_DECLARATION_MODIFIER_VIRTUAL;
    memberInfo->ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    memberInfo->receiverQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    memberInfo->receiverEffect = ZR_CANONICAL_RECEIVER_NONE;
    memberInfo->metaType = ZR_META_ENUM_MAX;
    memberInfo->virtualSlotIndex = (TZrUInt32)-1;
    memberInfo->interfaceContractSlot = (TZrUInt32)-1;
    memberInfo->propertyIdentity = (TZrUInt32)-1;
    memberInfo->accessorRole = 0;
    ZrCore_Array_Construct(&memberInfo->parameterTypes);
    ZrCore_Array_Construct(&memberInfo->parameterNames);
    ZrCore_Array_Construct(&memberInfo->parameterHasDefaultValues);
    ZrCore_Array_Construct(&memberInfo->parameterDefaultValues);
    ZrCore_Array_Construct(&memberInfo->genericParameters);
    ZrCore_Array_Construct(&memberInfo->parameterPassingModes);
    ZrCore_Array_Construct(&memberInfo->decorators);
    memberInfo->hasDecoratorMetadata = ZR_FALSE;
    ZrCore_Value_ResetAsNull(&memberInfo->decoratorMetadataValue);
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
    info.modifierFlags = ZR_DECLARATION_MODIFIER_ABSTRACT;
    info.isImportedNative = ZR_FALSE;
    info.allowValueConstruction = ZR_FALSE;
    info.allowBoxedConstruction = ZR_FALSE;
    info.nextVirtualSlotIndex = 0;
    info.nextPropertyIdentity = 0;
    info.layoutByteSize = 0;
    info.layoutByteAlign = 0;

    ZrCore_Array_Init(cs->state, &info.inherits, sizeof(SZrString *), ZR_PARSER_INITIAL_CAPACITY_TINY);
    ZrCore_Array_Init(cs->state, &info.implements, sizeof(SZrString *), 1);
    ZrCore_Array_Init(cs->state,
                      &info.genericParameters,
                      sizeof(SZrTypeGenericParameterInfo),
                      interfaceDecl->generic != ZR_NULL && interfaceDecl->generic->params != ZR_NULL
                              ? interfaceDecl->generic->params->count
                              : 1);
    ZrCore_Array_Init(cs->state, &info.decorators, sizeof(SZrTypeDecoratorInfo), ZR_PARSER_INITIAL_CAPACITY_TINY);
    ZrCore_Array_Init(cs->state, &info.members, sizeof(SZrTypeMemberInfo), ZR_PARSER_INITIAL_CAPACITY_SMALL);
    compiler_collect_generic_parameter_info(cs, &info.genericParameters, interfaceDecl->generic);
    if (info.genericParameters.length > 0u) {
        info.modifierFlags |= ZR_TYPE_MODIFIER_FLAG_OPEN_GENERIC;
    }

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

            if (member->type == ZR_AST_PROPERTY_DECLARATION) {
                if (!compiler_property_bind(
                            cs,
                            &info,
                            member,
                            typeName,
                            ZR_NULL,
                            ZR_COMPILER_PROPERTY_CONTAINER_INTERFACE,
                            (TZrUInt32)index,
                            ZR_TRUE)) {
                    cs->currentTypeName = oldTypeName;
                    cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
                    return;
                }
                continue;
            }
            if (member->type == ZR_AST_INTERFACE_PROPERTY_SIGNATURE) {
                ZrParser_Compiler_Error(
                        cs,
                        "legacy property accessor syntax is not a semantic source",
                        member->location);
                cs->currentTypeName = oldTypeName;
                cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
                return;
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
                    memberInfo.ownerTypeName = typeName;
                    memberInfo.baseDefinitionOwnerTypeName = typeName;
                    memberInfo.baseDefinitionName = memberInfo.name;
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
                    SZrAstNode *previousFunctionNode = cs->currentFunctionNode;

                    compiler_interface_init_member_defaults(&memberInfo);
                    memberInfo.memberType = ZR_AST_CLASS_METHOD;
                    memberInfo.declarationNode = member;
                    memberInfo.accessModifier = method->access;
                    memberInfo.receiverEffect = get_member_receiver_effect(member);
                    memberInfo.name = method->name != ZR_NULL ? method->name->name : ZR_NULL;
                    memberInfo.ownerTypeName = typeName;
                    memberInfo.baseDefinitionOwnerTypeName = typeName;
                    memberInfo.baseDefinitionName = memberInfo.name;
                    memberInfo.virtualSlotIndex = info.nextVirtualSlotIndex++;
                    memberInfo.interfaceContractSlot = memberInfo.virtualSlotIndex;
                    cs->currentFunctionNode = member;
                    compiler_type_member_capture_structured_return_type(
                            cs,
                            &memberInfo,
                            method->returnType);
                    ZrCore_Array_Init(cs->state,
                                      &memberInfo.genericParameters,
                                      sizeof(SZrTypeGenericParameterInfo),
                                      method->generic != ZR_NULL && method->generic->params != ZR_NULL
                                              ? method->generic->params->count
                                              : 1);
                    compiler_collect_generic_parameter_info(cs, &memberInfo.genericParameters, method->generic);
                    compiler_interface_collect_parameter_types(cs, &memberInfo.parameterTypes, method->params, member);
                    compiler_collect_parameter_passing_modes(cs->state,
                                                             &memberInfo.parameterPassingModes,
                                                             method->params);
                    memberInfo.parameterCount = (TZrUInt32)memberInfo.parameterTypes.length;
                    cs->currentFunctionNode = previousFunctionNode;
                    if (memberInfo.name != ZR_NULL) {
                        ZrCore_Array_Push(cs->state, &info.members, &memberInfo);
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
                    memberInfo.ownerTypeName = typeName;
                    memberInfo.baseDefinitionOwnerTypeName = typeName;
                    memberInfo.baseDefinitionName = memberInfo.name;
                    memberInfo.metaType = compiler_resolve_meta_type_name(memberInfo.name);
                    memberInfo.isMetaMethod = memberInfo.metaType != ZR_META_ENUM_MAX;
                    memberInfo.receiverEffect =
                            memberInfo.metaType == ZR_META_CONSTRUCTOR
                                    ? ZR_CANONICAL_RECEIVER_NONE
                                    : ZR_CANONICAL_RECEIVER_MUTABLE;
                    memberInfo.virtualSlotIndex = memberInfo.metaType == ZR_META_CONSTRUCTOR
                                                          ? (TZrUInt32)-1
                                                          : info.nextVirtualSlotIndex++;
                    memberInfo.interfaceContractSlot = memberInfo.virtualSlotIndex;
                    compiler_type_member_capture_structured_return_type(
                            cs,
                            &memberInfo,
                            metaSignature->returnType);
                    compiler_interface_collect_parameter_types(cs, &memberInfo.parameterTypes, metaSignature->params, member);
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
        ZrParser_TypeEnvironment_RegisterTypeDeclaration(
                cs->state, cs->typeEnv, typeName, node);
    }
    if (!cs->hasError) {
        compiler_validate_interface_variance_rules(cs, node);
    }

    cs->currentTypeName = oldTypeName;
    cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
}

static SZrTypePrototypeInfo *compiler_interface_find_prototype(
        SZrCompilerState *cs,
        SZrString *name) {
    if (cs == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrSize index = 0U; index < cs->typePrototypes.length; index++) {
        SZrTypePrototypeInfo *info =
                (SZrTypePrototypeInfo *)ZrCore_Array_Get(
                        &cs->typePrototypes, index);
        if (info != ZR_NULL && info->type == ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE &&
            info->name != ZR_NULL && ZrCore_String_Equal(info->name, name)) {
            return info;
        }
    }
    return ZR_NULL;
}

static TZrBool compiler_interface_attach_variadic_parameter_metadata(
        SZrCompilerState *cs,
        SZrTypeMemberInfo *memberInfo,
        SZrParameter *args,
        SZrAstNode *functionNode) {
    SZrAstNode *argsNode;
    SZrAstNode *nodes[1];
    SZrAstNodeArray params;

    if (args == ZR_NULL) {
        return ZR_TRUE;
    }
    argsNode = (SZrAstNode *)(
            (TZrByte *)args - offsetof(SZrAstNode, data.parameter));
    nodes[0] = argsNode;
    params.nodes = nodes;
    params.count = 1U;
    params.capacity = 1U;
    return compiler_parameter_metadata_attach_member_array(
            cs,
            memberInfo,
            &params,
            functionNode,
            "variadicParameters");
}

void compiler_finalize_interface_decorators(
        SZrCompilerState *cs,
        SZrAstNode *node) {
    SZrInterfaceDeclaration *declaration;
    SZrTypePrototypeInfo *info;
    SZrString *previousTypeName;

    if (cs == ZR_NULL || node == ZR_NULL ||
        node->type != ZR_AST_INTERFACE_DECLARATION || cs->hasError) {
        return;
    }
    declaration = &node->data.interfaceDeclaration;
    if (declaration->name == ZR_NULL || declaration->name->name == ZR_NULL) {
        return;
    }
    info = compiler_interface_find_prototype(cs, declaration->name->name);
    if (info == ZR_NULL) {
        ZrParser_Compiler_Error(
                cs,
                "interface.decorator_finalize: signature prototype is missing",
                node->location);
        return;
    }
    previousTypeName = cs->currentTypeName;
    cs->currentTypeName = info->name;
    for (TZrSize index = 0U; index < info->members.length; index++) {
        SZrTypeMemberInfo *memberInfo =
                (SZrTypeMemberInfo *)ZrCore_Array_Get(&info->members, index);
        SZrAstNode *memberNode =
                memberInfo != ZR_NULL ? memberInfo->declarationNode : ZR_NULL;
        SZrAstNodeArray *params = ZR_NULL;
        SZrParameter *args = ZR_NULL;

        if (memberNode == ZR_NULL) {
            continue;
        }
        if (memberNode->type == ZR_AST_INTERFACE_METHOD_SIGNATURE) {
            params = memberNode->data.interfaceMethodSignature.params;
            args = memberNode->data.interfaceMethodSignature.args;
        } else if (memberNode->type == ZR_AST_INTERFACE_META_SIGNATURE) {
            params = memberNode->data.interfaceMetaSignature.params;
            args = memberNode->data.interfaceMetaSignature.args;
        } else {
            continue;
        }
        if ((params != ZR_NULL && params->count > 0U) &&
            !compiler_parameter_metadata_attach_member_array(
                    cs,
                    memberInfo,
                    params,
                    memberNode,
                    "parameters")) {
            break;
        }
        if (!compiler_interface_attach_variadic_parameter_metadata(
                    cs, memberInfo, args, memberNode)) {
            break;
        }
    }
    cs->currentTypeName = previousTypeName;
}
