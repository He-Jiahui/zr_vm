/* Runtime evidence that a generated shared AOT throw restores the caller root chain. */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"

typedef struct SAotSharedLibraryThrowRootObservation {
    SZrState *state;
    FZrAllocator originalAllocator;
    TZrPtr originalAllocationArguments;
    SZrAotGcRootFrame *outerTop;
    TZrUInt32 outerDepth;
    TZrUInt32 allocatorCallCount;
    TZrUInt32 throwNormalizeObservedDepth;
    TZrBool targetThrowObjectAllocationAttempted;
    TZrBool sawThrowNormalizeObjectAllocation;
    TZrBool sawGeneratedManagedRootAtThrowNormalize;
    uintptr_t outerRootFrameIdentity;
    uintptr_t outerRootObjectIdentity;
    uintptr_t generatedRootFrameIdentity;
    uintptr_t generatedFrameBaseIdentity;
    uintptr_t generatedManagedObjectIdentity;
} SAotSharedLibraryThrowRootObservation;

static void aot_shared_library_throw_root_trace_stage(const char *stage) {
    fprintf(stderr, "[aot-throw-root] %s\n", stage);
    fflush(stderr);
}

static TZrBool aot_shared_library_throw_root_write_text(const TZrChar *path, const char *text) {
    FILE *file;
    size_t textLength;
    size_t writtenLength;
    int closeStatus;

    if (path == ZR_NULL || text == ZR_NULL || !ZrTests_Path_EnsureParentDirectory(path)) {
        return ZR_FALSE;
    }

    file = fopen(path, "wb");
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }

    textLength = strlen(text);
    writtenLength = fwrite(text, 1u, textLength, file);
    closeStatus = fclose(file);
    return (TZrBool)(writtenLength == textLength && closeStatus == 0);
}

static TZrBool aot_shared_library_throw_root_hash_file(const TZrChar *path,
                                                       TZrChar *outHash,
                                                       TZrSize outHashSize) {
    FILE *file;
    TZrByte chunk[ZR_STABLE_HASH_FILE_CHUNK_BUFFER_LENGTH];
    TZrUInt64 hash = ZR_STABLE_HASH_FNV1A64_OFFSET_BASIS;
    TZrSize readSize;
    TZrBool readSucceeded = ZR_TRUE;

    if (path == ZR_NULL || outHash == ZR_NULL || outHashSize == 0u) {
        return ZR_FALSE;
    }

    file = fopen(path, "rb");
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }

    while ((readSize = (TZrSize)fread(chunk, 1u, sizeof(chunk), file)) > 0u) {
        for (TZrSize index = 0u; index < readSize; index++) {
            hash ^= chunk[index];
            hash *= ZR_STABLE_HASH_FNV1A64_PRIME;
        }
    }
    if (ferror(file)) {
        readSucceeded = ZR_FALSE;
    }
    if (fclose(file) != 0) {
        readSucceeded = ZR_FALSE;
    }
    if (!readSucceeded) {
        return ZR_FALSE;
    }

    return (TZrBool)(snprintf(outHash,
                              outHashSize,
                              ZR_STABLE_HASH_HEX_PRINTF_FORMAT,
                              (unsigned long long)hash) > 0);
}

