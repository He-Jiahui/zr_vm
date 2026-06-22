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
    range.source = ZrCore_String_Create(g_state, "cfg_typed_catch_flow_test.zr", 28);
    return range;
}

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

static SZrAstNode *script_with_statement(SZrAstNode *statement) {
    SZrAstNode *script = test_node(ZR_AST_SCRIPT, 0, 160);

    script->data.script.statements = ZrParser_AstNodeArray_New(g_state, 1);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    ZrParser_AstNodeArray_Add(g_state, script->data.script.statements, statement);
    return script;
}

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

static SZrAstNode *block_with_three_statements(SZrAstNode *firstStatement,
                                               SZrAstNode *secondStatement,
                                               SZrAstNode *thirdStatement,
                                               TZrSize startOffset,
                                               TZrSize endOffset) {
    SZrAstNode *block = test_node(ZR_AST_BLOCK, startOffset, endOffset);

    block->data.block.body = ZrParser_AstNodeArray_New(g_state, 3);
    TEST_ASSERT_NOT_NULL(block->data.block.body);
    block->data.block.isStatement = ZR_TRUE;
    ZrParser_AstNodeArray_Add(g_state, block->data.block.body, firstStatement);
    ZrParser_AstNodeArray_Add(g_state, block->data.block.body, secondStatement);
    ZrParser_AstNodeArray_Add(g_state, block->data.block.body, thirdStatement);
    return block;
}

static SZrAstNode *block_with_four_statements(SZrAstNode *firstStatement,
                                              SZrAstNode *secondStatement,
                                              SZrAstNode *thirdStatement,
                                              SZrAstNode *fourthStatement,
                                              TZrSize startOffset,
                                              TZrSize endOffset) {
    SZrAstNode *block = test_node(ZR_AST_BLOCK, startOffset, endOffset);

    block->data.block.body = ZrParser_AstNodeArray_New(g_state, 4);
    TEST_ASSERT_NOT_NULL(block->data.block.body);
    block->data.block.isStatement = ZR_TRUE;
    ZrParser_AstNodeArray_Add(g_state, block->data.block.body, firstStatement);
    ZrParser_AstNodeArray_Add(g_state, block->data.block.body, secondStatement);
    ZrParser_AstNodeArray_Add(g_state, block->data.block.body, thirdStatement);
    ZrParser_AstNodeArray_Add(g_state, block->data.block.body, fourthStatement);
    return block;
}

static SZrAstNode *try_statement_with_four_catches(SZrAstNode *body,
                                                   SZrAstNode *firstCatchNode,
                                                   SZrAstNode *secondCatchNode,
                                                   SZrAstNode *thirdCatchNode,
                                                   SZrAstNode *fourthCatchNode) {
    SZrAstNode *tryNode = test_node(ZR_AST_TRY_CATCH_FINALLY_STATEMENT, 0, 160);

    tryNode->data.tryCatchFinallyStatement.block = body;
    tryNode->data.tryCatchFinallyStatement.catchClauses =
            ZrParser_AstNodeArray_New(g_state, 4);
    TEST_ASSERT_NOT_NULL(tryNode->data.tryCatchFinallyStatement.catchClauses);
    ZrParser_AstNodeArray_Add(g_state,
                              tryNode->data.tryCatchFinallyStatement.catchClauses,
                              firstCatchNode);
    ZrParser_AstNodeArray_Add(g_state,
                              tryNode->data.tryCatchFinallyStatement.catchClauses,
                              secondCatchNode);
    ZrParser_AstNodeArray_Add(g_state,
                              tryNode->data.tryCatchFinallyStatement.catchClauses,
                              thirdCatchNode);
    ZrParser_AstNodeArray_Add(g_state,
                              tryNode->data.tryCatchFinallyStatement.catchClauses,
                              fourthCatchNode);
    return tryNode;
}

static SZrAstNode *catch_clause(SZrAstNode *body) {
    SZrAstNode *catchNode = test_node(ZR_AST_CATCH_CLAUSE, 64, 128);

    catchNode->data.catchClause.block = body;
    return catchNode;
}

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

