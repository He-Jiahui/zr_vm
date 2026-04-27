#include "unity.h"

#include <string.h>

#include "tests/harness/runtime_support.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/ownership.h"
#include "zr_vm_core/profile.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_core/src/zr_vm_core/execution/execution_internal.h"
#include "zr_vm_core/src/zr_vm_core/object/object_internal.h"
#include "zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h"

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

typedef struct SZrSelectiveAllocatorContext {
    TZrSize failAllocateSize;
    TZrBool failAllocateActive;
    TZrUInt32 failAllocateCount;
} SZrSelectiveAllocatorContext;

static TZrPtr test_selective_allocator(TZrPtr userData,
                                       TZrPtr pointer,
                                       TZrSize originalSize,
                                       TZrSize newSize,
                                       TZrInt64 flag) {
    SZrSelectiveAllocatorContext *context = (SZrSelectiveAllocatorContext *)userData;

    if (context != ZR_NULL &&
        context->failAllocateActive &&
        pointer == ZR_NULL &&
        newSize == context->failAllocateSize) {
        context->failAllocateCount++;
        return ZR_NULL;
    }

    return ZrTests_Runtime_Allocator_Default(userData, pointer, originalSize, newSize, flag);
}

static SZrState *create_runtime_state_with_selective_allocator(SZrSelectiveAllocatorContext *context) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global = ZrCore_GlobalState_New(test_selective_allocator, context, 12345, &callbacks);
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

static void fill_ignore_registry_to_capacity(SZrState *state) {
    SZrGarbageCollector *collector;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);
    TEST_ASSERT_NOT_NULL(state->global->garbageCollector);

    collector = state->global->garbageCollector;
    while (collector->ignoredObjectCount < collector->ignoredObjectCapacity) {
        SZrObject *fillerObject = ZrCore_Object_New(state, ZR_NULL);

        TEST_ASSERT_NOT_NULL(fillerObject);
        ZrCore_Object_Init(state, fillerObject);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(fillerObject)));
    }
}

static TZrSize ignore_registry_growth_allocation_size(const SZrGarbageCollector *collector) {
    TZrSize currentCapacity;

    TEST_ASSERT_NOT_NULL(collector);

    currentCapacity = collector->ignoredObjectCapacity;
    TEST_ASSERT_TRUE(currentCapacity > 0u);
    return currentCapacity * 2u * sizeof(SZrRawObject *);
}

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

static SZrFunction *create_native_callable(SZrState *state, FZrNativeFunction nativeFunction) {
    SZrClosureNative *closure;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(nativeFunction);

    closure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(closure);

    closure->nativeFunction = nativeFunction;
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    return ZR_CAST(SZrFunction *, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
}

static TZrInt64 test_member_property_getter_native(SZrState *state) {
    TZrStackValuePointer base;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    base = state->callInfoList->functionBase.valuePointer;
    ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValueNoProfile(base), 77);
    state->stackTop.valuePointer = base + 1;
    return 1;
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

static SZrObject *create_plain_member_value_object(SZrState *state) {
    SZrObject *object;

    TEST_ASSERT_NOT_NULL(state);

    object = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(object);
    ZrCore_Object_Init(state, object);
    return object;
}

static SZrObject *create_struct_member_value_object(SZrState *state, TZrNativeString prototypeNameNative) {
    SZrString *prototypeName;
    SZrStructPrototype *prototype;
    SZrObject *object;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(prototypeNameNative);

    prototypeName = ZrCore_String_CreateFromNative(state, prototypeNameNative);
    TEST_ASSERT_NOT_NULL(prototypeName);
    prototype = ZrCore_StructPrototype_New(state, prototypeName);
    TEST_ASSERT_NOT_NULL(prototype);

    object = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_STRUCT);
    TEST_ASSERT_NOT_NULL(object);
    object->prototype = &prototype->super;
    ZrCore_Object_Init(state, object);
    return object;
}

static SZrHashKeyValuePair *set_member_access_fixture_object_value(SZrState *state,
                                                                   SZrMemberAccessFixture *fixture,
                                                                   SZrObject *object) {
    SZrTypeValue key;
    SZrTypeValue value;
    SZrHashKeyValuePair *pair;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(fixture);
    TEST_ASSERT_NOT_NULL(fixture->instance);
    TEST_ASSERT_NOT_NULL(fixture->memberName);
    TEST_ASSERT_NOT_NULL(object);

    init_string_key(state, fixture->memberName, &key);
    ZrCore_Value_InitAsRawObject(state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    value.type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Object_SetValue(state, fixture->instance, &key, &value);

    pair = object_get_own_string_pair_by_name_cached_unchecked(state, fixture->instance, fixture->memberName);
    TEST_ASSERT_NOT_NULL(pair);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, pair->value.type);
    return pair;
}

