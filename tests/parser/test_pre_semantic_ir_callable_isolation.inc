static void test_declared_callable_bodies_do_not_leak_semantic_slots(void) {
    static const TZrChar source[] =
            "var seed = 1 + 2;\n"
            "resource class Service {\n"
            "  pub const fn read(): int {\n"
            "    var value = 1;\n"
            "    value + 2;\n"
            "    return value;\n"
            "  }\n"
            "}\n"
            "fn helper(): int {\n"
            "  var value = 3;\n"
            "  value + 4;\n"
            "  return value;\n"
            "}\n";
    static TZrChar sourceName[] = "declared_callable_slot_isolation.zr";
    SZrString *name = ZrCore_String_Create(
            g_state, sourceName, sizeof(sourceName) - 1U);
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sizeof(source) - 1U, name);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *function;
    TZrSize slotsBefore;
    TZrSize placesBefore;
    TZrSize valuesBefore;
    TZrSize instructionsBefore;

    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.currentAst = ast;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    ZrParser_Compiler_PredeclareFunctionBindings(
            &compiler, ast->data.script.statements);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    ZrParser_Statement_Compile(
            &compiler, ast->data.script.statements->nodes[0]);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    slotsBefore = compiler.preSemanticIrSlots.length;
    placesBefore = function->places.places.length;
    valuesBefore = function->values.length;
    instructionsBefore = function->instructions.length;
    TEST_ASSERT_GREATER_THAN_UINT64(0U, slotsBefore);

    ZrParser_Compiler_CompileClassDeclaration(
            &compiler, ast->data.script.statements->nodes[1]);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_EQUAL_UINT64(slotsBefore, compiler.preSemanticIrSlots.length);
    TEST_ASSERT_EQUAL_UINT64(placesBefore, function->places.places.length);
    TEST_ASSERT_EQUAL_UINT64(valuesBefore, function->values.length);
    TEST_ASSERT_EQUAL_UINT64(instructionsBefore, function->instructions.length);

    ZrParser_Statement_Compile(
            &compiler, ast->data.script.statements->nodes[2]);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_EQUAL_UINT64(slotsBefore, compiler.preSemanticIrSlots.length);
    TEST_ASSERT_EQUAL_UINT64(placesBefore, function->places.places.length);
    TEST_ASSERT_EQUAL_UINT64(valuesBefore, function->values.length);
    TEST_ASSERT_EQUAL_UINT64(instructionsBefore, function->instructions.length);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}
