#ifndef ZR_VM_TEST_SEMANTIC_QUERY_TYPE_USE_CASES_H
#define ZR_VM_TEST_SEMANTIC_QUERY_TYPE_USE_CASES_H

#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/semantic_type_use.h"

static void test_type_use_query_preserves_closed_type_and_declaration_identity(void) {
    const TZrChar *source =
            "class Item { }\n"
            "class Derived<T, const N: int> { }\n"
            "fn use(): void {\n"
            "    var value: Derived<Item, 2 + 2> = null;\n"
            "    value;\n"
            "}\n";
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    const SZrSemanticSymbolRecord *declaration;
    SZrParserSemanticSymbolQuery use;
    SZrParserSemanticTypeQuery type;
    SZrFileRange position;
    TZrChar typeText[128];

    sourceName = ZrCore_String_CreateFromNative(g_state, "type_use_identity.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);
    TEST_ASSERT_FALSE(cs.hasError);

    declaration = symbol_find_registered_node(
            cs.semanticContext, ast->data.script.statements->nodes[1]);
    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, declaration->id);

    position = symbol_source_position(source, sourceName, "Derived", 1U);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_SemanticQuery_CanonicalTypeAt(
            cs.semanticContext, position, ZR_NULL, &type),
            "A closed type annotation must publish its canonical type");
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_Format(
            cs.semanticContext, type.typeId, typeText, sizeof(typeText)));
    TEST_ASSERT_EQUAL_STRING("Derived<Item, 4>", typeText);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_SemanticQuery_SymbolAt(
            cs.semanticContext, position, ZR_NULL, &use),
            "A closed type use must retain the open declaration identity");
    TEST_ASSERT_EQUAL_UINT32(declaration->id, use.symbolId);
    TEST_ASSERT_EQUAL_UINT32(type.typeId, use.typeId);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REFERENCE_TYPE, use.role);
    TEST_ASSERT_EQUAL_PTR(declaration->astNode, use.declarationNode);
    TEST_ASSERT_EQUAL_STRING("Derived", ZrCore_String_GetNativeString(use.displayName));
    TEST_ASSERT_EQUAL_UINT64(position.start.offset, use.referenceRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(position.start.offset + strlen("Derived"),
                             use.referenceRange.end.offset);
    TEST_ASSERT_EQUAL_UINT64(declaration->astNode->data.classDeclaration.nameLocation.start.offset,
                             use.declarationRange.start.offset);

    symbol_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void check_type_use_publication_identity(TZrBool missing,
                                               TZrBool ambiguous,
                                               TZrBool unresolved,
                                               TZrBool invalidRange) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrString *name = ZrCore_String_CreateFromNative(g_state, "Box");
    SZrString *module = ZrCore_String_CreateFromNative(g_state, "source.models");
    SZrString *otherModule = ZrCore_String_CreateFromNative(g_state, "other.models");
    SZrAstNode declarationNode;
    SZrAstNode decoyNode;
    SZrAstNode useNode;
    SZrIdentifier identifier = {0};
    SZrType typeUse = {0};
    TZrTypeId definition;
    TZrTypeId argument;
    TZrTypeId closed;
    TZrTypeId decoy;
    SZrParserSemanticSymbolQuery symbol;
    SZrParserSemanticTypeQuery type;
    SZrFileRange position;
    TZrSize referenceCount;

    TEST_ASSERT_NOT_NULL(context);
    definition = ZrParser_CanonicalType_InternNominal(context, module, name, 1U);
    decoy = ZrParser_CanonicalType_InternNominal(context, otherModule, name, 1U);
    argument = ZrParser_CanonicalType_InternPrimitive(context, ZR_VALUE_TYPE_INT64);
    closed = ZrParser_CanonicalType_InternGenericInstance(context, definition, &argument, 1U);
    symbol_init_node(&declarationNode, 4U, 20U);
    declarationNode.type = ZR_AST_CLASS_DECLARATION;
    declarationNode.data.classDeclaration.nameLocation = symbol_range(10U, 13U);
    symbol_init_node(&decoyNode, 22U, 38U);
    decoyNode.type = ZR_AST_CLASS_DECLARATION;
    decoyNode.data.classDeclaration.nameLocation = symbol_range(28U, 31U);
    TEST_ASSERT_EQUAL_UINT32(702U, ZrParser_Semantic_RegisterSymbolWithId(
            context, 702U, name, ZR_SEMANTIC_SYMBOL_KIND_TYPE,
            ambiguous ? definition : decoy, ZR_SEMANTIC_ID_INVALID,
            &decoyNode, decoyNode.location));
    if (!missing) {
        TEST_ASSERT_EQUAL_UINT32(701U, ZrParser_Semantic_RegisterSymbolWithId(
                context, 701U, name, ZR_SEMANTIC_SYMBOL_KIND_TYPE, definition,
                ZR_SEMANTIC_ID_INVALID, &declarationNode, declarationNode.location));
    }
    symbol_init_node(&useNode, 44U, 48U);
    useNode.type = ZR_AST_GENERIC_TYPE;
    identifier.name = name;
    useNode.data.genericType.name = &identifier;
    useNode.data.genericType.wholeRange = symbol_range(40U, 48U);
    if (invalidRange) {
        useNode.data.genericType.wholeRange.source = otherModule;
    }
    typeUse.name = &useNode;
    if (invalidRange) {
        TEST_ASSERT_FALSE(ZrParser_SemanticTypeUse_Publish(context, &typeUse, closed, ZR_TRUE));
        TEST_ASSERT_EQUAL_UINT(0U, context->referenceFacts.length);
    } else {
        TEST_ASSERT_TRUE(ZrParser_SemanticTypeUse_Publish(context, &typeUse, closed, !unresolved));
        referenceCount = context->referenceFacts.length;
        TEST_ASSERT_TRUE(ZrParser_SemanticTypeUse_Publish(context, &typeUse, closed, !unresolved));
        TEST_ASSERT_EQUAL_UINT(referenceCount, context->referenceFacts.length);
        position = symbol_range(41U, 41U);
        TEST_ASSERT_TRUE(ZrParser_SemanticQuery_CanonicalTypeAt(context, position, ZR_NULL, &type));
        TEST_ASSERT_EQUAL_UINT32(closed, type.typeId);
        if (missing || ambiguous || unresolved) {
            TEST_ASSERT_FALSE(ZrParser_SemanticQuery_SymbolAt(context, position, ZR_NULL, &symbol));
            TEST_ASSERT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, symbol.symbolId);
        } else {
            TEST_ASSERT_TRUE(ZrParser_SemanticQuery_SymbolAt(context, position, ZR_NULL, &symbol));
            TEST_ASSERT_EQUAL_UINT32(701U, symbol.symbolId);
            TEST_ASSERT_EQUAL_UINT64(10U, symbol.declarationRange.start.offset);
            TEST_ASSERT_EQUAL_UINT64(40U, symbol.referenceRange.start.offset);
            TEST_ASSERT_EQUAL_UINT64(43U, symbol.referenceRange.end.offset);
            TEST_ASSERT_TRUE(ZrParser_SemanticTypeUse_Publish(context, &typeUse, closed, ZR_FALSE));
            TEST_ASSERT_FALSE(ZrParser_SemanticQuery_SymbolAt(context, position, ZR_NULL, &symbol));
            TEST_ASSERT_TRUE(ZrParser_SemanticTypeUse_Publish(context, &typeUse, closed, ZR_TRUE));
            TEST_ASSERT_TRUE(ZrParser_SemanticQuery_SymbolAt(context, position, ZR_NULL, &symbol));
            TEST_ASSERT_EQUAL_UINT32(701U, symbol.symbolId);
            TEST_ASSERT_EQUAL_UINT(referenceCount, context->referenceFacts.length);
        }
    }
    ZrParser_SemanticContext_Free(context);
}

static void test_type_use_identity_distinguishes_same_name_modules(void) {
    check_type_use_publication_identity(ZR_FALSE, ZR_FALSE, ZR_FALSE, ZR_FALSE);
}

static void test_type_use_identity_keeps_missing_declaration_unavailable(void) {
    check_type_use_publication_identity(ZR_TRUE, ZR_FALSE, ZR_FALSE, ZR_FALSE);
}

static void test_type_use_identity_keeps_conflicting_declarations_unavailable(void) {
    check_type_use_publication_identity(ZR_FALSE, ZR_TRUE, ZR_FALSE, ZR_FALSE);
}

static void test_type_use_identity_keeps_unresolved_reference_unavailable(void) {
    check_type_use_publication_identity(ZR_FALSE, ZR_FALSE, ZR_TRUE, ZR_FALSE);
}

static void test_type_use_identity_rejects_inconsistent_whole_range(void) {
    check_type_use_publication_identity(ZR_FALSE, ZR_FALSE, ZR_FALSE, ZR_TRUE);
}

#endif
