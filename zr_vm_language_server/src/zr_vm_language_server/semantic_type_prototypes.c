#include "semantic/semantic_analyzer_internal.h"

#include <stdarg.h>
#include <stdlib.h>

SZrTypePrototypeInfo *find_compiler_type_prototype_inference(SZrCompilerState *cs, SZrString *typeName);

typedef struct SZrSemanticPrototypeContextSnapshot {
    SZrTypePrototypeInfo *typePrototype;
    SZrAstNode *typeNode;
    SZrString *typeName;
    SZrAstNode *functionNode;
} SZrSemanticPrototypeContextSnapshot;

static SZrTypePrototypeInfo *semantic_type_prototypes_find_exact(SZrCompilerState *compilerState,
                                                                 SZrString *typeName) {
    if (compilerState == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < compilerState->typePrototypes.length; index++) {
        SZrTypePrototypeInfo *prototype =
            (SZrTypePrototypeInfo *)ZrCore_Array_Get(&compilerState->typePrototypes, index);
        if (prototype != ZR_NULL &&
            prototype->name != ZR_NULL &&
            ZrCore_String_Equal(prototype->name, typeName)) {
            return prototype;
        }
    }

    return ZR_NULL;
}

static SZrString *semantic_type_prototypes_owner_name(SZrAstNode *ownerTypeNode) {
    if (ownerTypeNode == ZR_NULL) {
        return ZR_NULL;
    }

    switch (ownerTypeNode->type) {
        case ZR_AST_CLASS_DECLARATION:
            return ownerTypeNode->data.classDeclaration.name != ZR_NULL
                       ? ownerTypeNode->data.classDeclaration.name->name
                       : ZR_NULL;
        case ZR_AST_STRUCT_DECLARATION:
            return ownerTypeNode->data.structDeclaration.name != ZR_NULL
                       ? ownerTypeNode->data.structDeclaration.name->name
                       : ZR_NULL;
        case ZR_AST_INTERFACE_DECLARATION:
            return ownerTypeNode->data.interfaceDeclaration.name != ZR_NULL
                       ? ownerTypeNode->data.interfaceDeclaration.name->name
                       : ZR_NULL;
        case ZR_AST_ENUM_DECLARATION:
            return ownerTypeNode->data.enumDeclaration.name != ZR_NULL
                       ? ownerTypeNode->data.enumDeclaration.name->name
                       : ZR_NULL;
        default:
            return ZR_NULL;
    }
}

static void semantic_type_prototypes_push_context(SZrSemanticAnalyzer *analyzer,
                                                  SZrAstNode *ownerTypeNode,
                                                  SZrAstNode *functionNode,
                                                  SZrSemanticPrototypeContextSnapshot *snapshot) {
    SZrCompilerState *compilerState;
    SZrString *typeName;

    if (snapshot != ZR_NULL) {
        memset(snapshot, 0, sizeof(*snapshot));
    }

    if (analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL || snapshot == ZR_NULL) {
        return;
    }

    compilerState = analyzer->compilerState;
    snapshot->typePrototype = compilerState->currentTypePrototypeInfo;
    snapshot->typeNode = compilerState->currentTypeNode;
    snapshot->typeName = compilerState->currentTypeName;
    snapshot->functionNode = compilerState->currentFunctionNode;

    typeName = semantic_type_prototypes_owner_name(ownerTypeNode);
    if (typeName != ZR_NULL) {
        compilerState->currentTypeNode = ownerTypeNode;
        compilerState->currentTypeName = typeName;
        compilerState->currentTypePrototypeInfo = semantic_type_prototypes_find_exact(compilerState, typeName);
    }

    if (functionNode != ZR_NULL) {
        compilerState->currentFunctionNode = functionNode;
    }
}

static void semantic_type_prototypes_pop_context(SZrSemanticAnalyzer *analyzer,
                                                 const SZrSemanticPrototypeContextSnapshot *snapshot) {
    SZrCompilerState *compilerState;

    if (analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL || snapshot == ZR_NULL) {
        return;
    }

    compilerState = analyzer->compilerState;
    compilerState->currentTypePrototypeInfo = snapshot->typePrototype;
    compilerState->currentTypeNode = snapshot->typeNode;
    compilerState->currentTypeName = snapshot->typeName;
    compilerState->currentFunctionNode = snapshot->functionNode;
}

static void semantic_type_prototypes_init_prototype(SZrState *state,
                                                    SZrTypePrototypeInfo *info,
                                                    SZrString *name,
                                                    EZrObjectPrototypeType prototypeType,
                                                    EZrAccessModifier accessModifier) {
    if (state == ZR_NULL || info == ZR_NULL) {
        return;
    }

    memset(info, 0, sizeof(*info));
    info->name = name;
    info->type = prototypeType;
    info->accessModifier = accessModifier;
    info->isImportedNative = ZR_FALSE;
    info->allowValueConstruction =
        prototypeType != ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE && prototypeType != ZR_OBJECT_PROTOTYPE_TYPE_MODULE;
    info->allowBoxedConstruction =
        prototypeType != ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE && prototypeType != ZR_OBJECT_PROTOTYPE_TYPE_MODULE;
    info->nextVirtualSlotIndex = 0;
    info->nextPropertyIdentity = 0;
    ZrCore_Value_ResetAsNull(&info->decoratorMetadataValue);
    ZrCore_Array_Init(state, &info->inherits, sizeof(SZrString *), ZR_PARSER_INITIAL_CAPACITY_TINY);
    ZrCore_Array_Init(state, &info->implements, sizeof(SZrString *), ZR_PARSER_INITIAL_CAPACITY_PAIR);
    ZrCore_Array_Init(state,
                      &info->genericParameters,
                      sizeof(SZrTypeGenericParameterInfo),
                      ZR_PARSER_INITIAL_CAPACITY_PAIR);
    ZrCore_Array_Init(state, &info->decorators, sizeof(SZrTypeDecoratorInfo), ZR_PARSER_INITIAL_CAPACITY_TINY);
    ZrCore_Array_Init(state, &info->members, sizeof(SZrTypeMemberInfo), ZR_PARSER_INITIAL_CAPACITY_SMALL);
}

static void semantic_type_prototypes_init_member_defaults(SZrTypeMemberInfo *memberInfo) {
    if (memberInfo == ZR_NULL) {
        return;
    }

    memset(memberInfo, 0, sizeof(*memberInfo));
    memberInfo->minArgumentCount = ZR_MEMBER_PARAMETER_COUNT_UNKNOWN;
    memberInfo->virtualSlotIndex = (TZrUInt32)-1;
    memberInfo->interfaceContractSlot = (TZrUInt32)-1;
    memberInfo->propertyIdentity = (TZrUInt32)-1;
    memberInfo->metaType = ZR_META_ENUM_MAX;
    ZrCore_Array_Construct(&memberInfo->parameterTypes);
    ZrCore_Array_Construct(&memberInfo->parameterNames);
    ZrCore_Array_Construct(&memberInfo->parameterHasDefaultValues);
    ZrCore_Array_Construct(&memberInfo->parameterDefaultValues);
    ZrCore_Array_Construct(&memberInfo->genericParameters);
    ZrCore_Array_Construct(&memberInfo->parameterPassingModes);
    ZrCore_Array_Construct(&memberInfo->decorators);
    ZrCore_Value_ResetAsNull(&memberInfo->decoratorMetadataValue);
}

static SZrString *semantic_type_prototypes_create_hidden_property_accessor_name(SZrState *state,
                                                                                SZrString *propertyName,
                                                                                TZrBool isSetter) {
    const TZrChar *propertyNameText;
    const TZrChar *prefix = isSetter ? "__set_" : "__get_";
    TZrChar buffer[ZR_PARSER_DECLARATION_BUFFER_LENGTH];
    TZrInt32 written;

    if (state == ZR_NULL || propertyName == ZR_NULL) {
        return ZR_NULL;
    }

    propertyNameText = semantic_string_native(propertyName);
    if (propertyNameText == ZR_NULL || propertyNameText[0] == '\0') {
        return ZR_NULL;
    }

    written = snprintf(buffer, sizeof(buffer), "%s%s", prefix, propertyNameText);
    if (written <= 0 || (TZrSize)written >= sizeof(buffer)) {
        return ZR_NULL;
    }

    return ZrCore_String_Create(state, buffer, (TZrSize)written);
}

