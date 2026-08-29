#include "type_inference_semantic_facts.h"

#include <string.h>

#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/type_inference.h"

#include "type_inference_internal.h"

static SZrAstNodeArray *type_inference_source_type_members(
        SZrAstNode *declaration) {
    if (declaration == ZR_NULL) {
        return ZR_NULL;
    }
    if (declaration->type == ZR_AST_CLASS_DECLARATION) {
        return declaration->data.classDeclaration.members;
    }
    if (declaration->type == ZR_AST_STRUCT_DECLARATION) {
        return declaration->data.structDeclaration.members;
    }
    return ZR_NULL;
}

static SZrAstNode *type_inference_source_type_declaration_in_array(
        SZrAstNodeArray *declarations,
        SZrString *typeName) {
    TZrSize index;

    if (declarations == ZR_NULL || declarations->nodes == ZR_NULL ||
        typeName == ZR_NULL) {
        return ZR_NULL;
    }
    for (index = 0U; index < declarations->count; index++) {
        SZrAstNode *declaration = declarations->nodes[index];
        SZrIdentifier *name = ZR_NULL;

        if (declaration != ZR_NULL &&
            declaration->type == ZR_AST_CLASS_DECLARATION) {
            name = declaration->data.classDeclaration.name;
        } else if (declaration != ZR_NULL &&
                   declaration->type == ZR_AST_STRUCT_DECLARATION) {
            name = declaration->data.structDeclaration.name;
        }
        if (name != ZR_NULL && name->name != ZR_NULL &&
            ZrCore_String_Equal(name->name, typeName)) {
            return declaration;
        }
    }
    return ZR_NULL;
}

static SZrAstNode *type_inference_source_constructor_declaration(
        SZrCompilerState *cs,
        SZrAstNode *target,
        SZrString *typeName) {
    SZrString *resolvedTypeName = ZR_NULL;
    SZrString *prototypeTypeName = ZR_NULL;
    SZrTypePrototypeInfo *prototype = ZR_NULL;
    EZrObjectPrototypeType prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_INVALID;
    SZrAstNode *typeDeclaration;
    SZrAstNodeArray *members;
    TZrSize index;

    if (cs == ZR_NULL || cs->scriptAst == ZR_NULL ||
        cs->scriptAst->type != ZR_AST_SCRIPT || target == ZR_NULL ||
        typeName == ZR_NULL ||
        !resolve_source_type_declaration_target_inference(
                cs,
                target,
                &resolvedTypeName,
                &prototypeType,
                ZR_NULL,
                ZR_NULL) ||
        resolvedTypeName == ZR_NULL ||
        !ZrCore_String_Equal(resolvedTypeName, typeName) ||
        (prototypeType != ZR_OBJECT_PROTOTYPE_TYPE_CLASS &&
         prototypeType != ZR_OBJECT_PROTOTYPE_TYPE_STRUCT)) {
        return ZR_NULL;
    }
    typeDeclaration = resolve_prototype_target_inference(
                              cs,
                              target,
                              &prototype,
                              &prototypeTypeName) &&
                              prototype != ZR_NULL &&
                              prototype->declarationNode != ZR_NULL &&
                              prototypeTypeName != ZR_NULL &&
                              ZrCore_String_Equal(
                                      prototypeTypeName, resolvedTypeName)
                              ? prototype->declarationNode
                              : type_inference_source_type_declaration_in_array(
                                        cs->scriptAst->data.script.statements,
                                        resolvedTypeName);
    members = type_inference_source_type_members(typeDeclaration);
    if (members == ZR_NULL) {
        return ZR_NULL;
    }
    for (index = 0U; index < members->count; index++) {
        SZrAstNode *member = members->nodes[index];
        SZrIdentifier *meta = ZR_NULL;
        const TZrChar *metaName;

        if (member != ZR_NULL && member->type == ZR_AST_CLASS_META_FUNCTION) {
            meta = member->data.classMetaFunction.meta;
        } else if (member != ZR_NULL &&
                   member->type == ZR_AST_STRUCT_META_FUNCTION) {
            meta = member->data.structMetaFunction.meta;
        }
        metaName = meta != ZR_NULL && meta->name != ZR_NULL
                           ? ZrCore_String_GetNativeStringShort(meta->name)
                           : ZR_NULL;
        if (metaName != ZR_NULL && strcmp(metaName, "constructor") == 0) {
            return member;
        }
    }
    return ZR_NULL;
}

void type_inference_source_constructor_member_free(
        SZrCompilerState *cs,
        SZrTypeMemberInfo *member) {
    TZrSize index;

    if (cs == ZR_NULL || member == ZR_NULL) {
        return;
    }
    for (index = 0U; index < member->parameterTypes.length; index++) {
        SZrInferredType *type = (SZrInferredType *)ZrCore_Array_Get(
                &member->parameterTypes, index);
        if (type != ZR_NULL) {
            ZrParser_InferredType_Free(cs->state, type);
        }
    }
    if (member->parameterTypes.isValid) {
        ZrCore_Array_Free(cs->state, &member->parameterTypes);
    }
    if (member->parameterNames.isValid) {
        ZrCore_Array_Free(cs->state, &member->parameterNames);
    }
    if (member->parameterHasDefaultValues.isValid) {
        ZrCore_Array_Free(cs->state, &member->parameterHasDefaultValues);
    }
    if (member->parameterPassingModes.isValid) {
        ZrCore_Array_Free(cs->state, &member->parameterPassingModes);
    }
}

