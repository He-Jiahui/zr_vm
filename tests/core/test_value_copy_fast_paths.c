#include "unity.h"

#include "tests/harness/runtime_support.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

void setUp(void) {}

void tearDown(void) {}

// 普通字符串的值复制应保留托管对象身份，避免在常见传参路径重复分配。
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

// 普通堆对象走引用语义；复制值容器不得隐式克隆对象或附加所有权控制。
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

// 热路径的无 profile 尝试复制需要与公开 Copy 对普通对象的身份语义一致。
static void test_value_try_copy_fast_reuses_plain_heap_object(void) {
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

    TEST_ASSERT_TRUE(ZrCore_Value_TryCopyFastNoProfile(state, &destination, &source));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, destination.type);
    TEST_ASSERT_TRUE(destination.isGarbageCollectable);
    TEST_ASSERT_EQUAL_PTR(source.value.object, destination.value.object);
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_VALUE_KIND_NONE, destination.ownershipKind);
    TEST_ASSERT_NULL(destination.ownershipControl);
    TEST_ASSERT_NULL(destination.ownershipWeakRef);

    ZrTests_Runtime_State_Destroy(state);
}

// 非 ref-like 结构体是值语义；通用 Copy 必须返回独立对象而保留原型。
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

// 结构体复制需走深层路径；快速尝试失败时目的值必须仍处于空值状态。
static void test_value_try_copy_fast_rejects_plain_struct_object(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *prototypeName;
    SZrStructPrototype *prototype;
    SZrObject *sourceObject;
    SZrTypeValue source;
    SZrTypeValue destination;

    TEST_ASSERT_NOT_NULL(state);

    prototypeName = ZrCore_String_CreateFromNative(state, "FastCopyStructFastMiss");
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

    TEST_ASSERT_FALSE(ZrCore_Value_TryCopyFastNoProfile(state, &destination, &source));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NULL, destination.type);
    TEST_ASSERT_FALSE(destination.isGarbageCollectable);
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_VALUE_KIND_NONE, destination.ownershipKind);
    TEST_ASSERT_NULL(destination.ownershipControl);
    TEST_ASSERT_NULL(destination.ownershipWeakRef);

    ZrTests_Runtime_State_Destroy(state);
}

// ref-like 协议改变结构体的复制契约，快速与通用路径都应保留同一对象身份。
static void test_value_copy_preserves_ref_like_struct_identity(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *prototypeName;
    SZrStructPrototype *prototype;
    SZrObject *sourceObject;
    SZrTypeValue source;
    SZrTypeValue destination;

    TEST_ASSERT_NOT_NULL(state);

    prototypeName = ZrCore_String_CreateFromNative(
            state, "FastCopyRefLikeStruct");
    TEST_ASSERT_NOT_NULL(prototypeName);
    prototype = ZrCore_StructPrototype_New(state, prototypeName);
    TEST_ASSERT_NOT_NULL(prototype);
    prototype->super.protocolMask |=
            ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_REF_LIKE);

    sourceObject = ZrCore_Object_NewCustomized(
            state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_STRUCT);
    TEST_ASSERT_NOT_NULL(sourceObject);
    sourceObject->prototype = &prototype->super;
    ZrCore_Object_Init(state, sourceObject);

    ZrCore_Value_InitAsRawObject(
            state, &source, ZR_CAST_RAW_OBJECT_AS_SUPER(sourceObject));
    source.type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_ResetAsNull(&destination);

    TEST_ASSERT_TRUE(ZrCore_Value_TryCopyFastNoProfile(
            state, &destination, &source));
    TEST_ASSERT_EQUAL_PTR(source.value.object, destination.value.object);

    ZrCore_Value_ResetAsNull(&destination);
    ZrCore_Value_Copy(state, &destination, &source);
    TEST_ASSERT_EQUAL_PTR(source.value.object, destination.value.object);

    ZrTests_Runtime_State_Destroy(state);
}

// 类型标签不能单独证明对象有效；空载荷须拒绝快速复制且不污染目的值。
static void test_value_try_copy_fast_rejects_null_heap_object_payload(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrTypeValue source;
    SZrTypeValue destination;

    TEST_ASSERT_NOT_NULL(state);

    ZrCore_Value_ResetAsNull(&source);
    source.type = ZR_VALUE_TYPE_OBJECT;
    source.isGarbageCollectable = ZR_TRUE;
    source.isNative = ZR_FALSE;
    source.value.object = ZR_NULL;
    ZrCore_Value_ResetAsNull(&destination);

    TEST_ASSERT_FALSE(ZrCore_Value_CanFastCopyPlainHeapObject(state, &source));
    TEST_ASSERT_FALSE(ZrCore_Value_TryCopyFastNoProfile(state, &destination, &source));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NULL, destination.type);
    TEST_ASSERT_FALSE(destination.isGarbageCollectable);
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_VALUE_KIND_NONE, destination.ownershipKind);
    TEST_ASSERT_NULL(destination.ownershipControl);
    TEST_ASSERT_NULL(destination.ownershipWeakRef);

    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_value_copy_reuses_plain_string_object);
    RUN_TEST(test_value_copy_reuses_plain_heap_object);
    RUN_TEST(test_value_try_copy_fast_reuses_plain_heap_object);
    RUN_TEST(test_value_copy_clones_plain_struct_object);
    RUN_TEST(test_value_try_copy_fast_rejects_plain_struct_object);
    RUN_TEST(test_value_copy_preserves_ref_like_struct_identity);
    RUN_TEST(test_value_try_copy_fast_rejects_null_heap_object_payload);

    return UNITY_END();
}
