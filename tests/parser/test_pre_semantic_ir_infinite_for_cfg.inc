static void assert_infinite_for_cycle_cfg(
        const SZrSemanticIrFunction *function) {
    const SZrParserCfgBlock *header;
    const SZrParserCfgBlock *body;
    const SZrParserCfgBlock *step;
    const SZrParserCfgBlock *exitBlock;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(5U, function->cfg.blocks.length);
    header = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 1U);
    body = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 2U);
    step = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 3U);
    exitBlock = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 4U);
    TEST_ASSERT_NOT_NULL(header);
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_NOT_NULL(step);
    TEST_ASSERT_NOT_NULL(exitBlock);
    TEST_ASSERT_EQUAL_UINT32(2U, header->predecessorCount);
    TEST_ASSERT_EQUAL_UINT32(1U, header->successorCount);
    TEST_ASSERT_EQUAL_UINT32(
            body->id, ZrParser_Cfg_BlockEdgeAt(header, 0U)->toBlockId);
    TEST_ASSERT_EQUAL_UINT32(1U, body->predecessorCount);
    TEST_ASSERT_EQUAL_UINT32(1U, body->successorCount);
    TEST_ASSERT_EQUAL_UINT32(
            step->id, ZrParser_Cfg_BlockEdgeAt(body, 0U)->toBlockId);
    TEST_ASSERT_EQUAL_UINT32(1U, step->predecessorCount);
    TEST_ASSERT_EQUAL_UINT32(1U, step->successorCount);
    TEST_ASSERT_EQUAL_UINT32(
            header->id, ZrParser_Cfg_BlockEdgeAt(step, 0U)->toBlockId);
    TEST_ASSERT_EQUAL_UINT32(exitBlock->id, function->cfg.exitBlockId);
    TEST_ASSERT_EQUAL_UINT32(0U, exitBlock->predecessorCount);
    TEST_ASSERT_EQUAL_UINT32(0U, exitBlock->successorCount);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_TERMINATOR_EXIT, exitBlock->terminatorKind);

    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT32(5U, output.blockCount);
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_source_infinite_for_break_targets_join_cfg(void) {
    static const TZrChar source[] =
            "fn identity(value: bool): bool { return value; }\n"
            "for (;;) { break; }\n"
            "identity(true);\n";
    static TZrChar sourceName[] = "source_infinite_for_break_cfg.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_loop_exit_cfg_fixture(
            &compiler, source, sizeof(source) - 1U,
            sourceName, sizeof(sourceName) - 1U);
    const SZrSemanticIrFunction *function;
    const SZrParserCfgBlock *header;
    const SZrParserCfgBlock *body;
    const SZrParserCfgBlock *join;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    TZrSize breakTargetInstructionIndex = compiler.instructions.length;
    TZrSize forwardJumpCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(8U, function->cfg.blocks.length);
    header = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 1U);
    body = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 2U);
    join = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 3U);
    TEST_ASSERT_NOT_NULL(header);
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_NOT_NULL(join);
    TEST_ASSERT_EQUAL_UINT32(1U, header->predecessorCount);
    TEST_ASSERT_EQUAL_UINT32(1U, header->successorCount);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_EDGE_NORMAL,
            ZrParser_Cfg_BlockEdgeAt(header, 0U)->kind);
    TEST_ASSERT_EQUAL_UINT32(
            body->id, ZrParser_Cfg_BlockEdgeAt(header, 0U)->toBlockId);
    TEST_ASSERT_EQUAL_UINT32(1U, body->predecessorCount);
    TEST_ASSERT_EQUAL_UINT32(1U, body->successorCount);
    TEST_ASSERT_EQUAL_UINT32(
            join->id, ZrParser_Cfg_BlockEdgeAt(body, 0U)->toBlockId);
    TEST_ASSERT_EQUAL_UINT32(1U, join->predecessorCount);

    for (index = 0U; index < compiler.pendingJumps.length; index++) {
        const SZrPendingJump *pending =
                (const SZrPendingJump *)ZrCore_Array_Get(
                        &compiler.pendingJumps, index);
        const SZrLabel *label;
        const TZrInstruction *instruction;

        if (pending == ZR_NULL ||
            pending->instructionIndex >= compiler.instructions.length ||
            pending->labelId >= compiler.labels.length) {
            continue;
        }
        label = (const SZrLabel *)ZrCore_Array_Get(
                &compiler.labels, pending->labelId);
        instruction = (const TZrInstruction *)ZrCore_Array_Get(
                &compiler.instructions, pending->instructionIndex);
        if (label != ZR_NULL && label->isResolved &&
            instruction != ZR_NULL &&
            instruction->instruction.operationCode ==
                    ZR_INSTRUCTION_ENUM(JUMP) &&
            label->instructionIndex > pending->instructionIndex) {
            forwardJumpCount++;
            breakTargetInstructionIndex = label->instructionIndex;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(1U, forwardJumpCount);
    TEST_ASSERT_TRUE(breakTargetInstructionIndex >= 1U);
    TEST_ASSERT_EQUAL_INT(
            ZR_INSTRUCTION_ENUM(JUMP),
            ((const TZrInstruction *)ZrCore_Array_Get(
                     &compiler.instructions,
                     breakTargetInstructionIndex - 1U))
                    ->instruction.operationCode);
    TEST_ASSERT_TRUE(
            ((const TZrInstruction *)ZrCore_Array_Get(
                     &compiler.instructions,
                     breakTargetInstructionIndex - 1U))
                    ->instruction.operand.operand2[0] < 0);

    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT32(8U, output.blockCount);
    ZrCore_ExecIr_FreeFunction(&output);
    free_loop_exit_cfg_fixture(&compiler, ast);
}

static void test_source_infinite_for_break_preserves_active_call_cfg(void) {
    static const TZrChar source[] =
            "fn identity(value: bool): bool { return value; }\n"
            "identity(true);\n"
            "for (;;) { break; }\n"
            "identity(true);\n";
    static TZrChar sourceName[] = "source_infinite_for_break_active_cfg.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_loop_exit_cfg_fixture(
            &compiler, source, sizeof(source) - 1U,
            sourceName, sizeof(sourceName) - 1U);
    const SZrSemanticIrFunction *function;
    TZrSize callCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgActive);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(11U, function->cfg.blocks.length);
    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == ZR_SEMANTIC_IR_CALL_TYPED) {
            callCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(2U, callCount);
    free_loop_exit_cfg_fixture(&compiler, ast);
}

