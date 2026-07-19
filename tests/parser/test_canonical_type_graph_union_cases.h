#ifndef ZR_VM_TEST_CANONICAL_TYPE_GRAPH_UNION_CASES_H
#define ZR_VM_TEST_CANONICAL_TYPE_GRAPH_UNION_CASES_H

static const SZrSemanticSymbolRecord *find_canonical_test_type_symbol(
        const SZrSemanticContext *context,
        const TZrChar *name) {
    TZrSize index;

    if (context == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    for (index = 0; index < context->symbols.length; index++) {
        const SZrSemanticSymbolRecord *candidate =
                (const SZrSemanticSymbolRecord *)ZrCore_Array_Get(
                        (SZrArray *)&context->symbols,
                        index);
        if (candidate != ZR_NULL &&
            candidate->kind == ZR_SEMANTIC_SYMBOL_KIND_TYPE &&
            candidate->name != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(candidate->name), name) == 0) {
            return candidate;
        }
    }
    return ZR_NULL;
}

static void init_canonical_test_generic_type(
        SZrInferredType *type,
        const TZrChar *name,
        SZrInferredType *arguments,
        TZrSize argumentCount) {
    TZrSize index;

    ZrParser_InferredType_InitFull(
            g_state,
            type,
            ZR_VALUE_TYPE_OBJECT,
            ZR_FALSE,
            ZrCore_String_Create(g_state, (TZrNativeString)name, strlen(name)));
    ZrCore_Array_Init(g_state, &type->elementTypes, sizeof(SZrInferredType), argumentCount);
    for (index = 0; index < argumentCount; index++) {
        ZrCore_Array_Push(g_state, &type->elementTypes, &arguments[index]);
    }
}

