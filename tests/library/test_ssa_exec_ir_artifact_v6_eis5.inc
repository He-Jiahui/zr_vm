/* EIS5 test contract: a variable-length, little-endian scalar CFG payload. */
#define TEST_EIS5_HEADER_SIZE 44u
#define TEST_EIS5_FIVE_BLOCK_ENCODED_SIZE 1260u
#define TEST_EIS5_FIRST_CONSTANT_BITS_OFFSET 220u
#define TEST_EIS5_FIRST_VALUE_RECORD_OFFSET 260u
#define TEST_EIS5_FIRST_VALUE_TYPE_TOKEN_OFFSET 268u
#define TEST_EIS5_TOTAL_LENGTH_OFFSET 8u
#define TEST_EIS5_CONSTANT_COUNT_OFFSET 12u
#define TEST_EIS5_VALUE_COUNT_OFFSET 16u
#define TEST_EIS5_BLOCK_COUNT_OFFSET 20u
#define TEST_EIS5_INSTRUCTION_COUNT_OFFSET 24u
#define TEST_EIS5_OPERAND_COUNT_OFFSET 28u
#define TEST_EIS5_RESULT_COUNT_OFFSET 32u
#define TEST_EIS5_SUCCESSOR_COUNT_OFFSET 36u
#define TEST_EIS5_PREDECESSOR_COUNT_OFFSET 40u
#define TEST_EIS5_FIRST_SUCCESSOR_ID_OFFSET 1228u
#define TEST_EIS5_COUNT_COLLISION_ENCODED_SIZE 996u
#define TEST_EIS5_MAX_BLOCK_COUNT 256u
#define TEST_EIS5_MAX_SCALAR_NODE_COUNT 4096u
#define TEST_EIS5_MAX_POOL_COUNT 16384u
#define TEST_EIS5_MAX_ENCODED_SIZE (16u * 1024u * 1024u)

static void eis5_bool_predicate_write_phase(const char *basePath);
static void eis5_bool_predicate_read_phase(const char *basePath);
static void eis5_compare_write_phase(const char *basePath);
static void eis5_compare_read_phase(const char *basePath);

static TZrUInt32 eis5_get32(const TZrByte *bytes) {
    return (TZrUInt32)bytes[0] |
           ((TZrUInt32)bytes[1] << 8u) |
           ((TZrUInt32)bytes[2] << 16u) |
           ((TZrUInt32)bytes[3] << 24u);
}

static char *eis5_artifact_path(const char *basePath, TZrBool condition) {
    TZrSize capacity = strlen(basePath) + 16u;
    char *path = (char *)malloc(capacity);
    require_true(path != ZR_NULL, "allocate EIS5 artifact path");
    snprintf(path, capacity, "%s.eis5.%s", basePath,
             condition ? "true" : "false");
    return path;
}

static char *eis5_collision_artifact_path(const char *basePath,
                                         TZrBool condition) {
    TZrSize capacity = strlen(basePath) + 32u;
    char *path = (char *)malloc(capacity);
    require_true(path != ZR_NULL, "allocate EIS5 collision artifact path");
    snprintf(path, capacity, "%s.eis5-collision.%s", basePath,
             condition ? "true" : "false");
    return path;
}

static void eis5_append_instruction(SZrExecIrFunction *function,
                                    EZrExecIrOpcode opcode,
                                    SZrExecIrRange results,
                                    SZrExecIrRange operands,
                                    SZrExecIrRange successors,
                                    TZrUInt32 constantIndex,
                                    TZrExecIrInstructionId expectedId) {
    SZrExecIrInstruction instruction;
    TZrExecIrInstructionId actualId = 0u;
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (TZrUInt16)opcode;
    instruction.results = results;
    instruction.operands = operands;
    instruction.successorRange = successors;
    instruction.layoutId = constantIndex;
    instruction.sourceId = 500u + expectedId;
    require_true(ZrCore_ExecIr_FunctionAppendInstruction(
                 function, &instruction, &actualId) &&
                 actualId == expectedId, "append EIS5 CFG instruction");
}

