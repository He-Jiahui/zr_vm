static void assert_assignment_reference_diagnostic(
        const char *source,
        EZrValueType targetBase,
        EZrValueType sourceBase,
        const char *targetName,
        const char *sourceTypeName,
        EZrOwnershipQualifier targetOwnership,
        EZrOwnershipQualifier sourceOwnership,
        TZrUInt32 descriptorId) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName = ZrCore_String_Create(
            g_state, "assignment_reference_diagnostic.zr",
            strlen("assignment_reference_diagnostic.zr"));
    SZrAstNode *ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    SZrAstNode *targetDeclaration = script_statement_at(ast, 0U);
    SZrAstNode *sourceDeclaration = script_statement_at(ast, 1U);
    SZrAstNode *assignment = expression_statement_expression_at(ast, 2U);
    SZrAstNode *targetPattern;
    SZrAstNode *sourcePattern;
    SZrInferredType targetType;
    SZrInferredType sourceType;
    SZrInferredType result;
    const SZrSemanticReferenceFact *writeFact;
    const SZrSemanticReferenceFact *readFact;

    TEST_ASSERT_NOT_NULL(targetDeclaration);
    TEST_ASSERT_NOT_NULL(sourceDeclaration);
    TEST_ASSERT_NOT_NULL(assignment);
    cs->suppressErrorOutput = ZR_TRUE;
    targetPattern = targetDeclaration->data.variableDeclaration.pattern;
    sourcePattern = sourceDeclaration->data.variableDeclaration.pattern;
    ZrParser_InferredType_Init(g_state, &targetType, targetBase);
    ZrParser_InferredType_Init(g_state, &sourceType, sourceBase);
    targetType.ownershipQualifier = targetOwnership;
    sourceType.ownershipQualifier = sourceOwnership;
    if (targetName != ZR_NULL) {
        targetType.typeName = ZrCore_String_Create(
                g_state, (TZrNativeString)targetName, strlen(targetName));
    }
    if (sourceTypeName != ZR_NULL) {
        sourceType.typeName = ZrCore_String_Create(
                g_state, (TZrNativeString)sourceTypeName, strlen(sourceTypeName));
    }
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariableEx(
            g_state, cs->typeEnv, targetPattern->data.identifier.name,
            &targetType, targetDeclaration, targetPattern->location));
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariableEx(
            g_state, cs->typeEnv, sourcePattern->data.identifier.name,
            &sourceType, sourceDeclaration, sourcePattern->location));

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_FALSE(ZrParser_ExpressionType_Infer(cs, assignment, &result));
    TEST_ASSERT_TRUE(cs->hasStructuredError);
    TEST_ASSERT_EQUAL_UINT32(descriptorId, cs->structuredError.descriptorId);
    TEST_ASSERT_EQUAL_UINT64(assignment->data.assignmentExpression.right->location.start.offset,
                             cs->structuredError.location.start.offset);
    TEST_ASSERT_EQUAL_UINT64(assignment->data.assignmentExpression.right->location.end.offset,
                             cs->structuredError.location.end.offset);

    writeFact = ZrParser_SemanticFacts_FindReferenceAtPosition(
            cs->semanticContext, assignment->data.assignmentExpression.left->location);
    readFact = ZrParser_SemanticFacts_FindReferenceAtPosition(
            cs->semanticContext, assignment->data.assignmentExpression.right->location);
    TEST_ASSERT_NOT_NULL(writeFact);
    TEST_ASSERT_NOT_NULL(readFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REFERENCE_WRITE, writeFact->kind);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REFERENCE_READ, readFact->kind);
    TEST_ASSERT_TRUE(writeFact->isResolved);
    TEST_ASSERT_TRUE(readFact->isResolved);
    TEST_ASSERT_NOT_EQUAL(writeFact->symbolId, readFact->symbolId);

    if (descriptorId == 2008U) {
        const SZrSemanticOwnershipFact *ownership =
                ZrParser_SemanticFacts_FindOwnershipAtPosition(
                        cs->semanticContext,
                        assignment->data.assignmentExpression.right->location);
        TEST_ASSERT_NOT_NULL(ownership);
        TEST_ASSERT_TRUE(ownership->isViolation);
        TEST_ASSERT_EQUAL_INT(sourceOwnership, ownership->qualifier);
        TEST_ASSERT_EQUAL_UINT64(cs->structuredError.location.start.offset,
                                 ownership->range.start.offset);
        TEST_ASSERT_EQUAL_UINT64(cs->structuredError.location.end.offset,
                                 ownership->range.end.offset);
    } else {
        const SZrStructuredDiagnosticRelatedInformation *related =
                (const SZrStructuredDiagnosticRelatedInformation *)ZrCore_Array_Get(
                        &cs->structuredError.relatedInformation, 0U);
        TEST_ASSERT_NOT_NULL(related);
        TEST_ASSERT_EQUAL_UINT64(
                targetDeclaration->data.variableDeclaration.typeInfo->name->location.start.offset,
                related->location.start.offset);
        TEST_ASSERT_EQUAL_UINT64(
                targetDeclaration->data.variableDeclaration.typeInfo->name->location.end.offset,
                related->location.end.offset);
    }

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &sourceType);
    ZrParser_InferredType_Free(g_state, &targetType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_assignment_write_fact_preserves_ownership_diagnostic_range(void) {
    assert_assignment_reference_diagnostic(
            "var target: Unique<Resource>;\nvar source: Shared<Resource>;\ntarget = source;",
            ZR_VALUE_TYPE_OBJECT, ZR_VALUE_TYPE_OBJECT, "Resource", "Resource",
            ZR_OWNERSHIP_QUALIFIER_UNIQUE, ZR_OWNERSHIP_QUALIFIER_SHARED, 2008U);
}

static void test_assignment_write_fact_preserves_type_diagnostic_ranges(void) {
    assert_assignment_reference_diagnostic(
            "var target: int;\nvar source: string;\ntarget = source;",
            ZR_VALUE_TYPE_INT64, ZR_VALUE_TYPE_STRING, ZR_NULL, ZR_NULL,
            ZR_OWNERSHIP_QUALIFIER_NONE, ZR_OWNERSHIP_QUALIFIER_NONE, 2011U);
}

static void test_assignment_write_fact_rejects_incompatible_object_type(void) {
    assert_assignment_reference_diagnostic(
            "var target: Left;\nvar source: Right;\ntarget = source;",
            ZR_VALUE_TYPE_OBJECT, ZR_VALUE_TYPE_OBJECT, "Left", "Right",
            ZR_OWNERSHIP_QUALIFIER_NONE, ZR_OWNERSHIP_QUALIFIER_NONE, 2011U);
}

static void test_assignment_write_fact_rejects_null_nonnullable_owner(void) {
    assert_assignment_reference_diagnostic(
            "var target: Unique<Resource>;\nvar source;\ntarget = source;",
            ZR_VALUE_TYPE_OBJECT, ZR_VALUE_TYPE_NULL, "Resource", ZR_NULL,
            ZR_OWNERSHIP_QUALIFIER_UNIQUE, ZR_OWNERSHIP_QUALIFIER_NONE, 2011U);
}