static void test_union_compiler_path_unifies_declaration_and_use_type_ids(void) {
    const TZrChar *source =
            "union Choice { None; One(value: int); }\n"
            "union Option<T> { None; Some(value: T); }\n"
            "union Outer<T> { Empty; Wrap(value: Option<T>); }\n"
            "union Matrix<T, const N: int> { Empty; Item(value: T); }\n"
            "union Wrap<T, const N: int> { Empty; Value(value: Matrix<T, N>); }\n"
            "union MatrixUse { Empty; Item(value: Matrix<int, 4>); }\n"
            "union TextChoice { Empty; Text(value: string); }\n";
    SZrString *sourceName = ZrCore_String_Create(g_state, "canonical_union.zr", 18);
    SZrAstNode *ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    SZrCompilerState compiler;
    const SZrSemanticSymbolRecord *choiceSymbol;
    const SZrSemanticSymbolRecord *optionSymbol;
    const SZrSemanticSymbolRecord *outerSymbol;
    const SZrSemanticSymbolRecord *matrixSymbol;
    const SZrSemanticSymbolRecord *wrapSymbol;
    const SZrSemanticSymbolRecord *matrixUseSymbol;
    const SZrSemanticSymbolRecord *textChoiceSymbol;
    const SZrCanonicalTypeNode *optionNode;
    const SZrCanonicalTypeNode *unitPayload;
    const SZrCanonicalTypeNode *genericPayload;
    const SZrCanonicalTypeNode *closedOptionNode;
    const SZrCanonicalTypeNode *closedMatrixDefinition;
    const SZrCanonicalTypeNode *wrapNode;
    const SZrCanonicalTypeNode *wrapMatrixDefinition;
    SZrInferredType intArgument;
    SZrInferredType constFourArgument;
    SZrInferredType constFourCopy;
    SZrInferredType constFiveArgument;
    SZrInferredType optionOfInt;
    SZrInferredType outerOfInt;
    SZrInferredType matrixArguments[2];
    SZrInferredType matrixOfFour;
    SZrInferredType matrixOfFive;
    SZrInferredType wrapArguments[2];
    SZrInferredType wrapOfFour;
    SZrArray functionParameters;
    TZrTypeId closedOptionTypeId;
    TZrTypeId closedMatrixFourTypeId;
    TZrTypeId closedMatrixFiveTypeId;
    TZrTypeId functionTypeId;
    const SZrCanonicalTypeNode *functionNode;
    EZrCanonicalGcScanKind scanKind;
    TZrChar constDisplay[32];
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(7U, (TZrUInt32)ast->data.script.statements->count);

    ZrParser_CompilerState_Init(&compiler, g_state);
    for (index = 0; index < ast->data.script.statements->count; index++) {
        ZrParser_Compiler_CompileUnionDeclaration(
                &compiler,
                ast->data.script.statements->nodes[index]);
        TEST_ASSERT_FALSE(compiler.hasError);
    }

    choiceSymbol = find_canonical_test_type_symbol(compiler.semanticContext, "Choice");
    optionSymbol = find_canonical_test_type_symbol(compiler.semanticContext, "Option");
    outerSymbol = find_canonical_test_type_symbol(compiler.semanticContext, "Outer");
    matrixSymbol = find_canonical_test_type_symbol(compiler.semanticContext, "Matrix");
    wrapSymbol = find_canonical_test_type_symbol(compiler.semanticContext, "Wrap");
    matrixUseSymbol = find_canonical_test_type_symbol(compiler.semanticContext, "MatrixUse");
    textChoiceSymbol = find_canonical_test_type_symbol(compiler.semanticContext, "TextChoice");
    TEST_ASSERT_NOT_NULL(choiceSymbol);
    TEST_ASSERT_NOT_NULL(optionSymbol);
    TEST_ASSERT_NOT_NULL(outerSymbol);
    TEST_ASSERT_NOT_NULL(matrixSymbol);
    TEST_ASSERT_NOT_NULL(wrapSymbol);
    TEST_ASSERT_NOT_NULL(matrixUseSymbol);
    TEST_ASSERT_NOT_NULL(textChoiceSymbol);
    TEST_ASSERT_EQUAL_UINT32(
            choiceSymbol->typeId,
            ZrParser_CanonicalType_FromName(
                    compiler.semanticContext,
                    ZrCore_String_Create(g_state, "Choice", 6U)));
    TEST_ASSERT_EQUAL_UINT32(
            optionSymbol->typeId,
            ZrParser_CanonicalType_FromName(
                    compiler.semanticContext,
                    ZrCore_String_Create(g_state, "Option", 6U)));

    optionNode = ZrParser_CanonicalType_Find(compiler.semanticContext, optionSymbol->typeId);
    TEST_ASSERT_NOT_NULL(optionNode);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_UNION, optionNode->kind);
    TEST_ASSERT_EQUAL_UINT32(2U, (TZrUInt32)optionNode->data.unionType.variantTypeIds.length);
    unitPayload = ZrParser_CanonicalType_Find(
            compiler.semanticContext,
            *(const TZrTypeId *)ZrCore_Array_Get(
                    (SZrArray *)&optionNode->data.unionType.variantTypeIds,
                    0U));
    genericPayload = ZrParser_CanonicalType_Find(
            compiler.semanticContext,
            *(const TZrTypeId *)ZrCore_Array_Get(
                    (SZrArray *)&optionNode->data.unionType.variantTypeIds,
                    1U));
    TEST_ASSERT_NOT_NULL(unitPayload);
    TEST_ASSERT_NOT_NULL(genericPayload);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_TUPLE, unitPayload->kind);
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)unitPayload->data.typeList.elementTypeIds.length);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_GENERIC_PARAMETER, genericPayload->kind);
    TEST_ASSERT_EQUAL_UINT32(optionSymbol->id, genericPayload->data.genericParameter.ownerSymbolId);
    TEST_ASSERT_EQUAL_UINT32(0U, genericPayload->data.genericParameter.ordinal);

    wrapNode = ZrParser_CanonicalType_Find(compiler.semanticContext, wrapSymbol->typeId);
    TEST_ASSERT_NOT_NULL(wrapNode);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_UNION, wrapNode->kind);
    {
        const SZrCanonicalTypeNode *openMatrixProjection = ZrParser_CanonicalType_Find(
                compiler.semanticContext,
                *(const TZrTypeId *)ZrCore_Array_Get(
                        (SZrArray *)&wrapNode->data.unionType.variantTypeIds,
                        1U));
        TEST_ASSERT_NOT_NULL(openMatrixProjection);
        TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_UNION, openMatrixProjection->kind);
        wrapMatrixDefinition = ZrParser_CanonicalType_Find(
                compiler.semanticContext,
                openMatrixProjection->data.unionType.definitionTypeId);
        TEST_ASSERT_NOT_NULL(wrapMatrixDefinition);
        TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_GENERIC_INSTANCE, wrapMatrixDefinition->kind);
        TEST_ASSERT_EQUAL_INT(
                ZR_CANONICAL_GENERIC_ARGUMENT_CONST_PARAMETER,
                ((const SZrCanonicalGenericArgument *)ZrCore_Array_Get(
                        (SZrArray *)&wrapMatrixDefinition->data.genericInstance.arguments,
                        1U))->kind);
        TEST_ASSERT_EQUAL_UINT32(
                wrapSymbol->id,
                ((const SZrCanonicalGenericArgument *)ZrCore_Array_Get(
                        (SZrArray *)&wrapMatrixDefinition->data.genericInstance.arguments,
                        1U))->data.constParameter.ownerSymbolId);
        TEST_ASSERT_EQUAL_UINT32(
                1U,
                ((const SZrCanonicalGenericArgument *)ZrCore_Array_Get(
                        (SZrArray *)&wrapMatrixDefinition->data.genericInstance.arguments,
                        1U))->data.constParameter.ordinal);
    }

    ZrParser_InferredType_Init(g_state, &intArgument, ZR_VALUE_TYPE_INT64);
    init_canonical_test_generic_type(&optionOfInt, "Option<int>", &intArgument, 1U);
    closedOptionTypeId = ZrParser_CanonicalType_FromInferred(
            compiler.semanticContext,
            &optionOfInt);
    closedOptionNode = ZrParser_CanonicalType_Find(compiler.semanticContext, closedOptionTypeId);
    TEST_ASSERT_NOT_NULL(closedOptionNode);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_UNION, closedOptionNode->kind);
    TEST_ASSERT_EQUAL_UINT32(
            ZrParser_CanonicalType_InternPrimitive(compiler.semanticContext, ZR_VALUE_TYPE_INT64),
            *(const TZrTypeId *)ZrCore_Array_Get(
                    (SZrArray *)&closedOptionNode->data.unionType.variantTypeIds,
                    1U));
    TEST_ASSERT_EQUAL_UINT32(
            closedOptionTypeId,
            ZrParser_Semantic_RegisterInferredType(
                    compiler.semanticContext,
                    &optionOfInt,
                    ZR_SEMANTIC_TYPE_KIND_UNION,
                    optionOfInt.typeName,
                    ZR_NULL));

    init_canonical_test_generic_type(&outerOfInt, "Outer<int>", &intArgument, 1U);
    {
        TZrTypeId closedOuterTypeId = ZrParser_CanonicalType_FromInferred(
                compiler.semanticContext,
                &outerOfInt);
        const SZrCanonicalTypeNode *closedOuterNode =
                ZrParser_CanonicalType_Find(compiler.semanticContext, closedOuterTypeId);
        TEST_ASSERT_NOT_NULL(closedOuterNode);
        TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_UNION, closedOuterNode->kind);
        TEST_ASSERT_EQUAL_UINT32(
                closedOptionTypeId,
                *(const TZrTypeId *)ZrCore_Array_Get(
                        (SZrArray *)&closedOuterNode->data.unionType.variantTypeIds,
                        1U));
    }

    ZrCore_Array_Init(g_state, &functionParameters, sizeof(SZrInferredType), 1U);
    ZrCore_Array_Push(g_state, &functionParameters, &optionOfInt);
    functionTypeId = ZrParser_CanonicalType_FromFunctionSignature(
            compiler.semanticContext,
            &functionParameters,
            ZR_NULL,
            &optionOfInt,
            ZR_CANONICAL_RECEIVER_NONE,
            ZR_CANONICAL_CALLABLE_EFFECT_NONE);
    functionNode = ZrParser_CanonicalType_Find(compiler.semanticContext, functionTypeId);
    TEST_ASSERT_NOT_NULL(functionNode);
    TEST_ASSERT_EQUAL_UINT32(closedOptionTypeId, functionNode->data.function.returnTypeId);
    TEST_ASSERT_EQUAL_UINT32(
            closedOptionTypeId,
            ((const SZrCanonicalParameterContract *)ZrCore_Array_Get(
                    (SZrArray *)&functionNode->data.function.parameterContracts,
                    0U))->typeId);

    ZrParser_InferredType_InitConstIntGenericArgument(
            g_state,
            &constFourArgument,
            4,
            ZR_NULL);
    ZrParser_InferredType_InitConstIntGenericArgument(
            g_state,
            &constFiveArgument,
            5,
            ZR_NULL);
    ZrParser_InferredType_Copy(g_state, &constFourCopy, &constFourArgument);
    constFourCopy.typeName = ZrCore_String_Create(g_state, "four", 4U);
    TEST_ASSERT_TRUE(ZrParser_InferredType_Equal(&constFourArgument, &constFourCopy));
    TEST_ASSERT_FALSE(ZrParser_InferredType_Equal(&constFourArgument, &constFiveArgument));
    TEST_ASSERT_EQUAL_STRING(
            "4",
            ZrParser_TypeNameString_Get(
                    g_state,
                    &constFourCopy,
                    constDisplay,
                    sizeof(constDisplay)));
    matrixArguments[0] = intArgument;
    matrixArguments[1] = constFourArgument;
    init_canonical_test_generic_type(&matrixOfFour, "Matrix<int, 4>", matrixArguments, 2U);
    matrixArguments[1] = constFiveArgument;
    init_canonical_test_generic_type(&matrixOfFive, "Matrix<int, 5>", matrixArguments, 2U);
    closedMatrixFourTypeId = ZrParser_CanonicalType_FromInferred(
            compiler.semanticContext,
            &matrixOfFour);
    closedMatrixFiveTypeId = ZrParser_CanonicalType_FromInferred(
            compiler.semanticContext,
            &matrixOfFive);
    TEST_ASSERT_NOT_EQUAL(closedMatrixFourTypeId, closedMatrixFiveTypeId);
    TEST_ASSERT_EQUAL_INT(
            ZR_CANONICAL_TYPE_UNION,
            ZrParser_CanonicalType_Find(compiler.semanticContext, closedMatrixFourTypeId)->kind);
    closedMatrixDefinition = ZrParser_CanonicalType_Find(
            compiler.semanticContext,
            ZrParser_CanonicalType_Find(
                    compiler.semanticContext,
                    closedMatrixFourTypeId)->data.unionType.definitionTypeId);
    TEST_ASSERT_NOT_NULL(closedMatrixDefinition);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_GENERIC_INSTANCE, closedMatrixDefinition->kind);
    TEST_ASSERT_EQUAL_UINT32(
            2U,
            (TZrUInt32)closedMatrixDefinition->data.genericInstance.arguments.length);
    TEST_ASSERT_EQUAL_INT(
            ZR_CANONICAL_GENERIC_ARGUMENT_CONST_INT,
            ((const SZrCanonicalGenericArgument *)ZrCore_Array_Get(
                    (SZrArray *)&closedMatrixDefinition->data.genericInstance.arguments,
                    1U))->kind);
    TEST_ASSERT_EQUAL_INT64(
            4,
            ((const SZrCanonicalGenericArgument *)ZrCore_Array_Get(
                    (SZrArray *)&closedMatrixDefinition->data.genericInstance.arguments,
                    1U))->data.constIntValue);

    wrapArguments[0] = intArgument;
    wrapArguments[1] = constFourArgument;
    init_canonical_test_generic_type(&wrapOfFour, "Wrap<int, 4>", wrapArguments, 2U);
    {
        TZrTypeId closedWrapTypeId = ZrParser_CanonicalType_FromInferred(
                compiler.semanticContext,
                &wrapOfFour);
        const SZrCanonicalTypeNode *closedWrapNode =
                ZrParser_CanonicalType_Find(compiler.semanticContext, closedWrapTypeId);
        const SZrCanonicalTypeNode *closedMatrixNode;
        const SZrCanonicalTypeNode *closedDefinitionNode;
        const SZrCanonicalGenericArgument *closedConstArgument;

        TEST_ASSERT_NOT_NULL(closedWrapNode);
        TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_UNION, closedWrapNode->kind);
        closedMatrixNode = ZrParser_CanonicalType_Find(
                compiler.semanticContext,
                *(const TZrTypeId *)ZrCore_Array_Get(
                        (SZrArray *)&closedWrapNode->data.unionType.variantTypeIds,
                        1U));
        TEST_ASSERT_NOT_NULL(closedMatrixNode);
        TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_UNION, closedMatrixNode->kind);
        closedDefinitionNode = ZrParser_CanonicalType_Find(
                compiler.semanticContext,
                closedMatrixNode->data.unionType.definitionTypeId);
        TEST_ASSERT_NOT_NULL(closedDefinitionNode);
        closedConstArgument = (const SZrCanonicalGenericArgument *)ZrCore_Array_Get(
                (SZrArray *)&closedDefinitionNode->data.genericInstance.arguments,
                1U);
        TEST_ASSERT_NOT_NULL(closedConstArgument);
        TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_GENERIC_ARGUMENT_CONST_INT, closedConstArgument->kind);
        TEST_ASSERT_EQUAL_INT64(4, closedConstArgument->data.constIntValue);
    }

    TEST_ASSERT_TRUE(ZrParser_CanonicalType_GetGcScanKind(
            compiler.semanticContext,
            choiceSymbol->typeId,
            &scanKind));
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_GC_SCAN_FREE, scanKind);
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_GetGcScanKind(
            compiler.semanticContext,
            optionSymbol->typeId,
            &scanKind));
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_GC_SCAN_BARRIERED, scanKind);
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_GetGcScanKind(
            compiler.semanticContext,
            closedOptionTypeId,
            &scanKind));
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_GC_SCAN_FREE, scanKind);
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_GetGcScanKind(
            compiler.semanticContext,
            textChoiceSymbol->typeId,
            &scanKind));
    TEST_ASSERT_NOT_EQUAL(ZR_CANONICAL_GC_SCAN_FREE, scanKind);
    TEST_ASSERT_FALSE(ZrParser_CanonicalType_HasCapabilities(
            compiler.semanticContext,
            optionSymbol->typeId,
            ZR_CANONICAL_TYPE_CAPABILITY_VALUE_CONSTRUCTIBLE));

    {
        TZrSize typeNameCount = compiler.typeEnv->typeNames.length;
        TZrSize prototypeCount = compiler.typePrototypes.length;
        TZrSize symbolCount = compiler.semanticContext->symbols.length;
        TZrSize semanticTypeCount = compiler.semanticContext->types.length;
        TZrSize canonicalTypeCount = compiler.semanticContext->canonicalTypes.length;
        TZrSize definitionCount = compiler.semanticContext->canonicalTypeDefinitions.length;

        ZrParser_Compiler_CompileUnionDeclaration(
                &compiler,
                ast->data.script.statements->nodes[0]);
        TEST_ASSERT_TRUE(compiler.hasError);
        TEST_ASSERT_EQUAL_UINT32((TZrUInt32)typeNameCount, (TZrUInt32)compiler.typeEnv->typeNames.length);
        TEST_ASSERT_EQUAL_UINT32((TZrUInt32)prototypeCount, (TZrUInt32)compiler.typePrototypes.length);
        TEST_ASSERT_EQUAL_UINT32((TZrUInt32)symbolCount, (TZrUInt32)compiler.semanticContext->symbols.length);
        TEST_ASSERT_EQUAL_UINT32((TZrUInt32)semanticTypeCount, (TZrUInt32)compiler.semanticContext->types.length);
        TEST_ASSERT_EQUAL_UINT32((TZrUInt32)canonicalTypeCount, (TZrUInt32)compiler.semanticContext->canonicalTypes.length);
        TEST_ASSERT_EQUAL_UINT32((TZrUInt32)definitionCount, (TZrUInt32)compiler.semanticContext->canonicalTypeDefinitions.length);
    }

    ZrCore_Array_Free(g_state, &functionParameters);
    ZrParser_InferredType_Free(g_state, &optionOfInt);
    ZrParser_InferredType_Free(g_state, &outerOfInt);
    ZrParser_InferredType_Free(g_state, &matrixOfFour);
    ZrParser_InferredType_Free(g_state, &matrixOfFive);
    ZrParser_InferredType_Free(g_state, &wrapOfFour);
    ZrParser_InferredType_Free(g_state, &constFourCopy);
    ZrParser_Ast_Free(g_state, ast);
    ZrParser_CompilerState_Free(&compiler);
}

