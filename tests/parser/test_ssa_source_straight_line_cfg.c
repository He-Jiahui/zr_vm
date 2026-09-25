#include "unity.h"

#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/exec_ir_builder.h"
#include "zr_vm_parser/exec_ir_oracle.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic_ir.h"

ZR_PARSER_API void ZrParser_Compiler_PredeclareFunctionBindings(
        SZrCompilerState *cs, SZrAstNodeArray *statements);

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
            g_state, "ssa_straight_line.zr");
    SZrAstNode *ast = ZrParser_Parse(g_state, source, strlen(source), name);
    TZrSize index;
    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_CompilerState_Init(compiler, g_state);
    compiler->currentAst = ast;
    compiler->currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler->currentFunction);
    ZrParser_Compiler_PredeclareFunctionBindings(
            compiler, ast->data.script.statements);
    for (index = 0u; index < ast->data.script.statements->count; ++index) {
        SZrAstNode *statement = ast->data.script.statements->nodes[index];
        if (statement->type == ZR_AST_CLASS_DECLARATION)
            ZrParser_Compiler_CompileClassDeclaration(compiler, statement);
        else
            ZrParser_Statement_Compile(compiler, statement);
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

static const SZrSemanticIrInstruction *block_tail(
        const SZrSemanticIrFunction *function, const SZrParserCfgBlock *block) {
    TEST_ASSERT_NOT_NULL(block);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, block->instructionCount);
    return ZrParser_SemanticIr_InstructionAt(function,
            block->firstInstructionIndex + block->instructionCount - 1u);
}

static void assert_exact_coverage(const SZrSemanticIrFunction *function) {
    TZrSize instructionIndex, blockIndex;
    for (instructionIndex = 0u;
         instructionIndex < function->instructions.length; ++instructionIndex) {
        TZrUInt32 coverage = 0u;
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, instructionIndex);
        const SZrSemanticIrSourceMapEntry *source =
                (const SZrSemanticIrSourceMapEntry *)ZrCore_Array_Get(
                        (SZrArray *)&function->sourceMap, instructionIndex);
        TEST_ASSERT_EQUAL_UINT32(instructionIndex + 1u, instruction->id);
        TEST_ASSERT_NOT_NULL(source);
        TEST_ASSERT_EQUAL_UINT32(instruction->id, source->instructionId);
        for (blockIndex = 0u; blockIndex < function->cfg.blocks.length; ++blockIndex) {
            const SZrParserCfgBlock *block;
            TEST_ASSERT_TRUE(blockIndex <= UINT32_MAX);
            block = block_at(function, (TZrUInt32)blockIndex);
            if (instructionIndex >= block->firstInstructionIndex &&
                instructionIndex < block->firstInstructionIndex + block->instructionCount)
                ++coverage;
        }
        TEST_ASSERT_EQUAL_UINT32(1u, coverage);
    }
}

