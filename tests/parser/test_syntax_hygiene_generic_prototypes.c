#include "unity.h"

#include <stdio.h>
#include <string.h>

#include "compiler/compiler_internal.h"
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
static SZrSyntaxContext g_otherDefinition;
static SZrSyntaxContext g_mixed;
static SZrInferredType g_types[4];
static SZrTypeEnvironment *g_rootEnvironment;
static SZrTypeEnvironment *g_ownerEnvironment;

void setUp(void) {
    SZrSyntaxExpansionInfo expansion = {0};
    TZrSyntaxExpansionId expansionId;

    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
    for (TZrSize index = 0U; index < 4U; index++) {
        ZrParser_InferredType_Init(g_state, &g_types[index], ZR_VALUE_TYPE_OBJECT);
    }
    g_table = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_table);
    expansion.stableIdentity = 0xB201U;
    expansion.callSiteSpan = ZrParser_TokenSpan_None();
    expansion.definitionSiteSpan = ZrParser_TokenSpan_None();
    TEST_ASSERT_TRUE(ZrParser_SyntaxContextTable_RegisterExpansion(
            g_table, &expansion, &expansionId));
    g_definition = ZrParser_SyntaxContext_DefSite(g_table, expansionId);
    g_mixed = ZrParser_SyntaxContext_MixedSite(g_table, expansionId);
    expansion.stableIdentity = 0xB202U;
    TEST_ASSERT_TRUE(ZrParser_SyntaxContextTable_RegisterExpansion(
            g_table, &expansion, &expansionId));
    g_otherDefinition = ZrParser_SyntaxContext_DefSite(g_table, expansionId);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(g_definition));
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(g_otherDefinition));
    ZrParser_CompilerState_Init(&g_compiler, g_state);
    g_compilerInitialized = ZR_TRUE;
    TEST_ASSERT_NOT_NULL(g_compiler.typeEnv);
    TEST_ASSERT_NOT_NULL(g_compiler.semanticContext);
    g_rootEnvironment = g_compiler.typeEnv;
}

