#ifndef ZR_TEST_LSP_FIELD_REFERENCE_CASES_H
#define ZR_TEST_LSP_FIELD_REFERENCE_CASES_H

static TZrBool lsp_field_query_at(SZrState *state, SZrLspContext *context, SZrString *uri,
                                  const char *source, const char *needle, TZrSize offset,
                                  SZrParserSemanticSymbolQuery *query,
                                  SZrLspPosition *position) {
    SZrFileRange range = {0};
    SZrSemanticAnalyzer *analyzer = find_test_analyzer(state, context, uri);
    if (analyzer == ZR_NULL ||
        !lsp_find_position_for_substring(source, needle, 0, offset, position)) {
        return ZR_FALSE;
    }
    range.source = uri;
    range.start = ZrLanguageServer_Lsp_GetDocumentFilePosition(context, uri, *position);
    range.end = range.start;
    return ZrParser_SemanticQuery_SymbolAt(analyzer->semanticContext, range, ZR_NULL, query);
}

static void test_lsp_field_references_preserve_access_and_identity(SZrState *state) {
    static const char source[] =
            "class Base {\n"
            "    pub var value: int = 1;\n"
            "    pri var secret: int = 2;\n"
            "    pro var inherited: int = 3;\n"
            "    pub static var total: int = 4;\n"
            "    pub fn own(): int { this.secret = this.secret + 1; return this.secret; }\n"
            "}\n"
            "class Derived: Base {\n"
            "    pub var child: Derived;\n"
            "    pub fn read(): int { this.inherited += 1; return this.inherited + this.value; }\n"
            "    pub fn direct(): int { super.inherited = 5; return super.inherited; }\n"
            "    pub fn chained(): int { this.child.inherited = 6; return this.child.inherited; }\n"
            "}\n"
            "class Wrapper { pub var child: Derived; }\n"
            "fn use(item: Derived): int { item.value = 7; Base.total += 1; return item.value + Derived.total; }\n"
            "fn nested(box: Wrapper): int { box.child.value = 9; return box.child.value; }";
    static const struct {
        const char *declaration;
        TZrSize declarationOffset;
        const char *use;
        TZrSize useOffset;
        EZrSemanticReferenceKind role;
        TZrSize nameLength;
    } cases[] = {
            {"var secret:", 4, "this.secret =", 5, ZR_SEMANTIC_REFERENCE_MEMBER_WRITE, 6},
            {"var secret:", 4, "return this.secret", 12, ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS, 6},
            {"var inherited:", 4, "this.inherited +=", 5, ZR_SEMANTIC_REFERENCE_MEMBER_WRITE, 9},
            {"var inherited:", 4, "return this.inherited", 12, ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS, 9},
            {"var inherited:", 4, "super.inherited =", 6, ZR_SEMANTIC_REFERENCE_MEMBER_WRITE, 9},
            {"var inherited:", 4, "return super.inherited", 13, ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS, 9},
            {"var inherited:", 4, "this.child.inherited =", 11, ZR_SEMANTIC_REFERENCE_MEMBER_WRITE, 9},
            {"var inherited:", 4, "return this.child.inherited", 18, ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS, 9},
            {"var value:", 4, "item.value =", 5, ZR_SEMANTIC_REFERENCE_MEMBER_WRITE, 5},
            {"var value:", 4, "return item.value", 12, ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS, 5},
            {"var total:", 4, "Base.total +=", 5, ZR_SEMANTIC_REFERENCE_MEMBER_WRITE, 5},
            {"var total:", 4, "Derived.total", 8, ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS, 5},
            {"var value:", 4, "box.child.value =", 10, ZR_SEMANTIC_REFERENCE_MEMBER_WRITE, 5},
            {"var value:", 4, "return box.child.value", 17, ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS, 5}};
    const char *summary = "LSP Field References Preserve Access And Identity";
    SZrTestTimer timer;
    SZrLspContext *context = ZrLanguageServer_LspContext_New(state);
    SZrString *uri = ZrCore_String_CreateFromNative(state, "file:///field_references.zr");
    SZrArray diagnostics;
    TZrBool passed;
    TEST_START(summary);
    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrLspDiagnostic *), 4);
    passed = context != ZR_NULL &&
             ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, source, strlen(source), 1) &&
             ZrLanguageServer_Lsp_GetDiagnostics(state, context, uri, &diagnostics) &&
             diagnostics.length == 0;
    for (TZrSize index = 0; passed && index < sizeof(cases) / sizeof(cases[0]); index++) {
        SZrParserSemanticSymbolQuery declaration = {0};
        SZrParserSemanticSymbolQuery use = {0};
        SZrLspPosition declarationPosition;
        SZrLspPosition usePosition;
        SZrArray definitions;
        passed = lsp_field_query_at(state, context, uri, source, cases[index].declaration,
                                   cases[index].declarationOffset, &declaration, &declarationPosition) &&
                 lsp_field_query_at(state, context, uri, source, cases[index].use,
                                   cases[index].useOffset, &use, &usePosition) &&
                 use.role == cases[index].role && use.symbolId == declaration.symbolId &&
                 use.symbolId != ZR_SEMANTIC_ID_INVALID && use.typeId == declaration.typeId &&
                 use.declarationNode == declaration.declarationNode;
        ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 1);
        passed = passed && ZrLanguageServer_Lsp_GetDefinition(
                state, context, uri, usePosition, &definitions) &&
                location_array_contains_range(&definitions, declarationPosition.line,
                        declarationPosition.character, declarationPosition.line,
                        declarationPosition.character + (TZrUInt32)cases[index].nameLength);
        ZrCore_Array_Free(state, &definitions);
    }
    if (!passed && context != ZR_NULL) {
        dump_analyzer_state(state, context, uri);
    }
    ZrCore_Array_Free(state, &diagnostics);
    ZrLanguageServer_LspContext_Free(state, context);
    if (!passed) {
        TEST_FAIL(timer, summary, "Field read/write target or declaration span was lost");
        return;
    }
    TEST_PASS(timer, summary);
}

