#include "unity.h"

#include <string.h>

#include "types/canonical_type_definition_internal.h"
#include "harness/runtime_support.h"
#include "type_inference/type_inference_internal.h"
#include "type_inference/type_inference_nominal_prototype.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic.h"

static SZrState *g_state;
static SZrCompilerState g_compiler;
static TZrBool g_compilerInitialized;
static SZrAstNode *g_ast;
static SZrSyntaxContextTable *g_table;
static SZrSyntaxContext g_definition;
static SZrSyntaxContext g_otherDefinition;
static SZrSyntaxContext g_mixed;
static SZrInferredType g_targetType;

void setUp(void) {
    SZrSyntaxExpansionInfo expansion = {0};
    TZrSyntaxExpansionId expansionId;
    SZrScope scope = {0};

    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
    ZrParser_InferredType_Init(g_state, &g_targetType, ZR_VALUE_TYPE_OBJECT);
    g_table = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_table);
    expansion.stableIdentity = 0xB301U;
    expansion.callSiteSpan = ZrParser_TokenSpan_None();
    expansion.definitionSiteSpan = ZrParser_TokenSpan_None();
    TEST_ASSERT_TRUE(ZrParser_SyntaxContextTable_RegisterExpansion(
            g_table, &expansion, &expansionId));
    g_definition = ZrParser_SyntaxContext_DefSite(g_table, expansionId);
    g_mixed = ZrParser_SyntaxContext_MixedSite(g_table, expansionId);
    expansion.stableIdentity = 0xB302U;
    TEST_ASSERT_TRUE(ZrParser_SyntaxContextTable_RegisterExpansion(
            g_table, &expansion, &expansionId));
    g_otherDefinition = ZrParser_SyntaxContext_DefSite(g_table, expansionId);
    ZrParser_CompilerState_Init(&g_compiler, g_state);
    g_compilerInitialized = ZR_TRUE;
    g_compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(g_compiler.currentFunction);
    g_compiler.isScriptLevel = ZR_TRUE;
    scope.parentCompiler = &g_compiler;
    ZrCore_Array_Push(g_state, &g_compiler.scopeStack, &scope);
    TEST_ASSERT_NOT_NULL(g_compiler.typeEnv);
    TEST_ASSERT_NOT_NULL(g_compiler.semanticContext);
}

void tearDown(void) {
    ZrParser_InferredType_Free(g_state, &g_targetType);
    if (g_compilerInitialized) {
        if (g_compiler.currentFunction != ZR_NULL) {
            ZrCore_Function_Free(g_state, g_compiler.currentFunction);
            g_compiler.currentFunction = ZR_NULL;
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
    SZrString *name = ZrCore_String_CreateFromNative(g_state, "generic_definitions.zr");

    TEST_ASSERT_NOT_NULL(name);
    g_ast = ZrParser_Parse(g_state, source, strlen(source), name);
    TEST_ASSERT_NOT_NULL(g_ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_ast->type);
    g_compiler.scriptAst = g_ast;
    return g_ast->data.script.statements;
}

static const SZrNamedTypeBinding *find_binding(SZrAstNode *node, SZrSyntaxContext context) {
    const SZrNamedTypeBinding *binding = ZrParser_TypeEnvironment_FindTypeBindingWithContext(
            g_compiler.typeEnv, node->data.structDeclaration.name->name, context);

    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_PTR(node, binding->declarationNode);
    return binding;
}

static void compile_struct(SZrAstNode *node, SZrSyntaxContext context) {
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_DECLARATION, node->type);
    node->data.structDeclaration.name->syntaxContext = context;
    ZrParser_Compiler_CompileStructDeclaration(&g_compiler, node);
    TEST_ASSERT_FALSE(g_compiler.hasError);
}

static void assert_definition(SZrAstNode *node, SZrSyntaxContext context,
        TZrSize genericParameterCount) {
    const SZrNamedTypeBinding *binding = find_binding(node, context);
    TZrTypeId typeId = binding->typeId;
    TZrSymbolId symbolId = binding->symbolId;
    const SZrCanonicalTypeDefinitionRecord *definition;
    const SZrSemanticSymbolRecord *symbol;

    symbol = ZrParser_Semantic_FindSymbolById(g_compiler.semanticContext, symbolId);
    TEST_ASSERT_NOT_NULL(symbol);
    TEST_ASSERT_EQUAL_PTR(node, symbol->astNode);
    definition = ZrParser_CanonicalTypeDefinition_FindRecord(g_compiler.semanticContext, typeId);
    TEST_ASSERT_NOT_NULL(definition);
    TEST_ASSERT_EQUAL_UINT32(typeId, definition->typeId);
    TEST_ASSERT_EQUAL_UINT(genericParameterCount, definition->genericParameterCount);
    TEST_ASSERT_EQUAL_UINT32(genericParameterCount == 0U ? 0U : symbolId,
            definition->genericOwnerSymbolId);
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_HasCapabilities(g_compiler.semanticContext, typeId,
            ZR_CANONICAL_TYPE_CAPABILITY_VALUE_TYPE |
            ZR_CANONICAL_TYPE_CAPABILITY_VALUE_CONSTRUCTIBLE));
}

