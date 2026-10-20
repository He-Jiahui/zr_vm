#include <stdlib.h>
#include <string.h>

#include "unity.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/reflection.h"
#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_parser.h"

#include "reflection/field/reflection_construction_native_internal.h"
#include "reflection/reflection_descriptor_native_internal.h"

/* Retain and poison moved blocks until teardown so stale stack pointers fail deterministically. */
typedef struct StackRelocationAuditAllocator {
    TZrPtr retired[64];
    TZrUInt32 retiredCount;
    TZrUInt32 moveCount;
} StackRelocationAuditAllocator;

static TZrPtr stack_relocation_audit_allocator(
        TZrPtr userData,
        TZrPtr pointer,
        TZrSize originalSize,
        TZrSize newSize,
        TZrInt64 flag) {
    StackRelocationAuditAllocator *context = (StackRelocationAuditAllocator *)userData;
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
    memset(newPointer, 0xCD, newSize);
    memcpy(newPointer, pointer, originalSize < newSize ? originalSize : newSize);
    if (context->retiredCount >= sizeof(context->retired) / sizeof(context->retired[0])) {
        free(newPointer);
        return ZR_NULL;
    }
    memset(pointer, 0xDD, originalSize);
    context->retired[context->retiredCount++] = pointer;
    context->moveCount++;
    return newPointer;
}

static SZrState *stack_relocation_audit_create_state(StackRelocationAuditAllocator *allocator) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global = ZrCore_GlobalState_New(
            stack_relocation_audit_allocator, allocator, 12345, &callbacks);
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

static void stack_relocation_audit_free_state(
        SZrState *state,
        StackRelocationAuditAllocator *allocator) {
    TZrUInt32 index;

    if (state != ZR_NULL) {
        ZrCore_GlobalState_Free(state->global);
    }
    for (index = 0u; index < allocator->retiredCount; index++) {
        free(allocator->retired[index]);
    }
    allocator->retiredCount = 0u;
}

typedef struct StackRelocationAuditReflectionFixture {
    SZrFunction *function;
    SZrObjectModule *module;
    SZrObject *descriptor;
    SZrClosureNative *constructionClosure;
    SZrTypeValue descriptorRoot;
    SZrTypeValue closureRoot;
    SZrTypeValue descriptorMethodRoot;
} StackRelocationAuditReflectionFixture;

