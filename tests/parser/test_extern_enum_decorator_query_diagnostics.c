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

/** @brief 为装饰器测试取得唯一 extern 枚举声明，避免其他编译阶段干扰校验结果。
 * @note 返回的声明借用自 *outAst；调用方必须在编译器状态释放后释放整棵 AST。
 */
static SZrAstNode *parse_extern_enum(
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

/** @brief 验证枚举或成员装饰器的拒绝结果能够贯穿编译器与语义查询。
 * @pre decoratorText 必须原样存在于 source，且 memberDecorator 指向唯一待检装饰器。
 */
static void assert_invalid_enum_decorator(
        const TZrChar *source,
        const TZrChar *sourceName,
        const TZrChar *decoratorText,
        TZrBool memberDecorator) {
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics diagnostics;
    const SZrStructuredDiagnostic *published;
    SZrCompilerState compiler;
    SZrAstNode *ast;
    SZrAstNode *enumeration;
    SZrAstNode *decorator;
    TZrSize decoratorStart;

    enumeration = parse_extern_enum(source, sourceName, &ast);
    TEST_ASSERT_NOT_NULL(enumeration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_ENUM_DECLARATION, enumeration->type);
    if (memberDecorator) {
        SZrAstNode *member;
        TEST_ASSERT_NOT_NULL(enumeration->data.enumDeclaration.members);
        TEST_ASSERT_EQUAL_UINT64(
                1U, enumeration->data.enumDeclaration.members->count);
        member = enumeration->data.enumDeclaration.members->nodes[0];
        TEST_ASSERT_NOT_NULL(member);
        TEST_ASSERT_EQUAL_INT(ZR_AST_ENUM_MEMBER, member->type);
        TEST_ASSERT_NOT_NULL(member->data.enumMember.decorators);
        TEST_ASSERT_EQUAL_UINT64(1U, member->data.enumMember.decorators->count);
        decorator = member->data.enumMember.decorators->nodes[0];
    } else {
        TEST_ASSERT_NOT_NULL(enumeration->data.enumDeclaration.decorators);
        TEST_ASSERT_EQUAL_UINT64(
                1U, enumeration->data.enumDeclaration.decorators->count);
        decorator = enumeration->data.enumDeclaration.decorators->nodes[0];
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
            ZrParser_Compiler_ValidateExternEnumDecorators(
                    &compiler, enumeration));
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

/** @brief 以合法的枚举底层类型和成员常量作反例，约束校验器不能误报。 */
static void test_valid_extern_enum_and_member_decorators_are_accepted(void) {
    const TZrChar *source =
            "native extern(\"fixture\") {\n"
            "    #zr.ffi.underlying(\"u32\")#\n"
            "    enum Mode {\n"
            "        #zr.ffi.value(7)# Active\n"
            "    }\n"
            "}\n";
    SZrCompilerState compiler;
    SZrAstNode *ast;
    SZrAstNode *enumeration = parse_extern_enum(
            source, "valid_extern_enum_decorators.zr", &ast);

    memset(&compiler, 0, sizeof(compiler));
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidateExternEnumDecorators(
            &compiler, enumeration));
    TEST_ASSERT_FALSE(compiler.hasError);
    TEST_ASSERT_FALSE(compiler.hasStructuredError);

    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_invalid_extern_enum_underlying_shape_publishes_query_fact(void) {
    const TZrChar *source =
            "native extern(\"fixture\") {\n"
            "    #zr.ffi.underlying(32)#\n"
            "    enum Mode { Active }\n"
            "}\n";
    assert_invalid_enum_decorator(
            source,
            "invalid_extern_enum_underlying_shape.zr",
            "#zr.ffi.underlying(32)#",
            ZR_FALSE);
}

static void test_invalid_extern_enum_underlying_value_publishes_query_fact(void) {
    const TZrChar *source =
            "native extern(\"fixture\") {\n"
            "    #zr.ffi.underlying(\"object\")#\n"
            "    enum Mode { Active }\n"
            "}\n";
    assert_invalid_enum_decorator(
            source,
            "invalid_extern_enum_underlying_value.zr",
            "#zr.ffi.underlying(\"object\")#",
            ZR_FALSE);
}

static void test_invalid_extern_enum_member_value_shape_publishes_query_fact(void) {
    const TZrChar *source =
            "native extern(\"fixture\") {\n"
            "    enum Mode {\n"
            "        #zr.ffi.value(\"bad\")# Active\n"
            "    }\n"
            "}\n";
    assert_invalid_enum_decorator(
            source,
            "invalid_extern_enum_member_value_shape.zr",
            "#zr.ffi.value(\"bad\")#",
            ZR_TRUE);
}

static void test_unknown_extern_enum_member_decorator_publishes_query_fact(void) {
    const TZrChar *source =
            "native extern(\"fixture\") {\n"
            "    enum Mode {\n"
            "        #zr.ffi.offset(0)# Active\n"
            "    }\n"
            "}\n";
    assert_invalid_enum_decorator(
            source,
            "unknown_extern_enum_member_decorator.zr",
            "#zr.ffi.offset(0)#",
            ZR_TRUE);
}

/* TODO: 此目标已在 tests/CMakeLists.txt 建立，但尚无 add_test 注册，
 * 当前不会作为 CTest 用例运行；需确认 CI 是否直接运行该目标，或补注册。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_valid_extern_enum_and_member_decorators_are_accepted);
    RUN_TEST(test_invalid_extern_enum_underlying_shape_publishes_query_fact);
    RUN_TEST(test_invalid_extern_enum_underlying_value_publishes_query_fact);
    RUN_TEST(test_invalid_extern_enum_member_value_shape_publishes_query_fact);
    RUN_TEST(test_unknown_extern_enum_member_decorator_publishes_query_fact);
    return UNITY_END();
}
