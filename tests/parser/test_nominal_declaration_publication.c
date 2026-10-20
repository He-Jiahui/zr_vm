#include "unity.h"

#include <stdint.h>
#include <string.h>

#include "compiler/compiler_internal.h"
#include "harness/runtime_support.h"
#include "syntax/syntax_context_internal.h"
#include "type_inference/type_inference_internal.h"
#include "type_inference/type_inference_nominal_prototype.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/comptime_contract.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic.h"

static SZrState *g_state;
static SZrCompilerState g_compiler;
static TZrBool g_compilerInitialized;
static SZrAstNode *g_ast;
static SZrSyntaxContextTable *g_table;
static SZrSyntaxContext g_definition;
static TZrSyntaxExpansionId g_expansionId;
static SZrInferredType g_inferred;

typedef struct STestPublication {
    TZrSymbolId symbolId;
    TZrTypeId definitionTypeId;
    TZrTypeId typeId;
} STestPublication;

void setUp(void) {
    SZrSyntaxExpansionInfo expansion = {0};

    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
    ZrParser_InferredType_Init(g_state, &g_inferred, ZR_VALUE_TYPE_OBJECT);
    g_table = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_table);
    expansion.stableIdentity = 0xB101U;
    expansion.callSiteSpan = ZrParser_TokenSpan_None();
    expansion.definitionSiteSpan = ZrParser_TokenSpan_None();
    TEST_ASSERT_TRUE(ZrParser_SyntaxContextTable_RegisterExpansion(
            g_table, &expansion, &g_expansionId));
    g_definition = ZrParser_SyntaxContext_DefSite(g_table, g_expansionId);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(g_definition));
    ZrParser_CompilerState_Init(&g_compiler, g_state);
    g_compilerInitialized = ZR_TRUE;
    TEST_ASSERT_NOT_NULL(g_compiler.typeEnv);
    TEST_ASSERT_NOT_NULL(g_compiler.compileTimeTypeEnv);
    TEST_ASSERT_NOT_NULL(g_compiler.semanticContext);
}

void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrParser_InferredType_Free(g_state, &g_inferred);
    }
    if (g_compilerInitialized) {
        ZrParser_CompilerState_Free(&g_compiler);
        g_compilerInitialized = ZR_FALSE;
    }
    if (g_ast != ZR_NULL) {
        ZrParser_Ast_Free(g_state, g_ast);
        g_ast = ZR_NULL;
    }
    if (g_table != ZR_NULL) {
        ZrParser_SyntaxContextTable_Release(g_table);
        g_table = ZR_NULL;
    }
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

static SZrAstNodeArray *parse_source(const TZrChar *source) {
    SZrString *name = ZrCore_String_CreateFromNative(g_state, "nominal_publication.zr");

    TEST_ASSERT_NOT_NULL(name);
    g_ast = ZrParser_Parse(g_state, source, strlen(source), name);
    TEST_ASSERT_NOT_NULL(g_ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_ast->type);
    TEST_ASSERT_NOT_NULL(g_ast->data.script.statements);
    g_compiler.scriptAst = g_ast;
    return g_ast->data.script.statements;
}

static SZrIdentifier *declaration_identifier(SZrAstNode *node) {
    if (node->type == ZR_AST_UNION_DECLARATION) {
        return node->data.unionDeclaration.name;
    }
    if (node->type == ZR_AST_ENUM_DECLARATION) {
        return node->data.enumDeclaration.name;
    }
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_DECLARATION, node->type);
    return node->data.structDeclaration.name;
}

static SZrAstNodeArray *parse_struct_pair(void) {
    SZrAstNodeArray *declarations = parse_source(
            "struct Parcel { var value: int; }"
            "struct Parcel { var text: string; }");

    TEST_ASSERT_EQUAL_UINT(2U, declarations->count);
    return declarations;
}

static SZrAstNodeArray *parse_union_pair(void) {
    SZrAstNodeArray *declarations = parse_source(
            "union Parcel { Empty; }"
            "union Parcel { Value(value: int); }");

    TEST_ASSERT_EQUAL_UINT(2U, declarations->count);
    TEST_ASSERT_EQUAL_INT(ZR_AST_UNION_DECLARATION, declarations->nodes[0]->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_UNION_DECLARATION, declarations->nodes[1]->type);
    return declarations;
}

static SZrAstNodeArray *parse_extern_struct_pair(void) {
    SZrAstNodeArray *statements = parse_source(
            "native extern(\"fixture\") {"
            "struct Parcel { var value: int; }"
            "struct Parcel { var text: string; } }");

    TEST_ASSERT_EQUAL_UINT(1U, statements->count);
    TEST_ASSERT_EQUAL_INT(ZR_AST_EXTERN_BLOCK, statements->nodes[0]->type);
    TEST_ASSERT_NOT_NULL(statements->nodes[0]->data.externBlock.declarations);
    TEST_ASSERT_EQUAL_UINT(2U, statements->nodes[0]->data.externBlock.declarations->count);
    return statements->nodes[0]->data.externBlock.declarations;
}

static STestPublication prepare_publication(SZrAstNode *declaration,
        SZrSyntaxContext context) {
    STestPublication publication;
    SZrIdentifier *name = declaration_identifier(declaration);
    TZrTypeId variant;

    name->syntaxContext = context;
    publication.symbolId = ZrParser_Semantic_ReserveSymbolId(g_compiler.semanticContext);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, publication.symbolId);
    publication.definitionTypeId = ZrParser_CanonicalType_InternNominal(
            g_compiler.semanticContext, ZR_NULL, name->name,
            context.markId == 0U ? 0U : publication.symbolId);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, publication.definitionTypeId);
    variant = ZrParser_CanonicalType_InternPrimitive(
            g_compiler.semanticContext, ZR_VALUE_TYPE_INT64);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, variant);
    publication.typeId = ZrParser_CanonicalType_InternUnion(
            g_compiler.semanticContext, publication.definitionTypeId, &variant, 1U);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, publication.typeId);
    return publication;
}

