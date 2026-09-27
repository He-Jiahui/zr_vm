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

// 测试仅借用运行时私有记录的前缀字段；recordHandle 所读字段的顺序须保持一致。
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

// 捕获调用钩子是否确实触发扩栈及地址迁移，供借用别名断言区分路径。
typedef struct SZrAotLlvmStackRelocationCapture {
    TZrBool hookCalled;
    TZrBool growSucceeded;
    TZrBool stackRelocated;
} SZrAotLlvmStackRelocationCapture;

// 统计主动迁移次数，确认测试没有只在原地址扩容。
typedef struct SZrAotLlvmMovingAllocatorContext {
    TZrUInt32 moveCount;
} SZrAotLlvmMovingAllocatorContext;

// 调试钩子没有用户上下文；每个迁移场景开始前清零该短期捕获状态。
static SZrAotLlvmStackRelocationCapture g_aotLlvmStackRelocationCapture;

// 先分配并复制再释放旧块，强制真实地址迁移以暴露悬空栈指针。
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

// 用移动分配器建立独立 VM 状态；成功后由测试释放所属 global。
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

// BUG: 生成文本或建立 VM global 后若断言失败，函数尾部 free/Destroy 被 Unity TEST_ABORT 跳过；空 tearDown 无法清理资源（unity.c:2296-2303）。
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

// 生成子函数获取和静态直接调用指令，令 LLVM writer 输出真实调用关系。
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

// 稳定的生成入口用于校验函数表与注册表指向同一 callee。
static TZrInt64 aot_llvm_receiver_alias_test_thunk(SZrState *state) {
    ZR_UNUSED_PARAMETER(state);
    return 1;
}

// 不同入口专用于模拟 thunk 身份漂移的拒绝路径。
static TZrInt64 aot_llvm_receiver_alias_drift_test_thunk(SZrState *state) {
    ZR_UNUSED_PARAMETER(state);
    return 2;
}

// 静态调用身份校验失败后，输出必须归零且 caller 帧、目标槽和借用源字节不变。
static void assert_aot_llvm_static_direct_call_identity_drift_preserves_caller(
        SZrState *state,
        ZrAotGeneratedFrame *frame,
        ZrAotGeneratedDirectCall *directCall,
        const TZrPtr sourceAddress,
        const TZrByte *sourceBytes,
        TZrSize sourceByteCount) {
    ZrAotGeneratedDirectCall emptyDirectCall = {0};
    ZrAotGeneratedFrame frameSnapshot;
    SZrCallInfo callerCallInfoSnapshot;
    SZrTypeValue destinationSnapshot;
    TZrStackValuePointer stackTopSnapshot;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(frame);
    TEST_ASSERT_NOT_NULL(frame->callInfo);
    TEST_ASSERT_NOT_NULL(directCall);
    TEST_ASSERT_NOT_NULL(sourceAddress);
    TEST_ASSERT_NOT_NULL(sourceBytes);

    frameSnapshot = *frame;
    callerCallInfoSnapshot = *frame->callInfo;
    destinationSnapshot = frame->slotBase[2u].value;
    stackTopSnapshot = state->stackTop.valuePointer;
    memset(directCall, 0xa5, sizeof(*directCall));

    TEST_ASSERT_FALSE(ZrLibrary_AotRuntime_PrepareStaticDirectCall(
            state, frame, 2u, 0u, 1u, 1u, directCall));
    TEST_ASSERT_EQUAL_MEMORY(&emptyDirectCall, directCall, sizeof(*directCall));
    TEST_ASSERT_EQUAL_PTR(frameSnapshot.callInfo, state->callInfoList);
    TEST_ASSERT_EQUAL_PTR(stackTopSnapshot, state->stackTop.valuePointer);
    TEST_ASSERT_EQUAL_MEMORY(&frameSnapshot, frame, sizeof(*frame));
    TEST_ASSERT_EQUAL_MEMORY(
            &callerCallInfoSnapshot, frame->callInfo, sizeof(*frame->callInfo));
    TEST_ASSERT_EQUAL_MEMORY(
            &destinationSnapshot, &frame->slotBase[2u].value, sizeof(destinationSnapshot));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(sourceBytes, sourceAddress, sourceByteCount);
    TEST_ASSERT_EQUAL(ZR_THREAD_STATUS_RUNTIME_ERROR, state->threadStatus);

    state->threadStatus = ZR_THREAD_STATUS_FINE;
}

// 只在首次 CALL 钩子扩栈，检验参数借用在回调导致的栈迁移后仍有效。
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

// 构造根函数调用子函数的最小指令树，使符号改名同时覆盖定义和引用。
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

// 按 stripGeneratedSymbols 生成 LLVM IR；返回由调用方 free 的文本，VM 状态在此释放。
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

// 默认模式保留可读的函数符号，并验证定义、表项和直接调用一致。
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

// 剥离模式只重命名私有函数；外部模块描述符入口仍保持公开名称。
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

// 真实运行时准备调用时，接收者别名须在身份拒绝和扩栈后仍指向 caller 存储。
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
    SZrFunction *generatedFunctionTable[2];
    SZrFunctionFrameSlotLayout *callerLayout;
    SZrFunctionFrameSlotLayout *calleeLayout;
    SZrTypeLayout receiverTypeLayout;
    const SZrTypeLayout *typeLayouts[1];
    TZrUInt32 frameSlotCounts[2];
    FZrAotEntryThunk functionPointers[2] = {
            aot_llvm_receiver_alias_test_thunk,
            aot_llvm_receiver_alias_test_thunk};
    FZrAotEntryThunk generatedFunctionPointers[2] = {
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
    generatedFunctionTable[0] = callerFunction;
    generatedFunctionTable[1] = calleeFunction;
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
    frame.functionTable = generatedFunctionTable;
    frame.functionCount = 2u;
    frame.codeRegistration = &codeRegistration;
    frame.functionThunks = generatedFunctionPointers;
    frame.functionThunkCount = 2u;

    // 分别破坏函数对象和 thunk 身份，失败必须发生在任何 callee 帧写入之前。
    generatedFunctionTable[1] = callerFunction;
    assert_aot_llvm_static_direct_call_identity_drift_preserves_caller(
            state,
            &frame,
            &directCall,
            sourcePlace.address,
            payload,
            sizeof(payload));
    generatedFunctionTable[1] = calleeFunction;

    generatedFunctionPointers[1] = aot_llvm_receiver_alias_drift_test_thunk;
    assert_aot_llvm_static_direct_call_identity_drift_preserves_caller(
            state,
            &frame,
            &directCall,
            sourcePlace.address,
            payload,
            sizeof(payload));
    generatedFunctionPointers[1] = aot_llvm_receiver_alias_test_thunk;

    // CALL 钩子在准备 callee 时移动整段栈，迫使后续位置通过新基址重取。
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

    // 修改 callee 借用视图应立即反映到 caller 原槽，证明没有复制出临时接收者。
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