static const SZrTypePrototypeInfo *find_canonical_test_prototype(
        const SZrCompilerState *compiler,
        const TZrChar *name) {
    TZrSize index;

    if (compiler == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    for (index = 0; index < compiler->typePrototypes.length; index++) {
        const SZrTypePrototypeInfo *prototype =
                (const SZrTypePrototypeInfo *)ZrCore_Array_Get(
                        (SZrArray *)&compiler->typePrototypes,
                        index);
        if (prototype != ZR_NULL &&
            prototype->name != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(prototype->name), name) == 0) {
            return prototype;
        }
    }
    return ZR_NULL;
}

static const SZrTypeMemberInfo *find_canonical_test_member(
        const SZrTypePrototypeInfo *prototype,
        const TZrChar *name) {
    TZrSize index;

    if (prototype == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    for (index = 0; index < prototype->members.length; index++) {
        const SZrTypeMemberInfo *member =
                (const SZrTypeMemberInfo *)ZrCore_Array_Get(
                        (SZrArray *)&prototype->members,
                        index);
        if (member != ZR_NULL && member->name != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(member->name), name) == 0) {
            return member;
        }
    }
    return ZR_NULL;
}

static void test_open_const_function_and_closed_member_returns_preserve_structure(void) {
    const TZrChar *source =
            "union Matrix<T, const N: int> { Empty; Item(value: T); }\n"
            "class Box<const N: int> {\n"
            "    func shape(value: Matrix<int, N>): Matrix<int, N> { return value; }\n"
            "    func fixed(value: Matrix<int, 4>): Matrix<int, 4> { return value; }\n"
            "}\n"
            "func echo<T, const N: int>(value: Matrix<T, N>): Matrix<T, N> { return value; }\n"
            "var box: Box<4>;\n";
    SZrAstNode *ast = ZrParser_Parse(
            g_state,
            source,
            strlen(source),
            ZrCore_String_Create(g_state, "canonical_open_const.zr", 23U));
    SZrCompilerState compiler;
    SZrFunctionTypeInfo *echoInfo = ZR_NULL;
    const SZrCanonicalTypeNode *echoNode;
    const SZrCanonicalTypeNode *echoParameter;
    const SZrCanonicalGenericArgument *echoConstArgument;
    const SZrTypePrototypeInfo *closedBox;
    const SZrTypeMemberInfo *shapeMember;
    const SZrTypeMemberInfo *fixedMember;
    const SZrInferredType *shapeConstArgument;
    const SZrInferredType *fixedConstArgument;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_UINT32(4U, (TZrUInt32)ast->data.script.statements->count);
    ZrParser_CompilerState_Init(&compiler, g_state);
    for (index = 0; index < ast->data.script.statements->count; index++) {
        if (index == 0U) {
            ZrParser_Compiler_CompileUnionDeclaration(
                    &compiler,
                    ast->data.script.statements->nodes[index]);
        } else if (index == 1U) {
            ZrParser_Compiler_CompileClassDeclaration(
                    &compiler,
                    ast->data.script.statements->nodes[index]);
        } else {
            if (index == 3U && compiler.currentFunction == ZR_NULL) {
                compiler.currentFunction = ZrCore_Function_New(g_state);
            }
            ZrParser_Statement_Compile(
                    &compiler,
                    ast->data.script.statements->nodes[index]);
        }
        TEST_ASSERT_FALSE(compiler.hasError);
    }

    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_LookupFunction(
            compiler.typeEnv,
            ZrCore_String_Create(g_state, "echo", 4U),
            &echoInfo));
    TEST_ASSERT_NOT_NULL(echoInfo);
    echoNode = ZrParser_CanonicalType_Find(compiler.semanticContext, echoInfo->typeId);
    TEST_ASSERT_NOT_NULL(echoNode);
    echoParameter = ZrParser_CanonicalType_Find(
            compiler.semanticContext,
            ((const SZrCanonicalParameterContract *)ZrCore_Array_Get(
                    (SZrArray *)&echoNode->data.function.parameterContracts,
                    0U))->typeId);
    TEST_ASSERT_NOT_NULL(echoParameter);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_UNION, echoParameter->kind);
    echoParameter = ZrParser_CanonicalType_Find(
            compiler.semanticContext,
            echoParameter->data.unionType.definitionTypeId);
    TEST_ASSERT_NOT_NULL(echoParameter);
    echoConstArgument = (const SZrCanonicalGenericArgument *)ZrCore_Array_Get(
            (SZrArray *)&echoParameter->data.genericInstance.arguments,
            1U);
    TEST_ASSERT_NOT_NULL(echoConstArgument);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_GENERIC_ARGUMENT_CONST_PARAMETER, echoConstArgument->kind);
    TEST_ASSERT_EQUAL_UINT32(echoInfo->symbolId, echoConstArgument->data.constParameter.ownerSymbolId);
    TEST_ASSERT_EQUAL_UINT32(1U, echoConstArgument->data.constParameter.ordinal);

    closedBox = find_canonical_test_prototype(&compiler, "Box<4>");
    TEST_ASSERT_NOT_NULL(closedBox);
    shapeMember = find_canonical_test_member(closedBox, "shape");
    fixedMember = find_canonical_test_member(closedBox, "fixed");
    TEST_ASSERT_NOT_NULL(shapeMember);
    TEST_ASSERT_NOT_NULL(fixedMember);
    TEST_ASSERT_TRUE(shapeMember->hasStructuredReturnType);
    TEST_ASSERT_TRUE(fixedMember->hasStructuredReturnType);
    shapeConstArgument = (const SZrInferredType *)ZrCore_Array_Get(
            (SZrArray *)&shapeMember->structuredReturnType.elementTypes,
            1U);
    fixedConstArgument = (const SZrInferredType *)ZrCore_Array_Get(
            (SZrArray *)&fixedMember->structuredReturnType.elementTypes,
            1U);
    TEST_ASSERT_NOT_NULL(shapeConstArgument);
    TEST_ASSERT_NOT_NULL(fixedConstArgument);
    TEST_ASSERT_EQUAL_INT(ZR_INFERRED_GENERIC_ARGUMENT_CONST_INT, shapeConstArgument->genericArgumentKind);
    TEST_ASSERT_EQUAL_INT64(4, shapeConstArgument->genericConstIntValue);
    TEST_ASSERT_EQUAL_INT(ZR_INFERRED_GENERIC_ARGUMENT_CONST_INT, fixedConstArgument->genericArgumentKind);
    TEST_ASSERT_EQUAL_INT64(4, fixedConstArgument->genericConstIntValue);

    if (compiler.currentFunction != ZR_NULL) {
        ZrCore_Function_Free(g_state, compiler.currentFunction);
        compiler.currentFunction = ZR_NULL;
    }
    ZrParser_Ast_Free(g_state, ast);
    ZrParser_CompilerState_Free(&compiler);
}