static TZrBool publish(SZrAstNode *declaration, STestPublication publication) {
    return ZrParser_Semantic_PublishCanonicalTypeSymbol(
            g_compiler.semanticContext, publication.typeId, ZR_SEMANTIC_TYPE_KIND_UNION,
            declaration_identifier(declaration)->name, declaration,
            publication.symbolId, declaration->location);
}

static void warm_compiler_error_storage(void) {
    TZrBool suppressErrorOutput = g_compiler.suppressErrorOutput;

    g_compiler.suppressErrorOutput = ZR_TRUE;
    ZrParser_Compiler_Error(
            &g_compiler,
            "warm compiler error storage before measuring unpublished prototype cleanup",
            (SZrFileRange){0});
    g_compiler.suppressErrorOutput = suppressErrorOutput;
    g_compiler.hasError = ZR_FALSE;
    g_compiler.hadRecoverableError = ZR_FALSE;
    g_compiler.errorMessage = ZR_NULL;
}

static const SZrSemanticTypeRecord *find_type_record(TZrTypeId typeId) {
    const SZrSemanticTypeRecord *match = ZR_NULL;

    for (TZrSize index = 0U; index < g_compiler.semanticContext->types.length; index++) {
        const SZrSemanticTypeRecord *record =
                ZrCore_Array_Get(&g_compiler.semanticContext->types, index);

        if (record != ZR_NULL && record->id == typeId) {
            match = record;
            break;
        }
    }
    TEST_ASSERT_NOT_NULL_MESSAGE(match, "Published canonical type must have a semantic record");
    return match;
}

static void test_canonical_publisher_root_prerequisite(void) {
    SZrAstNode *declaration = parse_struct_pair()->nodes[0];
    STestPublication publication = prepare_publication(
            declaration, ZrParser_SyntaxContext_Root());

    TEST_ASSERT_TRUE(publish(declaration, publication));
    TEST_ASSERT_EQUAL_UINT(1U, g_compiler.semanticContext->types.length);
    TEST_ASSERT_EQUAL_UINT(1U, g_compiler.semanticContext->symbols.length);
    TEST_ASSERT_NOT_NULL(ZrParser_Semantic_FindSymbolById(
            g_compiler.semanticContext, publication.symbolId));
}

static void test_canonical_publisher_preserves_definition_context(void) {
    SZrAstNode *declaration = parse_struct_pair()->nodes[0];
    STestPublication publication = prepare_publication(declaration, g_definition);
    const SZrSemanticSymbolRecord *symbol;

    TEST_ASSERT_TRUE(publish(declaration, publication));
    symbol = ZrParser_Semantic_FindSymbolById(g_compiler.semanticContext,
            publication.symbolId);
    TEST_ASSERT_NOT_NULL(symbol);
    /* Check ownership before any query can dereference the published table. */
    TEST_ASSERT_EQUAL_PTR(g_table, symbol->syntaxContext.table);
    TEST_ASSERT_EQUAL_UINT32(g_definition.markId, symbol->syntaxContext.markId);
    TEST_ASSERT_EQUAL_PTR(declaration, symbol->astNode);
}

static void test_canonical_publisher_union_record_preserves_nominal_token(void) {
    SZrAstNode *declaration = parse_struct_pair()->nodes[0];
    STestPublication publication = prepare_publication(declaration, g_definition);

    TEST_ASSERT_TRUE(publish(declaration, publication));
    TEST_ASSERT_EQUAL_UINT32(publication.symbolId,
            find_type_record(publication.typeId)->inferredType.nominalDefinitionToken);
}

static void test_canonical_publisher_allows_distinct_item_contexts(void) {
    SZrAstNodeArray *declarations = parse_struct_pair();
    STestPublication root = prepare_publication(
            declarations->nodes[0], ZrParser_SyntaxContext_Root());
    STestPublication definition = prepare_publication(declarations->nodes[1], g_definition);

    TEST_ASSERT_TRUE(publish(declarations->nodes[0], root));
    TEST_ASSERT_TRUE(publish(declarations->nodes[1], definition));
    TEST_ASSERT_EQUAL_UINT(2U, g_compiler.semanticContext->types.length);
    TEST_ASSERT_EQUAL_UINT(2U, g_compiler.semanticContext->symbols.length);
}

static void test_canonical_publisher_rejects_same_item_context(void) {
    SZrAstNodeArray *declarations = parse_struct_pair();
    STestPublication first = prepare_publication(declarations->nodes[0], g_definition);
    STestPublication second = prepare_publication(declarations->nodes[1], g_definition);

    TEST_ASSERT_TRUE(publish(declarations->nodes[0], first));
    TEST_ASSERT_FALSE(publish(declarations->nodes[1], second));
    TEST_ASSERT_EQUAL_UINT(1U, g_compiler.semanticContext->types.length);
    TEST_ASSERT_EQUAL_UINT(1U, g_compiler.semanticContext->symbols.length);
}

static void test_canonical_publisher_rejects_invalid_context_without_publication(void) {
    SZrAstNode *declaration = parse_struct_pair()->nodes[0];
    SZrSyntaxContext invalid = {g_table, UINT32_MAX};
    STestPublication publication = prepare_publication(declaration, invalid);

    TEST_ASSERT_FALSE(publish(declaration, publication));
    TEST_ASSERT_EQUAL_UINT(0U, g_compiler.semanticContext->types.length);
    TEST_ASSERT_EQUAL_UINT(0U, g_compiler.semanticContext->symbols.length);
}

static void test_canonical_publisher_rejects_normalization_budget_failure(void) {
    SZrAstNode *declaration = parse_struct_pair()->nodes[0];
    STestPublication publication = prepare_publication(declaration, g_definition);
    SZrParserComptimeBudget *budget = ZrParser_SyntaxContext_Budget(g_table);

    TEST_ASSERT_NOT_NULL(budget);
    budget->limits.heapBytes = budget->usage.heapBytes;
    TEST_ASSERT_FALSE(publish(declaration, publication));
    TEST_ASSERT_EQUAL_UINT(0U, g_compiler.semanticContext->types.length);
    TEST_ASSERT_EQUAL_UINT(0U, g_compiler.semanticContext->symbols.length);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES, budget->exceededResource);
}

