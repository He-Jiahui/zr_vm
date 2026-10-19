#include "unity.h"

#include <stdio.h>
#include <string.h>

#include "compiler/compile_time/compile_time_executor_internal.h"
#include "compiler/compiler_internal.h"
#include "compiler/compiler_type_prototype_lookup.h"
#include "harness/runtime_support.h"
#include "type_inference/type_inference_internal.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/parser.h"

static SZrState *g_state;
static SZrCompilerState g_compiler;
static TZrBool g_compilerInitialized;
static SZrAstNode *g_ast;
static SZrSyntaxContextTable *g_table;
static SZrSyntaxContext g_definition;
static SZrInferredType g_element;
static SZrInferredType g_result;

typedef struct SAnnotationFixture {
    SZrAstNode *owner;
    SZrType *annotation;
    SZrAstNode *extent;
    SZrAstNode *constant;
    SZrAstNode *functionOwner;
    SZrAstNode *shadowOwner;
    SZrNamedTypeBinding expected;
    TZrSymbolId expectedToken;
} SAnnotationFixture;

void setUp(void) {
    SZrSyntaxExpansionInfo expansion = {0};
    TZrSyntaxExpansionId expansionId;
    SZrScope scope = {0};

    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
    ZrParser_InferredType_Init(g_state, &g_element, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &g_result, ZR_VALUE_TYPE_OBJECT);
    g_table = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_table);
    expansion.stableIdentity = 0xB801U;
    expansion.callSiteSpan = ZrParser_TokenSpan_None();
    expansion.definitionSiteSpan = ZrParser_TokenSpan_None();
    TEST_ASSERT_TRUE(ZrParser_SyntaxContextTable_RegisterExpansion(
            g_table, &expansion, &expansionId));
    g_definition = ZrParser_SyntaxContext_DefSite(g_table, expansionId);
    ZrParser_CompilerState_Init(&g_compiler, g_state);
    g_compilerInitialized = ZR_TRUE;
    g_compiler.suppressErrorOutput = ZR_TRUE;
    g_compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(g_compiler.currentFunction);
    g_compiler.isScriptLevel = ZR_TRUE;
    scope.parentCompiler = &g_compiler;
    ZrCore_Array_Push(g_state, &g_compiler.scopeStack, &scope);
}

