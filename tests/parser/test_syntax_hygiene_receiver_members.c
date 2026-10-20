#include "unity.h"

#include <string.h>
#include <stdio.h>

#include "compiler/compiler_internal.h"
#include "harness/runtime_support.h"
#include "type_inference/type_inference_internal.h"
#include "type_inference/type_inference_nominal_prototype.h"
#include "zr_vm_core/function.h"
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
static SZrInferredType g_types[6];

void setUp(void) {
    SZrSyntaxExpansionInfo expansion = {0};
    TZrSyntaxExpansionId expansionId;
    SZrScope scope = {0};

    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
    for (TZrSize index = 0U; index < 6U; index++) {
        ZrParser_InferredType_Init(g_state, &g_types[index], ZR_VALUE_TYPE_OBJECT);
    }
    g_table = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_table);
    expansion.stableIdentity = 0xB401U;
    expansion.callSiteSpan = ZrParser_TokenSpan_None();
    expansion.definitionSiteSpan = ZrParser_TokenSpan_None();
    TEST_ASSERT_TRUE(ZrParser_SyntaxContextTable_RegisterExpansion(
            g_table, &expansion, &expansionId));
    g_definition = ZrParser_SyntaxContext_DefSite(g_table, expansionId);
    g_mixed = ZrParser_SyntaxContext_MixedSite(g_table, expansionId);
    expansion.stableIdentity = 0xB402U;
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
    for (TZrSize index = 0U; index < 6U; index++) {
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
    SZrString *name = ZrCore_String_CreateFromNative(g_state, "receiver_hygiene.zr");

    TEST_ASSERT_NOT_NULL(name);
    g_ast = ZrParser_Parse(g_state, source, strlen(source), name);
    TEST_ASSERT_NOT_NULL(g_ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_ast->type);
    TEST_ASSERT_NOT_NULL(g_ast->data.script.statements);
    g_compiler.scriptAst = g_ast;
    return g_ast->data.script.statements;
}

static TZrSymbolId publish_struct(SZrAstNode *node, SZrSyntaxContext context,
        TZrBool isMethod) {
    const SZrNamedTypeBinding *binding;
    const SZrCanonicalTypeNode *nominal;

    TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_DECLARATION, node->type);
    node->data.structDeclaration.name->syntaxContext = context;
    if (isMethod) {
        ZrParser_Compiler_CompileStructDeclaration(&g_compiler, node);
    } else {
        extern_compiler_register_struct_prototype(&g_compiler, node);
    }
    TEST_ASSERT_FALSE(g_compiler.hasError);
    binding = ZrParser_TypeEnvironment_FindTypeBindingWithContext(g_compiler.typeEnv,
            node->data.structDeclaration.name->name, context);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_PTR(node, binding->declarationNode);
    nominal = ZrParser_CanonicalType_Find(g_compiler.semanticContext, binding->typeId);
    TEST_ASSERT_NOT_NULL(nominal);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_NOMINAL, nominal->kind);
    return nominal->data.nominal.definitionToken;
}

