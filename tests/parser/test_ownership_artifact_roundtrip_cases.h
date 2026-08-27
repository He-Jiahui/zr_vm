#ifndef ZR_VM_TEST_OWNERSHIP_ARTIFACT_ROUNDTRIP_CASES_H
#define ZR_VM_TEST_OWNERSHIP_ARTIFACT_ROUNDTRIP_CASES_H

typedef struct SZrOwnershipArtifactReader {
    const TZrByte *bytes;
    TZrSize length;
    TZrBool consumed;
} SZrOwnershipArtifactReader;

static TZrByte *ownership_artifact_read_file(
        const TZrChar *path,
        TZrSize *outLength) {
    FILE *file;
    long fileSize;
    TZrByte *bytes;

    if (path == ZR_NULL || outLength == ZR_NULL) {
        return ZR_NULL;
    }
    *outLength = 0u;
    file = fopen(path, "rb");
    if (file == ZR_NULL || fseek(file, 0, SEEK_END) != 0) {
        if (file != ZR_NULL) {
            fclose(file);
        }
        return ZR_NULL;
    }
    fileSize = ftell(file);
    if (fileSize <= 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return ZR_NULL;
    }
    bytes = (TZrByte *)malloc((size_t)fileSize);
    if (bytes == ZR_NULL ||
        fread(bytes, 1u, (size_t)fileSize, file) != (size_t)fileSize) {
        free(bytes);
        fclose(file);
        return ZR_NULL;
    }
    fclose(file);
    *outLength = (TZrSize)fileSize;
    return bytes;
}

static TZrBytePtr ownership_artifact_reader_read(
        SZrState *state,
        TZrPtr customData,
        TZrSize *outSize) {
    SZrOwnershipArtifactReader *reader =
            (SZrOwnershipArtifactReader *)customData;

    ZR_UNUSED_PARAMETER(state);
    if (reader == ZR_NULL || outSize == ZR_NULL || reader->consumed) {
        return ZR_NULL;
    }
    reader->consumed = ZR_TRUE;
    *outSize = reader->length;
    return (TZrBytePtr)reader->bytes;
}

static void ownership_artifact_reader_close(
        SZrState *state,
        TZrPtr customData) {
    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(customData);
}

static void assert_ownership_artifact_type_ref_equal(
        const SZrFunctionTypedTypeRef *expected,
        const SZrFunctionTypedTypeRef *actual) {
    TEST_ASSERT_NOT_NULL(expected);
    TEST_ASSERT_NOT_NULL(actual);
    TEST_ASSERT_EQUAL_INT(expected->baseType, actual->baseType);
    TEST_ASSERT_EQUAL_INT(expected->isNullable, actual->isNullable);
    TEST_ASSERT_EQUAL_UINT32(
            expected->ownershipQualifier, actual->ownershipQualifier);
    TEST_ASSERT_EQUAL_INT(expected->isArray, actual->isArray);
    TEST_ASSERT_EQUAL_INT(expected->elementBaseType, actual->elementBaseType);
    TEST_ASSERT_EQUAL_INT(expected->staticCType, actual->staticCType);
    if (expected->staticCType == ZR_STATIC_C_TYPE_STRUCT) {
        TEST_ASSERT_EQUAL_UINT32(
                expected->staticCTypeId, actual->staticCTypeId);
    } else {
        TEST_ASSERT_EQUAL_UINT32(
                ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE,
                actual->staticCTypeId);
    }
    if (expected->typeName == ZR_NULL) {
        TEST_ASSERT_NULL(actual->typeName);
    } else {
        TEST_ASSERT_NOT_NULL(actual->typeName);
        TEST_ASSERT_TRUE(ZrCore_String_Equal(expected->typeName, actual->typeName));
    }
    if (expected->elementTypeName == ZR_NULL) {
        TEST_ASSERT_NULL(actual->elementTypeName);
    } else {
        TEST_ASSERT_NOT_NULL(actual->elementTypeName);
        TEST_ASSERT_TRUE(ZrCore_String_Equal(
                expected->elementTypeName, actual->elementTypeName));
    }
}

