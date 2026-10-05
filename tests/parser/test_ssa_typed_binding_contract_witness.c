#include "unity.h"

#include <string.h>

#include "zr_vm_parser/exec_ir_binding_facts.h"

void setUp(void) {}
void tearDown(void) {}

static void init_function(SZrExecIrFunction *function) {
    SZrExecIrInstruction instruction;
    ZrCore_ExecIr_FunctionInit(function);
    function->functionToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 900u);
    function->signatureHash = UINT64_C(0xfeedcafe);
    function->contract.generation = 3u;
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_BLOCK_ID_ENTRY,
                      ZrCore_ExecIr_FunctionAddBlock(function,
                                                      ZR_EXEC_IR_BLOCK_FLAG_ENTRY));
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_CALL;
    instruction.sourceId = 4100u;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendInstruction(function,
                                                               &instruction, ZR_NULL));
    function->blocks[0].instructionRange.start = 1u;
    function->blocks[0].instructionRange.count = 1u;
    function->blocks[0].terminatorInstructionId = 1u;
}

static void init_facts(SZrExecIrBindingFacts *facts,
                       SZrExecIrBindingSegment *segment,
                       SZrExecIrBindingRow *row,
                       SZrCallBindingContract *contract,
                       EZrExecIrBindingSegmentKind segmentKind) {
    memset(segment, 0, sizeof(*segment));
    segment->index = 0u;
    segment->kind = segmentKind;
    segment->flags = ZR_EXEC_IR_BINDING_SEGMENT_FLAG_FINAL;
    segment->instructionId = 1u;
    segment->memberToken = contract->targetMetadataToken;
    segment->layoutVersion = contract->layoutVersion;
    segment->layoutHash = contract->layoutHash;
    segment->sourceId = 4101u;
    memset(row, 0, sizeof(*row));
    row->rowIndex = 0u;
    row->instructionId = 1u;
    row->segmentIndex = 0u;
    row->contract = *contract;
    row->location.kind = contract->bindingKind == ZR_CALL_BINDING_TYPED_FUNCTION
            ? ZR_CALL_BINDING_RELOCATION_NONE : ZR_CALL_BINDING_RELOCATION_MODULE;
    row->location.targetIndex = contract->bindingKind == ZR_CALL_BINDING_TYPED_FUNCTION
            ? ZR_CALL_BINDING_SLOT_NONE : 1u;
    row->sourceId = 4101u;
    ZrParser_ExecIr_BindingFacts_Init(facts);
    facts->functionToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 900u);
    facts->signatureHash = UINT64_C(0xfeedcafe);
    facts->moduleHash = contract->moduleSignatureHash;
    facts->generation = 3u;
    facts->segments = segment;
    facts->segmentCount = 1u;
    facts->rows = row;
    facts->rowCount = 1u;
    facts->finalSegmentIndex = 0u;
}

static SZrCallBindingContract make_contract(EZrCallBindingKind kind,
                                             TZrUInt32 rid) {
    SZrCallBindingContract contract;
    memset(&contract, 0, sizeof(contract));
    contract.bindingKind = (TZrUInt32)kind;
    contract.targetMetadataToken = kind == ZR_CALL_BINDING_TYPED_FUNCTION
            ? 0u : ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, rid);
    contract.signatureToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, rid);
    contract.signatureHash = UINT64_C(0x100000000) + rid;
    contract.moduleSignatureHash = UINT64_C(0xfeed5000) + rid;
    contract.dispatchSlot = ZR_CALL_BINDING_SLOT_NONE;
    if (kind == ZR_CALL_BINDING_VIRTUAL || kind == ZR_CALL_BINDING_INTERFACE) {
        contract.ownerTypeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 17u);
        contract.layoutVersion = 4u;
        contract.layoutHash = UINT64_C(0xabc00000) + rid;
        contract.dispatchSlot = 23u + rid;
    }
    return contract;
}