void tearDown(void) {
    ZrParser_InferredType_Free(g_state, &g_element);
    ZrParser_InferredType_Free(g_state, &g_result);
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

static SAnnotationFixture prepare_annotation(const TZrChar *extent,
        TZrBool marked, const TZrChar *constantValue) {
    TZrChar source[512];
    SZrString *sourceName;
    SZrAstNodeArray *nodes;
    SZrAstNode *field;
    SZrGenericDeclaration *generic;
    SZrType *expectedAnnotation;
    const SZrNamedTypeBinding *root;
    const SZrNamedTypeBinding *binding;
    SZrNamedTypeBinding rootCopy;
    SAnnotationFixture fixture = {0};
    int length = snprintf(source, sizeof(source),
            "struct Widget {} struct Widget {} "
            "class Holder<T, const N: int> { pub var value: Widget[%s]; } "
            "var expected: Widget; const N = %s; "
            "fn inner<const N: int>(): int { return 0; } "
            "fn inner_type<N>(): int { return 0; }", extent, constantValue);

    TEST_ASSERT_TRUE(length > 0 && (TZrSize)length < sizeof(source));
    sourceName = ZrCore_String_CreateFromNative(g_state, "open_array_annotation.zr");
    g_ast = ZrParser_Parse(g_state, source, (TZrSize)length, sourceName);
    TEST_ASSERT_NOT_NULL(g_ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_ast->type);
    nodes = g_ast->data.script.statements;
    TEST_ASSERT_NOT_NULL(nodes);
    TEST_ASSERT_EQUAL_UINT(7U, nodes->count);
    g_compiler.scriptAst = g_ast;
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_DECLARATION, nodes->nodes[0]->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_DECLARATION, nodes->nodes[1]->type);
    ZrParser_Compiler_CompileStructDeclaration(&g_compiler, nodes->nodes[0]);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    root = ZrParser_TypeEnvironment_FindTypeBindingWithContext(
            g_compiler.typeEnv, nodes->nodes[0]->data.structDeclaration.name->name,
            ZrParser_SyntaxContext_Root());
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_EQUAL_PTR(nodes->nodes[0], root->declarationNode);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, root->typeId);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, root->symbolId);
    rootCopy = *root;
    nodes->nodes[1]->data.structDeclaration.name->syntaxContext = g_definition;
    ZrParser_Compiler_CompileStructDeclaration(&g_compiler, nodes->nodes[1]);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    binding = ZrParser_TypeEnvironment_FindTypeBindingWithContext(
            g_compiler.typeEnv, nodes->nodes[1]->data.structDeclaration.name->name,
            g_definition);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_PTR(nodes->nodes[1], binding->declarationNode);
    TEST_ASSERT_NOT_EQUAL(rootCopy.typeId, binding->typeId);
    fixture.expected = marked ? *binding : rootCopy;
    fixture.expectedToken = marked ? binding->symbolId : 0U;
    fixture.owner = nodes->nodes[2];
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, fixture.owner->type);
    generic = fixture.owner->data.classDeclaration.generic;
    TEST_ASSERT_NOT_NULL(generic);
    TEST_ASSERT_NOT_NULL(generic->params);
    TEST_ASSERT_EQUAL_UINT(2U, generic->params->count);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PARAMETER, generic->params->nodes[1]->type);
    TEST_ASSERT_EQUAL_INT(ZR_GENERIC_PARAMETER_CONST_INT,
            generic->params->nodes[1]->data.parameter.genericKind);
    TEST_ASSERT_EQUAL_STRING("N", ZrCore_String_GetNativeString(
            generic->params->nodes[1]->data.parameter.name->name));
    TEST_ASSERT_NOT_NULL(fixture.owner->data.classDeclaration.members);
    TEST_ASSERT_EQUAL_UINT(1U, fixture.owner->data.classDeclaration.members->count);
    field = fixture.owner->data.classDeclaration.members->nodes[0];
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_FIELD, field->type);
    fixture.annotation = field->data.classField.typeInfo;
    TEST_ASSERT_NOT_NULL(fixture.annotation);
    TEST_ASSERT_EQUAL_INT(1, fixture.annotation->dimensions);
    TEST_ASSERT_TRUE(fixture.annotation->hasArraySizeConstraint);
    fixture.extent = fixture.annotation->arraySizeExpression;
    if (strcmp(extent, "0") == 0) {
        TEST_ASSERT_NULL(fixture.extent);
        TEST_ASSERT_EQUAL_UINT(0U, fixture.annotation->arrayFixedSize);
        TEST_ASSERT_EQUAL_UINT(0U, fixture.annotation->arrayMinSize);
        TEST_ASSERT_EQUAL_UINT(0U, fixture.annotation->arrayMaxSize);
    } else {
        TEST_ASSERT_NOT_NULL(fixture.extent);
    }
    if (strcmp(extent, "N") == 0 || strcmp(extent, "((N))") == 0) {
        TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, fixture.extent->type);
        TEST_ASSERT_EQUAL_STRING("N", ZrCore_String_GetNativeString(
                fixture.extent->data.identifier.name));
    }
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, fixture.annotation->name->type);
    fixture.annotation->name->data.identifier.syntaxContext = marked ?
            g_definition : ZrParser_SyntaxContext_Root();
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, nodes->nodes[3]->type);
    expectedAnnotation = nodes->nodes[3]->data.variableDeclaration.typeInfo;
    TEST_ASSERT_NOT_NULL(expectedAnnotation);
    expectedAnnotation->name->data.identifier.syntaxContext = marked ?
            g_definition : ZrParser_SyntaxContext_Root();
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, expectedAnnotation, &g_element));
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_UINT32(fixture.expectedToken, g_element.nominalDefinitionToken);
    TEST_ASSERT_EQUAL_UINT32(fixture.expected.typeId, ZrParser_CanonicalType_FromInferred(
            g_compiler.semanticContext, &g_element));
    fixture.constant = nodes->nodes[4];
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, fixture.constant->type);
    fixture.functionOwner = nodes->nodes[5];
    fixture.shadowOwner = nodes->nodes[6];
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, fixture.functionOwner->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, fixture.shadowOwner->type);
    TEST_ASSERT_NOT_NULL(fixture.functionOwner->data.functionDeclaration.generic);
    TEST_ASSERT_NOT_NULL(fixture.shadowOwner->data.functionDeclaration.generic);
    TEST_ASSERT_NOT_NULL(fixture.functionOwner->data.functionDeclaration.generic->params);
    TEST_ASSERT_NOT_NULL(fixture.shadowOwner->data.functionDeclaration.generic->params);
    TEST_ASSERT_EQUAL_UINT(1U, fixture.functionOwner->data.functionDeclaration.generic->params->count);
    TEST_ASSERT_EQUAL_UINT(1U, fixture.shadowOwner->data.functionDeclaration.generic->params->count);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PARAMETER,
            fixture.functionOwner->data.functionDeclaration.generic->params->nodes[0]->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PARAMETER,
            fixture.shadowOwner->data.functionDeclaration.generic->params->nodes[0]->type);
    TEST_ASSERT_NOT_NULL(fixture.functionOwner->data.functionDeclaration.generic->params->nodes[0]->data.parameter.name);
    TEST_ASSERT_NOT_NULL(fixture.shadowOwner->data.functionDeclaration.generic->params->nodes[0]->data.parameter.name);
    TEST_ASSERT_EQUAL_STRING("N", ZrCore_String_GetNativeString(
            fixture.functionOwner->data.functionDeclaration.generic->params->nodes[0]->data.parameter.name->name));
    TEST_ASSERT_EQUAL_STRING("N", ZrCore_String_GetNativeString(
            fixture.shadowOwner->data.functionDeclaration.generic->params->nodes[0]->data.parameter.name->name));
    TEST_ASSERT_EQUAL_INT(ZR_GENERIC_PARAMETER_CONST_INT,
            fixture.functionOwner->data.functionDeclaration.generic->params->nodes[0]->data.parameter.genericKind);
    TEST_ASSERT_EQUAL_INT(ZR_GENERIC_PARAMETER_TYPE,
            fixture.shadowOwner->data.functionDeclaration.generic->params->nodes[0]->data.parameter.genericKind);
    g_compiler.currentTypeNode = fixture.owner;
    g_compiler.currentTypeName = fixture.owner->data.classDeclaration.name->name;
    TEST_ASSERT_TRUE(type_inference_is_const_generic_parameter_reference(
            &g_compiler, generic->params->nodes[1]->data.parameter.name->name));
    TEST_ASSERT_FALSE(type_inference_is_const_generic_parameter_reference(
            &g_compiler, generic->params->nodes[0]->data.parameter.name->name));
    return fixture;
}

