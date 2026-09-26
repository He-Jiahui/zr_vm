#include "unity.h"

#include "zr_vm_core/exec_ir_state_map.h"
#include "zr_vm_parser/exec_ir_projections.h"
#include "zr_vm_parser/exec_ir_state_maps.h"

void test_aotir_owns_state_map(void);

void test_aotir_owns_state_map(void) {
    SZrExecIrFunction function;
    SZrAotIrProjection aot = {0};
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrInstruction instruction = {0};
    TZrExecIrValueId receiver, result;
    TZrExecIrMemoryTokenId before = 1u, after = 2u;
    SZrExecIrStateMapEntry *publishedEntries;
    TZrUInt32 originalRoots, originalValues, originalResumeId;

    ZrCore_ExecIr_FunctionInit(&function);
    function.id = 1u;
    function.functionToken = 99u;
    function.signatureHash = 123u;
    function.contract.generation = 7u;
    receiver = ZrCore_ExecIr_FunctionAddExternalValue(&function, 1u,
            ZR_EXEC_IR_OWNERSHIP_GC, ZR_EXEC_IR_NULLABILITY_NULLABLE);
    result = ZrCore_ExecIr_FunctionAddValue(&function, 1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_NULLABLE);
    TEST_ASSERT_EQUAL_UINT32(1u, receiver);
    TEST_ASSERT_EQUAL_UINT32(2u, result);
    TEST_ASSERT_EQUAL_UINT32(1u, ZrCore_ExecIr_FunctionAddBlock(
            &function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY));
    function.entryBlockId = 1u;
    instruction.opcode = ZR_EXEC_IR_OPCODE_CALL;
    instruction.flags = ZR_EXEC_IR_FLAG_MAY_THROW | ZR_EXEC_IR_FLAG_MAY_ALLOCATE;
    instruction.sourceId = 101u;
    instruction.effectIn = before;
    instruction.effectOut = after;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendOperands(
            &function, &receiver, 1u, &instruction.operandRange));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendResults(
            &function, &result, 1u, &instruction.resultRange));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendMemoryTokens(
            &function, &before, 1u, &instruction.memoryIn));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendMemoryTokens(
            &function, &after, 1u, &instruction.memoryOut));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendInstruction(&function, &instruction, NULL));
    instruction = (SZrExecIrInstruction){0};
    instruction.opcode = ZR_EXEC_IR_OPCODE_RETURN;
    instruction.sourceId = 102u;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendOperands(
            &function, &result, 1u, &instruction.operandRange));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendInstruction(&function, &instruction, NULL));
    function.blocks[0].instructionRange.count = 2u;
    function.blocks[0].terminatorInstructionId = 2u;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_VerifyFunction(&function,
            ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
    TEST_ASSERT_NOT_NULL(function.stateMap);
    TEST_ASSERT_TRUE(function.stateMap->entryCount > 0u);
    TEST_ASSERT_TRUE(function.stateMap->rootCount > 0u);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_StateMapStorageValid(function.stateMap));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_LowerAot(&function, &aot, &diagnostic));
    TEST_ASSERT_FALSE(aot.runnable);
    TEST_ASSERT_TRUE(aot.stateMapPresent);
    TEST_ASSERT_EQUAL_UINT32(function.stateMap->entryCount, aot.stateMap.entryCount);
    TEST_ASSERT_EQUAL_UINT32(function.stateMap->rootCount, aot.stateMap.rootCount);
    TEST_ASSERT_EQUAL_UINT32(function.stateMap->ownerStateCount,
                             aot.stateMap.ownerStateCount);
    TEST_ASSERT_TRUE(function.stateMap->entries != aot.stateMap.entries);
    TEST_ASSERT_TRUE(function.stateMap->valuePool != aot.stateMap.valuePool);
    TEST_ASSERT_TRUE(function.stateMap->rootPool != aot.stateMap.rootPool);
    TEST_ASSERT_TRUE(function.stateMap->ownerStatePool != aot.stateMap.ownerStatePool);
    publishedEntries = aot.stateMap.entries;
    originalRoots = aot.stateMap.rootPool[0];
    originalValues = function.stateMap->valueCount;
    originalResumeId = aot.stateMap.entries[0].resumeId;
    function.stateMap->rootPool[0] = 0u;
    function.stateMap->entries[0].resumeId = originalResumeId ^ 1u;
    TEST_ASSERT_EQUAL_UINT32(originalRoots, aot.stateMap.rootPool[0]);
    TEST_ASSERT_EQUAL_UINT32(originalResumeId, aot.stateMap.entries[0].resumeId);
    function.stateMap->rootPool[0] = originalRoots;
    function.stateMap->entries[0].resumeId = originalResumeId;
    function.stateMap->valueCount = function.stateMap->valueCapacity + 1u;
    TEST_ASSERT_FALSE(ZrCore_ExecIr_StateMapStorageValid(function.stateMap));
    TEST_ASSERT_FALSE(ZrParser_ExecIr_LowerAot(&function, &aot, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT32(function.functionToken, diagnostic.functionToken);
    TEST_ASSERT_EQUAL_PTR(publishedEntries, aot.stateMap.entries);
    TEST_ASSERT_EQUAL_UINT32(originalRoots, aot.stateMap.rootPool[0]);
    function.stateMap->valueCount = originalValues;
    ZrCore_ExecIr_FreeFunction(&function);
    TEST_ASSERT_EQUAL_UINT32(originalRoots, aot.stateMap.rootPool[0]);
    TEST_ASSERT_EQUAL_UINT32(originalResumeId, aot.stateMap.entries[0].resumeId);
    ZrParser_AotIrProjection_Free(&aot);
}
