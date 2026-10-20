#include "unity.h"

#include <string.h>

#include "compiler/compiler_internal.h"
#include "compiler/compiler_type_prototype_lookup.h"
#include "compiler/compiler_type_value_alias.h"
#include "harness/runtime_support.h"
#include "type_inference/type_inference_nominal_prototype.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_query.h"
#include "zr_vm_parser/semantic_relations.h"
#include "zr_vm_parser/semantic_type_use.h"
#include "zr_vm_parser/type_inference.h"
#include "type_inference/type_inference_internal.h"

static SZrState *g_state;
static SZrCompilerState g_compiler;
static TZrBool g_compilerInitialized;
static SZrAstNode *g_ast;
static SZrSyntaxContextTable *g_table;
static SZrSyntaxContext g_firstDefinition;
static SZrSyntaxContext g_secondDefinition;
static SZrSyntaxContext g_mixed;
static SZrInferredType g_baseType;
static SZrInferredType g_rootType;
static SZrArray g_relations;

void setUp(void) {
    SZrSyntaxExpansionInfo expansion = {0};
    TZrSyntaxExpansionId expansionId;
    SZrScope scope = {0};

    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
    ZrParser_InferredType_Init(g_state, &g_baseType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &g_rootType, ZR_VALUE_TYPE_OBJECT);
    ZrCore_Array_Construct(&g_relations);
    g_table = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_table);
    expansion.stableIdentity = 0xB601U;
    expansion.callSiteSpan = ZrParser_TokenSpan_None();
    expansion.definitionSiteSpan = ZrParser_TokenSpan_None();
    TEST_ASSERT_TRUE(ZrParser_SyntaxContextTable_RegisterExpansion(
            g_table, &expansion, &expansionId));
    g_firstDefinition = ZrParser_SyntaxContext_DefSite(g_table, expansionId);
    g_mixed = ZrParser_SyntaxContext_MixedSite(g_table, expansionId);
    expansion.stableIdentity = 0xB602U;
    TEST_ASSERT_TRUE(ZrParser_SyntaxContextTable_RegisterExpansion(
            g_table, &expansion, &expansionId));
    g_secondDefinition = ZrParser_SyntaxContext_DefSite(g_table, expansionId);
    TEST_ASSERT_FALSE(ZrParser_SyntaxContext_Equal(g_firstDefinition, g_secondDefinition));
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
    if (g_state == ZR_NULL) {
        return;
    }
    ZrParser_InferredType_Free(g_state, &g_baseType);
    ZrParser_InferredType_Free(g_state, &g_rootType);
    ZrCore_Array_Free(g_state, &g_relations);
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
    ZrTests_Runtime_State_Destroy(g_state);
    g_state = ZR_NULL;
}

static SZrAstNodeArray *parse_source_with_name(const TZrChar *source, SZrString *sourceName) {
    SZrAstNodeArray *nodes;

    g_ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(g_ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_ast->type);
    g_compiler.scriptAst = g_ast;
    nodes = g_ast->data.script.statements;
    TEST_ASSERT_NOT_NULL(nodes);
    return nodes;
}

static SZrAstNodeArray *parse_source(const TZrChar *source) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(g_state, "base_publication.zr");

    TEST_ASSERT_NOT_NULL(sourceName);
    return parse_source_with_name(source, sourceName);
}

static SZrAstNodeArray *parse_classes(TZrBool twoBases) {
    SZrAstNodeArray *nodes = parse_source(twoBases
            ? "class Base {} class Base {} class Derived : Base {}"
            : "class Base {} class Derived : Base {}");

    TEST_ASSERT_EQUAL_UINT(twoBases ? 3U : 2U, nodes->count);
    for (TZrSize index = 0U; index < nodes->count; index++) {
        TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, nodes->nodes[index]->type);
        TEST_ASSERT_NOT_NULL(nodes->nodes[index]->data.classDeclaration.name);
        TEST_ASSERT_NOT_NULL(nodes->nodes[index]->data.classDeclaration.members);
        TEST_ASSERT_EQUAL_UINT(0U, nodes->nodes[index]->data.classDeclaration.members->count);
    }
    return nodes;
}

static SZrType *base_annotation(SZrAstNode *derived) {
    SZrAstNodeArray *inherits = derived->data.classDeclaration.inherits;
    SZrAstNode *annotation;

    TEST_ASSERT_NOT_NULL(inherits);
    TEST_ASSERT_EQUAL_UINT(1U, inherits->count);
    annotation = inherits->nodes[0];
    TEST_ASSERT_NOT_NULL(annotation);
    TEST_ASSERT_EQUAL_INT(ZR_AST_TYPE, annotation->type);
    TEST_ASSERT_NOT_NULL(annotation->data.type.name);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, annotation->data.type.name->type);
    TEST_ASSERT_EQUAL_STRING("Base", ZrCore_String_GetNativeString(
            annotation->data.type.name->data.identifier.name));
    return &annotation->data.type;
}