static void build_source(const SZrSemanticIrFunction *function,
                         SZrExecIrFunction *output) {
    SZrExecIrDiagnostic diagnostic;
    TZrUInt32 index;
    ZrCore_ExecIr_FunctionInit(output);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(function, ZR_NULL, output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    for (index = 0u; index < output->instructionCount; ++index) {
        TZrExecIrSourceId sourceId = output->instructions[index].sourceId;
        TEST_ASSERT_GREATER_THAN_UINT32(0u, sourceId);
        TEST_ASSERT_TRUE(sourceId <= function->instructions.length);
    }
}

static void assert_implicit_exit(SZrCompilerState *compiler) {
    const SZrSemanticIrFunction *function;
    const SZrParserCfgBlock *entry, *exitBlock;
    const SZrParserCfgEdge *edge;
    const SZrSemanticIrInstruction *jump, *returnInstruction;
    SZrExecIrFunction output;
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(compiler));
    TEST_ASSERT_TRUE(compiler->preSemanticIrCfgActive);
    function = ZrParser_Compiler_PreSemanticIr(compiler);
    TEST_ASSERT_EQUAL_UINT32(2u, function->cfg.blocks.length);
    entry = block_at(function, function->cfg.entryBlockId);
    exitBlock = block_at(function, function->cfg.exitBlockId);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_BLOCK_ENTRY, entry->kind);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_BLOCK_EXIT, exitBlock->kind);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_TERMINATOR_BRANCH, entry->terminatorKind);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_TERMINATOR_RETURN, exitBlock->terminatorKind);
    TEST_ASSERT_EQUAL_UINT32(1u, entry->successorCount);
    TEST_ASSERT_EQUAL_UINT32(0u, exitBlock->successorCount);
    edge = ZrParser_Cfg_BlockEdgeAt(entry, 0u);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_EDGE_NORMAL, edge->kind);
    TEST_ASSERT_EQUAL_UINT32(exitBlock->id, edge->toBlockId);
    jump = block_tail(function, entry);
    returnInstruction = block_tail(function, exitBlock);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_IR_BRANCH, jump->opcode);
    TEST_ASSERT_EQUAL_UINT32(exitBlock->id, jump->targetBlockId);
    TEST_ASSERT_EQUAL_UINT32(0u, jump->operandCount);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_IR_RETURN, returnInstruction->opcode);
    TEST_ASSERT_EQUAL_UINT32(0u, returnInstruction->operandCount);
    assert_exact_coverage(function);
    build_source(function, &output);
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_empty_source_finalizes_and_oracle_returns(void) {
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(&compiler, "");
    SZrExecIrFunction output;
    SZrExecIrOracleInput input = {0};
    SZrExecIrOracleExecutionResult result;
    SZrExecIrDiagnostic diagnostic;
    assert_implicit_exit(&compiler);
    TEST_ASSERT_EQUAL_UINT32(2u, compiler.preSemanticIr.instructions.length);
    build_source(&compiler.preSemanticIr, &output);
    input.function = &output;
    ZrCore_ExecIr_OracleResultInit(&result);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Interpret(&input, &result, &diagnostic));
    TEST_ASSERT_TRUE(result.returned);
    TEST_ASSERT_FALSE(result.terminatedByThrow);
    TEST_ASSERT_EQUAL_INT(ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED, result.returnValue.kind);
    ZrCore_ExecIr_OracleResultFree(&result);
    ZrCore_ExecIr_FreeFunction(&output);
    free_source(&compiler, ast);
}

static void test_scalar_init_load_assignment_preserves_prefix(void) {
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(&compiler,
            "var value: int = 7;\nvar copy: int = value;\nvalue = 9;\nvalue;\n");
    SZrSemanticIrInstruction prefix[64];
    TZrSize count = compiler.preSemanticIr.instructions.length, index;
    TZrSize execInstructionCount = compiler.instructionCount;
    TZrSize execInstructionLength = compiler.instructions.length;
    TZrUInt32 loads = 0u, stores = 0u;
    TEST_ASSERT_TRUE(count < ZR_ARRAY_COUNT(prefix));
    for (index = 0u; index < count; ++index) {
        prefix[index] = *ZrParser_SemanticIr_InstructionAt(&compiler.preSemanticIr, index);
        if (prefix[index].opcode == ZR_SEMANTIC_IR_LOAD) ++loads;
        if (prefix[index].opcode == ZR_SEMANTIC_IR_STORE) ++stores;
    }
    TEST_ASSERT_GREATER_THAN_UINT32(0u, loads);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, stores);
    assert_implicit_exit(&compiler);
    TEST_ASSERT_EQUAL_UINT32(count + 2u, compiler.preSemanticIr.instructions.length);
    TEST_ASSERT_EQUAL_UINT64(execInstructionCount, compiler.instructionCount);
    TEST_ASSERT_EQUAL_UINT64(execInstructionLength, compiler.instructions.length);
    for (index = 0u; index < count; ++index)
        TEST_ASSERT_EQUAL_MEMORY(&prefix[index],
                ZrParser_SemanticIr_InstructionAt(&compiler.preSemanticIr, index),
                sizeof(prefix[index]));
    free_source(&compiler, ast);
}

static void test_repeated_validation_and_build_do_not_append_terminators(void) {
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(&compiler, "var value: int = 7;\nvalue;\n");
    TZrSize instructionCount, sourceCount, valueCount;
    unsigned iteration;
    assert_implicit_exit(&compiler);
    instructionCount = compiler.preSemanticIr.instructions.length;
    sourceCount = compiler.preSemanticIr.sourceMap.length;
    valueCount = compiler.preSemanticIr.values.length;
    for (iteration = 0u; iteration < 3u; ++iteration) {
        assert_implicit_exit(&compiler);
        TEST_ASSERT_EQUAL_UINT32(instructionCount, compiler.preSemanticIr.instructions.length);
        TEST_ASSERT_EQUAL_UINT32(sourceCount, compiler.preSemanticIr.sourceMap.length);
        TEST_ASSERT_EQUAL_UINT32(valueCount, compiler.preSemanticIr.values.length);
    }
    free_source(&compiler, ast);
}