TZrBool type_inference_source_constructor_member_build(
        SZrCompilerState *cs,
        SZrAstNode *target,
        SZrString *typeName,
        SZrTypeMemberInfo *outMember) {
    SZrAstNode *declaration;
    SZrAstNodeArray *parameters;
    SZrIdentifier *meta;
    EZrAccessModifier access;
    TZrSize index;

    if (outMember != ZR_NULL) {
        memset(outMember, 0, sizeof(*outMember));
    }
    if (cs == ZR_NULL || typeName == ZR_NULL || outMember == ZR_NULL ||
        (declaration = type_inference_source_constructor_declaration(
                 cs, target, typeName)) == ZR_NULL) {
        return ZR_FALSE;
    }
    if (declaration->type == ZR_AST_CLASS_META_FUNCTION) {
        meta = declaration->data.classMetaFunction.meta;
        access = declaration->data.classMetaFunction.access;
    } else {
        meta = declaration->data.structMetaFunction.meta;
        access = declaration->data.structMetaFunction.access;
    }
    parameters = declaration->type == ZR_AST_CLASS_META_FUNCTION
                         ? declaration->data.classMetaFunction.params
                         : declaration->data.structMetaFunction.params;
    if (meta == ZR_NULL || meta->name == ZR_NULL) {
        return ZR_FALSE;
    }

    outMember->memberType = declaration->type;
    outMember->name = meta->name;
    outMember->accessModifier = access;
    outMember->receiverEffect = ZR_CANONICAL_RECEIVER_NONE;
    outMember->ownerTypeName = typeName;
    outMember->declarationNode = declaration;
    outMember->metaType = ZR_META_CONSTRUCTOR;
    outMember->isMetaMethod = ZR_TRUE;
    outMember->symbolId = ZR_SEMANTIC_ID_INVALID;
    outMember->parameterCount =
            parameters != ZR_NULL ? (TZrUInt32)parameters->count : 0U;
    outMember->minArgumentCount = outMember->parameterCount;
    ZrCore_Array_Construct(&outMember->parameterTypes);
    ZrCore_Array_Construct(&outMember->parameterNames);
    ZrCore_Array_Construct(&outMember->parameterHasDefaultValues);
    ZrCore_Array_Construct(&outMember->parameterPassingModes);
    ZrCore_Array_Construct(&outMember->genericParameters);
    if (parameters == ZR_NULL || parameters->count == 0U) {
        return ZR_TRUE;
    }

    ZrCore_Array_Init(cs->state,
                      &outMember->parameterTypes,
                      sizeof(SZrInferredType),
                      parameters->count);
    ZrCore_Array_Init(cs->state,
                      &outMember->parameterNames,
                      sizeof(SZrString *),
                      parameters->count);
    ZrCore_Array_Init(cs->state,
                      &outMember->parameterHasDefaultValues,
                      sizeof(TZrBool),
                      parameters->count);
    ZrCore_Array_Init(cs->state,
                      &outMember->parameterPassingModes,
                      sizeof(EZrParameterPassingMode),
                      parameters->count);
    for (index = 0U; index < parameters->count; index++) {
        SZrAstNode *parameterNode = parameters->nodes[index];
        SZrParameter *parameter;
        SZrInferredType parameterType;
        SZrString *parameterName;
        TZrBool hasDefault;

        if (parameterNode == ZR_NULL || parameterNode->type != ZR_AST_PARAMETER ||
            parameterNode->data.parameter.typeInfo == ZR_NULL) {
            type_inference_source_constructor_member_free(cs, outMember);
            return ZR_FALSE;
        }
        parameter = &parameterNode->data.parameter;
        if (!ZrParser_AstTypeToInferredType_Convert(
                    cs, parameter->typeInfo, &parameterType)) {
            type_inference_source_constructor_member_free(cs, outMember);
            return ZR_FALSE;
        }
        parameterName = parameter->name != ZR_NULL
                                ? parameter->name->name
                                : ZR_NULL;
        hasDefault = parameter->defaultValue != ZR_NULL;
        if (hasDefault && outMember->minArgumentCount > 0U) {
            outMember->minArgumentCount--;
        }
        ZrCore_Array_Push(cs->state, &outMember->parameterTypes, &parameterType);
        ZrCore_Array_Push(cs->state, &outMember->parameterNames, &parameterName);
        ZrCore_Array_Push(
                cs->state, &outMember->parameterHasDefaultValues, &hasDefault);
        ZrCore_Array_Push(cs->state,
                          &outMember->parameterPassingModes,
                          &parameter->passingMode);
    }
    return ZR_TRUE;
}