static void assert_invalid_source_generic_kind_is_atomic(
        const TZrChar *declaration,
        const TZrChar *invalidType,
        const TZrChar *sourceNameText,
        const TZrChar *expectedErrorFragment) {
    TZrChar source[512];
    SZrAstNode *ast;
    SZrCompilerState compiler;
    TZrSize typeNameCount;
    TZrSize prototypeCount;
    TZrSize variableCount;
    TZrSize semanticTypeCount;
    TZrSize symbolCount;

    snprintf(source, sizeof(source), "%s\nvar invalid: %s;\n", declaration, invalidType);
    ast = ZrParser_Parse(
            g_state,
            source,
            strlen(source),
            ZrCore_String_Create(g_state, (TZrNativeString)sourceNameText, strlen(sourceNameText)));
    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_CompilerState_Init(&compiler, g_state);
    if (ast->data.script.statements->nodes[0]->type == ZR_AST_CLASS_DECLARATION) {
        ZrParser_Compiler_CompileClassDeclaration(
                &compiler,
                ast->data.script.statements->nodes[0]);
    } else {
        ZrParser_Compiler_CompileStructDeclaration(
                &compiler,
                ast->data.script.statements->nodes[0]);
    }
    TEST_ASSERT_FALSE(compiler.hasError);
    compiler.currentFunction = ZrCore_Function_New(g_state);
    typeNameCount = compiler.typeEnv->typeNames.length;
    prototypeCount = compiler.typePrototypes.length;
    variableCount = compiler.typeEnv->variableTypes.length;
    semanticTypeCount = compiler.semanticContext->types.length;
    symbolCount = compiler.semanticContext->symbols.length;

    ZrParser_Statement_Compile(&compiler, ast->data.script.statements->nodes[1]);
    TEST_ASSERT_TRUE(compiler.hasError);
    TEST_ASSERT_NOT_NULL(compiler.errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(compiler.errorMessage, expectedErrorFragment));
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)typeNameCount, (TZrUInt32)compiler.typeEnv->typeNames.length);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)prototypeCount, (TZrUInt32)compiler.typePrototypes.length);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)variableCount, (TZrUInt32)compiler.typeEnv->variableTypes.length);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)semanticTypeCount, (TZrUInt32)compiler.semanticContext->types.length);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)symbolCount, (TZrUInt32)compiler.semanticContext->symbols.length);

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_Ast_Free(g_state, ast);
    ZrParser_CompilerState_Free(&compiler);
}

