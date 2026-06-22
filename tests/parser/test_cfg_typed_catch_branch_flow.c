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

static SZrFileRange test_range(TZrSize startOffset, TZrSize endOffset) {
    SZrFileRange range;

    range.start.offset = startOffset;
    range.start.line = 1;
    range.start.column = (TZrInt32)startOffset + 1;
    range.end.offset = endOffset;
    range.end.line = 1;
    range.end.column = (TZrInt32)endOffset + 1;
    range.source =
            ZrCore_String_Create(g_state, "cfg_typed_catch_branch_flow_test.zr", 35);
    return range;
}

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

static SZrAstNode *script_with_statement(SZrAstNode *statement) {
    SZrAstNode *script = test_node(ZR_AST_SCRIPT, 0, 180);

    script->data.script.statements = ZrParser_AstNodeArray_New(g_state, 1);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    ZrParser_AstNodeArray_Add(g_state, script->data.script.statements, statement);
    return script;
}

static SZrAstNode *try_statement_with_catches(SZrAstNode *body,
                                              SZrAstNode **catchNodes,
                                              TZrSize catchCount) {
    SZrAstNode *tryNode = test_node(ZR_AST_TRY_CATCH_FINALLY_STATEMENT, 0, 180);
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

static SZrAstNode *catch_clause(SZrAstNode *body) {
    SZrAstNode *catchNode = test_node(ZR_AST_CATCH_CLAUSE, 84, 172);

    catchNode->data.catchClause.block = body;
    return catchNode;
}

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

static void add_catch_parameter(SZrAstNode *catchNode, SZrAstNode *parameter) {
    catchNode->data.catchClause.pattern = ZrParser_AstNodeArray_New(g_state, 1);
    TEST_ASSERT_NOT_NULL(catchNode->data.catchClause.pattern);
    ZrParser_AstNodeArray_Add(g_state, catchNode->data.catchClause.pattern, parameter);
}

static SZrAstNode *boolean_literal(TZrBool value,
                                   TZrSize startOffset,
                                   TZrSize endOffset) {
    SZrAstNode *literal = test_node(ZR_AST_BOOLEAN_LITERAL, startOffset, endOffset);

    literal->data.booleanLiteral.value = value;
    return literal;
}

static SZrAstNode *integer_literal(TZrInt64 value,
                                   TZrSize startOffset,
                                   TZrSize endOffset) {
    SZrAstNode *literal = test_node(ZR_AST_INTEGER_LITERAL, startOffset, endOffset);

    literal->data.integerLiteral.value = value;
    return literal;
}

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

static SZrAstNode *return_statement(TZrSize startOffset, TZrSize endOffset) {
    return test_node(ZR_AST_RETURN_STATEMENT, startOffset, endOffset);
}

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

static const SZrSemanticReachabilityFact *reachability_fact_at(
        SZrSemanticContext *context,
        SZrAstNode *node) {
    return ZrParser_SemanticFacts_FindReachabilityAtPosition(
            context,
            test_range(node->location.start.offset, node->location.start.offset));
}

static void
test_cfg_uses_constant_true_branch_assignment_for_typed_catch_matching(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *tryNodes[3];
    SZrAstNode *thenNodes[1];
    SZrAstNode *elseNodes[1];
    SZrAstNode *catchNodes[5];
    SZrAstNode *declaration = typed_variable_declaration(
            "value",
            5,
            "int",
            3,
            integer_literal(1, 18, 19),
            8,
            20);
    SZrAstNode *thenAssignment = expression_statement(
            assignment_expression(identifier_node("value", 5, 39, 44),
                                  string_literal("x", 1, 47, 50),
                                  39,
                                  50),
            39,
            51);
    SZrAstNode *elseAssignment = expression_statement(
            assignment_expression(identifier_node("value", 5, 60, 65),
                                  char_literal('c', 68, 71),
                                  60,
                                  71),
            60,
            72);
    SZrAstNode *branch;
    SZrAstNode *throwStmt;
    SZrAstNode *tryBody;
    SZrAstNode *tryNode;
    SZrAstNode *script;
    SZrAstNode *boolCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 96, 98);
    SZrAstNode *intCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 114, 116);
    SZrAstNode *stringCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 136, 138);
    SZrAstNode *charCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 156, 158);
    SZrAstNode *catchAllStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 170, 172);
    const SZrSemanticReachabilityFact *fact;

    thenNodes[0] = thenAssignment;
    elseNodes[0] = elseAssignment;
    branch = if_statement(boolean_literal(ZR_TRUE, 25, 29),
                          block_with_nodes(thenNodes, 1, 37, 53),
                          block_with_nodes(elseNodes, 1, 58, 74),
                          21,
                          75);
    throwStmt =
            throw_statement_with_expr(identifier_node("value", 5, 83, 88),
                                      77,
                                      89);

    tryNodes[0] = declaration;
    tryNodes[1] = branch;
    tryNodes[2] = throwStmt;
    tryBody = block_with_nodes(tryNodes, 3, 6, 91);

    catchNodes[0] = catch_clause(block_with_nodes(&boolCatchStmt, 1, 92, 100));
    catchNodes[1] = catch_clause(block_with_nodes(&intCatchStmt, 1, 110, 118));
    catchNodes[2] = catch_clause(block_with_nodes(&stringCatchStmt, 1, 132, 140));
    catchNodes[3] = catch_clause(block_with_nodes(&charCatchStmt, 1, 152, 160));
    catchNodes[4] = catch_clause(block_with_nodes(&catchAllStmt, 1, 166, 174));

    add_catch_parameter(catchNodes[0],
                        typed_parameter("e", 1, "bool", 4, 92, 93, 95, 99));
    add_catch_parameter(catchNodes[1],
                        typed_parameter("e", 1, "int", 3, 104, 105, 107, 110));
    add_catch_parameter(catchNodes[2],
                        typed_parameter("e", 1, "string", 6, 122, 123, 125, 131));
    add_catch_parameter(catchNodes[3],
                        typed_parameter("e", 1, "char", 4, 144, 145, 147, 151));

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

