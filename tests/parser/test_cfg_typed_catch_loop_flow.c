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
            ZrCore_String_Create(g_state, "cfg_typed_catch_loop_flow_test.zr", 33);
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

static SZrAstNode *boolean_literal(TZrBool value,
                                   TZrSize startOffset,
                                   TZrSize endOffset) {
    SZrAstNode *literal = test_node(ZR_AST_BOOLEAN_LITERAL, startOffset, endOffset);

    literal->data.booleanLiteral.value = value;
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

static SZrAstNode *break_statement(TZrSize startOffset, TZrSize endOffset) {
    SZrAstNode *breakStmt = test_node(ZR_AST_BREAK_CONTINUE_STATEMENT,
                                      startOffset,
                                      endOffset);

    breakStmt->data.breakContinueStatement.isBreak = ZR_TRUE;
    return breakStmt;
}

static SZrAstNode *continue_statement(TZrSize startOffset, TZrSize endOffset) {
    SZrAstNode *continueStmt = test_node(ZR_AST_BREAK_CONTINUE_STATEMENT,
                                         startOffset,
                                         endOffset);

    continueStmt->data.breakContinueStatement.isBreak = ZR_FALSE;
    return continueStmt;
}

static SZrAstNode *throw_statement_with_expr(SZrAstNode *expr,
                                             TZrSize startOffset,
                                             TZrSize endOffset) {
    SZrAstNode *throwStmt = test_node(ZR_AST_THROW_STATEMENT, startOffset, endOffset);

    throwStmt->data.throwStatement.expr = expr;
    return throwStmt;
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

static SZrAstNode *while_statement(SZrAstNode *condition,
                                   SZrAstNode *body,
                                   TZrSize startOffset,
                                   TZrSize endOffset) {
    SZrAstNode *whileNode = test_node(ZR_AST_WHILE_LOOP, startOffset, endOffset);

    whileNode->data.whileLoop.cond = condition;
    whileNode->data.whileLoop.block = body;
    whileNode->data.whileLoop.isStatement = ZR_TRUE;
    return whileNode;
}

static SZrAstNode *foreach_statement(SZrAstNode *iterable,
                                     SZrAstNode *body,
                                     TZrSize startOffset,
                                     TZrSize endOffset) {
    SZrAstNode *foreachNode = test_node(ZR_AST_FOREACH_LOOP,
                                        startOffset,
                                        endOffset);

    foreachNode->data.foreachLoop.expr = iterable;
    foreachNode->data.foreachLoop.block = body;
    foreachNode->data.foreachLoop.isStatement = ZR_TRUE;
    return foreachNode;
}

static const SZrSemanticReachabilityFact *reachability_fact_at(
        SZrSemanticContext *context,
        SZrAstNode *node) {
    return ZrParser_SemanticFacts_FindReachabilityAtPosition(
            context,
            test_range(node->location.start.offset, node->location.start.offset));
}

static void
test_cfg_preserves_conditional_break_assignment_for_post_loop_typed_catch_matching(
        void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *thenNodes[2];
    SZrAstNode *loopNodes[2];
    SZrAstNode *tryNodes[3];
    SZrAstNode *catchNodes[5];
    SZrAstNode *declaration = typed_variable_declaration(
            "value",
            5,
            "int",
            3,
            integer_literal(1, 18, 19),
            8,
            20);
    SZrAstNode *breakAssignment = expression_statement(
            assignment_expression(identifier_node("value", 5, 42, 47),
                                  string_literal("x", 1, 50, 53),
                                  42,
                                  53),
            42,
            54);
    SZrAstNode *breakStmt = break_statement(56, 62);
    SZrAstNode *normalAssignment = expression_statement(
            assignment_expression(identifier_node("value", 5, 68, 73),
                                  char_literal('c', 76, 79),
                                  68,
                                  79),
            68,
            80);
    SZrAstNode *branch;
    SZrAstNode *loop;
    SZrAstNode *throwStmt;
    SZrAstNode *tryBody;
    SZrAstNode *tryNode;
    SZrAstNode *script;
    SZrAstNode *boolCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 106, 108);
    SZrAstNode *intCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 124, 126);
    SZrAstNode *stringCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 146, 148);
    SZrAstNode *charCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 166, 168);
    SZrAstNode *catchAllStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 178, 180);
    const SZrSemanticReachabilityFact *fact;

    thenNodes[0] = breakAssignment;
    thenNodes[1] = breakStmt;
    branch = if_statement(identifier_node("done", 4, 34, 38),
                          block_with_nodes(thenNodes, 2, 40, 64),
                          ZR_NULL,
                          30,
                          65);
    loopNodes[0] = branch;
    loopNodes[1] = normalAssignment;
    loop = while_statement(identifier_node("flag", 4, 24, 28),
                           block_with_nodes(loopNodes, 2, 29, 82),
                           21,
                           83);
    throwStmt =
            throw_statement_with_expr(identifier_node("value", 5, 91, 96), 85, 97);

    tryNodes[0] = declaration;
    tryNodes[1] = loop;
    tryNodes[2] = throwStmt;
    tryBody = block_with_nodes(tryNodes, 3, 6, 99);

    catchNodes[0] = catch_clause(block_with_nodes(&boolCatchStmt, 1, 102, 110));
    catchNodes[1] = catch_clause(block_with_nodes(&intCatchStmt, 1, 120, 128));
    catchNodes[2] = catch_clause(block_with_nodes(&stringCatchStmt, 1, 142, 150));
    catchNodes[3] = catch_clause(block_with_nodes(&charCatchStmt, 1, 162, 170));
    catchNodes[4] = catch_clause(block_with_nodes(&catchAllStmt, 1, 174, 182));

    add_catch_parameter(catchNodes[0],
                        typed_parameter("e", 1, "bool", 4, 100, 101, 103, 107));
    add_catch_parameter(catchNodes[1],
                        typed_parameter("e", 1, "int", 3, 114, 115, 117, 120));
    add_catch_parameter(catchNodes[2],
                        typed_parameter("e", 1, "string", 6, 132, 133, 135, 141));
    add_catch_parameter(catchNodes[3],
                        typed_parameter("e", 1, "char", 4, 154, 155, 157, 161));

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
    TEST_ASSERT_NULL(reachability_fact_at(context, intCatchStmt));
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

