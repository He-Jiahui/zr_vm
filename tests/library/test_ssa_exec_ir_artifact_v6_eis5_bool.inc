/* BOOL-specific EIS5 tests; include after test_ssa_exec_ir_artifact_v6_eis5.inc. */
static char *eis5_bool_artifact_path(const char *basePath,
                                    TZrBool condition) {
    TZrSize capacity = strlen(basePath) + 32u;
    char *path = (char *)malloc(capacity);
    require_true(path != ZR_NULL, "allocate EIS5 BOOL artifact path");
    snprintf(path, capacity, "%s.eis5-bool.%s", basePath,
             condition ? "true" : "false");
    return path;
}

static void eis5_bool_predicate_write_phase(const char *basePath) {
    for (TZrUInt32 selected = 0u; selected < 2u; ++selected) {
        TZrBool condition = (TZrBool)selected;
        SFixture boolFixture;
        SZrExecIrModule source;
        SZrExecIrModule decoded;
        SZrExecIrFunction *function;
        SZrArtifactExecIrDiagnostic codecDiagnostic;
        SZrArtifactDiagnostic artifactDiagnostic;
        TZrByte *payload;
        TZrUInt32 encodedSize = UINT32_MAX;
        char *path;

        init_fixture(&boolFixture);
        eis5_build_graph(&source, condition, ZR_TRUE);
        require_true(ZrCore_ExecIr_VerifyModule(&source, ZR_NULL),
                     "verify EIS5 BOOL-predicate fixture before encoding");
        require_true(ZrCore_ArtifactExecIrScalar_GetEncodedSize(
                     &source, &encodedSize, &codecDiagnostic) ==
                     ZR_ARTIFACT_EXEC_IR_OK &&
                     encodedSize == TEST_EIS5_FIVE_BLOCK_ENCODED_SIZE,
                     "EIS5 BOOL-predicate dynamic size query");
        payload = (TZrByte *)malloc(encodedSize);
        require_true(payload != ZR_NULL,
                     "allocate EIS5 BOOL-predicate payload");
        require_true(ZrCore_ArtifactExecIrScalar_Write(
                     &source, payload, encodedSize, &codecDiagnostic) ==
                     ZR_ARTIFACT_EXEC_IR_OK &&
                     eis5_get32(payload + TEST_EIS5_HEADER_SIZE + 168u) ==
                             (TZrUInt32)ZR_VALUE_TYPE_BOOL &&
                     eis5_get32(payload + TEST_EIS5_HEADER_SIZE + 168u +
                                16u) == (TZrUInt32)ZR_VALUE_TYPE_INT64,
                     "write EIS5 BOOL predicate and i64 return constants");
        ZrCore_ExecIr_ModuleInit(&decoded);
        require_true(ZrCore_ArtifactExecIrScalar_Read(
                     payload, encodedSize, &decoded, &codecDiagnostic) ==
                     ZR_ARTIFACT_EXEC_IR_OK,
                     "read EIS5 BOOL-predicate payload directly");
        eis5_assert_graph_and_oracle(&decoded, condition, ZR_TRUE);
        ZrCore_ExecIr_FreeModule(&decoded);

        path = eis5_bool_artifact_path(basePath, condition);
        require_true(ZrParser_ExecIr_WriteCanonicalZroFile(
                     &boolFixture.metadata, &source, path,
                     &artifactDiagnostic) == ZR_ARTIFACT_STATUS_OK,
                     "write canonical EIS5 BOOL-predicate artifact");
        free(path);

        source.constants[0].bits = 2u;
        require_true(ZrCore_ArtifactExecIrScalar_GetEncodedSize(
                     &source, &encodedSize, &codecDiagnostic) ==
                         ZR_ARTIFACT_EXEC_IR_INVALID_SECTION &&
                     codecDiagnostic.byteOffset ==
                             TEST_EIS5_FIRST_CONSTANT_BITS_OFFSET,
                     "writer rejects noncanonical EIS5 BOOL bits");
        put64(payload + TEST_EIS5_FIRST_CONSTANT_BITS_OFFSET, 2u);
        eis5_require_codec_rejected(
                payload, TEST_EIS5_FIVE_BLOCK_ENCODED_SIZE,
                ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                TEST_EIS5_FIRST_CONSTANT_BITS_OFFSET, ZR_TRUE);

        source.constants[0].bits = condition ? 1u : 0u;
        function = ZrCore_ExecIr_ModuleFunctionAt(&source, 1u);
        require_true(function != ZR_NULL,
                     "find EIS5 BOOL-predicate source function");
        function->values[0].typeToken = ZR_VALUE_TYPE_INT64;
        require_true(ZrCore_ArtifactExecIrScalar_GetEncodedSize(
                     &source, &encodedSize, &codecDiagnostic) ==
                         ZR_ARTIFACT_EXEC_IR_INVALID_SECTION &&
                     codecDiagnostic.byteOffset ==
                             TEST_EIS5_FIRST_VALUE_RECORD_OFFSET,
                     "writer rejects an i64 value used as a BOOL predicate");
        free(payload);
        ZrCore_ExecIr_FreeModule(&source);
    }
}

