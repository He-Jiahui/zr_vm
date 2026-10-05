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

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_polymorphic_identity_witness_is_retained);
    RUN_TEST(test_typed_callable_identity_witness_is_target_free);
    return UNITY_END();
}