static void test_class_generic_kind_and_constraint_failures_are_atomic(void) {
    assert_invalid_source_generic_kind_is_atomic(
            "class TypeBox<T> { }",
            "TypeBox<4>",
            "canonical_class_const_for_type.zr",
            "Generic argument '4'");
    assert_invalid_source_generic_kind_is_atomic(
            "class ConstBox<const N: int> { }",
            "ConstBox<int>",
            "canonical_class_type_for_const.zr",
            "Generic argument 'int'");
    assert_invalid_source_generic_kind_is_atomic(
            "class NeedClass<T> where T: class { }",
            "NeedClass<int>",
            "canonical_class_constraint_atomic.zr",
            "Generic argument 'int'");
}

static void assert_invalid_canonical_generic_union_payload_is_rejected(
        const TZrChar *payloadType,
        const TZrChar *sourceNameText) {
    TZrChar source[512];
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrCompilerState compiler;
    TZrSize typeNameCount;
    TZrSize prototypeCount;
    TZrSize symbolCount;
    TZrSize semanticTypeCount;
    TZrSize definitionCount;

    snprintf(
            source,
            sizeof(source),
            "union Matrix<T, const N: int> { Empty; Item(value: T); }\n"
            "union InvalidUse { Value(value: %s); }\n",
            payloadType);
    sourceName = ZrCore_String_Create(
            g_state,
            (TZrNativeString)sourceNameText,
            strlen(sourceNameText));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_UINT32(2U, (TZrUInt32)ast->data.script.statements->count);

    ZrParser_CompilerState_Init(&compiler, g_state);
    ZrParser_Compiler_CompileUnionDeclaration(
            &compiler,
            ast->data.script.statements->nodes[0]);
    TEST_ASSERT_FALSE(compiler.hasError);
    typeNameCount = compiler.typeEnv->typeNames.length;
    prototypeCount = compiler.typePrototypes.length;
    symbolCount = compiler.semanticContext->symbols.length;
    semanticTypeCount = compiler.semanticContext->types.length;
    definitionCount = compiler.semanticContext->canonicalTypeDefinitions.length;

    ZrParser_Compiler_CompileUnionDeclaration(
            &compiler,
            ast->data.script.statements->nodes[1]);
    TEST_ASSERT_TRUE(compiler.hasError);
    TEST_ASSERT_NULL(find_canonical_test_type_symbol(
            compiler.semanticContext,
            "InvalidUse"));
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)typeNameCount, (TZrUInt32)compiler.typeEnv->typeNames.length);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)prototypeCount, (TZrUInt32)compiler.typePrototypes.length);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)symbolCount, (TZrUInt32)compiler.semanticContext->symbols.length);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)semanticTypeCount, (TZrUInt32)compiler.semanticContext->types.length);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)definitionCount,
                             (TZrUInt32)compiler.semanticContext->canonicalTypeDefinitions.length);

    ZrParser_Ast_Free(g_state, ast);
    ZrParser_CompilerState_Free(&compiler);
}

