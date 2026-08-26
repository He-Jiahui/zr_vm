#ifndef ZR_VM_TEST_OWNERSHIP_RECEIVER_GUARD_CONTRACT_CASES_H
#define ZR_VM_TEST_OWNERSHIP_RECEIVER_GUARD_CONTRACT_CASES_H

static TZrUInt32 receiver_guard_contract_count_opcode(
        const SZrFunction *function,
        EZrInstructionCode opcode,
        TZrUInt32 depth) {
    TZrUInt32 count = 0u;

    if (function == ZR_NULL || depth > 8u) {
        return 0u;
    }
    for (TZrUInt32 index = 0u; index < function->instructionsLength; index++) {
        if (function->instructionsList[index].instruction.operationCode == opcode) {
            count++;
        }
    }
    for (TZrUInt32 index = 0u; index < function->constantValueLength; index++) {
        const SZrTypeValue *constant = &function->constantValueList[index];
        SZrFunction *nested;

        if (constant->type != ZR_VALUE_TYPE_FUNCTION ||
            constant->value.object == ZR_NULL || constant->isNative) {
            continue;
        }
        nested = ZR_CAST_FUNCTION(g_state, constant->value.object);
        if (nested != function) {
            count += receiver_guard_contract_count_opcode(
                    nested, opcode, depth + 1u);
        }
    }
    return count;
}

static void receiver_guard_contract_assert_wake_cleanup_markers(
        const SZrFunction *function,
        TZrUInt32 depth) {
    if (function == ZR_NULL || depth > 8u) {
        return;
    }
    for (TZrUInt32 index = 0u; index < function->instructionsLength; index++) {
        const SZrInstruction *wakeInstruction =
                &function->instructionsList[index].instruction;

        if (wakeInstruction->operationCode != ZR_INSTRUCTION_ENUM(OWN_WAKE)) {
            continue;
        }
        TEST_ASSERT_LESS_THAN_UINT32(function->instructionsLength, index + 1u);
        TEST_ASSERT_EQUAL_UINT16(
                ZR_INSTRUCTION_ENUM(MARK_TO_BE_CLOSED),
                function->instructionsList[index + 1u].instruction.operationCode);
        TEST_ASSERT_EQUAL_UINT16(
                wakeInstruction->operandExtra,
                function->instructionsList[index + 1u].instruction.operandExtra);
    }
    for (TZrUInt32 index = 0u; index < function->constantValueLength; index++) {
        const SZrTypeValue *constant = &function->constantValueList[index];
        SZrFunction *nested;

        if (constant->type != ZR_VALUE_TYPE_FUNCTION ||
            constant->value.object == ZR_NULL || constant->isNative) {
            continue;
        }
        nested = ZR_CAST_FUNCTION(g_state, constant->value.object);
        if (nested != function) {
            receiver_guard_contract_assert_wake_cleanup_markers(
                    nested, depth + 1u);
        }
    }
}

static SZrAstNode *receiver_guard_contract_parse_source(const TZrChar *source) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "receiver_guard_contract.zr");

    return ZrParser_Parse(g_state, source, strlen(source), sourceName);
}

static SZrAstNode *receiver_guard_contract_statement_expression(
        SZrAstNode *script) {
    SZrAstNode *statement;

    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, script->type);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    TEST_ASSERT_GREATER_THAN_UINT32(
            0u, (TZrUInt32)script->data.script.statements->count);
    statement = script->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(statement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_EXPRESSION_STATEMENT, statement->type);
    return statement->data.expressionStatement.expr;
}

static SZrAstNode *receiver_guard_contract_segment(
        SZrAstNode *expression,
        TZrSize segmentIndex) {
    TEST_ASSERT_NOT_NULL(expression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, expression->type);
    TEST_ASSERT_NOT_NULL(expression->data.primaryExpression.members);
    TEST_ASSERT_GREATER_THAN_UINT32(
            (TZrUInt32)segmentIndex,
            (TZrUInt32)expression->data.primaryExpression.members->count);
    return expression->data.primaryExpression.members->nodes[segmentIndex];
}