static void eis5_build_graph(SZrExecIrModule *module, TZrBool condition,
                             TZrBool boolPredicate) {
    SZrExecIrConstant constants[3] = {
        {boolPredicate ? ZR_VALUE_TYPE_BOOL : ZR_VALUE_TYPE_INT64,
         0u, condition ? 1u : 0u},
        {ZR_VALUE_TYPE_INT64, 0u, 42u},
        {ZR_VALUE_TYPE_INT64, 0u, 7u}
    };
    TZrExecIrBlockId successors[4] = {2u, 4u, 3u, 5u};
    TZrExecIrBlockId predecessors[4] = {1u, 2u, 1u, 4u};
    TZrExecIrValueId values[3];
    TZrExecIrFunctionId functionId = 0u;
    SZrExecIrFunction *function;
    SZrExecIrRange constantsRange = {.start = 0u, .count = 0u};
    SZrExecIrRange allSuccessors = {.start = 0u, .count = 0u};
    SZrExecIrRange allPredecessors = {.start = 0u, .count = 0u};
    SZrExecIrRange conditionResult = {.start = 0u, .count = 0u};
    SZrExecIrRange trueResult = {.start = 0u, .count = 0u};
    SZrExecIrRange falseResult = {.start = 0u, .count = 0u};
    SZrExecIrRange conditionOperand = {.start = 0u, .count = 0u};
    SZrExecIrRange trueReturnOperand = {.start = 0u, .count = 0u};
    SZrExecIrRange falseReturnOperand = {.start = 0u, .count = 0u};
    SZrExecIrRange empty = {.start = 0u, .count = 0u};
    SZrExecIrDiagnostic diagnostic;

    ZrCore_ExecIr_ModuleInit(module);
    module->id = 1u;
    module->moduleToken = TEST_MODULE;
    module->moduleHash = TEST_MODULE_HASH;
    module->contract.schemaVersion = ZR_EXECUTION_CONTRACT_SCHEMA_VERSION;
    module->contract.abiVersion = ZR_EXECUTION_CONTRACT_ABI_VERSION;
    module->contract.logicalVersion = ZR_EXECUTION_CONTRACT_LOGICAL_VERSION;
    module->contract.generation = 1u;
    module->contract.targetToken = TEST_MODULE;
    module->contract.signatureHash = TEST_SIGNATURE_HASH;
    module->contract.layoutHash = TEST_LAYOUT_HASH;
    module->contract.moduleHash = TEST_MODULE_HASH;
    require_true(ZrCore_ExecIr_ModuleAppendConstant(module, constants, 3u,
                 &constantsRange) && constantsRange.start == 0u &&
                 constantsRange.count == 3u,
                 "append EIS5 branch and return constants");
    require_true(ZrCore_ExecIr_ModuleAddFunction(module, TEST_MEMBER,
                 TEST_SIGNATURE_HASH, &functionId) && functionId == 1u,
                 "add EIS5 function");
    function = ZrCore_ExecIr_ModuleFunctionAt(module, functionId);
    require_true(function != ZR_NULL, "lookup EIS5 function");
    function->contract.layoutHash = TEST_LAYOUT_HASH;
    function->contract.moduleHash = TEST_MODULE_HASH;
    require_true(ZrCore_ExecIr_FunctionAddBlock(
                 function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u &&
                 ZrCore_ExecIr_FunctionAddBlock(function, 0u) == 2u &&
                 ZrCore_ExecIr_FunctionAddBlock(function, 0u) == 3u &&
                 ZrCore_ExecIr_FunctionAddBlock(function, 0u) == 4u &&
                 ZrCore_ExecIr_FunctionAddBlock(function, 0u) == 5u,
                 "append five EIS5 CFG blocks");
    for (TZrUInt32 index = 0u; index < 3u; ++index) {
        values[index] = ZrCore_ExecIr_FunctionAddValue(
                function, boolPredicate && index == 0u
                                  ? ZR_VALUE_TYPE_BOOL
                                  : ZR_VALUE_TYPE_INT64,
                ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
                ZR_EXEC_IR_NULLABILITY_UNKNOWN);
        require_true(values[index] == index + 1u,
                     "append EIS5 result value");
    }
    require_true(ZrCore_ExecIr_FunctionAppendSuccessors(
                 function, successors, 4u, &allSuccessors) &&
                 allSuccessors.start == 0u && allSuccessors.count == 4u,
                 "append EIS5 successor pool");
    require_true(ZrCore_ExecIr_FunctionAppendPredecessors(
                 function, predecessors, 4u, &allPredecessors) &&
                 allPredecessors.start == 0u && allPredecessors.count == 4u,
                 "append reciprocal EIS5 predecessor pool");
    function->blocks[0].successors = (SZrExecIrRange){.offset = 0u,
                                                      .count = 2u};
    function->blocks[1].successors = (SZrExecIrRange){.offset = 2u,
                                                      .count = 1u};
    function->blocks[3].successors = (SZrExecIrRange){.offset = 3u,
                                                      .count = 1u};
    function->blocks[1].predecessors = (SZrExecIrRange){.offset = 0u,
                                                        .count = 1u};
    function->blocks[2].predecessors = (SZrExecIrRange){.offset = 1u,
                                                        .count = 1u};
    function->blocks[3].predecessors = (SZrExecIrRange){.offset = 2u,
                                                        .count = 1u};
    function->blocks[4].predecessors = (SZrExecIrRange){.offset = 3u,
                                                        .count = 1u};

    require_true(ZrCore_ExecIr_FunctionAppendResults(
                 function, &values[0], 1u, &conditionResult) &&
                 ZrCore_ExecIr_FunctionAppendResults(
                 function, &values[1], 1u, &trueResult) &&
                 ZrCore_ExecIr_FunctionAppendResults(
                 function, &values[2], 1u, &falseResult),
                 "append EIS5 result pool");
    require_true(ZrCore_ExecIr_FunctionAppendOperands(
                 function, &values[0], 1u, &conditionOperand) &&
                 ZrCore_ExecIr_FunctionAppendOperands(
                 function, &values[1], 1u, &trueReturnOperand) &&
                 ZrCore_ExecIr_FunctionAppendOperands(
                 function, &values[2], 1u, &falseReturnOperand),
                 "append EIS5 operand pool");

    eis5_append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT,
                            conditionResult, empty, empty, 0u, 1u);
    eis5_append_instruction(function,
                            ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,
                            empty, conditionOperand,
                            (SZrExecIrRange){.offset = 0u, .count = 2u},
                            0u, 2u);
    eis5_append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT,
                            trueResult, empty, empty, 1u, 3u);
    eis5_append_instruction(function, ZR_EXEC_IR_OPCODE_BRANCH,
                            empty, empty,
                            (SZrExecIrRange){.offset = 2u, .count = 1u},
                            0u, 4u);
    eis5_append_instruction(function, ZR_EXEC_IR_OPCODE_RETURN,
                            empty, trueReturnOperand, empty, 0u, 5u);
    eis5_append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT,
                            falseResult, empty, empty, 2u, 6u);
    eis5_append_instruction(function, ZR_EXEC_IR_OPCODE_BRANCH,
                            empty, empty,
                            (SZrExecIrRange){.offset = 3u, .count = 1u},
                            0u, 7u);
    eis5_append_instruction(function, ZR_EXEC_IR_OPCODE_RETURN,
                            empty, falseReturnOperand, empty, 0u, 8u);

    function->blocks[0].instructions =
            (SZrExecIrRange){.offset = 0u, .count = 2u};
    function->blocks[0].terminatorInstructionId = 2u;
    function->blocks[1].instructions =
            (SZrExecIrRange){.offset = 2u, .count = 2u};
    function->blocks[1].immediateDominator = 1u;
    function->blocks[1].terminatorInstructionId = 4u;
    function->blocks[2].instructions =
            (SZrExecIrRange){.offset = 4u, .count = 1u};
    function->blocks[2].immediateDominator = 2u;
    function->blocks[2].terminatorInstructionId = 5u;
    function->blocks[3].instructions =
            (SZrExecIrRange){.offset = 5u, .count = 2u};
    function->blocks[3].immediateDominator = 1u;
    function->blocks[3].terminatorInstructionId = 7u;
    function->blocks[4].instructions =
            (SZrExecIrRange){.offset = 7u, .count = 1u};
    function->blocks[4].immediateDominator = 4u;
    function->blocks[4].terminatorInstructionId = 8u;

    require_true(ZrCore_ExecIr_VerifyModule(module, &diagnostic),
                 "verify five-block EIS5 graph before persistence");
}