static void test_canonical_publisher_unwraps_generic_union_definition(void) {
    SZrAstNode *declaration = parse_struct_pair()->nodes[0];
    STestPublication publication = prepare_publication(declaration, g_definition);
    TZrTypeId argument = ZrParser_CanonicalType_InternPrimitive(
            g_compiler.semanticContext, ZR_VALUE_TYPE_INT64);
    TZrTypeId instance = ZrParser_CanonicalType_InternGenericInstance(
            g_compiler.semanticContext, publication.definitionTypeId, &argument, 1U);

    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, instance);
    publication.typeId = ZrParser_CanonicalType_InternUnion(
            g_compiler.semanticContext, instance, &argument, 1U);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, publication.typeId);
    TEST_ASSERT_TRUE(publish(declaration, publication));
    TEST_ASSERT_EQUAL_UINT32(publication.symbolId,
            find_type_record(publication.typeId)->inferredType.nominalDefinitionToken);
}

static void assert_published_binding_adoption(SZrSyntaxContext context) {
    SZrAstNode *declaration = parse_struct_pair()->nodes[0];
    STestPublication publication = prepare_publication(declaration, context);
    const SZrNamedTypeBinding *binding;
    const SZrSemanticSymbolRecord *symbol;

    TEST_ASSERT_TRUE(publish(declaration, publication));
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterTypeDeclaration(
            g_state, g_compiler.typeEnv, declaration_identifier(declaration)->name,
            declaration));
    binding = ZrParser_TypeEnvironment_FindTypeBindingWithContext(g_compiler.typeEnv,
            declaration_identifier(declaration)->name, context);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_UINT32(publication.symbolId, binding->symbolId);
    TEST_ASSERT_EQUAL_UINT32(publication.definitionTypeId, binding->typeId);
    TEST_ASSERT_EQUAL_PTR(declaration, binding->declarationNode);
    TEST_ASSERT_EQUAL_UINT(1U, g_compiler.semanticContext->symbols.length);
    TEST_ASSERT_EQUAL_UINT(1U, g_compiler.semanticContext->types.length);
    TEST_ASSERT_EQUAL_UINT(context.markId == 0U ? 1U : 0U,
            g_compiler.typeEnv->typeNames.length);
    symbol = ZrParser_Semantic_FindSymbolById(g_compiler.semanticContext, publication.symbolId);
    TEST_ASSERT_NOT_NULL(symbol);
    TEST_ASSERT_EQUAL_UINT32(publication.typeId, symbol->typeId);
}

static void test_root_type_binding_adopts_published_union_identity(void) {
    assert_published_binding_adoption(ZrParser_SyntaxContext_Root());
}

static void test_definition_type_binding_adopts_published_union_identity(void) {
    assert_published_binding_adoption(g_definition);
}

static void test_canonical_publisher_retains_context_owner(void) {
    SZrAstNode *declaration = parse_struct_pair()->nodes[0];
    STestPublication publication = prepare_publication(declaration, g_definition);
    const SZrSemanticSymbolRecord *symbol;

    TEST_ASSERT_TRUE(publish(declaration, publication));
    TEST_ASSERT_EQUAL_UINT(1U, g_compiler.semanticContext->syntaxContextTables.length);
    symbol = ZrParser_Semantic_FindSymbolById(g_compiler.semanticContext, publication.symbolId);
    TEST_ASSERT_NOT_NULL(symbol);
    TEST_ASSERT_EQUAL_PTR(g_table, symbol->syntaxContext.table);
    ZrParser_SyntaxContextTable_Release(g_table);
    g_table = ZR_NULL;
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(symbol->syntaxContext));
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Equal(g_definition, symbol->syntaxContext));
}

static void assert_source_binding(SZrAstNode *declaration, SZrSyntaxContext context,
        EZrCanonicalTypeKind projectedKind) {
    SZrString *name = declaration_identifier(declaration)->name;
    const SZrNamedTypeBinding *binding = ZrParser_TypeEnvironment_FindTypeBindingWithContext(
            g_compiler.typeEnv, name, context);
    const SZrSemanticSymbolRecord *symbol;
    const SZrCanonicalTypeNode *definition;
    const SZrCanonicalTypeNode *projection;
    const SZrNamedTypeBinding *compileTimeBinding;
    TZrTypeId projectedTypeId;

    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_PTR(declaration, binding->declarationNode);
    symbol = ZrParser_Semantic_FindSymbolById(g_compiler.semanticContext, binding->symbolId);
    TEST_ASSERT_NOT_NULL(symbol);
    TEST_ASSERT_EQUAL_PTR(declaration, symbol->astNode);
    TEST_ASSERT_EQUAL_PTR(context.table, symbol->syntaxContext.table);
    TEST_ASSERT_EQUAL_UINT32(context.markId, symbol->syntaxContext.markId);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Equal(
            ZrParser_SyntaxContext_Normalize(context, ZR_SYNTAX_NAMESPACE_ITEM),
            ZrParser_SyntaxContext_Normalize(symbol->syntaxContext, ZR_SYNTAX_NAMESPACE_ITEM)));
    definition = ZrParser_CanonicalType_Find(g_compiler.semanticContext, binding->typeId);
    TEST_ASSERT_NOT_NULL(definition);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_NOMINAL, definition->kind);
    if (!ZrParser_SyntaxContext_Equal(
                ZrParser_SyntaxContext_Normalize(context, ZR_SYNTAX_NAMESPACE_ITEM),
                ZrParser_SyntaxContext_Root())) {
        TEST_ASSERT_EQUAL_UINT32(binding->symbolId, definition->data.nominal.definitionToken);
    }
    projection = ZrParser_CanonicalType_Find(g_compiler.semanticContext, symbol->typeId);
    TEST_ASSERT_NOT_NULL(projection);
    TEST_ASSERT_EQUAL_INT(projectedKind, projection->kind);
    projectedTypeId = symbol->typeId;
    if (projectedKind == ZR_CANONICAL_TYPE_UNION) {
        TEST_ASSERT_EQUAL_UINT32(binding->typeId, projection->data.unionType.definitionTypeId);
    }
    compileTimeBinding = ZrParser_TypeEnvironment_FindTypeBindingWithContext(
            g_compiler.compileTimeTypeEnv, name, context);
    TEST_ASSERT_NOT_NULL(compileTimeBinding);
    TEST_ASSERT_EQUAL_PTR(declaration, compileTimeBinding->declarationNode);
    ZrParser_InferredType_Free(g_state, &g_inferred);
    ZrParser_InferredType_Copy(g_state, &g_inferred,
            &find_type_record(projectedTypeId)->inferredType);
    TEST_ASSERT_EQUAL_UINT32(projectedTypeId,
            ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, &g_inferred));
    TEST_ASSERT_NOT_NULL(type_inference_find_nominal_prototype(&g_compiler, &g_inferred));
    TEST_ASSERT_EQUAL_PTR(declaration,
            type_inference_find_nominal_prototype(&g_compiler, &g_inferred)->declarationNode);
}