void tearDown(void) {
    for (TZrSize index = 0U; index < 4U; index++) {
        ZrParser_InferredType_Free(g_state, &g_types[index]);
    }
    if (g_compilerInitialized) {
        if (g_ownerEnvironment != ZR_NULL) {
            g_compiler.typeEnv = g_rootEnvironment;
            ZrParser_TypeEnvironment_Free(g_state, g_ownerEnvironment);
            g_ownerEnvironment = ZR_NULL;
        }
        ZrParser_CompilerState_Free(&g_compiler);
        g_compilerInitialized = ZR_FALSE;
        g_rootEnvironment = ZR_NULL;
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
    SZrString *name = ZrCore_String_CreateFromNative(g_state, "generic_hygiene.zr");

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

static void init_instance(SZrInferredType *type, TZrNativeString displayName,
        TZrSymbolId definitionToken, SZrInferredType *argument) {
    SZrInferredType copy;

    ZrParser_InferredType_Free(g_state, type);
    ZrParser_InferredType_InitFull(g_state, type, ZR_VALUE_TYPE_OBJECT, ZR_FALSE,
            ZrCore_String_CreateFromNative(g_state, displayName));
    type->nominalDefinitionToken = definitionToken;
    ZrCore_Array_Init(g_state, &type->elementTypes, sizeof(SZrInferredType), 1U);
    ZrParser_InferredType_Copy(g_state, &copy, argument);
    ZrCore_Array_Push(g_state, &type->elementTypes, &copy);
}

static void init_int_instance(SZrInferredType *type, TZrSymbolId definitionToken) {
    SZrInferredType argument;

    ZrParser_InferredType_Init(g_state, &argument, ZR_VALUE_TYPE_INT64);
    init_instance(type, "Parcel<int>", definitionToken, &argument);
    ZrParser_InferredType_Free(g_state, &argument);
}

static SZrTypePrototypeInfo *materialize(SZrInferredType *type) {
    TEST_ASSERT_TRUE(ensure_generic_instance_type_prototype_from_inferred(&g_compiler, type));
    TEST_ASSERT_FALSE(g_compiler.hasError);
    return type_inference_find_nominal_prototype(&g_compiler, type);
}

static void assert_payload(SZrTypePrototypeInfo *prototype, const TZrChar *variant,
        EZrValueType baseType, TZrSymbolId argumentToken) {
    SZrTypeMemberInfo *member;
    SZrInferredType *payload;

    TEST_ASSERT_NOT_NULL(prototype);
    TEST_ASSERT_EQUAL_UINT(1U, prototype->members.length);
    member = ZrCore_Array_Get(&prototype->members, 0U);
    TEST_ASSERT_NOT_NULL(member);
    TEST_ASSERT_EQUAL_STRING(variant, ZrCore_String_GetNativeString(member->name));
    TEST_ASSERT_EQUAL_UINT(1U, member->parameterTypes.length);
    payload = ZrCore_Array_Get(&member->parameterTypes, 0U);
    TEST_ASSERT_NOT_NULL(payload);
    TEST_ASSERT_EQUAL_INT(baseType, payload->baseType);
    TEST_ASSERT_EQUAL_UINT32(argumentToken, payload->nominalDefinitionToken);
}

static void test_root_generic_materialization_prerequisite(void) {
    SZrAstNode *node = parse_source("union Parcel<T> { Item(value: T); }")->nodes[0];

    TEST_ASSERT_EQUAL_UINT32(0U, compile_union(node, ZrParser_SyntaxContext_Root()));
    init_int_instance(&g_types[0], 0U);
    assert_payload(materialize(&g_types[0]), "Item", ZR_VALUE_TYPE_INT64, 0U);
}

static void test_definition_only_generic_materializes_selected_declaration(void) {
    SZrAstNode *node = parse_source("union Parcel<T> { Item(value: T); }")->nodes[0];
    TZrSymbolId token = compile_union(node, g_definition);
    SZrTypePrototypeInfo *prototype;

    TEST_ASSERT_NOT_EQUAL(0U, token);
    init_int_instance(&g_types[0], token);
    prototype = materialize(&g_types[0]);
    TEST_ASSERT_NOT_NULL(prototype);
    TEST_ASSERT_EQUAL_PTR(node, prototype->declarationNode);
    assert_payload(prototype, "Item", ZR_VALUE_TYPE_INT64, 0U);
}

static void assert_distinct_definitions(SZrSyntaxContext first, SZrSyntaxContext second,
        TZrBool reverseMaterialization) {
    SZrAstNodeArray *nodes = parse_source(
            "union Parcel<T> { First(value: T); }"
            "union Parcel<T> { Second(value: T); }");
    TZrSymbolId firstToken = compile_union(nodes->nodes[0], first);
    TZrSymbolId secondToken = compile_union(nodes->nodes[1], second);

    TEST_ASSERT_NOT_EQUAL(firstToken, secondToken);
    init_int_instance(&g_types[0], firstToken);
    init_int_instance(&g_types[1], secondToken);
    TEST_ASSERT_TRUE(ensure_generic_instance_type_prototype_from_inferred(
            &g_compiler, &g_types[reverseMaterialization ? 1U : 0U]));
    TEST_ASSERT_TRUE(ensure_generic_instance_type_prototype_from_inferred(
            &g_compiler, &g_types[reverseMaterialization ? 0U : 1U]));
    assert_payload(type_inference_find_nominal_prototype(&g_compiler, &g_types[0]),
            "First", ZR_VALUE_TYPE_INT64, 0U);
    assert_payload(type_inference_find_nominal_prototype(&g_compiler, &g_types[1]),
            "Second", ZR_VALUE_TYPE_INT64, 0U);
    TEST_ASSERT_EQUAL_UINT(4U, g_compiler.typePrototypes.length);
}

static void test_root_then_definition_cache_isolation(void) {
    assert_distinct_definitions(ZrParser_SyntaxContext_Root(), g_definition, ZR_FALSE);
}

static void test_definition_then_root_cache_isolation(void) {
    assert_distinct_definitions(g_definition, ZrParser_SyntaxContext_Root(), ZR_FALSE);
}

static void test_reverse_materialization_cache_isolation(void) {
    assert_distinct_definitions(ZrParser_SyntaxContext_Root(), g_definition, ZR_TRUE);
}

static void test_two_opaque_definitions_cache_isolation(void) {
    assert_distinct_definitions(g_definition, g_otherDefinition, ZR_FALSE);
}

static void test_definition_specialization_is_not_root_visible(void) {
    SZrAstNode *node = parse_source("union Parcel<T> { Item(value: T); }")->nodes[0];

    init_int_instance(&g_types[0], compile_union(node, g_definition));
    TEST_ASSERT_NOT_NULL(materialize(&g_types[0]));
    TEST_ASSERT_FALSE(ZrParser_TypeEnvironment_LookupType(g_compiler.typeEnv, g_types[0].typeName));
    TEST_ASSERT_NULL(find_compiler_type_prototype_inference(&g_compiler, g_types[0].typeName));
}

static void test_repeated_structured_specialization_reuses_cache(void) {
    SZrAstNode *node = parse_source("union Parcel<T> { Item(value: T); }")->nodes[0];

    init_int_instance(&g_types[0], compile_union(node, g_definition));
    assert_payload(materialize(&g_types[0]), "Item", ZR_VALUE_TYPE_INT64, 0U);
    TEST_ASSERT_TRUE(ensure_generic_instance_type_prototype_from_inferred(&g_compiler, &g_types[0]));
    TEST_ASSERT_EQUAL_UINT(2U, g_compiler.typePrototypes.length);
}

static void test_mixed_site_item_generic_uses_root_definition(void) {
    SZrAstNode *node = parse_source("union Parcel<T> { Item(value: T); }")->nodes[0];

    init_int_instance(&g_types[0], compile_union(node, g_mixed));
    TEST_ASSERT_EQUAL_UINT32(0U, g_types[0].nominalDefinitionToken);
    assert_payload(materialize(&g_types[0]), "Item", ZR_VALUE_TYPE_INT64, 0U);
}

static TZrSymbolId prepare_distinct_nominal_arguments(void) {
    SZrAstNodeArray *nodes = parse_source(
            "union Parcel<T> { Item(value: T); }"
            "union Widget { Root; }"
            "union Widget { Generated; }");
    SZrInferredType argument;
    TZrSymbolId markedToken;

    TEST_ASSERT_EQUAL_UINT32(0U, compile_union(nodes->nodes[0], ZrParser_SyntaxContext_Root()));
    TEST_ASSERT_EQUAL_UINT32(0U, compile_union(nodes->nodes[1], ZrParser_SyntaxContext_Root()));
    markedToken = compile_union(nodes->nodes[2], g_definition);
    TEST_ASSERT_NOT_EQUAL(0U, markedToken);
    ZrParser_InferredType_InitFull(g_state, &argument, ZR_VALUE_TYPE_OBJECT, ZR_FALSE,
            nodes->nodes[1]->data.unionDeclaration.name->name);
    init_instance(&g_types[0], "Parcel<Widget>", 0U, &argument);
    argument.nominalDefinitionToken = markedToken;
    init_instance(&g_types[1], "Parcel<Widget>", 0U, &argument);
    ZrParser_InferredType_Free(g_state, &argument);
    TEST_ASSERT_NOT_EQUAL(
            ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, &g_types[0]),
            ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, &g_types[1]));
    return markedToken;
}

