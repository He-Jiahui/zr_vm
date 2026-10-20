#include "unity.h"

#include <string.h>

#include "compiler/expression/compile_expression_internal.h"
#include "compiler/compiler_internal.h"
#include "compiler/compiler_type_value_alias.h"
#include "harness/runtime_support.h"
#include "type_inference/type_inference_internal.h"
#include "type_inference/type_inference_nominal_prototype.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic.h"

static SZrState *g_state;
static SZrCompilerState g_compiler;
static TZrBool g_compilerInitialized;
static SZrAstNode *g_ast;
static SZrSyntaxContextTable *g_table;
static SZrSyntaxContext g_definition;
static SZrSyntaxContext g_mixed;
static SZrInferredType g_types[3];
static TZrSize g_hiddenPrototypeCount;

void setUp(void) {
    SZrSyntaxExpansionInfo expansion = {0};
    TZrSyntaxExpansionId expansionId;

    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
    for (TZrSize index = 0U; index < 3U; index++) {
        ZrParser_InferredType_Init(g_state, &g_types[index], ZR_VALUE_TYPE_OBJECT);
    }
    g_table = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_table);
    expansion.stableIdentity = 0xB301U;
    expansion.callSiteSpan = ZrParser_TokenSpan_None();
    expansion.definitionSiteSpan = ZrParser_TokenSpan_None();
    TEST_ASSERT_TRUE(ZrParser_SyntaxContextTable_RegisterExpansion(
            g_table, &expansion, &expansionId));
    g_definition = ZrParser_SyntaxContext_DefSite(g_table, expansionId);
    g_mixed = ZrParser_SyntaxContext_MixedSite(g_table, expansionId);
    ZrParser_CompilerState_Init(&g_compiler, g_state);
    g_compilerInitialized = ZR_TRUE;
    TEST_ASSERT_NOT_NULL(g_compiler.typeEnv);
    TEST_ASSERT_NOT_NULL(g_compiler.semanticContext);
}

void tearDown(void) {
    for (TZrSize index = 0U; index < 3U; index++) {
        ZrParser_InferredType_Free(g_state, &g_types[index]);
    }
    if (g_compilerInitialized) {
        g_compiler.currentTypePrototypeInfo = ZR_NULL;
        if (g_hiddenPrototypeCount != 0U) {
            g_compiler.typePrototypes.length = g_hiddenPrototypeCount;
            g_hiddenPrototypeCount = 0U;
        }
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
    SZrString *name = ZrCore_String_CreateFromNative(g_state, "compiler_lookup_hygiene.zr");

    TEST_ASSERT_NOT_NULL(name);
    g_ast = ZrParser_Parse(g_state, source, strlen(source), name);
    TEST_ASSERT_NOT_NULL(g_ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_ast->type);
    TEST_ASSERT_NOT_NULL(g_ast->data.script.statements);
    g_compiler.scriptAst = g_ast;
    return g_ast->data.script.statements;
}

static TZrSymbolId compile_union(SZrAstNode *node, SZrSyntaxContext context) {
    const SZrNamedTypeBinding *binding;
    const SZrCanonicalTypeNode *nominal;

    TEST_ASSERT_EQUAL_INT(ZR_AST_UNION_DECLARATION, node->type);
    node->data.unionDeclaration.name->syntaxContext = context;
    ZrParser_Compiler_CompileUnionDeclaration(&g_compiler, node);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    binding = ZrParser_TypeEnvironment_FindTypeBindingWithContext(g_compiler.typeEnv,
            node->data.unionDeclaration.name->name, context);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_PTR(node, binding->declarationNode);
    nominal = ZrParser_CanonicalType_Find(g_compiler.semanticContext, binding->typeId);
    TEST_ASSERT_NOT_NULL(nominal);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_NOMINAL, nominal->kind);
    return nominal->data.nominal.definitionToken;
}

static void assert_declaration(SZrTypePrototypeInfo *prototype, SZrAstNode *declaration) {
    TEST_ASSERT_NOT_NULL(prototype);
    TEST_ASSERT_EQUAL_PTR(declaration, prototype->declarationNode);
}