static TZrBool stack_relocation_audit_create_reflection_fixture(
        SZrState *state,
        StackRelocationAuditReflectionFixture *fixture) {
    static const char source[] =
            "module stack_relocation_audit;\n"
            "pub class RelocationTarget {\n"
            "  pub @constructor() {}\n"
            "}\n";
    SZrString *sourceName;
    SZrFunction *function;
    SZrObjectPrototype *prototype;
    SZrTypeValue prototypeValue;
    SZrTypeValue descriptorValue;
    SZrTypeValue methodNameValue;
    const SZrTypeValue *descriptorMethodValue;
    SZrMetadataRuntime *runtime;
    TZrStackValuePointer descriptorRootBase;
    TZrMemoryOffset descriptorRootOffset;

    memset(fixture, 0, sizeof(*fixture));
    sourceName = ZrCore_String_CreateFromNative(state, "stack_relocation_audit.zr");
    if (sourceName == ZR_NULL) {
        return ZR_FALSE;
    }
    function = ZrParser_Source_Compile(state, source, sizeof(source) - 1u, sourceName);
    if (function == ZR_NULL) {
        return ZR_FALSE;
    }
    fixture->function = function;
    fixture->module = ZrCore_Module_Create(state);
    if (fixture->module == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Module_SetInfo(
            state,
            fixture->module,
            ZrCore_String_CreateFromNative(state, "stack_relocation_audit"),
            ZrCore_Module_CalculatePathHash(state, sourceName),
            sourceName);
    if (ZrCore_Module_CreatePrototypesFromData(state, fixture->module, function) == 0u) {
        return ZR_FALSE;
    }
    {
        const SZrTypeValue *prototypeValue = ZrCore_Module_GetPubExport(
            state,
            fixture->module,
            ZrCore_String_CreateFromNative(state, "RelocationTarget"));
        if (prototypeValue == ZR_NULL || prototypeValue->value.object == ZR_NULL) {
            return ZR_FALSE;
        }
        prototype = (SZrObjectPrototype *)prototypeValue->value.object;
    }
    ZrCore_Value_InitAsRawObject(
            state, &prototypeValue, ZR_CAST_RAW_OBJECT_AS_SUPER(prototype));
    prototypeValue.type = ZR_VALUE_TYPE_OBJECT;
    if (!ZrCore_Reflection_TypeOfValue(state, &prototypeValue, &descriptorValue)) {
        return ZR_FALSE;
    }
    fixture->descriptor = ZR_CAST_OBJECT(state, descriptorValue.value.object);
    ZrCore_Value_Copy(state, &fixture->descriptorRoot, &descriptorValue);
    descriptorRootBase = ZrCore_Function_CheckStackAndGc(
            state, 1u, state->stackTop.valuePointer);
    descriptorRootOffset = ZrCore_Stack_SavePointerAsOffset(state, descriptorRootBase);
    ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(descriptorRootBase), &descriptorValue);
    state->stackTop.valuePointer = descriptorRootBase + 1;
    runtime = ZrCore_Module_GetMetadataRuntime(fixture->module);
    fixture->constructionClosure = ZrCore_Reflection_CreateInstanceNativeClosureInternal(
            state, runtime, ZR_FALSE);
    if (fixture->constructionClosure == ZR_NULL) {
        state->stackTop.valuePointer = ZrCore_Stack_LoadOffsetToPointer(state, descriptorRootOffset);
        return ZR_FALSE;
    }
    descriptorRootBase = ZrCore_Stack_LoadOffsetToPointer(state, descriptorRootOffset);
    ZrCore_Value_Copy(
            state, &fixture->descriptorRoot, ZrCore_Stack_GetValue(descriptorRootBase));
    state->stackTop.valuePointer = descriptorRootBase;
    fixture->descriptor = ZR_CAST_OBJECT(state, fixture->descriptorRoot.value.object);
    ZrCore_Value_InitAsRawObject(
            state,
            &methodNameValue,
            ZR_CAST_RAW_OBJECT_AS_SUPER(
                    ZrCore_String_CreateFromNative(state, "createInstance")));
    methodNameValue.type = ZR_VALUE_TYPE_STRING;
    descriptorMethodValue = ZrCore_Object_GetValue(state, fixture->descriptor, &methodNameValue);
    if (descriptorMethodValue == ZR_NULL || descriptorMethodValue->type != ZR_VALUE_TYPE_CLOSURE) {
        return ZR_FALSE;
    }
    ZrCore_Value_Copy(state, &fixture->descriptorMethodRoot, descriptorMethodValue);
    ZrCore_Value_InitAsRawObject(
            state,
            &fixture->closureRoot,
            ZR_CAST_RAW_OBJECT_AS_SUPER(fixture->constructionClosure));
    fixture->closureRoot.type = ZR_VALUE_TYPE_CLOSURE;
    return ZR_TRUE;
}

static void stack_relocation_audit_free_reflection_fixture(
        SZrState *state,
        StackRelocationAuditReflectionFixture *fixture) {
    if (state != ZR_NULL && fixture != ZR_NULL && fixture->function != ZR_NULL) {
        ZrCore_Function_Free(state, fixture->function);
        fixture->function = ZR_NULL;
    }
}

void setUp(void) {}
void tearDown(void) {}

