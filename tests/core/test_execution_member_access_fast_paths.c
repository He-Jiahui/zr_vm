#include "unity.h"

#include <string.h>

#include "tests/harness/runtime_support.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/profile.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_core/src/zr_vm_core/execution/execution_internal.h"
#include "zr_vm_core/src/zr_vm_core/object/object_internal.h"

void setUp(void) {}

void tearDown(void) {}

typedef struct SZrMemberAccessFixture {
    SZrFunction function;
    SZrFunctionCallSiteCacheEntry cacheEntry;
    SZrFunctionMemberEntry memberEntry;
    SZrTypeValue receiverValue;
    SZrString *memberName;
    SZrObject *instance;
    SZrObjectPrototype *prototype;
    SZrFunction *callableFunction;
} SZrMemberAccessFixture;

static SZrFunction *create_runtime_fixture_function(SZrState *state, SZrMemberAccessFixture *fixture) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(fixture);

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    function->memberEntries = fixture->function.memberEntries;
    function->memberEntryLength = fixture->function.memberEntryLength;
    function->callSiteCaches = fixture->function.callSiteCaches;
    function->callSiteCacheLength = fixture->function.callSiteCacheLength;
    function->prototypeInstances = fixture->function.prototypeInstances;
    function->prototypeInstancesLength = fixture->function.prototypeInstancesLength;
    return function;
}

static void detach_runtime_fixture_function(SZrFunction *function) {
    if (function == ZR_NULL) {
        return;
    }

    function->memberEntries = ZR_NULL;
    function->memberEntryLength = 0;
    function->callSiteCaches = ZR_NULL;
    function->callSiteCacheLength = 0;
    function->prototypeInstances = ZR_NULL;
    function->prototypeInstancesLength = 0;
}

static void init_string_key(SZrState *state, SZrString *stringValue, SZrTypeValue *outKey) {
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(stringValue);
    TEST_ASSERT_NOT_NULL(outKey);

    ZrCore_Value_InitAsRawObject(state, outKey, ZR_CAST_RAW_OBJECT_AS_SUPER(stringValue));
    outKey->type = ZR_VALUE_TYPE_STRING;
}

static void init_member_access_fixture(SZrState *state, SZrMemberAccessFixture *fixture, TZrInt64 storedValue) {
    SZrString *typeName;
    SZrMemberDescriptor descriptor;
    SZrObject *instance;
    SZrTypeValue key;
    SZrTypeValue value;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(fixture);

    memset(fixture, 0, sizeof(*fixture));

    typeName = ZrCore_String_CreateFromNative(state, "HotPathMemberBox");
    fixture->memberName = ZrCore_String_CreateFromNative(state, "value");
    TEST_ASSERT_NOT_NULL(typeName);
    TEST_ASSERT_NOT_NULL(fixture->memberName);

    fixture->prototype = ZrCore_ObjectPrototype_New(state, typeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    TEST_ASSERT_NOT_NULL(fixture->prototype);

    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.name = fixture->memberName;
    descriptor.kind = ZR_MEMBER_DESCRIPTOR_KIND_FIELD;
    descriptor.isWritable = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrCore_ObjectPrototype_AddMemberDescriptor(state, fixture->prototype, &descriptor));

    instance = ZrCore_Object_New(state, fixture->prototype);
    TEST_ASSERT_NOT_NULL(instance);
    ZrCore_Object_Init(state, instance);
    fixture->instance = instance;

    init_string_key(state, fixture->memberName, &key);
    ZrCore_Value_InitAsInt(state, &value, storedValue);
    ZrCore_Object_SetValue(state, instance, &key, &value);

    ZrCore_Value_InitAsRawObject(state, &fixture->receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(instance));
    fixture->receiverValue.type = ZR_VALUE_TYPE_OBJECT;

    fixture->memberEntry.symbol = fixture->memberName;
    fixture->memberEntry.entryKind = ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL;
    fixture->function.memberEntries = &fixture->memberEntry;
    fixture->function.memberEntryLength = 1;
    fixture->function.callSiteCaches = &fixture->cacheEntry;
    fixture->function.callSiteCacheLength = 1;

    fixture->cacheEntry.kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET;
    fixture->cacheEntry.instructionIndex = 0;
    fixture->cacheEntry.memberEntryIndex = 0;
    fixture->cacheEntry.picSlotCount = 1;
    fixture->cacheEntry.picSlots[0].cachedReceiverPrototype = fixture->prototype;
    fixture->cacheEntry.picSlots[0].cachedOwnerPrototype = fixture->prototype;
    fixture->cacheEntry.picSlots[0].cachedReceiverVersion = fixture->prototype->super.memberVersion;
    fixture->cacheEntry.picSlots[0].cachedOwnerVersion = fixture->prototype->super.memberVersion;
    fixture->cacheEntry.picSlots[0].cachedDescriptorIndex = 0;
}