static void test_union_root_compilation_prerequisite(void) {
    SZrAstNode *declaration = parse_union_pair()->nodes[0];

    ZrParser_Compiler_CompileUnionDeclaration(&g_compiler, declaration);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_UINT(1U, g_compiler.typePrototypes.length);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_LookupType(
            g_compiler.typeEnv, declaration_identifier(declaration)->name));
}

static void test_union_definition_publication_preserves_structure_and_identity(void) {
    SZrAstNode *declaration = parse_union_pair()->nodes[0];

    declaration_identifier(declaration)->syntaxContext = g_definition;
    ZrParser_Compiler_CompileUnionDeclaration(&g_compiler, declaration);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    assert_source_binding(declaration, g_definition, ZR_CANONICAL_TYPE_UNION);
    TEST_ASSERT_FALSE(ZrParser_TypeEnvironment_LookupType(
            g_compiler.typeEnv, declaration_identifier(declaration)->name));
    TEST_ASSERT_NULL(find_compiler_type_prototype_inference(
            &g_compiler, declaration_identifier(declaration)->name));
}

static void assert_union_pair(SZrSyntaxContext first, SZrSyntaxContext second) {
    SZrAstNodeArray *declarations = parse_union_pair();

    declaration_identifier(declarations->nodes[0])->syntaxContext = first;
    declaration_identifier(declarations->nodes[1])->syntaxContext = second;
    ZrParser_Compiler_CompileUnionDeclaration(&g_compiler, declarations->nodes[0]);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    ZrParser_Compiler_CompileUnionDeclaration(&g_compiler, declarations->nodes[1]);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_UINT(2U, g_compiler.typePrototypes.length);
    assert_source_binding(declarations->nodes[0], first, ZR_CANONICAL_TYPE_UNION);
    assert_source_binding(declarations->nodes[1], second, ZR_CANONICAL_TYPE_UNION);
}

static void test_union_root_then_definition_do_not_collapse(void) {
    assert_union_pair(ZrParser_SyntaxContext_Root(), g_definition);
}

static void test_union_definition_then_root_do_not_collapse(void) {
    assert_union_pair(g_definition, ZrParser_SyntaxContext_Root());
}

static void test_extern_struct_root_compilation_prerequisite(void) {
    SZrAstNode *declaration = parse_extern_struct_pair()->nodes[0];

    extern_compiler_register_struct_prototype(&g_compiler, declaration);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_UINT(1U, g_compiler.typePrototypes.length);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_LookupType(
            g_compiler.typeEnv, declaration_identifier(declaration)->name));
}

static void test_extern_struct_definition_has_source_binding(void) {
    SZrAstNode *declaration = parse_extern_struct_pair()->nodes[0];

    declaration_identifier(declaration)->syntaxContext = g_definition;
    extern_compiler_register_struct_prototype(&g_compiler, declaration);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    assert_source_binding(declaration, g_definition, ZR_CANONICAL_TYPE_NOMINAL);
    TEST_ASSERT_FALSE(ZrParser_TypeEnvironment_LookupType(
            g_compiler.typeEnv, declaration_identifier(declaration)->name));
}

static void assert_extern_pair(SZrSyntaxContext first, SZrSyntaxContext second) {
    SZrAstNodeArray *declarations = parse_extern_struct_pair();

    declaration_identifier(declarations->nodes[0])->syntaxContext = first;
    declaration_identifier(declarations->nodes[1])->syntaxContext = second;
    extern_compiler_register_struct_prototype(&g_compiler, declarations->nodes[0]);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    extern_compiler_register_struct_prototype(&g_compiler, declarations->nodes[1]);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_UINT(2U, g_compiler.typePrototypes.length);
    assert_source_binding(declarations->nodes[0], first, ZR_CANONICAL_TYPE_NOMINAL);
    assert_source_binding(declarations->nodes[1], second, ZR_CANONICAL_TYPE_NOMINAL);
}

static void test_extern_root_then_definition_do_not_collapse(void) {
    assert_extern_pair(ZrParser_SyntaxContext_Root(), g_definition);
}

static void test_extern_definition_then_root_do_not_collapse(void) {
    assert_extern_pair(g_definition, ZrParser_SyntaxContext_Root());
}

