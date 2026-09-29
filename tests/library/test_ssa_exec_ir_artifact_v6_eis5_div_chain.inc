/* EIS5 v5 counted CFG tests for a two-step signed i64 DIV effect chain. */
#define TEST_EIS5_DIV_CHAIN_ENCODED_SIZE 964u
#define TEST_EIS5_DIV_CHAIN_SECOND_EFFECT_IN_OFFSET 820u
#define TEST_EIS5_DIV_CHAIN_SECOND_EFFECT_OUT_OFFSET 824u

static char *eis5_div_chain_artifact_path(const char *basePath) {
    TZrSize capacity = strlen(basePath) + 40u;
    char *path = (char *)malloc(capacity);
    require_true(path != ZR_NULL, "allocate EIS5 DIV chain artifact path");
    snprintf(path, capacity, "%s.eis5-div-chain.zro", basePath);
    return path;
}

static void eis5_div_chain_build_graph(SZrExecIrModule *module) {
    SZrExecIrConstant constants[3] = {
        {ZR_VALUE_TYPE_INT64, 0u, 84u},
        {ZR_VALUE_TYPE_INT64, 0u, (TZrUInt64)-7},
        {ZR_VALUE_TYPE_INT64, 0u, 3u}
    };
    TZrExecIrValueId values[5];
    TZrExecIrValueId operands[2];
    TZrExecIrFunctionId functionId = 0u;
    SZrExecIrFunction *function;
    SZrExecIrRange range = {.start = 0u, .count = 0u};
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
                                                    &range) &&
                 range.start == 0u && range.count == 3u,
                 "append EIS5 DIV chain constants");
    require_true(ZrCore_ExecIr_ModuleAddFunction(
                         module, TEST_MEMBER, TEST_SIGNATURE_HASH,
                         &functionId) && functionId == 1u,
                 "add EIS5 DIV chain function");
    function = ZrCore_ExecIr_ModuleFunctionAt(module, functionId);
    require_true(function != ZR_NULL, "lookup EIS5 DIV chain function");
    function->contract.layoutHash = TEST_LAYOUT_HASH;
    function->contract.moduleHash = TEST_MODULE_HASH;
    require_true(ZrCore_ExecIr_FunctionAddBlock(
                         function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u,
                 "add EIS5 DIV chain entry block");
    for (TZrUInt32 index = 0u; index < 5u; ++index) {
        values[index] = ZrCore_ExecIr_FunctionAddValue(
                function, ZR_VALUE_TYPE_INT64,
                ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
                ZR_EXEC_IR_NULLABILITY_UNKNOWN);
        require_true(values[index] == index + 1u,
                     "append EIS5 DIV chain value");
    }
    for (TZrUInt32 index = 0u; index < 3u; ++index) {
        require_true(ZrCore_ExecIr_FunctionAppendResults(
                             function, &values[index], 1u, &range) &&
                     range.start == index && range.count == 1u,
                     "append EIS5 DIV chain constant result");
        eis5_append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT,
                                range, empty, empty, index, index + 1u);
    }

    operands[0] = values[0];
    operands[1] = values[1];
    require_true(ZrCore_ExecIr_FunctionAppendOperands(
                         function, operands, 2u, &range) &&
                 range.start == 0u && range.count == 2u,
                 "append first ordered EIS5 DIV chain operands");
    require_true(ZrCore_ExecIr_FunctionAppendResults(
                         function, &values[3], 1u, &range) &&
                 range.start == 3u && range.count == 1u,
                 "append first EIS5 DIV chain result");
    eis5_append_instruction(function, ZR_EXEC_IR_OPCODE_DIV, range,
                            (SZrExecIrRange){.start = 0u, .count = 2u},
                            empty, 0u, 4u);
    function->instructions[3].flags = ZR_EXEC_IR_FLAG_MAY_THROW;
    function->instructions[3].effectIn = 1u;
    function->instructions[3].effectOut = 2u;

    operands[0] = values[3];
    operands[1] = values[2];
    require_true(ZrCore_ExecIr_FunctionAppendOperands(
                         function, operands, 2u, &range) &&
                 range.start == 2u && range.count == 2u,
                 "append second ordered EIS5 DIV chain operands");
    require_true(ZrCore_ExecIr_FunctionAppendResults(
                         function, &values[4], 1u, &range) &&
                 range.start == 4u && range.count == 1u,
                 "append second EIS5 DIV chain result");
    eis5_append_instruction(function, ZR_EXEC_IR_OPCODE_DIV, range,
                            (SZrExecIrRange){.start = 2u, .count = 2u},
                            empty, 0u, 5u);
    function->instructions[4].flags = ZR_EXEC_IR_FLAG_MAY_THROW;
    function->instructions[4].effectIn = 2u;
    function->instructions[4].effectOut = 3u;

    require_true(ZrCore_ExecIr_FunctionAppendOperands(
                         function, &values[4], 1u, &range) &&
                 range.start == 4u && range.count == 1u,
                 "append EIS5 DIV chain return operand");
    eis5_append_instruction(function, ZR_EXEC_IR_OPCODE_RETURN,
                            empty, range, empty, 0u, 6u);
    function->blocks[0].instructions =
            (SZrExecIrRange){.start = 0u, .count = 6u};
    function->blocks[0].terminatorInstructionId = 6u;

    if (!ZrCore_ExecIr_VerifyModule(module, &diagnostic)) {
        fprintf(stderr,
                "EIS5 DIV chain fixture verifier: code=%u block=%u "
                "instruction=%u expected=%u actual=%u\n",
                (unsigned)diagnostic.code, diagnostic.blockId,
                diagnostic.instructionId, diagnostic.expectedVersion,
                diagnostic.actualVersion);
    }
    require_true(ZrCore_ExecIr_VerifyModule(module, &diagnostic),
                 "VerifyModule accepts 1-to-2-to-3 DIV effect chain");
}

