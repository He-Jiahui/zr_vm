#include <stdlib.h>
#include <string.h>

#include "unity.h"
#include "runtime_support.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_parser.h"

/* 由 VM 的调试 hook 回调写入、Unity 断言读取；一次测试只驱动一个 state。 */
typedef struct SZrCoreHookCapture {
    TZrUInt32 countEvents;
    TZrUInt32 lineEvents;
    TZrUInt32 duplicateLineEvents;
    TZrUInt32 callEvents;
    TZrUInt32 returnEvents;
    TZrUInt32 lastLine;
    TZrBool sawInnerFrame;
    TZrBool sawOuterFrame;
    TZrBool lineOnlyRespectedTypeMask;
    char innerName[64];
    char outerName[64];
    char sourceName[128];
} SZrCoreHookCapture;

static SZrCoreHookCapture g_hookCapture;

static void hook_capture_reset(void) {
    memset(&g_hookCapture, 0, sizeof(g_hookCapture));
}

static SZrFunction *compile_source(SZrState *state, const char *source, const char *sourceLabel) {
    SZrString *sourceName;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_NOT_NULL(sourceLabel);

    sourceName = ZrCore_String_Create(state, (TZrNativeString)sourceLabel, strlen(sourceLabel));
    TEST_ASSERT_NOT_NULL(sourceName);
    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static void count_capture_hook(struct SZrState *state, SZrDebugInfo *debugInfo) {
    /* 同一回调接收 count、line 和调用边界事件，以核对掩码与去重语义。 */
    ZR_UNUSED_PARAMETER(state);

    if (debugInfo == ZR_NULL) {
        return;
    }

    if (debugInfo->event == ZR_DEBUG_HOOK_EVENT_COUNT) {
        g_hookCapture.countEvents++;
    } else if (debugInfo->event == ZR_DEBUG_HOOK_EVENT_LINE) {
        if (debugInfo->currentLine != 0 && debugInfo->currentLine == g_hookCapture.lastLine) {
            g_hookCapture.duplicateLineEvents++;
        }
        g_hookCapture.lastLine = (TZrUInt32)debugInfo->currentLine;
        g_hookCapture.lineEvents++;
    } else if (debugInfo->event == ZR_DEBUG_HOOK_EVENT_CALL) {
        g_hookCapture.callEvents++;
    } else if (debugInfo->event == ZR_DEBUG_HOOK_EVENT_RETURN) {
        g_hookCapture.returnEvents++;
    }
}

static void stack_capture_hook(struct SZrState *state, SZrDebugInfo *debugInfo) {
    /* 只在内层函数的活动帧读栈；离开 hook 后 activation 不再作为快照使用。 */
    SZrDebugActivation innerActivation;
    SZrDebugActivation outerActivation;
    SZrDebugInfo innerInfo;
    SZrDebugInfo outerInfo;
    SZrDebugInfo lineOnlyInfo;

    if (state == ZR_NULL || debugInfo == ZR_NULL || debugInfo->event != ZR_DEBUG_HOOK_EVENT_LINE) {
        return;
    }

    memset(&innerActivation, 0, sizeof(innerActivation));
    memset(&outerActivation, 0, sizeof(outerActivation));
    if (!ZrCore_Debug_GetStack(state, 0, &innerActivation) ||
        !ZrCore_Debug_GetStack(state, 1, &outerActivation)) {
        return;
    }

    memset(&innerInfo, 0, sizeof(innerInfo));
    if (ZrCore_Debug_GetInfo(state,
                             &innerActivation,
                             (EZrDebugInfoType)(ZR_DEBUG_INFO_FUNCTION_NAME |
                                                ZR_DEBUG_INFO_SOURCE_FILE |
                                                ZR_DEBUG_INFO_LINE_NUMBER |
                                                ZR_DEBUG_INFO_CLOSURE),
                             &innerInfo) &&
        innerInfo.name != ZR_NULL &&
        strcmp(innerInfo.name, "inner") == 0) {
        g_hookCapture.sawInnerFrame = ZR_TRUE;
        snprintf(g_hookCapture.innerName, sizeof(g_hookCapture.innerName), "%s", innerInfo.name);
        snprintf(g_hookCapture.sourceName,
                 sizeof(g_hookCapture.sourceName),
                 "%s",
                 innerInfo.source != ZR_NULL ? innerInfo.source : "");
        /* BUG: 断言失败时 Unity longjmp 跳过 ZrCore_Debug_Hook 的重锁和栈顶恢复；
         * 应把检查结果记入 capture，返回 hook 后再由用例断言。 */
        TEST_ASSERT_TRUE(innerInfo.currentLine > 0);
        TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)innerInfo.parametersCount);

        memset(&outerInfo, 0, sizeof(outerInfo));
        if (ZrCore_Debug_GetInfo(state,
                                 &outerActivation,
                                 (EZrDebugInfoType)(ZR_DEBUG_INFO_FUNCTION_NAME |
                                                    ZR_DEBUG_INFO_LINE_NUMBER),
                                 &outerInfo) &&
            outerInfo.name != ZR_NULL &&
            strcmp(outerInfo.name, "outer") == 0 &&
            outerInfo.currentLine > 0) {
            g_hookCapture.sawOuterFrame = ZR_TRUE;
            snprintf(g_hookCapture.outerName, sizeof(g_hookCapture.outerName), "%s", outerInfo.name);
        }

        memset(&lineOnlyInfo, 0, sizeof(lineOnlyInfo));
        if (ZrCore_Debug_GetInfo(state,
                                 &innerActivation,
                                 ZR_DEBUG_INFO_LINE_NUMBER,
                                 &lineOnlyInfo) &&
            lineOnlyInfo.currentLine > 0 &&
            lineOnlyInfo.name == ZR_NULL &&
            lineOnlyInfo.source == ZR_NULL &&
            lineOnlyInfo.parametersCount == 0) {
            g_hookCapture.lineOnlyRespectedTypeMask = ZR_TRUE;
        }
    }
}

