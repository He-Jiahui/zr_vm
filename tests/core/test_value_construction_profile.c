#include <string.h>

#include "unity.h"

#include "tests/harness/runtime_support.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/profile.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/value.h"

static void enable_helper_profile(SZrState *state, SZrProfileRuntime *profileRuntime) {
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);
    TEST_ASSERT_NOT_NULL(profileRuntime);

    memset(profileRuntime, 0, sizeof(*profileRuntime));
    profileRuntime->recordHelpers = ZR_TRUE;
    state->global->profileRuntime = profileRuntime;
    ZrCore_Profile_SetCurrentState(state);
}

static void disable_helper_profile(SZrState *state) {
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    state->global->profileRuntime = ZR_NULL;
    ZrCore_Profile_SetCurrentState(ZR_NULL);
}

static void enable_memory_profile(SZrState *state, SZrProfileRuntime *profileRuntime) {
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);
    TEST_ASSERT_NOT_NULL(profileRuntime);

    memset(profileRuntime, 0, sizeof(*profileRuntime));
    profileRuntime->recordMemory = ZR_TRUE;
    state->global->profileRuntime = profileRuntime;
    ZrCore_Profile_SetCurrentState(state);
}

static void test_value_construction_helper_has_stable_profile_name(void) {
    TEST_ASSERT_EQUAL_STRING("value_construct",
                             ZrCore_Profile_HelperKindName(ZR_PROFILE_HELPER_VALUE_CONSTRUCT));
}

static void test_value_construction_helper_appends_without_renumbering_existing_helpers(void) {
    TEST_ASSERT_EQUAL_INT(0, ZR_PROFILE_HELPER_VALUE_COPY);
    TEST_ASSERT_EQUAL_INT(1, ZR_PROFILE_HELPER_VALUE_RESET_NULL);
    TEST_ASSERT_EQUAL_INT(2, ZR_PROFILE_HELPER_STACK_GET_VALUE);
    TEST_ASSERT_EQUAL_INT(3, ZR_PROFILE_HELPER_PRECALL);
    TEST_ASSERT_EQUAL_INT(4, ZR_PROFILE_HELPER_GET_MEMBER);
    TEST_ASSERT_EQUAL_INT(5, ZR_PROFILE_HELPER_SET_MEMBER);
    TEST_ASSERT_EQUAL_INT(6, ZR_PROFILE_HELPER_GET_BY_INDEX);
    TEST_ASSERT_EQUAL_INT(7, ZR_PROFILE_HELPER_SET_BY_INDEX);
    TEST_ASSERT_EQUAL_INT(8, ZR_PROFILE_HELPER_VALUE_CONSTRUCT);
}

static void test_value_construction_profile_counts_public_materialization_paths(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrObject *object;
    SZrProfileRuntime profileRuntime;
    SZrTypeValue values[8] = {0};

    TEST_ASSERT_NOT_NULL(state);
    object = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(object);

    enable_helper_profile(state, &profileRuntime);

    ZrCore_Value_InitAsRawObject(state, &values[0], ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    ZrCore_Value_InitAsUInt(state, &values[1], 42u);
    ZrCore_Value_InitAsInt(state, &values[2], -42);
    ZrCore_Value_InitAsBool(state, &values[3], ZR_TRUE);
    ZrCore_Value_InitAsFloat(state, &values[4], 42.5);
    ZrCore_Value_InitAsNativePointer(state, &values[5], (TZrPtr)object);
    ZrCore_Value_ResetAsNull(&values[6]);
    ZR_VALUE_FAST_SET(&values[7], nativeInt64, 7, ZR_VALUE_TYPE_INT64);

    TEST_ASSERT_EQUAL_UINT64(8u,
                             profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_CONSTRUCT]);
    TEST_ASSERT_EQUAL_UINT64(1u,
                             profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_RESET_NULL]);

    disable_helper_profile(state);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_value_construction_profile_stays_zero_when_helper_recording_is_disabled(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrProfileRuntime profileRuntime;
    SZrTypeValue first = {0};
    SZrTypeValue second = {0};

    TEST_ASSERT_NOT_NULL(state);
    memset(&profileRuntime, 0, sizeof(profileRuntime));
    state->global->profileRuntime = &profileRuntime;
    ZrCore_Profile_SetCurrentState(state);

    ZrCore_Value_InitAsInt(state, &first, 11);
    ZR_VALUE_FAST_SET(&second, nativeBool, ZR_TRUE, ZR_VALUE_TYPE_BOOL);

    TEST_ASSERT_EQUAL_UINT64(0u,
                             profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_CONSTRUCT]);

    disable_helper_profile(state);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_memory_metric_names_are_stable(void) {
    static const char *const expectedNames[ZR_PROFILE_MEMORY_ENUM_MAX] = {
            "allocation_count",
            "allocation_bytes",
            "value_copy_bytes",
            "write_barrier_count",
            "minor_collection_count",
            "full_collection_count",
            "mark_object_count",
            "rewrite_object_count",
            "promoted_bytes",
            "raw_int_hit_count",
            "node_map_materialization_count",
            "raw_node_sync_count",
            "member_cache_hit_count",
            "member_cache_miss_count",
            "member_cache_invalidation_count",
            "scan_bytes",
            "member_cache_monomorphic_hit_count",
            "member_cache_polymorphic_hit_count",
            "member_cache_megamorphic_hit_count",
            "member_cache_meta_fallback_count",
    };

    for (TZrUInt32 index = 0u; index < ZR_PROFILE_MEMORY_ENUM_MAX; index++) {
        TEST_ASSERT_EQUAL_STRING(expectedNames[index],
                                 ZrCore_Profile_MemoryMetricKindName((EZrProfileMemoryMetricKind)index));
    }
}

