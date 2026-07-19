#include "reference_loan_nll_test_support.h"

static void test_shared_and_mutable_loans_end_after_last_use(void) {
    SLoanFixture fixture;
    TZrPlaceId placeId;
    TZrValueId sourceValue;
    TZrValueId sharedValue;
    TZrValueId mutableValue;
    TZrValueId loadValue;
    TZrLoanId sharedLoan;
    TZrLoanId mutableLoan;
    TZrSemanticInstructionId sharedConflict;
    TZrSemanticInstructionId sharedAfter;
    TZrSemanticInstructionId mutableConflict;
    TZrSemanticInstructionId mutableAfter;

    fixture_init(&fixture);
    placeId = add_place(&fixture, ZR_PARSER_PLACE_BASE_PARAMETER, 10U);
    sourceValue = add_value(&fixture, 1);
    sharedValue = add_value(&fixture, 2);
    mutableValue = add_value(&fixture, 7);
    loadValue = add_value(&fixture, 8);
    sharedLoan = add_loan(
            &fixture, placeId, ZR_SEMANTIC_LOAN_SHARED, sharedValue, 2);
    mutableLoan = add_loan(
            &fixture, placeId, ZR_SEMANTIC_LOAN_MUTABLE, mutableValue, 7);

    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_BORROW_SHARED, placeId, 0U,
            sharedValue, sharedLoan, 2);
    sharedConflict = emit_instruction(
            &fixture, ZR_SEMANTIC_IR_STORE, placeId, sourceValue,
            0U, 0U, 3);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_CALL_TYPED, placeId, sharedValue,
            0U, 0U, 4);
    sharedAfter = emit_instruction(
            &fixture, ZR_SEMANTIC_IR_STORE, placeId, sourceValue,
            0U, 0U, 5);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_BORROW_MUT, placeId, 0U,
            mutableValue, mutableLoan, 7);
    mutableConflict = emit_instruction(
            &fixture, ZR_SEMANTIC_IR_LOAD, placeId, 0U,
            loadValue, 0U, 8);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_CALL_TYPED, placeId, mutableValue,
            0U, 0U, 9);
    mutableAfter = emit_instruction(
            &fixture, ZR_SEMANTIC_IR_LOAD, placeId, 0U,
            loadValue, 0U, 10);
    bind_linear_cfg(&fixture);
    analyze(&fixture);

    TEST_ASSERT_NOT_NULL(diagnostic_at_instruction(&fixture, sharedConflict));
    TEST_ASSERT_NULL(diagnostic_at_instruction(&fixture, sharedAfter));
    TEST_ASSERT_NOT_NULL(diagnostic_at_instruction(&fixture, mutableConflict));
    TEST_ASSERT_NULL(diagnostic_at_instruction(&fixture, mutableAfter));
    TEST_ASSERT_TRUE(ZrParser_SemanticFlow_LoanIsLiveAt(
            &fixture.result, sharedConflict, sharedLoan, ZR_TRUE));
    TEST_ASSERT_FALSE(ZrParser_SemanticFlow_LoanIsLiveAt(
            &fixture.result, sharedAfter, sharedLoan, ZR_TRUE));
    TEST_ASSERT_TRUE(ZrParser_SemanticFlow_LoanIsLiveAt(
            &fixture.result, mutableConflict, mutableLoan, ZR_TRUE));
    TEST_ASSERT_FALSE(ZrParser_SemanticFlow_LoanIsLiveAt(
            &fixture.result, mutableAfter, mutableLoan, ZR_TRUE));
    analyze(&fixture);
    TEST_ASSERT_NOT_NULL(diagnostic_at_instruction(&fixture, sharedConflict));
    TEST_ASSERT_FALSE(ZrParser_SemanticFlow_LoanIsLiveAt(
            &fixture.result, mutableAfter, mutableLoan, ZR_TRUE));
    fixture_free(&fixture);
}

static void test_rust_borrowck_move_and_drop_negatives_are_rejected(void) {
    SLoanFixture fixture;
    TZrPlaceId movePlace;
    TZrPlaceId dropPlace;
    TZrValueId moveRefValue;
    TZrValueId dropRefValue;
    TZrValueId movedValue;
    TZrLoanId moveLoan;
    TZrLoanId dropLoan;
    TZrSemanticInstructionId moveConflict;
    TZrSemanticInstructionId dropConflict;

    fixture_init(&fixture);
    movePlace = add_place(
            &fixture, ZR_PARSER_PLACE_BASE_PARAMETER, 11U);
    dropPlace = add_place(
            &fixture, ZR_PARSER_PLACE_BASE_PARAMETER, 12U);
    moveRefValue = add_value(&fixture, 2);
    dropRefValue = add_value(&fixture, 5);
    movedValue = add_value(&fixture, 3);
    moveLoan = add_loan(
            &fixture, movePlace, ZR_SEMANTIC_LOAN_SHARED,
            moveRefValue, 2);
    dropLoan = add_loan(
            &fixture, dropPlace, ZR_SEMANTIC_LOAN_SHARED,
            dropRefValue, 5);

    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_BORROW_SHARED, movePlace, 0U,
            moveRefValue, moveLoan, 2);
    moveConflict = emit_instruction(
            &fixture, ZR_SEMANTIC_IR_MOVE, movePlace, 0U,
            movedValue, 0U, 3);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_CALL_TYPED, movePlace,
            moveRefValue, 0U, 0U, 4);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_BORROW_SHARED, dropPlace, 0U,
            dropRefValue, dropLoan, 5);
    dropConflict = emit_instruction(
            &fixture, ZR_SEMANTIC_IR_DROP, dropPlace, 0U,
            0U, 0U, 6);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_CALL_TYPED, dropPlace,
            dropRefValue, 0U, 0U, 7);
    bind_linear_cfg(&fixture);
    analyze(&fixture);

    TEST_ASSERT_NOT_NULL(diagnostic_at_instruction(&fixture, moveConflict));
    TEST_ASSERT_NOT_NULL(diagnostic_at_instruction(&fixture, dropConflict));
    fixture_free(&fixture);
}