static TZrBool semantic_type_prototypes_append_text(TZrChar *buffer,
                                                    TZrSize bufferSize,
                                                    TZrSize *offset,
                                                    const TZrChar *text) {
    TZrSize textLength;

    if (buffer == ZR_NULL || offset == ZR_NULL || text == ZR_NULL) {
        return ZR_FALSE;
    }

    textLength = strlen(text);
    if (*offset + textLength >= bufferSize) {
        return ZR_FALSE;
    }

    memcpy(buffer + *offset, text, textLength);
    *offset += textLength;
    buffer[*offset] = '\0';
    return ZR_TRUE;
}

static TZrBool semantic_type_prototypes_append_format(TZrChar *buffer,
                                                      TZrSize bufferSize,
                                                      TZrSize *offset,
                                                      const TZrChar *format,
                                                      ...) {
    va_list args;
    TZrInt32 written;

    if (buffer == ZR_NULL || offset == ZR_NULL || format == ZR_NULL || *offset >= bufferSize) {
        return ZR_FALSE;
    }

    va_start(args, format);
    written = vsnprintf(buffer + *offset, bufferSize - *offset, format, args);
    va_end(args);
    if (written < 0 || (TZrSize)written >= bufferSize - *offset) {
        return ZR_FALSE;
    }

    *offset += (TZrSize)written;
    return ZR_TRUE;
}

static SZrString *semantic_type_prototypes_render_generic_argument(SZrSemanticAnalyzer *analyzer,
                                                                   SZrAstNode *node);

static SZrString *semantic_type_prototypes_render_type_name_node(SZrSemanticAnalyzer *analyzer,
                                                                 SZrAstNode *typeNameNode) {
    TZrChar buffer[ZR_PARSER_DECLARATION_BUFFER_LENGTH];
    TZrSize offset = 0;

    if (analyzer == ZR_NULL || typeNameNode == ZR_NULL) {
        return ZR_NULL;
    }

    if (typeNameNode->type == ZR_AST_IDENTIFIER_LITERAL) {
        return typeNameNode->data.identifier.name;
    }

    buffer[0] = '\0';
    if (typeNameNode->type == ZR_AST_GENERIC_TYPE) {
        SZrGenericType *genericType = &typeNameNode->data.genericType;

        if (genericType->name == ZR_NULL || genericType->name->name == ZR_NULL ||
            !semantic_type_prototypes_append_text(buffer,
                                                  sizeof(buffer),
                                                  &offset,
                                                  semantic_string_native(genericType->name->name)) ||
            !semantic_type_prototypes_append_text(buffer, sizeof(buffer), &offset, "<")) {
            return ZR_NULL;
        }

        if (genericType->params != ZR_NULL) {
            for (TZrSize index = 0; index < genericType->params->count; index++) {
                SZrString *argumentName =
                    semantic_type_prototypes_render_generic_argument(analyzer, genericType->params->nodes[index]);

                if (index > 0 &&
                    !semantic_type_prototypes_append_text(buffer, sizeof(buffer), &offset, ", ")) {
                    return ZR_NULL;
                }
                if (argumentName == ZR_NULL ||
                    !semantic_type_prototypes_append_text(buffer,
                                                          sizeof(buffer),
                                                          &offset,
                                                          semantic_string_native(argumentName))) {
                    return ZR_NULL;
                }
            }
        }

        if (!semantic_type_prototypes_append_text(buffer, sizeof(buffer), &offset, ">")) {
            return ZR_NULL;
        }
        return ZrCore_String_Create(analyzer->compilerState->state, buffer, offset);
    }

    if (typeNameNode->type == ZR_AST_TUPLE_TYPE) {
        SZrTupleType *tupleType = &typeNameNode->data.tupleType;

        if (!semantic_type_prototypes_append_text(buffer, sizeof(buffer), &offset, "(")) {
            return ZR_NULL;
        }

        if (tupleType->elements != ZR_NULL) {
            for (TZrSize index = 0; index < tupleType->elements->count; index++) {
                SZrString *elementName =
                    semantic_type_prototypes_render_generic_argument(analyzer, tupleType->elements->nodes[index]);

                if (index > 0 &&
                    !semantic_type_prototypes_append_text(buffer, sizeof(buffer), &offset, ", ")) {
                    return ZR_NULL;
                }
                if (elementName == ZR_NULL ||
                    !semantic_type_prototypes_append_text(buffer,
                                                          sizeof(buffer),
                                                          &offset,
                                                          semantic_string_native(elementName))) {
                    return ZR_NULL;
                }
            }
        }

        if (!semantic_type_prototypes_append_text(buffer, sizeof(buffer), &offset, ")")) {
            return ZR_NULL;
        }
        return ZrCore_String_Create(analyzer->compilerState->state, buffer, offset);
    }

    return ZR_NULL;
}

static SZrString *semantic_type_prototypes_render_type(SZrSemanticAnalyzer *analyzer,
                                                       const SZrType *typeNode) {
    TZrChar buffer[ZR_PARSER_DECLARATION_BUFFER_LENGTH];
    TZrSize offset = 0;
    SZrString *baseName;
    SZrString *constraintText = ZR_NULL;

    if (analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL || typeNode == ZR_NULL || typeNode->name == ZR_NULL) {
        return ZR_NULL;
    }

    baseName = semantic_type_prototypes_render_type_name_node(analyzer, typeNode->name);
    if (baseName == ZR_NULL) {
        return ZR_NULL;
    }

    buffer[0] = '\0';
    if (!semantic_type_prototypes_append_text(buffer, sizeof(buffer), &offset, semantic_string_native(baseName))) {
        return ZR_NULL;
    }

    if (typeNode->subType != ZR_NULL) {
        SZrString *subTypeName = semantic_type_prototypes_render_type(analyzer, typeNode->subType);
        if (subTypeName == ZR_NULL ||
            !semantic_type_prototypes_append_text(buffer, sizeof(buffer), &offset, ".") ||
            !semantic_type_prototypes_append_text(buffer,
                                                  sizeof(buffer),
                                                  &offset,
                                                  semantic_string_native(subTypeName))) {
            return ZR_NULL;
        }
    }

    if (typeNode->hasArraySizeConstraint) {
        TZrChar constraintBuffer[ZR_PARSER_INTEGER_BUFFER_LENGTH];

        if (typeNode->arraySizeExpression != ZR_NULL) {
            constraintText =
                semantic_type_prototypes_render_generic_argument(analyzer, typeNode->arraySizeExpression);
        } else if (typeNode->arrayFixedSize > 0 ||
                   (typeNode->arrayMinSize > 0 && typeNode->arrayMinSize == typeNode->arrayMaxSize)) {
            TZrSize fixedSize = typeNode->arrayFixedSize > 0 ? typeNode->arrayFixedSize : typeNode->arrayMinSize;
            TZrInt32 written = snprintf(constraintBuffer, sizeof(constraintBuffer), "%zu", fixedSize);
            if (written > 0 && (TZrSize)written < sizeof(constraintBuffer)) {
                constraintText = ZrCore_String_Create(analyzer->compilerState->state,
                                                      constraintBuffer,
                                                      (TZrSize)written);
            }
        } else if (typeNode->arrayMinSize > 0 || typeNode->arrayMaxSize > 0) {
            TZrInt32 written;

            if (typeNode->arrayMaxSize > 0) {
                written = snprintf(constraintBuffer,
                                   sizeof(constraintBuffer),
                                   "%zu..%zu",
                                   typeNode->arrayMinSize,
                                   typeNode->arrayMaxSize);
            } else {
                written = snprintf(constraintBuffer,
                                   sizeof(constraintBuffer),
                                   "%zu..",
                                   typeNode->arrayMinSize);
            }

            if (written > 0 && (TZrSize)written < sizeof(constraintBuffer)) {
                constraintText = ZrCore_String_Create(analyzer->compilerState->state,
                                                      constraintBuffer,
                                                      (TZrSize)written);
            }
        }
    }

    for (TZrInt32 index = 0; index < typeNode->dimensions; index++) {
        TZrBool isOutermost = (TZrBool)(index + 1 == typeNode->dimensions);

        if (isOutermost && typeNode->hasArraySizeConstraint && constraintText != ZR_NULL) {
            if (!semantic_type_prototypes_append_format(buffer,
                                                        sizeof(buffer),
                                                        &offset,
                                                        "[%s]",
                                                        semantic_string_native(constraintText))) {
                return ZR_NULL;
            }
            continue;
        }

        if (!semantic_type_prototypes_append_text(buffer, sizeof(buffer), &offset, "[]")) {
            return ZR_NULL;
        }
    }

    return ZrCore_String_Create(analyzer->compilerState->state, buffer, offset);
}