static void test_publisher_preserves_raw_mixed_site_context(void) {
    SZrAstNode *declaration = parse_struct_pair()->nodes[0];
    SZrSyntaxContext mixed = ZrParser_SyntaxContext_MixedSite(g_table, g_expansionId);
    STestPublication publication = prepare_publication(declaration, mixed);
    const SZrSemanticSymbolRecord *symbol;

    TEST_ASSERT_TRUE(publish(declaration, publication));
    symbol = ZrParser_Semantic_FindSymbolById(g_compiler.semanticContext, publication.symbolId);
    TEST_ASSERT_NOT_NULL(symbol);
    TEST_ASSERT_EQUAL_PTR(g_table, symbol->syntaxContext.table);
    TEST_ASSERT_EQUAL_UINT32(mixed.markId, symbol->syntaxContext.markId);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Equal(
            ZrParser_SyntaxContext_Normalize(symbol->syntaxContext, ZR_SYNTAX_NAMESPACE_ITEM),
            ZrParser_SyntaxContext_Root()));
}

static void test_publisher_mixed_site_collides_with_root_item_identity(void) {
    SZrAstNodeArray *declarations = parse_struct_pair();
    SZrSyntaxContext mixed = ZrParser_SyntaxContext_MixedSite(g_table, g_expansionId);
    STestPublication first = prepare_publication(declarations->nodes[0], mixed);
    STestPublication second = prepare_publication(
            declarations->nodes[1], ZrParser_SyntaxContext_Root());

    TEST_ASSERT_TRUE(publish(declarations->nodes[0], first));
    TEST_ASSERT_FALSE(publish(declarations->nodes[1], second));
    TEST_ASSERT_EQUAL_UINT(1U, g_compiler.semanticContext->symbols.length);
    TEST_ASSERT_EQUAL_UINT(1U, g_compiler.semanticContext->types.length);
}

static void test_publisher_retries_after_normalization_budget_restoration(void) {
    SZrAstNode *declaration = parse_struct_pair()->nodes[0];
    STestPublication publication = prepare_publication(declaration, g_definition);
    SZrParserComptimeBudget *budget = ZrParser_SyntaxContext_Budget(g_table);
    TZrUInt64 limit;

    TEST_ASSERT_NOT_NULL(budget);
    limit = budget->limits.heapBytes;
    budget->limits.heapBytes = budget->usage.heapBytes;
    TEST_ASSERT_FALSE(publish(declaration, publication));
    TEST_ASSERT_EQUAL_UINT(0U, g_compiler.semanticContext->types.length);
    TEST_ASSERT_EQUAL_UINT(0U, g_compiler.semanticContext->symbols.length);
    budget->limits.heapBytes = limit;
    TEST_ASSERT_TRUE(publish(declaration, publication));
    TEST_ASSERT_EQUAL_UINT(1U, g_compiler.semanticContext->types.length);
    TEST_ASSERT_EQUAL_UINT(1U, g_compiler.semanticContext->symbols.length);
    TEST_ASSERT_EQUAL_UINT32(publication.symbolId,
            find_type_record(publication.typeId)->inferredType.nominalDefinitionToken);
}

typedef void (*TTestDeclarationProducer)(SZrCompilerState *, SZrAstNode *);

static void assert_producer_reentry(SZrAstNode *declaration,
        TTestDeclarationProducer producer) {
    TZrSize symbols;
    TZrSize types;
    TZrSize definitions;

    declaration_identifier(declaration)->syntaxContext = g_definition;
    producer(&g_compiler, declaration);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    symbols = g_compiler.semanticContext->symbols.length;
    types = g_compiler.semanticContext->types.length;
    definitions = g_compiler.semanticContext->canonicalTypeDefinitions.length;
    producer(&g_compiler, declaration);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_UINT(1U, g_compiler.typePrototypes.length);
    TEST_ASSERT_EQUAL_UINT(symbols, g_compiler.semanticContext->symbols.length);
    TEST_ASSERT_EQUAL_UINT(types, g_compiler.semanticContext->types.length);
    TEST_ASSERT_EQUAL_UINT(definitions, g_compiler.semanticContext->canonicalTypeDefinitions.length);
    TEST_ASSERT_EQUAL_UINT(1U, g_compiler.typeEnv->namedTypeBindings.length);
    TEST_ASSERT_EQUAL_UINT(1U, g_compiler.compileTimeTypeEnv->namedTypeBindings.length);
}

static void test_union_same_declaration_reentry_is_idempotent(void) {
    assert_producer_reentry(parse_union_pair()->nodes[0],
            ZrParser_Compiler_CompileUnionDeclaration);
}

static void test_extern_same_declaration_reentry_is_idempotent(void) {
    assert_producer_reentry(parse_extern_struct_pair()->nodes[0],
            extern_compiler_register_struct_prototype);
}

static void assert_producer_duplicate(SZrAstNodeArray *declarations,
        TTestDeclarationProducer producer) {
    TZrSize symbols;
    TZrSize types;

    declaration_identifier(declarations->nodes[0])->syntaxContext = g_definition;
    declaration_identifier(declarations->nodes[1])->syntaxContext = g_definition;
    producer(&g_compiler, declarations->nodes[0]);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    symbols = g_compiler.semanticContext->symbols.length;
    types = g_compiler.semanticContext->types.length;
    producer(&g_compiler, declarations->nodes[1]);
    TEST_ASSERT_TRUE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_UINT(1U, g_compiler.typePrototypes.length);
    TEST_ASSERT_EQUAL_UINT(symbols, g_compiler.semanticContext->symbols.length);
    TEST_ASSERT_EQUAL_UINT(types, g_compiler.semanticContext->types.length);
}

static void test_union_same_item_context_duplicate_is_rejected(void) {
    assert_producer_duplicate(parse_union_pair(), ZrParser_Compiler_CompileUnionDeclaration);
}

static void test_extern_same_item_context_duplicate_is_rejected(void) {
    assert_producer_duplicate(parse_extern_struct_pair(), extern_compiler_register_struct_prototype);
}