static SZrAstNode *prepare_receiver(SZrAstNode *declaration, SZrAstNode *variable,
        SZrAstNode *statement, SZrSyntaxContext context, TZrBool isMethod,
        EZrValueType expectedType) {
    SZrAstNode *expression;
    SZrAstNode *identifier;
    SZrType *annotation;
    const SZrNamedTypeBinding *binding;
    const SZrCanonicalTypeNode *nominal;
    const SZrTypeBinding *variableBinding;
    SZrTypePrototypeInfo *prototype;
    SZrTypeMemberInfo *member;
    TZrSymbolId definitionToken;

    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, variable->type);
    annotation = variable->data.variableDeclaration.typeInfo;
    TEST_ASSERT_NOT_NULL(annotation);
    TEST_ASSERT_NOT_NULL(annotation->name);
    if (annotation->name->type == ZR_AST_GENERIC_TYPE) {
        TEST_ASSERT_NOT_NULL(annotation->name->data.genericType.name);
        annotation->name->data.genericType.name->syntaxContext = context;
    } else {
        TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, annotation->name->type);
        annotation->name->data.identifier.syntaxContext = context;
    }
    binding = ZrParser_TypeEnvironment_FindTypeBindingWithContext(g_compiler.typeEnv,
            declaration->data.structDeclaration.name->name, context);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_PTR(declaration, binding->declarationNode);
    nominal = ZrParser_CanonicalType_Find(g_compiler.semanticContext, binding->typeId);
    TEST_ASSERT_NOT_NULL(nominal);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_NOMINAL, nominal->kind);
    definitionToken = nominal->data.nominal.definitionToken;
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, annotation, &g_types[0]));
    TEST_ASSERT_EQUAL_UINT32(definitionToken, g_types[0].nominalDefinitionToken);
    if (g_types[0].elementTypes.length > 0U) {
        TEST_ASSERT_TRUE(ensure_generic_instance_type_prototype_from_inferred(
                &g_compiler, &g_types[0]));
        TEST_ASSERT_FALSE(g_compiler.hasError);
    }
    prototype = type_inference_find_nominal_prototype(&g_compiler, &g_types[0]);
    TEST_ASSERT_NOT_NULL(prototype);
    TEST_ASSERT_EQUAL_PTR(declaration, prototype->declarationNode);
    TEST_ASSERT_EQUAL_UINT(1U, prototype->members.length);
    member = ZrCore_Array_Get(&prototype->members, 0U);
    TEST_ASSERT_NOT_NULL(member);
    if (isMethod) {
        SZrInferredType *parameter;
        SZrAstNode *parameterNode;

        TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_METHOD, member->memberType);
        TEST_ASSERT_NOT_NULL(member->compiledFunction);
        TEST_ASSERT_EQUAL_PTR(declaration->data.structDeclaration.members->nodes[0],
                member->declarationNode);
        TEST_ASSERT_EQUAL_UINT(1U, member->parameterTypes.length);
        parameter = ZrCore_Array_Get(&member->parameterTypes, 0U);
        TEST_ASSERT_NOT_NULL(parameter);
        parameterNode = member->declarationNode->data.structMethod.params->nodes[0];
        TEST_ASSERT_EQUAL_INT(ZR_AST_PARAMETER, parameterNode->type);
        TEST_ASSERT_EQUAL_INT(parameterNode->data.parameter.typeInfo == ZR_NULL ?
                ZR_VALUE_TYPE_OBJECT : expectedType, parameter->baseType);
        TEST_ASSERT_TRUE(member->hasStructuredReturnType);
        TEST_ASSERT_EQUAL_INT(expectedType, member->structuredReturnType.baseType);
    } else {
        TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_FIELD, member->memberType);
        TEST_ASSERT_EQUAL_PTR(declaration->data.structDeclaration.members->nodes[0]
                ->data.structField.typeInfo, member->fieldType);
        TEST_ASSERT_NOT_NULL(member->fieldTypeName);
        TEST_ASSERT_EQUAL_STRING(expectedType == ZR_VALUE_TYPE_INT64 ? "int" : "string",
                ZrCore_String_GetNativeString(member->fieldTypeName));
    }
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
    TEST_ASSERT_EQUAL_UINT32(g_types[0].nominalDefinitionToken,
            variableBinding->type.nominalDefinitionToken);
    TEST_ASSERT_EQUAL_INT(ZR_AST_EXPRESSION_STATEMENT, statement->type);
    expression = statement->data.expressionStatement.expr;
    TEST_ASSERT_NOT_NULL(expression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, expression->type);
    TEST_ASSERT_NOT_NULL(expression->data.primaryExpression.property);
    TEST_ASSERT_TRUE(ZrParser_IdentifierType_Infer(&g_compiler,
            expression->data.primaryExpression.property, &g_types[1]));
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_UINT32(g_types[0].nominalDefinitionToken,
            g_types[1].nominalDefinitionToken);
    return expression;
}

static void assert_receiver_result(SZrAstNode *expression, TZrBool publicEntry,
        EZrValueType expectedType) {
    if (publicEntry) {
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(
                &g_compiler, expression, &g_types[2]));
    } else {
        TEST_ASSERT_TRUE(infer_primary_member_chain_type(&g_compiler, expression,
                &g_types[1], expression->data.primaryExpression.members,
                0U, ZR_FALSE, &g_types[2]));
    }
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_INT(expectedType, g_types[2].baseType);
}

