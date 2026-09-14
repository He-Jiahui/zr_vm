#include "unity.h"

#include "harness/runtime_support.h"
#include "zr_vm_core/gc_domain_clone.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

static SZrState *g_source_state;
static SZrState *g_target_state;

static SZrObject *new_plain_object(SZrState *state) {
    SZrObject *object = ZrCore_Object_NewCustomized(
            state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_OBJECT);
    TEST_ASSERT_NOT_NULL(object);
    ZrCore_Object_Init(state, object);
    return object;
}

static void init_object_value(
        SZrState *state, SZrObject *object, SZrTypeValue *value) {
    ZrCore_Value_InitAsRawObject(
            state, value, ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    value->type = ZR_VALUE_TYPE_OBJECT;
}

static void set_member(
        SZrState *state,
        SZrObject *object,
        TZrNativeString name,
        const SZrTypeValue *value) {
    SZrString *nameString = ZrCore_String_CreateFromNative(state, name);
    SZrTypeValue key;

    TEST_ASSERT_NOT_NULL(nameString);
    ZrCore_Value_InitAsRawObject(
            state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(nameString));
    key.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Object_SetValue(state, object, &key, value);
}

static const SZrTypeValue *get_member(
        SZrState *state, SZrObject *object, TZrNativeString name) {
    SZrString *nameString = ZrCore_String_CreateFromNative(state, name);
    SZrTypeValue key;

    TEST_ASSERT_NOT_NULL(nameString);
    ZrCore_Value_InitAsRawObject(
            state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(nameString));
    key.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Object_GetValue(state, object, &key);
}

static SZrDomainTransferQuota clone_quota(void) {
    SZrDomainTransferQuota quota = {0};
    quota.maxObjects = 16u;
    quota.maxBytes = 4096u;
    quota.maxDepth = 32u;
    return quota;
}

void setUp(void) {
    g_source_state = ZrTests_Runtime_State_Create(ZR_NULL);
    g_target_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_source_state);
    TEST_ASSERT_NOT_NULL(g_target_state);
}

void tearDown(void) {
    if (g_target_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_target_state);
        g_target_state = ZR_NULL;
    }
    if (g_source_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_source_state);
        g_source_state = ZR_NULL;
    }
}

