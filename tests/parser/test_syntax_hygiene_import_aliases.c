#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "compiler/compiler_type_value_alias.h"
#include "zr_vm_core/string.h"
#include "zr_vm_lib_math/module.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/syntax_context.h"
#include "zr_vm_parser/type_inference.h"
#include "zr_vm_parser/type_system.h"

static SZrState *g_state;
static SZrCompilerState g_compiler;
static TZrBool g_compilerInitialized;
static SZrAstNode *g_ast;
static SZrFunction *g_function;
static SZrFunction *g_executionFunction;
static SZrInferredType g_inferredType;
static TZrBool g_inferredInitialized;
static SZrSyntaxContextTable *g_contextTable;
static SZrSyntaxContext g_defSite;
static SZrSyntaxContext g_callSite;

void setUp(void) {
    SZrSyntaxExpansionInfo expansion = {0};
    TZrSyntaxExpansionId expansionId;

    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
    TEST_ASSERT_TRUE(ZrVmLibMath_Register(g_state->global));
    g_contextTable = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_contextTable);
    expansion.stableIdentity = 0xA112U;
    expansion.callSiteSpan = ZrParser_TokenSpan_None();
    expansion.definitionSiteSpan = ZrParser_TokenSpan_None();
    TEST_ASSERT_TRUE(ZrParser_SyntaxContextTable_RegisterExpansion(
            g_contextTable, &expansion, &expansionId));
    g_defSite = ZrParser_SyntaxContext_DefSite(g_contextTable, expansionId);
    g_callSite = ZrParser_SyntaxContext_CallSite(g_contextTable, expansionId);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(g_defSite));
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(g_callSite));
    ZrParser_CompilerState_Init(&g_compiler, g_state);
    g_compilerInitialized = ZR_TRUE;
    TEST_ASSERT_NOT_NULL(g_compiler.typeEnv);
    ZrParser_InferredType_Init(g_state, &g_inferredType, ZR_VALUE_TYPE_OBJECT);
    g_inferredInitialized = ZR_TRUE;
    g_compiler.suppressErrorOutput = ZR_TRUE;
    g_function = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(g_function);
    TEST_ASSERT_TRUE(ZrCore_Function_AcquireHostRoot(g_state, g_function));
    g_compiler.currentFunction = g_function;
}

void tearDown(void) {
    if (g_inferredInitialized) {
        ZrParser_InferredType_Free(g_state, &g_inferredType);
        g_inferredInitialized = ZR_FALSE;
    }
    if (g_compilerInitialized) {
        ZrParser_CompilerState_Free(&g_compiler);
        g_compilerInitialized = ZR_FALSE;
    }
    if (g_function != ZR_NULL) {
        ZrCore_Function_Free(g_state, g_function);
        g_function = ZR_NULL;
    }
    if (g_executionFunction != ZR_NULL) {
        ZrCore_Function_Free(g_state, g_executionFunction);
        g_executionFunction = ZR_NULL;
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

static SZrAstNodeArray *parse_imports(const TZrChar *source) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "import_alias_hygiene.zr");

    TEST_ASSERT_NOT_NULL(sourceName);
    g_ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(g_ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_ast->type);
    TEST_ASSERT_NOT_NULL(g_ast->data.script.statements);
    return g_ast->data.script.statements;
}

static SZrIdentifier *binding_identifier(SZrAstNode *declaration) {
    SZrAstNode *pattern;
    SZrAstNode *entry;

    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, declaration->type);
    pattern = declaration->data.variableDeclaration.pattern;
    TEST_ASSERT_NOT_NULL(pattern);
    TEST_ASSERT_EQUAL_INT(ZR_AST_DESTRUCTURING_OBJECT, pattern->type);
    TEST_ASSERT_NOT_NULL(pattern->data.destructuringObject.keys);
    TEST_ASSERT_EQUAL_UINT(1U, pattern->data.destructuringObject.keys->count);
    entry = pattern->data.destructuringObject.keys->nodes[0];
    TEST_ASSERT_NOT_NULL(entry);
    if (entry->type == ZR_AST_KEY_VALUE_PAIR) {
        entry = entry->data.keyValuePair.key;
    }
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, entry->type);
    return &entry->data.identifier;
}