static void assert_single_receiver(TZrBool isMethod, SZrSyntaxContext context,
        TZrBool publicEntry) {
    SZrAstNodeArray *nodes = parse_source(isMethod ?
            "struct Widget { pub fn read(value: int): int { return value; } }"
            "var receiver: Widget; receiver.read(1);" :
            "native extern(\"fixture\") { struct Widget { pub var value: int; } }"
            "var receiver: Widget; receiver.value;");
    SZrAstNode *declaration;
    SZrAstNode *expression;

    TEST_ASSERT_EQUAL_UINT(3U, nodes->count);
    if (isMethod) {
        declaration = nodes->nodes[0];
    } else {
        TEST_ASSERT_EQUAL_INT(ZR_AST_EXTERN_BLOCK, nodes->nodes[0]->type);
        TEST_ASSERT_EQUAL_UINT(1U, nodes->nodes[0]->data.externBlock.declarations->count);
        declaration = nodes->nodes[0]->data.externBlock.declarations->nodes[0];
    }
    publish_struct(declaration, context, isMethod);
    expression = prepare_receiver(declaration, nodes->nodes[1], nodes->nodes[2],
            context, isMethod, ZR_VALUE_TYPE_INT64);
    assert_receiver_result(expression, publicEntry, ZR_VALUE_TYPE_INT64);
}

static void assert_pair_receiver(TZrBool isMethod, SZrSyntaxContext first,
        SZrSyntaxContext second, TZrSize selected, TZrBool publicEntry) {
    SZrAstNodeArray *nodes = parse_source(isMethod ? (selected == 0U ?
            "struct Widget { pub fn read(value: int): int { return value; } }"
            "struct Widget { pub fn read(value: string): string { return value; } }"
            "var receiver: Widget; receiver.read(1);" :
            "struct Widget { pub fn read(value: int): int { return value; } }"
            "struct Widget { pub fn read(value: string): string { return value; } }"
            "var receiver: Widget; receiver.read(\"marked\");") :
            "native extern(\"fixture\") {"
            "struct Widget { pub var value: int; }"
            "struct Widget { pub var value: string; } }"
            "var receiver: Widget; receiver.value;");
    SZrAstNodeArray *declarations;
    SZrAstNode *expression;
    TZrSize variableIndex = isMethod ? 2U : 1U;
    EZrValueType expectedType = selected == 0U ? ZR_VALUE_TYPE_INT64 : ZR_VALUE_TYPE_STRING;
    SZrSyntaxContext context = selected == 0U ? first : second;

    TEST_ASSERT_EQUAL_UINT(isMethod ? 4U : 3U, nodes->count);
    if (isMethod) {
        declarations = nodes;
    } else {
        TEST_ASSERT_EQUAL_INT(ZR_AST_EXTERN_BLOCK, nodes->nodes[0]->type);
        declarations = nodes->nodes[0]->data.externBlock.declarations;
        TEST_ASSERT_NOT_NULL(declarations);
        TEST_ASSERT_EQUAL_UINT(2U, declarations->count);
    }
    publish_struct(declarations->nodes[0], first, isMethod);
    publish_struct(declarations->nodes[1], second, isMethod);
    expression = prepare_receiver(declarations->nodes[selected],
            nodes->nodes[variableIndex], nodes->nodes[variableIndex + 1U],
            context, isMethod, expectedType);
    assert_receiver_result(expression, publicEntry, expectedType);
}

static void test_root_field_prerequisite(void) {
    assert_single_receiver(ZR_FALSE, ZrParser_SyntaxContext_Root(), ZR_FALSE);
}

static void test_root_field_public_entry_prerequisite(void) {
    assert_single_receiver(ZR_FALSE, ZrParser_SyntaxContext_Root(), ZR_TRUE);
}

static void test_mixed_item_field_keeps_root_identity(void) {
    assert_single_receiver(ZR_FALSE, g_mixed, ZR_FALSE);
}

static void test_definition_only_field_keeps_receiver_identity(void) {
    assert_single_receiver(ZR_FALSE, g_definition, ZR_FALSE);
}