static void init_property_getter_member_access_fixture(SZrState *state, SZrMemberAccessFixture *fixture) {
    SZrString *typeName;
    SZrMemberDescriptor descriptor;
    SZrObject *instance;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(fixture);

    memset(fixture, 0, sizeof(*fixture));

    typeName = ZrCore_String_CreateFromNative(state, "HotPathPropertyGetterBox");
    fixture->memberName = ZrCore_String_CreateFromNative(state, "value");
    TEST_ASSERT_NOT_NULL(typeName);
    TEST_ASSERT_NOT_NULL(fixture->memberName);

    fixture->prototype = ZrCore_ObjectPrototype_New(state, typeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    TEST_ASSERT_NOT_NULL(fixture->prototype);

    fixture->callableFunction = create_native_callable(state, test_member_property_getter_native);
    TEST_ASSERT_NOT_NULL(fixture->callableFunction);

    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.name = fixture->memberName;
    descriptor.kind = ZR_MEMBER_DESCRIPTOR_KIND_PROPERTY;
    descriptor.isWritable = ZR_FALSE;
    descriptor.getterFunction = fixture->callableFunction;
    TEST_ASSERT_TRUE(ZrCore_ObjectPrototype_AddMemberDescriptor(state, fixture->prototype, &descriptor));

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
    fixture->cacheEntry.picSlotCount = 1;
    fixture->cacheEntry.picSlots[0].cachedReceiverPrototype = fixture->prototype;
    fixture->cacheEntry.picSlots[0].cachedOwnerPrototype = fixture->prototype;
    fixture->cacheEntry.picSlots[0].cachedReceiverVersion = fixture->prototype->super.memberVersion;
    fixture->cacheEntry.picSlots[0].cachedOwnerVersion = fixture->prototype->super.memberVersion;
    fixture->cacheEntry.picSlots[0].cachedDescriptorIndex = 0u;
}

static void init_shared_name_member_access_variant(SZrState *state,
                                                   SZrString *memberName,
                                                   TZrNativeString typeNameNative,
                                                   TZrInt64 storedValue,
                                                   SZrObjectPrototype **outPrototype,
                                                   SZrObject **outInstance,
                                                   SZrTypeValue *outReceiverValue) {
    SZrString *typeName;
    SZrMemberDescriptor descriptor;
    SZrTypeValue key;
    SZrTypeValue value;
    SZrObjectPrototype *prototype;
    SZrObject *instance;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(memberName);
    TEST_ASSERT_NOT_NULL(typeNameNative);
    TEST_ASSERT_NOT_NULL(outPrototype);
    TEST_ASSERT_NOT_NULL(outInstance);
    TEST_ASSERT_NOT_NULL(outReceiverValue);

    typeName = ZrCore_String_CreateFromNative(state, typeNameNative);
    TEST_ASSERT_NOT_NULL(typeName);

    prototype = ZrCore_ObjectPrototype_New(state, typeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    TEST_ASSERT_NOT_NULL(prototype);

    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.name = memberName;
    descriptor.kind = ZR_MEMBER_DESCRIPTOR_KIND_FIELD;
    descriptor.isWritable = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrCore_ObjectPrototype_AddMemberDescriptor(state, prototype, &descriptor));

    instance = ZrCore_Object_New(state, prototype);
    TEST_ASSERT_NOT_NULL(instance);
    ZrCore_Object_Init(state, instance);

    init_string_key(state, memberName, &key);
    ZrCore_Value_InitAsInt(state, &value, storedValue);
    ZrCore_Object_SetValue(state, instance, &key, &value);

    ZrCore_Value_InitAsRawObject(state, outReceiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(instance));
    outReceiverValue->type = ZR_VALUE_TYPE_OBJECT;

    *outPrototype = prototype;
    *outInstance = instance;
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

static SZrTypeValue *reserve_stack_result_slot_with_headroom(SZrState *state, TZrSize headroomSlots) {
    TZrStackValuePointer functionBase;
    SZrTypeValue *result;

    TEST_ASSERT_NOT_NULL(state);

    TEST_ASSERT_TRUE(headroomSlots <= (TZrSize)(state->stackTail.valuePointer - state->stackBase.valuePointer));

    functionBase = state->stackTail.valuePointer - (2 + headroomSlots);
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

static SZrTypeValue *reserve_stack_result_slot(SZrState *state) {
    return reserve_stack_result_slot_with_headroom(state, 0u);
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

static void test_member_get_cached_instance_field_pair_hit_plain_heap_object_reuses_original_object(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrProfileRuntime profileRuntime;
    SZrTypeValue result;
    SZrObject *storedObject;
    SZrHashKeyValuePair *storedPair;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    storedObject = create_plain_member_value_object(state);
    storedPair = set_member_access_fixture_object_value(state, &fixture, storedObject);
    fixture.cacheEntry.picSlots[0].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD;
    fixture.cacheEntry.picSlots[0].cachedReceiverObject = fixture.instance;
    fixture.cacheEntry.picSlots[0].cachedReceiverPair = storedPair;
    fixture.cacheEntry.picSlots[0].cachedDescriptorIndex = ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE;
    fixture.cacheEntry.picSlots[0].cachedMemberName = ZR_NULL;
    fixture.function.memberEntries = ZR_NULL;
    fixture.function.memberEntryLength = 0;
    ZrCore_Value_ResetAsNull(&result);
    reset_profile_counters(state, &profileRuntime);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, &fixture.function, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);
    TEST_ASSERT_TRUE(result.isGarbageCollectable);
    TEST_ASSERT_EQUAL_PTR(storedPair->value.value.object, result.value.object);
    TEST_ASSERT_EQUAL_PTR(storedObject, ZR_CAST_OBJECT(state, result.value.object));
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_VALUE_KIND_NONE, result.ownershipKind);
    TEST_ASSERT_NULL(result.ownershipControl);
    TEST_ASSERT_NULL(result.ownershipWeakRef);
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_GET_MEMBER]);
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY]);

    clear_profile_counters(state);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_get_cached_instance_field_pair_hit_struct_object_still_clones_result(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrProfileRuntime profileRuntime;
    SZrTypeValue result;
    SZrObject *sourceObject;
    SZrHashKeyValuePair *storedPair;
    SZrObject *storedObject;
    SZrObject *copiedObject;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    sourceObject = create_struct_member_value_object(state, "HotPathPairStructValue");
    storedPair = set_member_access_fixture_object_value(state, &fixture, sourceObject);
    storedObject = ZR_CAST_OBJECT(state, storedPair->value.value.object);
    TEST_ASSERT_NOT_NULL(storedObject);
    fixture.cacheEntry.picSlots[0].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD;
    fixture.cacheEntry.picSlots[0].cachedReceiverObject = fixture.instance;
    fixture.cacheEntry.picSlots[0].cachedReceiverPair = storedPair;
    fixture.cacheEntry.picSlots[0].cachedDescriptorIndex = ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE;
    fixture.cacheEntry.picSlots[0].cachedMemberName = ZR_NULL;
    fixture.function.memberEntries = ZR_NULL;
    fixture.function.memberEntryLength = 0;
    ZrCore_Value_ResetAsNull(&result);
    reset_profile_counters(state, &profileRuntime);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, &fixture.function, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);
    TEST_ASSERT_TRUE(result.isGarbageCollectable);
    TEST_ASSERT_NOT_EQUAL(storedPair->value.value.object, result.value.object);
    copiedObject = ZR_CAST_OBJECT(state, result.value.object);
    TEST_ASSERT_NOT_NULL(copiedObject);
    TEST_ASSERT_EQUAL_INT(ZR_OBJECT_INTERNAL_TYPE_STRUCT, copiedObject->internalType);
    TEST_ASSERT_EQUAL_PTR(storedObject->prototype, copiedObject->prototype);
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_GET_MEMBER]);
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY]);

    clear_profile_counters(state);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_get_cached_exact_receiver_object_hit_plain_heap_object_reuses_original_object(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrProfileRuntime profileRuntime;
    SZrFunction *runtimeFunction;
    SZrTypeValue result;
    SZrObject *storedObject;
    SZrHashKeyValuePair *storedPair;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    storedObject = create_plain_member_value_object(state);
    storedPair = set_member_access_fixture_object_value(state, &fixture, storedObject);
    runtimeFunction = create_runtime_fixture_function(state, &fixture);
    fixture.cacheEntry.picSlots[0].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD;
    fixture.cacheEntry.picSlots[0].cachedReceiverObject = fixture.instance;
    fixture.cacheEntry.picSlots[0].cachedReceiverPair = ZR_NULL;
    fixture.cacheEntry.picSlots[0].cachedMemberName = fixture.memberName;
    fixture.cacheEntry.picSlots[0].cachedReceiverPrototype = ZR_NULL;
    fixture.cacheEntry.picSlots[0].cachedOwnerPrototype = ZR_NULL;
    fixture.cacheEntry.picSlots[0].cachedDescriptorIndex = ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE;
    runtimeFunction->memberEntries = ZR_NULL;
    runtimeFunction->memberEntryLength = 0;
    ZrCore_Value_ResetAsNull(&result);
    reset_profile_counters(state, &profileRuntime);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, runtimeFunction, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);
    TEST_ASSERT_TRUE(result.isGarbageCollectable);
    TEST_ASSERT_EQUAL_PTR(storedPair, fixture.cacheEntry.picSlots[0].cachedReceiverPair);
    TEST_ASSERT_EQUAL_PTR(storedPair->value.value.object, result.value.object);
    TEST_ASSERT_EQUAL_PTR(storedObject, ZR_CAST_OBJECT(state, result.value.object));
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_VALUE_KIND_NONE, result.ownershipKind);
    TEST_ASSERT_NULL(result.ownershipControl);
    TEST_ASSERT_NULL(result.ownershipWeakRef);
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_GET_MEMBER]);
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY]);

    clear_profile_counters(state);
    detach_runtime_fixture_function(runtimeFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_get_cached_exact_receiver_object_hit_struct_object_still_clones_result(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrProfileRuntime profileRuntime;
    SZrFunction *runtimeFunction;
    SZrTypeValue result;
    SZrObject *sourceObject;
    SZrHashKeyValuePair *storedPair;
    SZrObject *storedObject;
    SZrObject *copiedObject;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    sourceObject = create_struct_member_value_object(state, "HotPathObjectStructValue");
    storedPair = set_member_access_fixture_object_value(state, &fixture, sourceObject);
    storedObject = ZR_CAST_OBJECT(state, storedPair->value.value.object);
    TEST_ASSERT_NOT_NULL(storedObject);
    runtimeFunction = create_runtime_fixture_function(state, &fixture);
    fixture.cacheEntry.picSlots[0].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD;
    fixture.cacheEntry.picSlots[0].cachedReceiverObject = fixture.instance;
    fixture.cacheEntry.picSlots[0].cachedReceiverPair = ZR_NULL;
    fixture.cacheEntry.picSlots[0].cachedMemberName = fixture.memberName;
    fixture.cacheEntry.picSlots[0].cachedReceiverPrototype = ZR_NULL;
    fixture.cacheEntry.picSlots[0].cachedOwnerPrototype = ZR_NULL;
    fixture.cacheEntry.picSlots[0].cachedDescriptorIndex = ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE;
    runtimeFunction->memberEntries = ZR_NULL;
    runtimeFunction->memberEntryLength = 0;
    ZrCore_Value_ResetAsNull(&result);
    reset_profile_counters(state, &profileRuntime);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, runtimeFunction, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);
    TEST_ASSERT_TRUE(result.isGarbageCollectable);
    TEST_ASSERT_EQUAL_PTR(storedPair, fixture.cacheEntry.picSlots[0].cachedReceiverPair);
    TEST_ASSERT_NOT_EQUAL(storedPair->value.value.object, result.value.object);
    copiedObject = ZR_CAST_OBJECT(state, result.value.object);
    TEST_ASSERT_NOT_NULL(copiedObject);
    TEST_ASSERT_EQUAL_INT(ZR_OBJECT_INTERNAL_TYPE_STRUCT, copiedObject->internalType);
    TEST_ASSERT_EQUAL_PTR(storedObject->prototype, copiedObject->prototype);
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_GET_MEMBER]);
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY]);

    clear_profile_counters(state);
    detach_runtime_fixture_function(runtimeFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_get_cached_stack_slot_fast_path_skips_anchor_copy(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrProfileRuntime profileRuntime;
    SZrTypeValue *result;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 97);
    result = reserve_stack_result_slot_with_headroom(state, 2u);
    reset_profile_counters(state, &profileRuntime);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, &fixture.function, 0, &fixture.receiverValue, result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result->type);
    TEST_ASSERT_EQUAL_INT64(97, result->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY]);
    TEST_ASSERT_EQUAL_UINT64(0u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_RESET_NULL]);
    TEST_ASSERT_EQUAL_UINT64(0u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_STACK_GET_VALUE]);

    clear_profile_counters(state);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_get_cached_property_getter_stack_slot_skips_anchor_restore_stack_lookup_when_stack_unchanged(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrProfileRuntime profileRuntime;
    SZrTypeValue *result;

    TEST_ASSERT_NOT_NULL(state);
    init_property_getter_member_access_fixture(state, &fixture);
    result = reserve_stack_result_slot_with_headroom(state, 2u);
    reset_profile_counters(state, &profileRuntime);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, &fixture.function, 0, &fixture.receiverValue, result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result->type);
    TEST_ASSERT_EQUAL_INT64(77, result->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_GET_MEMBER]);
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY]);
    TEST_ASSERT_EQUAL_UINT64(0u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_STACK_GET_VALUE]);

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
    TEST_ASSERT_EQUAL_UINT32(fixture.prototype->super.memberVersion,
                             fixture.cacheEntry.picSlots[0].cachedReceiverVersion);
    TEST_ASSERT_EQUAL_UINT32(fixture.prototype->super.memberVersion,
                             fixture.cacheEntry.picSlots[0].cachedOwnerVersion);

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
    TEST_ASSERT_EQUAL_PTR(fixture.instance, fixture.cacheEntry.picSlots[0].cachedReceiverObject);
    TEST_ASSERT_EQUAL_UINT32(fixture.prototype->super.memberVersion,
                             fixture.cacheEntry.picSlots[0].cachedReceiverVersion);
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
    fixture.cacheEntry.picSlots[0].cachedReceiverObject = fixture.instance;
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
    fixture.cacheEntry.picSlots[0].cachedReceiverVersion = fixture.prototype->super.memberVersion + 1u;
    fixture.cacheEntry.picSlots[0].cachedOwnerVersion = fixture.prototype->super.memberVersion + 1u;
    fixture.prototype->memberDescriptors[0].name = ZR_NULL;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, runtimeFunction, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(73, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(fixture.prototype->super.memberVersion,
                             fixture.cacheEntry.picSlots[0].cachedReceiverVersion);
    TEST_ASSERT_EQUAL_UINT32(fixture.prototype->super.memberVersion,
                             fixture.cacheEntry.picSlots[0].cachedOwnerVersion);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);

    detach_runtime_fixture_function(runtimeFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_get_cached_exact_receiver_object_hit_uses_cached_member_name_without_owner_metadata(void) {
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
    fixture.cacheEntry.picSlots[0].cachedMemberName = fixture.memberName;
    fixture.cacheEntry.picSlots[0].cachedReceiverPrototype = ZR_NULL;
    fixture.cacheEntry.picSlots[0].cachedOwnerPrototype = ZR_NULL;
    fixture.cacheEntry.picSlots[0].cachedDescriptorIndex = ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE;
    runtimeFunction->memberEntries = ZR_NULL;
    runtimeFunction->memberEntryLength = 0;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;
    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_NULL(fixture.cacheEntry.picSlots[0].cachedReceiverPair);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, runtimeFunction, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(73, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_NOT_NULL(fixture.cacheEntry.picSlots[0].cachedReceiverPair);
    TEST_ASSERT_EQUAL_PTR(fixture.instance->cachedStringLookupPair, fixture.cacheEntry.picSlots[0].cachedReceiverPair);
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

static void test_member_get_cached_exact_receiver_object_backfills_pair_from_slot_cached_name_without_descriptor_or_symbol(
        void) {
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
    fixture.cacheEntry.picSlots[0].cachedMemberName = fixture.memberName;
    fixture.cacheEntry.picSlots[0].cachedReceiverPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[0].cachedOwnerPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[0].cachedReceiverVersion = fixture.prototype->super.memberVersion;
    fixture.cacheEntry.picSlots[0].cachedOwnerVersion = fixture.prototype->super.memberVersion;
    fixture.cacheEntry.picSlots[0].cachedDescriptorIndex = 0u;
    fixture.prototype->memberDescriptors[0].name = ZR_NULL;
    runtimeFunction->memberEntries = ZR_NULL;
    runtimeFunction->memberEntryLength = 0;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;
    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_NULL(fixture.cacheEntry.picSlots[0].cachedReceiverPair);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, runtimeFunction, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(73, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_PTR(fixture.memberName, fixture.cacheEntry.picSlots[0].cachedMemberName);
    TEST_ASSERT_EQUAL_PTR(fixture.instance, fixture.cacheEntry.picSlots[0].cachedReceiverObject);
    TEST_ASSERT_NOT_NULL(fixture.cacheEntry.picSlots[0].cachedReceiverPair);
    TEST_ASSERT_EQUAL_PTR(fixture.instance->cachedStringLookupPair, fixture.cacheEntry.picSlots[0].cachedReceiverPair);

    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, runtimeFunction, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(73, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(2u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);

    detach_runtime_fixture_function(runtimeFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_get_cached_multi_slot_exact_receiver_object_version_mismatch_clears_slot_and_falls_back(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrFunction *runtimeFunction;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    runtimeFunction = create_runtime_fixture_function(state, &fixture);
    fixture.cacheEntry.picSlotCount = 2;
    memset(&fixture.cacheEntry.picSlots[0], 0, sizeof(fixture.cacheEntry.picSlots[0]));
    fixture.cacheEntry.picSlots[1].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD;
    fixture.cacheEntry.picSlots[1].cachedReceiverPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[1].cachedOwnerPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[1].cachedReceiverObject = fixture.instance;
    fixture.cacheEntry.picSlots[1].cachedReceiverPair = ZR_NULL;
    fixture.cacheEntry.picSlots[1].cachedDescriptorIndex = 0u;
    fixture.cacheEntry.picSlots[1].cachedMemberName = ZR_NULL;
    fixture.cacheEntry.picSlots[1].cachedReceiverVersion = fixture.prototype->super.memberVersion;
    fixture.cacheEntry.picSlots[1].cachedOwnerVersion = fixture.prototype->super.memberVersion;
    fixture.prototype->memberDescriptors[0].name = ZR_NULL;
    fixture.prototype->super.memberVersion++;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, runtimeFunction, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(73, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.picSlotCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeMissCount);

    detach_runtime_fixture_function(runtimeFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_get_cached_multi_slot_exact_receiver_object_hit_uses_member_name_when_descriptor_name_is_missing(
        void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrFunction *runtimeFunction;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    runtimeFunction = create_runtime_fixture_function(state, &fixture);
    fixture.cacheEntry.picSlotCount = 2;
    memset(&fixture.cacheEntry.picSlots[0], 0, sizeof(fixture.cacheEntry.picSlots[0]));
    fixture.cacheEntry.picSlots[1].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD;
    fixture.cacheEntry.picSlots[1].cachedReceiverPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[1].cachedOwnerPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[1].cachedReceiverObject = fixture.instance;
    fixture.cacheEntry.picSlots[1].cachedReceiverPair = ZR_NULL;
    fixture.cacheEntry.picSlots[1].cachedDescriptorIndex = 0u;
    fixture.cacheEntry.picSlots[1].cachedMemberName = ZR_NULL;
    fixture.cacheEntry.picSlots[1].cachedReceiverVersion = fixture.prototype->super.memberVersion;
    fixture.cacheEntry.picSlots[1].cachedOwnerVersion = fixture.prototype->super.memberVersion;
    fixture.prototype->memberDescriptors[0].name = ZR_NULL;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, runtimeFunction, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(73, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_PTR(fixture.memberName, fixture.cacheEntry.picSlots[1].cachedMemberName);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);

    detach_runtime_fixture_function(runtimeFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_get_cached_multi_slot_exact_receiver_object_hit_ignores_earlier_stale_prototype_slot(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    fixture.cacheEntry.picSlotCount = 2;
    fixture.cacheEntry.picSlots[0].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD;
    fixture.cacheEntry.picSlots[0].cachedReceiverPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[0].cachedOwnerPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[0].cachedReceiverVersion = fixture.prototype->super.memberVersion + 1u;
    fixture.cacheEntry.picSlots[0].cachedOwnerVersion = fixture.prototype->super.memberVersion + 1u;
    fixture.cacheEntry.picSlots[0].cachedDescriptorIndex = 0u;
    fixture.cacheEntry.picSlots[1].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD;
    fixture.cacheEntry.picSlots[1].cachedReceiverPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[1].cachedOwnerPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[1].cachedReceiverObject = fixture.instance;
    fixture.cacheEntry.picSlots[1].cachedReceiverPair = ZR_NULL;
    fixture.cacheEntry.picSlots[1].cachedReceiverVersion = fixture.prototype->super.memberVersion;
    fixture.cacheEntry.picSlots[1].cachedOwnerVersion = fixture.prototype->super.memberVersion;
    fixture.cacheEntry.picSlots[1].cachedDescriptorIndex = 0u;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, &fixture.function, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(73, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(2u, fixture.cacheEntry.picSlotCount);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_get_cached_multi_slot_exact_receiver_object_hit_uses_cached_member_name_without_owner_metadata(
        void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrFunction *runtimeFunction;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    runtimeFunction = create_runtime_fixture_function(state, &fixture);
    fixture.cacheEntry.picSlotCount = 2;
    memset(&fixture.cacheEntry.picSlots[0], 0, sizeof(fixture.cacheEntry.picSlots[0]));
    fixture.cacheEntry.picSlots[1].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD;
    fixture.cacheEntry.picSlots[1].cachedReceiverObject = fixture.instance;
    fixture.cacheEntry.picSlots[1].cachedReceiverPair = ZR_NULL;
    fixture.cacheEntry.picSlots[1].cachedMemberName = fixture.memberName;
    fixture.cacheEntry.picSlots[1].cachedReceiverPrototype = ZR_NULL;
    fixture.cacheEntry.picSlots[1].cachedOwnerPrototype = ZR_NULL;
    fixture.cacheEntry.picSlots[1].cachedDescriptorIndex = ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE;
    runtimeFunction->memberEntries = ZR_NULL;
    runtimeFunction->memberEntryLength = 0;
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

static void test_member_get_cached_multi_slot_exact_receiver_object_backfills_cached_member_name_to_sibling_slot(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrFunction *runtimeFunction;
    SZrObjectPrototype *otherPrototype;
    SZrObject *otherInstance;
    SZrTypeValue otherReceiverValue;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    init_shared_name_member_access_variant(
            state, fixture.memberName, "HotPathMemberBoxSibling", 105, &otherPrototype, &otherInstance, &otherReceiverValue);
    runtimeFunction = create_runtime_fixture_function(state, &fixture);
    fixture.cacheEntry.picSlotCount = 2;
    fixture.cacheEntry.picSlots[0].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD;
    fixture.cacheEntry.picSlots[0].cachedReceiverObject = fixture.instance;
    fixture.cacheEntry.picSlots[0].cachedReceiverPair = ZR_NULL;
    fixture.cacheEntry.picSlots[0].cachedMemberName = ZR_NULL;
    fixture.cacheEntry.picSlots[0].cachedReceiverPrototype = ZR_NULL;
    fixture.cacheEntry.picSlots[0].cachedOwnerPrototype = ZR_NULL;
    fixture.cacheEntry.picSlots[0].cachedDescriptorIndex = ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE;
    fixture.cacheEntry.picSlots[1].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD;
    fixture.cacheEntry.picSlots[1].cachedReceiverPrototype = otherPrototype;
    fixture.cacheEntry.picSlots[1].cachedOwnerPrototype = otherPrototype;
    fixture.cacheEntry.picSlots[1].cachedReceiverObject = otherInstance;
    fixture.cacheEntry.picSlots[1].cachedReceiverPair = ZR_NULL;
    fixture.cacheEntry.picSlots[1].cachedDescriptorIndex = 0u;
    fixture.cacheEntry.picSlots[1].cachedMemberName = ZR_NULL;
    fixture.cacheEntry.picSlots[1].cachedReceiverVersion = otherPrototype->super.memberVersion;
    fixture.cacheEntry.picSlots[1].cachedOwnerVersion = otherPrototype->super.memberVersion;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, runtimeFunction, 0, &otherReceiverValue, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(105, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_PTR(fixture.memberName, fixture.cacheEntry.picSlots[0].cachedMemberName);
    TEST_ASSERT_EQUAL_PTR(fixture.memberName, fixture.cacheEntry.picSlots[1].cachedMemberName);

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
    fixture.cacheEntry.picSlots[0].cachedReceiverVersion = fixture.prototype->super.memberVersion + 1u;
    fixture.cacheEntry.picSlots[0].cachedOwnerVersion = fixture.prototype->super.memberVersion + 1u;
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
    TEST_ASSERT_EQUAL_UINT32(fixture.prototype->super.memberVersion,
                             fixture.cacheEntry.picSlots[0].cachedReceiverVersion);
    TEST_ASSERT_EQUAL_UINT32(fixture.prototype->super.memberVersion,
                             fixture.cacheEntry.picSlots[0].cachedOwnerVersion);
    TEST_ASSERT_EQUAL_UINT32(initialMemberVersion, fixture.instance->memberVersion);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);

    detach_runtime_fixture_function(runtimeFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_set_cached_exact_receiver_object_hit_uses_cached_member_name_without_owner_metadata(void) {
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
    fixture.cacheEntry.picSlots[0].cachedMemberName = fixture.memberName;
    fixture.cacheEntry.picSlots[0].cachedReceiverPrototype = ZR_NULL;
    fixture.cacheEntry.picSlots[0].cachedOwnerPrototype = ZR_NULL;
    fixture.cacheEntry.picSlots[0].cachedDescriptorIndex = ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE;
    runtimeFunction->memberEntries = ZR_NULL;
    runtimeFunction->memberEntryLength = 0;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;
    initialMemberVersion = fixture.instance->memberVersion;
    ZrCore_Value_InitAsInt(state, &assignedValue, 105);
    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_NULL(fixture.cacheEntry.picSlots[0].cachedReceiverPair);

    TEST_ASSERT_TRUE(execution_member_set_cached(
            state, ZR_NULL, runtimeFunction, 0, &fixture.receiverValue, &assignedValue));
    TEST_ASSERT_TRUE(execution_member_get_by_name(state, ZR_NULL, &fixture.receiverValue, fixture.memberName, &result));
    TEST_ASSERT_EQUAL_INT64(105, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_NOT_NULL(fixture.cacheEntry.picSlots[0].cachedReceiverPair);
    TEST_ASSERT_EQUAL_PTR(fixture.instance->cachedStringLookupPair, fixture.cacheEntry.picSlots[0].cachedReceiverPair);
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

static void test_member_set_cached_exact_receiver_object_backfills_pair_from_slot_cached_name_without_descriptor_or_symbol(
        void) {
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
    fixture.cacheEntry.picSlots[0].cachedMemberName = fixture.memberName;
    fixture.cacheEntry.picSlots[0].cachedReceiverPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[0].cachedOwnerPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[0].cachedReceiverVersion = fixture.prototype->super.memberVersion;
    fixture.cacheEntry.picSlots[0].cachedOwnerVersion = fixture.prototype->super.memberVersion;
    fixture.cacheEntry.picSlots[0].cachedDescriptorIndex = 0u;
    fixture.prototype->memberDescriptors[0].name = ZR_NULL;
    runtimeFunction->memberEntries = ZR_NULL;
    runtimeFunction->memberEntryLength = 0;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;
    initialMemberVersion = fixture.instance->memberVersion;
    ZrCore_Value_InitAsInt(state, &assignedValue, 105);
    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_NULL(fixture.cacheEntry.picSlots[0].cachedReceiverPair);

    TEST_ASSERT_TRUE(execution_member_set_cached(
            state, ZR_NULL, runtimeFunction, 0, &fixture.receiverValue, &assignedValue));
    TEST_ASSERT_TRUE(execution_member_get_by_name(state, ZR_NULL, &fixture.receiverValue, fixture.memberName, &result));
    TEST_ASSERT_EQUAL_INT64(105, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_PTR(fixture.memberName, fixture.cacheEntry.picSlots[0].cachedMemberName);
    TEST_ASSERT_EQUAL_PTR(fixture.instance, fixture.cacheEntry.picSlots[0].cachedReceiverObject);
    TEST_ASSERT_NOT_NULL(fixture.cacheEntry.picSlots[0].cachedReceiverPair);
    TEST_ASSERT_EQUAL_PTR(fixture.instance->cachedStringLookupPair, fixture.cacheEntry.picSlots[0].cachedReceiverPair);
    TEST_ASSERT_EQUAL_UINT32(initialMemberVersion, fixture.instance->memberVersion);

    ZrCore_Value_InitAsInt(state, &assignedValue, 106);
    ZrCore_Value_ResetAsNull(&result);
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

static void test_member_set_cached_multi_slot_exact_receiver_object_hit_uses_member_name_when_descriptor_name_is_missing(
        void) {
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
    fixture.cacheEntry.picSlotCount = 2;
    memset(&fixture.cacheEntry.picSlots[0], 0, sizeof(fixture.cacheEntry.picSlots[0]));
    fixture.cacheEntry.picSlots[1].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD;
    fixture.cacheEntry.picSlots[1].cachedReceiverPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[1].cachedOwnerPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[1].cachedReceiverObject = fixture.instance;
    fixture.cacheEntry.picSlots[1].cachedReceiverPair = ZR_NULL;
    fixture.cacheEntry.picSlots[1].cachedDescriptorIndex = 0u;
    fixture.cacheEntry.picSlots[1].cachedMemberName = ZR_NULL;
    fixture.cacheEntry.picSlots[1].cachedReceiverVersion = fixture.prototype->super.memberVersion;
    fixture.cacheEntry.picSlots[1].cachedOwnerVersion = fixture.prototype->super.memberVersion;
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
    TEST_ASSERT_EQUAL_PTR(fixture.memberName, fixture.cacheEntry.picSlots[1].cachedMemberName);
    TEST_ASSERT_EQUAL_UINT32(initialMemberVersion, fixture.instance->memberVersion);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);

    detach_runtime_fixture_function(runtimeFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_set_cached_multi_slot_exact_receiver_object_hit_uses_cached_member_name_without_owner_metadata(
        void) {
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
    fixture.cacheEntry.picSlotCount = 2;
    memset(&fixture.cacheEntry.picSlots[0], 0, sizeof(fixture.cacheEntry.picSlots[0]));
    fixture.cacheEntry.picSlots[1].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD;
    fixture.cacheEntry.picSlots[1].cachedReceiverObject = fixture.instance;
    fixture.cacheEntry.picSlots[1].cachedReceiverPair = ZR_NULL;
    fixture.cacheEntry.picSlots[1].cachedMemberName = fixture.memberName;
    fixture.cacheEntry.picSlots[1].cachedReceiverPrototype = ZR_NULL;
    fixture.cacheEntry.picSlots[1].cachedOwnerPrototype = ZR_NULL;
    fixture.cacheEntry.picSlots[1].cachedDescriptorIndex = ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE;
    runtimeFunction->memberEntries = ZR_NULL;
    runtimeFunction->memberEntryLength = 0;
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

static void test_member_set_cached_multi_slot_exact_receiver_object_backfills_cached_member_name_to_sibling_slot(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrFunction *runtimeFunction;
    SZrObjectPrototype *otherPrototype;
    SZrObject *otherInstance;
    SZrTypeValue otherReceiverValue;
    SZrTypeValue assignedValue;
    SZrTypeValue result;
    TZrUInt32 initialMemberVersion;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    init_shared_name_member_access_variant(
            state, fixture.memberName, "HotPathMemberBoxSiblingSet", 205, &otherPrototype, &otherInstance, &otherReceiverValue);
    runtimeFunction = create_runtime_fixture_function(state, &fixture);
    fixture.cacheEntry.kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET;
    fixture.cacheEntry.picSlotCount = 2;
    fixture.cacheEntry.picSlots[0].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD;
    fixture.cacheEntry.picSlots[0].cachedReceiverObject = fixture.instance;
    fixture.cacheEntry.picSlots[0].cachedReceiverPair = ZR_NULL;
    fixture.cacheEntry.picSlots[0].cachedMemberName = ZR_NULL;
    fixture.cacheEntry.picSlots[0].cachedReceiverPrototype = ZR_NULL;
    fixture.cacheEntry.picSlots[0].cachedOwnerPrototype = ZR_NULL;
    fixture.cacheEntry.picSlots[0].cachedDescriptorIndex = ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE;
    fixture.cacheEntry.picSlots[1].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD;
    fixture.cacheEntry.picSlots[1].cachedReceiverPrototype = otherPrototype;
    fixture.cacheEntry.picSlots[1].cachedOwnerPrototype = otherPrototype;
    fixture.cacheEntry.picSlots[1].cachedReceiverObject = otherInstance;
    fixture.cacheEntry.picSlots[1].cachedReceiverPair = ZR_NULL;
    fixture.cacheEntry.picSlots[1].cachedDescriptorIndex = 0u;
    fixture.cacheEntry.picSlots[1].cachedMemberName = ZR_NULL;
    fixture.cacheEntry.picSlots[1].cachedReceiverVersion = otherPrototype->super.memberVersion;
    fixture.cacheEntry.picSlots[1].cachedOwnerVersion = otherPrototype->super.memberVersion;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;
    initialMemberVersion = fixture.instance->memberVersion;
    ZrCore_Value_InitAsInt(state, &assignedValue, 305);
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_member_set_cached(
            state, ZR_NULL, runtimeFunction, 0, &otherReceiverValue, &assignedValue));
    TEST_ASSERT_EQUAL_PTR(fixture.memberName, fixture.cacheEntry.picSlots[0].cachedMemberName);
    TEST_ASSERT_EQUAL_PTR(fixture.memberName, fixture.cacheEntry.picSlots[1].cachedMemberName);

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

static void test_member_set_cached_multi_slot_exact_receiver_object_hit_ignores_earlier_stale_prototype_slot(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrTypeValue assignedValue;
    SZrTypeValue result;
    TZrUInt32 initialMemberVersion;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    fixture.cacheEntry.kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET;
    fixture.cacheEntry.picSlotCount = 2;
    fixture.cacheEntry.picSlots[0].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD;
    fixture.cacheEntry.picSlots[0].cachedReceiverPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[0].cachedOwnerPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[0].cachedReceiverVersion = fixture.prototype->super.memberVersion + 1u;
    fixture.cacheEntry.picSlots[0].cachedOwnerVersion = fixture.prototype->super.memberVersion + 1u;
    fixture.cacheEntry.picSlots[0].cachedDescriptorIndex = 0u;
    fixture.cacheEntry.picSlots[1].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD;
    fixture.cacheEntry.picSlots[1].cachedReceiverPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[1].cachedOwnerPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[1].cachedReceiverObject = fixture.instance;
    fixture.cacheEntry.picSlots[1].cachedReceiverPair = ZR_NULL;
    fixture.cacheEntry.picSlots[1].cachedReceiverVersion = fixture.prototype->super.memberVersion;
    fixture.cacheEntry.picSlots[1].cachedOwnerVersion = fixture.prototype->super.memberVersion;
    fixture.cacheEntry.picSlots[1].cachedDescriptorIndex = 0u;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;
    initialMemberVersion = fixture.instance->memberVersion;
    ZrCore_Value_InitAsInt(state, &assignedValue, 105);
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_member_set_cached(
            state, ZR_NULL, &fixture.function, 0, &fixture.receiverValue, &assignedValue));
    TEST_ASSERT_TRUE(execution_member_get_by_name(state, ZR_NULL, &fixture.receiverValue, fixture.memberName, &result));
    TEST_ASSERT_EQUAL_INT64(105, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(initialMemberVersion, fixture.instance->memberVersion);
    TEST_ASSERT_EQUAL_UINT32(2u, fixture.cacheEntry.picSlotCount);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_set_cached_multi_slot_exact_receiver_descriptor_hit_ignores_earlier_stale_prototype_slot(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrTypeValue assignedValue;
    SZrTypeValue result;
    TZrUInt32 initialMemberVersion;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    fixture.cacheEntry.kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET;
    fixture.cacheEntry.picSlotCount = 2;
    fixture.cacheEntry.picSlots[0].cachedReceiverPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[0].cachedOwnerPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[0].cachedReceiverVersion = fixture.prototype->super.memberVersion + 1u;
    fixture.cacheEntry.picSlots[0].cachedOwnerVersion = fixture.prototype->super.memberVersion + 1u;
    fixture.cacheEntry.picSlots[0].cachedDescriptorIndex = 0u;
    fixture.cacheEntry.picSlots[1].cachedReceiverPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[1].cachedOwnerPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[1].cachedReceiverObject = fixture.instance;
    fixture.cacheEntry.picSlots[1].cachedReceiverPair = ZR_NULL;
    fixture.cacheEntry.picSlots[1].cachedReceiverVersion = fixture.prototype->super.memberVersion;
    fixture.cacheEntry.picSlots[1].cachedOwnerVersion = fixture.prototype->super.memberVersion;
    fixture.cacheEntry.picSlots[1].cachedDescriptorIndex = 0u;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;
    initialMemberVersion = fixture.instance->memberVersion;
    ZrCore_Value_InitAsInt(state, &assignedValue, 105);
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_member_set_cached(
            state, ZR_NULL, &fixture.function, 0, &fixture.receiverValue, &assignedValue));
    TEST_ASSERT_TRUE(execution_member_get_by_name(state, ZR_NULL, &fixture.receiverValue, fixture.memberName, &result));
    TEST_ASSERT_EQUAL_INT64(105, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(initialMemberVersion + 1u, fixture.instance->memberVersion);
    TEST_ASSERT_EQUAL_UINT32(2u, fixture.cacheEntry.picSlotCount);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_set_cached_multi_slot_exact_receiver_object_version_mismatch_clears_slot_and_falls_back(void) {
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
    fixture.cacheEntry.picSlotCount = 2;
    memset(&fixture.cacheEntry.picSlots[0], 0, sizeof(fixture.cacheEntry.picSlots[0]));
    fixture.cacheEntry.picSlots[1].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD;
    fixture.cacheEntry.picSlots[1].cachedReceiverPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[1].cachedOwnerPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[1].cachedReceiverObject = fixture.instance;
    fixture.cacheEntry.picSlots[1].cachedReceiverPair = ZR_NULL;
    fixture.cacheEntry.picSlots[1].cachedDescriptorIndex = 0u;
    fixture.cacheEntry.picSlots[1].cachedMemberName = ZR_NULL;
    fixture.cacheEntry.picSlots[1].cachedReceiverVersion = fixture.prototype->super.memberVersion;
    fixture.cacheEntry.picSlots[1].cachedOwnerVersion = fixture.prototype->super.memberVersion;
    fixture.prototype->memberDescriptors[0].name = ZR_NULL;
    fixture.prototype->super.memberVersion++;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;
    initialMemberVersion = fixture.instance->memberVersion;
    ZrCore_Value_InitAsInt(state, &assignedValue, 105);
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_member_set_cached(
            state, ZR_NULL, runtimeFunction, 0, &fixture.receiverValue, &assignedValue));
    TEST_ASSERT_TRUE(execution_member_get_by_name(state, ZR_NULL, &fixture.receiverValue, fixture.memberName, &result));
    TEST_ASSERT_EQUAL_INT64(105, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(initialMemberVersion + 1u, fixture.instance->memberVersion);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.picSlotCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeMissCount);

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
    fixture.cacheEntry.picSlots[0].cachedReceiverObject = fixture.instance;
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
    fixture.cacheEntry.picSlots[0].cachedReceiverObject = fixture.instance;
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

static void test_member_set_cached_instance_field_pair_non_hidden_slow_lane_does_not_touch_hidden_items_literal_cache(
        void) {
    SZrSelectiveAllocatorContext allocatorContext = {0};
    SZrState *state = create_runtime_state_with_selective_allocator(&allocatorContext);
    SZrMemberAccessFixture fixture;
    SZrHashKeyValuePair *pair;
    SZrObject *hiddenItemsObject;
    SZrObject *assignedObject;
    SZrTypeValue assignedValue;
    TZrUInt32 initialMemberVersion;
    TZrSize stringCountBefore;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    fixture.cacheEntry.kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET;
    fixture.cacheEntry.picSlots[0].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD;
    fixture.cacheEntry.picSlots[0].cachedReceiverObject = fixture.instance;
    fixture.cacheEntry.picSlots[0].cachedReceiverPair = fixture.instance->cachedStringLookupPair;
    fixture.cacheEntry.picSlots[0].cachedDescriptorIndex = ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE;
    fixture.cacheEntry.picSlots[0].cachedMemberName = ZR_NULL;
    fixture.cacheEntry.picSlots[0].cachedIsStatic =
            ZR_FUNCTION_CALLSITE_PIC_SLOT_FLAG_NON_HIDDEN_STRING_PAIR_FAST_SET;
    fixture.function.memberEntries = ZR_NULL;
    fixture.function.memberEntryLength = 0;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;

    pair = fixture.instance->cachedStringLookupPair;
    TEST_ASSERT_NOT_NULL(pair);

    hiddenItemsObject = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
    TEST_ASSERT_NOT_NULL(hiddenItemsObject);
    ZrCore_Object_Init(state, hiddenItemsObject);
    fixture.instance->cachedHiddenItemsPair = ZR_NULL;
    fixture.instance->cachedHiddenItemsObject = hiddenItemsObject;

    assignedObject = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(assignedObject);
    ZrCore_Object_Init(state, assignedObject);
    TEST_ASSERT_TRUE(ZrCore_Ownership_InitUniqueValue(
            state, &assignedValue, ZR_CAST_RAW_OBJECT_AS_SUPER(assignedObject)));

    allocatorContext.failAllocateSize = sizeof(SZrString) + ZR_VM_SHORT_STRING_MAX;
    allocatorContext.failAllocateActive = ZR_TRUE;
    initialMemberVersion = fixture.instance->memberVersion;
    stringCountBefore = state->global->stringTable->stringHashSet.elementCount;

    TEST_ASSERT_TRUE(execution_member_set_cached(
            state, ZR_NULL, &fixture.function, 0, &fixture.receiverValue, &assignedValue));

    TEST_ASSERT_EQUAL_UINT32(0u, allocatorContext.failAllocateCount);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)stringCountBefore,
                             (TZrUInt32)state->global->stringTable->stringHashSet.elementCount);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, pair->value.type);
    TEST_ASSERT_EQUAL_PTR(ZR_CAST_RAW_OBJECT_AS_SUPER(assignedObject), pair->value.value.object);
    TEST_ASSERT_EQUAL_PTR(hiddenItemsObject, fixture.instance->cachedHiddenItemsObject);
    TEST_ASSERT_NULL(fixture.instance->cachedHiddenItemsPair);
    TEST_ASSERT_EQUAL_UINT32(initialMemberVersion, fixture.instance->memberVersion);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);

    ZrCore_GlobalState_Free(state->global);
}

static void test_member_set_cached_exact_receiver_object_non_hidden_slow_lane_does_not_touch_hidden_items_literal_cache(
        void) {
    SZrSelectiveAllocatorContext allocatorContext = {0};
    SZrState *state = create_runtime_state_with_selective_allocator(&allocatorContext);
    SZrMemberAccessFixture fixture;
    SZrHashKeyValuePair *pair;
    SZrObject *hiddenItemsObject;
    SZrObject *assignedObject;
    SZrTypeValue assignedValue;
    TZrUInt32 initialMemberVersion;
    TZrSize stringCountBefore;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    fixture.cacheEntry.kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET;
    fixture.cacheEntry.picSlots[0].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD;
    fixture.cacheEntry.picSlots[0].cachedReceiverObject = fixture.instance;
    fixture.cacheEntry.picSlots[0].cachedReceiverPair = ZR_NULL;
    fixture.cacheEntry.picSlots[0].cachedDescriptorIndex = ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE;
    fixture.cacheEntry.picSlots[0].cachedMemberName = fixture.memberName;
    fixture.cacheEntry.picSlots[0].cachedReceiverPrototype = ZR_NULL;
    fixture.cacheEntry.picSlots[0].cachedOwnerPrototype = ZR_NULL;
    fixture.cacheEntry.picSlots[0].cachedIsStatic =
            ZR_FUNCTION_CALLSITE_PIC_SLOT_FLAG_NON_HIDDEN_STRING_PAIR_FAST_SET;
    fixture.function.memberEntries = ZR_NULL;
    fixture.function.memberEntryLength = 0;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;

    pair = fixture.instance->cachedStringLookupPair;
    TEST_ASSERT_NOT_NULL(pair);

    hiddenItemsObject = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
    TEST_ASSERT_NOT_NULL(hiddenItemsObject);
    ZrCore_Object_Init(state, hiddenItemsObject);
    fixture.instance->cachedHiddenItemsPair = ZR_NULL;
    fixture.instance->cachedHiddenItemsObject = hiddenItemsObject;

    assignedObject = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(assignedObject);
    ZrCore_Object_Init(state, assignedObject);
    TEST_ASSERT_TRUE(ZrCore_Ownership_InitUniqueValue(
            state, &assignedValue, ZR_CAST_RAW_OBJECT_AS_SUPER(assignedObject)));

    allocatorContext.failAllocateSize = sizeof(SZrString) + ZR_VM_SHORT_STRING_MAX;
    allocatorContext.failAllocateActive = ZR_TRUE;
    initialMemberVersion = fixture.instance->memberVersion;
    stringCountBefore = state->global->stringTable->stringHashSet.elementCount;

    TEST_ASSERT_TRUE(execution_member_set_cached(
            state, ZR_NULL, &fixture.function, 0, &fixture.receiverValue, &assignedValue));

    TEST_ASSERT_EQUAL_UINT32(0u, allocatorContext.failAllocateCount);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)stringCountBefore,
                             (TZrUInt32)state->global->stringTable->stringHashSet.elementCount);
    TEST_ASSERT_NOT_NULL(fixture.cacheEntry.picSlots[0].cachedReceiverPair);
    TEST_ASSERT_EQUAL_PTR(fixture.instance->cachedStringLookupPair, fixture.cacheEntry.picSlots[0].cachedReceiverPair);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, pair->value.type);
    TEST_ASSERT_EQUAL_PTR(ZR_CAST_RAW_OBJECT_AS_SUPER(assignedObject), pair->value.value.object);
    TEST_ASSERT_EQUAL_PTR(hiddenItemsObject, fixture.instance->cachedHiddenItemsObject);
    TEST_ASSERT_NULL(fixture.instance->cachedHiddenItemsPair);
    TEST_ASSERT_EQUAL_UINT32(initialMemberVersion, fixture.instance->memberVersion);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);

    ZrCore_GlobalState_Free(state->global);
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

static void test_member_get_cached_multi_slot_exact_receiver_pair_hit_ignores_cached_version_mismatch(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    fixture.cacheEntry.picSlotCount = 2;
    memset(&fixture.cacheEntry.picSlots[0], 0, sizeof(fixture.cacheEntry.picSlots[0]));
    fixture.cacheEntry.picSlots[1].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD;
    fixture.cacheEntry.picSlots[1].cachedReceiverPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[1].cachedOwnerPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[1].cachedReceiverObject = fixture.instance;
    fixture.cacheEntry.picSlots[1].cachedReceiverPair = fixture.instance->cachedStringLookupPair;
    fixture.cacheEntry.picSlots[1].cachedDescriptorIndex = 0u;
    fixture.cacheEntry.picSlots[1].cachedReceiverVersion = fixture.prototype->super.memberVersion + 1u;
    fixture.cacheEntry.picSlots[1].cachedOwnerVersion = fixture.prototype->super.memberVersion + 1u;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, &fixture.function, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(73, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(2u, fixture.cacheEntry.picSlotCount);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_set_cached_multi_slot_exact_receiver_pair_hit_ignores_cached_version_mismatch(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrTypeValue assignedValue;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    fixture.cacheEntry.kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET;
    fixture.cacheEntry.picSlotCount = 2;
    memset(&fixture.cacheEntry.picSlots[0], 0, sizeof(fixture.cacheEntry.picSlots[0]));
    fixture.cacheEntry.picSlots[1].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD;
    fixture.cacheEntry.picSlots[1].cachedReceiverPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[1].cachedOwnerPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[1].cachedReceiverObject = fixture.instance;
    fixture.cacheEntry.picSlots[1].cachedReceiverPair = fixture.instance->cachedStringLookupPair;
    fixture.cacheEntry.picSlots[1].cachedDescriptorIndex = 0u;
    fixture.cacheEntry.picSlots[1].cachedReceiverVersion = fixture.prototype->super.memberVersion + 1u;
    fixture.cacheEntry.picSlots[1].cachedOwnerVersion = fixture.prototype->super.memberVersion + 1u;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;
    ZrCore_Value_InitAsInt(state, &assignedValue, 105);
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_member_set_cached(
            state, ZR_NULL, &fixture.function, 0, &fixture.receiverValue, &assignedValue));
    TEST_ASSERT_TRUE(execution_member_get_by_name(state, ZR_NULL, &fixture.receiverValue, fixture.memberName, &result));
    TEST_ASSERT_EQUAL_INT64(105, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(2u, fixture.cacheEntry.picSlotCount);
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
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_SET_MEMBER]);
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY]);

    clear_profile_counters(state);
    TEST_ASSERT_TRUE(execution_member_get_by_name(state, ZR_NULL, &fixture.receiverValue, fixture.memberName, &result));
    TEST_ASSERT_EQUAL_INT64(105, result.value.nativeObject.nativeInt64);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_dispatch_exact_receiver_pair_get_hot_fast_hits_and_records_helpers(void) {
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

    TEST_ASSERT_TRUE(execution_member_try_dispatch_exact_receiver_pair_get_hot_fast(
            state, &fixture.function, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(73, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_GET_MEMBER]);
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY]);

    clear_profile_counters(state);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_dispatch_exact_receiver_pair_get_hot_fast_ignores_cached_version_mismatch(void) {
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

    TEST_ASSERT_TRUE(execution_member_try_dispatch_exact_receiver_pair_get_hot_fast(
            state, &fixture.function, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(73, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_dispatch_exact_receiver_pair_get_checked_object_hot_fast_hits_and_records_helpers(void) {
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

    TEST_ASSERT_TRUE(execution_member_try_dispatch_exact_receiver_pair_get_hot_fast_checked_object(
            state, &fixture.function, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(73, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_GET_MEMBER]);
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY]);

    clear_profile_counters(state);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_dispatch_exact_receiver_pair_set_hot_fast_hits_and_records_helpers(void) {
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

    TEST_ASSERT_TRUE(execution_member_try_dispatch_exact_receiver_pair_set_hot_fast(
            state, &fixture.function, 0, &fixture.receiverValue, &assignedValue));
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_SET_MEMBER]);
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY]);

    clear_profile_counters(state);
    TEST_ASSERT_TRUE(execution_member_get_by_name(state, ZR_NULL, &fixture.receiverValue, fixture.memberName, &result));
    TEST_ASSERT_EQUAL_INT64(105, result.value.nativeObject.nativeInt64);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_dispatch_exact_receiver_pair_set_hot_fast_non_hidden_slow_lane_updates_value_with_hidden_items_cached_state(
        void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrObject *hiddenItemsObject;
    SZrObject *assignedObject;
    SZrTypeValue assignedValue;
    SZrTypeValue result;
    TZrUInt32 initialMemberVersion;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    fixture.cacheEntry.kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET;
    fixture.cacheEntry.picSlots[0].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD;
    fixture.cacheEntry.picSlots[0].cachedReceiverObject = fixture.instance;
    fixture.cacheEntry.picSlots[0].cachedReceiverPair = fixture.instance->cachedStringLookupPair;
    fixture.cacheEntry.picSlots[0].cachedIsStatic =
            ZR_FUNCTION_CALLSITE_PIC_SLOT_FLAG_NON_HIDDEN_STRING_PAIR_FAST_SET;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;
    hiddenItemsObject = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
    TEST_ASSERT_NOT_NULL(hiddenItemsObject);
    ZrCore_Object_Init(state, hiddenItemsObject);
    fixture.instance->cachedHiddenItemsPair = ZR_NULL;
    fixture.instance->cachedHiddenItemsObject = hiddenItemsObject;
    assignedObject = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(assignedObject);
    ZrCore_Object_Init(state, assignedObject);
    ZrCore_Value_ResetAsNull(&assignedValue);
    TEST_ASSERT_TRUE(
            ZrCore_Ownership_InitUniqueValue(state, &assignedValue, ZR_CAST_RAW_OBJECT_AS_SUPER(assignedObject)));
    ZrCore_Value_ResetAsNull(&result);
    initialMemberVersion = fixture.instance->memberVersion;

    TEST_ASSERT_TRUE(execution_member_try_dispatch_exact_receiver_pair_set_hot_fast(
            state, &fixture.function, 0, &fixture.receiverValue, &assignedValue));
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, fixture.instance->cachedStringLookupPair->value.type);
    TEST_ASSERT_EQUAL_PTR(
            ZR_CAST_RAW_OBJECT_AS_SUPER(assignedObject), fixture.instance->cachedStringLookupPair->value.value.object);
    TEST_ASSERT_EQUAL_PTR(hiddenItemsObject, fixture.instance->cachedHiddenItemsObject);
    TEST_ASSERT_NULL(fixture.instance->cachedHiddenItemsPair);
    TEST_ASSERT_EQUAL_UINT32(initialMemberVersion, fixture.instance->memberVersion);

    TEST_ASSERT_TRUE(execution_member_get_by_name(state, ZR_NULL, &fixture.receiverValue, fixture.memberName, &result));
    TEST_ASSERT_EQUAL_PTR(ZR_CAST_RAW_OBJECT_AS_SUPER(assignedObject), result.value.object);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_dispatch_exact_receiver_pair_set_checked_object_hot_fast_hits_and_records_helpers(void) {
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

    TEST_ASSERT_TRUE(execution_member_try_dispatch_exact_receiver_pair_set_hot_fast_checked_object(
            state, &fixture.function, 0, &fixture.receiverValue, &assignedValue));
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_SET_MEMBER]);
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY]);

    clear_profile_counters(state);
    TEST_ASSERT_TRUE(execution_member_get_by_name(state, ZR_NULL, &fixture.receiverValue, fixture.memberName, &result));
    TEST_ASSERT_EQUAL_INT64(105, result.value.nativeObject.nativeInt64);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_set_cached_refresh_replaces_oldest_pic_slot_when_capacity_is_full(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrFunction *runtimeFunction;
    SZrObjectPrototype *prototypeB;
    SZrObjectPrototype *prototypeC;
    SZrObject *instanceB;
    SZrObject *instanceC;
    SZrTypeValue receiverValueB;
    SZrTypeValue receiverValueC;
    SZrTypeValue assignedValue;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 11);
    init_shared_name_member_access_variant(
            state, fixture.memberName, "HotPathMemberSetBoxB", 22, &prototypeB, &instanceB, &receiverValueB);
    init_shared_name_member_access_variant(
            state, fixture.memberName, "HotPathMemberSetBoxC", 33, &prototypeC, &instanceC, &receiverValueC);
    runtimeFunction = create_runtime_fixture_function(state, &fixture);
    fixture.cacheEntry.kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET;
    fixture.cacheEntry.picSlotCount = 0;
    fixture.cacheEntry.picNextInsertIndex = 0;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;
    memset(fixture.cacheEntry.picSlots, 0, sizeof(fixture.cacheEntry.picSlots));
    ZrCore_Value_ResetAsNull(&result);

    ZrCore_Value_InitAsInt(state, &assignedValue, 101);
    TEST_ASSERT_TRUE(execution_member_set_cached(
            state, ZR_NULL, runtimeFunction, 0, &fixture.receiverValue, &assignedValue));
    TEST_ASSERT_TRUE(execution_member_get_by_name(state, ZR_NULL, &fixture.receiverValue, fixture.memberName, &result));
    TEST_ASSERT_EQUAL_INT64(101, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.picSlotCount);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.picNextInsertIndex);
    TEST_ASSERT_EQUAL_PTR(fixture.prototype, fixture.cacheEntry.picSlots[0].cachedReceiverPrototype);
    TEST_ASSERT_EQUAL_PTR(fixture.instance, fixture.cacheEntry.picSlots[0].cachedReceiverObject);
    TEST_ASSERT_EQUAL_PTR(fixture.instance->cachedStringLookupPair, fixture.cacheEntry.picSlots[0].cachedReceiverPair);

    ZrCore_Value_InitAsInt(state, &assignedValue, 202);
    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE(execution_member_set_cached(state, ZR_NULL, runtimeFunction, 0, &receiverValueB, &assignedValue));
    TEST_ASSERT_TRUE(execution_member_get_by_name(state, ZR_NULL, &receiverValueB, fixture.memberName, &result));
    TEST_ASSERT_EQUAL_INT64(202, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY, fixture.cacheEntry.picSlotCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.picNextInsertIndex);
    TEST_ASSERT_EQUAL_PTR(fixture.prototype, fixture.cacheEntry.picSlots[0].cachedReceiverPrototype);
    TEST_ASSERT_EQUAL_PTR(prototypeB, fixture.cacheEntry.picSlots[1].cachedReceiverPrototype);
    TEST_ASSERT_EQUAL_PTR(instanceB->cachedStringLookupPair, fixture.cacheEntry.picSlots[1].cachedReceiverPair);

    ZrCore_Value_InitAsInt(state, &assignedValue, 303);
    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE(execution_member_set_cached(state, ZR_NULL, runtimeFunction, 0, &receiverValueC, &assignedValue));
    TEST_ASSERT_TRUE(execution_member_get_by_name(state, ZR_NULL, &receiverValueC, fixture.memberName, &result));
    TEST_ASSERT_EQUAL_INT64(303, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY, fixture.cacheEntry.picSlotCount);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.picNextInsertIndex);
    TEST_ASSERT_EQUAL_PTR(prototypeC, fixture.cacheEntry.picSlots[0].cachedReceiverPrototype);
    TEST_ASSERT_EQUAL_PTR(instanceC, fixture.cacheEntry.picSlots[0].cachedReceiverObject);
    TEST_ASSERT_EQUAL_PTR(prototypeB, fixture.cacheEntry.picSlots[1].cachedReceiverPrototype);
    TEST_ASSERT_EQUAL_PTR(instanceB, fixture.cacheEntry.picSlots[1].cachedReceiverObject);
    TEST_ASSERT_EQUAL_UINT32(3u, fixture.cacheEntry.runtimeMissCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeHitCount);

    ZrCore_Value_InitAsInt(state, &assignedValue, 204);
    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE(execution_member_set_cached(state, ZR_NULL, runtimeFunction, 0, &receiverValueB, &assignedValue));
    TEST_ASSERT_TRUE(execution_member_get_by_name(state, ZR_NULL, &receiverValueB, fixture.memberName, &result));
    TEST_ASSERT_EQUAL_INT64(204, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(3u, fixture.cacheEntry.runtimeMissCount);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.picNextInsertIndex);
    TEST_ASSERT_EQUAL_PTR(prototypeC, fixture.cacheEntry.picSlots[0].cachedReceiverPrototype);
    TEST_ASSERT_EQUAL_PTR(prototypeB, fixture.cacheEntry.picSlots[1].cachedReceiverPrototype);

    ZrCore_Value_InitAsInt(state, &assignedValue, 105);
    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE(execution_member_set_cached(
            state, ZR_NULL, runtimeFunction, 0, &fixture.receiverValue, &assignedValue));
    TEST_ASSERT_TRUE(execution_member_get_by_name(state, ZR_NULL, &fixture.receiverValue, fixture.memberName, &result));
    TEST_ASSERT_EQUAL_INT64(105, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(4u, fixture.cacheEntry.runtimeMissCount);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.picNextInsertIndex);
    TEST_ASSERT_EQUAL_PTR(prototypeC, fixture.cacheEntry.picSlots[0].cachedReceiverPrototype);
    TEST_ASSERT_EQUAL_PTR(instanceC, fixture.cacheEntry.picSlots[0].cachedReceiverObject);
    TEST_ASSERT_EQUAL_PTR(fixture.prototype, fixture.cacheEntry.picSlots[1].cachedReceiverPrototype);
    TEST_ASSERT_EQUAL_PTR(fixture.instance, fixture.cacheEntry.picSlots[1].cachedReceiverObject);
    TEST_ASSERT_EQUAL_PTR(fixture.instance->cachedStringLookupPair, fixture.cacheEntry.picSlots[1].cachedReceiverPair);

    detach_runtime_fixture_function(runtimeFunction);
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

static void test_object_set_value_existing_pair_updates_value_and_bumps_member_version(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrHashKeyValuePair *pair;
    SZrTypeValue assignedValue;
    TZrUInt32 initialMemberVersion;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    pair = fixture.instance->cachedStringLookupPair;
    TEST_ASSERT_NOT_NULL(pair);
    initialMemberVersion = fixture.instance->memberVersion;
    ZrCore_Value_InitAsInt(state, &assignedValue, 105);

    ZrCore_Object_SetValue(state, fixture.instance, &pair->key, &assignedValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, pair->value.type);
    TEST_ASSERT_EQUAL_INT64(105, pair->value.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_PTR(pair, fixture.instance->cachedStringLookupPair);
    TEST_ASSERT_EQUAL_UINT32(initialMemberVersion + 1u, fixture.instance->memberVersion);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_execution_member_set_by_name_stack_operands_reuse_vm_stack_roots_without_ignore_registry_growth(void) {
    SZrSelectiveAllocatorContext allocatorContext = {0};
    SZrState *state = create_runtime_state_with_selective_allocator(&allocatorContext);
    SZrMemberAccessFixture fixture;
    SZrObject *assignedObject;
    SZrTypeValue *stackReceiver;
    SZrTypeValue *stackAssignedValue;
    SZrHashKeyValuePair *pair;
    TZrUInt32 initialMemberVersion;
    SZrGarbageCollector *collector;
    TZrSize ignoredObjectCountBefore;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);
    TEST_ASSERT_NOT_NULL(state->global->garbageCollector);

    init_member_access_fixture(state, &fixture, 73);
    pair = fixture.instance->cachedStringLookupPair;
    TEST_ASSERT_NOT_NULL(pair);

    stackReceiver = ZrCore_Stack_GetValue(state->stackBase.valuePointer + 1);
    TEST_ASSERT_NOT_NULL(stackReceiver);
    *stackReceiver = fixture.receiverValue;
    if (ZrCore_GarbageCollector_IsObjectIgnored(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(fixture.instance))) {
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_UnignoreObject(state->global,
                                                                ZR_CAST_RAW_OBJECT_AS_SUPER(fixture.instance)));
    }
    TEST_ASSERT_FALSE(ZrCore_GarbageCollector_IsObjectIgnored(state->global,
                                                              ZR_CAST_RAW_OBJECT_AS_SUPER(fixture.instance)));

    assignedObject = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(assignedObject);
    ZrCore_Object_Init(state, assignedObject);
    stackAssignedValue = ZrCore_Stack_GetValue(state->stackBase.valuePointer + 2);
    TEST_ASSERT_NOT_NULL(stackAssignedValue);
    ZrCore_Value_InitAsRawObject(state, stackAssignedValue, ZR_CAST_RAW_OBJECT_AS_SUPER(assignedObject));
    stackAssignedValue->type = ZR_VALUE_TYPE_OBJECT;
    if (ZrCore_GarbageCollector_IsObjectIgnored(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(assignedObject))) {
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_UnignoreObject(state->global,
                                                                ZR_CAST_RAW_OBJECT_AS_SUPER(assignedObject)));
    }
    TEST_ASSERT_FALSE(ZrCore_GarbageCollector_IsObjectIgnored(state->global,
                                                              ZR_CAST_RAW_OBJECT_AS_SUPER(assignedObject)));
    fill_ignore_registry_to_capacity(state);
    collector = state->global->garbageCollector;
    ignoredObjectCountBefore = collector->ignoredObjectCount;
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)collector->ignoredObjectCapacity, (TZrUInt32)ignoredObjectCountBefore);
    allocatorContext.failAllocateSize = ignore_registry_growth_allocation_size(collector);
    allocatorContext.failAllocateActive = ZR_TRUE;
    initialMemberVersion = fixture.instance->memberVersion;

    TEST_ASSERT_TRUE(execution_member_set_by_name(
            state, ZR_NULL, stackReceiver, fixture.memberName, stackAssignedValue));

    TEST_ASSERT_EQUAL_UINT32(0u, allocatorContext.failAllocateCount);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, pair->value.type);
    TEST_ASSERT_EQUAL_PTR(ZR_CAST_RAW_OBJECT_AS_SUPER(assignedObject), pair->value.value.object);
    TEST_ASSERT_EQUAL_UINT32(initialMemberVersion + 1u, fixture.instance->memberVersion);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)ignoredObjectCountBefore, (TZrUInt32)collector->ignoredObjectCount);

    ZrCore_GlobalState_Free(state->global);
}

static void test_object_direct_storage_key_fast_path_reuses_live_short_string_key(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *memberName;
    SZrTypeValue key;
    SZrTypeValue normalizedKey;
    const SZrTypeValue *storageKey;

    TEST_ASSERT_NOT_NULL(state);

    memberName = ZrCore_String_CreateFromNative(state, "value");
    TEST_ASSERT_NOT_NULL(memberName);
    TEST_ASSERT_TRUE(ZrCore_String_IsShort(memberName));
    TEST_ASSERT_FALSE(ZrCore_RawObject_IsReleased(ZR_CAST_RAW_OBJECT_AS_SUPER(memberName)));

    init_string_key(state, memberName, &key);
    memset(&normalizedKey, 0, sizeof(normalizedKey));

    storageKey = object_try_get_direct_storage_key_unchecked(&key);

    TEST_ASSERT_EQUAL_PTR(&key, storageKey);
    TEST_ASSERT_EQUAL_INT(0, normalizedKey.type);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_execution_member_get_cached_descriptor_stack_receiver_hits_from_vm_stack_roots(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrTypeValue *stackReceiver;
    SZrTypeValue *stackResult;

    TEST_ASSERT_NOT_NULL(state);

    init_member_access_fixture(state, &fixture, 73);
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;
    stackReceiver = ZrCore_Stack_GetValue(state->stackBase.valuePointer + 1);
    stackResult = ZrCore_Stack_GetValue(state->stackBase.valuePointer + 2);
    TEST_ASSERT_NOT_NULL(stackReceiver);
    TEST_ASSERT_NOT_NULL(stackResult);
    *stackReceiver = fixture.receiverValue;
    ZrCore_Value_ResetAsNull(stackResult);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, &fixture.function, 0, stackReceiver, stackResult));

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, stackResult->type);
    TEST_ASSERT_EQUAL_INT64(73, stackResult->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_execution_member_set_cached_descriptor_stack_operands_reuse_vm_stack_roots_without_ignore_registry_growth(
        void) {
    SZrSelectiveAllocatorContext allocatorContext = {0};
    SZrState *state = create_runtime_state_with_selective_allocator(&allocatorContext);
    SZrMemberAccessFixture fixture;
    SZrTypeValue *stackReceiver;
    SZrObject *assignedObject;
    SZrTypeValue *stackValue;
    SZrHashKeyValuePair *pair;
    SZrGarbageCollector *collector;
    TZrSize ignoredObjectCountBefore;
    TZrUInt32 initialMemberVersion;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);
    TEST_ASSERT_NOT_NULL(state->global->garbageCollector);

    init_member_access_fixture(state, &fixture, 73);
    fixture.cacheEntry.kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET;
    fixture.cacheEntry.picSlotCount = 1;
    fixture.cacheEntry.picSlots[0].cachedReceiverObject = ZR_NULL;
    fixture.cacheEntry.picSlots[0].cachedReceiverPair = ZR_NULL;
    fixture.cacheEntry.picSlots[0].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_NONE;
    fixture.cacheEntry.picSlots[0].cachedReceiverPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[0].cachedOwnerPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[0].cachedReceiverVersion = fixture.prototype->super.memberVersion;
    fixture.cacheEntry.picSlots[0].cachedOwnerVersion = fixture.prototype->super.memberVersion;
    fixture.cacheEntry.picSlots[0].cachedDescriptorIndex = 0u;

    stackReceiver = ZrCore_Stack_GetValue(state->stackBase.valuePointer + 1);
    TEST_ASSERT_NOT_NULL(stackReceiver);
    *stackReceiver = fixture.receiverValue;
    if (ZrCore_GarbageCollector_IsObjectIgnored(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(fixture.instance))) {
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_UnignoreObject(state->global,
                                                                ZR_CAST_RAW_OBJECT_AS_SUPER(fixture.instance)));
    }
    TEST_ASSERT_FALSE(ZrCore_GarbageCollector_IsObjectIgnored(state->global,
                                                              ZR_CAST_RAW_OBJECT_AS_SUPER(fixture.instance)));

    assignedObject = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(assignedObject);
    ZrCore_Object_Init(state, assignedObject);
    stackValue = ZrCore_Stack_GetValue(state->stackBase.valuePointer + 2);
    TEST_ASSERT_NOT_NULL(stackValue);
    ZrCore_Value_InitAsRawObject(state, stackValue, ZR_CAST_RAW_OBJECT_AS_SUPER(assignedObject));
    stackValue->type = ZR_VALUE_TYPE_OBJECT;
    if (ZrCore_GarbageCollector_IsObjectIgnored(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(assignedObject))) {
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_UnignoreObject(state->global,
                                                                ZR_CAST_RAW_OBJECT_AS_SUPER(assignedObject)));
    }
    TEST_ASSERT_FALSE(ZrCore_GarbageCollector_IsObjectIgnored(state->global,
                                                              ZR_CAST_RAW_OBJECT_AS_SUPER(assignedObject)));
    fill_ignore_registry_to_capacity(state);
    collector = state->global->garbageCollector;
    ignoredObjectCountBefore = collector->ignoredObjectCount;
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)collector->ignoredObjectCapacity, (TZrUInt32)ignoredObjectCountBefore);
    allocatorContext.failAllocateSize = ignore_registry_growth_allocation_size(collector);
    allocatorContext.failAllocateActive = ZR_TRUE;
    initialMemberVersion = fixture.instance->memberVersion;

    TEST_ASSERT_TRUE(execution_member_set_cached(
            state, ZR_NULL, &fixture.function, 0, stackReceiver, stackValue));

    pair = fixture.instance->cachedStringLookupPair;
    TEST_ASSERT_EQUAL_UINT32(0u, allocatorContext.failAllocateCount);
    TEST_ASSERT_NOT_NULL(pair);
    TEST_ASSERT_EQUAL_PTR(fixture.memberName, ZR_CAST_STRING(state, pair->key.value.object));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, pair->value.type);
    TEST_ASSERT_EQUAL_PTR(ZR_CAST_RAW_OBJECT_AS_SUPER(assignedObject), pair->value.value.object);
    TEST_ASSERT_EQUAL_UINT32(initialMemberVersion + 1u, fixture.instance->memberVersion);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)ignoredObjectCountBefore, (TZrUInt32)collector->ignoredObjectCount);

    ZrCore_GlobalState_Free(state->global);
}