static void test_csharp_ref_branch_last_use_allows_join_write(void) {
    SLoanFixture fixture;
    TZrPlaceId placeId;
    TZrValueId sourceValue;
    TZrValueId refValue;
    TZrLoanId loanId;
    TZrSemanticInstructionId branchConflict;
    TZrSemanticInstructionId joinWrite;
    TZrUInt32 entry;
    TZrUInt32 header;
    TZrUInt32 trueBlock;
    TZrUInt32 falseBlock;
    TZrUInt32 join;
    TZrUInt32 exit;

    fixture_init(&fixture);
    placeId = add_place(
            &fixture, ZR_PARSER_PLACE_BASE_PARAMETER, 13U);
    sourceValue = add_value(&fixture, 1);
    refValue = add_value(&fixture, 2);
    loanId = add_loan(
            &fixture, placeId, ZR_SEMANTIC_LOAN_SHARED, refValue, 2);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_BORROW_SHARED, placeId, 0U,
            refValue, loanId, 2);
    branchConflict = emit_instruction(
            &fixture, ZR_SEMANTIC_IR_STORE, placeId, sourceValue,
            0U, 0U, 3);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_CALL_TYPED, placeId, refValue,
            0U, 0U, 4);
    joinWrite = emit_instruction(
            &fixture, ZR_SEMANTIC_IR_STORE, placeId, sourceValue,
            0U, 0U, 5);

    entry = append_block(&fixture, ZR_PARSER_CFG_BLOCK_ENTRY);
    header = append_block(&fixture, ZR_PARSER_CFG_BLOCK_STATEMENT);
    trueBlock = append_block(&fixture, ZR_PARSER_CFG_BLOCK_STATEMENT);
    falseBlock = append_block(&fixture, ZR_PARSER_CFG_BLOCK_STATEMENT);
    join = append_block(&fixture, ZR_PARSER_CFG_BLOCK_JOIN);
    exit = append_block(&fixture, ZR_PARSER_CFG_BLOCK_EXIT);
    fixture.function.cfg.entryBlockId = entry;
    fixture.function.cfg.exitBlockId = exit;
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            &fixture.function.cfg, entry, header,
            ZR_PARSER_CFG_EDGE_NORMAL, ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            &fixture.function.cfg, header, trueBlock,
            ZR_PARSER_CFG_EDGE_TRUE_BRANCH, ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            &fixture.function.cfg, header, falseBlock,
            ZR_PARSER_CFG_EDGE_FALSE_BRANCH, ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            &fixture.function.cfg, trueBlock, join,
            ZR_PARSER_CFG_EDGE_NORMAL, ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            &fixture.function.cfg, falseBlock, join,
            ZR_PARSER_CFG_EDGE_NORMAL, ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            &fixture.function.cfg, join, exit,
            ZR_PARSER_CFG_EDGE_NORMAL, ZR_NULL));
    bind_block(&fixture, entry, 0U, 1U, ZR_PARSER_CFG_TERMINATOR_NONE);
    bind_block(&fixture, header, 1U, 0U, ZR_PARSER_CFG_TERMINATOR_BRANCH);
    bind_block(&fixture, trueBlock, 1U, 2U, ZR_PARSER_CFG_TERMINATOR_NONE);
    bind_block(&fixture, falseBlock, 3U, 0U, ZR_PARSER_CFG_TERMINATOR_NONE);
    bind_block(&fixture, join, 3U, 1U, ZR_PARSER_CFG_TERMINATOR_NONE);
    bind_block(&fixture, exit, 4U, 0U, ZR_PARSER_CFG_TERMINATOR_EXIT);
    analyze(&fixture);

    TEST_ASSERT_NOT_NULL(diagnostic_at_instruction(&fixture, branchConflict));
    TEST_ASSERT_NULL(diagnostic_at_instruction(&fixture, joinWrite));
    TEST_ASSERT_TRUE(ZrParser_SemanticFlow_LoanIsLiveAt(
            &fixture.result, branchConflict, loanId, ZR_TRUE));
    TEST_ASSERT_FALSE(ZrParser_SemanticFlow_LoanIsLiveAt(
            &fixture.result, joinWrite, loanId, ZR_TRUE));
    fixture_free(&fixture);
}

