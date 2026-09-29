/* EIS4 test-only extension for the fixed scalar ADD graph. */

#define TEST_EIS4_ENCODED_SIZE 716u
#define TEST_EIS4_LENGTH_OFFSET 8u
#define TEST_EIS4_COUNT_OFFSET 180u
#define TEST_EIS4_FIRST_CONSTANT_BITS_OFFSET 220u
#define TEST_EIS4_BLOCK_COUNT_OFFSET 188u
#define TEST_EIS4_ADD_INSTRUCTION_OFFSET 524u
#define TEST_EIS4_ADD_OPERAND_RANGE_OFFSET 536u
#define TEST_EIS4_ADD_OPERAND_COUNT_OFFSET 540u
#define TEST_EIS4_BLOCK_SUCCESSOR_COUNT_OFFSET 344u
#define TEST_EIS4_THIRD_RESULT_OFFSET 700u
#define TEST_EIS4_FIRST_OPERAND_OFFSET 704u

static char *add_artifact_path(const char *path) {
    size_t length = strlen(path);
    char *result;
    require_true(length >= 4u && strcmp(path + length - 4u, ".zro") == 0 &&
                 length <= SIZE_MAX - 12u, "bounded ADD artifact path");
    result = (char *)malloc(length + 12u);
    require_true(result != NULL, "allocate ADD artifact path");
    memcpy(result, path, length - 4u);
    strcpy(result + length - 4u, "-add.zro");
    return result;
}

static void add_append_instruction(SZrExecIrFunction *function,
                                   EZrExecIrOpcode opcode,
                                   SZrExecIrRange results,
                                   SZrExecIrRange operands,
                                   TZrUInt32 constantIndex,
                                   TZrExecIrInstructionId expectedId) {
    SZrExecIrInstruction instruction;
    TZrExecIrInstructionId actualId = 0u;
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (TZrUInt16)opcode;
    instruction.results = results;
    instruction.operands = operands;
    instruction.layoutId = constantIndex;
    instruction.sourceId = 300u + expectedId;
    require_true(ZrCore_ExecIr_FunctionAppendInstruction(
                 function, &instruction, &actualId) &&
                 actualId == expectedId, "append scalar ADD instruction");
}

static void add_build_graph(SZrExecIrModule *module) {
    SZrExecIrConstant constants[2] = {
        {ZR_VALUE_TYPE_INT64, 0u, 20u},
        {ZR_VALUE_TYPE_INT64, 0u, 22u}
    };
    TZrExecIrValueId values[3];
    TZrExecIrValueId addOperands[2];
    TZrExecIrFunctionId functionId = 0u;
    SZrExecIrFunction *function;
    SZrExecIrRange constantRange, results, operands;
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
    require_true(ZrCore_ExecIr_ModuleAppendConstant(module, constants, 2u,
                 &constantRange) && constantRange.start == 0u &&
                 constantRange.count == 2u, "append ADD constants");
    require_true(ZrCore_ExecIr_ModuleAddFunction(module, TEST_MEMBER,
                 TEST_SIGNATURE_HASH, &functionId) && functionId == 1u,
                 "add scalar ADD function");
    function = ZrCore_ExecIr_ModuleFunctionAt(module, functionId);
    require_true(function != ZR_NULL, "lookup scalar ADD function");
    function->contract.layoutHash = TEST_LAYOUT_HASH;
    function->contract.moduleHash = TEST_MODULE_HASH;
    require_true(ZrCore_ExecIr_FunctionAddBlock(
                 function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u,
                 "add scalar ADD entry block");
    for (TZrUInt32 index = 0u; index < 3u; ++index) {
        values[index] = ZrCore_ExecIr_FunctionAddValue(
                function, ZR_VALUE_TYPE_INT64,
                ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
                ZR_EXEC_IR_NULLABILITY_UNKNOWN);
        require_true(values[index] == index + 1u, "append ADD value");
    }

    require_true(ZrCore_ExecIr_FunctionAppendResults(
                 function, &values[0], 1u, &results) &&
                 results.start == 0u && results.count == 1u,
                 "append first constant result");
    add_append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT,
                           results, empty, 0u, 1u);
    require_true(ZrCore_ExecIr_FunctionAppendResults(
                 function, &values[1], 1u, &results) &&
                 results.start == 1u && results.count == 1u,
                 "append second constant result");
    add_append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT,
                           results, empty, 1u, 2u);
    addOperands[0] = values[0];
    addOperands[1] = values[1];
    require_true(ZrCore_ExecIr_FunctionAppendOperands(
                 function, addOperands, 2u, &operands) &&
                 operands.start == 0u && operands.count == 2u,
                 "append ordered ADD inputs");
    require_true(ZrCore_ExecIr_FunctionAppendResults(
                 function, &values[2], 1u, &results) &&
                 results.start == 2u && results.count == 1u,
                 "append ADD result");
    add_append_instruction(function, ZR_EXEC_IR_OPCODE_ADD,
                           results, operands, 0u, 3u);
    require_true(ZrCore_ExecIr_FunctionAppendOperands(
                 function, &values[2], 1u, &operands) &&
                 operands.start == 2u && operands.count == 1u,
                 "append ADD return operand");
    add_append_instruction(function, ZR_EXEC_IR_OPCODE_RETURN,
                           empty, operands, 0u, 4u);

    function->blocks[0].instructions =
            (SZrExecIrRange){.start = 0u, .count = 4u};
    function->blocks[0].terminatorInstructionId = 4u;
    require_true(ZrCore_ExecIr_VerifyModule(module, &diagnostic),
                 "verify scalar ADD graph before persistence");
}

