static void canonical_zraf_eis6_set_contract(
        SZrExecutionContract *contract, TZrMetadataToken targetToken,
        TZrUInt64 signatureHash, TZrUInt64 layoutHash, TZrUInt64 moduleHash) {
    memset(contract, 0, sizeof(*contract));
    contract->schemaVersion = ZR_EXECUTION_CONTRACT_SCHEMA_VERSION;
    contract->abiVersion = ZR_EXECUTION_CONTRACT_ABI_VERSION;
    contract->logicalVersion = ZR_EXECUTION_CONTRACT_LOGICAL_VERSION;
    contract->generation = 1u;
    contract->targetToken = targetToken;
    contract->signatureHash = signatureHash;
    contract->layoutHash = layoutHash;
    contract->moduleHash = moduleHash;
}

static int canonical_zraf_eis6_build_typed_call(SZrExecIrModule *module) {
    const TZrExecIrMemoryTokenId memoryInTokens[2] = {
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 1u),
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_NATIVE_FFI, 1u)
    };
    const TZrExecIrMemoryTokenId memoryOutTokens[2] = {
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 2u),
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_NATIVE_FFI, 2u)
    };
    const SZrExecIrConstant constant = {ZR_VALUE_TYPE_INT64, 0u, 42u};
    SZrExecIrRange constantRange, constantResult, callOperand, callResult;
    SZrExecIrRange returnOperand, memoryIn, memoryOut;
    SZrExecIrBindingRow row;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrInstruction instruction;
    SZrExecIrFunction *function;
    TZrExecIrFunctionId functionId;
    TZrExecIrValueId inputValue, callValue;
    TZrExecIrInstructionId instructionId;

    ZrCore_ExecIr_ModuleInit(module);
    module->id = 1u;
    module->moduleToken = TEST_MODULE;
    module->moduleHash = TEST_MODULE_HASH;
    canonical_zraf_eis6_set_contract(&module->contract, TEST_MODULE,
                                     TEST_SIGNATURE_HASH, TEST_LAYOUT_HASH,
                                     TEST_MODULE_HASH);
    if (!ZrCore_ExecIr_ModuleAppendConstant(module, &constant, 1u,
                                            &constantRange) ||
        !ZrCore_ExecIr_ModuleAddFunction(module, TEST_MEMBER,
                                         TEST_SIGNATURE_HASH, &functionId))
        goto fail;
    function = ZrCore_ExecIr_ModuleFunctionAt(module, functionId);
    if (function == ZR_NULL) goto fail;
    canonical_zraf_eis6_set_contract(&function->contract, TEST_MEMBER,
                                     TEST_SIGNATURE_HASH, TEST_LAYOUT_HASH,
                                     TEST_MODULE_HASH);
    if (ZrCore_ExecIr_FunctionAddBlock(
                function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) !=
        ZR_EXEC_IR_BLOCK_ID_ENTRY)
        goto fail;
    function->entryBlockId = ZR_EXEC_IR_BLOCK_ID_ENTRY;

    inputValue = ZrCore_ExecIr_FunctionAddValue(
            function, ZR_VALUE_TYPE_INT64, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    if (inputValue != 1u ||
        !ZrCore_ExecIr_FunctionAppendResults(function, &inputValue, 1u,
                                             &constantResult))
        goto fail;
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_CONSTANT;
    instruction.results = constantResult;
    instruction.layoutId = constantRange.start;
    instruction.sourceId = 501u;
    if (!ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction,
                                                 &instructionId) ||
        instructionId != 1u)
        goto fail;

    callValue = ZrCore_ExecIr_FunctionAddValue(
            function, ZR_VALUE_TYPE_INT64, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    if (callValue != 2u ||
        !ZrCore_ExecIr_FunctionAppendResults(function, &callValue, 1u,
                                             &callResult) ||
        !ZrCore_ExecIr_FunctionAppendOperands(function, &inputValue, 1u,
                                              &callOperand) ||
        !ZrCore_ExecIr_FunctionAppendMemoryTokens(function, memoryInTokens, 2u,
                                                  &memoryIn) ||
        !ZrCore_ExecIr_FunctionAppendMemoryTokens(function, memoryOutTokens, 2u,
                                                  &memoryOut))
        goto fail;
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_CALL;
    instruction.flags = (TZrUInt16)(ZR_EXEC_IR_FLAG_MAY_THROW |
                                    ZR_EXEC_IR_FLAG_MAY_ALLOCATE);
    instruction.operands = callOperand;
    instruction.results = callResult;
    instruction.memoryIn = memoryIn;
    instruction.memoryOut = memoryOut;
    instruction.effectIn = 1u;
    instruction.effectOut = 2u;
    instruction.sourceId = 502u;
    if (!ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction,
                                                 &instructionId) ||
        instructionId != 2u)
        goto fail;

    if (!ZrCore_ExecIr_FunctionAppendOperands(function, &callValue, 1u,
                                              &returnOperand))
        goto fail;
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_RETURN;
    instruction.operands = returnOperand;
    instruction.sourceId = 503u;
    if (!ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction,
                                                 &instructionId) ||
        instructionId != 3u)
        goto fail;
    function->blocks[0].instructions.start = 0u;
    function->blocks[0].instructions.count = 3u;
    function->blocks[0].terminatorInstructionId = 3u;

    memset(&row, 0, sizeof(row));
    row.rowIndex = 0u;
    row.instructionId = 2u;
    row.segmentIndex = ZR_EXEC_IR_BINDING_SEGMENT_INDEX_NONE;
    row.contract.bindingKind = ZR_CALL_BINDING_DIRECT;
    row.contract.targetMetadataToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 77u);
    row.contract.signatureToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 91u);
    row.contract.signatureHash = UINT64_C(0x0102030405060708);
    row.contract.moduleSignatureHash = TEST_MODULE_HASH;
    row.contract.dispatchSlot = ZR_CALL_BINDING_SLOT_NONE;
    row.contract.operation = ZR_CALL_BINDING_OPERATION_CALL;
    row.location.kind = ZR_CALL_BINDING_RELOCATION_MODULE;
    row.location.targetIndex = UINT32_C(0x11223344);
    row.sourceId = UINT32_C(0xa1b2c3d4);
    if (!ZrCore_ExecIr_FunctionSetBindingRows(function, &row, 1u,
                                              &diagnostic) ||
        !ZrCore_ExecIr_VerifyModule(module, &diagnostic))
        goto fail;
    return 1;