static void test_source_infinite_for_continue_closes_cycle_cfg(void) {
    static const TZrChar source[] =
            "fn identity(value: bool): bool { return value; }\n"
            "for (;;) { continue; }\n"
            "identity(true);\n";
    static TZrChar sourceName[] = "source_infinite_for_continue_cfg.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_loop_exit_cfg_fixture(
            &compiler, source, sizeof(source) - 1U,
            sourceName, sizeof(sourceName) - 1U);
    const SZrSemanticIrFunction *function;
    TZrSize continueTargetInstructionIndex = compiler.instructions.length;
    TZrSize forwardJumpCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgActive);
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgTerminated);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    assert_infinite_for_cycle_cfg(function);

    for (index = 0U; index < compiler.pendingJumps.length; index++) {
        const SZrPendingJump *pending =
                (const SZrPendingJump *)ZrCore_Array_Get(
                        &compiler.pendingJumps, index);
        const SZrLabel *label;
        const TZrInstruction *instruction;

        if (pending == ZR_NULL ||
            pending->instructionIndex >= compiler.instructions.length ||
            pending->labelId >= compiler.labels.length) {
            continue;
        }
        label = (const SZrLabel *)ZrCore_Array_Get(
                &compiler.labels, pending->labelId);
        instruction = (const TZrInstruction *)ZrCore_Array_Get(
                &compiler.instructions, pending->instructionIndex);
        if (label != ZR_NULL && label->isResolved &&
            instruction != ZR_NULL &&
            instruction->instruction.operationCode ==
                    ZR_INSTRUCTION_ENUM(JUMP) &&
            label->instructionIndex > pending->instructionIndex) {
            forwardJumpCount++;
            continueTargetInstructionIndex = label->instructionIndex;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(1U, forwardJumpCount);
    TEST_ASSERT_TRUE(
            continueTargetInstructionIndex < compiler.instructions.length);
    TEST_ASSERT_EQUAL_INT(
            ZR_INSTRUCTION_ENUM(JUMP),
            ((const TZrInstruction *)ZrCore_Array_Get(
                     &compiler.instructions,
                     continueTargetInstructionIndex))
                    ->instruction.operationCode);
    TEST_ASSERT_TRUE(
            ((const TZrInstruction *)ZrCore_Array_Get(
                     &compiler.instructions,
                     continueTargetInstructionIndex))
                    ->instruction.operand.operand2[0] < 0);
    free_loop_exit_cfg_fixture(&compiler, ast);
}

static void test_source_infinite_for_fallthrough_closes_cycle_cfg(void) {
    static const TZrChar source[] =
            "fn identity(value: bool): bool { return value; }\n"
            "var index: int = 0;\n"
            "for (index = 0; ; index = 1) { index = 2; }\n"
            "identity(true);\n";
    static TZrChar sourceName[] = "source_infinite_for_fallthrough_cfg.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_loop_exit_cfg_fixture(
            &compiler, source, sizeof(source) - 1U,
            sourceName, sizeof(sourceName) - 1U);
    const SZrSemanticIrFunction *function;
    TZrSize callCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgActive);
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgTerminated);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    assert_infinite_for_cycle_cfg(function);
    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == ZR_SEMANTIC_IR_CALL_TYPED) {
            callCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(0U, callCount);
    free_loop_exit_cfg_fixture(&compiler, ast);
}

