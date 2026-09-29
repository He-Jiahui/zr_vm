/* EIS5 v5 counted CFG tests for the narrow scalar SUB extension. */
#define TEST_EIS5_SUB_ENCODED_SIZE 716u
#define TEST_EIS5_SUB_FIRST_CONSTANT_TYPE_OFFSET 212u
#define TEST_EIS5_SUB_FIRST_CONSTANT_BITS_OFFSET 220u
#define TEST_EIS5_SUB_FIRST_VALUE_RECORD_OFFSET 244u
#define TEST_EIS5_SUB_FIRST_VALUE_TYPE_OFFSET 252u
#define TEST_EIS5_SUB_RESULT_VALUE_RECORD_OFFSET 292u
#define TEST_EIS5_SUB_RESULT_VALUE_TYPE_OFFSET 300u
#define TEST_EIS5_SUB_INSTRUCTIONS_OFFSET 356u
#define TEST_EIS5_SUB_OPCODE_OFFSET 524u

static void eis5_sub_require_codec_rejected(
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
        fprintf(stderr, "%s: status=%u expected=%u offset=%u expectedOffset=%u\n",
                label, (unsigned)actual, (unsigned)expected,
                (unsigned)diagnostic.byteOffset, (unsigned)expectedOffset);
    require_true(actual == expected, label);
    require_true(diagnostic.byteOffset == expectedOffset,
                 "malformed EIS5 SUB codec byte offset");
    require_true(output.functionCount == 0u && output.functions == ZR_NULL &&
                 output.constantCount == 0u && output.constants == ZR_NULL &&
                 output.layoutCount == 0u && output.layouts == ZR_NULL &&
                 output.sourceMapCount == 0u && output.sourceMaps == ZR_NULL,
                 "rejected EIS5 SUB payload publishes no graph");
}

static char *eis5_sub_artifact_path(const char *basePath) {
    TZrSize capacity = strlen(basePath) + 32u;
    char *path = (char *)malloc(capacity);
    require_true(path != ZR_NULL, "allocate EIS5 SUB artifact path");
    snprintf(path, capacity, "%s.eis5-sub.zro", basePath);
    return path;
}

static void eis5_sub_build_graph(SZrExecIrModule *module) {
    SZrExecIrConstant constants[2] = {
        {ZR_VALUE_TYPE_INT64, 0u, (TZrUInt64)-11},
        {ZR_VALUE_TYPE_INT64, 0u, 4u}
    };
    TZrExecIrValueId values[3];
    TZrExecIrValueId subOperands[2];
    TZrExecIrFunctionId functionId = 0u;
    SZrExecIrFunction *function;
    SZrExecIrRange constantsRange = {.start = 0u, .count = 0u};
    SZrExecIrRange result = {.start = 0u, .count = 0u};
    SZrExecIrRange operands = {.start = 0u, .count = 0u};
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
    require_true(ZrCore_ExecIr_ModuleAppendConstant(
                 module, constants, 2u, &constantsRange) &&
                 constantsRange.start == 0u && constantsRange.count == 2u,
                 "append EIS5 SUB constants");
    require_true(ZrCore_ExecIr_ModuleAddFunction(
                 module, TEST_MEMBER, TEST_SIGNATURE_HASH, &functionId) &&
                 functionId == 1u,
                 "add EIS5 SUB function");
    function = ZrCore_ExecIr_ModuleFunctionAt(module, functionId);
    require_true(function != ZR_NULL, "lookup EIS5 SUB function");
    function->contract.layoutHash = TEST_LAYOUT_HASH;
    function->contract.moduleHash = TEST_MODULE_HASH;
    require_true(ZrCore_ExecIr_FunctionAddBlock(
                 function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u,
                 "add EIS5 SUB entry block");
    for (TZrUInt32 index = 0u; index < 3u; ++index) {
        values[index] = ZrCore_ExecIr_FunctionAddValue(
                function, ZR_VALUE_TYPE_INT64,
                ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
                ZR_EXEC_IR_NULLABILITY_UNKNOWN);
        require_true(values[index] == index + 1u,
                     "append EIS5 SUB value");
    }

    for (TZrUInt32 index = 0u; index < 2u; ++index) {
        require_true(ZrCore_ExecIr_FunctionAppendResults(
                     function, &values[index], 1u, &result) &&
                     result.start == index && result.count == 1u,
                     "append EIS5 SUB constant result");
        eis5_append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT,
                                result, empty, empty, index, index + 1u);
    }
    subOperands[0] = values[0];
    subOperands[1] = values[1];
    require_true(ZrCore_ExecIr_FunctionAppendOperands(
                 function, subOperands, 2u, &operands) &&
                 operands.start == 0u && operands.count == 2u,
                 "append ordered EIS5 SUB operands");
    require_true(ZrCore_ExecIr_FunctionAppendResults(
                 function, &values[2], 1u, &result) &&
                 result.start == 2u && result.count == 1u,
                 "append EIS5 SUB result");
    eis5_append_instruction(function, ZR_EXEC_IR_OPCODE_SUB,
                            result, operands, empty, 0u, 3u);
    require_true(ZrCore_ExecIr_FunctionAppendOperands(
                 function, &values[2], 1u, &operands) &&
                 operands.start == 2u && operands.count == 1u,
                 "append EIS5 SUB return operand");
    eis5_append_instruction(function, ZR_EXEC_IR_OPCODE_RETURN,
                            empty, operands, empty, 0u, 4u);
    function->blocks[0].instructions =
            (SZrExecIrRange){.start = 0u, .count = 4u};
    function->blocks[0].terminatorInstructionId = 4u;
    require_true(ZrCore_ExecIr_VerifyModule(module, &diagnostic),
                 "VerifyModule accepts one-block i64 SUB graph");
}