static void
test_cfg_preserves_pre_branch_break_assignment_for_post_loop_typed_catch_matching(
        void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *thenNodes[1];
    SZrAstNode *loopNodes[3];
    SZrAstNode *tryNodes[3];
    SZrAstNode *catchNodes[5];
    SZrAstNode *declaration = typed_variable_declaration(
            "value",
            5,
            "int",
            3,
            integer_literal(1, 18, 19),
            8,
            20);
    SZrAstNode *preBreakAssignment = expression_statement(
            assignment_expression(identifier_node("value", 5, 34, 39),
                                  string_literal("x", 1, 42, 45),
                                  34,
                                  45),
            34,
            46);
    SZrAstNode *breakStmt = break_statement(58, 64);
    SZrAstNode *normalAssignment = expression_statement(
            assignment_expression(identifier_node("value", 5, 70, 75),
                                  char_literal('c', 78, 81),
                                  70,
                                  81),
            70,
            82);
    SZrAstNode *branch;
    SZrAstNode *loop;
    SZrAstNode *throwStmt;
    SZrAstNode *tryBody;
    SZrAstNode *tryNode;
    SZrAstNode *script;
    SZrAstNode *boolCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 108, 110);
    SZrAstNode *intCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 126, 128);
    SZrAstNode *stringCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 148, 150);
    SZrAstNode *charCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 168, 170);
    SZrAstNode *catchAllStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 180, 182);
    const SZrSemanticReachabilityFact *fact;

    thenNodes[0] = breakStmt;
    branch = if_statement(identifier_node("done", 4, 50, 54),
                          block_with_nodes(thenNodes, 1, 56, 66),
                          ZR_NULL,
                          46,
                          67);
    loopNodes[0] = preBreakAssignment;
    loopNodes[1] = branch;
    loopNodes[2] = normalAssignment;
    loop = while_statement(identifier_node("flag", 4, 24, 28),
                           block_with_nodes(loopNodes, 3, 29, 84),
                           21,
                           85);
    throwStmt =
            throw_statement_with_expr(identifier_node("value", 5, 93, 98), 87, 99);

    tryNodes[0] = declaration;
    tryNodes[1] = loop;
    tryNodes[2] = throwStmt;
    tryBody = block_with_nodes(tryNodes, 3, 6, 101);

    catchNodes[0] = catch_clause(block_with_nodes(&boolCatchStmt, 1, 104, 112));
    catchNodes[1] = catch_clause(block_with_nodes(&intCatchStmt, 1, 122, 130));
    catchNodes[2] = catch_clause(block_with_nodes(&stringCatchStmt, 1, 144, 152));
    catchNodes[3] = catch_clause(block_with_nodes(&charCatchStmt, 1, 164, 172));
    catchNodes[4] = catch_clause(block_with_nodes(&catchAllStmt, 1, 176, 184));

    add_catch_parameter(catchNodes[0],
                        typed_parameter("e", 1, "bool", 4, 102, 103, 105, 109));
    add_catch_parameter(catchNodes[1],
                        typed_parameter("e", 1, "int", 3, 116, 117, 119, 122));
    add_catch_parameter(catchNodes[2],
                        typed_parameter("e", 1, "string", 6, 134, 135, 137, 143));
    add_catch_parameter(catchNodes[3],
                        typed_parameter("e", 1, "char", 4, 156, 157, 159, 163));

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
    TEST_ASSERT_NULL(reachability_fact_at(context, intCatchStmt));
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