static void receiver_guard_contract_append_expression_fact(
        SZrCompilerState *compiler,
        SZrAstNode *node,
        EZrValueType baseType,
        EZrOwnershipQualifier ownership,
        TZrBool isNullable) {
    SZrSemanticExpressionFact fact;

    TEST_ASSERT_NOT_NULL(compiler);
    TEST_ASSERT_NOT_NULL(compiler->semanticContext);
    TEST_ASSERT_NOT_NULL(node);
    memset(&fact, 0, sizeof(fact));
    fact.node = node;
    fact.range = node->location;
    fact.kind = ZR_SEMANTIC_EXPRESSION_FACT_MEMBER;
    fact.exactness = ZR_SEMANTIC_FACT_EXACT;
    if (baseType == ZR_VALUE_TYPE_OBJECT) {
        ZrParser_InferredType_InitFull(
                g_state,
                &fact.inferredType,
                baseType,
                isNullable,
                ZrCore_String_CreateFromNative(g_state, "Resource"));
    } else {
        ZrParser_InferredType_Init(g_state, &fact.inferredType, baseType);
    }
    fact.inferredType.ownershipQualifier = ownership;
    fact.inferredType.isNullable = isNullable;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendExpression(
            compiler->semanticContext, &fact));
    ZrParser_InferredType_Free(g_state, &fact.inferredType);
}

static void assert_receiver_guard_fact_drift_is_rejected_full(
        const TZrChar *source,
        TZrSize factSegmentIndex,
        TZrSize chainSegmentEnd,
        EZrReceiverGuardKind kind,
        EZrReceiverGuardMode mode,
        EZrReceiverGuardResultLift resultLift,
        EZrOwnershipQualifier receiverOwnership,
        TZrBool receiverIsNullable,
        EZrOwnershipQualifier canonicalReceiverOwnership,
        TZrBool canonicalReceiverIsNullable,
        EZrOwnershipQualifier guardedOwnership,
        TZrBool guardedIsNullable,
        EZrValueType resultBaseType,
        const TZrChar *expectedMessage) {
    SZrCompilerState compiler;
    SZrAstNode *script = receiver_guard_contract_parse_source(source);
    SZrAstNode *expression =
            receiver_guard_contract_statement_expression(script);
    SZrAstNode *segment = receiver_guard_contract_segment(
            expression, factSegmentIndex);
    SZrReceiverGuardFact fact;

    memset(&compiler, 0, sizeof(compiler));
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    TEST_ASSERT_NOT_NULL(compiler.semanticContext);
    memset(&fact, 0, sizeof(fact));
    fact.node = segment;
    fact.receiver = factSegmentIndex == 0u
                            ? expression->data.primaryExpression.property
                            : receiver_guard_contract_segment(
                                      expression, factSegmentIndex - 1u);
    fact.firstSegment = segment;
    fact.range = expression->location;
    fact.kind = kind;
    fact.mode = mode;
    fact.resultLift = resultLift;
    fact.chainSegmentStart = factSegmentIndex;
    fact.chainSegmentEnd = chainSegmentEnd;
    ZrParser_InferredType_Init(
            g_state, &fact.receiverType, ZR_VALUE_TYPE_NULL);
    fact.receiverType.isNullable = receiverIsNullable;
    fact.receiverType.ownershipQualifier = receiverOwnership;
    ZrParser_InferredType_Init(
            g_state, &fact.guardedType, ZR_VALUE_TYPE_NULL);
    fact.guardedType.ownershipQualifier =
            guardedOwnership;
    fact.guardedType.isNullable = guardedIsNullable;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReceiverGuard(
            compiler.semanticContext, &fact));
    ZrParser_InferredType_Free(g_state, &fact.receiverType);
    ZrParser_InferredType_Free(g_state, &fact.guardedType);

    receiver_guard_contract_append_expression_fact(
            &compiler,
            fact.receiver,
            ZR_VALUE_TYPE_NULL,
            canonicalReceiverOwnership,
            canonicalReceiverIsNullable);
    receiver_guard_contract_append_expression_fact(
            &compiler,
            expression,
            resultBaseType,
            ZR_OWNERSHIP_QUALIFIER_NONE,
            ZR_FALSE);

    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    ZrParser_Expression_Compile(&compiler, expression);

    TEST_ASSERT_TRUE(compiler.hasError);
    TEST_ASSERT_NOT_NULL(compiler.errorMessage);
    TEST_ASSERT_NOT_NULL_MESSAGE(
            strstr(compiler.errorMessage, expectedMessage),
            compiler.errorMessage);

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_Ast_Free(g_state, script);
    ZrParser_CompilerState_Free(&compiler);
}