static void test_execution_member_set_cached_descriptor_missing_pair_slow_lane_inserts_storage(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrObject *freshInstance;
    SZrTypeValue assignedValue;
    SZrTypeValue memberKey;
    const SZrTypeValue *storedValue;

    TEST_ASSERT_NOT_NULL(state);

    init_member_access_fixture(state, &fixture, 73);
    freshInstance = ZrCore_Object_New(state, fixture.prototype);
    TEST_ASSERT_NOT_NULL(freshInstance);
    ZrCore_Object_Init(state, freshInstance);
    fixture.instance = freshInstance;
    ZrCore_Value_InitAsRawObject(state, &fixture.receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(freshInstance));
    fixture.receiverValue.type = ZR_VALUE_TYPE_OBJECT;
    fixture.cacheEntry.kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;

    init_string_key(state, fixture.memberName, &memberKey);
    TEST_ASSERT_NULL(ZrCore_Object_GetValue(state, fixture.instance, &memberKey));

    ZrCore_Value_InitAsInt(state, &assignedValue, 105);

    TEST_ASSERT_TRUE(execution_member_set_cached(
            state, ZR_NULL, &fixture.function, 0, &fixture.receiverValue, &assignedValue));

    storedValue = ZrCore_Object_GetValue(state, fixture.instance, &memberKey);
    TEST_ASSERT_NOT_NULL(storedValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, storedValue->type);
    TEST_ASSERT_EQUAL_INT64(105, storedValue->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.instance->memberVersion);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_execution_member_set_by_name_existing_pair_slow_lane_reuses_existing_storage_without_ignore_registry_growth(
        void) {
    SZrSelectiveAllocatorContext allocatorContext = {0};
    SZrState *state = create_runtime_state_with_selective_allocator(&allocatorContext);
    SZrMemberAccessFixture fixture;
    SZrObject *assignedObject;
    SZrTypeValue stableReceiver;
    SZrTypeValue stableAssignedValue;
    SZrHashKeyValuePair *pair;
    TZrUInt32 initialMemberVersion;
    SZrGarbageCollector *collector;
    TZrSize ignoredObjectCountBefore;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);
    TEST_ASSERT_NOT_NULL(state->global->garbageCollector);

    init_member_access_fixture(state, &fixture, 73);
    pair = fixture.instance->cachedStringLookupPair;
    TEST_ASSERT_NOT_NULL(pair);

    stableReceiver = fixture.receiverValue;
    if (ZrCore_GarbageCollector_IsObjectIgnored(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(fixture.instance))) {
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_UnignoreObject(state->global,
                                                                ZR_CAST_RAW_OBJECT_AS_SUPER(fixture.instance)));
    }
    TEST_ASSERT_FALSE(ZrCore_GarbageCollector_IsObjectIgnored(state->global,
                                                              ZR_CAST_RAW_OBJECT_AS_SUPER(fixture.instance)));

    assignedObject = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(assignedObject);
    ZrCore_Object_Init(state, assignedObject);
    ZrCore_Value_InitAsRawObject(state, &stableAssignedValue, ZR_CAST_RAW_OBJECT_AS_SUPER(assignedObject));
    stableAssignedValue.type = ZR_VALUE_TYPE_OBJECT;
    if (ZrCore_GarbageCollector_IsObjectIgnored(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(assignedObject))) {
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_UnignoreObject(state->global,
                                                                ZR_CAST_RAW_OBJECT_AS_SUPER(assignedObject)));
    }
    TEST_ASSERT_FALSE(ZrCore_GarbageCollector_IsObjectIgnored(state->global,
                                                              ZR_CAST_RAW_OBJECT_AS_SUPER(assignedObject)));

    fill_ignore_registry_to_capacity(state);
    collector = state->global->garbageCollector;
    ignoredObjectCountBefore = collector->ignoredObjectCount;
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)collector->ignoredObjectCapacity, (TZrUInt32)ignoredObjectCountBefore);
    allocatorContext.failAllocateSize = ignore_registry_growth_allocation_size(collector);
    allocatorContext.failAllocateActive = ZR_TRUE;
    initialMemberVersion = fixture.instance->memberVersion;

    TEST_ASSERT_TRUE(execution_member_set_by_name(
            state, ZR_NULL, &stableReceiver, fixture.memberName, &stableAssignedValue));

    TEST_ASSERT_EQUAL_UINT32(0u, allocatorContext.failAllocateCount);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, pair->value.type);
    TEST_ASSERT_EQUAL_PTR(ZR_CAST_RAW_OBJECT_AS_SUPER(assignedObject), pair->value.value.object);
    TEST_ASSERT_EQUAL_UINT32(initialMemberVersion + 1u, fixture.instance->memberVersion);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)ignoredObjectCountBefore, (TZrUInt32)collector->ignoredObjectCount);

    ZrCore_GlobalState_Free(state->global);
}