static void assert_symbol(SZrNamedTypeBinding binding, SZrAstNode *declaration) {
    const SZrSemanticSymbolRecord *symbol = ZrParser_Semantic_FindSymbolById(
            g_compiler.semanticContext, binding.symbolId);
    const SZrCanonicalTypeNode *nominal = ZrParser_CanonicalType_Find(
            g_compiler.semanticContext, binding.typeId);

    TEST_ASSERT_NOT_NULL(symbol);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_SYMBOL_KIND_TYPE, symbol->kind);
    TEST_ASSERT_EQUAL_PTR(declaration, symbol->astNode);
    TEST_ASSERT_EQUAL_UINT32(binding.typeId, symbol->typeId);
    TEST_ASSERT_NOT_NULL(nominal);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_NOMINAL, nominal->kind);
}

static SZrNamedTypeBinding publish_class(SZrAstNode *declaration) {
    const SZrNamedTypeBinding *binding;

    ZrParser_Compiler_CompileClassDeclaration(&g_compiler, declaration);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    binding = ZrParser_TypeEnvironment_FindTypeBindingWithContext(g_compiler.typeEnv,
            declaration->data.classDeclaration.name->name,
            declaration->data.classDeclaration.name->syntaxContext);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_PTR(declaration, binding->declarationNode);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, binding->symbolId);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, binding->typeId);
    assert_symbol(*binding, declaration);
    return *binding;
}

static void assert_published_base(SZrNamedTypeBinding derivedBinding,
        SZrAstNode *derived, SZrNamedTypeBinding baseBinding, SZrAstNode *base) {
    const SZrParserSemanticRelationQuery *relation;

    assert_symbol(baseBinding, base);
    assert_symbol(derivedBinding, derived);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_BaseTypesOf(
            g_compiler.semanticContext, derivedBinding.typeId, &g_relations));
    TEST_ASSERT_EQUAL_UINT(1U, g_relations.length);
    relation = ZrCore_Array_Get(&g_relations, 0U);
    TEST_ASSERT_NOT_NULL(relation);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_RELATION_BASE_TYPE, relation->kind);
    TEST_ASSERT_EQUAL_UINT32(derivedBinding.symbolId, relation->sourceSymbolId);
    TEST_ASSERT_EQUAL_UINT32(derivedBinding.typeId, relation->sourceTypeId);
    TEST_ASSERT_EQUAL_UINT32(baseBinding.symbolId, relation->targetSymbolId);
    TEST_ASSERT_EQUAL_UINT32(baseBinding.typeId, relation->targetTypeId);
}

static void assert_base_relation(SZrSyntaxContext first, SZrSyntaxContext second,
        TZrBool twoBases, SZrSyntaxContext annotationContext,
        SZrSyntaxContext derivedContext, TZrSize selectedIndex) {
    SZrAstNodeArray *nodes = parse_classes(twoBases);
    TZrSize baseCount = twoBases ? 2U : 1U;
    SZrSyntaxContext contexts[2] = {first, second};
    SZrNamedTypeBinding bindings[2];
    SZrNamedTypeBinding derivedBinding;
    SZrAstNode *derived = nodes->nodes[baseCount];
    SZrType *annotation = base_annotation(derived);
    SZrTypePrototypeInfo *selectedPrototype;
    SZrSyntaxContext normalized;

    TEST_ASSERT_LESS_THAN_UINT(baseCount, selectedIndex);
    for (TZrSize index = 0U; index < baseCount; index++) {
        nodes->nodes[index]->data.classDeclaration.name->syntaxContext = contexts[index];
    }
    derived->data.classDeclaration.name->syntaxContext = derivedContext;
    annotation->name->data.identifier.syntaxContext = annotationContext;
    for (TZrSize index = 0U; index < baseCount; index++) {
        bindings[index] = publish_class(nodes->nodes[index]);
    }
    if (twoBases) {
        TEST_ASSERT_NOT_EQUAL(bindings[0].symbolId, bindings[1].symbolId);
        TEST_ASSERT_NOT_EQUAL(bindings[0].typeId, bindings[1].typeId);
    }
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, annotation, &g_baseType));
    TEST_ASSERT_FALSE(g_compiler.hasError);
    normalized = ZrParser_SyntaxContext_Normalize(annotationContext, ZR_SYNTAX_NAMESPACE_ITEM);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(normalized));
    TEST_ASSERT_EQUAL_UINT32(ZrParser_SyntaxContext_Equal(
            normalized, ZrParser_SyntaxContext_Root()) ? 0U : bindings[selectedIndex].symbolId,
            g_baseType.nominalDefinitionToken);
    TEST_ASSERT_EQUAL_UINT32(bindings[selectedIndex].typeId,
            ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, &g_baseType));
    selectedPrototype = type_inference_find_nominal_prototype(&g_compiler, &g_baseType);
    TEST_ASSERT_NOT_NULL(selectedPrototype);
    TEST_ASSERT_EQUAL_PTR(nodes->nodes[selectedIndex], selectedPrototype->declarationNode);
    TEST_ASSERT_EQUAL_UINT(0U, selectedPrototype->members.length);

    derivedBinding = publish_class(derived);
    assert_published_base(derivedBinding, derived, bindings[selectedIndex], nodes->nodes[selectedIndex]);
}

