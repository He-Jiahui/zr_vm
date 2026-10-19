#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "compiler/expression/compile_expression_internal.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/syntax_context.h"
#include "zr_vm_parser/type_inference.h"
#include "zr_vm_parser/type_system.h"

static SZrState *g_state;
static SZrCompilerState g_compiler;
static TZrBool g_compilerInitialized;
static SZrAstNode *g_ast;
static SZrSyntaxContextTable *g_contextTable;
static SZrSyntaxContext g_defSite;
static SZrSyntaxContext g_callSite;
static SZrSyntaxContext g_mixedSite;
static SZrTypeEnvironment *g_childEnvironment;
static SZrInferredType g_annotationType;
static SZrInferredType g_copiedType;

void setUp(void) {
    SZrSyntaxExpansionInfo expansion = {0};
    TZrSyntaxExpansionId expansionId;

    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
    ZrParser_InferredType_Init(g_state, &g_annotationType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &g_copiedType, ZR_VALUE_TYPE_OBJECT);
    g_contextTable = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_contextTable);
    expansion.stableIdentity = 0xA114U;
    expansion.callSiteSpan = ZrParser_TokenSpan_None();
    expansion.definitionSiteSpan = ZrParser_TokenSpan_None();
    TEST_ASSERT_TRUE(ZrParser_SyntaxContextTable_RegisterExpansion(
            g_contextTable, &expansion, &expansionId));
    g_defSite = ZrParser_SyntaxContext_DefSite(g_contextTable, expansionId);
    g_callSite = ZrParser_SyntaxContext_CallSite(g_contextTable, expansionId);
    g_mixedSite = ZrParser_SyntaxContext_MixedSite(g_contextTable, expansionId);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(g_callSite));
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(g_mixedSite));
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(g_defSite));
    TEST_ASSERT_FALSE(ZrParser_SyntaxContext_Equal(
            ZrParser_SyntaxContext_Normalize(g_defSite, ZR_SYNTAX_NAMESPACE_ITEM),
            ZrParser_SyntaxContext_Root()));
    ZrParser_CompilerState_Init(&g_compiler, g_state);
    g_compilerInitialized = ZR_TRUE;
    TEST_ASSERT_NOT_NULL(g_compiler.typeEnv);
    TEST_ASSERT_NOT_NULL(g_compiler.semanticContext);
}

void tearDown(void) {
    ZrParser_InferredType_Free(g_state, &g_annotationType);
    ZrParser_InferredType_Free(g_state, &g_copiedType);
    if (g_childEnvironment != ZR_NULL) {
        ZrParser_TypeEnvironment_Free(g_state, g_childEnvironment);
        g_childEnvironment = ZR_NULL;
    }
    if (g_compilerInitialized) {
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

static SZrAstNodeArray *parse_source(const TZrChar *source) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(g_state, "nominal_hygiene.zr");
    SZrAstNodeArray *statements;

    TEST_ASSERT_NOT_NULL(sourceName);
    g_ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(g_ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_ast->type);
    statements = g_ast->data.script.statements;
    TEST_ASSERT_NOT_NULL(statements);
    g_compiler.scriptAst = g_ast;
    return statements;
}

static SZrAstNodeArray *parse_declarations(void) {
    SZrAstNodeArray *statements = parse_source(
            "struct Widget { var value: int; }"
            "struct Widget { var text: string; }");

    TEST_ASSERT_EQUAL_UINT(2U, statements->count);
    for (TZrSize index = 0U; index < statements->count; index++) {
        TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_DECLARATION, statements->nodes[index]->type);
        TEST_ASSERT_NOT_NULL(statements->nodes[index]->data.structDeclaration.name);
    }
    return statements;
}

static void register_declaration(SZrAstNode *declaration, SZrSyntaxContext context) {
    SZrIdentifier *name = declaration->data.structDeclaration.name;

    name->syntaxContext = context;
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterTypeDeclaration(
            g_state, g_compiler.typeEnv, name->name, declaration));
}

static const SZrSemanticSymbolRecord *find_declaration_symbol(SZrAstNode *declaration) {
    for (TZrSize index = 0U; index < g_compiler.semanticContext->symbols.length; index++) {
        const SZrSemanticSymbolRecord *symbol =
                ZrCore_Array_Get(&g_compiler.semanticContext->symbols, index);

        if (symbol != ZR_NULL && symbol->kind == ZR_SEMANTIC_SYMBOL_KIND_TYPE &&
            symbol->astNode == declaration) {
            return symbol;
        }
    }
    return ZR_NULL;
}

static void test_root_nominal_registration_prerequisite(void) {
    SZrAstNode *declaration = parse_declarations()->nodes[0];
    const SZrSemanticSymbolRecord *symbol;
    const SZrCanonicalTypeNode *type;

    register_declaration(declaration, ZrParser_SyntaxContext_Root());
    symbol = find_declaration_symbol(declaration);
    TEST_ASSERT_NOT_NULL(symbol);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Equal(
            symbol->syntaxContext, ZrParser_SyntaxContext_Root()));
    type = ZrParser_CanonicalType_Find(g_compiler.semanticContext, symbol->typeId);
    TEST_ASSERT_NOT_NULL(type);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_NOMINAL, type->kind);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_LookupType(
            g_compiler.typeEnv, declaration->data.structDeclaration.name->name));
}