static void assert_explicit_terminal(const char *source,
                                     EZrSemanticIrOpcode opcode) {
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(&compiler, source);
    const SZrSemanticIrFunction *function = &compiler.preSemanticIr;
    const SZrSemanticIrInstruction *terminal;
    const TZrValueId *operand;
    const SZrSemanticIrValue *value;
    SZrExecIrFunction output;
    TZrSize instructionCount = function->instructions.length;
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgActive);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    TEST_ASSERT_EQUAL_UINT32(instructionCount, function->instructions.length);
    TEST_ASSERT_EQUAL_UINT32(1u, function->cfg.blocks.length);
    terminal = block_tail(function, block_at(function, function->cfg.exitBlockId));
    TEST_ASSERT_EQUAL_INT(opcode, terminal->opcode);
    TEST_ASSERT_EQUAL_UINT32(1u, terminal->operandCount);
    TEST_ASSERT_EQUAL_INT(2, terminal->sourceRange.start.line);
    operand = (const TZrValueId *)ZrCore_Array_Get(
            (SZrArray *)&function->valueOperands, terminal->operandStart);
    value = ZrParser_SemanticIr_Value(function, *operand);
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_IR_LOAD,
            ZrParser_SemanticIr_InstructionAt(function,
                    value->definitionInstructionId - 1u)->opcode);
    assert_exact_coverage(function);
    build_source(function, &output);
    ZrCore_ExecIr_FreeFunction(&output);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    TEST_ASSERT_EQUAL_UINT32(instructionCount, function->instructions.length);
    free_source(&compiler, ast);
}

static void test_explicit_return_keeps_value_and_unreachable_tail_isolated(void) {
    assert_explicit_terminal("var value: int = 7;\nreturn value;\nvalue + 2;\n",
            ZR_SEMANTIC_IR_RETURN);
}

static void test_explicit_throw_keeps_value(void) {
    assert_explicit_terminal("var value: int = 7;\nthrow value;\n", ZR_SEMANTIC_IR_THROW);
}

static void assert_analysis_only(const char *source, TZrBool suppress,
                                TZrBool blockStartup);

static void test_child_body_does_not_block_or_pollute_parent(void) {
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(&compiler,
            "var value: int = 7;\nfn child(): int { return 1 + 2; }\nvalue;\n");
    TZrSize index;
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgActive);
    for (index = 0u; index < compiler.preSemanticIr.instructions.length; ++index)
        TEST_ASSERT_NOT_EQUAL(2,
                ZrParser_SemanticIr_InstructionAt(&compiler.preSemanticIr, index)->sourceRange.start.line);
    free_source(&compiler, ast);
    assert_analysis_only(
            "var value: int = 7;\nfn child(): int { return 1 + 2; }\nvalue;\n",
            ZR_FALSE, ZR_FALSE);
}

typedef struct SZrStraightLineOracleMemory {
    const SZrSemanticIrFunction *semantic;
    SZrExecIrOracleValue places[64];
    TZrUInt32 stores;
} SZrStraightLineOracleMemory;

static TZrBool source_place_provider(
        void *userData, const SZrExecIrInstruction *instruction,
        const SZrExecIrOracleValue *operands, TZrUInt32 operandCount,
        SZrExecIrOracleValue *result) {
    SZrStraightLineOracleMemory *memory = (SZrStraightLineOracleMemory *)userData;
    const SZrSemanticIrInstruction *source = ZrParser_SemanticIr_InstructionAt(
            memory->semantic, instruction->sourceId - 1u);
    (void)operands;
    if (source == ZR_NULL || source->opcode != ZR_SEMANTIC_IR_PLACE_BASE ||
        operandCount != 1u || source->placeId >= ZR_ARRAY_COUNT(memory->places))
        return ZR_FALSE;
    result->kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    result->as.signedInteger = source->placeId;
    return ZR_TRUE;
}