static void test_root_struct_generic_definition_prerequisite(void) {
    SZrAstNode *node = parse_source("struct Parcel<T> {}")->nodes[0];

    compile_struct(node, ZrParser_SyntaxContext_Root());
    assert_definition(node, ZrParser_SyntaxContext_Root(), 1U);
}

static void test_definition_struct_generic_uses_its_bound_nominal(void) {
    SZrAstNode *node = parse_source("struct Parcel<T> {}")->nodes[0];

    compile_struct(node, g_definition);
    assert_definition(node, g_definition, 1U);
}

static void assert_distinct_struct_definitions(SZrSyntaxContext first, SZrSyntaxContext second) {
    SZrAstNodeArray *nodes = parse_source("struct Parcel<T> {} struct Parcel<T> {}");
    TZrTypeId firstType;
    TZrSymbolId firstSymbol;
    const SZrNamedTypeBinding *secondBinding;

    compile_struct(nodes->nodes[0], first);
    firstType = find_binding(nodes->nodes[0], first)->typeId;
    firstSymbol = find_binding(nodes->nodes[0], first)->symbolId;
    compile_struct(nodes->nodes[1], second);
    secondBinding = find_binding(nodes->nodes[1], second);
    TEST_ASSERT_NOT_EQUAL(firstType, secondBinding->typeId);
    TEST_ASSERT_NOT_EQUAL(firstSymbol, secondBinding->symbolId);
    assert_definition(nodes->nodes[0], first, 1U);
    assert_definition(nodes->nodes[1], second, 1U);
}

static void test_root_then_definition_struct_owners_are_distinct(void) {
    assert_distinct_struct_definitions(ZrParser_SyntaxContext_Root(), g_definition);
}

static void test_definition_then_root_struct_owners_are_distinct(void) {
    assert_distinct_struct_definitions(g_definition, ZrParser_SyntaxContext_Root());
}

static void test_two_opaque_struct_owners_are_distinct(void) {
    assert_distinct_struct_definitions(g_definition, g_otherDefinition);
}

static void test_marked_nongeneric_struct_capabilities_use_bound_nominal(void) {
    SZrAstNode *node = parse_source("struct Parcel {}")->nodes[0];

    compile_struct(node, g_definition);
    assert_definition(node, g_definition, 0U);
}

static void test_mixed_site_struct_definition_uses_item_root_identity(void) {
    SZrAstNode *node = parse_source("struct Parcel<T> {}")->nodes[0];

    compile_struct(node, g_mixed);
    assert_definition(node, ZrParser_SyntaxContext_Root(), 1U);
}

static void test_forward_struct_binding_keeps_its_generic_owner(void) {
    SZrAstNode *node = parse_source("struct Parcel<T> {}")->nodes[0];
    TZrTypeId typeId;
    TZrSymbolId symbolId;

    node->data.structDeclaration.name->syntaxContext = g_definition;
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterTypeDeclaration(g_state,
            g_compiler.typeEnv, node->data.structDeclaration.name->name, node));
    typeId = find_binding(node, g_definition)->typeId;
    symbolId = find_binding(node, g_definition)->symbolId;
    compile_struct(node, g_definition);
    TEST_ASSERT_EQUAL_UINT32(typeId, find_binding(node, g_definition)->typeId);
    TEST_ASSERT_EQUAL_UINT32(symbolId, find_binding(node, g_definition)->symbolId);
    assert_definition(node, g_definition, 1U);
}

