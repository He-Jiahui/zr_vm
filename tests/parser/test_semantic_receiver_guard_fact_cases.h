#ifndef ZR_VM_TESTS_SEMANTIC_RECEIVER_GUARD_FACT_CASES_H
#define ZR_VM_TESTS_SEMANTIC_RECEIVER_GUARD_FACT_CASES_H

typedef struct SZrReceiverGuardFactTestFixture {
    SZrSemanticContext *context;
    SZrAstNode nodes[2];
    SZrAstNode receivers[2];
    SZrReceiverGuardFact facts[2];
} SZrReceiverGuardFactTestFixture;

static SZrReceiverGuardFactTestFixture g_receiver_guard_fact_fixture;

static void semantic_receiver_guard_fact_free_inputs(void) {
    for (TZrSize index = 0; index < 2; index++) {
        SZrReceiverGuardFact *fact = &g_receiver_guard_fact_fixture.facts[index];

        ZrParser_InferredType_Free(g_state, &fact->receiverType);
        ZrParser_InferredType_Free(g_state, &fact->guardedType);
        memset(&fact->receiverType, 0, sizeof(fact->receiverType));
        memset(&fact->guardedType, 0, sizeof(fact->guardedType));
    }
}

static void semantic_receiver_guard_fact_cleanup(void) {
    if (g_state != ZR_NULL) {
        if (g_receiver_guard_fact_fixture.context != ZR_NULL) {
            ZrParser_SemanticContext_Free(g_receiver_guard_fact_fixture.context);
        }
        semantic_receiver_guard_fact_free_inputs();
    }
    memset(&g_receiver_guard_fact_fixture, 0, sizeof(g_receiver_guard_fact_fixture));
}

static void semantic_receiver_guard_fact_init_type(
        SZrInferredType *type,
        EZrOwnershipQualifier qualifier,
        TZrBool isNullable,
        EZrValueType elementBaseType) {
    SZrInferredType element;

    ZrParser_InferredType_Init(g_state, type, ZR_VALUE_TYPE_OBJECT);
    type->ownershipQualifier = qualifier;
    type->isNullable = isNullable;
    ZrCore_Array_Init(g_state, &type->elementTypes, sizeof(SZrInferredType), 1);
    TEST_ASSERT_NOT_NULL_MESSAGE(
            type->elementTypes.head,
            "Receiver guard fixture prerequisite: input type storage");
    ZrParser_InferredType_Init(g_state, &element, elementBaseType);
    ZrCore_Array_Push(g_state, &type->elementTypes, &element);
}

static void semantic_receiver_guard_fact_init(void) {
    SZrReceiverGuardFactTestFixture *fixture = &g_receiver_guard_fact_fixture;
    SZrFileRange range = test_range(8, 16);

    fixture->context = ZrParser_SemanticContext_New(g_state);
    TEST_ASSERT_NOT_NULL_MESSAGE(
            fixture->context,
            "Receiver guard fixture prerequisite: semantic context");
    TEST_ASSERT_TRUE_MESSAGE(
            fixture->context->receiverGuardFacts.isValid,
            "Receiver guard fixture prerequisite: fact storage");
    for (TZrSize index = 0; index < 2; index++) {
        SZrReceiverGuardFact *fact = &fixture->facts[index];
        EZrValueType elementBaseType = index == 0 ? ZR_VALUE_TYPE_INT64 : ZR_VALUE_TYPE_BOOL;

        init_identifier_node_with_range(&fixture->receivers[index], range);
        fixture->nodes[index].type = ZR_AST_MEMBER_EXPRESSION;
        fixture->nodes[index].location = range;
        fixture->nodes[index].data.memberExpression.accessMode = ZR_POSTFIX_ACCESS_DIRECT;
        fact->node = &fixture->nodes[index];
        fact->receiver = &fixture->receivers[index];
        fact->firstSegment = fact->node;
        fact->range = range;
        fact->kind = ZR_RECEIVER_GUARD_WEAK_WAKE;
        fact->mode = ZR_RECEIVER_GUARD_DIRECT;
        fact->resultLift = ZR_RECEIVER_GUARD_RESULT_UNCHANGED;
        fact->chainSegmentEnd = index + 1;
        semantic_receiver_guard_fact_init_type(
                &fact->receiverType, ZR_OWNERSHIP_QUALIFIER_WEAK,
                ZR_TRUE, elementBaseType);
        semantic_receiver_guard_fact_init_type(
                &fact->guardedType, ZR_OWNERSHIP_QUALIFIER_SHARED,
                ZR_FALSE, elementBaseType);
    }
}

static TZrBool semantic_receiver_guard_fact_has_element(
        const SZrInferredType *type,
        EZrValueType expectedBaseType) {
    const SZrInferredType *element;

    if (type == ZR_NULL || type->elementTypes.length != 1) {
        return ZR_FALSE;
    }
    element = (const SZrInferredType *)ZrCore_Array_Get(
            (SZrArray *)&type->elementTypes, 0);
    return element != ZR_NULL && element->baseType == expectedBaseType;
}