static void test_same_spelling_nominal_arguments_do_not_share_specialization(void) {
    TZrSymbolId markedToken = prepare_distinct_nominal_arguments();

    assert_payload(materialize(&g_types[0]), "Item", ZR_VALUE_TYPE_OBJECT, 0U);
    assert_payload(materialize(&g_types[1]), "Item", ZR_VALUE_TYPE_OBJECT, markedToken);
    assert_payload(type_inference_find_nominal_prototype(&g_compiler, &g_types[0]),
            "Item", ZR_VALUE_TYPE_OBJECT, 0U);
    TEST_ASSERT_EQUAL_UINT(5U, g_compiler.typePrototypes.length);
}

static void test_reverse_nominal_argument_materialization_is_isolated(void) {
    TZrSymbolId markedToken = prepare_distinct_nominal_arguments();

    assert_payload(materialize(&g_types[1]), "Item", ZR_VALUE_TYPE_OBJECT, markedToken);
    assert_payload(materialize(&g_types[0]), "Item", ZR_VALUE_TYPE_OBJECT, 0U);
    assert_payload(type_inference_find_nominal_prototype(&g_compiler, &g_types[1]),
            "Item", ZR_VALUE_TYPE_OBJECT, markedToken);
    TEST_ASSERT_EQUAL_UINT(5U, g_compiler.typePrototypes.length);
}

