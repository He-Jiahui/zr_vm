#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/cfg.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic_ir.h"

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

static SZrFileRange empty_range(void) {
    SZrFileRange range;

    memset(&range, 0, sizeof(range));
    return range;
}

static SZrSemanticIrInstructionSpec instruction_spec(EZrSemanticIrOpcode opcode,
                                                     TZrPlaceId placeId,
                                                     TZrValueId valueId,
                                                     TZrValueId resultValueId,
                                                     TZrTypeId typeId) {
    SZrSemanticIrInstructionSpec spec;

    memset(&spec, 0, sizeof(spec));
    spec.opcode = opcode;
    spec.placeId = placeId;
    spec.valueId = valueId;
    spec.resultValueId = resultValueId;
    spec.typeId = typeId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = empty_range();
    return spec;
}

static TZrSemanticInstructionId emit_instruction(
        SZrSemanticIrFunction *function,
        SZrSemanticIrInstructionSpec spec) {
    TZrSemanticInstructionId instructionId =
            ZrParser_SemanticIr_Emit(function, &spec);

    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_INSTRUCTION_ID_INVALID, instructionId);
    return instructionId;
}

static void test_pre_semantic_ir_opcode_golden_covers_supported_families(void) {
    static const EZrSemanticIrOpcode opcodes[] = {
        ZR_SEMANTIC_IR_CONSTANT,
        ZR_SEMANTIC_IR_CONVERT,
        ZR_SEMANTIC_IR_PLACE_BASE,
        ZR_SEMANTIC_IR_PLACE_PROJECT,
        ZR_SEMANTIC_IR_LOAD,
        ZR_SEMANTIC_IR_STORE,
        ZR_SEMANTIC_IR_INITIALIZE,
        ZR_SEMANTIC_IR_MOVE,
        ZR_SEMANTIC_IR_COPY,
        ZR_SEMANTIC_IR_DROP,
        ZR_SEMANTIC_IR_BORROW_SHARED,
        ZR_SEMANTIC_IR_BORROW_MUT,
        ZR_SEMANTIC_IR_REBORROW,
        ZR_SEMANTIC_IR_END_LOAN,
        ZR_SEMANTIC_IR_DEREFERENCE,
        ZR_SEMANTIC_IR_CALL_TYPED,
        ZR_SEMANTIC_IR_CALL_VIRTUAL,
        ZR_SEMANTIC_IR_CALL_DYNAMIC,
        ZR_SEMANTIC_IR_CALL_META,
        ZR_SEMANTIC_IR_BRANCH,
        ZR_SEMANTIC_IR_SWITCH,
        ZR_SEMANTIC_IR_RETURN,
        ZR_SEMANTIC_IR_THROW,
        ZR_SEMANTIC_IR_SCOPE_ENTER,
        ZR_SEMANTIC_IR_SCOPE_EXIT,
        ZR_SEMANTIC_IR_CLEANUP,
        ZR_SEMANTIC_IR_VALUE_CONSTRUCT,
        ZR_SEMANTIC_IR_AGGREGATE_CONSTRUCT,
        ZR_SEMANTIC_IR_FIELD_INITIALIZE,
        ZR_SEMANTIC_IR_UNION_CONSTRUCT,
        ZR_SEMANTIC_IR_GC_NEW,
        ZR_SEMANTIC_IR_OWN_CONSTRUCT,
        ZR_SEMANTIC_IR_PROPERTY_GET,
        ZR_SEMANTIC_IR_PROPERTY_SET,
        ZR_SEMANTIC_IR_PROPERTY_REF_GET,
        ZR_SEMANTIC_IR_DESTRUCTURE_EVALUATE,
        ZR_SEMANTIC_IR_SHAPE_VALIDATE,
        ZR_SEMANTIC_IR_DESTRUCTURE_PROJECT,
        ZR_SEMANTIC_IR_DESTRUCTURE_LEAF_ASSIGN,
        ZR_SEMANTIC_IR_DESTRUCTURE_LEAF_BIND,
        ZR_SEMANTIC_IR_DESTRUCTURE_REST,
    };
    static const char expected[] =
        "1 constant type=5 place=1 value=1 result=2\n"
        "2 convert type=5 place=1 value=1 result=2\n"
        "3 place.base type=5 place=1 value=1 result=2\n"
        "4 place.project type=5 place=1 value=1 result=2\n"
        "5 load type=5 place=1 value=1 result=2\n"
        "6 store type=5 place=1 value=1 result=2\n"
        "7 initialize type=5 place=1 value=1 result=2\n"
        "8 move type=5 place=1 value=1 result=2\n"
        "9 copy type=5 place=1 value=1 result=2\n"
        "10 drop type=5 place=1 value=1 result=2\n"
        "11 borrow.shared type=5 place=1 value=1 result=2\n"
        "12 borrow.mut type=5 place=1 value=1 result=2\n"
        "13 reborrow type=5 place=1 value=1 result=2\n"
        "14 loan.end type=5 place=1 value=1 result=2\n"
        "15 dereference type=5 place=1 value=1 result=2\n"
        "16 call.typed type=5 place=1 value=1 result=2\n"
        "17 call.virtual type=5 place=1 value=1 result=2\n"
        "18 call.dynamic type=5 place=1 value=1 result=2\n"
        "19 call.meta type=5 place=1 value=1 result=2\n"
        "20 branch type=5 place=1 value=1 result=2\n"
        "21 switch type=5 place=1 value=1 result=2\n"
        "22 return type=5 place=1 value=1 result=2\n"
        "23 throw type=5 place=1 value=1 result=2\n"
        "24 scope.enter type=5 place=1 value=1 result=2\n"
        "25 scope.exit type=5 place=1 value=1 result=2\n"
        "26 cleanup type=5 place=1 value=1 result=2\n"
        "27 value.construct type=5 place=1 value=1 result=2\n"
        "28 aggregate.construct type=5 place=1 value=1 result=2\n"
        "29 field.initialize type=5 place=1 value=1 result=2\n"
        "30 union.construct type=5 place=1 value=1 result=2\n"
        "31 gc.new type=5 place=1 value=1 result=2\n"
        "32 own.construct type=5 place=1 value=1 result=2\n"
        "33 property.get type=5 place=1 value=1 result=2\n"
        "34 property.set type=5 place=1 value=1 result=2\n"
        "35 property.ref_get type=5 place=1 value=1 result=2\n"
        "36 destructure.evaluate type=5 place=1 value=1 result=2\n"
        "37 shape.validate type=5 place=1 value=1 result=2\n"
        "38 destructure.project type=5 place=1 value=1 result=2\n"
        "39 destructure.leaf_assign type=5 place=1 value=1 result=2\n"
        "40 destructure.leaf_bind type=5 place=1 value=1 result=2\n"
        "41 destructure.rest type=5 place=1 value=1 result=2\n";
    SZrSemanticIrFunction function;
    SZrParserPlaceBase base;
    SZrSemanticIrInstructionSpec spec;
    TZrPlaceId placeId;
    TZrValueId sourceValueId;
    TZrValueId resultValueId;
    char actual[8192];
    TZrSize index;

    ZrParser_SemanticIrFunction_Init(g_state, &function, 11U, 12U);
    memset(&base, 0, sizeof(base));
    base.kind = ZR_PARSER_PLACE_BASE_LOCAL;
    base.identity = 101U;
    placeId = ZrParser_SemanticIr_AddLocal(
            &function, 101U, &base, 5U, empty_range(), ZR_FALSE);
    TEST_ASSERT_NOT_EQUAL(ZR_PLACE_ID_INVALID, placeId);
    sourceValueId = ZrParser_SemanticIr_AddValue(&function, 5U, empty_range());
    resultValueId = ZrParser_SemanticIr_AddValue(&function, 5U, empty_range());
    TEST_ASSERT_NOT_EQUAL(ZR_VALUE_ID_INVALID, sourceValueId);
    TEST_ASSERT_NOT_EQUAL(ZR_VALUE_ID_INVALID, resultValueId);

    for (index = 0; index < ZR_ARRAY_COUNT(opcodes); index++) {
        spec = instruction_spec(
                opcodes[index], placeId, sourceValueId, resultValueId, 5U);
        emit_instruction(&function, spec);
    }

    TEST_ASSERT_TRUE(ZrParser_SemanticIr_FormatGolden(
            &function, actual, sizeof(actual)));
    TEST_ASSERT_EQUAL_STRING(expected, actual);
    ZrParser_SemanticIrFunction_Free(g_state, &function);
}

