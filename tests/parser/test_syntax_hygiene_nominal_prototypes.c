#include "unity.h"

#include <stdint.h>
#include <string.h>

#include "compiler/compiler_top_level_duplicate.h"
#include "compiler/compiler_type_declaration.h"
#include "harness/runtime_support.h"
#include "syntax/syntax_context_internal.h"
#include "type_inference/type_inference_internal.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/comptime_contract.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/syntax_context.h"

static SZrState *g_state;
static SZrCompilerState g_compiler;
static TZrBool g_compilerInitialized;
static SZrAstNode *g_ast;
static SZrSyntaxContextTable *g_contextTable;
static SZrSyntaxContext g_defSite;
static SZrSyntaxContext g_mixedSite;
static SZrInferredType g_type;
static SZrTypePrototypeInfo g_currentPrototype;

void setUp(void) {
    SZrSyntaxExpansionInfo expansion = {0};
    TZrSyntaxExpansionId expansionId;

    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
    ZrParser_InferredType_Init(g_state, &g_type, ZR_VALUE_TYPE_OBJECT);
    memset(&g_currentPrototype, 0, sizeof(g_currentPrototype));
    g_contextTable = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_contextTable);
    expansion.stableIdentity = 0xA115U;
    expansion.callSiteSpan = ZrParser_TokenSpan_None();
    expansion.definitionSiteSpan = ZrParser_TokenSpan_None();
    TEST_ASSERT_TRUE(ZrParser_SyntaxContextTable_RegisterExpansion(
            g_contextTable, &expansion, &expansionId));
    g_defSite = ZrParser_SyntaxContext_DefSite(g_contextTable, expansionId);
    g_mixedSite = ZrParser_SyntaxContext_MixedSite(g_contextTable, expansionId);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(g_defSite));
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(g_mixedSite));
    ZrParser_CompilerState_Init(&g_compiler, g_state);
    g_compilerInitialized = ZR_TRUE;
    TEST_ASSERT_NOT_NULL(g_compiler.typeEnv);
    TEST_ASSERT_NOT_NULL(g_compiler.semanticContext);
}

void tearDown(void) {
    ZrParser_InferredType_Free(g_state, &g_type);
    if (g_compilerInitialized) {
        g_compiler.currentTypePrototypeInfo = ZR_NULL;
        ZrParser_CompilerState_Free(&g_compiler);
        g_compilerInitialized = ZR_FALSE;
    }
    if (g_ast != ZR_NULL) {
        ZrParser_Ast_Free(g_state, g_ast);
        g_ast = ZR_NULL;
    }
    if (g_contextTable != ZR_NULL) {
        ZrParser_SyntaxContextTable_Release(g_contextTable);
        g_contextTable = ZR_NULL;
    }
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

static SZrAstNodeArray *parse_declarations(void) {
    const TZrChar *source = "struct Widget { var value: int; }"
                           "struct Widget { var text: string; }";
    SZrString *sourceName = ZrCore_String_CreateFromNative(g_state, "nominal_prototypes.zr");
    SZrAstNodeArray *statements;

    TEST_ASSERT_NOT_NULL(sourceName);
    g_ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(g_ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_ast->type);
    statements = g_ast->data.script.statements;
    TEST_ASSERT_NOT_NULL(statements);
    TEST_ASSERT_EQUAL_UINT(2U, statements->count);
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_DECLARATION, statements->nodes[0]->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_DECLARATION, statements->nodes[1]->type);
    g_compiler.scriptAst = g_ast;
    return statements;
}

static SZrString *declaration_name(SZrAstNode *declaration) {
    return declaration->data.structDeclaration.name->name;
}

static void register_declaration(SZrAstNode *declaration, SZrSyntaxContext context) {
    declaration->data.structDeclaration.name->syntaxContext = context;
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterTypeDeclaration(
            g_state, g_compiler.typeEnv, declaration_name(declaration), declaration));
}

static void init_prototype(SZrTypePrototypeInfo *prototype, SZrAstNode *declaration,
        TZrUInt64 protocolMask) {
    memset(prototype, 0, sizeof(*prototype));
    prototype->name = declaration_name(declaration);
    prototype->declarationNode = declaration;
    prototype->type = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT;
    prototype->protocolMask = protocolMask;
}

