static void test_invoke_assignment_finally_reads_preinvoke_value(void) {
    static const TZrChar source[] =
            "fn identity(): int { return 7; }\n"
            "var result: int = 1;\n"
            "try { result = identity(); } finally { result; }\n"
            "result;\n";
    static TZrChar sourceName[] = "invoke_assignment_try_finally.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(
            &compiler, source, sizeof(source) - 1U, sourceName);
    const SZrSemanticIrFunction *function;
    const SZrSemanticIrInstruction *initialization = ZR_NULL;
    const SZrSemanticIrInstruction *call = ZR_NULL;
    const SZrSemanticIrInstruction *assignment = ZR_NULL;
    const SZrSemanticIrInstruction *cleanupRead = ZR_NULL;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    TZrUInt32 readCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(find_block_kind(
            function, ZR_PARSER_CFG_BLOCK_CLEANUP));
    for (index = 0U; index < function->instructions.length; ++index) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->sourceRange.start.line == 2U &&
            instruction->opcode == ZR_SEMANTIC_IR_INITIALIZE) {
            initialization = instruction;
        } else if (instruction->sourceRange.start.line == 3U &&
                   instruction->opcode == ZR_SEMANTIC_IR_CALL_TYPED) {
            call = instruction;
        }
    }
    TEST_ASSERT_NOT_NULL(initialization);
    TEST_ASSERT_NOT_NULL(call);
    for (index = 0U; index < function->instructions.length; ++index) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        if (instruction->sourceRange.start.line != 3U ||
            instruction->placeId != initialization->placeId) continue;
        if (instruction->opcode == ZR_SEMANTIC_IR_STORE) {
            assignment = instruction;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_LOAD) {
            cleanupRead = instruction;
        }
    }
    TEST_ASSERT_NOT_NULL(assignment);
    TEST_ASSERT_NOT_NULL(cleanupRead);
    TEST_ASSERT_EQUAL_UINT32(initialization->placeId, cleanupRead->placeId);
    TEST_ASSERT_EQUAL_UINT32(assignment->placeId, cleanupRead->placeId);
    TEST_ASSERT_EQUAL_UINT32(call->resultValueId, assignment->valueId);

    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TZrBool built = ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic);
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    TEST_ASSERT_TRUE(built);
    for (index = 0U; index < output.instructionCount; ++index) {
        const SZrExecIrInstruction *instruction = &output.instructions[index];
        if (instruction->sourceId != cleanupRead->id) continue;
        TEST_ASSERT_EQUAL_INT(ZR_EXEC_IR_OPCODE_COPY, instruction->opcode);
        TEST_ASSERT_EQUAL_UINT32(1U, instruction->operands.count);
        for (TZrUInt32 phiIndex = 0U; phiIndex < output.phiCount; ++phiIndex) {
            const SZrExecIrPhi *phi = &output.phiPool[phiIndex];
            TZrUInt32 oldEdges = 0U;
            TZrUInt32 normalEdges = 0U;
            if (phi->result != output.operands[instruction->operands.start])
                continue;
            TEST_ASSERT_EQUAL_UINT32(2U, phi->incomings.count);
            for (TZrUInt32 edge = 0U; edge < phi->incomings.count; ++edge) {
                TZrUInt32 value =
                        output.phiIncoming[phi->incomings.start + edge].value;
                oldEdges += (value == initialization->valueId);
                normalEdges += (value == call->resultValueId);
            }
            TEST_ASSERT_EQUAL_UINT32(1U, oldEdges);
            TEST_ASSERT_EQUAL_UINT32(1U, normalEdges);
            ++readCount;
        }
    }
    TEST_ASSERT_EQUAL_UINT32(1U, readCount);
    ZrCore_ExecIr_FreeFunction(&output);
    free_source(&compiler, ast);
}

static void test_compound_invoke_assignment_finally_keeps_legacy_cfg(void) {
    static const TZrChar source[] =
            "fn identity(): int { return 7; }\n"
            "var result: int = 1;\n"
            "try { result += identity(); } finally { result; }\n";
    static TZrChar sourceName[] = "compound_invoke_assignment_finally.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(
            &compiler, source, sizeof(source) - 1U, sourceName);
    const SZrSemanticIrFunction *function;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NULL(find_block_kind(
            function, ZR_PARSER_CFG_BLOCK_CLEANUP));
    free_source(&compiler, ast);
}
