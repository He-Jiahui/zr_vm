#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/cfg.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_facts.h"

/* Unity 每例持有独立 VM；手工 AST、CFG 与语义事实只在该资源域内有效。 */
static SZrState *g_state;

/* 每个 try/catch 场景重新建状态，隔离 GC 对象和事实数组。 */
void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

/* 断言中止后仍由 Unity 调用；状态析构不替代局部原生结构的显式释放。 */
void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

/* 给 AST 和事实位置查询提供同一虚拟源文件，避免跨源同偏移命中。 */
static SZrFileRange test_range(TZrSize startOffset, TZrSize endOffset) {
    SZrFileRange range;

    range.start.offset = startOffset;
    range.start.line = 1;
    range.start.column = (TZrInt32)startOffset + 1;
    range.end.offset = endOffset;
    range.end.line = 1;
    range.end.column = (TZrInt32)endOffset + 1;
    range.source = ZrCore_String_Create(g_state, "cfg_try_catch_edges_test.zr", 27);
    return range;
}

/* 绕过解析器构造最小 AST；挂入脚本后由 Ast_Free 递归释放原生节点。 */
static SZrAstNode *test_node(EZrAstNodeType type, TZrSize startOffset, TZrSize endOffset) {
    SZrAstNode *node = (SZrAstNode *)ZrCore_Memory_RawMallocWithType(
            g_state->global,
            sizeof(SZrAstNode),
            ZR_MEMORY_NATIVE_TYPE_ARRAY);

    TEST_ASSERT_NOT_NULL(node);
    memset(node, 0, sizeof(*node));
    node->type = type;
    node->location = test_range(startOffset, endOffset);
    return node;
}

/* 把一个 try 语句挂为 CFG 根，独立观察异常边及 catch 可达性。 */
static SZrAstNode *script_with_statement(SZrAstNode *statement) {
    /* TODO: 三、四 catch 用例的 try 范围分别到 112、128，均超过脚本固定的 96；
     * 若查询父节点位置，先核对并统一父子范围。 */
    SZrAstNode *script = test_node(ZR_AST_SCRIPT, 0, 96);

    script->data.script.statements = ZrParser_AstNodeArray_New(g_state, 1);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    ZrParser_AstNodeArray_Add(g_state, script->data.script.statements, statement);
    return script;
}

/* 用单语句块固定 try/catch 体的观测位置，使事实落到内部语句。 */
static SZrAstNode *block_with_statement(SZrAstNode *statement,
                                        TZrSize startOffset,
                                        TZrSize endOffset) {
    SZrAstNode *block = test_node(ZR_AST_BLOCK, startOffset, endOffset);

    block->data.block.body = ZrParser_AstNodeArray_New(g_state, 1);
    TEST_ASSERT_NOT_NULL(block->data.block.body);
    block->data.block.isStatement = ZR_TRUE;
    ZrParser_AstNodeArray_Add(g_state, block->data.block.body, statement);
    return block;
}

/* 保留声明到 throw 的顺序，让抛出类型分析读取前面的局部绑定。 */
static SZrAstNode *block_with_two_statements(SZrAstNode *firstStatement,
                                             SZrAstNode *secondStatement,
                                             TZrSize startOffset,
                                             TZrSize endOffset) {
    SZrAstNode *block = test_node(ZR_AST_BLOCK, startOffset, endOffset);

    block->data.block.body = ZrParser_AstNodeArray_New(g_state, 2);
    TEST_ASSERT_NOT_NULL(block->data.block.body);
    block->data.block.isStatement = ZR_TRUE;
    ZrParser_AstNodeArray_Add(g_state, block->data.block.body, firstStatement);
    ZrParser_AstNodeArray_Add(g_state, block->data.block.body, secondStatement);
    return block;
}

/* 保留声明、赋值、throw 的次序，验证较新的绑定覆盖初始类型。 */
static SZrAstNode *block_with_three_statements(SZrAstNode *firstStatement,
                                               SZrAstNode *secondStatement,
                                               SZrAstNode *thirdStatement,
                                               TZrSize startOffset,
                                               TZrSize endOffset) {
    /* TODO: 此夹具未像一、二语句块置 isStatement；CFG 当前只遍历 body，复用到
     * 语义编译前需核对语句块标志契约。 */
    SZrAstNode *block = test_node(ZR_AST_BLOCK, startOffset, endOffset);

    block->data.block.body = ZrParser_AstNodeArray_New(g_state, 3);
    TEST_ASSERT_NOT_NULL(block->data.block.body);
    ZrParser_AstNodeArray_Add(g_state, block->data.block.body, firstStatement);
    ZrParser_AstNodeArray_Add(g_state, block->data.block.body, secondStatement);
    ZrParser_AstNodeArray_Add(g_state, block->data.block.body, thirdStatement);
    return block;
}

