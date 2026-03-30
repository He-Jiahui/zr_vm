//
// Created by Auto on 2025/01/XX.
//

#include "compiler_internal.h"

SZrAstNode *extern_compiler_find_named_declaration(SZrExternBlock *externBlock, SZrString *name) {
    if (externBlock == ZR_NULL || name == ZR_NULL || externBlock->declarations == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < externBlock->declarations->count; index++) {
        SZrAstNode *declaration = externBlock->declarations->nodes[index];
        if (declaration == ZR_NULL) {
            continue;
        }

        switch (declaration->type) {
            case ZR_AST_EXTERN_DELEGATE_DECLARATION:
                if (declaration->data.externDelegateDeclaration.name != ZR_NULL &&
                    declaration->data.externDelegateDeclaration.name->name != ZR_NULL &&
                    ZrCore_String_Equal(declaration->data.externDelegateDeclaration.name->name, name)) {
                    return declaration;
                }
                break;
            case ZR_AST_STRUCT_DECLARATION:
                if (declaration->data.structDeclaration.name != ZR_NULL &&
                    declaration->data.structDeclaration.name->name != ZR_NULL &&
                    ZrCore_String_Equal(declaration->data.structDeclaration.name->name, name)) {
                    return declaration;
                }
                break;
            case ZR_AST_ENUM_DECLARATION:
                if (declaration->data.enumDeclaration.name != ZR_NULL &&
                    declaration->data.enumDeclaration.name->name != ZR_NULL &&
                    ZrCore_String_Equal(declaration->data.enumDeclaration.name->name, name)) {
                    return declaration;
                }
                break;
            default:
                break;
        }
    }

    return ZR_NULL;
}

