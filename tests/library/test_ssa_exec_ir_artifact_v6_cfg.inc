/* EIS3 test-only extension of test_ssa_exec_ir_artifact_v6.c. The enclosing
 * test owns SFixture, require_true, put16/put32/put64, read_file, and
 * require_rejected. This fixture intentionally exposes the EIS1/EIS2-only
 * codec boundary until EIS3 support is implemented. */

#define TEST_EIS3_ENCODED_SIZE 996u
#define TEST_EIS3_LENGTH_OFFSET 8u
#define TEST_EIS3_BLOCK_COUNT_OFFSET 188u
#define TEST_EIS3_ENTRY_SUCCESSOR_COUNT_OFFSET 360u
#define TEST_EIS3_FIRST_SUCCESSOR_OFFSET 980u
#define TEST_EIS3_FIRST_PREDECESSOR_OFFSET 988u

static char *cfg_artifact_path(const char *path, TZrBool condition) {
    size_t length = strlen(path);
    char *result;
    require_true(length >= 4u && strcmp(path + length - 4u, ".zro") == 0 &&
                 length <= SIZE_MAX - 16u, "bounded CFG artifact path");
    result = (char *)malloc(length + 16u);
    require_true(result != NULL, "allocate CFG artifact path");
    memcpy(result, path, length - 4u);
    strcpy(result + length - 4u,
           condition ? "-cfg-true.zro" : "-cfg-false.zro");
    return result;
}

static void cfg_append_instruction(SZrExecIrFunction *function,
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
    instruction.sourceId = 200u + expectedId;
    require_true(ZrCore_ExecIr_FunctionAppendInstruction(
                 function, &instruction, &actualId) &&
                 actualId == expectedId, "append counted CFG instruction");
}

static void cfg_build_graph(SZrExecIrModule *module, TZrBool condition) {
    SZrExecIrConstant constants[3] = {
        {ZR_VALUE_TYPE_INT64, 0u, condition ? 1u : 0u},
        {ZR_VALUE_TYPE_INT64, 0u, 42u},
        {ZR_VALUE_TYPE_INT64, 0u, 7u}
    };
    TZrExecIrBlockId successors[2] = {2u, 3u};
    TZrExecIrBlockId predecessor = 1u;
    TZrExecIrValueId values[3];
    TZrExecIrFunctionId functionId = 0u;
    SZrExecIrFunction *function;
    SZrExecIrRange constantsRange, results, operands, branchSuccessors;
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
                 constantsRange.count == 3u, "append CFG constants");
    require_true(ZrCore_ExecIr_ModuleAddFunction(module, TEST_MEMBER,
                 TEST_SIGNATURE_HASH, &functionId) && functionId == 1u,
                 "add CFG function");
    function = ZrCore_ExecIr_ModuleFunctionAt(module, functionId);
    require_true(function != ZR_NULL, "lookup CFG function");
    function->contract.layoutHash = TEST_LAYOUT_HASH;
    function->contract.moduleHash = TEST_MODULE_HASH;
    require_true(ZrCore_ExecIr_FunctionAddBlock(
                 function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u &&
                 ZrCore_ExecIr_FunctionAddBlock(function, 0u) == 2u &&
                 ZrCore_ExecIr_FunctionAddBlock(function, 0u) == 3u,
                 "append CFG fork blocks");
    for (TZrUInt32 index = 0u; index < 3u; ++index) {
        values[index] = ZrCore_ExecIr_FunctionAddValue(
                function, ZR_VALUE_TYPE_INT64,
                ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
                ZR_EXEC_IR_NULLABILITY_UNKNOWN);
        require_true(values[index] == index + 1u, "append CFG values");
    }
    require_true(ZrCore_ExecIr_FunctionAppendSuccessors(
                 function, successors, 2u, &branchSuccessors) &&
                 branchSuccessors.start == 0u &&
                 branchSuccessors.count == 2u,
                 "append ordered true and false successors");
    function->blocks[0].successors = branchSuccessors;
    require_true(ZrCore_ExecIr_FunctionAppendPredecessors(
                 function, &predecessor, 1u,
                 &function->blocks[1].predecessors) &&
                 ZrCore_ExecIr_FunctionAppendPredecessors(
                 function, &predecessor, 1u,
                 &function->blocks[2].predecessors),
                 "append reciprocal CFG predecessors");

    require_true(ZrCore_ExecIr_FunctionAppendResults(
                 function, &values[0], 1u, &results),
                 "append condition result");
    cfg_append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT,
                           results, empty, empty, 0u, 1u);
    require_true(ZrCore_ExecIr_FunctionAppendOperands(
                 function, &values[0], 1u, &operands),
                 "append branch condition operand");
    cfg_append_instruction(function, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,
                           empty, operands, branchSuccessors, 0u, 2u);
    require_true(ZrCore_ExecIr_FunctionAppendResults(
                 function, &values[1], 1u, &results),
                 "append true result");
    cfg_append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT,
                           results, empty, empty, 1u, 3u);
    require_true(ZrCore_ExecIr_FunctionAppendOperands(
                 function, &values[1], 1u, &operands),
                 "append true return operand");
    cfg_append_instruction(function, ZR_EXEC_IR_OPCODE_RETURN,
                           empty, operands, empty, 0u, 4u);
    require_true(ZrCore_ExecIr_FunctionAppendResults(
                 function, &values[2], 1u, &results),
                 "append false result");
    cfg_append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT,
                           results, empty, empty, 2u, 5u);
    require_true(ZrCore_ExecIr_FunctionAppendOperands(
                 function, &values[2], 1u, &operands),
                 "append false return operand");
    cfg_append_instruction(function, ZR_EXEC_IR_OPCODE_RETURN,
                           empty, operands, empty, 0u, 6u);

    function->blocks[0].instructions =
            (SZrExecIrRange){.start = 0u, .count = 2u};
    function->blocks[0].terminatorInstructionId = 2u;
    function->blocks[1].instructions =
            (SZrExecIrRange){.start = 2u, .count = 2u};
    function->blocks[1].immediateDominator = 1u;
    function->blocks[1].terminatorInstructionId = 4u;
    function->blocks[2].instructions =
            (SZrExecIrRange){.start = 4u, .count = 2u};
    function->blocks[2].immediateDominator = 1u;
    function->blocks[2].terminatorInstructionId = 6u;
    require_true(ZrCore_ExecIr_VerifyModule(module, &diagnostic),
                 "verify source CFG before persistence");
}

