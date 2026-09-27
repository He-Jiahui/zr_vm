#include "unity.h"

#include "test_support.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/generic_instantiation.h"

#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

/* Unity 每个用例持有独立 state；类型名由它管理，实例化表的原生数组仍需显式 Free。 */
static SZrState *g_state = ZR_NULL;

void setUp(void) {
    g_state = ZrTests_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

/* BUG: 断言失败会跳过各用例末尾的 Table_Free；仅销毁 state 不会释放表的原生数组。 */
void tearDown(void) {
    ZrTests_State_Destroy(g_state);
    g_state = ZR_NULL;
}

/* 构造表键的输入类型；可选类型名留在当前 Unity state 中供拷贝后的记录借用。 */
static void test_type_init(SZrInferredType *type, EZrValueType baseType, TZrNativeString typeName) {
    if (typeName != ZR_NULL) {
        ZrParser_InferredType_InitFull(
                g_state,
                type,
                baseType,
                ZR_FALSE,
                ZrCore_String_CreateFromNative(g_state, typeName));
        return;
    }

    ZrParser_InferredType_Init(g_state, type, baseType);
}

static void test_type_free_all(SZrInferredType *types, TZrSize count) {
    for (TZrSize i = 0; i < count; ++i) {
        ZrParser_InferredType_Free(g_state, &types[i]);
    }
}

/* 引用形状组合共享实例编号；重复的 base token 与类型键不应新增记录。 */
static void test_reference_arguments_share_and_dedupe(void) {
    SZrGenericInstantiationTable table;
    const SZrGenericInstantiationRecord *first = ZR_NULL;
    const SZrGenericInstantiationRecord *duplicate = ZR_NULL;
    SZrInferredType args[2];

    test_type_init(&args[0], ZR_VALUE_TYPE_OBJECT, "Device");
    test_type_init(&args[1], ZR_VALUE_TYPE_STRING, ZR_NULL);

    ZrParser_GenericInstantiationTable_Init(g_state, &table);
    TEST_ASSERT_TRUE(ZrParser_GenericInstantiationTable_GetOrAdd(
            g_state,
            &table,
            0x1001u,
            args,
            ZR_ARRAY_COUNT(args),
            &first));
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_EQUAL_UINT32(1u, first->cInstanceId);
    TEST_ASSERT_EQUAL_INT(ZR_GENERIC_INSTANTIATION_SHARE_KIND_SHARED_REFERENCE, first->shareKind);
    TEST_ASSERT_EQUAL_UINT(1u, ZrParser_GenericInstantiationTable_Count(&table));

    TEST_ASSERT_TRUE(ZrParser_GenericInstantiationTable_GetOrAdd(
            g_state,
            &table,
            0x1001u,
            args,
            ZR_ARRAY_COUNT(args),
            &duplicate));
    TEST_ASSERT_NOT_NULL(duplicate);
    TEST_ASSERT_EQUAL_UINT32(first->cInstanceId, duplicate->cInstanceId);
    TEST_ASSERT_EQUAL_INT(first->shareKind, duplicate->shareKind);
    TEST_ASSERT_EQUAL_UINT(1u, ZrParser_GenericInstantiationTable_Count(&table));

    ZrParser_GenericInstantiationTable_Free(g_state, &table);
    test_type_free_all(args, ZR_ARRAY_COUNT(args));
}

/* 值类型要求单态化，且参数类型或泛型基底变化都会形成新的实例键。 */
static void test_value_argument_monomorphizes_and_distinguishes_keys(void) {
    SZrGenericInstantiationTable table;
    const SZrGenericInstantiationRecord *first = ZR_NULL;
    const SZrGenericInstantiationRecord *duplicate = ZR_NULL;
    const SZrGenericInstantiationRecord *differentArg = ZR_NULL;
    const SZrGenericInstantiationRecord *differentBase = ZR_NULL;
    SZrInferredType intArg;
    SZrInferredType uintArg;

    test_type_init(&intArg, ZR_VALUE_TYPE_INT64, ZR_NULL);
    test_type_init(&uintArg, ZR_VALUE_TYPE_UINT64, ZR_NULL);

    ZrParser_GenericInstantiationTable_Init(g_state, &table);
    TEST_ASSERT_TRUE(ZrParser_GenericInstantiationTable_GetOrAdd(g_state, &table, 0x2001u, &intArg, 1u, &first));
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_EQUAL_UINT32(1u, first->cInstanceId);
    TEST_ASSERT_EQUAL_INT(ZR_GENERIC_INSTANTIATION_SHARE_KIND_MONOMORPHIZED_VALUE, first->shareKind);

    TEST_ASSERT_TRUE(ZrParser_GenericInstantiationTable_GetOrAdd(g_state, &table, 0x2001u, &intArg, 1u, &duplicate));
    TEST_ASSERT_EQUAL_UINT32(first->cInstanceId, duplicate->cInstanceId);
    TEST_ASSERT_EQUAL_UINT(1u, ZrParser_GenericInstantiationTable_Count(&table));

    TEST_ASSERT_TRUE(
            ZrParser_GenericInstantiationTable_GetOrAdd(g_state, &table, 0x2001u, &uintArg, 1u, &differentArg));
    TEST_ASSERT_NOT_NULL(differentArg);
    TEST_ASSERT_EQUAL_UINT32(2u, differentArg->cInstanceId);
    TEST_ASSERT_EQUAL_INT(ZR_GENERIC_INSTANTIATION_SHARE_KIND_MONOMORPHIZED_VALUE, differentArg->shareKind);

    TEST_ASSERT_TRUE(
            ZrParser_GenericInstantiationTable_GetOrAdd(g_state, &table, 0x2002u, &intArg, 1u, &differentBase));
    TEST_ASSERT_NOT_NULL(differentBase);
    TEST_ASSERT_EQUAL_UINT32(3u, differentBase->cInstanceId);
    TEST_ASSERT_EQUAL_UINT(3u, ZrParser_GenericInstantiationTable_Count(&table));

    ZrParser_GenericInstantiationTable_Free(g_state, &table);
    ZrParser_InferredType_Free(g_state, &uintArg);
    ZrParser_InferredType_Free(g_state, &intArg);
}

/* 同为 OBJECT 基础类型时，显式传入的类/结构体形状决定共享或单态化。 */
/* TODO: CLI AOT 的 preserve 实参目前先由类型文本构成 OBJECT，再从基础类型推断形状；
 * 需用命名结构体 TypeSpec 的集成用例核对导出的 shareKind 是否保持值形状。 */
static void test_resolved_object_shape_controls_share_kind(void) {
    SZrGenericInstantiationTable table;
    const SZrGenericInstantiationRecord *classRecord = ZR_NULL;
    const SZrGenericInstantiationRecord *structRecord = ZR_NULL;
    SZrInferredType classType;
    SZrInferredType structType;
    SZrGenericInstantiationTypeArgument classArg;
    SZrGenericInstantiationTypeArgument structArg;

    test_type_init(&classType, ZR_VALUE_TYPE_OBJECT, "SourceClass");
    test_type_init(&structType, ZR_VALUE_TYPE_OBJECT, "SourceStruct");
    classArg.type = classType;
    classArg.shape = ZR_GENERIC_INSTANTIATION_TYPE_SHAPE_REFERENCE;
    structArg.type = structType;
    structArg.shape = ZR_GENERIC_INSTANTIATION_TYPE_SHAPE_VALUE;

    ZrParser_GenericInstantiationTable_Init(g_state, &table);
    TEST_ASSERT_TRUE(
            ZrParser_GenericInstantiationTable_GetOrAddResolved(g_state, &table, 0x3001u, &classArg, 1u, &classRecord));
    TEST_ASSERT_NOT_NULL(classRecord);
    TEST_ASSERT_EQUAL_INT(ZR_GENERIC_INSTANTIATION_SHARE_KIND_SHARED_REFERENCE, classRecord->shareKind);

    TEST_ASSERT_TRUE(ZrParser_GenericInstantiationTable_GetOrAddResolved(
            g_state,
            &table,
            0x3002u,
            &structArg,
            1u,
            &structRecord));
    TEST_ASSERT_NOT_NULL(structRecord);
    TEST_ASSERT_EQUAL_INT(ZR_GENERIC_INSTANTIATION_SHARE_KIND_MONOMORPHIZED_VALUE, structRecord->shareKind);
    TEST_ASSERT_EQUAL_UINT(2u, ZrParser_GenericInstantiationTable_Count(&table));

    ZrParser_GenericInstantiationTable_Free(g_state, &table);
    ZrParser_InferredType_Free(g_state, &structType);
    ZrParser_InferredType_Free(g_state, &classType);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_reference_arguments_share_and_dedupe);
    RUN_TEST(test_value_argument_monomorphizes_and_distinguishes_keys);
    RUN_TEST(test_resolved_object_shape_controls_share_kind);
    return UNITY_END();
}