static void test_infinite_for_suffix_variable_keeps_semantic_state_frozen(
        void) {
    static const TZrChar source[] =
            "var index: int = 0;\n"
            "for (index = 0; ; index = 1) { index = 2; }\n"
            "var unreachable: int = 3;\n";
    static TZrChar sourceName[] =
            "infinite_for_suffix_variable_semantic_freeze.zr";
    SZrCompilerState compiler;
    SZrString *name = ZrCore_String_Create(
            g_state, sourceName, sizeof(sourceName) - 1U);
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sizeof(source) - 1U, name);
    const SZrSemanticIrFunction *function;
    const SZrTypeBinding *suffixBinding;
    SZrString *suffixName;
    TZrSize semanticInstructionsBefore;
    TZrSize semanticLocalsBefore;
    TZrSize semanticValuesBefore;
    TZrSize semanticPlacesBefore;
    TZrSize semanticSlotsBefore;
    TZrSize legacyInstructionsBefore;
    TZrSize legacyLocalsBefore;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_UINT64(3U, ast->data.script.statements->count);
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.currentAst = ast;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);

    ZrParser_Statement_Compile(
            &compiler, ast->data.script.statements->nodes[0]);
    ZrParser_Statement_Compile(
            &compiler, ast->data.script.statements->nodes[1]);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgActive);
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgTerminated);
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    semanticInstructionsBefore = function->instructions.length;
    semanticLocalsBefore = function->locals.length;
    semanticValuesBefore = function->values.length;
    semanticPlacesBefore = function->places.places.length;
    semanticSlotsBefore = compiler.preSemanticIrSlots.length;
    legacyInstructionsBefore = compiler.instructions.length;
    legacyLocalsBefore = compiler.localVars.length;

    ZrParser_Statement_Compile(
            &compiler, ast->data.script.statements->nodes[2]);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT64(
            semanticInstructionsBefore, function->instructions.length);
    TEST_ASSERT_EQUAL_UINT64(semanticLocalsBefore, function->locals.length);
    TEST_ASSERT_EQUAL_UINT64(semanticValuesBefore, function->values.length);
    TEST_ASSERT_EQUAL_UINT64(
            semanticPlacesBefore, function->places.places.length);
    TEST_ASSERT_EQUAL_UINT64(
            semanticSlotsBefore, compiler.preSemanticIrSlots.length);
    TEST_ASSERT_TRUE(compiler.instructions.length > legacyInstructionsBefore);
    TEST_ASSERT_EQUAL_UINT64(
            legacyLocalsBefore + 1U, compiler.localVars.length);
    suffixName = ast->data.script.statements->nodes[2]
                         ->data.variableDeclaration.pattern
                         ->data.identifier.name;
    suffixBinding = ZrParser_TypeEnvironment_FindVariableBinding(
            compiler.typeEnv, suffixName);
    TEST_ASSERT_NOT_NULL(suffixBinding);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    assert_infinite_for_cycle_cfg(function);
    free_loop_exit_cfg_fixture(&compiler, ast);
}

