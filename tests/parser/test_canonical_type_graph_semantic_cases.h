#ifndef ZR_VM_TEST_CANONICAL_TYPE_GRAPH_SEMANTIC_CASES_H
#define ZR_VM_TEST_CANONICAL_TYPE_GRAPH_SEMANTIC_CASES_H

static void test_legacy_inferred_types_project_to_structural_type_ids(void) {
    const SZrSemanticTypeRecord *structuralIntRecord;
    SZrInferredType rangedInt;
    SZrInferredType otherInt;
    SZrInferredType genericType;
    SZrInferredType genericArgument;
    SZrInferredType arrayType;
    SZrInferredType arrayElement;
    TZrTypeId firstIntType;
    TZrTypeId sameIntType;
    TZrTypeId namedType;
    TZrTypeId sameNamedType;
    TZrTypeId genericTypeId;
    TZrTypeId arrayTypeId;
    TZrChar buffer[256];
    SZrAstNode astNodes[4];

    ZrParser_InferredType_Init(g_state, &rangedInt, ZR_VALUE_TYPE_INT64);
    rangedInt.hasRangeConstraint = ZR_TRUE;
    rangedInt.minValue = 1;
    rangedInt.maxValue = 4;
    rangedInt.knownBoolValue = ZR_TRUE;
    rangedInt.hasKnownBoolValue = ZR_TRUE;
    rangedInt.arrayFixedSize = 3U;
    rangedInt.arrayMinSize = 3U;
    rangedInt.arrayMaxSize = 3U;
    rangedInt.hasArraySizeConstraint = ZR_TRUE;
    ZrParser_InferredType_Init(g_state, &otherInt, ZR_VALUE_TYPE_INT64);

    firstIntType = ZrParser_Semantic_RegisterInferredType(
            g_context,
            &rangedInt,
            ZR_SEMANTIC_TYPE_KIND_VALUE,
            ZrCore_String_Create(g_state, "first", 5),
            &astNodes[0]);
    sameIntType = ZrParser_Semantic_RegisterInferredType(
            g_context,
            &otherInt,
            ZR_SEMANTIC_TYPE_KIND_VALUE,
            ZrCore_String_Create(g_state, "second", 6),
            &astNodes[1]);
    TEST_ASSERT_EQUAL_UINT32(firstIntType, sameIntType);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)g_context->types.length);
    structuralIntRecord = (const SZrSemanticTypeRecord *)ZrCore_Array_Get(
            &g_context->types,
            0U);
    TEST_ASSERT_NOT_NULL(structuralIntRecord);
    TEST_ASSERT_FALSE(structuralIntRecord->inferredType.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(0, structuralIntRecord->inferredType.minValue);
    TEST_ASSERT_EQUAL_INT64(0, structuralIntRecord->inferredType.maxValue);
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)structuralIntRecord->inferredType.rangeSegmentCount);
    TEST_ASSERT_FALSE(structuralIntRecord->inferredType.hasKnownBoolValue);
    TEST_ASSERT_FALSE(structuralIntRecord->inferredType.hasArraySizeConstraint);
    TEST_ASSERT_NULL(structuralIntRecord->name);
    TEST_ASSERT_NULL(structuralIntRecord->astNode);
    TEST_ASSERT_EQUAL_INT(
            ZR_CANONICAL_TYPE_PRIMITIVE,
            ZrParser_CanonicalType_Find(g_context, firstIntType)->kind);

    namedType = ZrParser_Semantic_RegisterNamedType(
            g_context,
            ZrCore_String_Create(g_state, "app.Point", 9),
            ZR_SEMANTIC_TYPE_KIND_VALUE,
            &astNodes[2]);
    sameNamedType = ZrParser_Semantic_RegisterNamedType(
            g_context,
            ZrCore_String_Create(g_state, "app.Point", 9),
            ZR_SEMANTIC_TYPE_KIND_VALUE,
            &astNodes[3]);
    TEST_ASSERT_EQUAL_UINT32(namedType, sameNamedType);
    assert_canonical_type_format(namedType, "app.Point");

    ZrParser_InferredType_InitFull(
            g_state,
            &genericType,
            ZR_VALUE_TYPE_OBJECT,
            ZR_FALSE,
            ZrCore_String_Create(g_state, "app.Box<int>", 12));
    ZrParser_InferredType_Init(g_state, &genericArgument, ZR_VALUE_TYPE_INT64);
    ZrCore_Array_Init(g_state, &genericType.elementTypes, sizeof(SZrInferredType), 1);
    ZrCore_Array_Push(g_state, &genericType.elementTypes, &genericArgument);
    genericTypeId = ZrParser_Semantic_RegisterInferredType(
            g_context,
            &genericType,
            ZR_SEMANTIC_TYPE_KIND_GENERIC_INSTANCE,
            genericType.typeName,
            ZR_NULL);
    assert_canonical_type_format(genericTypeId, "app.Box<int>");

    ZrParser_InferredType_Init(g_state, &arrayType, ZR_VALUE_TYPE_ARRAY);
    ZrParser_InferredType_Init(g_state, &arrayElement, ZR_VALUE_TYPE_INT64);
    ZrCore_Array_Init(g_state, &arrayType.elementTypes, sizeof(SZrInferredType), 1);
    ZrCore_Array_Push(g_state, &arrayType.elementTypes, &arrayElement);
    arrayType.ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_SHARED;
    arrayType.isNullable = ZR_TRUE;
    arrayTypeId = ZrParser_Semantic_RegisterInferredType(
            g_context,
            &arrayType,
            ZR_SEMANTIC_TYPE_KIND_REFERENCE,
            ZR_NULL,
            ZR_NULL);
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_Format(
            g_context,
            arrayTypeId,
            buffer,
            sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("Shared<int[]>?", buffer);

    ZrParser_InferredType_Free(g_state, &rangedInt);
    ZrParser_InferredType_Free(g_state, &otherInt);
    ZrParser_InferredType_Free(g_state, &genericType);
    ZrParser_InferredType_Free(g_state, &arrayType);
}