static void eis5_build_count_collision_graph(SZrExecIrModule *module,
                                            TZrBool condition) {
    SZrExecIrConstant constants[3] = {
        {ZR_VALUE_TYPE_INT64, 0u, condition ? 1u : 0u},
        {ZR_VALUE_TYPE_INT64, 0u, 42u},
        {ZR_VALUE_TYPE_INT64, 0u, 7u}
    };
    TZrExecIrBlockId successors[2] = {2u, 3u};
    TZrExecIrBlockId predecessors[2] = {1u, 1u};
    TZrExecIrValueId values[3];
    TZrExecIrFunctionId functionId = 0u;
    SZrExecIrFunction *function;
    SZrExecIrRange constantsRange, allSuccessors, allPredecessors;
    SZrExecIrRange results[3], operands[3];
    SZrExecIrRange empty = {.start = 0u, .count = 0u};
    SZrExecIrDiagnostic diagnostic;

    ZrCore_ExecIr_ModuleInit(module);
    module->id = 1u;
    module->moduleToken = TEST_MODULE;
    module->moduleHash = TEST_MODULE_HASH;
    module->contract.schemaVersion = ZR_EXECUTION_CONTRACT_SCHEMA_VERSION;
    module->contract.abiVersion = ZR_EXECUTION_CONTRACT_ABI_VERSION;
    module->contract.logicalVersion = ZR_EXECUTION_CONTRACT_LOGICAL_VERSION;
    module->contract.generation = 1u;
    module->contract.targetToken = TEST_MODULE;
    module->contract.signatureHash = TEST_SIGNATURE_HASH;
    module->contract.layoutHash = TEST_LAYOUT_HASH;
    module->contract.moduleHash = TEST_MODULE_HASH;
    require_true(ZrCore_ExecIr_ModuleAppendConstant(module, constants, 3u,
                 &constantsRange) && constantsRange.start == 0u &&
                 constantsRange.count == 3u,
                 "append colliding-count EIS5 constants");
    require_true(ZrCore_ExecIr_ModuleAddFunction(module, TEST_MEMBER,
                 TEST_SIGNATURE_HASH, &functionId) && functionId == 1u,
                 "add colliding-count EIS5 function");
    function = ZrCore_ExecIr_ModuleFunctionAt(module, functionId);
    require_true(function != ZR_NULL, "lookup colliding-count EIS5 function");
    function->contract.layoutHash = TEST_LAYOUT_HASH;
    function->contract.moduleHash = TEST_MODULE_HASH;
    require_true(ZrCore_ExecIr_FunctionAddBlock(
                 function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u &&
                 ZrCore_ExecIr_FunctionAddBlock(function, 0u) == 2u &&
                 ZrCore_ExecIr_FunctionAddBlock(function, 0u) == 3u,
                 "append colliding-count EIS5 blocks");
    for (TZrUInt32 index = 0u; index < 3u; ++index) {
        values[index] = ZrCore_ExecIr_FunctionAddValue(
                function, ZR_VALUE_TYPE_INT64,
                ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
                ZR_EXEC_IR_NULLABILITY_UNKNOWN);
        require_true(values[index] == index + 1u,
                     "append colliding-count EIS5 values");
    }
    require_true(ZrCore_ExecIr_FunctionAppendSuccessors(
                 function, successors, 2u, &allSuccessors) &&
                 allSuccessors.start == 0u && allSuccessors.count == 2u &&
                 ZrCore_ExecIr_FunctionAppendPredecessors(
                 function, predecessors, 2u, &allPredecessors) &&
                 allPredecessors.start == 0u && allPredecessors.count == 2u,
                 "append colliding-count reciprocal CFG edges");
    function->blocks[0].successors =
            (SZrExecIrRange){.offset = 0u, .count = 2u};
    function->blocks[1].predecessors =
            (SZrExecIrRange){.offset = 0u, .count = 1u};
    function->blocks[2].predecessors =
            (SZrExecIrRange){.offset = 1u, .count = 1u};
    for (TZrUInt32 index = 0u; index < 3u; ++index) {
        require_true(ZrCore_ExecIr_FunctionAppendResults(
                     function, &values[index], 1u, &results[index]) &&
                     ZrCore_ExecIr_FunctionAppendOperands(
                     function, &values[index], 1u, &operands[index]),
                     "append colliding-count EIS5 value pools");
    }
    for (TZrUInt32 index = 0u; index < 3u; ++index)
        eis5_append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT,
                                results[index], empty, empty, index,
                                index + 1u);
    eis5_append_instruction(function,
                            ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,
                            empty, operands[0],
                            (SZrExecIrRange){.offset = 0u, .count = 2u},
                            0u, 4u);
    eis5_append_instruction(function, ZR_EXEC_IR_OPCODE_RETURN,
                            empty, operands[1], empty, 0u, 5u);
    eis5_append_instruction(function, ZR_EXEC_IR_OPCODE_RETURN,
                            empty, operands[2], empty, 0u, 6u);
    function->blocks[0].instructions =
            (SZrExecIrRange){.offset = 0u, .count = 4u};
    function->blocks[0].terminatorInstructionId = 4u;
    function->blocks[1].instructions =
            (SZrExecIrRange){.offset = 4u, .count = 1u};
    function->blocks[1].immediateDominator = 1u;
    function->blocks[1].terminatorInstructionId = 5u;
    function->blocks[2].instructions =
            (SZrExecIrRange){.offset = 5u, .count = 1u};
    function->blocks[2].immediateDominator = 1u;
    function->blocks[2].terminatorInstructionId = 6u;
    require_true(ZrCore_ExecIr_VerifyModule(module, &diagnostic),
                 "verify valid EIS5 graph with EIS3 count collision");
}

