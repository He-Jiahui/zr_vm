#include "unity.h"

#include <string.h>

#include "zr_vm_parser/exec_ir_binding_facts.h"

void setUp(void) {}
void tearDown(void) {}

static SZrCallBindingContract make_contract(EZrCallBindingKind kind,
                                            EZrCallBindingOperation operation,
                                            TZrUInt32 rid,
                                            TZrUInt64 moduleHash) {
    SZrCallBindingContract contract;
    memset(&contract, 0, sizeof(contract));
    contract.bindingKind = (TZrUInt32)kind;
    contract.targetMetadataToken = kind == ZR_CALL_BINDING_TYPED_FUNCTION
            ? 0u : ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, rid);
    contract.signatureToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, rid);
    contract.signatureHash = UINT64_C(0x100000000) + rid;
    contract.moduleSignatureHash = moduleHash;
    contract.dispatchSlot = ZR_CALL_BINDING_SLOT_NONE;
    contract.operation = (TZrUInt32)operation;
    if (kind == ZR_CALL_BINDING_VIRTUAL || kind == ZR_CALL_BINDING_INTERFACE) {
        contract.ownerTypeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 2u);
        contract.layoutVersion = 1u;
        contract.layoutHash = UINT64_C(0xabc000) + rid;
        contract.dispatchSlot = rid;
    }
    return contract;
}

static void init_function_with_opcodes(SZrExecIrFunction *function,
                                       const EZrExecIrOpcode *opcodes,
                                       TZrUInt32 opcodeCount) {
    SZrExecIrBlock *block;
    ZrCore_ExecIr_FunctionInit(function);
    function->functionToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 100u);
    function->signatureHash = UINT64_C(0xfeedcafe);
    function->contract.generation = 7u;
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_BLOCK_ID_ENTRY,
                      ZrCore_ExecIr_FunctionAddBlock(function,
                                                      ZR_EXEC_IR_BLOCK_FLAG_ENTRY));
    for (TZrUInt32 index = 0u; index < opcodeCount; ++index) {
        SZrExecIrInstruction instruction;
        memset(&instruction, 0, sizeof(instruction));
        instruction.opcode = (TZrUInt16)opcodes[index];
        instruction.sourceId = 900u + index;
        TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendInstruction(function,
                                                                  &instruction, ZR_NULL));
    }
    block = &function->blocks[0];
    block->instructionRange.start = 1u;
    block->instructionRange.count = opcodeCount;
    block->terminatorInstructionId = opcodeCount;
}

static void init_facts(SZrExecIrBindingFacts *facts,
                       SZrExecIrBindingSegment *segments,
                       TZrUInt32 segmentCount,
                       SZrExecIrBindingRow *rows,
                       TZrUInt32 rowCount,
                       TZrUInt64 moduleHash) {
    ZrParser_ExecIr_BindingFacts_Init(facts);
    facts->functionToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 100u);
    facts->signatureHash = UINT64_C(0xfeedcafe);
    facts->moduleHash = moduleHash;
    facts->generation = 7u;
    facts->segments = segments;
    facts->segmentCount = segmentCount;
    facts->rows = rows;
    facts->rowCount = rowCount;
}

static void fill_final_segment(SZrExecIrBindingSegment *segment,
                               TZrUInt32 index,
                               TZrExecIrInstructionId instructionId,
                               TZrUInt32 rowIndex,
                               TZrMetadataToken memberToken,
                               TZrUInt32 sourceId) {
    memset(segment, 0, sizeof(*segment));
    segment->index = index;
    segment->kind = ZR_EXEC_IR_BINDING_SEGMENT_FINAL_CALL;
    segment->flags = ZR_EXEC_IR_BINDING_SEGMENT_FLAG_FINAL;
    segment->instructionId = instructionId;
    segment->memberToken = memberToken;
    segment->memberId = ZR_EXEC_IR_BINDING_MEMBER_NONE;
    segment->bindingRow = rowIndex;
    segment->sourceId = sourceId;
}

