#include "unity.h"

#include "harness/runtime_support.h"
#include "zr_vm_common/zr_contract_conf.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/ownership.h"

static SZrState *g_state;

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

static SZrObject *create_object(
        SZrState *state,
        const TZrChar *name,
        TZrBool resource) {
    SZrString *typeName = ZrCore_String_CreateFromNative(
            state, (TZrNativeString)name);
    SZrObjectPrototype *prototype = ZrCore_ObjectPrototype_New(
            state, typeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    SZrObject *object;

    TEST_ASSERT_NOT_NULL(prototype);
    if (resource) {
        prototype->modifierFlags |= ZR_TYPE_MODIFIER_FLAG_RESOURCE;
    }
    object = ZrCore_Object_New(state, prototype);
    TEST_ASSERT_NOT_NULL(object);
    ZrCore_Object_Init(state, object);
    return object;
}

static TZrBool collector_contains_object(
        const SZrGarbageCollector *collector,
        const SZrRawObject *expected) {
    const SZrRawObject *object;

    if (collector == ZR_NULL || expected == ZR_NULL) {
        return ZR_FALSE;
    }
    object = collector->gcObjectList;
    while (object != ZR_NULL) {
        if (object == expected) {
            return ZR_TRUE;
        }
        object = object->next;
    }
    return ZR_FALSE;
}

static void test_domain_identity_is_stable_and_rejects_cross_domain_edges(void) {
    SZrState *other = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrObject *localObject = create_object(g_state, "LocalDocument", ZR_FALSE);
    SZrObject *localTarget = create_object(g_state, "LocalTarget", ZR_FALSE);
    SZrObject *otherObject = create_object(other, "OtherDocument", ZR_FALSE);
    SZrGcDomainIdentity localIdentity = ZrCore_GcDomain_GetIdentity(g_state);
    SZrGcDomainIdentity otherIdentity = ZrCore_GcDomain_GetIdentity(other);
    SZrGcDomainIdentity objectIdentity;
    SZrGcDomainIdentity staleIdentity;
    SZrTypeValue localValue;
    SZrTypeValue crossDomainValue;
    SZrTypeValue localReceiver;
    SZrTypeValue storageKey;
    SZrTypeValue crossDomainKey;
    SZrTypeValue primitiveKey;
    SZrTypeValue primitiveValue;
    SZrString *memberName = ZrCore_String_CreateFromNative(
            g_state, "domainTarget");
    SZrString *otherKeyName = ZrCore_String_CreateFromNative(
            other, "foreignDomainKey");
    struct SZrGcDomain *attachedDomain;

    TEST_ASSERT_NOT_NULL(other);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, localIdentity.id);
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, localIdentity.generation);
    TEST_ASSERT_FALSE(ZrCore_GcDomain_IdentityEquals(
            localIdentity, otherIdentity));
    TEST_ASSERT_TRUE(ZrCore_GcDomain_GetObjectIdentity(
            ZR_CAST_RAW_OBJECT_AS_SUPER(localObject), &objectIdentity));
    TEST_ASSERT_TRUE(ZrCore_GcDomain_IdentityEquals(
            localIdentity, objectIdentity));
    TEST_ASSERT_TRUE(ZrCore_GcDomain_ObjectBelongsToState(
            g_state, ZR_CAST_RAW_OBJECT_AS_SUPER(localObject)));
    TEST_ASSERT_FALSE(ZrCore_GcDomain_ObjectBelongsToState(
            g_state, ZR_CAST_RAW_OBJECT_AS_SUPER(otherObject)));
    TEST_ASSERT_TRUE(ZrCore_GcDomain_ValidateWrite(
            g_state,
            ZR_CAST_RAW_OBJECT_AS_SUPER(localObject),
            ZR_CAST_RAW_OBJECT_AS_SUPER(localObject)));
    TEST_ASSERT_FALSE(ZrCore_GcDomain_ValidateWrite(
            g_state,
            ZR_CAST_RAW_OBJECT_AS_SUPER(localObject),
            ZR_CAST_RAW_OBJECT_AS_SUPER(otherObject)));
    ZR_GC_SET_REFERENCED(ZR_CAST_RAW_OBJECT_AS_SUPER(localObject));
    ZrCore_Value_InitAsRawObject(
            g_state, &localValue, ZR_CAST_RAW_OBJECT_AS_SUPER(localTarget));
    ZrCore_Value_Barrier(
            g_state, ZR_CAST_RAW_OBJECT_AS_SUPER(localObject), &localValue);
    TEST_ASSERT_TRUE(ZrCore_RawObject_IsMarkWaitToScan(
            ZR_CAST_RAW_OBJECT_AS_SUPER(localTarget)));
    ZrCore_Value_InitAsRawObject(
            other, &crossDomainValue, ZR_CAST_RAW_OBJECT_AS_SUPER(otherObject));
    ZrCore_Value_Barrier(
            g_state, ZR_CAST_RAW_OBJECT_AS_SUPER(localObject), &crossDomainValue);
    TEST_ASSERT_TRUE(ZrCore_RawObject_IsMarkInited(
            ZR_CAST_RAW_OBJECT_AS_SUPER(otherObject)));

    ZrCore_Value_InitAsRawObject(
            g_state, &localReceiver, ZR_CAST_RAW_OBJECT_AS_SUPER(localObject));
    ZrCore_Value_InitAsRawObject(
            g_state, &storageKey, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));
    ZrCore_Value_InitAsRawObject(
            other, &crossDomainKey, ZR_CAST_RAW_OBJECT_AS_SUPER(otherKeyName));
    ZrCore_Value_InitAsInt(g_state, &primitiveKey, 17);
    ZrCore_Value_InitAsInt(g_state, &primitiveValue, 19);
    ZrCore_Object_SetValue(g_state, localObject, &storageKey, &crossDomainValue);
    TEST_ASSERT_NULL(ZrCore_Object_GetValue(g_state, localObject, &storageKey));
    ZrCore_Object_SetValue(g_state, localObject, &crossDomainKey, &localValue);
    TEST_ASSERT_NULL(ZrCore_Object_GetValue(g_state, localObject, &crossDomainKey));
    ZrCore_Object_SetValue(g_state, otherObject, &crossDomainKey, &crossDomainValue);
    TEST_ASSERT_NULL(ZrCore_Object_GetValue(other, otherObject, &crossDomainKey));
    ZrCore_Object_SetValue(g_state, otherObject, &primitiveKey, &primitiveValue);
    TEST_ASSERT_NULL(ZrCore_Object_GetValue(other, otherObject, &primitiveKey));
    TEST_ASSERT_FALSE(ZrCore_Object_SetMember(
            g_state, &localReceiver, memberName, &crossDomainValue));
    ZrCore_Object_SetValue(g_state, localObject, &storageKey, &localValue);
    TEST_ASSERT_NOT_NULL(ZrCore_Object_GetValue(
            g_state, localObject, &storageKey));

    staleIdentity = localIdentity;
    staleIdentity.generation++;
    TEST_ASSERT_FALSE(ZrCore_GcDomain_IdentityIsCurrent(
            g_state, staleIdentity));

    attachedDomain = g_state->gcDomain;
    g_state->gcDomain = ZR_NULL;
    TEST_ASSERT_NULL(ZrCore_Object_NewCustomized(
            g_state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_OBJECT));
    g_state->gcDomain = attachedDomain;

    ZrTests_Runtime_State_Destroy(other);
}