static void test_count_hook_fires_for_each_instruction_period_and_can_be_disabled(void) {
    const char *source =
            "var value = 1;\n"
            "value = value + 2;\n"
            "value = value + 3;\n"
            "return value;";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "debug_hook_count.zr");
    TEST_ASSERT_NOT_NULL(function);

    hook_capture_reset();
    ZrCore_Debug_SetHook(state, count_capture_hook, ZR_DEBUG_HOOK_MASK_COUNT, 1u);
    TEST_ASSERT_EQUAL_PTR(count_capture_hook, ZrCore_Debug_GetHook(state));
    TEST_ASSERT_EQUAL_UINT32(ZR_DEBUG_HOOK_MASK_COUNT, ZrCore_Debug_GetHookMask(state));
    TEST_ASSERT_EQUAL_UINT32(1u, ZrCore_Debug_GetHookCount(state));

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(6, result);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)function->instructionsLength, g_hookCapture.countEvents);

    hook_capture_reset();
    ZrCore_Debug_SetHook(state, ZR_NULL, 0u, 0u);
    TEST_ASSERT_NULL(ZrCore_Debug_GetHook(state));
    TEST_ASSERT_EQUAL_UINT32(0u, ZrCore_Debug_GetHookMask(state));
    TEST_ASSERT_EQUAL_UINT32(0u, ZrCore_Debug_GetHookCount(state));
    TEST_ASSERT_EQUAL_UINT32(0u, state->debugHookSignal);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(6, result);
    TEST_ASSERT_EQUAL_UINT32(0u, g_hookCapture.countEvents);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_line_and_count_hooks_are_independent_and_line_events_are_deduplicated(void) {
    const char *source =
            "var first = 1 + 2 + 3;\n"
            "var second = first + 4;\n"
            "return second;";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "debug_hook_line_count.zr");
    TEST_ASSERT_NOT_NULL(function);

    hook_capture_reset();
    ZrCore_Debug_SetHook(state,
                         count_capture_hook,
                         (TZrUInt32)(ZR_DEBUG_HOOK_MASK_LINE | ZR_DEBUG_HOOK_MASK_COUNT),
                         1u);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(10, result);
    TEST_ASSERT_TRUE(g_hookCapture.countEvents > 0);
    TEST_ASSERT_TRUE(g_hookCapture.lineEvents >= 2u);
    TEST_ASSERT_EQUAL_UINT32(0u, g_hookCapture.duplicateLineEvents);

    ZrCore_Debug_SetHook(state, ZR_NULL, 0u, 0u);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_getstack_and_getinfo_resolve_nested_frames_and_respect_type_mask(void) {
    /* 同时验证帧顺序和字段掩码，防止调试接口把未请求的名称/来源混入结果。 */
    const char *source =
            "fn inner(value: int): int {\n"
            "    var local = value + 1;\n"
            "    return local;\n"
            "}\n"
            "fn outer(value: int): int {\n"
            "    var result = inner(value);\n"
            "    return result;\n"
            "}\n"
            "return outer(4);";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "debug_hook_stack.zr");
    TEST_ASSERT_NOT_NULL(function);

    hook_capture_reset();
    ZrCore_Debug_SetHook(state, stack_capture_hook, ZR_DEBUG_HOOK_MASK_LINE, 0u);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(5, result);
    TEST_ASSERT_TRUE(g_hookCapture.sawInnerFrame);
    TEST_ASSERT_TRUE(g_hookCapture.sawOuterFrame);
    TEST_ASSERT_TRUE(g_hookCapture.lineOnlyRespectedTypeMask);
    TEST_ASSERT_EQUAL_STRING("inner", g_hookCapture.innerName);
    TEST_ASSERT_EQUAL_STRING("outer", g_hookCapture.outerName);
    TEST_ASSERT_EQUAL_STRING("debug_hook_stack.zr", g_hookCapture.sourceName);

    ZrCore_Debug_SetHook(state, ZR_NULL, 0u, 0u);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

