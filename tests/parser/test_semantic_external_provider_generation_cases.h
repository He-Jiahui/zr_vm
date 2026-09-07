static void test_external_references_preserve_analysis_provider_generation(void) {
    static const TZrUInt64 generations[] = {0U, 9U, 0x100000009ULL};
    static const TZrChar *source =
            "var api = import(\"semantic.external_identity.root\");\n"
            "var first = api.console.ping();\n"
            "var callable = api.console.ping;\n";
    TZrUInt32 metadataToken = 0U;
    TZrUInt32 signatureToken = 0U;
    TZrUInt64 signatureHash = 0U;

    TEST_ASSERT_TRUE(ZrLibrary_NativeRegistry_RegisterModule(
            g_state->global, &kSymbolExternalIdentityLeafModule));
    TEST_ASSERT_TRUE(ZrLibrary_NativeRegistry_RegisterModule(
            g_state->global, &kSymbolExternalIdentityRootModule));

    for (TZrSize pass = 0U; pass < ZR_ARRAY_COUNT(generations); pass++) {
        SZrString *sourceName = ZrCore_String_CreateFromNative(
                g_state, "external_provider_generation.zr");
        SZrAstNode *ast;
        SZrCompilerState cs;
        SZrParserSemanticSymbolQuery calledSymbol;
        SZrParserSemanticSymbolQuery callableValue;
        SZrArray references;
        TZrSize moduleCount = 0U;
        TZrSize callCount = 0U;
        TZrSize callableValueCount = 0U;

        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);
        memset(&cs, 0, sizeof(cs));
        ZrParser_CompilerState_Init(&cs, g_state);
        TEST_ASSERT_NOT_NULL(cs.semanticContext);
        TEST_ASSERT_EQUAL_UINT64(0U, cs.semanticContext->externalProviderGeneration);
        cs.semanticContext->externalProviderGeneration = generations[pass];
        cs.suppressErrorOutput = ZR_TRUE;
        cs.currentFunction = ZrCore_Function_New(g_state);
        TEST_ASSERT_NOT_NULL(cs.currentFunction);
        compile_script(&cs, ast);
        TEST_ASSERT_FALSE(cs.hasError);

        TEST_ASSERT_TRUE(ZrParser_SemanticQuery_SymbolAt(
                cs.semanticContext,
                symbol_source_position(source, sourceName, "ping", 0U),
                ZR_NULL, &calledSymbol));
        TEST_ASSERT_TRUE(ZrParser_SemanticQuery_SymbolAt(
                cs.semanticContext,
                symbol_source_position(source, sourceName, "ping", 1U),
                ZR_NULL, &callableValue));
        TEST_ASSERT_TRUE(calledSymbol.hasExternalTarget);
        TEST_ASSERT_TRUE(callableValue.hasExternalTarget);
        TEST_ASSERT_EQUAL_UINT64(
                generations[pass], calledSymbol.externalProviderGeneration);
        TEST_ASSERT_EQUAL_UINT64(
                generations[pass], callableValue.externalProviderGeneration);
        if (pass == 0U) {
            metadataToken = calledSymbol.externalMetadataToken;
            signatureToken = calledSymbol.externalSignatureToken;
            signatureHash = calledSymbol.externalSignatureHash;
        } else {
            TEST_ASSERT_EQUAL_UINT32(metadataToken, calledSymbol.externalMetadataToken);
            TEST_ASSERT_EQUAL_UINT32(signatureToken, calledSymbol.externalSignatureToken);
            TEST_ASSERT_EQUAL_UINT64(signatureHash, calledSymbol.externalSignatureHash);
        }

        ZrCore_Array_Construct(&references);
        TEST_ASSERT_TRUE(ZrParser_SemanticQuery_ExternalReferences(
                cs.semanticContext, ZR_NULL, &references));
        for (TZrSize index = 0U; index < references.length; index++) {
            const SZrParserSemanticExternalReferenceQuery *reference =
                    (const SZrParserSemanticExternalReferenceQuery *)ZrCore_Array_Get(
                            &references, index);

            TEST_ASSERT_NOT_NULL(reference);
            TEST_ASSERT_EQUAL_UINT64(
                    generations[pass], reference->externalProviderGeneration);
            if (reference->externalTargetKind == ZR_SEMANTIC_EXTERNAL_TARGET_MODULE) {
                TEST_ASSERT_EQUAL_STRING(
                        "semantic.external_identity.root",
                        ZrCore_String_GetNativeString(reference->externalOwnerIdentity));
                moduleCount++;
            } else if (reference->externalTargetKind ==
                       ZR_SEMANTIC_EXTERNAL_TARGET_CALLABLE) {
                TEST_ASSERT_EQUAL_STRING(
                        "semantic.external_identity.leaf",
                        ZrCore_String_GetNativeString(reference->externalOwnerIdentity));
                if (reference->role == ZR_SEMANTIC_REFERENCE_CALL) {
                    callCount++;
                } else if (reference->role == ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS) {
                    callableValueCount++;
                }
            }
        }
        TEST_ASSERT_TRUE(moduleCount >= 2U);
        TEST_ASSERT_TRUE(callCount >= 1U);
        TEST_ASSERT_TRUE(callableValueCount >= 1U);
        ZrCore_Array_Free(g_state, &references);

        symbol_release_compiler_function(&cs);
        ZrParser_SemanticContext_Reset(cs.semanticContext);
        TEST_ASSERT_EQUAL_UINT64(0U, cs.semanticContext->externalProviderGeneration);
        ZrParser_CompilerState_Free(&cs);
        ZrParser_Ast_Free(g_state, ast);
    }
}