static void assert_receiver_guard_fact_drift_is_rejected(
        const TZrChar *source,
        TZrSize factSegmentIndex,
        TZrSize chainSegmentEnd,
        EZrReceiverGuardKind kind,
        EZrReceiverGuardMode mode,
        EZrReceiverGuardResultLift resultLift,
        EZrOwnershipQualifier receiverOwnership,
        TZrBool receiverIsNullable,
        EZrValueType resultBaseType,
        const TZrChar *expectedMessage) {
    EZrOwnershipQualifier guardedOwnership =
            kind == ZR_RECEIVER_GUARD_WEAK_WAKE
                    ? ZR_OWNERSHIP_QUALIFIER_SHARED
                    : receiverOwnership;

    assert_receiver_guard_fact_drift_is_rejected_full(
            source,
            factSegmentIndex,
            chainSegmentEnd,
            kind,
            mode,
            resultLift,
            receiverOwnership,
            receiverIsNullable,
            receiverOwnership,
            receiverIsNullable,
            guardedOwnership,
            ZR_FALSE,
            resultBaseType,
            expectedMessage);
}

static void assert_missing_nullable_callable_guard_is_rejected(void) {
    SZrCompilerState compiler;
    SZrAstNode *script = receiver_guard_contract_parse_source(
            "null?.().value;");
    SZrAstNode *expression =
            receiver_guard_contract_statement_expression(script);
    SZrAstNode *callSegment = receiver_guard_contract_segment(expression, 0u);
    SZrAstNode *trailingSegment = receiver_guard_contract_segment(expression, 1u);
    SZrReceiverGuardFact unrelatedFact;

    memset(&compiler, 0, sizeof(compiler));
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    TEST_ASSERT_NOT_NULL(compiler.semanticContext);

    memset(&unrelatedFact, 0, sizeof(unrelatedFact));
    unrelatedFact.node = trailingSegment;
    unrelatedFact.receiver = callSegment;
    unrelatedFact.firstSegment = trailingSegment;
    unrelatedFact.range = trailingSegment->location;
    unrelatedFact.kind = ZR_RECEIVER_GUARD_NULL;
    unrelatedFact.mode = ZR_RECEIVER_GUARD_DIRECT;
    unrelatedFact.resultLift = ZR_RECEIVER_GUARD_RESULT_UNCHANGED;
    unrelatedFact.chainSegmentStart = 1u;
    unrelatedFact.chainSegmentEnd = 2u;
    ZrParser_InferredType_Init(
            g_state, &unrelatedFact.receiverType, ZR_VALUE_TYPE_OBJECT);
    unrelatedFact.receiverType.isNullable = ZR_TRUE;
    ZrParser_InferredType_Init(
            g_state, &unrelatedFact.guardedType, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReceiverGuard(
            compiler.semanticContext, &unrelatedFact));
    ZrParser_InferredType_Free(g_state, &unrelatedFact.receiverType);
    ZrParser_InferredType_Free(g_state, &unrelatedFact.guardedType);

    receiver_guard_contract_append_expression_fact(
            &compiler,
            expression->data.primaryExpression.property,
            ZR_VALUE_TYPE_FUNCTION,
            ZR_OWNERSHIP_QUALIFIER_NONE,
            ZR_TRUE);

    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    ZrParser_Expression_Compile(&compiler, expression);

    TEST_ASSERT_TRUE(compiler.hasError);
    TEST_ASSERT_NOT_NULL(compiler.errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(
            compiler.errorMessage,
            "Receiver guard fact is missing for guarded chain segment"));

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_Ast_Free(g_state, script);
    ZrParser_CompilerState_Free(&compiler);
}