static TZrBool source_memory_provider(
        void *userData, const SZrExecIrInstruction *instruction,
        EZrExecIrOracleMemoryOperation operation,
        const SZrExecIrOracleValue *operands, TZrUInt32 operandCount,
        SZrExecIrOracleValue *result) {
    SZrStraightLineOracleMemory *memory = (SZrStraightLineOracleMemory *)userData;
    TZrInt64 address;
    (void)instruction;
    if (operandCount == 0u || operands[0].kind != ZR_EXEC_IR_ORACLE_VALUE_SIGNED)
        return ZR_FALSE;
    address = operands[0].as.signedInteger;
    if (address < 1 || address >= (TZrInt64)ZR_ARRAY_COUNT(memory->places)) return ZR_FALSE;
    if (operation == ZR_EXEC_IR_ORACLE_MEMORY_LOAD && operandCount == 1u && result != ZR_NULL)
        *result = memory->places[address];
    else if (operation == ZR_EXEC_IR_ORACLE_MEMORY_STORE && operandCount == 2u) {
        memory->places[address] = operands[1];
        ++memory->stores;
    } else return ZR_FALSE;
    return ZR_TRUE;
}

static void test_source_assignment_oracle_returns_second_constant(void) {
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(&compiler,
            "var value: int = 7;\nvalue = 9;\nreturn value;\n");
    SZrExecIrFunction output;
    SZrExecIrOracleInput input = {0};
    SZrExecIrOracleExecutionResult result;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrOracleValue constants[16] = {{0}};
    SZrExecIrOracleValue initial[128] = {{0}};
    SZrStraightLineOracleMemory memory = {0};
    TZrSize index;
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    TEST_ASSERT_TRUE(compiler.constants.length <= ZR_ARRAY_COUNT(constants));
    for (index = 0u; index < compiler.constants.length; ++index) {
        const SZrTypeValue *constant = (const SZrTypeValue *)ZrCore_Array_Get(
                &compiler.constants, index);
        if (ZR_VALUE_IS_TYPE_SIGNED_INT(constant->type)) {
            constants[index].kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
            constants[index].as.signedInteger = constant->value.nativeObject.nativeInt64;
        } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(constant->type)) {
            constants[index].kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
            constants[index].as.signedInteger = (TZrInt64)constant->value.nativeObject.nativeUInt64;
        }
    }
    build_source(&compiler.preSemanticIr, &output);
    TEST_ASSERT_TRUE(output.valueCount <= ZR_ARRAY_COUNT(initial));
    for (index = 0u; index < output.valueCount; ++index) {
        /* The source builder exposes place provenance as external tokens;
         * the place provider resolves only these, never computed values. */
        if (output.values[index].definitionInstructionId == ZR_EXEC_IR_INSTRUCTION_ID_INVALID) {
            TZrUInt32 instructionIndex;
            TZrBool placeProvenance = ZR_FALSE;
            for (instructionIndex = 0u; instructionIndex < output.instructionCount;
                 ++instructionIndex) {
                const SZrExecIrInstruction *instruction = &output.instructions[instructionIndex];
                if (instruction->opcode == ZR_EXEC_IR_OPCODE_PLACE_BASE &&
                    instruction->operandRange.count == 1u &&
                    output.operandPool[instruction->operandRange.start] == index + 1u)
                    placeProvenance = ZR_TRUE;
            }
            TEST_ASSERT_TRUE(placeProvenance);
            initial[index].kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
            initial[index].as.signedInteger = (TZrInt64)index + 1;
        }
    }
    memory.semantic = &compiler.preSemanticIr;
    input.function = &output;
    input.constants = constants;
    input.constantCount = (TZrUInt32)compiler.constants.length;
    input.initialValues = initial;
    input.initialValueCount = output.valueCount;
    input.place = source_place_provider;
    input.placeUserData = &memory;
    input.memory = source_memory_provider;
    input.memoryUserData = &memory;
    ZrCore_ExecIr_OracleResultInit(&result);
    {
        TZrBool interpreted = ZrParser_ExecIr_Interpret(&input, &result, &diagnostic);
        if (!interpreted)
            fprintf(stderr, "source oracle diagnostic=%u instruction=%u source=%u expected=%u actual=%u\n",
                    diagnostic.code, diagnostic.instructionId, diagnostic.sourceId,
                    diagnostic.expectedVersion, diagnostic.actualVersion);
        TEST_ASSERT_TRUE(interpreted);
    }
    TEST_ASSERT_TRUE(result.returned);
    TEST_ASSERT_FALSE(result.terminatedByThrow);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, memory.stores);
    TEST_ASSERT_EQUAL_INT(ZR_EXEC_IR_ORACLE_VALUE_SIGNED, result.returnValue.kind);
    TEST_ASSERT_EQUAL_INT64(9, result.returnValue.as.signedInteger);
    ZrCore_ExecIr_OracleResultFree(&result);
    ZrCore_ExecIr_FreeFunction(&output);
    free_source(&compiler, ast);
}

