static void test_source_catch_local_flow_rejects_hidden_name_bindings(void) {
    static const TZrChar source[] =
            "fn identity(): int { return 7; }\n"
            "try { identity(); }\n"
            "catch (error) {\n"
            "  var observed = error;\n"
            "  observed;\n"
            "}\n";
    static TZrChar sourceName[] = "source_catch_hidden_names.zr";
    SZrString *name = ZrCore_String_Create(
            g_state, sourceName, sizeof(sourceName) - 1U);
    SZrString *catchName = ZrCore_String_CreateFromNative(g_state, "error");
    SZrString *localName = ZrCore_String_CreateFromNative(g_state, "observed");
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sizeof(source) - 1U, name);
    SZrAstNode *tryStatement;
    SZrCompilerState compiler;
    SZrInferredType returnType;
    SZrArray parameterTypes;
    SZrTypePrototypeInfo prototype;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_UINT64(2U, ast->data.script.statements->count);
    tryStatement = ast->data.script.statements->nodes[1];
    TEST_ASSERT_NOT_NULL(tryStatement);

    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.currentAst = ast;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    ZrParser_Compiler_PredeclareFunctionBindings(
            &compiler, ast->data.script.statements);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(compiler_semantic_cfg_try_catch_is_supported(
            &compiler, tryStatement));

    ZrParser_InferredType_Init(
            g_state, &returnType, ZR_VALUE_TYPE_OBJECT);
    ZrCore_Array_Construct(&parameterTypes);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterFunction(
            g_state,
            compiler.compileTimeTypeEnv,
            catchName,
            &returnType,
            &parameterTypes));
    ZrParser_InferredType_Free(g_state, &returnType);
    TEST_ASSERT_FALSE(compiler_semantic_cfg_try_catch_is_supported(
            &compiler, tryStatement));

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);

    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.currentAst = ast;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    ZrParser_Compiler_PredeclareFunctionBindings(
            &compiler, ast->data.script.statements);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(compiler_semantic_cfg_try_catch_is_supported(
            &compiler, tryStatement));

    memset(&prototype, 0, sizeof(prototype));
    prototype.name = localName;
    prototype.type = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT;
    ZrCore_Array_Push(g_state, &compiler.typePrototypes, &prototype);
    TEST_ASSERT_FALSE(compiler_semantic_cfg_try_catch_is_supported(
            &compiler, tryStatement));

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}