static void test_lsp_inaccessible_and_missing_fields_keep_diagnostics(SZrState *state) {
    static const char *sources[] = {
            "class Box { pri var hidden: int; } fn use(item: Box): int { return item.hidden; }",
            "class Box { pri var hidden: int; } fn use(item: Box) { item.hidden = 1; }",
            "class Box { pro var hidden: int; } fn use(item: Box): int { return item.hidden; }",
            "class Box { pro var hidden: int; } fn use(item: Box) { item.hidden = 1; }",
            "class Box { } fn use(item: Box): int { return item.missing; }",
            "class Box { } fn use(item: Box) { item.missing = 1; }",
            "class Box { pro var hidden: int; } class Child: Box { pub fn use(item: Box): int { return item.hidden; } }",
            "class Box { pro var hidden: int; } class Child: Box { pub fn use(item: Box) { item.hidden = 1; } }",
            "class Box { pro var hidden: int; pub var other: Box; } class Child: Box { pub fn use(): int { return super.other.hidden; } }",
            "class Box { pro var hidden: int; pub var other: Box; } class Child: Box { pub fn use() { super.other.hidden = 1; } }"};
    const char *summary = "LSP Inaccessible And Missing Fields Keep Diagnostics";
    SZrTestTimer timer;
    TEST_START(summary);
    for (TZrSize index = 0; index < sizeof(sources) / sizeof(sources[0]); index++) {
        SZrLspContext *context = ZrLanguageServer_LspContext_New(state);
        SZrString *uri = ZrCore_String_CreateFromNative(state, "file:///invalid_field_reference.zr");
        SZrArray diagnostics;
        TZrBool passed;
        ZrCore_Array_Init(state, &diagnostics, sizeof(SZrLspDiagnostic *), 4);
        passed = context != ZR_NULL &&
                 ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, sources[index],
                                                     strlen(sources[index]), 1) &&
                 ZrLanguageServer_Lsp_GetDiagnostics(state, context, uri, &diagnostics) &&
                 diagnostics.length > 0;
        if (passed) {
            SZrSemanticAnalyzer *analyzer = find_test_analyzer(state, context, uri);
            for (TZrSize factIndex = 0; analyzer != ZR_NULL &&
                 factIndex < analyzer->semanticContext->referenceFacts.length; factIndex++) {
                const SZrSemanticReferenceFact *fact = (const SZrSemanticReferenceFact *)
                        ZrCore_Array_Get(&analyzer->semanticContext->referenceFacts, factIndex);
                if ((fact->kind == ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS ||
                     fact->kind == ZR_SEMANTIC_REFERENCE_MEMBER_WRITE) && fact->isResolved &&
                    fact->name != ZR_NULL &&
                    (strcmp(ZrCore_String_GetNativeString(fact->name), "hidden") == 0 ||
                     strcmp(ZrCore_String_GetNativeString(fact->name), "missing") == 0)) {
                    passed = ZR_FALSE;
                }
            }
        }
        if (!passed && context != ZR_NULL) {
            fprintf(stderr, "Invalid field case %u produced %u diagnostics\n",
                    (unsigned)index, (unsigned)diagnostics.length);
            dump_analyzer_state(state, context, uri);
        }
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        if (!passed) {
            TEST_FAIL(timer, summary, "Inaccessible or missing field was published as a resolved reference");
            return;
        }
    }
    TEST_PASS(timer, summary);
}

#endif