static void test_gc_root_handle_copy_update_drop_and_stale_generation(void) {
    SZrObject *first = create_object(g_state, "FirstDocument", ZR_FALSE);
    SZrObject *second = create_object(g_state, "SecondDocument", ZR_FALSE);
    SZrGcRootHandle firstHandle;
    SZrGcRootHandle clone;
    SZrGcRootHandle stale;
    SZrGcRootHandle foreign;
    SZrRawObject *resolved = ZR_NULL;
    SZrState *other = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrObject *otherObject = create_object(other, "OtherRootTarget", ZR_FALSE);

    TEST_ASSERT_TRUE(ZrCore_GcRootHandle_Create(
            g_state, ZR_CAST_RAW_OBJECT_AS_SUPER(first), &firstHandle));
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)ZrCore_GcDomain_GetRootCount(g_state));
    TEST_ASSERT_TRUE(ZrCore_GcRootHandle_Clone(g_state, &firstHandle, &clone));
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)ZrCore_GcDomain_GetRootCount(g_state));
    TEST_ASSERT_FALSE(ZrCore_GcRootHandle_Clone(
            g_state, &firstHandle, &firstHandle));
    TEST_ASSERT_TRUE(ZrCore_GcRootHandle_Resolve(
            g_state, &firstHandle, &resolved));
    TEST_ASSERT_EQUAL_PTR(first, resolved);
    TEST_ASSERT_FALSE(ZrCore_GcRootHandle_Update(
            g_state, &clone, ZR_CAST_RAW_OBJECT_AS_SUPER(otherObject)));
    TEST_ASSERT_FALSE(ZrCore_GcRootHandle_Create(
            g_state, ZR_CAST_RAW_OBJECT_AS_SUPER(otherObject), &foreign));
    TEST_ASSERT_FALSE(ZrCore_GcRootHandle_Resolve(other, &clone, &resolved));

    TEST_ASSERT_TRUE(ZrCore_GcRootHandle_Update(
            g_state, &clone, ZR_CAST_RAW_OBJECT_AS_SUPER(second)));
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)ZrCore_GcDomain_GetRootCount(g_state));
    TEST_ASSERT_TRUE(ZrCore_GcRootHandle_Resolve(
            g_state, &firstHandle, &resolved));
    TEST_ASSERT_EQUAL_PTR(first, resolved);
    TEST_ASSERT_TRUE(ZrCore_GcRootHandle_Resolve(g_state, &clone, &resolved));
    TEST_ASSERT_EQUAL_PTR(second, resolved);

    ZrCore_GcRootHandle_Release(g_state, &firstHandle);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)ZrCore_GcDomain_GetRootCount(g_state));
    TEST_ASSERT_TRUE(ZrCore_GcRootHandle_Resolve(g_state, &clone, &resolved));
    TEST_ASSERT_EQUAL_PTR(second, resolved);

    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
    resolved = ZR_NULL;
    TEST_ASSERT_TRUE(ZrCore_GcRootHandle_Resolve(g_state, &clone, &resolved));
    TEST_ASSERT_EQUAL_PTR(second, resolved);

    stale = clone;
    stale.slotGeneration++;
    TEST_ASSERT_FALSE(ZrCore_GcRootHandle_Resolve(g_state, &stale, &resolved));
    ZrCore_GcRootHandle_Release(g_state, &clone);
    TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)ZrCore_GcDomain_GetRootCount(g_state));
    TEST_ASSERT_FALSE(ZrCore_GcRootHandle_Resolve(g_state, &clone, &resolved));
    ZrTests_Runtime_State_Destroy(other);
}