static void test_root_then_definition_field_selects_definition(void) {
    assert_pair_receiver(ZR_FALSE, ZrParser_SyntaxContext_Root(), g_definition, 1U, ZR_FALSE);
}

static void test_definition_then_root_field_selects_definition(void) {
    assert_pair_receiver(ZR_FALSE, g_definition, ZrParser_SyntaxContext_Root(), 0U, ZR_FALSE);
}

static void test_two_opaque_fields_select_second_definition(void) {
    assert_pair_receiver(ZR_FALSE, g_definition, g_otherDefinition, 1U, ZR_FALSE);
}

static void test_root_receiver_excludes_definition_field(void) {
    assert_pair_receiver(ZR_FALSE, g_definition, ZrParser_SyntaxContext_Root(), 1U, ZR_FALSE);
}

static void test_marked_field_public_entry_keeps_identity(void) {
    assert_pair_receiver(ZR_FALSE, ZrParser_SyntaxContext_Root(), g_definition, 1U, ZR_TRUE);
}

static void test_root_method_prerequisite(void) {
    assert_single_receiver(ZR_TRUE, ZrParser_SyntaxContext_Root(), ZR_FALSE);
}

static void test_root_method_public_entry_prerequisite(void) {
    assert_single_receiver(ZR_TRUE, ZrParser_SyntaxContext_Root(), ZR_TRUE);
}

static void test_mixed_item_method_keeps_root_identity(void) {
    assert_single_receiver(ZR_TRUE, g_mixed, ZR_FALSE);
}

static void test_definition_only_method_keeps_receiver_identity(void) {
    assert_single_receiver(ZR_TRUE, g_definition, ZR_FALSE);
}

static void test_root_then_definition_method_selects_definition(void) {
    assert_pair_receiver(ZR_TRUE, ZrParser_SyntaxContext_Root(), g_definition, 1U, ZR_FALSE);
}

static void test_definition_then_root_method_selects_definition(void) {
    assert_pair_receiver(ZR_TRUE, g_definition, ZrParser_SyntaxContext_Root(), 0U, ZR_FALSE);
}

static void test_two_opaque_methods_select_second_definition(void) {
    assert_pair_receiver(ZR_TRUE, g_definition, g_otherDefinition, 1U, ZR_FALSE);
}

static void test_root_receiver_excludes_definition_method(void) {
    assert_pair_receiver(ZR_TRUE, g_definition, ZrParser_SyntaxContext_Root(), 1U, ZR_FALSE);
}

static void test_marked_method_public_entry_keeps_identity(void) {
    assert_pair_receiver(ZR_TRUE, ZrParser_SyntaxContext_Root(), g_definition, 1U, ZR_TRUE);
}

static void assert_generic_receiver(TZrBool isMethod, SZrSyntaxContext first,
        SZrSyntaxContext second, TZrBool publicEntry) {
    SZrAstNodeArray *nodes = parse_source(isMethod ?
            "struct Parcel<T> { pub fn read(value: int): int { return value; } }"
            "struct Parcel<T> { pub fn read(value: string): string { return value; } }"
            "var receiver: Parcel<int>; receiver.read(\"marked\");" :
            "struct Parcel<T> { pub var value: int; }"
            "struct Parcel<T> { pub var value: string; }"
            "var receiver: Parcel<int>; receiver.value;");
    SZrAstNode *expression;
    SZrTypePrototypeInfo *prototype;
    TZrTypeId instanceId;

    TEST_ASSERT_EQUAL_UINT(4U, nodes->count);
    publish_struct(nodes->nodes[0], first, ZR_TRUE);
    publish_struct(nodes->nodes[1], second, ZR_TRUE);
    expression = prepare_receiver(nodes->nodes[1], nodes->nodes[2], nodes->nodes[3],
            second, isMethod, ZR_VALUE_TYPE_STRING);
    TEST_ASSERT_EQUAL_UINT(1U, g_types[0].elementTypes.length);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64,
            ((SZrInferredType *)ZrCore_Array_Get(&g_types[0].elementTypes, 0U))->baseType);
    prototype = type_inference_find_nominal_prototype(&g_compiler, &g_types[0]);
    TEST_ASSERT_NOT_NULL(prototype);
    instanceId = prototype->genericInstanceTypeId;
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, instanceId);
    assert_receiver_result(expression, publicEntry, ZR_VALUE_TYPE_STRING);
    prototype = type_inference_find_nominal_prototype(&g_compiler, &g_types[0]);
    TEST_ASSERT_NOT_NULL(prototype);
    TEST_ASSERT_EQUAL_UINT32(instanceId, prototype->genericInstanceTypeId);
}