static void publish_prototype(SZrAstNode *declaration, TZrUInt64 protocolMask) {
    SZrTypePrototypeInfo prototype;

    init_prototype(&prototype, declaration, protocolMask);
    ZrCore_Array_Push(g_state, &g_compiler.typePrototypes, &prototype);
    TEST_ASSERT_NOT_NULL(ZrCore_Array_Get(&g_compiler.typePrototypes,
            g_compiler.typePrototypes.length - 1U));
}

static void publish_metadata_stub(SZrString *name, TZrUInt64 protocolMask) {
    SZrTypePrototypeInfo prototype = {0};

    prototype.name = name;
    prototype.type = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT;
    prototype.isImportedNative = ZR_TRUE;
    prototype.protocolMask = protocolMask;
    ZrCore_Array_Push(g_state, &g_compiler.typePrototypes, &prototype);
}

static void copy_declaration_type(SZrAstNode *declaration, SZrSyntaxContext context) {
    const SZrNamedTypeBinding *binding = ZrParser_TypeEnvironment_FindTypeBindingWithContext(
            g_compiler.typeEnv, declaration_name(declaration), context);

    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_PTR(declaration, binding->declarationNode);
    for (TZrSize index = 0U; index < g_compiler.semanticContext->types.length; index++) {
        const SZrSemanticTypeRecord *record =
                ZrCore_Array_Get(&g_compiler.semanticContext->types, index);
        if (record != ZR_NULL && record->id == binding->typeId) {
            ZrParser_InferredType_Copy(g_state, &g_type, &record->inferredType);
            if (!ZrParser_SyntaxContext_Equal(
                    ZrParser_SyntaxContext_Normalize(context, ZR_SYNTAX_NAMESPACE_ITEM),
                    ZrParser_SyntaxContext_Root())) {
                TEST_ASSERT_NOT_EQUAL(0U, g_type.nominalDefinitionToken);
            }
            return;
        }
    }
    TEST_FAIL_MESSAGE("Registered nominal declaration must have a semantic type record");
}

static SZrAstNodeArray *register_distinct_declarations(void) {
    SZrAstNodeArray *declarations = parse_declarations();

    register_declaration(declarations->nodes[0], g_defSite);
    register_declaration(declarations->nodes[1], ZrParser_SyntaxContext_Root());
    return declarations;
}

static void test_root_prototype_lookup_prerequisite(void) {
    SZrAstNode *root = parse_declarations()->nodes[0];
    SZrTypePrototypeInfo *prototype;

    register_declaration(root, ZrParser_SyntaxContext_Root());
    publish_prototype(root, ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE));
    prototype = find_compiler_type_prototype_inference(&g_compiler, declaration_name(root));
    TEST_ASSERT_NOT_NULL(prototype);
    TEST_ASSERT_EQUAL_PTR(root, prototype->declarationNode);
    copy_declaration_type(root, ZrParser_SyntaxContext_Root());
    TEST_ASSERT_TRUE(inferred_type_implements_protocol_mask(
            &g_compiler, &g_type, ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE)));
}

static void test_root_lookup_rejects_definition_only_prototype(void) {
    SZrAstNode *definition = parse_declarations()->nodes[0];

    register_declaration(definition, g_defSite);
    publish_prototype(definition, 0U);
    TEST_ASSERT_NULL(find_compiler_type_prototype_inference(
            &g_compiler, declaration_name(definition)));
}

static void test_root_lookup_skips_definition_registered_first(void) {
    SZrAstNodeArray *declarations = register_distinct_declarations();
    SZrTypePrototypeInfo *prototype;

    publish_prototype(declarations->nodes[0], 0U);
    publish_prototype(declarations->nodes[1], 0U);
    prototype = find_compiler_type_prototype_inference(
            &g_compiler, declaration_name(declarations->nodes[1]));
    TEST_ASSERT_NOT_NULL(prototype);
    TEST_ASSERT_EQUAL_PTR(declarations->nodes[1], prototype->declarationNode);
}

