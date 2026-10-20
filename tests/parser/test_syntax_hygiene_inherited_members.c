#include "unity.h"

#include <stdio.h>
#include <string.h>

#include "compiler/compiler_internal.h"
#include "harness/runtime_support.h"
#include "type_inference/type_inference_internal.h"
#include "type_inference/type_inference_nominal_member.h"
#include "type_inference/type_inference_nominal_prototype.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_query.h"

static SZrState *g_state;
static SZrCompilerState g_compiler;
static TZrBool g_compilerInitialized;
static SZrAstNode *g_ast;
static SZrSyntaxContextTable *g_table;
static SZrSyntaxContext g_definition;
static SZrSyntaxContext g_mixed;
static SZrInferredType g_types[4];
static SZrArray g_relations;

void setUp(void) {
    SZrSyntaxExpansionInfo expansion = {0};
    TZrSyntaxExpansionId expansionId;
    SZrScope scope = {0};

    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
    for (TZrSize index = 0U; index < 4U; index++) {
        ZrParser_InferredType_Init(g_state, &g_types[index], ZR_VALUE_TYPE_OBJECT);
    }
    ZrCore_Array_Construct(&g_relations);
    g_table = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_table);
    expansion.stableIdentity = 0xB501U;
    expansion.callSiteSpan = ZrParser_TokenSpan_None();
    expansion.definitionSiteSpan = ZrParser_TokenSpan_None();
    TEST_ASSERT_TRUE(ZrParser_SyntaxContextTable_RegisterExpansion(
            g_table, &expansion, &expansionId));
    g_definition = ZrParser_SyntaxContext_DefSite(g_table, expansionId);
    g_mixed = ZrParser_SyntaxContext_MixedSite(g_table, expansionId);
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
    for (TZrSize index = 0U; index < 4U; index++) {
        ZrParser_InferredType_Free(g_state, &g_types[index]);
    }
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
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

static SZrNamedTypeBinding publish_class(SZrAstNode *node, SZrSyntaxContext context) {
    const SZrNamedTypeBinding *binding;
    const SZrCanonicalTypeNode *nominal;

    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, node->type);
    TEST_ASSERT_NOT_NULL(node->data.classDeclaration.name);
    node->data.classDeclaration.name->syntaxContext = context;
    ZrParser_Compiler_CompileClassDeclaration(&g_compiler, node);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    binding = ZrParser_TypeEnvironment_FindTypeBindingWithContext(g_compiler.typeEnv,
            node->data.classDeclaration.name->name, context);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_PTR(node, binding->declarationNode);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, binding->symbolId);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, binding->typeId);
    nominal = ZrParser_CanonicalType_Find(g_compiler.semanticContext, binding->typeId);
    TEST_ASSERT_NOT_NULL(nominal);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_NOMINAL, nominal->kind);
    return *binding;
}