static void assert_invalid_canonical_generic_variable_is_rejected(
        const TZrChar *variableType,
        const TZrChar *sourceNameText) {
    TZrChar source[512];
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrCompilerState compiler;
    TZrSize variableCount;
    TZrSize symbolCount;
    TZrSize referenceCount;

    snprintf(
            source,
            sizeof(source),
            "union Matrix<T, const N: int> { Empty; Item(value: T); }\n"
            "var invalid: %s;\n",
            variableType);
    sourceName = ZrCore_String_Create(
            g_state,
            (TZrNativeString)sourceNameText,
            strlen(sourceNameText));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_UINT32(2U, (TZrUInt32)ast->data.script.statements->count);

    ZrParser_CompilerState_Init(&compiler, g_state);
    ZrParser_Compiler_CompileUnionDeclaration(
            &compiler,
            ast->data.script.statements->nodes[0]);
    TEST_ASSERT_FALSE(compiler.hasError);
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    variableCount = compiler.typeEnv->variableTypes.length;
    symbolCount = compiler.semanticContext->symbols.length;
    referenceCount = compiler.semanticContext->referenceFacts.length;

    ZrParser_Statement_Compile(
            &compiler,
            ast->data.script.statements->nodes[1]);
    TEST_ASSERT_TRUE(compiler.hasError);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)variableCount,
                             (TZrUInt32)compiler.typeEnv->variableTypes.length);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)symbolCount,
                             (TZrUInt32)compiler.semanticContext->symbols.length);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)referenceCount,
                             (TZrUInt32)compiler.semanticContext->referenceFacts.length);

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_Ast_Free(g_state, ast);
    ZrParser_CompilerState_Free(&compiler);
}

