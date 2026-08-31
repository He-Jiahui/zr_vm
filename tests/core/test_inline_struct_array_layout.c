#include "unity.h"

#include <string.h>

#include "tests/harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/type_layout.h"

typedef struct TestInlineArrayDropRecord {
    const TZrByte *elementAddresses[2];
    TZrUInt32 order[2];
    TZrUInt32 count;
} TestInlineArrayDropRecord;

typedef struct TestInlineArrayGcVisit {
    SZrTypeValue *values[2];
    TZrUInt32 count;
} TestInlineArrayGcVisit;

static SZrTypeValue gExternalTracedValue;
static TZrUInt32 gExternalTraceCount;
static TZrUInt32 gExternalFinalizeCount;

void setUp(void) {}

void tearDown(void) {}

static void test_inline_array_drop(
        SZrState *state,
        TZrPtr storage,
        TZrPtr userData) {
    TestInlineArrayDropRecord *record = (TestInlineArrayDropRecord *)userData;
    ZR_UNUSED_PARAMETER(state);

    if (record == ZR_NULL || storage == ZR_NULL ||
        record->count >= ZR_ARRAY_COUNT(record->order)) {
        return;
    }
    for (TZrUInt32 index = 0u; index < ZR_ARRAY_COUNT(record->elementAddresses); index++) {
        if ((const TZrByte *)storage == record->elementAddresses[index]) {
            record->order[record->count++] = index;
            return;
        }
    }
}

static void test_inline_array_visit_gc_value(
        SZrState *state,
        SZrTypeValue *value,
        TZrPtr userData) {
    TestInlineArrayGcVisit *visit = (TestInlineArrayGcVisit *)userData;
    ZR_UNUSED_PARAMETER(state);

    if (visit == ZR_NULL || value == ZR_NULL ||
        visit->count >= ZR_ARRAY_COUNT(visit->values)) {
        return;
    }
    visit->values[visit->count++] = value;
}

static void test_inline_struct_array_skips_verified_non_gc_layout(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction function;
    SZrTypeLayout layout;
    SZrTypeLayout valueLayout;
    SZrTypeLayout invalidLayout;
    SZrTypeLayoutField nestedField;
    SZrTypeLayout nestedLayout;
    const SZrTypeLayout *layouts[1];
    SZrAotCodeRegistration registration;
    SZrObject *array;
    TestInlineArrayGcVisit visit;

    TEST_ASSERT_NOT_NULL(state);
    memset(&function, 0, sizeof(function));
    memset(&registration, 0, sizeof(registration));
    memset(&visit, 0, sizeof(visit));

    ZrCore_TypeLayout_InitStruct(
            &layout,
            (TZrUInt32)sizeof(TZrUInt32),
            (TZrUInt32)ZR_ALIGN_SIZE,
            ZR_TYPE_LAYOUT_COPY_KIND_BITWISE,
            ZR_TYPE_LAYOUT_DROP_KIND_NONE,
            ZR_NULL,
            0u);
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_Validate(&layout));
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_CanSkipGcScan(&layout));

    ZrCore_TypeLayout_InitValue(&valueLayout);
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_Validate(&valueLayout));
    TEST_ASSERT_FALSE(ZrCore_TypeLayout_CanSkipGcScan(&valueLayout));

    invalidLayout = layout;
    invalidLayout.layoutHash ^= UINT64_C(1);
    TEST_ASSERT_FALSE(ZrCore_TypeLayout_CanSkipGcScan(&invalidLayout));

    memset(&nestedField, 0, sizeof(nestedField));
    nestedField.byteSize = (TZrUInt32)sizeof(TZrUInt32);
    nestedField.flags = ZR_TYPE_LAYOUT_FIELD_FLAG_NESTED_LAYOUT;
    ZrCore_TypeLayout_InitStruct(
            &nestedLayout,
            (TZrUInt32)sizeof(TZrUInt32),
            (TZrUInt32)ZR_ALIGN_SIZE,
            ZR_TYPE_LAYOUT_COPY_KIND_FIELDWISE,
            ZR_TYPE_LAYOUT_DROP_KIND_NONE,
            &nestedField,
            1u);
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_Validate(&nestedLayout));
    TEST_ASSERT_FALSE(ZrCore_TypeLayout_CanSkipGcScan(&nestedLayout));

    layouts[0] = &layout;
    registration.typeLayouts = layouts;
    registration.typeLayoutCount = ZR_ARRAY_COUNT(layouts);
    function.metadataCodeRegistration = &registration;
    function.metadataTypeLayoutCount = ZR_ARRAY_COUNT(layouts);

    array = ZrCore_Object_NewInlineArray(state, &function, 0u, 1024u);
    TEST_ASSERT_NOT_NULL(array);
    {
        TZrUInt32 length = array->inlineArrayLength;
        array->inlineArrayLength = UINT32_MAX;
        TEST_ASSERT_FALSE(ZrCore_Object_VisitInlineArrayGcValues(
                state, array, test_inline_array_visit_gc_value, &visit));
        array->inlineArrayLength = length;
    }
    TEST_ASSERT_TRUE(ZrCore_Object_VisitInlineArrayGcValues(
            state, array, test_inline_array_visit_gc_value, &visit));
    TEST_ASSERT_EQUAL_UINT32(0u, visit.count);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_external_storage_trace(
        SZrState *state,
        SZrRawObject *owner,
        FZrRawObjectGcValueVisitor visitor,
        TZrPtr userData) {
    ZR_UNUSED_PARAMETER(owner);

    gExternalTraceCount++;
    visitor(state, &gExternalTracedValue, userData);
}