static void
test_cfg_prunes_terminating_symbolic_branch_assignment_for_typed_catch_matching(
        void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *tryNodes[3];
    SZrAstNode *thenNodes[2];
    SZrAstNode *elseNodes[1];
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
            assignment_expression(identifier_node("value", 5, 39, 44),
                                  string_literal("x", 1, 47, 50),
                                  39,
                                  50),
            39,
            51);
    SZrAstNode *terminatingReturn = return_statement(53, 59);
    SZrAstNode *fallthroughAssignment = expression_statement(
            assignment_expression(identifier_node("value", 5, 70, 75),
                                  char_literal('c', 78, 81),
                                  70,
                                  81),
            70,
            82);
    SZrAstNode *branch;
    SZrAstNode *throwStmt;
    SZrAstNode *tryBody;
    SZrAstNode *tryNode;
    SZrAstNode *script;
    SZrAstNode *boolCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 112, 114);
    SZrAstNode *intCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 130, 132);
    SZrAstNode *stringCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 152, 154);
    SZrAstNode *charCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 172, 174);
    SZrAstNode *catchAllStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 186, 188);
    const SZrSemanticReachabilityFact *fact;

    thenNodes[0] = terminatingAssignment;
    thenNodes[1] = terminatingReturn;
    elseNodes[0] = fallthroughAssignment;
    branch = if_statement(identifier_node("flag", 4, 25, 29),
                          block_with_nodes(thenNodes, 2, 37, 61),
                          block_with_nodes(elseNodes, 1, 68, 84),
                          21,
                          85);
    throwStmt =
            throw_statement_with_expr(identifier_node("value", 5, 93, 98),
                                      87,
                                      99);

    tryNodes[0] = declaration;
    tryNodes[1] = branch;
    tryNodes[2] = throwStmt;
    tryBody = block_with_nodes(tryNodes, 3, 6, 101);

    catchNodes[0] = catch_clause(block_with_nodes(&boolCatchStmt, 1, 108, 116));
    catchNodes[1] = catch_clause(block_with_nodes(&intCatchStmt, 1, 126, 134));
    catchNodes[2] = catch_clause(block_with_nodes(&stringCatchStmt, 1, 148, 156));
    catchNodes[3] = catch_clause(block_with_nodes(&charCatchStmt, 1, 168, 176));
    catchNodes[4] = catch_clause(block_with_nodes(&catchAllStmt, 1, 182, 190));

    add_catch_parameter(catchNodes[0],
                        typed_parameter("e", 1, "bool", 4, 102, 103, 105, 109));
    add_catch_parameter(catchNodes[1],
                        typed_parameter("e", 1, "int", 3, 120, 121, 123, 126));
    add_catch_parameter(catchNodes[2],
                        typed_parameter("e", 1, "string", 6, 138, 139, 141, 147));
    add_catch_parameter(catchNodes[3],
                        typed_parameter("e", 1, "char", 4, 160, 161, 163, 167));

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

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_cfg_uses_constant_true_branch_assignment_for_typed_catch_matching);
    RUN_TEST(
            test_cfg_prunes_terminating_symbolic_branch_assignment_for_typed_catch_matching);
    return UNITY_END();
}