/* 单 catch 夹具区分无抛出、显式 throw 与可能抛出的调用。 */
static SZrAstNode *try_statement_with_catch(SZrAstNode *body, SZrAstNode *catchNode) {
    SZrAstNode *tryNode = test_node(ZR_AST_TRY_CATCH_FINALLY_STATEMENT, 0, 80);

    tryNode->data.tryCatchFinallyStatement.block = body;
    tryNode->data.tryCatchFinallyStatement.catchClauses =
            ZrParser_AstNodeArray_New(g_state, 1);
    TEST_ASSERT_NOT_NULL(tryNode->data.tryCatchFinallyStatement.catchClauses);
    ZrParser_AstNodeArray_Add(
            g_state,
            tryNode->data.tryCatchFinallyStatement.catchClauses,
            catchNode);
    return tryNode;
}

/* 两个 catch 保留原始顺序，用来验证 catch-all 或类型匹配对后继的遮蔽。 */
static SZrAstNode *try_statement_with_two_catches(SZrAstNode *body,
                                                  SZrAstNode *firstCatchNode,
                                                  SZrAstNode *secondCatchNode) {
    SZrAstNode *tryNode = test_node(ZR_AST_TRY_CATCH_FINALLY_STATEMENT, 0, 96);

    tryNode->data.tryCatchFinallyStatement.block = body;
    tryNode->data.tryCatchFinallyStatement.catchClauses =
            ZrParser_AstNodeArray_New(g_state, 2);
    TEST_ASSERT_NOT_NULL(tryNode->data.tryCatchFinallyStatement.catchClauses);
    ZrParser_AstNodeArray_Add(
            g_state,
            tryNode->data.tryCatchFinallyStatement.catchClauses,
            firstCatchNode);
    ZrParser_AstNodeArray_Add(
            g_state,
            tryNode->data.tryCatchFinallyStatement.catchClauses,
            secondCatchNode);
    return tryNode;
}

/* 三个 catch 让前两支按类型分流，最后一支观测剩余异常是否存在。 */
static SZrAstNode *try_statement_with_three_catches(SZrAstNode *body,
                                                    SZrAstNode *firstCatchNode,
                                                    SZrAstNode *secondCatchNode,
                                                    SZrAstNode *thirdCatchNode) {
    SZrAstNode *tryNode = test_node(ZR_AST_TRY_CATCH_FINALLY_STATEMENT, 0, 112);

    tryNode->data.tryCatchFinallyStatement.block = body;
    tryNode->data.tryCatchFinallyStatement.catchClauses =
            ZrParser_AstNodeArray_New(g_state, 3);
    TEST_ASSERT_NOT_NULL(tryNode->data.tryCatchFinallyStatement.catchClauses);
    ZrParser_AstNodeArray_Add(
            g_state,
            tryNode->data.tryCatchFinallyStatement.catchClauses,
            firstCatchNode);
    ZrParser_AstNodeArray_Add(
            g_state,
            tryNode->data.tryCatchFinallyStatement.catchClauses,
            secondCatchNode);
    ZrParser_AstNodeArray_Add(
            g_state,
            tryNode->data.tryCatchFinallyStatement.catchClauses,
            thirdCatchNode);
    return tryNode;
}

/* 四支 catch 同时观测不匹配、分别消费两种已知异常及最终 catch-all。 */
static SZrAstNode *try_statement_with_four_catches(SZrAstNode *body,
                                                   SZrAstNode *firstCatchNode,
                                                   SZrAstNode *secondCatchNode,
                                                   SZrAstNode *thirdCatchNode,
                                                   SZrAstNode *fourthCatchNode) {
    SZrAstNode *tryNode = test_node(ZR_AST_TRY_CATCH_FINALLY_STATEMENT, 0, 128);

    tryNode->data.tryCatchFinallyStatement.block = body;
    tryNode->data.tryCatchFinallyStatement.catchClauses =
            ZrParser_AstNodeArray_New(g_state, 4);
    TEST_ASSERT_NOT_NULL(tryNode->data.tryCatchFinallyStatement.catchClauses);
    ZrParser_AstNodeArray_Add(
            g_state,
            tryNode->data.tryCatchFinallyStatement.catchClauses,
            firstCatchNode);
    ZrParser_AstNodeArray_Add(
            g_state,
            tryNode->data.tryCatchFinallyStatement.catchClauses,
            secondCatchNode);
    ZrParser_AstNodeArray_Add(
            g_state,
            tryNode->data.tryCatchFinallyStatement.catchClauses,
            thirdCatchNode);
    ZrParser_AstNodeArray_Add(
            g_state,
            tryNode->data.tryCatchFinallyStatement.catchClauses,
            fourthCatchNode);
    return tryNode;
}

/* 空 pattern 表示 catch-all；参数型场景随后由 add_catch_parameter 补齐。 */
static SZrAstNode *catch_clause(SZrAstNode *body) {
    /* TODO: 多 catch 用例的后续 body 可到偏移 112，此节点范围仍固定为 40..72；
     * 目前只查内部语句，增加 catch 节点位置查询前需按子树位置修正。 */
    SZrAstNode *catchNode = test_node(ZR_AST_CATCH_CLAUSE, 40, 72);

    catchNode->data.catchClause.block = body;
    return catchNode;
}