static void eis5_sub_assert_graph_and_oracle(
        const SZrExecIrModule *module) {
    const SZrExecIrFunction *function;
    SZrExecIrOracleValue constants[2];
    SZrExecIrOracleInput input;
    SZrExecIrOracleExecutionResult execution;
    SZrExecIrDiagnostic diagnostic;

    require_true(ZrCore_ExecIr_VerifyModule(module, &diagnostic),
                 "Verify decoded EIS5 SUB graph");
    require_true(module->constantCount == 2u && module->functionCount == 1u &&
                 module->constants[0].bits == (TZrUInt64)-11 &&
                 module->constants[1].bits == 4u,
                 "preserve signed EIS5 SUB constants");
    function = &module->functions[0];
    require_true(function->entryBlockId == 1u && function->blockCount == 1u &&
                 function->instructionCount == 4u && function->valueCount == 3u &&
                 function->resultCount == 3u && function->operandCount == 3u &&
                 function->successorCount == 0u &&
                 function->predecessorCount == 0u &&
                 function->values[0].typeToken == ZR_VALUE_TYPE_INT64 &&
                 function->values[1].typeToken == ZR_VALUE_TYPE_INT64 &&
                 function->values[2].typeToken == ZR_VALUE_TYPE_INT64 &&
                 function->instructions[2].opcode == ZR_EXEC_IR_OPCODE_SUB &&
                 function->instructions[2].results.start == 2u &&
                 function->instructions[2].results.count == 1u &&
                 function->instructions[2].operands.start == 0u &&
                 function->instructions[2].operands.count == 2u &&
                 function->operands[0] == 1u && function->operands[1] == 2u &&
                 function->operands[2] == 3u &&
                 function->blocks[0].instructions.start == 0u &&
                 function->blocks[0].instructions.count == 4u &&
                 function->blocks[0].terminatorInstructionId == 4u,
                 "preserve exact EIS5 SUB shape and i64 types");
    memset(constants, 0, sizeof(constants));
    for (TZrUInt32 index = 0u; index < 2u; ++index) {
        constants[index].kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
        constants[index].as.signedInteger =
                (TZrInt64)module->constants[index].bits;
    }
    memset(&input, 0, sizeof(input));
    input.function = function;
    input.constants = constants;
    input.constantCount = 2u;
    ZrCore_ExecIr_OracleResultInit(&execution);
    require_true(ZrCore_ExecIr_RunOracleEx(&input, &execution, &diagnostic),
                 "run EIS5 SUB graph through Oracle");
    require_true(execution.returned &&
                 execution.returnValue.kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED &&
                 execution.returnValue.as.signedInteger == -15,
                 "EIS5 Oracle computes -11 minus 4, distinct from ADD's -7");
    ZrCore_ExecIr_OracleResultFree(&execution);
}

