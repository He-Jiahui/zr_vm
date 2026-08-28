#include <string.h>

#include "unity.h"

#include "zr_vm_core/string.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/interface_contract.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic_query.h"

#include "harness/runtime_support.h"

static SZrState *g_state;

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

static void init_prototype(
        SZrTypePrototypeInfo *prototype,
        SZrString *name,
        SZrAstNode *declaration,
        EZrObjectPrototypeType type) {
    memset(prototype, 0, sizeof(*prototype));
    prototype->name = name;
    prototype->declarationNode = declaration;
    prototype->type = type;
    prototype->accessModifier = ZR_ACCESS_PUBLIC;
    ZrCore_Array_Init(g_state, &prototype->inherits, sizeof(SZrString *), 1U);
    ZrCore_Array_Init(g_state, &prototype->implements, sizeof(SZrString *), 1U);
    ZrCore_Array_Init(
            g_state,
            &prototype->genericParameters,
            sizeof(SZrTypeGenericParameterInfo),
            1U);
    ZrCore_Array_Init(
            g_state,
            &prototype->members,
            sizeof(SZrTypeMemberInfo),
            2U);
    ZrCore_Array_Init(
            g_state,
            &prototype->decorators,
            sizeof(SZrTypeDecoratorInfo),
            1U);
}

static void append_interface_field(
        SZrTypePrototypeInfo *prototype,
        SZrAstNode *fieldNode) {
    SZrTypeMemberInfo member;

    TEST_ASSERT_NOT_NULL(fieldNode);
    TEST_ASSERT_EQUAL_INT(
            ZR_AST_INTERFACE_FIELD_DECLARATION, fieldNode->type);
    memset(&member, 0, sizeof(member));
    member.memberType = ZR_AST_CLASS_FIELD;
    member.name = fieldNode->data.interfaceFieldDeclaration.name->name;
    member.declarationNode = fieldNode;
    member.accessModifier = fieldNode->data.interfaceFieldDeclaration.access;
    member.isConst = fieldNode->data.interfaceFieldDeclaration.isConst;
    ZrCore_Array_Push(g_state, &prototype->members, &member);
}

static void append_class_field(
        SZrTypePrototypeInfo *prototype,
        SZrAstNode *fieldNode) {
    SZrTypeMemberInfo member;

    TEST_ASSERT_NOT_NULL(fieldNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_FIELD, fieldNode->type);
    memset(&member, 0, sizeof(member));
    member.memberType = ZR_AST_CLASS_FIELD;
    member.name = fieldNode->data.classField.name->name;
    member.declarationNode = fieldNode;
    member.accessModifier = fieldNode->data.classField.access;
    member.isConst = fieldNode->data.classField.isConst;
    ZrCore_Array_Push(g_state, &prototype->members, &member);
}

static TZrSize count_interface_const_diagnostics(
        const SZrParserSemanticQueryDiagnostics *diagnostics) {
    TZrSize count = 0U;

    for (TZrSize index = 0U;
         diagnostics != ZR_NULL && index < diagnostics->count;
         index++) {
        const SZrStructuredDiagnostic *diagnostic = &diagnostics->items[index];
        const TZrChar *code = diagnostic->code != ZR_NULL
                                      ? ZrCore_String_GetNativeString(diagnostic->code)
                                      : ZR_NULL;
        if (code != ZR_NULL && strcmp(code, "const_interface_mismatch") == 0) {
            count++;
            TEST_ASSERT_EQUAL_UINT32(2014U, diagnostic->descriptorId);
            TEST_ASSERT_EQUAL_INT(
                    ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION,
                    diagnostic->noFixReason);
            TEST_ASSERT_TRUE(diagnostic->relatedInformation.isValid);
            TEST_ASSERT_EQUAL_UINT32(
                    1U, (TZrUInt32)diagnostic->relatedInformation.length);
            TEST_ASSERT_FALSE(diagnostic->fixes.isValid);
        }
    }
    return count;
}