/* 复用标识符节点表示 catch 参数、局部变量与未知条件，名称由当前 VM 持有。 */
static SZrAstNode *identifier_node(const char *name,
                                   TZrSize nameLength,
                                   TZrSize startOffset,
                                   TZrSize endOffset) {
    SZrAstNode *identifier = test_node(ZR_AST_IDENTIFIER_LITERAL, startOffset, endOffset);

    identifier->data.identifier.name =
            ZrCore_String_Create(g_state, (TZrChar *)name, nameLength);
    TEST_ASSERT_NOT_NULL(identifier->data.identifier.name);
    identifier->data.identifier.isMoveBinding = ZR_FALSE;
    return identifier;
}

/* 无 typeInfo 的单参数在 CFG 匹配中等同 catch-all；参数持有姓名标识符节点并负责析构。 */
static SZrAstNode *untyped_parameter(const char *name,
                                     TZrSize nameLength,
                                     TZrSize startOffset,
                                     TZrSize endOffset) {
    SZrAstNode *parameter = test_node(ZR_AST_PARAMETER, startOffset, endOffset);
    SZrAstNode *nameNode = identifier_node(name, nameLength, startOffset, endOffset);

    parameter->data.parameter.name = &nameNode->data.identifier;
    parameter->data.parameter.nameLocation = nameNode->location;
    parameter->data.parameter.typeInfo = ZR_NULL;
    parameter->data.parameter.defaultValue = ZR_NULL;
    parameter->data.parameter.isConst = ZR_FALSE;
    parameter->data.parameter.decorators = ZR_NULL;
    parameter->data.parameter.passingMode = ZR_PARAMETER_PASSING_MODE_VALUE;
    parameter->data.parameter.genericKind = ZR_GENERIC_PARAMETER_TYPE;
    parameter->data.parameter.variance = ZR_GENERIC_VARIANCE_NONE;
    parameter->data.parameter.genericTypeConstraints = ZR_NULL;
    parameter->data.parameter.genericRequiresClass = ZR_FALSE;
    parameter->data.parameter.genericRequiresStruct = ZR_FALSE;
    parameter->data.parameter.genericRequiresNew = ZR_FALSE;
    parameter->data.parameter.genericRequiresOwner = ZR_FALSE;
    parameter->data.parameter.genericRequiredOwnershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    return parameter;
}

/* 仅构造 CFG 可识别的简单类型名；原生 SZrType 转入参数、声明或转换 AST 并随其释放。 */
static SZrType *type_info_named(const char *typeName,
                                TZrSize typeNameLength,
                                TZrSize startOffset,
                                TZrSize endOffset) {
    SZrType *typeInfo = (SZrType *)ZrCore_Memory_RawMallocWithType(
            g_state->global,
            sizeof(SZrType),
            ZR_MEMORY_NATIVE_TYPE_ARRAY);

    TEST_ASSERT_NOT_NULL(typeInfo);
    memset(typeInfo, 0, sizeof(*typeInfo));
    typeInfo->name = identifier_node(typeName, typeNameLength, startOffset, endOffset);
    typeInfo->subType = ZR_NULL;
    typeInfo->dimensions = 0;
    typeInfo->ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    typeInfo->isDecoratorPseudoType = ZR_FALSE;
    typeInfo->isImplicitBuiltinType = ZR_FALSE;
    typeInfo->arrayFixedSize = 0;
    typeInfo->arrayMinSize = 0;
    typeInfo->arrayMaxSize = 0;
    typeInfo->hasArraySizeConstraint = ZR_FALSE;
    typeInfo->arraySizeExpression = ZR_NULL;
    return typeInfo;
}

/* 把单参数限定为具名异常类型，供已知 throw kind 的精确匹配。 */
static SZrAstNode *typed_parameter(const char *name,
                                   TZrSize nameLength,
                                   const char *typeName,
                                   TZrSize typeNameLength,
                                   TZrSize nameStartOffset,
                                   TZrSize nameEndOffset,
                                   TZrSize typeStartOffset,
                                   TZrSize typeEndOffset) {
    SZrAstNode *parameter =
            untyped_parameter(name, nameLength, nameStartOffset, nameEndOffset);

    parameter->data.parameter.typeInfo =
            type_info_named(typeName, typeNameLength, typeStartOffset, typeEndOffset);
    return parameter;
}