static void test_root_generic_field_prerequisite(void) {
    assert_generic_receiver(ZR_FALSE, g_definition, ZrParser_SyntaxContext_Root(), ZR_FALSE);
}

static void test_marked_generic_field_keeps_receiver_identity(void) {
    assert_generic_receiver(ZR_FALSE, ZrParser_SyntaxContext_Root(), g_definition, ZR_FALSE);
}

static void test_two_opaque_generic_fields_keep_receiver_identity(void) {
    assert_generic_receiver(ZR_FALSE, g_definition, g_otherDefinition, ZR_FALSE);
}

static void test_marked_generic_field_public_entry_keeps_identity(void) {
    assert_generic_receiver(ZR_FALSE, ZrParser_SyntaxContext_Root(), g_definition, ZR_TRUE);
}

static void test_root_generic_method_prerequisite(void) {
    assert_generic_receiver(ZR_TRUE, g_definition, ZrParser_SyntaxContext_Root(), ZR_FALSE);
}

static void test_marked_generic_method_keeps_receiver_identity(void) {
    assert_generic_receiver(ZR_TRUE, ZrParser_SyntaxContext_Root(), g_definition, ZR_FALSE);
}

static void test_two_opaque_generic_methods_keep_receiver_identity(void) {
    assert_generic_receiver(ZR_TRUE, g_definition, g_otherDefinition, ZR_FALSE);
}

static void test_marked_generic_method_public_entry_keeps_identity(void) {
    assert_generic_receiver(ZR_TRUE, ZrParser_SyntaxContext_Root(), g_definition, ZR_TRUE);
}

static void assert_invalid_receiver_identity(TZrBool isMethod) {
    SZrAstNodeArray *nodes = parse_source(isMethod ?
            "struct Widget { pub fn read(value: int): int { return value; } }"
            "var receiver: Widget; receiver.read(1);" :
            "native extern(\"fixture\") { struct Widget { pub var value: int; } }"
            "var receiver: Widget; receiver.value;");
    SZrAstNode *declaration = isMethod ? nodes->nodes[0] :
            nodes->nodes[0]->data.externBlock.declarations->nodes[0];
    SZrAstNode *expression;

    publish_struct(declaration, ZrParser_SyntaxContext_Root(), isMethod);
    expression = prepare_receiver(declaration, nodes->nodes[1], nodes->nodes[2],
            ZrParser_SyntaxContext_Root(), isMethod, ZR_VALUE_TYPE_INT64);
    g_types[1].nominalDefinitionToken = ~(TZrSymbolId)0U;
    TEST_ASSERT_NULL(type_inference_find_nominal_prototype(&g_compiler, &g_types[1]));
    assert_receiver_result(expression, ZR_FALSE, ZR_VALUE_TYPE_OBJECT);
}

static void test_invalid_field_receiver_never_borrows_root_member(void) {
    assert_invalid_receiver_identity(ZR_FALSE);
}

static void test_invalid_method_receiver_never_borrows_root_member(void) {
    assert_invalid_receiver_identity(ZR_TRUE);
}