static TZrBool semantic_ir_has_opcode(const SZrSemanticIrFunction *function,
                                      EZrSemanticIrOpcode opcode) {
    TZrSize index;

    if (function == ZR_NULL) {
        return ZR_FALSE;
    }
    for (index = 0; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        if (instruction != ZR_NULL && instruction->opcode == opcode) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static void test_compiler_emits_validated_pre_semantic_ir_before_exec_sidecar(void) {
    static const EZrSemanticIrOpcode expectedOpcodes[] = {
        ZR_SEMANTIC_IR_PLACE_BASE,
        ZR_SEMANTIC_IR_INITIALIZE,
        ZR_SEMANTIC_IR_LOAD,
        ZR_SEMANTIC_IR_PLACE_BASE,
        ZR_SEMANTIC_IR_INITIALIZE,
        ZR_SEMANTIC_IR_LOAD,
        ZR_SEMANTIC_IR_STORE,
    };
    static const TZrChar source[] =
            "var value: int = 1;\n"
            "var copy: int = value;\n"
            "value = copy;\n";
    SZrString *sourceName =
            ZrCore_String_Create(g_state, "pre_semantic_ir.zr", 18U);
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sizeof(source) - 1U, sourceName);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *function;
    SZrFunction *compiledFunction;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);

    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.currentAst = ast;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);

    for (index = 0; index < ast->data.script.statements->count; index++) {
        ZrParser_Statement_Compile(
                &compiler, ast->data.script.statements->nodes[index]);
    }

    TEST_ASSERT_FALSE(compiler.hasError);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    TEST_ASSERT_TRUE(ZrParser_Compiler_PreSemanticIrIsValidated(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT64(
            ZR_ARRAY_COUNT(expectedOpcodes), function->instructions.length);
    for (index = 0; index < ZR_ARRAY_COUNT(expectedOpcodes); index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        TEST_ASSERT_NOT_NULL(instruction);
        TEST_ASSERT_EQUAL_INT(expectedOpcodes[index], instruction->opcode);
    }
    TEST_ASSERT_TRUE(semantic_ir_has_opcode(
            function, ZR_SEMANTIC_IR_INITIALIZE));
    TEST_ASSERT_TRUE(semantic_ir_has_opcode(function, ZR_SEMANTIC_IR_LOAD));
    TEST_ASSERT_TRUE(semantic_ir_has_opcode(function, ZR_SEMANTIC_IR_STORE));
    TEST_ASSERT_NULL(compiler.currentFunction->semIrInstructions);
    TEST_ASSERT_EQUAL_UINT32(
            0U, compiler.currentFunction->semIrInstructionLength);

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);

    compiledFunction = ZrParser_Compiler_Compile(g_state, ast);
    TEST_ASSERT_NOT_NULL(compiledFunction);
    ZrCore_Function_Free(g_state, compiledFunction);
    ZrParser_Ast_Free(g_state, ast);
}

