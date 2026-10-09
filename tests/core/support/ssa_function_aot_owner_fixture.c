#include "ssa_function_aot_owner_fixture.h"

#include <stdlib.h>
#include <string.h>
#include "unity.h"

static void initialize_contract(SZrExecutionContract *contract,
                                TZrMetadataToken token,
                                TZrUInt64 signature, TZrUInt64 layout) {
    contract->schemaVersion = ZR_EXECUTION_CONTRACT_SCHEMA_VERSION;
    contract->abiVersion = ZR_EXECUTION_CONTRACT_ABI_VERSION;
    contract->logicalVersion = ZR_EXECUTION_CONTRACT_LOGICAL_VERSION;
    contract->generation = 1u;
    contract->targetToken = token;
    contract->signatureHash = signature;
    contract->layoutHash = layout;
    contract->moduleHash = 33u;
}

SZrTestsFunctionAotProducer *ZrTests_FunctionAotProducer_New(TZrInt64 literal) {
    SZrTestsFunctionAotProducer *p = calloc(1u, sizeof(*p));
    SZrAotIrFunction *f;
    if (p == ZR_NULL) return ZR_NULL;
    p->module.schemaVersion = ZR_AOT_IR_SCHEMA_VERSION;
    p->module.moduleHash = 33u;
    p->module.target.abiVersion = ZR_AOT_IR_TARGET_ABI_VERSION;
    p->module.target.pointerSize = (TZrUInt32)sizeof(void *);
    p->module.target.targetTripleHash = 67u;
    p->module.target.abiHash = 66u;
    initialize_contract(&p->module.contract, 0u, 44u, 55u);
    p->module.functions = &p->function;
    p->module.functionCount = 1u;
    p->module.constantPool = &p->constant;
    p->module.constantCount = 1u;
    p->constant.typeToken = 17u;
    p->constant.bits = (TZrUInt64)literal;
    f = &p->function;
    f->id = 1u;
    f->functionToken = 7u;
    f->signatureHash = 11u;
    initialize_contract(&f->contract, 7u, 11u, 22u);
    f->callableAbi.kind = ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64;
    f->callableAbi.returnTypeToken = 17u;
    f->frameLayout.logicalSlotCount = 1u;
    f->frameLayout.storageSlotCount = 1u;
    f->frameLayout.frameByteSize = 16u;
    f->frameLayout.frameByteAlign = 8u;
    f->frameLayout.layoutHash = 22u;
    p->slot.slotId = 1u;
    p->slot.byteSize = 8u;
    p->slot.byteAlign = 8u;
    p->slot.typeToken = 17u;
    f->frameSlots = &p->slot;
    f->frameSlotCount = 1u;
    f->valueSlotPool = &p->valueSlot;
    f->valueSlotCount = 1u;
    f->blocks = &p->block;
    f->blockCount = 1u;
    f->instructions = p->instructions;
    f->instructionCount = 2u;
    f->resultPool = &p->result;
    f->resultCount = 1u;
    f->operandPool = &p->operand;
    f->operandCount = 1u;
    p->result = p->operand = 1u;
    p->block.id = 1u;
    p->block.flags = ZR_EXEC_IR_BLOCK_FLAG_ENTRY;
    p->block.instructions.count = 2u;
    p->block.terminatorInstructionId = 2u;
    p->instructions[0].id = 1u;
    p->instructions[0].opcode = ZR_EXEC_IR_OPCODE_CONSTANT;
    p->instructions[0].results.count = 1u;
    p->instructions[0].typeToken = 17u;
    p->instructions[0].sourceId = 101u;
    p->instructions[1].id = 2u;
    p->instructions[1].opcode = ZR_EXEC_IR_OPCODE_RETURN;
    p->instructions[1].operands.count = 1u;
    p->instructions[1].typeToken = 17u;
    p->instructions[1].sourceId = 102u;
    f->sourceMaps = p->sourceMaps;
    f->sourceMapCount = 2u;
    for (TZrUInt32 i = 0u; i < 2u; ++i) {
        p->sourceMaps[i].instructionId = i + 1u;
        p->sourceMaps[i].sourceId = i + 101u;
        p->sourceMaps[i].startOffset = i;
        p->sourceMaps[i].endOffset = i + 1u;
        p->sourceMaps[i].startLine = p->sourceMaps[i].endLine = 1u;
        p->sourceMaps[i].startColumn = i + 1u;
        p->sourceMaps[i].endColumn = i + 2u;
    }
    f->logicalStateMap = &p->logicalMap;
    p->logicalMap.functionToken = 7u;
    p->logicalMap.signatureHash = 11u;
    p->logicalMap.generation = 1u;
    return p;
}