static SZrString *semantic_type_prototypes_render_generic_argument(SZrSemanticAnalyzer *analyzer,
                                                                   SZrAstNode *node) {
    SZrTypeValue evaluatedValue;
    TZrChar buffer[ZR_PARSER_INTEGER_BUFFER_LENGTH];

    if (analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL || node == ZR_NULL) {
        return ZR_NULL;
    }

    if (node->type == ZR_AST_TYPE) {
        return semantic_type_prototypes_render_type(analyzer, &node->data.type);
    }
    if (node->type == ZR_AST_IDENTIFIER_LITERAL) {
        return node->data.identifier.name;
    }
    if (node->type == ZR_AST_INTEGER_LITERAL && node->data.integerLiteral.literal != ZR_NULL) {
        return node->data.integerLiteral.literal;
    }

    if (ZrParser_Compiler_EvaluateCompileTimeExpression(analyzer->compilerState, node, &evaluatedValue)) {
        switch (evaluatedValue.type) {
            case ZR_VALUE_TYPE_INT8:
            case ZR_VALUE_TYPE_INT16:
            case ZR_VALUE_TYPE_INT32:
            case ZR_VALUE_TYPE_INT64:
                snprintf(buffer,
                         sizeof(buffer),
                         "%lld",
                         (long long)evaluatedValue.value.nativeObject.nativeInt64);
                return ZrCore_String_CreateFromNative(analyzer->compilerState->state, buffer);
            case ZR_VALUE_TYPE_UINT8:
            case ZR_VALUE_TYPE_UINT16:
            case ZR_VALUE_TYPE_UINT32:
            case ZR_VALUE_TYPE_UINT64:
                snprintf(buffer,
                         sizeof(buffer),
                         "%llu",
                         (unsigned long long)evaluatedValue.value.nativeObject.nativeUInt64);
                return ZrCore_String_CreateFromNative(analyzer->compilerState->state, buffer);
            default:
                break;
        }
    }

    analyzer->compilerState->hasError = ZR_FALSE;
    return ZR_NULL;
}

static EZrValueType semantic_type_prototypes_base_type_from_name(SZrString *typeName,
                                                                 const SZrType *typeNode) {
    const TZrChar *typeNameText = semantic_string_native(typeName);

    if (typeNode != ZR_NULL && typeNode->dimensions > 0) {
        return ZR_VALUE_TYPE_ARRAY;
    }
    if (typeNameText == ZR_NULL) {
        return ZR_VALUE_TYPE_OBJECT;
    }

    if (strcmp(typeNameText, "int") == 0 || strcmp(typeNameText, "i64") == 0) return ZR_VALUE_TYPE_INT64;
    if (strcmp(typeNameText, "uint") == 0 || strcmp(typeNameText, "u64") == 0) return ZR_VALUE_TYPE_UINT64;
    if (strcmp(typeNameText, "float") == 0 || strcmp(typeNameText, "f64") == 0) return ZR_VALUE_TYPE_DOUBLE;
    if (strcmp(typeNameText, "f32") == 0) return ZR_VALUE_TYPE_FLOAT;
    if (strcmp(typeNameText, "bool") == 0) return ZR_VALUE_TYPE_BOOL;
    if (strcmp(typeNameText, "string") == 0 || strcmp(typeNameText, "str") == 0) return ZR_VALUE_TYPE_STRING;
    if (strcmp(typeNameText, "null") == 0 || strcmp(typeNameText, "void") == 0) return ZR_VALUE_TYPE_NULL;
    if (strcmp(typeNameText, "i8") == 0) return ZR_VALUE_TYPE_INT8;
    if (strcmp(typeNameText, "u8") == 0) return ZR_VALUE_TYPE_UINT8;
    if (strcmp(typeNameText, "i16") == 0) return ZR_VALUE_TYPE_INT16;
    if (strcmp(typeNameText, "u16") == 0) return ZR_VALUE_TYPE_UINT16;
    if (strcmp(typeNameText, "i32") == 0) return ZR_VALUE_TYPE_INT32;
    if (strcmp(typeNameText, "u32") == 0) return ZR_VALUE_TYPE_UINT32;
    return ZR_VALUE_TYPE_OBJECT;
}

static TZrBool semantic_type_prototypes_parse_size_text(SZrString *sizeText, TZrSize *outSize) {
    const TZrChar *text;
    TZrChar *endPtr = ZR_NULL;
    unsigned long long parsed;

    if (sizeText == ZR_NULL || outSize == ZR_NULL) {
        return ZR_FALSE;
    }

    text = semantic_string_native(sizeText);
    if (text == ZR_NULL || text[0] == '\0') {
        return ZR_FALSE;
    }

    parsed = strtoull(text, &endPtr, 10);
    if (endPtr == text || endPtr == ZR_NULL || *endPtr != '\0') {
        return ZR_FALSE;
    }

    *outSize = (TZrSize)parsed;
    return ZR_TRUE;
}

static TZrBool semantic_type_prototypes_build_generic_argument_inferred_type(
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *ownerTypeNode,
        SZrAstNode *functionNode,
        SZrAstNode *argumentNode,
        SZrInferredType *outType);