static void eis5_assert_count_collision_graph(
        const SZrExecIrModule *module, TZrBool condition) {
    const SZrExecIrFunction *function;
    SZrExecIrOracleValue constants[3];
    SZrExecIrOracleInput input;
    SZrExecIrOracleExecutionResult execution;
    SZrExecIrDiagnostic diagnostic;
    require_true(ZrCore_ExecIr_VerifyModule(module, &diagnostic),
                 "verify decoded EIS5 count-collision graph");
    require_true(module->functionCount == 1u && module->constantCount == 3u &&
                 module->constants[0].bits == (condition ? 1u : 0u) &&
                 module->constants[1].bits == 42u &&
                 module->constants[2].bits == 7u,
                 "preserve EIS5 count-collision constants");
    function = &module->functions[0];
    require_true(function->blockCount == 3u &&
                 function->instructionCount == 6u &&
                 function->valueCount == 3u &&
                 function->operandCount == 3u &&
                 function->resultCount == 3u &&
                 function->successorCount == 2u &&
                 function->predecessorCount == 2u &&
                 function->blocks[0].instructions.count == 4u &&
                 function->blocks[1].instructions.start == 4u &&
                 function->blocks[2].instructions.start == 5u &&
                 function->instructions[3].opcode ==
                         ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,
                 "preserve EIS5 count-collision instruction layout");
    for (TZrUInt32 index = 0u; index < function->instructionCount; ++index)
        require_true(function->instructions[index].sourceId == 501u + index,
                     "preserve EIS5 count-collision source IDs");
    memset(constants, 0, sizeof(constants));
    for (TZrUInt32 index = 0u; index < 3u; ++index) {
        constants[index].kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
        constants[index].as.signedInteger =
                (TZrInt64)module->constants[index].bits;
    }
    memset(&input, 0, sizeof(input));
    input.function = function;
    input.constants = constants;
    input.constantCount = 3u;
    ZrCore_ExecIr_OracleResultInit(&execution);
    require_true(ZrCore_ExecIr_RunOracleEx(&input, &execution, &diagnostic),
                 "run colliding-count EIS5 graph through Oracle");
    require_true(execution.returned &&
                 execution.returnValue.kind ==
                         ZR_EXEC_IR_ORACLE_VALUE_SIGNED &&
                 execution.returnValue.as.signedInteger ==
                         (condition ? 42 : 7),
                 "colliding-count EIS5 Oracle returns true=42 and false=7");
    ZrCore_ExecIr_OracleResultFree(&execution);
}

static void eis5_colliding_count_write_test(const char *basePath,
                                            const SFixture *fixture) {
    TZrByte payload[TEST_EIS5_COUNT_COLLISION_ENCODED_SIZE];
    for (TZrUInt32 selected = 0u; selected < 2u; ++selected) {
        TZrBool condition = (TZrBool)selected;
        SZrExecIrModule source;
        SZrExecIrModule decoded;
        SZrArtifactExecIrDiagnostic diagnostic;
        SZrArtifactDiagnostic artifactDiagnostic;
        TZrUInt32 encodedSize = UINT32_MAX;
        char *path;
        eis5_build_count_collision_graph(&source, condition);
        eis5_assert_count_collision_graph(&source, condition);
        require_true(ZrCore_ArtifactExecIrScalar_GetEncodedSize(
                     &source, &encodedSize, &diagnostic) ==
                         ZR_ARTIFACT_EXEC_IR_OK &&
                     encodedSize == TEST_EIS5_COUNT_COLLISION_ENCODED_SIZE,
                     "EIS5 graph with EIS3 counts gets dynamic size");
        memset(payload, 0xa5, sizeof(payload));
        require_true(ZrCore_ArtifactExecIrScalar_Write(
                     &source, payload, encodedSize, &diagnostic) ==
                         ZR_ARTIFACT_EXEC_IR_OK &&
                     memcmp(payload, "EIS5", 4u) == 0 && payload[4] == 5u &&
                     eis5_get32(payload + TEST_EIS5_TOTAL_LENGTH_OFFSET) ==
                             encodedSize,
                     "EIS5 graph with EIS3 counts writes EIS5 magic");
        path = eis5_collision_artifact_path(basePath, condition);
        require_true(ZrParser_ExecIr_WriteCanonicalZroFile(
                     &fixture->metadata, &source, path,
                     &artifactDiagnostic) == ZR_ARTIFACT_STATUS_OK,
                     "write canonical EIS5 artifact with EIS3 length collision");
        free(path);
        ZrCore_ExecIr_ModuleInit(&decoded);
        require_true(ZrCore_ArtifactExecIrScalar_Read(
                     payload, encodedSize, &decoded, &diagnostic) ==
                         ZR_ARTIFACT_EXEC_IR_OK,
                     "read EIS5 graph with EIS3 count collision");
        eis5_assert_count_collision_graph(&decoded, condition);
        ZrCore_ExecIr_FreeModule(&decoded);
        ZrCore_ExecIr_FreeModule(&source);
    }
}