static void eis5_div_chain_assert_graph_and_oracle(
        const SZrExecIrModule *module) {
    const SZrExecIrFunction *function;
    SZrExecIrOracleValue constants[3];
    SZrExecIrOracleInput input;
    SZrExecIrOracleExecutionResult execution;
    SZrExecIrDiagnostic diagnostic;

    require_true(ZrCore_ExecIr_VerifyModule(module, &diagnostic),
                 "Verify decoded EIS5 DIV chain graph");
    require_true(module->constantCount == 3u && module->functionCount == 1u &&
                 module->constants[0].bits == 84u &&
                 module->constants[1].bits == (TZrUInt64)-7 &&
                 module->constants[2].bits == 3u,
                 "preserve EIS5 DIV chain signed constants");
    function = &module->functions[0];
    require_true(function->entryBlockId == 1u && function->blockCount == 1u &&
                 function->instructionCount == 6u && function->valueCount == 5u &&
                 function->resultCount == 5u && function->operandCount == 5u &&
                 function->successorCount == 0u &&
                 function->predecessorCount == 0u &&
                 function->instructions[3].opcode == ZR_EXEC_IR_OPCODE_DIV &&
                 function->instructions[3].flags == ZR_EXEC_IR_FLAG_MAY_THROW &&
                 function->instructions[3].effectIn == 1u &&
                 function->instructions[3].effectOut == 2u &&
                 function->instructions[4].opcode == ZR_EXEC_IR_OPCODE_DIV &&
                 function->instructions[4].flags == ZR_EXEC_IR_FLAG_MAY_THROW &&
                 function->instructions[4].effectIn == 2u &&
                 function->instructions[4].effectOut == 3u &&
                 function->blocks[0].instructions.start == 0u &&
                 function->blocks[0].instructions.count == 6u &&
                 function->blocks[0].terminatorInstructionId == 6u,
                 "preserve exact EIS5 DIV chain shape and effect tokens");

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
                 "run two-step EIS5 DIV chain through Oracle");
    require_true(execution.returned &&
                 execution.returnValue.kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED &&
                 execution.returnValue.as.signedInteger == -4,
                 "EIS5 Oracle computes (84 / -7) / 3 = -4");
    ZrCore_ExecIr_OracleResultFree(&execution);
}