static SZrAstNode *untyped_parameter(const char *name,
                                     TZrSize nameLength,
                                     TZrSize startOffset,
                                     TZrSize endOffset) {
    SZrAstNode *parameter = test_node(ZR_AST_PARAMETER, startOffset, endOffset);
    SZrAstNode *nameNode = identifier_node(name, nameLength, startOffset, endOffset);

    parameter->data.parameter.name = &nameNode->data.identifier;
    parameter->data.parameter.nameLocation = nameNode->location;
    parameter->data.parameter.passingMode = ZR_PARAMETER_PASSING_MODE_VALUE;
    parameter->data.parameter.genericKind = ZR_GENERIC_PARAMETER_TYPE;
    parameter->data.parameter.genericTypeConstraints = ZR_NULL;
    return parameter;
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
            untyped_parameter(name, nameLength, nameStartOffset, nameEndOffset);

    parameter->data.parameter.typeInfo =
            type_info_named(typeName, typeNameLength, typeStartOffset, typeEndOffset);
    return parameter;
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

static void add_catch_parameter(SZrAstNode *catchNode, SZrAstNode *parameter) {
    catchNode->data.catchClause.pattern = ZrParser_AstNodeArray_New(g_state, 1);
    TEST_ASSERT_NOT_NULL(catchNode->data.catchClause.pattern);
    ZrParser_AstNodeArray_Add(g_state, catchNode->data.catchClause.pattern, parameter);
}

static SZrAstNode *integer_literal(TZrInt64 value, TZrSize startOffset, TZrSize endOffset) {
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

static SZrAstNode *char_literal(TZrChar value, TZrSize startOffset, TZrSize endOffset) {
    SZrAstNode *literal = test_node(ZR_AST_CHAR_LITERAL, startOffset, endOffset);

    literal->data.charLiteral.value = value;
    literal->data.charLiteral.hasError = ZR_FALSE;
    return literal;
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

static SZrAstNode *break_statement(TZrSize startOffset, TZrSize endOffset) {
    SZrAstNode *breakStmt = test_node(ZR_AST_BREAK_CONTINUE_STATEMENT,
                                      startOffset,
                                      endOffset);

    breakStmt->data.breakContinueStatement.isBreak = ZR_TRUE;
    return breakStmt;
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

static SZrAstNode *for_statement(SZrAstNode *condition,
                                 SZrAstNode *body,
                                 TZrSize startOffset,
                                 TZrSize endOffset) {
    SZrAstNode *forNode = test_node(ZR_AST_FOR_LOOP, startOffset, endOffset);

    forNode->data.forLoop.cond = condition;
    forNode->data.forLoop.block = body;
    forNode->data.forLoop.isStatement = ZR_TRUE;
    return forNode;
}

static SZrAstNode *for_statement_with_init(SZrAstNode *init,
                                           SZrAstNode *condition,
                                           SZrAstNode *body,
                                           TZrSize startOffset,
                                           TZrSize endOffset) {
    SZrAstNode *forNode = for_statement(condition, body, startOffset, endOffset);

    forNode->data.forLoop.init = init;
    return forNode;
}

static SZrAstNode *for_statement_with_step(SZrAstNode *condition,
                                           SZrAstNode *body,
                                           SZrAstNode *step,
                                           TZrSize startOffset,
                                           TZrSize endOffset) {
    SZrAstNode *forNode = for_statement(condition, body, startOffset, endOffset);

    forNode->data.forLoop.step = step;
    return forNode;
}

static const SZrSemanticReachabilityFact *reachability_fact_at(
        SZrSemanticContext *context,
        SZrAstNode *node) {
    return ZrParser_SemanticFacts_FindReachabilityAtPosition(
            context,
            test_range(node->location.start.offset, node->location.start.offset));
}

static void test_cfg_merges_branch_assignment_types_for_typed_catch_matching(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *declaration = typed_variable_declaration(
            "value",
            5,
            "int",
            3,
            integer_literal(1, 18, 19),
            8,
            20);
    SZrAstNode *thenAssignment = expression_statement(
            assignment_expression(identifier_node("value", 5, 31, 36),
                                  string_literal("x", 1, 39, 42),
                                  31,
                                  42),
            31,
            43);
    SZrAstNode *elseAssignment = expression_statement(
            assignment_expression(identifier_node("value", 5, 52, 57),
                                  char_literal('c', 60, 63),
                                  52,
                                  63),
            52,
            64);
    SZrAstNode *branch = if_statement(identifier_node("flag", 4, 22, 26),
                                      block_with_statement(thenAssignment, 29, 45),
                                      block_with_statement(elseAssignment, 50, 66),
                                      21,
                                      67);
    SZrAstNode *throwStmt =
            throw_statement_with_expr(identifier_node("value", 5, 74, 79), 68, 80);
    SZrAstNode *tryBody = block_with_three_statements(declaration, branch, throwStmt, 6, 82);
    SZrAstNode *intCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 96, 98);
    SZrAstNode *intCatchNode = catch_clause(block_with_statement(intCatchStmt, 92, 100));
    SZrAstNode *stringCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 116, 118);
    SZrAstNode *stringCatchNode =
            catch_clause(block_with_statement(stringCatchStmt, 112, 120));
    SZrAstNode *charCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 136, 138);
    SZrAstNode *charCatchNode = catch_clause(block_with_statement(charCatchStmt, 132, 140));
    SZrAstNode *catchAllStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 152, 154);
    SZrAstNode *catchAllNode = catch_clause(block_with_statement(catchAllStmt, 148, 156));
    SZrAstNode *tryNode;
    SZrAstNode *script;
    const SZrSemanticReachabilityFact *fact;

    add_catch_parameter(intCatchNode,
                        typed_parameter("e", 1, "int", 3, 84, 85, 87, 90));
    add_catch_parameter(stringCatchNode,
                        typed_parameter("e", 1, "string", 6, 104, 105, 107, 113));
    add_catch_parameter(charCatchNode,
                        typed_parameter("e", 1, "char", 4, 124, 125, 127, 131));
    tryNode = try_statement_with_four_catches(
            tryBody,
            intCatchNode,
            stringCatchNode,
            charCatchNode,
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
    TEST_ASSERT_NULL(reachability_fact_at(context, charCatchStmt));

    fact = reachability_fact_at(context, catchAllStmt);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_merges_partial_branch_assignment_with_incoming_type_for_typed_catch_matching(
        void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *declaration = typed_variable_declaration(
            "value",
            5,
            "int",
            3,
            integer_literal(1, 18, 19),
            8,
            20);
    SZrAstNode *thenAssignment = expression_statement(
            assignment_expression(identifier_node("value", 5, 31, 36),
                                  string_literal("x", 1, 39, 42),
                                  31,
                                  42),
            31,
            43);
    SZrAstNode *branch = if_statement(identifier_node("flag", 4, 22, 26),
                                      block_with_statement(thenAssignment, 29, 45),
                                      ZR_NULL,
                                      21,
                                      46);
    SZrAstNode *throwStmt =
            throw_statement_with_expr(identifier_node("value", 5, 54, 59), 48, 60);
    SZrAstNode *tryBody = block_with_three_statements(declaration, branch, throwStmt, 6, 62);
    SZrAstNode *boolCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 76, 78);
    SZrAstNode *boolCatchNode = catch_clause(block_with_statement(boolCatchStmt, 72, 80));
    SZrAstNode *intCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 96, 98);
    SZrAstNode *intCatchNode = catch_clause(block_with_statement(intCatchStmt, 92, 100));
    SZrAstNode *stringCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 116, 118);
    SZrAstNode *stringCatchNode =
            catch_clause(block_with_statement(stringCatchStmt, 112, 120));
    SZrAstNode *catchAllStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 136, 138);
    SZrAstNode *catchAllNode = catch_clause(block_with_statement(catchAllStmt, 132, 140));
    SZrAstNode *tryNode;
    SZrAstNode *script;
    const SZrSemanticReachabilityFact *fact;

    add_catch_parameter(boolCatchNode,
                        typed_parameter("e", 1, "bool", 4, 64, 65, 67, 71));
    add_catch_parameter(intCatchNode,
                        typed_parameter("e", 1, "int", 3, 84, 85, 87, 90));
    add_catch_parameter(stringCatchNode,
                        typed_parameter("e", 1, "string", 6, 104, 105, 107, 113));
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