static void test_root_only_base_relation(void) {
    assert_base_relation(ZrParser_SyntaxContext_Root(), ZrParser_SyntaxContext_Root(),
            ZR_FALSE, ZrParser_SyntaxContext_Root(), ZrParser_SyntaxContext_Root(), 0U);
}
static void test_opaque_only_base_relation(void) {
    assert_base_relation(g_firstDefinition, ZrParser_SyntaxContext_Root(),
            ZR_FALSE, g_firstDefinition, ZrParser_SyntaxContext_Root(), 0U);
}
static void test_root_then_opaque_selects_root(void) {
    assert_base_relation(ZrParser_SyntaxContext_Root(), g_firstDefinition,
            ZR_TRUE, ZrParser_SyntaxContext_Root(), ZrParser_SyntaxContext_Root(), 0U);
}
static void test_opaque_then_root_selects_root(void) {
    assert_base_relation(g_firstDefinition, ZrParser_SyntaxContext_Root(),
            ZR_TRUE, ZrParser_SyntaxContext_Root(), ZrParser_SyntaxContext_Root(), 1U);
}
static void test_root_then_opaque_selects_opaque(void) {
    assert_base_relation(ZrParser_SyntaxContext_Root(), g_firstDefinition,
            ZR_TRUE, g_firstDefinition, ZrParser_SyntaxContext_Root(), 1U);
}
static void test_opaque_then_root_selects_opaque(void) {
    assert_base_relation(g_firstDefinition, ZrParser_SyntaxContext_Root(),
            ZR_TRUE, g_firstDefinition, ZrParser_SyntaxContext_Root(), 0U);
}
static void test_mixed_site_selects_root_item(void) {
    assert_base_relation(ZrParser_SyntaxContext_Root(), g_firstDefinition,
            ZR_TRUE, g_mixed, ZrParser_SyntaxContext_Root(), 0U);
}
static void test_two_opaque_bases_select_second_identity(void) {
    assert_base_relation(g_firstDefinition, g_secondDefinition,
            ZR_TRUE, g_secondDefinition, ZrParser_SyntaxContext_Root(), 1U);
}
static void test_derived_and_base_have_independent_definition_sites(void) {
    assert_base_relation(g_firstDefinition, ZrParser_SyntaxContext_Root(),
            ZR_FALSE, g_firstDefinition, g_secondDefinition, 0U);
}

static void test_root_forward_base_preserves_deferred_relation(void) {
    SZrAstNodeArray *nodes = parse_source("class Derived : Base {} class Base {}");
    SZrNamedTypeBinding derivedBinding;

    TEST_ASSERT_EQUAL_UINT(2U, nodes->count);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, nodes->nodes[0]->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, nodes->nodes[1]->type);
    (void)base_annotation(nodes->nodes[0]);
    derivedBinding = publish_class(nodes->nodes[0]);
    TEST_ASSERT_EQUAL_UINT(1U, g_compiler.typePrototypes.length);
    (void)ZrParser_SemanticQuery_BaseTypesOf(
            g_compiler.semanticContext, derivedBinding.typeId, &g_relations);
    TEST_ASSERT_EQUAL_UINT(0U, g_relations.length);
    (void)publish_class(nodes->nodes[1]);
}

static void test_root_generic_forward_base_preserves_success(void) {
    SZrAstNodeArray *nodes = parse_source("class Derived : Base<int> {} class Base<T> {}");
    SZrAstNode *inheritance;
    SZrTypePrototypeInfo *prototype = ZR_NULL;

    TEST_ASSERT_EQUAL_UINT(2U, nodes->count);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, nodes->nodes[0]->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, nodes->nodes[1]->type);
    TEST_ASSERT_NOT_NULL(nodes->nodes[0]->data.classDeclaration.inherits);
    TEST_ASSERT_EQUAL_UINT(1U, nodes->nodes[0]->data.classDeclaration.inherits->count);
    inheritance = nodes->nodes[0]->data.classDeclaration.inherits->nodes[0];
    TEST_ASSERT_EQUAL_INT(ZR_AST_TYPE, inheritance->type);
    TEST_ASSERT_NOT_NULL(inheritance->data.type.name);
    TEST_ASSERT_EQUAL_INT(ZR_AST_GENERIC_TYPE, inheritance->data.type.name->type);
    TEST_ASSERT_TRUE(compiler_resolve_type_annotation_prototype(&g_compiler,
            &inheritance->data.type, extract_type_name_string(&g_compiler, &inheritance->data.type),
            &prototype));
    TEST_ASSERT_FALSE(g_compiler.hasError);
    (void)publish_class(nodes->nodes[0]);
    (void)publish_class(nodes->nodes[1]);
}

static SZrIdentifier *alias_identifier(SZrAstNode *alias) {
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, alias->type);
    TEST_ASSERT_NOT_NULL(alias->data.variableDeclaration.pattern);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, alias->data.variableDeclaration.pattern->type);
    return &alias->data.variableDeclaration.pattern->data.identifier;
}

static SZrType *chosen_annotation(SZrAstNode *derived) {
    SZrAstNode *annotation;

    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, derived->type);
    TEST_ASSERT_NOT_NULL(derived->data.classDeclaration.inherits);
    TEST_ASSERT_EQUAL_UINT(1U, derived->data.classDeclaration.inherits->count);
    annotation = derived->data.classDeclaration.inherits->nodes[0];
    TEST_ASSERT_EQUAL_INT(ZR_AST_TYPE, annotation->type);
    TEST_ASSERT_NOT_NULL(annotation->data.type.name);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, annotation->data.type.name->type);
    TEST_ASSERT_EQUAL_STRING("Chosen", ZrCore_String_GetNativeString(
            annotation->data.type.name->data.identifier.name));
    return &annotation->data.type;
}