static void test_marked_struct_instance_resolves_registered_capabilities(void) {
    SZrAstNode *node = parse_source("struct Parcel<T> {}")->nodes[0];
    TZrTypeId definitionId;
    TZrTypeId intId;
    TZrTypeId instanceId;

    compile_struct(node, g_definition);
    definitionId = find_binding(node, g_definition)->typeId;
    intId = ZrParser_CanonicalType_InternPrimitive(g_compiler.semanticContext, ZR_VALUE_TYPE_INT64);
    instanceId = ZrParser_CanonicalType_InternGenericInstance(g_compiler.semanticContext,
            definitionId, &intId, 1U);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, instanceId);
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_HasCapabilities(g_compiler.semanticContext, instanceId,
            ZR_CANONICAL_TYPE_CAPABILITY_VALUE_CONSTRUCTIBLE));
}

static void assert_duplicate_struct_releases_unpublished_storage(SZrSyntaxContext context,
        TZrBool verifyRecovery) {
    SZrAstNodeArray *nodes = parse_source(
            "struct Parcel<T> {} struct Parcel<T> {} struct Parcel<T> {}");
    TZrUInt64 allocatedBytes;
    TZrSize symbolCount;
    TZrSize definitionCount;

    compile_struct(nodes->nodes[0], context);
    g_compiler.suppressErrorOutput = ZR_TRUE;
    ZrParser_Compiler_Error(&g_compiler,
            "warm persistent compiler diagnostic buffer before duplicate declaration cleanup",
            (SZrFileRange){0});
    g_compiler.hasError = ZR_FALSE;
    g_compiler.hadRecoverableError = ZR_FALSE;
    g_compiler.errorMessage = ZR_NULL;
    symbolCount = g_compiler.semanticContext->symbols.length;
    definitionCount = g_compiler.semanticContext->canonicalTypeDefinitions.length;
    allocatedBytes = g_state->global->allocatedBytes;
    nodes->nodes[1]->data.structDeclaration.name->syntaxContext = context;
    ZrParser_Compiler_CompileStructDeclaration(&g_compiler, nodes->nodes[1]);
    TEST_ASSERT_TRUE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_UINT(1U, g_compiler.typePrototypes.length);
    TEST_ASSERT_EQUAL_UINT(symbolCount, g_compiler.semanticContext->symbols.length);
    TEST_ASSERT_EQUAL_UINT(definitionCount, g_compiler.semanticContext->canonicalTypeDefinitions.length);
    TEST_ASSERT_EQUAL_PTR(nodes->nodes[0], find_binding(nodes->nodes[0], context)->declarationNode);
    TEST_ASSERT_EQUAL_UINT64(allocatedBytes, g_state->global->allocatedBytes);
    TEST_ASSERT_NULL(g_compiler.currentTypeName);
    TEST_ASSERT_NULL(g_compiler.currentTypeNode);
    TEST_ASSERT_NULL(g_compiler.currentTypePrototypeInfo);
    if (verifyRecovery) {
        g_compiler.hasError = ZR_FALSE;
        g_compiler.hadRecoverableError = ZR_FALSE;
        compile_struct(nodes->nodes[2], g_otherDefinition);
        TEST_ASSERT_EQUAL_UINT(2U, g_compiler.typePrototypes.length);
        assert_definition(nodes->nodes[0], context, 1U);
        assert_definition(nodes->nodes[2], g_otherDefinition, 1U);
    }
}

static void test_root_duplicate_struct_releases_unpublished_storage(void) {
    assert_duplicate_struct_releases_unpublished_storage(ZrParser_SyntaxContext_Root(), ZR_FALSE);
}

static void test_marked_duplicate_struct_releases_unpublished_storage(void) {
    assert_duplicate_struct_releases_unpublished_storage(g_definition, ZR_FALSE);
}