fail:
    ZrCore_ExecIr_FreeModule(module);
    return 0;
}

static int canonical_zraf_eis6_build_outer(
        SFixture *fixture, const SZrExecIrModule *source, TZrBool expectCall,
        TZrByte **outBytes, TZrSize *outLength,
        TZrUInt32 *outScalarPayloadOffset) {
    SZrExecIrModule decoded;
    SZrArtifactExecIrSectionInput scalarSection;
    SZrArtifactExecIrDocument nestedDocument;
    SZrArtifactSectionInput outerSections[8];
    SZrArtifactDocument outerDocument;
    SZrArtifactDiagnostic artifactDiagnostic;
    SZrArtifactExecIrDiagnostic execIrDiagnostic;
    SZrArtifactView outerView;
    SZrArtifactSectionView bundle;
    SZrArtifactExecIrView nestedView;
    const SZrArtifactExecIrSectionView *scalarView = ZR_NULL;
    EZrArtifactStatus artifactStatus;
    EZrArtifactExecIrStatus execIrStatus;
    TZrByte *scalarBytes = ZR_NULL;
    TZrByte *nestedBytes = ZR_NULL;
    TZrByte *outerBytes = ZR_NULL;
    TZrUInt32 scalarLength = 0u;
    TZrUInt32 nestedLength = 0u;
    TZrUInt32 nestedWritten = 0u;
    TZrSize outerLength = 0u;
    TZrSize outerWritten = 0u;
    TZrBool decodedInitialized = ZR_FALSE;
    int ok = 0;

    if (fixture == ZR_NULL || source == ZR_NULL || outBytes == ZR_NULL ||
        outLength == ZR_NULL || outScalarPayloadOffset == ZR_NULL)
        return 0;
    *outBytes = ZR_NULL;
    *outLength = 0u;
    *outScalarPayloadOffset = 0u;

    if (ZrCore_ArtifactExecIrScalar_GetEncodedSize(
                source, &scalarLength, &execIrDiagnostic) !=
            ZR_ARTIFACT_EXEC_IR_OK ||
        scalarLength < 4u)
        goto cleanup;
    scalarBytes = (TZrByte *)malloc((size_t)scalarLength);
    if (scalarBytes == ZR_NULL ||
        ZrCore_ArtifactExecIrScalar_Write(
                source, scalarBytes, scalarLength, &execIrDiagnostic) !=
                ZR_ARTIFACT_EXEC_IR_OK ||
        memcmp(scalarBytes, "EIS6", 4u) != 0)
        goto cleanup;

    ZrCore_ExecIr_ModuleInit(&decoded);
    decodedInitialized = ZR_TRUE;
    execIrStatus = ZrCore_ArtifactExecIrScalar_Read(
            scalarBytes, scalarLength, &decoded, &execIrDiagnostic);
    if (execIrStatus != ZR_ARTIFACT_EXEC_IR_OK ||
        decoded.functionCount != 1u || decoded.functions == ZR_NULL ||
        decoded.functions[0].bindingRowsSchemaVersion !=
                ZR_EXEC_IR_BINDING_ROWS_SCHEMA_TYPED ||
        !ZrCore_ExecIr_VerifyModule(&decoded, ZR_NULL))
        goto cleanup;
    if (expectCall) {
        if (decoded.functions[0].bindingRowCount != 1u ||
            decoded.functions[0].bindingRows == ZR_NULL ||
            decoded.functions[0].instructionCount != 3u ||
            decoded.functions[0].instructions[1].opcode !=
                    ZR_EXEC_IR_OPCODE_CALL ||
            decoded.functions[0].instructions[1].bindingRow != 1u ||
            decoded.functions[0].memoryTokenCount != 4u)
            goto cleanup;
    } else if (decoded.functions[0].bindingRowCount != 0u ||
               decoded.functions[0].bindingRows != ZR_NULL) {
        goto cleanup;
    }
    ZrCore_ExecIr_FreeModule(&decoded);
    decodedInitialized = ZR_FALSE;

    memset(&scalarSection, 0, sizeof(scalarSection));
    scalarSection.kind = ZR_ARTIFACT_EXEC_IR_SECTION_EXEC_IR;
    scalarSection.elementCount = scalarLength;
    scalarSection.elementSize = 1u;
    scalarSection.data = scalarBytes;
    scalarSection.byteLength = scalarLength;
    memset(&nestedDocument, 0, sizeof(nestedDocument));
    nestedDocument.abiVersion = (TZrUInt16)ZR_VM_AOT_ABI_VERSION;
    nestedDocument.moduleHash = source->moduleHash;
    nestedDocument.execIrHash = ZrCore_ArtifactExecIr_HashBytes(
            scalarBytes, scalarLength);
    nestedDocument.sectionCount = 1u;
    nestedDocument.sections = &scalarSection;
    if (nestedDocument.execIrHash == 0u ||
        ZrCore_ArtifactExecIr_GetEncodedSize(
                &nestedDocument, &nestedLength, &execIrDiagnostic) !=
                ZR_ARTIFACT_EXEC_IR_OK)
        goto cleanup;
    nestedBytes = (TZrByte *)malloc((size_t)nestedLength);
    if (nestedBytes == ZR_NULL ||
        ZrCore_ArtifactExecIr_Write(
                &nestedDocument, nestedBytes, nestedLength, &nestedWritten,
                &execIrDiagnostic) != ZR_ARTIFACT_EXEC_IR_OK ||
        nestedWritten != nestedLength)
        goto cleanup;

    memcpy(outerSections, fixture->sections, sizeof(fixture->sections));
    outerSections[7].kind = ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE;
    outerSections[7].flags = ZR_ARTIFACT_SECTION_FLAG_MANDATORY;
    outerSections[7].elementCount = nestedLength;
    outerSections[7].data = nestedBytes;
    outerDocument = fixture->metadata;
    outerDocument.sectionCount = 8u;
    outerDocument.sections = outerSections;
    if (ZrCore_Artifact_GetEncodedSize(
                &outerDocument, &outerLength, &artifactDiagnostic) !=
            ZR_ARTIFACT_STATUS_OK)
        goto cleanup;
    outerBytes = (TZrByte *)malloc((size_t)outerLength);
    if (outerBytes == ZR_NULL ||
        ZrCore_Artifact_Write(
                &outerDocument, outerBytes, outerLength, &outerWritten,
                &artifactDiagnostic) != ZR_ARTIFACT_STATUS_OK ||
        outerWritten != outerLength)
        goto cleanup;

    memset(&outerView, 0, sizeof(outerView));
    memset(&bundle, 0, sizeof(bundle));
    memset(&nestedView, 0, sizeof(nestedView));
    if (ZrCore_Artifact_Read(outerBytes, outerLength, &outerView,
                             &artifactDiagnostic) != ZR_ARTIFACT_STATUS_OK ||
        ZrCore_Artifact_FindSection(
                &outerView, ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE, &bundle,
                &artifactDiagnostic) != ZR_ARTIFACT_STATUS_OK ||
        ZrCore_ArtifactExecIr_Read(
                bundle.data, bundle.byteLength, &nestedView,
                &execIrDiagnostic) != ZR_ARTIFACT_EXEC_IR_OK ||
        ZrCore_ArtifactExecIr_FindSection(
                &nestedView, ZR_ARTIFACT_EXEC_IR_SECTION_EXEC_IR, &scalarView,
                &execIrDiagnostic) != ZR_ARTIFACT_EXEC_IR_OK ||
        scalarView == ZR_NULL ||
        scalarView->byteLength != scalarLength)
        goto cleanup;

    *outScalarPayloadOffset = bundle.byteOffset + scalarView->byteOffset;
    *outLength = outerLength;
    *outBytes = outerBytes;
    outerBytes = ZR_NULL;
    ok = 1;

cleanup:
    if (decodedInitialized) ZrCore_ExecIr_FreeModule(&decoded);
    free(outerBytes);
    free(nestedBytes);
    free(scalarBytes);
    return ok;
}

