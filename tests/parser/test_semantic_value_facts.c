#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_ir.h"
#include "zr_vm_parser/semantic_value_facts.h"

/* Unity 每个用例重建这组 fixture；tearDown 按 SemIR、语义上下文、运行时状态的依赖逆序释放。 */
static SZrState *g_state;
static SZrSemanticContext *g_context;
static SZrSemanticIrFunction g_function;

/* 每个 Unity 用例从独立运行时和语义上下文开始，避免事实池在场景间串联。 */
void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
    g_context = ZrParser_SemanticContext_New(g_state);
    TEST_ASSERT_NOT_NULL(g_context);
    ZrParser_SemanticIrFunction_Init(g_state, &g_function, 1u, 0u);
}

/* BUG: 前例依次释放 g_context 与 g_state 却不清指针；下例 State_Create 失败时 setUp 在重建 context 前断言跳出，Unity 仍进 tearDown，ZrParser_SemanticContext_Free 读取悬空 context->state，造成 UAF 并可能重复释放。 */
void tearDown(void) {
    ZrParser_SemanticIrFunction_Free(g_state, &g_function);
    ZrParser_SemanticContext_Free(g_context);
    ZrTests_Runtime_State_Destroy(g_state);
}

static TZrValueId add_value(TZrTypeId typeId) {
    SZrFileRange range = {0};
    TZrValueId id = ZrParser_SemanticIr_AddValue(&g_function, typeId, range);
    TEST_ASSERT_NOT_EQUAL(ZR_VALUE_ID_INVALID, id);
    return id;
}

static SZrSemanticIrValue *value_at(TZrValueId id) {
    return (SZrSemanticIrValue *)ZrCore_Array_Get(&g_function.values, id - 1u);
}

static SZrCanonicalTypeNode *type_at(TZrTypeId id) {
    return (SZrCanonicalTypeNode *)ZrParser_CanonicalType_Find(g_context, id);
}

static TZrTypeId primitive(EZrValueType type) {
    TZrTypeId id = ZrParser_CanonicalType_InternPrimitive(g_context, type);
    TEST_ASSERT_NOT_EQUAL(0u, id);
    return id;
}

static void assert_facts(TZrValueId id, EZrSemanticValueOwnership ownership,
                         EZrSemanticValueNullability nullability) {
    const SZrSemanticIrValue *value = value_at(id);
    TEST_ASSERT_EQUAL_UINT32(value->typeId, value->facts.typeId);
    TEST_ASSERT_EQUAL_INT(ownership, value->facts.ownership);
    TEST_ASSERT_EQUAL_INT(nullability, value->facts.nullability);
}

/* 固定原始值、空值和已知运行时句柄的事实分类，防止把普通 payload 误判为 GC 引用。 */
static void test_primitive_handles_are_not_native_pointers(void) {
    EZrValueType type;
    for (type = ZR_VALUE_TYPE_NULL; type < ZR_VALUE_TYPE_ENUM_MAX; type++)
        add_value(primitive(type));
    TEST_ASSERT_TRUE(ZrParser_SemanticIr_ResolveValueFacts(&g_function, g_context));
    for (type = ZR_VALUE_TYPE_NULL; type < ZR_VALUE_TYPE_ENUM_MAX; type++) {
        EZrSemanticValueOwnership ownership = ZR_SEMANTIC_VALUE_OWNERSHIP_UNKNOWN;
        EZrSemanticValueNullability nullability = ZR_SEMANTIC_VALUE_NULLABILITY_NONNULL;
        if (type <= ZR_VALUE_TYPE_DOUBLE)
            ownership = ZR_SEMANTIC_VALUE_OWNERSHIP_VALUE;
        else if (type >= ZR_VALUE_TYPE_STRING && type <= ZR_VALUE_TYPE_THREAD)
            ownership = ZR_SEMANTIC_VALUE_OWNERSHIP_GC;
        else
            nullability = ZR_SEMANTIC_VALUE_NULLABILITY_UNKNOWN;
        if (type == ZR_VALUE_TYPE_NULL)
            nullability = ZR_SEMANTIC_VALUE_NULLABILITY_NULLABLE;
        assert_facts((TZrValueId)type + 1u, ownership, nullability);
    }
}

