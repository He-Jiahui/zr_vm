#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/type_layout.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/aot_runtime.h"
#include "zr_vm_library/project.h"
#include "zr_vm_parser/writer.h"

typedef struct SZrAotLlvmReceiverAliasTestLoadedModule {
    EZrAotBackendKind backendKind;
    TZrChar *moduleName;
    TZrChar *sourcePath;
    TZrChar *zroPath;
    TZrChar *libraryPath;
    void *libraryHandle;
    const ZrAotCompiledModule *descriptor;
    const SZrAotCodeRegistration *codeRegistration;
    SZrFunction *moduleFunction;
    SZrFunction **functionTable;
    SZrGcNativeCallPin *functionPins;
    TZrUInt32 functionCount;
    TZrUInt32 functionCapacity;
    TZrUInt32 *generatedFrameSlotCounts;
    struct SZrObjectModule *module;
    TZrBool moduleExecuted;
} SZrAotLlvmReceiverAliasTestLoadedModule;

typedef struct SZrAotLlvmStackRelocationCapture {
    TZrBool hookCalled;
    TZrBool growSucceeded;
    TZrBool stackRelocated;
} SZrAotLlvmStackRelocationCapture;

typedef struct SZrAotLlvmMovingAllocatorContext {
    TZrUInt32 moveCount;
} SZrAotLlvmMovingAllocatorContext;

static SZrAotLlvmStackRelocationCapture g_aotLlvmStackRelocationCapture;

static TZrPtr aot_llvm_moving_allocator(TZrPtr userData,
                                        TZrPtr pointer,
                                        TZrSize originalSize,
                                        TZrSize newSize,
                                        TZrInt64 flag) {
    SZrAotLlvmMovingAllocatorContext *context =
            (SZrAotLlvmMovingAllocatorContext *)userData;
    TZrPtr newPointer;

    ZR_UNUSED_PARAMETER(flag);
    if (newSize == 0u) {
        if (pointer != ZR_NULL && pointer >= (TZrPtr)0x1000) {
            free(pointer);
        }
        return ZR_NULL;
    }
    if (pointer == ZR_NULL || pointer < (TZrPtr)0x1000) {
        return malloc(newSize);
    }

    newPointer = malloc(newSize);
    if (newPointer == ZR_NULL) {
        return ZR_NULL;
    }
    memcpy(newPointer,
           pointer,
           originalSize < newSize ? originalSize : newSize);
    free(pointer);
    if (context != ZR_NULL) {
        context->moveCount++;
    }
    return newPointer;
}

static SZrState *aot_llvm_create_state_with_moving_allocator(
        SZrAotLlvmMovingAllocatorContext *context) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global = ZrCore_GlobalState_New(
            aot_llvm_moving_allocator, context, 12345, &callbacks);

    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_GlobalState_InitRegistry(global->mainThreadState, global);
    return global->mainThreadState;
}

void setUp(void) {}

void tearDown(void) {}

static void assert_text_contains(const char *text, const char *needle) {
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(needle);
    TEST_ASSERT_NOT_NULL(strstr(text, needle));
}

static void assert_text_does_not_contain(const char *text, const char *needle) {
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(needle);
    TEST_ASSERT_NULL(strstr(text, needle));
}

static TZrInstruction test_create_instruction_2(EZrInstructionCode opcode,
                                                TZrUInt16 operandExtra,
                                                TZrUInt16 operandA,
                                                TZrUInt16 operandB) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)opcode;
    instruction.instruction.operandExtra = operandExtra;
    instruction.instruction.operand.operand1[0] = operandA;
    instruction.instruction.operand.operand1[1] = operandB;
    return instruction;
}

static TZrInt64 aot_llvm_receiver_alias_test_thunk(SZrState *state) {
    ZR_UNUSED_PARAMETER(state);
    return 1;
}

static void aot_llvm_force_call_hook_stack_relocation(SZrState *state,
                                                       SZrDebugInfo *debugInfo) {
    TZrStackValuePointer previousStackBase;
    TZrSize previousStackSize;

    if (state == ZR_NULL || debugInfo == ZR_NULL ||
        debugInfo->event != ZR_DEBUG_HOOK_EVENT_CALL ||
        g_aotLlvmStackRelocationCapture.hookCalled) {
        return;
    }

    previousStackBase = state->stackBase.valuePointer;
    previousStackSize =
            (TZrSize)(state->stackTail.valuePointer - previousStackBase);
    g_aotLlvmStackRelocationCapture.hookCalled = ZR_TRUE;
    g_aotLlvmStackRelocationCapture.growSucceeded =
            ZrCore_Stack_GrowTo(state, previousStackSize + 64u, ZR_TRUE);
    g_aotLlvmStackRelocationCapture.stackRelocated =
            g_aotLlvmStackRelocationCapture.growSucceeded &&
            state->stackBase.valuePointer != previousStackBase;
}