static void test_debug_push_function_reloads_callable_after_stack_growth(void) {
    StackRelocationAuditAllocator allocator = {0};
    SZrState *state = stack_relocation_audit_create_state(&allocator);
    SZrDebugActivation activation = {0};
    SZrDebugInfo info;
    SZrFunction function = {0};
    TZrStackValuePointer callableSlot;
    SZrTypeValue *pushedValue;
    TZrBool gotInfo;
    TZrBool pushedInt;
    TZrInt64 pushedIntValue;
    TZrUInt32 moveCount;

    if (state == ZR_NULL) {
        TEST_FAIL_MESSAGE("moving allocator state creation failed");
        return;
    }
    callableSlot = state->stackTail.valuePointer - 1;
    if (callableSlot < state->stackBase.valuePointer) {
        stack_relocation_audit_free_state(state, &allocator);
        TEST_FAIL_MESSAGE("initial VM stack has no callable slot");
        return;
    }
    ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(callableSlot), 0x12345678);
    state->baseCallInfo.functionBase.valuePointer = callableSlot;
    state->baseCallInfo.functionTop.valuePointer = state->stackTail.valuePointer;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = state->stackTail.valuePointer;
    activation.callInfo = &state->baseCallInfo;
    activation.function = &function;
    allocator.moveCount = 0u;

    gotInfo = ZrCore_Debug_GetInfo(state, &activation, ZR_DEBUG_INFO_PUSH_FUNCTION, &info);
    moveCount = allocator.moveCount;
    pushedValue = ZrCore_Stack_GetValueNoProfile(state->stackTop.valuePointer - 1);
    pushedInt = (TZrBool)(pushedValue != ZR_NULL && pushedValue->type == ZR_VALUE_TYPE_INT64);
    pushedIntValue = pushedInt ? pushedValue->value.nativeObject.nativeInt64 : 0;
    stack_relocation_audit_free_state(state, &allocator);

    TEST_ASSERT_TRUE(gotInfo);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, moveCount);
    TEST_ASSERT_TRUE(pushedInt);
    TEST_ASSERT_EQUAL_INT64(0x12345678, pushedIntValue);
}

static void test_reflection_construction_entry_reloads_frame_after_constructor_growth(void) {
    StackRelocationAuditAllocator allocator = {0};
    SZrState *state = stack_relocation_audit_create_state(&allocator);
    StackRelocationAuditReflectionFixture fixture;
    TZrStackValuePointer functionBase;
    SZrTypeValue *resultValue;
    TZrBool created;
    TZrBool resultIsObject;
    TZrBool stackTopIsInCurrentStack;

    if (state == ZR_NULL) {
        TEST_FAIL_MESSAGE("moving allocator state creation failed");
        return;
    }
    if (!stack_relocation_audit_create_reflection_fixture(state, &fixture)) {
        stack_relocation_audit_free_reflection_fixture(state, &fixture);
        stack_relocation_audit_free_state(state, &allocator);
        TEST_FAIL_MESSAGE("reflection fixture creation failed");
        return;
    }
    functionBase = state->stackTail.valuePointer - 2;
    ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(functionBase), &fixture.closureRoot);
    ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(functionBase + 1), &fixture.descriptorRoot);
    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = state->stackTail.valuePointer;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = state->stackTail.valuePointer;
    allocator.moveCount = 0u;

    created = ZrCore_Reflection_CreateInstanceNativeEntryInternal(state) == 1;
    stackTopIsInCurrentStack = state->stackTop.valuePointer >= state->stackBase.valuePointer &&
                               state->stackTop.valuePointer <= state->stackTail.valuePointer;
    resultValue = ZrCore_Stack_GetValueNoProfile(state->callInfoList->functionBase.valuePointer);
    resultIsObject = (TZrBool)(resultValue != ZR_NULL && resultValue->type == ZR_VALUE_TYPE_OBJECT);
    stack_relocation_audit_free_reflection_fixture(state, &fixture);
    stack_relocation_audit_free_state(state, &allocator);

    TEST_ASSERT_TRUE(created);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, allocator.moveCount);
    TEST_ASSERT_TRUE(stackTopIsInCurrentStack);
    TEST_ASSERT_TRUE(resultIsObject);
}

