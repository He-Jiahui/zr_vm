//
// Created by Auto on 2025/01/XX.
//

#include "compiler_internal.h"

SZrString *create_hidden_extern_local_name(SZrCompilerState *cs, const TZrChar *prefix) {
    TZrChar buffer[96];
    int length;

    if (cs == ZR_NULL || cs->state == ZR_NULL || prefix == ZR_NULL) {
        return ZR_NULL;
    }

    length = snprintf(buffer,
                      sizeof(buffer),
                      "__zr_extern_%s_%u_%u",
                      prefix,
                      (unsigned)cs->scopeStack.length,
                      (unsigned)cs->localVars.length);
    if (length < 0) {
        return ZR_NULL;
    }

    if ((size_t)length >= sizeof(buffer)) {
        length = (int)sizeof(buffer) - 1;
        buffer[length] = '\0';
    }

    return ZrCore_String_Create(cs->state, buffer, (TZrSize)length);
}

TZrBool extern_compiler_string_equals(SZrString *value, const TZrChar *literal) {
    TZrNativeString nativeValue;

    if (value == ZR_NULL || literal == ZR_NULL) {
        return ZR_FALSE;
    }

    nativeValue = ZrCore_String_GetNativeString(value);
    return nativeValue != ZR_NULL && strcmp(nativeValue, literal) == 0;
}

TZrBool extern_compiler_identifier_equals(SZrAstNode *node, const TZrChar *literal) {
    if (node == ZR_NULL || node->type != ZR_AST_IDENTIFIER_LITERAL || literal == ZR_NULL) {
        return ZR_FALSE;
    }

    return extern_compiler_string_equals(node->data.identifier.name, literal);
}

