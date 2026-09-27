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

/* Unity 串行运行每个场景；辅助构造器借用当前场景的 VM 状态。 */
static SZrState *g_state;

/* Unity 在每个 RUN_TEST 前调用，提供 AST 和语义事实共用的分配域。 */
void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

/* Unity 在测试成功或断言跳出后调用；测试体须自行释放原生 CFG/AST/语义上下文。 */
void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

/* 各构造器复用同一虚拟源名，使位置查询能够按源与偏移命中不可达事实。 */
static SZrFileRange test_range(TZrSize startOffset, TZrSize endOffset) {
    SZrFileRange range;

    range.start.offset = startOffset;
    range.start.line = 1;
    range.start.column = (TZrInt32)startOffset + 1;
    range.end.offset = endOffset;
    range.end.line = 1;
    range.end.column = (TZrInt32)endOffset + 1;
    range.source =
            ZrCore_String_Create(g_state, "cfg_typed_catch_switch_flow_test.zr", 35);
    return range;
}

/* 在测试状态的原生分配器中建立 AST 节点，最终由脚本根节点递归释放。 */
static SZrAstNode *test_node(EZrAstNodeType type,
                             TZrSize startOffset,
                             TZrSize endOffset) {
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

/* 将调用方传入的节点所有权交给语句块，以模拟解析器产出的语句序列。 */
static SZrAstNode *block_with_nodes(SZrAstNode **nodes,
                                    TZrSize count,
                                    TZrSize startOffset,
                                    TZrSize endOffset) {
    SZrAstNode *block = test_node(ZR_AST_BLOCK, startOffset, endOffset);
    TZrSize index;

    block->data.block.body = ZrParser_AstNodeArray_New(g_state, count);
    TEST_ASSERT_NOT_NULL(block->data.block.body);
    block->data.block.isStatement = ZR_TRUE;
    for (index = 0; index < count; index++) {
        ZrParser_AstNodeArray_Add(g_state, block->data.block.body, nodes[index]);
    }
    return block;
}

/* CFG_Build 需要脚本根节点；脚本接管唯一顶层语句并作为释放入口。 */
/* TODO: 第四、五场景的子节点偏移可达 242，而此处固定到 220；需核查
 * parser 的位置查询是否要求父范围包含所有子节点。 */
static SZrAstNode *script_with_statement(SZrAstNode *statement) {
    SZrAstNode *script = test_node(ZR_AST_SCRIPT, 0, 220);

    script->data.script.statements = ZrParser_AstNodeArray_New(g_state, 1);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    ZrParser_AstNodeArray_Add(g_state, script->data.script.statements, statement);
    return script;
}

/* 保持 catch 顺序，供 CFG 的类型分派与后备 catch 可达性检查。 */
/* TODO: 与 script 一样固定结束偏移 220，需核查后续场景的嵌套范围约束。 */
static SZrAstNode *try_statement_with_catches(SZrAstNode *body,
                                              SZrAstNode **catchNodes,
                                              TZrSize catchCount) {
    SZrAstNode *tryNode = test_node(ZR_AST_TRY_CATCH_FINALLY_STATEMENT, 0, 220);
    TZrSize index;

    tryNode->data.tryCatchFinallyStatement.block = body;
    tryNode->data.tryCatchFinallyStatement.catchClauses =
            ZrParser_AstNodeArray_New(g_state, catchCount);
    TEST_ASSERT_NOT_NULL(tryNode->data.tryCatchFinallyStatement.catchClauses);
    for (index = 0; index < catchCount; index++) {
        ZrParser_AstNodeArray_Add(g_state,
                                  tryNode->data.tryCatchFinallyStatement.catchClauses,
                                  catchNodes[index]);
    }
    return tryNode;
}

/* catch 包住独立语句块，参数模式稍后由 add_catch_parameter 填入。 */
/* TODO: 固定 100..210 会小于后两个场景的 catch 子节点终点；需核查
 * 语义范围查询是否依赖 catch 范围包含其参数与块。 */
static SZrAstNode *catch_clause(SZrAstNode *body) {
    SZrAstNode *catchNode = test_node(ZR_AST_CATCH_CLAUSE, 100, 210);

    catchNode->data.catchClause.block = body;
    return catchNode;
}

/* 名称 AST 节点交父树释放；字符串随 VM/GC 状态有效，调用方须提供准确字节长度。 */
static SZrAstNode *identifier_node(const char *name,
                                   TZrSize nameLength,
                                   TZrSize startOffset,
                                   TZrSize endOffset) {
    SZrAstNode *identifier = test_node(ZR_AST_IDENTIFIER_LITERAL,
                                       startOffset,
                                       endOffset);

    identifier->data.identifier.name =
            ZrCore_String_Create(g_state, (TZrChar *)name, nameLength);
    TEST_ASSERT_NOT_NULL(identifier->data.identifier.name);
    identifier->data.identifier.isMoveBinding = ZR_FALSE;
    return identifier;
}

/* 为声明和 catch 参数构造具名类型；拥有的名称由 AST 析构链回收。 */
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
    typeInfo->ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    return typeInfo;
}