/* 确认可空与只读包装保留 owner 类别，而 ref 在引用 pointee 后记录为借用。 */
static void test_wrappers_preserve_owner_and_stop_at_ref(void) {
    const EZrSemanticValueOwnership owners[] = {
        ZR_SEMANTIC_VALUE_OWNERSHIP_UNIQUE,
        ZR_SEMANTIC_VALUE_OWNERSHIP_SHARED,
        ZR_SEMANTIC_VALUE_OWNERSHIP_WEAK,
        ZR_SEMANTIC_VALUE_OWNERSHIP_ATOMIC_SHARED
    };
    TZrTypeId integer = primitive(ZR_VALUE_TYPE_INT64);
    TZrTypeId nullable = ZrParser_CanonicalType_InternNullable(g_context, integer);
    TZrTypeId ref = ZrParser_CanonicalType_InternRef(
            g_context, nullable, ZR_CANONICAL_REF_READONLY);
    TZrSize index;
    add_value(ref);
    add_value(ZrParser_CanonicalType_InternNullable(g_context, ref));
    for (index = 0; index < sizeof(owners) / sizeof(owners[0]); index++) {
        TZrTypeId owner = ZrParser_CanonicalType_InternOwner(
                g_context, nullable, (EZrCanonicalOwnerKind)index);
        TZrTypeId wrapped = ZrParser_CanonicalType_InternReadonlyView(g_context, owner);
        add_value(owner);
        add_value(wrapped);
        add_value(ZrParser_CanonicalType_InternNullable(g_context, wrapped));
        add_value(ZrParser_CanonicalType_InternReadonlyView(g_context,
                ZrParser_CanonicalType_InternNullable(g_context, owner)));
    }
    TEST_ASSERT_TRUE(ZrParser_SemanticIr_ResolveValueFacts(&g_function, g_context));
    assert_facts(1u, ZR_SEMANTIC_VALUE_OWNERSHIP_BORROWED,
                 ZR_SEMANTIC_VALUE_NULLABILITY_NONNULL);
    assert_facts(2u, ZR_SEMANTIC_VALUE_OWNERSHIP_BORROWED,
                 ZR_SEMANTIC_VALUE_NULLABILITY_NULLABLE);
    for (index = 0; index < sizeof(owners) / sizeof(owners[0]); index++) {
        assert_facts(3u + (TZrValueId)index * 4u, owners[index],
                     ZR_SEMANTIC_VALUE_NULLABILITY_NONNULL);
        assert_facts(4u + (TZrValueId)index * 4u, owners[index],
                     ZR_SEMANTIC_VALUE_NULLABILITY_NONNULL);
        assert_facts(5u + (TZrValueId)index * 4u, owners[index],
                     ZR_SEMANTIC_VALUE_NULLABILITY_NULLABLE);
        assert_facts(6u + (TZrValueId)index * 4u, owners[index],
                     ZR_SEMANTIC_VALUE_NULLABILITY_NULLABLE);
    }
}