static void assert_inherited_receiver(SZrSyntaxContext context, TZrBool sameName,
        TZrBool isMethod, TZrBool publicEntry) {
    TZrChar source[512];
    const TZrChar *baseName = sameName ? "Widget" : "Base";
    SZrString *sourceName;
    int written;
    SZrAstNodeArray *nodes;
    SZrAstNode *baseDeclaration;
    SZrAstNode *derivedDeclaration;
    SZrAstNode *variable;
    SZrAstNode *expression;
    SZrAstNode *identifier;
    SZrAstNode *baseAnnotation;
    SZrType *receiverAnnotation;
    SZrNamedTypeBinding baseBinding;
    SZrNamedTypeBinding derivedBinding;
    const SZrNamedTypeBinding *resolvedBase;
    const SZrCanonicalTypeNode *derivedNominal;
    const SZrParserSemanticRelationQuery *relation;
    const SZrTypeBinding *variableBinding;
    SZrTypePrototypeInfo *prototype;
    SZrTypeMemberInfo *member;
    SZrInferredType *parameter;
    TZrSymbolId receiverToken;

    written = snprintf(source, sizeof(source),
            "class %s { %s } class Widget: %s {} var receiver: Widget; receiver.%s;",
            baseName,
            isMethod ? "pub fn read(value: int): int { return value; }" : "pub var value: int;",
            baseName, isMethod ? "read(1)" : "value");
    TEST_ASSERT_GREATER_THAN_INT(0, written);
    TEST_ASSERT_LESS_THAN_UINT(sizeof(source), (TZrSize)written);
    sourceName = ZrCore_String_CreateFromNative(g_state, "inherited_hygiene.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    g_ast = ZrParser_Parse(g_state, source, (TZrSize)written, sourceName);
    TEST_ASSERT_NOT_NULL(g_ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_ast->type);
    g_compiler.scriptAst = g_ast;
    nodes = g_ast->data.script.statements;
    TEST_ASSERT_NOT_NULL(nodes);
    TEST_ASSERT_EQUAL_UINT(4U, nodes->count);
    baseDeclaration = nodes->nodes[0];
    derivedDeclaration = nodes->nodes[1];
    variable = nodes->nodes[2];
    baseBinding = publish_class(baseDeclaration, ZrParser_SyntaxContext_Root());
    derivedBinding = publish_class(derivedDeclaration, context);
    TEST_ASSERT_NOT_EQUAL(baseBinding.symbolId, derivedBinding.symbolId);
    TEST_ASSERT_NOT_EQUAL(baseBinding.typeId, derivedBinding.typeId);
    resolvedBase = ZrParser_TypeEnvironment_FindTypeBindingWithContext(g_compiler.typeEnv,
            baseDeclaration->data.classDeclaration.name->name, ZrParser_SyntaxContext_Root());
    TEST_ASSERT_NOT_NULL(resolvedBase);
    TEST_ASSERT_EQUAL_PTR(baseDeclaration, resolvedBase->declarationNode);
    TEST_ASSERT_EQUAL_UINT32(baseBinding.typeId, resolvedBase->typeId);
    derivedNominal = ZrParser_CanonicalType_Find(g_compiler.semanticContext, derivedBinding.typeId);
    TEST_ASSERT_NOT_NULL(derivedNominal);
    receiverToken = derivedNominal->data.nominal.definitionToken;

    TEST_ASSERT_NOT_NULL(derivedDeclaration->data.classDeclaration.inherits);
    TEST_ASSERT_EQUAL_UINT(1U, derivedDeclaration->data.classDeclaration.inherits->count);
    baseAnnotation = derivedDeclaration->data.classDeclaration.inherits->nodes[0];
    TEST_ASSERT_NOT_NULL(baseAnnotation);
    TEST_ASSERT_EQUAL_INT(ZR_AST_TYPE, baseAnnotation->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, baseAnnotation->data.type.name->type);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Equal(ZrParser_SyntaxContext_Root(),
            baseAnnotation->data.type.name->data.identifier.syntaxContext));
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, &baseAnnotation->data.type, &g_types[3]));
    TEST_ASSERT_EQUAL_UINT32(0U, g_types[3].nominalDefinitionToken);
    TEST_ASSERT_EQUAL_UINT32(baseBinding.typeId,
            ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, &g_types[3]));
    prototype = type_inference_find_nominal_prototype(&g_compiler, &g_types[3]);
    TEST_ASSERT_NOT_NULL(prototype);
    TEST_ASSERT_EQUAL_PTR(baseDeclaration, prototype->declarationNode);
    TEST_ASSERT_EQUAL_UINT(1U, prototype->members.length);
    member = ZrCore_Array_Get(&prototype->members, 0U);
    TEST_ASSERT_NOT_NULL(member);
    TEST_ASSERT_EQUAL_PTR(baseDeclaration->data.classDeclaration.members->nodes[0],
            member->declarationNode);
    if (isMethod) {
        TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_METHOD, member->memberType);
        TEST_ASSERT_NOT_NULL(member->compiledFunction);
        TEST_ASSERT_EQUAL_UINT(1U, member->parameterTypes.length);
        parameter = ZrCore_Array_Get(&member->parameterTypes, 0U);
        TEST_ASSERT_NOT_NULL(parameter);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, parameter->baseType);
        TEST_ASSERT_TRUE(member->hasStructuredReturnType);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, member->structuredReturnType.baseType);
    } else {
        TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_FIELD, member->memberType);
        TEST_ASSERT_EQUAL_PTR(baseDeclaration->data.classDeclaration.members->nodes[0]
                ->data.classField.typeInfo, member->fieldType);
        TEST_ASSERT_EQUAL_STRING("int", ZrCore_String_GetNativeString(member->fieldTypeName));
    }

    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_BaseTypesOf(g_compiler.semanticContext,
            derivedBinding.typeId, &g_relations));
    TEST_ASSERT_EQUAL_UINT(1U, g_relations.length);
    relation = ZrCore_Array_Get(&g_relations, 0U);
    TEST_ASSERT_NOT_NULL(relation);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_RELATION_BASE_TYPE, relation->kind);
    TEST_ASSERT_EQUAL_UINT32(derivedBinding.symbolId, relation->sourceSymbolId);
    TEST_ASSERT_EQUAL_UINT32(derivedBinding.typeId, relation->sourceTypeId);
    TEST_ASSERT_EQUAL_UINT32(baseBinding.symbolId, relation->targetSymbolId);
    TEST_ASSERT_EQUAL_UINT32(baseBinding.typeId, relation->targetTypeId);

    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, variable->type);
    receiverAnnotation = variable->data.variableDeclaration.typeInfo;
    TEST_ASSERT_NOT_NULL(receiverAnnotation);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, receiverAnnotation->name->type);
    receiverAnnotation->name->data.identifier.syntaxContext = context;
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, receiverAnnotation, &g_types[0]));
    TEST_ASSERT_EQUAL_UINT32(receiverToken, g_types[0].nominalDefinitionToken);
    TEST_ASSERT_EQUAL_UINT32(derivedBinding.typeId,
            ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, &g_types[0]));
    prototype = type_inference_find_nominal_prototype(&g_compiler, &g_types[0]);
    TEST_ASSERT_NOT_NULL(prototype);
    TEST_ASSERT_EQUAL_PTR(derivedDeclaration, prototype->declarationNode);
    TEST_ASSERT_EQUAL_UINT(0U, prototype->members.length);
    TEST_ASSERT_EQUAL_UINT(1U, prototype->inherits.length);
    TEST_ASSERT_EQUAL_STRING(baseName,
            ZrCore_String_GetNativeString(*(SZrString **)ZrCore_Array_Get(&prototype->inherits, 0U)));
    TEST_ASSERT_EQUAL_STRING(baseName, ZrCore_String_GetNativeString(prototype->extendsTypeName));
    identifier = variable->data.variableDeclaration.pattern;
    TEST_ASSERT_NOT_NULL(identifier);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, identifier->type);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariableExWithContext(g_state,
            g_compiler.typeEnv, identifier->data.identifier.name, &g_types[0], variable,
            variable->location, identifier->data.identifier.syntaxContext));
    variableBinding = ZrParser_TypeEnvironment_FindVariableBindingWithContext(
            g_compiler.typeEnv, identifier->data.identifier.name,
            identifier->data.identifier.syntaxContext);
    TEST_ASSERT_NOT_NULL(variableBinding);
    TEST_ASSERT_EQUAL_UINT32(receiverToken, variableBinding->type.nominalDefinitionToken);
    TEST_ASSERT_EQUAL_INT(ZR_AST_EXPRESSION_STATEMENT, nodes->nodes[3]->type);
    expression = nodes->nodes[3]->data.expressionStatement.expr;
    TEST_ASSERT_NOT_NULL(expression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, expression->type);
    TEST_ASSERT_NOT_NULL(expression->data.primaryExpression.members);
    TEST_ASSERT_EQUAL_UINT(isMethod ? 2U : 1U,
            expression->data.primaryExpression.members->count);
    {
        SZrAstNode *access = expression->data.primaryExpression.members->nodes[0];

        TEST_ASSERT_NOT_NULL(access);
        TEST_ASSERT_EQUAL_INT(ZR_AST_MEMBER_EXPRESSION, access->type);
        TEST_ASSERT_FALSE(access->data.memberExpression.computed);
        TEST_ASSERT_NOT_NULL(access->data.memberExpression.property);
        TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL,
                access->data.memberExpression.property->type);
        TEST_ASSERT_EQUAL_STRING(isMethod ? "read" : "value", ZrCore_String_GetNativeString(
                access->data.memberExpression.property->data.identifier.name));
        if (isMethod) {
            SZrAstNode *call = expression->data.primaryExpression.members->nodes[1];
            SZrAstNode *argument;

            TEST_ASSERT_NOT_NULL(call);
            TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_CALL, call->type);
            TEST_ASSERT_NOT_NULL(call->data.functionCall.args);
            TEST_ASSERT_EQUAL_UINT(1U, call->data.functionCall.args->count);
            argument = call->data.functionCall.args->nodes[0];
            TEST_ASSERT_NOT_NULL(argument);
            TEST_ASSERT_EQUAL_INT(ZR_AST_INTEGER_LITERAL, argument->type);
            TEST_ASSERT_EQUAL_INT64(1, argument->data.integerLiteral.value);
        }
    }
    TEST_ASSERT_TRUE(ZrParser_IdentifierType_Infer(
            &g_compiler, expression->data.primaryExpression.property, &g_types[1]));
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_UINT32(receiverToken, g_types[1].nominalDefinitionToken);
    if (publicEntry) {
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(&g_compiler, expression, &g_types[2]));
    } else {
        TEST_ASSERT_TRUE(infer_primary_member_chain_type(&g_compiler, expression,
                &g_types[1], expression->data.primaryExpression.members, 0U, ZR_FALSE, &g_types[2]));
    }
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, g_types[2].baseType);
}