static void test_definition_nominal_symbol_keeps_item_context(void) {
    SZrAstNode *declaration = parse_declarations()->nodes[0];
    const SZrSemanticSymbolRecord *symbol;

    register_declaration(declaration, g_defSite);
    symbol = find_declaration_symbol(declaration);
    TEST_ASSERT_NOT_NULL(symbol);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Equal(
            ZrParser_SyntaxContext_Normalize(symbol->syntaxContext, ZR_SYNTAX_NAMESPACE_ITEM),
            ZrParser_SyntaxContext_Normalize(g_defSite, ZR_SYNTAX_NAMESPACE_ITEM)));
}

static void test_definition_nominal_does_not_publish_root_type_name(void) {
    SZrAstNode *declaration = parse_declarations()->nodes[0];

    register_declaration(declaration, g_defSite);
    TEST_ASSERT_FALSE(ZrParser_TypeEnvironment_LookupType(
            g_compiler.typeEnv, declaration->data.structDeclaration.name->name));
}

static void test_definition_and_root_nominal_symbols_are_distinct(void) {
    SZrAstNodeArray *declarations = parse_declarations();
    const SZrSemanticSymbolRecord *definitionSymbol;
    const SZrSemanticSymbolRecord *rootSymbol;

    register_declaration(declarations->nodes[0], g_defSite);
    register_declaration(declarations->nodes[1], ZrParser_SyntaxContext_Root());
    definitionSymbol = find_declaration_symbol(declarations->nodes[0]);
    rootSymbol = find_declaration_symbol(declarations->nodes[1]);
    TEST_ASSERT_NOT_NULL(definitionSymbol);
    TEST_ASSERT_NOT_NULL(rootSymbol);
    TEST_ASSERT_NOT_EQUAL(definitionSymbol->id, rootSymbol->id);
    TEST_ASSERT_NOT_EQUAL(definitionSymbol->typeId, rootSymbol->typeId);
}

static void test_name_only_declaration_query_selects_root_context(void) {
    SZrAstNodeArray *declarations = parse_declarations();
    SZrIdentifier *definitionName = declarations->nodes[0]->data.structDeclaration.name;

    definitionName->syntaxContext = g_defSite;
    TEST_ASSERT_EQUAL_PTR(declarations->nodes[1],
            find_type_declaration(&g_compiler, definitionName->name));
}

static void test_definition_query_selects_matching_declaration(void) {
    SZrAstNodeArray *declarations = parse_declarations();
    SZrIdentifier *definitionName = declarations->nodes[0]->data.structDeclaration.name;

    definitionName->syntaxContext = g_defSite;
    TEST_ASSERT_EQUAL_PTR(declarations->nodes[0],
            find_type_declaration_with_context(&g_compiler, definitionName->name, g_defSite));
}

static void test_name_only_query_rejects_definition_only_declarations(void) {
    SZrAstNodeArray *declarations = parse_declarations();
    SZrIdentifier *name = declarations->nodes[0]->data.structDeclaration.name;

    name->syntaxContext = g_defSite;
    declarations->nodes[1]->data.structDeclaration.name->syntaxContext = g_defSite;
    TEST_ASSERT_NULL(find_type_declaration(&g_compiler, name->name));
}

static void test_call_site_query_preserves_root_compatibility(void) {
    SZrAstNodeArray *declarations = parse_declarations();
    SZrIdentifier *name = declarations->nodes[0]->data.structDeclaration.name;

    name->syntaxContext = g_defSite;
    TEST_ASSERT_EQUAL_PTR(declarations->nodes[1],
            find_type_declaration_with_context(&g_compiler, name->name, g_callSite));
}

static void test_definition_registration_is_idempotent(void) {
    SZrAstNode *declaration = parse_declarations()->nodes[0];
    const SZrSemanticSymbolRecord *symbol;
    TZrSymbolId symbolId;
    TZrTypeId typeId;
    TZrSize bindingCount;
    TZrSize symbolCount;
    TZrSize typeCount;

    register_declaration(declaration, g_defSite);
    symbol = find_declaration_symbol(declaration);
    TEST_ASSERT_NOT_NULL(symbol);
    symbolId = symbol->id;
    typeId = symbol->typeId;
    bindingCount = g_compiler.typeEnv->namedTypeBindings.length;
    symbolCount = g_compiler.semanticContext->symbols.length;
    typeCount = g_compiler.semanticContext->canonicalTypes.length;
    register_declaration(declaration, g_defSite);
    TEST_ASSERT_EQUAL_UINT(bindingCount, g_compiler.typeEnv->namedTypeBindings.length);
    TEST_ASSERT_EQUAL_UINT(symbolCount, g_compiler.semanticContext->symbols.length);
    TEST_ASSERT_EQUAL_UINT(typeCount, g_compiler.semanticContext->canonicalTypes.length);
    symbol = find_declaration_symbol(declaration);
    TEST_ASSERT_NOT_NULL(symbol);
    TEST_ASSERT_EQUAL_UINT(symbolId, symbol->id);
    TEST_ASSERT_EQUAL_UINT(typeId, symbol->typeId);
}

