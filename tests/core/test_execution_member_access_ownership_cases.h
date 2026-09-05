#ifndef ZR_TEST_EXECUTION_MEMBER_ACCESS_OWNERSHIP_CASES_H
#define ZR_TEST_EXECUTION_MEMBER_ACCESS_OWNERSHIP_CASES_H

typedef enum EZrMemberAliasCachePath {
    ZR_TEST_MEMBER_ALIAS_CACHE_MISS,
    ZR_TEST_MEMBER_ALIAS_CACHE_EXACT_PAIR,
    ZR_TEST_MEMBER_ALIAS_CACHE_MULTI_SLOT
} EZrMemberAliasCachePath;

static void assert_member_get_alias_transfers_owned_result(EZrMemberAliasCachePath cachePath,
                                                          TZrBool weakResult) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMemberAccessFixture fixture;
    SZrFunction *runtimeFunction;
    SZrObject *storedObject;
    SZrHashKeyValuePair *storedPair;
    SZrOwnershipControl *control;
    SZrTypeValue sharedValue;
    SZrTypeValue weakValue;
    SZrTypeValue *sharedSlot;

    TEST_ASSERT_NOT_NULL(state);
    init_member_access_fixture(state, &fixture, 73);
    storedObject = create_plain_member_value_object(state);
    storedPair = set_member_access_fixture_object_value(state, &fixture, storedObject);
    ZrCore_Value_ResetAsNull(&sharedValue);
    ZrCore_Value_ResetAsNull(&weakValue);
    TEST_ASSERT_TRUE(ZrCore_Ownership_SharePlainValue(state, &sharedValue, &storedPair->value));
    control = sharedValue.ownershipControl;
    TEST_ASSERT_NOT_NULL(control);
    /* A live strong owner keeps one implicit weak reference to its control. */
    TEST_ASSERT_EQUAL_UINT32(1u, control->weakRefCount);
    if (weakResult) {
        TEST_ASSERT_TRUE(ZrCore_Ownership_DegradeValue(state, &weakValue, &sharedValue));
        ZrCore_Value_Copy(state, &storedPair->value, &weakValue);
        ZrCore_Ownership_ReleaseValue(state, &weakValue);
        TEST_ASSERT_EQUAL_UINT32(2u, control->weakRefCount);
    } else {
        ZrCore_Value_Copy(state, &storedPair->value, &sharedValue);
        TEST_ASSERT_EQUAL_UINT32(2u, control->strongRefCount);
    }

    fixture.cacheEntry.picSlots[0].cachedAccessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD;
    fixture.cacheEntry.picSlots[0].cachedReceiverObject = fixture.instance;
    fixture.cacheEntry.picSlots[0].cachedReceiverPair = storedPair;
    fixture.cacheEntry.picSlots[0].cachedMemberName = fixture.memberName;
    if (cachePath == ZR_TEST_MEMBER_ALIAS_CACHE_MISS) {
        fixture.cacheEntry.picSlotCount = 0;
        memset(fixture.cacheEntry.picSlots, 0, sizeof(fixture.cacheEntry.picSlots));
    } else if (cachePath == ZR_TEST_MEMBER_ALIAS_CACHE_MULTI_SLOT) {
        fixture.cacheEntry.picSlotCount = 2;
    }
    runtimeFunction = create_runtime_fixture_function(state, &fixture);
    sharedSlot = reserve_stack_result_slot(state);
    ZrCore_Value_Copy(state, sharedSlot, &fixture.receiverValue);

    TEST_ASSERT_TRUE(execution_member_get_cached(state, ZR_NULL, runtimeFunction, 0, sharedSlot, sharedSlot));
    TEST_ASSERT_EQUAL_PTR(control, sharedSlot->ownershipControl);
    TEST_ASSERT_EQUAL_INT(weakResult ? ZR_OWNERSHIP_VALUE_KIND_WEAK : ZR_OWNERSHIP_VALUE_KIND_SHARED,
                          sharedSlot->ownershipKind);
    TEST_ASSERT_EQUAL_UINT32(cachePath == ZR_TEST_MEMBER_ALIAS_CACHE_MISS ? 1u : 0u,
                             fixture.cacheEntry.runtimeMissCount);
    TEST_ASSERT_EQUAL_UINT32(cachePath == ZR_TEST_MEMBER_ALIAS_CACHE_MISS ? 0u : 1u,
                             fixture.cacheEntry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(3u,
                             weakResult ? control->weakRefCount : control->strongRefCount);

    ZrCore_Ownership_ReleaseValue(state, sharedSlot);
    TEST_ASSERT_EQUAL_UINT32(2u,
                             weakResult ? control->weakRefCount : control->strongRefCount);
    ZrCore_Ownership_ReleaseValue(state, &storedPair->value);
    TEST_ASSERT_EQUAL_UINT32(1u, control->weakRefCount);
    TEST_ASSERT_EQUAL_UINT32(1u, control->strongRefCount);
    ZrCore_Ownership_ReleaseValue(state, &sharedValue);
    detach_runtime_fixture_function(runtimeFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_member_get_cached_alias_shared_miss_transfers_temporary(void) {
    assert_member_get_alias_transfers_owned_result(ZR_TEST_MEMBER_ALIAS_CACHE_MISS, ZR_FALSE);
}

static void test_member_get_cached_alias_shared_exact_pair_transfers_temporary(void) {
    assert_member_get_alias_transfers_owned_result(ZR_TEST_MEMBER_ALIAS_CACHE_EXACT_PAIR, ZR_FALSE);
}

static void test_member_get_cached_alias_shared_multi_slot_transfers_temporary(void) {
    assert_member_get_alias_transfers_owned_result(ZR_TEST_MEMBER_ALIAS_CACHE_MULTI_SLOT, ZR_FALSE);
}

static void test_member_get_cached_alias_weak_miss_transfers_temporary(void) {
    assert_member_get_alias_transfers_owned_result(ZR_TEST_MEMBER_ALIAS_CACHE_MISS, ZR_TRUE);
}

static void test_member_get_cached_alias_weak_exact_pair_transfers_temporary(void) {
    assert_member_get_alias_transfers_owned_result(ZR_TEST_MEMBER_ALIAS_CACHE_EXACT_PAIR, ZR_TRUE);
}

static void test_member_get_cached_alias_weak_multi_slot_transfers_temporary(void) {
    assert_member_get_alias_transfers_owned_result(ZR_TEST_MEMBER_ALIAS_CACHE_MULTI_SLOT, ZR_TRUE);
}

#endif