static void init_callable_member_access_fixture(SZrState *state, SZrMemberAccessFixture *fixture) {
    SZrString *typeName;
    SZrMemberDescriptor descriptor;
    SZrObject *instance;
    SZrTypeValue value;
    SZrTypeValue key;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(fixture);

    memset(fixture, 0, sizeof(*fixture));

    typeName = ZrCore_String_CreateFromNative(state, "HotPathCallableBox");
    fixture->memberName = ZrCore_String_CreateFromNative(state, "invoke");
    TEST_ASSERT_NOT_NULL(typeName);
    TEST_ASSERT_NOT_NULL(fixture->memberName);

    fixture->prototype = ZrCore_ObjectPrototype_New(state, typeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    TEST_ASSERT_NOT_NULL(fixture->prototype);

    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.name = fixture->memberName;
    descriptor.kind = ZR_MEMBER_DESCRIPTOR_KIND_METHOD;
    descriptor.isWritable = ZR_FALSE;
    TEST_ASSERT_TRUE(ZrCore_ObjectPrototype_AddMemberDescriptor(state, fixture->prototype, &descriptor));

    fixture->callableFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(fixture->callableFunction);

    init_string_key(state, fixture->memberName, &key);
    ZrCore_Value_InitAsRawObject(state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(fixture->callableFunction));
    ZrCore_Object_SetValue(state, &fixture->prototype->super, &key, &value);

    instance = ZrCore_Object_New(state, fixture->prototype);
    TEST_ASSERT_NOT_NULL(instance);
    ZrCore_Object_Init(state, instance);
    fixture->instance = instance;

    ZrCore_Value_InitAsRawObject(state, &fixture->receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(instance));
    fixture->receiverValue.type = ZR_VALUE_TYPE_OBJECT;

    fixture->memberEntry.symbol = fixture->memberName;
    fixture->memberEntry.entryKind = ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL;
    fixture->function.memberEntries = &fixture->memberEntry;
    fixture->function.memberEntryLength = 1;
    fixture->function.callSiteCaches = &fixture->cacheEntry;
    fixture->function.callSiteCacheLength = 1;

    fixture->cacheEntry.kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET;
    fixture->cacheEntry.instructionIndex = 0;
    fixture->cacheEntry.memberEntryIndex = 0;
}

static void init_descriptorless_callable_member_access_fixture(SZrState *state, SZrMemberAccessFixture *fixture) {
    SZrString *typeName;
    SZrObject *instance;
    SZrTypeValue value;
    SZrTypeValue key;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(fixture);

    memset(fixture, 0, sizeof(*fixture));

    typeName = ZrCore_String_CreateFromNative(state, "HotPathDescriptorlessCallableBox");
    fixture->memberName = ZrCore_String_CreateFromNative(state, "step");
    TEST_ASSERT_NOT_NULL(typeName);
    TEST_ASSERT_NOT_NULL(fixture->memberName);

    fixture->prototype = ZrCore_ObjectPrototype_New(state, typeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    TEST_ASSERT_NOT_NULL(fixture->prototype);

    fixture->callableFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(fixture->callableFunction);

    init_string_key(state, fixture->memberName, &key);
    ZrCore_Value_InitAsRawObject(state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(fixture->callableFunction));
    ZrCore_Object_SetValue(state, &fixture->prototype->super, &key, &value);

    instance = ZrCore_Object_New(state, fixture->prototype);
    TEST_ASSERT_NOT_NULL(instance);
    ZrCore_Object_Init(state, instance);
    fixture->instance = instance;

    ZrCore_Value_InitAsRawObject(state, &fixture->receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(instance));
    fixture->receiverValue.type = ZR_VALUE_TYPE_OBJECT;

    fixture->memberEntry.symbol = fixture->memberName;
    fixture->memberEntry.entryKind = ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL;
    fixture->function.memberEntries = &fixture->memberEntry;
    fixture->function.memberEntryLength = 1;
    fixture->function.callSiteCaches = &fixture->cacheEntry;
    fixture->function.callSiteCacheLength = 1;

    fixture->cacheEntry.kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET;
    fixture->cacheEntry.instructionIndex = 0;
    fixture->cacheEntry.memberEntryIndex = 0;
}

static void reset_profile_counters(SZrState *state, SZrProfileRuntime *profileRuntime) {
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);
    TEST_ASSERT_NOT_NULL(profileRuntime);

    memset(profileRuntime, 0, sizeof(*profileRuntime));
    memset(profileRuntime->helperCounts, 0, sizeof(profileRuntime->helperCounts));
    profileRuntime->recordHelpers = ZR_TRUE;
    profileRuntime->recordSlowPaths = ZR_TRUE;
    state->global->profileRuntime = profileRuntime;
    ZrCore_Profile_SetCurrentState(state);
}

static void reset_profile_counters_from_state_only(SZrState *state, SZrProfileRuntime *profileRuntime) {
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);
    TEST_ASSERT_NOT_NULL(profileRuntime);

    memset(profileRuntime, 0, sizeof(*profileRuntime));
    profileRuntime->recordHelpers = ZR_TRUE;
    profileRuntime->recordSlowPaths = ZR_TRUE;
    state->global->profileRuntime = profileRuntime;
    ZrCore_Profile_SetCurrentState(ZR_NULL);
}

static void clear_profile_counters(SZrState *state) {
    if (state != ZR_NULL && state->global != ZR_NULL) {
        state->global->profileRuntime = ZR_NULL;
    }
    ZrCore_Profile_SetCurrentState(ZR_NULL);
}

static SZrTypeValue *reserve_stack_result_slot(SZrState *state) {
    TZrStackValuePointer functionBase;
    SZrTypeValue *result;

    TEST_ASSERT_NOT_NULL(state);

    functionBase = state->stackTail.valuePointer - 2;
    TEST_ASSERT_TRUE(functionBase >= state->stackBase.valuePointer);

    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(functionBase));
    result = ZrCore_Stack_GetValue(functionBase + 1);
    TEST_ASSERT_NOT_NULL(result);
    ZrCore_Value_ResetAsNull(result);

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + 2;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + 2;

    return result;
}

