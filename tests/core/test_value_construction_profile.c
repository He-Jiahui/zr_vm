#include <string.h>

#include "unity.h"

#include "tests/harness/runtime_support.h"
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

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_value_construction_helper_has_stable_profile_name);
    RUN_TEST(test_value_construction_helper_appends_without_renumbering_existing_helpers);
    RUN_TEST(test_value_construction_profile_counts_public_materialization_paths);
    RUN_TEST(test_value_construction_profile_stays_zero_when_helper_recording_is_disabled);
    return UNITY_END();
}