static void init_instance(SZrInferredType *type, SZrString *name,
        TZrSymbolId definitionToken, SZrInferredType *argument) {
    SZrInferredType copy;

    ZrParser_InferredType_Free(g_state, type);
    ZrParser_InferredType_InitFull(g_state, type, ZR_VALUE_TYPE_OBJECT, ZR_FALSE, name);
    type->nominalDefinitionToken = definitionToken;
    ZrCore_Array_Init(g_state, &type->elementTypes, sizeof(SZrInferredType), 1U);
    ZrParser_InferredType_Copy(g_state, &copy, argument);
    ZrCore_Array_Push(g_state, &type->elementTypes, &copy);
}

static SZrTypePrototypeInfo *materialize(SZrInferredType *type) {
    TEST_ASSERT_TRUE(ensure_generic_instance_type_prototype_from_inferred(&g_compiler, type));
    TEST_ASSERT_FALSE(g_compiler.hasError);
    return type_inference_find_nominal_prototype(&g_compiler, type);
}

static void test_root_compiler_query_prerequisite(void) {
    SZrAstNode *node = parse_source("union Widget { Item; }")->nodes[0];

    TEST_ASSERT_EQUAL_UINT32(0U, compile_union(node, ZrParser_SyntaxContext_Root()));
    assert_declaration(find_compiler_type_prototype_inference(
            &g_compiler, node->data.unionDeclaration.name->name), node);
    assert_declaration(find_compiler_type_prototype(
            &g_compiler, node->data.unionDeclaration.name->name), node);
}

static void test_compiler_root_query_excludes_opaque_declaration(void) {
    SZrAstNode *node = parse_source("union Widget { Item; }")->nodes[0];

    TEST_ASSERT_NOT_EQUAL(0U, compile_union(node, g_definition));
    TEST_ASSERT_NULL(find_compiler_type_prototype_inference(
            &g_compiler, node->data.unionDeclaration.name->name));
    TEST_ASSERT_NULL(find_compiler_type_prototype(
            &g_compiler, node->data.unionDeclaration.name->name));
}

static void assert_root_insertion_order(TZrBool opaqueFirst) {
    SZrAstNodeArray *nodes = parse_source("union Widget { First; } union Widget { Second; }");
    SZrAstNode *root = nodes->nodes[opaqueFirst ? 1U : 0U];

    compile_union(nodes->nodes[0], opaqueFirst ? g_definition : ZrParser_SyntaxContext_Root());
    compile_union(nodes->nodes[1], opaqueFirst ? ZrParser_SyntaxContext_Root() : g_definition);
    assert_declaration(find_compiler_type_prototype_inference(
            &g_compiler, root->data.unionDeclaration.name->name), root);
    assert_declaration(find_compiler_type_prototype(
            &g_compiler, root->data.unionDeclaration.name->name), root);
}

static void test_opaque_then_root_compiler_query(void) {
    assert_root_insertion_order(ZR_TRUE);
}

static void test_root_then_opaque_compiler_query(void) {
    assert_root_insertion_order(ZR_FALSE);
}

static void test_mixed_item_compiler_query_is_root_visible(void) {
    SZrAstNode *node = parse_source("union Widget { Item; }")->nodes[0];

    TEST_ASSERT_EQUAL_UINT32(0U, compile_union(node, g_mixed));
    assert_declaration(find_compiler_type_prototype(&g_compiler,
            node->data.unionDeclaration.name->name), node);
}

static void assert_current_prototype_query(SZrSyntaxContext context, TZrBool visible) {
    SZrAstNode *node = parse_source("union Widget { Item; }")->nodes[0];

    compile_union(node, context);
    TEST_ASSERT_EQUAL_UINT(1U, g_compiler.typePrototypes.length);
    g_compiler.currentTypePrototypeInfo = ZrCore_Array_Get(&g_compiler.typePrototypes, 0U);
    /* Keep the real producer's arrays owned for teardown while querying only current metadata. */
    g_hiddenPrototypeCount = g_compiler.typePrototypes.length;
    g_compiler.typePrototypes.length = 0U;
    if (visible) {
        assert_declaration(find_compiler_type_prototype(&g_compiler,
                node->data.unionDeclaration.name->name), node);
    } else {
        TEST_ASSERT_NULL(find_compiler_type_prototype_inference(&g_compiler,
                node->data.unionDeclaration.name->name));
        TEST_ASSERT_NULL(find_compiler_type_prototype(&g_compiler,
                node->data.unionDeclaration.name->name));
    }
}

static void test_current_opaque_prototype_is_excluded(void) {
    assert_current_prototype_query(g_definition, ZR_FALSE);
}