static void test_infinite_for_ownership_suffix_uses_legacy_only_lowering(
        void) {
    static const TZrChar source[] =
            "resource class Value {}\n"
            "var owner = own Value();\n"
            "for (;;) { continue; }\n"
            "var shared = share(owner);\n";
    static TZrChar sourceName[] =
            "infinite_for_ownership_suffix_legacy_only.zr";
    SZrCompilerState compiler;
    SZrString *name = ZrCore_String_Create(
            g_state, sourceName, sizeof(sourceName) - 1U);
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sizeof(source) - 1U, name);
    const SZrSemanticIrFunction *function;
    const SZrTypeBinding *suffixBinding;
    SZrString *suffixName;
    TZrSize semanticInstructionsBefore;
    TZrSize semanticLocalsBefore;
    TZrSize semanticValuesBefore;
    TZrSize semanticPlacesBefore;
    TZrSize semanticLoansBefore;
    TZrSize semanticOperandsBefore;
    TZrSize semanticSlotsBefore;
    TZrSize legacyInstructionsBefore;
    TZrSize legacyLocalsBefore;
    TZrSize legacyShareCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_UINT64(4U, ast->data.script.statements->count);
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.currentAst = ast;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);

    ZrParser_Compiler_CompileClassDeclaration(
            &compiler, ast->data.script.statements->nodes[0]);
    ZrParser_Statement_Compile(
            &compiler, ast->data.script.statements->nodes[1]);
    ZrParser_Statement_Compile(
            &compiler, ast->data.script.statements->nodes[2]);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgActive);
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgTerminated);
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    semanticInstructionsBefore = function->instructions.length;
    semanticLocalsBefore = function->locals.length;
    semanticValuesBefore = function->values.length;
    semanticPlacesBefore = function->places.places.length;
    semanticLoansBefore = function->loanFacts.length;
    semanticOperandsBefore = function->valueOperands.length;
    semanticSlotsBefore = compiler.preSemanticIrSlots.length;
    legacyInstructionsBefore = compiler.instructions.length;
    legacyLocalsBefore = compiler.localVars.length;

    ZrParser_Statement_Compile(
            &compiler, ast->data.script.statements->nodes[3]);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT64(
            semanticInstructionsBefore, function->instructions.length);
    TEST_ASSERT_EQUAL_UINT64(semanticLocalsBefore, function->locals.length);
    TEST_ASSERT_EQUAL_UINT64(semanticValuesBefore, function->values.length);
    TEST_ASSERT_EQUAL_UINT64(
            semanticPlacesBefore, function->places.places.length);
    TEST_ASSERT_EQUAL_UINT64(semanticLoansBefore, function->loanFacts.length);
    TEST_ASSERT_EQUAL_UINT64(
            semanticOperandsBefore, function->valueOperands.length);
    TEST_ASSERT_EQUAL_UINT64(
            semanticSlotsBefore, compiler.preSemanticIrSlots.length);
    TEST_ASSERT_TRUE(compiler.instructions.length > legacyInstructionsBefore);
    TEST_ASSERT_EQUAL_UINT64(
            legacyLocalsBefore + 1U, compiler.localVars.length);
    for (index = legacyInstructionsBefore;
         index < compiler.instructions.length;
         index++) {
        const TZrInstruction *instruction =
                (const TZrInstruction *)ZrCore_Array_Get(
                        &compiler.instructions, index);
        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->instruction.operationCode ==
            ZR_INSTRUCTION_ENUM(OWN_SHARE)) {
            legacyShareCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(1U, legacyShareCount);
    suffixName = ast->data.script.statements->nodes[3]
                         ->data.variableDeclaration.pattern
                         ->data.identifier.name;
    suffixBinding = ZrParser_TypeEnvironment_FindVariableBinding(
            compiler.typeEnv, suffixName);
    TEST_ASSERT_NOT_NULL(suffixBinding);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    assert_infinite_for_cycle_cfg(function);
    free_loop_exit_cfg_fixture(&compiler, ast);
}

