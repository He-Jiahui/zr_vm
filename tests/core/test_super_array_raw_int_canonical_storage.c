#include "unity.h"

#include "tests/harness/runtime_support.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/reflection.h"
#include "zr_vm_core/value.h"

static SZrObject *make_array_receiver(SZrState *state, SZrObject **outItems) {
    SZrObject *receiver;
    SZrObject *items;
    SZrString *hiddenName;
    SZrTypeValue key;
    SZrTypeValue value;

    receiver = ZrCore_Object_New(state, ZR_NULL);
    items = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
    if (receiver == ZR_NULL || items == ZR_NULL) {
        return ZR_NULL;
    }
    /* SuperArray helpers resolve payloads through the ArrayLike protocol. */
    receiver->prototype = state->global->basicTypeObjectPrototype[ZR_VALUE_TYPE_ARRAY];
    ZrCore_Object_Init(state, receiver);
    ZrCore_Object_Init(state, items);
    hiddenName = ZrCore_String_Create(state, "__zr_items", 11u);
    if (hiddenName == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(hiddenName));
    key.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsRawObject(state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(items));
    value.type = ZR_VALUE_TYPE_ARRAY;
    ZrCore_Object_SetValue(state, receiver, &key, &value);
    receiver->cachedHiddenItemsObject = items;
    if (outItems != ZR_NULL) {
        *outItems = items;
    }
    return receiver;
}

void setUp(void) {}
void tearDown(void) {}