static void test_name_only_lookup_uses_root_nominal_argument(void) {
    TZrSymbolId markedToken = prepare_distinct_nominal_arguments();

    assert_payload(materialize(&g_types[1]), "Item", ZR_VALUE_TYPE_OBJECT, markedToken);
    TEST_ASSERT_TRUE(ensure_generic_instance_type_prototype(&g_compiler, g_types[0].typeName));
    assert_payload(find_compiler_type_prototype_inference(&g_compiler, g_types[0].typeName),
            "Item", ZR_VALUE_TYPE_OBJECT, 0U);
}

static void test_invalid_definition_identity_cannot_reuse_root_cache(void) {
    SZrAstNode *node = parse_source("union Parcel<T> { Item(value: T); }")->nodes[0];

    init_int_instance(&g_types[0], compile_union(node, ZrParser_SyntaxContext_Root()));
    TEST_ASSERT_NOT_NULL(materialize(&g_types[0]));
    init_int_instance(&g_types[1], (TZrSymbolId)~0U);
    TEST_ASSERT_FALSE(ensure_generic_instance_type_prototype_from_inferred(&g_compiler, &g_types[1]));
    TEST_ASSERT_NULL(type_inference_find_nominal_prototype(&g_compiler, &g_types[1]));
    TEST_ASSERT_EQUAL_UINT(2U, g_compiler.typePrototypes.length);
}

static void test_nested_nominal_argument_identity_is_preserved(void) {
    SZrAstNodeArray *nodes = parse_source(
            "union Parcel<T> { Item(value: T); }"
            "union Layer<T> { Inner(value: T); }"
            "union Widget { Root; }"
            "union Widget { Generated; }");
    SZrInferredType argument;
    TZrSymbolId markedToken;
    SZrTypePrototypeInfo *prototype;
    SZrTypeMemberInfo *member;
    SZrInferredType *payload;
    SZrInferredType *nestedArgument;

    compile_union(nodes->nodes[0], ZrParser_SyntaxContext_Root());
    compile_union(nodes->nodes[1], ZrParser_SyntaxContext_Root());
    compile_union(nodes->nodes[2], ZrParser_SyntaxContext_Root());
    markedToken = compile_union(nodes->nodes[3], g_definition);
    ZrParser_InferredType_InitFull(g_state, &argument, ZR_VALUE_TYPE_OBJECT, ZR_FALSE,
            nodes->nodes[2]->data.unionDeclaration.name->name);
    init_instance(&g_types[0], "Layer<Widget>", 0U, &argument);
    argument.nominalDefinitionToken = markedToken;
    init_instance(&g_types[1], "Layer<Widget>", 0U, &argument);
    ZrParser_InferredType_Free(g_state, &argument);
    init_instance(&g_types[2], "Parcel<Layer<Widget>>", 0U, &g_types[0]);
    init_instance(&g_types[3], "Parcel<Layer<Widget>>", 0U, &g_types[1]);
    TEST_ASSERT_NOT_NULL(materialize(&g_types[0]));
    TEST_ASSERT_NOT_NULL(materialize(&g_types[1]));
    TEST_ASSERT_NOT_NULL(materialize(&g_types[2]));
    prototype = materialize(&g_types[3]);
    assert_payload(prototype, "Item", ZR_VALUE_TYPE_OBJECT, 0U);
    member = ZrCore_Array_Get(&prototype->members, 0U);
    payload = ZrCore_Array_Get(&member->parameterTypes, 0U);
    TEST_ASSERT_EQUAL_UINT(1U, payload->elementTypes.length);
    nestedArgument = ZrCore_Array_Get(&payload->elementTypes, 0U);
    TEST_ASSERT_NOT_NULL(nestedArgument);
    TEST_ASSERT_EQUAL_UINT32(markedToken, nestedArgument->nominalDefinitionToken);
    TEST_ASSERT_EQUAL_UINT(8U, g_compiler.typePrototypes.length);
}

