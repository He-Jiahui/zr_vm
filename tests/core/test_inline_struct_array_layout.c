#include "unity.h"

#include <string.h>

#include "tests/harness/runtime_support.h"
#include "zr_vm_core/function.h"
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

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_inline_struct_array_uses_registry_layout_for_gc_and_drop);
    return UNITY_END();
}