/* catch 模式使用值传递参数；参数节点接管名称及类型，随后交给 catch。 */
static SZrAstNode *typed_parameter(const char *name,
                                   TZrSize nameLength,
                                   const char *typeName,
                                   TZrSize typeNameLength,
                                   TZrSize nameStartOffset,
                                   TZrSize nameEndOffset,
                                   TZrSize typeStartOffset,
                                   TZrSize typeEndOffset) {
    SZrAstNode *parameter =
            test_node(ZR_AST_PARAMETER, nameStartOffset, typeEndOffset);
    SZrAstNode *nameNode =
            identifier_node(name, nameLength, nameStartOffset, nameEndOffset);

    parameter->data.parameter.name = &nameNode->data.identifier;
    parameter->data.parameter.nameLocation = nameNode->location;
    parameter->data.parameter.passingMode = ZR_PARAMETER_PASSING_MODE_VALUE;
    parameter->data.parameter.genericKind = ZR_GENERIC_PARAMETER_TYPE;
    parameter->data.parameter.genericTypeConstraints = ZR_NULL;
    parameter->data.parameter.typeInfo =
            type_info_named(typeName, typeNameLength, typeStartOffset, typeEndOffset);
    return parameter;
}

/* 把单个类型模式交给 catch，以区分具名类型分支与无参数兜底分支。 */
static void add_catch_parameter(SZrAstNode *catchNode, SZrAstNode *parameter) {
    catchNode->data.catchClause.pattern = ZrParser_AstNodeArray_New(g_state, 1);
    TEST_ASSERT_NOT_NULL(catchNode->data.catchClause.pattern);
    ZrParser_AstNodeArray_Add(g_state, catchNode->data.catchClause.pattern, parameter);
}

static SZrAstNode *integer_literal(TZrInt64 value,
                                   TZrSize startOffset,
                                   TZrSize endOffset) {
    SZrAstNode *literal = test_node(ZR_AST_INTEGER_LITERAL, startOffset, endOffset);

    literal->data.integerLiteral.value = value;
    return literal;
}

/* 字符串字面量同时设置语义值和词法值，供常量分支与 throw 类型推断共用。 */
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

static SZrAstNode *char_literal(TZrChar value,
                                TZrSize startOffset,
                                TZrSize endOffset) {
    SZrAstNode *literal = test_node(ZR_AST_CHAR_LITERAL, startOffset, endOffset);

    literal->data.charLiteral.value = value;
    literal->data.charLiteral.hasError = ZR_FALSE;
    return literal;
}

static SZrAstNode *boolean_literal(TZrBool value,
                                   TZrSize startOffset,
                                   TZrSize endOffset) {
    SZrAstNode *literal = test_node(ZR_AST_BOOLEAN_LITERAL, startOffset, endOffset);

    literal->data.booleanLiteral.value = value;
    return literal;
}

/* 直接拼 AST 绕开语义类型检查：五个场景先声明 value: int，再让 switch 分支写入
 * string/char，单独测 CFG 对后续 throw 的路径相关类型推断。
 * 此辅助函数的固定名称/类型偏移只适用于本文件调用时的 value/int 长度。 */
static SZrAstNode *typed_variable_declaration(const char *name,
                                              TZrSize nameLength,
                                              const char *typeName,
                                              TZrSize typeNameLength,
                                              SZrAstNode *value,
                                              TZrSize startOffset,
                                              TZrSize endOffset) {
    SZrAstNode *declaration = test_node(ZR_AST_VARIABLE_DECLARATION,
                                        startOffset,
                                        endOffset);

    declaration->data.variableDeclaration.pattern =
            identifier_node(name, nameLength, startOffset + 4, startOffset + 9);
    declaration->data.variableDeclaration.typeInfo =
            type_info_named(typeName, typeNameLength, startOffset + 11, startOffset + 14);
    declaration->data.variableDeclaration.value = value;
    declaration->data.variableDeclaration.accessModifier = ZR_ACCESS_PRIVATE;
    declaration->data.variableDeclaration.isConst = ZR_FALSE;
    return declaration;
}