static void
test_cfg_preserves_conditional_continue_assignment_for_post_loop_typed_catch_matching(
        void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *thenNodes[2];
    SZrAstNode *loopNodes[2];
    SZrAstNode *tryNodes[3];
    SZrAstNode *catchNodes[5];
    SZrAstNode *declaration = typed_variable_declaration(
            "value",
            5,
            "int",
            3,
            integer_literal(1, 18, 19),
            8,
            20);
    SZrAstNode *continueAssignment = expression_statement(
            assignment_expression(identifier_node("value", 5, 42, 47),
                                  string_literal("x", 1, 50, 53),
                                  42,
                                  53),
            42,
            54);
    SZrAstNode *continueStmt = continue_statement(56, 65);
    SZrAstNode *normalAssignment = expression_statement(
            assignment_expression(identifier_node("value", 5, 71, 76),
                                  char_literal('c', 79, 82),
                                  71,
                                  82),
            71,
            83);
    SZrAstNode *branch;
    SZrAstNode *loop;
    SZrAstNode *throwStmt;
    SZrAstNode *tryBody;
    SZrAstNode *tryNode;
    SZrAstNode *script;
    SZrAstNode *boolCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 109, 111);
    SZrAstNode *intCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 127, 129);
    SZrAstNode *stringCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 149, 151);
    SZrAstNode *charCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 169, 171);
    SZrAstNode *catchAllStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 181, 183);
    const SZrSemanticReachabilityFact *fact;

    thenNodes[0] = continueAssignment;
    thenNodes[1] = continueStmt;
    branch = if_statement(identifier_node("done", 4, 34, 38),
                          block_with_nodes(thenNodes, 2, 40, 67),
                          ZR_NULL,
                          30,
                          68);
    loopNodes[0] = branch;
    loopNodes[1] = normalAssignment;
    loop = while_statement(identifier_node("flag", 4, 24, 28),
                           block_with_nodes(loopNodes, 2, 29, 85),
                           21,
                           86);
    throwStmt =
            throw_statement_with_expr(identifier_node("value", 5, 94, 99), 88, 100);

    tryNodes[0] = declaration;
    tryNodes[1] = loop;
    tryNodes[2] = throwStmt;
    tryBody = block_with_nodes(tryNodes, 3, 6, 102);

    catchNodes[0] = catch_clause(block_with_nodes(&boolCatchStmt, 1, 105, 113));
    catchNodes[1] = catch_clause(block_with_nodes(&intCatchStmt, 1, 123, 131));
    catchNodes[2] = catch_clause(block_with_nodes(&stringCatchStmt, 1, 145, 153));
    catchNodes[3] = catch_clause(block_with_nodes(&charCatchStmt, 1, 165, 173));
    catchNodes[4] = catch_clause(block_with_nodes(&catchAllStmt, 1, 177, 185));

    add_catch_parameter(catchNodes[0],
                        typed_parameter("e", 1, "bool", 4, 103, 104, 106, 110));
    add_catch_parameter(catchNodes[1],
                        typed_parameter("e", 1, "int", 3, 117, 118, 120, 123));
    add_catch_parameter(catchNodes[2],
                        typed_parameter("e", 1, "string", 6, 135, 136, 138, 144));
    add_catch_parameter(catchNodes[3],
                        typed_parameter("e", 1, "char", 4, 157, 158, 160, 164));

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
    TEST_ASSERT_NULL(reachability_fact_at(context, intCatchStmt));
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