/* 在 try 体前段建立具名局部绑定，供后续 throw 标识符解析其已知类型。 */
static SZrAstNode *typed_variable_declaration(const char *name,
                                              TZrSize nameLength,
                                              const char *typeName,
                                              TZrSize typeNameLength,
                                              SZrAstNode *value,
                                              TZrSize nameStartOffset,
                                              TZrSize nameEndOffset,
                                              TZrSize typeStartOffset,
                                              TZrSize typeEndOffset,
                                              TZrSize startOffset,
                                              TZrSize endOffset) {
    SZrAstNode *declaration = test_node(ZR_AST_VARIABLE_DECLARATION,
                                        startOffset,
                                        endOffset);

    declaration->data.variableDeclaration.pattern =
            identifier_node(name, nameLength, nameStartOffset, nameEndOffset);
    declaration->data.variableDeclaration.typeInfo =
            type_info_named(typeName, typeNameLength, typeStartOffset, typeEndOffset);
    declaration->data.variableDeclaration.value = value;
    declaration->data.variableDeclaration.accessModifier = ZR_ACCESS_PRIVATE;
    declaration->data.variableDeclaration.isConst = ZR_FALSE;
    return declaration;
}

/* 将参数交给 catch pattern 持有；CFG 依据 typeInfo 判断是否吞掉后续异常。 */
static void add_catch_parameter(SZrAstNode *catchNode, SZrAstNode *parameter) {
    TEST_ASSERT_NOT_NULL(catchNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CATCH_CLAUSE, catchNode->type);

    catchNode->data.catchClause.pattern = ZrParser_AstNodeArray_New(g_state, 1);
    TEST_ASSERT_NOT_NULL(catchNode->data.catchClause.pattern);
    ZrParser_AstNodeArray_Add(g_state, catchNode->data.catchClause.pattern, parameter);
}

/* 提供已知整数 throw kind，隔离 catch 过滤与表达式求值。 */
static SZrAstNode *integer_literal(TZrInt64 value, TZrSize startOffset, TZrSize endOffset) {
    SZrAstNode *literal = test_node(ZR_AST_INTEGER_LITERAL, startOffset, endOffset);

    literal->data.integerLiteral.value = value;
    literal->data.integerLiteral.literal = ZR_NULL;
    return literal;
}

/* 提供已知字符串 throw kind；GC 字符串随测试状态存活。 */
static SZrAstNode *string_literal(const char *value,
                                  TZrSize valueLength,
                                  TZrSize startOffset,
                                  TZrSize endOffset) {
    SZrAstNode *literal = test_node(ZR_AST_STRING_LITERAL, startOffset, endOffset);

    literal->data.stringLiteral.value =
            ZrCore_String_Create(g_state, (TZrChar *)value, valueLength);
    TEST_ASSERT_NOT_NULL(literal->data.stringLiteral.value);
    literal->data.stringLiteral.literal = literal->data.stringLiteral.value;
    literal->data.stringLiteral.hasError = ZR_FALSE;
    return literal;
}

/* 显式目标类型为抛出值提供 CFG 可读的类型来源，测试不运行真实转换。 */
static SZrAstNode *type_cast_expression(SZrAstNode *expr,
                                        const char *typeName,
                                        TZrSize typeNameLength,
                                        TZrSize startOffset,
                                        TZrSize endOffset,
                                        TZrSize typeStartOffset,
                                        TZrSize typeEndOffset) {
    SZrAstNode *castExpr = test_node(ZR_AST_TYPE_CAST_EXPRESSION, startOffset, endOffset);

    castExpr->data.typeCastExpression.expression = expr;
    castExpr->data.typeCastExpression.targetType =
            type_info_named(typeName, typeNameLength, typeStartOffset, typeEndOffset);
    return castExpr;
}

/* 让 try 体中的赋值改变本地类型绑定，随后由 throw 读取最新绑定。 */
static SZrAstNode *assignment_expression(SZrAstNode *left,
                                         SZrAstNode *right,
                                         const TZrChar *op,
                                         TZrSize startOffset,
                                         TZrSize endOffset) {
    SZrAstNode *assignment = test_node(ZR_AST_ASSIGNMENT_EXPRESSION,
                                       startOffset,
                                       endOffset);

    assignment->data.assignmentExpression.left = left;
    assignment->data.assignmentExpression.right = right;
    assignment->data.assignmentExpression.op.op = op;
    return assignment;
}

/* 将赋值接入 try 语句序列，供异常源扫描按执行顺序遍历。 */
static SZrAstNode *expression_statement(SZrAstNode *expr,
                                        TZrSize startOffset,
                                        TZrSize endOffset) {
    SZrAstNode *statement = test_node(ZR_AST_EXPRESSION_STATEMENT,
                                      startOffset,
                                      endOffset);

    statement->data.expressionStatement.expr = expr;
    return statement;
}

/* 显式 throw 保留表达式种类，驱动 CFG 的已知异常类型画像。 */
static SZrAstNode *throw_statement_with_expr(SZrAstNode *expr,
                                             TZrSize startOffset,
                                             TZrSize endOffset) {
    SZrAstNode *throwStmt = test_node(ZR_AST_THROW_STATEMENT, startOffset, endOffset);

    throwStmt->data.throwStatement.expr = expr;
    return throwStmt;
}