static void test_root_then_definition_registration_keeps_distinct_types(void) {
    SZrAstNodeArray *declarations = parse_declarations();
    const SZrSemanticSymbolRecord *rootSymbol;
    const SZrSemanticSymbolRecord *definitionSymbol;

    register_declaration(declarations->nodes[0], ZrParser_SyntaxContext_Root());
    register_declaration(declarations->nodes[1], g_defSite);
    rootSymbol = find_declaration_symbol(declarations->nodes[0]);
    definitionSymbol = find_declaration_symbol(declarations->nodes[1]);
    TEST_ASSERT_NOT_NULL(rootSymbol);
    TEST_ASSERT_NOT_NULL(definitionSymbol);
    TEST_ASSERT_NOT_EQUAL(rootSymbol->typeId, definitionSymbol->typeId);
}

static void test_definition_binding_context_lookup_is_isolated(void) {
    SZrAstNode *declaration = parse_declarations()->nodes[0];
    SZrString *name = declaration->data.structDeclaration.name->name;
    const SZrNamedTypeBinding *binding;

    register_declaration(declaration, g_defSite);
    binding = ZrParser_TypeEnvironment_FindTypeBindingWithContext(
            g_compiler.typeEnv, name, g_defSite);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_PTR(declaration, binding->declarationNode);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_LookupTypeWithContext(
            g_compiler.typeEnv, name, g_defSite));
    TEST_ASSERT_FALSE(ZrParser_TypeEnvironment_LookupTypeWithContext(
            g_compiler.typeEnv, name, g_callSite));
    TEST_ASSERT_EQUAL_UINT(0U, g_compiler.typeEnv->typeNames.length);
}

static void test_mixed_site_items_share_root_identity(void) {
    SZrAstNodeArray *declarations = parse_declarations();
    SZrString *name = declarations->nodes[0]->data.structDeclaration.name->name;
    const SZrNamedTypeBinding *binding;

    register_declaration(declarations->nodes[0], g_mixedSite);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_LookupType(g_compiler.typeEnv, name));
    binding = ZrParser_TypeEnvironment_FindTypeBindingWithContext(
            g_compiler.typeEnv, name, ZrParser_SyntaxContext_Root());
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_PTR(declarations->nodes[0], binding->declarationNode);
    register_declaration(declarations->nodes[1], ZrParser_SyntaxContext_Root());
    TEST_ASSERT_EQUAL_UINT(1U, g_compiler.typeEnv->namedTypeBindings.length);
    TEST_ASSERT_EQUAL_UINT(1U, g_compiler.typeEnv->typeNames.length);
    TEST_ASSERT_EQUAL_PTR(declarations->nodes[0], find_type_declaration(&g_compiler, name));
}

static void test_child_environment_shadows_and_preserves_parent_definition(void) {
    SZrAstNodeArray *declarations = parse_declarations();
    SZrString *name = declarations->nodes[0]->data.structDeclaration.name->name;
    const SZrNamedTypeBinding *parentBinding;
    const SZrNamedTypeBinding *childBinding;
    TZrTypeId parentTypeId;

    register_declaration(declarations->nodes[0], g_defSite);
    parentBinding = ZrParser_TypeEnvironment_FindTypeBindingWithContext(
            g_compiler.typeEnv, name, g_defSite);
    TEST_ASSERT_NOT_NULL(parentBinding);
    parentTypeId = parentBinding->typeId;
    g_childEnvironment = ZrParser_TypeEnvironment_New(g_state);
    TEST_ASSERT_NOT_NULL(g_childEnvironment);
    g_childEnvironment->parent = g_compiler.typeEnv;
    g_childEnvironment->semanticContext = g_compiler.semanticContext;
    TEST_ASSERT_EQUAL_PTR(parentBinding,
            ZrParser_TypeEnvironment_FindTypeBindingWithContext(
                    g_childEnvironment, name, g_defSite));
    declarations->nodes[1]->data.structDeclaration.name->syntaxContext = g_defSite;
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterTypeDeclaration(
            g_state, g_childEnvironment, name, declarations->nodes[1]));
    childBinding = ZrParser_TypeEnvironment_FindTypeBindingWithContext(
            g_childEnvironment, name, g_defSite);
    TEST_ASSERT_NOT_NULL(childBinding);
    TEST_ASSERT_EQUAL_PTR(declarations->nodes[1], childBinding->declarationNode);
    TEST_ASSERT_NOT_EQUAL(parentTypeId, childBinding->typeId);
    parentBinding = ZrParser_TypeEnvironment_FindTypeBindingWithContext(
            g_compiler.typeEnv, name, g_defSite);
    TEST_ASSERT_NOT_NULL(parentBinding);
    TEST_ASSERT_EQUAL_PTR(declarations->nodes[0], parentBinding->declarationNode);
    TEST_ASSERT_EQUAL_UINT(parentTypeId, parentBinding->typeId);
}

static void test_invalid_registration_context_preserves_environment(void) {
    SZrAstNode *declaration = parse_declarations()->nodes[0];
    SZrString *name = declaration->data.structDeclaration.name->name;
    SZrSyntaxContext invalid = g_defSite;
    TZrSize typeCount = g_compiler.semanticContext->canonicalTypes.length;
    TZrSize symbolCount = g_compiler.semanticContext->symbols.length;

    invalid.markId = ZR_SYNTAX_CONTEXT_INVALID_MARK_ID;
    TEST_ASSERT_FALSE(ZrParser_TypeEnvironment_RegisterTypeWithContext(
            g_state, g_compiler.typeEnv, name, declaration, invalid));
    TEST_ASSERT_FALSE(ZrParser_TypeEnvironment_LookupTypeWithContext(
            g_compiler.typeEnv, name, invalid));
    TEST_ASSERT_EQUAL_UINT(0U, g_compiler.typeEnv->namedTypeBindings.length);
    TEST_ASSERT_EQUAL_UINT(0U, g_compiler.typeEnv->typeNames.length);
    TEST_ASSERT_EQUAL_UINT(typeCount, g_compiler.semanticContext->canonicalTypes.length);
    TEST_ASSERT_EQUAL_UINT(symbolCount, g_compiler.semanticContext->symbols.length);
}