static void assert_analysis_only(const char *source, TZrBool suppress,
                                TZrBool blockStartup) {
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(&compiler, source);
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    TZrSize count = compiler.preSemanticIr.instructions.length;
    unsigned iteration;
    if (suppress) compiler.preSemanticIrCfgStartupSuppressed = ZR_TRUE;
    if (blockStartup) compiler.preSemanticIrCfgStartupBlocked = ZR_TRUE;
    for (iteration = 0u; iteration < 2u; ++iteration) {
        TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
        TEST_ASSERT_FALSE(compiler.preSemanticIrCfgActive);
        TEST_ASSERT_EQUAL_UINT32(count, compiler.preSemanticIr.instructions.length);
        TEST_ASSERT_EQUAL_UINT32(2u, compiler.preSemanticIr.cfg.blocks.length);
        ZrCore_ExecIr_FunctionInit(&output);
        TEST_ASSERT_FALSE(ZrParser_ExecIr_Build(
                &compiler.preSemanticIr, ZR_NULL, &output, &diagnostic));
        TEST_ASSERT_EQUAL_INT(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, diagnostic.code);
        TEST_ASSERT_EQUAL_UINT32(1u, diagnostic.blockId);
        TEST_ASSERT_EQUAL_UINT32(count, diagnostic.instructionId);
        TEST_ASSERT_EQUAL_UINT32(count, diagnostic.sourceId);
        TEST_ASSERT_EQUAL_UINT32(ZR_PARSER_CFG_EDGE_SWITCH_DEFAULT, diagnostic.expectedVersion);
        TEST_ASSERT_EQUAL_UINT32(ZR_PARSER_CFG_EDGE_RETURN, diagnostic.actualVersion);
        ZrCore_ExecIr_FreeFunction(&output);
    }
    free_source(&compiler, ast);
}

static void test_binary_initializer_without_semantic_producer_stays_analysis_only(void) {
    assert_analysis_only("var value: int = 1 + 2;\nvalue;\n", ZR_FALSE, ZR_FALSE);
}
static void test_discarded_binary_without_semantic_producer_stays_analysis_only(void) {
    assert_analysis_only("var value: int = 1;\nvalue + 2;\n", ZR_FALSE, ZR_FALSE);
}
static void test_compound_assignment_without_semantic_producer_stays_analysis_only(void) {
    assert_analysis_only("var value: int = 1;\nvalue += 2;\n", ZR_FALSE, ZR_FALSE);
}
static void test_unary_without_semantic_producer_stays_analysis_only(void) {
    assert_analysis_only("var value: int = 1;\n-value;\n", ZR_FALSE, ZR_FALSE);
}
static void test_uninitialized_local_stays_analysis_only(void) {
    assert_analysis_only("var value: int;\nvalue;\n", ZR_FALSE, ZR_FALSE);
}
static void test_legacy_global_identifier_read_stays_analysis_only(void) {
    assert_analysis_only("zr;\n", ZR_FALSE, ZR_FALSE);
}
static void test_earlier_global_read_is_not_certified_by_later_local(void) {
    assert_analysis_only("zr;\nvar zr: int = 7;\nzr;\n", ZR_FALSE, ZR_FALSE);
}
static void test_legacy_child_function_read_stays_analysis_only(void) {
    assert_analysis_only("fn child(): int { return 7; }\nchild;\n", ZR_FALSE, ZR_FALSE);
}
static void test_named_child_declaration_stays_analysis_only(void) {
    assert_analysis_only("fn child(): int { return 7; }\n", ZR_FALSE, ZR_FALSE);
}
static void test_legacy_global_assignment_stays_analysis_only(void) {
    assert_analysis_only("missing = 7;\n", ZR_FALSE, ZR_FALSE);
}
static void test_resource_constructor_requires_canonical_seed(void) {
    assert_analysis_only("resource class Value {}\nvar owner = own Value();\n",
            ZR_FALSE, ZR_FALSE);
}
static void test_numeric_local_initialization_conversion_stays_analysis_only(void) {
    assert_analysis_only("var value: float = 7;\nvalue;\n", ZR_FALSE, ZR_FALSE);
}
static void test_numeric_local_assignment_conversion_stays_analysis_only(void) {
    assert_analysis_only("var value: float = 1.0;\nvalue = 7;\nvalue;\n",
            ZR_FALSE, ZR_FALSE);
}
static void test_local_identifier_shadows_legacy_global(void) {
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(&compiler, "var zr: int = 7;\nzr;\n");
    assert_implicit_exit(&compiler);
    free_source(&compiler, ast);
}
static void test_callable_default_stays_analysis_only(void) {
    assert_analysis_only("var value: int = 7;\nfn child(arg: int = 1 + 2): int { return arg; }\nvalue;\n",
            ZR_FALSE, ZR_FALSE);
}
static void test_class_static_initializer_stays_analysis_only(void) {
    assert_analysis_only("class Value { pub static var value: int = 1 + 2; }\nvar seed: int = 7;\nseed;\n",
            ZR_FALSE, ZR_FALSE);
}
static void test_supported_literal_families(void) {
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(&compiler,
            "true;\n7;\n1.5;\n\"text\";\n'a';\nnull;\n(7);\n");
    assert_implicit_exit(&compiler);
    free_source(&compiler, ast);
}
static void test_block_expression_stays_analysis_only(void) {
    /* At script level braces are an expression, not a statement scope.
     * Its expression-result/lifetime producers are outside this subset. */
    assert_analysis_only("var value: int = 7;\n{ var other: int = value; other; };\n",
            ZR_FALSE, ZR_FALSE);
}
static void test_unsupported_control_flow_stays_analysis_only(void) {
    assert_analysis_only("var value: int = 1;\nwhile (value < 2) { value += 1; }\n",
            ZR_FALSE, ZR_FALSE);
}
static void test_suppressed_startup_is_not_promoted(void) {
    assert_analysis_only("var value: int = 1;\n", ZR_TRUE, ZR_FALSE);
}
static void test_blocked_startup_is_not_promoted(void) {
    assert_analysis_only("var value: int = 1;\n", ZR_FALSE, ZR_TRUE);
}

