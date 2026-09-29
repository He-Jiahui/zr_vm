/* Compare-specific EIS5 tests; include after the shared EIS5 test helpers. */
#define TEST_EIS5_COMPARE_ENCODED_SIZE 1508u
#define TEST_EIS5_COMPARE_MODE_LT 1u
#define TEST_EIS5_COMPARE_CONSTANTS_OFFSET 212u
#define TEST_EIS5_COMPARE_INPUT_VALUE_OFFSET 276u
#define TEST_EIS5_COMPARE_INPUT_VALUE_TYPE_TOKEN_OFFSET 284u
#define TEST_EIS5_COMPARE_MODE_OFFSET 800u
#define TEST_EIS5_COMPARE_RESULT_VALUE_OFFSET 324u
#define TEST_EIS5_COMPARE_RESULT_TYPE_TOKEN_OFFSET 332u

static char *eis5_compare_artifact_path(const char *basePath,
                                       TZrBool condition) {
    TZrSize capacity = strlen(basePath) + 36u;
    char *path = (char *)malloc(capacity);
    require_true(path != ZR_NULL, "allocate EIS5 Compare artifact path");
    snprintf(path, capacity, "%s.eis5-compare.%s", basePath,
             condition ? "true" : "false");
    return path;
}

static void eis5_compare_build_graph(SZrExecIrModule *module,
                                     TZrBool condition) {
    SZrExecIrConstant constants[4] = {
        {ZR_VALUE_TYPE_INT64, 0u, condition ? 1u : 3u},
        {ZR_VALUE_TYPE_INT64, 0u, 2u},
        {ZR_VALUE_TYPE_INT64, 0u, 42u},
        {ZR_VALUE_TYPE_INT64, 0u, 7u}
    };
    TZrExecIrBlockId successors[4] = {2u, 4u, 3u, 5u};
    TZrExecIrBlockId predecessors[4] = {1u, 2u, 1u, 4u};
    TZrExecIrValueId values[5];
    SZrExecIrRange results[5] = {{0}};
    SZrExecIrRange compareOperands = {0};
    SZrExecIrRange branchOperand = {0};
    SZrExecIrRange trueReturnOperand = {0};
    SZrExecIrRange falseReturnOperand = {0};
    SZrExecIrRange allSuccessors = {0};
    SZrExecIrRange allPredecessors = {0};
    SZrExecIrRange constantsRange = {0};
    SZrExecIrRange empty = {.start = 0u, .count = 0u};
    TZrExecIrFunctionId functionId = 0u;
    SZrExecIrFunction *function;
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
    require_true(ZrCore_ExecIr_ModuleAppendConstant(
                 module, constants, 4u, &constantsRange) &&
                 constantsRange.start == 0u && constantsRange.count == 4u,
                 "append EIS5 Compare inputs and return constants");
    require_true(ZrCore_ExecIr_ModuleAddFunction(
                 module, TEST_MEMBER, TEST_SIGNATURE_HASH, &functionId) &&
                 functionId == 1u,
                 "add EIS5 Compare function");
    function = ZrCore_ExecIr_ModuleFunctionAt(module, functionId);
    require_true(function != ZR_NULL, "lookup EIS5 Compare function");
    function->contract.layoutHash = TEST_LAYOUT_HASH;
    function->contract.moduleHash = TEST_MODULE_HASH;
    require_true(ZrCore_ExecIr_FunctionAddBlock(
                 function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u &&
                 ZrCore_ExecIr_FunctionAddBlock(function, 0u) == 2u &&
                 ZrCore_ExecIr_FunctionAddBlock(function, 0u) == 3u &&
                 ZrCore_ExecIr_FunctionAddBlock(function, 0u) == 4u &&
                 ZrCore_ExecIr_FunctionAddBlock(function, 0u) == 5u,
                 "append five EIS5 Compare CFG blocks");
    values[0] = ZrCore_ExecIr_FunctionAddValue(
            function, ZR_VALUE_TYPE_INT64,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    values[1] = ZrCore_ExecIr_FunctionAddValue(
            function, ZR_VALUE_TYPE_INT64,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    values[2] = ZrCore_ExecIr_FunctionAddValue(
            function, ZR_VALUE_TYPE_BOOL,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    values[3] = ZrCore_ExecIr_FunctionAddValue(
            function, ZR_VALUE_TYPE_INT64,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    values[4] = ZrCore_ExecIr_FunctionAddValue(
            function, ZR_VALUE_TYPE_INT64,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    for (TZrUInt32 index = 0u; index < 5u; ++index)
        require_true(values[index] == index + 1u,
                     "append EIS5 Compare SSA value");

    require_true(ZrCore_ExecIr_FunctionAppendSuccessors(
                 function, successors, 4u, &allSuccessors) &&
                 allSuccessors.start == 0u && allSuccessors.count == 4u &&
                 ZrCore_ExecIr_FunctionAppendPredecessors(
                 function, predecessors, 4u, &allPredecessors) &&
                 allPredecessors.start == 0u && allPredecessors.count == 4u,
                 "append reciprocal EIS5 Compare CFG edges");
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
    for (TZrUInt32 index = 0u; index < 5u; ++index)
        require_true(ZrCore_ExecIr_FunctionAppendResults(
                     function, &values[index], 1u, &results[index]),
                     "append EIS5 Compare result pool");
    require_true(ZrCore_ExecIr_FunctionAppendOperands(
                 function, values, 2u, &compareOperands) &&
                 ZrCore_ExecIr_FunctionAppendOperands(
                 function, &values[2], 1u, &branchOperand) &&
                 ZrCore_ExecIr_FunctionAppendOperands(
                 function, &values[3], 1u, &trueReturnOperand) &&
                 ZrCore_ExecIr_FunctionAppendOperands(
                 function, &values[4], 1u, &falseReturnOperand),
                 "append EIS5 Compare operand pools");

    eis5_append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT,
                            results[0], empty, empty, 0u, 1u);
    eis5_append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT,
                            results[1], empty, empty, 1u, 2u);
    eis5_append_instruction(function, ZR_EXEC_IR_OPCODE_COMPARE,
                            results[2], compareOperands, empty, 0u, 3u);
    function->instructions[2].typeToken = TEST_EIS5_COMPARE_MODE_LT;
    eis5_append_instruction(function,
                            ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,
                            empty, branchOperand,
                            (SZrExecIrRange){.offset = 0u, .count = 2u},
                            0u, 4u);
    eis5_append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT,
                            results[3], empty, empty, 2u, 5u);
    eis5_append_instruction(function, ZR_EXEC_IR_OPCODE_BRANCH,
                            empty, empty,
                            (SZrExecIrRange){.offset = 2u, .count = 1u},
                            0u, 6u);
    eis5_append_instruction(function, ZR_EXEC_IR_OPCODE_RETURN,
                            empty, trueReturnOperand, empty, 0u, 7u);
    eis5_append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT,
                            results[4], empty, empty, 3u, 8u);
    eis5_append_instruction(function, ZR_EXEC_IR_OPCODE_BRANCH,
                            empty, empty,
                            (SZrExecIrRange){.offset = 3u, .count = 1u},
                            0u, 9u);
    eis5_append_instruction(function, ZR_EXEC_IR_OPCODE_RETURN,
                            empty, falseReturnOperand, empty, 0u, 10u);

    function->blocks[0].instructions = (SZrExecIrRange){.offset = 0u,
                                                         .count = 4u};
    function->blocks[0].terminatorInstructionId = 4u;
    function->blocks[1].instructions = (SZrExecIrRange){.offset = 4u,
                                                         .count = 2u};
    function->blocks[1].immediateDominator = 1u;
    function->blocks[1].terminatorInstructionId = 6u;
    function->blocks[2].instructions = (SZrExecIrRange){.offset = 6u,
                                                         .count = 1u};
    function->blocks[2].immediateDominator = 2u;
    function->blocks[2].terminatorInstructionId = 7u;
    function->blocks[3].instructions = (SZrExecIrRange){.offset = 7u,
                                                         .count = 2u};
    function->blocks[3].immediateDominator = 1u;
    function->blocks[3].terminatorInstructionId = 9u;
    function->blocks[4].instructions = (SZrExecIrRange){.offset = 9u,
                                                         .count = 1u};
    function->blocks[4].immediateDominator = 4u;
    function->blocks[4].terminatorInstructionId = 10u;
    require_true(ZrCore_ExecIr_VerifyModule(module, &diagnostic),
                 "VerifyModule accepts EIS5 i64 LT Compare CFG");
}

static void eis5_compare_assert_graph_and_oracle(
        const SZrExecIrModule *module, TZrBool condition) {
    const SZrExecIrFunction *function;
    SZrExecIrOracleValue constants[4];
    SZrExecIrOracleInput input;
    SZrExecIrOracleExecutionResult execution;
    SZrExecIrDiagnostic diagnostic;

    require_true(ZrCore_ExecIr_VerifyModule(module, &diagnostic),
                 "Verify decoded EIS5 Compare graph");
    require_true(module->constantCount == 4u && module->functionCount == 1u &&
                 module->constants[0].bits == (condition ? 1u : 3u) &&
                 module->constants[1].bits == 2u &&
                 module->constants[2].bits == 42u &&
                 module->constants[3].bits == 7u,
                 "preserve EIS5 Compare constants");
    function = &module->functions[0];
    require_true(function->blockCount == 5u && function->instructionCount == 10u &&
                 function->valueCount == 5u && function->operandCount == 5u &&
                 function->resultCount == 5u && function->successorCount == 4u &&
                 function->predecessorCount == 4u &&
                 function->instructions[2].opcode == ZR_EXEC_IR_OPCODE_COMPARE &&
                 function->instructions[2].typeToken ==
                         TEST_EIS5_COMPARE_MODE_LT &&
                 function->instructions[2].operands.count == 2u &&
                 function->values[0].typeToken == ZR_VALUE_TYPE_INT64 &&
                 function->values[1].typeToken == ZR_VALUE_TYPE_INT64 &&
                 function->values[2].typeToken == ZR_VALUE_TYPE_BOOL &&
                 function->instructions[3].opcode ==
                         ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,
                 "preserve EIS5 i64 LT Compare shape and BOOL result");
    for (TZrUInt32 index = 0u; index < 4u; ++index) {
        memset(&constants[index], 0, sizeof(constants[index]));
        constants[index].kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
        constants[index].as.signedInteger =
                (TZrInt64)module->constants[index].bits;
    }
    memset(&input, 0, sizeof(input));
    input.function = function;
    input.constants = constants;
    input.constantCount = 4u;
    ZrCore_ExecIr_OracleResultInit(&execution);
    require_true(ZrCore_ExecIr_RunOracleEx(&input, &execution, &diagnostic),
                 "run EIS5 i64 LT Compare CFG through Oracle");
    require_true(execution.returned &&
                 execution.returnValue.kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED &&
                 execution.returnValue.as.signedInteger ==
                         (condition ? 42 : 7),
                 "EIS5 Compare Oracle returns true=42 and false=7");
    ZrCore_ExecIr_OracleResultFree(&execution);
}

static void eis5_compare_write_phase(const char *basePath) {
    SFixture fixture;
    init_fixture(&fixture);
    for (TZrUInt32 selected = 0u; selected < 2u; ++selected) {
        TZrBool condition = (TZrBool)selected;
        SZrExecIrModule source;
        SZrExecIrModule decoded;
        SZrExecIrFunction *function;
        SZrArtifactExecIrDiagnostic codecDiagnostic;
        SZrArtifactDiagnostic artifactDiagnostic;
        TZrByte *payload;
        TZrUInt32 encodedSize = UINT32_MAX;
        char *path;

        eis5_compare_build_graph(&source, condition);
        eis5_compare_assert_graph_and_oracle(&source, condition);
        require_true(ZrCore_ArtifactExecIrScalar_GetEncodedSize(
                     &source, &encodedSize, &codecDiagnostic) ==
                     ZR_ARTIFACT_EXEC_IR_OK &&
                     encodedSize == TEST_EIS5_COMPARE_ENCODED_SIZE,
                     "EIS5 i64 LT Compare dynamic size query");
        payload = (TZrByte *)malloc(encodedSize);
        require_true(payload != ZR_NULL,
                     "allocate EIS5 Compare payload");
        require_true(ZrCore_ArtifactExecIrScalar_Write(
                     &source, payload, encodedSize, &codecDiagnostic) ==
                     ZR_ARTIFACT_EXEC_IR_OK,
                     "write EIS5 i64 LT Compare payload");
        ZrCore_ExecIr_ModuleInit(&decoded);
        require_true(ZrCore_ArtifactExecIrScalar_Read(
                     payload, encodedSize, &decoded, &codecDiagnostic) ==
                     ZR_ARTIFACT_EXEC_IR_OK,
                     "read EIS5 i64 LT Compare payload directly");
        eis5_compare_assert_graph_and_oracle(&decoded, condition);
        ZrCore_ExecIr_FreeModule(&decoded);
        path = eis5_compare_artifact_path(basePath, condition);
        require_true(ZrParser_ExecIr_WriteCanonicalZroFile(
                     &fixture.metadata, &source, path, &artifactDiagnostic) ==
                     ZR_ARTIFACT_STATUS_OK,
                     "write canonical EIS5 Compare artifact");
        free(path);

        function = &source.functions[0];
        if (condition) {
            source.constants[0].typeToken = ZR_VALUE_TYPE_BOOL;
            function->values[0].typeToken = ZR_VALUE_TYPE_BOOL;
            require_true(ZrCore_ArtifactExecIrScalar_GetEncodedSize(
                         &source, &encodedSize, &codecDiagnostic) ==
                         ZR_ARTIFACT_EXEC_IR_INVALID_SECTION &&
                         codecDiagnostic.byteOffset ==
                                 TEST_EIS5_COMPARE_INPUT_VALUE_OFFSET,
                         "writer rejects non-i64 EIS5 Compare input");
            source.constants[0].typeToken = ZR_VALUE_TYPE_INT64;
            function->values[0].typeToken = ZR_VALUE_TYPE_INT64;
        }
        function->instructions[2].typeToken =
                TEST_EIS5_COMPARE_MODE_LT + 1u;
        require_true(ZrCore_ArtifactExecIrScalar_GetEncodedSize(
                     &source, &encodedSize, &codecDiagnostic) ==
                     ZR_ARTIFACT_EXEC_IR_INVALID_SECTION &&
                     codecDiagnostic.byteOffset ==
                             TEST_EIS5_COMPARE_MODE_OFFSET,
                     "writer rejects unsupported EIS5 Compare mode");
        function->instructions[2].typeToken = TEST_EIS5_COMPARE_MODE_LT;
        function->values[2].typeToken = ZR_VALUE_TYPE_INT64;
        require_true(ZrCore_ArtifactExecIrScalar_GetEncodedSize(
                     &source, &encodedSize, &codecDiagnostic) ==
                     ZR_ARTIFACT_EXEC_IR_INVALID_SECTION &&
                     codecDiagnostic.byteOffset ==
                             TEST_EIS5_COMPARE_RESULT_VALUE_OFFSET,
                     "writer rejects non-BOOL EIS5 Compare result");
        free(payload);
        ZrCore_ExecIr_FreeModule(&source);
    }
}

static void eis5_compare_read_phase(const char *basePath) {
    for (TZrUInt32 selected = 0u; selected < 2u; ++selected) {
        TZrBool condition = (TZrBool)selected;
        SFixture fixture;
        char *path = eis5_compare_artifact_path(basePath, condition);
        TZrSize length, bundleOffset, payloadOffset;
        TZrByte *bytes = read_file(path, &length);
        TZrByte *mutated = (TZrByte *)malloc(length);
        SZrExecIrModule module;
        SZrArtifactDiagnostic diagnostic;
        SZrArtifactView outer;
        SZrArtifactSectionView bundle = {0};
        SZrArtifactExecIrView nested;
        const SZrArtifactExecIrSectionView *payload = ZR_NULL;

        require_true(mutated != ZR_NULL,
                     "allocate EIS5 Compare mutation buffer");
        init_fixture(&fixture);
        ZrCore_ExecIr_ModuleInit(&module);
        require_true(ZrCore_Module_OpenExecIrArtifact(
                     bytes, length, &fixture.metadata.identity, &module,
                     &diagnostic) == ZR_ARTIFACT_STATUS_OK,
                     "open EIS5 Compare artifact in a separate process");
        eis5_compare_assert_graph_and_oracle(&module, condition);
        ZrCore_ExecIr_FreeModule(&module);
        require_true(ZrCore_Artifact_Read(bytes, length, &outer, &diagnostic) ==
                     ZR_ARTIFACT_STATUS_OK &&
                     ZrCore_Artifact_FindSection(
                     &outer, ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE,
                     &bundle, &diagnostic) == ZR_ARTIFACT_STATUS_OK &&
                     ZrCore_ArtifactExecIr_Read(bundle.data, bundle.byteLength,
                     &nested, ZR_NULL) == ZR_ARTIFACT_EXEC_IR_OK &&
                     ZrCore_ArtifactExecIr_FindSection(
                     &nested, ZR_ARTIFACT_EXEC_IR_SECTION_EXEC_IR,
                     &payload, ZR_NULL) == ZR_ARTIFACT_EXEC_IR_OK &&
                     payload->byteLength == TEST_EIS5_COMPARE_ENCODED_SIZE &&
                     memcmp(payload->data, "EIS5", 4u) == 0,
                     "find EIS5 Compare payload in canonical artifact");
        bundleOffset = (TZrSize)(bundle.data - bytes);
        payloadOffset = (TZrSize)(payload->data - bytes);

        memcpy(mutated, bytes, length);
        put32(mutated + payloadOffset + TEST_EIS5_COMPARE_MODE_OFFSET,
              TEST_EIS5_COMPARE_MODE_LT + 1u);
        eis5_rehash(mutated, bundleOffset, payloadOffset, payload->byteLength);
        eis5_require_codec_rejected(
                mutated + payloadOffset, payload->byteLength,
                ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                TEST_EIS5_COMPARE_MODE_OFFSET, ZR_TRUE);
        diagnostic = require_rejected(mutated, length,
                                      ZR_ARTIFACT_STATUS_INVALID_SECTION);
        require_true(diagnostic.sectionKind ==
                         ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE &&
                     diagnostic.byteOffset == payloadOffset +
                             TEST_EIS5_COMPARE_MODE_OFFSET,
                     "opener rejects rehashed unsupported Compare mode");

        if (condition) {
            memcpy(mutated, bytes, length);
            put32(mutated + payloadOffset +
                          TEST_EIS5_COMPARE_CONSTANTS_OFFSET,
                  (TZrUInt32)ZR_VALUE_TYPE_BOOL);
            put32(mutated + payloadOffset +
                          TEST_EIS5_COMPARE_INPUT_VALUE_TYPE_TOKEN_OFFSET,
                  (TZrUInt32)ZR_VALUE_TYPE_BOOL);
            eis5_rehash(mutated, bundleOffset, payloadOffset,
                        payload->byteLength);
            eis5_require_codec_rejected(
                    mutated + payloadOffset, payload->byteLength,
                    ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                    TEST_EIS5_COMPARE_INPUT_VALUE_OFFSET, ZR_TRUE);
            diagnostic = require_rejected(mutated, length,
                                          ZR_ARTIFACT_STATUS_INVALID_SECTION);
            require_true(diagnostic.sectionKind ==
                             ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE &&
                         diagnostic.byteOffset == payloadOffset +
                                 TEST_EIS5_COMPARE_INPUT_VALUE_OFFSET,
                         "opener rejects rehashed non-i64 Compare input");
        }

        memcpy(mutated, bytes, length);
        put32(mutated + payloadOffset +
                      TEST_EIS5_COMPARE_RESULT_TYPE_TOKEN_OFFSET,
              (TZrUInt32)ZR_VALUE_TYPE_INT64);
        eis5_rehash(mutated, bundleOffset, payloadOffset, payload->byteLength);
        eis5_require_codec_rejected(
                mutated + payloadOffset, payload->byteLength,
                ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                TEST_EIS5_COMPARE_RESULT_VALUE_OFFSET, ZR_TRUE);
        diagnostic = require_rejected(mutated, length,
                                      ZR_ARTIFACT_STATUS_INVALID_SECTION);
        require_true(diagnostic.sectionKind ==
                         ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE &&
                     diagnostic.byteOffset == payloadOffset +
                             TEST_EIS5_COMPARE_RESULT_VALUE_OFFSET,
                     "opener rejects rehashed non-BOOL Compare result");
        free(mutated);
        free(bytes);
        free(path);
    }
}
