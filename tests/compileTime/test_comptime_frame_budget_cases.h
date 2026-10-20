#ifndef ZR_TEST_COMPTIME_FRAME_BUDGET_CASES_H
#define ZR_TEST_COMPTIME_FRAME_BUDGET_CASES_H

typedef struct SFrameAllocationProbe {
    FZrAllocator allocator;
    TZrPtr userData;
    TZrSize arrayRequests;
} SFrameAllocationProbe;

static SFrameAllocationProbe g_frameProbe;

static TZrPtr count_frame_array_allocations(
        TZrPtr userData, TZrPtr pointer, TZrSize originalSize,
        TZrSize newSize, TZrInt64 flag) {
    SFrameAllocationProbe *probe = (SFrameAllocationProbe *)userData;
    if (newSize > 0U && flag == ZR_MEMORY_NATIVE_TYPE_ARRAY) {
        probe->arrayRequests++;
    }
    return probe->allocator(
            probe->userData, pointer, originalSize, newSize, flag);
}

static void test_public_scalar_evaluation_needs_no_frame_backing(void) {
    static const TZrChar *source = "var probe = 42;";
    SZrAstNode *declaration;
    SZrTypeValue result;
    TZrSize beforeRoots;
    TZrBool success;

    g_ast = ZrParser_Parse(
            g_state, source, strlen(source), make_string("scalar_frame_budget.zr"));
    TEST_ASSERT_NOT_NULL(g_ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_ast->type);
    TEST_ASSERT_EQUAL_UINT64(1U, g_ast->data.script.statements->count);
    declaration = g_ast->data.script.statements->nodes[0];
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, declaration->type);
    TEST_ASSERT_NOT_NULL(declaration->data.variableDeclaration.value);
    beforeRoots = ZrCore_GcDomain_GetRootCount(g_state);
    g_compiler.comptimeBudget.limits.heapBytes = 0U;
    ZrCore_Value_ResetAsNull(&result);
    memset(&g_frameProbe, 0, sizeof(g_frameProbe));
    g_frameProbe.allocator = g_state->global->allocator;
    g_frameProbe.userData = g_state->global->userAllocationArguments;
    g_state->global->allocator = count_frame_array_allocations;
    g_state->global->userAllocationArguments = &g_frameProbe;
    success = ZrParser_Compiler_EvaluateCompileTimeExpression(
            &g_compiler, declaration->data.variableDeclaration.value, &result);
    g_state->global->allocator = g_frameProbe.allocator;
    g_state->global->userAllocationArguments = g_frameProbe.userData;

    if (g_frameProbe.arrayRequests != 0U) {
        TEST_FAIL_MESSAGE("Public scalar evaluation allocated unbudgeted frame backing");
    }
    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
    TEST_ASSERT_EQUAL_INT64(42, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_FALSE(g_compiler.hasCompileTimeError);
    TEST_ASSERT_EQUAL_UINT64(0U, g_compiler.comptimeBudget.usage.heapBytes);
    TEST_ASSERT_EQUAL_UINT64(0U, g_compiler.comptimeBudget.usage.aggregateCount);
    TEST_ASSERT_EQUAL_UINT64(beforeRoots, ZrCore_GcDomain_GetRootCount(g_state));
}