static void assert_failed_producer_has_no_publication(SZrAstNode *declaration,
        TTestDeclarationProducer producer, TZrBool exhaustBudget) {
    SZrParserComptimeBudget *budget = ZrParser_SyntaxContext_Budget(g_table);

    TEST_ASSERT_NOT_NULL(budget);
    if (exhaustBudget) {
        declaration_identifier(declaration)->syntaxContext = g_definition;
        budget->limits.heapBytes = budget->usage.heapBytes;
    } else {
        declaration_identifier(declaration)->syntaxContext =
                (SZrSyntaxContext){g_table, UINT32_MAX};
    }
    producer(&g_compiler, declaration);
    TEST_ASSERT_TRUE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_UINT(0U, g_compiler.typePrototypes.length);
    TEST_ASSERT_EQUAL_UINT(0U, g_compiler.typeEnv->namedTypeBindings.length);
    TEST_ASSERT_EQUAL_UINT(0U, g_compiler.compileTimeTypeEnv->namedTypeBindings.length);
    TEST_ASSERT_EQUAL_UINT(0U, g_compiler.semanticContext->symbols.length);
    TEST_ASSERT_EQUAL_UINT(0U, g_compiler.semanticContext->types.length);
    TEST_ASSERT_EQUAL_UINT(0U, g_compiler.semanticContext->canonicalTypeDefinitions.length);
    if (exhaustBudget) {
        TEST_ASSERT_EQUAL_INT(ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES, budget->exceededResource);
    }
}

static void test_union_invalid_context_blocks_publication(void) {
    assert_failed_producer_has_no_publication(parse_union_pair()->nodes[0],
            ZrParser_Compiler_CompileUnionDeclaration, ZR_FALSE);
}

static void test_union_budget_failure_blocks_publication(void) {
    assert_failed_producer_has_no_publication(parse_union_pair()->nodes[0],
            ZrParser_Compiler_CompileUnionDeclaration, ZR_TRUE);
}

static void test_extern_invalid_context_blocks_publication(void) {
    TZrUInt64 allocatedBytes;
    SZrAstNode *declaration = parse_extern_struct_pair()->nodes[0];

    declaration_identifier(declaration)->syntaxContext =
            ZrParser_SyntaxContext_Invalid(g_table);
    warm_compiler_error_storage();
    allocatedBytes = g_state->global->allocatedBytes;
    extern_compiler_register_struct_prototype(&g_compiler, declaration);
    TEST_ASSERT_TRUE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_UINT(0U, g_compiler.typePrototypes.length);
    TEST_ASSERT_EQUAL_UINT(0U, g_compiler.typeEnv->namedTypeBindings.length);
    TEST_ASSERT_EQUAL_UINT(0U, g_compiler.compileTimeTypeEnv->namedTypeBindings.length);
    TEST_ASSERT_EQUAL_UINT64(allocatedBytes, g_state->global->allocatedBytes);
}

static void test_extern_enum_invalid_context_releases_unpublished_info(void) {
    SZrAstNodeArray *statements = parse_source(
            "native extern(\"fixture\") { enum Parcel { First; } }");
    SZrAstNode *declaration;
    TZrUInt64 allocatedBytes;

    TEST_ASSERT_EQUAL_UINT(1U, statements->count);
    TEST_ASSERT_EQUAL_INT(ZR_AST_EXTERN_BLOCK, statements->nodes[0]->type);
    declaration = statements->nodes[0]->data.externBlock.declarations->nodes[0];
    TEST_ASSERT_EQUAL_INT(ZR_AST_ENUM_DECLARATION, declaration->type);
    declaration_identifier(declaration)->syntaxContext =
            ZrParser_SyntaxContext_Invalid(g_table);
    warm_compiler_error_storage();
    allocatedBytes = g_state->global->allocatedBytes;
    extern_compiler_register_enum_prototype(&g_compiler, declaration);
    TEST_ASSERT_TRUE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_UINT(0U, g_compiler.typePrototypes.length);
    TEST_ASSERT_EQUAL_UINT64(allocatedBytes, g_state->global->allocatedBytes);
}

static void test_source_enum_invalid_context_releases_unpublished_info(void) {
    SZrAstNodeArray *statements = parse_source("enum Parcel { First; }");
    SZrAstNode *declaration;
    TZrUInt64 allocatedBytes;

    TEST_ASSERT_EQUAL_UINT(1U, statements->count);
    declaration = statements->nodes[0];
    TEST_ASSERT_EQUAL_INT(ZR_AST_ENUM_DECLARATION, declaration->type);
    declaration_identifier(declaration)->syntaxContext =
            ZrParser_SyntaxContext_Invalid(g_table);
    warm_compiler_error_storage();
    allocatedBytes = g_state->global->allocatedBytes;
    compile_enum_declaration(&g_compiler, declaration);
    TEST_ASSERT_TRUE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_UINT(0U, g_compiler.typePrototypes.length);
    TEST_ASSERT_EQUAL_UINT64(allocatedBytes, g_state->global->allocatedBytes);
}

static void test_extern_budget_failure_blocks_publication(void) {
    assert_failed_producer_has_no_publication(parse_extern_struct_pair()->nodes[0],
            extern_compiler_register_struct_prototype, ZR_TRUE);
}

static void test_mixed_site_union_remains_root_visible(void) {
    SZrAstNode *declaration = parse_union_pair()->nodes[0];
    SZrSyntaxContext mixed = ZrParser_SyntaxContext_MixedSite(g_table, g_expansionId);

    declaration_identifier(declaration)->syntaxContext = mixed;
    ZrParser_Compiler_CompileUnionDeclaration(&g_compiler, declaration);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    assert_source_binding(declaration, mixed, ZR_CANONICAL_TYPE_UNION);
    TEST_ASSERT_NOT_NULL(find_compiler_type_prototype_inference(
            &g_compiler, declaration_identifier(declaration)->name));
    TEST_ASSERT_EQUAL_UINT(1U, g_compiler.typeEnv->typeNames.length);
}