static void test_publisher_emits_all_interface_const_violations(void) {
    static TZrChar source[] =
            "interface Versioned {\n"
            "    pub const version: int;\n"
            "    pub const generation: int;\n"
            "}\n"
            "class MutableVersion: Versioned {\n"
            "    pub var version: int;\n"
            "}\n";
    SZrString *sourceName = ZrCore_String_Create(
            g_state,
            "compiler_interface_const_query_producer_test.zr",
            strlen("compiler_interface_const_query_producer_test.zr"));
    SZrAstNode *script = ZrParser_Parse(
            g_state, source, strlen(source), sourceName);
    SZrAstNode *interfaceNode;
    SZrAstNode *classNode;
    SZrCompilerState compiler;
    SZrTypePrototypeInfo interfacePrototype;
    SZrTypePrototypeInfo classPrototype;
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics diagnostics;
    static const TZrChar existingError[] = "existing compiler error";

    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, script->type);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(2U, script->data.script.statements->count);
    interfaceNode = script->data.script.statements->nodes[0U];
    classNode = script->data.script.statements->nodes[1U];
    TEST_ASSERT_NOT_NULL(interfaceNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_INTERFACE_DECLARATION, interfaceNode->type);
    TEST_ASSERT_NOT_NULL(classNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, classNode->type);
    TEST_ASSERT_NOT_NULL(interfaceNode->data.interfaceDeclaration.members);
    TEST_ASSERT_EQUAL_UINT32(
            2U, interfaceNode->data.interfaceDeclaration.members->count);
    TEST_ASSERT_NOT_NULL(classNode->data.classDeclaration.members);
    TEST_ASSERT_EQUAL_UINT32(1U, classNode->data.classDeclaration.members->count);

    memset(&compiler, 0, sizeof(compiler));
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    init_prototype(
            &interfacePrototype,
            interfaceNode->data.interfaceDeclaration.name->name,
            interfaceNode,
            ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE);
    append_interface_field(
            &interfacePrototype,
            interfaceNode->data.interfaceDeclaration.members->nodes[0U]);
    append_interface_field(
            &interfacePrototype,
            interfaceNode->data.interfaceDeclaration.members->nodes[1U]);
    ZrCore_Array_Push(
            g_state, &compiler.typePrototypes, &interfacePrototype);

    init_prototype(
            &classPrototype,
            classNode->data.classDeclaration.name->name,
            classNode,
            ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    {
        SZrString *interfaceName = interfacePrototype.name;
        ZrCore_Array_Push(
                g_state, &classPrototype.inherits, &interfaceName);
    }
    append_class_field(
            &classPrototype,
            classNode->data.classDeclaration.members->nodes[0U]);
    ZrCore_Array_Push(g_state, &compiler.typePrototypes, &classPrototype);

    compiler.hasError = ZR_TRUE;
    compiler.hasStructuredError = ZR_FALSE;
    compiler.errorMessage = existingError;
    TEST_ASSERT_TRUE(ZrParser_InterfaceContract_PublishConstFieldDiagnostics(
            &compiler, classNode));
    TEST_ASSERT_TRUE(compiler.hasError);
    TEST_ASSERT_FALSE(compiler.hasStructuredError);
    TEST_ASSERT_EQUAL_PTR(existingError, compiler.errorMessage);

    ZrParser_SemanticQueryScope_Module(&scope);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_MaterializeDiagnostics(
            compiler.semanticContext, &scope));
    memset(&diagnostics, 0, sizeof(diagnostics));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_Diagnostics(
            compiler.semanticContext, &scope, &diagnostics));
    TEST_ASSERT_EQUAL_UINT32(
            2U, count_interface_const_diagnostics(&diagnostics));

    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, script);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_publisher_emits_all_interface_const_violations);
    return UNITY_END();
}