static void fill_row(SZrExecIrBindingRow *row,
                     TZrUInt32 rowIndex,
                     TZrExecIrInstructionId instructionId,
                     TZrUInt32 segmentIndex,
                     const SZrCallBindingContract *contract,
                     TZrUInt32 sourceId) {
    memset(row, 0, sizeof(*row));
    row->rowIndex = rowIndex;
    row->instructionId = instructionId;
    row->segmentIndex = segmentIndex;
    row->contract = *contract;
    row->location.kind = ZR_CALL_BINDING_RELOCATION_MODULE;
    row->location.targetIndex = rowIndex + 1u;
    row->sourceId = sourceId;
}

static void test_static_binding_facts_project_final_direct_row(void) {
    const TZrUInt64 moduleHash = UINT64_C(0xfeed1001);
    const EZrExecIrOpcode opcodes[] = {ZR_EXEC_IR_OPCODE_CALL};
    SZrExecIrFunction function;
    SZrExecIrBindingFacts facts;
    SZrExecIrBindingSegment segment;
    SZrExecIrBindingRow row;
    SZrCallBindingContract contract = make_contract(
            ZR_CALL_BINDING_DIRECT, ZR_CALL_BINDING_OPERATION_CALL, 1u, moduleHash);
    SZrExecIrDiagnostic diagnostic;

    init_function_with_opcodes(&function, opcodes, 1u);
    fill_final_segment(&segment, 0u, 1u, 0u, contract.targetMetadataToken, 501u);
    fill_row(&row, 0u, 1u, 0u, &contract, 501u);
    init_facts(&facts, &segment, 1u, &row, 1u, moduleHash);
    facts.expectedHash = ZrParser_ExecIr_BindingFacts_Hash(&facts);

    TEST_ASSERT_EQUAL(ZR_EXEC_IR_BINDING_FACTS_OK,
                      ZrParser_ExecIr_BindingFacts_ValidateEx(&facts, &function,
                                                               &diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_ProjectBindingFacts(&facts, &function,
                                                         &diagnostic));
    TEST_ASSERT_EQUAL(0u, function.instructions[0].bindingRow);
    TEST_ASSERT_EQUAL_STRING("ok", ZrParser_ExecIr_BindingFacts_StatusName(
                                      ZR_EXEC_IR_BINDING_FACTS_OK));
    TEST_ASSERT_EQUAL_PTR(&row, ZrParser_ExecIr_BindingFacts_RowAt(&facts, 0u));
    TEST_ASSERT_EQUAL_PTR(&segment,
                          ZrParser_ExecIr_BindingFacts_SegmentAt(&facts, 0u));
    TEST_ASSERT_NULL(ZrParser_ExecIr_BindingFacts_RowAt(&facts, 1u));

    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_static_binding_facts_preserve_field_chain_and_accessor_writeback(void) {
    const TZrUInt64 moduleHash = UINT64_C(0xfeed1002);
    const EZrExecIrOpcode chainOpcodes[] = {
        ZR_EXEC_IR_OPCODE_PLACE_PROJECT, ZR_EXEC_IR_OPCODE_LOAD,
        ZR_EXEC_IR_OPCODE_LOAD, ZR_EXEC_IR_OPCODE_CALL
    };
    SZrExecIrFunction function;
    SZrExecIrBindingFacts facts;
    SZrExecIrBindingSegment segments[3];
    SZrExecIrBindingRow row;
    SZrCallBindingContract callContract = make_contract(
            ZR_CALL_BINDING_DIRECT, ZR_CALL_BINDING_OPERATION_CALL, 4u, moduleHash);
    SZrExecIrDiagnostic diagnostic;

    init_function_with_opcodes(&function, chainOpcodes, 4u);
    memset(segments, 0, sizeof(segments));
    segments[0].index = 0u;
    segments[0].kind = ZR_EXEC_IR_BINDING_SEGMENT_RUNTIME_FIELD;
    segments[0].instructionId = 2u;
    segments[0].receiverInstructionId = 1u;
    segments[0].memberToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);
    segments[0].memberId = 10u;
    segments[0].layoutId = 3u;
    segments[0].sourceId = 601u;
    segments[0].bindingRow = ZR_EXEC_IR_BINDING_ROW_NONE;
    segments[1] = segments[0];
    segments[1].index = 1u;
    segments[1].instructionId = 3u;
    segments[1].receiverInstructionId = 2u;
    segments[1].memberToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 3u);
    segments[1].memberId = 11u;
    segments[1].sourceId = 602u;
    fill_final_segment(&segments[2], 2u, 4u, 0u,
                      callContract.targetMetadataToken, 603u);
    fill_row(&row, 0u, 4u, 2u, &callContract, 603u);
    init_facts(&facts, segments, 3u, &row, 1u, moduleHash);
    facts.finalSegmentIndex = 2u;

    TEST_ASSERT_TRUE(ZrParser_ExecIr_ProjectBindingFacts(&facts, &function,
                                                         &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_OPCODE_PLACE_PROJECT, function.instructions[0].opcode);
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_OPCODE_LOAD, function.instructions[1].opcode);
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_OPCODE_LOAD, function.instructions[2].opcode);
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_OPCODE_CALL, function.instructions[3].opcode);
    TEST_ASSERT_EQUAL(0u, function.instructions[3].bindingRow);

    {
        const EZrExecIrOpcode accessorOpcodes[] = {
            ZR_EXEC_IR_OPCODE_CALL, ZR_EXEC_IR_OPCODE_STORE
        };
        SZrExecIrBindingSegment accessorSegment;
        SZrExecIrBindingRow accessorRow;
        SZrExecIrBindingFacts accessorFacts;
        SZrExecIrFunction accessorFunction;
        SZrCallBindingContract setter = make_contract(
                ZR_CALL_BINDING_DIRECT, ZR_CALL_BINDING_OPERATION_SET, 5u, moduleHash);
        init_function_with_opcodes(&accessorFunction, accessorOpcodes, 2u);
        memset(&accessorSegment, 0, sizeof(accessorSegment));
        accessorSegment.index = 0u;
        accessorSegment.kind = ZR_EXEC_IR_BINDING_SEGMENT_ACCESSOR;
        accessorSegment.flags = ZR_EXEC_IR_BINDING_SEGMENT_FLAG_FINAL |
                                ZR_EXEC_IR_BINDING_SEGMENT_FLAG_WRITEBACK;
        accessorSegment.instructionId = 1u;
        accessorSegment.writebackInstructionId = 2u;
        accessorSegment.memberToken = setter.targetMetadataToken;
        accessorSegment.operation = ZR_CALL_BINDING_OPERATION_SET;
        accessorSegment.bindingRow = 0u;
        accessorSegment.sourceId = 604u;
        fill_row(&accessorRow, 0u, 1u, 0u, &setter, 604u);
        init_facts(&accessorFacts, &accessorSegment, 1u, &accessorRow, 1u,
                   moduleHash);
        TEST_ASSERT_TRUE(ZrParser_ExecIr_ProjectBindingFacts(&accessorFacts,
                                                             &accessorFunction,
                                                             &diagnostic));
        TEST_ASSERT_EQUAL(0u, accessorFunction.instructions[0].bindingRow);
        ZrCore_ExecIr_FreeFunction(&accessorFunction);
    }
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_static_binding_facts_support_polymorphic_and_typed_contracts(void) {
    const TZrUInt64 moduleHash = UINT64_C(0xfeed1003);
    const EZrExecIrOpcode opcodes[] = {ZR_EXEC_IR_OPCODE_CALL};
    EZrCallBindingKind kinds[] = {ZR_CALL_BINDING_VIRTUAL,
                                  ZR_CALL_BINDING_INTERFACE,
                                  ZR_CALL_BINDING_TYPED_FUNCTION};

    for (TZrUInt32 index = 0u; index < 3u; ++index) {
        SZrExecIrFunction function;
        SZrExecIrBindingFacts facts;
        SZrExecIrBindingSegment segment;
        SZrExecIrBindingRow row;
        SZrExecIrDiagnostic diagnostic;
        SZrCallBindingContract contract = make_contract(
                kinds[index], ZR_CALL_BINDING_OPERATION_CALL, index + 1u, moduleHash);
        init_function_with_opcodes(&function, opcodes, 1u);
        fill_final_segment(&segment, 0u, 1u, 0u, contract.targetMetadataToken, 700u + index);
        segment.kind = kinds[index] == ZR_CALL_BINDING_TYPED_FUNCTION
                ? ZR_EXEC_IR_BINDING_SEGMENT_TYPED_CALLABLE
                : ZR_EXEC_IR_BINDING_SEGMENT_FINAL_CALL;
        fill_row(&row, 0u, 1u, 0u, &contract, 700u + index);
        if (kinds[index] != ZR_CALL_BINDING_TYPED_FUNCTION) {
            TEST_ASSERT_EQUAL(index + 1u, contract.dispatchSlot);
            segment.layoutVersion = contract.layoutVersion;
            segment.layoutHash = contract.layoutHash;
        } else {
            row.location.kind = ZR_CALL_BINDING_RELOCATION_NONE;
            row.location.targetIndex = ZR_CALL_BINDING_SLOT_NONE;
        }
        init_facts(&facts, &segment, 1u, &row, 1u, moduleHash);
        TEST_ASSERT_TRUE(ZrParser_ExecIr_ProjectBindingFacts(&facts, &function,
                                                             &diagnostic));
        if (kinds[index] == ZR_CALL_BINDING_TYPED_FUNCTION) {
            /* A typed callable is selected from the runtime value; carrying a
             * module relocation would incorrectly reuse a prior witness. */
            row.location.kind = ZR_CALL_BINDING_RELOCATION_MODULE;
            row.location.targetIndex = 1u;
            TEST_ASSERT_EQUAL(ZR_EXEC_IR_BINDING_FACTS_INVALID_CHAIN,
                              ZrParser_ExecIr_BindingFacts_ValidateEx(
                                      &facts, &function, &diagnostic));
            TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                              diagnostic.code);
        }
        ZrCore_ExecIr_FreeFunction(&function);
    }
}