static SZrType *parse_annotation(SZrSyntaxContext declarationContext,
        SZrSyntaxContext useContext, TZrBool registerType) {
    SZrAstNodeArray *statements = parse_source(
            "struct Widget { var value: int; } var holder: Widget;");
    SZrType *annotation;

    TEST_ASSERT_EQUAL_UINT(2U, statements->count);
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_DECLARATION, statements->nodes[0]->type);
    statements->nodes[0]->data.structDeclaration.name->syntaxContext = declarationContext;
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, statements->nodes[1]->type);
    annotation = statements->nodes[1]->data.variableDeclaration.typeInfo;
    TEST_ASSERT_NOT_NULL(annotation);
    TEST_ASSERT_NOT_NULL(annotation->name);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, annotation->name->type);
    annotation->name->data.identifier.syntaxContext = useContext;
    if (registerType) {
        register_declaration(statements->nodes[0], declarationContext);
    }
    g_compiler.suppressErrorOutput = ZR_TRUE;
    return annotation;
}

static void test_root_annotation_conversion_prerequisite(void) {
    SZrType *annotation = parse_annotation(ZrParser_SyntaxContext_Root(),
            ZrParser_SyntaxContext_Root(), ZR_TRUE);

    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, annotation, &g_annotationType));
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_STRING("Widget", ZrCore_String_GetNativeString(g_annotationType.typeName));
}

static void test_root_annotation_cannot_see_definition_only_declaration(void) {
    SZrType *annotation = parse_annotation(g_defSite,
            ZrParser_SyntaxContext_Root(), ZR_TRUE);

    TEST_ASSERT_FALSE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, annotation, &g_annotationType));
    TEST_ASSERT_TRUE(g_compiler.hasError);
}

static void test_definition_annotation_cannot_see_root_only_declaration(void) {
    SZrType *annotation = parse_annotation(ZrParser_SyntaxContext_Root(),
            g_defSite, ZR_TRUE);

    TEST_ASSERT_FALSE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, annotation, &g_annotationType));
    TEST_ASSERT_TRUE(g_compiler.hasError);
}

static void test_definition_annotation_uses_contextual_environment_binding(void) {
    SZrType *annotation = parse_annotation(g_defSite, g_defSite, ZR_TRUE);

    g_compiler.scriptAst = ZR_NULL;
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, annotation, &g_annotationType));
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_STRING("Widget", ZrCore_String_GetNativeString(g_annotationType.typeName));
}

static void test_definition_annotation_uses_contextual_forward_declaration(void) {
    SZrType *annotation = parse_annotation(g_defSite, g_defSite, ZR_FALSE);

    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, annotation, &g_annotationType));
    TEST_ASSERT_FALSE(g_compiler.hasError);
}

static void test_call_site_annotation_keeps_root_compatibility(void) {
    SZrType *annotation = parse_annotation(ZrParser_SyntaxContext_Root(),
            g_callSite, ZR_TRUE);

    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, annotation, &g_annotationType));
    TEST_ASSERT_FALSE(g_compiler.hasError);
}

static void test_mixed_site_annotation_keeps_item_root_compatibility(void) {
    SZrType *annotation = parse_annotation(ZrParser_SyntaxContext_Root(),
            g_mixedSite, ZR_TRUE);

    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, annotation, &g_annotationType));
    TEST_ASSERT_FALSE(g_compiler.hasError);
}

static void test_invalid_annotation_context_rejects_root_binding(void) {
    SZrSyntaxContext invalid = g_defSite;
    SZrType *annotation;

    invalid.markId = ZR_SYNTAX_CONTEXT_INVALID_MARK_ID;
    annotation = parse_annotation(ZrParser_SyntaxContext_Root(), invalid, ZR_TRUE);
    TEST_ASSERT_FALSE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, annotation, &g_annotationType));
    TEST_ASSERT_TRUE(g_compiler.hasError);
}

static SZrType *parse_generic_annotation(SZrSyntaxContext declarationContext,
        SZrSyntaxContext useContext, TZrBool registerType) {
    SZrAstNodeArray *statements = parse_source(
            "struct Wrapper<T> { var value: T; } var holder: Wrapper<int>;");
    SZrType *annotation;

    TEST_ASSERT_EQUAL_UINT(2U, statements->count);
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_DECLARATION, statements->nodes[0]->type);
    statements->nodes[0]->data.structDeclaration.name->syntaxContext = declarationContext;
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, statements->nodes[1]->type);
    annotation = statements->nodes[1]->data.variableDeclaration.typeInfo;
    TEST_ASSERT_NOT_NULL(annotation);
    TEST_ASSERT_NOT_NULL(annotation->name);
    TEST_ASSERT_EQUAL_INT(ZR_AST_GENERIC_TYPE, annotation->name->type);
    TEST_ASSERT_NOT_NULL(annotation->name->data.genericType.name);
    annotation->name->data.genericType.name->syntaxContext = useContext;
    if (registerType) {
        register_declaration(statements->nodes[0], declarationContext);
    }
    g_compiler.suppressErrorOutput = ZR_TRUE;
    return annotation;
}