static TZrUInt32 append_block(SZrParserCfg *cfg, EZrParserCfgBlockKind kind) {
    TZrUInt32 blockId = ZrParser_Cfg_AppendBlock(g_state, cfg, kind, ZR_NULL);

    TEST_ASSERT_NOT_EQUAL(ZR_PARSER_CFG_INVALID_BLOCK_ID, blockId);
    return blockId;
}

static void bind_block(SZrSemanticIrFunction *function,
                       SZrParserCfg *cfg,
                       TZrUInt32 blockId,
                       TZrUInt32 first,
                       TZrUInt32 count,
                       EZrParserCfgTerminatorKind terminatorKind) {
    TEST_ASSERT_TRUE(ZrParser_SemanticIr_BindBlockRange(
            function, cfg, blockId, first, count, terminatorKind));
}

static void test_flow_join_keeps_dimensions_separate_and_reports_negative_uses(void) {
    SZrSemanticIrFunction function;
    SZrSemanticFlowResult result;
    SZrParserPlaceBase base;
    SZrParserCfg *cfg;
    SZrSemanticIrInstructionSpec spec;
    const SZrSemanticBlockFlowFacts *joinFacts;
    const SZrSemanticPlaceFlowState *state;
    TZrPlaceId initPlace;
    TZrPlaceId movePlace;
    TZrPlaceId loanPlace;
    TZrPlaceId escapePlace;
    TZrValueId valueId;
    TZrRegionId regionId;
    TZrLoanId sharedLoanId;
    TZrLoanId mutableLoanId;
    TZrLoanId escapingLoanId;
    TZrUInt32 entryBlock;
    TZrUInt32 thenBlock;
    TZrUInt32 elseBlock;
    TZrUInt32 joinBlock;
    TZrUInt32 exitBlock;

    ZrParser_SemanticIrFunction_Init(g_state, &function, 21U, 22U);
    cfg = &function.cfg;
    ZrParser_SemanticFlowResult_Init(g_state, &result);

    memset(&base, 0, sizeof(base));
    base.kind = ZR_PARSER_PLACE_BASE_LOCAL;
    base.identity = 201U;
    initPlace = ZrParser_SemanticIr_AddLocal(
            &function, 201U, &base, 5U, empty_range(), ZR_FALSE);
    base.identity = 202U;
    movePlace = ZrParser_SemanticIr_AddLocal(
            &function, 202U, &base, 5U, empty_range(), ZR_FALSE);
    base.identity = 203U;
    loanPlace = ZrParser_SemanticIr_AddLocal(
            &function, 203U, &base, 5U, empty_range(), ZR_FALSE);
    base.identity = 204U;
    escapePlace = ZrParser_SemanticIr_AddLocal(
            &function, 204U, &base, 5U, empty_range(), ZR_FALSE);
    valueId = ZrParser_SemanticIr_AddValue(&function, 5U, empty_range());
    TEST_ASSERT_NOT_EQUAL(ZR_VALUE_ID_INVALID, valueId);
    regionId = ZrParser_SemanticIr_AddRegion(
            &function,
            ZR_SEMANTIC_REGION_ID_INVALID,
            ZR_SEMANTIC_ESCAPE_FUNCTION,
            empty_range());
    TEST_ASSERT_EQUAL_UINT32(1U, regionId);

    sharedLoanId = ZrParser_SemanticIr_AddLoan(
            &function,
            loanPlace,
            ZR_SEMANTIC_LOAN_SHARED,
            regionId,
            empty_range(),
            empty_range(),
            valueId);
    mutableLoanId = ZrParser_SemanticIr_AddLoan(
            &function,
            loanPlace,
            ZR_SEMANTIC_LOAN_MUTABLE,
            regionId,
            empty_range(),
            empty_range(),
            valueId);
    escapingLoanId = ZrParser_SemanticIr_AddLoan(
            &function,
            escapePlace,
            ZR_SEMANTIC_LOAN_SHARED,
            regionId,
            empty_range(),
            empty_range(),
            valueId);

    spec = instruction_spec(ZR_SEMANTIC_IR_INITIALIZE, movePlace, valueId, 0U, 5U);
    emit_instruction(&function, spec);
    spec = instruction_spec(ZR_SEMANTIC_IR_INITIALIZE, loanPlace, valueId, 0U, 5U);
    emit_instruction(&function, spec);
    spec = instruction_spec(ZR_SEMANTIC_IR_INITIALIZE, escapePlace, valueId, 0U, 5U);
    emit_instruction(&function, spec);

    spec = instruction_spec(ZR_SEMANTIC_IR_INITIALIZE, initPlace, valueId, 0U, 5U);
    emit_instruction(&function, spec);
    spec = instruction_spec(ZR_SEMANTIC_IR_MOVE, movePlace, 0U, valueId, 5U);
    emit_instruction(&function, spec);
    spec = instruction_spec(ZR_SEMANTIC_IR_BORROW_SHARED, loanPlace, 0U, valueId, 5U);
    spec.loanId = sharedLoanId;
    emit_instruction(&function, spec);
    spec = instruction_spec(ZR_SEMANTIC_IR_CALL_TYPED, escapePlace, valueId, 0U, 5U);
    spec.escape = ZR_SEMANTIC_ESCAPE_CALLER;
    emit_instruction(&function, spec);

    spec = instruction_spec(ZR_SEMANTIC_IR_LOAD, initPlace, 0U, valueId, 5U);
    emit_instruction(&function, spec);
    spec = instruction_spec(ZR_SEMANTIC_IR_COPY, movePlace, 0U, valueId, 5U);
    emit_instruction(&function, spec);
    spec = instruction_spec(ZR_SEMANTIC_IR_BORROW_MUT, loanPlace, 0U, valueId, 5U);
    spec.loanId = mutableLoanId;
    emit_instruction(&function, spec);
    spec = instruction_spec(ZR_SEMANTIC_IR_BORROW_SHARED, escapePlace, 0U, valueId, 5U);
    spec.loanId = escapingLoanId;
    emit_instruction(&function, spec);

    entryBlock = append_block(cfg, ZR_PARSER_CFG_BLOCK_ENTRY);
    thenBlock = append_block(cfg, ZR_PARSER_CFG_BLOCK_STATEMENT);
    elseBlock = append_block(cfg, ZR_PARSER_CFG_BLOCK_STATEMENT);
    joinBlock = append_block(cfg, ZR_PARSER_CFG_BLOCK_JOIN);
    exitBlock = append_block(cfg, ZR_PARSER_CFG_BLOCK_EXIT);
    cfg->entryBlockId = entryBlock;
    cfg->exitBlockId = exitBlock;
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            cfg, entryBlock, thenBlock, ZR_PARSER_CFG_EDGE_TRUE_BRANCH, ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            cfg, entryBlock, elseBlock, ZR_PARSER_CFG_EDGE_FALSE_BRANCH, ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            cfg, thenBlock, joinBlock, ZR_PARSER_CFG_EDGE_NORMAL, ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            cfg, elseBlock, joinBlock, ZR_PARSER_CFG_EDGE_NORMAL, ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            cfg, joinBlock, exitBlock, ZR_PARSER_CFG_EDGE_RETURN, ZR_NULL));

    bind_block(&function, cfg, entryBlock, 0U, 3U, ZR_PARSER_CFG_TERMINATOR_BRANCH);
    bind_block(&function, cfg, thenBlock, 3U, 4U, ZR_PARSER_CFG_TERMINATOR_NONE);
    bind_block(&function, cfg, elseBlock, 7U, 0U, ZR_PARSER_CFG_TERMINATOR_NONE);
    bind_block(&function, cfg, joinBlock, 7U, 4U, ZR_PARSER_CFG_TERMINATOR_RETURN);
    bind_block(&function, cfg, exitBlock, 11U, 0U, ZR_PARSER_CFG_TERMINATOR_EXIT);

    TEST_ASSERT_TRUE(ZrParser_SemanticIr_Validate(&function));
    TEST_ASSERT_TRUE(ZrParser_SemanticFlow_Analyze(
            g_state, &function, cfg, &result));
    joinFacts = ZrParser_SemanticFlow_BlockFacts(&result, joinBlock);
    TEST_ASSERT_NOT_NULL(joinFacts);

    state = ZrParser_SemanticFlow_PlaceState(joinFacts, initPlace, ZR_TRUE);
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_EQUAL_INT(
            ZR_SEMANTIC_INITIALIZATION_MAYBE_INITIALIZED,
            state->initialization);
    state = ZrParser_SemanticFlow_PlaceState(joinFacts, movePlace, ZR_TRUE);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_AVAILABILITY_MAYBE_MOVED, state->availability);
    state = ZrParser_SemanticFlow_PlaceState(joinFacts, loanPlace, ZR_TRUE);
    TEST_ASSERT_EQUAL_UINT64(0U, state->borrowing.sharedLoanIds.length);
    state = ZrParser_SemanticFlow_PlaceState(joinFacts, escapePlace, ZR_TRUE);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_ESCAPE_CALLER, state->escape);

    TEST_ASSERT_TRUE(ZrParser_SemanticFlow_HasDiagnostic(
            &result, ZR_SEMANTIC_FLOW_MAYBE_UNINITIALIZED, initPlace));
    TEST_ASSERT_TRUE(ZrParser_SemanticFlow_HasDiagnostic(
            &result, ZR_SEMANTIC_FLOW_MAYBE_MOVED, movePlace));
    TEST_ASSERT_TRUE(ZrParser_SemanticFlow_HasDiagnostic(
            &result, ZR_SEMANTIC_FLOW_LOAN_CONFLICT, loanPlace));
    TEST_ASSERT_TRUE(ZrParser_SemanticFlow_HasDiagnostic(
            &result, ZR_SEMANTIC_FLOW_ESCAPE_VIOLATION, escapePlace));

    ZrParser_SemanticFlowResult_Free(g_state, &result);
    ZrParser_SemanticIrFunction_Free(g_state, &function);
}