TZrBool extern_compiler_make_string_value(SZrCompilerState *cs, const TZrChar *text, SZrTypeValue *outValue) {
    SZrString *stringObject;

    if (cs == ZR_NULL || text == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    stringObject = ZrCore_String_CreateFromNative(cs->state, (TZrNativeString)text);
    if (stringObject == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(cs->state, outValue, ZR_CAST_RAW_OBJECT_AS_SUPER(stringObject));
    outValue->type = ZR_VALUE_TYPE_STRING;
    return ZR_TRUE;
}

TZrBool extern_compiler_temp_root_begin(SZrCompilerState *cs, ZrExternCompilerTempRoot *root) {
    TZrStackValuePointer savedStackTop;
    TZrStackValuePointer slot;

    if (cs == ZR_NULL || cs->state == ZR_NULL || root == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(root, 0, sizeof(*root));
    root->state = cs->state;
    savedStackTop = cs->state->stackTop.valuePointer;
    if (savedStackTop == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Function_StackAnchorInit(cs->state, savedStackTop, &root->savedStackTopAnchor);
    slot = ZrCore_Function_CheckStackAndAnchor(cs->state, 1, savedStackTop, savedStackTop, &root->slotAnchor);
    if (slot == ZR_NULL) {
        memset(root, 0, sizeof(*root));
        return ZR_FALSE;
    }

    slot = ZrCore_Function_StackAnchorRestore(cs->state, &root->slotAnchor);
    if (slot == ZR_NULL) {
        memset(root, 0, sizeof(*root));
        return ZR_FALSE;
    }

    cs->state->stackTop.valuePointer = slot + 1;
    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(slot));
    root->active = ZR_TRUE;
    return ZR_TRUE;
}

SZrTypeValue *extern_compiler_temp_root_value(ZrExternCompilerTempRoot *root) {
    TZrStackValuePointer slot;

    if (root == ZR_NULL || !root->active || root->state == ZR_NULL) {
        return ZR_NULL;
    }

    slot = ZrCore_Function_StackAnchorRestore(root->state, &root->slotAnchor);
    return slot != ZR_NULL ? ZrCore_Stack_GetValue(slot) : ZR_NULL;
}

TZrBool extern_compiler_temp_root_set_value(ZrExternCompilerTempRoot *root, const SZrTypeValue *value) {
    SZrTypeValue *slotValue = extern_compiler_temp_root_value(root);
    if (slotValue == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    *slotValue = *value;
    return ZR_TRUE;
}

TZrBool extern_compiler_temp_root_set_object(ZrExternCompilerTempRoot *root,
                                                    SZrObject *object,
                                                    EZrValueType type) {
    SZrTypeValue *slotValue = extern_compiler_temp_root_value(root);
    if (slotValue == ZR_NULL || object == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(root->state, slotValue, ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    slotValue->type = type;
    return ZR_TRUE;
}

void extern_compiler_temp_root_end(ZrExternCompilerTempRoot *root) {
    if (root == ZR_NULL || !root->active || root->state == ZR_NULL) {
        return;
    }

    root->state->stackTop.valuePointer = ZrCore_Function_StackAnchorRestore(root->state, &root->savedStackTopAnchor);
    memset(root, 0, sizeof(*root));
}

TZrBool extern_compiler_set_object_field(SZrCompilerState *cs,
                                                SZrObject *object,
                                                const TZrChar *fieldName,
                                                const SZrTypeValue *value) {
    SZrString *fieldString;
    SZrTypeValue key;
    ZrExternCompilerTempRoot objectRoot;
    ZrExternCompilerTempRoot valueRoot;
    TZrBool objectRootActive = ZR_FALSE;
    TZrBool valueRootActive = ZR_FALSE;

    if (cs == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    if (extern_compiler_temp_root_begin(cs, &objectRoot)) {
        objectRootActive = extern_compiler_temp_root_set_object(&objectRoot, object, ZR_VALUE_TYPE_OBJECT);
    }
    if ((value->type == ZR_VALUE_TYPE_OBJECT || value->type == ZR_VALUE_TYPE_ARRAY || value->type == ZR_VALUE_TYPE_STRING) &&
        value->value.object != ZR_NULL &&
        extern_compiler_temp_root_begin(cs, &valueRoot)) {
        valueRootActive = extern_compiler_temp_root_set_value(&valueRoot, value);
    }

    fieldString = ZrCore_String_CreateFromNative(cs->state, (TZrNativeString)fieldName);
    if (fieldString == ZR_NULL) {
        if (valueRootActive) {
            extern_compiler_temp_root_end(&valueRoot);
        }
        if (objectRootActive) {
            extern_compiler_temp_root_end(&objectRoot);
        }
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(cs->state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    key.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Object_SetValue(cs->state, object, &key, value);
    if (valueRootActive) {
        extern_compiler_temp_root_end(&valueRoot);
    }
    if (objectRootActive) {
        extern_compiler_temp_root_end(&objectRoot);
    }
    return ZR_TRUE;
}

TZrBool extern_compiler_push_array_value(SZrCompilerState *cs, SZrObject *array, const SZrTypeValue *value) {
    SZrTypeValue key;
    ZrExternCompilerTempRoot arrayRoot;
    ZrExternCompilerTempRoot valueRoot;
    TZrBool arrayRootActive = ZR_FALSE;
    TZrBool valueRootActive = ZR_FALSE;

    if (cs == ZR_NULL || array == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    if (extern_compiler_temp_root_begin(cs, &arrayRoot)) {
        arrayRootActive = extern_compiler_temp_root_set_object(&arrayRoot, array, ZR_VALUE_TYPE_ARRAY);
    }
    if ((value->type == ZR_VALUE_TYPE_OBJECT || value->type == ZR_VALUE_TYPE_ARRAY || value->type == ZR_VALUE_TYPE_STRING) &&
        value->value.object != ZR_NULL &&
        extern_compiler_temp_root_begin(cs, &valueRoot)) {
        valueRootActive = extern_compiler_temp_root_set_value(&valueRoot, value);
    }

    ZrCore_Value_InitAsInt(cs->state, &key, (TZrInt64)array->nodeMap.elementCount);
    ZrCore_Object_SetValue(cs->state, array, &key, value);
    if (valueRootActive) {
        extern_compiler_temp_root_end(&valueRoot);
    }
    if (arrayRootActive) {
        extern_compiler_temp_root_end(&arrayRoot);
    }
    return ZR_TRUE;
}

SZrObject *extern_compiler_new_object_constant(SZrCompilerState *cs) {
    SZrObject *object;

    if (cs == ZR_NULL || cs->state == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZrCore_Object_New(cs->state, ZR_NULL);
    if (object != ZR_NULL) {
        ZrCore_Object_Init(cs->state, object);
    }
    return object;
}

SZrObject *extern_compiler_new_array_constant(SZrCompilerState *cs) {
    SZrObject *array;

    if (cs == ZR_NULL || cs->state == ZR_NULL) {
        return ZR_NULL;
    }

    array = ZrCore_Object_NewCustomized(cs->state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
    if (array != ZR_NULL) {
        ZrCore_Object_Init(cs->state, array);
    }
    return array;
}

TZrBool extern_compiler_match_decorator_path(SZrAstNode *decoratorNode,
                                                    const TZrChar *leafName,
                                                    TZrBool requireCall,
                                                    SZrFunctionCall **outCall) {
    SZrAstNode *expr;
    SZrPrimaryExpression *primary;
    SZrAstNode *ffiMember;
    SZrAstNode *leafMember;
    SZrAstNode *callMember = ZR_NULL;

    if (outCall != ZR_NULL) {
        *outCall = ZR_NULL;
    }
    if (decoratorNode == ZR_NULL || decoratorNode->type != ZR_AST_DECORATOR_EXPRESSION || leafName == ZR_NULL) {
        return ZR_FALSE;
    }

    expr = decoratorNode->data.decoratorExpression.expr;
    if (expr == ZR_NULL || expr->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_FALSE;
    }

    primary = &expr->data.primaryExpression;
    if (!extern_compiler_identifier_equals(primary->property, "zr") ||
        primary->members == ZR_NULL ||
        primary->members->count < (requireCall ? 3 : 2)) {
        return ZR_FALSE;
    }

    ffiMember = primary->members->nodes[0];
    leafMember = primary->members->nodes[1];
    if (ffiMember == ZR_NULL || ffiMember->type != ZR_AST_MEMBER_EXPRESSION ||
        leafMember == ZR_NULL || leafMember->type != ZR_AST_MEMBER_EXPRESSION) {
        return ZR_FALSE;
    }

    if (!extern_compiler_identifier_equals(ffiMember->data.memberExpression.property, "ffi") ||
        !extern_compiler_identifier_equals(leafMember->data.memberExpression.property, leafName)) {
        return ZR_FALSE;
    }

    if (requireCall) {
        callMember = primary->members->nodes[2];
        if (callMember == ZR_NULL || callMember->type != ZR_AST_FUNCTION_CALL) {
            return ZR_FALSE;
        }
        if (outCall != ZR_NULL) {
            *outCall = &callMember->data.functionCall;
        }
        return primary->members->count == 3;
    }

    return primary->members->count == 2;
}

SZrAstNode *extern_compiler_decorators_find_call(SZrAstNodeArray *decorators,
                                                        const TZrChar *leafName,
                                                        SZrFunctionCall **outCall) {
    if (outCall != ZR_NULL) {
        *outCall = ZR_NULL;
    }
    if (decorators == ZR_NULL || leafName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < decorators->count; index++) {
        SZrFunctionCall *call = ZR_NULL;
        SZrAstNode *decorator = decorators->nodes[index];
        if (extern_compiler_match_decorator_path(decorator, leafName, ZR_TRUE, &call)) {
            if (outCall != ZR_NULL) {
                *outCall = call;
            }
            return decorator;
        }
    }

    return ZR_NULL;
}

TZrBool extern_compiler_decorators_has_flag(SZrAstNodeArray *decorators, const TZrChar *leafName) {
    if (decorators == ZR_NULL || leafName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < decorators->count; index++) {
        if (extern_compiler_match_decorator_path(decorators->nodes[index], leafName, ZR_FALSE, ZR_NULL)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

TZrBool extern_compiler_extract_string_argument(SZrFunctionCall *call, SZrString **outString) {
    if (outString != ZR_NULL) {
        *outString = ZR_NULL;
    }
    if (call == ZR_NULL || outString == ZR_NULL || call->args == ZR_NULL || call->args->count != 1) {
        return ZR_FALSE;
    }
    if (call->args->nodes[0] == ZR_NULL || call->args->nodes[0]->type != ZR_AST_STRING_LITERAL) {
        return ZR_FALSE;
    }

    *outString = call->args->nodes[0]->data.stringLiteral.value;
    return *outString != ZR_NULL;
}

TZrBool extern_compiler_extract_int_argument(SZrFunctionCall *call, TZrInt64 *outValue) {
    if (outValue != ZR_NULL) {
        *outValue = 0;
    }
    if (call == ZR_NULL || outValue == ZR_NULL || call->args == ZR_NULL || call->args->count != 1) {
        return ZR_FALSE;
    }
    if (call->args->nodes[0] == ZR_NULL || call->args->nodes[0]->type != ZR_AST_INTEGER_LITERAL) {
        return ZR_FALSE;
    }

    *outValue = call->args->nodes[0]->data.integerLiteral.value;
    return ZR_TRUE;
}

SZrString *extern_compiler_decorators_get_string_arg(SZrAstNodeArray *decorators, const TZrChar *leafName) {
    SZrFunctionCall *call = ZR_NULL;
    SZrString *result = ZR_NULL;

    if (extern_compiler_decorators_find_call(decorators, leafName, &call) == ZR_NULL) {
        return ZR_NULL;
    }

    return extern_compiler_extract_string_argument(call, &result) ? result : ZR_NULL;
}

TZrBool extern_compiler_decorators_get_int_arg(SZrAstNodeArray *decorators,
                                                      const TZrChar *leafName,
                                                      TZrInt64 *outValue) {
    SZrFunctionCall *call = ZR_NULL;
    if (extern_compiler_decorators_find_call(decorators, leafName, &call) == ZR_NULL) {
        return ZR_FALSE;
    }

    return extern_compiler_extract_int_argument(call, outValue);
}