static void test_root_duplicate_error_allows_distinct_context_recovery(void) {
    assert_duplicate_struct_releases_unpublished_storage(ZrParser_SyntaxContext_Root(), ZR_TRUE);
}

static void test_marked_duplicate_error_allows_distinct_context_recovery(void) {
    assert_duplicate_struct_releases_unpublished_storage(g_definition, ZR_TRUE);
}

static void assert_generic_constructor_target(SZrAstNode *declaration,
        SZrAstNode *variable, SZrSyntaxContext context) {
    SZrAstNode *construct;
    SZrAstNode *target;
    const SZrNamedTypeBinding *binding;
    SZrTypePrototypeInfo *prototype;
    SZrString *typeName = ZR_NULL;
    TZrTypeId definitionId;
    TZrTypeId intId;
    TZrTypeId expectedKey;

    assert_definition(declaration, context, 1U);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, variable->type);
    construct = variable->data.variableDeclaration.value;
    TEST_ASSERT_NOT_NULL(construct);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CONSTRUCT_EXPRESSION, construct->type);
    target = construct->data.constructExpression.target;
    TEST_ASSERT_NOT_NULL(target);
    TEST_ASSERT_EQUAL_INT(ZR_AST_TYPE, target->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_GENERIC_TYPE, target->data.type.name->type);
    target->data.type.name->data.genericType.name->syntaxContext = context;
    binding = find_binding(declaration, context);
    definitionId = binding->typeId;
    intId = ZrParser_CanonicalType_InternPrimitive(g_compiler.semanticContext, ZR_VALUE_TYPE_INT64);
    expectedKey = ZrParser_CanonicalType_InternGenericInstance(g_compiler.semanticContext,
            definitionId, &intId, 1U);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, expectedKey);
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(&g_compiler,
            &target->data.type, &g_targetType));
    TEST_ASSERT_FALSE(g_compiler.hasError);
    prototype = type_inference_find_nominal_prototype(&g_compiler, &g_targetType);
    TEST_ASSERT_NOT_NULL(prototype);
    TEST_ASSERT_EQUAL_PTR(declaration, prototype->declarationNode);
    TEST_ASSERT_EQUAL_UINT32(expectedKey, prototype->genericInstanceTypeId);
    prototype = ZR_NULL;
    TEST_ASSERT_TRUE(resolve_prototype_target_inference(&g_compiler, target, &prototype, &typeName));
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_NOT_NULL(prototype);
    TEST_ASSERT_EQUAL_PTR(declaration, prototype->declarationNode);
    TEST_ASSERT_EQUAL_UINT32(expectedKey, prototype->genericInstanceTypeId);
    TEST_ASSERT_NOT_NULL(typeName);
    TEST_ASSERT_EQUAL_STRING("Parcel<int>", ZrCore_String_GetNativeString(typeName));
}

static void test_root_generic_constructor_target_prerequisite(void) {
    SZrAstNodeArray *nodes = parse_source("struct Parcel<T> {} var result = new Parcel<int>();");

    compile_struct(nodes->nodes[0], ZrParser_SyntaxContext_Root());
    assert_generic_constructor_target(nodes->nodes[0], nodes->nodes[1], ZrParser_SyntaxContext_Root());
}

static void test_marked_only_generic_constructor_target_retains_selected_prototype(void) {
    SZrAstNodeArray *nodes = parse_source("struct Parcel<T> {} var result = new Parcel<int>();");

    compile_struct(nodes->nodes[0], g_definition);
    assert_generic_constructor_target(nodes->nodes[0], nodes->nodes[1], g_definition);
}

static void test_root_then_marked_generic_constructor_target_is_isolated(void) {
    SZrAstNodeArray *nodes = parse_source(
            "struct Parcel<T> {} struct Parcel<T> {} var result = new Parcel<int>();");

    compile_struct(nodes->nodes[0], ZrParser_SyntaxContext_Root());
    compile_struct(nodes->nodes[1], g_definition);
    assert_generic_constructor_target(nodes->nodes[1], nodes->nodes[2], g_definition);
}