static void cfg_assert_graph_and_oracle(const SZrExecIrModule *module,
                                         TZrBool condition) {
    const SZrExecIrFunction *function;
    SZrExecIrOracleValue constants[3];
    SZrExecIrOracleInput input;
    SZrExecIrOracleExecutionResult execution;
    SZrExecIrDiagnostic diagnostic;
    require_true(ZrCore_ExecIr_VerifyModule(module, &diagnostic),
                 "verify decoded CFG");
    require_true(module->functionCount == 1u && module->constantCount == 3u &&
                 module->constants[0].bits == (condition ? 1u : 0u) &&
                 module->constants[1].bits == 42u &&
                 module->constants[2].bits == 7u,
                 "decoded CFG constant table");
    function = &module->functions[0];
    require_true(function->entryBlockId == 1u && function->blockCount == 3u &&
                 function->instructionCount == 6u &&
                 function->valueCount == 3u &&
                 function->resultCount == 3u &&
                 function->operandCount == 3u &&
                 function->successorCount == 2u &&
                 function->predecessorCount == 2u &&
                 function->values[0].definition == 1u &&
                 function->values[1].definition == 3u &&
                 function->values[2].definition == 5u &&
                 function->instructions[0].results.start == 0u &&
                 function->instructions[0].results.count == 1u &&
                 function->instructions[1].operands.start == 0u &&
                 function->instructions[1].operands.count == 1u &&
                 function->instructions[1].successorRange.start == 0u &&
                 function->instructions[1].successorRange.count == 2u &&
                 function->instructions[1].opcode ==
                         ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH &&
                 function->instructions[0].sourceId == 201u &&
                 function->instructions[1].sourceId == 202u &&
                 function->instructions[2].sourceId == 203u &&
                 function->instructions[3].sourceId == 204u &&
                 function->instructions[4].sourceId == 205u &&
                 function->instructions[5].sourceId == 206u &&
                 function->successors[0] == 2u &&
                 function->successors[1] == 3u &&
                 function->predecessors[0] == 1u &&
                 function->predecessors[1] == 1u,
                 "decoded ordered CFG fork and reciprocal edges");
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
                 "execute decoded conditional CFG in Oracle");
    require_true(execution.returned &&
                 execution.returnValue.kind ==
                         ZR_EXEC_IR_ORACLE_VALUE_SIGNED &&
                 execution.returnValue.as.signedInteger ==
                         (condition ? 42 : 7),
                 "Oracle selects true 42 or false 7");
    ZrCore_ExecIr_OracleResultFree(&execution);
}