static void eis5_sub_require_writer_rejected(
        const SZrExecIrModule *module, TZrByte *payload,
        TZrUInt32 expectedOffset, const char *label) {
    TZrByte unchanged[TEST_EIS5_SUB_ENCODED_SIZE];
    TZrUInt32 encodedSize = TEST_EIS5_SUB_ENCODED_SIZE;
    SZrArtifactExecIrDiagnostic diagnostic;

    require_true(ZrCore_ArtifactExecIrScalar_GetEncodedSize(
                 module, &encodedSize, &diagnostic) ==
                         ZR_ARTIFACT_EXEC_IR_INVALID_SECTION &&
                 diagnostic.byteOffset == expectedOffset,
                 label);
    memcpy(unchanged, payload, sizeof(unchanged));
    require_true(ZrCore_ArtifactExecIrScalar_Write(
                 module, payload, TEST_EIS5_SUB_ENCODED_SIZE, &diagnostic) ==
                         ZR_ARTIFACT_EXEC_IR_INVALID_SECTION &&
                 diagnostic.byteOffset == expectedOffset &&
                 memcmp(payload, unchanged, sizeof(unchanged)) == 0,
                 "rejected EIS5 SUB write preserves destination bytes");
}

static void eis5_sub_require_canonical_write_rejected(
        const SFixture *fixture, const SZrExecIrModule *module,
        const char *path, const TZrByte *original, TZrSize originalLength,
        const char *label) {
    TZrSize retainedLength;
    TZrByte *retained;
    SZrArtifactDiagnostic diagnostic;

    require_true(ZrParser_ExecIr_WriteCanonicalZroFile(
                 &fixture->metadata, module, path, &diagnostic) ==
                         ZR_ARTIFACT_STATUS_INVALID_SECTION,
                 label);
    retained = read_file(path, &retainedLength);
    require_true(retainedLength == originalLength &&
                 memcmp(retained, original, originalLength) == 0,
                 "rejected EIS5 SUB canonical write preserves file");
    free(retained);
}