TZrBool extern_compiler_is_precise_ffi_primitive_name(SZrString *name) {
    static const TZrChar *kSupported[] = {
            "void", "bool", "i8", "u8", "i16", "u16", "i32", "u32", "i64", "u64", "f32", "f64"
    };

    if (name == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < ZR_ARRAY_COUNT(kSupported); index++) {
        if (extern_compiler_string_equals(name, kSupported[index])) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

TZrBool extern_compiler_wrap_pointer_descriptor(SZrCompilerState *cs,
                                                       SZrTypeValue *baseValue,
                                                       const TZrChar *directionText) {
    SZrObject *pointerObject;
    SZrTypeValue objectValue;
    ZrExternCompilerTempRoot pointerRoot;

    if (cs == ZR_NULL || baseValue == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!extern_compiler_temp_root_begin(cs, &pointerRoot)) {
        return ZR_FALSE;
    }

    pointerObject = extern_compiler_new_object_constant(cs);
    if (pointerObject == ZR_NULL) {
        extern_compiler_temp_root_end(&pointerRoot);
        return ZR_FALSE;
    }
    extern_compiler_temp_root_set_object(&pointerRoot, pointerObject, ZR_VALUE_TYPE_OBJECT);

    if (!extern_compiler_make_string_value(cs, "pointer", &objectValue) ||
        !extern_compiler_set_object_field(cs, pointerObject, "kind", &objectValue) ||
        !extern_compiler_set_object_field(cs, pointerObject, "to", baseValue)) {
        extern_compiler_temp_root_end(&pointerRoot);
        return ZR_FALSE;
    }

    if (directionText != ZR_NULL && directionText[0] != '\0') {
        if (!extern_compiler_make_string_value(cs, directionText, &objectValue) ||
            !extern_compiler_set_object_field(cs, pointerObject, "direction", &objectValue)) {
            extern_compiler_temp_root_end(&pointerRoot);
            return ZR_FALSE;
        }
    }

    ZrCore_Value_InitAsRawObject(cs->state, baseValue, ZR_CAST_RAW_OBJECT_AS_SUPER(pointerObject));
    baseValue->type = ZR_VALUE_TYPE_OBJECT;
    extern_compiler_temp_root_end(&pointerRoot);
    return ZR_TRUE;
}

TZrBool extern_compiler_descriptor_set_string_field(SZrCompilerState *cs,
                                                           SZrObject *object,
                                                           const TZrChar *fieldName,
                                                           const TZrChar *text) {
    SZrTypeValue stringValue;

    if (cs == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || text == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!extern_compiler_make_string_value(cs, text, &stringValue)) {
        return ZR_FALSE;
    }

    return extern_compiler_set_object_field(cs, object, fieldName, &stringValue);
}

TZrBool extern_compiler_descriptor_set_string_object_field(SZrCompilerState *cs,
                                                                  SZrObject *object,
                                                                  const TZrChar *fieldName,
                                                                  SZrString *text) {
    SZrTypeValue stringValue;

    if (cs == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || text == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(cs->state, &stringValue, ZR_CAST_RAW_OBJECT_AS_SUPER(text));
    stringValue.type = ZR_VALUE_TYPE_STRING;
    return extern_compiler_set_object_field(cs, object, fieldName, &stringValue);
}

TZrBool extern_compiler_descriptor_set_int_field(SZrCompilerState *cs,
                                                        SZrObject *object,
                                                        const TZrChar *fieldName,
                                                        TZrInt64 value) {
    SZrTypeValue intValue;

    if (cs == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsInt(cs->state, &intValue, value);
    return extern_compiler_set_object_field(cs, object, fieldName, &intValue);
}

const TZrChar *extern_compiler_direction_from_decorators(SZrAstNodeArray *decorators) {
    if (extern_compiler_decorators_has_flag(decorators, "out")) {
        return "out";
    }
    if (extern_compiler_decorators_has_flag(decorators, "inout")) {
        return "inout";
    }
    if (extern_compiler_decorators_has_flag(decorators, "in")) {
        return "in";
    }
    return ZR_NULL;
}

TZrBool extern_compiler_apply_string_charset(SZrCompilerState *cs,
                                                    SZrTypeValue *descriptorValue,
                                                    SZrString *charsetName) {
    SZrObject *stringObject;
    SZrTypeValue kindValue;
    const TZrChar *kindText = ZR_NULL;
    ZrExternCompilerTempRoot stringRoot;

    if (cs == ZR_NULL || descriptorValue == ZR_NULL || charsetName == ZR_NULL) {
        return ZR_TRUE;
    }

    if (descriptorValue->type == ZR_VALUE_TYPE_STRING && descriptorValue->value.object != ZR_NULL) {
        kindText = ZrCore_String_GetNativeString(ZR_CAST_STRING(cs->state, descriptorValue->value.object));
        if (kindText != ZR_NULL && strcmp(kindText, "string") == 0) {
            if (!extern_compiler_temp_root_begin(cs, &stringRoot)) {
                return ZR_FALSE;
            }
            stringObject = extern_compiler_new_object_constant(cs);
            if (stringObject == ZR_NULL) {
                extern_compiler_temp_root_end(&stringRoot);
                return ZR_FALSE;
            }
            extern_compiler_temp_root_set_object(&stringRoot, stringObject, ZR_VALUE_TYPE_OBJECT);
            if (!extern_compiler_make_string_value(cs, "string", &kindValue) ||
                !extern_compiler_set_object_field(cs, stringObject, "kind", &kindValue) ||
                !extern_compiler_descriptor_set_string_object_field(cs, stringObject, "encoding", charsetName)) {
                extern_compiler_temp_root_end(&stringRoot);
                return ZR_FALSE;
            }

            ZrCore_Value_InitAsRawObject(cs->state, descriptorValue, ZR_CAST_RAW_OBJECT_AS_SUPER(stringObject));
            descriptorValue->type = ZR_VALUE_TYPE_OBJECT;
            extern_compiler_temp_root_end(&stringRoot);
        }
        return ZR_TRUE;
    }

    if (descriptorValue->type == ZR_VALUE_TYPE_OBJECT && descriptorValue->value.object != ZR_NULL) {
        stringObject = ZR_CAST_OBJECT(cs->state, descriptorValue->value.object);
        if (stringObject == ZR_NULL) {
            return ZR_FALSE;
        }

        if (!extern_compiler_descriptor_set_string_object_field(cs, stringObject, "encoding", charsetName)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

TZrBool extern_compiler_build_type_descriptor_value(SZrCompilerState *cs,
                                                           SZrExternBlock *externBlock,
                                                           SZrType *type,
                                                           SZrAstNodeArray *decorators,
                                                           SZrFileRange location,
                                                           SZrTypeValue *outValue);

TZrBool extern_compiler_build_signature_descriptor_value(SZrCompilerState *cs,
                                                                SZrExternBlock *externBlock,
                                                                SZrAstNodeArray *params,
                                                                SZrParameter *args,
                                                                SZrType *returnType,
                                                                SZrAstNodeArray *decorators,
                                                                TZrBool includeKind,
                                                                SZrFileRange location,
                                                                SZrTypeValue *outValue);

TZrBool extern_compiler_build_struct_descriptor_value(SZrCompilerState *cs,
                                                             SZrExternBlock *externBlock,
                                                             SZrAstNode *declarationNode,
                                                             SZrTypeValue *outValue);

TZrBool extern_compiler_build_enum_descriptor_value(SZrCompilerState *cs,
                                                           SZrAstNode *declarationNode,
                                                           SZrTypeValue *outValue);

TZrBool extern_compiler_build_delegate_descriptor_value(SZrCompilerState *cs,
                                                               SZrExternBlock *externBlock,
                                                               SZrAstNode *declarationNode,
                                                               TZrBool includeKind,
                                                               SZrTypeValue *outValue) {
    SZrExternDelegateDeclaration *delegateDecl;

    if (cs == ZR_NULL || declarationNode == ZR_NULL || declarationNode->type != ZR_AST_EXTERN_DELEGATE_DECLARATION ||
        outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    delegateDecl = &declarationNode->data.externDelegateDeclaration;
    return extern_compiler_build_signature_descriptor_value(cs,
                                                            externBlock,
                                                            delegateDecl->params,
                                                            delegateDecl->args,
                                                            delegateDecl->returnType,
                                                            delegateDecl->decorators,
                                                            includeKind,
                                                            declarationNode->location,
                                                            outValue);
}

TZrBool extern_compiler_build_type_descriptor_value(SZrCompilerState *cs,
                                                           SZrExternBlock *externBlock,
                                                           SZrType *type,
                                                           SZrAstNodeArray *decorators,
                                                           SZrFileRange location,
                                                           SZrTypeValue *outValue) {
    SZrString *charsetName;
    const TZrChar *directionText;

    if (outValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
    }
    if (cs == ZR_NULL || type == ZR_NULL || outValue == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "extern ffi type descriptor is missing a type annotation", location);
        return ZR_FALSE;
    }

    charsetName = extern_compiler_decorators_get_string_arg(decorators, "charset");
    directionText = extern_compiler_direction_from_decorators(decorators);

    if (type->name != ZR_NULL && type->name->type == ZR_AST_GENERIC_TYPE) {
        SZrGenericType *genericType = &type->name->data.genericType;
        if (genericType->name != ZR_NULL &&
            extern_compiler_string_equals(genericType->name->name, "pointer") &&
            genericType->params != ZR_NULL &&
            genericType->params->count == 1 &&
            genericType->params->nodes[0] != ZR_NULL &&
            genericType->params->nodes[0]->type == ZR_AST_TYPE) {
            if (!extern_compiler_build_type_descriptor_value(cs,
                                                             externBlock,
                                                             &genericType->params->nodes[0]->data.type,
                                                             ZR_NULL,
                                                             genericType->params->nodes[0]->location,
                                                             outValue) ||
                !extern_compiler_wrap_pointer_descriptor(cs,
                                                         outValue,
                                                         directionText != ZR_NULL ? directionText : "in")) {
                return ZR_FALSE;
            }
            return ZR_TRUE;
        }
    }

    if (type->name == ZR_NULL || type->name->type != ZR_AST_IDENTIFIER_LITERAL ||
        type->name->data.identifier.name == ZR_NULL) {
        ZrParser_Compiler_Error(cs,
                                "extern ffi only accepts precise identifier or pointer<T> type syntax in v1",
                                location);
        return ZR_FALSE;
    }

    if (extern_compiler_is_precise_ffi_primitive_name(type->name->data.identifier.name)) {
        ZrCore_Value_InitAsRawObject(cs->state,
                                     outValue,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(type->name->data.identifier.name));
        outValue->type = ZR_VALUE_TYPE_STRING;
    } else if (extern_compiler_string_equals(type->name->data.identifier.name, "string")) {
        if (!extern_compiler_make_string_value(cs, "string", outValue)) {
            ZrParser_Compiler_Error(cs, "failed to build extern ffi string descriptor", location);
            return ZR_FALSE;
        }
    } else {
        SZrAstNode *namedDeclaration =
                extern_compiler_find_named_declaration(externBlock, type->name->data.identifier.name);
        if (namedDeclaration == ZR_NULL) {
            ZrParser_Compiler_Error(cs,
                                    "extern ffi type must resolve to a precise primitive or extern struct/enum/delegate",
                                    location);
            return ZR_FALSE;
        }

        switch (namedDeclaration->type) {
            case ZR_AST_EXTERN_DELEGATE_DECLARATION:
                if (!extern_compiler_build_delegate_descriptor_value(cs, externBlock, namedDeclaration, ZR_TRUE, outValue)) {
                    return ZR_FALSE;
                }
                break;
            case ZR_AST_STRUCT_DECLARATION:
                if (!extern_compiler_build_struct_descriptor_value(cs, externBlock, namedDeclaration, outValue)) {
                    return ZR_FALSE;
                }
                break;
            case ZR_AST_ENUM_DECLARATION:
                if (!extern_compiler_build_enum_descriptor_value(cs, namedDeclaration, outValue)) {
                    return ZR_FALSE;
                }
                break;
            default:
                ZrParser_Compiler_Error(cs, "unsupported extern ffi referenced type declaration", location);
                return ZR_FALSE;
        }
    }

    if (!extern_compiler_apply_string_charset(cs, outValue, charsetName)) {
        ZrParser_Compiler_Error(cs, "failed to apply extern ffi charset decorator", location);
        return ZR_FALSE;
    }

    if (directionText != ZR_NULL) {
        if (!extern_compiler_wrap_pointer_descriptor(cs, outValue, directionText)) {
            ZrParser_Compiler_Error(cs, "failed to apply extern ffi parameter direction", location);
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

TZrBool extern_compiler_build_signature_descriptor_value(SZrCompilerState *cs,
                                                                SZrExternBlock *externBlock,
                                                                SZrAstNodeArray *params,
                                                                SZrParameter *args,
                                                                SZrType *returnType,
                                                                SZrAstNodeArray *decorators,
                                                                TZrBool includeKind,
                                                                SZrFileRange location,
                                                                SZrTypeValue *outValue) {
    SZrObject *signatureObject;
    SZrObject *parametersArray;
    SZrTypeValue signatureValue;
    SZrTypeValue returnTypeValue;
    SZrString *callconvName;
    ZrExternCompilerTempRoot signatureRoot;
    ZrExternCompilerTempRoot parametersRoot;

    if (cs == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!extern_compiler_temp_root_begin(cs, &signatureRoot) ||
        !extern_compiler_temp_root_begin(cs, &parametersRoot)) {
        if (signatureRoot.active) {
            extern_compiler_temp_root_end(&signatureRoot);
        }
        if (parametersRoot.active) {
            extern_compiler_temp_root_end(&parametersRoot);
        }
        return ZR_FALSE;
    }

    signatureObject = extern_compiler_new_object_constant(cs);
    parametersArray = extern_compiler_new_array_constant(cs);
    if (signatureObject == ZR_NULL || parametersArray == ZR_NULL) {
        extern_compiler_temp_root_end(&parametersRoot);
        extern_compiler_temp_root_end(&signatureRoot);
        ZrParser_Compiler_Error(cs, "failed to allocate extern ffi signature descriptor", location);
        return ZR_FALSE;
    }
    extern_compiler_temp_root_set_object(&signatureRoot, signatureObject, ZR_VALUE_TYPE_OBJECT);
    extern_compiler_temp_root_set_object(&parametersRoot, parametersArray, ZR_VALUE_TYPE_ARRAY);
    if (includeKind &&
        !extern_compiler_descriptor_set_string_field(cs, signatureObject, "kind", "function")) {
        extern_compiler_temp_root_end(&parametersRoot);
        extern_compiler_temp_root_end(&signatureRoot);
        ZrParser_Compiler_Error(cs, "failed to build extern ffi function kind", location);
        return ZR_FALSE;
    }

    if (returnType != ZR_NULL) {
        if (!extern_compiler_build_type_descriptor_value(cs,
                                                         externBlock,
                                                         returnType,
                                                         decorators,
                                                         location,
                                                         &returnTypeValue)) {
            extern_compiler_temp_root_end(&parametersRoot);
            extern_compiler_temp_root_end(&signatureRoot);
            return ZR_FALSE;
        }
    } else if (!extern_compiler_make_string_value(cs, "void", &returnTypeValue)) {
        extern_compiler_temp_root_end(&parametersRoot);
        extern_compiler_temp_root_end(&signatureRoot);
        ZrParser_Compiler_Error(cs, "failed to build extern ffi void return type", location);
        return ZR_FALSE;
    }

    if (!extern_compiler_set_object_field(cs, signatureObject, "returnType", &returnTypeValue)) {
        extern_compiler_temp_root_end(&parametersRoot);
        extern_compiler_temp_root_end(&signatureRoot);
        ZrParser_Compiler_Error(cs, "failed to set extern ffi return type", location);
        return ZR_FALSE;
    }

    if (params != ZR_NULL) {
        for (TZrSize index = 0; index < params->count; index++) {
            SZrAstNode *paramNode = params->nodes[index];
            SZrObject *parameterObject;
            SZrTypeValue parameterObjectValue;
            SZrTypeValue parameterTypeValue;
            ZrExternCompilerTempRoot parameterRoot;

            if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
                continue;
            }

            if (!extern_compiler_build_type_descriptor_value(cs,
                                                             externBlock,
                                                             paramNode->data.parameter.typeInfo,
                                                             paramNode->data.parameter.decorators,
                                                             paramNode->location,
                                                             &parameterTypeValue)) {
                extern_compiler_temp_root_end(&parametersRoot);
                extern_compiler_temp_root_end(&signatureRoot);
                return ZR_FALSE;
            }

            if (!extern_compiler_temp_root_begin(cs, &parameterRoot)) {
                extern_compiler_temp_root_end(&parametersRoot);
                extern_compiler_temp_root_end(&signatureRoot);
                ZrParser_Compiler_Error(cs, "failed to root extern ffi parameter descriptor", paramNode->location);
                return ZR_FALSE;
            }
            parameterObject = extern_compiler_new_object_constant(cs);
            if (parameterObject == ZR_NULL ||
                !extern_compiler_set_object_field(cs, parameterObject, "type", &parameterTypeValue)) {
                extern_compiler_temp_root_end(&parameterRoot);
                extern_compiler_temp_root_end(&parametersRoot);
                extern_compiler_temp_root_end(&signatureRoot);
                ZrParser_Compiler_Error(cs, "failed to build extern ffi parameter descriptor", paramNode->location);
                return ZR_FALSE;
            }
            extern_compiler_temp_root_set_object(&parameterRoot, parameterObject, ZR_VALUE_TYPE_OBJECT);

            ZrCore_Value_InitAsRawObject(cs->state, &parameterObjectValue, ZR_CAST_RAW_OBJECT_AS_SUPER(parameterObject));
            parameterObjectValue.type = ZR_VALUE_TYPE_OBJECT;
            if (!extern_compiler_push_array_value(cs, parametersArray, &parameterObjectValue)) {
                extern_compiler_temp_root_end(&parameterRoot);
                extern_compiler_temp_root_end(&parametersRoot);
                extern_compiler_temp_root_end(&signatureRoot);
                ZrParser_Compiler_Error(cs, "failed to append extern ffi parameter descriptor", paramNode->location);
                return ZR_FALSE;
            }
            extern_compiler_temp_root_end(&parameterRoot);
        }
    }

    if (args != ZR_NULL) {
        SZrTypeValue varargsValue;
        ZrCore_Value_ResetAsNull(&varargsValue);
        varargsValue.type = ZR_VALUE_TYPE_BOOL;
        varargsValue.value.nativeObject.nativeBool = ZR_TRUE;
        if (!extern_compiler_set_object_field(cs, signatureObject, "varargs", &varargsValue)) {
            extern_compiler_temp_root_end(&parametersRoot);
            extern_compiler_temp_root_end(&signatureRoot);
            ZrParser_Compiler_Error(cs, "failed to mark extern ffi varargs signature", location);
            return ZR_FALSE;
        }
    }

    callconvName = extern_compiler_decorators_get_string_arg(decorators, "callconv");
    if (callconvName != ZR_NULL &&
        !extern_compiler_descriptor_set_string_object_field(cs, signatureObject, "abi", callconvName)) {
        extern_compiler_temp_root_end(&parametersRoot);
        extern_compiler_temp_root_end(&signatureRoot);
        ZrParser_Compiler_Error(cs, "failed to set extern ffi calling convention", location);
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(cs->state, &signatureValue, ZR_CAST_RAW_OBJECT_AS_SUPER(parametersArray));
    signatureValue.type = ZR_VALUE_TYPE_ARRAY;
    if (!extern_compiler_set_object_field(cs, signatureObject, "parameters", &signatureValue)) {
        extern_compiler_temp_root_end(&parametersRoot);
        extern_compiler_temp_root_end(&signatureRoot);
        ZrParser_Compiler_Error(cs, "failed to set extern ffi parameters array", location);
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(cs->state, outValue, ZR_CAST_RAW_OBJECT_AS_SUPER(signatureObject));
    outValue->type = ZR_VALUE_TYPE_OBJECT;
    extern_compiler_temp_root_end(&parametersRoot);
    extern_compiler_temp_root_end(&signatureRoot);
    return ZR_TRUE;
}

TZrBool extern_compiler_build_struct_descriptor_value(SZrCompilerState *cs,
                                                             SZrExternBlock *externBlock,
                                                             SZrAstNode *declarationNode,
                                                             SZrTypeValue *outValue) {
    SZrStructDeclaration *structDecl;
    SZrObject *structObject;
    SZrObject *fieldsArray;
    SZrTypeValue fieldsValue;
    TZrInt64 packValue = 0;
    TZrInt64 alignValue = 0;
    ZrExternCompilerTempRoot structRoot;
    ZrExternCompilerTempRoot fieldsRoot;

    if (cs == ZR_NULL || declarationNode == ZR_NULL || declarationNode->type != ZR_AST_STRUCT_DECLARATION ||
        outValue == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!extern_compiler_temp_root_begin(cs, &structRoot) ||
        !extern_compiler_temp_root_begin(cs, &fieldsRoot)) {
        if (structRoot.active) {
            extern_compiler_temp_root_end(&structRoot);
        }
        if (fieldsRoot.active) {
            extern_compiler_temp_root_end(&fieldsRoot);
        }
        return ZR_FALSE;
    }

    structDecl = &declarationNode->data.structDeclaration;
    structObject = extern_compiler_new_object_constant(cs);
    fieldsArray = extern_compiler_new_array_constant(cs);
    if (structObject == ZR_NULL || fieldsArray == ZR_NULL ||
        structDecl->name == ZR_NULL || structDecl->name->name == ZR_NULL) {
        extern_compiler_temp_root_end(&fieldsRoot);
        extern_compiler_temp_root_end(&structRoot);
        return ZR_FALSE;
    }
    extern_compiler_temp_root_set_object(&structRoot, structObject, ZR_VALUE_TYPE_OBJECT);
    extern_compiler_temp_root_set_object(&fieldsRoot, fieldsArray, ZR_VALUE_TYPE_ARRAY);

    if (!extern_compiler_descriptor_set_string_field(cs, structObject, "kind", "struct") ||
        !extern_compiler_descriptor_set_string_object_field(cs, structObject, "name", structDecl->name->name)) {
        extern_compiler_temp_root_end(&fieldsRoot);
        extern_compiler_temp_root_end(&structRoot);
        return ZR_FALSE;
    }

    if (extern_compiler_decorators_get_int_arg(structDecl->decorators, "pack", &packValue) &&
        !extern_compiler_descriptor_set_int_field(cs, structObject, "pack", packValue)) {
        extern_compiler_temp_root_end(&fieldsRoot);
        extern_compiler_temp_root_end(&structRoot);
        return ZR_FALSE;
    }
    if (extern_compiler_decorators_get_int_arg(structDecl->decorators, "align", &alignValue) &&
        !extern_compiler_descriptor_set_int_field(cs, structObject, "align", alignValue)) {
        extern_compiler_temp_root_end(&fieldsRoot);
        extern_compiler_temp_root_end(&structRoot);
        return ZR_FALSE;
    }

    if (structDecl->members != ZR_NULL) {
        for (TZrSize index = 0; index < structDecl->members->count; index++) {
            SZrAstNode *member = structDecl->members->nodes[index];
            SZrObject *fieldObject;
            SZrTypeValue fieldObjectValue;
            SZrTypeValue fieldTypeValue;
            TZrInt64 offsetValue = 0;
            ZrExternCompilerTempRoot fieldRoot;

            if (member == ZR_NULL || member->type != ZR_AST_STRUCT_FIELD) {
                continue;
            }

            if (!extern_compiler_build_type_descriptor_value(cs,
                                                             externBlock,
                                                             member->data.structField.typeInfo,
                                                             member->data.structField.decorators,
                                                             member->location,
                                                             &fieldTypeValue)) {
                extern_compiler_temp_root_end(&fieldsRoot);
                extern_compiler_temp_root_end(&structRoot);
                return ZR_FALSE;
            }

            if (!extern_compiler_temp_root_begin(cs, &fieldRoot)) {
                extern_compiler_temp_root_end(&fieldsRoot);
                extern_compiler_temp_root_end(&structRoot);
                return ZR_FALSE;
            }
            fieldObject = extern_compiler_new_object_constant(cs);
            if (fieldObject == ZR_NULL ||
                member->data.structField.name == ZR_NULL ||
                member->data.structField.name->name == ZR_NULL ||
                !extern_compiler_descriptor_set_string_object_field(cs,
                                                                    fieldObject,
                                                                    "name",
                                                                    member->data.structField.name->name) ||
                !extern_compiler_set_object_field(cs, fieldObject, "type", &fieldTypeValue)) {
                extern_compiler_temp_root_end(&fieldRoot);
                extern_compiler_temp_root_end(&fieldsRoot);
                extern_compiler_temp_root_end(&structRoot);
                return ZR_FALSE;
            }
            extern_compiler_temp_root_set_object(&fieldRoot, fieldObject, ZR_VALUE_TYPE_OBJECT);

            if (extern_compiler_decorators_get_int_arg(member->data.structField.decorators, "offset", &offsetValue) &&
                !extern_compiler_descriptor_set_int_field(cs, fieldObject, "offset", offsetValue)) {
                extern_compiler_temp_root_end(&fieldRoot);
                extern_compiler_temp_root_end(&fieldsRoot);
                extern_compiler_temp_root_end(&structRoot);
                return ZR_FALSE;
            }

            ZrCore_Value_InitAsRawObject(cs->state, &fieldObjectValue, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldObject));
            fieldObjectValue.type = ZR_VALUE_TYPE_OBJECT;
            if (!extern_compiler_push_array_value(cs, fieldsArray, &fieldObjectValue)) {
                extern_compiler_temp_root_end(&fieldRoot);
                extern_compiler_temp_root_end(&fieldsRoot);
                extern_compiler_temp_root_end(&structRoot);
                return ZR_FALSE;
            }
            extern_compiler_temp_root_end(&fieldRoot);
        }
    }

    ZrCore_Value_InitAsRawObject(cs->state, &fieldsValue, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldsArray));
    fieldsValue.type = ZR_VALUE_TYPE_ARRAY;
    if (!extern_compiler_set_object_field(cs, structObject, "fields", &fieldsValue)) {
        extern_compiler_temp_root_end(&fieldsRoot);
        extern_compiler_temp_root_end(&structRoot);
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(cs->state, outValue, ZR_CAST_RAW_OBJECT_AS_SUPER(structObject));
    outValue->type = ZR_VALUE_TYPE_OBJECT;
    extern_compiler_temp_root_end(&fieldsRoot);
    extern_compiler_temp_root_end(&structRoot);
    return ZR_TRUE;
}

TZrBool extern_compiler_build_enum_descriptor_value(SZrCompilerState *cs,
                                                           SZrAstNode *declarationNode,
                                                           SZrTypeValue *outValue) {
    SZrEnumDeclaration *enumDecl;
    SZrObject *enumObject;
    SZrObject *membersArray;
    SZrTypeValue membersValue;
    SZrTypeValue underlyingValue;
    TZrInt64 nextAutoValue = 0;
    ZrExternCompilerTempRoot enumRoot;
    ZrExternCompilerTempRoot membersRoot;

    if (cs == ZR_NULL || declarationNode == ZR_NULL || declarationNode->type != ZR_AST_ENUM_DECLARATION ||
        outValue == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!extern_compiler_temp_root_begin(cs, &enumRoot) ||
        !extern_compiler_temp_root_begin(cs, &membersRoot)) {
        if (enumRoot.active) {
            extern_compiler_temp_root_end(&enumRoot);
        }
        if (membersRoot.active) {
            extern_compiler_temp_root_end(&membersRoot);
        }
        return ZR_FALSE;
    }

    enumDecl = &declarationNode->data.enumDeclaration;
    enumObject = extern_compiler_new_object_constant(cs);
    membersArray = extern_compiler_new_array_constant(cs);
    if (enumObject == ZR_NULL || membersArray == ZR_NULL ||
        enumDecl->name == ZR_NULL || enumDecl->name->name == ZR_NULL) {
        extern_compiler_temp_root_end(&membersRoot);
        extern_compiler_temp_root_end(&enumRoot);
        return ZR_FALSE;
    }
    extern_compiler_temp_root_set_object(&enumRoot, enumObject, ZR_VALUE_TYPE_OBJECT);
    extern_compiler_temp_root_set_object(&membersRoot, membersArray, ZR_VALUE_TYPE_ARRAY);

    if (!extern_compiler_descriptor_set_string_field(cs, enumObject, "kind", "enum") ||
        !extern_compiler_descriptor_set_string_object_field(cs, enumObject, "name", enumDecl->name->name)) {
        extern_compiler_temp_root_end(&membersRoot);
        extern_compiler_temp_root_end(&enumRoot);
        return ZR_FALSE;
    }

    if (enumDecl->baseType != ZR_NULL) {
        if (!extern_compiler_build_type_descriptor_value(cs,
                                                         ZR_NULL,
                                                         enumDecl->baseType,
                                                         enumDecl->decorators,
                                                         declarationNode->location,
                                                         &underlyingValue)) {
            extern_compiler_temp_root_end(&membersRoot);
            extern_compiler_temp_root_end(&enumRoot);
            return ZR_FALSE;
        }
    } else {
        SZrString *underlyingName = extern_compiler_decorators_get_string_arg(enumDecl->decorators, "underlying");
        if (underlyingName != ZR_NULL) {
            ZrCore_Value_InitAsRawObject(cs->state, &underlyingValue, ZR_CAST_RAW_OBJECT_AS_SUPER(underlyingName));
            underlyingValue.type = ZR_VALUE_TYPE_STRING;
        } else if (!extern_compiler_make_string_value(cs, "i32", &underlyingValue)) {
            extern_compiler_temp_root_end(&membersRoot);
            extern_compiler_temp_root_end(&enumRoot);
            return ZR_FALSE;
        }
    }

    if (!extern_compiler_set_object_field(cs, enumObject, "underlyingType", &underlyingValue)) {
        extern_compiler_temp_root_end(&membersRoot);
        extern_compiler_temp_root_end(&enumRoot);
        return ZR_FALSE;
    }

    if (enumDecl->members != ZR_NULL) {
        for (TZrSize index = 0; index < enumDecl->members->count; index++) {
            SZrAstNode *memberNode = enumDecl->members->nodes[index];
            SZrObject *memberObject;
            SZrTypeValue memberObjectValue;
            SZrTypeValue memberValue;
            TZrInt64 explicitValue = 0;
            TZrBool hasExplicitValue = ZR_FALSE;
            const TZrChar *memberNameText;
            ZrExternCompilerTempRoot memberRoot;

            if (memberNode == ZR_NULL || memberNode->type != ZR_AST_ENUM_MEMBER ||
                memberNode->data.enumMember.name == ZR_NULL || memberNode->data.enumMember.name->name == ZR_NULL) {
                continue;
            }

            if (!extern_compiler_temp_root_begin(cs, &memberRoot)) {
                extern_compiler_temp_root_end(&membersRoot);
                extern_compiler_temp_root_end(&enumRoot);
                return ZR_FALSE;
            }
            memberObject = extern_compiler_new_object_constant(cs);
            if (memberObject == ZR_NULL ||
                !extern_compiler_descriptor_set_string_object_field(cs,
                                                                    memberObject,
                                                                    "name",
                                                                    memberNode->data.enumMember.name->name)) {
                extern_compiler_temp_root_end(&memberRoot);
                extern_compiler_temp_root_end(&membersRoot);
                extern_compiler_temp_root_end(&enumRoot);
                return ZR_FALSE;
            }
            extern_compiler_temp_root_set_object(&memberRoot, memberObject, ZR_VALUE_TYPE_OBJECT);

            if (memberNode->data.enumMember.value != ZR_NULL &&
                memberNode->data.enumMember.value->type == ZR_AST_INTEGER_LITERAL) {
                explicitValue = memberNode->data.enumMember.value->data.integerLiteral.value;
                hasExplicitValue = ZR_TRUE;
            } else if (extern_compiler_decorators_get_int_arg(memberNode->data.enumMember.decorators,
                                                              "value",
                                                              &explicitValue)) {
                hasExplicitValue = ZR_TRUE;
            }

            if (!hasExplicitValue) {
                explicitValue = nextAutoValue;
            }
            nextAutoValue = explicitValue + 1;

            ZrCore_Value_InitAsInt(cs->state, &memberValue, explicitValue);
            memberNameText = ZrCore_String_GetNativeString(memberNode->data.enumMember.name->name);
            if (memberNameText == ZR_NULL ||
                !extern_compiler_set_object_field(cs, memberObject, "value", &memberValue) ||
                !extern_compiler_set_object_field(cs, enumObject, memberNameText, &memberValue)) {
                extern_compiler_temp_root_end(&memberRoot);
                extern_compiler_temp_root_end(&membersRoot);
                extern_compiler_temp_root_end(&enumRoot);
                return ZR_FALSE;
            }

            ZrCore_Value_InitAsRawObject(cs->state, &memberObjectValue, ZR_CAST_RAW_OBJECT_AS_SUPER(memberObject));
            memberObjectValue.type = ZR_VALUE_TYPE_OBJECT;
            if (!extern_compiler_push_array_value(cs, membersArray, &memberObjectValue)) {
                extern_compiler_temp_root_end(&memberRoot);
                extern_compiler_temp_root_end(&membersRoot);
                extern_compiler_temp_root_end(&enumRoot);
                return ZR_FALSE;
            }
            extern_compiler_temp_root_end(&memberRoot);
        }
    }

    ZrCore_Value_InitAsRawObject(cs->state, &membersValue, ZR_CAST_RAW_OBJECT_AS_SUPER(membersArray));
    membersValue.type = ZR_VALUE_TYPE_ARRAY;
    if (!extern_compiler_set_object_field(cs, enumObject, "members", &membersValue)) {
        extern_compiler_temp_root_end(&membersRoot);
        extern_compiler_temp_root_end(&enumRoot);
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(cs->state, outValue, ZR_CAST_RAW_OBJECT_AS_SUPER(enumObject));
    outValue->type = ZR_VALUE_TYPE_OBJECT;
    extern_compiler_temp_root_end(&membersRoot);
    extern_compiler_temp_root_end(&enumRoot);
    return ZR_TRUE;
}