static void assert_array_value(const SZrInferredType *type,
        const SAnnotationFixture *fixture, TZrSize fixedSize) {
    const SZrInferredType *element;

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, type->baseType);
    TEST_ASSERT_EQUAL_UINT(1U, type->elementTypes.length);
    TEST_ASSERT_TRUE(type->hasArraySizeConstraint);
    TEST_ASSERT_EQUAL_UINT(fixedSize, type->arrayFixedSize);
    TEST_ASSERT_EQUAL_UINT(fixedSize, type->arrayMinSize);
    TEST_ASSERT_EQUAL_UINT(fixedSize, type->arrayMaxSize);
    element = ZrCore_Array_Get((SZrArray *)&type->elementTypes, 0U);
    TEST_ASSERT_NOT_NULL(element);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, element->baseType);
    TEST_ASSERT_EQUAL_STRING("Widget", ZrCore_String_GetNativeString(element->typeName));
    TEST_ASSERT_EQUAL_UINT32(fixture->expectedToken, element->nominalDefinitionToken);
    TEST_ASSERT_EQUAL_UINT32(fixture->expected.typeId,
            ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, element));
    TEST_ASSERT_EQUAL_PTR(fixture->extent, fixture->annotation->arraySizeExpression);
}

static void assert_conversion(const TZrChar *extent, TZrBool marked, TZrSize size) {
    SAnnotationFixture fixture = prepare_annotation(extent, marked, "4");

    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, fixture.annotation, &g_result));
    TEST_ASSERT_FALSE(g_compiler.hasError);
    assert_array_value(&g_result, &fixture, size);
}