static void test_duplicate_guard_allows_root_after_definition(void) {
    SZrAstNodeArray *declarations = register_distinct_declarations();

    publish_prototype(declarations->nodes[0], 0U);
    TEST_ASSERT_FALSE(compiler_report_duplicate_top_level_type(
            &g_compiler, declarations->nodes[1]));
    TEST_ASSERT_FALSE(g_compiler.hasError);
}

static void test_duplicate_guard_allows_definition_after_root(void) {
    SZrAstNodeArray *declarations = register_distinct_declarations();

    publish_prototype(declarations->nodes[1], 0U);
    TEST_ASSERT_FALSE(compiler_report_duplicate_top_level_type(
            &g_compiler, declarations->nodes[0]));
    TEST_ASSERT_FALSE(g_compiler.hasError);
}

static void test_duplicate_guard_rejects_same_context(void) {
    SZrAstNodeArray *declarations = parse_declarations();

    register_declaration(declarations->nodes[0], g_defSite);
    declarations->nodes[1]->data.structDeclaration.name->syntaxContext = g_defSite;
    publish_prototype(declarations->nodes[0], 0U);
    TEST_ASSERT_TRUE(compiler_report_duplicate_top_level_type(
            &g_compiler, declarations->nodes[1]));
    TEST_ASSERT_TRUE(g_compiler.hasError);
}

static void test_duplicate_guard_skips_repeated_declaration(void) {
    SZrAstNode *declaration = parse_declarations()->nodes[0];

    register_declaration(declaration, g_defSite);
    publish_prototype(declaration, 0U);
    TEST_ASSERT_FALSE(compiler_report_duplicate_top_level_type(&g_compiler, declaration));
}

static void test_duplicate_guard_normalizes_mixed_site_items(void) {
    SZrAstNodeArray *declarations = parse_declarations();

    register_declaration(declarations->nodes[0], g_mixedSite);
    publish_prototype(declarations->nodes[0], 0U);
    TEST_ASSERT_TRUE(compiler_report_duplicate_top_level_type(
            &g_compiler, declarations->nodes[1]));
}

static void test_definition_protocol_selects_own_declaration(void) {
    SZrAstNodeArray *declarations = register_distinct_declarations();

    publish_prototype(declarations->nodes[1], ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE));
    publish_prototype(declarations->nodes[0], ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_REF_LIKE));
    copy_declaration_type(declarations->nodes[0], g_defSite);
    TEST_ASSERT_TRUE(inferred_type_implements_protocol_mask(
            &g_compiler, &g_type, ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_REF_LIKE)));
}

static void test_definition_protocol_rejects_root_protocol(void) {
    SZrAstNodeArray *declarations = register_distinct_declarations();

    publish_prototype(declarations->nodes[1], ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE));
    publish_prototype(declarations->nodes[0], ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_REF_LIKE));
    copy_declaration_type(declarations->nodes[0], g_defSite);
    TEST_ASSERT_FALSE(inferred_type_implements_protocol_mask(
            &g_compiler, &g_type, ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE)));
}

static void test_root_protocol_selects_own_declaration(void) {
    SZrAstNodeArray *declarations = register_distinct_declarations();

    publish_prototype(declarations->nodes[0], ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_REF_LIKE));
    publish_prototype(declarations->nodes[1], ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE));
    copy_declaration_type(declarations->nodes[1], ZrParser_SyntaxContext_Root());
    TEST_ASSERT_TRUE(inferred_type_implements_protocol_mask(
            &g_compiler, &g_type, ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE)));
}

static void test_root_protocol_rejects_definition_protocol(void) {
    SZrAstNodeArray *declarations = register_distinct_declarations();

    publish_prototype(declarations->nodes[0], ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_REF_LIKE));
    publish_prototype(declarations->nodes[1], ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE));
    copy_declaration_type(declarations->nodes[1], ZrParser_SyntaxContext_Root());
    TEST_ASSERT_FALSE(inferred_type_implements_protocol_mask(
            &g_compiler, &g_type, ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_REF_LIKE)));
}