static void test_store_restores_moved_place_and_end_loan_restores_read_access(void) {
    SZrSemanticIrFunction function;
    SZrSemanticFlowResult result;
    SZrParserPlaceBase base;
    SZrSemanticIrInstructionSpec spec;
    const SZrSemanticBlockFlowFacts *entryFacts;
    const SZrSemanticPlaceFlowState *state;
    const SZrSemanticFlowDiagnostic *diagnostic;
    TZrPlaceId placeId;
    TZrValueId sourceValueId;
    TZrValueId movedValueId;
    TZrValueId borrowedValueId;
    TZrValueId firstLoadValueId;
    TZrValueId secondLoadValueId;
    TZrRegionId regionId;
    TZrLoanId loanId;
    TZrSemanticInstructionId conflictingLoadId;
    TZrUInt32 entryBlock;
    TZrUInt32 exitBlock;
    SZrParserCfg *cfg;

    ZrParser_SemanticIrFunction_Init(g_state, &function, 31U, 32U);
    ZrParser_SemanticFlowResult_Init(g_state, &result);
    cfg = &function.cfg;
    memset(&base, 0, sizeof(base));
    base.kind = ZR_PARSER_PLACE_BASE_LOCAL;
    base.identity = 301U;
    placeId = ZrParser_SemanticIr_AddLocal(
            &function, 301U, &base, 5U, empty_range(), ZR_FALSE);
    sourceValueId = ZrParser_SemanticIr_AddValue(&function, 5U, empty_range());
    movedValueId = ZrParser_SemanticIr_AddValue(&function, 5U, empty_range());
    borrowedValueId = ZrParser_SemanticIr_AddValue(&function, 5U, empty_range());
    firstLoadValueId = ZrParser_SemanticIr_AddValue(&function, 5U, empty_range());
    secondLoadValueId = ZrParser_SemanticIr_AddValue(&function, 5U, empty_range());
    regionId = ZrParser_SemanticIr_AddRegion(
            &function,
            ZR_SEMANTIC_REGION_ID_INVALID,
            ZR_SEMANTIC_ESCAPE_FUNCTION,
            empty_range());
    loanId = ZrParser_SemanticIr_AddLoan(
            &function,
            placeId,
            ZR_SEMANTIC_LOAN_MUTABLE,
            regionId,
            empty_range(),
            empty_range(),
            borrowedValueId);

    spec = instruction_spec(
            ZR_SEMANTIC_IR_INITIALIZE, placeId, sourceValueId, 0U, 5U);
    emit_instruction(&function, spec);
    spec = instruction_spec(
            ZR_SEMANTIC_IR_MOVE, placeId, 0U, movedValueId, 5U);
    emit_instruction(&function, spec);
    spec = instruction_spec(
            ZR_SEMANTIC_IR_STORE, placeId, sourceValueId, 0U, 5U);
    emit_instruction(&function, spec);
    spec = instruction_spec(
            ZR_SEMANTIC_IR_BORROW_MUT, placeId, 0U, borrowedValueId, 5U);
    spec.loanId = loanId;
    spec.regionId = regionId;
    emit_instruction(&function, spec);
    spec = instruction_spec(
            ZR_SEMANTIC_IR_LOAD, placeId, 0U, firstLoadValueId, 5U);
    conflictingLoadId = emit_instruction(&function, spec);
    spec = instruction_spec(
            ZR_SEMANTIC_IR_END_LOAN, placeId, 0U, 0U, 5U);
    spec.loanId = loanId;
    spec.regionId = regionId;
    emit_instruction(&function, spec);
    spec = instruction_spec(
            ZR_SEMANTIC_IR_LOAD, placeId, 0U, secondLoadValueId, 5U);
    emit_instruction(&function, spec);

    entryBlock = append_block(cfg, ZR_PARSER_CFG_BLOCK_ENTRY);
    exitBlock = append_block(cfg, ZR_PARSER_CFG_BLOCK_EXIT);
    cfg->entryBlockId = entryBlock;
    cfg->exitBlockId = exitBlock;
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            cfg, entryBlock, exitBlock, ZR_PARSER_CFG_EDGE_RETURN, ZR_NULL));
    bind_block(&function, cfg, entryBlock, 0U, 7U, ZR_PARSER_CFG_TERMINATOR_RETURN);
    bind_block(&function, cfg, exitBlock, 7U, 0U, ZR_PARSER_CFG_TERMINATOR_EXIT);

    TEST_ASSERT_TRUE(ZrParser_SemanticIr_Validate(&function));
    TEST_ASSERT_TRUE(ZrParser_SemanticFlow_Analyze(
            g_state, &function, cfg, &result));
    entryFacts = ZrParser_SemanticFlow_BlockFacts(&result, entryBlock);
    TEST_ASSERT_NOT_NULL(entryFacts);
    state = ZrParser_SemanticFlow_PlaceState(entryFacts, placeId, ZR_FALSE);
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_EQUAL_INT(
            ZR_SEMANTIC_INITIALIZATION_INITIALIZED, state->initialization);
    TEST_ASSERT_EQUAL_INT(
            ZR_SEMANTIC_AVAILABILITY_AVAILABLE, state->availability);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_SEMANTIC_LOAN_ID_INVALID, state->borrowing.mutableLoanId);
    TEST_ASSERT_EQUAL_UINT64(1U, result.diagnostics.length);
    diagnostic = (const SZrSemanticFlowDiagnostic *)ZrCore_Array_Get(
            &result.diagnostics, 0U);
    TEST_ASSERT_NOT_NULL(diagnostic);
    TEST_ASSERT_EQUAL_INT(
            ZR_SEMANTIC_FLOW_LOAN_CONFLICT, diagnostic->kind);
    TEST_ASSERT_EQUAL_UINT32(conflictingLoadId, diagnostic->instructionId);

    ZrParser_SemanticFlowResult_Free(g_state, &result);
    ZrParser_SemanticIrFunction_Free(g_state, &function);
}

