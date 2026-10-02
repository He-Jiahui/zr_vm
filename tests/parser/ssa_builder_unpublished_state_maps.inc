/* Unpublished source functions have no symbol token. Their map storage must
 * still be checked before the module assigns its final identity. */
static void make_unpublished_cleanup_function(
        SZrSemanticIrFunction *semantic, SZrParserCfgBlock *block,
        SZrSemanticIrInstruction instructions[2], SZrSemanticIrValue *value,
        TZrValueId *operand) {
    make_semantic_function(semantic, block, 1u);
    semantic->symbolId = 0u;
    memset(instructions, 0, sizeof(*instructions) * 2u);
    memset(value, 0, sizeof(*value));
    value->id = 1u;
    value->typeId = 1u;
    *operand = 1u;
    instructions[0].id = 1u;
    instructions[0].opcode = ZR_SEMANTIC_IR_DROP;
    instructions[0].operandCount = 1u;
    instructions[1].id = 2u;
    instructions[1].opcode = ZR_SEMANTIC_IR_RETURN;
    semantic->instructions = input_array(instructions, 2u, sizeof(*instructions));
    semantic->values = input_array(value, 1u, sizeof(*value));
    semantic->valueOperands = input_array(operand, 1u, sizeof(*operand));
    block->instructionCount = 2u;
}

static void test_builder_keeps_unpublished_state_map_identity(void) {
    SZrSemanticIrFunction semantic;
    SZrParserCfgBlock block;
    SZrSemanticIrInstruction instructions[2];
    SZrSemanticIrValue value;
    TZrValueId operand;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_unpublished_cleanup_function(
            &semantic, &block, instructions, &value, &operand);
    ZrCore_ExecIr_FunctionInit(&output);
    if (!ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic)) {
        fprintf(stderr, "unpublished map diagnostic code=%u token=%u instruction=%u\n",
                (unsigned)diagnostic.code, (unsigned)diagnostic.functionToken,
                (unsigned)diagnostic.instructionId);
        check(ZR_FALSE, "builder rejected a zero-symbol source cleanup function");
    }
    check(output.id == 0u && output.functionToken == 0u &&
              output.stateMap != NULL && output.stateMap->functionToken == 0u &&
              output.stateMap->signatureHash == output.signatureHash &&
              output.stateMap->generation == output.contract.generation &&
              output.stateMap->entryCount == 3u &&
              output.stateMap->entries[0].instructionId == 1u &&
              output.stateMap->entries[0].phase == ZR_EXEC_IR_STATE_BEFORE_EFFECT &&
              output.stateMap->entries[1].phase == ZR_EXEC_IR_STATE_AFTER_EFFECT &&
              output.stateMap->entries[2].phase == ZR_EXEC_IR_STATE_CLEANUP_COMPLETE,
          "candidate verification changed identity or lost cleanup checkpoints");
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_module_builder_publishes_zero_symbol_state_maps(void) {
    SZrSemanticIrFunction semantic;
    SZrParserCfgBlock block;
    SZrSemanticIrInstruction instructions[2];
    SZrSemanticIrValue value;
    TZrValueId operand;
    SZrExecIrBuildInput input = {0};
    SZrExecIrModule module;
    SZrExecIrFunction *function;
    SZrExecIrDiagnostic diagnostic;
    TZrUInt32 sourceId;

    make_unpublished_cleanup_function(
            &semantic, &block, instructions, &value, &operand);
    input.semanticFunction = &semantic;
    input.functionToken = 77u;
    input.signatureHash = 99u;
    ZrCore_ExecIr_ModuleInit(&module);
    check(ZrParser_ExecIr_BuildModule(&input, &module, &diagnostic),
          "module builder rejected zero-symbol source state maps");
    function = ZrCore_ExecIr_ModuleFunctionAt(&module, 1u);
    check(function != NULL && function->stateMap != NULL &&
              function->stateMap->entryCount == 3u &&
              function->id == 1u && function->functionToken == 77u &&
              function->stateMap->functionToken == 77u &&
              function->stateMap->signatureHash == 99u &&
              function->stateMap->generation == 1u &&
              semantic.symbolId == 0u &&
              ZrCore_ExecIr_VerifyFunction(function, ZR_EXEC_IR_VERIFY_ALL,
                                          &diagnostic),
          "published cleanup map does not match the final module identity");
    sourceId = function->stateMap->entries[0].sourceId;
    function->stateMap->entries[0].sourceId = sourceId + 1u;
    check(!ZrCore_ExecIr_VerifyFunction(function, ZR_EXEC_IR_VERIFY_ALL,
                                       &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID &&
              diagnostic.instructionId == 1u,
          "strict verification accepted a malformed cleanup checkpoint");
    function->stateMap->entries[0].sourceId = sourceId;
    check(ZrCore_ExecIr_VerifyFunction(function, ZR_EXEC_IR_VERIFY_ALL,
                                      &diagnostic),
          "checkpoint rejection changed the restored state map");
    ZrCore_ExecIr_FreeModule(&module);

    make_semantic_function(&semantic, &block, 1u);
    semantic.symbolId = 0u;
    input.functionToken = 78u;
    ZrCore_ExecIr_ModuleInit(&module);
    check(ZrParser_ExecIr_BuildModule(&input, &module, &diagnostic),
          "module builder rejected a zero-symbol source with an empty map");
    function = ZrCore_ExecIr_ModuleFunctionAt(&module, 1u);
    check(function != NULL && function->stateMap != NULL &&
              function->stateMap->entryCount == 0u &&
              function->stateMap->functionToken == 78u &&
              function->stateMap->signatureHash == input.signatureHash &&
              function->stateMap->generation == function->contract.generation &&
              semantic.symbolId == 0u &&
              ZrCore_ExecIr_VerifyFunction(function, ZR_EXEC_IR_VERIFY_ALL,
                                          &diagnostic),
          "empty source map did not retain its published identity");
    ZrCore_ExecIr_FreeModule(&module);
}