static TZrBool semantic_type_prototypes_build_inferred_type(SZrSemanticAnalyzer *analyzer,
                                                            SZrAstNode *ownerTypeNode,
                                                            SZrAstNode *functionNode,
                                                            const SZrType *typeNode,
                                                            SZrInferredType *outType) {
    SZrState *state;
    SZrSemanticPrototypeContextSnapshot snapshot;
    SZrString *renderedTypeName;

    if (analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL || typeNode == ZR_NULL || outType == ZR_NULL) {
        return ZR_FALSE;
    }

    state = analyzer->compilerState->state;
    semantic_type_prototypes_push_context(analyzer, ownerTypeNode, functionNode, &snapshot);
    renderedTypeName = semantic_type_prototypes_render_type(analyzer, typeNode);
    if (renderedTypeName == ZR_NULL) {
        semantic_type_prototypes_pop_context(analyzer, &snapshot);
        return ZR_FALSE;
    }

    if (typeNode->dimensions > 0) {
        SZrType elementTypeNode;
        SZrInferredType elementType;
        TZrSize fixedSize = 0;
        TZrBool hasFixedSize = ZR_FALSE;

        elementTypeNode = *typeNode;
        elementTypeNode.dimensions--;
        elementTypeNode.arrayFixedSize = 0;
        elementTypeNode.arrayMinSize = 0;
        elementTypeNode.arrayMaxSize = 0;
        elementTypeNode.hasArraySizeConstraint = ZR_FALSE;
        elementTypeNode.arraySizeExpression = ZR_NULL;

        ZrParser_InferredType_Init(state, &elementType, ZR_VALUE_TYPE_OBJECT);
        if (!semantic_type_prototypes_build_inferred_type(analyzer,
                                                          ownerTypeNode,
                                                          functionNode,
                                                          &elementTypeNode,
                                                          &elementType)) {
            ZrParser_InferredType_Free(state, &elementType);
            semantic_type_prototypes_pop_context(analyzer, &snapshot);
            return ZR_FALSE;
        }

        ZrParser_InferredType_Free(state, outType);
        ZrParser_InferredType_InitFull(state, outType, ZR_VALUE_TYPE_ARRAY, ZR_FALSE, renderedTypeName);
        ZrCore_Array_Init(state, &outType->elementTypes, sizeof(SZrInferredType), 1);
        ZrCore_Array_Push(state, &outType->elementTypes, &elementType);
        outType->ownershipQualifier = typeNode->ownershipQualifier;
        outType->hasArraySizeConstraint = typeNode->hasArraySizeConstraint;
        outType->arrayFixedSize = typeNode->arrayFixedSize;
        outType->arrayMinSize = typeNode->arrayMinSize;
        outType->arrayMaxSize = typeNode->arrayMaxSize;

        if (typeNode->hasArraySizeConstraint && typeNode->arraySizeExpression != ZR_NULL) {
            SZrString *constraintText =
                semantic_type_prototypes_render_generic_argument(analyzer, typeNode->arraySizeExpression);
            if (constraintText != ZR_NULL && semantic_type_prototypes_parse_size_text(constraintText, &fixedSize)) {
                hasFixedSize = ZR_TRUE;
            }

            if (hasFixedSize) {
                outType->arrayFixedSize = fixedSize;
                outType->arrayMinSize = fixedSize;
                outType->arrayMaxSize = fixedSize;
            }
        }

        semantic_type_prototypes_pop_context(analyzer, &snapshot);
        return ZR_TRUE;
    }

    ZrParser_InferredType_Free(analyzer->compilerState->state, outType);
    ZrParser_InferredType_InitFull(analyzer->compilerState->state,
                                   outType,
                                   semantic_type_prototypes_base_type_from_name(renderedTypeName, typeNode),
                                   ZR_FALSE,
                                   renderedTypeName);
    outType->ownershipQualifier = typeNode->ownershipQualifier;

    if (typeNode->name->type == ZR_AST_GENERIC_TYPE) {
        SZrGenericType *genericType = &typeNode->name->data.genericType;

        ZrCore_Array_Init(state,
                          &outType->elementTypes,
                          sizeof(SZrInferredType),
                          genericType->params != ZR_NULL && genericType->params->count > 0
                              ? genericType->params->count
                              : 1);
        if (genericType->params != ZR_NULL) {
            for (TZrSize index = 0; index < genericType->params->count; index++) {
                SZrInferredType argumentType;

                ZrParser_InferredType_Init(state, &argumentType, ZR_VALUE_TYPE_OBJECT);
                if (!semantic_type_prototypes_build_generic_argument_inferred_type(analyzer,
                                                                                   ownerTypeNode,
                                                                                   functionNode,
                                                                                   genericType->params->nodes[index],
                                                                                   &argumentType)) {
                    ZrParser_InferredType_Free(state, &argumentType);
                    ZrParser_InferredType_Free(state, outType);
                    semantic_type_prototypes_pop_context(analyzer, &snapshot);
                    return ZR_FALSE;
                }

                ZrCore_Array_Push(state, &outType->elementTypes, &argumentType);
            }
        }
    }

    semantic_type_prototypes_pop_context(analyzer, &snapshot);
    return ZR_TRUE;
}

static TZrBool semantic_type_prototypes_build_generic_argument_inferred_type(
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *ownerTypeNode,
        SZrAstNode *functionNode,
        SZrAstNode *argumentNode,
        SZrInferredType *outType) {
    SZrString *argumentText;
    SZrState *state;

    if (analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL || argumentNode == ZR_NULL || outType == ZR_NULL) {
        return ZR_FALSE;
    }

    state = analyzer->compilerState->state;
    if (argumentNode->type == ZR_AST_TYPE) {
        return semantic_type_prototypes_build_inferred_type(analyzer,
                                                            ownerTypeNode,
                                                            functionNode,
                                                            &argumentNode->data.type,
                                                            outType);
    }

    argumentText = semantic_type_prototypes_render_generic_argument(analyzer, argumentNode);
    if (argumentText == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Free(state, outType);
    ZrParser_InferredType_InitFull(state, outType, ZR_VALUE_TYPE_OBJECT, ZR_FALSE, argumentText);
    return ZR_TRUE;
}

static SZrString *semantic_type_prototypes_extract_constraint_name(SZrSemanticAnalyzer *analyzer,
                                                                   SZrAstNode *typeNode) {
    return analyzer != ZR_NULL && typeNode != ZR_NULL && typeNode->type == ZR_AST_TYPE
               ? semantic_type_prototypes_render_type(analyzer, &typeNode->data.type)
               : ZR_NULL;
}

static void semantic_type_prototypes_collect_generic_parameters(SZrSemanticAnalyzer *analyzer,
                                                                SZrArray *genericParameters,
                                                                SZrGenericDeclaration *genericDeclaration) {
    SZrCompilerState *compilerState;

    if (analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL || genericParameters == ZR_NULL ||
        genericDeclaration == ZR_NULL || genericDeclaration->params == ZR_NULL) {
        return;
    }

    compilerState = analyzer->compilerState;
    if (!genericParameters->isValid || genericParameters->head == ZR_NULL ||
        genericParameters->capacity == 0 || genericParameters->elementSize == 0) {
        ZrCore_Array_Init(compilerState->state,
                          genericParameters,
                          sizeof(SZrTypeGenericParameterInfo),
                          genericDeclaration->params->count > 0 ? genericDeclaration->params->count : 1);
    }

    for (TZrSize index = 0; index < genericDeclaration->params->count; index++) {
        SZrAstNode *paramNode = genericDeclaration->params->nodes[index];
        SZrTypeGenericParameterInfo genericInfo;
        SZrParameter *parameter;

        if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
            continue;
        }

        parameter = &paramNode->data.parameter;
        memset(&genericInfo, 0, sizeof(genericInfo));
        genericInfo.name = parameter->name != ZR_NULL ? parameter->name->name : ZR_NULL;
        genericInfo.genericKind = parameter->genericKind;
        genericInfo.variance = parameter->variance;
        genericInfo.requiresClass = parameter->genericRequiresClass;
        genericInfo.requiresStruct = parameter->genericRequiresStruct;
        genericInfo.requiresNew = parameter->genericRequiresNew;
        ZrCore_Array_Init(compilerState->state,
                          &genericInfo.constraintTypeNames,
                          sizeof(SZrString *),
                          parameter->genericTypeConstraints != ZR_NULL
                              ? parameter->genericTypeConstraints->count
                              : 1);

        if (parameter->genericTypeConstraints != ZR_NULL) {
            for (TZrSize constraintIndex = 0;
                 constraintIndex < parameter->genericTypeConstraints->count;
                 constraintIndex++) {
                SZrAstNode *constraintNode = parameter->genericTypeConstraints->nodes[constraintIndex];
                SZrString *constraintName =
                    semantic_type_prototypes_extract_constraint_name(analyzer, constraintNode);
                if (constraintName != ZR_NULL) {
                    ZrCore_Array_Push(compilerState->state, &genericInfo.constraintTypeNames, &constraintName);
                }
            }
        }

        ZrCore_Array_Push(compilerState->state, genericParameters, &genericInfo);
    }
}