static SZrFunction *create_llvm_direct_call_fixture(SZrState *state) {
    SZrFunction *root;
    SZrFunction *child;

    TEST_ASSERT_NOT_NULL(state);
    root = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(root);

    root->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 3u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(root->instructionsList);
    root->instructionsList[0] = test_create_instruction_2(ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION), 1u, 0u, 0u);
    root->instructionsList[1] =
            test_create_instruction_2(ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS), 0u, 1u, 0u);
    root->instructionsList[2] = test_create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1u, 0u, 0u);
    root->instructionsLength = 3u;
    root->stackSize = 2u;
    root->parameterCount = 0u;
    root->lineInSourceStart = 1u;
    root->lineInSourceEnd = 3u;

    root->childFunctionList = (SZrFunction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunction),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(root->childFunctionList);
    memset(root->childFunctionList, 0, sizeof(SZrFunction));
    root->childFunctionLength = 1u;

    child = &root->childFunctionList[0];
    child->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(child->instructionsList);
    child->instructionsList[0] = test_create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 0u, 0u, 0u);
    child->instructionsLength = 1u;
    child->stackSize = 1u;
    child->parameterCount = 0u;
    child->ownerFunction = root;
    child->lineInSourceStart = 10u;
    child->lineInSourceEnd = 10u;
    return root;
}

static char *write_llvm_fixture(TZrBool stripGeneratedSymbols, TZrSize *outGeneratedLength) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedLlvmPath[ZR_TESTS_PATH_MAX];
    char *generatedLlvmText;

    TEST_ASSERT_NOT_NULL(state);
    function = create_llvm_direct_call_fixture(state);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = stripGeneratedSymbols ? "aot_llvm_symbol_stripping_private" : "aot_llvm_symbol_stripping_default";
    options.sourceHash = "aot-llvm-symbol-stripping";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-llvm-symbol-stripping";
    options.requireExecutableLowering = ZR_TRUE;
    options.stripGeneratedSymbols = stripGeneratedSymbols;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_llvm_symbol_stripping",
                                                       "generated",
                                                       stripGeneratedSymbols ? "stripped" : "default",
                                                       ".ll",
                                                       generatedLlvmPath,
                                                       sizeof(generatedLlvmPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFileWithOptions(state, function, generatedLlvmPath, &options));

    generatedLlvmText = ZrTests_ReadTextFile(generatedLlvmPath, outGeneratedLength);
    TEST_ASSERT_NOT_NULL(generatedLlvmText);
    ZrTests_Runtime_State_Destroy(state);
    return generatedLlvmText;
}

static void test_aot_llvm_default_preserves_generated_function_symbols(void) {
    TZrSize generatedLength = 0u;
    char *generatedLlvmText = write_llvm_fixture(ZR_FALSE, &generatedLength);

    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedLlvmText, "; symbol_stripping.generatedSymbols = 0");
    assert_text_contains(generatedLlvmText, "define internal i64 @zr_aot_fn_0(ptr %state)");
    assert_text_contains(generatedLlvmText, "define internal i64 @zr_aot_fn_1(ptr %state)");
    assert_text_contains(generatedLlvmText, "ptr @zr_aot_fn_0");
    assert_text_contains(generatedLlvmText, "ptr @zr_aot_fn_1");
    assert_text_contains(generatedLlvmText, "call i64 @zr_aot_fn_0(ptr %state)");
    assert_text_contains(generatedLlvmText, "call i64 @zr_aot_fn_1(ptr %state)");
    assert_text_contains(generatedLlvmText, "define ptr @ZrVm_GetAotCompiledModule()");

    free(generatedLlvmText);
}