/* 只搬迁 VM 栈块的 allocator：扩栈必定换地址，旧块清零后保留到用例结束。
 * 仍按扩栈前地址读取的代码因此稳定读到 null，而不是碰巧尚未被复用的旧内容。 */
typedef struct SZrStackMovingAllocator {
    TZrPtr retired[8];
    TZrUInt32 retiredCount;
} SZrStackMovingAllocator;

static TZrPtr stack_moving_allocator(TZrPtr userData,
                                     TZrPtr pointer,
                                     TZrSize originalSize,
                                     TZrSize newSize,
                                     TZrInt64 flag) {
    SZrStackMovingAllocator *allocator = (SZrStackMovingAllocator *)userData;
    TZrPtr movedPointer;

    /* 首次分配、释放以及非栈块沿用测试默认 allocator，只拦截栈的重分配。 */
    if (flag != ZR_MEMORY_NATIVE_TYPE_STACK || pointer == ZR_NULL || newSize == 0u) {
        return ZrTests_Runtime_Allocator_Default(ZR_NULL, pointer, originalSize, newSize, flag);
    }
    if (allocator->retiredCount >= ZR_ARRAY_COUNT(allocator->retired)) {
        return ZR_NULL;
    }

    movedPointer = malloc(newSize);
    if (movedPointer == ZR_NULL) {
        return ZR_NULL;
    }
    memcpy(movedPointer, pointer, originalSize < newSize ? originalSize : newSize);
    memset(pointer, 0, originalSize);
    allocator->retired[allocator->retiredCount++] = pointer;
    return movedPointer;
}

/* 与常规运行时夹具保持相同 registry 初始化，但替换底层 allocator。 */
static SZrState *create_state_with_stack_moving_allocator(SZrStackMovingAllocator *allocator) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global = ZrCore_GlobalState_New(stack_moving_allocator, allocator, 12345, &callbacks);
    SZrState *state;

    if (global == ZR_NULL) {
        return ZR_NULL;
    }

    state = global->mainThreadState;
    if (state != ZR_NULL) {
        ZrCore_GlobalState_InitRegistry(state, global);
    }
    return state;
}

/* VM 只认得搬迁后的栈块；保留下来的旧块由用例在 state 释放后归还。 */
static void destroy_state_with_stack_moving_allocator(SZrState *state, SZrStackMovingAllocator *allocator) {
    TZrUInt32 index;

    if (state != ZR_NULL) {
        ZrCore_GlobalState_Free(state->global);
    }
    for (index = 0u; index < allocator->retiredCount; index++) {
        free(allocator->retired[index]);
    }
    allocator->retiredCount = 0u;
}