static void test_polymorphic_identity_witness_is_retained(void) {
    const EZrExecIrBindingSegmentKind kinds[] = {
        ZR_EXEC_IR_BINDING_SEGMENT_FINAL_CALL,
        ZR_EXEC_IR_BINDING_SEGMENT_FINAL_CALL
    };
    const EZrCallBindingKind contracts[] = {
        ZR_CALL_BINDING_VIRTUAL, ZR_CALL_BINDING_INTERFACE
    };
    for (TZrUInt32 index = 0u; index < 2u; ++index) {
        SZrExecIrFunction function;
        SZrExecIrBindingFacts facts;
        SZrExecIrBindingSegment segment;
        SZrExecIrBindingRow row;
        SZrExecIrDiagnostic diagnostic;
        SZrCallBindingContract contract = make_contract(contracts[index], index + 1u);
        init_function(&function);
        init_facts(&facts, &segment, &row, &contract, kinds[index]);
        TEST_ASSERT_TRUE(ZrParser_ExecIr_ProjectBindingFacts(&facts, &function,
                                                              &diagnostic));
        TEST_ASSERT_EQUAL_UINT32(0u, segment.receiverTypeToken);
        TEST_ASSERT_EQUAL_UINT32(contract.layoutVersion, segment.layoutVersion);
        TEST_ASSERT_EQUAL_UINT64(contract.layoutHash, segment.layoutHash);
        TEST_ASSERT_EQUAL_UINT32(contract.ownerTypeToken,
                function.bindingRows[0].contract.ownerTypeToken);
        TEST_ASSERT_EQUAL_UINT32(contract.dispatchSlot, function.bindingRows[0].contract.dispatchSlot);
        TEST_ASSERT_EQUAL_UINT32(0u, diagnostic.sourceId);
        ZrCore_ExecIr_FreeFunction(&function);
    }
}