static void test_const_generic_values_have_distinct_cache_entries(void) {
    SZrAstNode *node = parse_source("union Parcel<const N: int> { Item; }")->nodes[0];
    SZrInferredType argument;
    TZrSymbolId token = compile_union(node, g_definition);

    ZrParser_InferredType_InitConstIntGenericArgument(g_state, &argument, 1,
            ZrCore_String_CreateFromNative(g_state, "1"));
    init_instance(&g_types[0], "Parcel<1>", token, &argument);
    ZrParser_InferredType_Free(g_state, &argument);
    ZrParser_InferredType_InitConstIntGenericArgument(g_state, &argument, 2,
            ZrCore_String_CreateFromNative(g_state, "2"));
    init_instance(&g_types[1], "Parcel<2>", token, &argument);
    ZrParser_InferredType_Free(g_state, &argument);
    TEST_ASSERT_NOT_NULL(materialize(&g_types[0]));
    TEST_ASSERT_NOT_NULL(materialize(&g_types[1]));
    TEST_ASSERT_EQUAL_UINT(3U, g_compiler.typePrototypes.length);
    TEST_ASSERT_NOT_EQUAL(
            type_inference_find_nominal_prototype(&g_compiler, &g_types[0]),
            type_inference_find_nominal_prototype(&g_compiler, &g_types[1]));
}

static void test_generic_annotation_preserves_selected_protocols(void) {
    SZrAstNodeArray *nodes = parse_source(
            "union Parcel<T> { Root(value: T); }"
            "union Parcel<T> { Generated(value: T); }"
            "var result: Parcel<int>;");
    SZrType *annotation;
    SZrTypePrototypeInfo *open;
    TZrSymbolId token;

    compile_union(nodes->nodes[0], ZrParser_SyntaxContext_Root());
    token = compile_union(nodes->nodes[1], g_definition);
    open = ZrCore_Array_Get(&g_compiler.typePrototypes, 0U);
    TEST_ASSERT_NOT_NULL(open);
    open->protocolMask = ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE);
    open = ZrCore_Array_Get(&g_compiler.typePrototypes, 1U);
    TEST_ASSERT_NOT_NULL(open);
    open->protocolMask = ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_HASHABLE);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, nodes->nodes[2]->type);
    annotation = nodes->nodes[2]->data.variableDeclaration.typeInfo;
    TEST_ASSERT_NOT_NULL(annotation);
    TEST_ASSERT_EQUAL_INT(ZR_AST_GENERIC_TYPE, annotation->name->type);
    annotation->name->data.genericType.name->syntaxContext = g_definition;
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(&g_compiler, annotation, &g_types[0]));
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_UINT32(token, g_types[0].nominalDefinitionToken);
    TEST_ASSERT_EQUAL_UINT64(ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_HASHABLE), g_types[0].protocolMask);
    assert_payload(type_inference_find_nominal_prototype(&g_compiler, &g_types[0]),
            "Generated", ZR_VALUE_TYPE_INT64, 0U);
}

static void test_specialization_identity_survives_prototype_growth(void) {
    SZrAstNode *node = parse_source("union Parcel<const N: int> { Item; }")->nodes[0];
    TZrSymbolId token = compile_union(node, g_definition);
    SZrInferredType argument;
    TZrChar displayName[64];
    TZrChar argumentName[32];

    for (TZrInt64 value = 0; value < 64; value++) {
        snprintf(displayName, sizeof(displayName), "Parcel<%lld>", (long long)value);
        snprintf(argumentName, sizeof(argumentName), "%lld", (long long)value);
        ZrParser_InferredType_InitConstIntGenericArgument(g_state, &argument, value,
                ZrCore_String_CreateFromNative(g_state, argumentName));
        init_instance(&g_types[value == 0 ? 0U : 1U], displayName, token, &argument);
        ZrParser_InferredType_Free(g_state, &argument);
        TEST_ASSERT_NOT_NULL(materialize(&g_types[value == 0 ? 0U : 1U]));
    }
    TEST_ASSERT_EQUAL_UINT(65U, g_compiler.typePrototypes.length);
    TEST_ASSERT_NOT_NULL(materialize(&g_types[0]));
    TEST_ASSERT_EQUAL_UINT(65U, g_compiler.typePrototypes.length);
    TEST_ASSERT_EQUAL_PTR(node,
            type_inference_find_nominal_prototype(&g_compiler, &g_types[0])->declarationNode);
}