static void test_imported_root_field_keeps_null_declaration(void) {
    SZrAstNodeArray *nodes = parse_source(
            "native extern(\"fixture\") { struct Widget { pub var value: int; } }"
            "var receiver: Widget; receiver.value;");
    SZrAstNode *declaration = nodes->nodes[0]->data.externBlock.declarations->nodes[0];
    SZrAstNode *expression;
    SZrTypePrototypeInfo *prototype;

    publish_struct(declaration, ZrParser_SyntaxContext_Root(), ZR_FALSE);
    expression = prepare_receiver(declaration, nodes->nodes[1], nodes->nodes[2],
            ZrParser_SyntaxContext_Root(), ZR_FALSE, ZR_VALUE_TYPE_INT64);
    prototype = type_inference_find_nominal_prototype(&g_compiler, &g_types[0]);
    TEST_ASSERT_NOT_NULL(prototype);
    prototype->isImportedNative = ZR_TRUE;
    prototype->declarationNode = ZR_NULL;
    assert_receiver_result(expression, ZR_FALSE, ZR_VALUE_TYPE_INT64);
    prototype = type_inference_find_nominal_prototype(&g_compiler, &g_types[0]);
    TEST_ASSERT_NOT_NULL(prototype);
    TEST_ASSERT_NULL(prototype->declarationNode);
}

static void assert_nominal_argument_receiver(SZrSyntaxContext argumentContext,
        TZrBool publicEntry) {
    SZrAstNodeArray *nodes = parse_source(
            "struct Parcel<T> { pub fn echo(value: T): T { return value; } }"
            "struct Widget {} struct Widget {}"
            "var receiver: Parcel<Widget>; var value: Widget; receiver.echo(value);");
    SZrType *receiverAnnotation;
    SZrAstNode *argumentNode;
    SZrAstNode *expression;
    SZrAstNode *valueDeclaration;
    SZrAstNode *valueIdentifier;
    SZrTypePrototypeInfo *prototype;
    SZrTypeMemberInfo *member;
    SZrInferredType *argument;
    TZrSymbolId argumentToken;
    TZrTypeId rootInstanceId;
    TZrTypeId selectedInstanceId;

    TEST_ASSERT_EQUAL_UINT(6U, nodes->count);
    publish_struct(nodes->nodes[0], ZrParser_SyntaxContext_Root(), ZR_TRUE);
    publish_struct(nodes->nodes[1], ZrParser_SyntaxContext_Root(), ZR_TRUE);
    argumentToken = publish_struct(nodes->nodes[2], g_definition, ZR_TRUE);
    if (ZrParser_SyntaxContext_Equal(argumentContext, ZrParser_SyntaxContext_Root())) {
        argumentToken = 0U;
    }
    receiverAnnotation = nodes->nodes[3]->data.variableDeclaration.typeInfo;
    TEST_ASSERT_EQUAL_INT(ZR_AST_GENERIC_TYPE, receiverAnnotation->name->type);
    TEST_ASSERT_EQUAL_UINT(1U, receiverAnnotation->name->data.genericType.params->count);
    argumentNode = receiverAnnotation->name->data.genericType.params->nodes[0];
    TEST_ASSERT_EQUAL_INT(ZR_AST_TYPE, argumentNode->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, argumentNode->data.type.name->type);
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, receiverAnnotation, &g_types[4]));
    TEST_ASSERT_TRUE(ensure_generic_instance_type_prototype_from_inferred(
            &g_compiler, &g_types[4]));
    prototype = type_inference_find_nominal_prototype(&g_compiler, &g_types[4]);
    TEST_ASSERT_NOT_NULL(prototype);
    rootInstanceId = prototype->genericInstanceTypeId;
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, rootInstanceId);
    argumentNode->data.type.name->data.identifier.syntaxContext = argumentContext;
    expression = prepare_receiver(nodes->nodes[0], nodes->nodes[3], nodes->nodes[5],
            ZrParser_SyntaxContext_Root(), ZR_TRUE, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_EQUAL_UINT32(0U, g_types[0].nominalDefinitionToken);
    TEST_ASSERT_EQUAL_UINT(1U, g_types[0].elementTypes.length);
    argument = ZrCore_Array_Get(&g_types[0].elementTypes, 0U);
    TEST_ASSERT_EQUAL_UINT32(argumentToken, argument->nominalDefinitionToken);
    prototype = type_inference_find_nominal_prototype(&g_compiler, &g_types[0]);
    TEST_ASSERT_NOT_NULL(prototype);
    selectedInstanceId = prototype->genericInstanceTypeId;
    if (argumentToken != 0U) {
        TEST_ASSERT_NOT_EQUAL(rootInstanceId, selectedInstanceId);
    } else {
        TEST_ASSERT_EQUAL_UINT32(rootInstanceId, selectedInstanceId);
    }
    member = ZrCore_Array_Get(&prototype->members, 0U);
    TEST_ASSERT_EQUAL_UINT32(argumentToken, member->structuredReturnType.nominalDefinitionToken);
    TEST_ASSERT_EQUAL_UINT32(argumentToken,
            ((SZrInferredType *)ZrCore_Array_Get(&member->parameterTypes, 0U))->nominalDefinitionToken);
    valueDeclaration = nodes->nodes[4];
    valueDeclaration->data.variableDeclaration.typeInfo->name->data.identifier.syntaxContext =
            argumentContext;
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(&g_compiler,
            valueDeclaration->data.variableDeclaration.typeInfo, &g_types[3]));
    TEST_ASSERT_EQUAL_UINT32(argumentToken, g_types[3].nominalDefinitionToken);
    valueIdentifier = valueDeclaration->data.variableDeclaration.pattern;
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariableExWithContext(g_state,
            g_compiler.typeEnv, valueIdentifier->data.identifier.name, &g_types[3],
            valueDeclaration, valueDeclaration->location, valueIdentifier->data.identifier.syntaxContext));
    assert_receiver_result(expression, publicEntry, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_EQUAL_UINT32(argumentToken, g_types[2].nominalDefinitionToken);
    prototype = type_inference_find_nominal_prototype(&g_compiler, &g_types[0]);
    TEST_ASSERT_NOT_NULL(prototype);
    TEST_ASSERT_EQUAL_UINT32(selectedInstanceId, prototype->genericInstanceTypeId);
}