/* 未知条件分出两条 throw 路径，验证类型画像合并而非只看首支。 */
static SZrAstNode *if_statement(SZrAstNode *condition,
                                SZrAstNode *thenBody,
                                SZrAstNode *elseBody,
                                TZrSize startOffset,
                                TZrSize endOffset) {
    SZrAstNode *ifNode = test_node(ZR_AST_IF_EXPRESSION, startOffset, endOffset);

    ifNode->data.ifExpression.condition = condition;
    ifNode->data.ifExpression.thenExpr = thenBody;
    ifNode->data.ifExpression.elseExpr = elseBody;
    ifNode->data.ifExpression.isStatement = ZR_TRUE;
    return ifNode;
}

/* 返回 context 中的借用事实；本组用例以 null 表示语句未被标为不可达。 */
static const SZrSemanticReachabilityFact *reachability_fact_at(
        SZrSemanticContext *context,
        SZrAstNode *node) {
    return ZrParser_SemanticFacts_FindReachabilityAtPosition(
            context,
            test_range(node->location.start.offset, node->location.start.offset));
}

/* try 体没有异常源时不应连到 catch；不可达事实的原因保持 unknown。 */
static void test_cfg_marks_catch_body_unreachable_when_try_has_no_throw(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *tryStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 12, 20);
    SZrAstNode *tryBody = block_with_statement(tryStmt, 10, 24);
    SZrAstNode *catchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 48, 56);
    SZrAstNode *catchBody = block_with_statement(catchStmt, 44, 60);
    SZrAstNode *catchNode = catch_clause(catchBody);
    SZrAstNode *tryNode = try_statement_with_catch(tryBody, catchNode);
    SZrAstNode *script = script_with_statement(tryNode);
    const SZrSemanticReachabilityFact *fact;

    /* BUG: 本文件各用例在取得原生 AST、CFG 或 context 后以 Unity 断言；失败时
     * longjmp 跳过下方三个 Free，tearDown 只销毁 VM，已申请的原生块会泄漏。
     * 需让失败路径也经过局部清理。 */
    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    fact = reachability_fact_at(context, catchStmt);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CAUSE_UNKNOWN, fact->cause);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* 显式 throw 允许进入 catch，即使未提供可区分的异常值。 */
static void test_cfg_keeps_catch_body_reachable_when_try_has_explicit_throw(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *throwStmt = test_node(ZR_AST_THROW_STATEMENT, 12, 20);
    SZrAstNode *tryBody = block_with_statement(throwStmt, 10, 24);
    SZrAstNode *catchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 48, 56);
    SZrAstNode *catchBody = block_with_statement(catchStmt, 44, 60);
    SZrAstNode *catchNode = catch_clause(catchBody);
    SZrAstNode *tryNode = try_statement_with_catch(tryBody, catchNode);
    SZrAstNode *script = script_with_statement(tryNode);

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));
    TEST_ASSERT_NULL(reachability_fact_at(context, catchStmt));

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* 未知函数调用被保守视为可能抛出，不能凭缺少显式 throw 删掉 catch。 */
static void test_cfg_keeps_catch_body_reachable_when_try_has_call_expression(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *callExpr = test_node(ZR_AST_FUNCTION_CALL, 12, 20);
    SZrAstNode *tryStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 12, 20);
    SZrAstNode *tryBody;
    SZrAstNode *catchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 48, 56);
    SZrAstNode *catchBody = block_with_statement(catchStmt, 44, 60);
    SZrAstNode *catchNode = catch_clause(catchBody);
    SZrAstNode *tryNode;
    SZrAstNode *script;

    tryStmt->data.expressionStatement.expr = callExpr;
    tryBody = block_with_statement(tryStmt, 10, 24);
    tryNode = try_statement_with_catch(tryBody, catchNode);
    script = script_with_statement(tryNode);

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));
    TEST_ASSERT_NULL(reachability_fact_at(context, catchStmt));

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* 首支无 pattern 吞掉所有异常，后继 catch 必须记为常量分支不可达。 */
static void test_cfg_marks_catch_after_catch_all_unreachable(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *throwStmt = test_node(ZR_AST_THROW_STATEMENT, 12, 20);
    SZrAstNode *tryBody = block_with_statement(throwStmt, 10, 24);
    SZrAstNode *firstCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 48, 56);
    SZrAstNode *firstCatchBody = block_with_statement(firstCatchStmt, 44, 60);
    SZrAstNode *firstCatchNode = catch_clause(firstCatchBody);
    SZrAstNode *secondCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 72, 80);
    SZrAstNode *secondCatchBody = block_with_statement(secondCatchStmt, 68, 84);
    SZrAstNode *secondCatchNode = catch_clause(secondCatchBody);
    SZrAstNode *tryNode =
            try_statement_with_two_catches(tryBody, firstCatchNode, secondCatchNode);
    SZrAstNode *script = script_with_statement(tryNode);
    const SZrSemanticReachabilityFact *fact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));
    TEST_ASSERT_NULL(reachability_fact_at(context, firstCatchStmt));

    fact = reachability_fact_at(context, secondCatchStmt);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* 未标类型的 catch 参数也应吞掉异常，防止后继错误保留异常边。 */
