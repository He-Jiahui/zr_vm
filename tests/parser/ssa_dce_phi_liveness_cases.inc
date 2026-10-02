static void init_fixture(SZrExecIrFunction *f, unsigned values, unsigned blocks) {
    ZrCore_ExecIr_FunctionInit(f);
    f->id = 1u; f->functionToken = 7u; f->signatureHash = 99u;
    for (unsigned i = 0u; i < values; ++i)
        CHECK(ZrCore_ExecIr_FunctionAddValue(f, ZR_VALUE_TYPE_INT64,
                ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN) == i + 1u);
    for (unsigned i = 0u; i < blocks; ++i)
        CHECK(ZrCore_ExecIr_FunctionAddBlock(f,
                i == 0u ? ZR_EXEC_IR_BLOCK_FLAG_ENTRY : 0u) == i + 1u);
    f->entryBlockId = 1u;
}

static unsigned instruction(SZrExecIrFunction *f, EZrExecIrOpcode opcode,
                            TZrExecIrValueId left, TZrExecIrValueId right,
                            TZrExecIrValueId result, unsigned layout) {
    SZrExecIrInstruction inst = {0}; TZrExecIrInstructionId id;
    TZrExecIrValueId operands[2] = {left, right};
    inst.opcode = (TZrUInt16)opcode; inst.layoutId = layout;
    inst.typeToken = ZR_VALUE_TYPE_INT64;
    inst.sourceId = f->instructionCount + 101u;
    if (left != 0u)
        CHECK(ZrCore_ExecIr_FunctionAppendOperands(f, operands, right != 0u ? 2u : 1u,
                                                  &inst.operands));
    if (result != 0u)
        CHECK(ZrCore_ExecIr_FunctionAppendResults(f, &result, 1u, &inst.results));
    CHECK(ZrCore_ExecIr_FunctionAppendInstruction(f, &inst, &id));
    return id;
}

static void finish_block(SZrExecIrFunction *f, unsigned block, unsigned start) {
    f->blocks[block - 1u].instructions.start = start;
    f->blocks[block - 1u].instructions.count = f->instructionCount - start;
    f->blocks[block - 1u].terminatorInstructionId = f->instructionCount;
}

static void connect(SZrExecIrFunction *f, unsigned block,
                    const TZrExecIrBlockId *targets, unsigned count) {
    CHECK(ZrCore_ExecIr_FunctionAppendSuccessors(f, targets, count,
                                               &f->blocks[block - 1u].successors));
    for (unsigned i = 0u; i < count; ++i) {
        SZrExecIrBlock *target = &f->blocks[targets[i] - 1u];
        TZrExecIrBlockId predecessors[4];
        unsigned oldCount = target->predecessors.count;
        CHECK(oldCount < 4u);
        for (unsigned j = 0u; j < oldCount; ++j)
            predecessors[j] = f->predecessors[target->predecessors.start + j];
        predecessors[oldCount] = block;
        CHECK(ZrCore_ExecIr_FunctionAppendPredecessors(f, predecessors, oldCount + 1u,
                                                     &target->predecessors));
    }
    f->instructions[f->blocks[block - 1u].terminatorInstructionId - 1u].successorRange =
            f->blocks[block - 1u].successors;
}

static void phi(SZrExecIrFunction *f, unsigned block, TZrExecIrValueId result,
                unsigned predecessorA, TZrExecIrValueId valueA,
                unsigned predecessorB, TZrExecIrValueId valueB) {
    SZrExecIrPhi p = {0};
    SZrExecIrPhiIncoming incoming[2] = {{predecessorA, valueA}, {predecessorB, valueB}};
    p.result = result;
    CHECK(ZrCore_ExecIr_FunctionAppendPhiIncoming(f, incoming, 2u, &p.incomings));
    CHECK(ZrCore_ExecIr_FunctionAppendPhis(f, &p, 1u, &f->blocks[block - 1u].phis));
}