static void test_execution_member_set_cached_descriptor_existing_pair_slow_lane_reuses_existing_storage_without_ignore_registry_growth(
        void) {
    SZrSelectiveAllocatorContext allocatorContext = {0};
    SZrState *state = create_runtime_state_with_selective_allocator(&allocatorContext);
    SZrMemberAccessFixture fixture;
    SZrTypeValue stableReceiver;
    SZrObject *assignedObject;
    SZrTypeValue stableAssignedValue;
    SZrHashKeyValuePair *pair;
    SZrGarbageCollector *collector;
    TZrSize ignoredObjectCountBefore;
    TZrUInt32 initialMemberVersion;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);
    TEST_ASSERT_NOT_NULL(state->global->garbageCollector);

    init_member_access_fixture(state, &fixture, 73);
    fixture.cacheEntry.kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET;
    fixture.cacheEntry.picSlotCount = 1;
    fixture.cacheEntry.picSlots[0].cachedReceiverObject = ZR_NULL;
    fixture.cacheEntry.picSlots[0].cachedReceiverPair = ZR_NULL;
    fixture.cacheEntry.picSlots[0].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_NONE;
    fixture.cacheEntry.picSlots[0].cachedReceiverPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[0].cachedOwnerPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[0].cachedReceiverVersion = fixture.prototype->super.memberVersion;
    fixture.cacheEntry.picSlots[0].cachedOwnerVersion = fixture.prototype->super.memberVersion;
    fixture.cacheEntry.picSlots[0].cachedDescriptorIndex = 0u;
    pair = fixture.instance->cachedStringLookupPair;
    TEST_ASSERT_NOT_NULL(pair);

    stableReceiver = fixture.receiverValue;
    if (ZrCore_GarbageCollector_IsObjectIgnored(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(fixture.instance))) {
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_UnignoreObject(state->global,
                                                                ZR_CAST_RAW_OBJECT_AS_SUPER(fixture.instance)));
    }
    TEST_ASSERT_FALSE(ZrCore_GarbageCollector_IsObjectIgnored(state->global,
                                                              ZR_CAST_RAW_OBJECT_AS_SUPER(fixture.instance)));

    assignedObject = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(assignedObject);
    ZrCore_Object_Init(state, assignedObject);
    ZrCore_Value_InitAsRawObject(state, &stableAssignedValue, ZR_CAST_RAW_OBJECT_AS_SUPER(assignedObject));
    stableAssignedValue.type = ZR_VALUE_TYPE_OBJECT;
    if (ZrCore_GarbageCollector_IsObjectIgnored(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(assignedObject))) {
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_UnignoreObject(state->global,
                                                                ZR_CAST_RAW_OBJECT_AS_SUPER(assignedObject)));
    }
    TEST_ASSERT_FALSE(ZrCore_GarbageCollector_IsObjectIgnored(state->global,
                                                              ZR_CAST_RAW_OBJECT_AS_SUPER(assignedObject)));

    fill_ignore_registry_to_capacity(state);
    collector = state->global->garbageCollector;
    ignoredObjectCountBefore = collector->ignoredObjectCount;
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)collector->ignoredObjectCapacity, (TZrUInt32)ignoredObjectCountBefore);
    allocatorContext.failAllocateSize = ignore_registry_growth_allocation_size(collector);
    allocatorContext.failAllocateActive = ZR_TRUE;
    initialMemberVersion = fixture.instance->memberVersion;

    TEST_ASSERT_TRUE(execution_member_set_cached(
            state, ZR_NULL, &fixture.function, 0, &stableReceiver, &stableAssignedValue));

    TEST_ASSERT_EQUAL_UINT32(0u, allocatorContext.failAllocateCount);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, pair->value.type);
    TEST_ASSERT_EQUAL_PTR(ZR_CAST_RAW_OBJECT_AS_SUPER(assignedObject), pair->value.value.object);
    TEST_ASSERT_EQUAL_UINT32(initialMemberVersion + 1u, fixture.instance->memberVersion);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)ignoredObjectCountBefore, (TZrUInt32)collector->ignoredObjectCount);

    ZrCore_GlobalState_Free(state->global);
}