static void test_root_nominal_argument_method_prerequisite(void) {
    assert_nominal_argument_receiver(ZrParser_SyntaxContext_Root(), ZR_FALSE);
}

static void test_method_receiver_preserves_marked_nominal_argument(void) {
    assert_nominal_argument_receiver(g_definition, ZR_FALSE);
}

static void test_public_method_receiver_preserves_marked_nominal_argument(void) {
    assert_nominal_argument_receiver(g_definition, ZR_TRUE);
}

static void assert_call_argument_grows_prototype_table(SZrSyntaxContext context) {
    TZrChar source[16384];
    int written;
    TZrSize offset;
    SZrAstNodeArray *nodes;
    SZrAstNode *expression;
    TZrSize nextFiller = 4U;
    TZrSize previousCapacity;
    TZrSize previousLength;

    written = snprintf(source, sizeof(source),
            "struct Widget { pub fn read(value): int { return 1; } }"
            "struct Widget { pub fn read(value): int { return 1; } }"
            "struct Growth<const N: int> {}"
            "var receiver: Widget;");
    TEST_ASSERT_GREATER_THAN_INT(0, written);
    offset = (TZrSize)written;
    for (TZrSize index = 0U; index < 128U; index++) {
        written = snprintf(source + offset, sizeof(source) - offset,
                "var filler%zu: Growth<%zu>;", index, index);
        TEST_ASSERT_GREATER_THAN_INT(0, written);
        TEST_ASSERT_LESS_THAN_UINT(sizeof(source) - offset, (TZrSize)written);
        offset += (TZrSize)written;
    }
    written = snprintf(source + offset, sizeof(source) - offset,
            "receiver.read(Growth<1000>);");
    TEST_ASSERT_GREATER_THAN_INT(0, written);
    TEST_ASSERT_LESS_THAN_UINT(sizeof(source) - offset, (TZrSize)written);
    nodes = parse_source(source);
    TEST_ASSERT_EQUAL_UINT(133U, nodes->count);
    publish_struct(nodes->nodes[0], ZrParser_SyntaxContext_Root(), ZR_TRUE);
    publish_struct(nodes->nodes[1], g_definition, ZR_TRUE);
    publish_struct(nodes->nodes[2], ZrParser_SyntaxContext_Root(), ZR_TRUE);
    expression = prepare_receiver(nodes->nodes[
            ZrParser_SyntaxContext_Equal(context, g_definition) ? 1U : 0U],
            nodes->nodes[3], nodes->nodes[132], context, ZR_TRUE, ZR_VALUE_TYPE_INT64);
    /* Parsed generic instances fill capacity; type-literal inference registers reflection metadata. */
    while (g_compiler.typePrototypes.length < g_compiler.typePrototypes.capacity) {
        TEST_ASSERT_LESS_THAN_UINT(132U, nextFiller);
        ZrParser_InferredType_Free(g_state, &g_types[3]);
        ZrParser_InferredType_Init(g_state, &g_types[3], ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(&g_compiler,
                nodes->nodes[nextFiller++]->data.variableDeclaration.typeInfo, &g_types[3]));
        TEST_ASSERT_TRUE(ensure_generic_instance_type_prototype_from_inferred(
                &g_compiler, &g_types[3]));
        TEST_ASSERT_FALSE(g_compiler.hasError);
    }
    previousCapacity = g_compiler.typePrototypes.capacity;
    previousLength = g_compiler.typePrototypes.length;
    TEST_ASSERT_GREATER_THAN_UINT(0U, previousCapacity);
    assert_receiver_result(expression, ZR_FALSE, ZR_VALUE_TYPE_INT64);
    TEST_ASSERT_GREATER_THAN_UINT(previousCapacity, g_compiler.typePrototypes.capacity);
    TEST_ASSERT_GREATER_THAN_UINT(previousLength, g_compiler.typePrototypes.length);
}

