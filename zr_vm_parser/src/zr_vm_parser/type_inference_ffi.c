#include "type_inference_internal.h"

#include "zr_vm_core/string.h"

#include <string.h>

static TZrBool ffi_ast_node_matches_key(SZrAstNode *keyNode, const TZrChar *name) {
    if (keyNode == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }

    if (keyNode->type == ZR_AST_IDENTIFIER_LITERAL) {
        return zr_string_equals_cstr(keyNode->data.identifier.name, name);
    }

    if (keyNode->type == ZR_AST_STRING_LITERAL) {
        return zr_string_equals_cstr(keyNode->data.stringLiteral.value, name);
    }

    return ZR_FALSE;
}

static SZrAstNode *ffi_object_literal_find_property_value(SZrObjectLiteral *literal, const TZrChar *name) {
    if (literal == ZR_NULL || literal->properties == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < literal->properties->count; index++) {
        SZrAstNode *propertyNode = literal->properties->nodes[index];

        if (propertyNode == ZR_NULL || propertyNode->type != ZR_AST_KEY_VALUE_PAIR) {
            continue;
        }

        if (ffi_ast_node_matches_key(propertyNode->data.keyValuePair.key, name)) {
            return propertyNode->data.keyValuePair.value;
        }
    }

    return ZR_NULL;
}

