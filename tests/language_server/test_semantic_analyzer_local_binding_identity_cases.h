#ifndef ZR_TEST_SEMANTIC_ANALYZER_LOCAL_BINDING_IDENTITY_CASES_H
#define ZR_TEST_SEMANTIC_ANALYZER_LOCAL_BINDING_IDENTITY_CASES_H

/** 以遮蔽和显式/推断返回类型的样例检查局部声明与读取共享规范 SymbolId，且同名不同作用域仍相互独立。 */
static void test_semantic_analyzer_preserves_local_binding_identity(SZrState *state) {
    /* 同名 result 的第 N 次声明与读取位置必须成对维护；这些索引绑定下方固定源码样例。 */
    static const struct {
        const char *source;
        TZrSize declarationOccurrences[2];
        TZrSize useOccurrences[2];
        TZrSize declarationCount;
    } cases[] = {
        {"fn run(seed: int) {\n"
         "    var result = seed + 1;\n"
         "    return result;\n"
         "}\n", {0U, 0U}, {1U, 0U}, 1U},
        {"fn run(seed: int) {\n"
         "    var result = seed;\n"
         "    {\n"
         "        var result = seed + 1;\n"
         "        result + 2;\n"
         "    }\n"
         "    return result;\n"
         "}\n", {0U, 1U}, {3U, 2U}, 2U},
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
        TZrSize parameterDeclarations = 0U;
        TZrSize functionDeclarations = 0U;
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
            if (symbol != ZR_NULL && symbol->name != ZR_NULL &&
                strcmp(ZrCore_String_GetNativeString(symbol->name), "seed") == 0) {
                parameterDeclarations++;
                passed = symbol->astNode != ZR_NULL &&
                         symbol->kind == ZR_SEMANTIC_SYMBOL_KIND_PARAMETER;
            }
            if (symbol != ZR_NULL && symbol->name != ZR_NULL &&
                strcmp(ZrCore_String_GetNativeString(symbol->name), "run") == 0) {
                functionDeclarations++;
            }
        }
        passed = passed && canonicalDeclarations == cases[caseIndex].declarationCount &&
                 parameterDeclarations == 1U && functionDeclarations == 1U;
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

/** 当局部类型缺失、初值未解析或注解无效时，仍要求声明与使用共享身份，不能借用同名全局变量的类型。 */
// BUG: 每个成功夹具的 hover 对象未调用 HoverInfo_Free；三种未知类型场景各泄漏一个原生对象。
static void test_semantic_analyzer_unknown_local_preserves_identity(SZrState *state) {
    const char *summary = "Semantic Analyzer Unknown Local Preserves Identity";
    const char *sources[] = {
            "var missing: int = 1;\n"
            "fn run() {\n"
            "    var missing;\n"
            "    missing;\n"
            "}\n",
            "var missing: int = 1;\n"
            "fn run() {\n"
            "    var missing = absent;\n"
            "    missing;\n"
            "}\n",
            "var missing: int = 1;\n"
            "fn run() {\n"
            "    var missing: Unavailable;\n"
            "    missing;\n"
            "}\n"
    };
    SZrTestTimer timer;
    TEST_START(summary);
    for (TZrSize caseIndex = 0U; caseIndex < sizeof(sources) / sizeof(sources[0]); caseIndex++) {
        const char *source = sources[caseIndex];
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        SZrString *sourceName = ZrCore_String_CreateFromNative(state, "unknown_local_identity.zr");
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrParserSemanticSymbolQuery declaration = {0};
        SZrParserSemanticSymbolQuery usage = {0};
        SZrHoverInfo *hover = ZR_NULL;
        SZrFileRange declarationRange = file_range_for_nth_substring_in_source(
                source, "missing", 1U, ZR_FALSE, sourceName);
        SZrFileRange usageRange = file_range_for_nth_substring_in_source(
                source, "missing", 2U, ZR_FALSE, sourceName);
        TZrBool passed;

        passed = analyzer != ZR_NULL && ast != ZR_NULL &&
                ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast) &&
                ZrParser_SemanticQuery_SymbolAt(
                        analyzer->semanticContext, declarationRange, ZR_NULL, &declaration) &&
                ZrParser_SemanticQuery_SymbolAt(
                        analyzer->semanticContext, usageRange, ZR_NULL, &usage) &&
                declaration.symbolId != ZR_SEMANTIC_ID_INVALID &&
                declaration.symbolId == usage.symbolId &&
                declaration.declarationNode == usage.declarationNode &&
                declaration.typeId == ZR_SEMANTIC_ID_INVALID &&
                usage.typeId == ZR_SEMANTIC_ID_INVALID &&
                ZrLanguageServer_SemanticAnalyzer_GetHoverInfo(
                        state, analyzer, usageRange, &hover) && hover != ZR_NULL &&
                strstr(hover_contents_string(hover), "cannot infer exact type") != ZR_NULL &&
                strstr(hover_contents_string(hover), "Type: int") == ZR_NULL;
        const SZrSemanticSymbolRecord *record = passed
                ? ZrParser_Semantic_FindSymbolById(analyzer->semanticContext, declaration.symbolId)
                : ZR_NULL;
        passed = passed && record != ZR_NULL && record->typeId == ZR_SEMANTIC_ID_INVALID;
        if (analyzer != ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        }
        if (ast != ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
        }
        if (!passed) {
            char detail[192];
            snprintf(detail, sizeof(detail),
                     "Unknown-type fixture %zu lost its declaration identity or borrowed the outer type",
                     (size_t)caseIndex);
            TEST_FAIL(timer, summary, detail);
            return;
        }
    }
    TEST_PASS(timer, summary);
}