static void build_diamond(SZrExecIrFunction *f, TZrBool transitive, TZrBool unusedPhi) {
    init_fixture(f, 11u, 4u);
    instruction(f, ZR_EXEC_IR_OPCODE_CONSTANT, 0u, 0u, 1u, 0u);
    instruction(f, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH, 1u, 0u, 0u, 0u);
    finish_block(f, 1u, 0u);
    unsigned start = f->instructionCount;
    instruction(f, ZR_EXEC_IR_OPCODE_CONSTANT, 0u, 0u, 2u, 1u);
    if (transitive) instruction(f, ZR_EXEC_IR_OPCODE_COPY, 2u, 0u, 7u, 0u);
    instruction(f, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u, 0u);
    finish_block(f, 2u, start);
    start = f->instructionCount;
    instruction(f, ZR_EXEC_IR_OPCODE_CONSTANT, 0u, 0u, 3u, 2u);
    if (transitive) instruction(f, ZR_EXEC_IR_OPCODE_COPY, 3u, 0u, 8u, 0u);
    instruction(f, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u, 0u);
    finish_block(f, 3u, start);
    start = f->instructionCount;
    instruction(f, ZR_EXEC_IR_OPCODE_CONSTANT, 0u, 0u, 5u, 3u);
    instruction(f, ZR_EXEC_IR_OPCODE_ADD, unusedPhi ? 5u : 4u, 5u, 6u, 0u);
    instruction(f, ZR_EXEC_IR_OPCODE_CONSTANT, 0u, 0u, 9u, 1u);
    instruction(f, ZR_EXEC_IR_OPCODE_COPY, 9u, 0u, 10u, 0u);
    instruction(f, ZR_EXEC_IR_OPCODE_ADD, 10u, 5u, 11u, 0u);
    instruction(f, ZR_EXEC_IR_OPCODE_RETURN, 6u, 0u, 0u, 0u);
    finish_block(f, 4u, start);
    TZrExecIrBlockId arms[2] = {2u, 3u}, join = 4u;
    connect(f, 1u, arms, 2u); connect(f, 2u, &join, 1u); connect(f, 3u, &join, 1u);
    phi(f, 4u, 4u, 2u, transitive ? 7u : 2u, 3u, transitive ? 8u : 3u);
}

static void build_chained_phis(SZrExecIrFunction *f) {
    init_fixture(f, 9u, 7u);
    instruction(f, ZR_EXEC_IR_OPCODE_CONSTANT, 0u, 0u, 1u, 0u);
    instruction(f, ZR_EXEC_IR_OPCODE_CONSTANT, 0u, 0u, 5u, 3u);
    instruction(f, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH, 1u, 0u, 0u, 0u);
    finish_block(f, 1u, 0u);
    for (unsigned b = 2u; b <= 3u; ++b) {
        unsigned start = f->instructionCount;
        instruction(f, ZR_EXEC_IR_OPCODE_CONSTANT, 0u, 0u, b, b - 1u);
        instruction(f, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u, 0u);
        finish_block(f, b, start);
    }
    unsigned start = f->instructionCount;
    instruction(f, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH, 1u, 0u, 0u, 0u);
    finish_block(f, 4u, start);
    start = f->instructionCount;
    instruction(f, ZR_EXEC_IR_OPCODE_COPY, 4u, 0u, 6u, 0u);
    instruction(f, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u, 0u);
    finish_block(f, 5u, start);
    start = f->instructionCount;
    instruction(f, ZR_EXEC_IR_OPCODE_ADD, 4u, 5u, 7u, 0u);
    instruction(f, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u, 0u);
    finish_block(f, 6u, start);
    start = f->instructionCount;
    instruction(f, ZR_EXEC_IR_OPCODE_COPY, 8u, 0u, 9u, 0u);
    instruction(f, ZR_EXEC_IR_OPCODE_RETURN, 9u, 0u, 0u, 0u);
    finish_block(f, 7u, start);
    TZrExecIrBlockId a[2] = {2u, 3u}, b[2] = {5u, 6u}, first = 4u, last = 7u;
    connect(f, 1u, a, 2u); connect(f, 2u, &first, 1u); connect(f, 3u, &first, 1u);
    connect(f, 4u, b, 2u); connect(f, 5u, &last, 1u); connect(f, 6u, &last, 1u);
    phi(f, 4u, 4u, 2u, 2u, 3u, 3u); phi(f, 7u, 8u, 5u, 6u, 6u, 7u);
}

static void build_loop(SZrExecIrFunction *f) {
    init_fixture(f, 6u, 4u);
    f->values[4].typeToken = ZR_VALUE_TYPE_BOOL;
    instruction(f, ZR_EXEC_IR_OPCODE_CONSTANT, 0u, 0u, 1u, 0u);
    instruction(f, ZR_EXEC_IR_OPCODE_CONSTANT, 0u, 0u, 2u, 3u);
    instruction(f, ZR_EXEC_IR_OPCODE_CONSTANT, 0u, 0u, 3u, 4u);
    instruction(f, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u, 0u);
    finish_block(f, 1u, 0u);
    unsigned start = f->instructionCount;
    unsigned compare = instruction(f, ZR_EXEC_IR_OPCODE_COMPARE, 4u, 3u, 5u, 0u);
    f->instructions[compare - 1u].typeToken = 1u;
    instruction(f, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH, 5u, 0u, 0u, 0u);
    finish_block(f, 2u, start);
    start = f->instructionCount;
    instruction(f, ZR_EXEC_IR_OPCODE_ADD, 4u, 2u, 6u, 0u);
    instruction(f, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u, 0u);
    finish_block(f, 3u, start);
    start = f->instructionCount;
    instruction(f, ZR_EXEC_IR_OPCODE_RETURN, 4u, 0u, 0u, 0u);
    finish_block(f, 4u, start);
    TZrExecIrBlockId header = 2u, arms[2] = {3u, 4u};
    connect(f, 1u, &header, 1u); connect(f, 2u, arms, 2u); connect(f, 3u, &header, 1u);
    phi(f, 2u, 4u, 1u, 1u, 3u, 6u);
}