static void test_static_binding_facts_reject_unknown_and_ambiguous_segments(void) {
    const TZrUInt64 moduleHash = UINT64_C(0xfeed1004);
    const EZrExecIrOpcode opcodes[] = {ZR_EXEC_IR_OPCODE_CALL};
    SZrExecIrFunction function;
    SZrExecIrBindingFacts facts;
    SZrExecIrBindingSegment unknown;
    SZrExecIrDiagnostic diagnostic;

    init_function_with_opcodes(&function, opcodes, 1u);
    memset(&unknown, 0, sizeof(unknown));
    unknown.kind = ZR_EXEC_IR_BINDING_SEGMENT_RUNTIME_FIELD;
    unknown.memberId = ZR_EXEC_IR_BINDING_MEMBER_NONE;
    unknown.sourceId = 811u;
    unknown.bindingRow = ZR_EXEC_IR_BINDING_ROW_NONE;
    init_facts(&facts, &unknown, 1u, ZR_NULL, 0u, moduleHash);
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_BINDING_FACTS_UNKNOWN_MEMBER,
                      ZrParser_ExecIr_BindingFacts_ValidateEx(&facts, &function,
                                                               &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH, diagnostic.code);
    TEST_ASSERT_EQUAL(811u, diagnostic.sourceId);

    /* Two final segments are an ambiguity even when each individual row is
     * structurally valid. */
    {
        const EZrExecIrOpcode twoCallOpcodes[] = {
            ZR_EXEC_IR_OPCODE_CALL, ZR_EXEC_IR_OPCODE_CALL
        };
        SZrExecIrFunction ambiguousFunction;
        SZrExecIrBindingSegment segments[2];
        SZrExecIrBindingRow rows[2];
        SZrCallBindingContract first = make_contract(
                ZR_CALL_BINDING_DIRECT, ZR_CALL_BINDING_OPERATION_CALL, 1u, moduleHash);
        SZrCallBindingContract second = make_contract(
                ZR_CALL_BINDING_DIRECT, ZR_CALL_BINDING_OPERATION_CALL, 2u, moduleHash);
        init_function_with_opcodes(&ambiguousFunction, twoCallOpcodes, 2u);
        fill_final_segment(&segments[0], 0u, 1u, 0u, first.targetMetadataToken, 812u);
        fill_final_segment(&segments[1], 1u, 2u, 1u, second.targetMetadataToken, 813u);
        fill_row(&rows[0], 0u, 1u, 0u, &first, 812u);
        fill_row(&rows[1], 1u, 2u, 1u, &second, 813u);
        init_facts(&facts, segments, 2u, rows, 2u, moduleHash);
        TEST_ASSERT_EQUAL(ZR_EXEC_IR_BINDING_FACTS_AMBIGUOUS_MEMBER,
                          ZrParser_ExecIr_BindingFacts_ValidateEx(&facts,
                                                                   &ambiguousFunction,
                                                                   &diagnostic));
        TEST_ASSERT_EQUAL(ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH, diagnostic.code);
        TEST_ASSERT_EQUAL(813u, diagnostic.sourceId);
        ZrCore_ExecIr_FreeFunction(&ambiguousFunction);
    }
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_static_binding_facts_reject_signature_layout_and_missing_contract(void) {
    const TZrUInt64 moduleHash = UINT64_C(0xfeed1005);
    const EZrExecIrOpcode opcodes[] = {ZR_EXEC_IR_OPCODE_CALL};
    SZrExecIrFunction function;
    SZrExecIrBindingFacts facts;
    SZrExecIrBindingSegment segment;
    SZrExecIrBindingRow row;
    SZrExecIrDiagnostic diagnostic;
    SZrCallBindingContract contract = make_contract(
            ZR_CALL_BINDING_DIRECT, ZR_CALL_BINDING_OPERATION_CALL, 1u, moduleHash);

    init_function_with_opcodes(&function, opcodes, 1u);
    fill_final_segment(&segment, 0u, 1u, 0u, contract.targetMetadataToken, 901u);
    fill_row(&row, 0u, 1u, 0u, &contract, 901u);
    init_facts(&facts, &segment, 1u, &row, 1u, moduleHash);
    facts.expectedHash = UINT64_C(0x1234);
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_BINDING_FACTS_SIGNATURE_MISMATCH,
                      ZrParser_ExecIr_BindingFacts_ValidateEx(&facts, &function,
                                                               &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, diagnostic.code);
    TEST_ASSERT_EQUAL(901u, diagnostic.sourceId);

    facts.expectedHash = 0u;
    segment.layoutVersion = 2u;
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_BINDING_FACTS_LAYOUT_MISMATCH,
                      ZrParser_ExecIr_BindingFacts_ValidateEx(&facts, &function,
                                                               &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH, diagnostic.code);

    segment.layoutVersion = 0u;
    memset(&row.contract, 0, sizeof(row.contract));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_BINDING_FACTS_MISSING_CONTRACT,
                      ZrParser_ExecIr_BindingFacts_ValidateEx(&facts, &function,
                                                               &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION, diagnostic.code);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_static_binding_facts_projection_is_transactional_and_sealed_is_reported(void) {
    const TZrUInt64 moduleHash = UINT64_C(0xfeed1006);
    const EZrExecIrOpcode opcodes[] = {
        ZR_EXEC_IR_OPCODE_CALL, ZR_EXEC_IR_OPCODE_CALL
    };
    SZrExecIrFunction function;
    SZrExecIrBindingFacts facts;
    SZrExecIrBindingSegment segment;
    SZrExecIrBindingRow row;
    SZrExecIrDiagnostic diagnostic;
    SZrCallBindingContract contract = make_contract(
            ZR_CALL_BINDING_DIRECT, ZR_CALL_BINDING_OPERATION_CALL, 1u, moduleHash);
    TZrUInt64 hash;

    init_function_with_opcodes(&function, opcodes, 2u);
    function.instructions[0].bindingRow = 77u;
    fill_final_segment(&segment, 0u, 1u, 0u, contract.targetMetadataToken, 1001u);
    fill_row(&row, 0u, 99u, 0u, &contract, 1001u);
    init_facts(&facts, &segment, 1u, &row, 1u, moduleHash);
    hash = ZrParser_ExecIr_BindingFacts_Hash(&facts);
    TEST_ASSERT_EQUAL(hash, ZrParser_ExecIr_BindingFacts_Hash(&facts));
    row.contract.signatureHash += 1u;
    TEST_ASSERT_NOT_EQUAL(hash, ZrParser_ExecIr_BindingFacts_Hash(&facts));
    row.contract.signatureHash -= 1u;
    TEST_ASSERT_FALSE(ZrParser_ExecIr_ProjectBindingFacts(&facts, &function,
                                                          &diagnostic));
    TEST_ASSERT_EQUAL(77u, function.instructions[0].bindingRow);

    row.instructionId = 1u;
    facts.expectedHash = ZrParser_ExecIr_BindingFacts_Hash(&facts);
    function.sealed = ZR_TRUE;
    TEST_ASSERT_FALSE(ZrParser_ExecIr_ProjectBindingFacts(&facts, &function,
                                                          &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_SEALED, diagnostic.code);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_static_binding_facts_accept_row_owned_segment_association(void) {
    const TZrUInt64 moduleHash = UINT64_C(0xfeed1007);
    const EZrExecIrOpcode opcodes[] = {ZR_EXEC_IR_OPCODE_CALL};
    SZrExecIrFunction function;
    SZrExecIrBindingFacts facts;
    SZrExecIrBindingSegment segment;
    SZrExecIrBindingRow row;
    SZrExecIrDiagnostic diagnostic;
    SZrCallBindingContract contract = make_contract(
            ZR_CALL_BINDING_DIRECT, ZR_CALL_BINDING_OPERATION_CALL, 7u, moduleHash);

    init_function_with_opcodes(&function, opcodes, 1u);
    fill_final_segment(&segment, 0u, 1u, ZR_EXEC_IR_BINDING_ROW_NONE,
                      contract.targetMetadataToken, 1101u);
    fill_row(&row, 0u, 1u, 0u, &contract, 1101u);
    init_facts(&facts, &segment, 1u, &row, 1u, moduleHash);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_ProjectBindingFacts(&facts, &function,
                                                         &diagnostic));
    TEST_ASSERT_EQUAL(0u, function.instructions[0].bindingRow);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_static_binding_facts_require_field_layout_identity(void) {
    const TZrUInt64 moduleHash = UINT64_C(0xfeed1008);
    const EZrExecIrOpcode opcodes[] = {ZR_EXEC_IR_OPCODE_LOAD};
    SZrExecIrFunction function;
    SZrExecIrBindingFacts facts;
    SZrExecIrBindingSegment segment;
    SZrExecIrDiagnostic diagnostic;

    init_function_with_opcodes(&function, opcodes, 1u);
    memset(&segment, 0, sizeof(segment));
    segment.index = 0u;
    segment.kind = ZR_EXEC_IR_BINDING_SEGMENT_RUNTIME_FIELD;
    segment.instructionId = 1u;
    segment.memberToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 8u);
    segment.memberId = 8u;
    segment.sourceId = 1201u;
    init_facts(&facts, &segment, 1u, ZR_NULL, 0u, moduleHash);
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_BINDING_FACTS_LAYOUT_MISMATCH,
                      ZrParser_ExecIr_BindingFacts_ValidateEx(&facts, &function,
                                                               &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH, diagnostic.code);
    TEST_ASSERT_EQUAL(1201u, diagnostic.sourceId);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_static_binding_facts_accessor_getter_and_meta_operation(void) {
    const TZrUInt64 moduleHash = UINT64_C(0xfeed1009);
    const EZrExecIrOpcode opcodes[] = {
        ZR_EXEC_IR_OPCODE_CALL, ZR_EXEC_IR_OPCODE_CALL
    };
    SZrExecIrFunction function;
    SZrExecIrBindingFacts facts;
    SZrExecIrBindingSegment segments[2];
    SZrExecIrBindingRow rows[2];
    SZrExecIrDiagnostic diagnostic;
    SZrCallBindingContract getter = make_contract(
            ZR_CALL_BINDING_DIRECT, ZR_CALL_BINDING_OPERATION_GET, 9u, moduleHash);
    SZrCallBindingContract meta = make_contract(
            ZR_CALL_BINDING_DIRECT, ZR_CALL_BINDING_OPERATION_META, 10u, moduleHash);

    init_function_with_opcodes(&function, opcodes, 2u);
    memset(segments, 0, sizeof(segments));
    segments[0].index = 0u;
    segments[0].kind = ZR_EXEC_IR_BINDING_SEGMENT_ACCESSOR;
    segments[0].instructionId = 1u;
    segments[0].memberToken = getter.targetMetadataToken;
    segments[0].operation = ZR_CALL_BINDING_OPERATION_GET;
    segments[0].bindingRow = 0u;
    segments[0].sourceId = 1301u;
    segments[1].index = 1u;
    segments[1].kind = ZR_EXEC_IR_BINDING_SEGMENT_META;
    segments[1].flags = ZR_EXEC_IR_BINDING_SEGMENT_FLAG_FINAL;
    segments[1].instructionId = 2u;
    segments[1].memberToken = meta.targetMetadataToken;
    segments[1].operation = ZR_CALL_BINDING_OPERATION_META;
    segments[1].bindingRow = 1u;
    segments[1].sourceId = 1302u;
    fill_row(&rows[0], 0u, 1u, 0u, &getter, 1301u);
    fill_row(&rows[1], 1u, 2u, 1u, &meta, 1302u);
    init_facts(&facts, segments, 2u, rows, 2u, moduleHash);
    facts.finalSegmentIndex = 1u;
    /* A chain with a getter followed by a meta final call is valid when the
     * getter's row is represented as an intermediate operation and the final
     * row is the only row marked final. */
    TEST_ASSERT_TRUE(ZrParser_ExecIr_ProjectBindingFacts(&facts, &function,
                                                         &diagnostic));
    TEST_ASSERT_EQUAL(0u, function.instructions[0].bindingRow);
    TEST_ASSERT_EQUAL(1u, function.instructions[1].bindingRow);
    ZrCore_ExecIr_FreeFunction(&function);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_static_binding_facts_project_final_direct_row);
    RUN_TEST(test_static_binding_facts_preserve_field_chain_and_accessor_writeback);
    RUN_TEST(test_static_binding_facts_support_polymorphic_and_typed_contracts);
    RUN_TEST(test_static_binding_facts_reject_unknown_and_ambiguous_segments);
    RUN_TEST(test_static_binding_facts_reject_signature_layout_and_missing_contract);
    RUN_TEST(test_static_binding_facts_projection_is_transactional_and_sealed_is_reported);
    RUN_TEST(test_static_binding_facts_accept_row_owned_segment_association);
    RUN_TEST(test_static_binding_facts_require_field_layout_identity);
    RUN_TEST(test_static_binding_facts_accessor_getter_and_meta_operation);
    return UNITY_END();
}