static void test_resource_unique_uses_explicit_domain_root_not_ignore_registry(void) {
    SZrObject *resource = create_object(g_state, "DomainResource", ZR_TRUE);
    SZrTypeValue owner;

    ZrCore_Value_ResetAsNull(&owner);
    TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)g_state->global->garbageCollector->ignoredObjectCount);
    TEST_ASSERT_TRUE(ZrCore_Ownership_InitUniqueValue(
            g_state, &owner, ZR_CAST_RAW_OBJECT_AS_SUPER(resource)));
    TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)g_state->global->garbageCollector->ignoredObjectCount);
    TEST_ASSERT_EQUAL_UINT32(
            1u, (TZrUInt32)ZrCore_GcDomain_GetOwnershipRootCount(g_state));
    TEST_ASSERT_TRUE(ZrCore_GcDomain_IsOwnershipRoot(
            g_state, ZR_CAST_RAW_OBJECT_AS_SUPER(resource)));

    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
    TEST_ASSERT_TRUE(ZrCore_GcDomain_IsOwnershipRoot(
            g_state, ZR_CAST_RAW_OBJECT_AS_SUPER(resource)));

    ZrCore_Ownership_ReleaseValue(g_state, &owner);
    TEST_ASSERT_EQUAL_UINT32(
            0u, (TZrUInt32)ZrCore_GcDomain_GetOwnershipRootCount(g_state));
    TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)g_state->global->garbageCollector->ignoredObjectCount);
}