static void test_receiver_guard_lowering_rejects_fact_drift(void) {
    assert_receiver_guard_fact_drift_is_rejected(
            "null?.a.b;",
            0u,
            1u,
            ZR_RECEIVER_GUARD_NULL,
            ZR_RECEIVER_GUARD_OPTIONAL,
            ZR_RECEIVER_GUARD_RESULT_NULLABLE,
            ZR_OWNERSHIP_QUALIFIER_NONE,
            ZR_TRUE,
            ZR_VALUE_TYPE_INT64,
            "Receiver guard fact has invalid chain bounds");
    assert_receiver_guard_fact_drift_is_rejected(
            "null?.value;",
            0u,
            1u,
            ZR_RECEIVER_GUARD_NULL,
            ZR_RECEIVER_GUARD_DIRECT,
            ZR_RECEIVER_GUARD_RESULT_UNCHANGED,
            ZR_OWNERSHIP_QUALIFIER_NONE,
            ZR_TRUE,
            ZR_VALUE_TYPE_INT64,
            "Receiver guard fact mode does not match access syntax");
    assert_receiver_guard_fact_drift_is_rejected(
            "null?.value;",
            0u,
            1u,
            ZR_RECEIVER_GUARD_WEAK_WAKE,
            ZR_RECEIVER_GUARD_OPTIONAL,
            ZR_RECEIVER_GUARD_RESULT_NULLABLE,
            ZR_OWNERSHIP_QUALIFIER_NONE,
            ZR_TRUE,
            ZR_VALUE_TYPE_INT64,
            "Receiver guard fact kind does not match receiver type");
    assert_receiver_guard_fact_drift_is_rejected(
            "null?.value;",
            0u,
            1u,
            ZR_RECEIVER_GUARD_NULL,
            ZR_RECEIVER_GUARD_OPTIONAL,
            ZR_RECEIVER_GUARD_RESULT_VOID_NOOP,
            ZR_OWNERSHIP_QUALIFIER_NONE,
            ZR_TRUE,
            ZR_VALUE_TYPE_INT64,
            "Receiver guard fact result lift does not match chain result");
    assert_receiver_guard_fact_drift_is_rejected(
            "null?.value;",
            0u,
            1u,
            ZR_RECEIVER_GUARD_NULL,
            ZR_RECEIVER_GUARD_OPTIONAL,
            ZR_RECEIVER_GUARD_RESULT_NULLABLE,
            ZR_OWNERSHIP_QUALIFIER_NONE,
            ZR_TRUE,
            ZR_VALUE_TYPE_NULL,
            "Receiver guard fact result lift does not match chain result");
    assert_receiver_guard_fact_drift_is_rejected(
            "null?.a?.b;",
            1u,
            2u,
            ZR_RECEIVER_GUARD_NULL,
            ZR_RECEIVER_GUARD_OPTIONAL,
            ZR_RECEIVER_GUARD_RESULT_NULLABLE,
            ZR_OWNERSHIP_QUALIFIER_NONE,
            ZR_TRUE,
            ZR_VALUE_TYPE_INT64,
            "Receiver guard fact is missing for guarded chain segment");
}

static void test_receiver_guard_lowering_rejects_canonical_receiver_type_drift(void) {
    assert_receiver_guard_fact_drift_is_rejected_full(
            "null?.value;",
            0u,
            1u,
            ZR_RECEIVER_GUARD_WEAK_WAKE,
            ZR_RECEIVER_GUARD_OPTIONAL,
            ZR_RECEIVER_GUARD_RESULT_NULLABLE,
            ZR_OWNERSHIP_QUALIFIER_WEAK,
            ZR_FALSE,
            ZR_OWNERSHIP_QUALIFIER_NONE,
            ZR_TRUE,
            ZR_OWNERSHIP_QUALIFIER_SHARED,
            ZR_FALSE,
            ZR_VALUE_TYPE_INT64,
            "Receiver guard fact receiver type does not match canonical receiver");
}

static void test_receiver_guard_lowering_rejects_guarded_type_drift(void) {
    assert_receiver_guard_fact_drift_is_rejected_full(
            "null?.value;",
            0u,
            1u,
            ZR_RECEIVER_GUARD_NULL,
            ZR_RECEIVER_GUARD_OPTIONAL,
            ZR_RECEIVER_GUARD_RESULT_NULLABLE,
            ZR_OWNERSHIP_QUALIFIER_NONE,
            ZR_TRUE,
            ZR_OWNERSHIP_QUALIFIER_NONE,
            ZR_TRUE,
            ZR_OWNERSHIP_QUALIFIER_NONE,
            ZR_TRUE,
            ZR_VALUE_TYPE_INT64,
            "Receiver guard fact has an invalid guarded type");
}

static void test_receiver_guard_lowering_rejects_missing_nullable_callable_fact(void) {
    assert_missing_nullable_callable_guard_is_rejected();
}