static void assert_registered_alias(SZrString *name, SZrSyntaxContext context,
        const TZrChar *qualifiedTypeName) {
    SZrTypeBinding *alias = compiler_find_type_value_alias(
            &g_compiler, name, context);

    TEST_ASSERT_NOT_NULL(alias);
    TEST_ASSERT_NOT_NULL(alias->type.typeName);
    TEST_ASSERT_EQUAL_STRING(qualifiedTypeName,
            ZrCore_String_GetNativeString(alias->type.typeName));
}

static void test_root_import_alias_prerequisite(void) {
    SZrAstNodeArray *statements = parse_imports(
            "var {Vec: Vector3} = import(\"zr.math\");");
    SZrIdentifier *binding = binding_identifier(statements->nodes[0]);

    ZrParser_Statement_Compile(&g_compiler, statements->nodes[0]);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    assert_registered_alias(binding->name, ZrParser_SyntaxContext_Root(),
            "zr.math.Vector3");
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_LookupType(
            g_compiler.typeEnv, binding->name));
}

static void test_definition_import_alias_keeps_binding_context(void) {
    SZrAstNodeArray *statements = parse_imports(
            "var {Vec: Vector3} = import(\"zr.math\");");
    SZrIdentifier *binding = binding_identifier(statements->nodes[0]);

    binding->syntaxContext = g_defSite;
    ZrParser_Statement_Compile(&g_compiler, statements->nodes[0]);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    assert_registered_alias(binding->name, g_defSite, "zr.math.Vector3");
}

static void test_definition_import_alias_does_not_publish_root_name(void) {
    SZrAstNodeArray *statements = parse_imports(
            "var {Vec: Vector3} = import(\"zr.math\");");
    SZrIdentifier *binding = binding_identifier(statements->nodes[0]);

    binding->syntaxContext = g_defSite;
    ZrParser_Statement_Compile(&g_compiler, statements->nodes[0]);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_NULL(compiler_find_type_value_alias(
            &g_compiler, binding->name, ZrParser_SyntaxContext_Root()));
    TEST_ASSERT_FALSE(ZrParser_TypeEnvironment_LookupType(
            g_compiler.typeEnv, binding->name));
}

static void test_definition_and_root_import_aliases_are_independent(void) {
    SZrAstNodeArray *statements = parse_imports(
            "var {Vec: Vector3} = import(\"zr.math\");"
            "var {Vec: Vector2} = import(\"zr.math\");");
    SZrIdentifier *binding = binding_identifier(statements->nodes[0]);

    binding->syntaxContext = g_defSite;
    ZrParser_Statement_Compile(&g_compiler, statements->nodes[0]);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    ZrParser_Statement_Compile(&g_compiler, statements->nodes[1]);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    assert_registered_alias(binding->name, g_defSite, "zr.math.Vector3");
    assert_registered_alias(binding->name, ZrParser_SyntaxContext_Root(),
            "zr.math.Vector2");
}

static void test_later_definition_import_alias_preserves_root_alias(void) {
    SZrAstNodeArray *statements = parse_imports(
            "var {Vec: Vector2} = import(\"zr.math\");"
            "var {Vec: Vector3} = import(\"zr.math\");");
    SZrIdentifier *binding = binding_identifier(statements->nodes[1]);

    binding->syntaxContext = g_defSite;
    ZrParser_Statement_Compile(&g_compiler, statements->nodes[0]);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    ZrParser_Statement_Compile(&g_compiler, statements->nodes[1]);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    assert_registered_alias(binding->name, ZrParser_SyntaxContext_Root(),
            "zr.math.Vector2");
    assert_registered_alias(binding->name, g_defSite, "zr.math.Vector3");
}