/* GC 扫描能力描述聚合布局，不能单独证明 SemIR 值就是 GC 句柄。 */
static void test_aggregate_scan_kind_does_not_prove_handle_representation(void) {
    TZrTypeId string = primitive(ZR_VALUE_TYPE_STRING);
    TZrTypeId nominal = ZrParser_CanonicalType_InternNominal(g_context, ZR_NULL,
            ZrCore_String_Create(g_state, "Aggregate", 9u), 42u);
    TZrTypeId generic = ZrParser_CanonicalType_InternGenericParameter(g_context, 1u, 0u);
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_RegisterDefinition(g_context, nominal,
            ZR_CANONICAL_TYPE_CAPABILITY_VALUE_TYPE |
            ZR_CANONICAL_TYPE_CAPABILITY_HAS_GC_REFERENCES,
            ZR_CANONICAL_GC_SCAN_MAPPED));
    add_value(nominal);
    add_value(generic);
    add_value(ZrParser_CanonicalType_InternGenericInstance(g_context, nominal, &string, 1u));
    add_value(ZrParser_CanonicalType_InternTuple(g_context, &string, 1u));
    add_value(ZrParser_CanonicalType_InternUnion(g_context, nominal, &string, 1u));
    add_value(ZrParser_CanonicalType_InternFunction(g_context, ZR_NULL, 0u, string,
            ZR_CANONICAL_RECEIVER_NONE, ZR_CANONICAL_CALLABLE_EFFECT_NONE));
    add_value(ZrParser_CanonicalType_InternError(g_context));
    add_value(ZrParser_CanonicalType_InternNever(g_context));
    add_value(ZrParser_CanonicalType_InternArray(g_context, string, 1u,
            ZR_CANONICAL_ARRAY_STORAGE_MANAGED));
    add_value(ZrParser_CanonicalType_InternArray(g_context, string, 1u,
            ZR_CANONICAL_ARRAY_STORAGE_INLINE));
    add_value(ZrParser_CanonicalType_InternArray(g_context, string, 1u,
            ZR_CANONICAL_ARRAY_STORAGE_NATIVE));
    TEST_ASSERT_TRUE(ZrParser_SemanticIr_ResolveValueFacts(&g_function, g_context));
    for (TZrValueId id = 1u; id <= 11u; id++) {
        TEST_ASSERT_EQUAL_UINT32(value_at(id)->typeId, value_at(id)->facts.typeId);
        TEST_ASSERT_EQUAL_INT(id == 9u ? ZR_SEMANTIC_VALUE_OWNERSHIP_GC
                                      : ZR_SEMANTIC_VALUE_OWNERSHIP_UNKNOWN,
                              value_at(id)->facts.ownership);
    }
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_VALUE_NULLABILITY_UNKNOWN,
                          value_at(2u)->facts.nullability);
}

/* 类型变更后必须重建 typeId 见证；零类型哨兵重新得到全未知快照。 */
static void test_refresh_replaces_stale_snapshot_and_zero_clears_it(void) {
    TZrTypeId integer = primitive(ZR_VALUE_TYPE_INT64);
    TZrTypeId owner = ZrParser_CanonicalType_InternOwner(g_context, integer,
            ZR_CANONICAL_OWNER_UNIQUE);
    TZrValueId value = add_value(owner);
    TEST_ASSERT_TRUE(ZrParser_SemanticIr_ResolveValueFacts(&g_function, g_context));
    assert_facts(value, ZR_SEMANTIC_VALUE_OWNERSHIP_UNIQUE,
                 ZR_SEMANTIC_VALUE_NULLABILITY_NONNULL);
    value_at(value)->typeId = integer;
    TEST_ASSERT_FALSE(ZrParser_SemanticIr_Validate(&g_function));
    TEST_ASSERT_TRUE(ZrParser_SemanticIr_ResolveValueFacts(&g_function, g_context));
    assert_facts(value, ZR_SEMANTIC_VALUE_OWNERSHIP_VALUE,
                 ZR_SEMANTIC_VALUE_NULLABILITY_NONNULL);
    value_at(value)->typeId = 0u;
    TEST_ASSERT_TRUE(ZrParser_SemanticIr_ResolveValueFacts(&g_function, g_context));
    assert_facts(value, ZR_SEMANTIC_VALUE_OWNERSHIP_UNKNOWN,
                 ZR_SEMANTIC_VALUE_NULLABILITY_UNKNOWN);
}

/* 任一值引用无效类型都应使整次刷新失败，已发布的所有快照保持原样。 */
static void test_invalid_type_leaves_every_snapshot_unchanged(void) {
    TZrTypeId integer = primitive(ZR_VALUE_TYPE_INT64);
    TZrValueId first = add_value(integer);
    TZrValueId second = add_value(integer);
    SZrSemanticValueFacts saved[2];
    TEST_ASSERT_TRUE(ZrParser_SemanticIr_ResolveValueFacts(&g_function, g_context));
    saved[0] = value_at(first)->facts;
    saved[1] = value_at(second)->facts;
    value_at(first)->typeId = ZrParser_CanonicalType_InternNullable(g_context, integer);
    value_at(second)->typeId = UINT32_MAX;
    TEST_ASSERT_FALSE(ZrParser_SemanticIr_ResolveValueFacts(&g_function, g_context));
    TEST_ASSERT_EQUAL_MEMORY(&saved[0], &value_at(first)->facts, sizeof(saved[0]));
    TEST_ASSERT_EQUAL_MEMORY(&saved[1], &value_at(second)->facts, sizeof(saved[1]));
}