static void test_shared_loans_coexist_and_block_overlapping_mutable_borrow(void) {
    SLoanFixture fixture;
    TZrPlaceId placeId;
    TZrValueId firstSharedValue;
    TZrValueId secondSharedValue;
    TZrValueId mutableValue;
    TZrLoanId firstSharedLoan;
    TZrLoanId secondSharedLoan;
    TZrLoanId mutableLoan;
    TZrSemanticInstructionId secondSharedBorrow;
    TZrSemanticInstructionId mutableBorrow;

    fixture_init(&fixture);
    placeId = add_place(&fixture, ZR_PARSER_PLACE_BASE_PARAMETER, 15U);
    firstSharedValue = add_value(&fixture, 2);
    secondSharedValue = add_value(&fixture, 3);
    mutableValue = add_value(&fixture, 4);
    firstSharedLoan = add_loan(
            &fixture, placeId, ZR_SEMANTIC_LOAN_SHARED,
            firstSharedValue, 2);
    secondSharedLoan = add_loan(
            &fixture, placeId, ZR_SEMANTIC_LOAN_SHARED,
            secondSharedValue, 3);
    mutableLoan = add_loan(
            &fixture, placeId, ZR_SEMANTIC_LOAN_MUTABLE,
            mutableValue, 4);

    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_BORROW_SHARED, placeId, 0U,
            firstSharedValue, firstSharedLoan, 2);
    secondSharedBorrow = emit_instruction(
            &fixture, ZR_SEMANTIC_IR_BORROW_SHARED, placeId, 0U,
            secondSharedValue, secondSharedLoan, 3);
    mutableBorrow = emit_instruction(
            &fixture, ZR_SEMANTIC_IR_BORROW_MUT, placeId, 0U,
            mutableValue, mutableLoan, 4);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_CALL_TYPED, placeId, firstSharedValue,
            0U, 0U, 5);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_CALL_TYPED, placeId, secondSharedValue,
            0U, 0U, 6);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_CALL_TYPED, placeId, mutableValue,
            0U, 0U, 7);
    bind_linear_cfg(&fixture);
    analyze(&fixture);

    TEST_ASSERT_NULL(diagnostic_at_instruction(&fixture, secondSharedBorrow));
    TEST_ASSERT_NOT_NULL(diagnostic_at_instruction(&fixture, mutableBorrow));
    fixture_free(&fixture);
}

static void test_shared_loan_cannot_authorize_write_or_mutable_reborrow(void) {
    SLoanFixture fixture;
    TZrPlaceId placeId;
    TZrValueId sourceValue;
    TZrValueId sharedValue;
    TZrValueId childValue;
    TZrValueId loadValue;
    TZrLoanId sharedLoan;
    TZrLoanId childLoan;
    TZrSemanticInstructionId allowedRead;
    TZrSemanticInstructionId rejectedWrite;
    TZrSemanticInstructionId rejectedReborrow;

    fixture_init(&fixture);
    placeId = add_place(
            &fixture, ZR_PARSER_PLACE_BASE_PARAMETER, 14U);
    sourceValue = add_value(&fixture, 1);
    sharedValue = add_value(&fixture, 2);
    childValue = add_value(&fixture, 5);
    loadValue = add_value(&fixture, 3);
    sharedLoan = add_loan(
            &fixture, placeId, ZR_SEMANTIC_LOAN_SHARED, sharedValue, 2);
    childLoan = add_loan(
            &fixture, placeId, ZR_SEMANTIC_LOAN_MUTABLE, childValue, 5);

    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_BORROW_SHARED, placeId, 0U,
            sharedValue, sharedLoan, 2);
    allowedRead = emit_instruction(
            &fixture, ZR_SEMANTIC_IR_LOAD, placeId, 0U,
            loadValue, sharedLoan, 3);
    rejectedWrite = emit_instruction(
            &fixture, ZR_SEMANTIC_IR_STORE, placeId, sourceValue,
            0U, sharedLoan, 4);
    rejectedReborrow = emit_instruction(
            &fixture, ZR_SEMANTIC_IR_REBORROW, placeId, sharedValue,
            childValue, childLoan, 5);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_CALL_TYPED, placeId, childValue,
            0U, 0U, 6);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_CALL_TYPED, placeId, sharedValue,
            0U, 0U, 7);
    bind_linear_cfg(&fixture);
    analyze(&fixture);

    TEST_ASSERT_NULL(diagnostic_at_instruction(&fixture, allowedRead));
    TEST_ASSERT_NOT_NULL(diagnostic_at_instruction(&fixture, rejectedWrite));
    TEST_ASSERT_NOT_NULL(diagnostic_at_instruction(
            &fixture, rejectedReborrow));
    fixture_free(&fixture);
}

static void test_ref_value_store_load_propagation_preserves_loan_liveness(void) {
    SLoanFixture fixture;
    TZrPlaceId sourcePlace;
    TZrPlaceId refSlot;
    TZrValueId sourceValue;
    TZrValueId refValue;
    TZrValueId loadedRefValue;
    TZrLoanId loanId;
    TZrSemanticInstructionId conflict;
    TZrSemanticInstructionId afterLastUse;

    fixture_init(&fixture);
    sourcePlace = add_place(
            &fixture, ZR_PARSER_PLACE_BASE_PARAMETER, 16U);
    refSlot = add_place(&fixture, ZR_PARSER_PLACE_BASE_LOCAL, 17U);
    sourceValue = add_value(&fixture, 1);
    refValue = add_value(&fixture, 2);
    loadedRefValue = add_value(&fixture, 4);
    loanId = add_loan(
            &fixture, sourcePlace, ZR_SEMANTIC_LOAN_SHARED, refValue, 2);

    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_BORROW_SHARED, sourcePlace, 0U,
            refValue, loanId, 2);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_STORE, refSlot, refValue,
            0U, 0U, 3);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_LOAD, refSlot, 0U,
            loadedRefValue, 0U, 4);
    conflict = emit_instruction(
            &fixture, ZR_SEMANTIC_IR_STORE, sourcePlace, sourceValue,
            0U, 0U, 5);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_CALL_TYPED, sourcePlace, loadedRefValue,
            0U, 0U, 6);
    afterLastUse = emit_instruction(
            &fixture, ZR_SEMANTIC_IR_STORE, sourcePlace, sourceValue,
            0U, 0U, 7);
    bind_linear_cfg(&fixture);
    analyze(&fixture);

    TEST_ASSERT_NOT_NULL(diagnostic_at_instruction(&fixture, conflict));
    TEST_ASSERT_NULL(diagnostic_at_instruction(&fixture, afterLastUse));
    TEST_ASSERT_TRUE(ZrParser_SemanticFlow_LoanIsLiveAt(
            &fixture.result, conflict, loanId, ZR_TRUE));
    fixture_free(&fixture);
}