static void test_duplicate_definition_import_alias_is_rejected(void) {
    SZrAstNodeArray *statements = parse_imports(
            "var {Vec: Vector3} = import(\"zr.math\");"
            "var {Vec: Vector2} = import(\"zr.math\");");

    binding_identifier(statements->nodes[0])->syntaxContext = g_defSite;
    binding_identifier(statements->nodes[1])->syntaxContext = g_defSite;
    ZrParser_Statement_Compile(&g_compiler, statements->nodes[0]);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    ZrParser_Statement_Compile(&g_compiler, statements->nodes[1]);
    TEST_ASSERT_TRUE(g_compiler.hasError);
    assert_registered_alias(binding_identifier(statements->nodes[0])->name,
            g_defSite, "zr.math.Vector3");
}

static void test_call_site_import_alias_remains_visible_from_root(void) {
    SZrAstNodeArray *statements = parse_imports(
            "var {Vec: Vector3} = import(\"zr.math\");");
    SZrIdentifier *binding = binding_identifier(statements->nodes[0]);

    binding->syntaxContext = g_callSite;
    ZrParser_Statement_Compile(&g_compiler, statements->nodes[0]);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    assert_registered_alias(binding->name, g_callSite, "zr.math.Vector3");
    assert_registered_alias(binding->name, ZrParser_SyntaxContext_Root(),
            "zr.math.Vector3");
}

static void test_import_alias_annotations_select_their_binding_context(void) {
    SZrAstNodeArray *statements = parse_imports(
            "var {Vec: Vector3} = import(\"zr.math\");"
            "var {Vec: Vector2} = import(\"zr.math\");"
            "var hidden: Vec; var visible: Vec;");
    SZrType *hiddenType = statements->nodes[2]->data.variableDeclaration.typeInfo;
    SZrType *visibleType = statements->nodes[3]->data.variableDeclaration.typeInfo;

    binding_identifier(statements->nodes[0])->syntaxContext = g_defSite;
    TEST_ASSERT_NOT_NULL(hiddenType);
    TEST_ASSERT_NOT_NULL(hiddenType->name);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, hiddenType->name->type);
    hiddenType->name->data.identifier.syntaxContext = g_defSite;
    ZrParser_Statement_Compile(&g_compiler, statements->nodes[0]);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    ZrParser_Statement_Compile(&g_compiler, statements->nodes[1]);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, hiddenType, &g_inferredType));
    TEST_ASSERT_EQUAL_STRING("zr.math.Vector3",
            ZrCore_String_GetNativeString(g_inferredType.typeName));
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, visibleType, &g_inferredType));
    TEST_ASSERT_EQUAL_STRING("zr.math.Vector2",
            ZrCore_String_GetNativeString(g_inferredType.typeName));
}

static void test_shorthand_import_alias_keeps_definition_context(void) {
    SZrAstNodeArray *statements = parse_imports(
            "var {Vector3} = import(\"zr.math\");");
    SZrIdentifier *binding = binding_identifier(statements->nodes[0]);

    binding->syntaxContext = g_defSite;
    ZrParser_Statement_Compile(&g_compiler, statements->nodes[0]);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    assert_registered_alias(binding->name, g_defSite, "zr.math.Vector3");
    TEST_ASSERT_NULL(compiler_find_type_value_alias(
            &g_compiler, binding->name, ZrParser_SyntaxContext_Root()));
}

static void test_import_member_context_does_not_replace_destination_context(void) {
    SZrAstNodeArray *statements = parse_imports(
            "var {Vec: Vector3} = import(\"zr.math\");");
    SZrIdentifier *binding = binding_identifier(statements->nodes[0]);
    SZrAstNode *entry = statements->nodes[0]->data.variableDeclaration.pattern
            ->data.destructuringObject.keys->nodes[0];

    TEST_ASSERT_EQUAL_INT(ZR_AST_KEY_VALUE_PAIR, entry->type);
    TEST_ASSERT_NOT_NULL(entry->data.keyValuePair.value);
    entry->data.keyValuePair.value->data.identifier.syntaxContext = g_defSite;
    ZrParser_Statement_Compile(&g_compiler, statements->nodes[0]);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    assert_registered_alias(binding->name, ZrParser_SyntaxContext_Root(),
            "zr.math.Vector3");
    TEST_ASSERT_NULL(compiler_find_type_value_alias(&g_compiler, binding->name, g_defSite));
}

