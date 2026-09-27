#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/exec_ir_interpreter.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/exec_ir_builder.h"
#include "zr_vm_parser/exec_ir_oracle.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic_ir.h"

static SZrState *g_state;

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    if (g_state != ZR_NULL) ZrTests_Runtime_State_Destroy(g_state);
    g_state = ZR_NULL;
}

static SZrAstNode *compile_source(SZrCompilerState *compiler,
                                  const char *source) {
    SZrString *name = ZrCore_String_CreateFromNative(
            g_state, "ssa_while_short_circuit.zr");
    SZrAstNode *ast = ZrParser_Parse(g_state, source, strlen(source), name);
    TZrSize index;
    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_CompilerState_Init(compiler, g_state);
    compiler->currentAst = ast;
    compiler->currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler->currentFunction);
    for (index = 0u; index < ast->data.script.statements->count; ++index) {
        ZrParser_Statement_Compile(
                compiler, ast->data.script.statements->nodes[index]);
        TEST_ASSERT_FALSE_MESSAGE(compiler->hasError, compiler->errorMessage);
    }
    return ast;
}

static void free_source(SZrCompilerState *compiler, SZrAstNode *ast) {
    ZrCore_Function_Free(g_state, compiler->currentFunction);
    compiler->currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static const SZrParserCfgBlock *block_at(
        const SZrSemanticIrFunction *function, TZrUInt32 id) {
    return (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, id);
}

static void assert_edge(const SZrParserCfgBlock *block, TZrUInt32 ordinal,
                        TZrUInt32 target, EZrParserCfgEdgeKind kind) {
    const SZrParserCfgEdge *edge = ZrParser_Cfg_BlockEdgeAt(block, ordinal);
    TEST_ASSERT_NOT_NULL(edge);
    TEST_ASSERT_EQUAL_UINT32(target, edge->toBlockId);
    TEST_ASSERT_EQUAL_INT(kind, edge->kind);
}

static void assert_loop_with_logical_condition(const char *source,
                                               TZrBool isAnd) {
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(&compiler, source);
    const SZrSemanticIrFunction *function;
    const SZrParserCfgBlock *entry, *header, *body, *join;
    const SZrParserCfgBlock *right, *logicalJoin;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    TZrSize index;
    TZrUInt32 rightStores = 0u;

    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgActive);
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(7u, function->cfg.blocks.length);
    entry = block_at(function, 0u);
    header = block_at(function, 1u);
    body = block_at(function, 2u);
    join = block_at(function, 3u);
    right = block_at(function, 4u);
    logicalJoin = block_at(function, 5u);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_NOT_NULL(header);
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_NOT_NULL(join);
    TEST_ASSERT_NOT_NULL(right);
    TEST_ASSERT_NOT_NULL(logicalJoin);
    assert_edge(entry, 0u, 1u, ZR_PARSER_CFG_EDGE_NORMAL);
    TEST_ASSERT_EQUAL_UINT32(2u, header->predecessorCount);
    TEST_ASSERT_EQUAL_UINT32(2u, header->successorCount);
    assert_edge(header, 0u, isAnd ? 4u : 5u,
                ZR_PARSER_CFG_EDGE_TRUE_BRANCH);
    assert_edge(header, 1u, isAnd ? 5u : 4u,
                ZR_PARSER_CFG_EDGE_FALSE_BRANCH);
    assert_edge(right, 0u, 5u, ZR_PARSER_CFG_EDGE_NORMAL);
    TEST_ASSERT_EQUAL_UINT32(2u, logicalJoin->predecessorCount);
    assert_edge(logicalJoin, 0u, 2u, ZR_PARSER_CFG_EDGE_TRUE_BRANCH);
    assert_edge(logicalJoin, 1u, 3u, ZR_PARSER_CFG_EDGE_FALSE_BRANCH);
    assert_edge(body, 0u, 1u, ZR_PARSER_CFG_EDGE_NORMAL);
    assert_edge(join, 0u, 6u, ZR_PARSER_CFG_EDGE_NORMAL);
    for (index = 0u; index < right->instructionCount; ++index) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(
                        function, right->firstInstructionIndex + index);
        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == ZR_SEMANTIC_IR_STORE) ++rightStores;
    }
    TEST_ASSERT_GREATER_THAN_UINT32(0u, rightStores);
    ZrCore_ExecIr_FunctionInit(&output);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    ZrCore_ExecIr_FreeFunction(&output);
    free_source(&compiler, ast);
}

static void test_while_and_keeps_conditional_right_side_and_backedge(void) {
    assert_loop_with_logical_condition(
            "var flag: bool = true;\n"
            "var side: bool = false;\n"
            "while (flag && (side = true)) { flag = false; }\n"
            "var after: bool = side;\n", ZR_TRUE);
}