static void test_ref_slot_overwrite_kills_previous_loan_value(void) {
    SLoanFixture fixture;
    TZrPlaceId firstSource;
    TZrPlaceId secondSource;
    TZrPlaceId refSlot;
    TZrValueId storedValue;
    TZrValueId firstRefValue;
    TZrValueId secondRefValue;
    TZrValueId loadedRefValue;
    TZrLoanId firstLoan;
    TZrLoanId secondLoan;
    TZrSemanticInstructionId firstSourceWrite;
    TZrSemanticInstructionId secondSourceWrite;

    fixture_init(&fixture);
    firstSource = add_place(
            &fixture, ZR_PARSER_PLACE_BASE_PARAMETER, 18U);
    secondSource = add_place(
            &fixture, ZR_PARSER_PLACE_BASE_PARAMETER, 19U);
    refSlot = add_place(&fixture, ZR_PARSER_PLACE_BASE_LOCAL, 21U);
    storedValue = add_value(&fixture, 1);
    firstRefValue = add_value(&fixture, 2);
    secondRefValue = add_value(&fixture, 4);
    loadedRefValue = add_value(&fixture, 6);
    firstLoan = add_loan(
            &fixture, firstSource, ZR_SEMANTIC_LOAN_SHARED,
            firstRefValue, 2);
    secondLoan = add_loan(
            &fixture, secondSource, ZR_SEMANTIC_LOAN_SHARED,
            secondRefValue, 4);

    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_BORROW_SHARED, firstSource, 0U,
            firstRefValue, firstLoan, 2);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_STORE, refSlot, firstRefValue,
            0U, 0U, 3);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_BORROW_SHARED, secondSource, 0U,
            secondRefValue, secondLoan, 4);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_STORE, refSlot, secondRefValue,
            0U, 0U, 5);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_LOAD, refSlot, 0U,
            loadedRefValue, 0U, 6);
    firstSourceWrite = emit_instruction(
            &fixture, ZR_SEMANTIC_IR_STORE, firstSource, storedValue,
            0U, 0U, 7);
    secondSourceWrite = emit_instruction(
            &fixture, ZR_SEMANTIC_IR_STORE, secondSource, storedValue,
            0U, 0U, 8);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_CALL_TYPED, secondSource,
            loadedRefValue, 0U, 0U, 9);
    bind_linear_cfg(&fixture);
    analyze(&fixture);

    TEST_ASSERT_NULL(diagnostic_at_instruction(&fixture, firstSourceWrite));
    TEST_ASSERT_NOT_NULL(diagnostic_at_instruction(&fixture, secondSourceWrite));
    fixture_free(&fixture);
}

static void test_dynamic_indices_conflict_but_constant_indices_are_disjoint(void) {
    SLoanFixture fixture;
    TZrPlaceId basePlace;
    TZrPlaceId dynamicLeft;
    TZrPlaceId dynamicRight;
    TZrPlaceId constantLeft;
    TZrPlaceId constantRight;
    TZrValueId sourceValue;
    TZrValueId dynamicRef;
    TZrValueId constantRef;
    TZrLoanId dynamicLoan;
    TZrLoanId constantLoan;
    TZrSemanticInstructionId unknownConflict;
    TZrSemanticInstructionId disjointWrite;
    const SZrSemanticFlowDiagnostic *diagnostic;

    fixture_init(&fixture);
    basePlace = add_place(&fixture, ZR_PARSER_PLACE_BASE_PARAMETER, 20U);
    dynamicLeft = project_place(
            &fixture, basePlace, ZR_PARSER_PLACE_PROJECTION_INDEX, 101U);
    dynamicRight = project_place(
            &fixture, basePlace, ZR_PARSER_PLACE_PROJECTION_INDEX, 102U);
    constantLeft = project_place(
            &fixture, basePlace, ZR_PARSER_PLACE_PROJECTION_CONSTANT_INDEX, 0U);
    constantRight = project_place(
            &fixture, basePlace, ZR_PARSER_PLACE_PROJECTION_CONSTANT_INDEX, 1U);
    sourceValue = add_value(&fixture, 1);
    dynamicRef = add_value(&fixture, 2);
    constantRef = add_value(&fixture, 6);
    dynamicLoan = add_loan(
            &fixture, dynamicLeft, ZR_SEMANTIC_LOAN_SHARED, dynamicRef, 2);
    constantLoan = add_loan(
            &fixture, constantLeft, ZR_SEMANTIC_LOAN_SHARED, constantRef, 6);

    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_BORROW_SHARED, dynamicLeft, 0U,
            dynamicRef, dynamicLoan, 2);
    unknownConflict = emit_instruction(
            &fixture, ZR_SEMANTIC_IR_STORE, dynamicRight, sourceValue,
            0U, 0U, 3);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_CALL_TYPED, dynamicLeft, dynamicRef,
            0U, 0U, 4);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_BORROW_SHARED, constantLeft, 0U,
            constantRef, constantLoan, 6);
    disjointWrite = emit_instruction(
            &fixture, ZR_SEMANTIC_IR_STORE, constantRight, sourceValue,
            0U, 0U, 7);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_CALL_TYPED, constantLeft, constantRef,
            0U, 0U, 8);
    bind_linear_cfg(&fixture);
    analyze(&fixture);

    diagnostic = diagnostic_at_instruction(&fixture, unknownConflict);
    TEST_ASSERT_NOT_NULL(diagnostic);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_PLACE_UNKNOWN, diagnostic->overlap);
    TEST_ASSERT_EQUAL_INT(101, diagnostic->placeDeclarationRange.start.line);
    TEST_ASSERT_EQUAL_INT(2, diagnostic->loanOriginRange.start.line);
    TEST_ASSERT_EQUAL_INT(4, diagnostic->loanLastUseRange.start.line);
    TEST_ASSERT_NULL(diagnostic_at_instruction(&fixture, disjointWrite));
    fixture_free(&fixture);
}

