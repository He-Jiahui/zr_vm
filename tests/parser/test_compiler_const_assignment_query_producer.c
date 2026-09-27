#include <string.h>

#include "unity.h"

#include "zr_vm_core/string.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/const_assignment.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_parser/semantic_query.h"

#include "harness/runtime_support.h"

/* Unity 每个用例独立创建运行时；AST 与编译器状态由测试另行释放。 */
static SZrState *g_state;

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    /* BUG: Unity 断言失败会跳过测试尾部的 Ast_Free/CompilerState_Free；
     * 这里只销毁运行时，无法完成局部原生资源的清理。 */
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

/** @brief 从测试脚本安全取得语句，供声明与赋值用例共用。
 * @note 返回节点借用自 script，直到 Ast_Free 前有效。
 */
static SZrAstNode *script_statement_at(SZrAstNode *script, TZrSize index) {
    if (script == ZR_NULL || script->type != ZR_AST_SCRIPT ||
        script->data.script.statements == ZR_NULL ||
        index >= script->data.script.statements->count) {
        return ZR_NULL;
    }
    return script->data.script.statements->nodes[index];
}

/** @brief 取得表达式语句的表达式，供赋值用例定位写入节点。
 * @note 返回表达式借用自 script；调用方仍须校验具体赋值类型。
 */
static SZrAstNode *assignment_at(SZrAstNode *script, TZrSize index) {
    SZrAstNode *statement = script_statement_at(script, index);

    if (statement == ZR_NULL || statement->type != ZR_AST_EXPRESSION_STATEMENT) {
        return ZR_NULL;
    }
    return statement->data.expressionStatement.expr;
}

/** @brief 验证同名符号记录指向不同声明时，发布器按引用的符号 ID 识别只读目标。
 * @note 此 API 的仓内生产调用在 LSP；常规编译经独立的诊断构建路径。
 */
static void test_publisher_resolves_const_target_by_symbol_id(void) {
    static TZrChar source[] =
            "let frozen: int = 1;\n"
            "frozen = 2;\n"
            "var mutableValue: int = 3;\n";
    SZrString *sourceName = ZrCore_String_Create(
            g_state,
            "compiler_const_assignment_query_producer_test.zr",
            strlen("compiler_const_assignment_query_producer_test.zr"));
    SZrAstNode *script = ZrParser_Parse(
            g_state, source, strlen(source), sourceName);
    SZrAstNode *constDeclaration = script_statement_at(script, 0U);
    SZrAstNode *assignment = assignment_at(script, 1U);
    SZrAstNode *mutableDeclaration = script_statement_at(script, 2U);
    SZrAstNode *constName;
    SZrAstNode *target;
    SZrCompilerState compiler;
    SZrSemanticReferenceFact reference;
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics diagnostics;
    TZrSymbolId targetSymbolId;

    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_NOT_NULL(constDeclaration);
    TEST_ASSERT_NOT_NULL(assignment);
    TEST_ASSERT_NOT_NULL(mutableDeclaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, constDeclaration->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_ASSIGNMENT_EXPRESSION, assignment->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, mutableDeclaration->type);
    constName = constDeclaration->data.variableDeclaration.pattern;
    target = assignment->data.assignmentExpression.left;
    TEST_ASSERT_NOT_NULL(constName);
    TEST_ASSERT_NOT_NULL(target);

    memset(&compiler, 0, sizeof(compiler));
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    compiler.scriptAst = script;

    /* 先登记同名可变符号，再让写入事实明确指向只读符号 ID。 */
    TEST_ASSERT_NOT_EQUAL(
            ZR_SEMANTIC_ID_INVALID,
            ZrParser_Semantic_RegisterSymbol(
                    compiler.semanticContext,
                    constName->data.identifier.name,
                    ZR_SEMANTIC_SYMBOL_KIND_VARIABLE,
                    ZR_SEMANTIC_ID_INVALID,
                    ZR_SEMANTIC_ID_INVALID,
                    mutableDeclaration,
                    mutableDeclaration->location));
    targetSymbolId = ZrParser_Semantic_RegisterSymbol(
            compiler.semanticContext,
            constName->data.identifier.name,
            ZR_SEMANTIC_SYMBOL_KIND_VARIABLE,
            ZR_SEMANTIC_ID_INVALID,
            ZR_SEMANTIC_ID_INVALID,
            constDeclaration,
            constName->location);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, targetSymbolId);

    memset(&reference, 0, sizeof(reference));
    reference.node = target;
    reference.range = target->location;
    reference.declarationRange = constName->location;
    reference.kind = ZR_SEMANTIC_REFERENCE_WRITE;
    reference.symbolId = targetSymbolId;
    reference.name = constName->data.identifier.name;
    reference.isResolved = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(
            compiler.semanticContext, &reference));

    /* 诊断事实由语义上下文持有；物化查询返回借用视图并保留赋值和声明范围。 */
    TEST_ASSERT_TRUE(ZrParser_ConstAssignment_PublishDiagnostic(
            &compiler, script, assignment));
    TEST_ASSERT_FALSE(compiler.hasError);

    ZrParser_SemanticQueryScope_Module(&scope);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_MaterializeDiagnostics(
            compiler.semanticContext, &scope));
    memset(&diagnostics, 0, sizeof(diagnostics));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_Diagnostics(
            compiler.semanticContext, &scope, &diagnostics));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)diagnostics.count);
    TEST_ASSERT_EQUAL_UINT32(2012U, diagnostics.items[0].descriptorId);
    TEST_ASSERT_EQUAL_STRING(
            "const_assignment",
            ZrCore_String_GetNativeString(diagnostics.items[0].code));
    TEST_ASSERT_EQUAL_UINT64(
            assignment->location.start.offset,
            diagnostics.items[0].location.start.offset);
    TEST_ASSERT_TRUE(diagnostics.items[0].relatedInformation.isValid);
    TEST_ASSERT_EQUAL_UINT32(
            1U, (TZrUInt32)diagnostics.items[0].relatedInformation.length);
    TEST_ASSERT_EQUAL_UINT64(
            constName->location.start.offset,
            ((const SZrStructuredDiagnosticRelatedInformation *)ZrCore_Array_Get(
                    (SZrArray *)&diagnostics.items[0].relatedInformation, 0U))
                    ->location.start.offset);
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION,
            diagnostics.items[0].noFixReason);

    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, script);
}