static void test_cfg_merges_multi_variable_branch_assignments_for_later_typed_catch_matching(
        void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *firstDeclaration = typed_variable_declaration(
            "first",
            5,
            "int",
            3,
            integer_literal(1, 18, 19),
            8,
            20);
    SZrAstNode *secondDeclaration = typed_variable_declaration(
            "second",
            6,
            "int",
            3,
            integer_literal(2, 40, 41),
            24,
            42);
    SZrAstNode *thenFirstAssignment = expression_statement(
            assignment_expression(identifier_node("first", 5, 54, 59),
                                  string_literal("x", 1, 62, 65),
                                  54,
                                  65),
            54,
            66);
    SZrAstNode *thenSecondAssignment = expression_statement(
            assignment_expression(identifier_node("second", 6, 68, 74),
                                  string_literal("y", 1, 77, 80),
                                  68,
                                  80),
            68,
            81);
    SZrAstNode *elseFirstAssignment = expression_statement(
            assignment_expression(identifier_node("first", 5, 90, 95),
                                  char_literal('c', 98, 101),
                                  90,
                                  101),
            90,
            102);
    SZrAstNode *elseSecondAssignment = expression_statement(
            assignment_expression(identifier_node("second", 6, 104, 110),
                                  integer_literal(3, 113, 114),
                                  104,
                                  114),
            104,
            115);
    SZrAstNode *branch = if_statement(identifier_node("flag", 4, 44, 48),
                                      block_with_two_statements(thenFirstAssignment,
                                                                thenSecondAssignment,
                                                                52,
                                                                83),
                                      block_with_two_statements(elseFirstAssignment,
                                                                elseSecondAssignment,
                                                                88,
                                                                117),
                                      43,
                                      118);
    SZrAstNode *throwStmt =
            throw_statement_with_expr(identifier_node("first", 5, 126, 131), 120, 132);
    SZrAstNode *tryBody = block_with_four_statements(
            firstDeclaration,
            secondDeclaration,
            branch,
            throwStmt,
            6,
            134);
    SZrAstNode *intCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 146, 148);
    SZrAstNode *intCatchNode = catch_clause(block_with_statement(intCatchStmt, 142, 150));
    SZrAstNode *stringCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 166, 168);
    SZrAstNode *stringCatchNode =
            catch_clause(block_with_statement(stringCatchStmt, 162, 170));
    SZrAstNode *charCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 186, 188);
    SZrAstNode *charCatchNode = catch_clause(block_with_statement(charCatchStmt, 182, 190));
    SZrAstNode *catchAllStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 202, 204);
    SZrAstNode *catchAllNode = catch_clause(block_with_statement(catchAllStmt, 198, 206));
    SZrAstNode *tryNode;
    SZrAstNode *script;
    const SZrSemanticReachabilityFact *fact;

    add_catch_parameter(intCatchNode,
                        typed_parameter("e", 1, "int", 3, 136, 137, 139, 142));
    add_catch_parameter(stringCatchNode,
                        typed_parameter("e", 1, "string", 6, 154, 155, 157, 163));
    add_catch_parameter(charCatchNode,
                        typed_parameter("e", 1, "char", 4, 174, 175, 177, 181));
    tryNode = try_statement_with_four_catches(
            tryBody,
            intCatchNode,
            stringCatchNode,
            charCatchNode,
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
test_cfg_merges_unknown_while_assignment_with_incoming_type_for_typed_catch_matching(
        void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *declaration = typed_variable_declaration(
            "value",
            5,
            "int",
            3,
            integer_literal(1, 18, 19),
            8,
            20);
    SZrAstNode *loopAssignment = expression_statement(
            assignment_expression(identifier_node("value", 5, 35, 40),
                                  string_literal("x", 1, 43, 46),
                                  35,
                                  46),
            35,
            47);
    SZrAstNode *loop = while_statement(identifier_node("flag", 4, 27, 31),
                                       block_with_statement(loopAssignment, 33, 49),
                                       21,
                                       50);
    SZrAstNode *throwStmt =
            throw_statement_with_expr(identifier_node("value", 5, 58, 63), 52, 64);
    SZrAstNode *tryBody = block_with_three_statements(declaration, loop, throwStmt, 6, 66);
    SZrAstNode *boolCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 80, 82);
    SZrAstNode *boolCatchNode = catch_clause(block_with_statement(boolCatchStmt, 76, 84));
    SZrAstNode *intCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 100, 102);
    SZrAstNode *intCatchNode = catch_clause(block_with_statement(intCatchStmt, 96, 104));
    SZrAstNode *stringCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 120, 122);
    SZrAstNode *stringCatchNode =
            catch_clause(block_with_statement(stringCatchStmt, 116, 124));
    SZrAstNode *catchAllStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 140, 142);
    SZrAstNode *catchAllNode = catch_clause(block_with_statement(catchAllStmt, 136, 144));
    SZrAstNode *tryNode;
    SZrAstNode *script;
    const SZrSemanticReachabilityFact *fact;

    add_catch_parameter(boolCatchNode,
                        typed_parameter("e", 1, "bool", 4, 68, 69, 71, 75));
    add_catch_parameter(intCatchNode,
                        typed_parameter("e", 1, "int", 3, 88, 89, 91, 94));
    add_catch_parameter(stringCatchNode,
                        typed_parameter("e", 1, "string", 6, 108, 109, 111, 117));
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