static void test_gc_root_handle_survives_minor_major_and_compact_target_rewrites(void) {
    SZrGarbageCollector *collector = g_state->global->garbageCollector;
    SZrObject *object = create_object(g_state, "MovableDocument", ZR_FALSE);
    SZrGcRootHandle handle;
    SZrRawObject *resolved = ZR_NULL;

    TEST_ASSERT_TRUE(ZrCore_GcRootHandle_Create(
            g_state, ZR_CAST_RAW_OBJECT_AS_SUPER(object), &handle));
    collector->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
    collector->gcDebtSize = 4096;
    ZrCore_GarbageCollector_GcStep(g_state);
    TEST_ASSERT_TRUE(ZrCore_GcRootHandle_Resolve(g_state, &handle, &resolved));
    TEST_ASSERT_NOT_NULL(resolved);
    TEST_ASSERT_TRUE(ZrCore_GcDomain_ObjectBelongsToState(g_state, resolved));

    ZrCore_GarbageCollector_ScheduleCollection(
            g_state->global, ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAJOR);
    ZrCore_GarbageCollector_CheckGc(g_state);
    resolved = ZR_NULL;
    TEST_ASSERT_TRUE(ZrCore_GcRootHandle_Resolve(g_state, &handle, &resolved));
    TEST_ASSERT_NOT_NULL(resolved);

    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
    resolved = ZR_NULL;
    TEST_ASSERT_TRUE(ZrCore_GcRootHandle_Resolve(g_state, &handle, &resolved));
    TEST_ASSERT_NOT_NULL(resolved);
    TEST_ASSERT_TRUE(ZrCore_GcDomain_ObjectBelongsToState(g_state, resolved));
    TEST_ASSERT_EQUAL_UINT32(
            ZR_GARBAGE_COLLECT_COLLECTION_KIND_FULL,
            collector->statsSnapshot.lastCollectionKind);

    ZrCore_GcRootHandle_Release(g_state, &handle);
    TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)ZrCore_GcDomain_GetRootCount(g_state));
}

static void test_major_collection_scans_permanent_parent_children(void) {
    SZrObject *parent = create_object(g_state, "PermanentParent", ZR_FALSE);
    SZrObject *child = create_object(g_state, "PermanentChild", ZR_FALSE);
    SZrString *keyString = ZrCore_String_CreateFromNative(g_state, "child");
    SZrTypeValue key;
    SZrTypeValue childValue;
    const SZrTypeValue *storedValue;

    TEST_ASSERT_NOT_NULL(keyString);
    ZrCore_Value_InitAsRawObject(
            g_state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
    ZrCore_Value_InitAsRawObject(
            g_state, &childValue, ZR_CAST_RAW_OBJECT_AS_SUPER(child));
    ZrCore_RawObject_MarkAsPermanent(
            g_state, ZR_CAST_RAW_OBJECT_AS_SUPER(parent));
    ZrCore_Object_SetValue(g_state, parent, &key, &childValue);

    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);

    storedValue = ZrCore_Object_GetValue(g_state, parent, &key);
    TEST_ASSERT_NOT_NULL(storedValue);
    TEST_ASSERT_TRUE(storedValue->isGarbageCollectable);
    TEST_ASSERT_TRUE(collector_contains_object(
            g_state->global->garbageCollector, storedValue->value.object));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_domain_identity_is_stable_and_rejects_cross_domain_edges);
    RUN_TEST(test_gc_root_handle_copy_update_drop_and_stale_generation);
    RUN_TEST(test_resource_unique_uses_explicit_domain_root_not_ignore_registry);
    RUN_TEST(test_gc_root_handle_survives_minor_major_and_compact_target_rewrites);
    RUN_TEST(test_major_collection_scans_permanent_parent_children);
    return UNITY_END();
}