static void assert_rejection(const TZrChar *extent, const TZrChar *error,
        TZrBool insideOwner) {
    SAnnotationFixture fixture = prepare_annotation(extent, ZR_TRUE, "4");

    if (!insideOwner) {
        g_compiler.currentTypeNode = ZR_NULL;
        g_compiler.currentTypeName = ZR_NULL;
    }
    TEST_ASSERT_FALSE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, fixture.annotation, &g_result));
    TEST_ASSERT_TRUE(g_compiler.hasError);
    TEST_ASSERT_NOT_NULL(g_compiler.errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(g_compiler.errorMessage, error));
    TEST_ASSERT_EQUAL_PTR(fixture.extent, fixture.annotation->arraySizeExpression);
}

static void assert_bound_constant(const TZrChar *value, TZrBool accepted,
        const TZrChar *error) {
    SAnnotationFixture fixture = prepare_annotation("N", ZR_TRUE, value);

    TEST_ASSERT_TRUE(register_compile_time_variable_declaration(
            &g_compiler, fixture.constant, fixture.constant->location));
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_NOT_NULL(find_compile_time_variable_with_context(&g_compiler,
            fixture.extent->data.identifier.name,
            fixture.extent->data.identifier.syntaxContext));
    TEST_ASSERT_EQUAL_INT(accepted, ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, fixture.annotation, &g_result));
    if (accepted) {
        TEST_ASSERT_FALSE(g_compiler.hasError);
        assert_array_value(&g_result, &fixture, 4U);
    } else {
        TEST_ASSERT_TRUE(g_compiler.hasError);
        TEST_ASSERT_NOT_NULL(g_compiler.errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(g_compiler.errorMessage, error));
    }
}

static void assert_source_capture(TZrBool marked) {
    SAnnotationFixture fixture = prepare_annotation("N", marked, "4");
    const SZrNamedTypeBinding *binding;
    SZrTypePrototypeInfo *prototype;
    SZrTypeMemberInfo *member;

    g_compiler.currentTypeNode = ZR_NULL;
    g_compiler.currentTypeName = ZR_NULL;
    ZrParser_Compiler_CompileClassDeclaration(&g_compiler, fixture.owner);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    binding = ZrParser_TypeEnvironment_FindTypeBindingWithContext(g_compiler.typeEnv,
            fixture.owner->data.classDeclaration.name->name, ZrParser_SyntaxContext_Root());
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_PTR(fixture.owner, binding->declarationNode);
    prototype = find_compiler_type_prototype(&g_compiler, binding->name);
    TEST_ASSERT_NOT_NULL(prototype);
    TEST_ASSERT_EQUAL_PTR(fixture.owner, prototype->declarationNode);
    TEST_ASSERT_EQUAL_UINT(1U, prototype->members.length);
    member = ZrCore_Array_Get(&prototype->members, 0U);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_FIELD, member->memberType);
    TEST_ASSERT_EQUAL_PTR(fixture.annotation, member->fieldType);
    TEST_ASSERT_NULL(member->returnTypeName);
    TEST_ASSERT_TRUE(member->hasStructuredReturnType);
    assert_array_value(&member->structuredReturnType, &fixture, 0U);
}

