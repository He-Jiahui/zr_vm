#ifndef ZR_TEST_TYPE_INFERENCE_EXTERN_VISIBILITY_CASES_H
#define ZR_TEST_TYPE_INFERENCE_EXTERN_VISIBILITY_CASES_H

static void assert_ffi_extern_wrapper_field_visibility(
        const char *accessPrefix, EZrAccessModifier access, TZrBool allowed) {
    char source[1024];
    SZrState *state = create_test_state();
    SZrCompilerState *cs = state != ZR_NULL ? create_test_compiler_state(state) : ZR_NULL;
    SZrAstNode *ast = ZR_NULL;
    SZrAstNode *declaration = ZR_NULL;
    SZrAstNode *field = ZR_NULL;
    const SZrTypePrototypeInfo *prototype;
    const SZrTypeMemberInfo *member;
    SZrInferredType result;
    TZrBool fixtureReady = ZR_FALSE;
    TZrBool realIdentity = ZR_FALSE;
    TZrBool pointerInferred = ZR_FALSE;
    TZrBool valueInferred = ZR_FALSE;
    TZrBool fieldInferred = ZR_FALSE;
    TZrBool expectedDiagnostic = ZR_FALSE;
    EZrValueType fieldType = ZR_VALUE_TYPE_NULL;

    if (cs == ZR_NULL) {
        goto cleanup;
    }
    cs->suppressErrorOutput = ZR_TRUE;
    snprintf(source, sizeof(source),
             "native extern(\"fixture\") { struct NativePoint {"
             "var x: i32; %svar y: i32; } }"
             "let ffi = import(\"zr.ffi\");"
             "var buffer = ffi.BufferHandle.allocate(8);"
             "var bytePtr = buffer.pin();"
             "var pointPtr = bytePtr.as({ kind: \"pointer\", to: NativePoint, direction: \"inout\" });"
             "var pointValue = pointPtr.read(NativePoint);"
             "pointPtr; pointValue; pointValue.y;", accessPrefix);
    ast = ZrParser_Parse(state, source, strlen(source),
                        ZrCore_String_CreateFromNative(state, "ffi_extern_visibility.zr"));
    if (ast == ZR_NULL || ast->type != ZR_AST_SCRIPT ||
        ast->data.script.statements == ZR_NULL ||
        ast->data.script.statements->count != 9U) {
        goto cleanup;
    }
    if (ast->data.script.statements->nodes[0]->type != ZR_AST_EXTERN_BLOCK ||
        ast->data.script.statements->nodes[0]->data.externBlock.declarations == ZR_NULL ||
        ast->data.script.statements->nodes[0]->data.externBlock.declarations->count != 1U) {
        goto cleanup;
    }
    declaration = ast->data.script.statements->nodes[0]->data.externBlock.declarations->nodes[0];
    if (declaration->type != ZR_AST_STRUCT_DECLARATION ||
        declaration->data.structDeclaration.members == ZR_NULL ||
        declaration->data.structDeclaration.members->count != 2U) {
        goto cleanup;
    }
    field = declaration->data.structDeclaration.members->nodes[1];
    if (field->type != ZR_AST_STRUCT_FIELD || field->data.structField.access != access) {
        goto cleanup;
    }
    cs->currentFunction = ZrCore_Function_New(state);
    if (cs->currentFunction == ZR_NULL) {
        goto cleanup;
    }
    for (TZrSize index = 0U; index < 6U; index++) {
        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[index]);
        if (cs->hasError) {
            goto cleanup;
        }
    }
    fixtureReady = ZR_TRUE;
    prototype = find_test_type_prototype(cs, "NativePoint");
    member = find_test_type_member(prototype, "y");
    realIdentity = prototype != ZR_NULL && prototype->declarationNode == declaration &&
            member != ZR_NULL && member->declarationNode == field &&
            member->memberType == ZR_AST_STRUCT_FIELD && member->accessModifier == access;

    ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
    pointerInferred = ZrParser_ExpressionType_Infer(
            cs, ast->data.script.statements->nodes[6]->data.expressionStatement.expr, &result);
    pointerInferred = pointerInferred && result.baseType == ZR_VALUE_TYPE_OBJECT &&
            result.typeName != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(result.typeName), "Ptr<NativePoint>") == 0;
    ZrParser_InferredType_Free(state, &result);

    ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
    valueInferred = ZrParser_ExpressionType_Infer(
            cs, ast->data.script.statements->nodes[7]->data.expressionStatement.expr, &result);
    valueInferred = valueInferred && result.baseType == ZR_VALUE_TYPE_OBJECT &&
            result.typeName != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(result.typeName), "NativePoint") == 0;
    ZrParser_InferredType_Free(state, &result);

    ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
    fieldInferred = ZrParser_ExpressionType_Infer(
            cs, ast->data.script.statements->nodes[8]->data.expressionStatement.expr, &result);
    fieldType = result.baseType;
    expectedDiagnostic = allowed ? !cs->hasError :
            cs->hasError && cs->errorMessage != ZR_NULL &&
            strcmp(cs->errorMessage, "Field is not accessible in the current scope") == 0;
    ZrParser_InferredType_Free(state, &result);

cleanup:
    if (cs != ZR_NULL) {
        destroy_test_compiler_state(cs);
    }
    if (ast != ZR_NULL) {
        ZrParser_Ast_Free(state, ast);
    }
    if (state != ZR_NULL) {
        destroy_test_state(state);
    }
    TEST_ASSERT_TRUE_MESSAGE(fixtureReady, "Real extern-wrapper visibility fixture must compile");
    TEST_ASSERT_TRUE_MESSAGE(realIdentity, "Extern metadata must preserve the real field AST and access");
    TEST_ASSERT_TRUE(pointerInferred);
    TEST_ASSERT_TRUE(valueInferred);
    TEST_ASSERT_EQUAL_INT(allowed, fieldInferred);
    TEST_ASSERT_TRUE_MESSAGE(expectedDiagnostic, "Field access must follow the normal visibility diagnostic");
    if (allowed) {
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT32, fieldType);
    }
}

static void test_ffi_extern_wrapper_default_private_field_is_rejected(void) {
    assert_ffi_extern_wrapper_field_visibility("", ZR_ACCESS_PRIVATE, ZR_FALSE);
}

static void test_ffi_extern_wrapper_explicit_private_field_is_rejected(void) {
    assert_ffi_extern_wrapper_field_visibility("pri ", ZR_ACCESS_PRIVATE, ZR_FALSE);
}

static void test_ffi_extern_wrapper_public_field_keeps_provider_types(void) {
    assert_ffi_extern_wrapper_field_visibility("pub ", ZR_ACCESS_PUBLIC, ZR_TRUE);
}

#endif