static void test_reflection_descriptor_method_reloads_frame_after_constructor_growth(void) {
    StackRelocationAuditAllocator allocator = {0};
    SZrState *state = stack_relocation_audit_create_state(&allocator);
    StackRelocationAuditReflectionFixture fixture;
    TZrStackValuePointer functionBase;
    SZrTypeValue *resultValue;
    TZrBool resultIsObject;
    TZrBool stackTopIsInCurrentStack;

    if (state == ZR_NULL) {
        TEST_FAIL_MESSAGE("moving allocator state creation failed");
        return;
    }
    if (!stack_relocation_audit_create_reflection_fixture(state, &fixture)) {
        stack_relocation_audit_free_reflection_fixture(state, &fixture);
        stack_relocation_audit_free_state(state, &allocator);
        TEST_FAIL_MESSAGE("reflection fixture creation failed");
        return;
    }
    functionBase = state->stackTail.valuePointer - 1;
    ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(functionBase), &fixture.descriptorMethodRoot);
    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = state->stackTail.valuePointer;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = state->stackTail.valuePointer;
    allocator.moveCount = 0u;

    ZrCore_Function_Call(state, functionBase, 1u);
    stackTopIsInCurrentStack = state->stackTop.valuePointer >= state->stackBase.valuePointer &&
                               state->stackTop.valuePointer <= state->stackTail.valuePointer;
    resultValue = ZrCore_Stack_GetValueNoProfile(state->callInfoList->functionBase.valuePointer);
    resultIsObject = (TZrBool)(resultValue != ZR_NULL && resultValue->type == ZR_VALUE_TYPE_OBJECT);
    stack_relocation_audit_free_reflection_fixture(state, &fixture);
    stack_relocation_audit_free_state(state, &allocator);

    TEST_ASSERT_GREATER_THAN_UINT32(0u, allocator.moveCount);
    TEST_ASSERT_TRUE(stackTopIsInCurrentStack);
    TEST_ASSERT_TRUE(resultIsObject);
}

static void test_reflection_module_cache_miss_restores_root_base_after_builder_growth(void) {
    StackRelocationAuditAllocator allocator = {0};
    SZrState *state = stack_relocation_audit_create_state(&allocator);
    StackRelocationAuditReflectionFixture fixture;
    SZrMetadataRuntime *runtime;
    SZrObjectModule *serviceModule;
    TZrMemoryOffset stackTopOffset;
    TZrStackValuePointer expectedStackTop;
    TZrBool serviceCreated;
    TZrBool stackTopRestored;
    TZrUInt32 moveCount;

    if (state == ZR_NULL) {
        TEST_FAIL_MESSAGE("moving allocator state creation failed");
        return;
    }
    if (!stack_relocation_audit_create_reflection_fixture(state, &fixture)) {
        stack_relocation_audit_free_reflection_fixture(state, &fixture);
        stack_relocation_audit_free_state(state, &allocator);
        TEST_FAIL_MESSAGE("reflection fixture creation failed");
        return;
    }
    runtime = ZrCore_Module_GetMetadataRuntime(fixture.module);
    if (runtime == ZR_NULL) {
        stack_relocation_audit_free_reflection_fixture(state, &fixture);
        stack_relocation_audit_free_state(state, &allocator);
        TEST_FAIL_MESSAGE("reflection fixture metadata runtime missing");
        return;
    }
    state->baseCallInfo.functionBase.valuePointer = state->stackBase.valuePointer;
    state->baseCallInfo.functionTop.valuePointer = state->stackTail.valuePointer;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = state->stackTail.valuePointer - 6;
    stackTopOffset = ZrCore_Stack_SavePointerAsOffset(state, state->stackTop.valuePointer);
    allocator.moveCount = 0u;

    serviceModule = ZrCore_Reflection_GetOrCreateModuleForRuntime(state, runtime);
    expectedStackTop = ZrCore_Stack_LoadOffsetToPointer(state, stackTopOffset);
    serviceCreated = (TZrBool)(serviceModule != ZR_NULL);
    stackTopRestored = (TZrBool)(state->stackTop.valuePointer == expectedStackTop);
    moveCount = allocator.moveCount;
    stack_relocation_audit_free_reflection_fixture(state, &fixture);
    stack_relocation_audit_free_state(state, &allocator);

    TEST_ASSERT_TRUE(serviceCreated);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, moveCount);
    TEST_ASSERT_TRUE(stackTopRestored);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_debug_push_function_reloads_callable_after_stack_growth);
    RUN_TEST(test_reflection_construction_entry_reloads_frame_after_constructor_growth);
    RUN_TEST(test_reflection_descriptor_method_reloads_frame_after_constructor_growth);
    RUN_TEST(test_reflection_module_cache_miss_restores_root_base_after_builder_growth);
    return UNITY_END();
}