static void test_root_base_field(void) {
    assert_inherited_receiver(ZrParser_SyntaxContext_Root(), ZR_FALSE, ZR_FALSE, ZR_FALSE);
}

static void test_root_base_field_public(void) {
    assert_inherited_receiver(ZrParser_SyntaxContext_Root(), ZR_FALSE, ZR_FALSE, ZR_TRUE);
}

static void test_root_base_method(void) {
    assert_inherited_receiver(ZrParser_SyntaxContext_Root(), ZR_FALSE, ZR_TRUE, ZR_FALSE);
}

static void test_root_base_method_public(void) {
    assert_inherited_receiver(ZrParser_SyntaxContext_Root(), ZR_FALSE, ZR_TRUE, ZR_TRUE);
}

static void test_opaque_distinct_base_field(void) {
    assert_inherited_receiver(g_definition, ZR_FALSE, ZR_FALSE, ZR_FALSE);
}

static void test_opaque_distinct_base_field_public(void) {
    assert_inherited_receiver(g_definition, ZR_FALSE, ZR_FALSE, ZR_TRUE);
}

static void test_opaque_distinct_base_method(void) {
    assert_inherited_receiver(g_definition, ZR_FALSE, ZR_TRUE, ZR_FALSE);
}

static void test_opaque_distinct_base_method_public(void) {
    assert_inherited_receiver(g_definition, ZR_FALSE, ZR_TRUE, ZR_TRUE);
}