static void test_direct_weak_guard_preserves_shared_result(void) {
    const TZrChar *source =
            "resource class Leaf {\n"
            "    pub var value: int;\n"
            "    pub @constructor(value: int) { this.value = value; }\n"
            "}\n"
            "resource class Parent {\n"
            "    pub var leaf: Shared<Leaf>;\n"
            "    pub @constructor(leaf: Shared<Leaf>) { this.leaf = leaf; }\n"
            "}\n"
            "fn run(): int {\n"
            "    var leafSeed = own Leaf(41);\n"
            "    var leafShared = share(leafSeed);\n"
            "    var parentSeed = own Parent(leafShared);\n"
            "    var parentShared = share(parentSeed);\n"
            "    var parentWeak = degrade(parentShared);\n"
            "    var result: Shared<Leaf> = parentWeak.leaf;\n"
            "    drop(parentShared);\n"
            "    drop(leafShared);\n"
            "    return result.value;\n"
            "}\n"
            "return run();\n";
    SZrFunction *function = compile_source(source);
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(41, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_mixed_weak_guards_preserve_shared_result(void) {
    const TZrChar *source =
            "resource class Leaf {\n"
            "    pub var value: int;\n"
            "    pub @constructor(value: int) { this.value = value; }\n"
            "}\n"
            "resource class Branch {\n"
            "    pub var leaf: Shared<Leaf>;\n"
            "    pub @constructor(leaf: Shared<Leaf>) { this.leaf = leaf; }\n"
            "}\n"
            "resource class Root {\n"
            "    pub var branch: Weak<Branch>;\n"
            "    pub @constructor(branch: Weak<Branch>) { this.branch = branch; }\n"
            "}\n"
            "fn run(): int {\n"
            "    var leafSeed = own Leaf(73);\n"
            "    var leafShared = share(leafSeed);\n"
            "    var branchSeed = own Branch(leafShared);\n"
            "    var branchShared = share(branchSeed);\n"
            "    var branchWeak = degrade(branchShared);\n"
            "    var rootSeed = own Root(branchWeak);\n"
            "    var rootShared = share(rootSeed);\n"
            "    var rootWeak = degrade(rootShared);\n"
            "    var result = rootWeak?.branch.leaf;\n"
            "    drop(rootShared);\n"
            "    drop(branchShared);\n"
            "    drop(leafShared);\n"
            "    var value = result?.value;\n"
            "    if (value == 73) { return 73; }\n"
            "    return -1;\n"
            "}\n"
            "return run();\n";
    SZrFunction *function = compile_source(source);
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(73, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_mixed_weak_optional_chain_boundaries(void) {
    const TZrChar *source =
            "resource class Child {\n"
            "    pub var value: int;\n"
            "    pub @constructor(value: int) { this.value = value; }\n"
            "}\n"
            "resource class Parent {\n"
            "    pub var child: Weak<Child>;\n"
            "    pub @constructor(child: Weak<Child>) { this.child = child; }\n"
            "}\n"
            "fn run(): int {\n"
            "    var childSeed = own Child(7);\n"
            "    var childShared = share(childSeed);\n"
            "    var childWeak = degrade(childShared);\n"
            "    var parentSeed = own Parent(childWeak);\n"
            "    var parentShared = share(parentSeed);\n"
            "    var parentWeak = degrade(parentShared);\n"
            "    var liveDirect = parentWeak?.child.value;\n"
            "    var liveOptional = parentWeak?.child?.value;\n"
            "    if (liveDirect != 7 || liveOptional != 7) { return 32; }\n"
            "    drop(childShared);\n"
            "    var mask = 0;\n"
            "    try { var absentDirect = parentWeak?.child.value; }\n"
            "    catch (error: NullReferenceError) { mask = mask + 1; }\n"
            "    var absentOptional = parentWeak?.child?.value;\n"
            "    if (absentOptional == null) { mask = mask + 2; }\n"
            "    drop(parentShared);\n"
            "    var skippedSuffix = parentWeak?.child.value;\n"
            "    if (skippedSuffix == null) { mask = mask + 4; }\n"
            "    return mask;\n"
            "}\n"
            "return run();\n";
    SZrFunction *function;
    TZrInt64 result = 0;

    ZrParser_ToGlobalState_Register(g_state);
    TEST_ASSERT_TRUE(ZrVmLibSystem_Register(g_state->global));
    function = compile_source(source);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(
            10u,
            receiver_guard_contract_count_opcode(
                    function, ZR_INSTRUCTION_ENUM(OWN_WAKE), 0u));
    receiver_guard_contract_assert_wake_cleanup_markers(function, 0u);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(7, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_repeated_weak_live_expire_wake_transitions(void) {
    const TZrChar *source =
            "resource class Service {\n"
            "    pub var value: int;\n"
            "    pub @constructor(value: int) { this.value = value; }\n"
            "    pub const fn read(): int { return this.value; }\n"
            "}\n"
            "fn run(): int {\n"
            "    var iteration = 0;\n"
            "    var successes = 0;\n"
            "    while (iteration < 32) {\n"
            "        var seed = own Service(iteration);\n"
            "        var shared = share(seed);\n"
            "        var weak = degrade(shared);\n"
            "        var live = weak?.read();\n"
            "        drop(shared);\n"
            "        var expired = wake(weak);\n"
            "        var skipped = weak?.read();\n"
            "        if (live == iteration && expired == null && skipped == null) {\n"
            "            successes = successes + 1;\n"
            "        }\n"
            "        drop(expired);\n"
            "        iteration = iteration + 1;\n"
            "    }\n"
            "    return successes;\n"
            "}\n"
            "return run();\n";
    SZrFunction *function = compile_source(source);
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(32, result);

    ZrCore_Function_Free(g_state, function);
}

#endif
