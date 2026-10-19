#ifndef ZR_TEST_SEMANTIC_ANALYZER_GENERIC_SIGNATURE_RANGE_CASES_H
#define ZR_TEST_SEMANTIC_ANALYZER_GENERIC_SIGNATURE_RANGE_CASES_H

#include "zr_vm_parser/canonical_type.h"

static TZrBool generic_signature_range_has_exact_fact(
        const SZrSemanticContext *context,
        const SZrType *type,
        const TZrChar *source,
        SZrString *sourceName,
        const TZrChar *spelling,
        const TZrChar *baseName,
        const TZrChar *canonicalText) {
    const SZrSemanticReferenceFact *fact;
    SZrParserSemanticTypeQuery query = {0};
    SZrFileRange position;
    TZrChar formatted[128];
    TZrSize baseLength = strlen(baseName);

    if (context == ZR_NULL || type == ZR_NULL || type->name == ZR_NULL ||
        type->name->type != ZR_AST_GENERIC_TYPE ||
        type->name->data.genericType.name == ZR_NULL ||
        type->name->data.genericType.name->name == ZR_NULL ||
        strcmp(ZrCore_String_GetNativeString(
                       type->name->data.genericType.name->name), baseName) != 0 ||
        strstr(source, spelling) == ZR_NULL) {
        return ZR_FALSE;
    }
    fact = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
            context, type->name, ZR_SEMANTIC_REFERENCE_TYPE);
    position = file_range_for_nth_substring(source, spelling, 0U, ZR_FALSE);
    position.source = sourceName;
    if (fact == ZR_NULL || fact->node != type->name ||
        fact->kind != ZR_SEMANTIC_REFERENCE_TYPE || !fact->isResolved ||
        fact->typeId == ZR_SEMANTIC_ID_INVALID ||
        ZrParser_CanonicalType_Find(context, fact->typeId) == ZR_NULL ||
        fact->name == ZR_NULL ||
        strcmp(ZrCore_String_GetNativeString(fact->name), baseName) != 0 ||
        fact->range.source == ZR_NULL ||
        !ZrCore_String_Equal(fact->range.source, sourceName) ||
        fact->range.start.offset != position.start.offset ||
        fact->range.end.offset != position.start.offset + baseLength ||
        fact->range.start.line != position.start.line ||
        fact->range.end.line != position.start.line ||
        fact->range.start.column != position.start.column ||
        fact->range.end.column != position.start.column + (TZrInt32)baseLength ||
        !ZrParser_SemanticQuery_CanonicalTypeAt(context, position, ZR_NULL, &query) ||
        query.reference != fact || query.typeId != fact->typeId ||
        !ZrParser_CanonicalType_Format(context, fact->typeId, formatted, sizeof(formatted))) {
        return ZR_FALSE;
    }
    return strcmp(formatted, canonicalText) == 0;
}