static void test_function_registration_uses_function_type_id(void) {
    SZrTypeEnvironment *environment = ZrParser_TypeEnvironment_New(g_state);
    SZrInferredType returnType;
    SZrInferredType parameterType;
    SZrArray parameterTypes;
    SZrArray passingModes;
    SZrArray genericParameterTypes;
    SZrArray genericParameters;
    EZrParameterPassingMode passingMode = ZR_PARAMETER_PASSING_MODE_OUT;
    SZrFunctionTypeInfo *functionInfo = ZR_NULL;
    SZrFunctionTypeInfo *genericFunctionInfo = ZR_NULL;
    const SZrCanonicalTypeNode *functionNode;
    const SZrCanonicalTypeNode *genericFunctionNode;
    const SZrCanonicalTypeNode *genericParameterNode;
    SZrInferredType genericType;
    SZrInferredType invalidType;
    SZrTypeGenericParameterInfo genericParameterInfo;
    TZrSize functionCountBeforeFailure;
    TZrSize symbolCountBeforeFailure;

    TEST_ASSERT_NOT_NULL(environment);
    environment->semanticContext = g_context;
    ZrParser_InferredType_Init(g_state, &returnType, ZR_VALUE_TYPE_BOOL);
    ZrParser_InferredType_Init(g_state, &parameterType, ZR_VALUE_TYPE_INT64);
    ZrCore_Array_Init(g_state, &parameterTypes, sizeof(SZrInferredType), 1);
    ZrCore_Array_Push(g_state, &parameterTypes, &parameterType);
    ZrCore_Array_Init(g_state, &passingModes, sizeof(EZrParameterPassingMode), 1);
    ZrCore_Array_Push(g_state, &passingModes, &passingMode);

    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterFunctionEx(
            g_state,
            environment,
            ZrCore_String_Create(g_state, "tryRead", 7),
            &returnType,
            &parameterTypes,
            ZR_NULL,
            &passingModes,
            ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_LookupFunction(
            environment,
            ZrCore_String_Create(g_state, "tryRead", 7),
            &functionInfo));
    TEST_ASSERT_NOT_NULL(functionInfo);
    functionNode = ZrParser_CanonicalType_Find(g_context, functionInfo->typeId);
    TEST_ASSERT_NOT_NULL(functionNode);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_FUNCTION, functionNode->kind);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)functionNode->data.function.parameterContracts.length);
    TEST_ASSERT_EQUAL_INT(
            ZR_CANONICAL_PASSING_OUT,
            ((SZrCanonicalParameterContract *)ZrCore_Array_Get(
                    (SZrArray *)&functionNode->data.function.parameterContracts,
                    0))->passingForm);
    TEST_ASSERT_EQUAL_INT(
            ZR_CANONICAL_TYPE_REF,
            ZrParser_CanonicalType_Find(
                    g_context,
                    ((SZrCanonicalParameterContract *)ZrCore_Array_Get(
                            (SZrArray *)&functionNode->data.function.parameterContracts,
                            0))->typeId)->kind);

    ZrParser_InferredType_InitFull(
            g_state,
            &genericType,
            ZR_VALUE_TYPE_OBJECT,
            ZR_FALSE,
            ZrCore_String_Create(g_state, "T", 1));
    ZrCore_Array_Init(g_state, &genericParameterTypes, sizeof(SZrInferredType), 1U);
    ZrCore_Array_Push(g_state, &genericParameterTypes, &genericType);
    memset(&genericParameterInfo, 0, sizeof(genericParameterInfo));
    genericParameterInfo.name = genericType.typeName;
    genericParameterInfo.genericKind = ZR_GENERIC_PARAMETER_TYPE;
    ZrCore_Array_Construct(&genericParameterInfo.constraintTypeNames);
    ZrCore_Array_Init(g_state, &genericParameters, sizeof(SZrTypeGenericParameterInfo), 1U);
    ZrCore_Array_Push(g_state, &genericParameters, &genericParameterInfo);

    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterFunctionEx(
            g_state,
            environment,
            ZrCore_String_Create(g_state, "identity", 8),
            &genericType,
            &genericParameterTypes,
            &genericParameters,
            ZR_NULL,
            ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_LookupFunction(
            environment,
            ZrCore_String_Create(g_state, "identity", 8),
            &genericFunctionInfo));
    TEST_ASSERT_NOT_NULL(genericFunctionInfo);
    genericFunctionNode = ZrParser_CanonicalType_Find(g_context, genericFunctionInfo->typeId);
    TEST_ASSERT_NOT_NULL(genericFunctionNode);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_FUNCTION, genericFunctionNode->kind);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)genericFunctionNode->data.function.parameterContracts.length);
    genericParameterNode = ZrParser_CanonicalType_Find(
            g_context,
            ((SZrCanonicalParameterContract *)ZrCore_Array_Get(
                    (SZrArray *)&genericFunctionNode->data.function.parameterContracts,
                    0U))->typeId);
    TEST_ASSERT_NOT_NULL(genericParameterNode);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_GENERIC_PARAMETER, genericParameterNode->kind);
    TEST_ASSERT_EQUAL_UINT32(
            genericFunctionInfo->symbolId,
            genericParameterNode->data.genericParameter.ownerSymbolId);
    TEST_ASSERT_EQUAL_UINT32(0U, genericParameterNode->data.genericParameter.ordinal);
    TEST_ASSERT_EQUAL_UINT32(
            genericParameterNode->id,
            genericFunctionNode->data.function.returnTypeId);

    functionCountBeforeFailure = environment->functionReturnTypes.length;
    symbolCountBeforeFailure = g_context->symbols.length;
    ZrParser_InferredType_Init(g_state, &invalidType, ZR_VALUE_TYPE_INT64);
    invalidType.ownershipQualifier = (EZrOwnershipQualifier)99;
    TEST_ASSERT_FALSE(ZrParser_TypeEnvironment_RegisterFunctionEx(
            g_state,
            environment,
            ZrCore_String_Create(g_state, "invalid", 7),
            &invalidType,
            ZR_NULL,
            ZR_NULL,
            ZR_NULL,
            ZR_NULL));
    TEST_ASSERT_EQUAL_UINT32(
            (TZrUInt32)functionCountBeforeFailure,
            (TZrUInt32)environment->functionReturnTypes.length);
    TEST_ASSERT_EQUAL_UINT32(
            (TZrUInt32)symbolCountBeforeFailure,
            (TZrUInt32)g_context->symbols.length);

    ZrParser_TypeEnvironment_Free(g_state, environment);
    ZrParser_InferredType_Free(g_state, &returnType);
    ZrParser_InferredType_Free(g_state, &parameterType);
    ZrParser_InferredType_Free(g_state, &genericType);
    ZrParser_InferredType_Free(g_state, &invalidType);
    ZrCore_Array_Free(g_state, &parameterTypes);
    ZrCore_Array_Free(g_state, &passingModes);
    ZrCore_Array_Free(g_state, &genericParameterTypes);
    ZrCore_Array_Free(g_state, &genericParameters);
}