static void test_opaque_alias_can_target_root_nominal_token_zero(void) {
    SZrAstNodeArray *nodes = parse_source("class Base {} class Evidence : Base {}"
            "let Chosen = Base; class Derived : Chosen {}");
    SZrNamedTypeBinding baseBinding;
    SZrNamedTypeBinding derivedBinding;
    SZrTypeBinding *aliasBinding;
    SZrIdentifier *alias;
    SZrType *annotation;
    SZrTypePrototypeInfo *prototype;

    TEST_ASSERT_EQUAL_UINT(4U, nodes->count);
    baseBinding = publish_class(nodes->nodes[0]);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, nodes->nodes[1]->type);
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, base_annotation(nodes->nodes[1]), &g_rootType));
    TEST_ASSERT_FALSE(g_compiler.hasError);
    alias = alias_identifier(nodes->nodes[2]);
    TEST_ASSERT_NOT_NULL(nodes->nodes[2]->data.variableDeclaration.value);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, nodes->nodes[2]->data.variableDeclaration.value->type);
    TEST_ASSERT_EQUAL_STRING("Base", ZrCore_String_GetNativeString(
            nodes->nodes[2]->data.variableDeclaration.value->data.identifier.name));
    alias->syntaxContext = g_firstDefinition;
    TEST_ASSERT_EQUAL_UINT32(0U, g_rootType.nominalDefinitionToken);
    TEST_ASSERT_EQUAL_UINT32(baseBinding.typeId,
            ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, &g_rootType));
    compiler_register_type_value_alias(&g_compiler, alias->name, &g_rootType, alias->syntaxContext);
    aliasBinding = compiler_find_type_value_alias(&g_compiler, alias->name, alias->syntaxContext);
    TEST_ASSERT_NOT_NULL(aliasBinding);
    TEST_ASSERT_EQUAL_UINT32(0U, aliasBinding->type.nominalDefinitionToken);
    TEST_ASSERT_NULL(compiler_find_type_value_alias(
            &g_compiler, alias->name, ZrParser_SyntaxContext_Root()));
    annotation = chosen_annotation(nodes->nodes[3]);
    annotation->name->data.identifier.syntaxContext = alias->syntaxContext;
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(&g_compiler, annotation, &g_baseType));
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_UINT32(0U, g_baseType.nominalDefinitionToken);
    TEST_ASSERT_EQUAL_UINT32(baseBinding.typeId,
            ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, &g_baseType));
    prototype = type_inference_find_nominal_prototype(&g_compiler, &g_baseType);
    TEST_ASSERT_NOT_NULL(prototype);
    TEST_ASSERT_EQUAL_PTR(nodes->nodes[0], prototype->declarationNode);
    derivedBinding = publish_class(nodes->nodes[3]);
    assert_published_base(derivedBinding, nodes->nodes[3], baseBinding, nodes->nodes[0]);
}

static SZrNamedTypeBinding register_parsed_base_alias(
        SZrAstNodeArray *nodes, SZrSyntaxContext aliasContext) {
    SZrNamedTypeBinding baseBinding;
    SZrIdentifier *alias;
    SZrTypeBinding *binding;
    SZrAstNode *value;

    TEST_ASSERT_EQUAL_UINT(4U, nodes->count);
    baseBinding = publish_class(nodes->nodes[0]);
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, base_annotation(nodes->nodes[1]), &g_rootType));
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_UINT32(0U, g_rootType.nominalDefinitionToken);
    TEST_ASSERT_EQUAL_UINT32(baseBinding.typeId,
            ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, &g_rootType));
    alias = alias_identifier(nodes->nodes[2]);
    value = nodes->nodes[2]->data.variableDeclaration.value;
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, value->type);
    TEST_ASSERT_EQUAL_STRING("Base", ZrCore_String_GetNativeString(value->data.identifier.name));
    alias->syntaxContext = aliasContext;
    compiler_register_type_value_alias(&g_compiler, alias->name, &g_rootType, aliasContext);
    binding = compiler_find_type_value_alias(&g_compiler, alias->name, aliasContext);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_UINT32(baseBinding.typeId,
            ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, &binding->type));
    return baseBinding;
}

static void assert_matching_alias_base_relation(SZrSyntaxContext aliasContext) {
    SZrAstNodeArray *nodes = parse_source("class Base {} class Evidence : Base {}"
            "let Chosen = Base; class Derived : Chosen {}");
    SZrNamedTypeBinding baseBinding = register_parsed_base_alias(nodes, aliasContext);
    SZrType *annotation = chosen_annotation(nodes->nodes[3]);
    SZrNamedTypeBinding derivedBinding;

    annotation->name->data.identifier.syntaxContext = aliasContext;
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, annotation, &g_baseType));
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_UINT32(baseBinding.typeId,
            ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, &g_baseType));
    derivedBinding = publish_class(nodes->nodes[3]);
    assert_published_base(derivedBinding, nodes->nodes[3], baseBinding, nodes->nodes[0]);
}

static void test_root_alias_base_relation(void) {
    assert_matching_alias_base_relation(ZrParser_SyntaxContext_Root());
}

static void test_mixed_site_alias_base_relation(void) {
    assert_matching_alias_base_relation(g_mixed);
}