/* 分别破坏枚举、子类型边和环，再恢复合法图，覆盖 fail-closed 与可重试路径。 */
static void test_invalid_graph_enums_edges_and_cycles_are_rejected(void) {
    TZrTypeId integer = primitive(ZR_VALUE_TYPE_INT64);
    TZrTypeId owner = ZrParser_CanonicalType_InternOwner(g_context, integer,
            ZR_CANONICAL_OWNER_SHARED);
    SZrCanonicalTypeNode original = *type_at(owner);
    add_value(owner);
    /* 每轮仅保留一种畸形输入并恢复原节点，避免前一项失败遮蔽后续边界。 */
    type_at(owner)->data.owner.ownerKind = (EZrCanonicalOwnerKind)-1;
    TEST_ASSERT_FALSE(ZrParser_SemanticIr_ResolveValueFacts(&g_function, g_context));
    *type_at(owner) = original;
    type_at(owner)->data.owner.targetTypeId = UINT32_MAX;
    TEST_ASSERT_FALSE(ZrParser_SemanticIr_ResolveValueFacts(&g_function, g_context));
    type_at(owner)->data.owner.targetTypeId = owner;
    TEST_ASSERT_FALSE(ZrParser_SemanticIr_ResolveValueFacts(&g_function, g_context));
    *type_at(owner) = original;
    type_at(integer)->data.primitive.valueType = ZR_VALUE_TYPE_ENUM_MAX;
    TEST_ASSERT_FALSE(ZrParser_SemanticIr_ResolveValueFacts(&g_function, g_context));
    type_at(integer)->data.primitive.valueType = ZR_VALUE_TYPE_INT64;
    type_at(owner)->kind = (EZrCanonicalTypeKind)-1;
    TEST_ASSERT_FALSE(ZrParser_SemanticIr_ResolveValueFacts(&g_function, g_context));
    *type_at(owner) = original;
    TEST_ASSERT_TRUE(ZrParser_SemanticIr_ResolveValueFacts(&g_function, g_context));
}

/* 4096 层包装验证遍历不依赖 C 递归；随后注入自环确认深图仍拒绝循环。 */
static void test_deep_graph_validation_is_iterative(void) {
    TZrTypeId type = primitive(ZR_VALUE_TYPE_INT64);
    TZrSize index;
    for (index = 0; index < 4096u; index++)
        type = ZrParser_CanonicalType_InternReadonlyView(g_context, type);
    add_value(type);
    TEST_ASSERT_TRUE(ZrParser_SemanticIr_ResolveValueFacts(&g_function, g_context));
    assert_facts(1u, ZR_SEMANTIC_VALUE_OWNERSHIP_VALUE,
                 ZR_SEMANTIC_VALUE_NULLABILITY_NONNULL);
    type_at(type)->data.target.targetTypeId = type;
    TEST_ASSERT_FALSE(ZrParser_SemanticIr_ResolveValueFacts(&g_function, g_context));
}