static void test_getinfo_push_function_rereads_callable_after_stack_relocation(void) {
    /* PUSH_FUNCTION 在栈满时先扩栈再复制 callable。扩栈会搬迁整段栈，
     * 复制源必须取自搬迁后的函数槽，否则压入的是旧块里的失效内容。 */
    SZrStackMovingAllocator allocator;
    SZrState *state;
    SZrFunction *function;
    SZrDebugActivation activation;
    SZrDebugInfo info;
    TZrStackValuePointer functionSlot;
    TZrStackValuePointer slot;
    TZrStackValuePointer stackBaseBefore;
    TZrStackValuePointer relocatedTop;
    TZrMemoryOffset stackTopOffset;
    const SZrTypeValue *pushedValue;
    const SZrTypeValue *frameValue;
    TZrBool gotStack;
    TZrBool resolvedFunction;
    TZrBool gotInfo;
    TZrBool stackMoved;
    TZrBool pushedOneSlot;
    TZrBool pushedCallable = ZR_FALSE;
    TZrBool frameKeepsCallable;

    memset(&allocator, 0, sizeof(allocator));
    state = create_state_with_stack_moving_allocator(&allocator);
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_EQUAL_PTR(&state->baseCallInfo, state->callInfoList);
    TEST_ASSERT_TRUE(state->stackTail.valuePointer - state->stackTop.valuePointer >= 1);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    /* 把入口帧改成贴着 stackTail 的调用窗口：函数槽是最后一个逻辑槽，
     * 栈顶等于 stackTail，下一次压栈必须扩栈。 */
    functionSlot = state->stackTail.valuePointer - 1;
    for (slot = state->stackTop.valuePointer; slot < state->stackTail.valuePointer; slot++) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(slot));
    }
    ZrCore_Stack_SetRawObjectValue(state, functionSlot, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    state->baseCallInfo.functionBase.valuePointer = functionSlot;
    state->baseCallInfo.functionTop.valuePointer = state->stackTail.valuePointer;
    state->stackTop.valuePointer = state->stackTail.valuePointer;
    stackBaseBefore = state->stackBase.valuePointer;
    stackTopOffset = ZrCore_Stack_SavePointerAsOffset(state, state->stackTop.valuePointer);

    memset(&activation, 0, sizeof(activation));
    memset(&info, 0, sizeof(info));
    gotStack = ZrCore_Debug_GetStack(state, 0u, &activation);
    resolvedFunction = (TZrBool)(gotStack && activation.function == function);
    gotInfo = (TZrBool)(gotStack &&
                        ZrCore_Debug_GetInfo(state, &activation, ZR_DEBUG_INFO_PUSH_FUNCTION, &info));

    stackMoved = (TZrBool)(state->stackBase.valuePointer != stackBaseBefore);
    relocatedTop = ZrCore_Stack_LoadOffsetToPointer(state, stackTopOffset);
    pushedOneSlot = (TZrBool)(state->stackTop.valuePointer == relocatedTop + 1);
    if (pushedOneSlot) {
        pushedValue = ZrCore_Stack_GetValueNoProfile(relocatedTop);
        pushedCallable = (TZrBool)(pushedValue->type == ZR_VALUE_TYPE_FUNCTION &&
                                   pushedValue->value.object == ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    }
    frameValue = ZrCore_Stack_GetValueNoProfile(state->baseCallInfo.functionBase.valuePointer);
    frameKeepsCallable = (TZrBool)(frameValue->type == ZR_VALUE_TYPE_FUNCTION &&
                                   frameValue->value.object == ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    destroy_state_with_stack_moving_allocator(state, &allocator);

    TEST_ASSERT_TRUE_MESSAGE(resolvedFunction, "level 0 must resolve the frame that holds the VM function");
    TEST_ASSERT_TRUE_MESSAGE(gotInfo, "PUSH_FUNCTION must succeed when the push has to grow the stack");
    TEST_ASSERT_TRUE_MESSAGE(stackMoved, "the push must have relocated the VM stack");
    TEST_ASSERT_TRUE_MESSAGE(pushedOneSlot, "PUSH_FUNCTION must occupy exactly one slot above the old top");
    TEST_ASSERT_TRUE_MESSAGE(pushedCallable, "the pushed value must be the callable from the relocated frame");
    TEST_ASSERT_TRUE_MESSAGE(frameKeepsCallable, "the relocated function slot must still hold the callable");
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_count_hook_fires_for_each_instruction_period_and_can_be_disabled);
    RUN_TEST(test_line_and_count_hooks_are_independent_and_line_events_are_deduplicated);
    RUN_TEST(test_getstack_and_getinfo_resolve_nested_frames_and_respect_type_mask);
    RUN_TEST(test_getinfo_push_function_rereads_callable_after_stack_relocation);
    return UNITY_END();
}