static void test_mixed_site_missing_alias_cannot_capture_root_alias(void) {
    SZrAstNodeArray *nodes = parse_source("class Base {} class Evidence : Base {}"
            "let Chosen = Base; class Derived : Chosen {}");
    SZrNamedTypeBinding baseBinding = register_parsed_base_alias(
            nodes, ZrParser_SyntaxContext_Root());
    SZrType *annotation = chosen_annotation(nodes->nodes[3]);
    SZrSyntaxContext itemContext;
    SZrSyntaxContext localContext;
    SZrTypePrototypeInfo *rootAliasPrototype;
    SZrNamedTypeBinding derivedBinding;

    annotation->name->data.identifier.syntaxContext = g_mixed;
    itemContext = ZrParser_SyntaxContext_Normalize(g_mixed, ZR_SYNTAX_NAMESPACE_ITEM);
    localContext = ZrParser_SyntaxContext_Normalize(g_mixed, ZR_SYNTAX_NAMESPACE_LOCAL);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(itemContext));
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(localContext));
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Equal(itemContext, ZrParser_SyntaxContext_Root()));
    TEST_ASSERT_FALSE(ZrParser_SyntaxContext_Equal(localContext, ZrParser_SyntaxContext_Root()));
    TEST_ASSERT_NULL(compiler_find_type_value_alias(
            &g_compiler, annotation->name->data.identifier.name, g_mixed));
    rootAliasPrototype = find_compiler_type_prototype(
            &g_compiler, annotation->name->data.identifier.name);
    TEST_ASSERT_NOT_NULL(rootAliasPrototype);
    TEST_ASSERT_EQUAL_PTR(nodes->nodes[0], rootAliasPrototype->declarationNode);
    assert_symbol(baseBinding, nodes->nodes[0]);
    derivedBinding = publish_class(nodes->nodes[3]);
    (void)ZrParser_SemanticQuery_BaseTypesOf(
            g_compiler.semanticContext, derivedBinding.typeId, &g_relations);
    TEST_ASSERT_EQUAL_UINT(0U, g_relations.length);
}

static void assert_array_annotation_preserves_outer_shape(
        SZrType *annotation, SZrNamedTypeBinding baseBinding) {
    const SZrInferredType *element;
    const SZrCanonicalTypeNode *canonical;
    TZrTypeId arrayTypeId;

    TEST_ASSERT_EQUAL_INT(1, annotation->dimensions);
    TEST_ASSERT_NULL(annotation->subType);
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, annotation, &g_baseType));
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, g_baseType.baseType);
    TEST_ASSERT_EQUAL_UINT(1U, g_baseType.elementTypes.length);
    element = ZrCore_Array_Get(&g_baseType.elementTypes, 0U);
    TEST_ASSERT_NOT_NULL(element);
    TEST_ASSERT_EQUAL_UINT32(baseBinding.typeId,
            ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, element));
    arrayTypeId = ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, &g_baseType);
    TEST_ASSERT_NOT_EQUAL(baseBinding.typeId, arrayTypeId);
    canonical = ZrParser_CanonicalType_Find(g_compiler.semanticContext, arrayTypeId);
    TEST_ASSERT_NOT_NULL(canonical);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_ARRAY, canonical->kind);
}

static void test_root_array_base_keeps_no_nominal_relation(void) {
    SZrAstNodeArray *nodes = parse_source("class Base {} class Derived : Base[] {}");
    SZrNamedTypeBinding baseBinding;
    SZrNamedTypeBinding derivedBinding;
    SZrType *annotation;

    TEST_ASSERT_EQUAL_UINT(2U, nodes->count);
    baseBinding = publish_class(nodes->nodes[0]);
    annotation = base_annotation(nodes->nodes[1]);
    assert_array_annotation_preserves_outer_shape(annotation, baseBinding);
    TEST_ASSERT_NULL(find_compiler_type_prototype(
            &g_compiler, extract_type_name_string(&g_compiler, annotation)));
    TEST_ASSERT_FALSE(g_compiler.hasError);
    derivedBinding = publish_class(nodes->nodes[1]);
    (void)ZrParser_SemanticQuery_BaseTypesOf(
            g_compiler.semanticContext, derivedBinding.typeId, &g_relations);
    TEST_ASSERT_EQUAL_UINT(0U, g_relations.length);
}

static void test_root_array_alias_keeps_no_nominal_relation(void) {
    SZrAstNodeArray *nodes = parse_source("class Base {} class Evidence : Base {}"
            "let Chosen = Base; class Derived : Chosen[] {}");
    SZrNamedTypeBinding baseBinding = register_parsed_base_alias(
            nodes, ZrParser_SyntaxContext_Root());
    SZrType *annotation = chosen_annotation(nodes->nodes[3]);
    SZrString *legacyName;
    SZrNamedTypeBinding derivedBinding;

    assert_array_annotation_preserves_outer_shape(annotation, baseBinding);
    legacyName = extract_type_name_string(&g_compiler, annotation);
    TEST_ASSERT_NOT_NULL(legacyName);
    TEST_ASSERT_EQUAL_STRING("Chosen[]", ZrCore_String_GetNativeString(legacyName));
    TEST_ASSERT_NULL(find_compiler_type_prototype(&g_compiler, legacyName));
    TEST_ASSERT_FALSE(g_compiler.hasError);
    derivedBinding = publish_class(nodes->nodes[3]);
    (void)ZrParser_SemanticQuery_BaseTypesOf(
            g_compiler.semanticContext, derivedBinding.typeId, &g_relations);
    TEST_ASSERT_EQUAL_UINT(0U, g_relations.length);
}