static void eis5_assert_graph_and_oracle(const SZrExecIrModule *module,
                                         TZrBool condition,
                                         TZrBool boolPredicate) {
    const SZrExecIrFunction *function;
    const TZrExecIrTypeToken predicateType =
            (TZrExecIrTypeToken)(boolPredicate ? ZR_VALUE_TYPE_BOOL
                                               : ZR_VALUE_TYPE_INT64);
    SZrExecIrOracleValue constants[3];
    SZrExecIrOracleInput input;
    SZrExecIrOracleExecutionResult execution;
    SZrExecIrDiagnostic diagnostic;
    require_true(ZrCore_ExecIr_VerifyModule(module, &diagnostic),
                 "verify EIS5 graph after decoding");
    require_true(module->functionCount == 1u && module->constantCount == 3u &&
                 module->constants[0].typeToken == predicateType &&
                 module->constants[0].bits == (condition ? 1u : 0u) &&
                 module->constants[1].bits == 42u &&
                 module->constants[2].bits == 7u,
                 "roundtrip preserves EIS5 constants");
    function = &module->functions[0];
    require_true(function->entryBlockId == 1u && function->blockCount == 5u &&
                 function->instructionCount == 8u &&
                 function->valueCount == 3u &&
                 function->operandCount == 3u && function->resultCount == 3u &&
                 function->successorCount == 4u &&
                 function->predecessorCount == 4u &&
                 function->values[0].typeToken == predicateType &&
                 function->values[1].typeToken ==
                         (TZrExecIrTypeToken)ZR_VALUE_TYPE_INT64 &&
                 function->values[2].typeToken ==
                         (TZrExecIrTypeToken)ZR_VALUE_TYPE_INT64 &&
                 function->successors[0] == 2u &&
                 function->successors[1] == 4u &&
                 function->successors[2] == 3u &&
                 function->successors[3] == 5u &&
                 function->predecessors[0] == 1u &&
                 function->predecessors[1] == 2u &&
                 function->predecessors[2] == 1u &&
                 function->predecessors[3] == 4u &&
                 function->blocks[0].instructions.count == 2u &&
                 function->blocks[1].instructions.start == 2u &&
                 function->blocks[2].instructions.start == 4u &&
                 function->blocks[3].instructions.start == 5u &&
                 function->blocks[4].instructions.start == 7u &&
                 function->instructions[1].opcode ==
                         ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH &&
                 function->instructions[1].successorRange.count == 2u,
                 "roundtrip preserves EIS5 block and edge pools");
    for (TZrUInt32 index = 0u; index < function->instructionCount; ++index)
        require_true(function->instructions[index].sourceId == 501u + index,
                     "roundtrip preserves EIS5 instruction source IDs");
    memset(constants, 0, sizeof(constants));
    for (TZrUInt32 index = 0u; index < 3u; ++index) {
        if (boolPredicate && index == 0u) {
            constants[index].kind = ZR_EXEC_IR_ORACLE_VALUE_BOOL;
            constants[index].as.boolean =
                    (TZrBool)module->constants[index].bits;
        } else {
            constants[index].kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
            constants[index].as.signedInteger =
                    (TZrInt64)module->constants[index].bits;
        }
    }
    memset(&input, 0, sizeof(input));
    input.function = function;
    input.constants = constants;
    input.constantCount = 3u;
    ZrCore_ExecIr_OracleResultInit(&execution);
    require_true(ZrCore_ExecIr_RunOracleEx(&input, &execution, &diagnostic),
                 "run EIS5 CFG through Oracle");
    require_true(execution.returned &&
                 execution.returnValue.kind ==
                         ZR_EXEC_IR_ORACLE_VALUE_SIGNED &&
                 execution.returnValue.as.signedInteger ==
                         (condition ? 42 : 7),
                 "EIS5 Oracle returns true=42 and false=7");
    ZrCore_ExecIr_OracleResultFree(&execution);
}

static void eis5_require_codec_rejected(
        const TZrByte *bytes, TZrUInt32 length,
        EZrArtifactExecIrStatus expected, TZrUInt32 expectedOffset,
        TZrBool checkOffset) {
    SZrExecIrModule output;
    SZrArtifactExecIrDiagnostic diagnostic;
    EZrArtifactExecIrStatus actual;
    ZrCore_ExecIr_ModuleInit(&output);
    actual = ZrCore_ArtifactExecIrScalar_Read(bytes, length, &output,
                                               &diagnostic);
    require_true(actual == expected, "malformed EIS5 codec status");
    if (checkOffset)
        require_true(diagnostic.byteOffset == expectedOffset,
                     "malformed EIS5 codec byte offset");
    require_true(output.functionCount == 0u && output.functions == ZR_NULL &&
                 output.constantCount == 0u && output.constants == ZR_NULL &&
                 output.layoutCount == 0u && output.layouts == ZR_NULL &&
                 output.sourceMapCount == 0u && output.sourceMaps == ZR_NULL,
                 "rejected EIS5 payload publishes no graph");
}

