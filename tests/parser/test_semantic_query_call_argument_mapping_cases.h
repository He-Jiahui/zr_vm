#ifndef ZR_TEST_SEMANTIC_QUERY_CALL_ARGUMENT_MAPPING_CASES_H
#define ZR_TEST_SEMANTIC_QUERY_CALL_ARGUMENT_MAPPING_CASES_H

static void test_call_at_projects_source_argument_mapping_and_conversion(void) {
    const TZrChar *source =
            "fn reorder(first: int, second: float): float { return second; }\n"
            "fn widen(value: float): float { return value; }\n"
            "fn caller(): float {\n"
            "    var result = reorder(second: 2.5, first: 1);\n"
            "    return result + widen(1);\n"
            "}\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "semantic_call_argument_mapping.zr");
    SZrAstNode *ast;
    SZrCompilerState cs;
    SZrParserSemanticCallQuery reorderCall;
    SZrParserSemanticCallQuery widenCall;
    SZrSemanticCallArgumentFact *firstMapping;
    SZrSemanticCallArgumentFact *secondMapping;
    const SZrSemanticCallArgumentFact *mapping;
    TZrTypeId originalParameterTypeId;
    TZrSize originalParameterIndex;
    EZrParameterPassingMode originalPassingMode;
    EZrSemanticCallConversion originalConversion;
    const TZrChar *argumentText;

    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);

    TEST_ASSERT_FALSE_MESSAGE(cs.hasError, cs.errorMessage);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    memset(&reorderCall, 0, sizeof(reorderCall));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_CallAt(
            cs.semanticContext,
            call_source_position(source, sourceName, "reorder", 1U),
            ZR_NULL,
            &reorderCall));
    TEST_ASSERT_NOT_NULL(reorderCall.argumentMappings);
    TEST_ASSERT_TRUE(reorderCall.argumentMappings->isValid);
    TEST_ASSERT_EQUAL_UINT(2U, reorderCall.argumentMappings->length);

    firstMapping = (SZrSemanticCallArgumentFact *)ZrCore_Array_Get(
            (SZrArray *)reorderCall.argumentMappings, 0U);
    mapping = firstMapping;
    TEST_ASSERT_NOT_NULL(mapping);
    TEST_ASSERT_EQUAL_UINT(0U, mapping->argumentIndex);
    TEST_ASSERT_EQUAL_UINT(1U, mapping->parameterIndex);
    TEST_ASSERT_TRUE(mapping->isNamed);
    TEST_ASSERT_EQUAL_INT(ZR_PARAMETER_PASSING_MODE_VALUE, mapping->passingMode);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_CALL_CONVERSION_EXACT, mapping->conversion);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, mapping->argumentTypeId);
    TEST_ASSERT_EQUAL_UINT64(mapping->argumentTypeId, mapping->parameterTypeId);
    argumentText = strstr(source, "2.5");
    TEST_ASSERT_NOT_NULL(argumentText);
    TEST_ASSERT_EQUAL_UINT((TZrSize)(argumentText - source),
                           mapping->argumentRange.start.offset);
    TEST_ASSERT_EQUAL_UINT((TZrSize)(argumentText - source) + 3U,
                           mapping->argumentRange.end.offset);
    TEST_ASSERT_TRUE(mapping->argumentRange.source == sourceName);

    secondMapping = (SZrSemanticCallArgumentFact *)ZrCore_Array_Get(
            (SZrArray *)reorderCall.argumentMappings, 1U);
    mapping = secondMapping;
    TEST_ASSERT_NOT_NULL(mapping);
    TEST_ASSERT_EQUAL_UINT(1U, mapping->argumentIndex);
    TEST_ASSERT_EQUAL_UINT(0U, mapping->parameterIndex);
    TEST_ASSERT_TRUE(mapping->isNamed);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_CALL_CONVERSION_EXACT, mapping->conversion);

    memset(&widenCall, 0, sizeof(widenCall));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_CallAt(
            cs.semanticContext,
            call_source_position(source, sourceName, "widen", 1U),
            ZR_NULL,
            &widenCall));
    TEST_ASSERT_NOT_NULL(widenCall.argumentMappings);
    TEST_ASSERT_EQUAL_UINT(1U, widenCall.argumentMappings->length);
    mapping = (const SZrSemanticCallArgumentFact *)ZrCore_Array_Get(
            (SZrArray *)widenCall.argumentMappings, 0U);
    TEST_ASSERT_NOT_NULL(mapping);
    TEST_ASSERT_EQUAL_UINT(0U, mapping->argumentIndex);
    TEST_ASSERT_EQUAL_UINT(0U, mapping->parameterIndex);
    TEST_ASSERT_FALSE(mapping->isNamed);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_CALL_CONVERSION_IMPLICIT, mapping->conversion);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, mapping->argumentTypeId);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, mapping->parameterTypeId);
    TEST_ASSERT_NOT_EQUAL(mapping->argumentTypeId, mapping->parameterTypeId);

    TEST_ASSERT_NOT_NULL(firstMapping);
    TEST_ASSERT_NOT_NULL(secondMapping);
    firstMapping->parameterIndex = 2U;
    memset(&reorderCall, 0xA5, sizeof(reorderCall));
    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_CallAt(
            cs.semanticContext,
            call_source_position(source, sourceName, "reorder", 1U),
            ZR_NULL,
            &reorderCall));
    TEST_ASSERT_NULL(reorderCall.argumentMappings);
    TEST_ASSERT_NULL(reorderCall.expression);
    TEST_ASSERT_NULL(reorderCall.reference);
    firstMapping->parameterIndex = 1U;

    originalParameterTypeId = firstMapping->parameterTypeId;
    firstMapping->parameterTypeId = secondMapping->parameterTypeId;
    memset(&reorderCall, 0xA5, sizeof(reorderCall));
    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_CallAt(
            cs.semanticContext,
            call_source_position(source, sourceName, "reorder", 1U),
            ZR_NULL,
            &reorderCall));
    TEST_ASSERT_NULL(reorderCall.argumentMappings);
    TEST_ASSERT_NULL(reorderCall.expression);
    TEST_ASSERT_NULL(reorderCall.reference);
    firstMapping->parameterTypeId = originalParameterTypeId;

    originalParameterIndex = secondMapping->parameterIndex;
    originalParameterTypeId = secondMapping->parameterTypeId;
    originalConversion = secondMapping->conversion;
    secondMapping->parameterIndex = firstMapping->parameterIndex;
    secondMapping->parameterTypeId = firstMapping->parameterTypeId;
    secondMapping->conversion = ZR_SEMANTIC_CALL_CONVERSION_IMPLICIT;
    memset(&reorderCall, 0xA5, sizeof(reorderCall));
    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_CallAt(
            cs.semanticContext,
            call_source_position(source, sourceName, "reorder", 1U),
            ZR_NULL,
            &reorderCall));
    TEST_ASSERT_NULL(reorderCall.argumentMappings);
    TEST_ASSERT_NULL(reorderCall.expression);
    TEST_ASSERT_NULL(reorderCall.reference);
    secondMapping->parameterIndex = originalParameterIndex;
    secondMapping->parameterTypeId = originalParameterTypeId;
    secondMapping->conversion = originalConversion;

    originalPassingMode = firstMapping->passingMode;
    firstMapping->passingMode = ZR_PARAMETER_PASSING_MODE_REF;
    memset(&reorderCall, 0xA5, sizeof(reorderCall));
    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_CallAt(
            cs.semanticContext,
            call_source_position(source, sourceName, "reorder", 1U),
            ZR_NULL,
            &reorderCall));
    TEST_ASSERT_NULL(reorderCall.argumentMappings);
    TEST_ASSERT_NULL(reorderCall.expression);
    TEST_ASSERT_NULL(reorderCall.reference);
    firstMapping->passingMode = originalPassingMode;

    originalConversion = firstMapping->conversion;
    firstMapping->conversion = ZR_SEMANTIC_CALL_CONVERSION_IMPLICIT;
    memset(&reorderCall, 0xA5, sizeof(reorderCall));
    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_CallAt(
            cs.semanticContext,
            call_source_position(source, sourceName, "reorder", 1U),
            ZR_NULL,
            &reorderCall));
    TEST_ASSERT_NULL(reorderCall.argumentMappings);
    TEST_ASSERT_NULL(reorderCall.expression);
    TEST_ASSERT_NULL(reorderCall.reference);
    firstMapping->conversion = originalConversion;

    call_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

#endif