/* 同一标识符的赋值用于检查 CFG 只合并实际可到达的 switch 出口绑定。 */
static SZrAstNode *assignment_expression(SZrAstNode *left,
                                         SZrAstNode *right,
                                         TZrSize startOffset,
                                         TZrSize endOffset) {
    SZrAstNode *assignment = test_node(ZR_AST_ASSIGNMENT_EXPRESSION,
                                       startOffset,
                                       endOffset);

    assignment->data.assignmentExpression.left = left;
    assignment->data.assignmentExpression.right = right;
    assignment->data.assignmentExpression.op.op = "=";
    return assignment;
}

static SZrAstNode *expression_statement(SZrAstNode *expr,
                                        TZrSize startOffset,
                                        TZrSize endOffset) {
    SZrAstNode *statement = test_node(ZR_AST_EXPRESSION_STATEMENT,
                                      startOffset,
                                      endOffset);

    statement->data.expressionStatement.expr = expr;
    return statement;
}

static SZrAstNode *throw_statement_with_expr(SZrAstNode *expr,
                                             TZrSize startOffset,
                                             TZrSize endOffset) {
    SZrAstNode *throwStmt = test_node(ZR_AST_THROW_STATEMENT, startOffset, endOffset);

    throwStmt->data.throwStatement.expr = expr;
    return throwStmt;
}

/* return 使该 case 无法流入 switch 后的 throw，类型合并不应采用其赋值。 */
static SZrAstNode *return_statement(TZrSize startOffset, TZrSize endOffset) {
    return test_node(ZR_AST_RETURN_STATEMENT, startOffset, endOffset);
}

/* 恒真分支内的 return 与恒假分支的赋值共同验证分支退出路径裁剪。 */
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

static SZrAstNode *switch_case_node(SZrAstNode *value,
                                    SZrAstNode *body,
                                    TZrSize startOffset,
                                    TZrSize endOffset) {
    SZrAstNode *caseNode = test_node(ZR_AST_SWITCH_CASE, startOffset, endOffset);

    caseNode->data.switchCase.value = value;
    caseNode->data.switchCase.block = body;
    return caseNode;
}

static SZrAstNode *switch_default_node(SZrAstNode *body,
                                       TZrSize startOffset,
                                       TZrSize endOffset) {
    SZrAstNode *defaultNode =
            test_node(ZR_AST_SWITCH_DEFAULT, startOffset, endOffset);

    defaultNode->data.switchDefault.block = body;
    return defaultNode;
}

/* 保留 case/default 两种出口，供非恒定 selector 场景合并后续 throw 绑定。 */
static SZrAstNode *switch_statement_with_case_and_default(SZrAstNode *expr,
                                                          SZrAstNode *caseNode,
                                                          SZrAstNode *defaultNode,
                                                          TZrSize startOffset,
                                                          TZrSize endOffset) {
    SZrAstNode *switchNode =
            test_node(ZR_AST_SWITCH_EXPRESSION, startOffset, endOffset);

    switchNode->data.switchExpression.expr = expr;
    switchNode->data.switchExpression.cases = ZrParser_AstNodeArray_New(g_state, 1);
    TEST_ASSERT_NOT_NULL(switchNode->data.switchExpression.cases);
    ZrParser_AstNodeArray_Add(g_state,
                              switchNode->data.switchExpression.cases,
                              caseNode);
    switchNode->data.switchExpression.defaultCase = defaultNode;
    switchNode->data.switchExpression.isStatement = ZR_TRUE;
    return switchNode;
}

/* 保留 case 顺序，验证不同常量种类的前置 case 不遮蔽后续匹配 case。 */
static SZrAstNode *switch_statement_with_cases_and_default(SZrAstNode *expr,
                                                           SZrAstNode **caseNodes,
                                                           TZrSize caseCount,
                                                           SZrAstNode *defaultNode,
                                                           TZrSize startOffset,
                                                           TZrSize endOffset) {
    SZrAstNode *switchNode =
            test_node(ZR_AST_SWITCH_EXPRESSION, startOffset, endOffset);
    TZrSize index;

    switchNode->data.switchExpression.expr = expr;
    switchNode->data.switchExpression.cases =
            ZrParser_AstNodeArray_New(g_state, caseCount);
    TEST_ASSERT_NOT_NULL(switchNode->data.switchExpression.cases);
    for (index = 0; index < caseCount; index++) {
        ZrParser_AstNodeArray_Add(g_state,
                                  switchNode->data.switchExpression.cases,
                                  caseNodes[index]);
    }
    switchNode->data.switchExpression.defaultCase = defaultNode;
    switchNode->data.switchExpression.isStatement = ZR_TRUE;
    return switchNode;
}