static void test_typed_callable_identity_witness_is_target_free(void) {
    SZrExecIrFunction function;
    SZrExecIrBindingFacts facts;
    SZrExecIrBindingSegment segment;
    SZrExecIrBindingRow row;
    SZrExecIrDiagnostic diagnostic;
    SZrCallBindingContract contract = make_contract(ZR_CALL_BINDING_TYPED_FUNCTION, 3u);
    init_function(&function);
    init_facts(&facts, &segment, &row, &contract,
               ZR_EXEC_IR_BINDING_SEGMENT_TYPED_CALLABLE);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_ProjectBindingFacts(&facts, &function,
                                                          &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(contract.signatureToken, function.bindingRows[0].contract.signatureToken);
    TEST_ASSERT_EQUAL_UINT64(contract.signatureHash, function.bindingRows[0].contract.signatureHash);
    TEST_ASSERT_EQUAL_UINT32(0u, function.bindingRows[0].contract.targetMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(ZR_CALL_BINDING_RELOCATION_NONE, row.location.kind);
    TEST_ASSERT_EQUAL_UINT32(ZR_CALL_BINDING_SLOT_NONE, row.location.targetIndex);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void assert_published_binding_metadata_unchanged(
        const SZrExecIrFunction *function,
        const SZrExecIrFunction *published,
        const SZrExecIrBindingRow *publishedRow,
        TZrUInt64 publishedHash,
        TZrUInt32 publishedInstructionBindingRow) {
    const SZrExecIrBindingRow *row;
    TEST_ASSERT_EQUAL_PTR(published->bindingRows, function->bindingRows);
    TEST_ASSERT_EQUAL_UINT32(published->bindingRowsSchemaVersion,
                             function->bindingRowsSchemaVersion);
    TEST_ASSERT_EQUAL_UINT32(published->bindingRowCount, function->bindingRowCount);
    TEST_ASSERT_EQUAL_UINT32(published->bindingRowCapacity, function->bindingRowCapacity);
    TEST_ASSERT_EQUAL_UINT64(publishedHash,
                             ZrCore_ExecIr_FunctionBindingRowsHash(function));
    TEST_ASSERT_EQUAL_PTR(published->instructions, function->instructions);
    TEST_ASSERT_EQUAL_UINT32(published->instructionCount, function->instructionCount);
    TEST_ASSERT_EQUAL_UINT32(published->instructionCapacity, function->instructionCapacity);
    TEST_ASSERT_EQUAL_UINT32(publishedInstructionBindingRow,
                             function->instructions[0].bindingRow);
    TEST_ASSERT_EQUAL_UINT32(published->id, function->id);
    TEST_ASSERT_EQUAL_UINT32(published->functionToken, function->functionToken);
    TEST_ASSERT_EQUAL_UINT64(published->signatureHash, function->signatureHash);
    TEST_ASSERT_EQUAL_UINT32(published->entryBlockId, function->entryBlockId);
    TEST_ASSERT_EQUAL(published->sealed, function->sealed);
    TEST_ASSERT_EQUAL_UINT32(published->contract.schemaVersion, function->contract.schemaVersion);
    TEST_ASSERT_EQUAL_UINT32(published->contract.abiVersion, function->contract.abiVersion);
    TEST_ASSERT_EQUAL_UINT32(published->contract.logicalVersion, function->contract.logicalVersion);
    TEST_ASSERT_EQUAL_UINT32(published->contract.reserved0, function->contract.reserved0);
    TEST_ASSERT_EQUAL_UINT64(published->contract.generation, function->contract.generation);
    TEST_ASSERT_EQUAL_UINT32(published->contract.targetToken, function->contract.targetToken);
    TEST_ASSERT_EQUAL_UINT32(published->contract.reserved1, function->contract.reserved1);
    TEST_ASSERT_EQUAL_UINT64(published->contract.signatureHash, function->contract.signatureHash);
    TEST_ASSERT_EQUAL_UINT64(published->contract.layoutHash, function->contract.layoutHash);
    TEST_ASSERT_EQUAL_UINT64(published->contract.moduleHash, function->contract.moduleHash);
    TEST_ASSERT_EQUAL_UINT32(published->contract.requiredCapabilities,
                             function->contract.requiredCapabilities);
    TEST_ASSERT_EQUAL_UINT32(published->contract.declaredEffects, function->contract.declaredEffects);
    row = &function->bindingRows[0];
    TEST_ASSERT_EQUAL_UINT32(publishedRow->rowIndex, row->rowIndex);
    TEST_ASSERT_EQUAL_UINT32(publishedRow->instructionId, row->instructionId);
    TEST_ASSERT_EQUAL_UINT32(publishedRow->segmentIndex, row->segmentIndex);
    TEST_ASSERT_EQUAL_UINT32(publishedRow->sourceId, row->sourceId);
    TEST_ASSERT_EQUAL_UINT32(publishedRow->contract.bindingKind, row->contract.bindingKind);
    TEST_ASSERT_EQUAL_UINT32(publishedRow->contract.targetMetadataToken,
                             row->contract.targetMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(publishedRow->contract.signatureToken, row->contract.signatureToken);
    TEST_ASSERT_EQUAL_UINT32(publishedRow->contract.ownerTypeToken, row->contract.ownerTypeToken);
    TEST_ASSERT_EQUAL_UINT64(publishedRow->contract.signatureHash, row->contract.signatureHash);
    TEST_ASSERT_EQUAL_UINT64(publishedRow->contract.moduleSignatureHash,
                             row->contract.moduleSignatureHash);
    TEST_ASSERT_EQUAL_UINT32(publishedRow->contract.layoutVersion, row->contract.layoutVersion);
    TEST_ASSERT_EQUAL_UINT32(publishedRow->contract.dispatchSlot, row->contract.dispatchSlot);
    TEST_ASSERT_EQUAL_UINT64(publishedRow->contract.layoutHash, row->contract.layoutHash);
    TEST_ASSERT_EQUAL_UINT32(publishedRow->contract.operation, row->contract.operation);
    TEST_ASSERT_EQUAL_UINT32(publishedRow->contract.reserved0, row->contract.reserved0);
    TEST_ASSERT_EQUAL_UINT64(publishedRow->contract.reserved1, row->contract.reserved1);
    TEST_ASSERT_EQUAL_UINT32(publishedRow->location.kind, row->location.kind);
    TEST_ASSERT_EQUAL_UINT32(publishedRow->location.targetIndex, row->location.targetIndex);
    TEST_ASSERT_EQUAL_UINT32(publishedRow->location.ownerDepth, row->location.ownerDepth);
    TEST_ASSERT_EQUAL_UINT32(publishedRow->location.flags, row->location.flags);
}

static void assert_reserved_flags_rejected(TZrUInt32 flags) {
    const EZrCallBindingKind kinds[] = {
        ZR_CALL_BINDING_TYPED_FUNCTION, ZR_CALL_BINDING_DIRECT
    };
    for (TZrUInt32 index = 0u; index < 2u; ++index) {
        SZrExecIrFunction function;
        SZrExecIrFunction published;
        SZrExecIrBindingFacts facts;
        SZrExecIrBindingSegment segment;
        SZrExecIrBindingRow row;
        SZrExecIrBindingRow publishedRow;
        SZrExecIrDiagnostic diagnostic;
        SZrCallBindingContract contract = make_contract(kinds[index], index + 4u);
        TZrUInt64 publishedHash;
        TZrUInt32 publishedInstructionBindingRow;
        init_function(&function);
        init_facts(&facts, &segment, &row, &contract,
                   kinds[index] == ZR_CALL_BINDING_TYPED_FUNCTION
                           ? ZR_EXEC_IR_BINDING_SEGMENT_TYPED_CALLABLE
                           : ZR_EXEC_IR_BINDING_SEGMENT_FINAL_CALL);
        facts.expectedHash = ZrParser_ExecIr_BindingFacts_Hash(&facts);
        TEST_ASSERT_EQUAL(ZR_EXEC_IR_BINDING_FACTS_OK,
                          ZrParser_ExecIr_BindingFacts_ValidateEx(&facts, &function,
                                                                 &diagnostic));
        TEST_ASSERT_TRUE(ZrParser_ExecIr_ProjectBindingFacts(&facts, &function,
                                                              &diagnostic));
        TEST_ASSERT_EQUAL_UINT32(ZR_EXEC_IR_BINDING_ROWS_SCHEMA_TYPED,
                                 function.bindingRowsSchemaVersion);
        TEST_ASSERT_EQUAL_UINT32(1u, function.bindingRowCount);
        TEST_ASSERT_EQUAL_UINT32(1u, function.instructions[0].bindingRow);
        TEST_ASSERT_EQUAL_UINT32(0u, function.bindingRows[0].segmentIndex);
        TEST_ASSERT_EQUAL_UINT64(facts.moduleHash, function.contract.moduleHash);
        published = function;
        publishedRow = function.bindingRows[0];
        publishedHash = ZrCore_ExecIr_FunctionBindingRowsHash(&function);
        publishedInstructionBindingRow = function.instructions[0].bindingRow;

        facts.flags = flags;
        /* Re-pin the changed payload so a stale hash cannot mask the flags error. */
        facts.expectedHash = ZrParser_ExecIr_BindingFacts_Hash(&facts);
        TEST_ASSERT_EQUAL(ZR_EXEC_IR_BINDING_FACTS_INVALID_ARGUMENT,
                          ZrParser_ExecIr_BindingFacts_ValidateEx(&facts, &function,
                                                                 &diagnostic));
        TEST_ASSERT_EQUAL(ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, diagnostic.code);
        TEST_ASSERT_EQUAL_UINT32(0u, diagnostic.expectedVersion);
        TEST_ASSERT_EQUAL_UINT32(flags, diagnostic.actualVersion);
        assert_published_binding_metadata_unchanged(&function, &published,
                &publishedRow, publishedHash, publishedInstructionBindingRow);

        TEST_ASSERT_FALSE(ZrParser_ExecIr_ProjectBindingFacts(&facts, &function,
                                                               &diagnostic));
        TEST_ASSERT_EQUAL(ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, diagnostic.code);
        TEST_ASSERT_EQUAL_UINT32(0u, diagnostic.expectedVersion);
        TEST_ASSERT_EQUAL_UINT32(flags, diagnostic.actualVersion);
        assert_published_binding_metadata_unchanged(&function, &published,
                &publishedRow, publishedHash, publishedInstructionBindingRow);
        ZrCore_ExecIr_FreeFunction(&function);
    }
}

static void test_schema_one_rejects_reserved_low_flag_without_publication(void) {
    assert_reserved_flags_rejected(1u);
}

static void test_schema_one_rejects_reserved_high_flag_without_publication(void) {
    assert_reserved_flags_rejected(UINT32_C(0x80000000));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_polymorphic_identity_witness_is_retained);
    RUN_TEST(test_typed_callable_identity_witness_is_target_free);
    RUN_TEST(test_schema_one_rejects_reserved_low_flag_without_publication);
    RUN_TEST(test_schema_one_rejects_reserved_high_flag_without_publication);
    return UNITY_END();
}