static void eis5_div_chain_require_codec_rejected(
        const TZrByte *bytes, TZrUInt32 length,
        EZrArtifactExecIrStatus expected, TZrUInt32 expectedOffset,
        const char *label) {
    SZrExecIrModule output;
    SZrArtifactExecIrDiagnostic diagnostic;
    EZrArtifactExecIrStatus actual;

    ZrCore_ExecIr_ModuleInit(&output);
    actual = ZrCore_ArtifactExecIrScalar_Read(bytes, length, &output,
                                               &diagnostic);
    if (actual != expected || diagnostic.byteOffset != expectedOffset)
        fprintf(stderr,
                "%s: status=%u expected=%u offset=%u expectedOffset=%u\n",
                label, (unsigned)actual, (unsigned)expected,
                (unsigned)diagnostic.byteOffset, (unsigned)expectedOffset);
    require_true(actual == expected, label);
    require_true(diagnostic.byteOffset == expectedOffset,
                 "malformed EIS5 DIV chain codec byte offset");
    require_true(output.functionCount == 0u && output.functions == ZR_NULL &&
                 output.constantCount == 0u && output.constants == ZR_NULL &&
                 output.layoutCount == 0u && output.layouts == ZR_NULL &&
                 output.sourceMapCount == 0u && output.sourceMaps == ZR_NULL,
                 "rejected EIS5 DIV chain publishes no partial graph");
}