static void add_assert_graph_and_oracle(const SZrExecIrModule *module) {
    const SZrExecIrFunction *function;
    SZrExecIrOracleValue constants[2];
    SZrExecIrOracleInput input;
    SZrExecIrOracleExecutionResult execution;
    SZrExecIrDiagnostic diagnostic;

    require_true(ZrCore_ExecIr_VerifyModule(module, &diagnostic),
                 "verify decoded scalar ADD graph");
    require_true(module->functionCount == 1u && module->constantCount == 2u &&
                 module->constants[0].bits == 20u &&
                 module->constants[1].bits == 22u,
                 "decoded scalar ADD constants");
    function = &module->functions[0];
    require_true(function->entryBlockId == 1u && function->blockCount == 1u &&
                 function->instructionCount == 4u && function->valueCount == 3u &&
                 function->resultCount == 3u && function->operandCount == 3u &&
                 function->successorCount == 0u &&
                 function->predecessorCount == 0u &&
                 function->values[0].definition == 1u &&
                 function->values[1].definition == 2u &&
                 function->values[2].definition == 3u &&
                 function->instructions[0].sourceId == 301u &&
                 function->instructions[1].sourceId == 302u &&
                 function->instructions[2].sourceId == 303u &&
                 function->instructions[3].sourceId == 304u &&
                 function->instructions[2].opcode == ZR_EXEC_IR_OPCODE_ADD &&
                 function->instructions[2].results.start == 2u &&
                 function->instructions[2].results.count == 1u &&
                 function->instructions[2].operands.start == 0u &&
                 function->instructions[2].operands.count == 2u &&
                 function->operands[0] == 1u && function->operands[1] == 2u &&
                 function->operands[2] == 3u &&
                 function->blocks[0].instructions.start == 0u &&
                 function->blocks[0].instructions.count == 4u &&
                 function->blocks[0].terminatorInstructionId == 4u,
                 "decoded exact scalar ADD shape and source IDs");

    memset(constants, 0, sizeof(constants));
    constants[0].kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    constants[0].as.signedInteger = 20;
    constants[1].kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    constants[1].as.signedInteger = 22;
    memset(&input, 0, sizeof(input));
    input.function = function;
    input.constants = constants;
    input.constantCount = 2u;
    ZrCore_ExecIr_OracleResultInit(&execution);
    require_true(ZrCore_ExecIr_RunOracleEx(&input, &execution, &diagnostic),
                 "execute scalar ADD graph in Oracle");
    require_true(execution.returned &&
                 execution.returnValue.kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED &&
                 execution.returnValue.as.signedInteger == 42,
                 "scalar ADD Oracle returns 42");
    ZrCore_ExecIr_OracleResultFree(&execution);
}

static void add_rehash(TZrByte *bytes, TZrSize bundleOffset,
                       TZrSize payloadOffset, TZrUInt32 payloadLength) {
    put64(bytes + bundleOffset + 20u,
          ZrCore_ArtifactExecIr_HashBytes(bytes + payloadOffset,
                                         payloadLength));
}