static void assert_root_generic_argument_after_root_cache(TZrBool useAlias) {
    SZrAstNodeArray *nodes = parse_source(useAlias
            ? "class Widget {} class Widget {} class Base<T> {}"
              "let RootChoice = Base<Widget>; let Chosen = Base<Widget>; class Derived : Chosen {}"
            : "class Widget {} class Widget {} class Base<T> {}"
              "let RootChoice = Base<Widget>; let Chosen = Base<Widget>; class Derived : Base<Widget> {}");
    SZrNamedTypeBinding markedWidget;
    SZrNamedTypeBinding baseBinding;
    SZrNamedTypeBinding derivedBinding;
    SZrAstNode *value;
    SZrAstNode *argument;
    SZrIdentifier *alias;
    SZrTypeBinding *binding;
    SZrTypePrototypeInfo *prototype;
    TZrTypeId rootInstanceId = ZR_SEMANTIC_ID_INVALID;
    TZrTypeId markedInstanceId = ZR_SEMANTIC_ID_INVALID;
    SZrType *annotation;

    TEST_ASSERT_EQUAL_UINT(6U, nodes->count);
    nodes->nodes[1]->data.classDeclaration.name->syntaxContext = g_firstDefinition;
    (void)publish_class(nodes->nodes[0]);
    markedWidget = publish_class(nodes->nodes[1]);
    baseBinding = publish_class(nodes->nodes[2]);
    for (TZrSize index = 3U; index < 5U; index++) {
        alias = alias_identifier(nodes->nodes[index]);
        value = nodes->nodes[index]->data.variableDeclaration.value;
        TEST_ASSERT_NOT_NULL(value);
        TEST_ASSERT_EQUAL_INT(ZR_AST_TYPE_LITERAL_EXPRESSION, value->type);
        TEST_ASSERT_NOT_NULL(value->data.typeLiteralExpression.typeInfo);
        TEST_ASSERT_EQUAL_INT(ZR_AST_GENERIC_TYPE, value->data.typeLiteralExpression.typeInfo->name->type);
        TEST_ASSERT_NOT_NULL(value->data.typeLiteralExpression.typeInfo->name->data.genericType.params);
        TEST_ASSERT_EQUAL_UINT(1U, value->data.typeLiteralExpression.typeInfo->name->data.genericType.params->count);
        argument = value->data.typeLiteralExpression.typeInfo->name->data.genericType.params->nodes[0];
        TEST_ASSERT_EQUAL_INT(ZR_AST_TYPE, argument->type);
        TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, argument->data.type.name->type);
        if (index == 4U) {
            argument->data.type.name->data.identifier.syntaxContext = g_firstDefinition;
        }
        compiler_register_type_value_alias_literal(&g_compiler, alias->name, value, alias->syntaxContext);
        TEST_ASSERT_FALSE(g_compiler.hasError);
        binding = compiler_find_type_value_alias(&g_compiler, alias->name, alias->syntaxContext);
        TEST_ASSERT_NOT_NULL(binding);
        ZrParser_InferredType_Copy(g_state, index == 3U ? &g_rootType : &g_baseType, &binding->type);
        TEST_ASSERT_EQUAL_UINT32(0U, binding->type.nominalDefinitionToken);
        TEST_ASSERT_EQUAL_UINT(1U, binding->type.elementTypes.length);
        TEST_ASSERT_EQUAL_UINT32(index == 3U ? 0U : markedWidget.symbolId,
                ((SZrInferredType *)ZrCore_Array_Get(&binding->type.elementTypes, 0U))->nominalDefinitionToken);
        TEST_ASSERT_TRUE(ensure_generic_instance_type_prototype_from_inferred(
                &g_compiler, index == 3U ? &g_rootType : &g_baseType));
        prototype = type_inference_find_nominal_prototype(
                &g_compiler, index == 3U ? &g_rootType : &g_baseType);
        TEST_ASSERT_NOT_NULL(prototype);
        TEST_ASSERT_EQUAL_PTR(nodes->nodes[2], prototype->declarationNode);
        TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, prototype->genericInstanceTypeId);
        if (index == 3U) {
            rootInstanceId = prototype->genericInstanceTypeId;
        } else {
            markedInstanceId = prototype->genericInstanceTypeId;
        }
    }
    TEST_ASSERT_NOT_EQUAL(rootInstanceId, markedInstanceId);
    TEST_ASSERT_EQUAL_UINT32(markedInstanceId,
            ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, &g_baseType));
    if (useAlias) {
        annotation = chosen_annotation(nodes->nodes[5]);
    } else {
        TEST_ASSERT_NOT_NULL(nodes->nodes[5]->data.classDeclaration.inherits);
        TEST_ASSERT_EQUAL_UINT(1U, nodes->nodes[5]->data.classDeclaration.inherits->count);
        value = nodes->nodes[5]->data.classDeclaration.inherits->nodes[0];
        TEST_ASSERT_EQUAL_INT(ZR_AST_TYPE, value->type);
        annotation = &value->data.type;
        TEST_ASSERT_NOT_NULL(annotation->name);
        TEST_ASSERT_EQUAL_INT(ZR_AST_GENERIC_TYPE, annotation->name->type);
        TEST_ASSERT_NOT_NULL(annotation->name->data.genericType.params);
        TEST_ASSERT_EQUAL_UINT(1U, annotation->name->data.genericType.params->count);
        argument = annotation->name->data.genericType.params->nodes[0];
        TEST_ASSERT_EQUAL_INT(ZR_AST_TYPE, argument->type);
        TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, argument->data.type.name->type);
        argument->data.type.name->data.identifier.syntaxContext = g_firstDefinition;
    }
    prototype = ZR_NULL;
    TEST_ASSERT_TRUE(compiler_resolve_type_annotation_prototype(
            &g_compiler, annotation, extract_type_name_string(&g_compiler, annotation), &prototype));
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_NOT_NULL(prototype);
    TEST_ASSERT_EQUAL_UINT32(markedInstanceId, prototype->genericInstanceTypeId);
    TEST_ASSERT_NOT_EQUAL(rootInstanceId, prototype->genericInstanceTypeId);
    derivedBinding = publish_class(nodes->nodes[5]);
    assert_published_base(derivedBinding, nodes->nodes[5], baseBinding, nodes->nodes[2]);
}