/* 从嵌套引用、数组、泛型实参到函数参数契约，确认内部畸形也阻止发布。 */
static void test_nested_canonical_enum_and_contract_errors_are_rejected(void) {
    TZrTypeId integer = primitive(ZR_VALUE_TYPE_INT64);
    TZrTypeId ref = ZrParser_CanonicalType_InternRef(g_context, integer,
            ZR_CANONICAL_REF_WRITABLE);
    TZrTypeId array = ZrParser_CanonicalType_InternArray(g_context, integer, 1u,
            ZR_CANONICAL_ARRAY_STORAGE_MANAGED);
    TZrTypeId nominal = ZrParser_CanonicalType_InternNominal(g_context, ZR_NULL,
            ZrCore_String_Create(g_state, "Generic", 7u), 55u);
    TZrTypeId instance = ZrParser_CanonicalType_InternGenericInstance(
            g_context, nominal, &integer, 1u);
    SZrCanonicalParameterContract parameter = {0};
    TZrTypeId function;
    SZrCanonicalGenericArgument *argument;
    SZrCanonicalParameterContract *storedParameter;
    parameter.typeId = integer;
    parameter.escapeUpperBound = ZR_CANONICAL_ESCAPE_FUNCTION;
    parameter.acceptsTemporary = ZR_TRUE;
    function = ZrParser_CanonicalType_InternFunction(g_context, &parameter, 1u,
            integer, ZR_CANONICAL_RECEIVER_NONE, 0u);
    TEST_ASSERT_NOT_EQUAL(0u, function);
    add_value(ref);
    add_value(array);
    add_value(instance);
    add_value(function);
    /* 逐个破坏并恢复嵌套字段，避免早期错误遮蔽后续契约检查。 */
    type_at(ref)->data.refType.access = (EZrCanonicalRefAccess)-1;
    TEST_ASSERT_FALSE(ZrParser_SemanticIr_ResolveValueFacts(&g_function, g_context));
    type_at(ref)->data.refType.access = ZR_CANONICAL_REF_WRITABLE;
    type_at(array)->data.array.storageKind = (EZrCanonicalArrayStorageKind)-1;
    TEST_ASSERT_FALSE(ZrParser_SemanticIr_ResolveValueFacts(&g_function, g_context));
    type_at(array)->data.array.storageKind = ZR_CANONICAL_ARRAY_STORAGE_MANAGED;
    type_at(array)->data.array.rank = 0u;
    TEST_ASSERT_FALSE(ZrParser_SemanticIr_ResolveValueFacts(&g_function, g_context));
    type_at(array)->data.array.rank = 1u;
    argument = (SZrCanonicalGenericArgument *)ZrCore_Array_Get(
            &type_at(instance)->data.genericInstance.arguments, 0u);
    argument->kind = (EZrCanonicalGenericArgumentKind)-1;
    TEST_ASSERT_FALSE(ZrParser_SemanticIr_ResolveValueFacts(&g_function, g_context));
    argument->kind = ZR_CANONICAL_GENERIC_ARGUMENT_TYPE;
    type_at(function)->data.function.receiverEffect = (EZrCanonicalReceiverEffect)-1;
    TEST_ASSERT_FALSE(ZrParser_SemanticIr_ResolveValueFacts(&g_function, g_context));
    type_at(function)->data.function.receiverEffect = ZR_CANONICAL_RECEIVER_NONE;
    type_at(function)->data.function.effectFlags = UINT32_MAX;
    TEST_ASSERT_FALSE(ZrParser_SemanticIr_ResolveValueFacts(&g_function, g_context));
    type_at(function)->data.function.effectFlags = 0u;
    storedParameter = (SZrCanonicalParameterContract *)ZrCore_Array_Get(
            &type_at(function)->data.function.parameterContracts, 0u);
    storedParameter->passingForm = (EZrCanonicalPassingForm)-1;
    TEST_ASSERT_FALSE(ZrParser_SemanticIr_ResolveValueFacts(&g_function, g_context));
    *storedParameter = parameter;
    TEST_ASSERT_TRUE(ZrParser_SemanticIr_ResolveValueFacts(&g_function, g_context));
}

/* 刷新只需验证值可达的规范类型子图，无关节点损坏不应阻断当前值。 */
static void test_unreachable_type_details_do_not_block_value_refresh(void) {
    TZrTypeId integer = primitive(ZR_VALUE_TYPE_INT64);
    TZrTypeId unused = ZrParser_CanonicalType_InternOwner(g_context, integer,
            ZR_CANONICAL_OWNER_SHARED);
    SZrCanonicalTypeNode original = *type_at(unused);
    add_value(integer);
    type_at(unused)->data.owner.ownerKind = (EZrCanonicalOwnerKind)-1;
    TEST_ASSERT_TRUE(ZrParser_SemanticIr_ResolveValueFacts(&g_function, g_context));
    assert_facts(1u, ZR_SEMANTIC_VALUE_OWNERSHIP_VALUE,
                 ZR_SEMANTIC_VALUE_NULLABILITY_NONNULL);
    *type_at(unused) = original;
}