static void test_memory_metrics_accumulate_only_when_enabled(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrProfileRuntime profileRuntime;

    TEST_ASSERT_NOT_NULL(state);
    memset(&profileRuntime, 0, sizeof(profileRuntime));
    state->global->profileRuntime = &profileRuntime;
    ZrCore_Profile_SetCurrentState(state);

    ZrCore_Profile_RecordMemoryCurrent(ZR_PROFILE_MEMORY_ALLOCATION_BYTES, 17u);
    TEST_ASSERT_EQUAL_UINT64(0u,
                             profileRuntime.memoryMetricCounts[ZR_PROFILE_MEMORY_ALLOCATION_BYTES]);

    profileRuntime.recordMemory = ZR_TRUE;
    ZrCore_Profile_RecordMemoryCurrent(ZR_PROFILE_MEMORY_ALLOCATION_BYTES, 17u);
    ZrCore_Profile_RecordMemoryFromState(state, ZR_PROFILE_MEMORY_ALLOCATION_BYTES, 25u);
    TEST_ASSERT_EQUAL_UINT64(42u,
                             profileRuntime.memoryMetricCounts[ZR_PROFILE_MEMORY_ALLOCATION_BYTES]);

    disable_helper_profile(state);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_pause_samples_use_a_bounded_ring(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrProfileRuntime profileRuntime;
    TZrUInt64 expectedTotal = 0u;

    TEST_ASSERT_NOT_NULL(state);
    enable_memory_profile(state, &profileRuntime);

    for (TZrUInt32 index = 0u; index < ZR_PROFILE_PAUSE_SAMPLE_CAPACITY + 3u; index++) {
        TZrUInt64 durationUs = (TZrUInt64)index + 1u;
        ZrCore_Profile_RecordPauseFromState(state, durationUs);
        expectedTotal += durationUs;
    }

    TEST_ASSERT_EQUAL_UINT64(ZR_PROFILE_PAUSE_SAMPLE_CAPACITY + 3u,
                             profileRuntime.pauseCount);
    TEST_ASSERT_EQUAL_UINT64(expectedTotal, profileRuntime.pauseTotalUs);
    TEST_ASSERT_EQUAL_UINT64(ZR_PROFILE_PAUSE_SAMPLE_CAPACITY + 3u,
                             profileRuntime.pauseMaxUs);
    TEST_ASSERT_EQUAL_UINT32(ZR_PROFILE_PAUSE_SAMPLE_CAPACITY,
                             profileRuntime.pauseSampleCount);
    TEST_ASSERT_EQUAL_UINT32(3u, profileRuntime.pauseSampleNext);
    TEST_ASSERT_EQUAL_UINT64(ZR_PROFILE_PAUSE_SAMPLE_CAPACITY + 1u,
                             profileRuntime.pauseSamples[0]);
    TEST_ASSERT_EQUAL_UINT64(ZR_PROFILE_PAUSE_SAMPLE_CAPACITY + 3u,
                             profileRuntime.pauseSamples[2]);

    disable_helper_profile(state);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_managed_object_allocation_records_count_and_bytes(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrProfileRuntime profileRuntime;
    SZrObject *object;

    TEST_ASSERT_NOT_NULL(state);
    enable_memory_profile(state, &profileRuntime);

    object = ZrCore_Object_New(state, ZR_NULL);

    TEST_ASSERT_NOT_NULL(object);
    TEST_ASSERT_EQUAL_UINT64(1u,
                             profileRuntime.memoryMetricCounts[ZR_PROFILE_MEMORY_ALLOCATION_COUNT]);
    TEST_ASSERT_EQUAL_UINT64(sizeof(SZrObject),
                             profileRuntime.memoryMetricCounts[ZR_PROFILE_MEMORY_ALLOCATION_BYTES]);

    disable_helper_profile(state);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_value_copy_records_bytes_without_enabling_helper_counts(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrProfileRuntime profileRuntime;
    SZrTypeValue source = {0};
    SZrTypeValue destination = {0};

    TEST_ASSERT_NOT_NULL(state);
    ZrCore_Value_InitAsInt(state, &source, 42);
    enable_memory_profile(state, &profileRuntime);

    ZrCore_Value_Copy(state, &destination, &source);

    TEST_ASSERT_EQUAL_UINT64(sizeof(SZrTypeValue),
                             profileRuntime.memoryMetricCounts[ZR_PROFILE_MEMORY_VALUE_COPY_BYTES]);
    TEST_ASSERT_EQUAL_UINT64(0u,
                             profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY]);

    disable_helper_profile(state);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_bound_raw_int_get_records_hit(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrProfileRuntime profileRuntime;
    SZrObject *itemsObject;
    SZrTypeValue itemsValue = {0};
    SZrTypeValue keyValue = {0};
    SZrTypeValue resultValue = {0};

    TEST_ASSERT_NOT_NULL(state);
    itemsObject = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
    TEST_ASSERT_NOT_NULL(itemsObject);
    TEST_ASSERT_TRUE(ZrCore_Object_SuperArrayEnsureRawIntCapacity(state, itemsObject, 1u));
    itemsObject->superArrayRawIntLength = 1u;
    itemsObject->superArrayStorageMode = ZR_SUPER_ARRAY_STORAGE_MODE_RAW_CANONICAL;
    itemsObject->superArrayRawIntData[0] = 37;
    ZrCore_Value_InitAsRawObject(state, &itemsValue, ZR_CAST_RAW_OBJECT_AS_SUPER(itemsObject));
    itemsValue.type = ZR_VALUE_TYPE_ARRAY;
    ZrCore_Value_InitAsInt(state, &keyValue, 0);
    enable_memory_profile(state, &profileRuntime);

    TEST_ASSERT_TRUE(ZrCore_Object_SuperArrayGetIntBoundItems(state, &itemsValue, &keyValue, &resultValue));
    TEST_ASSERT_EQUAL_INT64(37, resultValue.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT64(1u,
                             profileRuntime.memoryMetricCounts[ZR_PROFILE_MEMORY_RAW_INT_HIT_COUNT]);

    disable_helper_profile(state);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_full_collection_records_scan_bytes(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrProfileRuntime profileRuntime;

    TEST_ASSERT_NOT_NULL(state);
    enable_memory_profile(state, &profileRuntime);

    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);

    TEST_ASSERT_GREATER_THAN_UINT64(0u,
                                    profileRuntime.memoryMetricCounts[ZR_PROFILE_MEMORY_FULL_COLLECTION_COUNT]);
    TEST_ASSERT_GREATER_THAN_UINT64(0u,
                                    profileRuntime.memoryMetricCounts[ZR_PROFILE_MEMORY_SCAN_BYTES]);

    disable_helper_profile(state);
    ZrTests_Runtime_State_Destroy(state);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_value_construction_helper_has_stable_profile_name);
    RUN_TEST(test_value_construction_helper_appends_without_renumbering_existing_helpers);
    RUN_TEST(test_value_construction_profile_counts_public_materialization_paths);
    RUN_TEST(test_value_construction_profile_stays_zero_when_helper_recording_is_disabled);
    RUN_TEST(test_memory_metric_names_are_stable);
    RUN_TEST(test_memory_metrics_accumulate_only_when_enabled);
    RUN_TEST(test_pause_samples_use_a_bounded_ring);
    RUN_TEST(test_managed_object_allocation_records_count_and_bytes);
    RUN_TEST(test_value_copy_records_bytes_without_enabling_helper_counts);
    RUN_TEST(test_bound_raw_int_get_records_hit);
    RUN_TEST(test_full_collection_records_scan_bytes);
    return UNITY_END();
}
