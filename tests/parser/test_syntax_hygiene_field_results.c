#include "unity.h"

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

typedef enum EFixtureProducer {
    FIXTURE_EXTERN_STRUCT,
    FIXTURE_SOURCE_STRUCT,
    FIXTURE_SOURCE_CLASS
} EFixtureProducer;

static SZrState *g_state;
static SZrCompilerState g_compiler;
static TZrBool g_compilerInitialized;
static SZrAstNode *g_ast;
static SZrSyntaxContextTable *g_table;
static SZrSyntaxContext g_definition;
static SZrSyntaxContext g_otherDefinition;
static SZrSyntaxContext g_mixed;
static SZrInferredType g_types[5];

void setUp(void) {
    SZrSyntaxExpansionInfo expansion = {0};
    TZrSyntaxExpansionId expansionId;
    SZrScope scope = {0};

    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
    for (TZrSize index = 0U; index < 5U; index++) {
        ZrParser_InferredType_Init(g_state, &g_types[index], ZR_VALUE_TYPE_OBJECT);
    }
    g_table = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_table);
    expansion.stableIdentity = 0xB601U;
    expansion.callSiteSpan = ZrParser_TokenSpan_None();
    expansion.definitionSiteSpan = ZrParser_TokenSpan_None();
    TEST_ASSERT_TRUE(ZrParser_SyntaxContextTable_RegisterExpansion(
            g_table, &expansion, &expansionId));
    g_definition = ZrParser_SyntaxContext_DefSite(g_table, expansionId);
    g_mixed = ZrParser_SyntaxContext_MixedSite(g_table, expansionId);
    expansion.stableIdentity = 0xB602U;
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
    for (TZrSize index = 0U; index < 5U; index++) {
        ZrParser_InferredType_Free(g_state, &g_types[index]);
    }
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
    SZrString *name = ZrCore_String_CreateFromNative(g_state, "field_result_hygiene.zr");

    TEST_ASSERT_NOT_NULL(name);
    g_ast = ZrParser_Parse(g_state, source, strlen(source), name);
    TEST_ASSERT_NOT_NULL(g_ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_ast->type);
    TEST_ASSERT_NOT_NULL(g_ast->data.script.statements);
    g_compiler.scriptAst = g_ast;
    return g_ast->data.script.statements;
}

static SZrNamedTypeBinding publish_type(SZrAstNode *node,
        SZrSyntaxContext context, EFixtureProducer producer) {
    SZrIdentifier *name;
    const SZrNamedTypeBinding *binding;
    const SZrCanonicalTypeNode *nominal;

    if (producer == FIXTURE_SOURCE_CLASS) {
        TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, node->type);
        name = node->data.classDeclaration.name;
    } else {
        TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_DECLARATION, node->type);
        name = node->data.structDeclaration.name;
    }
    TEST_ASSERT_NOT_NULL(name);
    name->syntaxContext = context;
    if (producer == FIXTURE_SOURCE_CLASS) {
        ZrParser_Compiler_CompileClassDeclaration(&g_compiler, node);
    } else if (producer == FIXTURE_SOURCE_STRUCT) {
        ZrParser_Compiler_CompileStructDeclaration(&g_compiler, node);
    } else {
        extern_compiler_register_struct_prototype(&g_compiler, node);
    }
    TEST_ASSERT_FALSE(g_compiler.hasError);
    binding = ZrParser_TypeEnvironment_FindTypeBindingWithContext(
            g_compiler.typeEnv, name->name, context);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_PTR(node, binding->declarationNode);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, binding->symbolId);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, binding->typeId);
    nominal = ZrParser_CanonicalType_Find(g_compiler.semanticContext, binding->typeId);
    TEST_ASSERT_NOT_NULL(nominal);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_NOMINAL, nominal->kind);
    TEST_ASSERT_EQUAL_UINT32(ZrParser_SyntaxContext_Equal(
            ZrParser_SyntaxContext_Normalize(context, ZR_SYNTAX_NAMESPACE_ITEM),
            ZrParser_SyntaxContext_Root()) ? 0U : binding->symbolId,
            nominal->data.nominal.definitionToken);
    return *binding;
}