static void test_root_growth_requires_full_candidate_heap_admission(void) {
    static const TZrChar *texts[] = {
            "frame root zero", "frame root one", "frame root two",
            "frame root three", "frame root rejected"};
    SZrCompileTimeFrame frame;
    SZrGcRootHandle roots[ZR_PARSER_INITIAL_CAPACITY_TINY];
    SZrTypeValue value;
    TZrSize baseline = ZrCore_GcDomain_GetRootCount(g_state);
    TZrBytePtr oldHead;
    TZrSize oldCapacity;
    TZrSize nextCapacity;
    TZrUInt64 beforeHeap;
    TZrUInt64 beforeAggregate;
    TZrSize duringRoots;
    TZrUInt64 afterHeap;
    TZrUInt64 afterAggregate;
    TZrBool unchanged;
    TZrBool rootsResolve = ZR_TRUE;
    TZrBool success;
    TZrSize arrayRequests;

    TEST_ASSERT_EQUAL_UINT64(4U, ZR_PARSER_INITIAL_CAPACITY_TINY);
    ct_frame_init(&g_compiler, &frame, ZR_NULL);
    for (TZrSize index = 0U; index < ZR_PARSER_INITIAL_CAPACITY_TINY; index++) {
        value = string_value(texts[index]);
        TEST_ASSERT_TRUE(ct_frame_root_value(&g_compiler, &frame, &value));
    }
    TEST_ASSERT_EQUAL_UINT64(ZR_PARSER_INITIAL_CAPACITY_TINY,
                             frame.temporaryRoots.capacity);
    oldHead = frame.temporaryRoots.head;
    oldCapacity = frame.temporaryRoots.capacity;
    memcpy(roots, oldHead, sizeof(roots));
    value = string_value(texts[ZR_PARSER_INITIAL_CAPACITY_TINY]);
    beforeHeap = g_compiler.comptimeBudget.usage.heapBytes;
    beforeAggregate = g_compiler.comptimeBudget.usage.aggregateCount;
    nextCapacity = oldCapacity * ZR_ARRAY_INCREASEMENT_MULTIPLIER_PERCENT / 100U + 1U;
    g_compiler.comptimeBudget.limits.heapBytes =
            beforeHeap + nextCapacity * sizeof(SZrGcRootHandle) - 1U;
    memset(&g_frameProbe, 0, sizeof(g_frameProbe));
    g_frameProbe.allocator = g_state->global->allocator;
    g_frameProbe.userData = g_state->global->userAllocationArguments;
    g_state->global->allocator = count_frame_array_allocations;
    g_state->global->userAllocationArguments = &g_frameProbe;
    success = ct_frame_root_value(&g_compiler, &frame, &value);
    g_state->global->allocator = g_frameProbe.allocator;
    g_state->global->userAllocationArguments = g_frameProbe.userData;
    arrayRequests = g_frameProbe.arrayRequests;
    unchanged = frame.temporaryRoots.head == oldHead &&
            frame.temporaryRoots.capacity == oldCapacity &&
            frame.temporaryRoots.length == ZR_PARSER_INITIAL_CAPACITY_TINY &&
            memcmp(frame.temporaryRoots.head, roots, sizeof(roots)) == 0;
    for (TZrSize index = 0U; index < ZR_PARSER_INITIAL_CAPACITY_TINY; index++) {
        SZrRawObject *resolved = ZR_NULL;
        if (!ZrCore_GcRootHandle_Resolve(g_state, &roots[index], &resolved) ||
            resolved == ZR_NULL ||
            strcmp(ZrCore_String_GetNativeString((SZrString *)resolved),
                   texts[index]) != 0) {
            rootsResolve = ZR_FALSE;
        }
    }
    duringRoots = ZrCore_GcDomain_GetRootCount(g_state);
    afterHeap = g_compiler.comptimeBudget.usage.heapBytes;
    afterAggregate = g_compiler.comptimeBudget.usage.aggregateCount;
    ct_frame_free(&g_compiler, &frame);

    if (success) {
        TEST_FAIL_MESSAGE("Frame root growth bypassed full candidate heap admission");
    }
    TEST_ASSERT_EQUAL_UINT64(0U, arrayRequests);
    TEST_ASSERT_TRUE(unchanged);
    TEST_ASSERT_TRUE(rootsResolve);
    TEST_ASSERT_TRUE(g_compiler.hasCompileTimeError);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES,
                          g_compiler.comptimeBudget.exceededResource);
    TEST_ASSERT_EQUAL_UINT64(beforeHeap + nextCapacity * sizeof(SZrGcRootHandle),
                             g_compiler.comptimeBudget.requestedUsage);
    TEST_ASSERT_EQUAL_UINT64(beforeHeap, afterHeap);
    TEST_ASSERT_EQUAL_UINT64(beforeAggregate, afterAggregate);
    TEST_ASSERT_EQUAL_UINT64(baseline + ZR_PARSER_INITIAL_CAPACITY_TINY, duringRoots);
    TEST_ASSERT_EQUAL_UINT64(baseline, ZrCore_GcDomain_GetRootCount(g_state));
}