static void eis5_sub_write_phase(const char *basePath) {
    SFixture fixture;
    SZrExecIrModule source;
    SZrExecIrModule decoded;
    SZrArtifactExecIrDiagnostic codecDiagnostic;
    SZrArtifactDiagnostic artifactDiagnostic;
    TZrUInt32 encodedSize = UINT32_MAX;
    TZrByte *payload;
    TZrSize originalLength;
    TZrByte *original;
    char *path;

    init_fixture(&fixture);
    eis5_sub_build_graph(&source);
    eis5_sub_assert_graph_and_oracle(&source);
    require_true(ZrCore_ArtifactExecIrScalar_GetEncodedSize(
                 &source, &encodedSize, &codecDiagnostic) ==
                         ZR_ARTIFACT_EXEC_IR_OK &&
                 encodedSize == TEST_EIS5_SUB_ENCODED_SIZE,
                 "EIS5 SUB dynamic size query shares the EIS4 716-byte tuple");
    payload = (TZrByte *)malloc(encodedSize);
    require_true(payload != ZR_NULL, "allocate EIS5 SUB payload");
    memset(payload, 0xa5, encodedSize);
    require_true(ZrCore_ArtifactExecIrScalar_Write(
                 &source, payload, encodedSize, &codecDiagnostic) ==
                         ZR_ARTIFACT_EXEC_IR_OK &&
                 memcmp(payload, "EIS5", 4u) == 0 && payload[4] == 5u &&
                 payload[5] == 0u && payload[6] == 0u && payload[7] == 0u &&
                 eis5_get32(payload + TEST_EIS5_TOTAL_LENGTH_OFFSET) ==
                         TEST_EIS5_SUB_ENCODED_SIZE,
                 "write EIS5 v5 SUB payload despite EIS4 length collision");
    ZrCore_ExecIr_ModuleInit(&decoded);
    require_true(ZrCore_ArtifactExecIrScalar_Read(
                 payload, encodedSize, &decoded, &codecDiagnostic) ==
                         ZR_ARTIFACT_EXEC_IR_OK,
                 "directly read EIS5 SUB payload");
    eis5_sub_assert_graph_and_oracle(&decoded);
    ZrCore_ExecIr_FreeModule(&decoded);

    path = eis5_sub_artifact_path(basePath);
    require_true(ZrParser_ExecIr_WriteCanonicalZroFile(
                 &fixture.metadata, &source, path, &artifactDiagnostic) ==
                         ZR_ARTIFACT_STATUS_OK,
                 "write canonical EIS5 SUB artifact");
    original = read_file(path, &originalLength);

    source.constants[0].typeToken = ZR_VALUE_TYPE_BOOL;
    source.constants[0].bits = 1u;
    source.functions[0].values[0].typeToken = ZR_VALUE_TYPE_BOOL;
    eis5_sub_require_writer_rejected(
            &source, payload, TEST_EIS5_SUB_FIRST_VALUE_RECORD_OFFSET,
            "writer rejects non-i64 EIS5 SUB input at value record");
    eis5_sub_require_canonical_write_rejected(
            &fixture, &source, path, original, originalLength,
            "canonical writer rejects non-i64 EIS5 SUB input");
    source.constants[0].typeToken = ZR_VALUE_TYPE_INT64;
    source.constants[0].bits = (TZrUInt64)-11;
    source.functions[0].values[0].typeToken = ZR_VALUE_TYPE_INT64;

    source.functions[0].values[2].typeToken = ZR_VALUE_TYPE_BOOL;
    eis5_sub_require_writer_rejected(
            &source, payload, TEST_EIS5_SUB_RESULT_VALUE_RECORD_OFFSET,
            "writer rejects non-i64 EIS5 SUB result at value record");
    eis5_sub_require_canonical_write_rejected(
            &fixture, &source, path, original, originalLength,
            "canonical writer rejects non-i64 EIS5 SUB result");
    source.functions[0].values[2].typeToken = ZR_VALUE_TYPE_INT64;

    source.functions[0].instructions[2].opcode =
            (TZrUInt16)ZR_EXEC_IR_OPCODE_ARITHMETIC;
    eis5_sub_require_writer_rejected(
            &source, payload, TEST_EIS5_SUB_OPCODE_OFFSET,
            "writer rejects unsupported EIS5 ARITHMETIC opcode");
    eis5_sub_require_canonical_write_rejected(
            &fixture, &source, path, original, originalLength,
            "canonical writer rejects unsupported EIS5 ARITHMETIC opcode");
    source.functions[0].instructions[2].opcode =
            (TZrUInt16)ZR_EXEC_IR_OPCODE_SUB;

    free(original);
    free(path);
    free(payload);
    ZrCore_ExecIr_FreeModule(&source);
}