static SZrString *semantic_type_prototypes_type_name_from_type_node(SZrSemanticAnalyzer *analyzer,
                                                                    SZrAstNode *ownerTypeNode,
                                                                    SZrAstNode *functionNode,
                                                                    const SZrType *typeNode) {
    SZrCompilerState *compilerState;
    SZrInferredType inferredType;
    SZrString *typeName = ZR_NULL;

    if (analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL || typeNode == ZR_NULL) {
        return ZR_NULL;
    }

    compilerState = analyzer->compilerState;
    ZrParser_InferredType_Init(compilerState->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    if (semantic_type_prototypes_build_inferred_type(analyzer, ownerTypeNode, functionNode, typeNode, &inferredType) &&
        inferredType.typeName != ZR_NULL) {
        typeName = inferredType.typeName;
    }
    ZrParser_InferredType_Free(compilerState->state, &inferredType);
    return typeName;
}

static void semantic_type_prototypes_collect_parameter_signature(SZrSemanticAnalyzer *analyzer,
                                                                 SZrAstNode *ownerTypeNode,
                                                                 SZrAstNode *functionNode,
                                                                 SZrAstNodeArray *params,
                                                                 SZrArray *parameterTypes,
                                                                 SZrArray *parameterNames,
                                                                 SZrArray *parameterPassingModes,
                                                                 TZrUInt32 *outParameterCount,
                                                                 TZrUInt32 *outMinArgumentCount) {
    SZrCompilerState *compilerState;
    SZrSemanticPrototypeContextSnapshot snapshot;
    TZrUInt32 parameterCount = 0;
    TZrUInt32 minArgumentCount = 0;

    if (outParameterCount != ZR_NULL) {
        *outParameterCount = 0;
    }
    if (outMinArgumentCount != ZR_NULL) {
        *outMinArgumentCount = 0;
    }

    if (analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL || params == ZR_NULL) {
        return;
    }

    compilerState = analyzer->compilerState;
    semantic_type_prototypes_push_context(analyzer, ownerTypeNode, functionNode, &snapshot);
    ZrCore_Array_Init(compilerState->state, parameterTypes, sizeof(SZrInferredType), params->count);
    ZrCore_Array_Init(compilerState->state, parameterNames, sizeof(SZrString *), params->count);
    ZrCore_Array_Init(compilerState->state, parameterPassingModes, sizeof(EZrParameterPassingMode), params->count);

    for (TZrSize index = 0; index < params->count; index++) {
        SZrAstNode *paramNode = params->nodes[index];
        SZrParameter *parameter;
        SZrInferredType inferredType;
        EZrParameterPassingMode passingMode = ZR_PARAMETER_PASSING_MODE_VALUE;
        SZrString *parameterName = ZR_NULL;

        if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
            continue;
        }

        parameter = &paramNode->data.parameter;
        parameterName = parameter->name != ZR_NULL ? parameter->name->name : ZR_NULL;
        passingMode = parameter->passingMode;

        if (parameterName != ZR_NULL) {
            ZrCore_Array_Push(compilerState->state, parameterNames, &parameterName);
        }
        ZrCore_Array_Push(compilerState->state, parameterPassingModes, &passingMode);

        if (parameter->defaultValue == ZR_NULL) {
            minArgumentCount++;
        }

        ZrParser_InferredType_Init(compilerState->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
        if (parameter->typeInfo != ZR_NULL &&
            semantic_type_prototypes_build_inferred_type(analyzer,
                                                         ownerTypeNode,
                                                         functionNode,
                                                         parameter->typeInfo,
                                                         &inferredType)) {
            ZrCore_Array_Push(compilerState->state, parameterTypes, &inferredType);
        } else {
            ZrParser_InferredType_Free(compilerState->state, &inferredType);
            parameterCount++;
            continue;
        }
        parameterCount++;
    }

    if (outParameterCount != ZR_NULL) {
        *outParameterCount = parameterCount;
    }
    if (outMinArgumentCount != ZR_NULL) {
        *outMinArgumentCount = minArgumentCount;
    }
    semantic_type_prototypes_pop_context(analyzer, &snapshot);
}

static void semantic_type_prototypes_append_class_member(SZrState *state,
                                                         SZrSemanticAnalyzer *analyzer,
                                                         SZrAstNode *ownerTypeNode,
                                                         SZrTypePrototypeInfo *prototype,
                                                         SZrAstNode *memberNode,
                                                         TZrUInt32 declarationOrder) {
    SZrCompilerState *compilerState;
    SZrTypeMemberInfo memberInfo;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL ||
        prototype == ZR_NULL || memberNode == ZR_NULL) {
        return;
    }

    compilerState = analyzer->compilerState;
    semantic_type_prototypes_init_member_defaults(&memberInfo);
    memberInfo.declarationNode = memberNode;
    memberInfo.declarationOrder = declarationOrder;
    memberInfo.ownerTypeName = prototype->name;
    memberInfo.baseDefinitionOwnerTypeName = prototype->name;

    switch (memberNode->type) {
        case ZR_AST_CLASS_FIELD: {
            SZrClassField *field = &memberNode->data.classField;
            memberInfo.memberType = ZR_AST_CLASS_FIELD;
            memberInfo.accessModifier = field->access;
            memberInfo.isStatic = field->isStatic;
            memberInfo.isConst = field->isConst;
            memberInfo.isUsingManaged = field->isUsingManaged;
            memberInfo.name = field->name != ZR_NULL ? field->name->name : ZR_NULL;
            memberInfo.baseDefinitionName = memberInfo.name;
            memberInfo.fieldType = field->typeInfo;
            memberInfo.fieldTypeName =
                semantic_type_prototypes_type_name_from_type_node(analyzer, ownerTypeNode, ZR_NULL, field->typeInfo);
            memberInfo.ownershipQualifier =
                field->typeInfo != ZR_NULL ? field->typeInfo->ownershipQualifier : ZR_OWNERSHIP_QUALIFIER_NONE;
            break;
        }
        case ZR_AST_CLASS_METHOD: {
            SZrClassMethod *method = &memberNode->data.classMethod;
            memberInfo.memberType = ZR_AST_CLASS_METHOD;
            memberInfo.accessModifier = method->access;
            memberInfo.isStatic = method->isStatic;
            memberInfo.modifierFlags = method->modifierFlags;
            memberInfo.receiverQualifier = method->receiverQualifier;
            memberInfo.name = method->name != ZR_NULL ? method->name->name : ZR_NULL;
            memberInfo.baseDefinitionName = memberInfo.name;
            memberInfo.returnTypeName =
                semantic_type_prototypes_type_name_from_type_node(analyzer,
                                                                  ownerTypeNode,
                                                                  memberNode,
                                                                  method->returnType);
            semantic_type_prototypes_collect_generic_parameters(analyzer,
                                                                &memberInfo.genericParameters,
                                                                method->generic);
            semantic_type_prototypes_collect_parameter_signature(analyzer,
                                                                 ownerTypeNode,
                                                                 memberNode,
                                                                 method->params,
                                                                 &memberInfo.parameterTypes,
                                                                 &memberInfo.parameterNames,
                                                                 &memberInfo.parameterPassingModes,
                                                                 &memberInfo.parameterCount,
                                                                 &memberInfo.minArgumentCount);
            break;
        }
        case ZR_AST_CLASS_PROPERTY: {
            SZrClassProperty *property = &memberNode->data.classProperty;
            memberInfo.memberType = ZR_AST_CLASS_PROPERTY;
            memberInfo.accessModifier = property->access;
            memberInfo.isStatic = property->isStatic;
            memberInfo.modifierFlags = property->modifierFlags;
            memberInfo.propertyIdentity = prototype->nextPropertyIdentity++;

            if (property->modifier == ZR_NULL) {
                return;
            }

            if (property->modifier->type == ZR_AST_PROPERTY_GET) {
                SZrPropertyGet *getter = &property->modifier->data.propertyGet;

                memberInfo.modifierFlags |= getter->modifierFlags;
                memberInfo.accessorRole = 1;
                memberInfo.name = getter->name != ZR_NULL ? getter->name->name : ZR_NULL;
                memberInfo.baseDefinitionName = memberInfo.name;
                memberInfo.fieldType = getter->targetType;
                memberInfo.fieldTypeName = semantic_type_prototypes_type_name_from_type_node(analyzer,
                                                                                             ownerTypeNode,
                                                                                             memberNode,
                                                                                             getter->targetType);
                memberInfo.returnTypeName = memberInfo.fieldTypeName;
                memberInfo.ownershipQualifier = getter->targetType != ZR_NULL
                                                    ? getter->targetType->ownershipQualifier
                                                    : ZR_OWNERSHIP_QUALIFIER_NONE;
            } else if (property->modifier->type == ZR_AST_PROPERTY_SET) {
                SZrPropertySet *setter = &property->modifier->data.propertySet;
                EZrParameterPassingMode passingMode = ZR_PARAMETER_PASSING_MODE_VALUE;
                SZrInferredType parameterType;

                memberInfo.modifierFlags |= setter->modifierFlags;
                memberInfo.accessorRole = 2;
                memberInfo.name = setter->name != ZR_NULL ? setter->name->name : ZR_NULL;
                memberInfo.baseDefinitionName = memberInfo.name;
                memberInfo.fieldType = setter->targetType;
                memberInfo.fieldTypeName = semantic_type_prototypes_type_name_from_type_node(analyzer,
                                                                                             ownerTypeNode,
                                                                                             memberNode,
                                                                                             setter->targetType);
                memberInfo.ownershipQualifier = setter->targetType != ZR_NULL
                                                    ? setter->targetType->ownershipQualifier
                                                    : ZR_OWNERSHIP_QUALIFIER_NONE;

                if (setter->targetType != ZR_NULL) {
                    ZrCore_Array_Init(compilerState->state, &memberInfo.parameterTypes, sizeof(SZrInferredType), 1);
                    ZrParser_InferredType_Init(compilerState->state, &parameterType, ZR_VALUE_TYPE_OBJECT);
                    if (semantic_type_prototypes_build_inferred_type(analyzer,
                                                                     ownerTypeNode,
                                                                     memberNode,
                                                                     setter->targetType,
                                                                     &parameterType)) {
                        ZrCore_Array_Push(compilerState->state, &memberInfo.parameterTypes, &parameterType);
                    } else {
                        ZrParser_InferredType_Free(compilerState->state, &parameterType);
                    }
                }

                if (setter->param != ZR_NULL && setter->param->name != ZR_NULL) {
                    ZrCore_Array_Init(compilerState->state, &memberInfo.parameterNames, sizeof(SZrString *), 1);
                    ZrCore_Array_Push(compilerState->state, &memberInfo.parameterNames, &setter->param->name);
                }
                ZrCore_Array_Init(compilerState->state,
                                  &memberInfo.parameterPassingModes,
                                  sizeof(EZrParameterPassingMode),
                                  1);
                ZrCore_Array_Push(compilerState->state, &memberInfo.parameterPassingModes, &passingMode);
                memberInfo.parameterCount = (TZrUInt32)memberInfo.parameterTypes.length;
                memberInfo.minArgumentCount = memberInfo.parameterCount;
            } else {
                return;
            }

            if (memberInfo.name != ZR_NULL) {
                memberInfo.baseDefinitionName =
                    semantic_type_prototypes_create_hidden_property_accessor_name(state,
                                                                                  memberInfo.name,
                                                                                  memberInfo.accessorRole == 2);
            }
            break;
        }
        default:
            return;
    }

    if (memberInfo.name != ZR_NULL) {
        ZrCore_Array_Push(compilerState->state, &prototype->members, &memberInfo);
    }
}