static void assert_extern_struct_constraint(TZrNativeString source, SZrSyntaxContext context) {
    SZrAstNodeArray *nodes = parse_source(source);
    SZrAstNode *declaration;
    const SZrNamedTypeBinding *binding;
    const SZrCanonicalTypeNode *nominal;
    SZrInferredType argument;
    TZrSymbolId argumentToken;

    TEST_ASSERT_EQUAL_UINT(2U, nodes->count);
    compile_union(nodes->nodes[0], ZrParser_SyntaxContext_Root());
    TEST_ASSERT_EQUAL_INT(ZR_AST_EXTERN_BLOCK, nodes->nodes[1]->type);
    declaration = nodes->nodes[1]->data.externBlock.declarations->nodes[0];
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_DECLARATION, declaration->type);
    declaration->data.structDeclaration.name->syntaxContext = context;
    extern_compiler_register_struct_prototype(&g_compiler, declaration);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    binding = ZrParser_TypeEnvironment_FindTypeBindingWithContext(g_compiler.typeEnv,
            declaration->data.structDeclaration.name->name, context);
    TEST_ASSERT_NOT_NULL(binding);
    nominal = ZrParser_CanonicalType_Find(g_compiler.semanticContext, binding->typeId);
    TEST_ASSERT_NOT_NULL(nominal);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_NOMINAL, nominal->kind);
    argumentToken = nominal->data.nominal.definitionToken;
    ZrParser_InferredType_InitFull(g_state, &argument, ZR_VALUE_TYPE_OBJECT, ZR_FALSE,
            declaration->data.structDeclaration.name->name);
    argument.nominalDefinitionToken = argumentToken;
    init_instance(&g_types[0], "Parcel<Widget>", 0U, &argument);
    ZrParser_InferredType_Free(g_state, &argument);
    assert_payload(materialize(&g_types[0]), "Item", ZR_VALUE_TYPE_OBJECT, argumentToken);
}

static void test_root_struct_constraint_prerequisite(void) {
    assert_extern_struct_constraint(
            "union Parcel<T> where T: struct { Item(value: T); }"
            "native extern(\"fixture\") { struct Widget {} }", ZrParser_SyntaxContext_Root());
}

static void test_marked_argument_satisfies_struct_constraint(void) {
    assert_extern_struct_constraint(
            "union Parcel<T> where T: struct { Item(value: T); }"
            "native extern(\"fixture\") { struct Widget {} }", g_definition);
}

static void test_marked_argument_satisfies_new_constraint(void) {
    assert_extern_struct_constraint(
            "union Parcel<T> where T: new() { Item(value: T); }"
            "native extern(\"fixture\") { struct Widget {} }", g_definition);
}

static TZrSymbolId register_const_owner(SZrAstNode *node) {
    TZrSymbolId symbolId;

    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, node->type);
    TEST_ASSERT_NOT_NULL(node->data.functionDeclaration.generic);
    symbolId = ZrParser_Semantic_RegisterSymbol(g_compiler.semanticContext,
            node->data.functionDeclaration.name->name, ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
            ZR_SEMANTIC_ID_INVALID, ZR_SEMANTIC_ID_INVALID, node, node->location);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, symbolId);
    TEST_ASSERT_EQUAL_PTR(node,
            ZrParser_Semantic_FindSymbolById(g_compiler.semanticContext, symbolId)->astNode);
    return symbolId;
}

static SZrAstNodeArray *prepare_const_owners(void) {
    SZrAstNodeArray *nodes = parse_source(
            "union Parcel<T> { Item(value: T); }"
            "union Layer<const N: int> { Inner; }"
            "fn first<T, const N: int>() {}"
            "fn second<T, const N: int>() {}");
    SZrInferredType argument;

    TEST_ASSERT_EQUAL_UINT(4U, nodes->count);
    compile_union(nodes->nodes[0], ZrParser_SyntaxContext_Root());
    compile_union(nodes->nodes[1], ZrParser_SyntaxContext_Root());
    register_const_owner(nodes->nodes[2]);
    register_const_owner(nodes->nodes[3]);
    ZrParser_InferredType_InitConstParameterGenericArgument(g_state, &argument,
            nodes->nodes[2]->data.functionDeclaration.generic->params->nodes[1]->data.parameter.name->name);
    init_instance(&g_types[0], "Layer<N>", 0U, &argument);
    ZrParser_InferredType_Free(g_state, &argument);
    init_instance(&g_types[1], "Parcel<Layer<N>>", 0U, &g_types[0]);
    return nodes;
}