static void test_root_generic_alias_keeps_marked_argument_after_root_cache(void) {
    assert_root_generic_argument_after_root_cache(ZR_TRUE);
}

static void test_root_generic_annotation_keeps_marked_argument_after_root_cache(void) {
    assert_root_generic_argument_after_root_cache(ZR_FALSE);
}

static void test_root_generic_selector_preserves_first_registered_import(void) {
    SZrAstNodeArray *nodes = parse_source("class Base<T> {} class Derived : Base<int> {}");
    SZrAstNode *inheritance;
    SZrTypePrototypeInfo *prototype;
    SZrTypePrototypeInfo source;
    SZrTypePrototypeInfo imported = {0};
    TZrSize sourceIndex = 0U;
    TZrBool found = ZR_FALSE;

    TEST_ASSERT_EQUAL_UINT(2U, nodes->count);
    (void)publish_class(nodes->nodes[0]);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, nodes->nodes[1]->type);
    TEST_ASSERT_NOT_NULL(nodes->nodes[1]->data.classDeclaration.inherits);
    TEST_ASSERT_EQUAL_UINT(1U, nodes->nodes[1]->data.classDeclaration.inherits->count);
    inheritance = nodes->nodes[1]->data.classDeclaration.inherits->nodes[0];
    TEST_ASSERT_EQUAL_INT(ZR_AST_TYPE, inheritance->type);
    TEST_ASSERT_NOT_NULL(inheritance->data.type.name);
    TEST_ASSERT_EQUAL_INT(ZR_AST_GENERIC_TYPE, inheritance->data.type.name->type);
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, &inheritance->data.type, &g_baseType));
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_TRUE(ensure_generic_instance_type_prototype_from_inferred(&g_compiler, &g_baseType));
    prototype = type_inference_find_nominal_prototype(&g_compiler, &g_baseType);
    TEST_ASSERT_NOT_NULL(prototype);
    TEST_ASSERT_EQUAL_PTR(nodes->nodes[0], prototype->declarationNode);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, prototype->genericInstanceTypeId);
    source = *prototype;
    for (TZrSize index = 0U; index < g_compiler.typePrototypes.length; index++) {
        if (ZrCore_Array_Get(&g_compiler.typePrototypes, index) == prototype) {
            sourceIndex = index;
            found = ZR_TRUE;
            break;
        }
    }
    TEST_ASSERT_TRUE(found);
    imported.name = source.name;
    imported.type = source.type;
    imported.isImportedNative = ZR_TRUE;
    imported.genericInstanceTypeId = source.genericInstanceTypeId;
    /* Transfer source ownership before replacing its slot with borrowed import metadata. */
    ZrCore_Array_Push(g_state, &g_compiler.typePrototypes, &source);
    *(SZrTypePrototypeInfo *)ZrCore_Array_Get(&g_compiler.typePrototypes, sourceIndex) = imported;
    prototype = find_compiler_type_prototype(&g_compiler, source.name);
    TEST_ASSERT_EQUAL_PTR(ZrCore_Array_Get(&g_compiler.typePrototypes, sourceIndex), prototype);
    TEST_ASSERT_NULL(prototype->declarationNode);
    TEST_ASSERT_TRUE(prototype->isImportedNative);
    prototype = ZR_NULL;
    TEST_ASSERT_TRUE(compiler_resolve_type_annotation_prototype(&g_compiler,
            &inheritance->data.type, source.name, &prototype));
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_PTR(ZrCore_Array_Get(&g_compiler.typePrototypes, sourceIndex), prototype);
    TEST_ASSERT_NOT_NULL(prototype);
    TEST_ASSERT_NULL(prototype->declarationNode);
    TEST_ASSERT_TRUE(prototype->isImportedNative);
    TEST_ASSERT_EQUAL_UINT32(source.genericInstanceTypeId, prototype->genericInstanceTypeId);
}