static void test_cfg_treats_untyped_catch_parameter_as_catch_all(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *throwStmt = test_node(ZR_AST_THROW_STATEMENT, 12, 20);
    SZrAstNode *tryBody = block_with_statement(throwStmt, 10, 24);
    SZrAstNode *firstCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 48, 56);
    SZrAstNode *firstCatchBody = block_with_statement(firstCatchStmt, 44, 60);
    SZrAstNode *firstCatchNode = catch_clause(firstCatchBody);
    SZrAstNode *secondCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 72, 80);
    SZrAstNode *secondCatchBody = block_with_statement(secondCatchStmt, 68, 84);
    SZrAstNode *secondCatchNode = catch_clause(secondCatchBody);
    SZrAstNode *tryNode;
    SZrAstNode *script;
    const SZrSemanticReachabilityFact *fact;

    add_catch_parameter(firstCatchNode, untyped_parameter("e", 1, 41, 42));
    tryNode = try_statement_with_two_catches(tryBody, firstCatchNode, secondCatchNode);
    script = script_with_statement(tryNode);

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));
    TEST_ASSERT_NULL(reachability_fact_at(context, firstCatchStmt));

    fact = reachability_fact_at(context, secondCatchStmt);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* 整数字面量不会进入 string catch；后继 catch 仍保留可达。 */
static void test_cfg_skips_typed_catch_when_literal_throw_type_mismatches(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *throwExpr = integer_literal(1, 18, 19);
    SZrAstNode *throwStmt = throw_statement_with_expr(throwExpr, 12, 20);
    SZrAstNode *tryBody = block_with_statement(throwStmt, 10, 24);
    SZrAstNode *firstCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 48, 56);
    SZrAstNode *firstCatchBody = block_with_statement(firstCatchStmt, 44, 60);
    SZrAstNode *firstCatchNode = catch_clause(firstCatchBody);
    SZrAstNode *secondCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 72, 80);
    SZrAstNode *secondCatchBody = block_with_statement(secondCatchStmt, 68, 84);
    SZrAstNode *secondCatchNode = catch_clause(secondCatchBody);
    SZrAstNode *tryNode;
    SZrAstNode *script;
    const SZrSemanticReachabilityFact *fact;

    add_catch_parameter(
            firstCatchNode,
            typed_parameter("e", 1, "string", 6, 41, 42, 44, 50));
    tryNode = try_statement_with_two_catches(tryBody, firstCatchNode, secondCatchNode);
    script = script_with_statement(tryNode);

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    fact = reachability_fact_at(context, firstCatchStmt);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);
    TEST_ASSERT_NULL(reachability_fact_at(context, secondCatchStmt));

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* 已知整数 throw 被 int catch 消费后，后继 catch 应被遮蔽。 */
static void test_cfg_treats_matching_typed_catch_as_consuming_literal_throw(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *throwExpr = integer_literal(1, 18, 19);
    SZrAstNode *throwStmt = throw_statement_with_expr(throwExpr, 12, 20);
    SZrAstNode *tryBody = block_with_statement(throwStmt, 10, 24);
    SZrAstNode *firstCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 48, 56);
    SZrAstNode *firstCatchBody = block_with_statement(firstCatchStmt, 44, 60);
    SZrAstNode *firstCatchNode = catch_clause(firstCatchBody);
    SZrAstNode *secondCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 72, 80);
    SZrAstNode *secondCatchBody = block_with_statement(secondCatchStmt, 68, 84);
    SZrAstNode *secondCatchNode = catch_clause(secondCatchBody);
    SZrAstNode *tryNode;
    SZrAstNode *script;
    const SZrSemanticReachabilityFact *fact;

    add_catch_parameter(
            firstCatchNode,
            typed_parameter("e", 1, "int", 3, 41, 42, 44, 47));
    tryNode = try_statement_with_two_catches(tryBody, firstCatchNode, secondCatchNode);
    script = script_with_statement(tryNode);

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));
    TEST_ASSERT_NULL(reachability_fact_at(context, firstCatchStmt));

    fact = reachability_fact_at(context, secondCatchStmt);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* 未知 if 条件产生整数和字符串两种异常；各自匹配的 catch 均须可达。 */