/* 仅查询当前语义上下文中的不可达事实；null 表示该位置未发射不可达记录。 */
static const SZrSemanticReachabilityFact *reachability_fact_at(
        SZrSemanticContext *context,
        SZrAstNode *node) {
    return ZrParser_SemanticFacts_FindReachabilityAtPosition(
            context,
            test_range(node->location.start.offset, node->location.start.offset));
}

/* BUG: 以下五个测试在建立 CFG、AST 与语义上下文后使用 Unity 断言；断言失败会
 * longjmp 到 Unity 运行器，跳过测试体末尾的三个 Free，而 tearDown 只释放 VM 状态。
 * 原生 AST/CFG/上下文分配无法沿正常析构链回收；核查 UnityFail 与各测试的清理尾部。 */
/* 非恒定 selector 下，case 和 default 都能到达 throw，catch 须覆盖 string 与 char。 */
static void
test_cfg_merges_switch_case_and_default_assignments_for_typed_catch_matching(
        void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *tryNodes[3];
    SZrAstNode *caseNodes[1];
    SZrAstNode *defaultNodes[1];
    SZrAstNode *catchNodes[5];
    SZrAstNode *declaration = typed_variable_declaration(
            "value",
            5,
            "int",
            3,
            integer_literal(1, 18, 19),
            8,
            20);
    SZrAstNode *caseAssignment = expression_statement(
            assignment_expression(identifier_node("value", 5, 44, 49),
                                  string_literal("x", 1, 52, 55),
                                  44,
                                  55),
            44,
            56);
    SZrAstNode *defaultAssignment = expression_statement(
            assignment_expression(identifier_node("value", 5, 76, 81),
                                  char_literal('c', 84, 87),
                                  76,
                                  87),
            76,
            88);
    SZrAstNode *switchNode;
    SZrAstNode *throwStmt;
    SZrAstNode *tryBody;
    SZrAstNode *tryNode;
    SZrAstNode *script;
    SZrAstNode *boolCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 122, 124);
    SZrAstNode *intCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 140, 142);
    SZrAstNode *stringCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 162, 164);
    SZrAstNode *charCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 182, 184);
    SZrAstNode *catchAllStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 196, 198);
    const SZrSemanticReachabilityFact *fact;

    caseNodes[0] = caseAssignment;
    defaultNodes[0] = defaultAssignment;
    switchNode = switch_statement_with_case_and_default(
            identifier_node("tag", 3, 29, 32),
            switch_case_node(integer_literal(1, 38, 39),
                             block_with_nodes(caseNodes, 1, 42, 58),
                             34,
                             60),
            switch_default_node(block_with_nodes(defaultNodes, 1, 74, 90),
                                66,
                                92),
            21,
            94);
    throwStmt =
            throw_statement_with_expr(identifier_node("value", 5, 102, 107),
                                      96,
                                      108);

    tryNodes[0] = declaration;
    tryNodes[1] = switchNode;
    tryNodes[2] = throwStmt;
    tryBody = block_with_nodes(tryNodes, 3, 6, 110);

    catchNodes[0] = catch_clause(block_with_nodes(&boolCatchStmt, 1, 118, 126));
    catchNodes[1] = catch_clause(block_with_nodes(&intCatchStmt, 1, 136, 144));
    catchNodes[2] = catch_clause(block_with_nodes(&stringCatchStmt, 1, 158, 166));
    catchNodes[3] = catch_clause(block_with_nodes(&charCatchStmt, 1, 178, 186));
    catchNodes[4] = catch_clause(block_with_nodes(&catchAllStmt, 1, 192, 200));

    add_catch_parameter(catchNodes[0],
                        typed_parameter("e", 1, "bool", 4, 112, 113, 115, 119));
    add_catch_parameter(catchNodes[1],
                        typed_parameter("e", 1, "int", 3, 130, 131, 133, 136));
    add_catch_parameter(catchNodes[2],
                        typed_parameter("e", 1, "string", 6, 148, 149, 151, 157));
    add_catch_parameter(catchNodes[3],
                        typed_parameter("e", 1, "char", 4, 170, 171, 173, 177));

    tryNode = try_statement_with_catches(tryBody, catchNodes, 5);
    script = script_with_statement(tryNode);

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    fact = reachability_fact_at(context, boolCatchStmt);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);

    fact = reachability_fact_at(context, intCatchStmt);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);

    TEST_ASSERT_NULL(reachability_fact_at(context, stringCatchStmt));
    TEST_ASSERT_NULL(reachability_fact_at(context, charCatchStmt));

    fact = reachability_fact_at(context, catchAllStmt);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* 恒定 selector 只选择匹配 case；default 产生的 char 不应进入后续 catch 集。 */