static void add_require_codec_rejected(const TZrByte *bytes,
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
                 "malformed EIS4 decoder rejects without publishing graph");
}

static void add_write_phase(const char *basePath) {
    SFixture fixture;
    SZrExecIrModule source;
    SZrExecIrModule decoded;
    SZrArtifactExecIrDiagnostic codecDiagnostic;
    SZrArtifactDiagnostic artifactDiagnostic;
    TZrByte payload[TEST_EIS4_ENCODED_SIZE];
    TZrByte unchanged[TEST_EIS4_ENCODED_SIZE];
    TZrUInt32 encodedSize = 0u;
    TZrSize fileLength, retainedLength;
    TZrByte *fileBytes, *retainedBytes;
    char *path = add_artifact_path(basePath);

    init_fixture(&fixture);
    add_build_graph(&source);
    add_assert_graph_and_oracle(&source);
    require_true(ZrCore_ArtifactExecIrScalar_GetEncodedSize(
                 &source, &encodedSize, &codecDiagnostic) ==
                     ZR_ARTIFACT_EXEC_IR_OK &&
                 encodedSize == TEST_EIS4_ENCODED_SIZE,
                 "EIS4 scalar ADD exact 716-byte payload size");
    memset(payload, 0xa5, sizeof(payload));
    require_true(ZrCore_ArtifactExecIrScalar_Write(
                 &source, payload, sizeof(payload), &codecDiagnostic) ==
                     ZR_ARTIFACT_EXEC_IR_OK &&
                 memcmp(payload, "EIS4", 4u) == 0 && payload[4] == 4u &&
                 payload[6] == 0u && payload[7] == 0u &&
                 ((TZrUInt32)payload[8] | ((TZrUInt32)payload[9] << 8u) |
                  ((TZrUInt32)payload[10] << 16u) |
                  ((TZrUInt32)payload[11] << 24u)) ==
                     TEST_EIS4_ENCODED_SIZE,
                 "encode counted EIS4 scalar ADD");
    ZrCore_ExecIr_ModuleInit(&decoded);
    require_true(ZrCore_ArtifactExecIrScalar_Read(
                 payload, sizeof(payload), &decoded, &codecDiagnostic) ==
                     ZR_ARTIFACT_EXEC_IR_OK,
                 "decode EIS4 scalar ADD directly");
    add_assert_graph_and_oracle(&decoded);
    ZrCore_ExecIr_FreeModule(&decoded);
    memcpy(unchanged, payload, sizeof(payload));
    source.constants[0].bits = 21u;
    encodedSize = 0xa5a5u;
    require_true(ZrCore_ArtifactExecIrScalar_GetEncodedSize(
                 &source, &encodedSize, &codecDiagnostic) ==
                     ZR_ARTIFACT_EXEC_IR_INVALID_SECTION &&
                 encodedSize == 0xa5a5u,
                 "reject EIS4 graph with a noncanonical first literal");
    require_true(ZrCore_ArtifactExecIrScalar_Write(
                 &source, payload, sizeof(payload), &codecDiagnostic) ==
                     ZR_ARTIFACT_EXEC_IR_INVALID_SECTION &&
                 memcmp(payload, unchanged, sizeof(payload)) == 0,
                 "noncanonical EIS4 literal preserves write destination");
    source.constants[0].bits = 20u;
    source.functions[0].instructions[2].bindingRow = 1u;
    require_true(ZrCore_ArtifactExecIrScalar_Write(
                 &source, payload, sizeof(payload), &codecDiagnostic) ==
                     ZR_ARTIFACT_EXEC_IR_INVALID_SECTION &&
                 memcmp(payload, unchanged, sizeof(payload)) == 0,
                 "rejected EIS4 encode preserves destination");
    source.functions[0].instructions[2].bindingRow = 0u;
    require_true(ZrParser_ExecIr_WriteCanonicalZroFile(
                 &fixture.metadata, &source, path, &artifactDiagnostic) ==
                     ZR_ARTIFACT_STATUS_OK,
                 "write EIS4 scalar ADD ZRAF file");
    fileBytes = read_file(path, &fileLength);
    source.functions[0].instructions[2].bindingRow = 1u;
    require_true(ZrParser_ExecIr_WriteCanonicalZroFile(
                 &fixture.metadata, &source, path, &artifactDiagnostic) ==
                     ZR_ARTIFACT_STATUS_INVALID_SECTION,
                 "reject unresolved EIS4 binding before publication");
    source.functions[0].instructions[2].bindingRow = 0u;
    retainedBytes = read_file(path, &retainedLength);
    require_true(fileLength == retainedLength &&
                 memcmp(fileBytes, retainedBytes, fileLength) == 0,
                 "rejected EIS4 rewrite preserves prior artifact");
    free(retainedBytes);
    free(fileBytes);
    ZrCore_ExecIr_FreeModule(&source);
    free(path);
}