static void canonical_zraf_eis6_assert_rejected(
        const SFixture *fixture, const SZrExecIrModule *source,
        const TZrByte *outerBytes, TZrSize outerLength,
        TZrUInt32 scalarPayloadOffset, const char *caseName) {
    SZrExecIrModule opened;
    SZrExecIrModule openedBefore;
    SZrArtifactDiagnostic artifactDiagnostic;
    SZrHotPatchCapabilityRequirement requirement;
    SZrHotPatchCapabilityManifest manifest;
    SZrValidatedHotPatchZraf validated;
    SZrHotPatchDiagnostic hotPatchDiagnostic;
    SCanonicalZrafVerifyContext verify;
    TZrByte *expectedCopy = (TZrByte *)malloc((size_t)outerLength);
    TZrUInt64 contentHash;
    EZrArtifactStatus artifactStatus;
    EZrHotPatchCapabilityStatus hotPatchStatus;

    require_true(expectedCopy != ZR_NULL,
                 "allocate typed EIS6 signature snapshot");
    memcpy(expectedCopy, outerBytes, (size_t)outerLength);
    contentHash = canonical_zraf_hash(outerBytes, outerLength);
    require_true(contentHash != 0u, "typed EIS6 outer hash is nonzero");

    ZrCore_ExecIr_ModuleInit(&opened);
    openedBefore = opened;
    memset(&artifactDiagnostic, 0xa5, sizeof(artifactDiagnostic));
    artifactStatus = ZrCore_Module_OpenExecIrArtifact(
            outerBytes, outerLength, &fixture->metadata.identity,
            &opened, &artifactDiagnostic);
    require_true(artifactStatus == ZR_ARTIFACT_STATUS_INVALID_SECTION,
                 caseName);
    require_true(artifactDiagnostic.status == ZR_ARTIFACT_STATUS_INVALID_SECTION &&
                         artifactDiagnostic.sectionKind ==
                                 ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE &&
                         artifactDiagnostic.byteOffset == scalarPayloadOffset,
                 "typed EIS6 module rejection reports nested payload location");
    require_true(memcmp(&opened, &openedBefore, sizeof(opened)) == 0,
                 "typed EIS6 module rejection leaves caller output unchanged");

    canonical_zraf_init_manifest(&manifest, &requirement, contentHash);
    canonical_zraf_init_verify_context(&verify, outerBytes, outerLength,
                                       expectedCopy, ZR_TRUE);
    memset(&validated, 0xa5, sizeof(validated));
    memset(&hotPatchDiagnostic, 0xa5, sizeof(hotPatchDiagnostic));
    hotPatchStatus = canonical_zraf_validate_with_selector(
            outerBytes, outerLength, &fixture->metadata.identity, &manifest,
            source->functions[0].functionToken,
            source->functions[0].signatureHash, contentHash,
            &verify, &validated, &hotPatchDiagnostic);
    canonical_zraf_require_rejected(
            hotPatchStatus, ZR_HOT_PATCH_ARTIFACT_INVALID, &validated,
            &hotPatchDiagnostic, caseName);
    require_true(verify.callCount == 1u && verify.sawFullOuterSpan &&
                         verify.observedHash == contentHash,
                 "typed EIS6 rejection follows full-span signature callback");
    require_true(hotPatchDiagnostic.sourceOffset == scalarPayloadOffset,
                 "hotpatch diagnostic retains typed EIS6 payload offset");
    free(expectedCopy);
}