static void test_object_direct_storage_key_fast_path_rejects_released_short_string_key(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *memberName;
    SZrTypeValue key;
    const SZrTypeValue *storageKey;

    TEST_ASSERT_NOT_NULL(state);

    memberName = ZrCore_String_CreateFromNative(state, "value");
    TEST_ASSERT_NOT_NULL(memberName);
    TEST_ASSERT_TRUE(ZrCore_String_IsShort(memberName));

    ZrCore_RawObject_MarkAsReleased(ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));
    TEST_ASSERT_TRUE(ZrCore_RawObject_IsReleased(ZR_CAST_RAW_OBJECT_AS_SUPER(memberName)));

    init_string_key(state, memberName, &key);
    storageKey = object_try_get_direct_storage_key_unchecked(&key);

    TEST_ASSERT_NULL(storageKey);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_object_set_value_references_released_short_string_key_through_slow_canonicalization(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrObject *object;
    SZrString *memberName;
    SZrTypeValue key;
    SZrTypeValue value;
    SZrHashKeyValuePair *pair;

    TEST_ASSERT_NOT_NULL(state);

    object = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(object);
    ZrCore_Object_Init(state, object);

    memberName = ZrCore_String_CreateFromNative(state, "value");
    TEST_ASSERT_NOT_NULL(memberName);
    TEST_ASSERT_TRUE(ZrCore_String_IsShort(memberName));

    ZrCore_RawObject_MarkAsReleased(ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));
    TEST_ASSERT_TRUE(ZrCore_RawObject_IsReleased(ZR_CAST_RAW_OBJECT_AS_SUPER(memberName)));

    init_string_key(state, memberName, &key);
    ZrCore_Value_InitAsInt(state, &value, 73);
    ZrCore_Object_SetValue(state, object, &key, &value);

    pair = object->cachedStringLookupPair;
    TEST_ASSERT_NOT_NULL(pair);
    TEST_ASSERT_EQUAL_PTR(ZR_CAST_RAW_OBJECT_AS_SUPER(memberName), pair->key.value.object);
    TEST_ASSERT_FALSE(ZrCore_RawObject_IsReleased(ZR_CAST_RAW_OBJECT_AS_SUPER(memberName)));
    TEST_ASSERT_TRUE(ZrCore_RawObject_IsReferenced(ZR_CAST_RAW_OBJECT_AS_SUPER(memberName)));

    ZrTests_Runtime_State_Destroy(state);
}