static void test_known_generic_contract_mismatches_fail_compiler_canonicalization(void) {
    assert_invalid_canonical_generic_union_payload_is_rejected(
            "Matrix<int, bool>",
            "canonical_wrong_kind.zr");
    assert_invalid_canonical_generic_union_payload_is_rejected(
            "Matrix<int>",
            "canonical_wrong_arity.zr");
    assert_invalid_canonical_generic_variable_is_rejected(
            "Matrix<int, bool>",
            "canonical_variable_wrong_kind.zr");
    assert_invalid_canonical_generic_variable_is_rejected(
            "Matrix<int>",
            "canonical_variable_wrong_arity.zr");
}

static void test_union_registration_failure_does_not_publish_partial_state(void) {
    const TZrChar *source = "union Broken<T> { Value(value: T); }";
    SZrString *sourceName = ZrCore_String_Create(g_state, "broken_union.zr", 15U);
    SZrAstNode *ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    SZrCompilerState compiler;
    SZrAstNode *genericParameter;

    TEST_ASSERT_NOT_NULL(ast);
    genericParameter = ast->data.script.statements->nodes[0]
                               ->data.unionDeclaration.generic->params->nodes[0];
    TEST_ASSERT_NOT_NULL(genericParameter);
    genericParameter->data.parameter.genericKind = (EZrGenericParameterKind)99;

    ZrParser_CompilerState_Init(&compiler, g_state);
    ZrParser_Compiler_CompileUnionDeclaration(
            &compiler,
            ast->data.script.statements->nodes[0]);
    TEST_ASSERT_TRUE(compiler.hasError);
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)compiler.typeEnv->typeNames.length);
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)compiler.typePrototypes.length);
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)compiler.semanticContext->symbols.length);
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)compiler.semanticContext->types.length);
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)compiler.semanticContext->canonicalTypes.length);
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)compiler.semanticContext->canonicalTypeDefinitions.length);

    ZrParser_Ast_Free(g_state, ast);
    ZrParser_CompilerState_Free(&compiler);
}

#endif // ZR_VM_TEST_CANONICAL_TYPE_GRAPH_UNION_CASES_H