static SZrAstNode *prepare_receiver(SZrAstNode *owner, SZrAstNode *variable,
        SZrAstNode *statement, SZrSyntaxContext context, SZrType *fieldType) {
    SZrType *annotation;
    SZrAstNode *identifier;
    SZrAstNode *expression;
    SZrTypePrototypeInfo *prototype;
    SZrTypeMemberInfo *member;
    const SZrTypeBinding *binding;

    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, variable->type);
    annotation = variable->data.variableDeclaration.typeInfo;
    TEST_ASSERT_NOT_NULL(annotation);
    TEST_ASSERT_NOT_NULL(annotation->name);
    if (annotation->name->type == ZR_AST_GENERIC_TYPE) {
        annotation->name->data.genericType.name->syntaxContext = context;
    } else {
        TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, annotation->name->type);
        annotation->name->data.identifier.syntaxContext = context;
    }
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, annotation, &g_types[0]));
    if (g_types[0].elementTypes.length > 0U) {
        TEST_ASSERT_TRUE(ensure_generic_instance_type_prototype_from_inferred(
                &g_compiler, &g_types[0]));
    }
    TEST_ASSERT_FALSE(g_compiler.hasError);
    prototype = type_inference_find_nominal_prototype(&g_compiler, &g_types[0]);
    TEST_ASSERT_NOT_NULL(prototype);
    TEST_ASSERT_EQUAL_PTR(owner, prototype->declarationNode);
    TEST_ASSERT_EQUAL_UINT(1U, prototype->members.length);
    member = ZrCore_Array_Get(&prototype->members, 0U);
    TEST_ASSERT_NOT_NULL(member);
    TEST_ASSERT_EQUAL_INT(owner->type == ZR_AST_CLASS_DECLARATION ?
            ZR_AST_CLASS_FIELD : ZR_AST_STRUCT_FIELD, member->memberType);
    TEST_ASSERT_EQUAL_STRING("value", ZrCore_String_GetNativeString(member->name));
    TEST_ASSERT_EQUAL_PTR(fieldType, member->fieldType);
    TEST_ASSERT_EQUAL_STRING(fieldType != ZR_NULL && fieldType->dimensions > 0 ?
            "Widget[]" : "Widget", ZrCore_String_GetNativeString(member->fieldTypeName));
    TEST_ASSERT_EQUAL_PTR(member, type_inference_find_receiver_member(
            &g_compiler, &g_types[0], member->name));
    identifier = variable->data.variableDeclaration.pattern;
    TEST_ASSERT_NOT_NULL(identifier);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, identifier->type);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariableExWithContext(g_state,
            g_compiler.typeEnv, identifier->data.identifier.name, &g_types[0], variable,
            variable->location, identifier->data.identifier.syntaxContext));
    binding = ZrParser_TypeEnvironment_FindVariableBindingWithContext(g_compiler.typeEnv,
            identifier->data.identifier.name, identifier->data.identifier.syntaxContext);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_UINT32(ZrParser_CanonicalType_FromInferred(
            g_compiler.semanticContext, &g_types[0]), ZrParser_CanonicalType_FromInferred(
            g_compiler.semanticContext, &binding->type));
    TEST_ASSERT_EQUAL_INT(ZR_AST_EXPRESSION_STATEMENT, statement->type);
    expression = statement->data.expressionStatement.expr;
    TEST_ASSERT_NOT_NULL(expression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, expression->type);
    TEST_ASSERT_NOT_NULL(expression->data.primaryExpression.members);
    TEST_ASSERT_EQUAL_UINT(1U, expression->data.primaryExpression.members->count);
    TEST_ASSERT_EQUAL_INT(ZR_AST_MEMBER_EXPRESSION,
            expression->data.primaryExpression.members->nodes[0]->type);
    TEST_ASSERT_TRUE(ZrParser_IdentifierType_Infer(
            &g_compiler, expression->data.primaryExpression.property, &g_types[1]));
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_UINT32(ZrParser_CanonicalType_FromInferred(
            g_compiler.semanticContext, &g_types[0]), ZrParser_CanonicalType_FromInferred(
            g_compiler.semanticContext, &g_types[1]));
    return expression;
}

static void assert_field_result(SZrAstNode *expression, TZrBool publicEntry,
        const SZrNamedTypeBinding *expected, TZrSymbolId expectedToken) {
    if (publicEntry) {
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(&g_compiler, expression, &g_types[2]));
    } else {
        TEST_ASSERT_TRUE(infer_primary_member_chain_type(&g_compiler, expression,
                &g_types[1], expression->data.primaryExpression.members, 0U, ZR_FALSE, &g_types[2]));
    }
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, g_types[2].baseType);
    TEST_ASSERT_EQUAL_STRING("Widget", ZrCore_String_GetNativeString(g_types[2].typeName));
    TEST_ASSERT_EQUAL_UINT32(expectedToken, g_types[2].nominalDefinitionToken);
    TEST_ASSERT_EQUAL_UINT32(expected->typeId,
            ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, &g_types[2]));
}