static void test_semantic_analyzer_generic_signature_query_uses_exact_type_reference_range(
        SZrState *state) {
    static const TZrChar source[] =
            "resource class Hero {\n"
            "    pub @constructor() {\n"
            "    }\n"
            "}\n"
            "fn take(seed: Shared<Hero>): Unique<Hero> {\n"
            "    return own Hero();\n"
            "}\n"
            "fn use(sharedSeed: Shared<Hero>): void {\n"
            "    var hero = take(sharedSeed);\n"
            "}\n";
    const TZrChar *summary =
            "Semantic Analyzer Generic Signature Query Uses Exact Type Reference Range";
    const TZrChar *failure = ZR_NULL;
    SZrTestTimer timer;
    SZrSemanticAnalyzer *analyzer = ZR_NULL;
    SZrAstNode *ast = ZR_NULL;
    SZrAstNode *function;
    SZrAstNode *parameter;
    SZrString *sourceName;
    SZrHoverInfo *hover = ZR_NULL;
    SZrFileRange position;
    const TZrChar *text;

    TEST_START(summary);
    analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    sourceName = ZrCore_String_CreateFromNative(state, "generic_signature_range.zr");
    if (sourceName == ZR_NULL || analyzer == ZR_NULL) {
        failure = "Fixture prerequisite: signature range source and analyzer must initialize";
        goto cleanup;
    }
    ast = ZrParser_Parse(state, source, sizeof(source) - 1U, sourceName);
    if (ast == ZR_NULL || ast->type != ZR_AST_SCRIPT ||
        ast->data.script.statements == ZR_NULL ||
        ast->data.script.statements->count != 3U ||
        ast->data.script.statements->nodes == ZR_NULL) {
        failure = "Fixture prerequisite: ownership signature source must parse into three declarations";
        goto cleanup;
    }
    function = ast->data.script.statements->nodes[1];
    if (function == ZR_NULL || function->type != ZR_AST_FUNCTION_DECLARATION ||
        function->data.functionDeclaration.name == ZR_NULL ||
        function->data.functionDeclaration.name->name == ZR_NULL ||
        strcmp(ZrCore_String_GetNativeString(
                       function->data.functionDeclaration.name->name), "take") != 0 ||
        function->data.functionDeclaration.params == ZR_NULL ||
        function->data.functionDeclaration.params->count != 1U ||
        function->data.functionDeclaration.params->nodes == ZR_NULL ||
        function->data.functionDeclaration.returnType == ZR_NULL) {
        failure = "Fixture prerequisite: take must retain its real parameter and return type ASTs";
        goto cleanup;
    }
    parameter = function->data.functionDeclaration.params->nodes[0];
    if (parameter == ZR_NULL || parameter->type != ZR_AST_PARAMETER ||
        parameter->data.parameter.typeInfo == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast) ||
        analyzer->compilerState == ZR_NULL || analyzer->semanticContext == ZR_NULL ||
        analyzer->compilerState->semanticContext != analyzer->semanticContext) {
        failure = "Fixture prerequisite: real signature analysis must publish one shared semantic snapshot";
        goto cleanup;
    }
    if (!generic_signature_range_has_exact_fact(
                analyzer->semanticContext, parameter->data.parameter.typeInfo,
                source, sourceName, "Shared<Hero>", "Shared", "Shared<Hero>") ||
        !generic_signature_range_has_exact_fact(
                analyzer->semanticContext, function->data.functionDeclaration.returnType,
                source, sourceName, "Unique<Hero>", "Unique", "Unique<Hero>")) {
        failure = "Fixture prerequisite: parsed generic TYPE facts must retain exact AST identity, canonical type and basename ranges";
        goto cleanup;
    }
    if (strstr(source, "take(sharedSeed)") == ZR_NULL) {
        failure = "Fixture prerequisite: the actual take call must exist in the source";
        goto cleanup;
    }
    position = file_range_for_nth_substring(source, "take(sharedSeed)", 0U, ZR_FALSE);
    position.source = sourceName;
    if (!ZrLanguageServer_SemanticAnalyzer_GetHoverInfo(state, analyzer, position, &hover) ||
        hover == ZR_NULL || (text = hover_contents_string(hover)) == ZR_NULL) {
        failure = "Fixture prerequisite: the real take call must resolve to a hover result";
        goto cleanup;
    }
    if (strstr(text, "Signature: take(seed: Shared<Hero>): Unique<Hero>") == ZR_NULL ||
        strstr(text, "cannot infer exact type") != ZR_NULL) {
        failure = "Generic signatures must consume the resolved TYPE fact for their exact source reference";
    }

cleanup:
    if (hover != ZR_NULL) {
        ZrLanguageServer_HoverInfo_Free(state, hover);
    }
    ZrParser_Ast_Free(state, ast);
    if (analyzer != ZR_NULL) {
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }
    if (failure != ZR_NULL) {
        TEST_FAIL(timer, summary, failure);
    } else {
        TEST_PASS(timer, summary);
    }
}

#endif