static void test_raw_int_appends_do_not_allocate_node_pairs_until_generic_boundary(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrObject *items = ZR_NULL;
    SZrObject *receiver;
    SZrTypeValue receiverValue;
    SZrTypeValue input;
    SZrTypeValue result;
    SZrTypeValue key;
    const SZrTypeValue *genericValue;

    TEST_ASSERT_NOT_NULL(state);
    receiver = make_array_receiver(state, &items);
    TEST_ASSERT_NOT_NULL(receiver);
    TEST_ASSERT_NOT_NULL(items);
    ZrCore_Value_InitAsRawObject(state, &receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiver));
    receiverValue.type = ZR_VALUE_TYPE_OBJECT;

    ZrCore_Value_InitAsInt(state, &input, 11);
    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE(ZrCore_Object_SuperArrayAddInt(state, &receiverValue, &input, &result));
    ZrCore_Value_InitAsInt(state, &input, 22);
    TEST_ASSERT_TRUE(ZrCore_Object_SuperArrayAddInt(state, &receiverValue, &input, &result));
    TEST_ASSERT_EQUAL_INT(ZR_SUPER_ARRAY_STORAGE_MODE_RAW_CANONICAL, items->superArrayStorageMode);
    TEST_ASSERT_EQUAL_UINT64(0u, (UNITY_UINT64)items->nodeMap.elementCount);
    TEST_ASSERT_EQUAL_UINT64(2u, (UNITY_UINT64)items->superArrayRawIntLength);

    ZrCore_Value_InitAsInt(state, &key, 1);
    genericValue = ZrCore_Object_GetValue(state, items, &key);
    TEST_ASSERT_NOT_NULL(genericValue);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(genericValue->type));
    TEST_ASSERT_EQUAL_INT64(22, genericValue->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(ZR_SUPER_ARRAY_STORAGE_MODE_NODE_CANONICAL, items->superArrayStorageMode);
    TEST_ASSERT_NULL(items->superArrayRawIntData);
    TEST_ASSERT_EQUAL_UINT64(2u, (UNITY_UINT64)items->nodeMap.elementCount);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_raw_int_materialization_rounds_capacity_for_non_power_of_two_length(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrObject *items = ZR_NULL;
    SZrObject *receiver;
    SZrTypeValue receiverValue;
    SZrTypeValue input;
    SZrTypeValue result;
    SZrTypeValue key;
    const SZrTypeValue *genericValue;

    TEST_ASSERT_NOT_NULL(state);
    receiver = make_array_receiver(state, &items);
    TEST_ASSERT_NOT_NULL(receiver);
    TEST_ASSERT_NOT_NULL(items);
    ZrCore_Value_InitAsRawObject(state, &receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiver));
    receiverValue.type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_ResetAsNull(&result);

    for (TZrInt64 value = 1; value <= 3; value++) {
        ZrCore_Value_InitAsInt(state, &input, value);
        TEST_ASSERT_TRUE(ZrCore_Object_SuperArrayAddInt(state, &receiverValue, &input, &result));
    }
    ZrCore_Value_InitAsInt(state, &key, 2);
    genericValue = ZrCore_Object_GetValue(state, items, &key);
    TEST_ASSERT_NOT_NULL(genericValue);
    TEST_ASSERT_EQUAL_INT64(3, genericValue->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(ZR_SUPER_ARRAY_STORAGE_MODE_NODE_CANONICAL, items->superArrayStorageMode);
    TEST_ASSERT_EQUAL_UINT64(3u, (UNITY_UINT64)items->nodeMap.elementCount);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_generic_type_drift_keeps_node_storage_canonical(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrObject *items = ZR_NULL;
    SZrObject *receiver;
    SZrTypeValue receiverValue;
    SZrTypeValue input;
    SZrTypeValue result;
    SZrTypeValue key;
    SZrTypeValue objectValue;
    SZrString *text;

    TEST_ASSERT_NOT_NULL(state);
    receiver = make_array_receiver(state, &items);
    TEST_ASSERT_NOT_NULL(receiver);
    TEST_ASSERT_NOT_NULL(items);
    ZrCore_Value_InitAsRawObject(state, &receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiver));
    receiverValue.type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_InitAsInt(state, &input, 7);
    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE(ZrCore_Object_SuperArrayAddInt(state, &receiverValue, &input, &result));

    text = ZrCore_String_Create(state, "drift", 5u);
    TEST_ASSERT_NOT_NULL(text);
    ZrCore_Value_InitAsInt(state, &key, 0);
    ZrCore_Value_InitAsRawObject(state, &objectValue, ZR_CAST_RAW_OBJECT_AS_SUPER(text));
    objectValue.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Object_SetValue(state, items, &key, &objectValue);

    TEST_ASSERT_EQUAL_INT(ZR_SUPER_ARRAY_STORAGE_MODE_NODE_CANONICAL, items->superArrayStorageMode);
    TEST_ASSERT_NULL(items->superArrayRawIntData);
    TEST_ASSERT_EQUAL_UINT64(1u, (UNITY_UINT64)items->nodeMap.elementCount);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_raw_int_storage_survives_gc_move_and_root_resolution(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrObject *items = ZR_NULL;
    SZrObject *receiver;
    SZrGcRootHandle itemsRoot;
    SZrRawObject *originalRaw;
    TZrInt64 *originalRawData;
    SZrRawObject *resolvedRaw = ZR_NULL;
    SZrObject *resolvedItems;
    SZrTypeValue receiverValue;
    SZrTypeValue input;
    SZrTypeValue result;
    SZrTypeValue key;
    const SZrTypeValue *value;

    TEST_ASSERT_NOT_NULL(state);
    receiver = make_array_receiver(state, &items);
    TEST_ASSERT_NOT_NULL(receiver);
    TEST_ASSERT_NOT_NULL(items);
    ZrCore_Value_InitAsRawObject(state, &receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiver));
    receiverValue.type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_ResetAsNull(&result);
    ZrCore_Value_InitAsInt(state, &input, 31);
    TEST_ASSERT_TRUE(ZrCore_Object_SuperArrayAddInt(state, &receiverValue, &input, &result));
    ZrCore_Value_InitAsInt(state, &input, 47);
    TEST_ASSERT_TRUE(ZrCore_Object_SuperArrayAddInt(state, &receiverValue, &input, &result));
    TEST_ASSERT_TRUE(ZrCore_GcRootHandle_Create(
            state, ZR_CAST_RAW_OBJECT_AS_SUPER(items), &itemsRoot));
    originalRaw = ZR_CAST_RAW_OBJECT_AS_SUPER(items);
    originalRawData = items->superArrayRawIntData;

    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);

    TEST_ASSERT_TRUE(ZrCore_GcRootHandle_Resolve(state, &itemsRoot, &resolvedRaw));
    TEST_ASSERT_NOT_NULL(resolvedRaw);
    TEST_ASSERT_TRUE(resolvedRaw == originalRaw ||
                     originalRaw->garbageCollectMark.forwardingAddress == resolvedRaw);
    resolvedItems = ZR_CAST_OBJECT(state, resolvedRaw);
    TEST_ASSERT_NOT_NULL(resolvedItems);
    TEST_ASSERT_EQUAL_INT(ZR_SUPER_ARRAY_STORAGE_MODE_RAW_CANONICAL,
                          resolvedItems->superArrayStorageMode);
    TEST_ASSERT_NOT_NULL(resolvedItems->superArrayRawIntData);
    TEST_ASSERT_EQUAL_UINT64(2u, (UNITY_UINT64)resolvedItems->superArrayRawIntLength);
    TEST_ASSERT_EQUAL_INT64(31, resolvedItems->superArrayRawIntData[0]);
    TEST_ASSERT_EQUAL_INT64(47, resolvedItems->superArrayRawIntData[1]);
    if (resolvedRaw != originalRaw) {
        TEST_ASSERT_TRUE(resolvedItems->superArrayRawIntData != originalRawData);
    }

    ZrCore_Value_InitAsInt(state, &key, 1);
    value = ZrCore_Object_GetValue(state, resolvedItems, &key);
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_INT64(47, value->value.nativeObject.nativeInt64);

    ZrCore_GcRootHandle_Release(state, &itemsRoot);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_reflection_boundary_reads_raw_int_array_without_losing_values(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrObject *items = ZR_NULL;
    SZrObject *receiver;
    SZrTypeValue receiverValue;
    SZrTypeValue input;
    SZrTypeValue result;
    SZrTypeValue arrayValue;
    SZrTypeValue reflectionValue;
    SZrTypeValue key;
    const SZrTypeValue *value;

    TEST_ASSERT_NOT_NULL(state);
    receiver = make_array_receiver(state, &items);
    TEST_ASSERT_NOT_NULL(receiver);
    TEST_ASSERT_NOT_NULL(items);
    ZrCore_Value_InitAsRawObject(state, &receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiver));
    receiverValue.type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_ResetAsNull(&result);
    ZrCore_Value_InitAsInt(state, &input, 73);
    TEST_ASSERT_TRUE(ZrCore_Object_SuperArrayAddInt(state, &receiverValue, &input, &result));
    ZrCore_Value_InitAsRawObject(state, &arrayValue, ZR_CAST_RAW_OBJECT_AS_SUPER(items));
    arrayValue.type = ZR_VALUE_TYPE_ARRAY;
    ZrCore_Value_ResetAsNull(&reflectionValue);

    TEST_ASSERT_TRUE(ZrCore_Reflection_TypeOfValue(state, &arrayValue, &reflectionValue));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, reflectionValue.type);
    TEST_ASSERT_EQUAL_INT(ZR_SUPER_ARRAY_STORAGE_MODE_RAW_CANONICAL,
                          items->superArrayStorageMode);
    TEST_ASSERT_NOT_NULL(items->superArrayRawIntData);

    ZrCore_Value_InitAsInt(state, &key, 0);
    value = ZrCore_Object_GetValue(state, items, &key);
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_INT64(73, value->value.nativeObject.nativeInt64);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_int_to_object_transition_materializes_prior_raw_values(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrObject *items = ZR_NULL;
    SZrObject *receiver;
    SZrObject *child;
    SZrTypeValue receiverValue;
    SZrTypeValue input;
    SZrTypeValue result;
    SZrTypeValue key;
    SZrTypeValue objectValue;
    const SZrTypeValue *value;

    TEST_ASSERT_NOT_NULL(state);
    receiver = make_array_receiver(state, &items);
    TEST_ASSERT_NOT_NULL(receiver);
    TEST_ASSERT_NOT_NULL(items);
    child = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(child);
    ZrCore_Object_Init(state, child);
    ZrCore_Value_InitAsRawObject(state, &receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiver));
    receiverValue.type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_ResetAsNull(&result);
    ZrCore_Value_InitAsInt(state, &input, 83);
    TEST_ASSERT_TRUE(ZrCore_Object_SuperArrayAddInt(state, &receiverValue, &input, &result));
    ZrCore_Value_InitAsInt(state, &input, 97);
    TEST_ASSERT_TRUE(ZrCore_Object_SuperArrayAddInt(state, &receiverValue, &input, &result));

    ZrCore_Value_InitAsInt(state, &key, 1);
    ZrCore_Value_InitAsRawObject(state, &objectValue, ZR_CAST_RAW_OBJECT_AS_SUPER(child));
    objectValue.type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Object_SetValue(state, items, &key, &objectValue);

    TEST_ASSERT_EQUAL_INT(ZR_SUPER_ARRAY_STORAGE_MODE_NODE_CANONICAL,
                          items->superArrayStorageMode);
    TEST_ASSERT_NULL(items->superArrayRawIntData);
    ZrCore_Value_InitAsInt(state, &key, 0);
    value = ZrCore_Object_GetValue(state, items, &key);
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_INT64(83, value->value.nativeObject.nativeInt64);
    ZrCore_Value_InitAsInt(state, &key, 1);
    value = ZrCore_Object_GetValue(state, items, &key);
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, value->type);
    TEST_ASSERT_EQUAL_PTR(ZR_CAST_RAW_OBJECT_AS_SUPER(child), value->value.object);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_four_lane_append_supports_mixed_raw_and_node_canonical_arrays(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrObject *items[4] = {ZR_NULL, ZR_NULL, ZR_NULL, ZR_NULL};
    SZrObject *receiverObjects[4];
    SZrTypeValue receiverValues[4];
    SZrTypeValue *receivers[4];
    SZrTypeValue input;
    SZrTypeValue result;
    SZrTypeValue key;
    TZrUInt32 index;

    TEST_ASSERT_NOT_NULL(state);
    ZrCore_Value_ResetAsNull(&result);
    for (index = 0u; index < 4u; index++) {
        receiverObjects[index] = make_array_receiver(state, &items[index]);
        TEST_ASSERT_NOT_NULL(receiverObjects[index]);
        TEST_ASSERT_NOT_NULL(items[index]);
        ZrCore_Value_InitAsRawObject(
                state, &receiverValues[index], ZR_CAST_RAW_OBJECT_AS_SUPER(receiverObjects[index]));
        receiverValues[index].type = ZR_VALUE_TYPE_OBJECT;
        receivers[index] = &receiverValues[index];
    }

    ZrCore_Value_InitAsInt(state, &key, 0);
    ZrCore_Value_InitAsInt(state, &input, 10);
    ZrCore_Object_SetValue(state, items[0], &key, &input);
    TEST_ASSERT_EQUAL_INT(ZR_SUPER_ARRAY_STORAGE_MODE_NODE_CANONICAL,
                          items[0]->superArrayStorageMode);

    for (index = 1u; index < 4u; index++) {
        ZrCore_Value_InitAsInt(state, &input, (TZrInt64)index + 10);
        TEST_ASSERT_TRUE(ZrCore_Object_SuperArrayAddInt(
                state, &receiverValues[index], &input, &result));
        TEST_ASSERT_EQUAL_INT(ZR_SUPER_ARRAY_STORAGE_MODE_RAW_CANONICAL,
                              items[index]->superArrayStorageMode);
    }

    TEST_ASSERT_TRUE(ZrCore_Object_SuperArrayAddInt4ValuesAssumeFast(state, receivers, 99));
    TEST_ASSERT_EQUAL_INT(ZR_SUPER_ARRAY_STORAGE_MODE_NODE_CANONICAL,
                          items[0]->superArrayStorageMode);
    TEST_ASSERT_EQUAL_UINT64(2u, (UNITY_UINT64)items[0]->nodeMap.elementCount);
    TEST_ASSERT_EQUAL_INT64(99, items[0]->nodeMap.buckets[1]->value.value.nativeObject.nativeInt64);
    for (index = 1u; index < 4u; index++) {
        TEST_ASSERT_EQUAL_INT(ZR_SUPER_ARRAY_STORAGE_MODE_RAW_CANONICAL,
                              items[index]->superArrayStorageMode);
        TEST_ASSERT_EQUAL_UINT64(2u, (UNITY_UINT64)items[index]->superArrayRawIntLength);
        TEST_ASSERT_EQUAL_INT64(99, items[index]->superArrayRawIntData[1]);
    }

    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_raw_int_appends_do_not_allocate_node_pairs_until_generic_boundary);
    RUN_TEST(test_raw_int_materialization_rounds_capacity_for_non_power_of_two_length);
    RUN_TEST(test_generic_type_drift_keeps_node_storage_canonical);
    RUN_TEST(test_raw_int_storage_survives_gc_move_and_root_resolution);
    RUN_TEST(test_reflection_boundary_reads_raw_int_array_without_losing_values);
    RUN_TEST(test_int_to_object_transition_materializes_prior_raw_values);
    RUN_TEST(test_four_lane_append_supports_mixed_raw_and_node_canonical_arrays);
    return UNITY_END();
}