static void assert_plain_field(EFixtureProducer producer, SZrSyntaxContext fieldContext,
        SZrSyntaxContext ownerContext, TZrBool markedFirst, TZrBool publicEntry) {
    SZrAstNodeArray *nodes = parse_source(producer == FIXTURE_EXTERN_STRUCT ?
            "native extern(\"fixture\") { struct Widget {} struct Widget {}"
            "struct Holder { pub var value: Widget; } } var receiver: Holder; receiver.value;" :
            producer == FIXTURE_SOURCE_STRUCT ?
            "struct Widget {} struct Widget {} struct Holder { pub var value: Widget; }"
            "var receiver: Holder; receiver.value;" :
            "class Widget {} class Widget {} class Holder { pub var value: Widget; }"
            "var receiver: Holder; receiver.value;");
    SZrAstNodeArray *declarations;
    SZrAstNode *owner;
    SZrAstNode *field;
    SZrType *fieldType;
    SZrAstNode *expression;
    SZrNamedTypeBinding rootBinding = {0};
    SZrNamedTypeBinding markedBinding = {0};
    SZrNamedTypeBinding ownerBinding;
    SZrNamedTypeBinding expected;
    TZrSize variableIndex = producer == FIXTURE_EXTERN_STRUCT ? 1U : 3U;
    TZrSymbolId expectedToken;

    TEST_ASSERT_EQUAL_UINT(producer == FIXTURE_EXTERN_STRUCT ? 3U : 5U, nodes->count);
    if (producer == FIXTURE_EXTERN_STRUCT) {
        TEST_ASSERT_EQUAL_INT(ZR_AST_EXTERN_BLOCK, nodes->nodes[0]->type);
        declarations = nodes->nodes[0]->data.externBlock.declarations;
        TEST_ASSERT_NOT_NULL(declarations);
        TEST_ASSERT_EQUAL_UINT(3U, declarations->count);
    } else {
        declarations = nodes;
    }
    for (TZrSize index = 0U; index < 2U; index++) {
        TZrBool marked = markedFirst ? index == 0U : index == 1U;
        SZrNamedTypeBinding binding = publish_type(declarations->nodes[index],
                marked ? g_definition : ZrParser_SyntaxContext_Root(), producer);

        if (marked) {
            markedBinding = binding;
        } else {
            rootBinding = binding;
        }
    }
    TEST_ASSERT_NOT_EQUAL(rootBinding.symbolId, markedBinding.symbolId);
    TEST_ASSERT_NOT_EQUAL(rootBinding.typeId, markedBinding.typeId);
    expected = ZrParser_SyntaxContext_Equal(ZrParser_SyntaxContext_Normalize(
            fieldContext, ZR_SYNTAX_NAMESPACE_ITEM), ZrParser_SyntaxContext_Root()) ?
            rootBinding : markedBinding;
    expectedToken = expected.typeId == rootBinding.typeId ? 0U : markedBinding.symbolId;
    owner = declarations->nodes[2];
    field = producer == FIXTURE_SOURCE_CLASS ? owner->data.classDeclaration.members->nodes[0] :
            owner->data.structDeclaration.members->nodes[0];
    TEST_ASSERT_NOT_NULL(field);
    TEST_ASSERT_EQUAL_INT(producer == FIXTURE_SOURCE_CLASS ? ZR_AST_CLASS_FIELD :
            ZR_AST_STRUCT_FIELD, field->type);
    fieldType = producer == FIXTURE_SOURCE_CLASS ? field->data.classField.typeInfo :
            field->data.structField.typeInfo;
    TEST_ASSERT_NOT_NULL(fieldType);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, fieldType->name->type);
    fieldType->name->data.identifier.syntaxContext = fieldContext;
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(&g_compiler, fieldType, &g_types[3]));
    TEST_ASSERT_EQUAL_UINT32(expectedToken, g_types[3].nominalDefinitionToken);
    TEST_ASSERT_EQUAL_UINT32(expected.typeId,
            ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, &g_types[3]));
    ownerBinding = publish_type(owner, ownerContext, producer);
    expression = prepare_receiver(owner, nodes->nodes[variableIndex],
            nodes->nodes[variableIndex + 1U], ownerContext, fieldType);
    TEST_ASSERT_EQUAL_UINT32(ownerBinding.typeId,
            ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, &g_types[0]));
    assert_field_result(expression, publicEntry, &expected, expectedToken);
}