static void test_semantic_receiver_guard_republish_replaces_same_node(void) {
    const TZrChar *reason =
            "Receiver guard republish must replace the same AST node and retain the latest owned type payloads";
    SZrReceiverGuardFactTestFixture *fixture = &g_receiver_guard_fact_fixture;
    const SZrReceiverGuardFact *found;
    TZrBool republished;
    TZrBool passed;

    semantic_receiver_guard_fact_init();
    fixture->facts[1].node = fixture->facts[0].node;
    fixture->facts[1].receiver = fixture->facts[0].receiver;
    fixture->facts[1].firstSegment = fixture->facts[0].firstSegment;
    fixture->facts[1].kind = ZR_RECEIVER_GUARD_NULL;
    fixture->facts[1].receiverType.ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_SHARED;
    TEST_ASSERT_TRUE_MESSAGE(
            ZrParser_SemanticFacts_AppendReceiverGuard(fixture->context, &fixture->facts[0]),
            "Receiver guard fixture prerequisite: first publication");
    republished = ZrParser_SemanticFacts_AppendReceiverGuard(
            fixture->context, &fixture->facts[1]);
    found = ZrParser_SemanticFacts_FindReceiverGuardByNode(
            fixture->context, &fixture->nodes[0]);
    passed = republished && fixture->context->receiverGuardFacts.length == 1 &&
             found != ZR_NULL && found->kind == ZR_RECEIVER_GUARD_NULL &&
             found->mode == ZR_RECEIVER_GUARD_DIRECT &&
             found->resultLift == ZR_RECEIVER_GUARD_RESULT_UNCHANGED &&
             found->receiver == &fixture->receivers[0] &&
             found->firstSegment == &fixture->nodes[0] &&
             found->chainSegmentStart == 0 && found->chainSegmentEnd == 2 &&
             found->receiverType.ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_SHARED &&
             found->receiverType.isNullable && !found->guardedType.isNullable &&
             found->receiverType.elementTypes.head != fixture->facts[1].receiverType.elementTypes.head &&
             found->guardedType.elementTypes.head != fixture->facts[1].guardedType.elementTypes.head;
    semantic_receiver_guard_fact_free_inputs();
    if (!passed ||
        !semantic_receiver_guard_fact_has_element(&found->receiverType, ZR_VALUE_TYPE_BOOL) ||
        !semantic_receiver_guard_fact_has_element(&found->guardedType, ZR_VALUE_TYPE_BOOL)) {
        TEST_FAIL_MESSAGE(reason);
    }

    /* The input may alias the stored record; replacement must copy before freeing it. */
    republished = ZrParser_SemanticFacts_AppendReceiverGuard(fixture->context, found);
    found = ZrParser_SemanticFacts_FindReceiverGuardByNode(
            fixture->context, &fixture->nodes[0]);
    if (!republished || fixture->context->receiverGuardFacts.length != 1 ||
        found == ZR_NULL || found->kind != ZR_RECEIVER_GUARD_NULL ||
        found->chainSegmentEnd != 2 ||
        !semantic_receiver_guard_fact_has_element(&found->receiverType, ZR_VALUE_TYPE_BOOL) ||
        !semantic_receiver_guard_fact_has_element(&found->guardedType, ZR_VALUE_TYPE_BOOL)) {
        TEST_FAIL_MESSAGE(reason);
    }
}

static void test_semantic_receiver_guards_preserve_distinct_nodes(void) {
    const TZrChar *reason =
            "Receiver guard facts for distinct AST nodes must remain independent";
    SZrReceiverGuardFactTestFixture *fixture = &g_receiver_guard_fact_fixture;
    const SZrReceiverGuardFact *first;
    const SZrReceiverGuardFact *second;
    TZrBool appended;
    TZrBool passed;

    semantic_receiver_guard_fact_init();
    TEST_ASSERT_TRUE_MESSAGE(
            ZrParser_SemanticFacts_AppendReceiverGuard(fixture->context, &fixture->facts[0]),
            "Receiver guard fixture prerequisite: first publication");
    appended = ZrParser_SemanticFacts_AppendReceiverGuard(
            fixture->context, &fixture->facts[1]);
    first = ZrParser_SemanticFacts_FindReceiverGuardByNode(fixture->context, &fixture->nodes[0]);
    second = ZrParser_SemanticFacts_FindReceiverGuardByNode(fixture->context, &fixture->nodes[1]);
    passed = appended && fixture->context->receiverGuardFacts.length == 2 &&
             first != ZR_NULL && second != ZR_NULL && first != second &&
             first->receiver == &fixture->receivers[0] &&
             second->receiver == &fixture->receivers[1] &&
             first->kind == ZR_RECEIVER_GUARD_WEAK_WAKE &&
             second->kind == ZR_RECEIVER_GUARD_WEAK_WAKE &&
             first->mode == ZR_RECEIVER_GUARD_DIRECT &&
             second->mode == ZR_RECEIVER_GUARD_DIRECT &&
             first->range.source == second->range.source &&
             first->range.start.offset == second->range.start.offset &&
             first->range.end.offset == second->range.end.offset;
    semantic_receiver_guard_fact_free_inputs();
    if (!passed ||
        !semantic_receiver_guard_fact_has_element(&first->receiverType, ZR_VALUE_TYPE_INT64) ||
        !semantic_receiver_guard_fact_has_element(&first->guardedType, ZR_VALUE_TYPE_INT64) ||
        !semantic_receiver_guard_fact_has_element(&second->receiverType, ZR_VALUE_TYPE_BOOL) ||
        !semantic_receiver_guard_fact_has_element(&second->guardedType, ZR_VALUE_TYPE_BOOL)) {
        TEST_FAIL_MESSAGE(reason);
    }
}

#endif