static void assert_ownership_artifact_function_projection_equal(
        const SZrFunction *expected,
        const SZrFunction *actual) {
    TEST_ASSERT_NOT_NULL(expected);
    TEST_ASSERT_NOT_NULL(actual);
    TEST_ASSERT_EQUAL_UINT32(expected->stackSize, actual->stackSize);
    TEST_ASSERT_EQUAL_UINT32(
            expected->instructionsLength, actual->instructionsLength);
    if (expected->instructionsLength > 0u) {
        TEST_ASSERT_EQUAL_MEMORY(
                expected->instructionsList,
                actual->instructionsList,
                sizeof(TZrInstruction) * expected->instructionsLength);
    }
    TEST_ASSERT_EQUAL_UINT32(
            expected->catchClauseCount, actual->catchClauseCount);
    for (TZrUInt32 index = 0u; index < expected->catchClauseCount; index++) {
        TEST_ASSERT_EQUAL_UINT64(
                expected->catchClauseList[index].targetInstructionOffset,
                actual->catchClauseList[index].targetInstructionOffset);
        TEST_ASSERT_TRUE(ZrCore_String_Equal(
                expected->catchClauseList[index].typeName,
                actual->catchClauseList[index].typeName));
    }
    TEST_ASSERT_EQUAL_UINT32(
            expected->exceptionHandlerCount, actual->exceptionHandlerCount);
    for (TZrUInt32 index = 0u;
         index < expected->exceptionHandlerCount;
         index++) {
        const SZrFunctionExceptionHandlerInfo *expectedHandler =
                &expected->exceptionHandlerList[index];
        const SZrFunctionExceptionHandlerInfo *actualHandler =
                &actual->exceptionHandlerList[index];

        TEST_ASSERT_EQUAL_UINT64(
                expectedHandler->protectedStartInstructionOffset,
                actualHandler->protectedStartInstructionOffset);
        TEST_ASSERT_EQUAL_UINT64(
                expectedHandler->finallyTargetInstructionOffset,
                actualHandler->finallyTargetInstructionOffset);
        TEST_ASSERT_EQUAL_UINT64(
                expectedHandler->afterFinallyInstructionOffset,
                actualHandler->afterFinallyInstructionOffset);
        TEST_ASSERT_EQUAL_UINT32(
                expectedHandler->catchClauseStartIndex,
                actualHandler->catchClauseStartIndex);
        TEST_ASSERT_EQUAL_UINT32(
                expectedHandler->catchClauseCount,
                actualHandler->catchClauseCount);
        TEST_ASSERT_EQUAL_INT(
                expectedHandler->hasFinally, actualHandler->hasFinally);
    }
    TEST_ASSERT_EQUAL_UINT32(
            expected->semIrInstructionLength, actual->semIrInstructionLength);
    if (expected->semIrInstructionLength > 0u) {
        TEST_ASSERT_EQUAL_MEMORY(
                expected->semIrInstructions,
                actual->semIrInstructions,
                sizeof(SZrSemIrInstruction) * expected->semIrInstructionLength);
    }
    TEST_ASSERT_EQUAL_UINT32(
            expected->semIrEffectTableLength, actual->semIrEffectTableLength);
    if (expected->semIrEffectTableLength > 0u) {
        TEST_ASSERT_EQUAL_MEMORY(
                expected->semIrEffectTable,
                actual->semIrEffectTable,
                sizeof(SZrSemIrEffectEntry) * expected->semIrEffectTableLength);
    }
    TEST_ASSERT_EQUAL_UINT32(
            expected->semIrOwnershipTableLength,
            actual->semIrOwnershipTableLength);
    if (expected->semIrOwnershipTableLength > 0u) {
        TEST_ASSERT_EQUAL_MEMORY(
                expected->semIrOwnershipTable,
                actual->semIrOwnershipTable,
                sizeof(SZrSemIrOwnershipEntry) *
                        expected->semIrOwnershipTableLength);
    }
    TEST_ASSERT_EQUAL_UINT32(
            expected->semIrBlockTableLength, actual->semIrBlockTableLength);
    if (expected->semIrBlockTableLength > 0u) {
        TEST_ASSERT_EQUAL_MEMORY(
                expected->semIrBlockTable,
                actual->semIrBlockTable,
                sizeof(SZrSemIrBlockEntry) * expected->semIrBlockTableLength);
    }
    TEST_ASSERT_EQUAL_UINT32(
            expected->semIrDeoptTableLength, actual->semIrDeoptTableLength);
    if (expected->semIrDeoptTableLength > 0u) {
        TEST_ASSERT_EQUAL_MEMORY(
                expected->semIrDeoptTable,
                actual->semIrDeoptTable,
                sizeof(SZrSemIrDeoptEntry) * expected->semIrDeoptTableLength);
    }
    TEST_ASSERT_EQUAL_UINT32(
            expected->semIrTypeTableLength, actual->semIrTypeTableLength);
    for (TZrUInt32 index = 0u;
         index < expected->semIrTypeTableLength;
         index++) {
        assert_ownership_artifact_type_ref_equal(
                &expected->semIrTypeTable[index],
                &actual->semIrTypeTable[index]);
    }
    TEST_ASSERT_EQUAL_UINT32(
            expected->typedLocalBindingLength,
            actual->typedLocalBindingLength);
    for (TZrUInt32 index = 0u;
         index < expected->typedLocalBindingLength;
         index++) {
        const SZrFunctionTypedLocalBinding *expectedBinding =
                &expected->typedLocalBindings[index];
        const SZrFunctionTypedLocalBinding *actualBinding =
                &actual->typedLocalBindings[index];

        TEST_ASSERT_EQUAL_UINT32(
                expectedBinding->stackSlot, actualBinding->stackSlot);
        TEST_ASSERT_EQUAL_UINT32(
                expectedBinding->typeId, actualBinding->typeId);
        TEST_ASSERT_EQUAL_UINT32(
                expectedBinding->placeId, actualBinding->placeId);
        assert_ownership_artifact_type_ref_equal(
                &expectedBinding->type, &actualBinding->type);
    }
    TEST_ASSERT_EQUAL_UINT32(
            expected->childFunctionLength, actual->childFunctionLength);
    for (TZrUInt32 index = 0u;
         index < expected->childFunctionLength;
         index++) {
        assert_ownership_artifact_function_projection_equal(
                &expected->childFunctionList[index],
                &actual->childFunctionList[index]);
    }
}