/* BUG: 断言失败时 Unity longjmp 跳过末尾原生资源释放；tearDown 仅销毁 VM 状态。 */
static void
test_cfg_uses_matching_switch_case_assignment_for_constant_selector_typed_catch_matching(
        void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *tryNodes[3];
    SZrAstNode *caseNodes[1];
    SZrAstNode *defaultNodes[1];
    SZrAstNode *catchNodes[5];
    SZrAstNode *declaration = typed_variable_declaration(
            "value",
            5,
            "int",
            3,
            integer_literal(1, 18, 19),
            8,
            20);
    SZrAstNode *caseAssignment = expression_statement(
            assignment_expression(identifier_node("value", 5, 44, 49),
                                  string_literal("x", 1, 52, 55),
                                  44,
                                  55),
            44,
            56);
    SZrAstNode *defaultAssignment = expression_statement(
            assignment_expression(identifier_node("value", 5, 76, 81),
                                  char_literal('c', 84, 87),
                                  76,
                                  87),
            76,
            88);
    SZrAstNode *switchNode;
    SZrAstNode *throwStmt;
    SZrAstNode *tryBody;
    SZrAstNode *tryNode;
    SZrAstNode *script;
    SZrAstNode *boolCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 122, 124);
    SZrAstNode *intCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 140, 142);
    SZrAstNode *stringCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 162, 164);
    SZrAstNode *charCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 182, 184);
    SZrAstNode *catchAllStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 196, 198);
    const SZrSemanticReachabilityFact *fact;

    caseNodes[0] = caseAssignment;
    defaultNodes[0] = defaultAssignment;
    switchNode = switch_statement_with_case_and_default(
            integer_literal(1, 29, 30),
            switch_case_node(integer_literal(1, 38, 39),
                             block_with_nodes(caseNodes, 1, 42, 58),
                             34,
                             60),
            switch_default_node(block_with_nodes(defaultNodes, 1, 74, 90),
                                66,
                                92),
            21,
            94);
    throwStmt =
            throw_statement_with_expr(identifier_node("value", 5, 102, 107),
                                      96,
                                      108);

    tryNodes[0] = declaration;
    tryNodes[1] = switchNode;
    tryNodes[2] = throwStmt;
    tryBody = block_with_nodes(tryNodes, 3, 6, 110);

    catchNodes[0] = catch_clause(block_with_nodes(&boolCatchStmt, 1, 118, 126));
    catchNodes[1] = catch_clause(block_with_nodes(&intCatchStmt, 1, 136, 144));
    catchNodes[2] = catch_clause(block_with_nodes(&stringCatchStmt, 1, 158, 166));
    catchNodes[3] = catch_clause(block_with_nodes(&charCatchStmt, 1, 178, 186));
    catchNodes[4] = catch_clause(block_with_nodes(&catchAllStmt, 1, 192, 200));

    add_catch_parameter(catchNodes[0],
                        typed_parameter("e", 1, "bool", 4, 112, 113, 115, 119));
    add_catch_parameter(catchNodes[1],
                        typed_parameter("e", 1, "int", 3, 130, 131, 133, 136));
    add_catch_parameter(catchNodes[2],
                        typed_parameter("e", 1, "string", 6, 148, 149, 151, 157));
    add_catch_parameter(catchNodes[3],
                        typed_parameter("e", 1, "char", 4, 170, 171, 173, 177));

    tryNode = try_statement_with_catches(tryBody, catchNodes, 5);
    script = script_with_statement(tryNode);

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    fact = reachability_fact_at(context, boolCatchStmt);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);

    fact = reachability_fact_at(context, intCatchStmt);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);

    TEST_ASSERT_NULL(reachability_fact_at(context, stringCatchStmt));

    fact = reachability_fact_at(context, charCatchStmt);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);

    fact = reachability_fact_at(context, catchAllStmt);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* case 中赋值后 return 不会流至后续 throw，只有 default 的 char 仍可匹配。 */