static void test_aot_llvm_strip_generated_symbols_renames_private_function_symbols(void) {
    TZrSize generatedLength = 0u;
    char *generatedLlvmText = write_llvm_fixture(ZR_TRUE, &generatedLength);

    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedLlvmText, "; symbol_stripping.generatedSymbols = 1");
    assert_text_contains(generatedLlvmText, "define internal i64 @zr_fn_g0(ptr %state)");
    assert_text_contains(generatedLlvmText, "define internal i64 @zr_fn_g1(ptr %state)");
    assert_text_contains(generatedLlvmText, "ptr @zr_fn_g0");
    assert_text_contains(generatedLlvmText, "ptr @zr_fn_g1");
    assert_text_contains(generatedLlvmText, "call i64 @zr_fn_g0(ptr %state)");
    assert_text_contains(generatedLlvmText, "call i64 @zr_fn_g1(ptr %state)");
    assert_text_does_not_contain(generatedLlvmText, "@zr_aot_fn_");
    assert_text_contains(generatedLlvmText, "define ptr @ZrVm_GetAotCompiledModule()");

    free(generatedLlvmText);
}

static void test_aot_llvm_static_direct_call_borrows_readonly_receiver_storage(void) {
    static const TZrByte payload[] = {0x10u, 0x32u, 0x54u, 0x76u,
                                      0x98u, 0xbau, 0xdcu, 0xfeu};
    SZrAotLlvmMovingAllocatorContext allocatorContext = {0};
    SZrState *state =
            aot_llvm_create_state_with_moving_allocator(&allocatorContext);
    SZrLibrary_Project project;
    SZrFunction *callerFunction;
    SZrFunction *calleeFunction;
    SZrFunction *functionTable[2];
    SZrFunctionFrameSlotLayout *callerLayout;
    SZrFunctionFrameSlotLayout *calleeLayout;
    SZrTypeLayout receiverTypeLayout;
    const SZrTypeLayout *typeLayouts[1];
    TZrUInt32 frameSlotCounts[2];
    FZrAotEntryThunk functionPointers[2] = {
            aot_llvm_receiver_alias_test_thunk,
            aot_llvm_receiver_alias_test_thunk};
    SZrAotCodeRegistration codeRegistration;
    SZrAotLlvmReceiverAliasTestLoadedModule record;
    ZrAotGeneratedFrame frame;
    ZrAotGeneratedDirectCall directCall;
    SZrCallInfo *callerCallInfo;
    SZrClosureNative *callerClosure;
    TZrStackValuePointer callerFunctionBase;
    TZrStackValuePointer callerFrameBase;
    TZrStackValuePointer calleeFrameBase;
    SZrStackFramePlace sourcePlace;
    SZrStackFramePlace borrowedPlace;
    TZrUInt32 slotIndex;
    const TZrUInt32 sourceByteOffset =
            (TZrUInt32)(sizeof(SZrTypeValueOnStack) * 8u);
    const TZrUInt32 bindingByteOffset =
            (TZrUInt32)(sizeof(SZrTypeValueOnStack) * 4u);

    TEST_ASSERT_NOT_NULL(state);
    memset(&project, 0, sizeof(project));
    memset(&receiverTypeLayout, 0, sizeof(receiverTypeLayout));
    memset(&codeRegistration, 0, sizeof(codeRegistration));
    memset(&record, 0, sizeof(record));
    memset(&frame, 0, sizeof(frame));
    memset(&directCall, 0, sizeof(directCall));

    callerFunction = ZrCore_Function_New(state);
    calleeFunction = ZrCore_Function_New(state);
    callerLayout = (SZrFunctionFrameSlotLayout *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(*callerLayout),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    calleeLayout = (SZrFunctionFrameSlotLayout *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(*calleeLayout),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(callerFunction);
    TEST_ASSERT_NOT_NULL(calleeFunction);
    TEST_ASSERT_NOT_NULL(callerLayout);
    TEST_ASSERT_NOT_NULL(calleeLayout);
    memset(callerLayout, 0, sizeof(*callerLayout));
    memset(calleeLayout, 0, sizeof(*calleeLayout));

    ZrCore_TypeLayout_InitStruct(
            &receiverTypeLayout,
            (TZrUInt32)sizeof(payload),
            (TZrUInt32)_Alignof(TZrUInt64),
            ZR_TYPE_LAYOUT_COPY_KIND_BITWISE,
            ZR_TYPE_LAYOUT_DROP_KIND_NONE,
            ZR_NULL,
            0u);
    typeLayouts[0] = &receiverTypeLayout;

    callerLayout->stackSlot = 1u;
    callerLayout->byteOffset = sourceByteOffset;
    callerLayout->byteSize = (TZrUInt32)sizeof(payload);
    callerLayout->byteAlign = (TZrUInt32)_Alignof(TZrUInt64);
    callerLayout->typeLayoutId = 0u;
    callerLayout->slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT;
    callerLayout->reserved0 =
            ZR_FUNCTION_FRAME_SLOT_FLAG_ALIAS |
            ZR_FUNCTION_FRAME_SLOT_FLAG_INLINE_RECEIVER_ARGUMENT;
    callerFunction->stackSize = 3u;
    callerFunction->frameSlotLayouts = callerLayout;
    callerFunction->frameSlotLayoutLength = 1u;
    callerFunction->frameByteSize = sourceByteOffset + (TZrUInt32)sizeof(payload);
    callerFunction->frameByteAlign = callerLayout->byteAlign;

    calleeLayout->stackSlot = 0u;
    calleeLayout->byteOffset = bindingByteOffset;
    calleeLayout->byteSize = (TZrUInt32)sizeof(payload);
    calleeLayout->byteAlign = (TZrUInt32)_Alignof(TZrUInt64);
    calleeLayout->typeLayoutId = 0u;
    calleeLayout->slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT;
    calleeLayout->isParameter = 1u;
    calleeLayout->reserved0 =
            ZR_FUNCTION_FRAME_SLOT_FLAG_ALIAS |
            ZR_FUNCTION_FRAME_SLOT_FLAG_INDIRECT_ALIAS |
            ZR_FUNCTION_FRAME_SLOT_FLAG_BORROWED_ALIAS;
    calleeFunction->stackSize = 1u;
    calleeFunction->parameterCount = 1u;
    calleeFunction->frameSlotLayouts = calleeLayout;
    calleeFunction->frameSlotLayoutLength = 1u;
    calleeFunction->frameByteSize =
            bindingByteOffset +
            (TZrUInt32)sizeof(SZrFunctionFrameBorrowedAliasBinding);
    calleeFunction->frameByteAlign =
            (TZrUInt32)_Alignof(SZrFunctionFrameBorrowedAliasBinding);

    functionTable[0] = callerFunction;
    functionTable[1] = calleeFunction;
    frameSlotCounts[0] =
            (TZrUInt32)ZrCore_Function_GetFrameStorageSlotCount(callerFunction);
    frameSlotCounts[1] =
            (TZrUInt32)ZrCore_Function_GetFrameStorageSlotCount(calleeFunction);
    codeRegistration.functionCount = 2u;
    codeRegistration.functionPointers = functionPointers;
    codeRegistration.typeLayouts = typeLayouts;
    codeRegistration.typeLayoutCount = 1u;
    callerFunction->metadataCodeRegistration = &codeRegistration;
    callerFunction->metadataTypeLayoutCount = 1u;
    calleeFunction->metadataCodeRegistration = &codeRegistration;
    calleeFunction->metadataTypeLayoutCount = 1u;
    record.backendKind = ZR_AOT_BACKEND_KIND_LLVM;
    record.codeRegistration = &codeRegistration;
    record.moduleFunction = callerFunction;
    record.functionTable = functionTable;
    record.functionCount = 2u;
    record.functionCapacity = 2u;
    record.generatedFrameSlotCounts = frameSlotCounts;

    state->global->userData = &project;
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(
            state->global,
            ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_LLVM,
            ZR_FALSE));

    callerFunctionBase = ZrCore_Function_CheckStackAndGc(
            state, 64u, state->stackTop.valuePointer);
    TEST_ASSERT_NOT_NULL(callerFunctionBase);
    for (slotIndex = 0u; slotIndex < 64u; slotIndex++) {
        ZrCore_Value_ResetAsNull(
                ZrCore_Stack_GetValue(callerFunctionBase + slotIndex));
    }
    callerClosure = ZrCore_ClosureNative_New(state, 0u);
    TEST_ASSERT_NOT_NULL(callerClosure);
    callerClosure->aotShimFunction = callerFunction;
    ZrCore_Value_InitAsRawObject(
            state,
            ZrCore_Stack_GetValue(callerFunctionBase),
            ZR_CAST_RAW_OBJECT_AS_SUPER(callerClosure));
    ZrCore_Stack_GetValue(callerFunctionBase)->type = ZR_VALUE_TYPE_CLOSURE;
    ZrCore_Stack_GetValue(callerFunctionBase)->isNative = ZR_TRUE;
    ZrCore_Stack_GetValue(callerFunctionBase)->isGarbageCollectable = ZR_TRUE;
    callerFrameBase = callerFunctionBase + 1u;
    ZrCore_Function_InitializeFrameLayoutStorage(
            state, callerFunctionBase, callerFunction, 0u);
    TEST_ASSERT_TRUE(ZrCore_Function_MakeFrameSlotPlace(
            state, callerFunction, callerFrameBase, 1u, &sourcePlace));
    memcpy(sourcePlace.address, payload, sizeof(payload));

    callerCallInfo = &state->baseCallInfo;
    memset(callerCallInfo, 0, sizeof(*callerCallInfo));
    callerCallInfo->metadataFunction = callerFunction;
    callerCallInfo->functionBase.valuePointer = callerFunctionBase;
    callerCallInfo->functionTop.valuePointer =
            callerFrameBase + frameSlotCounts[0];
    state->callInfoList = callerCallInfo;
    state->stackTop.valuePointer = callerCallInfo->functionTop.valuePointer;

    frame.recordHandle = &record;
    frame.function = callerFunction;
    frame.callInfo = callerCallInfo;
    frame.slotBase = callerFrameBase;
    frame.functionIndex = 0u;
    frame.generatedFrameSlotCount = frameSlotCounts[0];

    memset(&g_aotLlvmStackRelocationCapture,
           0,
           sizeof(g_aotLlvmStackRelocationCapture));
    ZrCore_Debug_SetHook(state,
                         aot_llvm_force_call_hook_stack_relocation,
                         ZR_DEBUG_HOOK_MASK_CALL,
                         0u);
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_PrepareStaticDirectCall(
            state, &frame, 2u, 0u, 1u, 1u, &directCall));
    ZrCore_Debug_SetHook(state, ZR_NULL, 0u, 0u);
    TEST_ASSERT_TRUE(g_aotLlvmStackRelocationCapture.hookCalled);
    TEST_ASSERT_TRUE(g_aotLlvmStackRelocationCapture.growSucceeded);
    TEST_ASSERT_TRUE(g_aotLlvmStackRelocationCapture.stackRelocated);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, allocatorContext.moveCount);
    TEST_ASSERT_TRUE(directCall.prepared);
    TEST_ASSERT_NOT_NULL(directCall.calleeCallInfo);
    callerFrameBase = callerCallInfo->functionBase.valuePointer + 1u;
    TEST_ASSERT_EQUAL_PTR(
            callerFrameBase + frameSlotCounts[0],
            directCall.calleeCallInfo->functionBase.valuePointer);
    TEST_ASSERT_EQUAL_PTR(
            directCall.calleeCallInfo->functionBase.valuePointer + 1u + frameSlotCounts[1],
            directCall.calleeCallInfo->functionTop.valuePointer);

    calleeFrameBase = directCall.calleeCallInfo->functionBase.valuePointer + 1u;
    TEST_ASSERT_TRUE(ZrCore_Function_MakeFrameSlotPlace(
            state, calleeFunction, calleeFrameBase, 0u, &borrowedPlace));
    TEST_ASSERT_TRUE(ZrCore_Function_MakeFrameSlotPlace(
            state, callerFunction, callerFrameBase, 1u, &sourcePlace));
    TEST_ASSERT_EQUAL_PTR(sourcePlace.address, borrowedPlace.address);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, borrowedPlace.address, sizeof(payload));

    ((TZrByte *)borrowedPlace.address)[0] = 0x5au;
    TEST_ASSERT_EQUAL_HEX8(0x5au, ((const TZrByte *)sourcePlace.address)[0]);
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_FinishDirectCall(
            state, &frame, &directCall, 0u));

    ZrLibrary_AotRuntime_FreeProjectState(state, &project);
    state->global->userData = ZR_NULL;
    ZrCore_GlobalState_Free(state->global);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_llvm_default_preserves_generated_function_symbols);
    RUN_TEST(test_aot_llvm_strip_generated_symbols_renames_private_function_symbols);
    RUN_TEST(test_aot_llvm_static_direct_call_borrows_readonly_receiver_storage);
    return UNITY_END();
}