static void cfg_rehash(TZrByte *bytes, TZrSize bundleOffset,
                       TZrSize payloadOffset, TZrUInt32 payloadLength) {
    put64(bytes + bundleOffset + 20u,
          ZrCore_ArtifactExecIr_HashBytes(bytes + payloadOffset,
                                          payloadLength));
}

static TZrUInt32 cfg_get32(const TZrByte *bytes) {
    return (TZrUInt32)bytes[0] |
           ((TZrUInt32)bytes[1] << 8u) |
           ((TZrUInt32)bytes[2] << 16u) |
           ((TZrUInt32)bytes[3] << 24u);
}

static void cfg_require_codec_rejected(const TZrByte *bytes,
                                       TZrUInt32 payloadLength,
                                       EZrArtifactExecIrStatus expected,
                                       TZrUInt32 expectedOffset) {
    SZrExecIrModule output;
    SZrArtifactExecIrDiagnostic diagnostic;
    ZrCore_ExecIr_ModuleInit(&output);
    require_true(ZrCore_ArtifactExecIrScalar_Read(
                 bytes, payloadLength, &output, &diagnostic) == expected &&
                 diagnostic.byteOffset == expectedOffset &&
                 output.functionCount == 0u && output.functions == ZR_NULL &&
                 output.constantCount == 0u && output.constants == ZR_NULL,
                 "malformed EIS3 decoder rejects without publishing graph");
}

static void cfg_write_phase(const char *basePath) {
    SFixture fixture;
    SZrExecIrModule source;
    SZrExecIrModule decoded;
    SZrArtifactSectionInput sectionsWithRelocation[8];
    SZrExecIrSourceMap unsupportedMap;
    SZrArtifactExecIrDiagnostic codecDiagnostic;
    SZrArtifactDiagnostic artifactDiagnostic;
    TZrUInt32 encodedSize;
    TZrByte payload[TEST_EIS3_ENCODED_SIZE];
    TZrByte unchanged[TEST_EIS3_ENCODED_SIZE];
    init_fixture(&fixture);
    for (TZrUInt32 selected = 0u; selected < 2u; ++selected) {
        char *path = cfg_artifact_path(basePath, (TZrBool)selected);
        TZrSize originalLength, retainedLength;
        TZrByte *original, *retained;
        cfg_build_graph(&source, (TZrBool)selected);
        encodedSize = 0u;
        require_true(ZrCore_ArtifactExecIrScalar_GetEncodedSize(
                     &source, &encodedSize, &codecDiagnostic) ==
                     ZR_ARTIFACT_EXEC_IR_OK &&
                     encodedSize == TEST_EIS3_ENCODED_SIZE,
                     "EIS3 counted CFG exact 996-byte payload size");
        memset(payload, 0xa5, sizeof(payload));
        require_true(ZrCore_ArtifactExecIrScalar_Write(
                     &source, payload, sizeof(payload), &codecDiagnostic) ==
                     ZR_ARTIFACT_EXEC_IR_OK &&
                      memcmp(payload, "EIS3", 4u) == 0 && payload[4] == 3u &&
                      payload[6] == 0u && payload[7] == 0u &&
                      cfg_get32(payload + TEST_EIS3_LENGTH_OFFSET) ==
                              TEST_EIS3_ENCODED_SIZE,
                     "EIS3 distinct versioned counted payload");
        ZrCore_ExecIr_ModuleInit(&decoded);
        require_true(ZrCore_ArtifactExecIrScalar_Read(
                     payload, sizeof(payload), &decoded, &codecDiagnostic) ==
                     ZR_ARTIFACT_EXEC_IR_OK,
                     "decode counted CFG payload directly");
        cfg_assert_graph_and_oracle(&decoded, (TZrBool)selected);
        ZrCore_ExecIr_FreeModule(&decoded);
        memcpy(unchanged, payload, sizeof(payload));
        source.functions[0].instructions[1].bindingRow = 1u;
        encodedSize = 777u;
        require_true(ZrCore_ArtifactExecIrScalar_GetEncodedSize(
                     &source, &encodedSize, &codecDiagnostic) ==
                     ZR_ARTIFACT_EXEC_IR_INVALID_SECTION &&
                     encodedSize == 777u,
                     "rejected EIS3 size query preserves output");
        require_true(ZrCore_ArtifactExecIrScalar_Write(
                     &source, payload, sizeof(payload), &codecDiagnostic) ==
                     ZR_ARTIFACT_EXEC_IR_INVALID_SECTION &&
                     memcmp(payload, unchanged, sizeof(payload)) == 0,
                     "rejected EIS3 encode preserves destination");
        source.functions[0].instructions[1].bindingRow = 0u;
        require_true(ZrParser_ExecIr_WriteCanonicalZroFile(
                     &fixture.metadata, &source, path,
                     &artifactDiagnostic) == ZR_ARTIFACT_STATUS_OK,
                     "write real EIS3 CFG ZRAF file");
        original = read_file(path, &originalLength);
#ifdef ZR_ARTIFACT_TEST_IO_FAILURE
        ZrParser_ExecIrArtifact_TestInjectWriteFailure(ZR_TRUE);
        require_true(ZrParser_ExecIr_WriteCanonicalZroFile(
                     &fixture.metadata, &source, path,
                     &artifactDiagnostic) ==
                     ZR_ARTIFACT_STATUS_INVALID_SECTION,
                     "injected EIS3 rewrite failure");
        ZrParser_ExecIrArtifact_TestInjectWriteFailure(ZR_FALSE);
#endif
        source.functions[0].instructions[1].bindingRow = 1u;
        require_true(ZrParser_ExecIr_WriteCanonicalZroFile(
                     &fixture.metadata, &source, path,
                     &artifactDiagnostic) ==
                     ZR_ARTIFACT_STATUS_INVALID_SECTION,
                     "reject unresolved CFG binding before file publication");
        source.functions[0].instructions[1].bindingRow = 0u;
        memset(&unsupportedMap, 0, sizeof(unsupportedMap));
        source.functions[0].sourceMaps = &unsupportedMap;
        source.functions[0].sourceMapCount = 1u;
        source.functions[0].sourceMapCapacity = 1u;
        require_true(ZrParser_ExecIr_WriteCanonicalZroFile(
                     &fixture.metadata, &source, path,
                     &artifactDiagnostic) ==
                     ZR_ARTIFACT_STATUS_INVALID_SECTION,
                     "reject unsupported EIS3 source map");
        source.functions[0].sourceMaps = NULL;
        source.functions[0].sourceMapCount = 0u;
        source.functions[0].sourceMapCapacity = 0u;
        memcpy(sectionsWithRelocation, fixture.sections,
               sizeof(fixture.sections));
        sectionsWithRelocation[7] = (SZrArtifactSectionInput){
                ZR_ARTIFACT_SECTION_RELOCATION_BINDING_TABLE,
                0u, 0u, NULL};
        fixture.metadata.sections = sectionsWithRelocation;
        fixture.metadata.sectionCount = 8u;
        require_true(ZrParser_ExecIr_WriteCanonicalZroFile(
                     &fixture.metadata, &source, path,
                     &artifactDiagnostic) ==
                     ZR_ARTIFACT_STATUS_INVALID_SECTION,
                     "reject unsupported EIS3 relocation section");
        fixture.metadata.sections = fixture.sections;
        fixture.metadata.sectionCount = 7u;
        retained = read_file(path, &retainedLength);
        require_true(retainedLength == originalLength &&
                     memcmp(retained, original, originalLength) == 0,
                     "invalid CFG rewrite preserves existing file bytes");
        free(retained);
        free(original);
        ZrCore_ExecIr_FreeModule(&source);
        free(path);
    }
}