static void test_external_storage_finalize(
        SZrState *state,
        SZrRawObject *owner) {
    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(owner);
    gExternalFinalizeCount++;
}

static void test_inline_struct_array_uses_registry_layout_for_gc_and_drop(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction function;
    SZrTypeLayoutField field;
    SZrTypeLayoutContract contract;
    SZrTypeLayout layout;
    const SZrTypeLayout *layouts[1];
    SZrAotCodeRegistration registration;
    SZrObject *array;
    TZrUInt32 gcOffset = 0u;
    TZrUInt32 ownershipOffset = 0u;
    TZrUInt32 offsets[2];
    TestInlineArrayDropRecord dropRecord;
    TestInlineArrayGcVisit visit;

    TEST_ASSERT_NOT_NULL(state);
    memset(&function, 0, sizeof(function));
    memset(&field, 0, sizeof(field));
    memset(&contract, 0, sizeof(contract));
    memset(&registration, 0, sizeof(registration));
    memset(&dropRecord, 0, sizeof(dropRecord));
    memset(&visit, 0, sizeof(visit));

    field.byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    field.flags = ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT |
                  ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE |
                  ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE;
    contract.gcScanKind = ZR_TYPE_LAYOUT_GC_SCAN_MAPPED;
    contract.gcFieldOffsets = &gcOffset;
    contract.gcFieldCount = 1u;
    contract.ownershipFieldOffsets = &ownershipOffset;
    contract.ownershipFieldCount = 1u;
    contract.customDrop = test_inline_array_drop;
    contract.customDropUserData = &dropRecord;
    ZrCore_TypeLayout_InitStructWithContract(
            &layout,
            (TZrUInt32)sizeof(SZrTypeValue),
            (TZrUInt32)ZR_ALIGN_SIZE,
            ZR_TYPE_LAYOUT_COPY_KIND_FIELDWISE,
            ZR_TYPE_LAYOUT_DROP_KIND_CUSTOM_THEN_FIELDS,
            &field,
            1u,
            &contract);
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_Validate(&layout));

    layouts[0] = &layout;
    registration.typeLayouts = layouts;
    registration.typeLayoutCount = ZR_ARRAY_COUNT(layouts);
    function.metadataCodeRegistration = &registration;
    function.metadataTypeLayoutCount = ZR_ARRAY_COUNT(layouts);

    array = ZrCore_Object_NewInlineArray(state, &function, 0u, 2u);
    TEST_ASSERT_NOT_NULL(array);
    TEST_ASSERT_EQUAL_UINT32(0u, array->nodeMap.elementCount);
    TEST_ASSERT_TRUE(ZrCore_Object_TryGetInlineArrayElementOffset(
            state, array, &function, 0u, 0, &offsets[0]));
    TEST_ASSERT_TRUE(ZrCore_Object_TryGetInlineArrayElementOffset(
            state, array, &function, 0u, 1, &offsets[1]));
    TEST_ASSERT_EQUAL_UINT32(layout.byteSize, offsets[1] - offsets[0]);
    TEST_ASSERT_EQUAL_UINT32(
            0u,
            (TZrUInt32)((uintptr_t)((TZrByte *)array + offsets[0]) %
                        layout.byteAlign));

    dropRecord.elementAddresses[0] = (const TZrByte *)array + offsets[0];
    dropRecord.elementAddresses[1] = (const TZrByte *)array + offsets[1];
    ZrCore_Value_InitAsInt(
            state, (SZrTypeValue *)dropRecord.elementAddresses[0], 11);
    ZrCore_Value_InitAsInt(
            state, (SZrTypeValue *)dropRecord.elementAddresses[1], 22);

    TEST_ASSERT_TRUE(ZrCore_Object_VisitInlineArrayGcValues(
            state, array, test_inline_array_visit_gc_value, &visit));
    TEST_ASSERT_EQUAL_UINT32(2u, visit.count);
    TEST_ASSERT_EQUAL_PTR(dropRecord.elementAddresses[0], visit.values[0]);
    TEST_ASSERT_EQUAL_PTR(dropRecord.elementAddresses[1], visit.values[1]);

    TEST_ASSERT_TRUE(ZrCore_Object_DropInlineArrayElements(state, array));
    TEST_ASSERT_EQUAL_UINT32(2u, dropRecord.count);
    TEST_ASSERT_EQUAL_UINT32(1u, dropRecord.order[0]);
    TEST_ASSERT_EQUAL_UINT32(0u, dropRecord.order[1]);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NULL, visit.values[0]->type);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NULL, visit.values[1]->type);
    TEST_ASSERT_TRUE(ZrCore_Object_DropInlineArrayElements(state, array));
    TEST_ASSERT_EQUAL_UINT32(2u, dropRecord.count);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_external_closed_storage_trace_survives_full_compaction(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrObject *owner;
    SZrObject *child;
    SZrGcRootHandle ownerRoot;
    SZrRawObject *resolvedOwner = ZR_NULL;
    SZrRawObject *resolvedChild;
    SZrObject *youngChild;

    TEST_ASSERT_NOT_NULL(state);
    owner = ZrCore_Object_New(state, ZR_NULL);
    child = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(owner);
    TEST_ASSERT_NOT_NULL(child);
    ZrCore_Object_Init(state, owner);
    ZrCore_Object_Init(state, child);
    TEST_ASSERT_NULL(owner->super.finalizerData);
    TEST_ASSERT_NULL(child->super.finalizerData);
    ZrCore_Value_InitAsRawObject(
            state,
            &gExternalTracedValue,
            ZR_CAST_RAW_OBJECT_AS_SUPER(child));
    gExternalTracedValue.type = ZR_VALUE_TYPE_OBJECT;
    gExternalTraceCount = 0u;
    gExternalFinalizeCount = 0u;
    owner->super.traceGcFunction = test_external_storage_trace;
    owner->super.scanMarkGcFunction = test_external_storage_finalize;
    TEST_ASSERT_TRUE(ZrCore_GcRootHandle_Create(
            state, ZR_CAST_RAW_OBJECT_AS_SUPER(owner), &ownerRoot));

    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);

    TEST_ASSERT_TRUE(ZrCore_GcRootHandle_Resolve(
            state, &ownerRoot, &resolvedOwner));
    TEST_ASSERT_NOT_NULL(resolvedOwner);
    resolvedChild = ZrCore_Value_GetRawObject(&gExternalTracedValue);
    TEST_ASSERT_NOT_NULL(resolvedChild);
    TEST_ASSERT_TRUE(ZrCore_GcDomain_ObjectBelongsToState(
            state, resolvedChild));
    TEST_ASSERT_GREATER_THAN_UINT32(0u, gExternalTraceCount);
    TEST_ASSERT_EQUAL_UINT32(0u, gExternalFinalizeCount);

    state->global->garbageCollector->gcMode =
            ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
    youngChild = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(youngChild);
    ZrCore_Object_Init(state, youngChild);
    ZrCore_Value_InitAsRawObject(
            state,
            &gExternalTracedValue,
            ZR_CAST_RAW_OBJECT_AS_SUPER(youngChild));
    gExternalTracedValue.type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_GarbageCollector_Barrier(
            state,
            resolvedOwner,
            ZR_CAST_RAW_OBJECT_AS_SUPER(youngChild));
    ZrCore_GarbageCollector_ScheduleCollection(
            state->global, ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR);
    ZrCore_GarbageCollector_GcStep(state);

    resolvedChild = ZrCore_Value_GetRawObject(&gExternalTracedValue);
    TEST_ASSERT_NOT_NULL(resolvedChild);
    TEST_ASSERT_FALSE(ZrCore_RawObject_IsReleased(resolvedChild));
    TEST_ASSERT_TRUE(ZrCore_GcDomain_ObjectBelongsToState(
            state, resolvedChild));

    ZrCore_GcRootHandle_Release(state, &ownerRoot);
    ZrCore_Value_ResetAsNull(&gExternalTracedValue);
    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    TEST_ASSERT_EQUAL_UINT32(1u, gExternalFinalizeCount);

    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_inline_struct_array_skips_verified_non_gc_layout);
    RUN_TEST(test_inline_struct_array_uses_registry_layout_for_gc_and_drop);
    RUN_TEST(test_external_closed_storage_trace_survives_full_compaction);
    return UNITY_END();
}