static void assert_generic_field_details(SZrSyntaxContext argumentContext,
        TZrBool publicEntry, TZrBool arrayField) {
    SZrAstNodeArray *nodes = parse_source(arrayField ?
            "struct Widget {} struct Widget {}"
            "struct Parcel<T> { pub var value: T[]; }"
            "var receiver: Parcel<Widget>; receiver.value; var expected: Widget[];" :
            "struct Widget {} struct Widget {}"
            "struct Parcel<T> { pub var value: T; }"
            "var receiver: Parcel<Widget>; receiver.value;");
    SZrNamedTypeBinding rootBinding;
    SZrNamedTypeBinding markedBinding;
    SZrNamedTypeBinding ownerBinding;
    SZrNamedTypeBinding expected;
    SZrType *annotation;
    SZrType *fieldType;
    SZrAstNode *argument;
    SZrAstNode *expression;
    SZrTypePrototypeInfo *prototype;
    SZrInferredType *argumentType;
    TZrTypeId rootInstance;
    TZrTypeId expectedInstance;
    TZrTypeId expectedArray = ZR_SEMANTIC_ID_INVALID;
    TZrSymbolId expectedToken;

    TEST_ASSERT_EQUAL_UINT(arrayField ? 6U : 5U, nodes->count);
    rootBinding = publish_type(nodes->nodes[0], ZrParser_SyntaxContext_Root(), FIXTURE_SOURCE_STRUCT);
    markedBinding = publish_type(nodes->nodes[1], g_definition, FIXTURE_SOURCE_STRUCT);
    TEST_ASSERT_NOT_EQUAL(rootBinding.typeId, markedBinding.typeId);
    ownerBinding = publish_type(nodes->nodes[2], ZrParser_SyntaxContext_Root(), FIXTURE_SOURCE_STRUCT);
    expected = ZrParser_SyntaxContext_Equal(argumentContext, ZrParser_SyntaxContext_Root()) ?
            rootBinding : markedBinding;
    expectedToken = expected.typeId == rootBinding.typeId ? 0U : markedBinding.symbolId;
    annotation = nodes->nodes[3]->data.variableDeclaration.typeInfo;
    TEST_ASSERT_NOT_NULL(annotation);
    TEST_ASSERT_EQUAL_INT(ZR_AST_GENERIC_TYPE, annotation->name->type);
    TEST_ASSERT_EQUAL_UINT(1U, annotation->name->data.genericType.params->count);
    argument = annotation->name->data.genericType.params->nodes[0];
    TEST_ASSERT_NOT_NULL(argument);
    TEST_ASSERT_EQUAL_INT(ZR_AST_TYPE, argument->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, argument->data.type.name->type);
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(&g_compiler, annotation, &g_types[4]));
    TEST_ASSERT_TRUE(ensure_generic_instance_type_prototype_from_inferred(&g_compiler, &g_types[4]));
    prototype = type_inference_find_nominal_prototype(&g_compiler, &g_types[4]);
    TEST_ASSERT_NOT_NULL(prototype);
    rootInstance = prototype->genericInstanceTypeId;
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, rootInstance);
    argument->data.type.name->data.identifier.syntaxContext = argumentContext;
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, &argument->data.type, &g_types[3]));
    TEST_ASSERT_EQUAL_UINT32(expectedToken, g_types[3].nominalDefinitionToken);
    TEST_ASSERT_EQUAL_UINT32(expected.typeId,
            ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, &g_types[3]));
    expectedInstance = ZrParser_CanonicalType_InternGenericInstance(g_compiler.semanticContext,
            ownerBinding.typeId, &expected.typeId, 1U);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, expectedInstance);
    if (expectedToken != 0U) {
        TEST_ASSERT_NOT_EQUAL(rootInstance, expectedInstance);
    } else {
        TEST_ASSERT_EQUAL_UINT32(rootInstance, expectedInstance);
    }
    fieldType = nodes->nodes[2]->data.structDeclaration.members->nodes[0]->data.structField.typeInfo;
    TEST_ASSERT_NOT_NULL(fieldType);
    TEST_ASSERT_EQUAL_STRING("T", ZrCore_String_GetNativeString(fieldType->name->data.identifier.name));
    TEST_ASSERT_EQUAL_INT(arrayField ? 1 : 0, fieldType->dimensions);
    if (arrayField) {
        SZrType *expectedAnnotation;
        SZrInferredType *expectedElement;

        TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, nodes->nodes[5]->type);
        expectedAnnotation = nodes->nodes[5]->data.variableDeclaration.typeInfo;
        TEST_ASSERT_NOT_NULL(expectedAnnotation);
        TEST_ASSERT_EQUAL_INT(1, expectedAnnotation->dimensions);
        TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, expectedAnnotation->name->type);
        expectedAnnotation->name->data.identifier.syntaxContext = argumentContext;
        ZrParser_InferredType_Free(g_state, &g_types[3]);
        TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
                &g_compiler, expectedAnnotation, &g_types[3]));
        TEST_ASSERT_FALSE(g_compiler.hasError);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, g_types[3].baseType);
        TEST_ASSERT_EQUAL_UINT(1U, g_types[3].elementTypes.length);
        expectedElement = ZrCore_Array_Get(&g_types[3].elementTypes, 0U);
        TEST_ASSERT_NOT_NULL(expectedElement);
        TEST_ASSERT_EQUAL_UINT32(expectedToken, expectedElement->nominalDefinitionToken);
        TEST_ASSERT_EQUAL_UINT32(expected.typeId,
                ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, expectedElement));
        expectedArray = ZrParser_CanonicalType_FromInferred(
                g_compiler.semanticContext, &g_types[3]);
        TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, expectedArray);
    }
    expression = prepare_receiver(nodes->nodes[2], nodes->nodes[3], nodes->nodes[4],
            ZrParser_SyntaxContext_Root(), fieldType);
    TEST_ASSERT_EQUAL_UINT(1U, g_types[0].elementTypes.length);
    argumentType = ZrCore_Array_Get(&g_types[0].elementTypes, 0U);
    TEST_ASSERT_NOT_NULL(argumentType);
    TEST_ASSERT_EQUAL_UINT32(expectedToken, argumentType->nominalDefinitionToken);
    prototype = type_inference_find_nominal_prototype(&g_compiler, &g_types[0]);
    TEST_ASSERT_NOT_NULL(prototype);
    TEST_ASSERT_EQUAL_UINT32(expectedInstance, prototype->genericInstanceTypeId);
    if (!arrayField) {
        assert_field_result(expression, publicEntry, &expected, expectedToken);
        return;
    }
    for (TZrSize pass = 0U; pass < 2U; pass++) {
        SZrTypeMemberInfo *member;
        SZrInferredType *element;

        if (publicEntry) {
            TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(&g_compiler, expression, &g_types[2]));
        } else {
            TEST_ASSERT_TRUE(infer_primary_member_chain_type(&g_compiler, expression,
                    &g_types[1], expression->data.primaryExpression.members, 0U, ZR_FALSE, &g_types[2]));
        }
        TEST_ASSERT_FALSE(g_compiler.hasError);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, g_types[2].baseType);
        TEST_ASSERT_EQUAL_UINT(1U, g_types[2].elementTypes.length);
        element = ZrCore_Array_Get(&g_types[2].elementTypes, 0U);
        TEST_ASSERT_NOT_NULL(element);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, element->baseType);
        TEST_ASSERT_EQUAL_UINT32(expectedToken, element->nominalDefinitionToken);
        TEST_ASSERT_EQUAL_UINT32(expected.typeId,
                ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, element));
        TEST_ASSERT_EQUAL_UINT32(expectedArray,
                ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, &g_types[2]));
        prototype = type_inference_find_nominal_prototype(&g_compiler, &g_types[0]);
        TEST_ASSERT_NOT_NULL(prototype);
        TEST_ASSERT_EQUAL_UINT32(expectedInstance, prototype->genericInstanceTypeId);
        member = ZrCore_Array_Get(&prototype->members, 0U);
        TEST_ASSERT_NOT_NULL(member);
        TEST_ASSERT_EQUAL_PTR(fieldType, member->fieldType);
        TEST_ASSERT_NULL(member->returnTypeName);
        TEST_ASSERT_TRUE(member->hasStructuredReturnType);
        TEST_ASSERT_EQUAL_UINT32(expectedArray, ZrParser_CanonicalType_FromInferred(
                g_compiler.semanticContext, &member->structuredReturnType));
        TEST_ASSERT_NOT_EQUAL(member->structuredReturnType.elementTypes.head,
                g_types[2].elementTypes.head);
        ZrParser_InferredType_Free(g_state, &g_types[2]);
    }
}