static void test_while_or_keeps_conditional_right_side_and_backedge(void) {
    assert_loop_with_logical_condition(
            "var flag: bool = true;\n"
            "var side: bool = false;\n"
            "while (flag || (side = true)) { flag = false; }\n"
            "var after: bool = side;\n", ZR_FALSE);
}

static void test_nested_while_conditions_keep_both_rhs_branches(void) {
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(&compiler,
            "var flag: bool = true;\nvar middle: bool = false;\n"
            "var side: bool = false;\n"
            "while ((flag && (middle = true)) && (side = true)) { flag = false; }\n"
            "return side;\n");
    const SZrSemanticIrFunction *function;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgActive);
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_EQUAL_UINT32(8u, function->cfg.blocks.length);
    assert_edge(block_at(function, 1u), 0u, 4u,
                ZR_PARSER_CFG_EDGE_TRUE_BRANCH);
    assert_edge(block_at(function, 4u), 0u, 5u,
                ZR_PARSER_CFG_EDGE_NORMAL);
    assert_edge(block_at(function, 5u), 0u, 6u,
                ZR_PARSER_CFG_EDGE_TRUE_BRANCH);
    assert_edge(block_at(function, 6u), 0u, 7u,
                ZR_PARSER_CFG_EDGE_NORMAL);
    assert_edge(block_at(function, 7u), 0u, 2u,
                ZR_PARSER_CFG_EDGE_TRUE_BRANCH);
    assert_edge(block_at(function, 2u), 0u, 1u,
                ZR_PARSER_CFG_EDGE_NORMAL);
    ZrCore_ExecIr_FunctionInit(&output);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    ZrCore_ExecIr_FreeFunction(&output);
    free_source(&compiler, ast);
}

static void test_unmodeled_while_rhs_remains_analysis_only(void) {
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(&compiler,
            "var flag: bool = true;\n"
            "while (flag && (1 == 1)) { flag = false; }\n");
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgActive);
    TEST_ASSERT_EQUAL_UINT32(2u, compiler.preSemanticIr.cfg.blocks.length);
    ZrCore_ExecIr_FunctionInit(&output);
    TEST_ASSERT_FALSE(ZrParser_ExecIr_Build(
            &compiler.preSemanticIr, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, diagnostic.code);
    ZrCore_ExecIr_FreeFunction(&output);
    free_source(&compiler, ast);
}

static void test_nested_unmodeled_rhs_remains_analysis_only(void) {
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(&compiler,
            "var flag: bool = true;\nvar side: bool = false;\n"
            "while (flag && (side && (1 == 1))) { flag = false; }\n");
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgActive);
    ZrCore_ExecIr_FunctionInit(&output);
    TEST_ASSERT_FALSE(ZrParser_ExecIr_Build(
            &compiler.preSemanticIr, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, diagnostic.code);
    ZrCore_ExecIr_FreeFunction(&output);
    free_source(&compiler, ast);
}

typedef struct SZrLoopOracleMemory {
    const SZrSemanticIrFunction *source;
    SZrExecIrOracleValue places[64];
    TZrUInt32 rightFirstInstruction;
    TZrUInt32 rightInstructionCount;
    TZrInt32 rhsAssignmentColumn;
    TZrInt32 rhsAssignmentEndColumn;
    TZrUInt32 conditionStores;
} SZrLoopOracleMemory;

static TZrBool loop_place_provider(
        void *userData, const SZrExecIrInstruction *instruction,
        const SZrExecIrOracleValue *operands, TZrUInt32 operandCount,
        SZrExecIrOracleValue *result) {
    SZrLoopOracleMemory *memory = (SZrLoopOracleMemory *)userData;
    const SZrSemanticIrInstruction *source = ZrParser_SemanticIr_InstructionAt(
            memory->source, instruction->sourceId - 1u);
    (void)operands;
    if (source == ZR_NULL || source->opcode != ZR_SEMANTIC_IR_PLACE_BASE ||
        operandCount != 1u || source->placeId >= ZR_ARRAY_COUNT(memory->places))
        return ZR_FALSE;
    result->kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    result->as.signedInteger = source->placeId;
    return ZR_TRUE;
}