static void test_nested_reborrow_suspends_parent_until_child_last_use(void) {
    SLoanFixture fixture;
    TZrPlaceId placeId;
    TZrValueId sourceValue;
    TZrValueId parentValue;
    TZrValueId childValue;
    TZrValueId grandchildValue;
    TZrLoanId parentLoan;
    TZrLoanId childLoan;
    TZrLoanId grandchildLoan;
    TZrSemanticInstructionId parentConflict;
    TZrSemanticInstructionId childAccess;
    TZrSemanticInstructionId parentAccess;

    fixture_init(&fixture);
    placeId = add_place(&fixture, ZR_PARSER_PLACE_BASE_PARAMETER, 30U);
    sourceValue = add_value(&fixture, 1);
    parentValue = add_value(&fixture, 2);
    childValue = add_value(&fixture, 3);
    grandchildValue = add_value(&fixture, 4);
    parentLoan = add_loan(
            &fixture, placeId, ZR_SEMANTIC_LOAN_MUTABLE, parentValue, 2);
    childLoan = add_loan(
            &fixture, placeId, ZR_SEMANTIC_LOAN_MUTABLE, childValue, 3);
    grandchildLoan = add_loan(
            &fixture, placeId, ZR_SEMANTIC_LOAN_SHARED, grandchildValue, 4);

    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_BORROW_MUT, placeId, 0U,
            parentValue, parentLoan, 2);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_REBORROW, placeId, parentValue,
            childValue, childLoan, 3);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_REBORROW, placeId, childValue,
            grandchildValue, grandchildLoan, 4);
    parentConflict = emit_instruction(
            &fixture, ZR_SEMANTIC_IR_STORE, placeId, sourceValue,
            0U, parentLoan, 5);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_CALL_TYPED, placeId, grandchildValue,
            0U, 0U, 6);
    childAccess = emit_instruction(
            &fixture, ZR_SEMANTIC_IR_STORE, placeId, sourceValue,
            0U, childLoan, 7);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_CALL_TYPED, placeId, childValue,
            0U, 0U, 8);
    parentAccess = emit_instruction(
            &fixture, ZR_SEMANTIC_IR_STORE, placeId, sourceValue,
            0U, parentLoan, 9);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_CALL_TYPED, placeId, parentValue,
            0U, 0U, 10);
    bind_linear_cfg(&fixture);
    analyze(&fixture);

    TEST_ASSERT_NOT_NULL(diagnostic_at_instruction(&fixture, parentConflict));
    TEST_ASSERT_NULL(diagnostic_at_instruction(&fixture, childAccess));
    TEST_ASSERT_NULL(diagnostic_at_instruction(&fixture, parentAccess));
    TEST_ASSERT_TRUE(ZrParser_SemanticFlow_LoanIsLiveAt(
            &fixture.result, parentConflict, grandchildLoan, ZR_TRUE));
    TEST_ASSERT_FALSE(ZrParser_SemanticFlow_LoanIsLiveAt(
            &fixture.result, childAccess, grandchildLoan, ZR_TRUE));
    TEST_ASSERT_TRUE(ZrParser_SemanticFlow_LoanIsLiveAt(
            &fixture.result, childAccess, childLoan, ZR_TRUE));
    TEST_ASSERT_FALSE(ZrParser_SemanticFlow_LoanIsLiveAt(
            &fixture.result, parentAccess, childLoan, ZR_TRUE));
    fixture_free(&fixture);
}