static void test_full_source_compile_keeps_execbc_artifact(void) {
    const char *source = "var value: int = 7;\nvalue = 9;\nvalue;\n";
    SZrString *name = ZrCore_String_CreateFromNative(g_state, "ssa_full_straight_line.zr");
    SZrFunction *function = ZrParser_Source_Compile(g_state, source, strlen(source), name);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, function->instructionsLength);
    ZrCore_Function_Free(g_state, function);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_empty_source_finalizes_and_oracle_returns);
    RUN_TEST(test_scalar_init_load_assignment_preserves_prefix);
    RUN_TEST(test_repeated_validation_and_build_do_not_append_terminators);
    RUN_TEST(test_explicit_return_keeps_value_and_unreachable_tail_isolated);
    RUN_TEST(test_explicit_throw_keeps_value);
    RUN_TEST(test_child_body_does_not_block_or_pollute_parent);
    RUN_TEST(test_source_assignment_oracle_returns_second_constant);
    RUN_TEST(test_binary_initializer_without_semantic_producer_stays_analysis_only);
    RUN_TEST(test_discarded_binary_without_semantic_producer_stays_analysis_only);
    RUN_TEST(test_compound_assignment_without_semantic_producer_stays_analysis_only);
    RUN_TEST(test_unary_without_semantic_producer_stays_analysis_only);
    RUN_TEST(test_uninitialized_local_stays_analysis_only);
    RUN_TEST(test_legacy_global_identifier_read_stays_analysis_only);
    RUN_TEST(test_earlier_global_read_is_not_certified_by_later_local);
    RUN_TEST(test_legacy_child_function_read_stays_analysis_only);
    RUN_TEST(test_named_child_declaration_stays_analysis_only);
    RUN_TEST(test_legacy_global_assignment_stays_analysis_only);
    RUN_TEST(test_resource_constructor_requires_canonical_seed);
    RUN_TEST(test_numeric_local_initialization_conversion_stays_analysis_only);
    RUN_TEST(test_numeric_local_assignment_conversion_stays_analysis_only);
    RUN_TEST(test_local_identifier_shadows_legacy_global);
    RUN_TEST(test_callable_default_stays_analysis_only);
    RUN_TEST(test_class_static_initializer_stays_analysis_only);
    RUN_TEST(test_supported_literal_families);
    RUN_TEST(test_block_expression_stays_analysis_only);
    RUN_TEST(test_unsupported_control_flow_stays_analysis_only);
    RUN_TEST(test_suppressed_startup_is_not_promoted);
    RUN_TEST(test_blocked_startup_is_not_promoted);
    RUN_TEST(test_full_source_compile_keeps_execbc_artifact);
    return UNITY_END();
}