static void test_infinite_for_invalid_declaration_suffix_restores_semantic_ir(
        void) {
    static const TZrChar source[] =
            "var index: int = 0;\n"
            "for (index = 0; ; index = 1) { index = 2; }\n"
            "var invalid: int = 3;\n";
    static TZrChar sourceName[] =
            "infinite_for_invalid_declaration_suffix.zr";
    SZrCompilerState compiler;
    SZrString *name = ZrCore_String_Create(
            g_state, sourceName, sizeof(sourceName) - 1U);
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sizeof(source) - 1U, name);
    SZrAstNode *suffix;
    const SZrSemanticIrFunction *function;
    EZrAstNodeType suffixType;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_UINT64(3U, ast->data.script.statements->count);
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.currentAst = ast;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);

    ZrParser_Statement_Compile(
            &compiler, ast->data.script.statements->nodes[0]);
    ZrParser_Statement_Compile(
            &compiler, ast->data.script.statements->nodes[1]);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgTerminated);
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    assert_infinite_for_cycle_cfg(function);

    suffix = ast->data.script.statements->nodes[2];
    suffixType = suffix->type;
    suffix->type = ZR_AST_STRUCT_DECLARATION;
    ZrParser_Statement_Compile(&compiler, suffix);
    suffix->type = suffixType;

    TEST_ASSERT_TRUE(compiler.hasError);
    TEST_ASSERT_EQUAL_PTR(ast, compiler.currentAst);
    TEST_ASSERT_EQUAL_PTR(
            function, ZrParser_Compiler_PreSemanticIr(&compiler));
    assert_infinite_for_cycle_cfg(function);
    free_loop_exit_cfg_fixture(&compiler, ast);
}

static void test_infinite_for_nonterminal_continue_abandons_active_call_cfg(
        void) {
    static const TZrChar source[] =
            "fn identity(value: bool): bool { return value; }\n"
            "identity(true);\n"
            "for (;;) { continue; identity(true); }\n"
            "identity(true);\n";
    static TZrChar sourceName[] =
            "infinite_for_nonterminal_continue_active_fallback.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_loop_exit_cfg_fixture(
            &compiler, source, sizeof(source) - 1U,
            sourceName, sizeof(sourceName) - 1U);
    const SZrSemanticIrFunction *function;
    TZrSize callCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgActive);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(2U, function->cfg.blocks.length);
    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == ZR_SEMANTIC_IR_CALL_TYPED) {
            callCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(1U, callCount);
    free_loop_exit_cfg_fixture(&compiler, ast);
}