static void assert_generic_field(SZrSyntaxContext argumentContext, TZrBool publicEntry) {
    assert_generic_field_details(argumentContext, publicEntry, ZR_FALSE);
}

static void assert_initializer_field(EFixtureProducer producer,
        TZrBool marked, TZrBool publicEntry) {
    SZrAstNodeArray *nodes = parse_source(producer == FIXTURE_SOURCE_STRUCT ?
            "struct Widget {} struct Widget {} var seed: Widget;"
            "struct Holder { pub var value = seed; } var receiver: Holder; receiver.value;" :
            "class Widget {} class Widget {} var seed: Widget;"
            "class Holder { pub var value = seed; } var receiver: Holder; receiver.value;");
    SZrNamedTypeBinding rootBinding;
    SZrNamedTypeBinding markedBinding;
    SZrNamedTypeBinding ownerBinding;
    SZrNamedTypeBinding expected;
    SZrAstNode *seed;
    SZrAstNode *identifier;
    SZrAstNode *owner;
    SZrAstNode *field;
    SZrAstNode *initializer;
    SZrAstNode *expression;
    SZrAstNodeArray *members;
    SZrType *seedType;
    const SZrTypeBinding *binding;
    TZrSymbolId expectedToken;

    TEST_ASSERT_NOT_EQUAL(FIXTURE_EXTERN_STRUCT, producer);
    TEST_ASSERT_EQUAL_UINT(6U, nodes->count);
    rootBinding = publish_type(nodes->nodes[0], ZrParser_SyntaxContext_Root(), producer);
    markedBinding = publish_type(nodes->nodes[1], g_definition, producer);
    TEST_ASSERT_NOT_EQUAL(rootBinding.symbolId, markedBinding.symbolId);
    TEST_ASSERT_NOT_EQUAL(rootBinding.typeId, markedBinding.typeId);
    expected = marked ? markedBinding : rootBinding;
    expectedToken = marked ? markedBinding.symbolId : 0U;
    seed = nodes->nodes[2];
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, seed->type);
    seedType = seed->data.variableDeclaration.typeInfo;
    TEST_ASSERT_NOT_NULL(seedType);
    TEST_ASSERT_NOT_NULL(seedType->name);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, seedType->name->type);
    seedType->name->data.identifier.syntaxContext = marked ?
            g_definition : ZrParser_SyntaxContext_Root();
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, seedType, &g_types[3]));
    TEST_ASSERT_EQUAL_UINT32(expectedToken, g_types[3].nominalDefinitionToken);
    TEST_ASSERT_EQUAL_UINT32(expected.typeId,
            ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, &g_types[3]));
    identifier = seed->data.variableDeclaration.pattern;
    TEST_ASSERT_NOT_NULL(identifier);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, identifier->type);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariableExWithContext(g_state,
            g_compiler.typeEnv, identifier->data.identifier.name, &g_types[3], seed,
            seed->location, identifier->data.identifier.syntaxContext));
    binding = ZrParser_TypeEnvironment_FindVariableBindingWithContext(g_compiler.typeEnv,
            identifier->data.identifier.name, identifier->data.identifier.syntaxContext);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_UINT32(expectedToken, binding->type.nominalDefinitionToken);
    TEST_ASSERT_EQUAL_UINT32(expected.typeId,
            ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, &binding->type));
    owner = nodes->nodes[3];
    TEST_ASSERT_EQUAL_INT(producer == FIXTURE_SOURCE_CLASS ?
            ZR_AST_CLASS_DECLARATION : ZR_AST_STRUCT_DECLARATION, owner->type);
    members = producer == FIXTURE_SOURCE_CLASS ? owner->data.classDeclaration.members :
            owner->data.structDeclaration.members;
    TEST_ASSERT_NOT_NULL(members);
    TEST_ASSERT_EQUAL_UINT(1U, members->count);
    field = members->nodes[0];
    TEST_ASSERT_EQUAL_INT(producer == FIXTURE_SOURCE_CLASS ?
            ZR_AST_CLASS_FIELD : ZR_AST_STRUCT_FIELD, field->type);
    TEST_ASSERT_NULL(producer == FIXTURE_SOURCE_CLASS ?
            field->data.classField.typeInfo : field->data.structField.typeInfo);
    initializer = producer == FIXTURE_SOURCE_CLASS ? field->data.classField.init :
            field->data.structField.init;
    TEST_ASSERT_NOT_NULL(initializer);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(&g_compiler, initializer, &g_types[4]));
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_UINT32(expectedToken, g_types[4].nominalDefinitionToken);
    TEST_ASSERT_EQUAL_UINT32(expected.typeId,
            ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, &g_types[4]));
    ownerBinding = publish_type(owner, ZrParser_SyntaxContext_Root(), producer);
    expression = prepare_receiver(owner, nodes->nodes[4], nodes->nodes[5],
            ZrParser_SyntaxContext_Root(), ZR_NULL);
    TEST_ASSERT_EQUAL_UINT32(ownerBinding.typeId,
            ZrParser_CanonicalType_FromInferred(g_compiler.semanticContext, &g_types[0]));
    assert_field_result(expression, publicEntry, &expected, expectedToken);
}

