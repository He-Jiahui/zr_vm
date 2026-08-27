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

static SZrAstNode *parse_wrapper_class(
        const TZrChar *source,
        const TZrChar *sourceName,
        SZrAstNode **outAst) {
    SZrString *name;
    SZrAstNode *ast;

    TEST_ASSERT_NOT_NULL(outAst);
    *outAst = ZR_NULL;
    name = ZrCore_String_Create(
            g_state, (TZrNativeString)sourceName, strlen(sourceName));
    TEST_ASSERT_NOT_NULL(name);
    ast = ZrParser_Parse(g_state, source, strlen(source), name);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    for (TZrSize index = 0U;
         index < ast->data.script.statements->count;
         index++) {
        SZrAstNode *statement = ast->data.script.statements->nodes[index];

        if (statement != ZR_NULL &&
            statement->type == ZR_AST_CLASS_DECLARATION) {
            *outAst = ast;
            return statement;
        }
    }
    ZrParser_Ast_Free(g_state, ast);
    TEST_FAIL_MESSAGE("Expected one wrapper class declaration");
    return ZR_NULL;
}

static SZrAstNode *find_decorator_at_offset(
        SZrAstNodeArray *decorators,
        TZrSize offset) {
    if (decorators == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrSize index = 0U; index < decorators->count; index++) {
        SZrAstNode *decorator = decorators->nodes[index];

        if (decorator != ZR_NULL &&
            decorator->location.start.offset == offset) {
            return decorator;
        }
    }
    return ZR_NULL;
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

static void assert_invalid_wrapper_decorator(
        const TZrChar *source,
        const TZrChar *sourceName,
        const TZrChar *decoratorText) {
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics diagnostics;
    const SZrStructuredDiagnostic *published;
    SZrCompilerState compiler;
    SZrAstNode *ast;
    SZrAstNode *classNode;
    SZrAstNode *decorator;
    const TZrChar *decoratorStartText;
    TZrSize decoratorStart;

    classNode = parse_wrapper_class(source, sourceName, &ast);
    TEST_ASSERT_NOT_NULL(classNode);
    decoratorStartText = strstr(source, decoratorText);
    TEST_ASSERT_NOT_NULL(decoratorStartText);
    decoratorStart = (TZrSize)(decoratorStartText - source);
    decorator = find_decorator_at_offset(
            classNode->data.classDeclaration.decorators,
            decoratorStart);
    TEST_ASSERT_NOT_NULL(decorator);
    TEST_ASSERT_EQUAL_UINT64(
            decoratorStart + strlen(decoratorText),
            decorator->location.end.offset);

    memset(&compiler, 0, sizeof(compiler));
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    compiler.scriptAst = ast;
    TEST_ASSERT_FALSE(
            ZrParser_Compiler_ValidateFfiWrapperDecorators(
                    &compiler, classNode));
    TEST_ASSERT_TRUE(compiler.hasStructuredError);
    TEST_ASSERT_EQUAL_UINT32(2019U, compiler.structuredError.descriptorId);
    TEST_ASSERT_EQUAL_STRING(
            "invalid_decorator",
            ZrCore_String_GetNativeString(compiler.structuredError.code));
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

    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_valid_ffi_wrapper_decorators_are_accepted(void) {
    const TZrChar *source =
            "native extern(\"fixture\") {\n"
            "    struct NativeView { var raw:i32; }\n"
            "}\n"
            "#zr.ffi.lowering(\"handle_id\")#\n"
            "#zr.ffi.underlying(\"i32\")#\n"
            "#zr.ffi.viewType(\"NativeView\")#\n"
            "#zr.ffi.ownerMode(\"owned\")#\n"
            "#zr.ffi.releaseHook(\"release_native\")#\n"
            "class Handle { var handleId:i32; }\n";
    SZrCompilerState compiler;
    SZrAstNode *ast;
    SZrAstNode *classNode = parse_wrapper_class(
            source, "valid_ffi_wrapper_decorators.zr", &ast);

    memset(&compiler, 0, sizeof(compiler));
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    compiler.scriptAst = ast;
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidateFfiWrapperDecorators(
            &compiler, classNode));
    TEST_ASSERT_FALSE(compiler.hasError);
    TEST_ASSERT_FALSE(compiler.hasStructuredError);

    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_invalid_wrapper_lowering_value_publishes_query_fact(void) {
    assert_invalid_wrapper_decorator(
            "#zr.ffi.lowering(\"bad\")#\nclass Handle {}\n",
            "invalid_wrapper_lowering.zr",
            "#zr.ffi.lowering(\"bad\")#");
}

static void test_wrapper_underlying_requires_handle_id_publishes_query_fact(void) {
    assert_invalid_wrapper_decorator(
            "#zr.ffi.lowering(\"value\")#\n"
            "#zr.ffi.underlying(\"i32\")#\n"
            "class Handle {}\n",
            "wrapper_underlying_requires_handle_id.zr",
            "#zr.ffi.underlying(\"i32\")#");
}

static void test_handle_id_requires_underlying_publishes_query_fact(void) {
    assert_invalid_wrapper_decorator(
            "#zr.ffi.lowering(\"handle_id\")#\nclass Handle {}\n",
            "handle_id_requires_underlying.zr",
            "#zr.ffi.lowering(\"handle_id\")#");
}

static void test_invalid_wrapper_underlying_value_publishes_query_fact(void) {
    assert_invalid_wrapper_decorator(
            "#zr.ffi.lowering(\"handle_id\")#\n"
            "#zr.ffi.underlying(\"string\")#\n"
            "class Handle {}\n",
            "invalid_wrapper_underlying.zr",
            "#zr.ffi.underlying(\"string\")#");
}

static void test_wrapper_view_type_requires_source_extern_struct(void) {
    assert_invalid_wrapper_decorator(
            "struct PlainView { var raw:i32; }\n"
            "#zr.ffi.viewType(\"PlainView\")#\n"
            "class Handle {}\n",
            "wrapper_view_type_requires_extern_struct.zr",
            "#zr.ffi.viewType(\"PlainView\")#");
}

static void test_invalid_wrapper_owner_mode_publishes_query_fact(void) {
    assert_invalid_wrapper_decorator(
            "#zr.ffi.ownerMode(\"shared\")#\nclass Handle {}\n",
            "invalid_wrapper_owner_mode.zr",
            "#zr.ffi.ownerMode(\"shared\")#");
}

static void test_invalid_wrapper_release_hook_shape_publishes_query_fact(void) {
    assert_invalid_wrapper_decorator(
            "#zr.ffi.releaseHook(42)#\nclass Handle {}\n",
            "invalid_wrapper_release_hook.zr",
            "#zr.ffi.releaseHook(42)#");
}

static void test_unknown_wrapper_ffi_decorator_publishes_query_fact(void) {
    assert_invalid_wrapper_decorator(
            "#zr.ffi.unknown(\"bad\")#\nclass Handle {}\n",
            "unknown_wrapper_ffi_decorator.zr",
            "#zr.ffi.unknown(\"bad\")#");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_valid_ffi_wrapper_decorators_are_accepted);
    RUN_TEST(test_invalid_wrapper_lowering_value_publishes_query_fact);
    RUN_TEST(test_wrapper_underlying_requires_handle_id_publishes_query_fact);
    RUN_TEST(test_handle_id_requires_underlying_publishes_query_fact);
    RUN_TEST(test_invalid_wrapper_underlying_value_publishes_query_fact);
    RUN_TEST(test_wrapper_view_type_requires_source_extern_struct);
    RUN_TEST(test_invalid_wrapper_owner_mode_publishes_query_fact);
    RUN_TEST(test_invalid_wrapper_release_hook_shape_publishes_query_fact);
    RUN_TEST(test_unknown_wrapper_ffi_decorator_publishes_query_fact);
    return UNITY_END();
}