static void semantic_type_prototypes_append_struct_member(SZrState *state,
                                                          SZrSemanticAnalyzer *analyzer,
                                                          SZrAstNode *ownerTypeNode,
                                                          SZrTypePrototypeInfo *prototype,
                                                          SZrAstNode *memberNode,
                                                          TZrUInt32 declarationOrder) {
    SZrCompilerState *compilerState;
    SZrTypeMemberInfo memberInfo;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL ||
        prototype == ZR_NULL || memberNode == ZR_NULL) {
        return;
    }

    compilerState = analyzer->compilerState;
    semantic_type_prototypes_init_member_defaults(&memberInfo);
    memberInfo.declarationNode = memberNode;
    memberInfo.declarationOrder = declarationOrder;
    memberInfo.ownerTypeName = prototype->name;
    memberInfo.baseDefinitionOwnerTypeName = prototype->name;

    switch (memberNode->type) {
        case ZR_AST_STRUCT_FIELD: {
            SZrStructField *field = &memberNode->data.structField;
            memberInfo.memberType = ZR_AST_STRUCT_FIELD;
            memberInfo.accessModifier = field->access;
            memberInfo.isStatic = field->isStatic;
            memberInfo.isConst = field->isConst;
            memberInfo.isUsingManaged = field->isUsingManaged;
            memberInfo.name = field->name != ZR_NULL ? field->name->name : ZR_NULL;
            memberInfo.baseDefinitionName = memberInfo.name;
            memberInfo.fieldType = field->typeInfo;
            memberInfo.fieldTypeName =
                semantic_type_prototypes_type_name_from_type_node(analyzer, ownerTypeNode, ZR_NULL, field->typeInfo);
            memberInfo.ownershipQualifier =
                field->typeInfo != ZR_NULL ? field->typeInfo->ownershipQualifier : ZR_OWNERSHIP_QUALIFIER_NONE;
            break;
        }
        case ZR_AST_STRUCT_METHOD: {
            SZrStructMethod *method = &memberNode->data.structMethod;
            memberInfo.memberType = ZR_AST_STRUCT_METHOD;
            memberInfo.accessModifier = method->access;
            memberInfo.isStatic = method->isStatic;
            memberInfo.receiverQualifier = method->receiverQualifier;
            memberInfo.name = method->name != ZR_NULL ? method->name->name : ZR_NULL;
            memberInfo.baseDefinitionName = memberInfo.name;
            memberInfo.returnTypeName =
                semantic_type_prototypes_type_name_from_type_node(analyzer,
                                                                  ownerTypeNode,
                                                                  memberNode,
                                                                  method->returnType);
            semantic_type_prototypes_collect_generic_parameters(analyzer,
                                                                &memberInfo.genericParameters,
                                                                method->generic);
            semantic_type_prototypes_collect_parameter_signature(analyzer,
                                                                 ownerTypeNode,
                                                                 memberNode,
                                                                 method->params,
                                                                 &memberInfo.parameterTypes,
                                                                 &memberInfo.parameterNames,
                                                                 &memberInfo.parameterPassingModes,
                                                                 &memberInfo.parameterCount,
                                                                 &memberInfo.minArgumentCount);
            break;
        }
        default:
            return;
    }

    if (memberInfo.name != ZR_NULL) {
        ZrCore_Array_Push(compilerState->state, &prototype->members, &memberInfo);
    }
}

static void semantic_type_prototypes_append_interface_member(SZrState *state,
                                                             SZrSemanticAnalyzer *analyzer,
                                                             SZrAstNode *ownerTypeNode,
                                                             SZrTypePrototypeInfo *prototype,
                                                             SZrAstNode *memberNode,
                                                             TZrUInt32 declarationOrder) {
    SZrCompilerState *compilerState;
    SZrTypeMemberInfo memberInfo;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL ||
        prototype == ZR_NULL || memberNode == ZR_NULL) {
        return;
    }

    compilerState = analyzer->compilerState;
    semantic_type_prototypes_init_member_defaults(&memberInfo);
    memberInfo.declarationNode = memberNode;
    memberInfo.declarationOrder = declarationOrder;
    memberInfo.ownerTypeName = prototype->name;
    memberInfo.baseDefinitionOwnerTypeName = prototype->name;

    switch (memberNode->type) {
        case ZR_AST_INTERFACE_FIELD_DECLARATION: {
            SZrInterfaceFieldDeclaration *field = &memberNode->data.interfaceFieldDeclaration;
            memberInfo.memberType = ZR_AST_CLASS_FIELD;
            memberInfo.accessModifier = field->access;
            memberInfo.isConst = field->isConst;
            memberInfo.name = field->name != ZR_NULL ? field->name->name : ZR_NULL;
            memberInfo.baseDefinitionName = memberInfo.name;
            memberInfo.fieldType = field->typeInfo;
            memberInfo.fieldTypeName =
                semantic_type_prototypes_type_name_from_type_node(analyzer, ownerTypeNode, ZR_NULL, field->typeInfo);
            memberInfo.ownershipQualifier =
                field->typeInfo != ZR_NULL ? field->typeInfo->ownershipQualifier : ZR_OWNERSHIP_QUALIFIER_NONE;
            break;
        }
        case ZR_AST_INTERFACE_METHOD_SIGNATURE: {
            SZrInterfaceMethodSignature *method = &memberNode->data.interfaceMethodSignature;
            memberInfo.memberType = ZR_AST_CLASS_METHOD;
            memberInfo.accessModifier = method->access;
            memberInfo.name = method->name != ZR_NULL ? method->name->name : ZR_NULL;
            memberInfo.baseDefinitionName = memberInfo.name;
            memberInfo.returnTypeName =
                semantic_type_prototypes_type_name_from_type_node(analyzer,
                                                                  ownerTypeNode,
                                                                  memberNode,
                                                                  method->returnType);
            semantic_type_prototypes_collect_generic_parameters(analyzer,
                                                                &memberInfo.genericParameters,
                                                                method->generic);
            semantic_type_prototypes_collect_parameter_signature(analyzer,
                                                                 ownerTypeNode,
                                                                 memberNode,
                                                                 method->params,
                                                                 &memberInfo.parameterTypes,
                                                                 &memberInfo.parameterNames,
                                                                 &memberInfo.parameterPassingModes,
                                                                 &memberInfo.parameterCount,
                                                                 &memberInfo.minArgumentCount);
            break;
        }
        default:
            return;
    }

    if (memberInfo.name != ZR_NULL) {
        ZrCore_Array_Push(compilerState->state, &prototype->members, &memberInfo);
    }
}