static void test_object_set_value_equal_long_string_key_reuses_cached_pair_without_extra_managed_memory(void) {
    static char longFieldName[] =
            "cached_member_lookup_long_field_name_that_must_stay_beyond_short_string_limit_"
            "and_keep_going_past_the_runtime_short_string_threshold_for_a_second_segment";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrObject *object;
    SZrString *memberNameA;
    SZrString *memberNameB;
    SZrTypeValue keyA;
    SZrTypeValue keyB;
    SZrTypeValue initialValue;
    SZrTypeValue assignedValue;
    SZrHashKeyValuePair *pair;
    SZrString *storedKeyString;
    SZrGarbageCollectorStatsSnapshot beforeSnapshot;
    SZrGarbageCollectorStatsSnapshot afterSnapshot;
    TZrUInt32 initialMemberVersion;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    object = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(object);
    ZrCore_Object_Init(state, object);

    memberNameA = ZrCore_String_Create(state, longFieldName, strlen(longFieldName));
    memberNameB = ZrCore_String_Create(state, longFieldName, strlen(longFieldName));
    TEST_ASSERT_NOT_NULL(memberNameA);
    TEST_ASSERT_NOT_NULL(memberNameB);
    TEST_ASSERT_TRUE(memberNameA != memberNameB);
    TEST_ASSERT_TRUE(ZrCore_String_Equal(memberNameA, memberNameB));

    init_string_key(state, memberNameA, &keyA);
    init_string_key(state, memberNameB, &keyB);

    ZrCore_Value_InitAsInt(state, &initialValue, 73);
    ZrCore_Object_SetValue(state, object, &keyA, &initialValue);

    pair = object->cachedStringLookupPair;
    TEST_ASSERT_NOT_NULL(pair);
    storedKeyString = ZR_CAST_STRING(state, pair->key.value.object);
    TEST_ASSERT_NOT_NULL(storedKeyString);
    TEST_ASSERT_TRUE(ZrCore_String_Equal(storedKeyString, memberNameA));
    TEST_ASSERT_TRUE(storedKeyString != memberNameB);

    ZrCore_Value_InitAsInt(state, &assignedValue, 105);
    initialMemberVersion = object->memberVersion;
    memset(&beforeSnapshot, 0, sizeof(beforeSnapshot));
    memset(&afterSnapshot, 0, sizeof(afterSnapshot));
    ZrCore_GarbageCollector_GetStatsSnapshot(state->global, &beforeSnapshot);

    ZrCore_Object_SetValue(state, object, &keyB, &assignedValue);

    ZrCore_GarbageCollector_GetStatsSnapshot(state->global, &afterSnapshot);
    TEST_ASSERT_EQUAL_UINT64(beforeSnapshot.managedMemoryBytes, afterSnapshot.managedMemoryBytes);
    TEST_ASSERT_EQUAL_PTR(pair, object->cachedStringLookupPair);
    TEST_ASSERT_EQUAL_PTR(storedKeyString, ZR_CAST_STRING(state, pair->key.value.object));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, pair->value.type);
    TEST_ASSERT_EQUAL_INT64(105, pair->value.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(initialMemberVersion + 1u, object->memberVersion);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_object_set_value_distinct_long_string_same_length_does_not_reuse_cached_pair(void) {
    static char longFieldNameA[] =
            "cached_member_lookup_long_field_name_that_must_stay_beyond_short_string_limit_"
            "and_keep_going_past_the_runtime_short_string_threshold_for_a_second_segment";
    static char longFieldNameB[] =
            "cached_member_lookup_long_field_name_that_must_stay_beyond_short_string_limit_"
            "and_keep_going_past_the_runtime_short_string_threshold_for_a_second_segmenu";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrObject *object;
    SZrString *memberNameA;
    SZrString *memberNameB;
    SZrTypeValue keyA;
    SZrTypeValue keyB;
    SZrTypeValue initialValue;
    SZrTypeValue assignedValue;
    SZrHashKeyValuePair *pair;
    SZrHashKeyValuePair *newPair;
    SZrString *storedKeyString;
    TZrUInt32 initialMemberVersion;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_EQUAL_INT(strlen(longFieldNameA), strlen(longFieldNameB));

    object = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(object);
    ZrCore_Object_Init(state, object);

    memberNameA = ZrCore_String_Create(state, longFieldNameA, strlen(longFieldNameA));
    memberNameB = ZrCore_String_Create(state, longFieldNameB, strlen(longFieldNameB));
    TEST_ASSERT_NOT_NULL(memberNameA);
    TEST_ASSERT_NOT_NULL(memberNameB);
    TEST_ASSERT_TRUE(memberNameA != memberNameB);
    TEST_ASSERT_FALSE(ZrCore_String_Equal(memberNameA, memberNameB));

    init_string_key(state, memberNameA, &keyA);
    init_string_key(state, memberNameB, &keyB);

    ZrCore_Value_InitAsInt(state, &initialValue, 73);
    ZrCore_Object_SetValue(state, object, &keyA, &initialValue);

    pair = object->cachedStringLookupPair;
    TEST_ASSERT_NOT_NULL(pair);
    storedKeyString = ZR_CAST_STRING(state, pair->key.value.object);
    TEST_ASSERT_NOT_NULL(storedKeyString);
    TEST_ASSERT_TRUE(ZrCore_String_Equal(storedKeyString, memberNameA));
    TEST_ASSERT_FALSE(ZrCore_String_Equal(storedKeyString, memberNameB));

    ZrCore_Value_InitAsInt(state, &assignedValue, 105);
    initialMemberVersion = object->memberVersion;

    ZrCore_Object_SetValue(state, object, &keyB, &assignedValue);

    newPair = object->cachedStringLookupPair;
    TEST_ASSERT_NOT_NULL(newPair);
    TEST_ASSERT_TRUE(newPair != pair);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, pair->value.type);
    TEST_ASSERT_EQUAL_INT64(73, pair->value.value.nativeObject.nativeInt64);
    TEST_ASSERT_TRUE(ZrCore_String_Equal(ZR_CAST_STRING(state, newPair->key.value.object), memberNameB));
    TEST_ASSERT_FALSE(ZrCore_String_Equal(ZR_CAST_STRING(state, newPair->key.value.object), memberNameA));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, newPair->value.type);
    TEST_ASSERT_EQUAL_INT64(105, newPair->value.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(initialMemberVersion + 1u, object->memberVersion);

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

static void test_object_set_existing_pair_value_after_fast_miss_updates_value_with_hidden_items_cached_state(void) {
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

    TEST_ASSERT_FALSE(
            object_try_set_existing_pair_plain_value_fast_unchecked(state, fixture.instance, pair, &assignedValue));

    ZrCore_Object_SetExistingPairValueAfterFastMissUnchecked(state, fixture.instance, pair, &assignedValue);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, pair->value.type);
    TEST_ASSERT_EQUAL_INT64(105, pair->value.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_PTR(pair, fixture.instance->cachedStringLookupPair);
    TEST_ASSERT_EQUAL_PTR(hiddenItemsObject, fixture.instance->cachedHiddenItemsObject);
    TEST_ASSERT_EQUAL_UINT32(initialMemberVersion, fixture.instance->memberVersion);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_object_set_value_non_hidden_same_length_string_does_not_refresh_hidden_items_cache(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrObject *object;
    SZrObject *hiddenItemsObject;
    SZrString *hiddenFieldName;
    SZrString *nonHiddenFieldName;
    SZrTypeValue hiddenKey;
    SZrTypeValue nonHiddenKey;
    SZrTypeValue hiddenValue;
    SZrTypeValue assignedValue;

    TEST_ASSERT_NOT_NULL(state);

    object = ZrCore_Object_New(state, ZR_NULL);
    hiddenItemsObject = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
    TEST_ASSERT_NOT_NULL(object);
    TEST_ASSERT_NOT_NULL(hiddenItemsObject);
    ZrCore_Object_Init(state, object);
    ZrCore_Object_Init(state, hiddenItemsObject);
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(object)));
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(hiddenItemsObject)));

    hiddenFieldName = ZrCore_Object_CachedKnownFieldString(state, ZR_OBJECT_HIDDEN_ITEMS_FIELD);
    nonHiddenFieldName = ZrCore_String_CreateFromNative(state, "__zr_itemt");
    TEST_ASSERT_NOT_NULL(hiddenFieldName);
    TEST_ASSERT_NOT_NULL(nonHiddenFieldName);
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(nonHiddenFieldName)));
    TEST_ASSERT_TRUE(hiddenFieldName->super.hash != nonHiddenFieldName->super.hash);

    ZrCore_Value_InitAsRawObject(state, &hiddenKey, ZR_CAST_RAW_OBJECT_AS_SUPER(hiddenFieldName));
    hiddenKey.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsRawObject(state, &nonHiddenKey, ZR_CAST_RAW_OBJECT_AS_SUPER(nonHiddenFieldName));
    nonHiddenKey.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsRawObject(state, &hiddenValue, ZR_CAST_RAW_OBJECT_AS_SUPER(hiddenItemsObject));
    ZrCore_Value_InitAsInt(state, &assignedValue, 77);

    ZrCore_Object_SetValue(state, object, &hiddenKey, &hiddenValue);
    TEST_ASSERT_NOT_NULL(object->cachedHiddenItemsPair);
    TEST_ASSERT_EQUAL_PTR(hiddenItemsObject, object->cachedHiddenItemsObject);

    ZrCore_Object_SetValue(state, object, &nonHiddenKey, &assignedValue);
    TEST_ASSERT_NOT_NULL(object->cachedHiddenItemsPair);
    TEST_ASSERT_EQUAL_PTR(hiddenItemsObject, object->cachedHiddenItemsObject);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_object_get_own_string_pair_by_name_cached_preserves_existing_cache_on_miss(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrString *otherMemberName;
    SZrHashKeyValuePair *pair;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);

    pair = fixture.instance->cachedStringLookupPair;
    TEST_ASSERT_NOT_NULL(pair);

    otherMemberName = ZrCore_String_CreateFromNative(state, "other");
    TEST_ASSERT_NOT_NULL(otherMemberName);

    TEST_ASSERT_NULL(object_get_own_string_pair_by_name_cached_unchecked(state, fixture.instance, otherMemberName));
    TEST_ASSERT_EQUAL_PTR(pair, fixture.instance->cachedStringLookupPair);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_object_get_own_string_pair_by_name_cached_reuses_equal_long_string_cached_pair(void) {
    static char longFieldName[] =
            "cached_member_lookup_long_field_name_that_must_stay_beyond_short_string_limit_"
            "and_keep_going_past_the_runtime_short_string_threshold_for_a_second_segment";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrObject *object;
    SZrString *memberNameA;
    SZrString *memberNameB;
    SZrTypeValue keyA;
    SZrTypeValue initialValue;
    SZrHashKeyValuePair *pair;

    TEST_ASSERT_NOT_NULL(state);

    object = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(object);
    ZrCore_Object_Init(state, object);

    memberNameA = ZrCore_String_Create(state, longFieldName, strlen(longFieldName));
    memberNameB = ZrCore_String_Create(state, longFieldName, strlen(longFieldName));
    TEST_ASSERT_NOT_NULL(memberNameA);
    TEST_ASSERT_NOT_NULL(memberNameB);
    TEST_ASSERT_TRUE(memberNameA != memberNameB);
    TEST_ASSERT_TRUE(ZrCore_String_Equal(memberNameA, memberNameB));

    init_string_key(state, memberNameA, &keyA);
    ZrCore_Value_InitAsInt(state, &initialValue, 73);
    ZrCore_Object_SetValue(state, object, &keyA, &initialValue);

    pair = object->cachedStringLookupPair;
    TEST_ASSERT_NOT_NULL(pair);
    TEST_ASSERT_EQUAL_PTR(pair, object_get_own_string_pair_by_name_cached_unchecked(state, object, memberNameB));
    TEST_ASSERT_EQUAL_PTR(pair, object->cachedStringLookupPair);

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

static void test_object_get_member_cached_descriptor_plain_heap_object_reuses_original_object(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrProfileRuntime profileRuntime;
    SZrTypeValue result;
    SZrObject *storedObject;
    SZrHashKeyValuePair *storedPair;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    storedObject = create_plain_member_value_object(state);
    storedPair = set_member_access_fixture_object_value(state, &fixture, storedObject);
    ZrCore_Value_ResetAsNull(&result);
    reset_profile_counters_from_state_only(state, &profileRuntime);

    TEST_ASSERT_TRUE(ZrCore_Object_GetMemberCachedDescriptorUnchecked(
            state, &fixture.receiverValue, fixture.prototype, 0u, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);
    TEST_ASSERT_TRUE(result.isGarbageCollectable);
    TEST_ASSERT_EQUAL_PTR(storedPair->value.value.object, result.value.object);
    TEST_ASSERT_EQUAL_PTR(storedObject, ZR_CAST_OBJECT(state, result.value.object));
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_VALUE_KIND_NONE, result.ownershipKind);
    TEST_ASSERT_NULL(result.ownershipControl);
    TEST_ASSERT_NULL(result.ownershipWeakRef);
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_GET_MEMBER]);
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY]);

    clear_profile_counters(state);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_object_get_member_cached_descriptor_struct_object_still_clones_result(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrProfileRuntime profileRuntime;
    SZrTypeValue result;
    SZrObject *sourceObject;
    SZrHashKeyValuePair *storedPair;
    SZrObject *storedObject;
    SZrObject *copiedObject;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    sourceObject = create_struct_member_value_object(state, "HotPathDescriptorStructValue");
    storedPair = set_member_access_fixture_object_value(state, &fixture, sourceObject);
    storedObject = ZR_CAST_OBJECT(state, storedPair->value.value.object);
    TEST_ASSERT_NOT_NULL(storedObject);
    ZrCore_Value_ResetAsNull(&result);
    reset_profile_counters_from_state_only(state, &profileRuntime);

    TEST_ASSERT_TRUE(ZrCore_Object_GetMemberCachedDescriptorUnchecked(
            state, &fixture.receiverValue, fixture.prototype, 0u, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);
    TEST_ASSERT_TRUE(result.isGarbageCollectable);
    TEST_ASSERT_NOT_EQUAL(storedPair->value.value.object, result.value.object);
    copiedObject = ZR_CAST_OBJECT(state, result.value.object);
    TEST_ASSERT_NOT_NULL(copiedObject);
    TEST_ASSERT_EQUAL_INT(ZR_OBJECT_INTERNAL_TYPE_STRUCT, copiedObject->internalType);
    TEST_ASSERT_EQUAL_PTR(storedObject->prototype, copiedObject->prototype);
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_GET_MEMBER]);
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY]);

    clear_profile_counters(state);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_object_get_member_cached_descriptor_prototype_plain_heap_object_reuses_original_object(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrProfileRuntime profileRuntime;
    SZrTypeValue result;
    SZrString *receiverTypeName;
    SZrObjectPrototype *receiverPrototype;
    SZrMemberDescriptor descriptor;
    SZrTypeValue key;
    SZrTypeValue value;
    SZrObject *storedObject;
    SZrHashKeyValuePair *storedPair;

    TEST_ASSERT_NOT_NULL(state);

    memset(&fixture, 0, sizeof(fixture));
    fixture.memberName = ZrCore_String_CreateFromNative(state, "value");
    fixture.prototype = ZrCore_ObjectPrototype_New(
            state, ZrCore_String_CreateFromNative(state, "HotPathDescriptorOwnerBox"), ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    receiverTypeName = ZrCore_String_CreateFromNative(state, "HotPathDescriptorReceiverBox");
    TEST_ASSERT_NOT_NULL(fixture.memberName);
    TEST_ASSERT_NOT_NULL(fixture.prototype);
    TEST_ASSERT_NOT_NULL(receiverTypeName);

    receiverPrototype = ZrCore_ObjectPrototype_New(state, receiverTypeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    TEST_ASSERT_NOT_NULL(receiverPrototype);
    ZrCore_ObjectPrototype_SetSuper(state, receiverPrototype, fixture.prototype);

    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.name = fixture.memberName;
    descriptor.kind = ZR_MEMBER_DESCRIPTOR_KIND_FIELD;
    descriptor.isWritable = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrCore_ObjectPrototype_AddMemberDescriptor(state, fixture.prototype, &descriptor));

    fixture.instance = ZrCore_Object_New(state, receiverPrototype);
    TEST_ASSERT_NOT_NULL(fixture.instance);
    ZrCore_Object_Init(state, fixture.instance);
    ZrCore_Value_InitAsRawObject(state, &fixture.receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(fixture.instance));
    fixture.receiverValue.type = ZR_VALUE_TYPE_OBJECT;

    storedObject = create_plain_member_value_object(state);
    init_string_key(state, fixture.memberName, &key);
    ZrCore_Value_InitAsRawObject(state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(storedObject));
    value.type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Object_SetValue(state, &fixture.prototype->super, &key, &value);
    storedPair = object_get_own_string_pair_by_name_cached_unchecked(state, &fixture.prototype->super, fixture.memberName);
    TEST_ASSERT_NOT_NULL(storedPair);

    ZrCore_Value_ResetAsNull(&result);
    reset_profile_counters_from_state_only(state, &profileRuntime);

    TEST_ASSERT_TRUE(ZrCore_Object_GetMemberCachedDescriptorUnchecked(
            state, &fixture.receiverValue, fixture.prototype, 0u, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);
    TEST_ASSERT_TRUE(result.isGarbageCollectable);
    TEST_ASSERT_EQUAL_PTR(storedPair->value.value.object, result.value.object);
    TEST_ASSERT_EQUAL_PTR(storedObject, ZR_CAST_OBJECT(state, result.value.object));
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_VALUE_KIND_NONE, result.ownershipKind);
    TEST_ASSERT_NULL(result.ownershipControl);
    TEST_ASSERT_NULL(result.ownershipWeakRef);
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_GET_MEMBER]);
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY]);

    clear_profile_counters(state);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_object_get_member_cached_descriptor_prototype_struct_object_still_clones_result(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrProfileRuntime profileRuntime;
    SZrTypeValue result;
    SZrString *receiverTypeName;
    SZrObjectPrototype *receiverPrototype;
    SZrMemberDescriptor descriptor;
    SZrTypeValue key;
    SZrTypeValue value;
    SZrObject *sourceObject;
    SZrHashKeyValuePair *storedPair;
    SZrObject *storedObject;
    SZrObject *copiedObject;

    TEST_ASSERT_NOT_NULL(state);

    memset(&fixture, 0, sizeof(fixture));
    fixture.memberName = ZrCore_String_CreateFromNative(state, "value");
    fixture.prototype = ZrCore_ObjectPrototype_New(
            state, ZrCore_String_CreateFromNative(state, "HotPathDescriptorOwnerStructBox"), ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    receiverTypeName = ZrCore_String_CreateFromNative(state, "HotPathDescriptorReceiverStructBox");
    TEST_ASSERT_NOT_NULL(fixture.memberName);
    TEST_ASSERT_NOT_NULL(fixture.prototype);
    TEST_ASSERT_NOT_NULL(receiverTypeName);

    receiverPrototype = ZrCore_ObjectPrototype_New(state, receiverTypeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    TEST_ASSERT_NOT_NULL(receiverPrototype);
    ZrCore_ObjectPrototype_SetSuper(state, receiverPrototype, fixture.prototype);

    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.name = fixture.memberName;
    descriptor.kind = ZR_MEMBER_DESCRIPTOR_KIND_FIELD;
    descriptor.isWritable = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrCore_ObjectPrototype_AddMemberDescriptor(state, fixture.prototype, &descriptor));

    fixture.instance = ZrCore_Object_New(state, receiverPrototype);
    TEST_ASSERT_NOT_NULL(fixture.instance);
    ZrCore_Object_Init(state, fixture.instance);
    ZrCore_Value_InitAsRawObject(state, &fixture.receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(fixture.instance));
    fixture.receiverValue.type = ZR_VALUE_TYPE_OBJECT;

    sourceObject = create_struct_member_value_object(state, "HotPathDescriptorPrototypeStructValue");
    init_string_key(state, fixture.memberName, &key);
    ZrCore_Value_InitAsRawObject(state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(sourceObject));
    value.type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Object_SetValue(state, &fixture.prototype->super, &key, &value);
    storedPair = object_get_own_string_pair_by_name_cached_unchecked(state, &fixture.prototype->super, fixture.memberName);
    TEST_ASSERT_NOT_NULL(storedPair);
    storedObject = ZR_CAST_OBJECT(state, storedPair->value.value.object);
    TEST_ASSERT_NOT_NULL(storedObject);

    ZrCore_Value_ResetAsNull(&result);
    reset_profile_counters_from_state_only(state, &profileRuntime);

    TEST_ASSERT_TRUE(ZrCore_Object_GetMemberCachedDescriptorUnchecked(
            state, &fixture.receiverValue, fixture.prototype, 0u, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);
    TEST_ASSERT_TRUE(result.isGarbageCollectable);
    TEST_ASSERT_NOT_EQUAL(storedPair->value.value.object, result.value.object);
    copiedObject = ZR_CAST_OBJECT(state, result.value.object);
    TEST_ASSERT_NOT_NULL(copiedObject);
    TEST_ASSERT_EQUAL_INT(ZR_OBJECT_INTERNAL_TYPE_STRUCT, copiedObject->internalType);
    TEST_ASSERT_EQUAL_PTR(storedObject->prototype, copiedObject->prototype);
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

static void test_object_get_value_reuses_equal_long_string_cached_pair(void) {
    static char longFieldName[] =
            "cached_member_get_value_long_field_name_that_must_stay_beyond_short_string_limit_"
            "and_keep_going_past_the_runtime_short_string_threshold_for_a_second_segment";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrObject *object;
    SZrString *memberNameA;
    SZrString *memberNameB;
    SZrTypeValue keyA;
    SZrTypeValue keyB;
    SZrTypeValue initialValue;
    SZrHashKeyValuePair *pair;
    const SZrTypeValue *result;

    TEST_ASSERT_NOT_NULL(state);

    object = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(object);
    ZrCore_Object_Init(state, object);

    memberNameA = ZrCore_String_Create(state, longFieldName, strlen(longFieldName));
    memberNameB = ZrCore_String_Create(state, longFieldName, strlen(longFieldName));
    TEST_ASSERT_NOT_NULL(memberNameA);
    TEST_ASSERT_NOT_NULL(memberNameB);
    TEST_ASSERT_TRUE(memberNameA != memberNameB);
    TEST_ASSERT_TRUE(ZrCore_String_Equal(memberNameA, memberNameB));

    init_string_key(state, memberNameA, &keyA);
    init_string_key(state, memberNameB, &keyB);
    ZrCore_Value_InitAsInt(state, &initialValue, 73);
    ZrCore_Object_SetValue(state, object, &keyA, &initialValue);

    pair = object->cachedStringLookupPair;
    TEST_ASSERT_NOT_NULL(pair);

    result = ZrCore_Object_GetValue(state, object, &keyB);
    TEST_ASSERT_EQUAL_PTR(&pair->value, result);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result->type);
    TEST_ASSERT_EQUAL_INT64(73, result->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_PTR(pair, object->cachedStringLookupPair);

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

static void test_object_get_member_with_key_records_helpers_from_state_without_tls_current(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrProfileRuntime profileRuntime;
    SZrTypeValue key;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    init_callable_member_access_fixture(state, &fixture);
    init_string_key(state, fixture.memberName, &key);
    ZrCore_Value_ResetAsNull(&result);
    reset_profile_counters_from_state_only(state, &profileRuntime);

    TEST_ASSERT_TRUE(ZrCore_Object_GetMemberWithKeyUnchecked(
            state, &fixture.receiverValue, fixture.memberName, &key, &result));
    TEST_ASSERT_EQUAL_PTR(fixture.callableFunction, result.value.object);
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_GET_MEMBER]);
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY]);

    clear_profile_counters(state);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_object_try_get_member_fast_records_value_copy_helper_from_state_without_tls_current(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrProfileRuntime profileRuntime;
    SZrTypeValue result;
    TZrBool handled = ZR_FALSE;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    fixture.instance->cachedStringLookupPair = ZR_NULL;
    ZrCore_Value_ResetAsNull(&result);
    reset_profile_counters_from_state_only(state, &profileRuntime);

    TEST_ASSERT_TRUE(ZrCore_Object_TryGetMemberWithKeyFastUnchecked(
            state, &fixture.receiverValue, fixture.memberName, ZR_NULL, &result, &handled));
    TEST_ASSERT_TRUE(handled);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(73, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY]);

    clear_profile_counters(state);
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
    fixture.cacheEntry.picSlots[0].cachedReceiverVersion = fixture.prototype->super.memberVersion;
    fixture.cacheEntry.picSlots[0].cachedOwnerVersion = fixture.prototype->super.memberVersion;
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
    fixture.cacheEntry.picSlots[1].cachedReceiverVersion = fixture.prototype->super.memberVersion;
    fixture.cacheEntry.picSlots[1].cachedOwnerVersion = fixture.prototype->super.memberVersion;
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

static void test_member_get_cached_multi_slot_callable_receiver_object_hit_ignores_earlier_stale_prototype_slot(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    init_callable_member_access_fixture(state, &fixture);
    fixture.cacheEntry.picSlotCount = 2;
    fixture.cacheEntry.picSlots[0].cachedReceiverPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[0].cachedOwnerPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[0].cachedReceiverVersion = fixture.prototype->super.memberVersion + 1u;
    fixture.cacheEntry.picSlots[0].cachedOwnerVersion = fixture.prototype->super.memberVersion + 1u;
    fixture.cacheEntry.picSlots[0].cachedDescriptorIndex = 0u;
    fixture.cacheEntry.picSlots[1].cachedReceiverPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[1].cachedOwnerPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[1].cachedFunction = fixture.callableFunction;
    fixture.cacheEntry.picSlots[1].cachedReceiverObject = fixture.instance;
    fixture.cacheEntry.picSlots[1].cachedReceiverVersion = fixture.prototype->super.memberVersion;
    fixture.cacheEntry.picSlots[1].cachedOwnerVersion = fixture.prototype->super.memberVersion;
    fixture.cacheEntry.picSlots[1].cachedDescriptorIndex = ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, &fixture.function, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_EQUAL_PTR(fixture.callableFunction, result.value.object);
    TEST_ASSERT_EQUAL_UINT32(2u, fixture.cacheEntry.picSlotCount);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_get_cached_multi_slot_exact_receiver_descriptor_hit_ignores_earlier_stale_prototype_slot(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    init_property_getter_member_access_fixture(state, &fixture);
    fixture.cacheEntry.picSlotCount = 2;
    fixture.cacheEntry.picSlots[0].cachedReceiverPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[0].cachedOwnerPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[0].cachedReceiverVersion = fixture.prototype->super.memberVersion + 1u;
    fixture.cacheEntry.picSlots[0].cachedOwnerVersion = fixture.prototype->super.memberVersion + 1u;
    fixture.cacheEntry.picSlots[0].cachedDescriptorIndex = 0u;
    fixture.cacheEntry.picSlots[1].cachedReceiverPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[1].cachedOwnerPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[1].cachedReceiverObject = fixture.instance;
    fixture.cacheEntry.picSlots[1].cachedReceiverVersion = fixture.prototype->super.memberVersion;
    fixture.cacheEntry.picSlots[1].cachedOwnerVersion = fixture.prototype->super.memberVersion;
    fixture.cacheEntry.picSlots[1].cachedDescriptorIndex = 0u;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, &fixture.function, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(77, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(2u, fixture.cacheEntry.picSlotCount);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_cached_known_vm_call_fast_path_resolves_cached_function_without_member_get_helper(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrProfileRuntime profileRuntime;
    SZrFunction *resolvedFunction = ZR_NULL;
    TZrUInt32 argumentCount = 0;

    TEST_ASSERT_NOT_NULL(state);
    init_callable_member_access_fixture(state, &fixture);
    fixture.cacheEntry.picSlotCount = 1;
    fixture.cacheEntry.argumentCount = 1;
    fixture.cacheEntry.picSlots[0].cachedReceiverPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[0].cachedOwnerPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[0].cachedFunction = fixture.callableFunction;
    fixture.cacheEntry.picSlots[0].cachedReceiverObject = fixture.instance;
    fixture.cacheEntry.picSlots[0].cachedReceiverVersion = fixture.prototype->super.memberVersion;
    fixture.cacheEntry.picSlots[0].cachedOwnerVersion = fixture.prototype->super.memberVersion;
    fixture.cacheEntry.picSlots[0].cachedDescriptorIndex = ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;
    reset_profile_counters_from_state_only(state, &profileRuntime);

    TEST_ASSERT_TRUE(execution_member_try_resolve_cached_known_vm_function(state,
                                                                           &fixture.function,
                                                                           0,
                                                                           &fixture.receiverValue,
                                                                           &resolvedFunction,
                                                                           &argumentCount));
    TEST_ASSERT_EQUAL_PTR(fixture.callableFunction, resolvedFunction);
    TEST_ASSERT_EQUAL_UINT32(1u, argumentCount);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);
    TEST_ASSERT_EQUAL_UINT64(0u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_GET_MEMBER]);
    TEST_ASSERT_EQUAL_UINT64(0u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY]);

    clear_profile_counters(state);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_cached_known_vm_call_entry_fast_path_reuses_checked_cache_entry(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrFunctionCallSiteCacheEntry *entry;
    SZrFunction *resolvedFunction = ZR_NULL;
    TZrUInt32 argumentCount = 0;

    TEST_ASSERT_NOT_NULL(state);
    init_callable_member_access_fixture(state, &fixture);
    fixture.cacheEntry.picSlotCount = 1;
    fixture.cacheEntry.argumentCount = 1;
    fixture.cacheEntry.picSlots[0].cachedReceiverPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[0].cachedOwnerPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[0].cachedFunction = fixture.callableFunction;
    fixture.cacheEntry.picSlots[0].cachedReceiverObject = fixture.instance;
    fixture.cacheEntry.picSlots[0].cachedReceiverVersion = fixture.prototype->super.memberVersion;
    fixture.cacheEntry.picSlots[0].cachedOwnerVersion = fixture.prototype->super.memberVersion;
    fixture.cacheEntry.picSlots[0].cachedDescriptorIndex = ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;

    entry = execution_member_get_cache_entry_fast(&fixture.function, 0, ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET);
    TEST_ASSERT_EQUAL_PTR(&fixture.cacheEntry, entry);
    TEST_ASSERT_TRUE(execution_member_try_resolve_cached_known_vm_function_entry(state,
                                                                                 &fixture.function,
                                                                                 0,
                                                                                 entry,
                                                                                 &fixture.receiverValue,
                                                                                 &resolvedFunction,
                                                                                 &argumentCount));
    TEST_ASSERT_EQUAL_PTR(fixture.callableFunction, resolvedFunction);
    TEST_ASSERT_EQUAL_UINT32(1u, argumentCount);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_cached_known_vm_call_entry_fast_path_returns_false_on_exact_receiver_miss(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrObject *otherInstance;
    SZrTypeValue otherReceiverValue;
    SZrFunctionCallSiteCacheEntry *entry;
    SZrFunction *resolvedFunction = ZR_NULL;
    TZrUInt32 argumentCount = 0;

    TEST_ASSERT_NOT_NULL(state);
    init_callable_member_access_fixture(state, &fixture);
    fixture.cacheEntry.picSlotCount = 1;
    fixture.cacheEntry.argumentCount = 1;
    fixture.cacheEntry.picSlots[0].cachedReceiverPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[0].cachedOwnerPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[0].cachedFunction = fixture.callableFunction;
    fixture.cacheEntry.picSlots[0].cachedReceiverObject = fixture.instance;
    fixture.cacheEntry.picSlots[0].cachedReceiverVersion = fixture.prototype->super.memberVersion;
    fixture.cacheEntry.picSlots[0].cachedOwnerVersion = fixture.prototype->super.memberVersion;
    fixture.cacheEntry.picSlots[0].cachedDescriptorIndex = ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;

    otherInstance = ZrCore_Object_New(state, fixture.prototype);
    TEST_ASSERT_NOT_NULL(otherInstance);
    ZrCore_Object_Init(state, otherInstance);
    ZrCore_Value_InitAsRawObject(state, &otherReceiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(otherInstance));
    otherReceiverValue.type = ZR_VALUE_TYPE_OBJECT;

    entry = execution_member_get_cache_entry_fast(&fixture.function, 0, ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET);
    TEST_ASSERT_EQUAL_PTR(&fixture.cacheEntry, entry);
    TEST_ASSERT_FALSE(execution_member_try_resolve_cached_known_vm_function_entry(state,
                                                                                  &fixture.function,
                                                                                  0,
                                                                                  entry,
                                                                                  &otherReceiverValue,
                                                                                  &resolvedFunction,
                                                                                  &argumentCount));
    TEST_ASSERT_NULL(resolvedFunction);
    TEST_ASSERT_EQUAL_UINT32(1u, argumentCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_cached_known_vm_call_fast_path_rejects_closure_function_on_exact_receiver_hit(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrFunction *resolvedFunction = ZR_NULL;
    TZrUInt32 argumentCount = 0;

    TEST_ASSERT_NOT_NULL(state);
    init_callable_member_access_fixture(state, &fixture);
    fixture.callableFunction->closureValueLength = 1u;
    fixture.cacheEntry.picSlotCount = 1;
    fixture.cacheEntry.argumentCount = 1;
    fixture.cacheEntry.picSlots[0].cachedReceiverPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[0].cachedOwnerPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[0].cachedFunction = fixture.callableFunction;
    fixture.cacheEntry.picSlots[0].cachedReceiverObject = fixture.instance;
    fixture.cacheEntry.picSlots[0].cachedReceiverVersion = fixture.prototype->super.memberVersion;
    fixture.cacheEntry.picSlots[0].cachedOwnerVersion = fixture.prototype->super.memberVersion;
    fixture.cacheEntry.picSlots[0].cachedDescriptorIndex = ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;

    TEST_ASSERT_FALSE(execution_member_try_resolve_cached_known_vm_function(state,
                                                                            &fixture.function,
                                                                            0,
                                                                            &fixture.receiverValue,
                                                                            &resolvedFunction,
                                                                            &argumentCount));
    TEST_ASSERT_NULL(resolvedFunction);
    TEST_ASSERT_EQUAL_UINT32(1u, argumentCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_cached_known_vm_call_fast_path_version_mismatch_clears_cache_and_falls_back(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrFunction *resolvedFunction = ZR_NULL;
    TZrUInt32 argumentCount = 0;

    TEST_ASSERT_NOT_NULL(state);
    init_callable_member_access_fixture(state, &fixture);
    fixture.cacheEntry.picSlotCount = 1;
    fixture.cacheEntry.argumentCount = 1;
    fixture.cacheEntry.picSlots[0].cachedReceiverPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[0].cachedOwnerPrototype = fixture.prototype;
    fixture.cacheEntry.picSlots[0].cachedFunction = fixture.callableFunction;
    fixture.cacheEntry.picSlots[0].cachedReceiverObject = fixture.instance;
    fixture.cacheEntry.picSlots[0].cachedReceiverVersion = fixture.prototype->super.memberVersion;
    fixture.cacheEntry.picSlots[0].cachedOwnerVersion = fixture.prototype->super.memberVersion;
    fixture.cacheEntry.picSlots[0].cachedDescriptorIndex = ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;
    fixture.prototype->super.memberVersion++;

    TEST_ASSERT_FALSE(execution_member_try_resolve_cached_known_vm_function(state,
                                                                            &fixture.function,
                                                                            0,
                                                                            &fixture.receiverValue,
                                                                            &resolvedFunction,
                                                                            &argumentCount));
    TEST_ASSERT_NULL(resolvedFunction);
    TEST_ASSERT_EQUAL_UINT32(1u, argumentCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.picSlotCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeMissCount);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_get_cached_refresh_marks_non_hidden_plain_value_fast_set_flag_for_instance_field(void) {
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
    TEST_ASSERT_TRUE((fixture.cacheEntry.picSlots[0].cachedIsStatic & ZR_TRUE) == 0u);
    TEST_ASSERT_TRUE((fixture.cacheEntry.picSlots[0].cachedIsStatic &
                      ZR_FUNCTION_CALLSITE_PIC_SLOT_FLAG_NON_HIDDEN_STRING_PAIR_FAST_SET) != 0u);

    detach_runtime_fixture_function(runtimeFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_get_cached_refresh_replaces_oldest_pic_slot_when_capacity_is_full(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrFunction *runtimeFunction;
    SZrObjectPrototype *prototypeB;
    SZrObjectPrototype *prototypeC;
    SZrObject *instanceB;
    SZrObject *instanceC;
    SZrTypeValue receiverValueB;
    SZrTypeValue receiverValueC;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 11);
    init_shared_name_member_access_variant(
            state, fixture.memberName, "HotPathMemberBoxB", 22, &prototypeB, &instanceB, &receiverValueB);
    init_shared_name_member_access_variant(
            state, fixture.memberName, "HotPathMemberBoxC", 33, &prototypeC, &instanceC, &receiverValueC);
    runtimeFunction = create_runtime_fixture_function(state, &fixture);
    fixture.cacheEntry.picSlotCount = 0;
    fixture.cacheEntry.picNextInsertIndex = 0;
    fixture.cacheEntry.runtimeHitCount = 0;
    fixture.cacheEntry.runtimeMissCount = 0;
    memset(fixture.cacheEntry.picSlots, 0, sizeof(fixture.cacheEntry.picSlots));
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, runtimeFunction, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_EQUAL_INT64(11, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.picSlotCount);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.picNextInsertIndex);
    TEST_ASSERT_EQUAL_PTR(fixture.prototype, fixture.cacheEntry.picSlots[0].cachedReceiverPrototype);
    TEST_ASSERT_EQUAL_PTR(fixture.instance, fixture.cacheEntry.picSlots[0].cachedReceiverObject);

    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, runtimeFunction, 0, &receiverValueB, &result));
    TEST_ASSERT_EQUAL_INT64(22, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY, fixture.cacheEntry.picSlotCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.picNextInsertIndex);
    TEST_ASSERT_EQUAL_PTR(fixture.prototype, fixture.cacheEntry.picSlots[0].cachedReceiverPrototype);
    TEST_ASSERT_EQUAL_PTR(prototypeB, fixture.cacheEntry.picSlots[1].cachedReceiverPrototype);

    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, runtimeFunction, 0, &receiverValueC, &result));
    TEST_ASSERT_EQUAL_INT64(33, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY, fixture.cacheEntry.picSlotCount);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.picNextInsertIndex);
    TEST_ASSERT_EQUAL_PTR(prototypeC, fixture.cacheEntry.picSlots[0].cachedReceiverPrototype);
    TEST_ASSERT_EQUAL_PTR(instanceC, fixture.cacheEntry.picSlots[0].cachedReceiverObject);
    TEST_ASSERT_EQUAL_PTR(prototypeB, fixture.cacheEntry.picSlots[1].cachedReceiverPrototype);
    TEST_ASSERT_EQUAL_PTR(instanceB, fixture.cacheEntry.picSlots[1].cachedReceiverObject);
    TEST_ASSERT_EQUAL_UINT32(3u, fixture.cacheEntry.runtimeMissCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.runtimeHitCount);

    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, runtimeFunction, 0, &receiverValueB, &result));
    TEST_ASSERT_EQUAL_INT64(22, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(3u, fixture.cacheEntry.runtimeMissCount);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.picNextInsertIndex);
    TEST_ASSERT_EQUAL_PTR(prototypeC, fixture.cacheEntry.picSlots[0].cachedReceiverPrototype);
    TEST_ASSERT_EQUAL_PTR(prototypeB, fixture.cacheEntry.picSlots[1].cachedReceiverPrototype);

    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, runtimeFunction, 0, &fixture.receiverValue, &result));
    TEST_ASSERT_EQUAL_INT64(11, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(4u, fixture.cacheEntry.runtimeMissCount);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture.cacheEntry.picNextInsertIndex);
    TEST_ASSERT_EQUAL_PTR(prototypeC, fixture.cacheEntry.picSlots[0].cachedReceiverPrototype);
    TEST_ASSERT_EQUAL_PTR(instanceC, fixture.cacheEntry.picSlots[0].cachedReceiverObject);
    TEST_ASSERT_EQUAL_PTR(fixture.prototype, fixture.cacheEntry.picSlots[1].cachedReceiverPrototype);
    TEST_ASSERT_EQUAL_PTR(fixture.instance, fixture.cacheEntry.picSlots[1].cachedReceiverObject);

    detach_runtime_fixture_function(runtimeFunction);
    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_member_get_by_name_fast_path_skips_extra_stable_result_copy);
    RUN_TEST(test_member_get_cached_fast_path_skips_extra_stable_result_copy);
    RUN_TEST(test_member_get_cached_instance_field_pair_hit_plain_heap_object_reuses_original_object);
    RUN_TEST(test_member_get_cached_instance_field_pair_hit_struct_object_still_clones_result);
    RUN_TEST(test_member_get_cached_exact_receiver_object_hit_plain_heap_object_reuses_original_object);
    RUN_TEST(test_member_get_cached_exact_receiver_object_hit_struct_object_still_clones_result);
    RUN_TEST(test_member_get_cached_stack_slot_fast_path_skips_anchor_copy);
    RUN_TEST(test_member_get_cached_property_getter_stack_slot_skips_anchor_restore_stack_lookup_when_stack_unchanged);
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
    RUN_TEST(test_member_get_cached_exact_receiver_object_hit_uses_cached_member_name_without_owner_metadata);
    RUN_TEST(test_member_get_cached_exact_receiver_object_backfills_cached_member_name_for_symbolless_followup);
    RUN_TEST(test_member_get_cached_exact_receiver_object_backfills_pair_from_slot_cached_name_without_descriptor_or_symbol);
    RUN_TEST(test_member_get_cached_multi_slot_exact_receiver_object_version_mismatch_clears_slot_and_falls_back);
    RUN_TEST(test_member_get_cached_multi_slot_exact_receiver_object_hit_uses_member_name_when_descriptor_name_is_missing);
    RUN_TEST(test_member_get_cached_multi_slot_exact_receiver_object_hit_ignores_earlier_stale_prototype_slot);
    RUN_TEST(test_member_get_cached_multi_slot_exact_receiver_object_hit_uses_cached_member_name_without_owner_metadata);
    RUN_TEST(test_member_get_cached_multi_slot_exact_receiver_object_backfills_cached_member_name_to_sibling_slot);
    RUN_TEST(test_member_set_cached_instance_field_hit_does_not_fallback_when_descriptor_index_is_missing);
    RUN_TEST(test_member_set_cached_exact_receiver_object_hit_uses_member_name_when_descriptor_name_is_missing);
    RUN_TEST(test_member_set_cached_exact_receiver_object_hit_uses_cached_member_name_without_owner_metadata);
    RUN_TEST(test_member_set_cached_exact_receiver_object_backfills_cached_member_name_for_symbolless_followup);
    RUN_TEST(test_member_set_cached_exact_receiver_object_backfills_pair_from_slot_cached_name_without_descriptor_or_symbol);
    RUN_TEST(test_member_set_cached_multi_slot_exact_receiver_object_hit_uses_member_name_when_descriptor_name_is_missing);
    RUN_TEST(test_member_set_cached_multi_slot_exact_receiver_object_hit_uses_cached_member_name_without_owner_metadata);
    RUN_TEST(test_member_set_cached_multi_slot_exact_receiver_object_backfills_cached_member_name_to_sibling_slot);
    RUN_TEST(test_member_set_cached_multi_slot_exact_receiver_object_hit_ignores_earlier_stale_prototype_slot);
    RUN_TEST(test_member_set_cached_multi_slot_exact_receiver_descriptor_hit_ignores_earlier_stale_prototype_slot);
    RUN_TEST(test_member_set_cached_multi_slot_exact_receiver_object_version_mismatch_clears_slot_and_falls_back);
    RUN_TEST(test_member_set_cached_instance_field_pair_receiver_object_hit_does_not_require_receiver_shape_cache);
    RUN_TEST(test_member_set_cached_instance_field_pair_hit_does_not_require_descriptor_or_member_name);
    RUN_TEST(test_member_set_cached_instance_field_pair_hit_does_not_bump_receiver_member_version);
    RUN_TEST(test_member_set_cached_instance_field_pair_non_hidden_slow_lane_does_not_touch_hidden_items_literal_cache);
    RUN_TEST(test_member_set_cached_exact_receiver_object_non_hidden_slow_lane_does_not_touch_hidden_items_literal_cache);
    RUN_TEST(test_member_get_cached_exact_receiver_pair_hit_ignores_cached_version_mismatch);
    RUN_TEST(test_member_set_cached_exact_receiver_pair_hit_ignores_cached_version_mismatch);
    RUN_TEST(test_member_get_cached_multi_slot_exact_receiver_pair_hit_ignores_cached_version_mismatch);
    RUN_TEST(test_member_set_cached_multi_slot_exact_receiver_pair_hit_ignores_cached_version_mismatch);
    RUN_TEST(test_member_get_cached_exact_receiver_pair_hit_records_helpers_from_state_without_tls_current);
    RUN_TEST(test_member_set_cached_exact_receiver_pair_hit_records_helpers_from_state_without_tls_current);
    RUN_TEST(test_dispatch_exact_receiver_pair_get_hot_fast_hits_and_records_helpers);
    RUN_TEST(test_dispatch_exact_receiver_pair_get_hot_fast_ignores_cached_version_mismatch);
    RUN_TEST(test_dispatch_exact_receiver_pair_get_checked_object_hot_fast_hits_and_records_helpers);
    RUN_TEST(test_dispatch_exact_receiver_pair_set_hot_fast_hits_and_records_helpers);
    RUN_TEST(test_dispatch_exact_receiver_pair_set_hot_fast_non_hidden_slow_lane_updates_value_with_hidden_items_cached_state);
    RUN_TEST(test_dispatch_exact_receiver_pair_set_checked_object_hot_fast_hits_and_records_helpers);
    RUN_TEST(test_member_set_cached_refresh_replaces_oldest_pic_slot_when_capacity_is_full);
    RUN_TEST(test_object_try_set_existing_pair_plain_value_fast_updates_value_and_lookup_cache);
    RUN_TEST(test_object_try_set_existing_pair_plain_value_fast_rejects_hidden_items_cached_state);
    RUN_TEST(test_object_set_value_existing_pair_updates_value_and_bumps_member_version);
    RUN_TEST(test_execution_member_set_by_name_stack_operands_reuse_vm_stack_roots_without_ignore_registry_growth);
    RUN_TEST(test_object_direct_storage_key_fast_path_reuses_live_short_string_key);
    RUN_TEST(test_execution_member_get_cached_descriptor_stack_receiver_hits_from_vm_stack_roots);
    RUN_TEST(test_execution_member_set_cached_descriptor_stack_operands_reuse_vm_stack_roots_without_ignore_registry_growth);
    RUN_TEST(test_execution_member_set_cached_descriptor_missing_pair_slow_lane_inserts_storage);
    RUN_TEST(test_execution_member_set_by_name_existing_pair_slow_lane_reuses_existing_storage_without_ignore_registry_growth);
    RUN_TEST(test_execution_member_set_cached_descriptor_existing_pair_slow_lane_reuses_existing_storage_without_ignore_registry_growth);
    RUN_TEST(test_object_direct_storage_key_fast_path_rejects_released_short_string_key);
    RUN_TEST(test_object_set_value_references_released_short_string_key_through_slow_canonicalization);
    RUN_TEST(test_object_set_value_equal_long_string_key_reuses_cached_pair_without_extra_managed_memory);
    RUN_TEST(test_object_set_value_distinct_long_string_same_length_does_not_reuse_cached_pair);
    RUN_TEST(test_object_try_set_existing_string_pair_plain_value_assume_non_hidden_updates_value_with_hidden_items_cached_state);
    RUN_TEST(test_object_set_existing_pair_value_after_fast_miss_updates_value_with_hidden_items_cached_state);
    RUN_TEST(test_object_set_value_non_hidden_same_length_string_does_not_refresh_hidden_items_cache);
    RUN_TEST(test_object_get_own_string_pair_by_name_cached_preserves_existing_cache_on_miss);
    RUN_TEST(test_object_get_own_string_pair_by_name_cached_reuses_equal_long_string_cached_pair);
    RUN_TEST(test_object_get_member_cached_descriptor_records_helpers_from_state_without_tls_current);
    RUN_TEST(test_object_get_member_cached_descriptor_plain_heap_object_reuses_original_object);
    RUN_TEST(test_object_get_member_cached_descriptor_struct_object_still_clones_result);
    RUN_TEST(test_object_get_member_cached_descriptor_prototype_plain_heap_object_reuses_original_object);
    RUN_TEST(test_object_get_member_cached_descriptor_prototype_struct_object_still_clones_result);
    RUN_TEST(test_object_get_value_populates_own_string_lookup_cache);
    RUN_TEST(test_object_get_value_reuses_equal_long_string_cached_pair);
    RUN_TEST(test_object_get_value_populates_prototype_string_lookup_cache_on_fallback);
    RUN_TEST(test_object_try_get_member_fast_allows_null_key_for_own_field_hit);
    RUN_TEST(test_object_try_get_member_fast_allows_null_key_for_prototype_method_hit);
    RUN_TEST(test_object_get_member_with_key_records_helpers_from_state_without_tls_current);
    RUN_TEST(test_object_try_get_member_fast_records_value_copy_helper_from_state_without_tls_current);
    RUN_TEST(test_member_get_cached_callable_receiver_hit_does_not_fallback_when_descriptor_index_is_missing);
    RUN_TEST(test_member_get_cached_callable_receiver_object_hit_does_not_require_receiver_shape_cache);
    RUN_TEST(test_member_get_cached_multi_slot_callable_receiver_object_hit_ignores_earlier_stale_prototype_slot);
    RUN_TEST(test_member_get_cached_multi_slot_exact_receiver_descriptor_hit_ignores_earlier_stale_prototype_slot);
    RUN_TEST(test_member_cached_known_vm_call_fast_path_resolves_cached_function_without_member_get_helper);
    RUN_TEST(test_member_cached_known_vm_call_entry_fast_path_reuses_checked_cache_entry);
    RUN_TEST(test_member_cached_known_vm_call_entry_fast_path_returns_false_on_exact_receiver_miss);
    RUN_TEST(test_member_cached_known_vm_call_fast_path_rejects_closure_function_on_exact_receiver_hit);
    RUN_TEST(test_member_cached_known_vm_call_fast_path_version_mismatch_clears_cache_and_falls_back);
    RUN_TEST(test_member_get_cached_refresh_marks_non_hidden_plain_value_fast_set_flag_for_instance_field);
    RUN_TEST(test_member_get_cached_refresh_replaces_oldest_pic_slot_when_capacity_is_full);

    return UNITY_END();
}