static void test_invalid_import_binding_context_is_rejected(void) {
    SZrAstNodeArray *statements = parse_imports(
            "var {Vec: Vector3} = import(\"zr.math\");");
    SZrIdentifier *binding = binding_identifier(statements->nodes[0]);

    binding->syntaxContext.table = g_contextTable;
    binding->syntaxContext.markId = ZR_SYNTAX_CONTEXT_INVALID_MARK_ID;
    ZrParser_Statement_Compile(&g_compiler, statements->nodes[0]);
    TEST_ASSERT_TRUE(g_compiler.hasError);
    TEST_ASSERT_NULL(compiler_find_type_value_alias(
            &g_compiler, binding->name, ZrParser_SyntaxContext_Root()));
}

static void assert_executed_result(TZrInt64 expected) {
    TZrInt64 result;

    g_executionFunction = ZrParser_Compiler_Compile(g_state, g_ast);
    TEST_ASSERT_NOT_NULL(g_executionFunction);
    TEST_ASSERT_TRUE(ZrCore_Function_AcquireHostRoot(g_state, g_executionFunction));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, g_executionFunction, &result));
    TEST_ASSERT_EQUAL_INT64(expected, result);
}

static void test_root_import_alias_annotation_accepts_native_value(void) {
    parse_imports(
            "var math = import(\"zr.math\");"
            "var {Vec: Vector3} = import(\"zr.math\");"
            "var value: Vec = init math.Vector3(7.0, 2.0, 3.0); return <int>value.x;");
    assert_executed_result(7);
}

static void test_marked_import_alias_annotation_executes_beside_root_array_alias(void) {
    SZrAstNodeArray *statements = parse_imports(
            "var math = import(\"zr.math\");"
            "var {Vec: Vector3} = import(\"zr.math\");"
            "var Vec = int[];"
            "var hidden: Vec = init math.Vector3(7.0, 2.0, 3.0);"
            "var visible: Vec = [11];"
            "return <int>hidden.x + visible[0];");
    SZrType *hiddenType = statements->nodes[3]->data.variableDeclaration.typeInfo;

    binding_identifier(statements->nodes[1])->syntaxContext = g_defSite;
    TEST_ASSERT_NOT_NULL(hiddenType);
    TEST_ASSERT_NOT_NULL(hiddenType->name);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, hiddenType->name->type);
    hiddenType->name->data.identifier.syntaxContext = g_defSite;
    assert_executed_result(18);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_root_import_alias_prerequisite);
    RUN_TEST(test_definition_import_alias_keeps_binding_context);
    RUN_TEST(test_definition_import_alias_does_not_publish_root_name);
    RUN_TEST(test_definition_and_root_import_aliases_are_independent);
    RUN_TEST(test_later_definition_import_alias_preserves_root_alias);
    RUN_TEST(test_duplicate_definition_import_alias_is_rejected);
    RUN_TEST(test_call_site_import_alias_remains_visible_from_root);
    RUN_TEST(test_import_alias_annotations_select_their_binding_context);
    RUN_TEST(test_shorthand_import_alias_keeps_definition_context);
    RUN_TEST(test_import_member_context_does_not_replace_destination_context);
    RUN_TEST(test_invalid_import_binding_context_is_rejected);
    RUN_TEST(test_root_import_alias_annotation_accepts_native_value);
    RUN_TEST(test_marked_import_alias_annotation_executes_beside_root_array_alias);
    return UNITY_END();
}