static void test_ownership_guard_binary_roundtrip_preserves_execution_projection(
        void) {
    static const TZrChar source[] =
            "resource class Box {\n"
            "    pub var value: int;\n"
            "    pub @constructor(value: int) { this.value = value; }\n"
            "}\n"
            "fn live(): int {\n"
            "    var seed = own Box(7);\n"
            "    var shared = share(seed);\n"
            "    var weak = degrade(shared);\n"
            "    var value = weak?.value;\n"
            "    drop(shared);\n"
            "    if (value == 7) { return 1; }\n"
            "    return 0;\n"
            "}\n"
            "fn expired(): int {\n"
            "    var seed = own Box(9);\n"
            "    var shared = share(seed);\n"
            "    var weak = degrade(shared);\n"
            "    drop(shared);\n"
            "    var optional = weak?.value;\n"
            "    var caught = 0;\n"
            "    try { var direct = weak.value; }\n"
            "    catch (error: NullReferenceError) { caught = 1; }\n"
            "    if (optional == null && caught == 1) { return 1; }\n"
            "    return 0;\n"
            "}\n"
            "return live() + expired();\n";
    TZrChar artifactBaseName[96];
    SZrString *sourceName;
    SZrFunction *sourceFunction;
    SZrFunction *runtimeFunction;
    SZrIo *io;
    SZrIoSource *sourceObject;
    SZrOwnershipArtifactReader reader;
    TZrByte *artifactBytes;
    TZrSize artifactLength = 0u;
    TZrInt64 sourceResult = 0;
    TZrInt64 runtimeResult = 0;
    int artifactBaseNameLength;

    g_ownership_artifact_path[0] = '\0';
    artifactBaseNameLength = snprintf(
            artifactBaseName,
            sizeof(artifactBaseName),
            "ownership_guard_execution_projection_%u",
            ownership_test_process_id());
    TEST_ASSERT_TRUE(artifactBaseNameLength > 0);
    TEST_ASSERT_TRUE(
            (TZrSize)artifactBaseNameLength < sizeof(artifactBaseName));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "ownership_intrinsic_member_separation",
            "roundtrip",
            artifactBaseName,
            ".zro",
            g_ownership_artifact_path,
            sizeof(g_ownership_artifact_path)));

    ZrParser_ToGlobalState_Register(g_state);
    TEST_ASSERT_TRUE(ZrVmLibSystem_Register(g_state->global));
    sourceName = ZrCore_String_CreateFromNative(
            g_state, "ownership_guard_execution_projection.zr");
    sourceFunction = ZrParser_Source_Compile(
            g_state, source, sizeof(source) - 1u, sourceName);
    TEST_ASSERT_NOT_NULL(sourceFunction);
    TEST_ASSERT_TRUE(function_contains_opcode_recursive(
            sourceFunction, ZR_INSTRUCTION_ENUM(OWN_SHARE), 0u));
    TEST_ASSERT_TRUE(function_contains_opcode_recursive(
            sourceFunction, ZR_INSTRUCTION_ENUM(OWN_DEGRADE), 0u));
    TEST_ASSERT_TRUE(function_contains_opcode_recursive(
            sourceFunction, ZR_INSTRUCTION_ENUM(OWN_WAKE), 0u));
    TEST_ASSERT_TRUE(function_contains_opcode_recursive(
            sourceFunction, ZR_INSTRUCTION_ENUM(JUMP_IF_NULL), 0u));
    TEST_ASSERT_TRUE(function_contains_opcode_recursive(
            sourceFunction, ZR_INSTRUCTION_ENUM(REQUIRE_NON_NULL), 0u));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, sourceFunction, &sourceResult));
    TEST_ASSERT_EQUAL_INT64(2, sourceResult);
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(
            g_state, sourceFunction, g_ownership_artifact_path));

    artifactBytes = ownership_artifact_read_file(
            g_ownership_artifact_path, &artifactLength);
    TEST_ASSERT_NOT_NULL(artifactBytes);
    reader.bytes = artifactBytes;
    reader.length = artifactLength;
    reader.consumed = ZR_FALSE;
    io = ZrCore_Io_New(g_state->global);
    TEST_ASSERT_NOT_NULL(io);
    ZrCore_Io_Init(
            g_state,
            io,
            ownership_artifact_reader_read,
            ownership_artifact_reader_close,
            &reader);
    io->isBinary = ZR_TRUE;
    sourceObject = ZrCore_Io_ReadSourceNew(io);
    TEST_ASSERT_NOT_NULL(sourceObject);
    runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(
            g_state, sourceObject);
    TEST_ASSERT_NOT_NULL(runtimeFunction);

    assert_ownership_artifact_function_projection_equal(
            sourceFunction, runtimeFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, runtimeFunction, &runtimeResult));
    TEST_ASSERT_EQUAL_INT64(2, runtimeResult);

    ZrCore_Function_Free(g_state, runtimeFunction);
    ZrCore_Io_Free(g_state->global, io);
    free(artifactBytes);
    TEST_ASSERT_EQUAL_INT(0, remove(g_ownership_artifact_path));
    g_ownership_artifact_path[0] = '\0';
    ZrCore_Function_Free(g_state, sourceFunction);
}

#endif