static void test_joined_ref_reborrow_preserves_all_possible_parents(void) {
    SLoanFixture fixture;
    TZrPlaceId sourceBase;
    TZrPlaceId firstSource;
    TZrPlaceId secondSource;
    TZrPlaceId refSlot;
    TZrValueId storedValue;
    TZrValueId firstParentValue;
    TZrValueId secondParentValue;
    TZrValueId joinedValue;
    TZrValueId childValue;
    TZrLoanId firstParentLoan;
    TZrLoanId secondParentLoan;
    TZrLoanId childLoan;
    TZrSemanticInstructionId secondSourceConflict;
    const SZrSemanticLoanRegionFact *childRegion;
    TZrUInt32 entry;
    TZrUInt32 header;
    TZrUInt32 trueBlock;
    TZrUInt32 falseBlock;
    TZrUInt32 join;
    TZrUInt32 exit;

    fixture_init(&fixture);
    sourceBase = add_place(
            &fixture, ZR_PARSER_PLACE_BASE_PARAMETER, 31U);
    firstSource = project_place(
            &fixture, sourceBase, ZR_PARSER_PLACE_PROJECTION_INDEX, 131U);
    secondSource = project_place(
            &fixture, sourceBase, ZR_PARSER_PLACE_PROJECTION_INDEX, 132U);
    refSlot = add_place(&fixture, ZR_PARSER_PLACE_BASE_LOCAL, 33U);
    storedValue = add_value(&fixture, 1);
    firstParentValue = add_value(&fixture, 2);
    secondParentValue = add_value(&fixture, 3);
    joinedValue = add_value(&fixture, 6);
    childValue = add_value(&fixture, 7);
    firstParentLoan = add_loan(
            &fixture, firstSource, ZR_SEMANTIC_LOAN_MUTABLE,
            firstParentValue, 2);
    secondParentLoan = add_loan(
            &fixture, secondSource, ZR_SEMANTIC_LOAN_MUTABLE,
            secondParentValue, 3);
    childLoan = add_loan(
            &fixture, firstSource, ZR_SEMANTIC_LOAN_MUTABLE,
            childValue, 7);

    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_BORROW_MUT, firstSource, 0U,
            firstParentValue, firstParentLoan, 2);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_BORROW_MUT, secondSource, 0U,
            secondParentValue, secondParentLoan, 3);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_STORE, refSlot, firstParentValue,
            0U, 0U, 4);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_STORE, refSlot, secondParentValue,
            0U, 0U, 5);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_LOAD, refSlot, 0U,
            joinedValue, 0U, 6);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_REBORROW, firstSource, joinedValue,
            childValue, childLoan, 7);
    secondSourceConflict = emit_instruction(
            &fixture, ZR_SEMANTIC_IR_STORE, secondSource, storedValue,
            0U, 0U, 8);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_CALL_TYPED, firstSource, childValue,
            0U, 0U, 9);

    entry = append_block(&fixture, ZR_PARSER_CFG_BLOCK_ENTRY);
    header = append_block(&fixture, ZR_PARSER_CFG_BLOCK_STATEMENT);
    trueBlock = append_block(&fixture, ZR_PARSER_CFG_BLOCK_STATEMENT);
    falseBlock = append_block(&fixture, ZR_PARSER_CFG_BLOCK_STATEMENT);
    join = append_block(&fixture, ZR_PARSER_CFG_BLOCK_JOIN);
    exit = append_block(&fixture, ZR_PARSER_CFG_BLOCK_EXIT);
    fixture.function.cfg.entryBlockId = entry;
    fixture.function.cfg.exitBlockId = exit;
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            &fixture.function.cfg, entry, header,
            ZR_PARSER_CFG_EDGE_NORMAL, ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            &fixture.function.cfg, header, trueBlock,
            ZR_PARSER_CFG_EDGE_TRUE_BRANCH, ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            &fixture.function.cfg, header, falseBlock,
            ZR_PARSER_CFG_EDGE_FALSE_BRANCH, ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            &fixture.function.cfg, trueBlock, join,
            ZR_PARSER_CFG_EDGE_NORMAL, ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            &fixture.function.cfg, falseBlock, join,
            ZR_PARSER_CFG_EDGE_NORMAL, ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            &fixture.function.cfg, join, exit,
            ZR_PARSER_CFG_EDGE_NORMAL, ZR_NULL));
    bind_block(&fixture, entry, 0U, 2U, ZR_PARSER_CFG_TERMINATOR_NONE);
    bind_block(&fixture, header, 2U, 0U, ZR_PARSER_CFG_TERMINATOR_BRANCH);
    bind_block(&fixture, trueBlock, 2U, 1U, ZR_PARSER_CFG_TERMINATOR_NONE);
    bind_block(&fixture, falseBlock, 3U, 1U, ZR_PARSER_CFG_TERMINATOR_NONE);
    bind_block(&fixture, join, 4U, 4U, ZR_PARSER_CFG_TERMINATOR_NONE);
    bind_block(&fixture, exit, 8U, 0U, ZR_PARSER_CFG_TERMINATOR_EXIT);
    analyze(&fixture);

    TEST_ASSERT_NOT_NULL(diagnostic_at_instruction(
            &fixture, secondSourceConflict));
    childRegion = ZrParser_SemanticFlow_LoanRegion(
            &fixture.result, childLoan);
    TEST_ASSERT_NOT_NULL(childRegion);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_SEMANTIC_LOAN_ID_MULTIPLE, childRegion->parentLoanId);
    fixture_free(&fixture);
}

static void test_reborrow_requires_parent_and_overlapping_provenance(void) {
    SLoanFixture fixture;
    TZrPlaceId firstPlace;
    TZrPlaceId secondPlace;
    TZrValueId parentValue;
    TZrValueId childValue;
    TZrValueId ordinaryValue;
    TZrLoanId parentLoan;
    TZrLoanId childLoan;

    fixture_init(&fixture);
    firstPlace = add_place(
            &fixture, ZR_PARSER_PLACE_BASE_PARAMETER, 36U);
    secondPlace = add_place(
            &fixture, ZR_PARSER_PLACE_BASE_PARAMETER, 37U);
    parentValue = add_value(&fixture, 2);
    childValue = add_value(&fixture, 3);
    parentLoan = add_loan(
            &fixture, firstPlace, ZR_SEMANTIC_LOAN_MUTABLE,
            parentValue, 2);
    childLoan = add_loan(
            &fixture, secondPlace, ZR_SEMANTIC_LOAN_MUTABLE,
            childValue, 3);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_BORROW_MUT, firstPlace, 0U,
            parentValue, parentLoan, 2);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_REBORROW, secondPlace, parentValue,
            childValue, childLoan, 3);
    bind_linear_cfg(&fixture);
    TEST_ASSERT_TRUE(ZrParser_SemanticIr_Validate(&fixture.function));
    TEST_ASSERT_FALSE(ZrParser_SemanticFlow_Analyze(
            g_state,
            &fixture.function,
            &fixture.function.cfg,
            &fixture.result));
    fixture_free(&fixture);

    fixture_init(&fixture);
    firstPlace = add_place(
            &fixture, ZR_PARSER_PLACE_BASE_PARAMETER, 38U);
    ordinaryValue = add_value(&fixture, 4);
    childValue = add_value(&fixture, 5);
    childLoan = add_loan(
            &fixture, firstPlace, ZR_SEMANTIC_LOAN_MUTABLE,
            childValue, 5);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_REBORROW, firstPlace, ordinaryValue,
            childValue, childLoan, 5);
    bind_linear_cfg(&fixture);
    TEST_ASSERT_TRUE(ZrParser_SemanticIr_Validate(&fixture.function));
    TEST_ASSERT_FALSE(ZrParser_SemanticFlow_Analyze(
            g_state,
            &fixture.function,
            &fixture.function.cfg,
            &fixture.result));
    fixture_free(&fixture);

    fixture_init(&fixture);
    firstPlace = add_place(
            &fixture, ZR_PARSER_PLACE_BASE_PARAMETER, 39U);
    secondPlace = project_place(
            &fixture, firstPlace, ZR_PARSER_PLACE_PROJECTION_FIELD, 400U);
    parentValue = add_value(&fixture, 6);
    childValue = add_value(&fixture, 7);
    parentLoan = add_loan(
            &fixture, secondPlace, ZR_SEMANTIC_LOAN_MUTABLE,
            parentValue, 6);
    childLoan = add_loan(
            &fixture, firstPlace, ZR_SEMANTIC_LOAN_MUTABLE,
            childValue, 7);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_BORROW_MUT, secondPlace, 0U,
            parentValue, parentLoan, 6);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_REBORROW, firstPlace, parentValue,
            childValue, childLoan, 7);
    bind_linear_cfg(&fixture);
    TEST_ASSERT_TRUE(ZrParser_SemanticIr_Validate(&fixture.function));
    TEST_ASSERT_FALSE(ZrParser_SemanticFlow_Analyze(
            g_state,
            &fixture.function,
            &fixture.function.cfg,
            &fixture.result));
    fixture_free(&fixture);
}