static void test_root_generic_annotation_conversion_prerequisite(void) {
    SZrType *annotation = parse_generic_annotation(ZrParser_SyntaxContext_Root(),
            ZrParser_SyntaxContext_Root(), ZR_TRUE);

    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, annotation, &g_annotationType));
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_STRING("Wrapper<int>", ZrCore_String_GetNativeString(g_annotationType.typeName));
    TEST_ASSERT_EQUAL_UINT(1U, g_annotationType.elementTypes.length);
}

static void test_root_generic_annotation_cannot_see_definition_only_declaration(void) {
    SZrType *annotation = parse_generic_annotation(g_defSite,
            ZrParser_SyntaxContext_Root(), ZR_TRUE);

    TEST_ASSERT_FALSE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, annotation, &g_annotationType));
    TEST_ASSERT_TRUE(g_compiler.hasError);
}

static void test_definition_generic_annotation_cannot_see_root_only_declaration(void) {
    SZrType *annotation = parse_generic_annotation(ZrParser_SyntaxContext_Root(),
            g_defSite, ZR_TRUE);

    TEST_ASSERT_FALSE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, annotation, &g_annotationType));
    TEST_ASSERT_TRUE(g_compiler.hasError);
}

static void test_definition_generic_annotation_uses_contextual_environment_binding(void) {
    SZrType *annotation = parse_generic_annotation(g_defSite, g_defSite, ZR_TRUE);

    g_compiler.scriptAst = ZR_NULL;
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, annotation, &g_annotationType));
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_STRING("Wrapper<int>", ZrCore_String_GetNativeString(g_annotationType.typeName));
}

static void test_definition_generic_annotation_uses_contextual_forward_declaration(void) {
    SZrType *annotation = parse_generic_annotation(g_defSite, g_defSite, ZR_FALSE);

    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, annotation, &g_annotationType));
    TEST_ASSERT_FALSE(g_compiler.hasError);
}

static const SZrNamedTypeBinding *find_annotation_binding(SZrType *annotation,
        SZrSyntaxContext context) {
    SZrString *name = annotation->name->type == ZR_AST_GENERIC_TYPE
            ? annotation->name->data.genericType.name->name
            : annotation->name->data.identifier.name;
    const SZrNamedTypeBinding *binding = ZrParser_TypeEnvironment_FindTypeBindingWithContext(
            g_compiler.typeEnv, name, context);

    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, binding->typeId);
    return binding;
}

static const SZrSemanticTypeRecord *find_type_record(TZrTypeId typeId) {
    for (TZrSize index = 0U; index < g_compiler.semanticContext->types.length; index++) {
        const SZrSemanticTypeRecord *record =
                ZrCore_Array_Get(&g_compiler.semanticContext->types, index);

        if (record != ZR_NULL && record->id == typeId) {
            return record;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(ZR_FALSE, "Registered canonical type must have a semantic record");
    return ZR_NULL;
}

static void test_root_annotation_canonical_identity_prerequisite(void) {
    SZrType *annotation = parse_annotation(ZrParser_SyntaxContext_Root(),
            ZrParser_SyntaxContext_Root(), ZR_TRUE);
    const SZrNamedTypeBinding *binding =
            find_annotation_binding(annotation, ZrParser_SyntaxContext_Root());

    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, annotation, &g_annotationType));
    TEST_ASSERT_EQUAL_UINT(binding->typeId,
            ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, &g_annotationType));
}

static void test_definition_annotation_preserves_registered_canonical_identity(void) {
    SZrType *annotation = parse_annotation(g_defSite, g_defSite, ZR_TRUE);
    const SZrNamedTypeBinding *binding = find_annotation_binding(annotation, g_defSite);

    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, annotation, &g_annotationType));
    TEST_ASSERT_EQUAL_UINT(binding->typeId,
            ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, &g_annotationType));
}

static void test_definition_semantic_record_preserves_nominal_identity(void) {
    SZrAstNode *declaration = parse_declarations()->nodes[0];
    const SZrSemanticSymbolRecord *symbol;
    const SZrSemanticTypeRecord *record;

    register_declaration(declaration, g_defSite);
    symbol = find_declaration_symbol(declaration);
    TEST_ASSERT_NOT_NULL(symbol);
    record = find_type_record(symbol->typeId);
    TEST_ASSERT_EQUAL_UINT(symbol->typeId,
            ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, &record->inferredType));
}

static void register_distinct_nominal_records(const SZrSemanticTypeRecord **definition,
        const SZrSemanticTypeRecord **root) {
    SZrAstNodeArray *declarations = parse_declarations();
    const SZrSemanticSymbolRecord *definitionSymbol;
    const SZrSemanticSymbolRecord *rootSymbol;

    register_declaration(declarations->nodes[0], g_defSite);
    register_declaration(declarations->nodes[1], ZrParser_SyntaxContext_Root());
    definitionSymbol = find_declaration_symbol(declarations->nodes[0]);
    rootSymbol = find_declaration_symbol(declarations->nodes[1]);
    TEST_ASSERT_NOT_NULL(definitionSymbol);
    TEST_ASSERT_NOT_NULL(rootSymbol);
    TEST_ASSERT_NOT_EQUAL(definitionSymbol->typeId, rootSymbol->typeId);
    *definition = find_type_record(definitionSymbol->typeId);
    *root = find_type_record(rootSymbol->typeId);
}