static void test_current_root_prototype_remains_visible(void) {
    assert_current_prototype_query(ZrParser_SyntaxContext_Root(), ZR_TRUE);
}

static void test_imported_null_declaration_remains_visible(void) {
    SZrAstNode *node = parse_source("union Widget { Item; }")->nodes[0];
    SZrTypePrototypeInfo *prototype;

    compile_union(node, ZrParser_SyntaxContext_Root());
    prototype = ZrCore_Array_Get(&g_compiler.typePrototypes, 0U);
    prototype->isImportedNative = ZR_TRUE;
    prototype->declarationNode = ZR_NULL;
    TEST_ASSERT_EQUAL_PTR(prototype, find_compiler_type_prototype(&g_compiler,
            node->data.unionDeclaration.name->name));
    TEST_ASSERT_NULL(prototype->declarationNode);
}

static void test_compiler_preserves_first_registration_precedence(void) {
    SZrAstNode *node = parse_source("union Widget { Item; }")->nodes[0];
    SZrTypePrototypeInfo imported = {0};
    SZrTypePrototypeInfo source;

    compile_union(node, ZrParser_SyntaxContext_Root());
    source = *(SZrTypePrototypeInfo *)ZrCore_Array_Get(&g_compiler.typePrototypes, 0U);
    imported.name = source.name;
    imported.type = source.type;
    imported.isImportedNative = ZR_TRUE;
    ZrCore_Array_Push(g_state, &g_compiler.typePrototypes, &source);
    *(SZrTypePrototypeInfo *)ZrCore_Array_Get(&g_compiler.typePrototypes, 0U) = imported;
    assert_declaration(find_compiler_type_prototype_inference(&g_compiler, source.name), node);
    TEST_ASSERT_EQUAL_PTR(ZrCore_Array_Get(&g_compiler.typePrototypes, 0U),
            find_compiler_type_prototype(&g_compiler, source.name));
}

static void test_root_alias_preserves_compiler_first_registration_precedence(void) {
    SZrAstNodeArray *nodes = parse_source("union Widget { Item; } let value: Widget;");
    SZrTypePrototypeInfo imported = {0};
    SZrTypePrototypeInfo source;
    SZrString *aliasName = ZrCore_String_CreateFromNative(g_state, "Selected");

    compile_union(nodes->nodes[0], ZrParser_SyntaxContext_Root());
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(&g_compiler,
            nodes->nodes[1]->data.variableDeclaration.typeInfo, &g_types[0]));
    TEST_ASSERT_EQUAL_UINT32(0U, g_types[0].nominalDefinitionToken);
    compiler_register_type_value_alias(&g_compiler, aliasName, &g_types[0],
            ZrParser_SyntaxContext_Root());
    source = *(SZrTypePrototypeInfo *)ZrCore_Array_Get(&g_compiler.typePrototypes, 0U);
    imported.name = source.name;
    imported.type = source.type;
    imported.isImportedNative = ZR_TRUE;
    ZrCore_Array_Push(g_state, &g_compiler.typePrototypes, &source);
    *(SZrTypePrototypeInfo *)ZrCore_Array_Get(&g_compiler.typePrototypes, 0U) = imported;
    assert_declaration(find_compiler_type_prototype_inference(&g_compiler, aliasName), nodes->nodes[0]);
    TEST_ASSERT_EQUAL_PTR(ZrCore_Array_Get(&g_compiler.typePrototypes, 0U),
            find_compiler_type_prototype(&g_compiler, aliasName));
}

static void test_compiler_root_query_excludes_opaque_specialization(void) {
    SZrAstNode *node = parse_source("union Parcel<T> { Item(value: T); }")->nodes[0];

    ZrParser_InferredType_Free(g_state, &g_types[1]);
    ZrParser_InferredType_Init(g_state, &g_types[1], ZR_VALUE_TYPE_INT64);
    init_instance(&g_types[0], ZrCore_String_CreateFromNative(g_state, "Parcel<int>"),
            compile_union(node, g_definition), &g_types[1]);
    assert_declaration(materialize(&g_types[0]), node);
    TEST_ASSERT_NULL(find_compiler_type_prototype_inference(&g_compiler, g_types[0].typeName));
    TEST_ASSERT_NULL(find_compiler_type_prototype(&g_compiler, g_types[0].typeName));
}