static void activate_const_owner(SZrAstNode *owner) {
    if (g_ownerEnvironment != ZR_NULL) {
        g_compiler.typeEnv = g_rootEnvironment;
        ZrParser_TypeEnvironment_Free(g_state, g_ownerEnvironment);
        g_ownerEnvironment = ZR_NULL;
    }
    g_ownerEnvironment = ZrParser_TypeEnvironment_New(g_state);
    TEST_ASSERT_NOT_NULL(g_ownerEnvironment);
    g_ownerEnvironment->parent = g_rootEnvironment;
    g_ownerEnvironment->semanticContext = g_compiler.semanticContext;
    g_compiler.typeEnv = g_ownerEnvironment;
    g_compiler.currentFunctionNode = owner;
}

static TZrTypeId assert_open_const_key(SZrTypePrototypeInfo *prototype, TZrSymbolId ownerId) {
    const SZrCanonicalTypeNode *instance;
    const SZrCanonicalGenericArgument *argument;
    TZrTypeId typeId;

    TEST_ASSERT_NOT_NULL(prototype);
    typeId = prototype->genericInstanceTypeId;
    instance = ZrParser_CanonicalType_Find(g_compiler.semanticContext, typeId);
    TEST_ASSERT_NOT_NULL(instance);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_GENERIC_INSTANCE, instance->kind);
    TEST_ASSERT_EQUAL_UINT(1U, instance->data.genericInstance.arguments.length);
    argument = (const SZrCanonicalGenericArgument *)instance->data.genericInstance.arguments.head;
    TEST_ASSERT_NOT_NULL(argument);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_GENERIC_ARGUMENT_CONST_PARAMETER, argument->kind);
    TEST_ASSERT_EQUAL_UINT32(ownerId, argument->data.constParameter.ownerSymbolId);
    TEST_ASSERT_EQUAL_UINT32(1U, argument->data.constParameter.ordinal);
    return typeId;
}

static void test_direct_const_forwarding_uses_owner_and_ordinal(void) {
    SZrAstNodeArray *nodes = prepare_const_owners();
    const SZrSemanticSymbolRecord *owner;
    TZrSymbolId ownerId;

    activate_const_owner(nodes->nodes[2]);
    owner = ZrParser_Semantic_FindSymbolByNameAndKind(g_compiler.semanticContext,
            nodes->nodes[2]->data.functionDeclaration.name->name, ZR_SEMANTIC_SYMBOL_KIND_FUNCTION);
    TEST_ASSERT_NOT_NULL(owner);
    ownerId = owner->id;
    assert_open_const_key(materialize(&g_types[0]), ownerId);
}

static void test_same_spelling_const_owners_have_distinct_direct_cache_keys(void) {
    SZrAstNodeArray *nodes = prepare_const_owners();
    TZrTypeId firstKey;
    TZrTypeId secondKey;

    activate_const_owner(nodes->nodes[2]);
    firstKey = materialize(&g_types[0])->genericInstanceTypeId;
    activate_const_owner(nodes->nodes[3]);
    secondKey = materialize(&g_types[0])->genericInstanceTypeId;
    TEST_ASSERT_NOT_EQUAL(firstKey, secondKey);
    activate_const_owner(nodes->nodes[2]);
    TEST_ASSERT_EQUAL_UINT32(firstKey, materialize(&g_types[0])->genericInstanceTypeId);
    TEST_ASSERT_EQUAL_UINT(4U, g_compiler.typePrototypes.length);
}

static void test_nested_const_forwarding_preserves_owner_identity(void) {
    SZrAstNodeArray *nodes = prepare_const_owners();
    SZrTypePrototypeInfo *prototype;
    const SZrCanonicalTypeNode *instance;
    const SZrCanonicalGenericArgument *argument;
    TZrTypeId innerKey;

    activate_const_owner(nodes->nodes[2]);
    innerKey = materialize(&g_types[0])->genericInstanceTypeId;
    prototype = materialize(&g_types[1]);
    TEST_ASSERT_NOT_NULL(prototype);
    instance = ZrParser_CanonicalType_Find(g_compiler.semanticContext, prototype->genericInstanceTypeId);
    TEST_ASSERT_NOT_NULL(instance);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_GENERIC_INSTANCE, instance->kind);
    TEST_ASSERT_EQUAL_UINT(1U, instance->data.genericInstance.arguments.length);
    argument = (const SZrCanonicalGenericArgument *)instance->data.genericInstance.arguments.head;
    TEST_ASSERT_NOT_NULL(argument);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_GENERIC_ARGUMENT_TYPE, argument->kind);
    TEST_ASSERT_EQUAL_UINT32(innerKey, argument->data.typeId);
}

