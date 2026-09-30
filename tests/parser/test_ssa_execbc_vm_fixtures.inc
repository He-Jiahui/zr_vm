/* Hand-built ExecIR functions for the ExecBC-to-VM integration cases. */

static TZrBool append_instruction(
        SZrExecIrFunction *function,
        TZrExecIrBlockId blockId,
        EZrExecIrOpcode opcode,
        TZrExecIrTypeToken typeToken,
        TZrUInt32 layoutId,
        TZrExecIrSourceId sourceId,
        const TZrExecIrValueId *operands,
        TZrUInt32 operandCount,
        const TZrExecIrValueId *results,
        TZrUInt32 resultCount,
        const TZrExecIrBlockId *successors,
        TZrUInt32 successorCount,
        TZrExecIrInstructionId *outInstructionId) {
    SZrExecIrInstruction instruction;
    SZrExecIrBlock *block;
    SZrExecIrRange operandRange = {0u, 0u};
    SZrExecIrRange resultRange = {0u, 0u};
    SZrExecIrRange successorRange = {0u, 0u};
    TZrExecIrInstructionId instructionId = 0u;

    if (function == ZR_NULL || blockId == 0u || blockId > function->blockCount ||
        (operandCount != 0u && operands == ZR_NULL) ||
        (resultCount != 0u && results == ZR_NULL) ||
        (successorCount != 0u && successors == ZR_NULL)) {
        return ZR_FALSE;
    }
    if (operandCount != 0u &&
        !ZrCore_ExecIr_FunctionAppendOperands(
                function, operands, operandCount, &operandRange)) {
        return ZR_FALSE;
    }
    if (resultCount != 0u &&
        !ZrCore_ExecIr_FunctionAppendResults(
                function, results, resultCount, &resultRange)) {
        return ZR_FALSE;
    }
    if (successorCount != 0u) {
        if (!ZrCore_ExecIr_FunctionAppendSuccessors(
                    function, successors, successorCount, &successorRange)) {
            return ZR_FALSE;
        }
        function->blocks[blockId - 1u].successorRange = successorRange;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (TZrUInt16)opcode;
    instruction.operands = operandRange;
    instruction.results = resultRange;
    instruction.successorRange = successorRange;
    instruction.typeToken = typeToken;
    if (opcode == ZR_EXEC_IR_OPCODE_COMPARE) {
        instruction.matchTypeToken = ZR_VALUE_TYPE_INT64;
    }
    instruction.layoutId = layoutId;
    instruction.sourceId = sourceId;
    if (!ZrCore_ExecIr_FunctionAppendInstruction(
                function, &instruction, &instructionId)) {
        return ZR_FALSE;
    }

    block = &function->blocks[blockId - 1u];
    if (block->instructionRange.count == 0u) {
        block->instructionRange.start = instructionId - 1u;
    }
    block->instructionRange.count++;
    if (opcode == ZR_EXEC_IR_OPCODE_BRANCH ||
        opcode == ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH ||
        opcode == ZR_EXEC_IR_OPCODE_RETURN) {
        block->terminatorInstructionId = instructionId;
    }
    if (outInstructionId != ZR_NULL) {
        *outInstructionId = instructionId;
    }
    return ZR_TRUE;
}

static TZrBool append_predecessors(
        SZrExecIrFunction *function,
        TZrExecIrBlockId blockId,
        const TZrExecIrBlockId *predecessors,
        TZrUInt32 predecessorCount) {
    if (function == ZR_NULL || blockId == 0u || blockId > function->blockCount ||
        (predecessorCount != 0u && predecessors == ZR_NULL)) {
        return ZR_FALSE;
    }
    if (predecessorCount == 0u) {
        function->blocks[blockId - 1u].predecessorRange =
                (SZrExecIrRange){0u, 0u};
        return ZR_TRUE;
    }
    return ZrCore_ExecIr_FunctionAppendPredecessors(
            function, predecessors, predecessorCount,
            &function->blocks[blockId - 1u].predecessorRange);
}

static TZrExecIrValueId add_value(
        SZrExecIrFunction *function,
        TZrExecIrTypeToken typeToken) {
    return ZrCore_ExecIr_FunctionAddValue(
            function, typeToken, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
}

static TZrBool initialize_function(SZrExecIrFunction *function) {
    if (function == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(function, 0, sizeof(*function));
    ZrCore_ExecIr_FunctionInit(function);
    function->id = 1u;
    function->functionToken = 0x5105u;
    function->signatureHash = 0x51050001u;
    return ZR_TRUE;
}

static TZrBool add_instruction_source_maps(SZrExecIrFunction *function) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->instructionCount == 0u) {
        return ZR_FALSE;
    }
    function->sourceMaps = (SZrExecIrSourceMap *)calloc(
            function->instructionCount, sizeof(*function->sourceMaps));
    if (function->sourceMaps == ZR_NULL) {
        return ZR_FALSE;
    }
    function->sourceMapCount = function->instructionCount;
    function->sourceMapCapacity = function->instructionCount;
    for (index = 0u; index < function->instructionCount; ++index) {
        function->sourceMaps[index].sourceId =
                function->instructions[index].sourceId;
        function->sourceMaps[index].instructionId = index + 1u;
        /* Source text and AST locations are intentionally absent in these
         * hand-built IR fixtures. Stable source IDs still test PC mapping. */
        function->sourceMaps[index].startOffset = 0u;
        function->sourceMaps[index].endOffset = 0u;
        function->sourceMaps[index].startLine = 0u;
        function->sourceMaps[index].startColumn = 0u;
        function->sourceMaps[index].endLine = 0u;
        function->sourceMaps[index].endColumn = 0u;
    }
    return ZR_TRUE;
}

static TZrBool build_add_sub_function(
        SZrExecIrFunction *function,
        TZrExecIrValueId *outReturnValue) {
    TZrExecIrBlockId entry;
    TZrExecIrValueId returnValue, left, right, sum, subtractor;
    TZrExecIrValueId operands[2];

    if (!initialize_function(function)) {
        return ZR_FALSE;
    }
    entry = ZrCore_ExecIr_FunctionAddBlock(
            function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    returnValue = add_value(function, ZR_VALUE_TYPE_INT64);
    left = add_value(function, ZR_VALUE_TYPE_INT64);
    right = add_value(function, ZR_VALUE_TYPE_INT64);
    sum = add_value(function, ZR_VALUE_TYPE_INT64);
    subtractor = add_value(function, ZR_VALUE_TYPE_INT64);
    if (entry != 1u || returnValue == 0u || left == 0u || right == 0u ||
        sum == 0u || subtractor == 0u) {
        return ZR_FALSE;
    }
    function->entryBlockId = entry;

    /* With no copied constant pool, the legacy scalar subset stores the
     * immediate literal in CONSTANT.layoutId. */
    if (!append_instruction(function, entry, ZR_EXEC_IR_OPCODE_CONSTANT,
                            ZR_VALUE_TYPE_INT64, 40u, 101u,
                            ZR_NULL, 0u, &left, 1u, ZR_NULL, 0u, ZR_NULL) ||
        !append_instruction(function, entry, ZR_EXEC_IR_OPCODE_CONSTANT,
                            ZR_VALUE_TYPE_INT64, 2u, 102u,
                            ZR_NULL, 0u, &right, 1u, ZR_NULL, 0u, ZR_NULL)) {
        return ZR_FALSE;
    }
    operands[0] = left;
    operands[1] = right;
    if (!append_instruction(function, entry, ZR_EXEC_IR_OPCODE_ADD,
                            ZR_VALUE_TYPE_INT64, 0u, 103u,
                            operands, 2u, &sum, 1u, ZR_NULL, 0u, ZR_NULL) ||
        !append_instruction(function, entry, ZR_EXEC_IR_OPCODE_CONSTANT,
                            ZR_VALUE_TYPE_INT64, 7u, 104u,
                            ZR_NULL, 0u, &subtractor, 1u,
                            ZR_NULL, 0u, ZR_NULL)) {
        return ZR_FALSE;
    }
    operands[0] = sum;
    operands[1] = subtractor;
    if (!append_instruction(function, entry, ZR_EXEC_IR_OPCODE_SUB,
                            ZR_VALUE_TYPE_INT64, 0u, 105u,
                            operands, 2u, &returnValue, 1u,
                            ZR_NULL, 0u, ZR_NULL) ||
        !append_instruction(function, entry, ZR_EXEC_IR_OPCODE_RETURN,
                            0u, 0u, 106u, &returnValue, 1u,
                            ZR_NULL, 0u, ZR_NULL, 0u, ZR_NULL) ||
        !add_instruction_source_maps(function)) {
        return ZR_FALSE;
    }
    if (outReturnValue != ZR_NULL) {
        *outReturnValue = returnValue;
    }
    return ZR_TRUE;
}

static TZrBool build_nonfirst_entry_function(
        SZrExecIrFunction *function,
        TZrExecIrValueId *outReturnValue) {
    TZrExecIrBlockId returnBlock, entryBlock;
    TZrExecIrBlockId successors[2], predecessors[2];
    TZrExecIrValueId result, left, right, condition;
    TZrExecIrValueId compareOperands[2];

    if (!initialize_function(function)) {
        return ZR_FALSE;
    }
    returnBlock = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    entryBlock = ZrCore_ExecIr_FunctionAddBlock(
            function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    result = add_value(function, ZR_VALUE_TYPE_INT64);
    left = add_value(function, ZR_VALUE_TYPE_INT64);
    right = add_value(function, ZR_VALUE_TYPE_INT64);
    condition = add_value(function, ZR_VALUE_TYPE_BOOL);
    if (returnBlock != 1u || entryBlock != 2u || result == 0u ||
        left == 0u || right == 0u || condition == 0u) {
        return ZR_FALSE;
    }
    function->entryBlockId = entryBlock;
    successors[0] = returnBlock;
    successors[1] = returnBlock;
    predecessors[0] = entryBlock;
    predecessors[1] = entryBlock;
    compareOperands[0] = left;
    compareOperands[1] = right;

    if (!append_instruction(function, entryBlock, ZR_EXEC_IR_OPCODE_CONSTANT,
                            ZR_VALUE_TYPE_INT64, 1u, 701u,
                            ZR_NULL, 0u, &left, 1u, ZR_NULL, 0u, ZR_NULL) ||
        !append_instruction(function, entryBlock, ZR_EXEC_IR_OPCODE_CONSTANT,
                            ZR_VALUE_TYPE_INT64, 0u, 702u,
                            ZR_NULL, 0u, &right, 1u, ZR_NULL, 0u, ZR_NULL) ||
        !append_instruction(function, entryBlock, ZR_EXEC_IR_OPCODE_COMPARE,
                            ZR_TEST_COMPARE_GREATER, 0u, 703u,
                            compareOperands, 2u, &condition, 1u,
                            ZR_NULL, 0u, ZR_NULL) ||
        !append_instruction(function, entryBlock, ZR_EXEC_IR_OPCODE_CONSTANT,
                            ZR_VALUE_TYPE_INT64, 42u, 704u,
                            ZR_NULL, 0u, &result, 1u, ZR_NULL, 0u, ZR_NULL) ||
        !append_instruction(function, entryBlock,
                            ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,
                            0u, 0u, 705u, &condition, 1u, ZR_NULL, 0u,
                            successors, 2u, ZR_NULL) ||
        !append_predecessors(function, returnBlock, predecessors, 2u) ||
        !append_instruction(function, returnBlock, ZR_EXEC_IR_OPCODE_RETURN,
                            0u, 0u, 706u, &result, 1u, ZR_NULL, 0u,
                            ZR_NULL, 0u, ZR_NULL) ||
        !add_instruction_source_maps(function)) {
        return ZR_FALSE;
    }
    if (outReturnValue != ZR_NULL) {
        *outReturnValue = result;
    }
    return ZR_TRUE;
}

static TZrBool build_constant_pool_function(
        SZrExecIrFunction *function,
        TZrExecIrValueId *outReturnValue) {
    TZrExecIrBlockId entry, trueBlock, falseBlock;
    TZrExecIrBlockId successors[2], predecessor;
    TZrExecIrValueId left, right, sum, condition, falseValue;
    TZrExecIrValueId addOperands[2];

    if (!initialize_function(function)) {
        return ZR_FALSE;
    }
    entry = ZrCore_ExecIr_FunctionAddBlock(
            function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    trueBlock = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    falseBlock = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    left = add_value(function, ZR_VALUE_TYPE_INT64);
    right = add_value(function, ZR_VALUE_TYPE_INT64);
    sum = add_value(function, ZR_VALUE_TYPE_INT64);
    condition = add_value(function, ZR_VALUE_TYPE_BOOL);
    falseValue = add_value(function, ZR_VALUE_TYPE_INT64);
    if (entry != 1u || trueBlock != 2u || falseBlock != 3u ||
        left == 0u || right == 0u || sum == 0u || condition == 0u ||
        falseValue == 0u) {
        return ZR_FALSE;
    }
    function->entryBlockId = entry;
    successors[0] = trueBlock;
    successors[1] = falseBlock;
    predecessor = entry;
    addOperands[0] = left;
    addOperands[1] = right;

    if (!append_instruction(function, entry, ZR_EXEC_IR_OPCODE_CONSTANT,
                            ZR_VALUE_TYPE_INT64, 0u, 801u,
                            ZR_NULL, 0u, &left, 1u, ZR_NULL, 0u, ZR_NULL) ||
        !append_instruction(function, entry, ZR_EXEC_IR_OPCODE_CONSTANT,
                            ZR_VALUE_TYPE_INT64, 1u, 802u,
                            ZR_NULL, 0u, &right, 1u, ZR_NULL, 0u, ZR_NULL) ||
        !append_instruction(function, entry, ZR_EXEC_IR_OPCODE_ADD,
                            ZR_VALUE_TYPE_INT64, 0u, 803u,
                            addOperands, 2u, &sum, 1u, ZR_NULL, 0u, ZR_NULL) ||
        !append_instruction(function, entry, ZR_EXEC_IR_OPCODE_CONSTANT,
                            ZR_VALUE_TYPE_BOOL, 2u, 804u,
                            ZR_NULL, 0u, &condition, 1u, ZR_NULL, 0u, ZR_NULL) ||
        !append_instruction(function, entry, ZR_EXEC_IR_OPCODE_CONSTANT,
                            ZR_VALUE_TYPE_INT64, 3u, 805u,
                            ZR_NULL, 0u, &falseValue, 1u, ZR_NULL, 0u, ZR_NULL) ||
        !append_instruction(function, entry,
                            ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,
                            0u, 0u, 806u, &condition, 1u, ZR_NULL, 0u,
                            successors, 2u, ZR_NULL) ||
        !append_predecessors(function, trueBlock, &predecessor, 1u) ||
        !append_predecessors(function, falseBlock, &predecessor, 1u) ||
        !append_instruction(function, trueBlock, ZR_EXEC_IR_OPCODE_RETURN,
                            0u, 0u, 807u, &sum, 1u, ZR_NULL, 0u,
                            ZR_NULL, 0u, ZR_NULL) ||
        !append_instruction(function, falseBlock, ZR_EXEC_IR_OPCODE_RETURN,
                            0u, 0u, 808u, &falseValue, 1u, ZR_NULL, 0u,
                            ZR_NULL, 0u, ZR_NULL) ||
        !add_instruction_source_maps(function)) {
        return ZR_FALSE;
    }
    if (outReturnValue != ZR_NULL) {
        *outReturnValue = sum;
    }
    return ZR_TRUE;
}

static TZrBool build_compare_phi_function(
        SZrExecIrFunction *function,
        TZrInt64 leftLiteral,
        TZrInt64 rightLiteral,
        TZrExecIrValueId *outReturnValue) {
    TZrExecIrBlockId entry, trueBlock, falseBlock, mergeBlock;
    TZrExecIrBlockId successors[2], predecessors[2];
    TZrExecIrValueId merged, left, right, condition, trueValue, falseValue;
    TZrExecIrValueId operand;
    SZrExecIrPhi phi;
    SZrExecIrPhiIncoming incoming[2];
    SZrExecIrRange incomingRange;

    if (!initialize_function(function)) {
        return ZR_FALSE;
    }
    entry = ZrCore_ExecIr_FunctionAddBlock(
            function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    trueBlock = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    falseBlock = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    mergeBlock = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    merged = add_value(function, ZR_VALUE_TYPE_INT64);
    left = add_value(function, ZR_VALUE_TYPE_INT64);
    right = add_value(function, ZR_VALUE_TYPE_INT64);
    condition = add_value(function, ZR_VALUE_TYPE_BOOL);
    trueValue = add_value(function, ZR_VALUE_TYPE_INT64);
    falseValue = add_value(function, ZR_VALUE_TYPE_INT64);
    if (entry != 1u || trueBlock != 2u || falseBlock != 3u ||
        mergeBlock != 4u || merged == 0u || left == 0u || right == 0u ||
        condition == 0u || trueValue == 0u || falseValue == 0u) {
        return ZR_FALSE;
    }
    function->entryBlockId = entry;

    if (!append_instruction(function, entry, ZR_EXEC_IR_OPCODE_CONSTANT,
                            ZR_VALUE_TYPE_INT64, (TZrUInt32)leftLiteral,
                            201u, ZR_NULL, 0u, &left, 1u,
                            ZR_NULL, 0u, ZR_NULL) ||
        !append_instruction(function, entry, ZR_EXEC_IR_OPCODE_CONSTANT,
                            ZR_VALUE_TYPE_INT64, (TZrUInt32)rightLiteral,
                            202u, ZR_NULL, 0u, &right, 1u,
                            ZR_NULL, 0u, ZR_NULL)) {
        return ZR_FALSE;
    }
    {
        TZrExecIrValueId compareOperands[2] = {left, right};
        if (!append_instruction(function, entry, ZR_EXEC_IR_OPCODE_COMPARE,
                                ZR_TEST_COMPARE_LESS, 0u, 203u,
                                compareOperands, 2u, &condition, 1u,
                                ZR_NULL, 0u, ZR_NULL)) {
            return ZR_FALSE;
        }
    }
    successors[0] = trueBlock;
    successors[1] = falseBlock;
    if (!append_instruction(function, entry,
                            ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,
                            0u, 0u, 204u, &condition, 1u,
                            ZR_NULL, 0u, successors, 2u, ZR_NULL) ||
        !append_instruction(function, trueBlock, ZR_EXEC_IR_OPCODE_CONSTANT,
                            ZR_VALUE_TYPE_INT64, 111u, 205u,
                            ZR_NULL, 0u, &trueValue, 1u,
                            ZR_NULL, 0u, ZR_NULL)) {
        return ZR_FALSE;
    }
    successors[0] = mergeBlock;
    if (!append_instruction(function, trueBlock, ZR_EXEC_IR_OPCODE_BRANCH,
                            0u, 0u, 206u, ZR_NULL, 0u, ZR_NULL, 0u,
                            successors, 1u, ZR_NULL) ||
        !append_instruction(function, falseBlock, ZR_EXEC_IR_OPCODE_CONSTANT,
                            ZR_VALUE_TYPE_INT64, 222u, 207u,
                            ZR_NULL, 0u, &falseValue, 1u,
                            ZR_NULL, 0u, ZR_NULL) ||
        !append_instruction(function, falseBlock, ZR_EXEC_IR_OPCODE_BRANCH,
                            0u, 0u, 208u, ZR_NULL, 0u, ZR_NULL, 0u,
                            successors, 1u, ZR_NULL)) {
        return ZR_FALSE;
    }
    operand = merged;
    if (!append_instruction(function, mergeBlock, ZR_EXEC_IR_OPCODE_RETURN,
                            0u, 0u, 209u, &operand, 1u,
                            ZR_NULL, 0u, ZR_NULL, 0u, ZR_NULL)) {
        return ZR_FALSE;
    }

    predecessors[0] = entry;
    if (!append_predecessors(function, trueBlock, predecessors, 1u) ||
        !append_predecessors(function, falseBlock, predecessors, 1u)) {
        return ZR_FALSE;
    }
    predecessors[0] = trueBlock;
    predecessors[1] = falseBlock;
    if (!append_predecessors(function, mergeBlock, predecessors, 2u)) {
        return ZR_FALSE;
    }

    incoming[0].predecessor = trueBlock;
    incoming[0].value = trueValue;
    incoming[1].predecessor = falseBlock;
    incoming[1].value = falseValue;
    memset(&phi, 0, sizeof(phi));
    phi.result = merged;
    if (!ZrCore_ExecIr_FunctionAppendPhiIncoming(
                function, incoming, 2u, &incomingRange)) {
        return ZR_FALSE;
    }
    phi.incomings = incomingRange;
    if (!ZrCore_ExecIr_FunctionAppendPhis(
                function, &phi, 1u,
                &function->blocks[mergeBlock - 1u].phis) ||
        !add_instruction_source_maps(function)) {
        return ZR_FALSE;
    }
    if (outReturnValue != ZR_NULL) {
        *outReturnValue = merged;
    }
    return ZR_TRUE;
}

static TZrBool build_slot_zero_phi_function(
        SZrExecIrFunction *function,
        TZrExecIrValueId *outReturnValue) {
    TZrExecIrBlockId entry, trueBlock, falseBlock, mergeBlock;
    TZrExecIrBlockId successors[2], predecessors[2];
    TZrExecIrValueId source, left, right, condition, alternate, merged;
    TZrExecIrValueId compareOperands[2];
    TZrExecIrValueId returnOperand;
    SZrExecIrPhi phi;
    SZrExecIrPhiIncoming incoming[2];
    SZrExecIrRange incomingRange;

    if (!initialize_function(function)) {
        return ZR_FALSE;
    }
    entry = ZrCore_ExecIr_FunctionAddBlock(
            function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    trueBlock = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    falseBlock = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    mergeBlock = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    source = add_value(function, ZR_VALUE_TYPE_INT64);
    left = add_value(function, ZR_VALUE_TYPE_INT64);
    right = add_value(function, ZR_VALUE_TYPE_INT64);
    condition = add_value(function, ZR_VALUE_TYPE_BOOL);
    alternate = add_value(function, ZR_VALUE_TYPE_INT64);
    merged = add_value(function, ZR_VALUE_TYPE_INT64);
    if (entry != 1u || trueBlock != 2u || falseBlock != 3u ||
        mergeBlock != 4u || source != 1u || left == 0u || right == 0u ||
        condition == 0u || alternate == 0u || merged == 0u) {
        return ZR_FALSE;
    }
    function->entryBlockId = entry;

    if (!append_instruction(function, entry, ZR_EXEC_IR_OPCODE_CONSTANT,
                            ZR_VALUE_TYPE_INT64, 42u, 501u,
                            ZR_NULL, 0u, &source, 1u, ZR_NULL, 0u, ZR_NULL) ||
        !append_instruction(function, entry, ZR_EXEC_IR_OPCODE_CONSTANT,
                            ZR_VALUE_TYPE_INT64, 1u, 502u,
                            ZR_NULL, 0u, &left, 1u, ZR_NULL, 0u, ZR_NULL) ||
        !append_instruction(function, entry, ZR_EXEC_IR_OPCODE_CONSTANT,
                            ZR_VALUE_TYPE_INT64, 2u, 503u,
                            ZR_NULL, 0u, &right, 1u, ZR_NULL, 0u, ZR_NULL)) {
        return ZR_FALSE;
    }
    compareOperands[0] = left;
    compareOperands[1] = right;
    if (!append_instruction(function, entry, ZR_EXEC_IR_OPCODE_COMPARE,
                            ZR_TEST_COMPARE_LESS, 0u, 504u,
                            compareOperands, 2u, &condition, 1u,
                            ZR_NULL, 0u, ZR_NULL)) {
        return ZR_FALSE;
    }
    successors[0] = trueBlock;
    successors[1] = falseBlock;
    if (!append_instruction(function, entry,
                            ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,
                            0u, 0u, 505u, &condition, 1u,
                            ZR_NULL, 0u, successors, 2u, ZR_NULL)) {
        return ZR_FALSE;
    }
    successors[0] = mergeBlock;
    if (!append_instruction(function, trueBlock, ZR_EXEC_IR_OPCODE_BRANCH,
                            0u, 0u, 506u, ZR_NULL, 0u, ZR_NULL, 0u,
                            successors, 1u, ZR_NULL) ||
        !append_instruction(function, falseBlock, ZR_EXEC_IR_OPCODE_CONSTANT,
                            ZR_VALUE_TYPE_INT64, 99u, 507u,
                            ZR_NULL, 0u, &alternate, 1u,
                            ZR_NULL, 0u, ZR_NULL) ||
        !append_instruction(function, falseBlock, ZR_EXEC_IR_OPCODE_BRANCH,
                            0u, 0u, 508u, ZR_NULL, 0u, ZR_NULL, 0u,
                            successors, 1u, ZR_NULL)) {
        return ZR_FALSE;
    }
    returnOperand = merged;
    if (!append_instruction(function, mergeBlock, ZR_EXEC_IR_OPCODE_RETURN,
                            0u, 0u, 509u, &returnOperand, 1u,
                            ZR_NULL, 0u, ZR_NULL, 0u, ZR_NULL)) {
        return ZR_FALSE;
    }

    predecessors[0] = entry;
    if (!append_predecessors(function, trueBlock, predecessors, 1u) ||
        !append_predecessors(function, falseBlock, predecessors, 1u)) {
        return ZR_FALSE;
    }
    predecessors[0] = trueBlock;
    predecessors[1] = falseBlock;
    if (!append_predecessors(function, mergeBlock, predecessors, 2u)) {
        return ZR_FALSE;
    }
    incoming[0].predecessor = trueBlock;
    incoming[0].value = source;
    incoming[1].predecessor = falseBlock;
    incoming[1].value = alternate;
    memset(&phi, 0, sizeof(phi));
    phi.result = merged;
    if (!ZrCore_ExecIr_FunctionAppendPhiIncoming(
                function, incoming, 2u, &incomingRange)) {
        return ZR_FALSE;
    }
    phi.incomings = incomingRange;
    if (!ZrCore_ExecIr_FunctionAppendPhis(
                function, &phi, 1u,
                &function->blocks[mergeBlock - 1u].phis) ||
        !add_instruction_source_maps(function)) {
        return ZR_FALSE;
    }
    if (outReturnValue != ZR_NULL) {
        *outReturnValue = merged;
    }
    return ZR_TRUE;
}

static TZrBool build_critical_edge_function(
        SZrExecIrFunction *function,
        TZrExecIrValueId *outReturnValue) {
    TZrExecIrBlockId entry, sideBlock, mergeBlock;
    TZrExecIrBlockId successors[2], predecessors[2];
    TZrExecIrValueId merged, left, right, condition, directValue, sideValue;
    SZrExecIrPhi phi;
    SZrExecIrPhiIncoming incoming[2];
    SZrExecIrRange incomingRange;
    TZrExecIrValueId compareOperands[2];

    if (!initialize_function(function)) {
        return ZR_FALSE;
    }
    entry = ZrCore_ExecIr_FunctionAddBlock(
            function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    sideBlock = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    mergeBlock = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    merged = add_value(function, ZR_VALUE_TYPE_INT64);
    left = add_value(function, ZR_VALUE_TYPE_INT64);
    right = add_value(function, ZR_VALUE_TYPE_INT64);
    condition = add_value(function, ZR_VALUE_TYPE_BOOL);
    directValue = add_value(function, ZR_VALUE_TYPE_INT64);
    sideValue = add_value(function, ZR_VALUE_TYPE_INT64);
    if (entry != 1u || sideBlock != 2u || mergeBlock != 3u ||
        merged == 0u || left == 0u || right == 0u || condition == 0u ||
        directValue == 0u || sideValue == 0u) {
        return ZR_FALSE;
    }
    function->entryBlockId = entry;

    if (!append_instruction(function, entry, ZR_EXEC_IR_OPCODE_CONSTANT,
                            ZR_VALUE_TYPE_INT64, 1u, 301u,
                            ZR_NULL, 0u, &left, 1u,
                            ZR_NULL, 0u, ZR_NULL) ||
        !append_instruction(function, entry, ZR_EXEC_IR_OPCODE_CONSTANT,
                            ZR_VALUE_TYPE_INT64, 2u, 302u,
                            ZR_NULL, 0u, &right, 1u,
                            ZR_NULL, 0u, ZR_NULL)) {
        return ZR_FALSE;
    }
    compareOperands[0] = left;
    compareOperands[1] = right;
    if (!append_instruction(function, entry, ZR_EXEC_IR_OPCODE_COMPARE,
                            ZR_TEST_COMPARE_LESS, 0u, 303u,
                            compareOperands, 2u, &condition, 1u,
                            ZR_NULL, 0u, ZR_NULL) ||
        !append_instruction(function, entry, ZR_EXEC_IR_OPCODE_CONSTANT,
                            ZR_VALUE_TYPE_INT64, 13u, 304u,
                            ZR_NULL, 0u, &directValue, 1u,
                            ZR_NULL, 0u, ZR_NULL)) {
        return ZR_FALSE;
    }
    successors[0] = mergeBlock;
    successors[1] = sideBlock;
    if (!append_instruction(function, entry,
                            ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,
                            0u, 0u, 305u, &condition, 1u,
                            ZR_NULL, 0u, successors, 2u, ZR_NULL) ||
        !append_instruction(function, sideBlock, ZR_EXEC_IR_OPCODE_CONSTANT,
                            ZR_VALUE_TYPE_INT64, 99u, 306u,
                            ZR_NULL, 0u, &sideValue, 1u,
                            ZR_NULL, 0u, ZR_NULL)) {
        return ZR_FALSE;
    }
    successors[0] = mergeBlock;
    if (!append_instruction(function, sideBlock, ZR_EXEC_IR_OPCODE_BRANCH,
                            0u, 0u, 307u, ZR_NULL, 0u, ZR_NULL, 0u,
                            successors, 1u, ZR_NULL) ||
        !append_instruction(function, mergeBlock, ZR_EXEC_IR_OPCODE_RETURN,
                            0u, 0u, 308u, &merged, 1u,
                            ZR_NULL, 0u, ZR_NULL, 0u, ZR_NULL)) {
        return ZR_FALSE;
    }

    predecessors[0] = entry;
    if (!append_predecessors(function, sideBlock, predecessors, 1u)) {
        return ZR_FALSE;
    }
    predecessors[0] = entry;
    predecessors[1] = sideBlock;
    if (!append_predecessors(function, mergeBlock, predecessors, 2u)) {
        return ZR_FALSE;
    }
    incoming[0].predecessor = entry;
    incoming[0].value = directValue;
    incoming[1].predecessor = sideBlock;
    incoming[1].value = sideValue;
    memset(&phi, 0, sizeof(phi));
    phi.result = merged;
    if (!ZrCore_ExecIr_FunctionAppendPhiIncoming(
                function, incoming, 2u, &incomingRange)) {
        return ZR_FALSE;
    }
    phi.incomings = incomingRange;
    if (!ZrCore_ExecIr_FunctionAppendPhis(
                function, &phi, 1u,
                &function->blocks[mergeBlock - 1u].phis) ||
        !add_instruction_source_maps(function)) {
        return ZR_FALSE;
    }
    if (outReturnValue != ZR_NULL) {
        *outReturnValue = merged;
    }
    return ZR_TRUE;
}

static TZrBool build_loop_swap_function(
        SZrExecIrFunction *function,
        TZrExecIrValueId *outReturnValue) {
    TZrExecIrBlockId entry, header, body, exitBlock;
    TZrExecIrBlockId successor, predecessors[2];
    TZrExecIrValueId firstPhi, secondPhi, countPhi, condition;
    TZrExecIrValueId initialFirst, initialSecond, initialCount, zero, one;
    TZrExecIrValueId nextCount, operands[2];
    SZrExecIrPhi phis[3];
    SZrExecIrPhiIncoming incoming[2];
    TZrUInt32 phiIndex;

    if (!initialize_function(function)) {
        return ZR_FALSE;
    }
    entry = ZrCore_ExecIr_FunctionAddBlock(
            function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    header = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    body = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    exitBlock = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    firstPhi = add_value(function, ZR_VALUE_TYPE_INT64);
    secondPhi = add_value(function, ZR_VALUE_TYPE_INT64);
    countPhi = add_value(function, ZR_VALUE_TYPE_INT64);
    condition = add_value(function, ZR_VALUE_TYPE_BOOL);
    initialFirst = add_value(function, ZR_VALUE_TYPE_INT64);
    initialSecond = add_value(function, ZR_VALUE_TYPE_INT64);
    initialCount = add_value(function, ZR_VALUE_TYPE_INT64);
    zero = add_value(function, ZR_VALUE_TYPE_INT64);
    one = add_value(function, ZR_VALUE_TYPE_INT64);
    nextCount = add_value(function, ZR_VALUE_TYPE_INT64);
    if (entry != 1u || header != 2u || body != 3u || exitBlock != 4u ||
        firstPhi == 0u || secondPhi == 0u || countPhi == 0u ||
        condition == 0u || initialFirst == 0u || initialSecond == 0u ||
        initialCount == 0u || zero == 0u || one == 0u || nextCount == 0u) {
        return ZR_FALSE;
    }
    function->entryBlockId = entry;

    if (!append_instruction(function, entry, ZR_EXEC_IR_OPCODE_CONSTANT,
                            ZR_VALUE_TYPE_INT64, 2u, 401u,
                            ZR_NULL, 0u, &initialFirst, 1u,
                            ZR_NULL, 0u, ZR_NULL) ||
        !append_instruction(function, entry, ZR_EXEC_IR_OPCODE_CONSTANT,
                            ZR_VALUE_TYPE_INT64, 3u, 402u,
                            ZR_NULL, 0u, &initialSecond, 1u,
                            ZR_NULL, 0u, ZR_NULL) ||
        !append_instruction(function, entry, ZR_EXEC_IR_OPCODE_CONSTANT,
                            ZR_VALUE_TYPE_INT64, 3u, 403u,
                            ZR_NULL, 0u, &initialCount, 1u,
                            ZR_NULL, 0u, ZR_NULL) ||
        !append_instruction(function, entry, ZR_EXEC_IR_OPCODE_CONSTANT,
                            ZR_VALUE_TYPE_INT64, 0u, 404u,
                            ZR_NULL, 0u, &zero, 1u,
                            ZR_NULL, 0u, ZR_NULL) ||
        !append_instruction(function, entry, ZR_EXEC_IR_OPCODE_CONSTANT,
                            ZR_VALUE_TYPE_INT64, 1u, 405u,
                            ZR_NULL, 0u, &one, 1u,
                            ZR_NULL, 0u, ZR_NULL)) {
        return ZR_FALSE;
    }
    successor = header;
    if (!append_instruction(function, entry, ZR_EXEC_IR_OPCODE_BRANCH,
                            0u, 0u, 406u, ZR_NULL, 0u, ZR_NULL, 0u,
                            &successor, 1u, ZR_NULL)) {
        return ZR_FALSE;
    }
    operands[0] = countPhi;
    operands[1] = zero;
    if (!append_instruction(function, header, ZR_EXEC_IR_OPCODE_COMPARE,
                            ZR_TEST_COMPARE_GREATER, 0u, 407u,
                            operands, 2u, &condition, 1u,
                            ZR_NULL, 0u, ZR_NULL)) {
        return ZR_FALSE;
    }
    {
        TZrExecIrBlockId loopSuccessors[2] = {body, exitBlock};
        if (!append_instruction(function, header,
                                ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,
                                0u, 0u, 408u, &condition, 1u,
                                ZR_NULL, 0u, loopSuccessors, 2u, ZR_NULL)) {
            return ZR_FALSE;
        }
    }
    operands[0] = countPhi;
    operands[1] = one;
    if (!append_instruction(function, body, ZR_EXEC_IR_OPCODE_SUB,
                            ZR_VALUE_TYPE_INT64, 0u, 409u,
                            operands, 2u, &nextCount, 1u,
                            ZR_NULL, 0u, ZR_NULL)) {
        return ZR_FALSE;
    }
    successor = header;
    if (!append_instruction(function, body, ZR_EXEC_IR_OPCODE_BRANCH,
                            0u, 0u, 410u, ZR_NULL, 0u, ZR_NULL, 0u,
                            &successor, 1u, ZR_NULL) ||
        !append_instruction(function, exitBlock, ZR_EXEC_IR_OPCODE_RETURN,
                            0u, 0u, 411u, &firstPhi, 1u,
                            ZR_NULL, 0u, ZR_NULL, 0u, ZR_NULL)) {
        return ZR_FALSE;
    }

    predecessors[0] = entry;
    predecessors[1] = body;
    if (!append_predecessors(function, header, predecessors, 2u)) {
        return ZR_FALSE;
    }
    predecessors[0] = header;
    if (!append_predecessors(function, body, predecessors, 1u) ||
        !append_predecessors(function, exitBlock, predecessors, 1u)) {
        return ZR_FALSE;
    }

    memset(phis, 0, sizeof(phis));
    phis[0].result = firstPhi;
    phis[1].result = secondPhi;
    phis[2].result = countPhi;
    incoming[0].predecessor = entry;
    incoming[0].value = initialFirst;
    incoming[1].predecessor = body;
    incoming[1].value = secondPhi;
    if (!ZrCore_ExecIr_FunctionAppendPhiIncoming(
                function, incoming, 2u, &phis[0].incomings)) {
        return ZR_FALSE;
    }
    incoming[0].value = initialSecond;
    incoming[1].value = firstPhi;
    if (!ZrCore_ExecIr_FunctionAppendPhiIncoming(
                function, incoming, 2u, &phis[1].incomings)) {
        return ZR_FALSE;
    }
    incoming[0].value = initialCount;
    incoming[1].value = nextCount;
    if (!ZrCore_ExecIr_FunctionAppendPhiIncoming(
                function, incoming, 2u, &phis[2].incomings) ||
        !ZrCore_ExecIr_FunctionAppendPhis(
                function, phis, 3u, &function->blocks[header - 1u].phis) ||
        !add_instruction_source_maps(function)) {
        return ZR_FALSE;
    }
    for (phiIndex = 0u; phiIndex < 3u; ++phiIndex) {
        if (phis[phiIndex].incomings.count != 2u) {
            return ZR_FALSE;
        }
    }
    if (outReturnValue != ZR_NULL) {
        *outReturnValue = firstPhi;
    }
    return ZR_TRUE;
}

static TZrBool build_call_function(SZrExecIrFunction *function) {
    TZrExecIrBlockId entry;
    TZrExecIrValueId argument, callResult;
    TZrExecIrMemoryTokenId readTokens[2] = {
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 1u),
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_NATIVE_FFI, 1u)
    };
    TZrExecIrMemoryTokenId writeTokens[2] = {
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 2u),
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_NATIVE_FFI, 2u)
    };
    SZrExecIrRange memoryIn, memoryOut;
    TZrExecIrInstructionId callInstruction;

    if (!initialize_function(function)) {
        return ZR_FALSE;
    }
    entry = ZrCore_ExecIr_FunctionAddBlock(
            function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    argument = add_value(function, ZR_VALUE_TYPE_INT64);
    callResult = add_value(function, ZR_VALUE_TYPE_INT64);
    if (entry != 1u || argument == 0u || callResult == 0u) {
        return ZR_FALSE;
    }
    function->entryBlockId = entry;
    if (!append_instruction(function, entry, ZR_EXEC_IR_OPCODE_CONSTANT,
                            ZR_VALUE_TYPE_INT64, 5u, 501u,
                            ZR_NULL, 0u, &argument, 1u,
                            ZR_NULL, 0u, ZR_NULL) ||
        !append_instruction(function, entry, ZR_EXEC_IR_OPCODE_CALL,
                            0u, 0u, 502u, &argument, 1u,
                            &callResult, 1u, ZR_NULL, 0u,
                            &callInstruction) ||
        !ZrCore_ExecIr_FunctionAppendMemoryTokens(
                function, readTokens, 2u, &memoryIn) ||
        !ZrCore_ExecIr_FunctionAppendMemoryTokens(
                function, writeTokens, 2u, &memoryOut) ||
        !append_instruction(function, entry, ZR_EXEC_IR_OPCODE_RETURN,
                            0u, 0u, 503u, &callResult, 1u,
                            ZR_NULL, 0u, ZR_NULL, 0u, ZR_NULL) ||
        !add_instruction_source_maps(function)) {
        return ZR_FALSE;
    }
    function->instructions[callInstruction - 1u].memoryIn = memoryIn;
    function->instructions[callInstruction - 1u].memoryOut = memoryOut;
    function->instructions[callInstruction - 1u].flags =
            (TZrUInt16)(ZR_EXEC_IR_FLAG_MAY_THROW |
                        ZR_EXEC_IR_FLAG_MAY_ALLOCATE);
    function->instructions[callInstruction - 1u].effectIn = 1u;
    function->instructions[callInstruction - 1u].effectOut = 2u;
    return ZR_TRUE;
}

static TZrBool build_double_function(SZrExecIrFunction *function) {
    TZrExecIrBlockId entry;
    TZrExecIrValueId result, left, right, operands[2];

    if (!initialize_function(function)) {
        return ZR_FALSE;
    }
    entry = ZrCore_ExecIr_FunctionAddBlock(
            function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    result = add_value(function, ZR_VALUE_TYPE_DOUBLE);
    left = add_value(function, ZR_VALUE_TYPE_DOUBLE);
    right = add_value(function, ZR_VALUE_TYPE_DOUBLE);
    if (entry != 1u || result == 0u || left == 0u || right == 0u) {
        return ZR_FALSE;
    }
    function->entryBlockId = entry;
    if (!append_instruction(function, entry, ZR_EXEC_IR_OPCODE_CONSTANT,
                            ZR_VALUE_TYPE_DOUBLE, 0u, 601u,
                            ZR_NULL, 0u, &left, 1u, ZR_NULL, 0u, ZR_NULL) ||
        !append_instruction(function, entry, ZR_EXEC_IR_OPCODE_CONSTANT,
                            ZR_VALUE_TYPE_DOUBLE, 1u, 602u,
                            ZR_NULL, 0u, &right, 1u, ZR_NULL, 0u, ZR_NULL)) {
        return ZR_FALSE;
    }
    operands[0] = left;
    operands[1] = right;
    if (!append_instruction(function, entry, ZR_EXEC_IR_OPCODE_ADD,
                            ZR_VALUE_TYPE_DOUBLE, 0u, 603u,
                            operands, 2u, &result, 1u,
                            ZR_NULL, 0u, ZR_NULL) ||
        !append_instruction(function, entry, ZR_EXEC_IR_OPCODE_RETURN,
                            0u, 0u, 604u, &result, 1u,
                            ZR_NULL, 0u, ZR_NULL, 0u, ZR_NULL) ||
        !add_instruction_source_maps(function)) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrUInt64 double_bits(TZrFloat64 value) {
    TZrUInt64 bits = 0u;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}
