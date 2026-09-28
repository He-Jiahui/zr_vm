/* An unknown dynamic source value keeps its source binding but has no
 * canonical local producer. A declared scalar is the control case. */
static SZrAstNode *compile_dynamic_object_local_fixture(
        SZrCompilerState *compiler,
        TZrNativeString path,
        const char *source) {
    SZrString *name = ZrCore_String_CreateFromNative(g_state, path);
    SZrAstNode *ast;
    TZrSize index;

    if (name == ZR_NULL) return ZR_NULL;
    ast = ZrParser_Parse(g_state, source, strlen(source), name);
    if (ast == ZR_NULL) return ZR_NULL;
    ZrParser_CompilerState_Init(compiler, g_state);
    compiler->currentAst = ast;
    compiler->scriptAst = ast;
    compiler->currentFunction = ZrCore_Function_New(g_state);
    if (compiler->currentFunction == ZR_NULL) return ast;
    for (index = 0U; index < ast->data.script.statements->count; index++) {
        ZrParser_Statement_Compile(
                compiler, ast->data.script.statements->nodes[index]);
        if (compiler->hasError) break;
    }
    return ast;
}

static void free_dynamic_object_local_fixture(SZrCompilerState *compiler,
                                              SZrAstNode *ast) {
    if (compiler->currentFunction != ZR_NULL) {
        ZrCore_Function_Free(g_state, compiler->currentFunction);
        compiler->currentFunction = ZR_NULL;
    }
    ZrParser_CompilerState_Free(compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_explicit_scalar_local_keeps_source_cfg_load(void) {
    static const char source[] =
            "var marker: int = 2;\n"
            "return marker;\n";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_dynamic_object_local_fixture(
            &compiler, "typed_marker_source_cfg.zr", source);
    const SZrTypeBinding *binding;
    const SZrSemanticIrFunction *function;
    TZrUInt32 localSlot;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    TEST_ASSERT_EQUAL_UINT32(2U, ast->data.script.statements->count);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    binding = ZrParser_TypeEnvironment_FindVariableBinding(
            compiler.typeEnv,
            ast->data.script.statements->nodes[0]
                    ->data.variableDeclaration.pattern->data.identifier.name);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, binding->typeId);
    localSlot = find_local_var(&compiler, binding->name);
    TEST_ASSERT_NOT_EQUAL(ZR_PARSER_SLOT_NONE, localSlot);
    TEST_ASSERT_NOT_NULL(compiler_semantic_ir_find_slot(&compiler, localSlot));
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(semantic_ir_has_opcode(function, ZR_SEMANTIC_IR_LOAD));
    TEST_ASSERT_TRUE(semantic_ir_has_opcode(function, ZR_SEMANTIC_IR_RETURN));

    free_dynamic_object_local_fixture(&compiler, ast);
}

static void test_unannotated_dynamic_object_local_read_keeps_legacy_cfg(void) {
    static const char source[] =
            "var marker = zr.__forcePoolFfiGc();\n"
            "return 42 + marker;\n";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_dynamic_object_local_fixture(
            &compiler, "dynamic_marker_source_cfg.zr", source);
    SZrAstNode *declaration;
    SZrAstNode *markerRead;
    const SZrTypeBinding *binding;
    const SZrSemanticReferenceFact *reference;
    const SZrSemanticIrFunction *function;
    TZrUInt32 localSlot;
    TZrBool hasLegacyLoad = ZR_FALSE;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    TEST_ASSERT_EQUAL_UINT32(2U, ast->data.script.statements->count);
    declaration = ast->data.script.statements->nodes[0];
    markerRead = ast->data.script.statements->nodes[1]
                         ->data.returnStatement.expr->data.binaryExpression.right;
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, markerRead->type);
    binding = ZrParser_TypeEnvironment_FindVariableBinding(
            compiler.typeEnv,
            declaration->data.variableDeclaration.pattern->data.identifier.name);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_TRUE(binding->hasDeclarationRange);
    TEST_ASSERT_EQUAL_INT(
            ZR_SEMANTIC_REFERENCE_ORIGIN_SOURCE_DECLARATION,
            binding->originKind);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, binding->symbolId);
    TEST_ASSERT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, binding->typeId);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, binding->type.baseType);
    TEST_ASSERT_NULL(binding->type.typeName);
    TEST_ASSERT_EQUAL_UINT32(0U, binding->type.elementTypes.length);
    localSlot = find_local_var(&compiler, binding->name);
    TEST_ASSERT_NOT_EQUAL(ZR_PARSER_SLOT_NONE, localSlot);

    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgActive);
    reference = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
            compiler.semanticContext, markerRead, ZR_SEMANTIC_REFERENCE_READ);
    TEST_ASSERT_NOT_NULL(reference);
    TEST_ASSERT_EQUAL_UINT32(binding->symbolId, reference->symbolId);
    TEST_ASSERT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, reference->typeId);
    for (index = 0U; index < compiler.instructions.length; index++) {
        const TZrInstruction *instruction =
                (const TZrInstruction *)ZrCore_Array_Get(
                        &compiler.instructions, index);
        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->instruction.operationCode ==
                    ZR_INSTRUCTION_ENUM(GET_STACK) &&
            instruction->instruction.operand.operand2[0] ==
                    (TZrInt32)localSlot) {
            hasLegacyLoad = ZR_TRUE;
        }
    }
    TEST_ASSERT_TRUE(hasLegacyLoad);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_FALSE(semantic_ir_has_opcode(function, ZR_SEMANTIC_IR_RETURN));
    TEST_ASSERT_EQUAL_UINT32(2U, function->cfg.blocks.length);
    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        TEST_ASSERT_NOT_NULL(instruction);
        TEST_ASSERT_FALSE(instruction->opcode == ZR_SEMANTIC_IR_LOAD &&
                          instruction->sourceRange.start.offset ==
                                  markerRead->location.start.offset);
    }

    free_dynamic_object_local_fixture(&compiler, ast);
}