/* 零类型合法地清空单值事实；空参数、无效节点和坏数组形状均不得部分发布。 */
static void test_zero_sentinel_and_invalid_inputs_do_not_publish(void) {
    TZrTypeId integer = primitive(ZR_VALUE_TYPE_INT64);
    SZrSemanticValueFacts saved;
    SZrArray savedValues;
    TZrTypeId savedId;
    SZrCanonicalTypeNode *node;
    add_value(0u);
    add_value(integer);
    TEST_ASSERT_TRUE(ZrParser_SemanticIr_ResolveValueFacts(&g_function, g_context));
    assert_facts(1u, ZR_SEMANTIC_VALUE_OWNERSHIP_UNKNOWN,
                 ZR_SEMANTIC_VALUE_NULLABILITY_UNKNOWN);
    saved = value_at(2u)->facts;
    TEST_ASSERT_FALSE(ZrParser_SemanticIr_ResolveValueFacts(ZR_NULL, g_context));
    TEST_ASSERT_FALSE(ZrParser_SemanticIr_ResolveValueFacts(&g_function, ZR_NULL));
    node = type_at(integer);
    savedId = node->id;
    node->id = 0u;
    TEST_ASSERT_FALSE(ZrParser_SemanticIr_ResolveValueFacts(&g_function, g_context));
    node->id = savedId;
    savedValues = g_function.values;
    g_function.values.length = g_function.values.capacity + 1u;
    TEST_ASSERT_FALSE(ZrParser_SemanticIr_ResolveValueFacts(&g_function, g_context));
    g_function.values = savedValues;
    TEST_ASSERT_EQUAL_MEMORY(&saved, &value_at(2u)->facts, sizeof(saved));
}

/* 损坏数组元数据或嵌套边时，失败路径不得覆盖调用前的事实快照。 */
static void test_malformed_array_shapes_and_nested_graphs_fail_atomically(void) {
    TZrTypeId integer = primitive(ZR_VALUE_TYPE_INT64);
    TZrTypeId tuple = ZrParser_CanonicalType_InternTuple(g_context, &integer, 1u);
    SZrArray savedValues;
    SZrArray savedTypes;
    SZrArray savedTuple;
    SZrSemanticValueFacts saved;
    add_value(tuple);
    TEST_ASSERT_TRUE(ZrParser_SemanticIr_ResolveValueFacts(&g_function, g_context));
    saved = value_at(1u)->facts;
    savedValues = g_function.values;
    savedTypes = g_context->canonicalTypes;
    savedTuple = type_at(tuple)->data.typeList.elementTypeIds;
    /* 恢复数组元数据后再测下一个形状，最后单独覆盖递归子边。 */
    g_function.values.elementSize--;
    TEST_ASSERT_FALSE(ZrParser_SemanticIr_ResolveValueFacts(&g_function, g_context));
    g_function.values = savedValues;
    g_context->canonicalTypes.length = g_context->canonicalTypes.capacity + 1u;
    TEST_ASSERT_FALSE(ZrParser_SemanticIr_ResolveValueFacts(&g_function, g_context));
    g_context->canonicalTypes = savedTypes;
    type_at(tuple)->data.typeList.elementTypeIds.head = ZR_NULL;
    TEST_ASSERT_FALSE(ZrParser_SemanticIr_ResolveValueFacts(&g_function, g_context));
    type_at(tuple)->data.typeList.elementTypeIds = savedTuple;
    *(TZrTypeId *)ZrCore_Array_Get(&type_at(tuple)->data.typeList.elementTypeIds, 0u) = tuple;
    TEST_ASSERT_FALSE(ZrParser_SemanticIr_ResolveValueFacts(&g_function, g_context));
    *(TZrTypeId *)ZrCore_Array_Get(&type_at(tuple)->data.typeList.elementTypeIds, 0u) = integer;
    TEST_ASSERT_EQUAL_MEMORY(&saved, &value_at(1u)->facts, sizeof(saved));
}