/* BUG: 断言失败时 Unity longjmp 跳过末尾原生资源释放；tearDown 仅销毁 VM 状态。 */
static void
test_cfg_ignores_terminating_switch_case_assignment_for_post_switch_typed_catch_matching(
        void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *tryNodes[3];
    SZrAstNode *caseBodyNodes[2];
    SZrAstNode *defaultNodes[1];
    SZrAstNode *catchNodes[5];
    SZrAstNode *declaration = typed_variable_declaration(
            "value",
            5,
            "int",
            3,
            integer_literal(1, 18, 19),
            8,
            20);
    SZrAstNode *caseAssignment = expression_statement(
            assignment_expression(identifier_node("value", 5, 44, 49),
                                  string_literal("x", 1, 52, 55),
                                  44,
                                  55),
            44,
            56);
    SZrAstNode *defaultAssignment = expression_statement(
            assignment_expression(identifier_node("value", 5, 84, 89),
                                  char_literal('c', 92, 95),
                                  84,
                                  95),
            84,
            96);
    SZrAstNode *switchNode;
    SZrAstNode *throwStmt;
    SZrAstNode *tryBody;
    SZrAstNode *tryNode;
    SZrAstNode *script;
    SZrAstNode *boolCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 130, 132);
    SZrAstNode *intCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 148, 150);
    SZrAstNode *stringCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 170, 172);
    SZrAstNode *charCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 190, 192);
    SZrAstNode *catchAllStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 204, 206);
    const SZrSemanticReachabilityFact *fact;

    caseBodyNodes[0] = caseAssignment;
    caseBodyNodes[1] = return_statement(58, 64);
    defaultNodes[0] = defaultAssignment;
    switchNode = switch_statement_with_case_and_default(
            identifier_node("tag", 3, 29, 32),
            switch_case_node(integer_literal(1, 38, 39),
                             block_with_nodes(caseBodyNodes, 2, 42, 66),
                             34,
                             68),
            switch_default_node(block_with_nodes(defaultNodes, 1, 82, 98),
                                74,
                                100),
            21,
            102);
    throwStmt =
            throw_statement_with_expr(identifier_node("value", 5, 110, 115),
                                      104,
                                      116);

    tryNodes[0] = declaration;
    tryNodes[1] = switchNode;
    tryNodes[2] = throwStmt;
    tryBody = block_with_nodes(tryNodes, 3, 6, 118);

    catchNodes[0] = catch_clause(block_with_nodes(&boolCatchStmt, 1, 126, 134));
    catchNodes[1] = catch_clause(block_with_nodes(&intCatchStmt, 1, 144, 152));
    catchNodes[2] = catch_clause(block_with_nodes(&stringCatchStmt, 1, 166, 174));
    catchNodes[3] = catch_clause(block_with_nodes(&charCatchStmt, 1, 186, 194));
    catchNodes[4] = catch_clause(block_with_nodes(&catchAllStmt, 1, 200, 208));

    add_catch_parameter(catchNodes[0],
                        typed_parameter("e", 1, "bool", 4, 120, 121, 123, 127));
    add_catch_parameter(catchNodes[1],
                        typed_parameter("e", 1, "int", 3, 138, 139, 141, 144));
    add_catch_parameter(catchNodes[2],
                        typed_parameter("e", 1, "string", 6, 156, 157, 159, 165));
    add_catch_parameter(catchNodes[3],
                        typed_parameter("e", 1, "char", 4, 178, 179, 181, 185));

    tryNode = try_statement_with_catches(tryBody, catchNodes, 5);
    script = script_with_statement(tryNode);

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    fact = reachability_fact_at(context, boolCatchStmt);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);

    fact = reachability_fact_at(context, intCatchStmt);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);

    fact = reachability_fact_at(context, stringCatchStmt);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);

    TEST_ASSERT_NULL(reachability_fact_at(context, charCatchStmt));

    fact = reachability_fact_at(context, catchAllStmt);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* 前置 string case 与 int selector 不同型；后续 int case 仍须被选中并留下 string 绑定。 */