static void test_current_definition_prototype_is_not_root_visible(void) {
    SZrAstNode *definition = parse_declarations()->nodes[0];

    register_declaration(definition, g_defSite);
    init_prototype(&g_currentPrototype, definition, 0U);
    g_compiler.currentTypePrototypeInfo = &g_currentPrototype;
    TEST_ASSERT_NULL(find_compiler_type_prototype_inference(
            &g_compiler, declaration_name(definition)));
}

static void test_current_definition_protocol_preserves_identity(void) {
    SZrAstNodeArray *declarations = register_distinct_declarations();

    publish_prototype(declarations->nodes[1], ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE));
    init_prototype(&g_currentPrototype, declarations->nodes[0],
            ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_REF_LIKE));
    g_compiler.currentTypePrototypeInfo = &g_currentPrototype;
    copy_declaration_type(declarations->nodes[0], g_defSite);
    TEST_ASSERT_TRUE(inferred_type_implements_protocol_mask(
            &g_compiler, &g_type, ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_REF_LIKE)));
}

static void test_imported_metadata_stub_keeps_ordinary_lookup(void) {
    SZrAstNode *declaration = parse_declarations()->nodes[0];
    SZrTypePrototypeInfo *prototype;

    publish_metadata_stub(declaration_name(declaration), ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE));
    prototype = find_compiler_type_prototype_inference(&g_compiler, declaration_name(declaration));
    TEST_ASSERT_NOT_NULL(prototype);
    TEST_ASSERT_TRUE(prototype->isImportedNative);
    TEST_ASSERT_NULL(prototype->declarationNode);
    g_type.typeName = declaration_name(declaration);
    TEST_ASSERT_TRUE(inferred_type_implements_protocol_mask(
            &g_compiler, &g_type, ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE)));
}

static void test_metadata_stub_cannot_capture_marked_definition(void) {
    SZrAstNode *definition = parse_declarations()->nodes[0];

    register_declaration(definition, g_defSite);
    publish_metadata_stub(declaration_name(definition), ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE));
    copy_declaration_type(definition, g_defSite);
    TEST_ASSERT_FALSE(inferred_type_implements_protocol_mask(
            &g_compiler, &g_type, ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE)));
}

static void test_unresolved_nominal_token_does_not_fall_back_to_root(void) {
    SZrAstNode *root = parse_declarations()->nodes[0];

    register_declaration(root, ZrParser_SyntaxContext_Root());
    publish_prototype(root, ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE));
    copy_declaration_type(root, ZrParser_SyntaxContext_Root());
    g_type.nominalDefinitionToken = UINT32_MAX;
    TEST_ASSERT_FALSE(inferred_type_implements_protocol_mask(
            &g_compiler, &g_type, ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE)));
}

static SZrParserComptimeBudget *exhaust_normalization_budget(void) {
    SZrParserComptimeBudget *budget = ZrParser_SyntaxContext_Budget(g_contextTable);

    TEST_ASSERT_NOT_NULL(budget);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(g_defSite));
    budget->limits.heapBytes = budget->usage.heapBytes;
    return budget;
}

static void test_declaration_context_rejects_failed_normalization(void) {
    SZrAstNode *definition = parse_declarations()->nodes[0];
    SZrSyntaxContext normalized;
    SZrParserComptimeBudget *budget;

    register_declaration(definition, g_defSite);
    budget = exhaust_normalization_budget();
    TEST_ASSERT_FALSE(compiler_type_declaration_item_context(definition, &normalized));
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES, budget->exceededResource);
}

static void test_duplicate_guard_blocks_incoming_normalization_failure(void) {
    SZrAstNodeArray *declarations = parse_declarations();
    SZrParserComptimeBudget *budget;

    register_declaration(declarations->nodes[0], g_defSite);
    declarations->nodes[1]->data.structDeclaration.name->syntaxContext = g_defSite;
    publish_prototype(declarations->nodes[0], 0U);
    budget = exhaust_normalization_budget();
    TEST_ASSERT_TRUE(compiler_report_duplicate_top_level_type(
            &g_compiler, declarations->nodes[1]));
    TEST_ASSERT_TRUE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES, budget->exceededResource);
}