static void test_distinct_nominal_records_are_not_equal(void) {
    const SZrSemanticTypeRecord *definition;
    const SZrSemanticTypeRecord *root;

    register_distinct_nominal_records(&definition, &root);
    TEST_ASSERT_FALSE(ZrParser_InferredType_Equal(&definition->inferredType, &root->inferredType));
}

static void test_distinct_nominal_records_are_not_compatible(void) {
    const SZrSemanticTypeRecord *definition;
    const SZrSemanticTypeRecord *root;

    register_distinct_nominal_records(&definition, &root);
    TEST_ASSERT_FALSE(ZrParser_InferredType_IsCompatible(&definition->inferredType, &root->inferredType));
    TEST_ASSERT_FALSE(ZrParser_InferredType_IsCompatible(&root->inferredType, &definition->inferredType));
}

static void test_nullable_nominal_conversion_does_not_merge_distinct_definitions(void) {
    const SZrSemanticTypeRecord *definition;
    const SZrSemanticTypeRecord *root;

    register_distinct_nominal_records(&definition, &root);
    ZrParser_InferredType_Copy(g_state, &g_annotationType, &root->inferredType);
    g_annotationType.isNullable = ZR_TRUE;
    TEST_ASSERT_FALSE(ZrParser_InferredType_IsCompatible(&definition->inferredType, &g_annotationType));
}

static void test_definition_generic_annotation_preserves_base_canonical_identity(void) {
    SZrType *annotation = parse_generic_annotation(g_defSite, g_defSite, ZR_TRUE);
    const SZrNamedTypeBinding *binding = find_annotation_binding(annotation, g_defSite);
    const SZrCanonicalTypeNode *type;
    TZrTypeId typeId;

    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, annotation, &g_annotationType));
    typeId = ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, &g_annotationType);
    type = ZrParser_CanonicalType_Find(g_compiler.semanticContext, typeId);
    TEST_ASSERT_NOT_NULL(type);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_GENERIC_INSTANCE, type->kind);
    TEST_ASSERT_EQUAL_UINT(binding->typeId, type->data.genericInstance.definitionTypeId);
}

static void test_definition_reference_annotation_preserves_pointee_canonical_identity(void) {
    SZrType *annotation = parse_annotation(g_defSite, g_defSite, ZR_TRUE);
    const SZrNamedTypeBinding *binding = find_annotation_binding(annotation, g_defSite);
    TZrTypeId expected = ZrParser_CanonicalType_InternRef(
            g_compiler.semanticContext, binding->typeId, ZR_CANONICAL_REF_READONLY);

    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, expected);
    annotation->referenceAccess = ZR_REFERENCE_ACCESS_READONLY;
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, annotation, &g_annotationType));
    TEST_ASSERT_EQUAL_UINT(expected,
            ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, &g_annotationType));
}

static void test_definition_array_annotation_preserves_element_canonical_identity(void) {
    SZrType *annotation = parse_annotation(g_defSite, g_defSite, ZR_TRUE);
    const SZrNamedTypeBinding *binding = find_annotation_binding(annotation, g_defSite);
    TZrTypeId expected = ZrParser_CanonicalType_InternArray(g_compiler.semanticContext,
            binding->typeId, 1U, ZR_CANONICAL_ARRAY_STORAGE_MANAGED);

    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, expected);
    annotation->dimensions = 1;
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, annotation, &g_annotationType));
    TEST_ASSERT_EQUAL_UINT(expected,
            ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, &g_annotationType));
}

static void test_definition_forward_annotation_publishes_stable_nominal_binding(void) {
    SZrType *annotation = parse_annotation(g_defSite, g_defSite, ZR_FALSE);
    const SZrNamedTypeBinding *binding;

    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, annotation, &g_annotationType));
    binding = find_annotation_binding(annotation, g_defSite);
    TEST_ASSERT_EQUAL_UINT(binding->typeId,
            ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, &g_annotationType));
}

static void test_definition_annotation_copy_preserves_nominal_identity(void) {
    SZrType *annotation = parse_annotation(g_defSite, g_defSite, ZR_TRUE);
    const SZrNamedTypeBinding *binding = find_annotation_binding(annotation, g_defSite);

    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, annotation, &g_annotationType));
    ZrParser_InferredType_Copy(g_state, &g_copiedType, &g_annotationType);
    ZrParser_InferredType_Free(g_state, &g_annotationType);
    TEST_ASSERT_EQUAL_UINT(binding->typeId,
            ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, &g_copiedType));
}

static void test_nominal_identity_initialization_and_free_clear_token(void) {
    SZrType *annotation;

    TEST_ASSERT_EQUAL_UINT(0U, g_annotationType.nominalDefinitionToken);
    annotation = parse_annotation(g_defSite, g_defSite, ZR_TRUE);
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, annotation, &g_annotationType));
    TEST_ASSERT_NOT_EQUAL(0U, g_annotationType.nominalDefinitionToken);
    ZrParser_InferredType_Free(g_state, &g_annotationType);
    TEST_ASSERT_EQUAL_UINT(0U, g_annotationType.nominalDefinitionToken);
    ZrParser_InferredType_Init(g_state, &g_annotationType, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_EQUAL_UINT(0U, g_annotationType.nominalDefinitionToken);
}

