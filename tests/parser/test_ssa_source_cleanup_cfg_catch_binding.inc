static void test_top_level_catch_if_reads_payload_without_using(void) {
    static const TZrChar source[] =
            "var caught: int = 0;\n"
            "try { throw \"original\"; } catch (error) {\n"
            "  if (error.exception == \"original\") { caught = 10; }\n"
            "}\n"
            "return caught;\n";
    static TZrChar sourceName[] = "top_level_catch_if_no_using.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(
            &compiler, source, sizeof(source) - 1U, sourceName);

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));

    free_source(&compiler, ast);
}

static void test_class_member_parameter_does_not_escape_type_scope(void) {
    static const TZrChar source[] =
            "class CloseProbe {\n"
            "  pub @close(error) { throw \"replacement\"; }\n"
            "}\n";
    static TZrChar sourceName[] = "class_member_parameter_scope.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(
            &compiler, source, sizeof(source) - 1U, sourceName);
    SZrString *name;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    name = ZrCore_String_CreateFromNative(g_state, "error");
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_NULL(ZrParser_TypeEnvironment_FindVariableBinding(
            compiler.typeEnv, name));

    free_source(&compiler, ast);
}

static void test_function_parameter_does_not_escape_type_scope(void) {
    static const TZrChar source[] =
            "fn discard(error) { return 0; }\n";
    static TZrChar sourceName[] = "function_parameter_scope.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(
            &compiler, source, sizeof(source) - 1U, sourceName);
    SZrString *name;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    name = ZrCore_String_CreateFromNative(g_state, "error");
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_NULL(ZrParser_TypeEnvironment_FindVariableBinding(
            compiler.typeEnv, name));

    free_source(&compiler, ast);
}

static void test_class_member_parameter_scope_restores_after_error(void) {
    static const TZrChar source[] =
            "class CloseProbe {\n"
            "  pub @close(error) { break; }\n"
            "}\n";
    static TZrChar sourceName[] = "class_member_parameter_error_scope.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(
            &compiler, source, sizeof(source) - 1U, sourceName);
    SZrString *name;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_TRUE(compiler.hasError);
    TEST_ASSERT_NOT_NULL(compiler.typeEnv);
    name = ZrCore_String_CreateFromNative(g_state, "error");
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_NULL(ZrParser_TypeEnvironment_FindVariableBinding(
            compiler.typeEnv, name));

    free_source(&compiler, ast);
}

static void test_function_parameter_scope_restores_after_error(void) {
    static const TZrChar source[] =
            "fn broken(error) { break; }\n";
    static TZrChar sourceName[] = "function_parameter_error_scope.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(
            &compiler, source, sizeof(source) - 1U, sourceName);
    SZrString *name;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_TRUE(compiler.hasError);
    TEST_ASSERT_NOT_NULL(compiler.typeEnv);
    name = ZrCore_String_CreateFromNative(g_state, "error");
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_NULL(ZrParser_TypeEnvironment_FindVariableBinding(
            compiler.typeEnv, name));

    free_source(&compiler, ast);
}

static void test_function_parameter_preserves_outer_binding(void) {
    static const TZrChar source[] =
            "var error: int = 5;\n"
            "fn discard(error) { return 0; }\n"
            "return error;\n";
    static TZrChar sourceName[] = "function_parameter_outer_binding.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(
            &compiler, source, sizeof(source) - 1U, sourceName);
    SZrString *name;
    const SZrTypeBinding *binding;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    name = ZrCore_String_CreateFromNative(g_state, "error");
    TEST_ASSERT_NOT_NULL(name);
    binding = ZrParser_TypeEnvironment_FindVariableBinding(
            compiler.typeEnv, name);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_TRUE(binding->hasDeclarationRange);
    TEST_ASSERT_EQUAL_INT(1, binding->declarationRange.start.line);

    free_source(&compiler, ast);
}

static void test_function_parameter_scope_restores_after_foreach_error(void) {
    static const TZrChar source[] =
            "fn broken(error) {\n"
            "  var values = [true];\n"
            "  for (var item in values) { break 1; }\n"
            "}\n";
    static TZrChar sourceName[] = "function_parameter_foreach_error_scope.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(
            &compiler, source, sizeof(source) - 1U, sourceName);
    SZrString *name;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_TRUE(compiler.hasError);
    TEST_ASSERT_NOT_NULL(compiler.typeEnv);
    name = ZrCore_String_CreateFromNative(g_state, "error");
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_NULL(ZrParser_TypeEnvironment_FindVariableBinding(
            compiler.typeEnv, name));
    TEST_ASSERT_EQUAL_size_t(0U, compiler.typeEnvStack.length);

    free_source(&compiler, ast);
}

static void test_top_level_catch_if_shadows_prior_close_parameter(void) {
    static const TZrChar source[] =
            "class CloseProbe {\n"
            "  pub @constructor() { }\n"
            "  pub @close(error) { throw \"replacement\"; }\n"
            "}\n"
            "var caught: int = 0;\n"
            "try { throw \"original\"; } catch (error) {\n"
            "  if (error.exception == \"original\") { caught = 10; }\n"
            "}\n"
            "return caught;\n";
    static TZrChar sourceName[] = "top_level_catch_shadows_close_parameter.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(
            &compiler, source, sizeof(source) - 1U, sourceName);

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));

    free_source(&compiler, ast);
}

static void test_top_level_using_catch_with_distinct_name_compiles(void) {
    static const TZrChar source[] =
            "class CloseProbe {\n"
            "  pub @constructor() { }\n"
            "  pub @close(error) { throw \"replacement\"; }\n"
            "}\n"
            "var caught: int = 0;\n"
            "try { using (new CloseProbe()) { throw \"original\"; } }\n"
            "catch (caughtError) {\n"
            "  if (caughtError.exception == \"replacement\") { caught = 10; }\n"
            "}\n"
            "return caught;\n";
    static TZrChar sourceName[] = "top_level_using_catch_distinct_name.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(
            &compiler, source, sizeof(source) - 1U, sourceName);

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));

    free_source(&compiler, ast);
}

static void test_top_level_using_throw_catch_if_reads_payload(void) {
    static const TZrChar source[] =
            "class CloseProbe {\n"
            "  pub @constructor() { }\n"
            "  pub @close(error) { throw \"replacement\"; }\n"
            "}\n"
            "var caught: int = 0;\n"
            "try {\n"
            "  using (new CloseProbe()) { throw \"original\"; }\n"
            "} catch (error) {\n"
            "  if (error.exception == \"replacement\") { caught = 10; }\n"
            "  else { caught = 20; }\n"
            "}\n"
            "return caught;\n";
    static TZrChar sourceName[] = "top_level_using_throw_catch_if.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(
            &compiler, source, sizeof(source) - 1U, sourceName);
    const SZrSemanticIrFunction *function;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    /* RED: catch-if's `error` read currently fails Semantic IR load lowering. */
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgActive);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        TEST_ASSERT_NOT_NULL(instruction);
        TEST_ASSERT_FALSE(instruction->opcode == ZR_SEMANTIC_IR_LOAD &&
                          instruction->sourceRange.start.line == 9U);
    }

    free_source(&compiler, ast);
}