static void assert_rejected_base(SZrSyntaxContext annotationContext) {
    SZrAstNodeArray *nodes = parse_classes(ZR_FALSE);
    SZrType *annotation = base_annotation(nodes->nodes[1]);
    SZrString *oldName;
    SZrAstNode *oldNode;
    SZrTypePrototypeInfo *oldPrototype;
    const SZrNamedTypeBinding *derivedBinding;
    TZrSize prototypeCount;

    (void)publish_class(nodes->nodes[0]);
    TEST_ASSERT_NULL(ZrParser_TypeEnvironment_FindTypeBindingWithContext(g_compiler.typeEnv,
            nodes->nodes[0]->data.classDeclaration.name->name, g_firstDefinition));
    annotation->name->data.identifier.syntaxContext = annotationContext;
    oldName = g_compiler.currentTypeName;
    oldNode = g_compiler.currentTypeNode;
    oldPrototype = g_compiler.currentTypePrototypeInfo;
    prototypeCount = g_compiler.typePrototypes.length;
    ZrParser_Compiler_CompileClassDeclaration(&g_compiler, nodes->nodes[1]);
    TEST_ASSERT_TRUE(g_compiler.hasError);
    TEST_ASSERT_NOT_NULL(g_compiler.errorMessage);
    TEST_ASSERT_EQUAL_UINT(prototypeCount, g_compiler.typePrototypes.length);
    TEST_ASSERT_EQUAL_PTR(oldName, g_compiler.currentTypeName);
    TEST_ASSERT_EQUAL_PTR(oldNode, g_compiler.currentTypeNode);
    TEST_ASSERT_EQUAL_PTR(oldPrototype, g_compiler.currentTypePrototypeInfo);
    derivedBinding = ZrParser_TypeEnvironment_FindTypeBindingWithContext(g_compiler.typeEnv,
            nodes->nodes[1]->data.classDeclaration.name->name, ZrParser_SyntaxContext_Root());
    TEST_ASSERT_NOT_NULL(derivedBinding);
    TEST_ASSERT_EQUAL_PTR(nodes->nodes[1], derivedBinding->declarationNode);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, derivedBinding->symbolId);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, derivedBinding->typeId);
    assert_symbol(*derivedBinding, nodes->nodes[1]);
    (void)ZrParser_SemanticQuery_BaseTypesOf(
            g_compiler.semanticContext, derivedBinding->typeId, &g_relations);
    TEST_ASSERT_EQUAL_UINT(0U, g_relations.length);
}
static void test_missing_opaque_base_cannot_fall_back_to_root(void) {
    assert_rejected_base(g_firstDefinition);
}
static void test_invalid_base_context_cannot_fall_back_to_root(void) {
    assert_rejected_base((SZrSyntaxContext){g_table, ZR_SYNTAX_CONTEXT_INVALID_MARK_ID});
}

#include "test_syntax_hygiene_base_contract_cases.h"

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_root_only_base_relation);
    RUN_TEST(test_opaque_only_base_relation);
    RUN_TEST(test_root_then_opaque_selects_root);
    RUN_TEST(test_opaque_then_root_selects_root);
    RUN_TEST(test_root_then_opaque_selects_opaque);
    RUN_TEST(test_opaque_then_root_selects_opaque);
    RUN_TEST(test_mixed_site_selects_root_item);
    RUN_TEST(test_two_opaque_bases_select_second_identity);
    RUN_TEST(test_derived_and_base_have_independent_definition_sites);
    RUN_TEST(test_root_forward_base_preserves_deferred_relation);
    RUN_TEST(test_root_generic_forward_base_preserves_success);
    RUN_TEST(test_opaque_alias_can_target_root_nominal_token_zero);
    RUN_TEST(test_root_alias_base_relation);
    RUN_TEST(test_mixed_site_alias_base_relation);
    RUN_TEST(test_mixed_site_missing_alias_cannot_capture_root_alias);
    RUN_TEST(test_root_array_base_keeps_no_nominal_relation);
    RUN_TEST(test_root_array_alias_keeps_no_nominal_relation);
    RUN_TEST(test_root_generic_alias_keeps_marked_argument_after_root_cache);
    RUN_TEST(test_root_generic_annotation_keeps_marked_argument_after_root_cache);
    RUN_TEST(test_root_generic_selector_preserves_first_registered_import);
    RUN_TEST(test_missing_opaque_base_cannot_fall_back_to_root);
    RUN_TEST(test_invalid_base_context_cannot_fall_back_to_root);
    RUN_TEST(test_root_base_relation_survives_contracts);
    RUN_TEST(test_opaque_only_base_relation_survives_contracts);
    RUN_TEST(test_root_then_opaque_base_relation_survives_contracts);
    RUN_TEST(test_opaque_then_root_base_relation_survives_contracts);
    RUN_TEST(test_root_forward_base_materializes_in_contracts);
    RUN_TEST(test_mixed_missing_alias_stays_unbound_after_contracts);
    RUN_TEST(test_expired_opaque_alias_keeps_published_base_contract);
    RUN_TEST(test_closed_source_keeps_open_declaration_base_contract);
    RUN_TEST(test_alias_base_producer_persists_target_before_alias_cleanup);
    RUN_TEST(test_unnamed_source_keeps_alias_base_after_alias_cleanup);
    RUN_TEST(test_existing_interface_does_not_block_forward_base_contract);
    RUN_TEST(test_imported_first_generic_base_stays_unpublished_after_contracts);
    RUN_TEST(test_forward_generic_interface_materializes_exact_typed_contract);
    return UNITY_END();
}