static void eis5_bool_predicate_read_phase(const char *basePath) {
    for (TZrUInt32 selected = 0u; selected < 2u; ++selected) {
        TZrBool condition = (TZrBool)selected;
        SFixture fixture;
        char *path = eis5_bool_artifact_path(basePath, condition);
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
                     "allocate EIS5 BOOL-predicate mutation buffer");
        init_fixture(&fixture);
        ZrCore_ExecIr_ModuleInit(&module);
        require_true(ZrCore_Module_OpenExecIrArtifact(
                     bytes, length, &fixture.metadata.identity, &module,
                     &diagnostic) == ZR_ARTIFACT_STATUS_OK,
                     "open EIS5 BOOL-predicate artifact in a separate process");
        eis5_assert_graph_and_oracle(&module, condition, ZR_TRUE);
        ZrCore_ExecIr_FreeModule(&module);
        require_true(ZrCore_Artifact_Read(bytes, length, &outer,
                     &diagnostic) == ZR_ARTIFACT_STATUS_OK &&
                     ZrCore_Artifact_FindSection(
                     &outer, ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE,
                     &bundle, &diagnostic) == ZR_ARTIFACT_STATUS_OK &&
                     ZrCore_ArtifactExecIr_Read(bundle.data,
                     bundle.byteLength, &nested, ZR_NULL) ==
                     ZR_ARTIFACT_EXEC_IR_OK &&
                     ZrCore_ArtifactExecIr_FindSection(
                     &nested, ZR_ARTIFACT_EXEC_IR_SECTION_EXEC_IR,
                     &payload, ZR_NULL) == ZR_ARTIFACT_EXEC_IR_OK &&
                     payload->byteLength ==
                             TEST_EIS5_FIVE_BLOCK_ENCODED_SIZE,
                     "find EIS5 BOOL-predicate payload in ERI1");
        bundleOffset = (TZrSize)(bundle.data - bytes);
        payloadOffset = (TZrSize)(payload->data - bytes);

        memcpy(mutated, bytes, length);
        put64(mutated + payloadOffset +
                      TEST_EIS5_FIRST_CONSTANT_BITS_OFFSET,
              2u);
        eis5_rehash(mutated, bundleOffset, payloadOffset,
                    payload->byteLength);
        eis5_require_codec_rejected(
                mutated + payloadOffset, payload->byteLength,
                ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                TEST_EIS5_FIRST_CONSTANT_BITS_OFFSET, ZR_TRUE);
        diagnostic = require_rejected(mutated, length,
                                      ZR_ARTIFACT_STATUS_INVALID_SECTION);
        require_true(diagnostic.sectionKind ==
                         ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE &&
                     diagnostic.byteOffset == payloadOffset +
                             TEST_EIS5_FIRST_CONSTANT_BITS_OFFSET,
                     "opener rejects rehashed noncanonical EIS5 BOOL bits");

        memcpy(mutated, bytes, length);
        put32(mutated + payloadOffset +
                      TEST_EIS5_FIRST_VALUE_TYPE_TOKEN_OFFSET,
              (TZrUInt32)ZR_VALUE_TYPE_INT64);
        eis5_rehash(mutated, bundleOffset, payloadOffset,
                    payload->byteLength);
        eis5_require_codec_rejected(
                mutated + payloadOffset, payload->byteLength,
                ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                TEST_EIS5_FIRST_VALUE_RECORD_OFFSET, ZR_TRUE);
        diagnostic = require_rejected(mutated, length,
                                      ZR_ARTIFACT_STATUS_INVALID_SECTION);
        require_true(diagnostic.sectionKind ==
                         ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE &&
                     diagnostic.byteOffset == payloadOffset +
                             TEST_EIS5_FIRST_VALUE_RECORD_OFFSET,
                     "opener rejects rehashed EIS5 BOOL type mismatch");
        free(mutated);
        free(bytes);
        free(path);
    }
}