static void eis5_div_chain_write_phase(const char *basePath) {
    SFixture fixture;
    SZrExecIrModule source;
    SZrExecIrModule decoded;
    SZrArtifactExecIrDiagnostic codecDiagnostic;
    SZrArtifactDiagnostic artifactDiagnostic;
    TZrUInt32 encodedSize = UINT32_MAX;
    TZrByte *payload;
    TZrByte expectedPayload[TEST_EIS5_DIV_CHAIN_ENCODED_SIZE];
    TZrSize originalLength;
    TZrByte *original;
    TZrSize retainedLength;
    TZrByte *retained;
    char *path;
    EZrArtifactExecIrStatus sizeStatus;

    init_fixture(&fixture);
    eis5_div_chain_build_graph(&source);
    eis5_div_chain_assert_graph_and_oracle(&source);
    sizeStatus = ZrCore_ArtifactExecIrScalar_GetEncodedSize(
            &source, &encodedSize, &codecDiagnostic);
    if (sizeStatus != ZR_ARTIFACT_EXEC_IR_OK ||
        encodedSize != TEST_EIS5_DIV_CHAIN_ENCODED_SIZE)
        fprintf(stderr,
                "EIS5 two-DIV effect-chain size query: status=%u offset=%u "
                "size=%u expectedSize=%u\n",
                (unsigned)sizeStatus, (unsigned)codecDiagnostic.byteOffset,
                (unsigned)encodedSize,
                (unsigned)TEST_EIS5_DIV_CHAIN_ENCODED_SIZE);
    require_true(sizeStatus == ZR_ARTIFACT_EXEC_IR_OK &&
                 encodedSize == TEST_EIS5_DIV_CHAIN_ENCODED_SIZE,
                 "EIS5 two-DIV effect-chain dynamic size query");

    payload = (TZrByte *)malloc(encodedSize);
    require_true(payload != ZR_NULL, "allocate EIS5 DIV chain payload");
    require_true(ZrCore_ArtifactExecIrScalar_Write(
                         &source, payload, encodedSize, &codecDiagnostic) ==
                         ZR_ARTIFACT_EXEC_IR_OK &&
                 memcmp(payload, "EIS5", 4u) == 0,
                 "write EIS5 DIV chain payload");
    memcpy(expectedPayload, payload, sizeof(expectedPayload));
    ZrCore_ExecIr_ModuleInit(&decoded);
    require_true(ZrCore_ArtifactExecIrScalar_Read(
                         payload, encodedSize, &decoded, &codecDiagnostic) ==
                         ZR_ARTIFACT_EXEC_IR_OK,
                 "directly read EIS5 DIV chain payload");
    eis5_div_chain_assert_graph_and_oracle(&decoded);
    ZrCore_ExecIr_FreeModule(&decoded);

    path = eis5_div_chain_artifact_path(basePath);
    require_true(ZrParser_ExecIr_WriteCanonicalZroFile(
                         &fixture.metadata, &source, path,
                         &artifactDiagnostic) == ZR_ARTIFACT_STATUS_OK,
                 "write canonical EIS5 DIV chain artifact");
    original = read_file(path, &originalLength);

    source.functions[0].instructions[4].effectIn = 1u;
    require_true(ZrCore_ArtifactExecIrScalar_GetEncodedSize(
                         &source, &encodedSize, &codecDiagnostic) ==
                         ZR_ARTIFACT_EXEC_IR_INVALID_SECTION &&
                 codecDiagnostic.byteOffset ==
                         TEST_EIS5_DIV_CHAIN_SECOND_EFFECT_IN_OFFSET,
                 "writer rejects a broken second DIV effect input");
    require_true(ZrCore_ArtifactExecIrScalar_Write(
                         &source, payload, TEST_EIS5_DIV_CHAIN_ENCODED_SIZE,
                         &codecDiagnostic) ==
                         ZR_ARTIFACT_EXEC_IR_INVALID_SECTION &&
                 codecDiagnostic.byteOffset ==
                         TEST_EIS5_DIV_CHAIN_SECOND_EFFECT_IN_OFFSET &&
                 memcmp(payload, expectedPayload, sizeof(expectedPayload)) == 0,
                 "failed second DIV write preserves destination bytes");
    require_true(ZrParser_ExecIr_WriteCanonicalZroFile(
                         &fixture.metadata, &source, path,
                         &artifactDiagnostic) == ZR_ARTIFACT_STATUS_INVALID_SECTION,
                 "canonical writer rejects a restarted second DIV effect chain");
    retained = read_file(path, &retainedLength);
    require_true(retainedLength == originalLength &&
                 memcmp(retained, original, originalLength) == 0,
                 "rejected DIV chain rewrite preserves canonical artifact");
    free(retained);

    source.functions[0].instructions[4].effectIn = 2u;
    source.functions[0].instructions[4].effectOut = 4u;
    require_true(ZrCore_ArtifactExecIrScalar_GetEncodedSize(
                         &source, &encodedSize, &codecDiagnostic) ==
                         ZR_ARTIFACT_EXEC_IR_INVALID_SECTION &&
                 codecDiagnostic.byteOffset ==
                         TEST_EIS5_DIV_CHAIN_SECOND_EFFECT_OUT_OFFSET,
                 "writer rejects a broken second DIV effect output");
    source.functions[0].instructions[4].effectOut = 3u;
    free(original);
    free(path);
    free(payload);
    ZrCore_ExecIr_FreeModule(&source);
}