static void test_cfg_tracks_multiple_known_throw_types_across_typed_catches(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *condition = identifier_node("flag", 4, 12, 16);
    SZrAstNode *thenThrow = throw_statement_with_expr(integer_literal(1, 26, 27), 20, 28);
    SZrAstNode *elseThrow =
            throw_statement_with_expr(string_literal("x", 1, 42, 45), 36, 46);
    SZrAstNode *thenBody = block_with_statement(thenThrow, 18, 30);
    SZrAstNode *elseBody = block_with_statement(elseThrow, 34, 48);
    SZrAstNode *ifNode = if_statement(condition, thenBody, elseBody, 10, 50);
    SZrAstNode *tryBody = block_with_statement(ifNode, 8, 52);
    SZrAstNode *boolCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 64, 66);
    SZrAstNode *boolCatchBody = block_with_statement(boolCatchStmt, 60, 68);
    SZrAstNode *boolCatchNode = catch_clause(boolCatchBody);
    SZrAstNode *intCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 78, 80);
    SZrAstNode *intCatchBody = block_with_statement(intCatchStmt, 74, 82);
    SZrAstNode *intCatchNode = catch_clause(intCatchBody);
    SZrAstNode *stringCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 92, 94);
    SZrAstNode *stringCatchBody = block_with_statement(stringCatchStmt, 88, 96);
    SZrAstNode *stringCatchNode = catch_clause(stringCatchBody);
    SZrAstNode *catchAllStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 106, 108);
    SZrAstNode *catchAllBody = block_with_statement(catchAllStmt, 102, 110);
    SZrAstNode *catchAllNode = catch_clause(catchAllBody);
    SZrAstNode *tryNode;
    SZrAstNode *script;
    const SZrSemanticReachabilityFact *fact;

    add_catch_parameter(
            boolCatchNode,
            typed_parameter("e", 1, "bool", 4, 54, 55, 57, 61));
    add_catch_parameter(
            intCatchNode,
            typed_parameter("e", 1, "int", 3, 70, 71, 73, 76));
    add_catch_parameter(
            stringCatchNode,
            typed_parameter("e", 1, "string", 6, 84, 85, 87, 93));
    tryNode = try_statement_with_four_catches(
            tryBody,
            boolCatchNode,
            intCatchNode,
            stringCatchNode,
            catchAllNode);
    script = script_with_statement(tryNode);

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    /* bool 不匹配；int 与 string 各消费一种已知异常，最终 catch-all 没有剩余类型。 */
    fact = reachability_fact_at(context, boolCatchStmt);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);
    TEST_ASSERT_NULL(reachability_fact_at(context, intCatchStmt));
    TEST_ASSERT_NULL(reachability_fact_at(context, stringCatchStmt));

    fact = reachability_fact_at(context, catchAllStmt);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* 抛出显式 string 转换时，仅 string catch 保留；此例不验证转换执行。 */
static void test_cfg_uses_explicit_cast_throw_type_for_typed_catch_matching(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *valueExpr = identifier_node("value", 5, 18, 23);
    SZrAstNode *throwExpr =
            type_cast_expression(valueExpr, "string", 6, 17, 33, 27, 33);
    SZrAstNode *throwStmt = throw_statement_with_expr(throwExpr, 12, 34);
    SZrAstNode *tryBody = block_with_statement(throwStmt, 10, 36);
    SZrAstNode *intCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 52, 54);
    SZrAstNode *intCatchBody = block_with_statement(intCatchStmt, 48, 56);
    SZrAstNode *intCatchNode = catch_clause(intCatchBody);
    SZrAstNode *stringCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 72, 74);
    SZrAstNode *stringCatchBody = block_with_statement(stringCatchStmt, 68, 76);
    SZrAstNode *stringCatchNode = catch_clause(stringCatchBody);
    SZrAstNode *catchAllStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 92, 94);
    SZrAstNode *catchAllBody = block_with_statement(catchAllStmt, 88, 96);
    SZrAstNode *catchAllNode = catch_clause(catchAllBody);
    SZrAstNode *tryNode;
    SZrAstNode *script;
    const SZrSemanticReachabilityFact *fact;

    add_catch_parameter(
            intCatchNode,
            typed_parameter("e", 1, "int", 3, 40, 41, 43, 46));
    add_catch_parameter(
            stringCatchNode,
            typed_parameter("e", 1, "string", 6, 60, 61, 63, 69));
    tryNode = try_statement_with_three_catches(
            tryBody,
            intCatchNode,
            stringCatchNode,
            catchAllNode);
    script = script_with_statement(tryNode);

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    fact = reachability_fact_at(context, intCatchStmt);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);
    TEST_ASSERT_NULL(reachability_fact_at(context, stringCatchStmt));

    fact = reachability_fact_at(context, catchAllStmt);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* throw 局部标识符时沿前面的 string 声明取类型，排除 int catch。 */