static void test_nominal_adapter_rejects_token_on_primitive_or_unnamed_type(void) {
    TZrSize typeCount = g_compiler.semanticContext->canonicalTypes.length;

    g_annotationType.nominalDefinitionToken = 1U;
    TEST_ASSERT_EQUAL_UINT(ZR_SEMANTIC_ID_INVALID,
            ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, &g_annotationType));
    g_annotationType.baseType = ZR_VALUE_TYPE_INT32;
    TEST_ASSERT_EQUAL_UINT(ZR_SEMANTIC_ID_INVALID,
            ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, &g_annotationType));
    TEST_ASSERT_EQUAL_UINT(typeCount, g_compiler.semanticContext->canonicalTypes.length);
}

static void test_generic_binding_cannot_capture_resolved_nominal_identity(void) {
    SZrType *annotation = parse_annotation(g_defSite, g_defSite, ZR_TRUE);
    TZrTypeId expected = find_annotation_binding(annotation, g_defSite)->typeId;
    SZrCanonicalGenericBinding binding = {0};

    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, annotation, &g_annotationType));
    binding.name = g_annotationType.typeName;
    binding.kind = ZR_CANONICAL_GENERIC_ARGUMENT_TYPE;
    binding.typeId = ZrParser_CanonicalType_InternPrimitive(
            g_compiler.semanticContext, ZR_VALUE_TYPE_INT32);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, binding.typeId);
    TEST_ASSERT_EQUAL_UINT(expected, ZrParser_CanonicalType_FromInferredWithGenericBindings(
            g_compiler.semanticContext, &g_annotationType, &binding, 1U));
}

static void test_unmarked_nominal_keeps_explicit_generic_binding_behavior(void) {
    SZrType *annotation = parse_annotation(ZrParser_SyntaxContext_Root(),
            ZrParser_SyntaxContext_Root(), ZR_TRUE);
    SZrCanonicalGenericBinding binding = {0};

    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, annotation, &g_annotationType));
    TEST_ASSERT_EQUAL_UINT(0U, g_annotationType.nominalDefinitionToken);
    binding.name = g_annotationType.typeName;
    binding.kind = ZR_CANONICAL_GENERIC_ARGUMENT_TYPE;
    binding.typeId = ZrParser_CanonicalType_InternPrimitive(
            g_compiler.semanticContext, ZR_VALUE_TYPE_INT32);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, binding.typeId);
    TEST_ASSERT_EQUAL_UINT(binding.typeId, ZrParser_CanonicalType_FromInferredWithGenericBindings(
            g_compiler.semanticContext, &g_annotationType, &binding, 1U));
}

static void test_same_nominal_nullable_and_untyped_object_compatibility(void) {
    SZrType *annotation = parse_annotation(g_defSite, g_defSite, ZR_TRUE);

    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, annotation, &g_annotationType));
    ZrParser_InferredType_Copy(g_state, &g_copiedType, &g_annotationType);
    TEST_ASSERT_TRUE(ZrParser_InferredType_Equal(&g_annotationType, &g_copiedType));
    g_copiedType.isNullable = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_InferredType_IsCompatible(&g_annotationType, &g_copiedType));
    ZrParser_InferredType_Free(g_state, &g_copiedType);
    ZrParser_InferredType_Init(g_state, &g_copiedType, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_InferredType_IsCompatible(&g_annotationType, &g_copiedType));
}

static void test_mixed_site_annotation_preserves_root_canonical_identity(void) {
    SZrType *annotation = parse_annotation(g_mixedSite, g_mixedSite, ZR_TRUE);
    TZrTypeId expected = find_annotation_binding(annotation, g_mixedSite)->typeId;

    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, annotation, &g_annotationType));
    TEST_ASSERT_EQUAL_UINT(0U, g_annotationType.nominalDefinitionToken);
    TEST_ASSERT_EQUAL_UINT(expected,
            ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, &g_annotationType));
}

static void test_repeated_definition_annotation_does_not_publish_root_nominal(void) {
    SZrType *annotation = parse_annotation(g_defSite, g_defSite, ZR_TRUE);
    TZrTypeId expected = find_annotation_binding(annotation, g_defSite)->typeId;
    TZrSize typeCount;

    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, annotation, &g_annotationType));
    typeCount = g_compiler.semanticContext->canonicalTypes.length;
    ZrParser_InferredType_Free(g_state, &g_annotationType);
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, annotation, &g_annotationType));
    TEST_ASSERT_EQUAL_UINT(expected,
            ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, &g_annotationType));
    TEST_ASSERT_EQUAL_UINT(typeCount, g_compiler.semanticContext->canonicalTypes.length);
    TEST_ASSERT_EQUAL_UINT(1U, g_compiler.typeEnv->namedTypeBindings.length);
    TEST_ASSERT_EQUAL_UINT(0U, g_compiler.typeEnv->typeNames.length);
}

static void test_definition_nullable_projection_preserves_canonical_target(void) {
    SZrType *annotation = parse_annotation(g_defSite, g_defSite, ZR_TRUE);
    TZrTypeId target = find_annotation_binding(annotation, g_defSite)->typeId;
    TZrTypeId expected = ZrParser_CanonicalType_InternNullable(
            g_compiler.semanticContext, target);

    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, expected);
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, annotation, &g_annotationType));
    g_annotationType.isNullable = ZR_TRUE;
    TEST_ASSERT_EQUAL_UINT(expected,
            ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, &g_annotationType));
}