static void cfg_read_phase(const char *basePath) {
    SFixture fixture;
    init_fixture(&fixture);
    for (TZrUInt32 selected = 0u; selected < 2u; ++selected) {
        char *path = cfg_artifact_path(basePath, (TZrBool)selected);
        TZrSize length, bundleOffset, payloadOffset;
        TZrByte *bytes = read_file(path, &length);
        TZrByte *mutated = (TZrByte *)malloc(length);
        SZrExecIrModule module;
        SZrArtifactDiagnostic diagnostic;
        SZrArtifactView outer;
        SZrArtifactSectionView bundle;
        SZrArtifactExecIrView nested;
        SZrArtifactExecIrView rejectedView;
        SZrArtifactExecIrDiagnostic nestedDiagnostic;
        const SZrArtifactExecIrSectionView *payload;
        require_true(mutated != NULL, "allocate CFG mutation buffer");
        ZrCore_ExecIr_ModuleInit(&module);
        require_true(ZrCore_Module_OpenExecIrArtifact(
                     bytes, length, &fixture.metadata.identity, &module,
                     &diagnostic) == ZR_ARTIFACT_STATUS_OK,
                     "open real EIS3 CFG artifact in separate process");
        cfg_assert_graph_and_oracle(&module, (TZrBool)selected);
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
                     payload->byteLength == TEST_EIS3_ENCODED_SIZE &&
                     memcmp(payload->data, "EIS3", 4u) == 0,
                     "locate complete EIS3 payload inside ERI1");
        bundleOffset = (TZrSize)(bundle.data - bytes);
        payloadOffset = (TZrSize)(payload->data - bytes);

        memcpy(mutated, bytes, length);
        put32(mutated + payloadOffset +
              TEST_EIS3_FIRST_SUCCESSOR_OFFSET, 4u);
        memset(&rejectedView, 0xa5, sizeof(rejectedView));
        require_true(ZrCore_ArtifactExecIr_Read(
                     mutated + bundleOffset, bundle.byteLength,
                     &rejectedView, &nestedDiagnostic) ==
                     ZR_ARTIFACT_EXEC_IR_HASH_MISMATCH &&
                     rejectedView.buffer == NULL &&
                     rejectedView.sectionCount == 0u,
                     "changed EIS3 edge fails ERI1 hash without a view");
        require_rejected(mutated, length, ZR_ARTIFACT_STATUS_INVALID_SECTION);
        cfg_rehash(mutated, bundleOffset, payloadOffset,
                   payload->byteLength);
        cfg_require_codec_rejected(
                mutated + payloadOffset, payload->byteLength,
                ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                TEST_EIS3_FIRST_SUCCESSOR_OFFSET);
        diagnostic = require_rejected(mutated, length,
                                      ZR_ARTIFACT_STATUS_INVALID_SECTION);
        require_true(diagnostic.sectionKind ==
                     ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE &&
                     diagnostic.byteOffset == payloadOffset +
                             TEST_EIS3_FIRST_SUCCESSOR_OFFSET,
                     "rehashed invalid successor identifies wire offset");

        memcpy(mutated, bytes, length);
        put32(mutated + payloadOffset +
              TEST_EIS3_FIRST_PREDECESSOR_OFFSET, 2u);
        cfg_rehash(mutated, bundleOffset, payloadOffset,
                   payload->byteLength);
        cfg_require_codec_rejected(
                mutated + payloadOffset, payload->byteLength,
                ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                TEST_EIS3_FIRST_PREDECESSOR_OFFSET);
        require_rejected(mutated, length,
                         ZR_ARTIFACT_STATUS_INVALID_SECTION);

        memcpy(mutated, bytes, length);
        put32(mutated + payloadOffset +
              TEST_EIS3_BLOCK_COUNT_OFFSET, UINT32_MAX);
        cfg_rehash(mutated, bundleOffset, payloadOffset,
                   payload->byteLength);
        cfg_require_codec_rejected(
                mutated + payloadOffset, payload->byteLength,
                ZR_ARTIFACT_EXEC_IR_LIMIT,
                TEST_EIS3_BLOCK_COUNT_OFFSET);
        require_rejected(mutated, length,
                         ZR_ARTIFACT_STATUS_INVALID_SECTION);

        memcpy(mutated, bytes, length);
        put32(mutated + payloadOffset + TEST_EIS3_LENGTH_OFFSET,
              TEST_EIS3_ENCODED_SIZE + 4u);
        cfg_rehash(mutated, bundleOffset, payloadOffset,
                   payload->byteLength);
        cfg_require_codec_rejected(
                mutated + payloadOffset, payload->byteLength,
                ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                TEST_EIS3_LENGTH_OFFSET);
        require_rejected(mutated, length,
                         ZR_ARTIFACT_STATUS_INVALID_SECTION);

        memcpy(mutated, bytes, length);
        put32(mutated + payloadOffset +
              TEST_EIS3_ENTRY_SUCCESSOR_COUNT_OFFSET, 3u);
        cfg_rehash(mutated, bundleOffset, payloadOffset,
                   payload->byteLength);
        cfg_require_codec_rejected(
                mutated + payloadOffset, payload->byteLength,
                ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                TEST_EIS3_ENTRY_SUCCESSOR_COUNT_OFFSET);
        require_rejected(mutated, length,
                         ZR_ARTIFACT_STATUS_INVALID_SECTION);

        memcpy(mutated, bytes, length);
        put16(mutated + payloadOffset + 4u, 4u);
        cfg_rehash(mutated, bundleOffset, payloadOffset,
                   payload->byteLength);
        cfg_require_codec_rejected(
                mutated + payloadOffset, payload->byteLength,
                ZR_ARTIFACT_EXEC_IR_UNSUPPORTED_VERSION, 4u);
        require_rejected(mutated, length,
                         ZR_ARTIFACT_STATUS_UNSUPPORTED_VERSION);
        free(mutated);
        free(bytes);
        free(path);
    }
}