/** 当形参类型无法解析时检查其身份不回退到同名全局绑定，函数返回签名也不得伪称可精确推断。 */
// BUG: 每个成功夹具的 hover 对象未调用 HoverInfo_Free；两种未知形参场景各泄漏一个原生对象。
static void test_semantic_analyzer_unknown_parameter_preserves_identity(SZrState *state) {
    const char *summary = "Semantic Analyzer Unknown Parameter Preserves Identity";
    const char *sources[] = {
            "var missing: int = 1;\n"
            "fn run(missing: Unavailable) {\n"
            "    return missing;\n"
            "}\n"
            "run(null);\n",
            "var missing: int = 1;\n"
            "fn run(missing: Unavailable): int {\n"
            "    return missing;\n"
            "}\n"
            "run(null);\n"
    };
    SZrTestTimer timer;
    TEST_START(summary);
    for (TZrSize caseIndex = 0U; caseIndex < sizeof(sources) / sizeof(sources[0]); caseIndex++) {
        const char *source = sources[caseIndex];
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        SZrString *sourceName = ZrCore_String_CreateFromNative(state, "unknown_parameter_identity.zr");
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrParserSemanticSymbolQuery declaration = {0};
        SZrParserSemanticSymbolQuery usage = {0};
        SZrParserSemanticSymbolQuery callable = {0};
        SZrHoverInfo *hover = ZR_NULL;
        SZrFileRange declarationRange = file_range_for_nth_substring_in_source(
                source, "missing", 1U, ZR_FALSE, sourceName);
        SZrFileRange usageRange = file_range_for_nth_substring_in_source(
                source, "missing", 2U, ZR_FALSE, sourceName);
        SZrFileRange callableRange = file_range_for_nth_substring_in_source(
                source, "run", 1U, ZR_FALSE, sourceName);
        TZrBool passed;

        passed = analyzer != ZR_NULL && ast != ZR_NULL &&
                ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast) &&
                ZrParser_SemanticQuery_SymbolAt(
                        analyzer->semanticContext, declarationRange, ZR_NULL, &declaration) &&
                ZrParser_SemanticQuery_SymbolAt(
                        analyzer->semanticContext, usageRange, ZR_NULL, &usage) &&
                ZrParser_SemanticQuery_SymbolAt(
                        analyzer->semanticContext, callableRange, ZR_NULL, &callable) &&
                declaration.symbolId != ZR_SEMANTIC_ID_INVALID &&
                declaration.symbolId == usage.symbolId &&
                declaration.declarationNode == usage.declarationNode &&
                declaration.typeId == ZR_SEMANTIC_ID_INVALID &&
                usage.typeId == ZR_SEMANTIC_ID_INVALID &&
                callable.typeId == ZR_SEMANTIC_ID_INVALID &&
                ZrLanguageServer_SemanticAnalyzer_GetHoverInfo(
                        state, analyzer, callableRange, &hover) && hover != ZR_NULL &&
                strstr(hover_contents_string(hover), "cannot infer exact type") != ZR_NULL;
        const SZrSemanticSymbolRecord *parameterRecord = passed
                ? ZrParser_Semantic_FindSymbolById(analyzer->semanticContext, declaration.symbolId)
                : ZR_NULL;
        const SZrSemanticSymbolRecord *callableRecord = passed
                ? ZrParser_Semantic_FindSymbolById(analyzer->semanticContext, callable.symbolId)
                : ZR_NULL;
        passed = passed && parameterRecord != ZR_NULL && callableRecord != ZR_NULL &&
                 parameterRecord->kind == ZR_SEMANTIC_SYMBOL_KIND_PARAMETER &&
                 callableRecord->kind == ZR_SEMANTIC_SYMBOL_KIND_FUNCTION &&
                 parameterRecord->typeId == ZR_SEMANTIC_ID_INVALID &&
                 callableRecord->typeId == ZR_SEMANTIC_ID_INVALID;
        if (analyzer != ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        }
        if (ast != ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
        }
        if (!passed) {
            TEST_FAIL(timer, summary, "An unavailable parameter type must not expose an outer binding or infer its return type");
            return;
        }
    }
    TEST_PASS(timer, summary);
}

#endif