static void test_cfg_uses_local_variable_type_for_typed_catch_matching(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *declaration = typed_variable_declaration(
            "value",
            5,
            "string",
            6,
            string_literal("x", 1, 27, 30),
            14,
            19,
            21,
            27,
            10,
            31);
    SZrAstNode *throwStmt =
            throw_statement_with_expr(identifier_node("value", 5, 38, 43), 32, 44);
    SZrAstNode *tryBody = block_with_two_statements(declaration, throwStmt, 8, 46);
    SZrAstNode *intCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 60, 62);
    SZrAstNode *intCatchBody = block_with_statement(intCatchStmt, 56, 64);
    SZrAstNode *intCatchNode = catch_clause(intCatchBody);
    SZrAstNode *stringCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 80, 82);
    SZrAstNode *stringCatchBody = block_with_statement(stringCatchStmt, 76, 84);
    SZrAstNode *stringCatchNode = catch_clause(stringCatchBody);
    SZrAstNode *catchAllStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 100, 102);
    SZrAstNode *catchAllBody = block_with_statement(catchAllStmt, 96, 104);
    SZrAstNode *catchAllNode = catch_clause(catchAllBody);
    SZrAstNode *tryNode;
    SZrAstNode *script;
    const SZrSemanticReachabilityFact *fact;

    add_catch_parameter(
            intCatchNode,
            typed_parameter("e", 1, "int", 3, 48, 49, 51, 54));
    add_catch_parameter(
            stringCatchNode,
            typed_parameter("e", 1, "string", 6, 68, 69, 71, 77));
    tryNode = try_statement_with_three_catches(
            tryBody,
            intCatchNode,
            stringCatchNode,
            catchAllNode);
    script = script_with_statement(tryNode);

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    fact = reachability_fact_at(context, intCatchStmt);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);
    TEST_ASSERT_NULL(reachability_fact_at(context, stringCatchStmt));

    fact = reachability_fact_at(context, catchAllStmt);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* throw 局部变量时以最近赋值的 string 类型覆盖最初的 int 声明。 */
static void test_cfg_uses_latest_assignment_type_for_typed_catch_matching(void) {
    /* TODO: 此夹具让 int 声明接收 string 值，未运行语义类型检查；把它当成
     * 源语言有效路径前，需用解析与语义集成测试确认该赋值是否合法。 */
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *declaration = typed_variable_declaration(
            "value",
            5,
            "int",
            3,
            integer_literal(1, 23, 24),
            14,
            19,
            21,
            24,
            10,
            25);
    SZrAstNode *assignment = expression_statement(
            assignment_expression(
                    identifier_node("value", 5, 28, 33),
                    string_literal("x", 1, 36, 39),
                    "=",
                    28,
                    39),
            28,
            40);
    SZrAstNode *throwStmt =
            throw_statement_with_expr(identifier_node("value", 5, 47, 52), 41, 53);
    SZrAstNode *tryBody =
            block_with_three_statements(declaration, assignment, throwStmt, 8, 55);
    SZrAstNode *intCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 68, 70);
    SZrAstNode *intCatchBody = block_with_statement(intCatchStmt, 64, 72);
    SZrAstNode *intCatchNode = catch_clause(intCatchBody);
    SZrAstNode *stringCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 88, 90);
    SZrAstNode *stringCatchBody = block_with_statement(stringCatchStmt, 84, 92);
    SZrAstNode *stringCatchNode = catch_clause(stringCatchBody);
    SZrAstNode *catchAllStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 108, 110);
    SZrAstNode *catchAllBody = block_with_statement(catchAllStmt, 104, 112);
    SZrAstNode *catchAllNode = catch_clause(catchAllBody);
    SZrAstNode *tryNode;
    SZrAstNode *script;
    const SZrSemanticReachabilityFact *fact;

    add_catch_parameter(
            intCatchNode,
            typed_parameter("e", 1, "int", 3, 56, 57, 59, 62));
    add_catch_parameter(
            stringCatchNode,
            typed_parameter("e", 1, "string", 6, 76, 77, 79, 85));
    tryNode = try_statement_with_three_catches(
            tryBody,
            intCatchNode,
            stringCatchNode,
            catchAllNode);
    script = script_with_statement(tryNode);

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    fact = reachability_fact_at(context, intCatchStmt);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);
    TEST_ASSERT_NULL(reachability_fact_at(context, stringCatchStmt));

    fact = reachability_fact_at(context, catchAllStmt);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* Unity 注册全部异常边回归场景，CMake 将独立目标纳入语言流水线。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_cfg_marks_catch_body_unreachable_when_try_has_no_throw);
    RUN_TEST(test_cfg_keeps_catch_body_reachable_when_try_has_explicit_throw);
    RUN_TEST(test_cfg_keeps_catch_body_reachable_when_try_has_call_expression);
    RUN_TEST(test_cfg_marks_catch_after_catch_all_unreachable);
    RUN_TEST(test_cfg_treats_untyped_catch_parameter_as_catch_all);
    RUN_TEST(test_cfg_skips_typed_catch_when_literal_throw_type_mismatches);
    RUN_TEST(test_cfg_treats_matching_typed_catch_as_consuming_literal_throw);
    RUN_TEST(test_cfg_tracks_multiple_known_throw_types_across_typed_catches);
    RUN_TEST(test_cfg_uses_explicit_cast_throw_type_for_typed_catch_matching);
    RUN_TEST(test_cfg_uses_local_variable_type_for_typed_catch_matching);
    RUN_TEST(test_cfg_uses_latest_assignment_type_for_typed_catch_matching);
    return UNITY_END();
}