static void test_extern_root_field_result(void) {
    assert_plain_field(FIXTURE_EXTERN_STRUCT, ZrParser_SyntaxContext_Root(),
            ZrParser_SyntaxContext_Root(), ZR_FALSE, ZR_FALSE);
}
static void test_extern_root_field_result_public(void) {
    assert_plain_field(FIXTURE_EXTERN_STRUCT, ZrParser_SyntaxContext_Root(),
            ZrParser_SyntaxContext_Root(), ZR_FALSE, ZR_TRUE);
}
static void test_extern_marked_field_result(void) {
    assert_plain_field(FIXTURE_EXTERN_STRUCT, g_definition,
            ZrParser_SyntaxContext_Root(), ZR_FALSE, ZR_FALSE);
}
static void test_extern_marked_field_result_public(void) {
    assert_plain_field(FIXTURE_EXTERN_STRUCT, g_definition,
            ZrParser_SyntaxContext_Root(), ZR_FALSE, ZR_TRUE);
}
static void test_struct_root_field_result(void) {
    assert_plain_field(FIXTURE_SOURCE_STRUCT, ZrParser_SyntaxContext_Root(),
            ZrParser_SyntaxContext_Root(), ZR_FALSE, ZR_FALSE);
}
static void test_struct_root_field_result_public(void) {
    assert_plain_field(FIXTURE_SOURCE_STRUCT, ZrParser_SyntaxContext_Root(),
            ZrParser_SyntaxContext_Root(), ZR_FALSE, ZR_TRUE);
}
static void test_struct_marked_field_result(void) {
    assert_plain_field(FIXTURE_SOURCE_STRUCT, g_definition,
            ZrParser_SyntaxContext_Root(), ZR_FALSE, ZR_FALSE);
}
static void test_struct_marked_field_result_public(void) {
    assert_plain_field(FIXTURE_SOURCE_STRUCT, g_definition,
            ZrParser_SyntaxContext_Root(), ZR_FALSE, ZR_TRUE);
}
static void test_class_root_field_result(void) {
    assert_plain_field(FIXTURE_SOURCE_CLASS, ZrParser_SyntaxContext_Root(),
            ZrParser_SyntaxContext_Root(), ZR_FALSE, ZR_FALSE);
}
static void test_class_root_field_result_public(void) {
    assert_plain_field(FIXTURE_SOURCE_CLASS, ZrParser_SyntaxContext_Root(),
            ZrParser_SyntaxContext_Root(), ZR_FALSE, ZR_TRUE);
}
static void test_class_marked_field_result(void) {
    assert_plain_field(FIXTURE_SOURCE_CLASS, g_definition,
            ZrParser_SyntaxContext_Root(), ZR_FALSE, ZR_FALSE);
}
static void test_class_marked_field_result_public(void) {
    assert_plain_field(FIXTURE_SOURCE_CLASS, g_definition,
            ZrParser_SyntaxContext_Root(), ZR_FALSE, ZR_TRUE);
}
static void test_mixed_item_field_result_stays_root(void) {
    assert_plain_field(FIXTURE_EXTERN_STRUCT, g_mixed,
            ZrParser_SyntaxContext_Root(), ZR_FALSE, ZR_FALSE);
}
static void test_marked_first_field_result(void) {
    assert_plain_field(FIXTURE_EXTERN_STRUCT, g_definition,
            ZrParser_SyntaxContext_Root(), ZR_TRUE, ZR_TRUE);
}
static void test_marked_owner_and_field_have_independent_identities(void) {
    assert_plain_field(FIXTURE_SOURCE_STRUCT, g_definition,
            g_otherDefinition, ZR_FALSE, ZR_FALSE);
}
static void test_marked_owner_and_field_public_have_independent_identities(void) {
    assert_plain_field(FIXTURE_SOURCE_STRUCT, g_definition,
            g_otherDefinition, ZR_FALSE, ZR_TRUE);
}
static void test_generic_root_field_result(void) {
    assert_generic_field(ZrParser_SyntaxContext_Root(), ZR_FALSE);
}
static void test_generic_root_field_result_public(void) {
    assert_generic_field(ZrParser_SyntaxContext_Root(), ZR_TRUE);
}
static void test_generic_marked_field_result(void) {
    assert_generic_field(g_definition, ZR_FALSE);
}
static void test_generic_marked_field_result_public(void) {
    assert_generic_field(g_definition, ZR_TRUE);
}
static void test_generic_root_array_field_result(void) {
    assert_generic_field_details(ZrParser_SyntaxContext_Root(), ZR_FALSE, ZR_TRUE);
}
static void test_generic_root_array_field_result_public(void) {
    assert_generic_field_details(ZrParser_SyntaxContext_Root(), ZR_TRUE, ZR_TRUE);
}
static void test_generic_marked_array_field_result(void) {
    assert_generic_field_details(g_definition, ZR_FALSE, ZR_TRUE);
}
static void test_generic_marked_array_field_result_public(void) {
    assert_generic_field_details(g_definition, ZR_TRUE, ZR_TRUE);
}