static void canonical_zraf_test_eis6_guard(void) {
    SFixture fixture;
    SZrExecIrModule module;
    SZrExecIrDiagnostic execIrDiagnostic;
    TZrByte *outerBytes = ZR_NULL;
    TZrSize outerLength = 0u;
    TZrUInt32 scalarPayloadOffset = 0u;

    init_fixture(&fixture);
    build_graph(&module, ZR_FALSE);
    require_true(ZrCore_ExecIr_FunctionSetBindingRows(
                         &module.functions[0], ZR_NULL, 0u,
                         &execIrDiagnostic) &&
                         ZrCore_ExecIr_VerifyModule(&module,
                                                   &execIrDiagnostic),
                 "typed-empty EIS6 module passes Core verification");
    require_true(canonical_zraf_eis6_build_outer(
                         &fixture, &module, ZR_FALSE, &outerBytes,
                         &outerLength, &scalarPayloadOffset),
                 "public scalar codec roundtrips typed-empty EIS6 before artifact wrapping");
    canonical_zraf_eis6_assert_rejected(
            &fixture, &module, outerBytes, outerLength, scalarPayloadOffset,
            "canonical module opener rejects typed-empty EIS6 schema");
    free(outerBytes);
    outerBytes = ZR_NULL;
    ZrCore_ExecIr_FreeModule(&module);

    require_true(canonical_zraf_eis6_build_typed_call(&module),
                 "typed CALL EIS6 fixture passes Core VerifyModule");
    require_true(canonical_zraf_eis6_build_outer(
                         &fixture, &module, ZR_TRUE, &outerBytes,
                         &outerLength, &scalarPayloadOffset),
                 "public scalar codec roundtrips typed CALL EIS6 before artifact wrapping");
    canonical_zraf_eis6_assert_rejected(
            &fixture, &module, outerBytes, outerLength, scalarPayloadOffset,
            "canonical module opener rejects typed CALL EIS6 schema");
    free(outerBytes);
    ZrCore_ExecIr_FreeModule(&module);
}