static void test_duplicate_guard_blocks_previous_normalization_failure(void) {
    SZrAstNodeArray *declarations = register_distinct_declarations();
    SZrParserComptimeBudget *budget;

    publish_prototype(declarations->nodes[0], 0U);
    budget = exhaust_normalization_budget();
    TEST_ASSERT_TRUE(compiler_report_duplicate_top_level_type(
            &g_compiler, declarations->nodes[1]));
    TEST_ASSERT_TRUE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES, budget->exceededResource);
}

static SZrSyntaxContext second_definition_context(void) {
    SZrSyntaxExpansionInfo expansion = {0};
    TZrSyntaxExpansionId expansionId;
    SZrSyntaxContext context;

    expansion.stableIdentity = 0xA116U;
    expansion.callSiteSpan = ZrParser_TokenSpan_None();
    expansion.definitionSiteSpan = ZrParser_TokenSpan_None();
    TEST_ASSERT_TRUE(ZrParser_SyntaxContextTable_RegisterExpansion(
            g_contextTable, &expansion, &expansionId));
    context = ZrParser_SyntaxContext_DefSite(g_contextTable, expansionId);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(context));
    TEST_ASSERT_FALSE(ZrParser_SyntaxContext_Equal(context, g_defSite));
    return context;
}

static void test_duplicate_guard_allows_distinct_definition_sites(void) {
    SZrAstNodeArray *declarations = parse_declarations();
    SZrSyntaxContext secondSite = second_definition_context();

    register_declaration(declarations->nodes[0], g_defSite);
    register_declaration(declarations->nodes[1], secondSite);
    publish_prototype(declarations->nodes[0], 0U);
    TEST_ASSERT_FALSE(compiler_report_duplicate_top_level_type(
            &g_compiler, declarations->nodes[1]));
    TEST_ASSERT_FALSE(g_compiler.hasError);
}

static void test_protocol_isolated_between_definition_sites(void) {
    SZrAstNodeArray *declarations = parse_declarations();
    SZrSyntaxContext secondSite = second_definition_context();

    register_declaration(declarations->nodes[0], g_defSite);
    register_declaration(declarations->nodes[1], secondSite);
    publish_prototype(declarations->nodes[0], ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE));
    publish_prototype(declarations->nodes[1], ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_REF_LIKE));
    copy_declaration_type(declarations->nodes[1], secondSite);
    TEST_ASSERT_TRUE(inferred_type_implements_protocol_mask(
            &g_compiler, &g_type, ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_REF_LIKE)));
    TEST_ASSERT_FALSE(inferred_type_implements_protocol_mask(
            &g_compiler, &g_type, ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE)));
}

static void test_mixed_site_prototype_remains_root_visible(void) {
    SZrAstNode *declaration = parse_declarations()->nodes[0];
    SZrTypePrototypeInfo *prototype;

    register_declaration(declaration, g_mixedSite);
    publish_prototype(declaration, 0U);
    prototype = find_compiler_type_prototype_inference(&g_compiler, declaration_name(declaration));
    TEST_ASSERT_NOT_NULL(prototype);
    TEST_ASSERT_EQUAL_PTR(declaration, prototype->declarationNode);
}

static void test_mixed_site_protocol_keeps_item_root_behavior(void) {
    SZrAstNode *declaration = parse_declarations()->nodes[0];

    register_declaration(declaration, g_mixedSite);
    publish_prototype(declaration, ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE));
    copy_declaration_type(declaration, g_mixedSite);
    TEST_ASSERT_TRUE(inferred_type_implements_protocol_mask(
            &g_compiler, &g_type, ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE)));
}

