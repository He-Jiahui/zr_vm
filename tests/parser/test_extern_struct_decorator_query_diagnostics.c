#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic_query.h"

/* Unity 每个用例各建一个运行时；AST 与编译器状态仍由用例自身负责释放。 */
static SZrState *g_state;

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    /* BUG: Unity 断言失败会跳过用例末尾的 Ast_Free/CompilerState_Free；
     * 这里只销毁运行时，解析树和编译器的原生资源未按所有权契约清理。 */
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

/** @brief 为装饰器测试取得唯一 extern 结构声明，隔离后续编译阶段的影响。
 * @note 返回的声明借用自 *outAst；调用方须在编译器状态释放后释放整棵 AST。
 */
static SZrAstNode *parse_extern_struct(
        const TZrChar *source,
        const TZrChar *sourceName,
        SZrAstNode **outAst) {
    SZrString *name;
    SZrAstNode *ast;
    SZrAstNode *externBlock;

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
    *outAst = ast;
    return externBlock->data.externBlock.declarations->nodes[0];
}

/** @brief 从模块查询结果定位发布的错误，供断言诊断身份及源码范围。
 * @note 返回值借用语义上下文的物化数组；重新物化或释放编译器后失效。
 */
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

/** @brief 验证结构或字段装饰器的拒绝结果能够贯穿编译器与语义查询。
 * @pre decoratorText 必须原样存在于 source，且 fieldDecorator 指向唯一待检装饰器。
 */
static void assert_invalid_struct_decorator(
        const TZrChar *source,
        const TZrChar *sourceName,
        const TZrChar *decoratorText,
        TZrBool fieldDecorator) {
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics diagnostics;
    const SZrStructuredDiagnostic *published;
    SZrCompilerState compiler;
    SZrAstNode *ast;
    SZrAstNode *structure;
    SZrAstNode *decorator;
    TZrSize decoratorStart;

    structure = parse_extern_struct(source, sourceName, &ast);
    TEST_ASSERT_NOT_NULL(structure);
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_DECLARATION, structure->type);
    if (fieldDecorator) {
        SZrAstNode *field;
        TEST_ASSERT_NOT_NULL(structure->data.structDeclaration.members);
        TEST_ASSERT_EQUAL_UINT64(
                1U, structure->data.structDeclaration.members->count);
        field = structure->data.structDeclaration.members->nodes[0];
        TEST_ASSERT_NOT_NULL(field);
        TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_FIELD, field->type);
        TEST_ASSERT_NOT_NULL(field->data.structField.decorators);
        TEST_ASSERT_EQUAL_UINT64(1U, field->data.structField.decorators->count);
        decorator = field->data.structField.decorators->nodes[0];
    } else {
        TEST_ASSERT_NOT_NULL(structure->data.structDeclaration.decorators);
        TEST_ASSERT_EQUAL_UINT64(
                1U, structure->data.structDeclaration.decorators->count);
        decorator = structure->data.structDeclaration.decorators->nodes[0];
    }
    TEST_ASSERT_NOT_NULL(decorator);
    decoratorStart = (TZrSize)(strstr(source, decoratorText) - source);
    TEST_ASSERT_EQUAL_UINT64(decoratorStart, decorator->location.start.offset);
    TEST_ASSERT_EQUAL_UINT64(
            decoratorStart + strlen(decoratorText), decorator->location.end.offset);

    memset(&compiler, 0, sizeof(compiler));
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    TEST_ASSERT_FALSE(
            ZrParser_Compiler_ValidateExternStructDecorators(
                    &compiler, structure));
    TEST_ASSERT_TRUE(compiler.hasError);
    TEST_ASSERT_TRUE(compiler.hasStructuredError);
    TEST_ASSERT_EQUAL_UINT32(2019U, compiler.structuredError.descriptorId);
    TEST_ASSERT_NOT_NULL(compiler.structuredError.code);
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

    /* 发布会深拷贝诊断事实；清除当前错误后仍须能从模块查询取得相同位置与修复约束。 */
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

/** @brief 以布局及字段编码的合法组合约束校验器不能误报。 */
static void test_valid_extern_struct_and_field_decorators_are_accepted(void) {
    const TZrChar *source =
            "native extern(\"fixture\") {\n"
            "    #zr.ffi.kind(\"union\")#\n"
            "    #zr.ffi.pack(8)#\n"
            "    #zr.ffi.align(16)#\n"
            "    struct Packet {\n"
            "        #zr.ffi.offset(0)#\n"
            "        #zr.ffi.charset(\"utf8\")#\n"
            "        var value: i32;\n"
            "    }\n"
            "}\n";
    SZrCompilerState compiler;
    SZrAstNode *ast;
    SZrAstNode *structure = parse_extern_struct(
            source, "valid_extern_struct_decorators.zr", &ast);

    memset(&compiler, 0, sizeof(compiler));
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidateExternStructDecorators(
            &compiler, structure));
    TEST_ASSERT_FALSE(compiler.hasError);
    TEST_ASSERT_FALSE(compiler.hasStructuredError);

    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_invalid_extern_struct_pack_shape_publishes_query_fact(void) {
    const TZrChar *source =
            "native extern(\"fixture\") {\n"
            "    #zr.ffi.pack(\"bad\")#\n"
            "    struct Packet { var value: i32; }\n"
            "}\n";
    assert_invalid_struct_decorator(
            source,
            "invalid_extern_struct_pack_shape.zr",
            "#zr.ffi.pack(\"bad\")#",
            ZR_FALSE);
}

static void test_invalid_extern_struct_alignment_value_publishes_query_fact(void) {
    const TZrChar *source =
            "native extern(\"fixture\") {\n"
            "    #zr.ffi.align(3)#\n"
            "    struct Packet { var value: i32; }\n"
            "}\n";
    assert_invalid_struct_decorator(
            source,
            "invalid_extern_struct_alignment_value.zr",
            "#zr.ffi.align(3)#",
            ZR_FALSE);
}

static void test_invalid_extern_field_offset_shape_publishes_query_fact(void) {
    const TZrChar *source =
            "native extern(\"fixture\") {\n"
            "    struct Packet {\n"
            "        #zr.ffi.offset(\"bad\")#\n"
            "        var value: i32;\n"
            "    }\n"
            "}\n";
    assert_invalid_struct_decorator(
            source,
            "invalid_extern_field_offset_shape.zr",
            "#zr.ffi.offset(\"bad\")#",
            ZR_TRUE);
}

static void test_invalid_extern_field_charset_value_publishes_query_fact(void) {
    const TZrChar *source =
            "native extern(\"fixture\") {\n"
            "    struct Packet {\n"
            "        #zr.ffi.charset(\"wide\")#\n"
            "        var value: i32;\n"
            "    }\n"
            "}\n";
    assert_invalid_struct_decorator(
            source,
            "invalid_extern_field_charset_value.zr",
            "#zr.ffi.charset(\"wide\")#",
            ZR_TRUE);
}

/* TODO: 此目标已在 tests/CMakeLists.txt 建立，但尚无 add_test 注册，
 * 当前不会作为 CTest 用例运行；需确认 CI 是否直接运行该目标，或补注册。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_valid_extern_struct_and_field_decorators_are_accepted);
    RUN_TEST(test_invalid_extern_struct_pack_shape_publishes_query_fact);
    RUN_TEST(test_invalid_extern_struct_alignment_value_publishes_query_fact);
    RUN_TEST(test_invalid_extern_field_offset_shape_publishes_query_fact);
    RUN_TEST(test_invalid_extern_field_charset_value_publishes_query_fact);
    return UNITY_END();
}