static void eis5_write_phase(const char *basePath) {
    SFixture fixture;
    init_fixture(&fixture);
    eis5_colliding_count_write_test(basePath, &fixture);
    for (TZrUInt32 selected = 0u; selected < 2u; ++selected) {
        TZrBool condition = (TZrBool)selected;
        SZrExecIrModule source;
        SZrExecIrModule decoded;
        SZrArtifactExecIrDiagnostic codecDiagnostic;
        SZrArtifactDiagnostic artifactDiagnostic;
        TZrByte *payload;
        TZrUInt32 encodedSize = UINT32_MAX;
        char *path;

        init_fixture(&fixture);
        eis5_build_graph(&source, condition, ZR_FALSE);
        require_true(ZrCore_ArtifactExecIrScalar_GetEncodedSize(
                     &source, &encodedSize, &codecDiagnostic) ==
                     ZR_ARTIFACT_EXEC_IR_OK &&
                     encodedSize == TEST_EIS5_FIVE_BLOCK_ENCODED_SIZE &&
                     encodedSize <= TEST_EIS5_MAX_ENCODED_SIZE,
                     "EIS5 five-block dynamic size query");
        payload = (TZrByte *)malloc(encodedSize);
        require_true(payload != ZR_NULL,
                     "allocate bounded EIS5 payload");
        memset(payload, 0xa5, encodedSize);
        require_true(ZrCore_ArtifactExecIrScalar_Write(
                     &source, payload, encodedSize, &codecDiagnostic) ==
                         ZR_ARTIFACT_EXEC_IR_OK,
                     "write variable-length EIS5 graph");
        require_true(memcmp(payload, "EIS5", 4u) == 0 &&
                     payload[4] == 5u && payload[5] == 0u &&
                     payload[6] == 0u && payload[7] == 0u &&
                     eis5_get32(payload + TEST_EIS5_TOTAL_LENGTH_OFFSET) ==
                             encodedSize &&
                     eis5_get32(payload + TEST_EIS5_CONSTANT_COUNT_OFFSET) ==
                             3u &&
                     eis5_get32(payload + TEST_EIS5_VALUE_COUNT_OFFSET) == 3u &&
                     eis5_get32(payload + TEST_EIS5_BLOCK_COUNT_OFFSET) == 5u &&
                     eis5_get32(payload +
                                TEST_EIS5_INSTRUCTION_COUNT_OFFSET) == 8u &&
                     eis5_get32(payload + TEST_EIS5_OPERAND_COUNT_OFFSET) == 3u &&
                     eis5_get32(payload + TEST_EIS5_RESULT_COUNT_OFFSET) == 3u &&
                     eis5_get32(payload + TEST_EIS5_SUCCESSOR_COUNT_OFFSET) == 4u &&
                     eis5_get32(payload + TEST_EIS5_PREDECESSOR_COUNT_OFFSET) == 4u,
                     "EIS5 header stores exact length and dynamic counts");
        ZrCore_ExecIr_ModuleInit(&decoded);
        require_true(ZrCore_ArtifactExecIrScalar_Read(
                     payload, encodedSize, &decoded, &codecDiagnostic) ==
                         ZR_ARTIFACT_EXEC_IR_OK,
                     "directly read variable-length EIS5 graph");
        eis5_assert_graph_and_oracle(&decoded, condition, ZR_FALSE);
        ZrCore_ExecIr_FreeModule(&decoded);
        path = eis5_artifact_path(basePath, condition);
        require_true(ZrParser_ExecIr_WriteCanonicalZroFile(
                     &fixture.metadata, &source, path,
                     &artifactDiagnostic) == ZR_ARTIFACT_STATUS_OK,
                     "write canonical EIS5 artifact");
        free(path);
        free(payload);
        ZrCore_ExecIr_FreeModule(&source);
    }

    eis5_bool_predicate_write_phase(basePath);
    eis5_compare_write_phase(basePath);
}

static void eis5_collision_read_phase(const char *basePath) {
    for (TZrUInt32 selected = 0u; selected < 2u; ++selected) {
        TZrBool condition = (TZrBool)selected;
        SFixture fixture;
        char *path = eis5_collision_artifact_path(basePath, condition);
        TZrSize length;
        TZrByte *bytes = read_file(path, &length);
        SZrExecIrModule module;
        SZrArtifactDiagnostic diagnostic;
        SZrArtifactView outer;
        SZrArtifactSectionView bundle = {0};
        SZrArtifactExecIrView nested;
        const SZrArtifactExecIrSectionView *payload = ZR_NULL;
        init_fixture(&fixture);
        ZrCore_ExecIr_ModuleInit(&module);
        require_true(ZrCore_Module_OpenExecIrArtifact(
                     bytes, length, &fixture.metadata.identity, &module,
                     &diagnostic) == ZR_ARTIFACT_STATUS_OK,
                     "open EIS5/EIS3-length collision in a separate process");
        eis5_assert_count_collision_graph(&module, condition);
        ZrCore_ExecIr_FreeModule(&module);
        require_true(ZrCore_Artifact_Read(bytes, length, &outer,
                     &diagnostic) == ZR_ARTIFACT_STATUS_OK &&
                     ZrCore_Artifact_FindSection(
                     &outer, ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE,
                     &bundle, &diagnostic) == ZR_ARTIFACT_STATUS_OK &&
                     ZrCore_ArtifactExecIr_Read(bundle.data,
                     bundle.byteLength, &nested, NULL) ==
                     ZR_ARTIFACT_EXEC_IR_OK &&
                     ZrCore_ArtifactExecIr_FindSection(
                     &nested, ZR_ARTIFACT_EXEC_IR_SECTION_EXEC_IR,
                     &payload, NULL) == ZR_ARTIFACT_EXEC_IR_OK &&
                     payload->byteLength ==
                             TEST_EIS5_COUNT_COLLISION_ENCODED_SIZE &&
                     memcmp(payload->data, "EIS5", 4u) == 0 &&
                     eis5_get32(payload->data +
                                TEST_EIS5_TOTAL_LENGTH_OFFSET) ==
                             payload->byteLength,
                     "canonical opener dispatches 996-byte payload by EIS5 magic");
        free(bytes);
        free(path);
    }
}

static void eis5_rehash(TZrByte *bytes, TZrSize bundleOffset,
                        TZrSize payloadOffset, TZrUInt32 payloadLength) {
    put64(bytes + bundleOffset + 20u,
          ZrCore_ArtifactExecIr_HashBytes(bytes + payloadOffset,
                                          payloadLength));
}