static void test_cfg_ignores_unreachable_assignment_after_loop_break_for_typed_catch(
        void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *declaration = typed_variable_declaration(
            "value",
            5,
            "int",
            3,
            integer_literal(1, 18, 19),
            8,
            20);
    SZrAstNode *breakStmt = break_statement(35, 41);
    SZrAstNode *unreachableAssignment = expression_statement(
            assignment_expression(identifier_node("value", 5, 43, 48),
                                  string_literal("x", 1, 51, 54),
                                  43,
                                  54),
            43,
            55);
    SZrAstNode *loop = while_statement(
            identifier_node("flag", 4, 27, 31),
            block_with_two_statements(breakStmt, unreachableAssignment, 33, 57),
            21,
            58);
    SZrAstNode *throwStmt =
            throw_statement_with_expr(identifier_node("value", 5, 66, 71), 60, 72);
    SZrAstNode *tryBody = block_with_three_statements(declaration, loop, throwStmt, 6, 74);
    SZrAstNode *boolCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 88, 90);
    SZrAstNode *boolCatchNode = catch_clause(block_with_statement(boolCatchStmt, 84, 92));
    SZrAstNode *intCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 108, 110);
    SZrAstNode *intCatchNode = catch_clause(block_with_statement(intCatchStmt, 104, 112));
    SZrAstNode *stringCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 128, 130);
    SZrAstNode *stringCatchNode =
            catch_clause(block_with_statement(stringCatchStmt, 124, 132));
    SZrAstNode *catchAllStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 148, 150);
    SZrAstNode *catchAllNode = catch_clause(block_with_statement(catchAllStmt, 144, 152));
    SZrAstNode *tryNode;
    SZrAstNode *script;
    const SZrSemanticReachabilityFact *fact;

    add_catch_parameter(boolCatchNode,
                        typed_parameter("e", 1, "bool", 4, 76, 77, 79, 83));
    add_catch_parameter(intCatchNode,
                        typed_parameter("e", 1, "int", 3, 96, 97, 99, 102));
    add_catch_parameter(stringCatchNode,
                        typed_parameter("e", 1, "string", 6, 116, 117, 119, 125));
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

    fact = reachability_fact_at(context, boolCatchStmt);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);
    TEST_ASSERT_NULL(reachability_fact_at(context, intCatchStmt));

    fact = reachability_fact_at(context, stringCatchStmt);
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
test_cfg_merges_unknown_for_assignment_with_incoming_type_for_typed_catch_matching(
        void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *declaration = typed_variable_declaration(
            "value",
            5,
            "int",
            3,
            integer_literal(1, 18, 19),
            8,
            20);
    SZrAstNode *loopAssignment = expression_statement(
            assignment_expression(identifier_node("value", 5, 36, 41),
                                  string_literal("x", 1, 44, 47),
                                  36,
                                  47),
            36,
            48);
    SZrAstNode *loop = for_statement(identifier_node("flag", 4, 28, 32),
                                     block_with_statement(loopAssignment, 34, 50),
                                     21,
                                     51);
    SZrAstNode *throwStmt =
            throw_statement_with_expr(identifier_node("value", 5, 59, 64), 53, 65);
    SZrAstNode *tryBody = block_with_three_statements(declaration, loop, throwStmt, 6, 67);
    SZrAstNode *boolCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 81, 83);
    SZrAstNode *boolCatchNode = catch_clause(block_with_statement(boolCatchStmt, 77, 85));
    SZrAstNode *intCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 101, 103);
    SZrAstNode *intCatchNode = catch_clause(block_with_statement(intCatchStmt, 97, 105));
    SZrAstNode *stringCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 121, 123);
    SZrAstNode *stringCatchNode =
            catch_clause(block_with_statement(stringCatchStmt, 117, 125));
    SZrAstNode *catchAllStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 141, 143);
    SZrAstNode *catchAllNode = catch_clause(block_with_statement(catchAllStmt, 137, 145));
    SZrAstNode *tryNode;
    SZrAstNode *script;
    const SZrSemanticReachabilityFact *fact;

    add_catch_parameter(boolCatchNode,
                        typed_parameter("e", 1, "bool", 4, 69, 70, 72, 76));
    add_catch_parameter(intCatchNode,
                        typed_parameter("e", 1, "int", 3, 89, 90, 92, 95));
    add_catch_parameter(stringCatchNode,
                        typed_parameter("e", 1, "string", 6, 109, 110, 112, 118));
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

