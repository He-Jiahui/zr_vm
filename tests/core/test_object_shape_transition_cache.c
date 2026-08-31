#include "unity.h"

#include <stdio.h>
#include <string.h>

#include "tests/harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/profile.h"
#include "zr_vm_core/value.h"
#include "zr_vm_core/src/zr_vm_core/execution/execution_internal.h"

void setUp(void) {}
void tearDown(void) {}

static SZrObjectPrototype *new_shape_prototype(SZrState *state,
                                                SZrString *name,
                                                SZrString *memberName) {
    SZrMemberDescriptor descriptor;
    SZrObjectPrototype *prototype;

    prototype = ZrCore_ObjectPrototype_New(state, name, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    TEST_ASSERT_NOT_NULL(prototype);
    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.name = memberName;
    descriptor.kind = ZR_MEMBER_DESCRIPTOR_KIND_FIELD;
    descriptor.isWritable = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrCore_ObjectPrototype_AddMemberDescriptor(state, prototype, &descriptor));
    return prototype;
}

static SZrObject *new_shape_instance(SZrState *state,
                                      SZrObjectPrototype *prototype,
                                      SZrString *memberName,
                                      TZrInt64 value) {
    SZrObject *instance;
    SZrTypeValue key;
    SZrTypeValue storedValue;

    instance = ZrCore_Object_New(state, prototype);
    TEST_ASSERT_NOT_NULL(instance);
    ZrCore_Object_Init(state, instance);
    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));
    key.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsInt(state, &storedValue, value);
    ZrCore_Object_SetValue(state, instance, &key, &storedValue);
    return instance;
}

static void init_shape_cache(SZrFunction *function,
                             SZrFunctionMemberEntry *memberEntry,
                             SZrFunctionCallSiteCacheEntry *cacheEntry,
                             SZrString *memberName) {
    memset(memberEntry, 0, sizeof(*memberEntry));
    memberEntry->symbol = memberName;
    memberEntry->entryKind = ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL;
    function->memberEntries = memberEntry;
    function->memberEntryLength = 1u;
    function->callSiteCaches = cacheEntry;
    function->callSiteCacheLength = 1u;
    memset(cacheEntry, 0, sizeof(*cacheEntry));
    cacheEntry->kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET;
    cacheEntry->memberEntryIndex = 0u;
}

static void reset_shape_cache_entry(SZrFunctionCallSiteCacheEntry *cacheEntry) {
    memset(cacheEntry, 0, sizeof(*cacheEntry));
    cacheEntry->kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET;
    cacheEntry->memberEntryIndex = 0u;
}

static void detach_shape_cache(SZrFunction *function) {
    if (function == ZR_NULL) {
        return;
    }

    function->memberEntries = ZR_NULL;
    function->memberEntryLength = 0u;
    function->callSiteCaches = ZR_NULL;
    function->callSiteCacheLength = 0u;
}