void ZrTests_FunctionAotProducer_Free(SZrTestsFunctionAotProducer **slot) {
    SZrTestsFunctionAotProducer *live = *slot;
    *slot = ZR_NULL;
    free(live);
}

void ZrTests_FunctionAotWitness_Capture(const SZrAotIrModule *m,
                                       SZrTestsFunctionAotWitness *w) {
    const SZrAotIrFunction *f = &m->functions[0];
    memset(w, 0, sizeof(*w));
    w->semanticHash = ZrCore_AotIr_HashModule(m);
    memcpy(&w->module, m, sizeof(w->module));
    w->module.functions = ZR_NULL;
    w->module.constantPool = ZR_NULL;
    w->module.layoutPool = ZR_NULL;
    memcpy(&w->function, f, sizeof(w->function));
#define CLEAR_POINTER(member) w->function.member = ZR_NULL
    CLEAR_POINTER(frameSlots); CLEAR_POINTER(valueSlotPool);
    CLEAR_POINTER(blocks); CLEAR_POINTER(instructions);
    CLEAR_POINTER(operandPool); CLEAR_POINTER(resultPool);
    CLEAR_POINTER(phiIncomingPool); CLEAR_POINTER(successorPool);
    CLEAR_POINTER(memoryTokenPool); CLEAR_POINTER(sourceMaps);
    CLEAR_POINTER(gcMap); CLEAR_POINTER(gcRootPool);
    CLEAR_POINTER(deoptStates); CLEAR_POINTER(deoptValuePool);
    CLEAR_POINTER(deoptAggregates); CLEAR_POINTER(deoptAggregateFields);
    CLEAR_POINTER(logicalStateMap);
#undef CLEAR_POINTER
    memcpy(&w->constant, m->constantPool, sizeof(w->constant));
    memcpy(&w->slot, f->frameSlots, sizeof(w->slot));
    w->valueSlot = f->valueSlotPool[0];
    w->result = f->resultPool[0];
    w->operand = f->operandPool[0];
    memcpy(&w->block, f->blocks, sizeof(w->block));
    memcpy(w->instructions, f->instructions, sizeof(w->instructions));
    memcpy(w->sourceMaps, f->sourceMaps, sizeof(w->sourceMaps));
    memcpy(&w->logicalMap, f->logicalStateMap, sizeof(w->logicalMap));
    w->logicalMap.entries = ZR_NULL;
    w->logicalMap.valuePool = ZR_NULL;
    w->logicalMap.rootPool = ZR_NULL;
    w->logicalMap.ownerStatePool = ZR_NULL;
}