static void test_open_extent_keeps_root_element(void) { assert_conversion("N", ZR_FALSE, 0U); }
static void test_open_extent_keeps_marked_element(void) { assert_conversion("N", ZR_TRUE, 0U); }
static void test_parenthesized_open_extent(void) { assert_conversion("((N))", ZR_TRUE, 0U); }
static void test_concrete_expression_extent(void) { assert_conversion("2 + 2", ZR_TRUE, 4U); }
static void test_concrete_zero_extent(void) { assert_conversion("0", ZR_TRUE, 0U); }
static void test_unknown_extent_is_rejected(void) {
    assert_rejection("Missing", "Unknown compile-time identifier: Missing", ZR_TRUE);
}
static void test_out_of_owner_extent_is_rejected(void) {
    assert_rejection("N", "Unknown compile-time identifier: N", ZR_FALSE);
}
static void test_type_parameter_extent_is_rejected(void) {
    assert_rejection("T", "Unknown compile-time identifier: T", ZR_TRUE);
}
static void test_negative_extent_is_rejected(void) {
    assert_rejection("-1", "must evaluate to a non-negative integer", ZR_TRUE);
}
static void test_noninteger_extent_is_rejected(void) {
    assert_rejection("true", "must evaluate to an integer", ZR_TRUE);
}
static void test_open_arithmetic_extent_is_not_claimed(void) {
    assert_rejection("N + 1", "Unknown compile-time identifier: N", ZR_TRUE);
}
static void test_invalid_extent_context_is_rejected(void) {
    SAnnotationFixture fixture = prepare_annotation("N", ZR_TRUE, "4");
    fixture.extent->data.identifier.syntaxContext = (SZrSyntaxContext){g_table, ~0U};
    TEST_ASSERT_FALSE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, fixture.annotation, &g_result));
}
static void test_other_extent_context_is_rejected(void) {
    SAnnotationFixture fixture = prepare_annotation("N", ZR_TRUE, "4");
    fixture.extent->data.identifier.syntaxContext = g_definition;
    TEST_ASSERT_FALSE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, fixture.annotation, &g_result));
}
static void test_bound_const_value_takes_precedence(void) { assert_bound_constant("4", ZR_TRUE, ZR_NULL); }
static void test_bound_negative_value_is_not_deferred(void) {
    assert_bound_constant("-1", ZR_FALSE, "must evaluate to a non-negative integer");
}
static void test_bound_noninteger_value_is_not_deferred(void) {
    assert_bound_constant("true", ZR_FALSE, "must evaluate to an integer");
}
static void test_root_open_field_is_published(void) { assert_source_capture(ZR_FALSE); }
static void test_marked_open_field_is_published(void) { assert_source_capture(ZR_TRUE); }

static void test_function_const_owner_defers_extent(void) {
    SAnnotationFixture fixture = prepare_annotation("N", ZR_TRUE, "4");

    g_compiler.currentTypeNode = ZR_NULL;
    g_compiler.currentTypeName = ZR_NULL;
    g_compiler.currentFunctionNode = fixture.functionOwner;
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, fixture.annotation, &g_result));
    TEST_ASSERT_FALSE(g_compiler.hasError);
    assert_array_value(&g_result, &fixture, 0U);
}

static void test_nearer_type_parameter_blocks_outer_const(void) {
    SAnnotationFixture fixture = prepare_annotation("N", ZR_TRUE, "4");

    g_compiler.currentFunctionNode = fixture.shadowOwner;
    TEST_ASSERT_FALSE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, fixture.annotation, &g_result));
    TEST_ASSERT_TRUE(g_compiler.hasError);
    TEST_ASSERT_NOT_NULL(g_compiler.errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(g_compiler.errorMessage, "Unknown compile-time identifier: N"));
}

static void test_different_nearer_context_does_not_shadow_outer_const(void) {
    SAnnotationFixture fixture = prepare_annotation("N", ZR_TRUE, "4");

    fixture.shadowOwner->data.functionDeclaration.generic->params->nodes[0]->
            data.parameter.name->syntaxContext = g_definition;
    g_compiler.currentFunctionNode = fixture.shadowOwner;
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, fixture.annotation, &g_result));
    TEST_ASSERT_FALSE(g_compiler.hasError);
    assert_array_value(&g_result, &fixture, 0U);
}

static void test_matching_opaque_parameter_context_defers(void) {
    SAnnotationFixture fixture = prepare_annotation("N", ZR_TRUE, "4");

    fixture.owner->data.classDeclaration.generic->params->nodes[1]->
            data.parameter.name->syntaxContext = g_definition;
    fixture.extent->data.identifier.syntaxContext = g_definition;
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, fixture.annotation, &g_result));
    TEST_ASSERT_FALSE(g_compiler.hasError);
    assert_array_value(&g_result, &fixture, 0U);
}