static void semantic_type_prototypes_populate_class(SZrState *state,
                                                    SZrSemanticAnalyzer *analyzer,
                                                    SZrAstNode *node,
                                                    SZrTypePrototypeInfo *prototype) {
    SZrClassDeclaration *classDecl;
    SZrString *primarySuperTypeName = ZR_NULL;

    if (state == ZR_NULL || analyzer == ZR_NULL || node == ZR_NULL || prototype == ZR_NULL ||
        node->type != ZR_AST_CLASS_DECLARATION) {
        return;
    }

    classDecl = &node->data.classDeclaration;
    prototype->modifierFlags = classDecl->modifierFlags;

    if (classDecl->inherits != ZR_NULL) {
        for (TZrSize index = 0; index < classDecl->inherits->count; index++) {
            SZrAstNode *inheritNode = classDecl->inherits->nodes[index];
            SZrString *inheritTypeName;
            SZrTypePrototypeInfo *inheritPrototype;

            if (inheritNode == ZR_NULL || inheritNode->type != ZR_AST_TYPE) {
                continue;
            }

            inheritTypeName =
                semantic_type_prototypes_type_name_from_type_node(analyzer, node, ZR_NULL, &inheritNode->data.type);
            if (inheritTypeName == ZR_NULL) {
                continue;
            }

            inheritPrototype = semantic_type_prototypes_find_exact(analyzer->compilerState, inheritTypeName);
            if (inheritPrototype != ZR_NULL && inheritPrototype->type == ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE) {
                ZrCore_Array_Push(state, &prototype->implements, &inheritTypeName);
            } else if (primarySuperTypeName == ZR_NULL) {
                primarySuperTypeName = inheritTypeName;
            }

            ZrCore_Array_Push(state, &prototype->inherits, &inheritTypeName);
        }
    }

    if (primarySuperTypeName == ZR_NULL) {
        primarySuperTypeName = ZrCore_String_Create(state, "zr.builtin.Object", strlen("zr.builtin.Object"));
        if (primarySuperTypeName != ZR_NULL) {
            ZrCore_Array_Push(state, &prototype->inherits, &primarySuperTypeName);
        }
    }
    prototype->extendsTypeName = primarySuperTypeName;

    if (classDecl->members != ZR_NULL) {
        for (TZrSize index = 0; index < classDecl->members->count; index++) {
            semantic_type_prototypes_append_class_member(state,
                                                         analyzer,
                                                         node,
                                                         prototype,
                                                         classDecl->members->nodes[index],
                                                         (TZrUInt32)index);
        }
    }
}

static void semantic_type_prototypes_populate_struct(SZrState *state,
                                                     SZrSemanticAnalyzer *analyzer,
                                                     SZrAstNode *node,
                                                     SZrTypePrototypeInfo *prototype) {
    SZrStructDeclaration *structDecl;

    if (state == ZR_NULL || analyzer == ZR_NULL || node == ZR_NULL || prototype == ZR_NULL ||
        node->type != ZR_AST_STRUCT_DECLARATION) {
        return;
    }

    structDecl = &node->data.structDeclaration;
    if (structDecl->inherits != ZR_NULL) {
        for (TZrSize index = 0; index < structDecl->inherits->count; index++) {
            SZrAstNode *inheritNode = structDecl->inherits->nodes[index];
            SZrString *inheritTypeName;

            if (inheritNode == ZR_NULL || inheritNode->type != ZR_AST_TYPE) {
                continue;
            }

            inheritTypeName =
                semantic_type_prototypes_type_name_from_type_node(analyzer, node, ZR_NULL, &inheritNode->data.type);
            if (inheritTypeName == ZR_NULL) {
                continue;
            }

            if (prototype->extendsTypeName == ZR_NULL) {
                prototype->extendsTypeName = inheritTypeName;
            }
            ZrCore_Array_Push(state, &prototype->inherits, &inheritTypeName);
        }
    }

    if (structDecl->members != ZR_NULL) {
        for (TZrSize index = 0; index < structDecl->members->count; index++) {
            semantic_type_prototypes_append_struct_member(state,
                                                          analyzer,
                                                          node,
                                                          prototype,
                                                          structDecl->members->nodes[index],
                                                          (TZrUInt32)index);
        }
    }
}

static void semantic_type_prototypes_populate_interface(SZrState *state,
                                                        SZrSemanticAnalyzer *analyzer,
                                                        SZrAstNode *node,
                                                        SZrTypePrototypeInfo *prototype) {
    SZrInterfaceDeclaration *interfaceDecl;

    if (state == ZR_NULL || analyzer == ZR_NULL || node == ZR_NULL || prototype == ZR_NULL ||
        node->type != ZR_AST_INTERFACE_DECLARATION) {
        return;
    }

    interfaceDecl = &node->data.interfaceDeclaration;
    prototype->modifierFlags = ZR_DECLARATION_MODIFIER_ABSTRACT;
    prototype->allowValueConstruction = ZR_FALSE;
    prototype->allowBoxedConstruction = ZR_FALSE;

    if (interfaceDecl->inherits != ZR_NULL) {
        for (TZrSize index = 0; index < interfaceDecl->inherits->count; index++) {
            SZrAstNode *inheritNode = interfaceDecl->inherits->nodes[index];
            SZrString *inheritTypeName;

            if (inheritNode == ZR_NULL || inheritNode->type != ZR_AST_TYPE) {
                continue;
            }

            inheritTypeName =
                semantic_type_prototypes_type_name_from_type_node(analyzer, node, ZR_NULL, &inheritNode->data.type);
            if (inheritTypeName == ZR_NULL) {
                continue;
            }

            if (prototype->extendsTypeName == ZR_NULL) {
                prototype->extendsTypeName = inheritTypeName;
            }
            ZrCore_Array_Push(state, &prototype->inherits, &inheritTypeName);
        }
    }

    if (interfaceDecl->members != ZR_NULL) {
        for (TZrSize index = 0; index < interfaceDecl->members->count; index++) {
            semantic_type_prototypes_append_interface_member(state,
                                                             analyzer,
                                                             node,
                                                             prototype,
                                                             interfaceDecl->members->nodes[index],
                                                             (TZrUInt32)index);
        }
    }
}

static void semantic_type_prototypes_populate_enum(SZrSemanticAnalyzer *analyzer,
                                                   SZrAstNode *node,
                                                   SZrTypePrototypeInfo *prototype) {
    if (analyzer == ZR_NULL || node == ZR_NULL || prototype == ZR_NULL || node->type != ZR_AST_ENUM_DECLARATION) {
        return;
    }

    prototype->enumValueTypeName =
        semantic_type_prototypes_type_name_from_type_node(analyzer, node, ZR_NULL, node->data.enumDeclaration.baseType);
}