static void test_binding_growth_requires_full_candidate_heap_admission(void) {
    static const TZrChar *names[] = {
            "frameBindingZero", "frameBindingOne", "frameBindingTwo",
            "frameBindingThree", "frameBindingRejected"};
    SZrCompileTimeFrame frame;
    SZrCompileTimeBinding bindings[ZR_PARSER_INITIAL_CAPACITY_TINY];
    SZrTypeValue value;
    SZrString *name;
    TZrSize baseline = ZrCore_GcDomain_GetRootCount(g_state);
    TZrBytePtr oldHead;
    TZrSize oldCapacity;
    TZrSize nextCapacity;
    TZrUInt64 beforeHeap;
    TZrUInt64 beforeAggregate;
    TZrSize duringRoots;
    TZrUInt64 afterHeap;
    TZrUInt64 afterAggregate;
    TZrBool unchanged;
    TZrBool rootsResolve = ZR_TRUE;
    TZrBool success;
    TZrSize arrayRequests;

    TEST_ASSERT_EQUAL_UINT64(4U, ZR_PARSER_INITIAL_CAPACITY_TINY);
    ct_frame_init(&g_compiler, &frame, ZR_NULL);
    for (TZrSize index = 0U; index < ZR_PARSER_INITIAL_CAPACITY_TINY; index++) {
        name = make_string(names[index]);
        ZrCore_Value_InitAsInt(g_state, &value, (TZrInt64)index);
        TEST_ASSERT_TRUE(ct_frame_set(&g_compiler, &frame, name, &value));
    }
    TEST_ASSERT_EQUAL_UINT64(ZR_PARSER_INITIAL_CAPACITY_TINY, frame.bindings.capacity);
    oldHead = frame.bindings.head;
    oldCapacity = frame.bindings.capacity;
    memcpy(bindings, oldHead, sizeof(bindings));
    name = make_string(names[ZR_PARSER_INITIAL_CAPACITY_TINY]);
    ZrCore_Value_InitAsInt(g_state, &value, 99);
    beforeHeap = g_compiler.comptimeBudget.usage.heapBytes;
    beforeAggregate = g_compiler.comptimeBudget.usage.aggregateCount;
    nextCapacity = oldCapacity * ZR_ARRAY_INCREASEMENT_MULTIPLIER_PERCENT / 100U + 1U;
    g_compiler.comptimeBudget.limits.heapBytes =
            beforeHeap + nextCapacity * sizeof(SZrCompileTimeBinding) - 1U;
    memset(&g_frameProbe, 0, sizeof(g_frameProbe));
    g_frameProbe.allocator = g_state->global->allocator;
    g_frameProbe.userData = g_state->global->userAllocationArguments;
    g_state->global->allocator = count_frame_array_allocations;
    g_state->global->userAllocationArguments = &g_frameProbe;
    success = ct_frame_set(&g_compiler, &frame, name, &value);
    g_state->global->allocator = g_frameProbe.allocator;
    g_state->global->userAllocationArguments = g_frameProbe.userData;
    arrayRequests = g_frameProbe.arrayRequests;
    unchanged = frame.bindings.head == oldHead &&
            frame.bindings.capacity == oldCapacity &&
            frame.bindings.length == ZR_PARSER_INITIAL_CAPACITY_TINY &&
            memcmp(frame.bindings.head, bindings, sizeof(bindings)) == 0;
    for (TZrSize index = 0U; index < ZR_PARSER_INITIAL_CAPACITY_TINY; index++) {
        SZrRawObject *resolved = ZR_NULL;
        if (!ZrCore_GcRootHandle_Resolve(
                    g_state, &bindings[index].nameRoot, &resolved) ||
            resolved == ZR_NULL ||
            strcmp(ZrCore_String_GetNativeString((SZrString *)resolved),
                   names[index]) != 0 ||
            bindings[index].value.value.nativeObject.nativeInt64 != (TZrInt64)index) {
            rootsResolve = ZR_FALSE;
        }
    }
    duringRoots = ZrCore_GcDomain_GetRootCount(g_state);
    afterHeap = g_compiler.comptimeBudget.usage.heapBytes;
    afterAggregate = g_compiler.comptimeBudget.usage.aggregateCount;
    ct_frame_free(&g_compiler, &frame);

    if (success) {
        TEST_FAIL_MESSAGE("Frame binding growth bypassed full candidate heap admission");
    }
    TEST_ASSERT_EQUAL_UINT64(0U, arrayRequests);
    TEST_ASSERT_TRUE(unchanged);
    TEST_ASSERT_TRUE(rootsResolve);
    TEST_ASSERT_TRUE(g_compiler.hasCompileTimeError);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES,
                          g_compiler.comptimeBudget.exceededResource);
    TEST_ASSERT_EQUAL_UINT64(beforeHeap + nextCapacity * sizeof(SZrCompileTimeBinding),
                             g_compiler.comptimeBudget.requestedUsage);
    TEST_ASSERT_EQUAL_UINT64(beforeHeap, afterHeap);
    TEST_ASSERT_EQUAL_UINT64(beforeAggregate, afterAggregate);
    TEST_ASSERT_EQUAL_UINT64(baseline + ZR_PARSER_INITIAL_CAPACITY_TINY, duringRoots);
    TEST_ASSERT_EQUAL_UINT64(baseline, ZrCore_GcDomain_GetRootCount(g_state));
}