static char *aot_shared_library_throw_root_read_text(const TZrChar *path) {
    FILE *file;
    long fileLength;
    char *buffer;
    size_t readLength;
    int closeStatus;

    if (path == ZR_NULL) {
        return ZR_NULL;
    }

    file = fopen(path, "rb");
    if (file == ZR_NULL) {
        return ZR_NULL;
    }
    if (fseek(file, 0, SEEK_END) != 0 || (fileLength = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return ZR_NULL;
    }

    buffer = (char *)malloc((size_t)fileLength + 1u);
    if (buffer == ZR_NULL) {
        fclose(file);
        return ZR_NULL;
    }

    readLength = fread(buffer, 1u, (size_t)fileLength, file);
    closeStatus = fclose(file);
    if (readLength != (size_t)fileLength || closeStatus != 0) {
        free(buffer);
        return ZR_NULL;
    }

    buffer[fileLength] = '\0';
    return buffer;
}

static SZrFunction *aot_shared_library_throw_root_compile_source(SZrState *state,
                                                                  const char *source,
                                                                  const char *sourceNameText) {
    SZrString *sourceName;

    if (state == ZR_NULL || source == ZR_NULL || sourceNameText == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state, (TZrNativeString)sourceNameText, strlen(sourceNameText));
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }
    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static SZrRawObject *aot_shared_library_throw_root_find_live_object_in_slot(
        const SZrGarbageCollector *collector,
        const void *pointerSlotAddress) {
    unsigned char slotBytes[sizeof(SZrRawObject *)];
    SZrRawObject *nullObject = ZR_NULL;
    SZrRawObject *currentObject;

    if (collector == ZR_NULL || pointerSlotAddress == ZR_NULL) {
        return ZR_NULL;
    }

    /* Copy pointer representation bytes so a missed rewrite is never evaluated. */
    memcpy(slotBytes, pointerSlotAddress, sizeof(slotBytes));
    if (memcmp(slotBytes, &nullObject, sizeof(slotBytes)) == 0) {
        return ZR_NULL;
    }

    for (currentObject = collector->gcObjectList; currentObject != ZR_NULL;
         currentObject = currentObject->next) {
        if (memcmp(slotBytes, &currentObject, sizeof(slotBytes)) == 0) {
            return currentObject;
        }
    }
    return ZR_NULL;
}

static TZrBool aot_shared_library_throw_root_trace_live_object(
        const char *stage, SZrGarbageCollector *collector, const void *slotAddress) {
    SZrRawObject *object = aot_shared_library_throw_root_find_live_object_in_slot(collector, slotAddress);
    if (object == ZR_NULL) {
        fprintf(stderr, "[aot-throw-root] %s: live=0\n", stage);
    } else {
        fprintf(stderr, "[aot-throw-root] %s: live=1 region=%u storage=%u heap=%u age=%u pin=%u escape=%u\n",
                stage,
                (unsigned)object->garbageCollectMark.regionKind,
                (unsigned)object->garbageCollectMark.storageKind,
                (unsigned)object->garbageCollectMark.heapGenerationKind,
                (unsigned)object->garbageCollectMark.survivalAge,
                (unsigned)object->garbageCollectMark.pinFlags,
                (unsigned)object->garbageCollectMark.escapeFlags);
    }
    fflush(stderr);
    return (TZrBool)(object != ZR_NULL);
}

/* The fixture's generated throw function contains only one source-level
 * instruction: throwing this string. During NormalizeThrownValue the source
 * slot still carries that string and the current metadata function identifies
 * the frame which issued the AOT THROW. */
static TZrBool aot_shared_library_throw_root_is_normalizing_fixture_throw(SZrState *state) {
    SZrCallInfo *callInfo;
    SZrFunction *function;
    const char *functionName;
    TZrUInt32 throwInstructionCount = 0u;
    TZrUInt32 throwSourceSlot = 0u;
    TZrStackValuePointer slotBase;
    const SZrTypeValue *payload;

    if (state == ZR_NULL || state->hasCurrentException || state->callInfoList == ZR_NULL) {
        return ZR_FALSE;
    }
    callInfo = state->callInfoList;
    function = callInfo->metadataFunction;
    if (function == ZR_NULL || function->functionName == ZR_NULL ||
        function->instructionsList == ZR_NULL) {
        return ZR_FALSE;
    }
    functionName = ZrCore_String_GetNativeString(function->functionName);
    if (functionName == ZR_NULL || strcmp(functionName, "throw_with_live_root") != 0) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < function->instructionsLength; index++) {
        const TZrInstruction *instruction = &function->instructionsList[index];

        if (instruction->instruction.operationCode == (TZrUInt16)ZR_INSTRUCTION_ENUM(THROW)) {
            throwInstructionCount++;
            throwSourceSlot = instruction->instruction.operandExtra;
        }
    }
    if (throwInstructionCount != 1u || throwSourceSlot >= function->stackSize ||
        callInfo->functionBase.valuePointer == ZR_NULL) {
        return ZR_FALSE;
    }

    slotBase = callInfo->functionBase.valuePointer + 1;
    if (state->stackTop.valuePointer == ZR_NULL ||
        state->stackTop.valuePointer <= slotBase + throwSourceSlot) {
        return ZR_FALSE;
    }
    payload = ZrCore_Stack_GetValue(slotBase + throwSourceSlot);
    if (payload == ZR_NULL || payload->type != ZR_VALUE_TYPE_STRING || payload->value.object == ZR_NULL) {
        return ZR_FALSE;
    }
    functionName = ZrCore_String_GetNativeString(ZR_CAST_STRING(state, payload->value.object));
    return (TZrBool)(functionName != ZR_NULL && strcmp(functionName, "aot-root-chain-smoke") == 0);
}

/* Delegate allocator calls unchanged, then sample only active root metadata. The
 * callback performs no allocation and does not trigger collection. */
