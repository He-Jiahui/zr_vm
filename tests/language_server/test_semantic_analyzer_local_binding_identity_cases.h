#ifndef ZR_TEST_SEMANTIC_ANALYZER_LOCAL_BINDING_IDENTITY_CASES_H
#define ZR_TEST_SEMANTIC_ANALYZER_LOCAL_BINDING_IDENTITY_CASES_H

static void test_semantic_analyzer_preserves_local_binding_identity(SZrState *state) {
    static const struct {
        const char *source;
        TZrSize declarationOccurrences[2];
        TZrSize useOccurrences[2];
        TZrSize declarationCount;
    } cases[] = {
        {"fn run(seed: int): int {\n"
         "    var result = seed + 1;\n"
         "    return result;\n"
         "}\n", {0U, 0U}, {1U, 0U}, 1U},
        {"fn run(seed: int): int {\n"
         "    var result: int = seed + 1;\n"
         "    return result;\n"
         "}\n", {0U, 0U}, {1U, 0U}, 1U},
        {"fn run(seed: int): int {\n"
         "    var result = seed;\n"
         "    {\n"
         "        var result = seed + 1;\n"
         "        result + 2;\n"
         "    }\n"
         "    return result;\n"
         "}\n", {0U, 1U}, {3U, 2U}, 2U}
    };
    const char *summary = "Semantic Analyzer Preserves Local Binding Identity";
    SZrTestTimer timer;

    TEST_START(summary);
    for (TZrSize caseIndex = 0; caseIndex < sizeof(cases) / sizeof(cases[0]); caseIndex++) {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        SZrString *sourceName = ZrCore_String_CreateFromNative(
                state, "file:///canonical_local_binding.zr");
        SZrAstNode *ast = ZrParser_Parse(
                state, cases[caseIndex].source, strlen(cases[caseIndex].source), sourceName);
        TZrSymbolId previousSymbolId = ZR_SEMANTIC_ID_INVALID;
        TZrSize canonicalDeclarations = 0U;
        TZrBool passed = analyzer != ZR_NULL && ast != ZR_NULL &&
                ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast) &&
                analyzer->semanticContext != ZR_NULL && analyzer->diagnostics.length == 0U;

        for (TZrSize bindingIndex = 0;
             passed && bindingIndex < cases[caseIndex].declarationCount;
             bindingIndex++) {
            SZrParserSemanticSymbolQuery declaration = {0};
            SZrParserSemanticSymbolQuery use = {0};
            SZrFileRange declarationRange = file_range_for_nth_substring_in_source(
                    cases[caseIndex].source, "result",
                    cases[caseIndex].declarationOccurrences[bindingIndex], ZR_FALSE, sourceName);
            SZrFileRange useRange = file_range_for_nth_substring_in_source(
                    cases[caseIndex].source, "result",
                    cases[caseIndex].useOccurrences[bindingIndex], ZR_FALSE, sourceName);
            SZrSymbol *symbol;

            passed = ZrParser_SemanticQuery_SymbolAt(
                    analyzer->semanticContext, declarationRange, ZR_NULL, &declaration) &&
                    ZrParser_SemanticQuery_SymbolAt(
                            analyzer->semanticContext, useRange, ZR_NULL, &use);
            symbol = passed ? ZrLanguageServer_SymbolTable_FindBySemanticId(
                    analyzer->symbolTable, use.symbolId) : ZR_NULL;
            passed = passed && symbol != ZR_NULL &&
                    declaration.symbolId != ZR_SEMANTIC_ID_INVALID &&
                    declaration.symbolId != previousSymbolId &&
                    declaration.typeId != ZR_SEMANTIC_ID_INVALID &&
                    declaration.role == ZR_SEMANTIC_REFERENCE_DECLARATION &&
                    use.role == ZR_SEMANTIC_REFERENCE_READ &&
                    use.symbolId == declaration.symbolId &&
                    use.typeId == declaration.typeId &&
                    use.declarationNode == declaration.declarationNode &&
                    use.declarationNode == symbol->astNode &&
                    use.declarationRange.source == sourceName &&
                    use.declarationRange.start.offset == declarationRange.start.offset &&
                    use.declarationRange.end.offset == declarationRange.start.offset + strlen("result");
            previousSymbolId = declaration.symbolId;
        }

        for (TZrSize symbolIndex = 0;
             passed && symbolIndex < analyzer->semanticContext->symbols.length;
             symbolIndex++) {
            const SZrSemanticSymbolRecord *symbol =
                    (const SZrSemanticSymbolRecord *)ZrCore_Array_Get(
                            &analyzer->semanticContext->symbols, symbolIndex);
            if (symbol != ZR_NULL && symbol->name != ZR_NULL &&
                strcmp(ZrCore_String_GetNativeString(symbol->name), "result") == 0) {
                canonicalDeclarations++;
            }
        }
        passed = passed && canonicalDeclarations == cases[caseIndex].declarationCount;
        if (analyzer != ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        }
        if (ast != ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
        }
        if (!passed) {
            char detail[128];
            snprintf(detail, sizeof(detail),
                     "Fixture %zu did not preserve the declaration's canonical identity and range",
                     (size_t)caseIndex);
            TEST_FAIL(timer, summary, detail);
            return;
        }
    }
    TEST_PASS(timer, summary);
}

#endif