static void test_rejected_binding_value_keeps_mutability(void) {
    SZrCompileTimeFrame frame;
    SZrState *foreignState = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *name = make_string("mutableAfterRejectedValue");
    SZrString *foreignString;
    SZrTypeValue value;
    SZrTypeValue result;
    SZrCompileTimeBinding beforeBinding;
    SZrParserComptimeBudgetUsage beforeUsage;
    TZrSize baseline = ZrCore_GcDomain_GetRootCount(g_state);
    TZrSize beforeRoots;
    TZrSize foreignRoots;
    TZrBool success;
    TZrBool unchanged;
    TZrBool isConst;
    TZrBool found;
    TZrBool foreignRootsUnchanged;

    TEST_ASSERT_NOT_NULL(foreignState);
    foreignString = ZrCore_String_CreateFromNative(
            foreignState, "real foreign-domain value");
    TEST_ASSERT_NOT_NULL(foreignString);
    TEST_ASSERT_FALSE(ZrCore_GcDomain_ObjectBelongsToState(
            g_state, ZR_CAST_RAW_OBJECT_AS_SUPER(foreignString)));
    ct_frame_init(&g_compiler, &frame, ZR_NULL);
    ZrCore_Value_InitAsInt(g_state, &value, 17);
    TEST_ASSERT_TRUE(ct_frame_set(&g_compiler, &frame, name, &value));
    memcpy(&beforeBinding, binding_at(&frame, 0U), sizeof(beforeBinding));
    beforeUsage = g_compiler.comptimeBudget.usage;
    beforeRoots = ZrCore_GcDomain_GetRootCount(g_state);
    foreignRoots = ZrCore_GcDomain_GetRootCount(foreignState);
    ZrCore_Value_InitAsRawObject(
            foreignState, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(foreignString));
    value.type = ZR_VALUE_TYPE_STRING;
    success = ct_frame_declare(&g_compiler, &frame, name, &value, ZR_TRUE);
    isConst = binding_at(&frame, 0U)->isConst;
    unchanged = memcmp(&beforeBinding, binding_at(&frame, 0U),
                       sizeof(beforeBinding)) == 0 &&
            memcmp(&beforeUsage, &g_compiler.comptimeBudget.usage,
                   sizeof(beforeUsage)) == 0;
    found = ct_frame_get(&frame, name, &result);
    ct_frame_free(&g_compiler, &frame);
    foreignRootsUnchanged = foreignRoots ==
            ZrCore_GcDomain_GetRootCount(foreignState);
    ZrTests_Runtime_State_Destroy(foreignState);

    if (isConst) {
        TEST_FAIL_MESSAGE("Rejected frame value changed binding mutability");
    }
    TEST_ASSERT_FALSE(success);
    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_TRUE(foreignRootsUnchanged);
    TEST_ASSERT_TRUE(unchanged);
    TEST_ASSERT_EQUAL_INT64(17, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT64(baseline + 1U, beforeRoots);
    TEST_ASSERT_EQUAL_UINT64(baseline, ZrCore_GcDomain_GetRootCount(g_state));
}

#endif