static void test_root_method_argument_metadata_registration_grows_prototypes(void) {
    assert_call_argument_grows_prototype_table(ZrParser_SyntaxContext_Root());
}

static void test_marked_method_argument_metadata_registration_grows_prototypes(void) {
    assert_call_argument_grows_prototype_table(g_definition);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_root_field_prerequisite);
    RUN_TEST(test_root_field_public_entry_prerequisite);
    RUN_TEST(test_mixed_item_field_keeps_root_identity);
    RUN_TEST(test_definition_only_field_keeps_receiver_identity);
    RUN_TEST(test_root_then_definition_field_selects_definition);
    RUN_TEST(test_definition_then_root_field_selects_definition);
    RUN_TEST(test_two_opaque_fields_select_second_definition);
    RUN_TEST(test_root_receiver_excludes_definition_field);
    RUN_TEST(test_marked_field_public_entry_keeps_identity);
    RUN_TEST(test_root_method_prerequisite);
    RUN_TEST(test_root_method_public_entry_prerequisite);
    RUN_TEST(test_mixed_item_method_keeps_root_identity);
    RUN_TEST(test_definition_only_method_keeps_receiver_identity);
    RUN_TEST(test_root_then_definition_method_selects_definition);
    RUN_TEST(test_definition_then_root_method_selects_definition);
    RUN_TEST(test_two_opaque_methods_select_second_definition);
    RUN_TEST(test_root_receiver_excludes_definition_method);
    RUN_TEST(test_marked_method_public_entry_keeps_identity);
    RUN_TEST(test_root_generic_field_prerequisite);
    RUN_TEST(test_marked_generic_field_keeps_receiver_identity);
    RUN_TEST(test_two_opaque_generic_fields_keep_receiver_identity);
    RUN_TEST(test_marked_generic_field_public_entry_keeps_identity);
    RUN_TEST(test_root_generic_method_prerequisite);
    RUN_TEST(test_marked_generic_method_keeps_receiver_identity);
    RUN_TEST(test_two_opaque_generic_methods_keep_receiver_identity);
    RUN_TEST(test_marked_generic_method_public_entry_keeps_identity);
    RUN_TEST(test_invalid_field_receiver_never_borrows_root_member);
    RUN_TEST(test_invalid_method_receiver_never_borrows_root_member);
    RUN_TEST(test_imported_root_field_keeps_null_declaration);
    RUN_TEST(test_root_nominal_argument_method_prerequisite);
    RUN_TEST(test_method_receiver_preserves_marked_nominal_argument);
    RUN_TEST(test_public_method_receiver_preserves_marked_nominal_argument);
    RUN_TEST(test_root_method_argument_metadata_registration_grows_prototypes);
    RUN_TEST(test_marked_method_argument_metadata_registration_grows_prototypes);
    return UNITY_END();
}