static void test_clone_preserves_cycle_alias_and_survives_source_destroy(void) {
    SZrObject *root = new_plain_object(g_source_state);
    SZrObject *child = new_plain_object(g_source_state);
    SZrTypeValue rootValue;
    SZrTypeValue childValue;
    SZrTypeValue targetValue;
    SZrDomainTransferDiagnostic diagnostic;
    SZrDomainTransferQuota quota = clone_quota();
    SZrGcDomainCloneTransaction *transaction;
    SZrGcRootHandle targetRootHandle;
    SZrOwnershipTransferSnapshot snapshot;
    SZrObject *targetRoot;
    const SZrTypeValue *left;
    const SZrTypeValue *right;
    const SZrTypeValue *parent;

    init_object_value(g_source_state, root, &rootValue);
    init_object_value(g_source_state, child, &childValue);
    set_member(g_source_state, root, "left", &childValue);
    set_member(g_source_state, root, "right", &childValue);
    set_member(g_source_state, child, "parent", &rootValue);

    transaction = ZrCore_GcDomainClone_Prepare(
            g_source_state,
            g_target_state,
            &rootValue,
            &quota,
            &diagnostic);
    TEST_ASSERT_NOT_NULL(transaction);
    TEST_ASSERT_EQUAL_INT(ZR_DOMAIN_TRANSFER_STATUS_OK, diagnostic.status);
    TEST_ASSERT_TRUE(ZrCore_GcDomainClone_Publish(transaction, &diagnostic));
    TEST_ASSERT_TRUE(ZrCore_GcDomainClone_Claim(
            transaction, 17u, 19u, &diagnostic));
    ZrCore_Value_ResetAsNull(&targetValue);
    TEST_ASSERT_TRUE(ZrCore_GcDomainClone_Commit(
            transaction, &targetValue, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_DOMAIN_TRANSFER_STATUS_OK, diagnostic.status);
    TEST_ASSERT_TRUE(ZrCore_GcDomainClone_GetSnapshot(
            transaction, &snapshot));
    TEST_ASSERT_EQUAL_INT(
            ZR_OWNERSHIP_TRANSFER_STATE_COMMITTED, snapshot.state);

    targetRoot = ZR_CAST_OBJECT(g_target_state, targetValue.value.object);
    TEST_ASSERT_NOT_NULL(targetRoot);
    TEST_ASSERT_NOT_EQUAL(root, targetRoot);
    left = get_member(g_target_state, targetRoot, "left");
    right = get_member(g_target_state, targetRoot, "right");
    TEST_ASSERT_NOT_NULL(left);
    TEST_ASSERT_NOT_NULL(right);
    TEST_ASSERT_EQUAL_PTR(left->value.object, right->value.object);
    parent = get_member(
            g_target_state, ZR_CAST_OBJECT(g_target_state, left->value.object),
            "parent");
    TEST_ASSERT_NOT_NULL(parent);
    TEST_ASSERT_EQUAL_PTR(targetRoot, parent->value.object);

    TEST_ASSERT_TRUE(ZrCore_GcRootHandle_Create(
            g_target_state,
            ZR_CAST_RAW_OBJECT_AS_SUPER(targetRoot),
            &targetRootHandle));

    /* The clone owns only target-domain objects; source teardown is safe. */
    ZrTests_Runtime_State_Destroy(g_source_state);
    g_source_state = ZR_NULL;
    TEST_ASSERT_EQUAL_UINT32(
            1u, (TZrUInt32)ZrCore_GcDomain_GetRootCount(g_target_state));
    {
        SZrRawObject *resolvedRoot = ZR_NULL;
        TEST_ASSERT_TRUE(ZrCore_GcRootHandle_Resolve(
                g_target_state, &targetRootHandle, &resolvedRoot));
        TEST_ASSERT_EQUAL_PTR(targetRoot, resolvedRoot);
    }
    TEST_ASSERT_EQUAL_PTR(targetRoot, targetValue.value.object);
    ZrCore_GcRootHandle_Release(g_target_state, &targetRootHandle);
    ZrCore_Value_ResetAsNull(&targetValue);
    ZrCore_GcDomainClone_Free(transaction);
}

static void test_clone_quota_rejects_before_target_allocation_and_keeps_source(void) {
    SZrObject *root = new_plain_object(g_source_state);
    SZrObject *child = new_plain_object(g_source_state);
    SZrTypeValue rootValue;
    SZrTypeValue childValue;
    SZrDomainTransferDiagnostic diagnostic;
    SZrDomainTransferQuota quota = clone_quota();
    SZrGcDomainCloneTransaction *transaction;

    init_object_value(g_source_state, root, &rootValue);
    init_object_value(g_source_state, child, &childValue);
    set_member(g_source_state, root, "child", &childValue);
    quota.maxObjects = 1u;
    transaction = ZrCore_GcDomainClone_Prepare(
            g_source_state,
            g_target_state,
            &rootValue,
            &quota,
            &diagnostic);
    TEST_ASSERT_NULL(transaction);
    TEST_ASSERT_EQUAL_INT(
            ZR_DOMAIN_TRANSFER_STATUS_OBJECT_QUOTA, diagnostic.status);
    TEST_ASSERT_EQUAL_PTR(root, rootValue.value.object);
    TEST_ASSERT_EQUAL_UINT32(
            0u, (TZrUInt32)ZrCore_GcDomain_GetRootCount(g_target_state));
}

static void test_claimed_clone_can_abort_after_target_generation_stales(void) {
    SZrObject *root = new_plain_object(g_source_state);
    SZrTypeValue rootValue;
    SZrDomainTransferDiagnostic diagnostic;
    SZrDomainTransferQuota quota = clone_quota();
    SZrGcDomainCloneTransaction *transaction;
    SZrOwnershipTransferSnapshot snapshot;

    init_object_value(g_source_state, root, &rootValue);
    transaction = ZrCore_GcDomainClone_Prepare(
            g_source_state,
            g_target_state,
            &rootValue,
            &quota,
            &diagnostic);
    TEST_ASSERT_NOT_NULL(transaction);
    TEST_ASSERT_TRUE(ZrCore_GcDomainClone_Publish(transaction, &diagnostic));
    TEST_ASSERT_TRUE(ZrCore_GcDomainClone_Claim(
            transaction, 23u, 29u, &diagnostic));
    ZrTests_Runtime_State_Destroy(g_target_state);
    g_target_state = ZR_NULL;

    TEST_ASSERT_TRUE(ZrCore_GcDomainClone_Abort(transaction, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_DOMAIN_TRANSFER_STATUS_OK, diagnostic.status);
    TEST_ASSERT_TRUE(ZrCore_GcDomainClone_GetSnapshot(transaction, &snapshot));
    TEST_ASSERT_EQUAL_INT(
            ZR_OWNERSHIP_TRANSFER_STATE_ABORTED, snapshot.state);
    TEST_ASSERT_EQUAL_PTR(root, rootValue.value.object);
    ZrCore_GcDomainClone_Free(transaction);
}

static void test_clone_rejects_same_domain_without_touching_source(void) {
    SZrObject *root = new_plain_object(g_source_state);
    SZrTypeValue rootValue;
    SZrDomainTransferDiagnostic diagnostic;
    SZrDomainTransferQuota quota = clone_quota();

    init_object_value(g_source_state, root, &rootValue);
    TEST_ASSERT_NULL(ZrCore_GcDomainClone_Prepare(
            g_source_state,
            g_source_state,
            &rootValue,
            &quota,
            &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_DOMAIN_TRANSFER_STATUS_DOMAIN_MISMATCH, diagnostic.status);
    TEST_ASSERT_EQUAL_PTR(root, rootValue.value.object);
}

static void test_clone_execute_facade_closes_transaction(void) {
    SZrTypeValue source;
    SZrTypeValue target;
    SZrDomainTransferDiagnostic diagnostic;
    SZrDomainTransferQuota quota = clone_quota();

    ZrCore_Value_InitAsInt(g_source_state, &source, 8080);
    ZrCore_Value_ResetAsNull(&target);
    TEST_ASSERT_TRUE(ZrCore_GcDomainClone_Execute(
            g_source_state,
            g_target_state,
            &source,
            &quota,
            41u,
            43u,
            &target,
            &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_DOMAIN_TRANSFER_STATUS_OK, diagnostic.status);
    TEST_ASSERT_EQUAL_INT64(8080, target.value.nativeObject.nativeInt64);
    ZrTests_Runtime_State_Destroy(g_source_state);
    g_source_state = ZR_NULL;
    TEST_ASSERT_EQUAL_INT64(8080, target.value.nativeObject.nativeInt64);
    ZrCore_Value_ResetAsNull(&target);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_clone_preserves_cycle_alias_and_survives_source_destroy);
    RUN_TEST(test_clone_quota_rejects_before_target_allocation_and_keeps_source);
    RUN_TEST(test_claimed_clone_can_abort_after_target_generation_stales);
    RUN_TEST(test_clone_rejects_same_domain_without_touching_source);
    RUN_TEST(test_clone_execute_facade_closes_transaction);
    return UNITY_END();
}