static void test_shape_ids_are_stable_and_generation_tracks_mutation(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *nameA;
    SZrString *nameB;
    SZrString *fieldA;
    SZrString *fieldB;
    SZrObjectPrototype *prototypeA;
    SZrObjectPrototype *prototypeB;
    SZrFunction *metaFunction;
    TZrUInt64 shapeId;
    TZrUInt64 generation;

    TEST_ASSERT_NOT_NULL(state);
    nameA = ZrCore_String_CreateFromNative(state, "ShapeA");
    nameB = ZrCore_String_CreateFromNative(state, "ShapeB");
    fieldA = ZrCore_String_CreateFromNative(state, "value");
    fieldB = ZrCore_String_CreateFromNative(state, "other");
    TEST_ASSERT_NOT_NULL(nameA);
    TEST_ASSERT_NOT_NULL(nameB);
    TEST_ASSERT_NOT_NULL(fieldA);
    TEST_ASSERT_NOT_NULL(fieldB);

    prototypeA = new_shape_prototype(state, nameA, fieldA);
    prototypeB = new_shape_prototype(state, nameB, fieldA);
    TEST_ASSERT_NOT_EQUAL(prototypeA->shapeId, prototypeB->shapeId);
    TEST_ASSERT_NOT_EQUAL(0u, prototypeA->shapeId);
    TEST_ASSERT_NOT_EQUAL(0u, prototypeA->shapeGeneration);

    shapeId = prototypeA->shapeId;
    generation = prototypeA->shapeGeneration;
    {
        SZrMemberDescriptor descriptor;
        memset(&descriptor, 0, sizeof(descriptor));
        descriptor.name = fieldB;
        descriptor.kind = ZR_MEMBER_DESCRIPTOR_KIND_FIELD;
        descriptor.isWritable = ZR_TRUE;
        TEST_ASSERT_TRUE(ZrCore_ObjectPrototype_AddMemberDescriptor(state, prototypeA, &descriptor));
    }
    TEST_ASSERT_EQUAL_UINT64(shapeId, prototypeA->shapeId);
    TEST_ASSERT_GREATER_THAN_UINT64(generation, prototypeA->shapeGeneration);

    generation = prototypeA->shapeGeneration;
    ZrCore_ObjectPrototype_SetSuper(state, prototypeA, prototypeB);
    TEST_ASSERT_GREATER_THAN_UINT64(generation, prototypeA->shapeGeneration);

    metaFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(metaFunction);
    generation = prototypeA->shapeGeneration;
    ZrCore_ObjectPrototype_AddMeta(state, prototypeA, ZR_META_TO_STRING, metaFunction);
    TEST_ASSERT_GREATER_THAN_UINT64(generation, prototypeA->shapeGeneration);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_cache_has_four_shape_slots_and_rejects_stale_generation(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *memberName;
    SZrFunction *function;
    SZrFunctionMemberEntry memberEntry;
    SZrFunctionCallSiteCacheEntry cacheEntry;
    SZrObjectPrototype *prototypes[4];
    SZrObject *instances[4];
    SZrTypeValue receivers[4];
    SZrTypeValue result;
    TZrUInt64 staleGeneration;
    TZrUInt32 index;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_EQUAL_UINT32(4u, ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY);
    memberName = ZrCore_String_CreateFromNative(state, "value");
    TEST_ASSERT_NOT_NULL(memberName);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    init_shape_cache(function, &memberEntry, &cacheEntry, memberName);

    for (index = 0u; index < 4u; index++) {
        char nativeName[32];
        SZrString *prototypeName;

        (void)snprintf(nativeName, sizeof(nativeName), "ShapeSlot%u", index);
        prototypeName = ZrCore_String_CreateFromNative(state, nativeName);
        TEST_ASSERT_NOT_NULL(prototypeName);
        prototypes[index] = new_shape_prototype(state, prototypeName, memberName);
        instances[index] = new_shape_instance(state, prototypes[index], memberName, (TZrInt64)index + 10);
        ZrCore_Value_InitAsRawObject(state, &receivers[index], ZR_CAST_RAW_OBJECT_AS_SUPER(instances[index]));
        receivers[index].type = ZR_VALUE_TYPE_OBJECT;
        ZrCore_Value_ResetAsNull(&result);
        TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, function, 0u, &receivers[index], &result));
        TEST_ASSERT_EQUAL_INT64((TZrInt64)index + 10, result.value.nativeObject.nativeInt64);
    }

    TEST_ASSERT_EQUAL_UINT32(4u, cacheEntry.picSlotCount);
    for (index = 0u; index < 4u; index++) {
        TEST_ASSERT_EQUAL_UINT64(prototypes[index]->shapeId,
                                 cacheEntry.picSlots[index].cachedReceiverShapeId);
        TEST_ASSERT_EQUAL_UINT64(prototypes[index]->shapeGeneration,
                                 cacheEntry.picSlots[index].cachedReceiverShapeGeneration);
    }

    staleGeneration = prototypes[0]->shapeGeneration;
    {
        SZrMemberDescriptor descriptor;
        SZrString *otherName = ZrCore_String_CreateFromNative(state, "other");
        memset(&descriptor, 0, sizeof(descriptor));
        descriptor.name = otherName;
        descriptor.kind = ZR_MEMBER_DESCRIPTOR_KIND_FIELD;
        descriptor.isWritable = ZR_TRUE;
        TEST_ASSERT_TRUE(ZrCore_ObjectPrototype_AddMemberDescriptor(state, prototypes[0], &descriptor));
    }
    TEST_ASSERT_GREATER_THAN_UINT64(staleGeneration, prototypes[0]->shapeGeneration);
    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, function, 0u, &receivers[0], &result));
    TEST_ASSERT_EQUAL_INT64(10, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT64(prototypes[0]->shapeGeneration,
                             cacheEntry.picSlots[0].cachedReceiverShapeGeneration);

    detach_shape_cache(function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_cache_profile_classifies_fixed_pic_hits(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrProfileRuntime profileRuntime;
    SZrString *memberName;
    SZrFunction *function;
    SZrFunctionMemberEntry memberEntry;
    SZrFunctionCallSiteCacheEntry cacheEntry;
    SZrTypeValue receivers[4];
    SZrTypeValue result;
    TZrUInt32 index;

    TEST_ASSERT_NOT_NULL(state);
    memset(&profileRuntime, 0, sizeof(profileRuntime));
    profileRuntime.recordMemory = ZR_TRUE;
    state->global->profileRuntime = &profileRuntime;
    ZrCore_Profile_SetCurrentState(state);
    memberName = ZrCore_String_CreateFromNative(state, "value");
    TEST_ASSERT_NOT_NULL(memberName);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    init_shape_cache(function, &memberEntry, &cacheEntry, memberName);

    for (index = 0u; index < 4u; index++) {
        char nativeName[32];
        SZrString *prototypeName;
        SZrObjectPrototype *prototype;
        SZrObject *instance;

        (void)snprintf(nativeName, sizeof(nativeName), "ProfileShape%u", index);
        prototypeName = ZrCore_String_CreateFromNative(state, nativeName);
        TEST_ASSERT_NOT_NULL(prototypeName);
        prototype = new_shape_prototype(state, prototypeName, memberName);
        instance = new_shape_instance(state, prototype, memberName, (TZrInt64)index);
        ZrCore_Value_InitAsRawObject(state, &receivers[index], ZR_CAST_RAW_OBJECT_AS_SUPER(instance));
        receivers[index].type = ZR_VALUE_TYPE_OBJECT;
    }

    reset_shape_cache_entry(&cacheEntry);
    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, function, 0u, &receivers[0], &result));
    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, function, 0u, &receivers[0], &result));
    TEST_ASSERT_EQUAL_UINT64(
            1u,
            profileRuntime.memoryMetricCounts[ZR_PROFILE_MEMORY_MEMBER_CACHE_MONOMORPHIC_HIT_COUNT]);

    reset_shape_cache_entry(&cacheEntry);
    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, function, 0u, &receivers[0], &result));
    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, function, 0u, &receivers[1], &result));
    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, function, 0u, &receivers[0], &result));
    TEST_ASSERT_EQUAL_UINT64(
            1u,
            profileRuntime.memoryMetricCounts[ZR_PROFILE_MEMORY_MEMBER_CACHE_POLYMORPHIC_HIT_COUNT]);

    reset_shape_cache_entry(&cacheEntry);
    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, function, 0u, &receivers[0], &result));
    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, function, 0u, &receivers[1], &result));
    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, function, 0u, &receivers[2], &result));
    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, function, 0u, &receivers[3], &result));
    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, function, 0u, &receivers[0], &result));
    TEST_ASSERT_EQUAL_UINT64(
            1u,
            profileRuntime.memoryMetricCounts[ZR_PROFILE_MEMORY_MEMBER_CACHE_MEGAMORPHIC_HIT_COUNT]);

    state->global->profileRuntime = ZR_NULL;
    ZrCore_Profile_SetCurrentState(ZR_NULL);
    detach_shape_cache(function);
    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_shape_ids_are_stable_and_generation_tracks_mutation);
    RUN_TEST(test_member_cache_has_four_shape_slots_and_rejects_stale_generation);
    RUN_TEST(test_member_cache_profile_classifies_fixed_pic_hits);
    return UNITY_END();
}