static void eis5_sub_read_phase(const char *basePath) {
    SFixture fixture;
    char *path = eis5_sub_artifact_path(basePath);
    TZrSize length;
    TZrSize bundleOffset, payloadOffset;
    TZrByte *bytes = read_file(path, &length);
    TZrByte *mutated = (TZrByte *)malloc(length);
    SZrExecIrModule module;
    SZrArtifactDiagnostic diagnostic;
    SZrArtifactView outer = {0};
    SZrArtifactSectionView bundle = {0};
    SZrArtifactExecIrView nested = {0};
    const SZrArtifactExecIrSectionView *payload = ZR_NULL;

    require_true(mutated != ZR_NULL, "allocate EIS5 SUB mutation buffer");
    init_fixture(&fixture);
    ZrCore_ExecIr_ModuleInit(&module);
    require_true(ZrCore_Module_OpenExecIrArtifact(
                 bytes, length, &fixture.metadata.identity, &module,
                 &diagnostic) == ZR_ARTIFACT_STATUS_OK,
                 "open canonical EIS5 SUB artifact in separate process");
    eis5_sub_assert_graph_and_oracle(&module);
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
                 payload->byteLength == TEST_EIS5_SUB_ENCODED_SIZE &&
                 memcmp(payload->data, "EIS5", 4u) == 0,
                 "find EIS5 SUB payload with the EIS4 colliding length");
    bundleOffset = (TZrSize)(bundle.data - bytes);
    payloadOffset = (TZrSize)(payload->data - bytes);

    memcpy(mutated, bytes, length);
    put32(mutated + payloadOffset + TEST_EIS5_SUB_FIRST_CONSTANT_TYPE_OFFSET,
          (TZrUInt32)ZR_VALUE_TYPE_BOOL);
    put64(mutated + payloadOffset + TEST_EIS5_SUB_FIRST_CONSTANT_BITS_OFFSET,
          1u);
    put32(mutated + payloadOffset + TEST_EIS5_SUB_FIRST_VALUE_TYPE_OFFSET,
          (TZrUInt32)ZR_VALUE_TYPE_BOOL);
    eis5_rehash(mutated, bundleOffset, payloadOffset, payload->byteLength);
    eis5_sub_require_codec_rejected(
            mutated + payloadOffset, payload->byteLength,
            ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
            TEST_EIS5_SUB_FIRST_VALUE_RECORD_OFFSET,
            "reject malformed EIS5 SUB input type");
    diagnostic = require_rejected(mutated, length,
                                  ZR_ARTIFACT_STATUS_INVALID_SECTION);
    require_true(diagnostic.sectionKind ==
                         ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE &&
                 diagnostic.byteOffset == payloadOffset +
                         TEST_EIS5_SUB_FIRST_VALUE_RECORD_OFFSET,
                 "opener rejects rehashed non-i64 SUB input at exact value offset");

    memcpy(mutated, bytes, length);
    put32(mutated + payloadOffset + TEST_EIS5_SUB_RESULT_VALUE_TYPE_OFFSET,
          (TZrUInt32)ZR_VALUE_TYPE_BOOL);
    eis5_rehash(mutated, bundleOffset, payloadOffset, payload->byteLength);
    eis5_sub_require_codec_rejected(
            mutated + payloadOffset, payload->byteLength,
            ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
            TEST_EIS5_SUB_RESULT_VALUE_RECORD_OFFSET,
            "reject malformed EIS5 SUB result type");
    diagnostic = require_rejected(mutated, length,
                                  ZR_ARTIFACT_STATUS_INVALID_SECTION);
    require_true(diagnostic.sectionKind ==
                         ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE &&
                 diagnostic.byteOffset == payloadOffset +
                         TEST_EIS5_SUB_RESULT_VALUE_RECORD_OFFSET,
                 "opener rejects rehashed non-i64 SUB result at exact value offset");

    memcpy(mutated, bytes, length);
    put16(mutated + payloadOffset + TEST_EIS5_SUB_OPCODE_OFFSET,
          (TZrUInt16)ZR_EXEC_IR_OPCODE_ARITHMETIC);
    eis5_rehash(mutated, bundleOffset, payloadOffset, payload->byteLength);
    eis5_sub_require_codec_rejected(
            mutated + payloadOffset, payload->byteLength,
            ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
            TEST_EIS5_SUB_OPCODE_OFFSET,
            "reject malformed EIS5 SUB unsupported ARITHMETIC opcode");
    diagnostic = require_rejected(mutated, length,
                                  ZR_ARTIFACT_STATUS_INVALID_SECTION);
    require_true(diagnostic.sectionKind ==
                         ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE &&
                 diagnostic.byteOffset == payloadOffset +
                         TEST_EIS5_SUB_OPCODE_OFFSET,
                 "opener rejects rehashed unsupported ARITHMETIC at instruction offset");

    free(mutated);
    free(bytes);
    free(path);
}