static void test_cfg_uses_for_init_assignment_for_typed_catch_matching(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *declaration = typed_variable_declaration(
            "value",
            5,
            "int",
            3,
            integer_literal(1, 18, 19),
            8,
            20);
    SZrAstNode *initAssignment =
            assignment_expression(identifier_node("value", 5, 26, 31),
                                  string_literal("x", 1, 34, 37),
                                  26,
                                  37);
    SZrAstNode *bodyStatement =
            expression_statement(identifier_node("flag", 4, 48, 52), 48, 53);
    SZrAstNode *loop = for_statement_with_init(
            initAssignment,
            identifier_node("flag", 4, 39, 43),
            block_with_statement(bodyStatement, 46, 55),
            21,
            56);
    SZrAstNode *throwStmt =
            throw_statement_with_expr(identifier_node("value", 5, 64, 69), 58, 70);
    SZrAstNode *tryBody = block_with_three_statements(declaration, loop, throwStmt, 6, 72);
    SZrAstNode *boolCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 86, 88);
    SZrAstNode *boolCatchNode = catch_clause(block_with_statement(boolCatchStmt, 82, 90));
    SZrAstNode *intCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 106, 108);
    SZrAstNode *intCatchNode = catch_clause(block_with_statement(intCatchStmt, 102, 110));
    SZrAstNode *stringCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 126, 128);
    SZrAstNode *stringCatchNode =
            catch_clause(block_with_statement(stringCatchStmt, 122, 130));
    SZrAstNode *catchAllStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 146, 148);
    SZrAstNode *catchAllNode = catch_clause(block_with_statement(catchAllStmt, 142, 150));
    SZrAstNode *tryNode;
    SZrAstNode *script;
    const SZrSemanticReachabilityFact *fact;

    add_catch_parameter(boolCatchNode,
                        typed_parameter("e", 1, "bool", 4, 74, 75, 77, 81));
    add_catch_parameter(intCatchNode,
                        typed_parameter("e", 1, "int", 3, 94, 95, 97, 100));
    add_catch_parameter(stringCatchNode,
                        typed_parameter("e", 1, "string", 6, 114, 115, 117, 123));
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

    fact = reachability_fact_at(context, boolCatchStmt);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);

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