static void test_mixed_distinct_base_field(void) {
    assert_inherited_receiver(g_mixed, ZR_FALSE, ZR_FALSE, ZR_FALSE);
}

static void test_mixed_distinct_base_method(void) {
    assert_inherited_receiver(g_mixed, ZR_FALSE, ZR_TRUE, ZR_FALSE);
}

static void test_opaque_same_spelling_base_field(void) {
    assert_inherited_receiver(g_definition, ZR_TRUE, ZR_FALSE, ZR_FALSE);
}

static void test_opaque_same_spelling_base_field_public(void) {
    assert_inherited_receiver(g_definition, ZR_TRUE, ZR_FALSE, ZR_TRUE);
}

static void test_opaque_same_spelling_base_method(void) {
    assert_inherited_receiver(g_definition, ZR_TRUE, ZR_TRUE, ZR_FALSE);
}

static void test_opaque_same_spelling_base_method_public(void) {
    assert_inherited_receiver(g_definition, ZR_TRUE, ZR_TRUE, ZR_TRUE);
}

static void test_real_generic_prototype_identity_distinguishes_open_and_instances(void) {
    const TZrChar *source = "struct Parcel<const N: int> {}"
            "var first: Parcel<1>; var second: Parcel<2>;";
    SZrString *sourceName = ZrCore_String_CreateFromNative(g_state, "instance_identity.zr");
    SZrAstNodeArray *nodes;
    SZrTypePrototypeInfo *prototype;
    SZrTypePrototypeInfo openSnapshot = {0};
    SZrTypePrototypeInfo firstSnapshot = {0};
    SZrTypePrototypeInfo secondSnapshot = {0};
    const SZrNamedTypeBinding *binding;

    TEST_ASSERT_NOT_NULL(sourceName);
    g_ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(g_ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_ast->type);
    g_compiler.scriptAst = g_ast;
    nodes = g_ast->data.script.statements;
    TEST_ASSERT_NOT_NULL(nodes);
    TEST_ASSERT_EQUAL_UINT(3U, nodes->count);
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_DECLARATION, nodes->nodes[0]->type);
    ZrParser_Compiler_CompileStructDeclaration(&g_compiler, nodes->nodes[0]);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    binding = ZrParser_TypeEnvironment_FindTypeBindingWithContext(g_compiler.typeEnv,
            nodes->nodes[0]->data.structDeclaration.name->name, ZrParser_SyntaxContext_Root());
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_PTR(nodes->nodes[0], binding->declarationNode);
    prototype = find_compiler_type_prototype_inference(&g_compiler, binding->name);
    TEST_ASSERT_NOT_NULL(prototype);
    TEST_ASSERT_EQUAL_PTR(nodes->nodes[0], prototype->declarationNode);
    TEST_ASSERT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, prototype->genericInstanceTypeId);
    TEST_ASSERT_EQUAL_UINT(1U, prototype->genericParameters.length);
    openSnapshot = *prototype;
    for (TZrSize index = 0U; index < 2U; index++) {
        TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, nodes->nodes[index + 1U]->type);
        TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(&g_compiler,
                nodes->nodes[index + 1U]->data.variableDeclaration.typeInfo, &g_types[index]));
        TEST_ASSERT_TRUE(ensure_generic_instance_type_prototype_from_inferred(
                &g_compiler, &g_types[index]));
        TEST_ASSERT_FALSE(g_compiler.hasError);
        prototype = type_inference_find_nominal_prototype(&g_compiler, &g_types[index]);
        TEST_ASSERT_NOT_NULL(prototype);
        TEST_ASSERT_EQUAL_PTR(nodes->nodes[0], prototype->declarationNode);
        TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, prototype->genericInstanceTypeId);
        if (index == 0U) {
            firstSnapshot = *prototype;
        } else {
            secondSnapshot = *prototype;
        }
    }
    TEST_ASSERT_NOT_EQUAL(firstSnapshot.genericInstanceTypeId, secondSnapshot.genericInstanceTypeId);
    TEST_ASSERT_TRUE(type_inference_member_prototypes_have_same_identity(
            &g_compiler, &openSnapshot, &openSnapshot));
    TEST_ASSERT_FALSE(type_inference_member_prototypes_have_same_identity(
            &g_compiler, &openSnapshot, &firstSnapshot));
    TEST_ASSERT_FALSE(type_inference_member_prototypes_have_same_identity(
            &g_compiler, &firstSnapshot, &openSnapshot));
    TEST_ASSERT_FALSE(type_inference_member_prototypes_have_same_identity(
            &g_compiler, &firstSnapshot, &secondSnapshot));
    prototype = type_inference_find_nominal_prototype(&g_compiler, &g_types[0]);
    TEST_ASSERT_NOT_NULL(prototype);
    TEST_ASSERT_EQUAL_UINT32(firstSnapshot.genericInstanceTypeId, prototype->genericInstanceTypeId);
    TEST_ASSERT_TRUE(type_inference_member_prototypes_have_same_identity(
            &g_compiler, &firstSnapshot, prototype));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_root_base_field);
    RUN_TEST(test_root_base_field_public);
    RUN_TEST(test_root_base_method);
    RUN_TEST(test_root_base_method_public);
    RUN_TEST(test_opaque_distinct_base_field);
    RUN_TEST(test_opaque_distinct_base_field_public);
    RUN_TEST(test_opaque_distinct_base_method);
    RUN_TEST(test_opaque_distinct_base_method_public);
    RUN_TEST(test_mixed_distinct_base_field);
    RUN_TEST(test_mixed_distinct_base_method);
    RUN_TEST(test_opaque_same_spelling_base_field);
    RUN_TEST(test_opaque_same_spelling_base_field_public);
    RUN_TEST(test_opaque_same_spelling_base_method);
    RUN_TEST(test_opaque_same_spelling_base_method_public);
    RUN_TEST(test_real_generic_prototype_identity_distinguishes_open_and_instances);
    return UNITY_END();
}