static void test_generic_argument_preserves_nested_nominal_identity(void) {
    SZrType *annotation = parse_annotation(g_defSite, g_defSite, ZR_TRUE);
    TZrTypeId expected = find_annotation_binding(annotation, g_defSite)->typeId;
    SZrInferredType argument;
    const SZrCanonicalTypeNode *instance;
    const SZrCanonicalGenericArgument *canonicalArgument;
    TZrTypeId typeId;

    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, annotation, &g_annotationType));
    g_copiedType.typeName = ZrCore_String_CreateFromNative(g_state, "Wrapper<Widget>");
    TEST_ASSERT_NOT_NULL(g_copiedType.typeName);
    ZrParser_InferredType_Copy(g_state, &argument, &g_annotationType);
    ZrCore_Array_Init(g_state, &g_copiedType.elementTypes, sizeof(argument), 1U);
    ZrCore_Array_Push(g_state, &g_copiedType.elementTypes, &argument);
    typeId = ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, &g_copiedType);
    instance = ZrParser_CanonicalType_Find(g_compiler.semanticContext, typeId);
    TEST_ASSERT_NOT_NULL(instance);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_GENERIC_INSTANCE, instance->kind);
    TEST_ASSERT_EQUAL_UINT(1U, instance->data.genericInstance.arguments.length);
    canonicalArgument = ZrCore_Array_Get((SZrArray *)&instance->data.genericInstance.arguments, 0U);
    TEST_ASSERT_NOT_NULL(canonicalArgument);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_GENERIC_ARGUMENT_TYPE, canonicalArgument->kind);
    TEST_ASSERT_EQUAL_UINT(expected, canonicalArgument->data.typeId);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_root_nominal_registration_prerequisite);
    RUN_TEST(test_definition_nominal_symbol_keeps_item_context);
    RUN_TEST(test_definition_nominal_does_not_publish_root_type_name);
    RUN_TEST(test_definition_and_root_nominal_symbols_are_distinct);
    RUN_TEST(test_name_only_declaration_query_selects_root_context);
    RUN_TEST(test_definition_query_selects_matching_declaration);
    RUN_TEST(test_name_only_query_rejects_definition_only_declarations);
    RUN_TEST(test_call_site_query_preserves_root_compatibility);
    RUN_TEST(test_definition_registration_is_idempotent);
    RUN_TEST(test_root_then_definition_registration_keeps_distinct_types);
    RUN_TEST(test_definition_binding_context_lookup_is_isolated);
    RUN_TEST(test_mixed_site_items_share_root_identity);
    RUN_TEST(test_child_environment_shadows_and_preserves_parent_definition);
    RUN_TEST(test_invalid_registration_context_preserves_environment);
    RUN_TEST(test_root_annotation_conversion_prerequisite);
    RUN_TEST(test_root_annotation_cannot_see_definition_only_declaration);
    RUN_TEST(test_definition_annotation_cannot_see_root_only_declaration);
    RUN_TEST(test_definition_annotation_uses_contextual_environment_binding);
    RUN_TEST(test_definition_annotation_uses_contextual_forward_declaration);
    RUN_TEST(test_call_site_annotation_keeps_root_compatibility);
    RUN_TEST(test_mixed_site_annotation_keeps_item_root_compatibility);
    RUN_TEST(test_invalid_annotation_context_rejects_root_binding);
    RUN_TEST(test_root_generic_annotation_conversion_prerequisite);
    RUN_TEST(test_root_generic_annotation_cannot_see_definition_only_declaration);
    RUN_TEST(test_definition_generic_annotation_cannot_see_root_only_declaration);
    RUN_TEST(test_definition_generic_annotation_uses_contextual_environment_binding);
    RUN_TEST(test_definition_generic_annotation_uses_contextual_forward_declaration);
    RUN_TEST(test_root_annotation_canonical_identity_prerequisite);
    RUN_TEST(test_definition_annotation_preserves_registered_canonical_identity);
    RUN_TEST(test_definition_semantic_record_preserves_nominal_identity);
    RUN_TEST(test_distinct_nominal_records_are_not_equal);
    RUN_TEST(test_distinct_nominal_records_are_not_compatible);
    RUN_TEST(test_nullable_nominal_conversion_does_not_merge_distinct_definitions);
    RUN_TEST(test_definition_generic_annotation_preserves_base_canonical_identity);
    RUN_TEST(test_definition_reference_annotation_preserves_pointee_canonical_identity);
    RUN_TEST(test_definition_array_annotation_preserves_element_canonical_identity);
    RUN_TEST(test_definition_forward_annotation_publishes_stable_nominal_binding);
    RUN_TEST(test_definition_annotation_copy_preserves_nominal_identity);
    RUN_TEST(test_nominal_identity_initialization_and_free_clear_token);
    RUN_TEST(test_nominal_adapter_rejects_token_on_primitive_or_unnamed_type);
    RUN_TEST(test_generic_binding_cannot_capture_resolved_nominal_identity);
    RUN_TEST(test_unmarked_nominal_keeps_explicit_generic_binding_behavior);
    RUN_TEST(test_same_nominal_nullable_and_untyped_object_compatibility);
    RUN_TEST(test_mixed_site_annotation_preserves_root_canonical_identity);
    RUN_TEST(test_repeated_definition_annotation_does_not_publish_root_nominal);
    RUN_TEST(test_definition_nullable_projection_preserves_canonical_target);
    RUN_TEST(test_generic_argument_preserves_nested_nominal_identity);
    return UNITY_END();
}