static void prepare_nominal_arguments(void) {
    SZrAstNodeArray *nodes = parse_source("union Parcel<T> { Item(value: T); }"
            "union Widget { Root; } union Widget { Generated; }");
    TZrSymbolId markedToken;

    compile_union(nodes->nodes[0], ZrParser_SyntaxContext_Root());
    compile_union(nodes->nodes[1], ZrParser_SyntaxContext_Root());
    markedToken = compile_union(nodes->nodes[2], g_definition);
    ZrParser_InferredType_Free(g_state, &g_types[2]);
    ZrParser_InferredType_InitFull(g_state, &g_types[2], ZR_VALUE_TYPE_OBJECT, ZR_FALSE,
            nodes->nodes[1]->data.unionDeclaration.name->name);
    init_instance(&g_types[0], ZrCore_String_CreateFromNative(g_state, "Parcel<Widget>"),
            0U, &g_types[2]);
    g_types[2].nominalDefinitionToken = markedToken;
    init_instance(&g_types[1], g_types[0].typeName, 0U, &g_types[2]);
    TEST_ASSERT_NOT_NULL(materialize(&g_types[1]));
}

static void assert_root_argument_selected(SZrTypePrototypeInfo *prototype) {
    SZrTypeMemberInfo *member;
    SZrInferredType *payload;

    TEST_ASSERT_NOT_NULL(prototype);
    TEST_ASSERT_EQUAL_UINT(1U, prototype->members.length);
    member = ZrCore_Array_Get(&prototype->members, 0U);
    TEST_ASSERT_EQUAL_UINT(1U, member->parameterTypes.length);
    payload = ZrCore_Array_Get(&member->parameterTypes, 0U);
    TEST_ASSERT_EQUAL_UINT32(0U, payload->nominalDefinitionToken);
}

static void test_compiler_root_query_selects_root_argument_after_marked_cache(void) {
    prepare_nominal_arguments();
    TEST_ASSERT_NOT_NULL(materialize(&g_types[0]));
    assert_root_argument_selected(find_compiler_type_prototype_inference(
            &g_compiler, g_types[0].typeName));
    assert_root_argument_selected(find_compiler_type_prototype(&g_compiler, g_types[0].typeName));
}

static void test_compiler_root_query_lazily_materializes_root_argument(void) {
    prepare_nominal_arguments();
    assert_root_argument_selected(find_compiler_type_prototype(&g_compiler, g_types[0].typeName));
    assert_root_argument_selected(find_compiler_type_prototype_inference(
            &g_compiler, g_types[0].typeName));
}

static SZrString *prepare_explicit_alias(TZrBool rootAlsoDeclared, SZrSyntaxContext aliasContext) {
    SZrAstNodeArray *nodes = parse_source(rootAlsoDeclared
            ? "union Parcel<T> { Root(value: T); } union Parcel<T> { Generated(value: T); }"
              "let Selected = Parcel<int>;"
            : "union Parcel<T> { Generated(value: T); } let Selected = Parcel<int>;");
    TZrSize markedIndex = rootAlsoDeclared ? 1U : 0U;
    SZrAstNode *alias = nodes->nodes[markedIndex + 1U];
    SZrAstNode *value;
    SZrTypeBinding *binding;
    SZrString *name;
    TZrSymbolId token;

    if (rootAlsoDeclared) {
        compile_union(nodes->nodes[0], ZrParser_SyntaxContext_Root());
        ZrParser_InferredType_Free(g_state, &g_types[2]);
        ZrParser_InferredType_Init(g_state, &g_types[2], ZR_VALUE_TYPE_INT64);
        init_instance(&g_types[1], ZrCore_String_CreateFromNative(g_state, "Parcel<int>"),
                0U, &g_types[2]);
        assert_declaration(materialize(&g_types[1]), nodes->nodes[0]);
    }
    token = compile_union(nodes->nodes[markedIndex], g_definition);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, alias->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, alias->data.variableDeclaration.pattern->type);
    name = alias->data.variableDeclaration.pattern->data.identifier.name;
    value = alias->data.variableDeclaration.value;
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_INT(ZR_AST_TYPE_LITERAL_EXPRESSION, value->type);
    TEST_ASSERT_NOT_NULL(value->data.typeLiteralExpression.typeInfo);
    TEST_ASSERT_EQUAL_INT(ZR_AST_GENERIC_TYPE, value->data.typeLiteralExpression.typeInfo->name->type);
    TEST_ASSERT_NOT_NULL(value->data.typeLiteralExpression.typeInfo->name->data.genericType.name);
    value->data.typeLiteralExpression.typeInfo->name->data.genericType.name->syntaxContext = g_definition;
    compiler_register_type_value_alias_literal(&g_compiler, name, value, aliasContext);
    binding = compiler_find_type_value_alias(&g_compiler, name, aliasContext);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_UINT32(token, binding->type.nominalDefinitionToken);
    TEST_ASSERT_EQUAL_UINT(1U, binding->type.elementTypes.length);
    ZrParser_InferredType_Free(g_state, &g_types[0]);
    ZrParser_InferredType_Copy(g_state, &g_types[0], &binding->type);
    assert_declaration(materialize(&g_types[0]), nodes->nodes[markedIndex]);
    return name;
}