/* SemIR 校验必须拒绝过期类型见证以及越界 ownership/nullability 枚举。 */
static void test_snapshot_validation_checks_witness_and_enum_ranges(void) {
    TZrTypeId integer = primitive(ZR_VALUE_TYPE_INT64);
    SZrSemanticIrValue *value;
    add_value(integer);
    value = value_at(1u);
    TEST_ASSERT_TRUE(ZrParser_SemanticIr_Validate(&g_function));
    value->facts.ownership = ZR_SEMANTIC_VALUE_OWNERSHIP_VALUE;
    TEST_ASSERT_FALSE(ZrParser_SemanticIr_Validate(&g_function));
    value->facts.typeId = integer;
    value->facts.nullability = ZR_SEMANTIC_VALUE_NULLABILITY_NONNULL;
    TEST_ASSERT_TRUE(ZrParser_SemanticIr_Validate(&g_function));
    value->facts.typeId = integer + 1u;
    TEST_ASSERT_FALSE(ZrParser_SemanticIr_Validate(&g_function));
    value->facts.typeId = integer;
    value->facts.ownership = ZR_SEMANTIC_VALUE_OWNERSHIP_COUNT;
    TEST_ASSERT_FALSE(ZrParser_SemanticIr_Validate(&g_function));
    value->facts.ownership = (EZrSemanticValueOwnership)-1;
    TEST_ASSERT_FALSE(ZrParser_SemanticIr_Validate(&g_function));
    value->facts.ownership = ZR_SEMANTIC_VALUE_OWNERSHIP_VALUE;
    value->facts.nullability = ZR_SEMANTIC_VALUE_NULLABILITY_COUNT;
    TEST_ASSERT_FALSE(ZrParser_SemanticIr_Validate(&g_function));
    value->facts.nullability = (EZrSemanticValueNullability)-1;
    TEST_ASSERT_FALSE(ZrParser_SemanticIr_Validate(&g_function));
}

/* 保持既有零初始化 value 的兼容语义：未解析时三项事实仍为 unknown。 */
static void test_existing_value_initializer_leaves_unknown_snapshot(void) {
    SZrSemanticIrValue value = {1u, 2u, 3u, {0}};
    TEST_ASSERT_EQUAL_UINT32(1u, value.id);
    TEST_ASSERT_EQUAL_UINT32(2u, value.typeId);
    TEST_ASSERT_EQUAL_UINT32(3u, value.definitionInstructionId);
    TEST_ASSERT_EQUAL_UINT32(0u, value.facts.typeId);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_VALUE_OWNERSHIP_UNKNOWN, value.facts.ownership);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_VALUE_NULLABILITY_UNKNOWN, value.facts.nullability);
}

/* CTest 注册 semantic_value_facts 目标；RUN_TEST 让 Unity 为每个场景调用 fixture。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_primitive_handles_are_not_native_pointers);
    RUN_TEST(test_wrappers_preserve_owner_and_stop_at_ref);
    RUN_TEST(test_aggregate_scan_kind_does_not_prove_handle_representation);
    RUN_TEST(test_refresh_replaces_stale_snapshot_and_zero_clears_it);
    RUN_TEST(test_invalid_type_leaves_every_snapshot_unchanged);
    RUN_TEST(test_invalid_graph_enums_edges_and_cycles_are_rejected);
    RUN_TEST(test_deep_graph_validation_is_iterative);
    RUN_TEST(test_nested_canonical_enum_and_contract_errors_are_rejected);
    RUN_TEST(test_unreachable_type_details_do_not_block_value_refresh);
    RUN_TEST(test_zero_sentinel_and_invalid_inputs_do_not_publish);
    RUN_TEST(test_malformed_array_shapes_and_nested_graphs_fail_atomically);
    RUN_TEST(test_snapshot_validation_checks_witness_and_enum_ranges);
    RUN_TEST(test_existing_value_initializer_leaves_unknown_snapshot);
    return UNITY_END();
}