static void test_member_get_by_name_fast_path_skips_extra_stable_result_copy(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrProfileRuntime profileRuntime;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 41);
    ZrCore_Value_ResetAsNull(&result);
    reset_profile_counters(state, &profileRuntime);

    TEST_ASSERT_TRUE(execution_member_get_by_name(state, ZR_NULL, &fixture.receiverValue, fixture.memberName, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(41, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY]);
    TEST_ASSERT_EQUAL_UINT64(0u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_RESET_NULL]);

    clear_profile_counters(state);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_get_cached_fast_path_skips_extra_stable_result_copy(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrProfileRuntime profileRuntime;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    ZrCore_Value_ResetAsNull(&result);
    reset_profile_counters(state, &profileRuntime);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, &fixture.function, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(73, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY]);
    TEST_ASSERT_EQUAL_UINT64(0u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_RESET_NULL]);

    clear_profile_counters(state);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_get_cached_stack_slot_fast_path_skips_anchor_copy(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrProfileRuntime profileRuntime;
    SZrTypeValue *result;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 97);
    result = reserve_stack_result_slot(state);
    reset_profile_counters(state, &profileRuntime);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, &fixture.function, 0, &fixture.receiverValue, result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result->type);
    TEST_ASSERT_EQUAL_INT64(97, result->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY]);
    TEST_ASSERT_EQUAL_UINT64(0u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_RESET_NULL]);

    clear_profile_counters(state);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_get_cached_hit_does_not_record_callsite_lookup_slowpath(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrProfileRuntime profileRuntime;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    ZrCore_Value_ResetAsNull(&result);
    reset_profile_counters(state, &profileRuntime);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, &fixture.function, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_EQUAL_UINT64(0u, profileRuntime.slowPathCounts[ZR_PROFILE_SLOWPATH_CALLSITE_CACHE_LOOKUP]);

    clear_profile_counters(state);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_set_cached_hit_does_not_record_callsite_lookup_slowpath(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrProfileRuntime profileRuntime;
    SZrTypeValue assignedValue;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    fixture.cacheEntry.kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET;
    ZrCore_Value_InitAsInt(state, &assignedValue, 105);
    ZrCore_Value_ResetAsNull(&result);
    reset_profile_counters(state, &profileRuntime);

    TEST_ASSERT_TRUE(execution_member_set_cached(
            state, ZR_NULL, &fixture.function, 0, &fixture.receiverValue, &assignedValue));
    TEST_ASSERT_TRUE(execution_member_get_by_name(state, ZR_NULL, &fixture.receiverValue, fixture.memberName, &result));
    TEST_ASSERT_EQUAL_INT64(105, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT64(0u, profileRuntime.slowPathCounts[ZR_PROFILE_SLOWPATH_CALLSITE_CACHE_LOOKUP]);

    clear_profile_counters(state);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_get_cached_refresh_stores_callable_target_for_method_descriptor(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrFunction *runtimeFunction;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    init_callable_member_access_fixture(state, &fixture);
    runtimeFunction = create_runtime_fixture_function(state, &fixture);
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, runtimeFunction, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_EQUAL_PTR(fixture.callableFunction, result.value.object);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.picSlotCount);
    TEST_ASSERT_EQUAL_PTR(fixture.callableFunction, fixture.cacheEntry.picSlots[0].cachedFunction);

    detach_runtime_fixture_function(runtimeFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_get_cached_refresh_stores_receiver_object_for_callable_method_descriptor(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrFunction *runtimeFunction;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    init_callable_member_access_fixture(state, &fixture);
    runtimeFunction = create_runtime_fixture_function(state, &fixture);
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, runtimeFunction, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_EQUAL_PTR(fixture.instance, fixture.cacheEntry.picSlots[0].cachedReceiverObject);
    TEST_ASSERT_EQUAL_PTR(fixture.instance->nodeMap.buckets, fixture.cacheEntry.picSlots[0].cachedReceiverBuckets);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)fixture.instance->nodeMap.elementCount,
                             fixture.cacheEntry.picSlots[0].cachedReceiverElementCount);

    detach_runtime_fixture_function(runtimeFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_get_cached_refresh_stores_callable_target_for_descriptorless_prototype_value(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrFunction *runtimeFunction;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    init_descriptorless_callable_member_access_fixture(state, &fixture);
    runtimeFunction = create_runtime_fixture_function(state, &fixture);
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, runtimeFunction, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_EQUAL_PTR(fixture.callableFunction, result.value.object);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.picSlotCount);
    TEST_ASSERT_EQUAL_PTR(fixture.callableFunction, fixture.cacheEntry.picSlots[0].cachedFunction);
    TEST_ASSERT_EQUAL_UINT32(ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE,
                             fixture.cacheEntry.picSlots[0].cachedDescriptorIndex);
    TEST_ASSERT_EQUAL_PTR(fixture.instance, fixture.cacheEntry.picSlots[0].cachedReceiverObject);

    detach_runtime_fixture_function(runtimeFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_get_cached_descriptorless_callable_receiver_slot_hits_on_second_access(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrFunction *runtimeFunction;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    init_descriptorless_callable_member_access_fixture(state, &fixture);
    runtimeFunction = create_runtime_fixture_function(state, &fixture);
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, runtimeFunction, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, runtimeFunction, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_EQUAL_PTR(fixture.callableFunction, result.value.object);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.picSlotCount);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeMissCount);

    detach_runtime_fixture_function(runtimeFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_get_cached_descriptorless_callable_refresh_survives_receiver_result_alias(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrFunction *runtimeFunction;
    SZrTypeValue *sharedSlot;

    TEST_ASSERT_NOT_NULL(state);
    init_descriptorless_callable_member_access_fixture(state, &fixture);
    runtimeFunction = create_runtime_fixture_function(state, &fixture);
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;

    sharedSlot = reserve_stack_result_slot(state);
    ZrCore_Value_Copy(state, sharedSlot, &fixture.receiverValue);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, runtimeFunction, 0, sharedSlot, sharedSlot));
    TEST_ASSERT_EQUAL_PTR(fixture.callableFunction, sharedSlot->value.object);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.picSlotCount);
    TEST_ASSERT_EQUAL_PTR(fixture.callableFunction, fixture.cacheEntry.picSlots[0].cachedFunction);
    TEST_ASSERT_EQUAL_PTR(fixture.instance, fixture.cacheEntry.picSlots[0].cachedReceiverObject);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeMissCount);

    ZrCore_Value_Copy(state, sharedSlot, &fixture.receiverValue);
    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, runtimeFunction, 0, sharedSlot, sharedSlot));
    TEST_ASSERT_EQUAL_PTR(fixture.callableFunction, sharedSlot->value.object);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeMissCount);

    detach_runtime_fixture_function(runtimeFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_get_cached_refresh_marks_instance_field_slot_kind(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrFunction *runtimeFunction;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    runtimeFunction = create_runtime_fixture_function(state, &fixture);
    fixture.cacheEntry.picSlotCount = 0;
    fixture.cacheEntry.picNextInsertIndex = 0;
    memset(&fixture.cacheEntry.picSlots[0], 0, sizeof(fixture.cacheEntry.picSlots[0]));
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, runtimeFunction, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.picSlotCount);
    TEST_ASSERT_EQUAL_UINT8(ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD,
                            fixture.cacheEntry.picSlots[0].cachedAccessKind);

    detach_runtime_fixture_function(runtimeFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_get_cached_refresh_stores_receiver_pair_for_instance_field_descriptor(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrFunction *runtimeFunction;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    runtimeFunction = create_runtime_fixture_function(state, &fixture);
    fixture.cacheEntry.picSlotCount = 0;
    fixture.cacheEntry.picNextInsertIndex = 0;
    memset(&fixture.cacheEntry.picSlots[0], 0, sizeof(fixture.cacheEntry.picSlots[0]));
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, runtimeFunction, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_EQUAL_PTR(fixture.instance->nodeMap.buckets, fixture.cacheEntry.picSlots[0].cachedReceiverBuckets);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)fixture.instance->nodeMap.elementCount,
                             fixture.cacheEntry.picSlots[0].cachedReceiverElementCount);
    TEST_ASSERT_NOT_NULL(fixture.cacheEntry.picSlots[0].cachedReceiverPair);

    detach_runtime_fixture_function(runtimeFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_get_cached_refresh_stores_receiver_object_for_instance_field_pair_descriptor(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrFunction *runtimeFunction;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    runtimeFunction = create_runtime_fixture_function(state, &fixture);
    fixture.cacheEntry.picSlotCount = 0;
    fixture.cacheEntry.picNextInsertIndex = 0;
    memset(&fixture.cacheEntry.picSlots[0], 0, sizeof(fixture.cacheEntry.picSlots[0]));
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, runtimeFunction, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_EQUAL_PTR(fixture.instance, fixture.cacheEntry.picSlots[0].cachedReceiverObject);

    detach_runtime_fixture_function(runtimeFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_get_cached_refresh_uses_bound_member_entry_without_descriptor_name(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrFunction *runtimeFunction;
    SZrTypeValue result;
    SZrObjectPrototype *prototypeInstances[1];

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    fixture.cacheEntry.picSlotCount = 0;
    fixture.cacheEntry.picNextInsertIndex = 0;
    memset(&fixture.cacheEntry.picSlots[0], 0, sizeof(fixture.cacheEntry.picSlots[0]));
    fixture.memberEntry.entryKind = ZR_FUNCTION_MEMBER_ENTRY_KIND_BOUND_DESCRIPTOR;
    fixture.memberEntry.prototypeIndex = 0u;
    fixture.memberEntry.descriptorIndex = 0u;
    fixture.prototype->memberDescriptors[0].name = ZR_NULL;
    prototypeInstances[0] = fixture.prototype;
    fixture.function.prototypeInstances = prototypeInstances;
    fixture.function.prototypeInstancesLength = 1u;
    runtimeFunction = create_runtime_fixture_function(state, &fixture);
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, runtimeFunction, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(73, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.picSlotCount);
    TEST_ASSERT_EQUAL_PTR(fixture.prototype, fixture.cacheEntry.picSlots[0].cachedOwnerPrototype);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.picSlots[0].cachedDescriptorIndex);

    detach_runtime_fixture_function(runtimeFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_get_cached_instance_field_hit_does_not_fallback_when_descriptor_index_is_missing(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    fixture.cacheEntry.picSlots[0].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD;
    fixture.cacheEntry.picSlots[0].cachedDescriptorIndex = ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, &fixture.function, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(73, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_get_cached_instance_field_pair_hit_does_not_require_descriptor_or_member_name(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    fixture.cacheEntry.picSlots[0].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD;
    fixture.cacheEntry.picSlots[0].cachedReceiverBuckets = fixture.instance->nodeMap.buckets;
    fixture.cacheEntry.picSlots[0].cachedReceiverElementCount = (TZrUInt32)fixture.instance->nodeMap.elementCount;
    fixture.cacheEntry.picSlots[0].cachedReceiverPair = fixture.instance->cachedStringLookupPair;
    fixture.cacheEntry.picSlots[0].cachedDescriptorIndex = ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE;
    fixture.cacheEntry.picSlots[0].cachedMemberName = ZR_NULL;
    fixture.function.memberEntries = ZR_NULL;
    fixture.function.memberEntryLength = 0;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, &fixture.function, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(73, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_get_cached_instance_field_pair_receiver_object_hit_does_not_require_receiver_shape_cache(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    fixture.cacheEntry.picSlots[0].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD;
    fixture.cacheEntry.picSlots[0].cachedReceiverObject = fixture.instance;
    fixture.cacheEntry.picSlots[0].cachedReceiverBuckets = ZR_NULL;
    fixture.cacheEntry.picSlots[0].cachedReceiverElementCount = 0u;
    fixture.cacheEntry.picSlots[0].cachedReceiverPair = fixture.instance->cachedStringLookupPair;
    fixture.cacheEntry.picSlots[0].cachedDescriptorIndex = ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE;
    fixture.cacheEntry.picSlots[0].cachedMemberName = ZR_NULL;
    fixture.function.memberEntries = ZR_NULL;
    fixture.function.memberEntryLength = 0;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, &fixture.function, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(73, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_get_cached_exact_receiver_object_hit_uses_member_name_when_descriptor_name_is_missing(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrFunction *runtimeFunction;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    runtimeFunction = create_runtime_fixture_function(state, &fixture);
    fixture.cacheEntry.picSlots[0].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD;
    fixture.cacheEntry.picSlots[0].cachedReceiverObject = fixture.instance;
    fixture.cacheEntry.picSlots[0].cachedReceiverPair = ZR_NULL;
    fixture.cacheEntry.picSlots[0].cachedMemberName = ZR_NULL;
    fixture.prototype->memberDescriptors[0].name = ZR_NULL;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, runtimeFunction, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(73, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);

    detach_runtime_fixture_function(runtimeFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_get_cached_exact_receiver_object_backfills_cached_member_name_for_symbolless_followup(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrFunction *runtimeFunction;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    runtimeFunction = create_runtime_fixture_function(state, &fixture);
    fixture.cacheEntry.picSlots[0].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD;
    fixture.cacheEntry.picSlots[0].cachedReceiverObject = fixture.instance;
    fixture.cacheEntry.picSlots[0].cachedReceiverPair = ZR_NULL;
    fixture.cacheEntry.picSlots[0].cachedMemberName = ZR_NULL;
    fixture.prototype->memberDescriptors[0].name = ZR_NULL;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, runtimeFunction, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_EQUAL_PTR(fixture.memberName, fixture.cacheEntry.picSlots[0].cachedMemberName);

    runtimeFunction->memberEntries = ZR_NULL;
    runtimeFunction->memberEntryLength = 0;
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, runtimeFunction, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(73, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(2u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);

    detach_runtime_fixture_function(runtimeFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_set_cached_instance_field_hit_does_not_fallback_when_descriptor_index_is_missing(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrTypeValue assignedValue;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    fixture.cacheEntry.kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET;
    fixture.cacheEntry.picSlots[0].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD;
    fixture.cacheEntry.picSlots[0].cachedDescriptorIndex = ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;
    ZrCore_Value_InitAsInt(state, &assignedValue, 105);
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_member_set_cached(
            state, ZR_NULL, &fixture.function, 0, &fixture.receiverValue, &assignedValue));
    TEST_ASSERT_TRUE(execution_member_get_by_name(state, ZR_NULL, &fixture.receiverValue, fixture.memberName, &result));
    TEST_ASSERT_EQUAL_INT64(105, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_set_cached_exact_receiver_object_hit_uses_member_name_when_descriptor_name_is_missing(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrFunction *runtimeFunction;
    SZrTypeValue assignedValue;
    SZrTypeValue result;
    TZrUInt32 initialMemberVersion;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    runtimeFunction = create_runtime_fixture_function(state, &fixture);
    fixture.cacheEntry.kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET;
    fixture.cacheEntry.picSlots[0].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD;
    fixture.cacheEntry.picSlots[0].cachedReceiverObject = fixture.instance;
    fixture.cacheEntry.picSlots[0].cachedReceiverPair = ZR_NULL;
    fixture.cacheEntry.picSlots[0].cachedMemberName = ZR_NULL;
    fixture.prototype->memberDescriptors[0].name = ZR_NULL;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;
    initialMemberVersion = fixture.instance->memberVersion;
    ZrCore_Value_InitAsInt(state, &assignedValue, 105);
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_member_set_cached(
            state, ZR_NULL, runtimeFunction, 0, &fixture.receiverValue, &assignedValue));
    TEST_ASSERT_TRUE(execution_member_get_by_name(state, ZR_NULL, &fixture.receiverValue, fixture.memberName, &result));
    TEST_ASSERT_EQUAL_INT64(105, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(initialMemberVersion, fixture.instance->memberVersion);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);

    detach_runtime_fixture_function(runtimeFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_set_cached_exact_receiver_object_backfills_cached_member_name_for_symbolless_followup(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrFunction *runtimeFunction;
    SZrTypeValue assignedValue;
    SZrTypeValue result;
    TZrUInt32 initialMemberVersion;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    runtimeFunction = create_runtime_fixture_function(state, &fixture);
    fixture.cacheEntry.kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET;
    fixture.cacheEntry.picSlots[0].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD;
    fixture.cacheEntry.picSlots[0].cachedReceiverObject = fixture.instance;
    fixture.cacheEntry.picSlots[0].cachedReceiverPair = ZR_NULL;
    fixture.cacheEntry.picSlots[0].cachedMemberName = ZR_NULL;
    fixture.prototype->memberDescriptors[0].name = ZR_NULL;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;
    initialMemberVersion = fixture.instance->memberVersion;
    ZrCore_Value_InitAsInt(state, &assignedValue, 105);
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_member_set_cached(
            state, ZR_NULL, runtimeFunction, 0, &fixture.receiverValue, &assignedValue));
    TEST_ASSERT_EQUAL_PTR(fixture.memberName, fixture.cacheEntry.picSlots[0].cachedMemberName);

    runtimeFunction->memberEntries = ZR_NULL;
    runtimeFunction->memberEntryLength = 0;
    ZrCore_Value_InitAsInt(state, &assignedValue, 106);

    TEST_ASSERT_TRUE(execution_member_set_cached(
            state, ZR_NULL, runtimeFunction, 0, &fixture.receiverValue, &assignedValue));
    TEST_ASSERT_TRUE(execution_member_get_by_name(state, ZR_NULL, &fixture.receiverValue, fixture.memberName, &result));
    TEST_ASSERT_EQUAL_INT64(106, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(initialMemberVersion, fixture.instance->memberVersion);
    TEST_ASSERT_EQUAL_UINT32(2u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);

    detach_runtime_fixture_function(runtimeFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_set_cached_instance_field_pair_receiver_object_hit_does_not_require_receiver_shape_cache(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrTypeValue assignedValue;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    fixture.cacheEntry.kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET;
    fixture.cacheEntry.picSlots[0].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD;
    fixture.cacheEntry.picSlots[0].cachedReceiverObject = fixture.instance;
    fixture.cacheEntry.picSlots[0].cachedReceiverBuckets = ZR_NULL;
    fixture.cacheEntry.picSlots[0].cachedReceiverElementCount = 0u;
    fixture.cacheEntry.picSlots[0].cachedReceiverPair = fixture.instance->cachedStringLookupPair;
    fixture.cacheEntry.picSlots[0].cachedDescriptorIndex = ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE;
    fixture.cacheEntry.picSlots[0].cachedMemberName = ZR_NULL;
    fixture.function.memberEntries = ZR_NULL;
    fixture.function.memberEntryLength = 0;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;
    ZrCore_Value_InitAsInt(state, &assignedValue, 105);
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_member_set_cached(
            state, ZR_NULL, &fixture.function, 0, &fixture.receiverValue, &assignedValue));
    TEST_ASSERT_TRUE(execution_member_get_by_name(state, ZR_NULL, &fixture.receiverValue, fixture.memberName, &result));
    TEST_ASSERT_EQUAL_INT64(105, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_set_cached_instance_field_pair_hit_does_not_require_descriptor_or_member_name(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrTypeValue assignedValue;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    fixture.cacheEntry.kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET;
    fixture.cacheEntry.picSlots[0].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD;
    fixture.cacheEntry.picSlots[0].cachedReceiverBuckets = fixture.instance->nodeMap.buckets;
    fixture.cacheEntry.picSlots[0].cachedReceiverElementCount = (TZrUInt32)fixture.instance->nodeMap.elementCount;
    fixture.cacheEntry.picSlots[0].cachedReceiverPair = fixture.instance->cachedStringLookupPair;
    fixture.cacheEntry.picSlots[0].cachedDescriptorIndex = ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE;
    fixture.cacheEntry.picSlots[0].cachedMemberName = ZR_NULL;
    fixture.function.memberEntries = ZR_NULL;
    fixture.function.memberEntryLength = 0;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;
    ZrCore_Value_InitAsInt(state, &assignedValue, 105);
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_member_set_cached(
            state, ZR_NULL, &fixture.function, 0, &fixture.receiverValue, &assignedValue));
    TEST_ASSERT_TRUE(execution_member_get_by_name(state, ZR_NULL, &fixture.receiverValue, fixture.memberName, &result));
    TEST_ASSERT_EQUAL_INT64(105, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_set_cached_instance_field_pair_hit_does_not_bump_receiver_member_version(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrTypeValue assignedValue;
    TZrUInt32 initialMemberVersion;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    fixture.cacheEntry.kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET;
    fixture.cacheEntry.picSlots[0].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD;
    fixture.cacheEntry.picSlots[0].cachedReceiverBuckets = fixture.instance->nodeMap.buckets;
    fixture.cacheEntry.picSlots[0].cachedReceiverElementCount = (TZrUInt32)fixture.instance->nodeMap.elementCount;
    fixture.cacheEntry.picSlots[0].cachedReceiverPair = fixture.instance->cachedStringLookupPair;
    fixture.cacheEntry.picSlots[0].cachedDescriptorIndex = ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE;
    fixture.cacheEntry.picSlots[0].cachedMemberName = ZR_NULL;
    fixture.function.memberEntries = ZR_NULL;
    fixture.function.memberEntryLength = 0;
    initialMemberVersion = fixture.instance->memberVersion;
    ZrCore_Value_InitAsInt(state, &assignedValue, 105);

    TEST_ASSERT_TRUE(execution_member_set_cached(
            state, ZR_NULL, &fixture.function, 0, &fixture.receiverValue, &assignedValue));
    TEST_ASSERT_EQUAL_UINT32(initialMemberVersion, fixture.instance->memberVersion);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_get_cached_exact_receiver_pair_hit_ignores_cached_version_mismatch(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    fixture.cacheEntry.picSlots[0].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD;
    fixture.cacheEntry.picSlots[0].cachedReceiverObject = fixture.instance;
    fixture.cacheEntry.picSlots[0].cachedReceiverPair = fixture.instance->cachedStringLookupPair;
    fixture.cacheEntry.picSlots[0].cachedReceiverVersion = fixture.prototype->super.memberVersion + 1u;
    fixture.cacheEntry.picSlots[0].cachedOwnerVersion = fixture.prototype->super.memberVersion + 1u;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, &fixture.function, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(73, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_set_cached_exact_receiver_pair_hit_ignores_cached_version_mismatch(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrTypeValue assignedValue;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    fixture.cacheEntry.kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET;
    fixture.cacheEntry.picSlots[0].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD;
    fixture.cacheEntry.picSlots[0].cachedReceiverObject = fixture.instance;
    fixture.cacheEntry.picSlots[0].cachedReceiverPair = fixture.instance->cachedStringLookupPair;
    fixture.cacheEntry.picSlots[0].cachedReceiverVersion = fixture.prototype->super.memberVersion + 1u;
    fixture.cacheEntry.picSlots[0].cachedOwnerVersion = fixture.prototype->super.memberVersion + 1u;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;
    ZrCore_Value_InitAsInt(state, &assignedValue, 105);
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_member_set_cached(
            state, ZR_NULL, &fixture.function, 0, &fixture.receiverValue, &assignedValue));
    TEST_ASSERT_TRUE(execution_member_get_by_name(state, ZR_NULL, &fixture.receiverValue, fixture.memberName, &result));
    TEST_ASSERT_EQUAL_INT64(105, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_get_cached_exact_receiver_pair_hit_records_helpers_from_state_without_tls_current(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrProfileRuntime profileRuntime;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    fixture.cacheEntry.picSlots[0].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD;
    fixture.cacheEntry.picSlots[0].cachedReceiverObject = fixture.instance;
    fixture.cacheEntry.picSlots[0].cachedReceiverPair = fixture.instance->cachedStringLookupPair;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;
    ZrCore_Value_ResetAsNull(&result);
    reset_profile_counters_from_state_only(state, &profileRuntime);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, &fixture.function, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(73, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_GET_MEMBER]);
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY]);

    clear_profile_counters(state);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_set_cached_exact_receiver_pair_hit_records_helpers_from_state_without_tls_current(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrProfileRuntime profileRuntime;
    SZrTypeValue assignedValue;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    fixture.cacheEntry.kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET;
    fixture.cacheEntry.picSlots[0].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD;
    fixture.cacheEntry.picSlots[0].cachedReceiverObject = fixture.instance;
    fixture.cacheEntry.picSlots[0].cachedReceiverPair = fixture.instance->cachedStringLookupPair;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;
    ZrCore_Value_InitAsInt(state, &assignedValue, 105);
    ZrCore_Value_ResetAsNull(&result);
    reset_profile_counters_from_state_only(state, &profileRuntime);

    TEST_ASSERT_TRUE(execution_member_set_cached(
            state, ZR_NULL, &fixture.function, 0, &fixture.receiverValue, &assignedValue));
    TEST_ASSERT_TRUE(execution_member_get_by_name(state, ZR_NULL, &fixture.receiverValue, fixture.memberName, &result));
    TEST_ASSERT_EQUAL_INT64(105, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_SET_MEMBER]);
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY]);

    clear_profile_counters(state);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_object_try_set_existing_pair_plain_value_fast_updates_value_and_lookup_cache(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrHashKeyValuePair *pair;
    SZrTypeValue assignedValue;
    TZrUInt32 initialMemberVersion;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    pair = fixture.instance->cachedStringLookupPair;
    TEST_ASSERT_NOT_NULL(pair);
    fixture.instance->cachedStringLookupPair = ZR_NULL;
    initialMemberVersion = fixture.instance->memberVersion;
    ZrCore_Value_InitAsInt(state, &assignedValue, 105);

    TEST_ASSERT_TRUE(
            object_try_set_existing_pair_plain_value_fast_unchecked(state, fixture.instance, pair, &assignedValue));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, pair->value.type);
    TEST_ASSERT_EQUAL_INT64(105, pair->value.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_PTR(pair, fixture.instance->cachedStringLookupPair);
    TEST_ASSERT_EQUAL_UINT32(initialMemberVersion, fixture.instance->memberVersion);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_object_try_set_existing_pair_plain_value_fast_rejects_hidden_items_cached_state(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrHashKeyValuePair *pair;
    SZrObject *hiddenItemsObject;
    SZrTypeValue assignedValue;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    pair = fixture.instance->cachedStringLookupPair;
    TEST_ASSERT_NOT_NULL(pair);
    hiddenItemsObject = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
    TEST_ASSERT_NOT_NULL(hiddenItemsObject);
    ZrCore_Object_Init(state, hiddenItemsObject);
    fixture.instance->cachedHiddenItemsObject = hiddenItemsObject;
    ZrCore_Value_InitAsInt(state, &assignedValue, 105);

    TEST_ASSERT_FALSE(
            object_try_set_existing_pair_plain_value_fast_unchecked(state, fixture.instance, pair, &assignedValue));
    TEST_ASSERT_EQUAL_INT64(73, pair->value.value.nativeObject.nativeInt64);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_object_try_set_existing_string_pair_plain_value_assume_non_hidden_updates_value_with_hidden_items_cached_state(
        void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrHashKeyValuePair *pair;
    SZrObject *hiddenItemsObject;
    SZrTypeValue assignedValue;
    TZrUInt32 initialMemberVersion;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    pair = fixture.instance->cachedStringLookupPair;
    TEST_ASSERT_NOT_NULL(pair);
    hiddenItemsObject = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
    TEST_ASSERT_NOT_NULL(hiddenItemsObject);
    ZrCore_Object_Init(state, hiddenItemsObject);
    fixture.instance->cachedHiddenItemsObject = hiddenItemsObject;
    initialMemberVersion = fixture.instance->memberVersion;
    ZrCore_Value_InitAsInt(state, &assignedValue, 105);

    TEST_ASSERT_TRUE(object_try_set_existing_string_pair_plain_value_assume_non_hidden_unchecked(
            state, fixture.instance, pair, &assignedValue));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, pair->value.type);
    TEST_ASSERT_EQUAL_INT64(105, pair->value.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_PTR(pair, fixture.instance->cachedStringLookupPair);
    TEST_ASSERT_EQUAL_PTR(hiddenItemsObject, fixture.instance->cachedHiddenItemsObject);
    TEST_ASSERT_EQUAL_UINT32(initialMemberVersion, fixture.instance->memberVersion);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_object_get_member_cached_descriptor_records_helpers_from_state_without_tls_current(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrProfileRuntime profileRuntime;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    ZrCore_Value_ResetAsNull(&result);
    reset_profile_counters_from_state_only(state, &profileRuntime);

    TEST_ASSERT_TRUE(ZrCore_Object_GetMemberCachedDescriptorUnchecked(
            state, &fixture.receiverValue, fixture.prototype, 0u, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(73, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_GET_MEMBER]);
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY]);

    clear_profile_counters(state);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_object_get_value_populates_own_string_lookup_cache(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrTypeValue key;
    const SZrTypeValue *result;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    init_string_key(state, fixture.memberName, &key);
    fixture.instance->cachedStringLookupPair = ZR_NULL;

    result = ZrCore_Object_GetValue(state, fixture.instance, &key);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result->type);
    TEST_ASSERT_EQUAL_INT64(73, result->value.nativeObject.nativeInt64);
    TEST_ASSERT_NOT_NULL(fixture.instance->cachedStringLookupPair);
    TEST_ASSERT_EQUAL_PTR(fixture.memberName,
                          ZR_CAST_STRING(state, fixture.instance->cachedStringLookupPair->key.value.object));

    ZrTests_Runtime_State_Destroy(state);
}

static void test_object_get_value_populates_prototype_string_lookup_cache_on_fallback(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrTypeValue key;
    const SZrTypeValue *result;

    TEST_ASSERT_NOT_NULL(state);
    init_callable_member_access_fixture(state, &fixture);
    init_string_key(state, fixture.memberName, &key);
    fixture.instance->cachedStringLookupPair = ZR_NULL;
    fixture.prototype->super.cachedStringLookupPair = ZR_NULL;

    result = ZrCore_Object_GetValue(state, fixture.instance, &key);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_PTR(fixture.callableFunction, result->value.object);
    TEST_ASSERT_NULL(fixture.instance->cachedStringLookupPair);
    TEST_ASSERT_NOT_NULL(fixture.prototype->super.cachedStringLookupPair);
    TEST_ASSERT_EQUAL_PTR(fixture.memberName,
                          ZR_CAST_STRING(state, fixture.prototype->super.cachedStringLookupPair->key.value.object));

    ZrTests_Runtime_State_Destroy(state);
}

static void test_object_try_get_member_fast_allows_null_key_for_own_field_hit(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrTypeValue result;
    TZrBool handled = ZR_FALSE;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    fixture.instance->cachedStringLookupPair = ZR_NULL;
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(ZrCore_Object_TryGetMemberWithKeyFastUnchecked(
            state, &fixture.receiverValue, fixture.memberName, ZR_NULL, &result, &handled));
    TEST_ASSERT_TRUE(handled);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(73, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_NOT_NULL(fixture.instance->cachedStringLookupPair);
    TEST_ASSERT_EQUAL_PTR(fixture.memberName,
                          ZR_CAST_STRING(state, fixture.instance->cachedStringLookupPair->key.value.object));

    ZrTests_Runtime_State_Destroy(state);
}

static void test_object_try_get_member_fast_allows_null_key_for_prototype_method_hit(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrTypeValue result;
    TZrBool handled = ZR_FALSE;

    TEST_ASSERT_NOT_NULL(state);
    init_callable_member_access_fixture(state, &fixture);
    fixture.prototype->super.cachedStringLookupPair = ZR_NULL;
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(ZrCore_Object_TryGetMemberWithKeyFastUnchecked(
            state, &fixture.receiverValue, fixture.memberName, ZR_NULL, &result, &handled));
    TEST_ASSERT_TRUE(handled);
    TEST_ASSERT_EQUAL_PTR(fixture.callableFunction, result.value.object);
    TEST_ASSERT_NOT_NULL(fixture.prototype->super.cachedStringLookupPair);
    TEST_ASSERT_EQUAL_PTR(fixture.memberName,
                          ZR_CAST_STRING(state, fixture.prototype->super.cachedStringLookupPair->key.value.object));

    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_get_cached_callable_receiver_hit_does_not_fallback_when_descriptor_index_is_missing(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    init_callable_member_access_fixture(state, &fixture);
    fixture.cacheEntry.picSlotCount = 1;
    fixture.cacheEntry.picSlots[0].cachedReceiverPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[0].cachedOwnerPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[0].cachedFunction = fixture.callableFunction;
    fixture.cacheEntry.picSlots[0].cachedReceiverObject = fixture.instance;
    fixture.cacheEntry.picSlots[0].cachedReceiverBuckets = fixture.instance->nodeMap.buckets;
    fixture.cacheEntry.picSlots[0].cachedReceiverVersion = fixture.prototype->super.memberVersion;
    fixture.cacheEntry.picSlots[0].cachedOwnerVersion = fixture.prototype->super.memberVersion;
    fixture.cacheEntry.picSlots[0].cachedReceiverElementCount = (TZrUInt32)fixture.instance->nodeMap.elementCount;
    fixture.cacheEntry.picSlots[0].cachedDescriptorIndex = ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, &fixture.function, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_EQUAL_PTR(fixture.callableFunction, result.value.object);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_get_cached_callable_receiver_object_hit_does_not_require_receiver_shape_cache(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    init_callable_member_access_fixture(state, &fixture);
    fixture.cacheEntry.picSlotCount = 2;
    memset(&fixture.cacheEntry.picSlots[0], 0, sizeof(fixture.cacheEntry.picSlots[0]));
    fixture.cacheEntry.picSlots[1].cachedReceiverPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[1].cachedOwnerPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[1].cachedFunction = fixture.callableFunction;
    fixture.cacheEntry.picSlots[1].cachedReceiverObject = fixture.instance;
    fixture.cacheEntry.picSlots[1].cachedReceiverBuckets = ZR_NULL;
    fixture.cacheEntry.picSlots[1].cachedReceiverVersion = fixture.prototype->super.memberVersion;
    fixture.cacheEntry.picSlots[1].cachedOwnerVersion = fixture.prototype->super.memberVersion;
    fixture.cacheEntry.picSlots[1].cachedReceiverElementCount = 0u;
    fixture.cacheEntry.picSlots[1].cachedDescriptorIndex = ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, &fixture.function, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_EQUAL_PTR(fixture.callableFunction, result.value.object);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_get_cached_refresh_keeps_non_hidden_plain_value_fast_set_flag_clear_for_instance_field(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrFunction *runtimeFunction;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    runtimeFunction = create_runtime_fixture_function(state, &fixture);
    fixture.cacheEntry.picSlotCount = 0;
    fixture.cacheEntry.picNextInsertIndex = 0;
    memset(&fixture.cacheEntry.picSlots[0], 0, sizeof(fixture.cacheEntry.picSlots[0]));
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, runtimeFunction, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.picSlotCount);
    TEST_ASSERT_TRUE((fixture.cacheEntry.picSlots[0].reserved1 &
                      ZR_FUNCTION_CALLSITE_PIC_SLOT_FLAG_NON_HIDDEN_STRING_PAIR_FAST_SET) == 0u);

    detach_runtime_fixture_function(runtimeFunction);
    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_member_get_by_name_fast_path_skips_extra_stable_result_copy);
    RUN_TEST(test_member_get_cached_fast_path_skips_extra_stable_result_copy);
    RUN_TEST(test_member_get_cached_stack_slot_fast_path_skips_anchor_copy);
    RUN_TEST(test_member_get_cached_hit_does_not_record_callsite_lookup_slowpath);
    RUN_TEST(test_member_set_cached_hit_does_not_record_callsite_lookup_slowpath);
    RUN_TEST(test_member_get_cached_refresh_stores_callable_target_for_method_descriptor);
    RUN_TEST(test_member_get_cached_refresh_stores_receiver_object_for_callable_method_descriptor);
    RUN_TEST(test_member_get_cached_refresh_stores_callable_target_for_descriptorless_prototype_value);
    RUN_TEST(test_member_get_cached_descriptorless_callable_receiver_slot_hits_on_second_access);
    RUN_TEST(test_member_get_cached_descriptorless_callable_refresh_survives_receiver_result_alias);
    RUN_TEST(test_member_get_cached_refresh_marks_instance_field_slot_kind);
    RUN_TEST(test_member_get_cached_refresh_stores_receiver_pair_for_instance_field_descriptor);
    RUN_TEST(test_member_get_cached_refresh_stores_receiver_object_for_instance_field_pair_descriptor);
    RUN_TEST(test_member_get_cached_refresh_uses_bound_member_entry_without_descriptor_name);
    RUN_TEST(test_member_get_cached_instance_field_hit_does_not_fallback_when_descriptor_index_is_missing);
    RUN_TEST(test_member_get_cached_instance_field_pair_hit_does_not_require_descriptor_or_member_name);
    RUN_TEST(test_member_get_cached_instance_field_pair_receiver_object_hit_does_not_require_receiver_shape_cache);
    RUN_TEST(test_member_get_cached_exact_receiver_object_hit_uses_member_name_when_descriptor_name_is_missing);
    RUN_TEST(test_member_get_cached_exact_receiver_object_backfills_cached_member_name_for_symbolless_followup);
    RUN_TEST(test_member_set_cached_instance_field_hit_does_not_fallback_when_descriptor_index_is_missing);
    RUN_TEST(test_member_set_cached_exact_receiver_object_hit_uses_member_name_when_descriptor_name_is_missing);
    RUN_TEST(test_member_set_cached_exact_receiver_object_backfills_cached_member_name_for_symbolless_followup);
    RUN_TEST(test_member_set_cached_instance_field_pair_receiver_object_hit_does_not_require_receiver_shape_cache);
    RUN_TEST(test_member_set_cached_instance_field_pair_hit_does_not_require_descriptor_or_member_name);
    RUN_TEST(test_member_set_cached_instance_field_pair_hit_does_not_bump_receiver_member_version);
    RUN_TEST(test_member_get_cached_exact_receiver_pair_hit_ignores_cached_version_mismatch);
    RUN_TEST(test_member_set_cached_exact_receiver_pair_hit_ignores_cached_version_mismatch);
    RUN_TEST(test_member_get_cached_exact_receiver_pair_hit_records_helpers_from_state_without_tls_current);
    RUN_TEST(test_member_set_cached_exact_receiver_pair_hit_records_helpers_from_state_without_tls_current);
    RUN_TEST(test_object_try_set_existing_pair_plain_value_fast_updates_value_and_lookup_cache);
    RUN_TEST(test_object_try_set_existing_pair_plain_value_fast_rejects_hidden_items_cached_state);
    RUN_TEST(test_object_try_set_existing_string_pair_plain_value_assume_non_hidden_updates_value_with_hidden_items_cached_state);
    RUN_TEST(test_object_get_member_cached_descriptor_records_helpers_from_state_without_tls_current);
    RUN_TEST(test_object_get_value_populates_own_string_lookup_cache);
    RUN_TEST(test_object_get_value_populates_prototype_string_lookup_cache_on_fallback);
    RUN_TEST(test_object_try_get_member_fast_allows_null_key_for_own_field_hit);
    RUN_TEST(test_object_try_get_member_fast_allows_null_key_for_prototype_method_hit);
    RUN_TEST(test_member_get_cached_callable_receiver_hit_does_not_fallback_when_descriptor_index_is_missing);
    RUN_TEST(test_member_get_cached_callable_receiver_object_hit_does_not_require_receiver_shape_cache);
    RUN_TEST(test_member_get_cached_refresh_keeps_non_hidden_plain_value_fast_set_flag_clear_for_instance_field);

    return UNITY_END();
}