static TZrPtr aot_shared_library_throw_root_observing_allocator(TZrPtr userData,
                                                                TZrPtr pointer,
                                                                TZrSize originalSize,
                                                                TZrSize newSize,
                                                                TZrInt64 flag) {
    SAotSharedLibraryThrowRootObservation *observation =
            (SAotSharedLibraryThrowRootObservation *)userData;
    TZrPtr result;
    SZrState *state;
    SZrAotGcRootFrame *activeRootFrame;
    TZrUInt32 remainingDepth;
    TZrBool isFixtureThrowErrorObjectAllocation = ZR_FALSE;

    if (observation == ZR_NULL || observation->originalAllocator == ZR_NULL) {
        return ZR_NULL;
    }

    state = observation->state;
    if (flag == (TZrInt64)ZR_MEMORY_NATIVE_TYPE_OBJECT && newSize > 0u &&
        !observation->targetThrowObjectAllocationAttempted && state != ZR_NULL &&
        state->aotGcRootFrameDepth > observation->outerDepth &&
        aot_shared_library_throw_root_is_normalizing_fixture_throw(state)) {
        /* NormalizeThrownValue checks the string payload without allocating,
         * then its first object allocation creates the Error object. Latch
         * before delegation so a failed allocation followed by GC cannot make
         * a nested collector allocation impersonate that Error allocation. */
        observation->targetThrowObjectAllocationAttempted = ZR_TRUE;
        isFixtureThrowErrorObjectAllocation = ZR_TRUE;
    }

    result = observation->originalAllocator(observation->originalAllocationArguments,
                                            pointer,
                                            originalSize,
                                            newSize,
                                            flag);
    if (newSize != 0u && result == ZR_NULL) {
        return result;
    }

    observation->allocatorCallCount++;
    if (!isFixtureThrowErrorObjectAllocation) {
        return result;
    }

    if (state == ZR_NULL) {
        return result;
    }

    if (state->aotGcRootFrameDepth <= observation->outerDepth || state->global == ZR_NULL ||
        state->global->garbageCollector == ZR_NULL) {
        return result;
    }

    observation->sawThrowNormalizeObjectAllocation = ZR_TRUE;
    observation->throwNormalizeObservedDepth = state->aotGcRootFrameDepth;
    remainingDepth = state->aotGcRootFrameDepth - observation->outerDepth;
    activeRootFrame = state->aotGcRootFrameStack;
    while (activeRootFrame != ZR_NULL && activeRootFrame != observation->outerTop &&
           remainingDepth > 0u) {
        const SZrAotGcRootMap *rootMap = activeRootFrame->rootMap;

        /* This fixture's generated entry has one LOCAL_ADDRESS pointer local in
         * stack slot 3, byte offset 0, with the ordinary object layout id 0. */
        if (rootMap != ZR_NULL && rootMap->rootCount == 1u && rootMap->roots != ZR_NULL &&
            activeRootFrame->frameBase != ZR_NULL && rootMap->roots[0].stackSlot == 3u &&
            rootMap->roots[0].frameByteOffset == 0u && rootMap->roots[0].typeLayoutId == 0u &&
            rootMap->roots[0].locationKind == (TZrUInt8)ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS) {
            const void *generatedRootSlotAddress =
                    (const TZrByte *)(const void *)activeRootFrame->frameBase +
                    rootMap->roots[0].frameByteOffset;
            SZrRawObject *generatedMappedObject = aot_shared_library_throw_root_find_live_object_in_slot(
                    state->global->garbageCollector, generatedRootSlotAddress);

            if (generatedMappedObject != ZR_NULL &&
                (uintptr_t)(void *)activeRootFrame != observation->outerRootFrameIdentity &&
                (uintptr_t)(void *)generatedMappedObject != observation->outerRootObjectIdentity) {
                observation->generatedRootFrameIdentity = (uintptr_t)(void *)activeRootFrame;
                observation->generatedFrameBaseIdentity = (uintptr_t)(void *)activeRootFrame->frameBase;
                observation->generatedManagedObjectIdentity = (uintptr_t)(void *)generatedMappedObject;
                observation->sawGeneratedManagedRootAtThrowNormalize = ZR_TRUE;
            }
        }

        activeRootFrame = activeRootFrame->previous;
        remainingDepth--;
    }
    return result;
}