/* BUG: 断言失败时 Unity longjmp 跳过末尾原生资源释放；tearDown 仅销毁 VM 状态。 */
static void
test_cfg_uses_matching_switch_case_after_mismatched_constant_kind_for_typed_catch_matching(
        void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *tryNodes[3];
    SZrAstNode *caseNodes[2];
    SZrAstNode *mismatchedCaseNodes[1];
    SZrAstNode *matchingCaseNodes[1];
    SZrAstNode *defaultNodes[1];
    SZrAstNode *catchNodes[5];
    SZrAstNode *declaration = typed_variable_declaration(
            "value",
            5,
            "int",
            3,
            integer_literal(1, 18, 19),
            8,
            20);
    SZrAstNode *mismatchedAssignment = expression_statement(
            assignment_expression(identifier_node("value", 5, 50, 55),
                                  string_literal("stale", 5, 58, 65),
                                  50,
                                  65),
            50,
            66);
    SZrAstNode *matchingAssignment = expression_statement(
            assignment_expression(identifier_node("value", 5, 86, 91),
                                  string_literal("x", 1, 94, 97),
                                  86,
                                  97),
            86,
            98);
    SZrAstNode *defaultAssignment = expression_statement(
            assignment_expression(identifier_node("value", 5, 118, 123),
                                  char_literal('c', 126, 129),
                                  118,
                                  129),
            118,
            130);
    SZrAstNode *switchNode;
    SZrAstNode *throwStmt;
    SZrAstNode *tryBody;
    SZrAstNode *tryNode;
    SZrAstNode *script;
    SZrAstNode *boolCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 162, 164);
    SZrAstNode *intCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 180, 182);
    SZrAstNode *stringCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 202, 204);
    SZrAstNode *charCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 222, 224);
    SZrAstNode *catchAllStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 236, 238);
    const SZrSemanticReachabilityFact *fact;

    mismatchedCaseNodes[0] = mismatchedAssignment;
    matchingCaseNodes[0] = matchingAssignment;
    defaultNodes[0] = defaultAssignment;
    caseNodes[0] = switch_case_node(string_literal("x", 1, 38, 41),
                                    block_with_nodes(mismatchedCaseNodes, 1, 48, 68),
                                    34,
                                    70);
    caseNodes[1] = switch_case_node(integer_literal(1, 74, 75),
                                    block_with_nodes(matchingCaseNodes, 1, 84, 100),
                                    72,
                                    102);
    switchNode = switch_statement_with_cases_and_default(
            integer_literal(1, 29, 30),
            caseNodes,
            2,
            switch_default_node(block_with_nodes(defaultNodes, 1, 116, 132),
                                108,
                                134),
            21,
            136);
    throwStmt =
            throw_statement_with_expr(identifier_node("value", 5, 144, 149),
                                      138,
                                      150);

    tryNodes[0] = declaration;
    tryNodes[1] = switchNode;
    tryNodes[2] = throwStmt;
    tryBody = block_with_nodes(tryNodes, 3, 6, 152);

    catchNodes[0] = catch_clause(block_with_nodes(&boolCatchStmt, 1, 158, 166));
    catchNodes[1] = catch_clause(block_with_nodes(&intCatchStmt, 1, 176, 184));
    catchNodes[2] = catch_clause(block_with_nodes(&stringCatchStmt, 1, 198, 206));
    catchNodes[3] = catch_clause(block_with_nodes(&charCatchStmt, 1, 218, 226));
    catchNodes[4] = catch_clause(block_with_nodes(&catchAllStmt, 1, 232, 240));

    add_catch_parameter(catchNodes[0],
                        typed_parameter("e", 1, "bool", 4, 154, 155, 157, 161));
    add_catch_parameter(catchNodes[1],
                        typed_parameter("e", 1, "int", 3, 172, 173, 175, 178));
    add_catch_parameter(catchNodes[2],
                        typed_parameter("e", 1, "string", 6, 190, 191, 193, 199));
    add_catch_parameter(catchNodes[3],
                        typed_parameter("e", 1, "char", 4, 212, 213, 215, 219));

    tryNode = try_statement_with_catches(tryBody, catchNodes, 5);
    script = script_with_statement(tryNode);

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    fact = reachability_fact_at(context, boolCatchStmt);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);

    fact = reachability_fact_at(context, intCatchStmt);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);

    TEST_ASSERT_NULL(reachability_fact_at(context, stringCatchStmt));

    fact = reachability_fact_at(context, charCatchStmt);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);

    fact = reachability_fact_at(context, catchAllStmt);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* 恒定命中的 case 内，恒真 return 排除 string，未选中的 else 也不得污染出口绑定。 */