static void test_marked_then_root_generic_constructor_target_is_isolated(void) {
    SZrAstNodeArray *nodes = parse_source(
            "struct Parcel<T> {} struct Parcel<T> {} var result = new Parcel<int>();");

    compile_struct(nodes->nodes[0], g_definition);
    compile_struct(nodes->nodes[1], ZrParser_SyntaxContext_Root());
    assert_generic_constructor_target(nodes->nodes[0], nodes->nodes[2], g_definition);
}

static void test_two_opaque_generic_constructor_targets_are_distinct(void) {
    SZrAstNodeArray *nodes = parse_source(
            "struct Parcel<T> {} struct Parcel<T> {} var result = new Parcel<int>();");

    compile_struct(nodes->nodes[0], g_definition);
    compile_struct(nodes->nodes[1], g_otherDefinition);
    assert_generic_constructor_target(nodes->nodes[1], nodes->nodes[2], g_otherDefinition);
}

static void test_mixed_site_generic_constructor_target_keeps_item_root_behavior(void) {
    SZrAstNodeArray *nodes = parse_source("struct Parcel<T> {} var result = new Parcel<int>();");

    compile_struct(nodes->nodes[0], g_mixed);
    assert_generic_constructor_target(nodes->nodes[0], nodes->nodes[1], g_mixed);
}

static void test_invalid_generic_target_context_cannot_fall_back_to_root(void) {
    SZrAstNodeArray *nodes = parse_source("struct Parcel<T> {} var result = new Parcel<int>();");
    SZrAstNode *target;
    SZrTypePrototypeInfo *prototype;
    SZrString *typeName;

    compile_struct(nodes->nodes[0], ZrParser_SyntaxContext_Root());
    target = nodes->nodes[1]->data.variableDeclaration.value->data.constructExpression.target;
    TEST_ASSERT_EQUAL_INT(ZR_AST_TYPE, target->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_GENERIC_TYPE, target->data.type.name->type);
    target->data.type.name->data.genericType.name->syntaxContext = (SZrSyntaxContext){g_table, ~0U};
    prototype = ZrCore_Array_Get(&g_compiler.typePrototypes, 0U);
    typeName = nodes->nodes[0]->data.structDeclaration.name->name;
    TEST_ASSERT_FALSE(resolve_prototype_target_inference(&g_compiler, target, &prototype, &typeName));
    TEST_ASSERT_NULL(prototype);
    TEST_ASSERT_NULL(typeName);
    TEST_ASSERT_EQUAL_UINT(1U, g_compiler.typePrototypes.length);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_root_struct_generic_definition_prerequisite);
    RUN_TEST(test_definition_struct_generic_uses_its_bound_nominal);
    RUN_TEST(test_root_then_definition_struct_owners_are_distinct);
    RUN_TEST(test_definition_then_root_struct_owners_are_distinct);
    RUN_TEST(test_two_opaque_struct_owners_are_distinct);
    RUN_TEST(test_marked_nongeneric_struct_capabilities_use_bound_nominal);
    RUN_TEST(test_mixed_site_struct_definition_uses_item_root_identity);
    RUN_TEST(test_forward_struct_binding_keeps_its_generic_owner);
    RUN_TEST(test_marked_struct_instance_resolves_registered_capabilities);
    RUN_TEST(test_root_duplicate_struct_releases_unpublished_storage);
    RUN_TEST(test_marked_duplicate_struct_releases_unpublished_storage);
    RUN_TEST(test_root_duplicate_error_allows_distinct_context_recovery);
    RUN_TEST(test_marked_duplicate_error_allows_distinct_context_recovery);
    RUN_TEST(test_root_generic_constructor_target_prerequisite);
    RUN_TEST(test_marked_only_generic_constructor_target_retains_selected_prototype);
    RUN_TEST(test_root_then_marked_generic_constructor_target_is_isolated);
    RUN_TEST(test_marked_then_root_generic_constructor_target_is_isolated);
    RUN_TEST(test_two_opaque_generic_constructor_targets_are_distinct);
    RUN_TEST(test_mixed_site_generic_constructor_target_keeps_item_root_behavior);
    RUN_TEST(test_invalid_generic_target_context_cannot_fall_back_to_root);
    return UNITY_END();
}