static void assert_geometry(const SZrAotIrModule *m) {
    const SZrAotIrFunction *f;
    TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_NOT_NULL(m->functions);
    TEST_ASSERT_EQUAL_UINT32(1u, m->functionCount);
    TEST_ASSERT_EQUAL_UINT32(1u, m->constantCount);
    TEST_ASSERT_NOT_NULL(m->constantPool);
    f = &m->functions[0];
#define PRESENT(member, count, expected) \
    TEST_ASSERT_NOT_NULL(f->member); TEST_ASSERT_EQUAL_UINT32(expected, f->count)
    PRESENT(frameSlots, frameSlotCount, 1u);
    PRESENT(valueSlotPool, valueSlotCount, 1u);
    PRESENT(blocks, blockCount, 1u);
    PRESENT(instructions, instructionCount, 2u);
    PRESENT(operandPool, operandCount, 1u);
    PRESENT(resultPool, resultCount, 1u);
    PRESENT(sourceMaps, sourceMapCount, 2u);
#undef PRESENT
    TEST_ASSERT_NULL(m->layoutPool);
    TEST_ASSERT_EQUAL_UINT32(0u, m->layoutCount);
#define EMPTY(member, count) \
    TEST_ASSERT_NULL(f->member); TEST_ASSERT_EQUAL_UINT32(0u, f->count)
    EMPTY(phiIncomingPool, phiIncomingCount);
    EMPTY(successorPool, successorCount);
    EMPTY(memoryTokenPool, memoryTokenCount);
    EMPTY(gcRootPool, gcRootCount);
    EMPTY(deoptStates, deoptStateCount);
    EMPTY(deoptValuePool, deoptValueCount);
    EMPTY(deoptAggregates, deoptAggregateCount);
    EMPTY(deoptAggregateFields, deoptAggregateFieldCount);
#undef EMPTY
    TEST_ASSERT_NULL(f->gcMap);
    TEST_ASSERT_NOT_NULL(f->logicalStateMap);
#define EMPTY_MAP(member, count, capacity) \
    TEST_ASSERT_NULL(f->logicalStateMap->member); \
    TEST_ASSERT_EQUAL_UINT32(0u, f->logicalStateMap->count); \
    TEST_ASSERT_EQUAL_UINT32(0u, f->logicalStateMap->capacity)
    EMPTY_MAP(entries, entryCount, entryCapacity);
    EMPTY_MAP(valuePool, valueCount, valueCapacity);
    EMPTY_MAP(rootPool, rootCount, rootCapacity);
    EMPTY_MAP(ownerStatePool, ownerStateCount, ownerStateCapacity);
#undef EMPTY_MAP
}

void ZrTests_FunctionAot_AssertView(const SZrAotIrModule *m,
                                  const SZrTestsFunctionAotWitness *w) {
    SZrTestsFunctionAotWitness observed;
    SZrAotIrDiagnostic diagnostic = {0};
    SZrAotIrCallableAbi abi = {0};
    assert_geometry(m);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK, ZrCore_AotIr_ValidateModule(m, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK,
            ZrCore_AotIr_RequireExecutableAbi(m, 1u, &abi, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64, abi.kind);
    TEST_ASSERT_EQUAL_UINT32(17u, abi.returnTypeToken);
    ZrTests_FunctionAotWitness_Capture(m, &observed);
    TEST_ASSERT_EQUAL_MEMORY(w, &observed, sizeof(observed));
}

void ZrTests_FunctionAot_AssertIndependent(const SZrAotIrModule *source,
                                         const SZrAotIrModule *copy) {
    SZrTestsFunctionAotWitness witness;
    const SZrAotIrFunction *a = &source->functions[0], *b;
    assert_geometry(source);
    assert_geometry(copy);
    b = &copy->functions[0];
    TEST_ASSERT_TRUE(source != copy);
    TEST_ASSERT_TRUE(source->functions != copy->functions);
    TEST_ASSERT_TRUE(source->constantPool != copy->constantPool);
#define INDEPENDENT(member) TEST_ASSERT_TRUE(a->member != b->member)
    INDEPENDENT(frameSlots); INDEPENDENT(valueSlotPool);
    INDEPENDENT(blocks); INDEPENDENT(instructions);
    INDEPENDENT(operandPool); INDEPENDENT(resultPool);
    INDEPENDENT(sourceMaps); INDEPENDENT(logicalStateMap);
#undef INDEPENDENT
    ZrTests_FunctionAotWitness_Capture(source, &witness);
    ZrTests_FunctionAot_AssertView(copy, &witness);
}