static void assert_alias_target(TZrBool rootAlsoDeclared, TZrBool inference) {
    SZrString *name = prepare_explicit_alias(rootAlsoDeclared, ZrParser_SyntaxContext_Root());
    SZrTypePrototypeInfo *expected = type_inference_find_nominal_prototype(&g_compiler, &g_types[0]);
    SZrAstNode *declaration = expected->declarationNode;
    TZrTypeId instanceId = expected->genericInstanceTypeId;
    SZrTypePrototypeInfo *actual = inference
            ? find_compiler_type_prototype_inference(&g_compiler, name)
            : find_compiler_type_prototype(&g_compiler, name);

    assert_declaration(actual, declaration);
    TEST_ASSERT_EQUAL_UINT32(instanceId, actual->genericInstanceTypeId);
}

static void test_compiler_alias_explicitly_selects_opaque_only_target(void) {
    assert_alias_target(ZR_FALSE, ZR_FALSE);
}

static void test_inference_alias_explicitly_selects_opaque_only_target(void) {
    assert_alias_target(ZR_FALSE, ZR_TRUE);
}

static void test_compiler_alias_selects_marked_target_despite_same_spelling_root(void) {
    assert_alias_target(ZR_TRUE, ZR_FALSE);
}

static void test_inference_alias_selects_marked_target_despite_same_spelling_root(void) {
    assert_alias_target(ZR_TRUE, ZR_TRUE);
}

static void assert_alias_nominal_argument(TZrBool inference) {
    SZrAstNodeArray *nodes = parse_source("union Parcel<T> { Item(value: T); }"
            "union Widget { Root; } union Widget { Generated; }"
            "let Selected = Parcel<Widget>;");
    SZrAstNode *alias = nodes->nodes[3];
    SZrAstNode *value = alias->data.variableDeclaration.value;
    SZrGenericType *generic;
    SZrAstNode *argumentNode;
    SZrTypeBinding *binding;
    SZrInferredType *argument;
    SZrString *name = alias->data.variableDeclaration.pattern->data.identifier.name;
    TZrSymbolId token;
    TZrTypeId expectedId;
    SZrTypePrototypeInfo *actual;

    compile_union(nodes->nodes[0], ZrParser_SyntaxContext_Root());
    compile_union(nodes->nodes[1], ZrParser_SyntaxContext_Root());
    token = compile_union(nodes->nodes[2], g_definition);
    TEST_ASSERT_EQUAL_INT(ZR_AST_TYPE_LITERAL_EXPRESSION, value->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_GENERIC_TYPE, value->data.typeLiteralExpression.typeInfo->name->type);
    generic = &value->data.typeLiteralExpression.typeInfo->name->data.genericType;
    TEST_ASSERT_EQUAL_UINT(1U, generic->params->count);
    argumentNode = generic->params->nodes[0];
    TEST_ASSERT_EQUAL_INT(ZR_AST_TYPE, argumentNode->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, argumentNode->data.type.name->type);
    argumentNode->data.type.name->data.identifier.syntaxContext = g_definition;
    compiler_register_type_value_alias_literal(&g_compiler, name, value, ZrParser_SyntaxContext_Root());
    binding = compiler_find_type_value_alias(&g_compiler, name, ZrParser_SyntaxContext_Root());
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_UINT32(0U, binding->type.nominalDefinitionToken);
    argument = ZrCore_Array_Get(&binding->type.elementTypes, 0U);
    TEST_ASSERT_EQUAL_UINT32(token, argument->nominalDefinitionToken);
    ZrParser_InferredType_Free(g_state, &g_types[0]);
    ZrParser_InferredType_Copy(g_state, &g_types[0], &binding->type);
    ZrParser_InferredType_Free(g_state, &g_types[1]);
    ZrParser_InferredType_Copy(g_state, &g_types[1], &binding->type);
    argument = ZrCore_Array_Get(&g_types[1].elementTypes, 0U);
    argument->nominalDefinitionToken = 0U;
    assert_declaration(materialize(&g_types[1]), nodes->nodes[0]);
    expectedId = materialize(&g_types[0])->genericInstanceTypeId;
    actual = inference ? find_compiler_type_prototype_inference(&g_compiler, name)
                       : find_compiler_type_prototype(&g_compiler, name);
    assert_declaration(actual, nodes->nodes[0]);
    TEST_ASSERT_EQUAL_UINT32(expectedId, actual->genericInstanceTypeId);
}

