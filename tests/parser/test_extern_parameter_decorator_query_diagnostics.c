#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic_query.h"

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

static SZrAstNode *parse_extern_parameter(
        const TZrChar *source,
        const TZrChar *sourceName,
        SZrAstNode **outAst) {
    SZrString *name;
    SZrAstNode *ast;
    SZrAstNode *externBlock;
    SZrAstNode *declaration;

    TEST_ASSERT_NOT_NULL(outAst);
    *outAst = ZR_NULL;
    name = ZrCore_String_Create(
            g_state, (TZrNativeString)sourceName, strlen(sourceName));
    TEST_ASSERT_NOT_NULL(name);
    ast = ZrParser_Parse(g_state, source, strlen(source), name);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT64(1U, ast->data.script.statements->count);
    externBlock = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(externBlock);
    TEST_ASSERT_EQUAL_INT(ZR_AST_EXTERN_BLOCK, externBlock->type);
    TEST_ASSERT_NOT_NULL(externBlock->data.externBlock.declarations);
    TEST_ASSERT_EQUAL_UINT64(
            1U, externBlock->data.externBlock.declarations->count);
    declaration = externBlock->data.externBlock.declarations->nodes[0];
    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_EXTERN_DELEGATE_DECLARATION, declaration->type);
    TEST_ASSERT_NOT_NULL(declaration->data.externDelegateDeclaration.params);
    TEST_ASSERT_EQUAL_UINT64(
            1U, declaration->data.externDelegateDeclaration.params->count);
    *outAst = ast;
    return declaration->data.externDelegateDeclaration.params->nodes[0];
}

static const SZrStructuredDiagnostic *find_diagnostic_by_code(
        const SZrParserSemanticQueryDiagnostics *diagnostics,
        const TZrChar *code) {
    if (diagnostics == ZR_NULL || code == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrSize index = 0U; index < diagnostics->count; index++) {
        const SZrStructuredDiagnostic *diagnostic = &diagnostics->items[index];
        if (diagnostic->code != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(diagnostic->code), code) == 0) {
            return diagnostic;
        }
    }
    return ZR_NULL;
}

static void assert_invalid_parameter_decorator(
        const TZrChar *source,
        const TZrChar *sourceName,
        TZrSize decoratorIndex,
        const TZrChar *decoratorText,
        const TZrChar *expectedMessage) {
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics diagnostics;
    const SZrStructuredDiagnostic *published;
    SZrCompilerState compiler;
    SZrAstNode *ast;
    SZrAstNode *parameter;
    SZrAstNode *decorator;
    TZrSize decoratorStart;

    parameter = parse_extern_parameter(source, sourceName, &ast);
    TEST_ASSERT_NOT_NULL(parameter);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PARAMETER, parameter->type);
    TEST_ASSERT_NOT_NULL(parameter->data.parameter.decorators);
    TEST_ASSERT_TRUE(decoratorIndex < parameter->data.parameter.decorators->count);
    decorator = parameter->data.parameter.decorators->nodes[decoratorIndex];
    TEST_ASSERT_NOT_NULL(decorator);
    decoratorStart = (TZrSize)(strstr(source, decoratorText) - source);
    TEST_ASSERT_EQUAL_UINT64(decoratorStart, decorator->location.start.offset);
    TEST_ASSERT_EQUAL_UINT64(
            decoratorStart + strlen(decoratorText), decorator->location.end.offset);

    memset(&compiler, 0, sizeof(compiler));
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    TEST_ASSERT_FALSE(ZrParser_Compiler_ValidateExternParameterDecorators(
            &compiler, parameter));
    TEST_ASSERT_TRUE(compiler.hasError);
    TEST_ASSERT_TRUE(compiler.hasStructuredError);
    TEST_ASSERT_EQUAL_UINT32(2019U, compiler.structuredError.descriptorId);
    TEST_ASSERT_NOT_NULL(compiler.structuredError.code);
    TEST_ASSERT_EQUAL_STRING(
            "invalid_decorator",
            ZrCore_String_GetNativeString(compiler.structuredError.code));
    TEST_ASSERT_EQUAL_STRING(
            expectedMessage,
            ZrCore_String_GetNativeString(compiler.structuredError.message));
    TEST_ASSERT_EQUAL_UINT64(
            decorator->location.start.offset,
            compiler.structuredError.location.start.offset);
    TEST_ASSERT_EQUAL_UINT64(
            decorator->location.end.offset,
            compiler.structuredError.location.end.offset);
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION,
            compiler.structuredError.noFixReason);
    TEST_ASSERT_FALSE(compiler.structuredError.fixes.isValid);

    TEST_ASSERT_TRUE(ZrParser_Compiler_PublishCurrentDiagnostic(&compiler));
    compiler.hasError = ZR_FALSE;
    ZrParser_Compiler_ClearStructuredError(&compiler);
    ZrParser_SemanticQueryScope_Module(&scope);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_MaterializeDiagnostics(
            compiler.semanticContext, &scope));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_Diagnostics(
            compiler.semanticContext, &scope, &diagnostics));
    published = find_diagnostic_by_code(&diagnostics, "invalid_decorator");
    TEST_ASSERT_NOT_NULL(published);
    TEST_ASSERT_EQUAL_UINT32(2019U, published->descriptorId);
    TEST_ASSERT_EQUAL_UINT64(
            decorator->location.start.offset, published->location.start.offset);
    TEST_ASSERT_EQUAL_UINT64(
            decorator->location.end.offset, published->location.end.offset);
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION,
            published->noFixReason);

    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_valid_extern_parameter_decorators_are_accepted(void) {
    const TZrChar *source =
            "native extern(\"fixture\") {\n"
            "    delegate Read(\n"
            "        #zr.ffi.inout#\n"
            "        #zr.ffi.charset(\"utf8\")#\n"
            "        value:pointer<u8>\n"
            "    ): void;\n"
            "}\n";
    SZrCompilerState compiler;
    SZrAstNode *ast;
    SZrAstNode *parameter = parse_extern_parameter(
            source, "valid_extern_parameter_decorators.zr", &ast);

    memset(&compiler, 0, sizeof(compiler));
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidateExternParameterDecorators(
            &compiler, parameter));
    TEST_ASSERT_FALSE(compiler.hasError);
    TEST_ASSERT_FALSE(compiler.hasStructuredError);

    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_direction_arguments_publish_query_fact(void) {
    const TZrChar *source =
            "native extern(\"fixture\") {\n"
            "    delegate Read(#zr.ffi.in(1)# value:pointer<u8>): void;\n"
            "}\n";
    assert_invalid_parameter_decorator(
            source,
            "invalid_extern_parameter_direction_shape.zr",
            0U,
            "#zr.ffi.in(1)#",
            "zr.ffi.in has invalid arguments for this extern parameter");
}