static void test_joined_mutable_loans_end_after_conservative_path_conflict(void) {
    SZrSemanticIrFunction function;
    SZrSemanticFlowResult result;
    SZrParserPlaceBase base;
    SZrSemanticIrInstructionSpec spec;
    const SZrSemanticBlockFlowFacts *joinFacts;
    const SZrSemanticPlaceFlowState *state;
    TZrPlaceId placeId;
    TZrValueId sourceValueId;
    TZrValueId firstBorrowValueId;
    TZrValueId secondBorrowValueId;
    TZrValueId loadValueId;
    TZrRegionId regionId;
    TZrLoanId firstLoanId;
    TZrLoanId secondLoanId;
    TZrUInt32 entryBlock;
    TZrUInt32 thenBlock;
    TZrUInt32 elseBlock;
    TZrUInt32 joinBlock;
    TZrUInt32 exitBlock;
    SZrParserCfg *cfg;

    ZrParser_SemanticIrFunction_Init(g_state, &function, 41U, 42U);
    ZrParser_SemanticFlowResult_Init(g_state, &result);
    cfg = &function.cfg;
    memset(&base, 0, sizeof(base));
    base.kind = ZR_PARSER_PLACE_BASE_LOCAL;
    base.identity = 401U;
    placeId = ZrParser_SemanticIr_AddLocal(
            &function, 401U, &base, 5U, empty_range(), ZR_FALSE);
    sourceValueId = ZrParser_SemanticIr_AddValue(&function, 5U, empty_range());
    firstBorrowValueId = ZrParser_SemanticIr_AddValue(&function, 5U, empty_range());
    secondBorrowValueId = ZrParser_SemanticIr_AddValue(&function, 5U, empty_range());
    loadValueId = ZrParser_SemanticIr_AddValue(&function, 5U, empty_range());
    regionId = ZrParser_SemanticIr_AddRegion(
            &function,
            ZR_SEMANTIC_REGION_ID_INVALID,
            ZR_SEMANTIC_ESCAPE_FUNCTION,
            empty_range());
    firstLoanId = ZrParser_SemanticIr_AddLoan(
            &function,
            placeId,
            ZR_SEMANTIC_LOAN_MUTABLE,
            regionId,
            empty_range(),
            empty_range(),
            firstBorrowValueId);
    secondLoanId = ZrParser_SemanticIr_AddLoan(
            &function,
            placeId,
            ZR_SEMANTIC_LOAN_MUTABLE,
            regionId,
            empty_range(),
            empty_range(),
            secondBorrowValueId);

    spec = instruction_spec(
            ZR_SEMANTIC_IR_INITIALIZE, placeId, sourceValueId, 0U, 5U);
    emit_instruction(&function, spec);
    spec = instruction_spec(
            ZR_SEMANTIC_IR_BORROW_MUT, placeId, 0U, firstBorrowValueId, 5U);
    spec.loanId = firstLoanId;
    spec.regionId = regionId;
    emit_instruction(&function, spec);
    spec = instruction_spec(
            ZR_SEMANTIC_IR_BORROW_MUT, placeId, 0U, secondBorrowValueId, 5U);
    spec.loanId = secondLoanId;
    spec.regionId = regionId;
    emit_instruction(&function, spec);
    spec = instruction_spec(
            ZR_SEMANTIC_IR_END_LOAN, placeId, 0U, 0U, 5U);
    spec.loanId = firstLoanId;
    spec.regionId = regionId;
    emit_instruction(&function, spec);
    spec = instruction_spec(
            ZR_SEMANTIC_IR_LOAD, placeId, 0U, loadValueId, 5U);
    emit_instruction(&function, spec);

    entryBlock = append_block(cfg, ZR_PARSER_CFG_BLOCK_ENTRY);
    thenBlock = append_block(cfg, ZR_PARSER_CFG_BLOCK_STATEMENT);
    elseBlock = append_block(cfg, ZR_PARSER_CFG_BLOCK_STATEMENT);
    joinBlock = append_block(cfg, ZR_PARSER_CFG_BLOCK_JOIN);
    exitBlock = append_block(cfg, ZR_PARSER_CFG_BLOCK_EXIT);
    cfg->entryBlockId = entryBlock;
    cfg->exitBlockId = exitBlock;
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            cfg, entryBlock, thenBlock, ZR_PARSER_CFG_EDGE_TRUE_BRANCH, ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            cfg, entryBlock, elseBlock, ZR_PARSER_CFG_EDGE_FALSE_BRANCH, ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            cfg, thenBlock, joinBlock, ZR_PARSER_CFG_EDGE_NORMAL, ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            cfg, elseBlock, joinBlock, ZR_PARSER_CFG_EDGE_NORMAL, ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            cfg, joinBlock, exitBlock, ZR_PARSER_CFG_EDGE_RETURN, ZR_NULL));
    bind_block(&function, cfg, entryBlock, 0U, 1U, ZR_PARSER_CFG_TERMINATOR_BRANCH);
    bind_block(&function, cfg, thenBlock, 1U, 1U, ZR_PARSER_CFG_TERMINATOR_NONE);
    bind_block(&function, cfg, elseBlock, 2U, 1U, ZR_PARSER_CFG_TERMINATOR_NONE);
    bind_block(&function, cfg, joinBlock, 3U, 2U, ZR_PARSER_CFG_TERMINATOR_RETURN);
    bind_block(&function, cfg, exitBlock, 5U, 0U, ZR_PARSER_CFG_TERMINATOR_EXIT);

    TEST_ASSERT_TRUE(ZrParser_SemanticIr_Validate(&function));
    TEST_ASSERT_TRUE(ZrParser_SemanticFlow_Analyze(
            g_state, &function, cfg, &result));
    joinFacts = ZrParser_SemanticFlow_BlockFacts(&result, joinBlock);
    TEST_ASSERT_NOT_NULL(joinFacts);
    state = ZrParser_SemanticFlow_PlaceState(joinFacts, placeId, ZR_FALSE);
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_SEMANTIC_LOAN_ID_INVALID, state->borrowing.mutableLoanId);
    TEST_ASSERT_TRUE(ZrParser_SemanticFlow_HasDiagnostic(
            &result, ZR_SEMANTIC_FLOW_LOAN_CONFLICT, placeId));

    ZrParser_SemanticFlowResult_Free(g_state, &result);
    ZrParser_SemanticIrFunction_Free(g_state, &function);
}