static TZrBool loop_memory_provider(
        void *userData, const SZrExecIrInstruction *instruction,
        EZrExecIrOracleMemoryOperation operation,
        const SZrExecIrOracleValue *operands, TZrUInt32 operandCount,
        SZrExecIrOracleValue *result) {
    SZrLoopOracleMemory *memory = (SZrLoopOracleMemory *)userData;
    TZrInt64 address;
    if (operandCount == 0u || operands[0].kind != ZR_EXEC_IR_ORACLE_VALUE_SIGNED)
        return ZR_FALSE;
    address = operands[0].as.signedInteger;
    if (address < 1 || address >= (TZrInt64)ZR_ARRAY_COUNT(memory->places))
        return ZR_FALSE;
    if (operation == ZR_EXEC_IR_ORACLE_MEMORY_LOAD && operandCount == 1u && result != ZR_NULL) {
        *result = memory->places[address];
    } else if (operation == ZR_EXEC_IR_ORACLE_MEMORY_STORE && operandCount == 2u) {
        const SZrSemanticIrInstruction *source = ZrParser_SemanticIr_InstructionAt(
                memory->source, instruction->sourceId - 1u);
        if (source == ZR_NULL) return ZR_FALSE;
        memory->places[address] = operands[1];
        if (instruction->sourceId - 1u >= memory->rightFirstInstruction &&
            instruction->sourceId - 1u < memory->rightFirstInstruction + memory->rightInstructionCount &&
            source->sourceRange.start.column >= memory->rhsAssignmentColumn &&
            source->sourceRange.start.column <= memory->rhsAssignmentEndColumn)
            ++memory->conditionStores;
    } else {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static void assert_loop_execution_at(const char *source, TZrBool expectedResult,
                                     TZrUInt32 expectedConditionStores,
                                     TZrUInt32 trackedBlock,
                                     TZrBool trackNestedLeft,
                                     TZrBool trackNestedRight) {
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(&compiler, source);
    SZrExecIrFunction output;
    SZrExecIrOracleInput input = {0};
    SZrExecIrOracleExecutionResult result;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrOracleValue constants[16] = {{0}};
    SZrExecIrOracleValue initial[128] = {{0}};
    SZrLoopOracleMemory memory = {0};
    const SZrAstNode *loop;
    const SZrAstNode *condition;
    const SZrParserCfgBlock *right;
    TZrSize index;

    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgActive);
    TEST_ASSERT_TRUE(compiler.constants.length <= ZR_ARRAY_COUNT(constants));
    for (index = 0u; index < compiler.constants.length; ++index) {
        const SZrTypeValue *constant = (const SZrTypeValue *)ZrCore_Array_Get(
                &compiler.constants, index);
        if (ZR_VALUE_IS_TYPE_BOOL(constant->type)) {
            constants[index].kind = ZR_EXEC_IR_ORACLE_VALUE_BOOL;
            constants[index].as.boolean = (TZrBool)constant->value.nativeObject.nativeBool;
        } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(constant->type)) {
            constants[index].kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
            constants[index].as.signedInteger = constant->value.nativeObject.nativeInt64;
        } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(constant->type)) {
            constants[index].kind = ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED;
            constants[index].as.unsignedInteger = constant->value.nativeObject.nativeUInt64;
        }
    }
    ZrCore_ExecIr_FunctionInit(&output);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            &compiler.preSemanticIr, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_TRUE(output.valueCount <= ZR_ARRAY_COUNT(initial));
    for (index = 0u; index < output.valueCount; ++index) {
        if (output.values[index].definitionInstructionId ==
            ZR_EXEC_IR_INSTRUCTION_ID_INVALID) {
            initial[index].kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
            initial[index].as.signedInteger = (TZrInt64)index + 1;
        }
    }
    memory.source = &compiler.preSemanticIr;
    right = block_at(memory.source, trackedBlock);
    TEST_ASSERT_NOT_NULL(right);
    memory.rightFirstInstruction = right->firstInstructionIndex;
    memory.rightInstructionCount = right->instructionCount;
    loop = ZR_NULL;
    for (index = 0u; index < ast->data.script.statements->count; ++index) {
        const SZrAstNode *statement = ast->data.script.statements->nodes[index];
        if (statement != ZR_NULL && statement->type == ZR_AST_WHILE_LOOP) {
            loop = statement;
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(loop);
    TEST_ASSERT_EQUAL_INT(ZR_AST_WHILE_LOOP, loop->type);
    condition = loop->data.whileLoop.cond;
    TEST_ASSERT_EQUAL_INT(ZR_AST_LOGICAL_EXPRESSION, condition->type);
    if (trackNestedLeft) condition = condition->data.logicalExpression.left;
    if (trackNestedRight) condition = condition->data.logicalExpression.right;
    TEST_ASSERT_EQUAL_INT(ZR_AST_LOGICAL_EXPRESSION, condition->type);
    memory.rhsAssignmentColumn = condition->data.logicalExpression.right->location.start.column;
    memory.rhsAssignmentEndColumn = condition->data.logicalExpression.right->location.end.column;
    TEST_ASSERT_GREATER_THAN_INT(0, memory.rhsAssignmentColumn);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(memory.rhsAssignmentColumn, memory.rhsAssignmentEndColumn);
    input.function = &output;
    input.constants = constants;
    input.constantCount = (TZrUInt32)compiler.constants.length;
    input.initialValues = initial;
    input.initialValueCount = output.valueCount;
    input.place = loop_place_provider;
    input.placeUserData = &memory;
    input.memory = loop_memory_provider;
    input.memoryUserData = &memory;
    input.maxSteps = 128u;
    ZrCore_ExecIr_OracleResultInit(&result);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Interpret(&input, &result, &diagnostic));
    TEST_ASSERT_TRUE(result.returned);
    TEST_ASSERT_EQUAL_INT(ZR_EXEC_IR_ORACLE_VALUE_BOOL, result.returnValue.kind);
    TEST_ASSERT_EQUAL(expectedResult, result.returnValue.as.boolean);
    TEST_ASSERT_EQUAL_UINT32(expectedConditionStores, memory.conditionStores);
    ZrCore_ExecIr_OracleResultFree(&result);
    ZrCore_ExecIr_FreeFunction(&output);
    free_source(&compiler, ast);
}

