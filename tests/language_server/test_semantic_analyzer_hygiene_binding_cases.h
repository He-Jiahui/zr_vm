#ifndef ZR_TEST_SEMANTIC_ANALYZER_HYGIENE_BINDING_CASES_H
#define ZR_TEST_SEMANTIC_ANALYZER_HYGIENE_BINDING_CASES_H

#include "semantic/analyzer/semantic_analyzer_internal.h"
#include "zr_vm_parser/syntax_context.h"

typedef struct SZrTestSemanticHygieneBindingFixture {
    SZrSemanticAnalyzer analyzer;
    SZrCompilerState compiler;
    SZrAstNode *ast;
    SZrAstNode *declaration;
    SZrSyntaxContextTable *contextTable;
    SZrSyntaxContext syntaxContext;
    SZrInferredType type;
    SZrString *name;
    SZrSymbol *symbol;
    SZrFileRange nameRange;
    TZrSymbolId symbolId;
    TZrSymbolId rootSymbolId;
    TZrTypeId typeId;
} SZrTestSemanticHygieneBindingFixture;

static void semantic_hygiene_binding_fixture_free(
        SZrState *state, SZrTestSemanticHygieneBindingFixture *fixture) {
    if (fixture->analyzer.symbolTable != ZR_NULL) {
        ZrLanguageServer_SymbolTable_Free(state, fixture->analyzer.symbolTable);
    }
    if (fixture->compiler.typeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_Free(state, fixture->compiler.typeEnv);
    }
    if (fixture->analyzer.semanticContext != ZR_NULL) {
        ZrParser_SemanticContext_Free(fixture->analyzer.semanticContext);
    }
    if (fixture->ast != ZR_NULL) {
        ZrParser_Ast_Free(state, fixture->ast);
    }
    ZrParser_SyntaxContextTable_Release(fixture->contextTable);
    ZrParser_InferredType_Free(state, &fixture->type);
}