static void test_compiler_alias_preserves_marked_generic_argument(void) {
    assert_alias_nominal_argument(ZR_FALSE);
}

static void test_inference_alias_preserves_marked_generic_argument(void) {
    assert_alias_nominal_argument(ZR_TRUE);
}

static void test_marked_alias_does_not_escape_to_root(void) {
    SZrString *name = prepare_explicit_alias(ZR_TRUE, g_definition);

    TEST_ASSERT_NULL(compiler_find_type_value_alias(&g_compiler, name, ZrParser_SyntaxContext_Root()));
    TEST_ASSERT_NULL(find_compiler_type_prototype_inference(&g_compiler, name));
    TEST_ASSERT_NULL(find_compiler_type_prototype(&g_compiler, name));
}

static void assert_invalid_alias_target(TZrBool inference) {
    SZrString *name = prepare_explicit_alias(ZR_TRUE, ZrParser_SyntaxContext_Root());

    g_types[0].nominalDefinitionToken = ~0U;
    compiler_register_type_value_alias(&g_compiler, name, &g_types[0], ZrParser_SyntaxContext_Root());
    TEST_ASSERT_NULL(type_inference_find_nominal_prototype(&g_compiler, &g_types[0]));
    TEST_ASSERT_NULL(inference ? find_compiler_type_prototype_inference(&g_compiler, name)
                              : find_compiler_type_prototype(&g_compiler, name));
}

static void test_compiler_invalid_alias_target_cannot_fall_back_to_root(void) {
    assert_invalid_alias_target(ZR_FALSE);
}

static void test_inference_invalid_alias_target_cannot_fall_back_to_root(void) {
    assert_invalid_alias_target(ZR_TRUE);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_root_compiler_query_prerequisite);
    RUN_TEST(test_compiler_root_query_excludes_opaque_declaration);
    RUN_TEST(test_opaque_then_root_compiler_query);
    RUN_TEST(test_root_then_opaque_compiler_query);
    RUN_TEST(test_mixed_item_compiler_query_is_root_visible);
    RUN_TEST(test_current_opaque_prototype_is_excluded);
    RUN_TEST(test_current_root_prototype_remains_visible);
    RUN_TEST(test_imported_null_declaration_remains_visible);
    RUN_TEST(test_compiler_preserves_first_registration_precedence);
    RUN_TEST(test_root_alias_preserves_compiler_first_registration_precedence);
    RUN_TEST(test_compiler_root_query_excludes_opaque_specialization);
    RUN_TEST(test_compiler_root_query_selects_root_argument_after_marked_cache);
    RUN_TEST(test_compiler_root_query_lazily_materializes_root_argument);
    RUN_TEST(test_compiler_alias_explicitly_selects_opaque_only_target);
    RUN_TEST(test_inference_alias_explicitly_selects_opaque_only_target);
    RUN_TEST(test_compiler_alias_selects_marked_target_despite_same_spelling_root);
    RUN_TEST(test_inference_alias_selects_marked_target_despite_same_spelling_root);
    RUN_TEST(test_compiler_alias_preserves_marked_generic_argument);
    RUN_TEST(test_inference_alias_preserves_marked_generic_argument);
    RUN_TEST(test_marked_alias_does_not_escape_to_root);
    RUN_TEST(test_compiler_invalid_alias_target_cannot_fall_back_to_root);
    RUN_TEST(test_inference_invalid_alias_target_cannot_fall_back_to_root);
    return UNITY_END();
}