/* BUG: 断言失败时 Unity longjmp 跳过末尾原生资源释放；tearDown 仅销毁 VM 状态。 */
static void
test_cfg_ignores_constant_terminating_switch_branch_assignment_for_post_switch_typed_catch_matching(
        void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *tryNodes[3];
    SZrAstNode *thenNodes[2];
    SZrAstNode *elseNodes[1];
    SZrAstNode *caseBodyNodes[1];
    SZrAstNode *defaultNodes[1];
    SZrAstNode *catchNodes[5];
    SZrAstNode *declaration = typed_variable_declaration(
            "value",
            5,
            "int",
            3,
            integer_literal(1, 18, 19),
            8,
            20);
    SZrAstNode *terminatingAssignment = expression_statement(
            assignment_expression(identifier_node("value", 5, 50, 55),
                                  string_literal("x", 1, 58, 61),
                                  50,
                                  61),
            50,
            62);
    SZrAstNode *terminatingReturn = return_statement(64, 70);
    SZrAstNode *unselectedAssignment = expression_statement(
            assignment_expression(identifier_node("value", 5, 80, 85),
                                  string_literal("stale", 5, 88, 95),
                                  80,
                                  95),
            80,
            96);
    SZrAstNode *defaultAssignment = expression_statement(
            assignment_expression(identifier_node("value", 5, 122, 127),
                                  char_literal('c', 130, 133),
                                  122,
                                  133),
            122,
            134);
    SZrAstNode *caseBranch;
    SZrAstNode *switchNode;
    SZrAstNode *throwStmt;
    SZrAstNode *tryBody;
    SZrAstNode *tryNode;
    SZrAstNode *script;
    SZrAstNode *boolCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 166, 168);
    SZrAstNode *intCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 184, 186);
    SZrAstNode *stringCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 206, 208);
    SZrAstNode *charCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 226, 228);
    SZrAstNode *catchAllStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 238, 240);
    const SZrSemanticReachabilityFact *fact;

    thenNodes[0] = terminatingAssignment;
    thenNodes[1] = terminatingReturn;
    elseNodes[0] = unselectedAssignment;
    caseBranch = if_statement(boolean_literal(ZR_TRUE, 42, 46),
                              block_with_nodes(thenNodes, 2, 48, 72),
                              block_with_nodes(elseNodes, 1, 78, 98),
                              38,
                              100);
    caseBodyNodes[0] = caseBranch;
    defaultNodes[0] = defaultAssignment;
    switchNode = switch_statement_with_case_and_default(
            identifier_node("tag", 3, 29, 32),
            switch_case_node(integer_literal(1, 36, 37),
                             block_with_nodes(caseBodyNodes, 1, 38, 102),
                             34,
                             104),
            switch_default_node(block_with_nodes(defaultNodes, 1, 120, 136),
                                112,
                                138),
            21,
            140);
    throwStmt =
            throw_statement_with_expr(identifier_node("value", 5, 148, 153),
                                      142,
                                      154);

    tryNodes[0] = declaration;
    tryNodes[1] = switchNode;
    tryNodes[2] = throwStmt;
    tryBody = block_with_nodes(tryNodes, 3, 6, 156);

    catchNodes[0] = catch_clause(block_with_nodes(&boolCatchStmt, 1, 162, 170));
    catchNodes[1] = catch_clause(block_with_nodes(&intCatchStmt, 1, 180, 188));
    catchNodes[2] = catch_clause(block_with_nodes(&stringCatchStmt, 1, 202, 210));
    catchNodes[3] = catch_clause(block_with_nodes(&charCatchStmt, 1, 222, 230));
    catchNodes[4] = catch_clause(block_with_nodes(&catchAllStmt, 1, 234, 242));

    add_catch_parameter(catchNodes[0],
                        typed_parameter("e", 1, "bool", 4, 158, 159, 161, 165));
    add_catch_parameter(catchNodes[1],
                        typed_parameter("e", 1, "int", 3, 176, 177, 179, 182));
    add_catch_parameter(catchNodes[2],
                        typed_parameter("e", 1, "string", 6, 194, 195, 197, 203));
    add_catch_parameter(catchNodes[3],
                        typed_parameter("e", 1, "char", 4, 216, 217, 219, 223));

    tryNode = try_statement_with_catches(tryBody, catchNodes, 5);
    script = script_with_statement(tryNode);

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    fact = reachability_fact_at(context, boolCatchStmt);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);

    fact = reachability_fact_at(context, intCatchStmt);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);

    fact = reachability_fact_at(context, stringCatchStmt);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);

    TEST_ASSERT_NULL(reachability_fact_at(context, charCatchStmt));

    fact = reachability_fact_at(context, catchAllStmt);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* CMake 独立注册本程序；Unity 通过 RUN_TEST 间接调用各测试及 setUp/tearDown。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(
            test_cfg_merges_switch_case_and_default_assignments_for_typed_catch_matching);
    RUN_TEST(
            test_cfg_uses_matching_switch_case_assignment_for_constant_selector_typed_catch_matching);
    RUN_TEST(
            test_cfg_ignores_terminating_switch_case_assignment_for_post_switch_typed_catch_matching);
    RUN_TEST(
            test_cfg_uses_matching_switch_case_after_mismatched_constant_kind_for_typed_catch_matching);
    RUN_TEST(
            test_cfg_ignores_constant_terminating_switch_branch_assignment_for_post_switch_typed_catch_matching);
    return UNITY_END();
}