static void test_struct_root_initializer_field_result(void) {
    assert_initializer_field(FIXTURE_SOURCE_STRUCT, ZR_FALSE, ZR_FALSE);
}
static void test_struct_root_initializer_field_result_public(void) {
    assert_initializer_field(FIXTURE_SOURCE_STRUCT, ZR_FALSE, ZR_TRUE);
}
static void test_struct_marked_initializer_field_result(void) {
    assert_initializer_field(FIXTURE_SOURCE_STRUCT, ZR_TRUE, ZR_FALSE);
}
static void test_struct_marked_initializer_field_result_public(void) {
    assert_initializer_field(FIXTURE_SOURCE_STRUCT, ZR_TRUE, ZR_TRUE);
}
static void test_class_root_initializer_field_result(void) {
    assert_initializer_field(FIXTURE_SOURCE_CLASS, ZR_FALSE, ZR_FALSE);
}
static void test_class_root_initializer_field_result_public(void) {
    assert_initializer_field(FIXTURE_SOURCE_CLASS, ZR_FALSE, ZR_TRUE);
}
static void test_class_marked_initializer_field_result(void) {
    assert_initializer_field(FIXTURE_SOURCE_CLASS, ZR_TRUE, ZR_FALSE);
}
static void test_class_marked_initializer_field_result_public(void) {
    assert_initializer_field(FIXTURE_SOURCE_CLASS, ZR_TRUE, ZR_TRUE);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_extern_root_field_result);
    RUN_TEST(test_extern_root_field_result_public);
    RUN_TEST(test_extern_marked_field_result);
    RUN_TEST(test_extern_marked_field_result_public);
    RUN_TEST(test_struct_root_field_result);
    RUN_TEST(test_struct_root_field_result_public);
    RUN_TEST(test_struct_marked_field_result);
    RUN_TEST(test_struct_marked_field_result_public);
    RUN_TEST(test_class_root_field_result);
    RUN_TEST(test_class_root_field_result_public);
    RUN_TEST(test_class_marked_field_result);
    RUN_TEST(test_class_marked_field_result_public);
    RUN_TEST(test_mixed_item_field_result_stays_root);
    RUN_TEST(test_marked_first_field_result);
    RUN_TEST(test_marked_owner_and_field_have_independent_identities);
    RUN_TEST(test_marked_owner_and_field_public_have_independent_identities);
    RUN_TEST(test_generic_root_field_result);
    RUN_TEST(test_generic_root_field_result_public);
    RUN_TEST(test_generic_marked_field_result);
    RUN_TEST(test_generic_marked_field_result_public);
    RUN_TEST(test_generic_root_array_field_result);
    RUN_TEST(test_generic_root_array_field_result_public);
    RUN_TEST(test_generic_marked_array_field_result);
    RUN_TEST(test_generic_marked_array_field_result_public);
    RUN_TEST(test_struct_root_initializer_field_result);
    RUN_TEST(test_struct_root_initializer_field_result_public);
    RUN_TEST(test_struct_marked_initializer_field_result);
    RUN_TEST(test_struct_marked_initializer_field_result_public);
    RUN_TEST(test_class_root_initializer_field_result);
    RUN_TEST(test_class_root_initializer_field_result_public);
    RUN_TEST(test_class_marked_initializer_field_result);
    RUN_TEST(test_class_marked_initializer_field_result_public);
    return UNITY_END();
}