static void test_cfg_merges_foreach_assignment_with_incoming_type_for_typed_catch_matching(
        void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *loopNodes[1];
    SZrAstNode *tryNodes[3];
    SZrAstNode *catchNodes[4];
    SZrAstNode *declaration = typed_variable_declaration(
            "value",
            5,
            "int",
            3,
            integer_literal(1, 18, 19),
            8,
            20);
    SZrAstNode *loopAssignment = expression_statement(
            assignment_expression(identifier_node("value", 5, 40, 45),
                                  string_literal("x", 1, 48, 51),
                                  40,
                                  51),
            40,
            52);
    SZrAstNode *loop;
    SZrAstNode *throwStmt;
    SZrAstNode *tryBody;
    SZrAstNode *tryNode;
    SZrAstNode *script;
    SZrAstNode *boolCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 86, 88);
    SZrAstNode *intCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 104, 106);
    SZrAstNode *stringCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 126, 128);
    SZrAstNode *catchAllStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 140, 142);
    const SZrSemanticReachabilityFact *fact;

    loopNodes[0] = loopAssignment;
    loop = foreach_statement(identifier_node("items", 5, 28, 33),
                             block_with_nodes(loopNodes, 1, 38, 54),
                             21,
                             55);
    throwStmt =
            throw_statement_with_expr(identifier_node("value", 5, 63, 68), 57, 69);

    tryNodes[0] = declaration;
    tryNodes[1] = loop;
    tryNodes[2] = throwStmt;
    tryBody = block_with_nodes(tryNodes, 3, 6, 71);

    catchNodes[0] = catch_clause(block_with_nodes(&boolCatchStmt, 1, 82, 90));
    catchNodes[1] = catch_clause(block_with_nodes(&intCatchStmt, 1, 100, 108));
    catchNodes[2] = catch_clause(block_with_nodes(&stringCatchStmt, 1, 122, 130));
    catchNodes[3] = catch_clause(block_with_nodes(&catchAllStmt, 1, 136, 144));

    add_catch_parameter(catchNodes[0],
                        typed_parameter("e", 1, "bool", 4, 74, 75, 77, 81));
    add_catch_parameter(catchNodes[1],
                        typed_parameter("e", 1, "int", 3, 92, 93, 95, 98));
    add_catch_parameter(catchNodes[2],
                        typed_parameter("e", 1, "string", 6, 112, 113, 115, 121));

    tryNode = try_statement_with_catches(tryBody, catchNodes, 4);
    script = script_with_statement(tryNode);

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

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