static void test_reborrow_parent_cycle_is_rejected(void) {
    SLoanFixture fixture;
    TZrPlaceId firstPlace;
    TZrPlaceId secondPlace;
    TZrValueId firstValue;
    TZrValueId secondValue;
    TZrLoanId firstLoan;
    TZrLoanId secondLoan;

    fixture_init(&fixture);
    firstPlace = add_place(
            &fixture, ZR_PARSER_PLACE_BASE_PARAMETER, 34U);
    secondPlace = add_place(
            &fixture, ZR_PARSER_PLACE_BASE_PARAMETER, 35U);
    firstValue = add_value(&fixture, 2);
    secondValue = add_value(&fixture, 3);
    firstLoan = add_loan(
            &fixture, firstPlace, ZR_SEMANTIC_LOAN_MUTABLE,
            firstValue, 2);
    secondLoan = add_loan(
            &fixture, secondPlace, ZR_SEMANTIC_LOAN_MUTABLE,
            secondValue, 3);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_REBORROW, firstPlace, secondValue,
            firstValue, firstLoan, 2);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_REBORROW, secondPlace, firstValue,
            secondValue, secondLoan, 3);
    bind_linear_cfg(&fixture);

    TEST_ASSERT_TRUE(ZrParser_SemanticIr_Validate(&fixture.function));
    TEST_ASSERT_FALSE(ZrParser_SemanticFlow_Analyze(
            g_state,
            &fixture.function,
            &fixture.function.cfg,
            &fixture.result));
    fixture_free(&fixture);
}

static void test_loop_back_edge_keeps_loan_live_only_inside_loop(void) {
    SLoanFixture fixture;
    TZrPlaceId placeId;
    TZrValueId sourceValue;
    TZrValueId refValue;
    TZrLoanId loanId;
    TZrSemanticInstructionId loopConflict;
    TZrSemanticInstructionId exitWrite;
    TZrUInt32 entry;
    TZrUInt32 header;
    TZrUInt32 body;
    TZrUInt32 exit;

    fixture_init(&fixture);
    placeId = add_place(&fixture, ZR_PARSER_PLACE_BASE_PARAMETER, 40U);
    sourceValue = add_value(&fixture, 1);
    refValue = add_value(&fixture, 2);
    loanId = add_loan(
            &fixture, placeId, ZR_SEMANTIC_LOAN_SHARED, refValue, 2);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_BORROW_SHARED, placeId, 0U,
            refValue, loanId, 2);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_CALL_TYPED, placeId, refValue,
            0U, 0U, 3);
    loopConflict = emit_instruction(
            &fixture, ZR_SEMANTIC_IR_STORE, placeId, sourceValue,
            0U, 0U, 4);
    exitWrite = emit_instruction(
            &fixture, ZR_SEMANTIC_IR_STORE, placeId, sourceValue,
            0U, 0U, 5);

    entry = append_block(&fixture, ZR_PARSER_CFG_BLOCK_ENTRY);
    header = append_block(&fixture, ZR_PARSER_CFG_BLOCK_STATEMENT);
    body = append_block(&fixture, ZR_PARSER_CFG_BLOCK_STATEMENT);
    exit = append_block(&fixture, ZR_PARSER_CFG_BLOCK_EXIT);
    fixture.function.cfg.entryBlockId = entry;
    fixture.function.cfg.exitBlockId = exit;
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            &fixture.function.cfg, entry, header,
            ZR_PARSER_CFG_EDGE_NORMAL, ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            &fixture.function.cfg, header, body,
            ZR_PARSER_CFG_EDGE_TRUE_BRANCH, ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            &fixture.function.cfg, header, exit,
            ZR_PARSER_CFG_EDGE_FALSE_BRANCH, ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            &fixture.function.cfg, body, header,
            ZR_PARSER_CFG_EDGE_NORMAL, ZR_NULL));
    bind_block(&fixture, entry, 0U, 1U, ZR_PARSER_CFG_TERMINATOR_NONE);
    bind_block(&fixture, header, 1U, 1U, ZR_PARSER_CFG_TERMINATOR_BRANCH);
    bind_block(&fixture, body, 2U, 1U, ZR_PARSER_CFG_TERMINATOR_NONE);
    bind_block(&fixture, exit, 3U, 1U, ZR_PARSER_CFG_TERMINATOR_EXIT);
    analyze(&fixture);

    TEST_ASSERT_NOT_NULL(diagnostic_at_instruction(&fixture, loopConflict));
    TEST_ASSERT_NULL(diagnostic_at_instruction(&fixture, exitWrite));
    TEST_ASSERT_TRUE(ZrParser_SemanticFlow_LoanIsLiveAt(
            &fixture.result, loopConflict, loanId, ZR_TRUE));
    TEST_ASSERT_FALSE(ZrParser_SemanticFlow_LoanIsLiveAt(
            &fixture.result, exitWrite, loanId, ZR_TRUE));
    fixture_free(&fixture);
}