/** @brief 构造已解析但未注册目标 ID 的写入事实，检查发布失败且查询为空。 */
static void test_publisher_rejects_missing_symbol_record(void) {
    static TZrChar source[] =
            "let frozen: int = 1;\n"
            "frozen = 2;\n";
    SZrString *sourceName = ZrCore_String_Create(
            g_state,
            "compiler_const_assignment_missing_symbol_test.zr",
            strlen("compiler_const_assignment_missing_symbol_test.zr"));
    SZrAstNode *script = ZrParser_Parse(
            g_state, source, strlen(source), sourceName);
    SZrAstNode *assignment = assignment_at(script, 1U);
    SZrAstNode *target;
    SZrCompilerState compiler;
    SZrSemanticReferenceFact reference;
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics diagnostics;

    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_NOT_NULL(assignment);
    /* TODO: 这里仍需断言 ZR_AST_ASSIGNMENT_EXPRESSION；若解析形状回归，
     * 随后读取 assignmentExpression 联合体成员将失去类型前提。 */
    target = assignment->data.assignmentExpression.left;
    TEST_ASSERT_NOT_NULL(target);

    memset(&compiler, 0, sizeof(compiler));
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    compiler.scriptAst = script;

    memset(&reference, 0, sizeof(reference));
    reference.node = target;
    reference.range = target->location;
    reference.declarationRange = target->location;
    reference.kind = ZR_SEMANTIC_REFERENCE_WRITE;
    reference.symbolId = 999U;
    reference.name = target->data.identifier.name;
    reference.isResolved = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(
            compiler.semanticContext, &reference));

    /* TODO: 需先断言 SymbolAt 命中该引用且返回 ID 999；否则下面的 false
     * 也可能来自查询未命中，不能证明走到缺失符号记录的分支。 */
    TEST_ASSERT_FALSE(ZrParser_ConstAssignment_PublishDiagnostic(
            &compiler, script, assignment));
    ZrParser_SemanticQueryScope_Module(&scope);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_MaterializeDiagnostics(
            compiler.semanticContext, &scope));
    memset(&diagnostics, 0, sizeof(diagnostics));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_Diagnostics(
            compiler.semanticContext, &scope, &diagnostics));
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)diagnostics.count);

    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, script);
}

/* TODO: 此目标已在 tests/CMakeLists.txt 建立，但尚无 add_test 注册，
 * 当前不会作为 CTest 用例运行；需确认 CI 是否直接运行该目标，或补注册。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_publisher_resolves_const_target_by_symbol_id);
    RUN_TEST(test_publisher_rejects_missing_symbol_record);
    return UNITY_END();
}