static const TZrChar *ffi_pointer_family_literal_from_type_name(SZrState *state, SZrString *typeName) {
    SZrString *baseName = ZR_NULL;
    SZrArray argumentTypeNames;
    const TZrChar *family = ZR_NULL;

    if (state == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    if (zr_string_equals_cstr(typeName, "Ptr")) {
        return "Ptr";
    }
    if (zr_string_equals_cstr(typeName, "Ptr32")) {
        return "Ptr32";
    }
    if (zr_string_equals_cstr(typeName, "Ptr64")) {
        return "Ptr64";
    }

    ZrCore_Array_Construct(&argumentTypeNames);
    if (!try_parse_generic_instance_type_name(state, typeName, &baseName, &argumentTypeNames)) {
        return ZR_NULL;
    }

    if (baseName != ZR_NULL) {
        if (zr_string_equals_cstr(baseName, "Ptr")) {
            family = "Ptr";
        } else if (zr_string_equals_cstr(baseName, "Ptr32")) {
            family = "Ptr32";
        } else if (zr_string_equals_cstr(baseName, "Ptr64")) {
            family = "Ptr64";
        }
    }

    ZrCore_Array_Free(state, &argumentTypeNames);
    return family;
}

static const TZrChar *ffi_default_pointer_family_literal(SZrState *state, const SZrInferredType *receiverType) {
    const TZrChar *family = ZR_NULL;

    if (state != ZR_NULL && receiverType != ZR_NULL && receiverType->typeName != ZR_NULL) {
        family = ffi_pointer_family_literal_from_type_name(state, receiverType->typeName);
    }

    return family != ZR_NULL ? family : "Ptr";
}

static SZrString *ffi_create_type_name(SZrState *state, const TZrChar *typeNameText) {
    if (state == ZR_NULL || typeNameText == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_CreateFromNative(state, (TZrNativeString)typeNameText);
}

static TZrBool ffi_build_pointer_type(SZrCompilerState *cs,
                                      const TZrChar *familyLiteral,
                                      const SZrInferredType *pointeeType,
                                      SZrInferredType *result) {
    TZrChar pointerTypeNameBuffer[ZR_PARSER_TEXT_BUFFER_LENGTH];
    TZrChar pointeeTypeBuffer[ZR_PARSER_TYPE_NAME_BUFFER_LENGTH];
    SZrString *pointerTypeName;
    const TZrChar *pointeeTypeText;

    if (cs == ZR_NULL || familyLiteral == ZR_NULL || pointeeType == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    pointeeTypeText = ZrParser_TypeNameString_Get(cs->state, pointeeType, pointeeTypeBuffer, sizeof(pointeeTypeBuffer));
    if (pointeeTypeText == ZR_NULL) {
        return ZR_FALSE;
    }

    if (snprintf(pointerTypeNameBuffer,
                 sizeof(pointerTypeNameBuffer),
                 "%s<%s>",
                 familyLiteral,
                 pointeeTypeText) <= 0) {
        return ZR_FALSE;
    }

    pointerTypeName = ffi_create_type_name(cs->state, pointerTypeNameBuffer);
    if (pointerTypeName == ZR_NULL) {
        return ZR_FALSE;
    }

    return inferred_type_from_type_name(cs, pointerTypeName, result);
}

static TZrBool ffi_descriptor_infer_from_type_name(SZrCompilerState *cs,
                                                   SZrString *typeName,
                                                   SZrInferredType *result,
                                                   TZrBool *outIsPointer) {
    if (outIsPointer != ZR_NULL) {
        *outIsPointer = ZR_FALSE;
    }

    if (cs == ZR_NULL || typeName == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!inferred_type_from_type_name(cs, typeName, result)) {
        return ZR_FALSE;
    }

    if (outIsPointer != ZR_NULL && ffi_pointer_family_literal_from_type_name(cs->state, typeName) != ZR_NULL) {
        *outIsPointer = ZR_TRUE;
    }

    return ZR_TRUE;
}

static TZrBool ffi_descriptor_infer_from_node(SZrCompilerState *cs,
                                              SZrAstNode *node,
                                              const TZrChar *defaultPointerFamily,
                                              SZrInferredType *result,
                                              TZrBool *outIsPointer) {
    SZrTypePrototypeInfo *prototype = ZR_NULL;
    SZrString *resolvedTypeName = ZR_NULL;

    if (outIsPointer != ZR_NULL) {
        *outIsPointer = ZR_FALSE;
    }

    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (node->type == ZR_AST_STRING_LITERAL) {
        SZrString *typeName = node->data.stringLiteral.value;

        if (typeName != ZR_NULL && zr_string_equals_cstr(typeName, "char")) {
            typeName = ffi_create_type_name(cs->state, "Char");
        } else if (typeName != ZR_NULL && zr_string_equals_cstr(typeName, "wchar")) {
            typeName = ffi_create_type_name(cs->state, "WChar");
        }

        return ffi_descriptor_infer_from_type_name(cs, typeName, result, outIsPointer);
    }

    if (node->type == ZR_AST_OBJECT_LITERAL) {
        SZrAstNode *kindNode = ffi_object_literal_find_property_value(&node->data.objectLiteral, "kind");
        SZrAstNode *widthNode = ffi_object_literal_find_property_value(&node->data.objectLiteral, "pointerWidth");
        const TZrChar *familyLiteral = defaultPointerFamily != ZR_NULL ? defaultPointerFamily : "Ptr";
        const TZrChar *kindText = ZR_NULL;

        if (kindNode != ZR_NULL && kindNode->type == ZR_AST_STRING_LITERAL && kindNode->data.stringLiteral.value != ZR_NULL) {
            kindText = ZrCore_String_GetNativeString(kindNode->data.stringLiteral.value);
        }

        if (widthNode != ZR_NULL && widthNode->type == ZR_AST_INTEGER_LITERAL) {
            if (widthNode->data.integerLiteral.value == 32) {
                familyLiteral = "Ptr32";
            } else if (widthNode->data.integerLiteral.value == 64) {
                familyLiteral = "Ptr64";
            }
        }

        if (kindText != ZR_NULL) {
            if (strcmp(kindText, "pointer32") == 0) {
                familyLiteral = "Ptr32";
            } else if (strcmp(kindText, "pointer64") == 0) {
                familyLiteral = "Ptr64";
            }

            if (strcmp(kindText, "pointer") == 0 || strcmp(kindText, "pointer32") == 0 || strcmp(kindText, "pointer64") == 0) {
                SZrAstNode *toNode = ffi_object_literal_find_property_value(&node->data.objectLiteral, "to");
                SZrInferredType pointeeType;
                TZrBool nestedPointer = ZR_FALSE;

                if (toNode == ZR_NULL) {
                    return ZR_FALSE;
                }

                ZrParser_InferredType_Init(cs->state, &pointeeType, ZR_VALUE_TYPE_OBJECT);
                if (!ffi_descriptor_infer_from_node(cs, toNode, familyLiteral, &pointeeType, &nestedPointer)) {
                    ZrParser_InferredType_Free(cs->state, &pointeeType);
                    return ZR_FALSE;
                }

                if (!ffi_build_pointer_type(cs, familyLiteral, &pointeeType, result)) {
                    ZrParser_InferredType_Free(cs->state, &pointeeType);
                    return ZR_FALSE;
                }

                ZrParser_InferredType_Free(cs->state, &pointeeType);
                if (outIsPointer != ZR_NULL) {
                    *outIsPointer = ZR_TRUE;
                }
                return ZR_TRUE;
            }

            if (strcmp(kindText, "string") == 0) {
                return ffi_descriptor_infer_from_type_name(cs, ffi_create_type_name(cs->state, "string"), result,
                                                           outIsPointer);
            }

            if (strcmp(kindText, "char") == 0) {
                return ffi_descriptor_infer_from_type_name(cs, ffi_create_type_name(cs->state, "Char"), result,
                                                           outIsPointer);
            }

            if (strcmp(kindText, "wchar") == 0) {
                return ffi_descriptor_infer_from_type_name(cs, ffi_create_type_name(cs->state, "WChar"), result,
                                                           outIsPointer);
            }
        }
    }

    if (resolve_prototype_target_inference(cs, node, &prototype, &resolvedTypeName) && resolvedTypeName != ZR_NULL) {
        return ffi_descriptor_infer_from_type_name(cs, resolvedTypeName, result, outIsPointer);
    }

    return ZR_FALSE;
}

static TZrBool ffi_receiver_is_imported_ffi_type(SZrCompilerState *cs, const SZrInferredType *receiverType) {
    SZrTypePrototypeInfo *prototype;

    if (cs == ZR_NULL || receiverType == ZR_NULL || receiverType->typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    prototype = find_compiler_type_prototype_inference(cs, receiverType->typeName);
    if (prototype == ZR_NULL || !prototype->isImportedNative) {
        return ZR_FALSE;
    }

    return ffi_pointer_family_literal_from_type_name(cs->state, receiverType->typeName) != ZR_NULL ||
           zr_string_equals_cstr(receiverType->typeName, "PointerHandle") ||
           zr_string_equals_cstr(receiverType->typeName, "BufferHandle");
}

TZrBool infer_ffi_member_call_type(SZrCompilerState *cs,
                                   const SZrInferredType *receiverType,
                                   const SZrTypeMemberInfo *memberInfo,
                                   SZrFunctionCall *call,
                                   SZrInferredType *result,
                                   TZrBool *outHandled) {
    const TZrChar *defaultPointerFamily;

    if (outHandled != ZR_NULL) {
        *outHandled = ZR_FALSE;
    }

    if (cs == ZR_NULL || receiverType == ZR_NULL || memberInfo == ZR_NULL || call == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ffi_receiver_is_imported_ffi_type(cs, receiverType) || memberInfo->name == ZR_NULL) {
        return ZR_TRUE;
    }

    defaultPointerFamily = ffi_default_pointer_family_literal(cs->state, receiverType);

    if (zr_string_equals_cstr(receiverType->typeName, "BufferHandle") && zr_string_equals_cstr(memberInfo->name, "pin")) {
        SZrInferredType pointeeType;

        ZrParser_InferredType_Init(cs->state, &pointeeType, ZR_VALUE_TYPE_OBJECT);
        if (!inferred_type_from_type_name(cs, ffi_create_type_name(cs->state, "u8"), &pointeeType) ||
            !ffi_build_pointer_type(cs, "Ptr", &pointeeType, result)) {
            ZrParser_InferredType_Free(cs->state, &pointeeType);
            return ZR_FALSE;
        }

        ZrParser_InferredType_Free(cs->state, &pointeeType);
        if (outHandled != ZR_NULL) {
            *outHandled = ZR_TRUE;
        }
        return ZR_TRUE;
    }

    if (zr_string_equals_cstr(memberInfo->name, "as")) {
        SZrInferredType descriptorType;
        TZrBool descriptorIsPointer = ZR_FALSE;

        if (call->args == ZR_NULL || call->args->count == 0 || call->args->nodes[0] == ZR_NULL) {
            return ZR_TRUE;
        }

        ZrParser_InferredType_Init(cs->state, &descriptorType, ZR_VALUE_TYPE_OBJECT);
        if (!ffi_descriptor_infer_from_node(cs,
                                            call->args->nodes[0],
                                            defaultPointerFamily,
                                            &descriptorType,
                                            &descriptorIsPointer)) {
            ZrParser_InferredType_Free(cs->state, &descriptorType);
            return ZR_TRUE;
        }

        if (descriptorIsPointer) {
            ZrParser_InferredType_Copy(cs->state, result, &descriptorType);
        } else if (!ffi_build_pointer_type(cs, defaultPointerFamily, &descriptorType, result)) {
            ZrParser_InferredType_Free(cs->state, &descriptorType);
            return ZR_FALSE;
        }

        ZrParser_InferredType_Free(cs->state, &descriptorType);
        if (outHandled != ZR_NULL) {
            *outHandled = ZR_TRUE;
        }
        return ZR_TRUE;
    }

    if (zr_string_equals_cstr(memberInfo->name, "read")) {
        TZrBool descriptorIsPointer = ZR_FALSE;

        if (call->args == ZR_NULL || call->args->count == 0 || call->args->nodes[0] == ZR_NULL) {
            return ZR_TRUE;
        }

        if (!ffi_descriptor_infer_from_node(cs,
                                            call->args->nodes[0],
                                            defaultPointerFamily,
                                            result,
                                            &descriptorIsPointer)) {
            return ZR_TRUE;
        }

        if (outHandled != ZR_NULL) {
            *outHandled = ZR_TRUE;
        }
        return ZR_TRUE;
    }

    return ZR_TRUE;
}
