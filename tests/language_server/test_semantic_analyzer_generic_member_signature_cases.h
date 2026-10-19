#ifndef ZR_TEST_SEMANTIC_ANALYZER_GENERIC_MEMBER_SIGNATURE_CASES_H
#define ZR_TEST_SEMANTIC_ANALYZER_GENERIC_MEMBER_SIGNATURE_CASES_H

static void test_semantic_analyzer_preserves_owner_generic_context_in_member_signatures(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Preserves Owner Generic Context In Member Signatures");

    TEST_INFO("Owner generic member signatures",
              "Class member signatures should resolve owner and method generic parameters without emitting compiler diagnostics");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "class Matrix<T, const N: int> { }\n"
            "class Box<T> {\n"
            "    fn shape<const N: int>(value: Matrix<T, N>): Matrix<T, N> {\n"
            "        return value;\n"
            "    }\n"
            "}\n"
            "fn use(): void {\n"
            "    var box = new Box<int>();\n"
            "    var value = new Matrix<int, 4>();\n"
            "    box.shape(value);\n"
            "}\n";
        SZrString *sourceName =
            ZrCore_String_Create(state, "owner_generic_member_signature_test.zr", 38);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer,
                      "Semantic Analyzer Preserves Owner Generic Context In Member Signatures",
                      "Failed to create semantic analyzer");
            return;
        }

        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Preserves Owner Generic Context In Member Signatures",
                      "Failed to parse test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Preserves Owner Generic Context In Member Signatures",
                      "Failed to analyze AST");
            return;
        }

        if (count_diagnostics_with_code(analyzer, "compiler_error") != 0 ||
            analyzer->diagnostics.length != 0) {
            for (TZrSize index = 0; index < analyzer->diagnostics.length; index++) {
                const SZrDiagnostic *diagnostic = *(SZrDiagnostic **)
                        ZrCore_Array_Get(&analyzer->diagnostics, index);
                if (diagnostic != ZR_NULL) {
                    fprintf(stderr, "Owner generic signature diagnostic code=%s id=%u "
                            "range=%llu..%llu message=%s\n",
                            diagnostic->code != ZR_NULL
                                    ? ZrCore_String_GetNativeString(diagnostic->code) : "<none>",
                            (unsigned)diagnostic->descriptorId,
                            (unsigned long long)diagnostic->location.start.offset,
                            (unsigned long long)diagnostic->location.end.offset,
                            diagnostic->message != ZR_NULL
                                    ? ZrCore_String_GetNativeString(diagnostic->message) : "<none>");
                }
            }
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Preserves Owner Generic Context In Member Signatures",
                      "Owner/member generic signature analysis should not emit diagnostics");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Preserves Owner Generic Context In Member Signatures");
}

#endif