static TZrBool semantic_hygiene_binding_fixture_init(
        SZrState *state, SZrTestSemanticHygieneBindingFixture *fixture) {
    static const char source[] = "var marked: int = 1;";
    SZrString *sourceName;
    SZrAstNode *pattern;
    SZrSyntaxExpansionInfo expansion = {0};
    TZrSyntaxExpansionId expansionId;
    SZrInferredType rootType;
    TZrTypeId rootTypeId;

    memset(fixture, 0, sizeof(*fixture));
    ZrParser_InferredType_Init(state, &fixture->type, ZR_VALUE_TYPE_INT64);
    sourceName = ZrCore_String_CreateFromNative(state, "hygienic_lsp_binding.zr");
    if (sourceName == ZR_NULL) {
        return ZR_FALSE;
    }
    fixture->ast = ZrParser_Parse(state, source, sizeof(source) - 1U, sourceName);
    if (fixture->ast == ZR_NULL || fixture->ast->type != ZR_AST_SCRIPT ||
        fixture->ast->data.script.statements == ZR_NULL ||
        fixture->ast->data.script.statements->count != 1U) {
        return ZR_FALSE;
    }
    fixture->declaration = fixture->ast->data.script.statements->nodes[0];
    if (fixture->declaration == ZR_NULL ||
        fixture->declaration->type != ZR_AST_VARIABLE_DECLARATION) {
        return ZR_FALSE;
    }
    pattern = fixture->declaration->data.variableDeclaration.pattern;
    if (pattern == ZR_NULL || pattern->type != ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_FALSE;
    }
    fixture->name = pattern->data.identifier.name;
    fixture->nameRange = pattern->location;
    if (fixture->name == ZR_NULL || fixture->nameRange.source != sourceName ||
        fixture->nameRange.end.offset <= fixture->nameRange.start.offset) {
        return ZR_FALSE;
    }

    fixture->contextTable = ZrParser_SyntaxContextTable_Create(state->global, ZR_NULL);
    if (fixture->contextTable == ZR_NULL) {
        return ZR_FALSE;
    }
    expansion.stableIdentity = 0x4D314C53U;
    expansion.callSiteSpan = ZrParser_TokenSpan_None();
    expansion.definitionSiteSpan = ZrParser_TokenSpan_None();
    if (!ZrParser_SyntaxContextTable_RegisterExpansion(
                fixture->contextTable, &expansion, &expansionId)) {
        return ZR_FALSE;
    }
    fixture->syntaxContext = ZrParser_SyntaxContext_DefSite(
            fixture->contextTable, expansionId);
    if (!ZrParser_SyntaxContext_IsValid(fixture->syntaxContext) ||
        ZrParser_SyntaxContext_Equal(
                ZrParser_SyntaxContext_Normalize(
                        fixture->syntaxContext, ZR_SYNTAX_NAMESPACE_LOCAL),
                ZrParser_SyntaxContext_Root())) {
        return ZR_FALSE;
    }
    pattern->data.identifier.syntaxContext = fixture->syntaxContext;

    fixture->analyzer.state = state;
    fixture->analyzer.compilerState = &fixture->compiler;
    fixture->analyzer.semanticContext = ZrParser_SemanticContext_New(state);
    fixture->analyzer.symbolTable = ZrLanguageServer_SymbolTable_New(state);
    fixture->compiler.typeEnv = ZrParser_TypeEnvironment_New(state);
    if (fixture->analyzer.semanticContext == ZR_NULL ||
        fixture->analyzer.symbolTable == ZR_NULL || fixture->compiler.typeEnv == ZR_NULL) {
        return ZR_FALSE;
    }
    fixture->compiler.state = state;
    fixture->compiler.semanticContext = fixture->analyzer.semanticContext;
    fixture->compiler.typeEnv->semanticContext = fixture->analyzer.semanticContext;
    fixture->typeId = ZrParser_Semantic_RegisterInferredType(
            fixture->analyzer.semanticContext, &fixture->type,
            ZR_SEMANTIC_TYPE_KIND_VALUE, ZR_NULL, fixture->declaration);
    ZrParser_InferredType_Init(state, &rootType, ZR_VALUE_TYPE_BOOL);
    rootTypeId = ZrParser_Semantic_RegisterInferredType(
            fixture->analyzer.semanticContext, &rootType,
            ZR_SEMANTIC_TYPE_KIND_VALUE, ZR_NULL, fixture->declaration);
    ZrParser_InferredType_Free(state, &rootType);
    if (fixture->typeId == ZR_SEMANTIC_ID_INVALID ||
        rootTypeId == ZR_SEMANTIC_ID_INVALID || rootTypeId == fixture->typeId) {
        return ZR_FALSE;
    }

    /* Same-node/name/kind decoy makes a hardcoded root lookup observably wrong. */
    fixture->rootSymbolId = ZrParser_Semantic_RegisterSymbolWithContext(
            fixture->analyzer.semanticContext, fixture->name,
            ZR_SEMANTIC_SYMBOL_KIND_VARIABLE, rootTypeId, ZR_SEMANTIC_ID_INVALID,
            fixture->declaration, fixture->nameRange, ZrParser_SyntaxContext_Root());
    fixture->symbolId = ZrParser_Semantic_RegisterSymbolWithContext(
            fixture->analyzer.semanticContext, fixture->name,
            ZR_SEMANTIC_SYMBOL_KIND_VARIABLE, fixture->typeId, ZR_SEMANTIC_ID_INVALID,
            fixture->declaration, fixture->nameRange, fixture->syntaxContext);
    if (fixture->rootSymbolId == ZR_SEMANTIC_ID_INVALID ||
        fixture->symbolId == ZR_SEMANTIC_ID_INVALID ||
        fixture->rootSymbolId == fixture->symbolId) {
        return ZR_FALSE;
    }
    return ZrLanguageServer_SymbolTable_AddSymbolEx(
            state, fixture->analyzer.symbolTable, ZR_SYMBOL_VARIABLE, fixture->name,
            fixture->nameRange, &fixture->type, ZR_ACCESS_PUBLIC,
            fixture->declaration, &fixture->symbol) && fixture->symbol != ZR_NULL;
}