static void test_nested_const_forwarding_isolated_between_owners(void) {
    SZrAstNodeArray *nodes = prepare_const_owners();
    TZrTypeId firstKey;

    activate_const_owner(nodes->nodes[2]);
    TEST_ASSERT_NOT_NULL(materialize(&g_types[0]));
    firstKey = materialize(&g_types[1])->genericInstanceTypeId;
    activate_const_owner(nodes->nodes[3]);
    TEST_ASSERT_NOT_NULL(materialize(&g_types[0]));
    TEST_ASSERT_NOT_EQUAL(firstKey, materialize(&g_types[1])->genericInstanceTypeId);
    activate_const_owner(nodes->nodes[2]);
    TEST_ASSERT_EQUAL_UINT32(firstKey, materialize(&g_types[1])->genericInstanceTypeId);
    TEST_ASSERT_EQUAL_UINT(6U, g_compiler.typePrototypes.length);
}

static void test_open_const_instance_does_not_leave_a_root_name_binding(void) {
    SZrAstNodeArray *nodes = prepare_const_owners();

    activate_const_owner(nodes->nodes[2]);
    TEST_ASSERT_NOT_NULL(materialize(&g_types[0]));
    TEST_ASSERT_FALSE(ZrParser_TypeEnvironment_LookupType(g_rootEnvironment, g_types[0].typeName));
    g_compiler.typeEnv = g_rootEnvironment;
    g_compiler.currentFunctionNode = ZR_NULL;
    TEST_ASSERT_NULL(find_compiler_type_prototype_inference(&g_compiler, g_types[0].typeName));
    TEST_ASSERT_FALSE(ZrParser_TypeEnvironment_LookupType(g_compiler.typeEnv, g_types[0].typeName));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_root_generic_materialization_prerequisite);
    RUN_TEST(test_definition_only_generic_materializes_selected_declaration);
    RUN_TEST(test_root_then_definition_cache_isolation);
    RUN_TEST(test_definition_then_root_cache_isolation);
    RUN_TEST(test_reverse_materialization_cache_isolation);
    RUN_TEST(test_two_opaque_definitions_cache_isolation);
    RUN_TEST(test_definition_specialization_is_not_root_visible);
    RUN_TEST(test_repeated_structured_specialization_reuses_cache);
    RUN_TEST(test_mixed_site_item_generic_uses_root_definition);
    RUN_TEST(test_same_spelling_nominal_arguments_do_not_share_specialization);
    RUN_TEST(test_reverse_nominal_argument_materialization_is_isolated);
    RUN_TEST(test_name_only_lookup_uses_root_nominal_argument);
    RUN_TEST(test_invalid_definition_identity_cannot_reuse_root_cache);
    RUN_TEST(test_nested_nominal_argument_identity_is_preserved);
    RUN_TEST(test_const_generic_values_have_distinct_cache_entries);
    RUN_TEST(test_generic_annotation_preserves_selected_protocols);
    RUN_TEST(test_specialization_identity_survives_prototype_growth);
    RUN_TEST(test_root_struct_constraint_prerequisite);
    RUN_TEST(test_marked_argument_satisfies_struct_constraint);
    RUN_TEST(test_marked_argument_satisfies_new_constraint);
    RUN_TEST(test_direct_const_forwarding_uses_owner_and_ordinal);
    RUN_TEST(test_same_spelling_const_owners_have_distinct_direct_cache_keys);
    RUN_TEST(test_nested_const_forwarding_preserves_owner_identity);
    RUN_TEST(test_nested_const_forwarding_isolated_between_owners);
    RUN_TEST(test_open_const_instance_does_not_leave_a_root_name_binding);
    return UNITY_END();
}