static void test_invalid_parameter_context_cannot_defer(void) {
    SAnnotationFixture fixture = prepare_annotation("N", ZR_TRUE, "4");

    fixture.owner->data.classDeclaration.generic->params->nodes[1]->
            data.parameter.name->syntaxContext = (SZrSyntaxContext){g_table, ~0U};
    TEST_ASSERT_FALSE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, fixture.annotation, &g_result));
    TEST_ASSERT_TRUE(g_compiler.hasError);
    TEST_ASSERT_NOT_NULL(g_compiler.errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(g_compiler.errorMessage, "Invalid array size expression context"));
}

static void test_other_context_const_binding_does_not_capture_extent(void) {
    SAnnotationFixture fixture = prepare_annotation("N", ZR_TRUE, "4");

    fixture.constant->data.variableDeclaration.pattern->data.identifier.syntaxContext = g_definition;
    TEST_ASSERT_TRUE(register_compile_time_variable_declaration(
            &g_compiler, fixture.constant, fixture.constant->location));
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_NOT_NULL(find_compile_time_variable_with_context(&g_compiler,
            fixture.extent->data.identifier.name, g_definition));
    TEST_ASSERT_NULL(find_compile_time_variable_with_context(&g_compiler,
            fixture.extent->data.identifier.name, ZrParser_SyntaxContext_Root()));
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, fixture.annotation, &g_result));
    TEST_ASSERT_FALSE(g_compiler.hasError);
    assert_array_value(&g_result, &fixture, 0U);
}

static void test_invalid_const_binding_context_cannot_be_absent(void) {
    SAnnotationFixture fixture = prepare_annotation("N", ZR_TRUE, "4");
    SZrCompileTimeVariable *variable;

    TEST_ASSERT_TRUE(register_compile_time_variable_declaration(
            &g_compiler, fixture.constant, fixture.constant->location));
    TEST_ASSERT_FALSE(g_compiler.hasError);
    variable = find_compile_time_variable_with_context(&g_compiler,
            fixture.extent->data.identifier.name, ZrParser_SyntaxContext_Root());
    TEST_ASSERT_NOT_NULL(variable);
    variable->syntaxContext = (SZrSyntaxContext){g_table, ~0U};
    TEST_ASSERT_FALSE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, fixture.annotation, &g_result));
    TEST_ASSERT_TRUE(g_compiler.hasError);
    TEST_ASSERT_NOT_NULL(g_compiler.errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(g_compiler.errorMessage, "Invalid array size expression context"));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_open_extent_keeps_root_element);
    RUN_TEST(test_open_extent_keeps_marked_element);
    RUN_TEST(test_parenthesized_open_extent);
    RUN_TEST(test_concrete_expression_extent);
    RUN_TEST(test_concrete_zero_extent);
    RUN_TEST(test_unknown_extent_is_rejected);
    RUN_TEST(test_out_of_owner_extent_is_rejected);
    RUN_TEST(test_type_parameter_extent_is_rejected);
    RUN_TEST(test_negative_extent_is_rejected);
    RUN_TEST(test_noninteger_extent_is_rejected);
    RUN_TEST(test_open_arithmetic_extent_is_not_claimed);
    RUN_TEST(test_invalid_extent_context_is_rejected);
    RUN_TEST(test_other_extent_context_is_rejected);
    RUN_TEST(test_bound_const_value_takes_precedence);
    RUN_TEST(test_bound_negative_value_is_not_deferred);
    RUN_TEST(test_bound_noninteger_value_is_not_deferred);
    RUN_TEST(test_root_open_field_is_published);
    RUN_TEST(test_marked_open_field_is_published);
    RUN_TEST(test_function_const_owner_defers_extent);
    RUN_TEST(test_nearer_type_parameter_blocks_outer_const);
    RUN_TEST(test_different_nearer_context_does_not_shadow_outer_const);
    RUN_TEST(test_matching_opaque_parameter_context_defers);
    RUN_TEST(test_invalid_parameter_context_cannot_defer);
    RUN_TEST(test_other_context_const_binding_does_not_capture_extent);
    RUN_TEST(test_invalid_const_binding_context_cannot_be_absent);
    return UNITY_END();
}
