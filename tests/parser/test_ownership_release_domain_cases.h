#ifndef ZR_TEST_OWNERSHIP_RELEASE_DOMAIN_CASES_H
#define ZR_TEST_OWNERSHIP_RELEASE_DOMAIN_CASES_H

static void release_domain_create_shared(SZrTypeValue *shared) {
    SZrTypeValue unique;
    init_direct_unique(create_resource_object(), &unique);
    ZrCore_Value_ResetAsNull(shared);
    TEST_ASSERT_TRUE(ZrCore_Ownership_ShareValue(g_state, shared, &unique));
}

static void test_shared_foreign_release_preserves_owner(void) {
    SZrState *otherState = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrTypeValue shared;
    SZrOwnershipControl *control;
    SZrRawObject *object;
    TEST_ASSERT_NOT_NULL(otherState);
    release_domain_create_shared(&shared);
    control = shared.ownershipControl;
    object = shared.value.object;
    ZrCore_Ownership_ReleaseValue(otherState, &shared);
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_VALUE_KIND_SHARED, shared.ownershipKind);
    TEST_ASSERT_EQUAL_PTR(control, shared.ownershipControl);
    TEST_ASSERT_EQUAL_PTR(object, shared.value.object);
    TEST_ASSERT_EQUAL_UINT32(1u, control->strongRefCount);
    TEST_ASSERT_EQUAL_UINT32(1u, control->weakRefCount);
    TEST_ASSERT_TRUE(control->objectIsAlive);
    ZrCore_Ownership_ReleaseValue(g_state, &shared);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(shared.type));
    ZrTests_Runtime_State_Destroy(otherState);
}

static void test_weak_foreign_release_preserves_live_and_expired_handle(void) {
    SZrState *otherState = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrTypeValue shared;
    SZrTypeValue weak;
    SZrOwnershipControl *control;
    TEST_ASSERT_NOT_NULL(otherState);
    release_domain_create_shared(&shared);
    ZrCore_Value_ResetAsNull(&weak);
    TEST_ASSERT_TRUE(ZrCore_Ownership_DegradeValue(g_state, &weak, &shared));
    control = weak.ownershipControl;
    for (TZrSize expired = 0u; expired < 2u; ++expired) {
        ZrCore_Ownership_ReleaseValue(otherState, &weak);
        TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_VALUE_KIND_WEAK, weak.ownershipKind);
        TEST_ASSERT_EQUAL_PTR(control, weak.ownershipControl);
        TEST_ASSERT_EQUAL_UINT32(1u - expired, control->strongRefCount);
        TEST_ASSERT_EQUAL_UINT32(2u - expired, control->weakRefCount);
        if (expired == 0u) {
            ZrCore_Ownership_ReleaseValue(g_state, &shared);
        }
    }
    ZrCore_Ownership_ReleaseValue(g_state, &weak);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(weak.type));
    ZrTests_Runtime_State_Destroy(otherState);
}

#endif