static void eis5_div_chain_read_phase(const char *basePath) {
    SFixture fixture;
    char *path = eis5_div_chain_artifact_path(basePath);
    TZrSize length;
    TZrSize bundleOffset, payloadOffset;
    TZrByte *bytes = read_file(path, &length);
    TZrByte *mutated = (TZrByte *)malloc(length);
    SZrExecIrModule module;
    SZrArtifactDiagnostic diagnostic;
    SZrArtifactExecIrDiagnostic nestedDiagnostic = {0};
    SZrArtifactView outer = {0};
    SZrArtifactSectionView bundle = {0};
    SZrArtifactExecIrView nested = {0};
    const SZrArtifactExecIrSectionView *payload = ZR_NULL;

    require_true(mutated != ZR_NULL, "allocate EIS5 DIV chain mutations");
    init_fixture(&fixture);
    ZrCore_ExecIr_ModuleInit(&module);
    require_true(ZrCore_Module_OpenExecIrArtifact(
                         bytes, length, &fixture.metadata.identity, &module,
                         &diagnostic) == ZR_ARTIFACT_STATUS_OK,
                 "open canonical EIS5 DIV chain in separate process");
    eis5_div_chain_assert_graph_and_oracle(&module);
    ZrCore_ExecIr_FreeModule(&module);
    require_true(ZrCore_Artifact_Read(bytes, length, &outer, &diagnostic) ==
                         ZR_ARTIFACT_STATUS_OK &&
                 ZrCore_Artifact_FindSection(
                         &outer, ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE,
                         &bundle, &diagnostic) == ZR_ARTIFACT_STATUS_OK &&
                 ZrCore_ArtifactExecIr_Read(
                         bundle.data, bundle.byteLength, &nested,
                         &nestedDiagnostic) == ZR_ARTIFACT_EXEC_IR_OK &&
                 ZrCore_ArtifactExecIr_FindSection(
                         &nested, ZR_ARTIFACT_EXEC_IR_SECTION_EXEC_IR,
                         &payload, &nestedDiagnostic) == ZR_ARTIFACT_EXEC_IR_OK &&
                 payload->byteLength == TEST_EIS5_DIV_CHAIN_ENCODED_SIZE &&
                 memcmp(payload->data, "EIS5", 4u) == 0,
                 "find canonical EIS5 DIV chain payload");
    bundleOffset = (TZrSize)(bundle.data - bytes);
    payloadOffset = (TZrSize)(payload->data - bytes);

    memcpy(mutated, bytes, length);
    put32(mutated + payloadOffset +
                  TEST_EIS5_DIV_CHAIN_SECOND_EFFECT_IN_OFFSET, 1u);
    eis5_rehash(mutated, bundleOffset, payloadOffset, payload->byteLength);
    eis5_div_chain_require_codec_rejected(
            mutated + payloadOffset, payload->byteLength,
            ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
            TEST_EIS5_DIV_CHAIN_SECOND_EFFECT_IN_OFFSET,
            "reader rejects a second DIV that restarts the effect chain");
    diagnostic = require_rejected(mutated, length,
                                  ZR_ARTIFACT_STATUS_INVALID_SECTION);
    require_true(diagnostic.sectionKind ==
                         ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE &&
                 diagnostic.byteOffset == payloadOffset +
                         TEST_EIS5_DIV_CHAIN_SECOND_EFFECT_IN_OFFSET,
                 "opener reports second DIV effect-input byte offset");

    memcpy(mutated, bytes, length);
    put32(mutated + payloadOffset +
                  TEST_EIS5_DIV_CHAIN_SECOND_EFFECT_OUT_OFFSET, 4u);
    eis5_rehash(mutated, bundleOffset, payloadOffset, payload->byteLength);
    eis5_div_chain_require_codec_rejected(
            mutated + payloadOffset, payload->byteLength,
            ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
            TEST_EIS5_DIV_CHAIN_SECOND_EFFECT_OUT_OFFSET,
            "reader rejects a noncanonical second DIV effect output");
    diagnostic = require_rejected(mutated, length,
                                  ZR_ARTIFACT_STATUS_INVALID_SECTION);
    require_true(diagnostic.sectionKind ==
                         ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE &&
                 diagnostic.byteOffset == payloadOffset +
                         TEST_EIS5_DIV_CHAIN_SECOND_EFFECT_OUT_OFFSET,
                 "opener reports second DIV effect-output byte offset");

    free(mutated);
    free(bytes);
    free(path);
}