static void test_cfg_uses_unknown_for_step_assignment_for_typed_catch_matching(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *declaration = typed_variable_declaration(
            "value",
            5,
            "int",
            3,
            integer_literal(1, 18, 19),
            8,
            20);
    SZrAstNode *loopAssignment = expression_statement(
            assignment_expression(identifier_node("value", 5, 36, 41),
                                  string_literal("x", 1, 44, 47),
                                  36,
                                  47),
            36,
            48);
    SZrAstNode *stepAssignment = expression_statement(
            assignment_expression(identifier_node("value", 5, 52, 57),
                                  char_literal('c', 60, 63),
                                  52,
                                  63),
            52,
            64);
    SZrAstNode *loop = for_statement_with_step(
            identifier_node("flag", 4, 28, 32),
            block_with_statement(loopAssignment, 34, 50),
            stepAssignment,
            21,
            65);
    SZrAstNode *throwStmt =
            throw_statement_with_expr(identifier_node("value", 5, 73, 78), 67, 79);
    SZrAstNode *tryBody = block_with_three_statements(declaration, loop, throwStmt, 6, 81);
    SZrAstNode *intCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 95, 97);
    SZrAstNode *intCatchNode = catch_clause(block_with_statement(intCatchStmt, 91, 99));
    SZrAstNode *stringCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 115, 117);
    SZrAstNode *stringCatchNode =
            catch_clause(block_with_statement(stringCatchStmt, 111, 119));
    SZrAstNode *charCatchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 135, 137);
    SZrAstNode *charCatchNode = catch_clause(block_with_statement(charCatchStmt, 131, 139));
    SZrAstNode *catchAllStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 151, 153);
    SZrAstNode *catchAllNode = catch_clause(block_with_statement(catchAllStmt, 147, 155));
    SZrAstNode *tryNode;
    SZrAstNode *script;
    const SZrSemanticReachabilityFact *fact;

    add_catch_parameter(intCatchNode,
                        typed_parameter("e", 1, "int", 3, 83, 84, 86, 89));
    add_catch_parameter(stringCatchNode,
                        typed_parameter("e", 1, "string", 6, 103, 104, 106, 112));
    add_catch_parameter(charCatchNode,
                        typed_parameter("e", 1, "char", 4, 123, 124, 126, 130));
    tryNode = try_statement_with_four_catches(
            tryBody,
            intCatchNode,
            stringCatchNode,
            charCatchNode,
            catchAllNode);
    script = script_with_statement(tryNode);

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    TEST_ASSERT_NULL(reachability_fact_at(context, intCatchStmt));

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
    RUN_TEST(test_cfg_merges_branch_assignment_types_for_typed_catch_matching);
    RUN_TEST(
            test_cfg_merges_partial_branch_assignment_with_incoming_type_for_typed_catch_matching);
    RUN_TEST(
            test_cfg_merges_multi_variable_branch_assignments_for_later_typed_catch_matching);
    RUN_TEST(
            test_cfg_merges_unknown_while_assignment_with_incoming_type_for_typed_catch_matching);
    RUN_TEST(
            test_cfg_ignores_unreachable_assignment_after_loop_break_for_typed_catch);
    RUN_TEST(
            test_cfg_merges_unknown_for_assignment_with_incoming_type_for_typed_catch_matching);
    RUN_TEST(test_cfg_uses_for_init_assignment_for_typed_catch_matching);
    RUN_TEST(test_cfg_uses_unknown_for_step_assignment_for_typed_catch_matching);
    return UNITY_END();
}