static void assert_enum_source_binding(const TZrChar *source,
        TTestDeclarationProducer producer, TZrBool isExtern) {
    SZrAstNodeArray *declarations = parse_source(source);

    if (isExtern) {
        TEST_ASSERT_EQUAL_INT(ZR_AST_EXTERN_BLOCK, declarations->nodes[0]->type);
        declarations = declarations->nodes[0]->data.externBlock.declarations;
        TEST_ASSERT_NOT_NULL(declarations);
    }
    TEST_ASSERT_EQUAL_UINT(2U, declarations->count);
    TEST_ASSERT_EQUAL_INT(ZR_AST_ENUM_DECLARATION, declarations->nodes[0]->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_ENUM_DECLARATION, declarations->nodes[1]->type);
    declaration_identifier(declarations->nodes[0])->syntaxContext = g_definition;
    producer(&g_compiler, declarations->nodes[0]);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    producer(&g_compiler, declarations->nodes[1]);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_UINT(2U, g_compiler.typePrototypes.length);
    assert_source_binding(declarations->nodes[0], g_definition, ZR_CANONICAL_TYPE_NOMINAL);
    assert_source_binding(declarations->nodes[1], ZrParser_SyntaxContext_Root(),
            ZR_CANONICAL_TYPE_NOMINAL);
}

static void test_extern_enum_contexts_keep_distinct_source_bindings(void) {
    assert_enum_source_binding(
            "native extern(\"fixture\") { enum Parcel { First; } enum Parcel { Second; } }",
            extern_compiler_register_enum_prototype, ZR_TRUE);
}

static void test_source_enum_contexts_keep_distinct_source_bindings(void) {
    assert_enum_source_binding("enum Parcel { First; } enum Parcel { Second; }",
            compile_enum_declaration, ZR_FALSE);
}

static void test_marked_union_annotation_keeps_projected_identity(void) {
    SZrAstNodeArray *statements = parse_source(
            "union Parcel { Empty; } var holder: Parcel;");
    SZrType *annotation;
    const SZrNamedTypeBinding *binding;
    const SZrSemanticSymbolRecord *symbol;

    TEST_ASSERT_EQUAL_UINT(2U, statements->count);
    declaration_identifier(statements->nodes[0])->syntaxContext = g_definition;
    annotation = statements->nodes[1]->data.variableDeclaration.typeInfo;
    TEST_ASSERT_NOT_NULL(annotation);
    TEST_ASSERT_NOT_NULL(annotation->name);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, annotation->name->type);
    annotation->name->data.identifier.syntaxContext = g_definition;
    ZrParser_Compiler_CompileUnionDeclaration(&g_compiler, statements->nodes[0]);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(&g_compiler, annotation, &g_inferred));
    binding = ZrParser_TypeEnvironment_FindTypeBindingWithContext(g_compiler.typeEnv,
            declaration_identifier(statements->nodes[0])->name, g_definition);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_UINT32(binding->symbolId, g_inferred.nominalDefinitionToken);
    symbol = ZrParser_Semantic_FindSymbolById(g_compiler.semanticContext, binding->symbolId);
    TEST_ASSERT_NOT_NULL(symbol);
    TEST_ASSERT_EQUAL_UINT32(symbol->typeId,
            ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, &g_inferred));
}

static void test_forward_marked_union_annotation_adopts_provisional_identity(void) {
    SZrAstNodeArray *statements = parse_source(
            "union Parcel { Empty; Value(value: int); }"
            "struct Holder { var item: Parcel; }");
    SZrAstNode *unionDeclaration;
    SZrAstNode *structDeclaration;
    SZrType *annotation;
    SZrInferredType inferred;
    const SZrNamedTypeBinding *provisionalBinding;
    const SZrNamedTypeBinding *publishedBinding;
    const SZrSemanticSymbolRecord *publishedSymbol;
    const SZrCanonicalTypeNode *publishedType;
    TZrSymbolId provisionalSymbolId;
    TZrTypeId provisionalDefinitionTypeId;

    TEST_ASSERT_EQUAL_UINT(2U, statements->count);
    unionDeclaration = statements->nodes[0];
    structDeclaration = statements->nodes[1];
    TEST_ASSERT_EQUAL_INT(ZR_AST_UNION_DECLARATION, unionDeclaration->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_DECLARATION, structDeclaration->type);
    declaration_identifier(unionDeclaration)->syntaxContext = g_definition;
    annotation = structDeclaration->data.structDeclaration.members->nodes[0]
            ->data.structField.typeInfo;
    TEST_ASSERT_NOT_NULL(annotation);
    TEST_ASSERT_NOT_NULL(annotation->name);
    annotation->name->data.identifier.syntaxContext = g_definition;

    ZrParser_InferredType_Init(g_state, &inferred, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, annotation, &inferred));
    provisionalBinding = ZrParser_TypeEnvironment_FindTypeBindingWithContext(
            g_compiler.typeEnv,
            declaration_identifier(unionDeclaration)->name,
            g_definition);
    TEST_ASSERT_NOT_NULL(provisionalBinding);
    provisionalSymbolId = provisionalBinding->symbolId;
    provisionalDefinitionTypeId = provisionalBinding->typeId;
    TEST_ASSERT_EQUAL_UINT32(provisionalSymbolId, inferred.nominalDefinitionToken);
    TEST_ASSERT_EQUAL_UINT32(1U, g_compiler.typeEnv->namedTypeBindings.length);

    ZrParser_Compiler_CompileUnionDeclaration(&g_compiler, unionDeclaration);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_UINT(1U, g_compiler.typePrototypes.length);
    publishedBinding = ZrParser_TypeEnvironment_FindTypeBindingWithContext(
            g_compiler.typeEnv,
            declaration_identifier(unionDeclaration)->name,
            g_definition);
    TEST_ASSERT_NOT_NULL(publishedBinding);
    TEST_ASSERT_EQUAL_UINT32(provisionalSymbolId, publishedBinding->symbolId);
    TEST_ASSERT_EQUAL_UINT32(provisionalDefinitionTypeId, publishedBinding->typeId);
    publishedSymbol = ZrParser_Semantic_FindSymbolById(
            g_compiler.semanticContext, provisionalSymbolId);
    TEST_ASSERT_NOT_NULL(publishedSymbol);
    publishedType = ZrParser_CanonicalType_Find(
            g_compiler.semanticContext, publishedSymbol->typeId);
    TEST_ASSERT_NOT_NULL(publishedType);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_UNION, publishedType->kind);
    TEST_ASSERT_EQUAL_UINT32(
            provisionalDefinitionTypeId,
            publishedType->data.unionType.definitionTypeId);
    TEST_ASSERT_EQUAL_UINT32(
            publishedSymbol->typeId,
            ZrParser_CanonicalType_FromInferred(
                    g_compiler.semanticContext, &inferred));
    TEST_ASSERT_EQUAL_UINT(1U, g_compiler.compileTimeTypeEnv->namedTypeBindings.length);
    ZrParser_InferredType_Free(g_state, &inferred);
}

