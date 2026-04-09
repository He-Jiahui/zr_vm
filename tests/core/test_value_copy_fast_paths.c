#include "unity.h"

#include "tests/harness/runtime_support.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

void setUp(void) {}

void tearDown(void) {}

static void test_value_copy_reuses_plain_string_object(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *text;
    SZrTypeValue source;
    SZrTypeValue destination;

    TEST_ASSERT_NOT_NULL(state);

    text = ZrCore_String_CreateFromNative(state, "fast-copy-string");
    TEST_ASSERT_NOT_NULL(text);

    ZrCore_Value_InitAsRawObject(state, &source, ZR_CAST_RAW_OBJECT_AS_SUPER(text));
    source.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_ResetAsNull(&destination);

    ZrCore_Value_Copy(state, &destination, &source);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, destination.type);
    TEST_ASSERT_TRUE(destination.isGarbageCollectable);
    TEST_ASSERT_EQUAL_PTR(source.value.object, destination.value.object);
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_VALUE_KIND_NONE, destination.ownershipKind);
    TEST_ASSERT_NULL(destination.ownershipControl);
    TEST_ASSERT_NULL(destination.ownershipWeakRef);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_value_copy_reuses_plain_heap_object(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrObject *object;
    SZrTypeValue source;
    SZrTypeValue destination;

    TEST_ASSERT_NOT_NULL(state);

    object = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(object);
    ZrCore_Object_Init(state, object);

    ZrCore_Value_InitAsRawObject(state, &source, ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    source.type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_ResetAsNull(&destination);

    ZrCore_Value_Copy(state, &destination, &source);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, destination.type);
    TEST_ASSERT_TRUE(destination.isGarbageCollectable);
    TEST_ASSERT_EQUAL_PTR(source.value.object, destination.value.object);
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_VALUE_KIND_NONE, destination.ownershipKind);
    TEST_ASSERT_NULL(destination.ownershipControl);
    TEST_ASSERT_NULL(destination.ownershipWeakRef);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_value_copy_clones_plain_struct_object(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *prototypeName;
    SZrStructPrototype *prototype;
    SZrObject *sourceObject;
    SZrObject *copiedObject;
    SZrTypeValue source;
    SZrTypeValue destination;

    TEST_ASSERT_NOT_NULL(state);

    prototypeName = ZrCore_String_CreateFromNative(state, "FastCopyStruct");
    TEST_ASSERT_NOT_NULL(prototypeName);
    prototype = ZrCore_StructPrototype_New(state, prototypeName);
    TEST_ASSERT_NOT_NULL(prototype);

    sourceObject = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_STRUCT);
    TEST_ASSERT_NOT_NULL(sourceObject);
    sourceObject->prototype = &prototype->super;
    ZrCore_Object_Init(state, sourceObject);

    ZrCore_Value_InitAsRawObject(state, &source, ZR_CAST_RAW_OBJECT_AS_SUPER(sourceObject));
    source.type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_ResetAsNull(&destination);

    ZrCore_Value_Copy(state, &destination, &source);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, destination.type);
    TEST_ASSERT_TRUE(destination.isGarbageCollectable);
    TEST_ASSERT_NOT_EQUAL(source.value.object, destination.value.object);

    copiedObject = ZR_CAST_OBJECT(state, destination.value.object);
    TEST_ASSERT_NOT_NULL(copiedObject);
    TEST_ASSERT_EQUAL_INT(ZR_OBJECT_INTERNAL_TYPE_STRUCT, copiedObject->internalType);
    TEST_ASSERT_EQUAL_PTR(sourceObject->prototype, copiedObject->prototype);
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_VALUE_KIND_NONE, destination.ownershipKind);
    TEST_ASSERT_NULL(destination.ownershipControl);
    TEST_ASSERT_NULL(destination.ownershipWeakRef);

    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_value_copy_reuses_plain_string_object);
    RUN_TEST(test_value_copy_reuses_plain_heap_object);
    RUN_TEST(test_value_copy_clones_plain_struct_object);

    return UNITY_END();
}