static void test_conflicting_directions_publish_second_decorator_range(void) {
    const TZrChar *source =
            "native extern(\"fixture\") {\n"
            "    delegate Read(\n"
            "        #zr.ffi.in#\n"
            "        #zr.ffi.out#\n"
            "        value:pointer<u8>\n"
            "    ): void;\n"
            "}\n";
    assert_invalid_parameter_decorator(
            source,
            "conflicting_extern_parameter_directions.zr",
            1U,
            "#zr.ffi.out#",
            "Extern parameters may specify only one of zr.ffi.in/out/inout");
}

static void test_charset_shape_publishes_query_fact(void) {
    const TZrChar *source =
            "native extern(\"fixture\") {\n"
            "    delegate Read(#zr.ffi.charset(8)# value:string): void;\n"
            "}\n";
    assert_invalid_parameter_decorator(
            source,
            "invalid_extern_parameter_charset_shape.zr",
            0U,
            "#zr.ffi.charset(8)#",
            "zr.ffi.charset has invalid arguments for this extern parameter");
}

static void test_charset_requires_call_shape(void) {
    const TZrChar *source =
            "native extern(\"fixture\") {\n"
            "    delegate Read(#zr.ffi.charset# value:string): void;\n"
            "}\n";
    assert_invalid_parameter_decorator(
            source,
            "invalid_extern_parameter_charset_call_shape.zr",
            0U,
            "#zr.ffi.charset#",
            "zr.ffi.charset has invalid arguments for this extern parameter");
}

static void test_charset_value_publishes_query_fact(void) {
    const TZrChar *source =
            "native extern(\"fixture\") {\n"
            "    delegate Read(#zr.ffi.charset(\"utf32\")# value:string): void;\n"
            "}\n";
    assert_invalid_parameter_decorator(
            source,
            "invalid_extern_parameter_charset_value.zr",
            0U,
            "#zr.ffi.charset(\"utf32\")#",
            "zr.ffi.charset has invalid arguments for this extern parameter");
}

static void test_unknown_parameter_decorator_publishes_query_fact(void) {
    const TZrChar *source =
            "native extern(\"fixture\") {\n"
            "    delegate Read(#zr.ffi.ownerMode(\"borrowed\")# value:pointer<u8>): void;\n"
            "}\n";
    assert_invalid_parameter_decorator(
            source,
            "unknown_extern_parameter_decorator.zr",
            0U,
            "#zr.ffi.ownerMode(\"borrowed\")#",
            "Decorator is not valid on extern parameters");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_valid_extern_parameter_decorators_are_accepted);
    RUN_TEST(test_direction_arguments_publish_query_fact);
    RUN_TEST(test_conflicting_directions_publish_second_decorator_range);
    RUN_TEST(test_charset_shape_publishes_query_fact);
    RUN_TEST(test_charset_requires_call_shape);
    RUN_TEST(test_charset_value_publishes_query_fact);
    RUN_TEST(test_unknown_parameter_decorator_publishes_query_fact);
    return UNITY_END();
}