static void
test_cfg_prunes_assignment_after_constant_break_branch_for_typed_catch_matching(
        void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *thenNodes[2];
    SZrAstNode *loopNodes[2];
    SZrAstNode *tryNodes[3];
    SZrAstNode *catchNodes[5];
    SZrAstNode *declaration = typed_variable_declaration(
            "value",
            5,
            "int",
            3,
            integer_literal(1, 18, 19),
            8,
            20);
    SZrAstNode *breakAssignment = expression_statement(
            assignment_expression(identifier_node("value", 5, 42, 47),
                                  string_literal("x", 1, 50, 53),
                                  42,
                                  53),
            42,
            54);
    SZrAstNode *breakStmt = break_statement(56, 62);
    SZrAstNode *unreachableAssignment = expression_statement(
            assignment_expression(identifier_node("value", 5, 68, 73),
                                  char_literal('c', 76, 79),
                                  68,
                                  79),
            68,
            80);
    SZrAstNode *branch;
    SZrAstNode *loop;
    SZrAstNode *throwStmt;
    SZrAstNode *tryBody;
    SZrAstNode *tryNode;
    SZrAstNode *script;
    SZrAstNode *boolCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 106, 108);
    SZrAstNode *intCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 124, 126);
    SZrAstNode *stringCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 146, 148);
    SZrAstNode *charCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 166, 168);
    SZrAstNode *catchAllStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 178, 180);
    const SZrSemanticReachabilityFact *fact;

    thenNodes[0] = breakAssignment;
    thenNodes[1] = breakStmt;
    branch = if_statement(boolean_literal(ZR_TRUE, 34, 38),
                          block_with_nodes(thenNodes, 2, 40, 64),
                          ZR_NULL,
                          30,
                          65);
    loopNodes[0] = branch;
    loopNodes[1] = unreachableAssignment;
    loop = while_statement(identifier_node("flag", 4, 24, 28),
                           block_with_nodes(loopNodes, 2, 29, 82),
                           21,
                           83);
    throwStmt =
            throw_statement_with_expr(identifier_node("value", 5, 91, 96), 85, 97);

    tryNodes[0] = declaration;
    tryNodes[1] = loop;
    tryNodes[2] = throwStmt;
    tryBody = block_with_nodes(tryNodes, 3, 6, 99);

    catchNodes[0] = catch_clause(block_with_nodes(&boolCatchStmt, 1, 102, 110));
    catchNodes[1] = catch_clause(block_with_nodes(&intCatchStmt, 1, 120, 128));
    catchNodes[2] = catch_clause(block_with_nodes(&stringCatchStmt, 1, 142, 150));
    catchNodes[3] = catch_clause(block_with_nodes(&charCatchStmt, 1, 162, 170));
    catchNodes[4] = catch_clause(block_with_nodes(&catchAllStmt, 1, 174, 182));

    add_catch_parameter(catchNodes[0],
                        typed_parameter("e", 1, "bool", 4, 100, 101, 103, 107));
    add_catch_parameter(catchNodes[1],
                        typed_parameter("e", 1, "int", 3, 114, 115, 117, 120));
    add_catch_parameter(catchNodes[2],
                        typed_parameter("e", 1, "string", 6, 132, 133, 135, 141));
    add_catch_parameter(catchNodes[3],
                        typed_parameter("e", 1, "char", 4, 154, 155, 157, 161));

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
    TEST_ASSERT_NULL(reachability_fact_at(context, intCatchStmt));
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
test_cfg_prunes_unselected_constant_break_exit_branch_for_typed_catch_matching(
        void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *thenNodes[2];
    SZrAstNode *elseNodes[2];
    SZrAstNode *loopNodes[1];
    SZrAstNode *tryNodes[3];
    SZrAstNode *catchNodes[5];
    SZrAstNode *declaration = typed_variable_declaration(
            "value",
            5,
            "int",
            3,
            integer_literal(1, 18, 19),
            8,
            20);
    SZrAstNode *selectedAssignment = expression_statement(
            assignment_expression(identifier_node("value", 5, 42, 47),
                                  string_literal("x", 1, 50, 53),
                                  42,
                                  53),
            42,
            54);
    SZrAstNode *selectedBreak = break_statement(56, 62);
    SZrAstNode *unselectedAssignment = expression_statement(
            assignment_expression(identifier_node("value", 5, 70, 75),
                                  char_literal('c', 78, 81),
                                  70,
                                  81),
            70,
            82);
    SZrAstNode *unselectedBreak = break_statement(84, 90);
    SZrAstNode *branch;
    SZrAstNode *loop;
    SZrAstNode *throwStmt;
    SZrAstNode *tryBody;
    SZrAstNode *tryNode;
    SZrAstNode *script;
    SZrAstNode *boolCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 116, 118);
    SZrAstNode *intCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 134, 136);
    SZrAstNode *stringCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 156, 158);
    SZrAstNode *charCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 176, 178);
    SZrAstNode *catchAllStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 188, 190);
    const SZrSemanticReachabilityFact *fact;

    thenNodes[0] = selectedAssignment;
    thenNodes[1] = selectedBreak;
    elseNodes[0] = unselectedAssignment;
    elseNodes[1] = unselectedBreak;
    branch = if_statement(boolean_literal(ZR_TRUE, 34, 38),
                          block_with_nodes(thenNodes, 2, 40, 64),
                          block_with_nodes(elseNodes, 2, 68, 92),
                          30,
                          93);
    loopNodes[0] = branch;
    loop = while_statement(identifier_node("flag", 4, 24, 28),
                           block_with_nodes(loopNodes, 1, 29, 95),
                           21,
                           96);
    throwStmt =
            throw_statement_with_expr(identifier_node("value", 5, 104, 109),
                                      98,
                                      110);

    tryNodes[0] = declaration;
    tryNodes[1] = loop;
    tryNodes[2] = throwStmt;
    tryBody = block_with_nodes(tryNodes, 3, 6, 112);

    catchNodes[0] = catch_clause(block_with_nodes(&boolCatchStmt, 1, 112, 120));
    catchNodes[1] = catch_clause(block_with_nodes(&intCatchStmt, 1, 130, 138));
    catchNodes[2] = catch_clause(block_with_nodes(&stringCatchStmt, 1, 152, 160));
    catchNodes[3] = catch_clause(block_with_nodes(&charCatchStmt, 1, 172, 180));
    catchNodes[4] = catch_clause(block_with_nodes(&catchAllStmt, 1, 184, 192));

    add_catch_parameter(catchNodes[0],
                        typed_parameter("e", 1, "bool", 4, 110, 111, 113, 117));
    add_catch_parameter(catchNodes[1],
                        typed_parameter("e", 1, "int", 3, 124, 125, 127, 130));
    add_catch_parameter(catchNodes[2],
                        typed_parameter("e", 1, "string", 6, 142, 143, 145, 151));
    add_catch_parameter(catchNodes[3],
                        typed_parameter("e", 1, "char", 4, 164, 165, 167, 171));

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
    TEST_ASSERT_NULL(reachability_fact_at(context, intCatchStmt));
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

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(
            test_cfg_preserves_conditional_break_assignment_for_post_loop_typed_catch_matching);
    RUN_TEST(
            test_cfg_preserves_pre_branch_break_assignment_for_post_loop_typed_catch_matching);
    RUN_TEST(
            test_cfg_preserves_conditional_continue_assignment_for_post_loop_typed_catch_matching);
    RUN_TEST(
            test_cfg_merges_foreach_assignment_with_incoming_type_for_typed_catch_matching);
    RUN_TEST(
            test_cfg_prunes_assignment_after_constant_break_branch_for_typed_catch_matching);
    RUN_TEST(
            test_cfg_prunes_unselected_constant_break_exit_branch_for_typed_catch_matching);
    return UNITY_END();
}