static TZrBool aot_shared_library_throw_root_has_expected_exception_payload(SZrState *state) {
    SZrString *fieldName;
    SZrObject *errorObject;
    SZrTypeValue key;
    const SZrTypeValue *payload;
    const char *payloadText;

    if (state == ZR_NULL || !state->hasCurrentException ||
        state->currentException.type != ZR_VALUE_TYPE_OBJECT ||
        state->currentException.value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    fieldName = ZrCore_String_CreateFromNative(state, "exception");
    if (fieldName == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldName));
    key.type = ZR_VALUE_TYPE_STRING;
    errorObject = ZR_CAST_OBJECT(state, state->currentException.value.object);
    if (errorObject == ZR_NULL) {
        return ZR_FALSE;
    }

    payload = ZrCore_Object_GetValue(state, errorObject, &key);
    if (payload == ZR_NULL || payload->type != ZR_VALUE_TYPE_STRING || payload->value.object == ZR_NULL) {
        printf("AOT throw did not preserve its string payload in Error.exception\n");
        return ZR_FALSE;
    }
    payloadText = ZrCore_String_GetNativeString(ZR_CAST_STRING(state, payload->value.object));
    if (payloadText == ZR_NULL || strcmp(payloadText, "aot-root-chain-smoke") != 0) {
        printf("AOT throw payload was '%s', expected 'aot-root-chain-smoke'\n",
               payloadText != ZR_NULL ? payloadText : "<null>");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static void test_aot_c_generated_throw_restores_caller_root_through_full_gc(void) {
#if !defined(ZR_PLATFORM_UNIX) || !defined(__linux__)
    TEST_IGNORE_MESSAGE("Generated AOT throw-root integration requires the Linux shared-library toolchain");
#else
    const char *source =
            "class Marker {}\n"
            "fn throw_with_live_root(marker: object): int {\n"
            "    throw \"aot-root-chain-smoke\";\n"
            "}\n"
            "var marker: object = new Marker();\n"
            "return throw_with_live_root(marker);\n";
    const char *projectJson =
            "{"
            "\"name\":\"aot-runtime-throw-root\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}";
    SZrState *state = ZR_NULL;
    SZrFunction *function = ZR_NULL;
    SZrLibrary_Project *project = ZR_NULL;
    SZrGarbageCollector *collector = ZR_NULL;
    SZrAotGcRootSlot outerRootSlot;
    SZrAotGcRootMap outerRootMap;
    SZrAotGcRootFrame outerRootFrame;
    SAotSharedLibraryThrowRootObservation observation;
    SZrObject *outerObject = ZR_NULL;
    SZrRawObject *outerRoot = ZR_NULL;
    SZrRawObject *survivorObject = ZR_NULL;
    SZrRawObject *unrootedCanary = ZR_NULL;
    SZrCallInfo *callerCallInfo = ZR_NULL;
    SZrFunctionStackAnchor callerStackAnchor;
    SZrAotGcRootFrame *outerPreviousTop = ZR_NULL;
    SZrAotGcRootFrame *expectedOuterTop = ZR_NULL;
    TZrUInt32 outerPreviousDepth = 0u;
    TZrUInt32 expectedOuterDepth = 0u;
    SZrBinaryWriterOptions binaryOptions;
    SZrAotWriterOptions aotOptions;
    SZrTypeValue result;
    TZrBytePtr embeddedBlob = ZR_NULL;
    TZrSize embeddedBlobLength = 0u;
    TZrChar inputHash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH];
    TZrChar projectPath[ZR_TESTS_PATH_MAX] = {0};
    TZrChar sourcePath[ZR_TESTS_PATH_MAX] = {0};
    TZrChar binaryPath[ZR_TESTS_PATH_MAX] = {0};
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX] = {0};
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX] = {0};
    TZrChar compileLogPath[ZR_TESTS_PATH_MAX] = {0};
    char command[4096];
    char *generatedCText = ZR_NULL;
    TZrBool stateCreated = ZR_FALSE;
    TZrBool functionCompiled = ZR_FALSE;
    TZrBool generatedSourceWritten = ZR_FALSE;
    TZrBool generatedSourceHasPush = ZR_FALSE;
    TZrBool generatedSourceHasPop = ZR_FALSE;
    TZrBool generatedSourceHasThrow = ZR_FALSE;
    TZrBool generatedSourceHasDirectThrowCall = ZR_FALSE;
    TZrBool sharedLibraryCompiled = ZR_FALSE;
    TZrBool projectConfigured = ZR_FALSE;
    TZrBool outerObjectWasYoung = ZR_FALSE;
    TZrBool outerRootPushed = ZR_FALSE;
    TZrBool allocatorWrapperInstalled = ZR_FALSE;
    TZrBool executeEntryCalled = ZR_FALSE;
    TZrBool executeEntrySucceeded = ZR_TRUE;
    TZrBool expectedThrowPayloadCaptured = ZR_FALSE;
    TZrBool postThrowTopMatched = ZR_FALSE;
    TZrBool postThrowDepthMatched = ZR_FALSE;
    TZrBool fullCollectionRan = ZR_FALSE;
    TZrBool outerRootLiveBeforePostThrowCollection = ZR_FALSE;
    TZrBool outerRootSurvivedFullCollection = ZR_FALSE;
    TZrBool unrootedCanaryCreated = ZR_FALSE;
    TZrBool unrootedCanaryCollected = ZR_FALSE;
    TZrUInt64 fullCollectionsBefore = 0u;
    TZrBool outerRootFramePopped = ZR_FALSE;
    TZrBool unhandledAotExceptionCaptured = ZR_FALSE;
    TZrUInt32 postThrowDepth = 0u;
    int commandLength;
    int compilerStatus = -1;

    memset(&outerRootSlot, 0, sizeof(outerRootSlot));
    memset(&outerRootMap, 0, sizeof(outerRootMap));
    memset(&outerRootFrame, 0, sizeof(outerRootFrame));
    memset(&observation, 0, sizeof(observation));
    memset(&binaryOptions, 0, sizeof(binaryOptions));
    memset(&aotOptions, 0, sizeof(aotOptions));
    ZrCore_Value_ResetAsNull(&result);

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    if (state == ZR_NULL) {
        goto cleanup;
    }
    stateCreated = ZR_TRUE;

    aot_shared_library_throw_root_trace_stage("source compile start");
    function = aot_shared_library_throw_root_compile_source(state, source, "aot_throw_root_smoke.zr");
    if (function == ZR_NULL) {
        goto cleanup;
    }
    functionCompiled = ZR_TRUE;
    aot_shared_library_throw_root_trace_stage("source compile complete");

    if (!ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library_throw_root",
                                           "runtime_project",
                                           "throw_root",
                                           ".zrp",
                                           projectPath,
                                           sizeof(projectPath)) ||
        !ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library_throw_root",
                                           "runtime_project/src",
                                           "main",
                                           ".zr",
                                           sourcePath,
                                           sizeof(sourcePath)) ||
        !ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library_throw_root",
                                           "runtime_project/bin",
                                           "main",
                                           ".zro",
                                           binaryPath,
                                           sizeof(binaryPath)) ||
        !ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library_throw_root",
                                           "runtime_project/bin/aot_c/src",
                                           "main",
                                           ".c",
                                           generatedCPath,
                                           sizeof(generatedCPath)) ||
        !ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library_throw_root",
                                           "runtime_project/bin/aot_c/lib",
                                           "zrvm_aot_main",
                                           ".so",
                                           sharedLibraryPath,
                                           sizeof(sharedLibraryPath)) ||
        !ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library_throw_root",
                                           "runtime_project/logs",
                                           "compile_generated_aot",
                                           ".log",
                                           compileLogPath,
                                           sizeof(compileLogPath))) {
        goto cleanup;
    }

    if (!aot_shared_library_throw_root_write_text(projectPath, projectJson) ||
        !aot_shared_library_throw_root_write_text(sourcePath, source)) {
        goto cleanup;
    }

    binaryOptions.moduleName = "main";
    if (!ZrParser_Writer_WriteBinaryFileWithOptions(state, function, binaryPath, &binaryOptions) ||
        !aot_shared_library_throw_root_hash_file(binaryPath, inputHash, sizeof(inputHash)) ||
        !ZrTests_ReadFileBytes(binaryPath, &embeddedBlob, &embeddedBlobLength) || embeddedBlob == ZR_NULL ||
        embeddedBlobLength == 0u) {
        goto cleanup;
    }

    aotOptions.moduleName = "main";
    aotOptions.inputKind = ZR_AOT_INPUT_KIND_BINARY;
    aotOptions.inputHash = inputHash;
    aotOptions.embeddedModuleBlob = embeddedBlob;
    aotOptions.embeddedModuleBlobLength = embeddedBlobLength;
    aotOptions.requireExecutableLowering = ZR_TRUE;
    aot_shared_library_throw_root_trace_stage("AOT writer start");
    if (!ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &aotOptions)) {
        goto cleanup;
    }
    generatedSourceWritten = ZR_TRUE;
    aot_shared_library_throw_root_trace_stage("AOT writer complete");
    generatedCText = aot_shared_library_throw_root_read_text(generatedCPath);
    if (generatedCText == ZR_NULL) {
        goto cleanup;
    }
    generatedSourceHasPush = (TZrBool)(strstr(generatedCText, "ZrCore_Gc_AotRootFramePush") != ZR_NULL);
    generatedSourceHasPop = (TZrBool)(strstr(generatedCText, "ZrCore_Gc_AotRootFramePop") != ZR_NULL);
    generatedSourceHasThrow = (TZrBool)(strstr(generatedCText, "ZrLibrary_AotRuntime_Throw") != ZR_NULL);
    generatedSourceHasDirectThrowCall = (TZrBool)(strstr(generatedCText,
            "zr_aot_static_direct_succeeded = (TZrBool)(zr_aot_fn_1(state) != 0);") != ZR_NULL);

    commandLength = snprintf(command,
                             sizeof(command),
                             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
                             "-I\"%s/zr_vm_common/include\" "
                             "-I\"%s/zr_vm_core/include\" "
                             "-I\"%s/zr_vm_library/include\" "
                             "\"%s\" "
                             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
                             ZR_TESTS_AOT_C_RUNTIME_LINK_FLAGS
                             "-o \"%s\" > \"%s\" 2>&1",
                             ZR_VM_TESTS_C_COMPILER,
                             ZR_VM_TESTS_REPO_ROOT,
                             ZR_VM_TESTS_REPO_ROOT,
                             ZR_VM_TESTS_REPO_ROOT,
                             generatedCPath,
                             ZR_VM_TESTS_BUILD_LIB_DIR,
                             ZR_VM_TESTS_BUILD_LIB_DIR,
                             sharedLibraryPath,
                             compileLogPath);
    if (commandLength < 0 || (size_t)commandLength >= sizeof(command)) {
        goto cleanup;
    }
    aot_shared_library_throw_root_trace_stage("external compile start");
    compilerStatus = system(command);
    sharedLibraryCompiled = (TZrBool)(compilerStatus == 0);
    if (!sharedLibraryCompiled) {
        goto cleanup;
    }
    aot_shared_library_throw_root_trace_stage("external compile complete");

    ZrCore_Function_Free(state, function);
    function = ZR_NULL;

    project = ZrLibrary_Project_New(state, (TZrNativeString)projectJson, (TZrNativeString)projectPath);
    if (project == ZR_NULL) {
        goto cleanup;
    }
    state->global->userData = project;
    projectConfigured = ZrLibrary_AotRuntime_ConfigureGlobal(
            state->global, ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C, ZR_TRUE);
    if (!projectConfigured) {
        goto cleanup;
    }

    collector = state->global->garbageCollector;
    if (collector == ZR_NULL) {
        goto cleanup;
    }
    collector->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
    outerObject = ZrCore_Object_New(state, ZR_NULL);
    if (outerObject == ZR_NULL) {
        goto cleanup;
    }
    outerRoot = ZR_CAST_RAW_OBJECT_AS_SUPER(outerObject);
    outerObjectWasYoung = (TZrBool)(outerRoot->garbageCollectMark.storageKind ==
                                            ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE &&
                                    outerRoot->garbageCollectMark.heapGenerationKind ==
                                            ZR_GARBAGE_COLLECT_HEAP_GENERATION_KIND_YOUNG);

    outerRootSlot.typeLayoutId = 7u;
    outerRootSlot.locationKind = (TZrUInt8)ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS;
    outerRootMap.rootCount = 1u;
    outerRootMap.roots = &outerRootSlot;
    outerPreviousTop = state->aotGcRootFrameStack;
    outerPreviousDepth = state->aotGcRootFrameDepth;
    expectedOuterTop = &outerRootFrame;
    expectedOuterDepth = outerPreviousDepth + 1u;
    if (!ZrCore_Gc_AotRootFramePush(state,
                                    &outerRootFrame,
                                    (TZrStackValuePointer)(void *)&outerRoot,
                                    &outerRootMap)) {
        goto cleanup;
    }
    outerRootPushed = ZR_TRUE;

    observation.outerTop = expectedOuterTop;
    observation.outerDepth = expectedOuterDepth;
    observation.outerRootFrameIdentity = (uintptr_t)(void *)expectedOuterTop;
    observation.outerRootObjectIdentity = (uintptr_t)(void *)outerRoot;
    observation.state = state;
    observation.originalAllocator = state->global->allocator;
    observation.originalAllocationArguments = state->global->userAllocationArguments;
    if (observation.originalAllocator == ZR_NULL) {
        goto cleanup;
    }
    state->global->allocator = aot_shared_library_throw_root_observing_allocator;
    state->global->userAllocationArguments = &observation;
    allocatorWrapperInstalled = ZR_TRUE;

    executeEntryCalled = ZR_TRUE;
    callerCallInfo = state->callInfoList;
    ZrCore_Function_StackAnchorInit(state, state->stackTop.valuePointer, &callerStackAnchor);
    (void)aot_shared_library_throw_root_trace_live_object("before ExecuteEntry", collector, &outerRoot);
    aot_shared_library_throw_root_trace_stage("ExecuteEntry start");
    executeEntrySucceeded = ZrLibrary_AotRuntime_ExecuteEntry(
            state, ZR_AOT_BACKEND_KIND_C, &result);
    aot_shared_library_throw_root_trace_stage(executeEntrySucceeded
            ? "ExecuteEntry returned success" : "ExecuteEntry returned failure");
    state->global->allocator = observation.originalAllocator;
    state->global->userAllocationArguments = observation.originalAllocationArguments;
    allocatorWrapperInstalled = ZR_FALSE;

    /* Never evaluate or dereference the possibly abandoned generated-frame pointer. */
    postThrowTopMatched = (TZrBool)(memcmp(&state->aotGcRootFrameStack,
                                           &expectedOuterTop,
                                           sizeof(expectedOuterTop)) == 0);
    postThrowDepth = state->aotGcRootFrameDepth;
    postThrowDepthMatched = (TZrBool)(postThrowDepth == expectedOuterDepth);
    state->aotGcRootFrameStack = expectedOuterTop;
    state->aotGcRootFrameDepth = expectedOuterDepth;
    unhandledAotExceptionCaptured =
            (TZrBool)(state->hasCurrentException &&
                      state->currentExceptionStatus == ZR_THREAD_STATUS_RUNTIME_ERROR);
    expectedThrowPayloadCaptured = aot_shared_library_throw_root_has_expected_exception_payload(state);

    /* This gate owns only root-chain recovery, so retire the failed call's
     * retained VM/native frame window before the following collection. */
    state->callInfoList = callerCallInfo;
    state->stackTop.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callerStackAnchor);
    outerRootLiveBeforePostThrowCollection = aot_shared_library_throw_root_trace_live_object(
            "after ExecuteEntry", collector, &outerRoot);
    if (outerRootLiveBeforePostThrowCollection) {
        SZrObject *canary = ZrCore_Object_New(state, ZR_NULL);
        if (canary != ZR_NULL) {
            unrootedCanary = ZR_CAST_RAW_OBJECT_AS_SUPER(canary);
            unrootedCanaryCreated = ZR_TRUE;
        }
        fullCollectionsBefore = collector->statsSnapshot.fullCollectionCount;
        ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
        fullCollectionRan = (TZrBool)(collector->statsSnapshot.lastCollectionKind ==
                ZR_GARBAGE_COLLECT_COLLECTION_KIND_FULL &&
                collector->statsSnapshot.fullCollectionCount == fullCollectionsBefore + 1u);
        (void)aot_shared_library_throw_root_trace_live_object("after full GC", collector, &outerRoot);
    }
    if (fullCollectionRan) {
        survivorObject = aot_shared_library_throw_root_find_live_object_in_slot(collector, &outerRoot);
        outerRootSurvivedFullCollection = (TZrBool)(survivorObject != ZR_NULL);
        unrootedCanaryCollected = (TZrBool)(unrootedCanaryCreated &&
                aot_shared_library_throw_root_find_live_object_in_slot(collector, &unrootedCanary) == ZR_NULL);
    }