static void test_union_distinct_definition_sites_survive_table_growth(void) {
    const TZrChar *declarationText = "union Parcel { Empty; }";
    TZrChar source[1024] = {0};
    SZrAstNodeArray *declarations;

    for (TZrSize index = 0U; index < 20U; index++) {
        strcat(source, declarationText);
    }
    declarations = parse_source(source);
    TEST_ASSERT_EQUAL_UINT(20U, declarations->count);
    for (TZrSize index = 0U; index < declarations->count; index++) {
        SZrSyntaxExpansionInfo expansion = {0};
        TZrSyntaxExpansionId expansionId;
        SZrSyntaxContext context;

        expansion.stableIdentity = 0xB200U + index;
        expansion.callSiteSpan = ZrParser_TokenSpan_None();
        expansion.definitionSiteSpan = ZrParser_TokenSpan_None();
        TEST_ASSERT_TRUE(ZrParser_SyntaxContextTable_RegisterExpansion(
                g_table, &expansion, &expansionId));
        context = ZrParser_SyntaxContext_DefSite(g_table, expansionId);
        TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(context));
        declaration_identifier(declarations->nodes[index])->syntaxContext = context;
        ZrParser_Compiler_CompileUnionDeclaration(&g_compiler, declarations->nodes[index]);
        TEST_ASSERT_FALSE(g_compiler.hasError);
    }
    TEST_ASSERT_EQUAL_UINT(20U, g_compiler.typePrototypes.length);
    TEST_ASSERT_EQUAL_UINT(20U, g_compiler.typeEnv->namedTypeBindings.length);
    TEST_ASSERT_EQUAL_UINT(20U, g_compiler.compileTimeTypeEnv->namedTypeBindings.length);
    TEST_ASSERT_EQUAL_UINT(20U, g_compiler.semanticContext->symbols.length);
    TEST_ASSERT_EQUAL_UINT(20U, g_compiler.semanticContext->types.length);
    TEST_ASSERT_EQUAL_UINT(0U, g_compiler.typeEnv->typeNames.length);
    for (TZrSize index = 0U; index < declarations->count; index++) {
        assert_source_binding(declarations->nodes[index],
                declaration_identifier(declarations->nodes[index])->syntaxContext,
                ZR_CANONICAL_TYPE_UNION);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_canonical_publisher_root_prerequisite);
    RUN_TEST(test_canonical_publisher_preserves_definition_context);
    RUN_TEST(test_canonical_publisher_union_record_preserves_nominal_token);
    RUN_TEST(test_canonical_publisher_allows_distinct_item_contexts);
    RUN_TEST(test_canonical_publisher_rejects_same_item_context);
    RUN_TEST(test_canonical_publisher_rejects_invalid_context_without_publication);
    RUN_TEST(test_canonical_publisher_rejects_normalization_budget_failure);
    RUN_TEST(test_canonical_publisher_unwraps_generic_union_definition);
    RUN_TEST(test_root_type_binding_adopts_published_union_identity);
    RUN_TEST(test_definition_type_binding_adopts_published_union_identity);
    RUN_TEST(test_canonical_publisher_retains_context_owner);
    RUN_TEST(test_union_root_compilation_prerequisite);
    RUN_TEST(test_union_definition_publication_preserves_structure_and_identity);
    RUN_TEST(test_union_root_then_definition_do_not_collapse);
    RUN_TEST(test_union_definition_then_root_do_not_collapse);
    RUN_TEST(test_extern_struct_root_compilation_prerequisite);
    RUN_TEST(test_extern_struct_definition_has_source_binding);
    RUN_TEST(test_extern_root_then_definition_do_not_collapse);
    RUN_TEST(test_extern_definition_then_root_do_not_collapse);
    RUN_TEST(test_publisher_preserves_raw_mixed_site_context);
    RUN_TEST(test_publisher_mixed_site_collides_with_root_item_identity);
    RUN_TEST(test_publisher_retries_after_normalization_budget_restoration);
    RUN_TEST(test_union_same_declaration_reentry_is_idempotent);
    RUN_TEST(test_extern_same_declaration_reentry_is_idempotent);
    RUN_TEST(test_union_same_item_context_duplicate_is_rejected);
    RUN_TEST(test_extern_same_item_context_duplicate_is_rejected);
    RUN_TEST(test_union_invalid_context_blocks_publication);
    RUN_TEST(test_union_budget_failure_blocks_publication);
    RUN_TEST(test_extern_invalid_context_blocks_publication);
    RUN_TEST(test_extern_enum_invalid_context_releases_unpublished_info);
    RUN_TEST(test_source_enum_invalid_context_releases_unpublished_info);
    RUN_TEST(test_extern_budget_failure_blocks_publication);
    RUN_TEST(test_mixed_site_union_remains_root_visible);
    RUN_TEST(test_extern_enum_contexts_keep_distinct_source_bindings);
    RUN_TEST(test_source_enum_contexts_keep_distinct_source_bindings);
    RUN_TEST(test_marked_union_annotation_keeps_projected_identity);
    RUN_TEST(test_forward_marked_union_annotation_adopts_provisional_identity);
    RUN_TEST(test_union_distinct_definition_sites_survive_table_growth);
    return UNITY_END();
}