static void test_compiler_ownership_lowering_records_explicit_semantic_operation(void) {
    static const TZrChar source[] =
            "var value: int = 1;\n"
            "var owner = %unique(value);\n"
            "var borrowed = %borrow(owner);\n"
            "var memberBorrowed = owner.borrow();\n";
    SZrString *sourceName =
            ZrCore_String_Create(g_state, "pre_semantic_ownership.zr", 25U);
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sizeof(source) - 1U, sourceName);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *function;
    const SZrSemanticIrInstruction *ownConstruct = ZR_NULL;
    const SZrSemanticIrInstruction *borrowShared = ZR_NULL;
    TZrSize borrowSharedCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);

    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.currentAst = ast;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);

    for (index = 0; index < ast->data.script.statements->count; index++) {
        ZrParser_Statement_Compile(
                &compiler, ast->data.script.statements->nodes[index]);
    }

    TEST_ASSERT_FALSE(compiler.hasError);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    for (index = 0; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == ZR_SEMANTIC_IR_OWN_CONSTRUCT) {
            ownConstruct = instruction;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_BORROW_SHARED) {
            borrowShared = instruction;
            borrowSharedCount++;
        }
    }
    TEST_ASSERT_NOT_NULL(ownConstruct);
    TEST_ASSERT_EQUAL_INT(
            ZR_SEMANTIC_OWNERSHIP_UNIQUE,
            ownConstruct->ownershipOperation);
    TEST_ASSERT_NOT_NULL(borrowShared);
    TEST_ASSERT_EQUAL_UINT64(2U, borrowSharedCount);
    TEST_ASSERT_NOT_EQUAL(
            ZR_SEMANTIC_LOAN_ID_INVALID, borrowShared->loanId);
    TEST_ASSERT_EQUAL_INT(
            ZR_SEMANTIC_LOAN_SHARED,
            ZrParser_SemanticIr_Loan(function, borrowShared->loanId)->access);

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_pre_semantic_ir_opcode_golden_covers_supported_families);
    RUN_TEST(test_compiler_emits_validated_pre_semantic_ir_before_exec_sidecar);
    RUN_TEST(test_flow_join_keeps_dimensions_separate_and_reports_negative_uses);
    RUN_TEST(test_store_restores_moved_place_and_end_loan_restores_read_access);
    RUN_TEST(test_joined_mutable_loans_end_after_conservative_path_conflict);
    RUN_TEST(test_compiler_ownership_lowering_records_explicit_semantic_operation);
    return UNITY_END();
}