static TZrBool semantic_type_prototypes_register_shell(SZrState *state,
                                                       SZrSemanticAnalyzer *analyzer,
                                                       SZrAstNode *node) {
    SZrCompilerState *compilerState;
    SZrString *typeName;
    SZrTypePrototypeInfo prototypeInfo;
    EZrObjectPrototypeType prototypeType;
    EZrAccessModifier accessModifier;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL || node == ZR_NULL) {
        return ZR_TRUE;
    }

    compilerState = analyzer->compilerState;
    typeName = semantic_type_prototypes_owner_name(node);
    if (typeName == ZR_NULL || semantic_type_prototypes_find_exact(compilerState, typeName) != ZR_NULL) {
        return ZR_TRUE;
    }

    prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_INVALID;
    accessModifier = ZR_ACCESS_PRIVATE;
    switch (node->type) {
        case ZR_AST_CLASS_DECLARATION:
            prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_CLASS;
            accessModifier = node->data.classDeclaration.accessModifier;
            break;
        case ZR_AST_STRUCT_DECLARATION:
            prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT;
            accessModifier = node->data.structDeclaration.accessModifier;
            break;
        case ZR_AST_INTERFACE_DECLARATION:
            prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE;
            accessModifier = node->data.interfaceDeclaration.accessModifier;
            break;
        case ZR_AST_ENUM_DECLARATION:
            prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_ENUM;
            accessModifier = node->data.enumDeclaration.accessModifier;
            break;
        default:
            return ZR_TRUE;
    }

    semantic_type_prototypes_init_prototype(state, &prototypeInfo, typeName, prototypeType, accessModifier);
    switch (node->type) {
        case ZR_AST_CLASS_DECLARATION:
            semantic_type_prototypes_collect_generic_parameters(analyzer,
                                                                &prototypeInfo.genericParameters,
                                                                node->data.classDeclaration.generic);
            prototypeInfo.modifierFlags = node->data.classDeclaration.modifierFlags;
            break;
        case ZR_AST_STRUCT_DECLARATION:
            semantic_type_prototypes_collect_generic_parameters(analyzer,
                                                                &prototypeInfo.genericParameters,
                                                                node->data.structDeclaration.generic);
            break;
        case ZR_AST_INTERFACE_DECLARATION:
            semantic_type_prototypes_collect_generic_parameters(analyzer,
                                                                &prototypeInfo.genericParameters,
                                                                node->data.interfaceDeclaration.generic);
            prototypeInfo.modifierFlags = ZR_DECLARATION_MODIFIER_ABSTRACT;
            prototypeInfo.allowValueConstruction = ZR_FALSE;
            prototypeInfo.allowBoxedConstruction = ZR_FALSE;
            break;
        default:
            break;
    }

    ZrCore_Array_Push(state, &compilerState->typePrototypes, &prototypeInfo);
    return ZR_TRUE;
}

static TZrBool semantic_type_prototypes_walk_shells(SZrState *state,
                                                    SZrSemanticAnalyzer *analyzer,
                                                    SZrAstNode *node) {
    if (state == ZR_NULL || analyzer == ZR_NULL || node == ZR_NULL) {
        return ZR_TRUE;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            if (node->data.script.statements != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.script.statements->count; index++) {
                    if (!semantic_type_prototypes_walk_shells(state,
                                                              analyzer,
                                                              node->data.script.statements->nodes[index])) {
                        return ZR_FALSE;
                    }
                }
            }
            return ZR_TRUE;
        case ZR_AST_EXTERN_BLOCK:
            if (node->data.externBlock.declarations != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.externBlock.declarations->count; index++) {
                    if (!semantic_type_prototypes_walk_shells(state,
                                                              analyzer,
                                                              node->data.externBlock.declarations->nodes[index])) {
                        return ZR_FALSE;
                    }
                }
            }
            return ZR_TRUE;
        case ZR_AST_COMPILE_TIME_DECLARATION:
            return semantic_type_prototypes_walk_shells(state,
                                                        analyzer,
                                                        node->data.compileTimeDeclaration.declaration);
        case ZR_AST_CLASS_DECLARATION:
        case ZR_AST_STRUCT_DECLARATION:
        case ZR_AST_INTERFACE_DECLARATION:
        case ZR_AST_ENUM_DECLARATION:
            return semantic_type_prototypes_register_shell(state, analyzer, node);
        default:
            return ZR_TRUE;
    }
}

static TZrBool semantic_type_prototypes_walk_details(SZrState *state,
                                                     SZrSemanticAnalyzer *analyzer,
                                                     SZrAstNode *node) {
    SZrTypePrototypeInfo *prototype;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL || node == ZR_NULL) {
        return ZR_TRUE;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            if (node->data.script.statements != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.script.statements->count; index++) {
                    if (!semantic_type_prototypes_walk_details(state,
                                                               analyzer,
                                                               node->data.script.statements->nodes[index])) {
                        return ZR_FALSE;
                    }
                }
            }
            return ZR_TRUE;
        case ZR_AST_EXTERN_BLOCK:
            if (node->data.externBlock.declarations != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.externBlock.declarations->count; index++) {
                    if (!semantic_type_prototypes_walk_details(state,
                                                               analyzer,
                                                               node->data.externBlock.declarations->nodes[index])) {
                        return ZR_FALSE;
                    }
                }
            }
            return ZR_TRUE;
        case ZR_AST_COMPILE_TIME_DECLARATION:
            return semantic_type_prototypes_walk_details(state,
                                                         analyzer,
                                                         node->data.compileTimeDeclaration.declaration);
        case ZR_AST_CLASS_DECLARATION:
        case ZR_AST_STRUCT_DECLARATION:
        case ZR_AST_INTERFACE_DECLARATION:
        case ZR_AST_ENUM_DECLARATION:
            prototype = semantic_type_prototypes_find_exact(analyzer->compilerState,
                                                            semantic_type_prototypes_owner_name(node));
            if (prototype == ZR_NULL || prototype->members.length > 0 || prototype->inherits.length > 0 ||
                prototype->implements.length > 0 || prototype->extendsTypeName != ZR_NULL ||
                prototype->enumValueTypeName != ZR_NULL) {
                return ZR_TRUE;
            }
            switch (node->type) {
                case ZR_AST_CLASS_DECLARATION:
                    semantic_type_prototypes_populate_class(state, analyzer, node, prototype);
                    break;
                case ZR_AST_STRUCT_DECLARATION:
                    semantic_type_prototypes_populate_struct(state, analyzer, node, prototype);
                    break;
                case ZR_AST_INTERFACE_DECLARATION:
                    semantic_type_prototypes_populate_interface(state, analyzer, node, prototype);
                    break;
                case ZR_AST_ENUM_DECLARATION:
                    semantic_type_prototypes_populate_enum(analyzer, node, prototype);
                    break;
                default:
                    break;
            }
            return ZR_TRUE;
        default:
            return ZR_TRUE;
    }
}

TZrBool ZrLanguageServer_SemanticAnalyzer_BootstrapTypePrototypes(SZrState *state,
                                                                  SZrSemanticAnalyzer *analyzer,
                                                                  SZrAstNode *ast) {
    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL || ast == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!semantic_type_prototypes_walk_shells(state, analyzer, ast)) {
        return ZR_FALSE;
    }

    if (!semantic_type_prototypes_walk_details(state, analyzer, ast)) {
        return ZR_FALSE;
    }

    analyzer->compilerState->hasError = ZR_FALSE;
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_SemanticAnalyzer_BuildDeclaredTypeInferredType(
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *ownerTypeNode,
        SZrAstNode *functionNode,
        const SZrType *typeNode,
        SZrInferredType *outType) {
    if (analyzer == ZR_NULL || typeNode == ZR_NULL || outType == ZR_NULL) {
        return ZR_FALSE;
    }

    return semantic_type_prototypes_build_inferred_type(analyzer,
                                                        ownerTypeNode,
                                                        functionNode,
                                                        typeNode,
                                                        outType);
}