static void test_marked_protocol_requires_active_environment_snapshot(void) {
    SZrAstNode *definition = parse_declarations()->nodes[0];
    SZrSemanticContext *savedContext;
    TZrBool implementsProtocol;

    register_declaration(definition, g_defSite);
    publish_prototype(definition, ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE));
    copy_declaration_type(definition, g_defSite);
    savedContext = g_compiler.typeEnv->semanticContext;
    g_compiler.typeEnv->semanticContext = ZR_NULL;
    implementsProtocol = inferred_type_implements_protocol_mask(
            &g_compiler, &g_type, ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE));
    g_compiler.typeEnv->semanticContext = savedContext;
    TEST_ASSERT_FALSE(implementsProtocol);
}

static void test_duplicate_detection_recovers_after_budget_failure(void) {
    SZrAstNodeArray *declarations = parse_declarations();
    SZrParserComptimeBudget *budget = ZrParser_SyntaxContext_Budget(g_contextTable);
    TZrUInt64 originalLimit;
    TZrUInt64 originalUsage;
    TZrBool blocked;
    TZrBool reportedError;
    SZrSyntaxContext normalized;

    register_declaration(declarations->nodes[0], g_defSite);
    declarations->nodes[1]->data.structDeclaration.name->syntaxContext = g_defSite;
    publish_prototype(declarations->nodes[0], 0U);
    TEST_ASSERT_NOT_NULL(budget);
    originalLimit = budget->limits.heapBytes;
    originalUsage = budget->usage.heapBytes;
    (void)exhaust_normalization_budget();
    blocked = compiler_report_duplicate_top_level_type(&g_compiler, declarations->nodes[1]);
    reportedError = g_compiler.hasError;
    budget->limits.heapBytes = originalLimit;
    g_compiler.hasError = ZR_FALSE;
    TEST_ASSERT_TRUE(blocked);
    TEST_ASSERT_TRUE(reportedError);
    TEST_ASSERT_EQUAL_UINT64(originalUsage, budget->usage.heapBytes);
    TEST_ASSERT_TRUE(compiler_type_declaration_item_context(
            declarations->nodes[1], &normalized));
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Equal(normalized, g_defSite));
    TEST_ASSERT_TRUE(compiler_report_duplicate_top_level_type(
            &g_compiler, declarations->nodes[1]));
    TEST_ASSERT_TRUE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_UINT(1U, g_compiler.typePrototypes.length);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_root_prototype_lookup_prerequisite);
    RUN_TEST(test_root_lookup_rejects_definition_only_prototype);
    RUN_TEST(test_root_lookup_skips_definition_registered_first);
    RUN_TEST(test_duplicate_guard_allows_root_after_definition);
    RUN_TEST(test_duplicate_guard_allows_definition_after_root);
    RUN_TEST(test_duplicate_guard_rejects_same_context);
    RUN_TEST(test_duplicate_guard_skips_repeated_declaration);
    RUN_TEST(test_duplicate_guard_normalizes_mixed_site_items);
    RUN_TEST(test_definition_protocol_selects_own_declaration);
    RUN_TEST(test_definition_protocol_rejects_root_protocol);
    RUN_TEST(test_root_protocol_selects_own_declaration);
    RUN_TEST(test_root_protocol_rejects_definition_protocol);
    RUN_TEST(test_current_definition_prototype_is_not_root_visible);
    RUN_TEST(test_current_definition_protocol_preserves_identity);
    RUN_TEST(test_imported_metadata_stub_keeps_ordinary_lookup);
    RUN_TEST(test_metadata_stub_cannot_capture_marked_definition);
    RUN_TEST(test_unresolved_nominal_token_does_not_fall_back_to_root);
    RUN_TEST(test_declaration_context_rejects_failed_normalization);
    RUN_TEST(test_duplicate_guard_blocks_incoming_normalization_failure);
    RUN_TEST(test_duplicate_guard_blocks_previous_normalization_failure);
    RUN_TEST(test_duplicate_guard_allows_distinct_definition_sites);
    RUN_TEST(test_protocol_isolated_between_definition_sites);
    RUN_TEST(test_mixed_site_prototype_remains_root_visible);
    RUN_TEST(test_mixed_site_protocol_keeps_item_root_behavior);
    RUN_TEST(test_marked_protocol_requires_active_environment_snapshot);
    RUN_TEST(test_duplicate_detection_recovers_after_budget_failure);
    return UNITY_END();
}