static void test_semantic_analyzer_marked_symbol_reuses_canonical_identity(SZrState *state) {
    const char *summary = "Semantic Analyzer Marked Symbol Reuses Canonical Identity";
    SZrTestTimer timer;
    SZrTestSemanticHygieneBindingFixture fixture;
    TZrBool passed;

    TEST_START(summary);
    passed = semantic_hygiene_binding_fixture_init(state, &fixture);
    if (!passed) {
        semantic_hygiene_binding_fixture_free(state, &fixture);
        TEST_FAIL(timer, summary,
                  "Hygiene fixture prerequisite failed before RegisterSymbolSemantics");
        return;
    }
    if (passed) {
        TZrSize symbolCount = fixture.analyzer.semanticContext->symbols.length;
        TZrSize typeCount = fixture.analyzer.semanticContext->types.length;
        passed = ZrLanguageServer_SemanticAnalyzer_RegisterSymbolSemantics(
                &fixture.analyzer, fixture.symbol, ZR_SEMANTIC_SYMBOL_KIND_VARIABLE,
                &fixture.type, ZR_SEMANTIC_TYPE_KIND_VALUE) &&
                fixture.symbol->semanticId == fixture.symbolId &&
                fixture.symbol->semanticId != fixture.rootSymbolId &&
                fixture.symbol->semanticTypeId == fixture.typeId &&
                fixture.symbol->overloadSetId == ZR_SEMANTIC_ID_INVALID &&
                fixture.analyzer.semanticContext->symbols.length == symbolCount &&
                fixture.analyzer.semanticContext->types.length == typeCount;
    }
    semantic_hygiene_binding_fixture_free(state, &fixture);
    if (!passed) {
        TEST_FAIL(timer, summary,
                  "Marked declaration must reuse its exact canonical IDs, not the root decoy");
        return;
    }
    TEST_PASS(timer, summary);
}

static void test_semantic_analyzer_marked_declaration_registers_canonical_binding(SZrState *state) {
    const char *summary = "Semantic Analyzer Marked Declaration Registers Canonical Binding";
    SZrTestTimer timer;
    SZrTestSemanticHygieneBindingFixture fixture;
    TZrBool passed;

    TEST_START(summary);
    passed = semantic_hygiene_binding_fixture_init(state, &fixture);
    if (!passed) {
        semantic_hygiene_binding_fixture_free(state, &fixture);
        TEST_FAIL(timer, summary,
                  "Hygiene fixture prerequisite failed before RegisterDeclarationTypeBinding");
        return;
    }
    if (passed) {
        const SZrTypeBinding *binding;
        TZrSize symbolCount = fixture.analyzer.semanticContext->symbols.length;
        TZrSize typeCount = fixture.analyzer.semanticContext->types.length;
        TZrSize bindingCount = fixture.compiler.typeEnv->variableTypes.length;

        /* Isolate this call from RegisterSymbolSemantics in the preceding case. */
        fixture.symbol->semanticId = fixture.symbolId;
        fixture.symbol->semanticTypeId = fixture.typeId;
        ZrLanguageServer_SemanticAnalyzer_RegisterDeclarationTypeBinding(
                state, &fixture.analyzer, fixture.name, &fixture.type, fixture.declaration);
        binding = ZrParser_TypeEnvironment_FindVariableBindingWithContext(
                fixture.compiler.typeEnv, fixture.name, fixture.syntaxContext);
        passed = binding != ZR_NULL && binding->symbolId == fixture.symbolId &&
                binding->typeId == fixture.typeId &&
                binding->type.baseType == ZR_VALUE_TYPE_INT64 && binding->hasDeclarationRange &&
                ZrParser_SyntaxContext_Equal(binding->syntaxContext, fixture.syntaxContext) &&
                binding->declarationRange.source == fixture.nameRange.source &&
                binding->declarationRange.start.line == fixture.nameRange.start.line &&
                binding->declarationRange.start.column == fixture.nameRange.start.column &&
                binding->declarationRange.start.offset == fixture.nameRange.start.offset &&
                binding->declarationRange.end.line == fixture.nameRange.end.line &&
                binding->declarationRange.end.column == fixture.nameRange.end.column &&
                binding->declarationRange.end.offset == fixture.nameRange.end.offset &&
                ZrParser_TypeEnvironment_FindVariableBindingWithContext(
                        fixture.compiler.typeEnv, fixture.name,
                        ZrParser_SyntaxContext_Root()) == ZR_NULL &&
                fixture.compiler.typeEnv->variableTypes.length == bindingCount + 1U &&
                fixture.analyzer.semanticContext->symbols.length == symbolCount &&
                fixture.analyzer.semanticContext->types.length == typeCount;
    }
    semantic_hygiene_binding_fixture_free(state, &fixture);
    if (!passed) {
        TEST_FAIL(timer, summary,
                  "Marked binding must preserve canonical IDs, context and source range without a root binding");
        return;
    }
    TEST_PASS(timer, summary);
}

#endif