static void add_read_phase(const char *basePath) {
    SFixture fixture;
    char *path = add_artifact_path(basePath);
    TZrSize length;
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
    TZrSize bundleOffset, payloadOffset;

    require_true(mutated != NULL, "allocate EIS4 mutation buffer");
    init_fixture(&fixture);
    ZrCore_ExecIr_ModuleInit(&module);
    require_true(ZrCore_Module_OpenExecIrArtifact(
                 bytes, length, &fixture.metadata.identity, &module,
                 &diagnostic) == ZR_ARTIFACT_STATUS_OK,
                 "open EIS4 scalar ADD artifact in separate process");
    add_assert_graph_and_oracle(&module);
    ZrCore_ExecIr_FreeModule(&module);

    require_true(ZrCore_Artifact_Read(bytes, length, &outer, &diagnostic) ==
                     ZR_ARTIFACT_STATUS_OK &&
                 ZrCore_Artifact_FindSection(
                     &outer, ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE,
                     &bundle, &diagnostic) == ZR_ARTIFACT_STATUS_OK &&
                 ZrCore_ArtifactExecIr_Read(bundle.data, bundle.byteLength,
                                            &nested, NULL) ==
                     ZR_ARTIFACT_EXEC_IR_OK &&
                 ZrCore_ArtifactExecIr_FindSection(
                     &nested, ZR_ARTIFACT_EXEC_IR_SECTION_EXEC_IR,
                     &payload, NULL) == ZR_ARTIFACT_EXEC_IR_OK &&
                 payload->byteLength == TEST_EIS4_ENCODED_SIZE &&
                 memcmp(payload->data, "EIS4", 4u) == 0,
                 "locate EIS4 payload inside ERI1 bundle");
    bundleOffset = (TZrSize)(bundle.data - bytes);
    payloadOffset = (TZrSize)(payload->data - bytes);

    add_require_codec_rejected(payload->data, payload->byteLength - 1u,
                               ZR_ARTIFACT_EXEC_IR_TRUNCATED, 0u);

    memcpy(mutated, bytes, length);
    put16(mutated + payloadOffset + 4u, 5u);
    add_rehash(mutated, bundleOffset, payloadOffset, payload->byteLength);
    add_require_codec_rejected(mutated + payloadOffset, payload->byteLength,
                               ZR_ARTIFACT_EXEC_IR_UNSUPPORTED_VERSION, 4u);
    diagnostic = require_rejected(mutated, length,
                                  ZR_ARTIFACT_STATUS_UNSUPPORTED_VERSION);
    require_true(diagnostic.byteOffset == payloadOffset + 4u,
                 "rejected EIS4 version reports payload offset");

    memcpy(mutated, bytes, length);
    put32(mutated + payloadOffset + TEST_EIS4_LENGTH_OFFSET,
          TEST_EIS4_ENCODED_SIZE + 4u);
    add_rehash(mutated, bundleOffset, payloadOffset, payload->byteLength);
    add_require_codec_rejected(mutated + payloadOffset, payload->byteLength,
                               ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                               TEST_EIS4_LENGTH_OFFSET);
    diagnostic = require_rejected(mutated, length,
                                  ZR_ARTIFACT_STATUS_INVALID_SECTION);
    require_true(diagnostic.byteOffset ==
                     payloadOffset + TEST_EIS4_LENGTH_OFFSET,
                 "rejected EIS4 declared length reports payload offset");

    memcpy(mutated, bytes, length);
    put64(mutated + payloadOffset + TEST_EIS4_FIRST_CONSTANT_BITS_OFFSET, 21u);
    add_rehash(mutated, bundleOffset, payloadOffset, payload->byteLength);
    add_require_codec_rejected(
            mutated + payloadOffset, payload->byteLength,
            ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
            TEST_EIS4_FIRST_CONSTANT_BITS_OFFSET);
    diagnostic = require_rejected(mutated, length,
                                  ZR_ARTIFACT_STATUS_INVALID_SECTION);
    require_true(diagnostic.byteOffset ==
                     payloadOffset + TEST_EIS4_FIRST_CONSTANT_BITS_OFFSET,
                 "rehashed EIS4 literal mutation reports payload offset");

    memcpy(mutated, bytes, length);
    put32(mutated + payloadOffset + TEST_EIS4_BLOCK_COUNT_OFFSET, UINT32_MAX);
    add_rehash(mutated, bundleOffset, payloadOffset, payload->byteLength);
    add_require_codec_rejected(mutated + payloadOffset, payload->byteLength,
                               ZR_ARTIFACT_EXEC_IR_LIMIT,
                               TEST_EIS4_BLOCK_COUNT_OFFSET);
    require_rejected(mutated, length, ZR_ARTIFACT_STATUS_INVALID_SECTION);

    memcpy(mutated, bytes, length);
    put32(mutated + payloadOffset + TEST_EIS4_BLOCK_SUCCESSOR_COUNT_OFFSET, 1u);
    add_rehash(mutated, bundleOffset, payloadOffset, payload->byteLength);
    add_require_codec_rejected(
            mutated + payloadOffset, payload->byteLength,
            ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
            TEST_EIS4_BLOCK_SUCCESSOR_COUNT_OFFSET);
    require_rejected(mutated, length, ZR_ARTIFACT_STATUS_INVALID_SECTION);

    memcpy(mutated, bytes, length);
    put16(mutated + payloadOffset + TEST_EIS4_ADD_INSTRUCTION_OFFSET, 0u);
    add_rehash(mutated, bundleOffset, payloadOffset, payload->byteLength);
    add_require_codec_rejected(mutated + payloadOffset, payload->byteLength,
                               ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                               TEST_EIS4_ADD_INSTRUCTION_OFFSET);
    require_rejected(mutated, length, ZR_ARTIFACT_STATUS_INVALID_SECTION);

    memcpy(mutated, bytes, length);
    put32(mutated + payloadOffset + TEST_EIS4_ADD_OPERAND_COUNT_OFFSET, 3u);
    add_rehash(mutated, bundleOffset, payloadOffset, payload->byteLength);
    add_require_codec_rejected(mutated + payloadOffset, payload->byteLength,
                               ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                               TEST_EIS4_ADD_OPERAND_RANGE_OFFSET);
    require_rejected(mutated, length, ZR_ARTIFACT_STATUS_INVALID_SECTION);

    memcpy(mutated, bytes, length);
    put32(mutated + payloadOffset + TEST_EIS4_THIRD_RESULT_OFFSET, 0u);
    add_rehash(mutated, bundleOffset, payloadOffset, payload->byteLength);
    add_require_codec_rejected(mutated + payloadOffset, payload->byteLength,
                               ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                               TEST_EIS4_THIRD_RESULT_OFFSET);
    require_rejected(mutated, length, ZR_ARTIFACT_STATUS_INVALID_SECTION);

    memcpy(mutated, bytes, length);
    put32(mutated + payloadOffset + 6u, 1u);
    add_rehash(mutated, bundleOffset, payloadOffset, payload->byteLength);
    add_require_codec_rejected(mutated + payloadOffset, payload->byteLength,
                               ZR_ARTIFACT_EXEC_IR_INVALID_SECTION, 6u);
    require_rejected(mutated, length, ZR_ARTIFACT_STATUS_INVALID_SECTION);

    memcpy(mutated, bytes, length);
    put16(mutated + payloadOffset + TEST_EIS4_ADD_INSTRUCTION_OFFSET, 0u);
    memset(&rejectedView, 0xa5, sizeof(rejectedView));
    require_true(ZrCore_ArtifactExecIr_Read(
                 mutated + bundleOffset, bundle.byteLength, &rejectedView,
                 &nestedDiagnostic) == ZR_ARTIFACT_EXEC_IR_HASH_MISMATCH &&
                 rejectedView.buffer == ZR_NULL &&
                 rejectedView.sectionCount == 0u,
                 "stale EIS4 hash rejects bundle without a view");
    require_rejected(mutated, length, ZR_ARTIFACT_STATUS_INVALID_SECTION);

    free(mutated);
    free(bytes);
    free(path);
}