static void test_unreachable_block_does_not_publish_loan_conflicts(void) {
    SLoanFixture fixture;
    TZrPlaceId placeId;
    TZrValueId sourceValue;
    TZrValueId refValue;
    TZrLoanId loanId;
    TZrSemanticInstructionId deadConflict;
    TZrUInt32 entry;
    TZrUInt32 deadBlock;
    TZrUInt32 exit;

    fixture_init(&fixture);
    placeId = add_place(
            &fixture, ZR_PARSER_PLACE_BASE_PARAMETER, 50U);
    sourceValue = add_value(&fixture, 1);
    refValue = add_value(&fixture, 2);
    loanId = add_loan(
            &fixture, placeId, ZR_SEMANTIC_LOAN_SHARED, refValue, 2);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_BORROW_SHARED, placeId, 0U,
            refValue, loanId, 2);
    deadConflict = emit_instruction(
            &fixture, ZR_SEMANTIC_IR_STORE, placeId, sourceValue,
            0U, 0U, 3);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_CALL_TYPED, placeId, refValue,
            0U, 0U, 4);

    entry = append_block(&fixture, ZR_PARSER_CFG_BLOCK_ENTRY);
    deadBlock = append_block(&fixture, ZR_PARSER_CFG_BLOCK_STATEMENT);
    exit = append_block(&fixture, ZR_PARSER_CFG_BLOCK_EXIT);
    fixture.function.cfg.entryBlockId = entry;
    fixture.function.cfg.exitBlockId = exit;
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            &fixture.function.cfg, entry, exit,
            ZR_PARSER_CFG_EDGE_RETURN, ZR_NULL));
    bind_block(&fixture, entry, 0U, 0U, ZR_PARSER_CFG_TERMINATOR_RETURN);
    bind_block(&fixture, deadBlock, 0U, 3U, ZR_PARSER_CFG_TERMINATOR_NONE);
    bind_block(&fixture, exit, 3U, 0U, ZR_PARSER_CFG_TERMINATOR_EXIT);
    analyze(&fixture);

    TEST_ASSERT_NULL(diagnostic_at_instruction(&fixture, deadConflict));
    TEST_ASSERT_FALSE(ZrParser_SemanticFlow_LoanIsLiveAt(
            &fixture.result, deadConflict, loanId, ZR_TRUE));
    fixture_free(&fixture);
}

static void test_large_function_tracks_hundreds_of_disjoint_loans(void) {
    enum { LOAN_STRESS_COUNT = 512 };
    SLoanFixture fixture;

    fixture_init(&fixture);
    for (TZrUInt32 index = 0U; index < LOAN_STRESS_COUNT; index++) {
        TZrPlaceId placeId = add_place(
                &fixture,
                ZR_PARSER_PLACE_BASE_PARAMETER,
                1000U + index);
        TZrValueId refValue = add_value(
                &fixture, (TZrInt32)(2000U + index));
        TZrLoanId loanId = add_loan(
                &fixture,
                placeId,
                ZR_SEMANTIC_LOAN_SHARED,
                refValue,
                (TZrInt32)(2000U + index));
        emit_instruction(
                &fixture,
                ZR_SEMANTIC_IR_BORROW_SHARED,
                placeId,
                0U,
                refValue,
                loanId,
                (TZrInt32)(2000U + index));
        emit_instruction(
                &fixture,
                ZR_SEMANTIC_IR_CALL_TYPED,
                placeId,
                refValue,
                0U,
                0U,
                (TZrInt32)(3000U + index));
    }
    bind_linear_cfg(&fixture);
    analyze(&fixture);

    TEST_ASSERT_EQUAL_UINT64(LOAN_STRESS_COUNT, fixture.result.loanCount);
    TEST_ASSERT_EQUAL_UINT64(0U, fixture.result.diagnostics.length);
    TEST_ASSERT_NOT_NULL(ZrParser_SemanticFlow_LoanRegion(
            &fixture.result, LOAN_STRESS_COUNT));
    fixture_free(&fixture);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_shared_and_mutable_loans_end_after_last_use);
    RUN_TEST(test_rust_borrowck_move_and_drop_negatives_are_rejected);
    RUN_TEST(test_csharp_ref_branch_last_use_allows_join_write);
    RUN_TEST(test_shared_loans_coexist_and_block_overlapping_mutable_borrow);
    RUN_TEST(test_shared_loan_cannot_authorize_write_or_mutable_reborrow);
    RUN_TEST(test_ref_value_store_load_propagation_preserves_loan_liveness);
    RUN_TEST(test_ref_slot_overwrite_kills_previous_loan_value);
    RUN_TEST(test_dynamic_indices_conflict_but_constant_indices_are_disjoint);
    RUN_TEST(test_nested_reborrow_suspends_parent_until_child_last_use);
    RUN_TEST(test_joined_ref_reborrow_preserves_all_possible_parents);
    RUN_TEST(test_reborrow_requires_parent_and_overlapping_provenance);
    RUN_TEST(test_reborrow_parent_cycle_is_rejected);
    RUN_TEST(test_loop_back_edge_keeps_loan_live_only_inside_loop);
    RUN_TEST(test_unreachable_block_does_not_publish_loan_conflicts);
    RUN_TEST(test_large_function_tracks_hundreds_of_disjoint_loans);
    return UNITY_END();
}