static void eis5_read_phase(const char *basePath) {
    for (TZrUInt32 selected = 0u; selected < 2u; ++selected) {
        TZrBool condition = (TZrBool)selected;
        SFixture fixture;
        char *path = eis5_artifact_path(basePath, condition);
        TZrSize length, bundleOffset, payloadOffset;
        TZrByte *bytes = read_file(path, &length);
        TZrByte *mutated = (TZrByte *)malloc(length);
        SZrExecIrModule module;
        SZrArtifactDiagnostic diagnostic;
        SZrArtifactView outer;
        SZrArtifactSectionView bundle;
        SZrArtifactExecIrView nested;
        const SZrArtifactExecIrSectionView *payload = ZR_NULL;
        require_true(mutated != ZR_NULL, "allocate EIS5 mutation buffer");
        init_fixture(&fixture);
        ZrCore_ExecIr_ModuleInit(&module);
        require_true(ZrCore_Module_OpenExecIrArtifact(
                     bytes, length, &fixture.metadata.identity, &module,
                     &diagnostic) == ZR_ARTIFACT_STATUS_OK,
                     "open EIS5 artifact in a separate process");
        eis5_assert_graph_and_oracle(&module, condition, ZR_FALSE);
        ZrCore_ExecIr_FreeModule(&module);
        require_true(ZrCore_Artifact_Read(bytes, length, &outer,
                     &diagnostic) == ZR_ARTIFACT_STATUS_OK &&
                     ZrCore_Artifact_FindSection(
                     &outer, ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE,
                     &bundle, &diagnostic) == ZR_ARTIFACT_STATUS_OK &&
                     ZrCore_ArtifactExecIr_Read(bundle.data,
                     bundle.byteLength, &nested, NULL) ==
                     ZR_ARTIFACT_EXEC_IR_OK &&
                     ZrCore_ArtifactExecIr_FindSection(
                     &nested, ZR_ARTIFACT_EXEC_IR_SECTION_EXEC_IR,
                     &payload, NULL) == ZR_ARTIFACT_EXEC_IR_OK &&
                     payload->byteLength > TEST_EIS5_HEADER_SIZE &&
                     payload->byteLength ==
                             TEST_EIS5_FIVE_BLOCK_ENCODED_SIZE &&
                     memcmp(payload->data, "EIS5", 4u) == 0 &&
                     eis5_get32(payload->data +
                                TEST_EIS5_TOTAL_LENGTH_OFFSET) ==
                             payload->byteLength,
                     "find bounded EIS5 payload in ERI1");
        bundleOffset = (TZrSize)(bundle.data - bytes);
        payloadOffset = (TZrSize)(payload->data - bytes);

        memcpy(mutated, bytes, length);
        put32(mutated + payloadOffset +
                      TEST_EIS5_FIRST_SUCCESSOR_ID_OFFSET,
              TEST_EIS5_MAX_BLOCK_COUNT + 1u);
        eis5_rehash(mutated, bundleOffset, payloadOffset,
                    payload->byteLength);
        eis5_require_codec_rejected(
                mutated + payloadOffset, payload->byteLength,
                ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                TEST_EIS5_FIRST_SUCCESSOR_ID_OFFSET, ZR_TRUE);
        diagnostic = require_rejected(mutated, length,
                                      ZR_ARTIFACT_STATUS_INVALID_SECTION);
        require_true(diagnostic.sectionKind ==
                         ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE &&
                     diagnostic.byteOffset == payloadOffset +
                             TEST_EIS5_FIRST_SUCCESSOR_ID_OFFSET,
                     "rehashed invalid EIS5 edge reports its wire offset");

        memcpy(mutated, bytes, length);
        put32(mutated + payloadOffset +
                      TEST_EIS5_FIRST_SUCCESSOR_ID_OFFSET,
              3u);
        eis5_rehash(mutated, bundleOffset, payloadOffset,
                    payload->byteLength);
        eis5_require_codec_rejected(
                mutated + payloadOffset, payload->byteLength,
                ZR_ARTIFACT_EXEC_IR_INVALID_SECTION, 0u, ZR_FALSE);
        diagnostic = require_rejected(mutated, length,
                                      ZR_ARTIFACT_STATUS_INVALID_SECTION);
        require_true(diagnostic.sectionKind ==
                         ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE,
                     "rehashed EIS5 nonreciprocal CFG edge is rejected");

        memcpy(mutated, bytes, length);
        put16(mutated + payloadOffset + 6u, 1u);
        eis5_rehash(mutated, bundleOffset, payloadOffset,
                    payload->byteLength);
        eis5_require_codec_rejected(
                mutated + payloadOffset, payload->byteLength,
                ZR_ARTIFACT_EXEC_IR_INVALID_SECTION, 6u, ZR_TRUE);
        diagnostic = require_rejected(mutated, length,
                                      ZR_ARTIFACT_STATUS_INVALID_SECTION);
        require_true(diagnostic.sectionKind ==
                         ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE &&
                     diagnostic.byteOffset == payloadOffset + 6u,
                     "rehashed nonzero EIS5 reserved field reports its offset");

        eis5_require_codec_rejected(
                payload->data, payload->byteLength - 1u,
                ZR_ARTIFACT_EXEC_IR_TRUNCATED, 0u, ZR_FALSE);

        memcpy(mutated, bytes, length);
        put32(mutated + payloadOffset + TEST_EIS5_BLOCK_COUNT_OFFSET,
              TEST_EIS5_MAX_BLOCK_COUNT + 1u);
        eis5_rehash(mutated, bundleOffset, payloadOffset,
                    payload->byteLength);
        eis5_require_codec_rejected(
                mutated + payloadOffset, payload->byteLength,
                ZR_ARTIFACT_EXEC_IR_LIMIT,
                TEST_EIS5_BLOCK_COUNT_OFFSET, ZR_TRUE);
        diagnostic = require_rejected(mutated, length,
                                      ZR_ARTIFACT_STATUS_INVALID_SECTION);
        require_true(diagnostic.sectionKind ==
                         ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE &&
                     diagnostic.byteOffset ==
                         payloadOffset + TEST_EIS5_BLOCK_COUNT_OFFSET,
                     "canonical opener rejects excessive EIS5 block count");

        memcpy(mutated, bytes, length);
        put32(mutated + payloadOffset + TEST_EIS5_VALUE_COUNT_OFFSET,
              TEST_EIS5_MAX_SCALAR_NODE_COUNT - 3u + 1u);
        eis5_rehash(mutated, bundleOffset, payloadOffset,
                    payload->byteLength);
        eis5_require_codec_rejected(
                mutated + payloadOffset, payload->byteLength,
                ZR_ARTIFACT_EXEC_IR_LIMIT,
                TEST_EIS5_VALUE_COUNT_OFFSET, ZR_TRUE);
        diagnostic = require_rejected(mutated, length,
                                      ZR_ARTIFACT_STATUS_INVALID_SECTION);
        require_true(diagnostic.sectionKind ==
                         ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE &&
                     diagnostic.byteOffset ==
                         payloadOffset + TEST_EIS5_VALUE_COUNT_OFFSET,
                     "canonical opener rejects excessive EIS5 node count");

        memcpy(mutated, bytes, length);
        put32(mutated + payloadOffset + TEST_EIS5_PREDECESSOR_COUNT_OFFSET,
              TEST_EIS5_MAX_POOL_COUNT - 3u - 3u - 4u + 1u);
        eis5_rehash(mutated, bundleOffset, payloadOffset,
                    payload->byteLength);
        eis5_require_codec_rejected(
                mutated + payloadOffset, payload->byteLength,
                ZR_ARTIFACT_EXEC_IR_LIMIT,
                TEST_EIS5_PREDECESSOR_COUNT_OFFSET, ZR_TRUE);
        diagnostic = require_rejected(mutated, length,
                                      ZR_ARTIFACT_STATUS_INVALID_SECTION);
        require_true(diagnostic.sectionKind ==
                         ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE &&
                     diagnostic.byteOffset == payloadOffset +
                             TEST_EIS5_PREDECESSOR_COUNT_OFFSET,
                     "canonical opener rejects excessive EIS5 pool count");

        memcpy(mutated, bytes, length);
        put32(mutated + payloadOffset + TEST_EIS5_TOTAL_LENGTH_OFFSET,
              TEST_EIS5_MAX_ENCODED_SIZE + 1u);
        eis5_rehash(mutated, bundleOffset, payloadOffset,
                    payload->byteLength);
        eis5_require_codec_rejected(
                mutated + payloadOffset, payload->byteLength,
                ZR_ARTIFACT_EXEC_IR_LIMIT,
                TEST_EIS5_TOTAL_LENGTH_OFFSET, ZR_TRUE);
        diagnostic = require_rejected(mutated, length,
                                      ZR_ARTIFACT_STATUS_INVALID_SECTION);
        require_true(diagnostic.sectionKind ==
                         ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE &&
                     diagnostic.byteOffset ==
                         payloadOffset + TEST_EIS5_TOTAL_LENGTH_OFFSET,
                     "canonical opener rejects declared EIS5 length over cap");

        memcpy(mutated, bytes, length);
        put32(mutated + payloadOffset + TEST_EIS5_BLOCK_COUNT_OFFSET,
              UINT32_MAX);
        eis5_rehash(mutated, bundleOffset, payloadOffset,
                    payload->byteLength);
        eis5_require_codec_rejected(
                mutated + payloadOffset, payload->byteLength,
                ZR_ARTIFACT_EXEC_IR_LIMIT,
                TEST_EIS5_BLOCK_COUNT_OFFSET, ZR_TRUE);
        diagnostic = require_rejected(mutated, length,
                                      ZR_ARTIFACT_STATUS_INVALID_SECTION);
        require_true(diagnostic.sectionKind ==
                         ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE &&
                     diagnostic.byteOffset ==
                         payloadOffset + TEST_EIS5_BLOCK_COUNT_OFFSET,
                     "rehashed excessive EIS5 count reports its wire offset");

        memcpy(mutated, bytes, length);
        put32(mutated + payloadOffset + TEST_EIS5_TOTAL_LENGTH_OFFSET,
              payload->byteLength - 1u);
        eis5_rehash(mutated, bundleOffset, payloadOffset,
                    payload->byteLength);
        eis5_require_codec_rejected(
                mutated + payloadOffset, payload->byteLength,
                ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                TEST_EIS5_TOTAL_LENGTH_OFFSET, ZR_TRUE);
        diagnostic = require_rejected(mutated, length,
                                      ZR_ARTIFACT_STATUS_INVALID_SECTION);
        require_true(diagnostic.sectionKind ==
                         ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE &&
                     diagnostic.byteOffset ==
                         payloadOffset + TEST_EIS5_TOTAL_LENGTH_OFFSET,
                     "rehashed EIS5 length mismatch reports its wire offset");

        memcpy(mutated, bytes, length);
        put16(mutated + payloadOffset + 4u, 6u);
        eis5_rehash(mutated, bundleOffset, payloadOffset,
                    payload->byteLength);
        eis5_require_codec_rejected(
                mutated + payloadOffset, payload->byteLength,
                ZR_ARTIFACT_EXEC_IR_UNSUPPORTED_VERSION, 4u, ZR_TRUE);
        require_rejected(mutated, length,
                         ZR_ARTIFACT_STATUS_UNSUPPORTED_VERSION);

        free(mutated);
        free(bytes);
        free(path);
    }
    eis5_collision_read_phase(basePath);
    eis5_bool_predicate_read_phase(basePath);
    eis5_compare_read_phase(basePath);
}