static void assert_loop_execution(const char *source, TZrBool expectedResult,
                                  TZrUInt32 expectedConditionStores) {
    assert_loop_execution_at(source, expectedResult,
                             expectedConditionStores, 4u, ZR_FALSE, ZR_FALSE);
}

static void test_nested_rhs_runs_only_after_its_own_left_branch(void) {
    assert_loop_execution_at(
            "var flag: bool = false;\nvar middle: bool = true;\n"
            "var side: bool = false;\n"
            "while ((flag && (middle = true)) && (side = true)) { flag = false; }\n"
            "return middle;\n", ZR_TRUE, 0u, 4u, ZR_TRUE, ZR_FALSE);
    assert_loop_execution_at(
            "var flag: bool = true;\nvar middle: bool = false;\n"
            "var side: bool = false;\n"
            "while ((flag && (middle = true)) && (side = true)) { flag = false; }\n"
            "return middle;\n", ZR_TRUE, 1u, 4u, ZR_TRUE, ZR_FALSE);
    assert_loop_execution_at(
            "var flag: bool = false;\nvar middle: bool = true;\n"
            "var side: bool = false;\n"
            "while (flag && (middle && (side = true))) { flag = false; }\n"
            "return side;\n", ZR_FALSE, 0u, 6u, ZR_FALSE, ZR_TRUE);
    assert_loop_execution_at(
            "var flag: bool = true;\nvar middle: bool = true;\n"
            "var side: bool = false;\n"
            "while (flag && (middle && (side = true))) { flag = false; }\n"
            "return side;\n", ZR_TRUE, 1u, 6u, ZR_FALSE, ZR_TRUE);
    assert_loop_execution_at(
            "var flag: bool = true;\nvar middle: bool = false;\n"
            "var side: bool = false;\n"
            "while ((flag || (middle = true)) && side) { flag = false; }\n"
            "return middle;\n", ZR_FALSE, 0u, 4u, ZR_TRUE, ZR_FALSE);
    assert_loop_execution_at(
            "var flag: bool = false;\nvar middle: bool = false;\n"
            "var side: bool = false;\n"
            "while ((flag || (middle = true)) && side) { flag = false; }\n"
            "return middle;\n", ZR_TRUE, 1u, 4u, ZR_TRUE, ZR_FALSE);
}

static void test_and_rhs_runs_only_when_left_is_true(void) {
    assert_loop_execution(
            "var flag: bool = false;\nvar side: bool = false;\n"
            "while (flag && (side = true)) {\n flag = false;\n}\nreturn side;\n",
            ZR_FALSE, 0u);
    assert_loop_execution(
            "var flag: bool = true;\nvar side: bool = false;\n"
            "while (flag && (side = true)) {\n flag = false;\n}\nreturn side;\n",
            ZR_TRUE, 1u);
}

static void test_or_rhs_runs_only_when_left_is_false(void) {
    assert_loop_execution(
            "var flag: bool = false;\nvar side: bool = true;\n"
            "while (flag || (side = false)) {\n flag = false;\n}\nreturn side;\n",
            ZR_FALSE, 1u);
    assert_loop_execution(
            "var flag: bool = true;\nvar side: bool = true;\n"
            "while (flag || (side = false)) {\n flag = false;\n}\nreturn side;\n",
            ZR_FALSE, 1u);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_while_and_keeps_conditional_right_side_and_backedge);
    RUN_TEST(test_while_or_keeps_conditional_right_side_and_backedge);
    RUN_TEST(test_nested_while_conditions_keep_both_rhs_branches);
    RUN_TEST(test_unmodeled_while_rhs_remains_analysis_only);
    RUN_TEST(test_nested_unmodeled_rhs_remains_analysis_only);
    RUN_TEST(test_and_rhs_runs_only_when_left_is_true);
    RUN_TEST(test_or_rhs_runs_only_when_left_is_false);
    RUN_TEST(test_nested_rhs_runs_only_after_its_own_left_branch);
    return UNITY_END();
}
