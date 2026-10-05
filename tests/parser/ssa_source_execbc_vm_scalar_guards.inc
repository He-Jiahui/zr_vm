/* Compile real source through the current compiler only. Rejected source
 * CFGs must preserve ordinary compilation; no Oracle, projection or VM runs. */
static void assert_source_scalar_cfg_guard(const TZrChar *source) {
    SZrCompilerState currentCompiler;
    SZrAstNode *ast;
    SZrString *name = ZrCore_String_CreateFromNative(g_state, "ssa_scalar_guard.zr");
    TZrBool compiled = ZR_FALSE;
    TZrBool validated = ZR_FALSE;
    TZrBool sourceCfgPublished = ZR_FALSE;
    TZrBool initialized = ZR_FALSE;
    TZrUInt32 index;
    memset(&currentCompiler, 0, sizeof(currentCompiler));
    ast = name == ZR_NULL ? ZR_NULL :
            ZrParser_Parse(g_state, source, strlen(source), name);
    if (ast != ZR_NULL) {
        ZrParser_CompilerState_Init(&currentCompiler, g_state);
        initialized = ZR_TRUE;
        currentCompiler.currentAst = ast;
        currentCompiler.currentFunction = ZrCore_Function_New(g_state);
        if (currentCompiler.currentFunction != ZR_NULL) {
            for (index = 0u; index < ast->data.script.statements->count; ++index) {
                ZrParser_Statement_Compile(&currentCompiler,
                        ast->data.script.statements->nodes[index]);
                if (currentCompiler.hasError != ZR_FALSE) break;
            }
            compiled = (TZrBool)(currentCompiler.hasError == ZR_FALSE);
            if (compiled != ZR_FALSE) {
                validated = ZrParser_Compiler_ValidatePreSemanticIr(&currentCompiler);
                sourceCfgPublished = currentCompiler.preSemanticIrCfgActive;
            } else {
                (void)printf("scalar source guard compiler error: %s\n",
                        currentCompiler.errorMessage);
            }
        }
    }
    if (initialized != ZR_FALSE) {
        if (currentCompiler.currentFunction != ZR_NULL) {
            ZrCore_Function_Free(g_state, currentCompiler.currentFunction);
            currentCompiler.currentFunction = ZR_NULL;
        }
        ZrParser_CompilerState_Free(&currentCompiler);
    }
    if (ast != ZR_NULL) ZrParser_Ast_Free(g_state, ast);
    TEST_ASSERT_TRUE_MESSAGE(compiled,
            "source scalar guard must preserve legal ordinary parse/compilation");
    TEST_ASSERT_TRUE_MESSAGE(validated,
            "unsupported source lowering must preserve ordinary SemIR validation");
    TEST_ASSERT_FALSE_MESSAGE(sourceCfgPublished,
            "unsupported scalar source must not publish an executable source CFG");
}

static void test_source_float_local_less_remains_cfg_unsupported(void) {
    assert_source_scalar_cfg_guard(
            "var value:float=1.0; if(value<2.0){return 9;} return 8;\n");
}

static void test_source_unsigned_local_less_remains_cfg_unsupported(void) {
    assert_source_scalar_cfg_guard(
            "var value:uint=1; var limit:uint=2; if(value<limit){return 9;} return 8;\n");
}

static void test_source_mixed_width_local_less_remains_cfg_unsupported(void) {
    assert_source_scalar_cfg_guard(
            "var narrow:i32=1; var wide:int=2; if(narrow<wide){return 9;} return 8;\n");
}

static void test_source_while_multiply_update_remains_cfg_unsupported(void) {
    assert_source_scalar_cfg_guard(
            "var i:int=1; while(i<3){i=i*2;} return i;\n");
}

static void test_source_while_divide_update_remains_cfg_unsupported(void) {
    assert_source_scalar_cfg_guard(
            "var i:int=3; while(i>1){i=i/2;} return i;\n");
}

static void test_source_while_compound_update_remains_cfg_unsupported(void) {
    assert_source_scalar_cfg_guard(
            "var i:int=0; while(i<3){i+=1;} return i;\n");
}

static void test_source_while_index_assignment_remains_cfg_unsupported(void) {
    assert_source_scalar_cfg_guard(
            "var values=[0]; var i:int=0; while(i<1){values[0]=i; i=i+1;} return i;\n");
}