cleanup:
    if (state != ZR_NULL) {
        if (allocatorWrapperInstalled && state->global != ZR_NULL) {
            state->global->allocator = observation.originalAllocator;
            state->global->userAllocationArguments = observation.originalAllocationArguments;
        }

        if (outerRootPushed) {
            state->aotGcRootFrameStack = expectedOuterTop;
            state->aotGcRootFrameDepth = expectedOuterDepth;
            outerRootFramePopped = ZrCore_Gc_AotRootFramePop(state, &outerRootFrame);
            if (!outerRootFramePopped) {
                state->aotGcRootFrameStack = outerPreviousTop;
                state->aotGcRootFrameDepth = outerPreviousDepth;
            }
        }

        if (project != ZR_NULL) {
            state->global->userData = ZR_NULL;
            ZrLibrary_Project_Free(state, project);
        }
        if (function != ZR_NULL) {
            ZrCore_Function_Free(state, function);
        }
        ZrTests_Runtime_State_Destroy(state);
    }
    free(embeddedBlob);
    free(generatedCText);

    TEST_ASSERT_TRUE_MESSAGE(stateCreated, "runtime state creation must succeed");
    TEST_ASSERT_TRUE_MESSAGE(functionCompiled, "throw fixture source must compile");
    TEST_ASSERT_TRUE_MESSAGE(generatedSourceWritten, "AOT C source must be emitted");
    TEST_ASSERT_TRUE_MESSAGE(generatedSourceHasPush, "generated source must push an AOT root frame");
    TEST_ASSERT_TRUE_MESSAGE(generatedSourceHasPop, "generated source must contain its normal-path Pop");
    TEST_ASSERT_TRUE_MESSAGE(generatedSourceHasThrow, "generated source must call the AOT throw runtime");
    TEST_ASSERT_TRUE_MESSAGE(generatedSourceHasDirectThrowCall,
                             "the generated entry must directly call the compiled throw helper");
    TEST_ASSERT_TRUE_MESSAGE(sharedLibraryCompiled, "generated AOT shared library must compile");
    TEST_ASSERT_TRUE_MESSAGE(projectConfigured, "runtime loader project must configure for AOT C");
    TEST_ASSERT_TRUE_MESSAGE(outerObjectWasYoung, "outer root must start as a young movable object");
    TEST_ASSERT_TRUE_MESSAGE(executeEntryCalled, "runtime loader must execute the generated entry");
    TEST_ASSERT_FALSE_MESSAGE(executeEntrySucceeded, "the unhandled generated AOT throw must escape ExecuteEntry");
    TEST_ASSERT_TRUE_MESSAGE(unhandledAotExceptionCaptured,
                             "ExecuteEntry must retain the unhandled generated AOT exception");
    TEST_ASSERT_TRUE_MESSAGE(expectedThrowPayloadCaptured,
                             "the generated non-local Throw must preserve the exact fixture payload");
    TEST_ASSERT_TRUE_MESSAGE(observation.allocatorCallCount > 0u,
                             "the delegated allocator observer must run during generated entry execution");
    TEST_ASSERT_TRUE_MESSAGE(observation.sawThrowNormalizeObjectAllocation,
                             "the observer must run for the fixture THROW's Error allocation during normalization");
    TEST_ASSERT_TRUE_MESSAGE(observation.throwNormalizeObservedDepth > expectedOuterDepth,
                             "Throw normalization must observe a deeper root chain than the caller frame");
    TEST_ASSERT_TRUE_MESSAGE(observation.sawGeneratedManagedRootAtThrowNormalize,
                             "Throw normalization must see the generated LOCAL_ADDRESS map point to a live object");
    TEST_ASSERT_TRUE_MESSAGE(observation.generatedRootFrameIdentity != 0u &&
                                     observation.generatedRootFrameIdentity != observation.outerRootFrameIdentity,
                             "the observed root frame identity must be a generated frame, not the host root frame");
    TEST_ASSERT_TRUE_MESSAGE(observation.generatedFrameBaseIdentity != 0u,
                             "the observed generated root frame must retain its generated C frame-base identity");
    TEST_ASSERT_TRUE_MESSAGE(observation.generatedManagedObjectIdentity != 0u &&
                                     observation.generatedManagedObjectIdentity != observation.outerRootObjectIdentity,
                             "the mapped live managed object must be distinct from the manually rooted caller object");
    TEST_ASSERT_TRUE_MESSAGE(postThrowTopMatched,
                             "TryRun must restore the caller root top before returning from the generated throw");
    TEST_ASSERT_TRUE_MESSAGE(postThrowDepthMatched,
                             "TryRun must restore the caller root depth before returning from the generated throw");
    TEST_ASSERT_TRUE_MESSAGE(outerRootLiveBeforePostThrowCollection,
                             "the restored outer root must identify a live object before post-throw collection");
    TEST_ASSERT_TRUE_MESSAGE(fullCollectionRan, "the post-throw collection must complete a real full GC");
    TEST_ASSERT_TRUE_MESSAGE(outerRootSurvivedFullCollection,
                             "the outer LOCAL_ADDRESS root must identify a live object after full GC");
    TEST_ASSERT_TRUE_MESSAGE(unrootedCanaryCreated, "the unrooted collection control must be created");
    TEST_ASSERT_TRUE_MESSAGE(unrootedCanaryCollected,
                             "the full collection must release the unrooted control object");
    TEST_ASSERT_TRUE_MESSAGE(outerRootFramePopped, "the host outer root frame must be popped during cleanup");
#endif
}