static void test_generic_constructor_substitution_is_closed_and_kind_aware(void) {
    TZrTypeId intType = ZrParser_CanonicalType_InternPrimitive(g_context, ZR_VALUE_TYPE_INT64);
    TZrTypeId boxDefinition = ZrParser_CanonicalType_InternNominal(
            g_context,
            ZrCore_String_Create(g_state, "app", 3U),
            ZrCore_String_Create(g_state, "Box", 3U),
            0x02001001U);
    TZrTypeId listDefinition = ZrParser_CanonicalType_InternNominal(
            g_context,
            ZrCore_String_Create(g_state, "app", 3U),
            ZrCore_String_Create(g_state, "List", 4U),
            0x02001002U);
    TZrTypeId matrixDefinition = ZrParser_CanonicalType_InternNominal(
            g_context,
            ZrCore_String_Create(g_state, "app", 3U),
            ZrCore_String_Create(g_state, "Matrix", 6U),
            0x02001003U);
    TZrTypeId optionDefinition = ZrParser_CanonicalType_InternNominal(
            g_context,
            ZrCore_String_Create(g_state, "app", 3U),
            ZrCore_String_Create(g_state, "Option", 6U),
            0x02001004U);
    TZrSymbolId boxOwner = 7101U;
    TZrSymbolId matrixOwner = 7102U;
    TZrSymbolId optionOwner = 7103U;
    TZrTypeId boxParameter = ZrParser_CanonicalType_InternGenericParameter(g_context, boxOwner, 0U);
    TZrTypeId matrixParameter = ZrParser_CanonicalType_InternGenericParameter(g_context, matrixOwner, 0U);
    TZrTypeId optionParameter = ZrParser_CanonicalType_InternGenericParameter(g_context, optionOwner, 0U);
    TZrTypeId boxTypeArguments[1] = {intType};
    TZrTypeId boxOfInt = ZrParser_CanonicalType_InternGenericInstance(
            g_context,
            boxDefinition,
            boxTypeArguments,
            1U);
    TZrTypeId listPatternArguments[1] = {boxParameter};
    TZrTypeId listActualArguments[1] = {intType};
    TZrTypeId listOfParameter = ZrParser_CanonicalType_InternGenericInstance(
            g_context,
            listDefinition,
            listPatternArguments,
            1U);
    TZrTypeId listOfInt = ZrParser_CanonicalType_InternGenericInstance(
            g_context,
            listDefinition,
            listActualArguments,
            1U);
    EZrCanonicalGenericArgumentKind matrixParameterKinds[2] = {
            ZR_CANONICAL_GENERIC_ARGUMENT_TYPE,
            ZR_CANONICAL_GENERIC_ARGUMENT_CONST_INT,
    };
    SZrCanonicalGenericArgument matrixArguments[2];
    SZrCanonicalGenericArgument matrixPatternArguments[2];
    SZrCanonicalGenericArgument wrongKindArguments[2];
    TZrTypeId matrixOpenPattern;
    TZrTypeId matrixOfFour;
    TZrTypeId matrixOfFourAgain;
    TZrTypeId matrixWrongArity;
    TZrTypeId matrixWrongKind;
    TZrTypeId optionProjection = ZrParser_CanonicalType_InternUnion(
            g_context,
            optionDefinition,
            &optionParameter,
            1U);
    TZrTypeId optionOfInt = ZrParser_CanonicalType_InternGenericInstance(
            g_context,
            optionDefinition,
            &intType,
            1U);
    TZrTypeId closedOptionProjection;
    TZrSymbolId constructorSymbol = ZR_SEMANTIC_ID_INVALID;

    TEST_ASSERT_TRUE(ZrParser_CanonicalType_RegisterGenericDefinition(
            g_context,
            boxDefinition,
            boxOwner,
            1U,
            ZR_CANONICAL_TYPE_CAPABILITY_VALUE_TYPE |
                    ZR_CANONICAL_TYPE_CAPABILITY_VALUE_CONSTRUCTIBLE,
            ZR_CANONICAL_GC_SCAN_FREE));
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_RegisterConstructor(
            g_context,
            boxDefinition,
            7201U,
            &boxParameter,
            1U,
            ZR_TRUE));
    TEST_ASSERT_FALSE(ZrParser_CanonicalType_ResolveValueConstructor(
            g_context,
            boxOfInt,
            &boxParameter,
            1U,
            &constructorSymbol));
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_RegisterConstructor(
            g_context,
            boxDefinition,
            7202U,
            &listOfParameter,
            1U,
            ZR_TRUE));
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_ResolveValueConstructor(
            g_context,
            boxOfInt,
            &listOfInt,
            1U,
            &constructorSymbol));
    TEST_ASSERT_EQUAL_UINT32(7202U, constructorSymbol);

    matrixArguments[0].kind = ZR_CANONICAL_GENERIC_ARGUMENT_TYPE;
    matrixArguments[0].data.typeId = intType;
    matrixArguments[1].kind = ZR_CANONICAL_GENERIC_ARGUMENT_CONST_INT;
    matrixArguments[1].data.constIntValue = 4;
    matrixPatternArguments[0].kind = ZR_CANONICAL_GENERIC_ARGUMENT_TYPE;
    matrixPatternArguments[0].data.typeId = matrixParameter;
    matrixPatternArguments[1].kind = ZR_CANONICAL_GENERIC_ARGUMENT_CONST_PARAMETER;
    matrixPatternArguments[1].data.constParameter.ownerSymbolId = matrixOwner;
    matrixPatternArguments[1].data.constParameter.ordinal = 1U;
    matrixPatternArguments[1].data.constParameter.displayName =
            ZrCore_String_Create(g_state, "N", 1U);
    wrongKindArguments[0] = matrixArguments[0];
    wrongKindArguments[1].kind = ZR_CANONICAL_GENERIC_ARGUMENT_TYPE;
    wrongKindArguments[1].data.typeId = intType;
    matrixOfFour = ZrParser_CanonicalType_InternGenericInstanceEx(
            g_context,
            matrixDefinition,
            matrixArguments,
            2U);
    matrixOfFourAgain = ZrParser_CanonicalType_InternGenericInstanceEx(
            g_context,
            matrixDefinition,
            matrixArguments,
            2U);
    matrixWrongArity = ZrParser_CanonicalType_InternGenericInstanceEx(
            g_context,
            matrixDefinition,
            matrixArguments,
            1U);
    matrixWrongKind = ZrParser_CanonicalType_InternGenericInstanceEx(
            g_context,
            matrixDefinition,
            wrongKindArguments,
            2U);
    matrixOpenPattern = ZrParser_CanonicalType_InternGenericInstanceEx(
            g_context,
            matrixDefinition,
            matrixPatternArguments,
            2U);
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_RegisterGenericDefinitionEx(
            g_context,
            matrixDefinition,
            matrixOwner,
            matrixParameterKinds,
            2U,
            ZR_CANONICAL_TYPE_CAPABILITY_VALUE_TYPE |
            ZR_CANONICAL_TYPE_CAPABILITY_VALUE_CONSTRUCTIBLE,
            ZR_CANONICAL_GC_SCAN_FREE));
    TEST_ASSERT_EQUAL_UINT32(matrixOfFour, matrixOfFourAgain);
    TEST_ASSERT_NOT_EQUAL(matrixOfFour, matrixWrongKind);
    assert_canonical_type_format(matrixOfFour, "app.Matrix<int, 4>");
    TEST_ASSERT_EQUAL_UINT32(
            matrixOfFour,
            ZrParser_CanonicalType_ResolveProjection(g_context, matrixOfFour));
    TEST_ASSERT_EQUAL_UINT32(
            ZR_SEMANTIC_ID_INVALID,
            ZrParser_CanonicalType_ResolveProjection(g_context, matrixWrongArity));
    TEST_ASSERT_EQUAL_UINT32(
            ZR_SEMANTIC_ID_INVALID,
            ZrParser_CanonicalType_ResolveProjection(g_context, matrixWrongKind));
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_RegisterConstructor(
            g_context,
            matrixDefinition,
            7203U,
            &matrixParameter,
            1U,
            ZR_TRUE));
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_RegisterConstructor(
            g_context,
            matrixDefinition,
            7204U,
            &matrixOpenPattern,
            1U,
            ZR_TRUE));
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_HasCapabilities(
            g_context,
            matrixOfFour,
            ZR_CANONICAL_TYPE_CAPABILITY_VALUE_CONSTRUCTIBLE));
    TEST_ASSERT_FALSE(ZrParser_CanonicalType_HasCapabilities(
            g_context,
            matrixWrongKind,
            ZR_CANONICAL_TYPE_CAPABILITY_VALUE_CONSTRUCTIBLE));
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_ResolveValueConstructor(
            g_context,
            matrixOfFour,
            &intType,
            1U,
            &constructorSymbol));
    TEST_ASSERT_EQUAL_UINT32(7203U, constructorSymbol);
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_ResolveValueConstructor(
            g_context,
            matrixOfFour,
            &matrixOfFour,
            1U,
            &constructorSymbol));
    TEST_ASSERT_EQUAL_UINT32(7204U, constructorSymbol);

    TEST_ASSERT_TRUE(ZrParser_CanonicalType_RegisterGenericDefinitionProjection(
            g_context,
            optionDefinition,
            optionOwner,
            matrixParameterKinds,
            1U,
            ZR_CANONICAL_TYPE_CAPABILITY_VALUE_TYPE |
                    ZR_CANONICAL_TYPE_CAPABILITY_VALUE_CONSTRUCTIBLE,
            ZR_CANONICAL_GC_SCAN_FREE,
            optionProjection));
    closedOptionProjection = ZrParser_CanonicalType_ResolveProjection(g_context, optionOfInt);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, closedOptionProjection);
    TEST_ASSERT_NOT_EQUAL(optionProjection, closedOptionProjection);
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_RegisterConstructor(
            g_context,
            optionDefinition,
            7205U,
            &optionProjection,
            1U,
            ZR_TRUE));
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_ResolveValueConstructor(
            g_context,
            optionOfInt,
            &closedOptionProjection,
            1U,
            &constructorSymbol));
    TEST_ASSERT_EQUAL_UINT32(7205U, constructorSymbol);
}

#endif // ZR_VM_TEST_CANONICAL_TYPE_GRAPH_SEMANTIC_CASES_H
