#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "unity.h"
#include "tests/harness/runtime_support.h"
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/exec_ir.h"
#include "zr_vm_core/exec_ir_interpreter.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_parser/exec_ir_execbc_vm.h"
#include "zr_vm_parser/exec_ir_projections.h"

void setUp(void) {}
void tearDown(void) {}

#define ZR_TEST_EXECBC_TRACE_CAPACITY 256u
#define ZR_TEST_EXECBC_PATH_CAPACITY 64u

enum {
    ZR_TEST_COMPARE_LESS = 1u,
    ZR_TEST_COMPARE_GREATER = 3u
};

typedef enum EZrTestExecBcFixture {
    ZR_TEST_EXECBC_FIXTURE_ADD_SUB = 0,
    ZR_TEST_EXECBC_FIXTURE_COMPARE_PHI,
    ZR_TEST_EXECBC_FIXTURE_SLOT_ZERO_PHI,
    ZR_TEST_EXECBC_FIXTURE_CRITICAL_EDGE,
    ZR_TEST_EXECBC_FIXTURE_LOOP_SWAP,
    ZR_TEST_EXECBC_FIXTURE_CALL,
    ZR_TEST_EXECBC_FIXTURE_DOUBLE,
    ZR_TEST_EXECBC_FIXTURE_DEOPT_ID,
    ZR_TEST_EXECBC_FIXTURE_PHI_MISSING_MOVE,
    ZR_TEST_EXECBC_FIXTURE_PHI_WRONG_SOURCE,
    ZR_TEST_EXECBC_FIXTURE_SYNTHETIC_BLOCK_BODY,
    ZR_TEST_EXECBC_FIXTURE_PHI_CFG_MISSING_PREDECESSOR,
    ZR_TEST_EXECBC_FIXTURE_NONFIRST_ENTRY,
    ZR_TEST_EXECBC_FIXTURE_CONSTANT_POOL_TRUE,
    ZR_TEST_EXECBC_FIXTURE_CONSTANT_POOL_FALSE
} EZrTestExecBcFixture;

typedef struct SZrTestExecBcTrace {
    TZrUInt32 offsets[ZR_TEST_EXECBC_TRACE_CAPACITY];
    TZrUInt32 count;
    TZrBool overflow;
    TZrBool invalidProgramCounter;
    const SZrFunction *expectedFunction;
} SZrTestExecBcTrace;

typedef struct SZrTestExecBcReport {
    TZrBool fixtureBuilt;
    TZrBool oracleSucceeded;
    TZrBool oracleReturned;
    TZrBool projectionSucceeded;
    TZrBool materializationSucceeded;
    TZrBool outputWasEmptyOnFailure;
    TZrBool mutationApplied;
    TZrBool vmSucceeded;
    TZrBool pcMapValid;
    TZrBool sawExpectedSource;
    TZrBool sawSyntheticBlock;
    TZrBool frameCoversProjection;
    TZrBool frameClearsAllSlots;
    TZrBool frameHasNoPackedLayout;
    TZrBool returnValueUsesSlotZero;
    TZrBool returnValueUsesSparseSlot;
    TZrBool phiTemporaryIsAllocated;
    TZrBool phiTemporaryFitsFrame;
    TZrBool phiReadsPhysicalSlotZero;
    TZrBool traceOverflow;
    TZrBool invalidProgramCounter;
    EZrExecutionDiagnosticCode materializationCode;
    TZrExecIrBlockId materializationBlockId;
    TZrExecIrInstructionId materializationInstructionId;
    TZrExecIrSourceId materializationSourceId;
    TZrInt64 oracleInteger;
    TZrFloat64 oracleFloat;
    TZrInt64 vmInteger;
    TZrUInt32 oracleEventCount;
    TZrUInt32 traceCount;
    TZrUInt32 pathCount;
    TZrUInt32 phiTemporaryMoveCount;
    TZrUInt32 path[ZR_TEST_EXECBC_PATH_CAPACITY];
} SZrTestExecBcReport;

#include "test_ssa_execbc_vm_trace.inc"
#include "test_ssa_execbc_vm_fixtures.inc"
#include "test_ssa_execbc_vm_cfg_mutations.inc"

static TZrBool remap_to_sparse_slots(SZrExecBcProjection *projection) {
    TZrUInt32 index;
    TZrUInt32 oldPhysicalCount;
    TZrUInt32 newPhysicalCount;
    TZrUInt32 *newValueSlots;
    SZrExecIrValue *newSlotValues;

    if (projection == ZR_NULL || projection->phiMoveCount != 0u ||
        projection->temporarySlotCount != 0u) {
        return ZR_FALSE;
    }
    oldPhysicalCount = projection->physicalSlotCount;
    if (oldPhysicalCount > (UINT32_MAX - 3u) / 2u) {
        return ZR_FALSE;
    }
    newPhysicalCount = oldPhysicalCount * 2u + 3u;
    if ((size_t)projection->valueSlotCount >
                SIZE_MAX / sizeof(*newValueSlots) ||
        (size_t)newPhysicalCount > SIZE_MAX / sizeof(*newSlotValues)) {
        return ZR_FALSE;
    }
    newValueSlots = (TZrUInt32 *)malloc(
            sizeof(*newValueSlots) * projection->valueSlotCount);
    newSlotValues = (SZrExecIrValue *)calloc(
            newPhysicalCount, sizeof(*newSlotValues));
    if ((projection->valueSlotCount != 0u && newValueSlots == ZR_NULL) ||
        (newPhysicalCount != 0u && newSlotValues == ZR_NULL)) {
        free(newValueSlots);
        free(newSlotValues);
        return ZR_FALSE;
    }
    for (index = 0u; index < projection->valueSlotCount; ++index) {
        TZrUInt32 oldSlot = projection->valueSlots[index];
        TZrUInt32 newSlot;
        if (oldSlot >= oldPhysicalCount || oldSlot > (UINT32_MAX - 3u) / 2u) {
            free(newValueSlots);
            free(newSlotValues);
            return ZR_FALSE;
        }
        newSlot = oldSlot * 2u + 3u;
        newValueSlots[index] = newSlot;
        newSlotValues[newSlot] = projection->slotValues[oldSlot];
    }
    free(projection->valueSlots);
    free(projection->slotValues);
    projection->valueSlots = newValueSlots;
    projection->slotValues = newSlotValues;
    projection->physicalSlotCount = newPhysicalCount;
    return ZR_TRUE;
}

static void initialize_constant_pool(
        TZrBool conditionValue,
        SZrExecIrConstant constants[4],
        SZrExecIrOracleValue oracleValues[4]) {
    memset(constants, 0, sizeof(*constants) * 4u);
    memset(oracleValues, 0, sizeof(*oracleValues) * 4u);

    constants[0].typeToken = ZR_VALUE_TYPE_INT64;
    constants[0].bits = 30u;
    oracleValues[0].kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    oracleValues[0].as.signedInteger = 30;

    constants[1].typeToken = ZR_VALUE_TYPE_INT64;
    constants[1].bits = 12u;
    oracleValues[1].kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    oracleValues[1].as.signedInteger = 12;

    constants[2].typeToken = ZR_VALUE_TYPE_BOOL;
    constants[2].bits = conditionValue != ZR_FALSE ? 1u : 0u;
    oracleValues[2].kind = ZR_EXEC_IR_ORACLE_VALUE_BOOL;
    oracleValues[2].as.boolean = conditionValue;

    constants[3].typeToken = ZR_VALUE_TYPE_INT64;
    constants[3].bits = (TZrUInt64)(TZrInt64)-17;
    oracleValues[3].kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    oracleValues[3].as.signedInteger = -17;
}

static SZrTestExecBcReport run_success_fixture(
        EZrTestExecBcFixture fixture,
        TZrInt64 leftLiteral,
        TZrInt64 rightLiteral,
        TZrBool sparseSlots) {
    SZrTestExecBcReport report;
    SZrState *state = ZR_NULL;
    SZrExecIrFunction function;
    SZrExecBcProjection projection;
    SZrExecBcVmEmission emission;
    SZrExecIrOracleInput oracleInput;
    SZrExecIrOracleExecutionResult oracleResult;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrConstant poolConstants[4];
    SZrExecIrOracleValue oraclePoolValues[4];
    SZrTestExecBcTrace trace;
    TZrExecIrValueId returnValue = 0u;
    TZrExecIrSourceId expectedSource = 0u;
    TZrUInt32 originalBlockCount = 0u;
    TZrUInt32 requiredFrameSlots = 0u;
    TZrBool functionInitialized = ZR_FALSE;
    TZrBool functionRooted = ZR_FALSE;
    TZrBool usesConstantPool = ZR_FALSE;

    memset(&report, 0, sizeof(report));
    memset(&function, 0, sizeof(function));
    memset(&projection, 0, sizeof(projection));
    memset(&emission, 0, sizeof(emission));
    memset(&oracleInput, 0, sizeof(oracleInput));
    memset(&oracleResult, 0, sizeof(oracleResult));
    memset(&diagnostic, 0, sizeof(diagnostic));
    memset(poolConstants, 0, sizeof(poolConstants));
    memset(oraclePoolValues, 0, sizeof(oraclePoolValues));
    memset(&trace, 0, sizeof(trace));
    ZrCore_ExecIr_OracleResultInit(&oracleResult);

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    if (state == ZR_NULL) {
        goto cleanup;
    }
    switch (fixture) {
        case ZR_TEST_EXECBC_FIXTURE_ADD_SUB:
            report.fixtureBuilt = build_add_sub_function(&function, &returnValue);
            expectedSource = 103u;
            break;
        case ZR_TEST_EXECBC_FIXTURE_COMPARE_PHI:
            report.fixtureBuilt = build_compare_phi_function(
                    &function, leftLiteral, rightLiteral, &returnValue);
            expectedSource = 203u;
            break;
        case ZR_TEST_EXECBC_FIXTURE_SLOT_ZERO_PHI:
            report.fixtureBuilt = build_slot_zero_phi_function(
                    &function, &returnValue);
            expectedSource = 509u;
            break;
        case ZR_TEST_EXECBC_FIXTURE_CRITICAL_EDGE:
            report.fixtureBuilt = build_critical_edge_function(
                    &function, &returnValue);
            expectedSource = 305u;
            break;
        case ZR_TEST_EXECBC_FIXTURE_LOOP_SWAP:
            report.fixtureBuilt = build_loop_swap_function(
                    &function, &returnValue);
            expectedSource = 408u;
            break;
        case ZR_TEST_EXECBC_FIXTURE_NONFIRST_ENTRY:
            report.fixtureBuilt = build_nonfirst_entry_function(
                    &function, &returnValue);
            expectedSource = 706u;
            break;
        case ZR_TEST_EXECBC_FIXTURE_CONSTANT_POOL_TRUE:
        case ZR_TEST_EXECBC_FIXTURE_CONSTANT_POOL_FALSE:
            report.fixtureBuilt = build_constant_pool_function(
                    &function, &returnValue);
            expectedSource = fixture ==
                            ZR_TEST_EXECBC_FIXTURE_CONSTANT_POOL_TRUE
                    ? 807u
                    : 808u;
            usesConstantPool = ZR_TRUE;
            break;
        default:
            goto cleanup;
    }
    functionInitialized = ZR_TRUE;
    if (!report.fixtureBuilt) {
        goto cleanup;
    }
    originalBlockCount = function.blockCount;

    if (usesConstantPool != ZR_FALSE) {
        initialize_constant_pool(
                (TZrBool)(fixture ==
                          ZR_TEST_EXECBC_FIXTURE_CONSTANT_POOL_TRUE),
                poolConstants, oraclePoolValues);
        oracleInput.constants = oraclePoolValues;
        oracleInput.constantCount = 4u;
    }
    oracleInput.function = &function;
    oracleInput.maxSteps = 256u;
    report.oracleSucceeded = ZrCore_ExecIr_RunOracleEx(
            &oracleInput, &oracleResult, &diagnostic);
    report.oracleReturned = (TZrBool)(report.oracleSucceeded &&
                                      oracleResult.returned &&
                                      oracleResult.returnValue.kind ==
                                              ZR_EXEC_IR_ORACLE_VALUE_SIGNED);
    report.oracleEventCount = oracleResult.eventCount;
    if (report.oracleReturned) {
        report.oracleInteger = oracleResult.returnValue.as.signedInteger;
    }
    if (!report.oracleReturned) {
        goto cleanup;
    }
    if (usesConstantPool != ZR_FALSE) {
        if (!ZrParser_ExecIr_BuildProjectionWithConstants(
                    &function, poolConstants, 4u, &projection, &diagnostic)) {
            goto cleanup;
        }
    } else if (!ZrParser_ExecIr_LowerExecBc(
                       &function, &projection, &diagnostic)) {
        goto cleanup;
    }
    report.projectionSucceeded = ZR_TRUE;
    if (sparseSlots != ZR_FALSE &&
        !remap_to_sparse_slots(&projection)) {
        goto cleanup;
    }
    if (fixture == ZR_TEST_EXECBC_FIXTURE_LOOP_SWAP) {
        TZrUInt32 projectionSlotLimit;
        TZrUInt32 moveIndex;
        if (projection.temporarySlotCount == 0u ||
            projection.physicalSlotCount >
                    UINT32_MAX - projection.temporarySlotCount) {
            goto cleanup;
        }
        projectionSlotLimit = projection.physicalSlotCount +
                              projection.temporarySlotCount;
        report.phiTemporaryIsAllocated = (TZrBool)(
                projection.phiTemporarySlot >= projection.physicalSlotCount &&
                projection.phiTemporarySlot < projectionSlotLimit);
        for (moveIndex = 0u; moveIndex < projection.phiMoveCount; ++moveIndex) {
            if (projection.phiMoves[moveIndex].sourceSlot ==
                        projection.phiTemporarySlot ||
                projection.phiMoves[moveIndex].destinationSlot ==
                        projection.phiTemporarySlot) {
                ++report.phiTemporaryMoveCount;
            }
        }
    }
    if (fixture == ZR_TEST_EXECBC_FIXTURE_SLOT_ZERO_PHI) {
        TZrUInt32 moveIndex;
        if (projection.temporarySlotCount == 0u &&
            projection.phiTemporarySlot == 0u) {
            for (moveIndex = 0u; moveIndex < projection.phiMoveCount; ++moveIndex) {
                if (projection.phiMoves[moveIndex].sourceSlot == 0u &&
                    projection.phiMoves[moveIndex].destinationSlot != 0u) {
                    report.phiReadsPhysicalSlotZero = ZR_TRUE;
                    break;
                }
            }
        }
    }
    if (returnValue == 0u || returnValue > projection.valueSlotCount) {
        goto cleanup;
    }
    report.returnValueUsesSlotZero = (TZrBool)(
            projection.valueSlots[returnValue - 1u] == 0u);
    report.returnValueUsesSparseSlot = (TZrBool)(
            sparseSlots != ZR_FALSE &&
            projection.valueSlots[returnValue - 1u] > 0u);

    if (!ZrParser_ExecBcProjection_MaterializeVmFunction(
                state, &projection, &emission, &diagnostic)) {
        report.materializationCode = diagnostic.code;
        report.outputWasEmptyOnFailure = (TZrBool)(
                emission.function == ZR_NULL && emission.pcMap == ZR_NULL &&
                emission.pcMapCount == 0u);
        goto cleanup;
    }
    report.materializationSucceeded = ZR_TRUE;
    if (emission.function != ZR_NULL) {
        functionRooted = ZrCore_GarbageCollector_IgnoreObject(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(emission.function));
    }
    if (functionRooted == ZR_FALSE) {
        goto cleanup;
    }
    if (emission.function == ZR_NULL || emission.pcMap == ZR_NULL ||
        emission.pcMapCount == 0u) {
        goto cleanup;
    }
    requiredFrameSlots = projection.physicalSlotCount;
    if (projection.temporarySlotCount >
            UINT32_MAX - requiredFrameSlots) {
        goto cleanup;
    }
    requiredFrameSlots += projection.temporarySlotCount;
    if (projection.temporarySlotCount != 0u) {
        if (projection.phiTemporarySlot == UINT32_MAX) {
            goto cleanup;
        }
        if (requiredFrameSlots < projection.phiTemporarySlot + 1u) {
            requiredFrameSlots = projection.phiTemporarySlot + 1u;
        }
    }
    report.frameCoversProjection = (TZrBool)(
            emission.function->parameterCount == 0u &&
            emission.function->stackSize >= requiredFrameSlots);
    report.frameClearsAllSlots = (TZrBool)(
            emission.function->stackSize < UINT32_MAX &&
            emission.function->vmEntryClearStackSizePlusOne ==
                    emission.function->stackSize + 1u);
    report.frameHasNoPackedLayout = (TZrBool)(
            emission.function->frameByteSize == 0u &&
            emission.function->frameSlotLayouts == ZR_NULL &&
            emission.function->frameSlotLayoutLength == 0u);
    if (fixture == ZR_TEST_EXECBC_FIXTURE_LOOP_SWAP &&
        projection.temporarySlotCount > 0u) {
        report.phiTemporaryFitsFrame = (TZrBool)(
                projection.phiTemporarySlot < emission.function->stackSize);
    }

    trace.expectedFunction = emission.function;
    ZrCore_Debug_SetTraceObserver(state, record_vm_instruction, &trace);
    report.vmSucceeded = ZrTests_Runtime_Function_ExecuteExpectInt64(
            state, emission.function, &report.vmInteger);
    ZrCore_Debug_SetTraceObserver(state, ZR_NULL, ZR_NULL);
    report.traceCount = trace.count;
    report.traceOverflow = trace.overflow;
    report.invalidProgramCounter = trace.invalidProgramCounter;
    if (report.vmSucceeded) {
        collect_vm_trace(&trace, &emission, originalBlockCount,
                         expectedSource, &report);
    }

cleanup:
    if (state != ZR_NULL) {
        ZrCore_Debug_SetTraceObserver(state, ZR_NULL, ZR_NULL);
        if (functionRooted != ZR_FALSE && emission.function != ZR_NULL) {
            ZrCore_GarbageCollector_UnignoreObject(
                    state->global,
                    ZR_CAST_RAW_OBJECT_AS_SUPER(emission.function));
        }
        ZrParser_ExecBcVmEmission_Free(state, &emission);
    }
    ZrParser_ExecBcProjection_Free(&projection);
    ZrCore_ExecIr_OracleResultFree(&oracleResult);
    if (functionInitialized != ZR_FALSE || function.values != ZR_NULL ||
        function.instructions != ZR_NULL || function.blocks != ZR_NULL) {
        ZrCore_ExecIr_FreeFunction(&function);
    }
    ZrTests_Runtime_State_Destroy(state);
    return report;
}

static SZrTestExecBcReport run_rejected_fixture(
        EZrTestExecBcFixture fixture) {
    SZrTestExecBcReport report;
    SZrState *state = ZR_NULL;
    SZrExecIrFunction function;
    SZrExecBcProjection projection;
    SZrExecBcVmEmission emission;
    SZrExecIrOracleInput oracleInput;
    SZrExecIrOracleExecutionResult oracleResult;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrConstant constants[2];
    SZrExecIrOracleValue oracleConstants[2];
    TZrBool functionInitialized = ZR_FALSE;
    TZrBool functionRooted = ZR_FALSE;
    TZrExecIrValueId returnValue = 0u;

    memset(&report, 0, sizeof(report));
    memset(&function, 0, sizeof(function));
    memset(&projection, 0, sizeof(projection));
    memset(&emission, 0, sizeof(emission));
    memset(&oracleInput, 0, sizeof(oracleInput));
    memset(&oracleResult, 0, sizeof(oracleResult));
    memset(&diagnostic, 0, sizeof(diagnostic));
    memset(constants, 0, sizeof(constants));
    memset(oracleConstants, 0, sizeof(oracleConstants));
    ZrCore_ExecIr_OracleResultInit(&oracleResult);

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    if (state == ZR_NULL) {
        goto cleanup;
    }
    if (fixture == ZR_TEST_EXECBC_FIXTURE_CALL) {
        report.fixtureBuilt = build_call_function(&function);
    } else if (fixture == ZR_TEST_EXECBC_FIXTURE_DOUBLE) {
        report.fixtureBuilt = build_double_function(&function);
    } else if (fixture == ZR_TEST_EXECBC_FIXTURE_ADD_SUB) {
        report.fixtureBuilt = build_add_sub_function(&function, ZR_NULL);
    } else if (fixture == ZR_TEST_EXECBC_FIXTURE_DEOPT_ID) {
        report.fixtureBuilt = build_add_sub_function(&function, ZR_NULL);
    } else if (fixture == ZR_TEST_EXECBC_FIXTURE_PHI_MISSING_MOVE ||
               fixture == ZR_TEST_EXECBC_FIXTURE_PHI_WRONG_SOURCE) {
        report.fixtureBuilt = build_slot_zero_phi_function(
                &function, &returnValue);
    } else if (fixture ==
               ZR_TEST_EXECBC_FIXTURE_PHI_CFG_MISSING_PREDECESSOR) {
        report.fixtureBuilt = build_compare_phi_function(
                &function, 5, 3, &returnValue);
    } else if (fixture == ZR_TEST_EXECBC_FIXTURE_SYNTHETIC_BLOCK_BODY) {
        report.fixtureBuilt = build_critical_edge_function(
                &function, &returnValue);
    } else {
        goto cleanup;
    }
    functionInitialized = ZR_TRUE;
    if (!report.fixtureBuilt) {
        goto cleanup;
    }

    if (fixture == ZR_TEST_EXECBC_FIXTURE_DOUBLE) {
        constants[0].typeToken = ZR_VALUE_TYPE_DOUBLE;
        constants[0].bits = double_bits(1.25);
        constants[1].typeToken = ZR_VALUE_TYPE_DOUBLE;
        constants[1].bits = double_bits(2.75);
        oracleConstants[0].kind = ZR_EXEC_IR_ORACLE_VALUE_FLOAT;
        oracleConstants[0].as.floating = 1.25;
        oracleConstants[1].kind = ZR_EXEC_IR_ORACLE_VALUE_FLOAT;
        oracleConstants[1].as.floating = 2.75;
        oracleInput.function = &function;
        oracleInput.constants = oracleConstants;
        oracleInput.constantCount = 2u;
        oracleInput.maxSteps = 32u;
        report.oracleSucceeded = ZrCore_ExecIr_RunOracleEx(
                &oracleInput, &oracleResult, &diagnostic);
        report.oracleReturned = (TZrBool)(report.oracleSucceeded &&
                                          oracleResult.returned &&
                                          oracleResult.returnValue.kind ==
                                                  ZR_EXEC_IR_ORACLE_VALUE_FLOAT);
        report.oracleEventCount = oracleResult.eventCount;
        if (report.oracleReturned) {
            report.oracleFloat = oracleResult.returnValue.as.floating;
        }
        if (!report.oracleReturned ||
            !ZrParser_ExecIr_BuildProjectionWithConstants(
                    &function, constants, 2u, &projection, &diagnostic)) {
            goto cleanup;
        }
    } else {
        if (fixture == ZR_TEST_EXECBC_FIXTURE_PHI_MISSING_MOVE ||
            fixture == ZR_TEST_EXECBC_FIXTURE_PHI_WRONG_SOURCE ||
            fixture ==
                    ZR_TEST_EXECBC_FIXTURE_PHI_CFG_MISSING_PREDECESSOR ||
            fixture == ZR_TEST_EXECBC_FIXTURE_SYNTHETIC_BLOCK_BODY) {
            oracleInput.function = &function;
            oracleInput.maxSteps = 256u;
            report.oracleSucceeded = ZrCore_ExecIr_RunOracleEx(
                    &oracleInput, &oracleResult, &diagnostic);
            report.oracleReturned = (TZrBool)(
                    report.oracleSucceeded && oracleResult.returned &&
                    oracleResult.returnValue.kind ==
                            ZR_EXEC_IR_ORACLE_VALUE_SIGNED);
            report.oracleEventCount = oracleResult.eventCount;
            if (report.oracleReturned) {
                report.oracleInteger =
                        oracleResult.returnValue.as.signedInteger;
            }
            if (!report.oracleReturned) {
                goto cleanup;
            }
        }
        if (!ZrParser_ExecIr_LowerExecBc(
                    &function, &projection, &diagnostic)) {
            goto cleanup;
        }
    }
    report.projectionSucceeded = ZR_TRUE;

    if (fixture == ZR_TEST_EXECBC_FIXTURE_ADD_SUB) {
        /* The fixed-width opcode is deliberately outside the ExecIR enum. */
        projection.instructions[0].opcode = UINT16_MAX;
        projection.opcodes[0] = UINT16_MAX;
    } else if (fixture == ZR_TEST_EXECBC_FIXTURE_DEOPT_ID) {
        projection.instructions[0].deoptId = 1u;
        report.mutationApplied = ZR_TRUE;
    } else if (fixture == ZR_TEST_EXECBC_FIXTURE_PHI_MISSING_MOVE) {
        projection.phiMoveCount = 0u;
        report.mutationApplied = ZR_TRUE;
    } else if (fixture == ZR_TEST_EXECBC_FIXTURE_PHI_WRONG_SOURCE) {
        TZrUInt32 moveIndex;
        for (moveIndex = 0u; moveIndex < projection.phiMoveCount; ++moveIndex) {
            if (projection.phiMoves[moveIndex].sourceSlot == 0u) {
                projection.phiMoves[moveIndex].sourceSlot = 1u;
                report.mutationApplied = ZR_TRUE;
                break;
            }
        }
    } else if (fixture ==
               ZR_TEST_EXECBC_FIXTURE_PHI_CFG_MISSING_PREDECESSOR) {
        report.mutationApplied =
                drop_phi_predecessor_without_removing_cfg_edge(&projection);
    } else if (fixture == ZR_TEST_EXECBC_FIXTURE_SYNTHETIC_BLOCK_BODY) {
        if (projection.blockCount > projection.syntheticBlockCount &&
            projection.syntheticBlockCount != 0u) {
            SZrExecBcBlock *side = &projection.blocks[1u];
            SZrExecBcBlock *synthetic =
                    &projection.blocks[projection.blockCount - 1u];
            if (side->instructions.count >= 2u &&
                side->terminatorInstructionId !=
                        ZR_EXEC_IR_INSTRUCTION_ID_INVALID &&
                synthetic->instructions.count == 0u &&
                synthetic->terminatorInstructionId ==
                        ZR_EXEC_IR_INSTRUCTION_ID_INVALID) {
                TZrUInt32 branchIndex =
                        side->instructions.start + side->instructions.count - 1u;
                synthetic->instructions.start = branchIndex;
                synthetic->instructions.count = 1u;
                synthetic->terminatorInstructionId =
                        side->terminatorInstructionId;
                projection.instructions[branchIndex].successorRange =
                        synthetic->successors;
                --side->instructions.count;
                side->terminatorInstructionId =
                        ZR_EXEC_IR_INSTRUCTION_ID_INVALID;
                report.mutationApplied = ZR_TRUE;
            }
        }
    }
    if (ZrParser_ExecBcProjection_MaterializeVmFunction(
                state, &projection, &emission, &diagnostic)) {
        report.materializationSucceeded = ZR_TRUE;
        if (emission.function != ZR_NULL) {
            functionRooted = ZrCore_GarbageCollector_IgnoreObject(
                    state, ZR_CAST_RAW_OBJECT_AS_SUPER(emission.function));
        }
    }
    report.materializationCode = diagnostic.code;
    report.materializationBlockId = diagnostic.blockId;
    report.materializationInstructionId = diagnostic.instructionId;
    report.materializationSourceId = diagnostic.sourceId;
    report.outputWasEmptyOnFailure = (TZrBool)(
            report.materializationSucceeded == ZR_FALSE &&
            emission.function == ZR_NULL && emission.pcMap == ZR_NULL &&
            emission.pcMapCount == 0u);

cleanup:
    if (state != ZR_NULL) {
        if (functionRooted != ZR_FALSE && emission.function != ZR_NULL) {
            ZrCore_GarbageCollector_UnignoreObject(
                    state->global,
                    ZR_CAST_RAW_OBJECT_AS_SUPER(emission.function));
        }
        ZrParser_ExecBcVmEmission_Free(state, &emission);
    }
    ZrParser_ExecBcProjection_Free(&projection);
    ZrCore_ExecIr_OracleResultFree(&oracleResult);
    if (functionInitialized != ZR_FALSE || function.values != ZR_NULL ||
        function.instructions != ZR_NULL || function.blocks != ZR_NULL) {
        ZrCore_ExecIr_FreeFunction(&function);
    }
    ZrTests_Runtime_State_Destroy(state);
    return report;
}

static void test_zero_parameter_i64_add_sub_uses_frame_slot_zero(void) {
    SZrTestExecBcReport report = run_success_fixture(
            ZR_TEST_EXECBC_FIXTURE_ADD_SUB, 0, 0, ZR_FALSE);

    TEST_ASSERT_TRUE(report.fixtureBuilt);
    TEST_ASSERT_TRUE(report.oracleReturned);
    TEST_ASSERT_EQUAL_INT64(35, report.oracleInteger);
    TEST_ASSERT_EQUAL_UINT32(0u, report.oracleEventCount);
    TEST_ASSERT_TRUE(report.projectionSucceeded);
    TEST_ASSERT_TRUE(report.returnValueUsesSlotZero);
    TEST_ASSERT_TRUE(report.materializationSucceeded);
    TEST_ASSERT_TRUE(report.frameCoversProjection);
    TEST_ASSERT_TRUE(report.frameClearsAllSlots);
    TEST_ASSERT_TRUE(report.frameHasNoPackedLayout);
    TEST_ASSERT_TRUE(report.vmSucceeded);
    TEST_ASSERT_EQUAL_INT64(report.oracleInteger, report.vmInteger);
    TEST_ASSERT_TRUE(report.pcMapValid);
    TEST_ASSERT_TRUE(report.sawExpectedSource);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, report.traceCount);
    TEST_ASSERT_FALSE(report.traceOverflow);
    TEST_ASSERT_FALSE(report.invalidProgramCounter);
    {
        const TZrUInt32 expectedPath[1] = {1u};
        TEST_ASSERT_TRUE(path_equals(&report, expectedPath, 1u));
    }
}

static void test_nonfirst_entry_and_parallel_cfg_edges_run_through_vm(void) {
    SZrTestExecBcReport report = run_success_fixture(
            ZR_TEST_EXECBC_FIXTURE_NONFIRST_ENTRY, 0, 0, ZR_FALSE);
    const TZrUInt32 expectedPath[3] = {2u, 3u, 1u};

    TEST_ASSERT_TRUE(report.fixtureBuilt);
    TEST_ASSERT_TRUE(report.oracleReturned);
    TEST_ASSERT_EQUAL_INT64(42, report.oracleInteger);
    TEST_ASSERT_TRUE(report.projectionSucceeded);
    TEST_ASSERT_TRUE(report.materializationSucceeded);
    TEST_ASSERT_TRUE(report.vmSucceeded);
    TEST_ASSERT_EQUAL_INT64(report.oracleInteger, report.vmInteger);
    TEST_ASSERT_TRUE(report.pcMapValid);
    TEST_ASSERT_TRUE(report.sawExpectedSource);
    if (!path_equals(&report, expectedPath, 3u)) {
        TZrUInt32 pathIndex;
        (void)fprintf(stderr, "nonfirst parallel-edge path count=%u ids=",
                      (unsigned int)report.pathCount);
        for (pathIndex = 0u;
             pathIndex < report.pathCount &&
             pathIndex < ZR_TEST_EXECBC_PATH_CAPACITY;
             ++pathIndex) {
            (void)fprintf(stderr, "%s%u", pathIndex == 0u ? "" : ",",
                          (unsigned int)report.path[pathIndex]);
        }
        (void)fprintf(stderr, "\n");
    }
    TEST_ASSERT_EQUAL_UINT32(3u, report.pathCount);
    TEST_ASSERT_EQUAL_UINT32(expectedPath[0], report.path[0]);
    TEST_ASSERT_EQUAL_UINT32(expectedPath[1], report.path[1]);
    TEST_ASSERT_EQUAL_UINT32(expectedPath[2], report.path[2]);
}

static void test_i64_and_bool_constant_pool_entries_match_oracle(void) {
    SZrTestExecBcReport trueReport = run_success_fixture(
            ZR_TEST_EXECBC_FIXTURE_CONSTANT_POOL_TRUE, 0, 0, ZR_FALSE);
    SZrTestExecBcReport falseReport = run_success_fixture(
            ZR_TEST_EXECBC_FIXTURE_CONSTANT_POOL_FALSE, 0, 0, ZR_FALSE);
    const TZrUInt32 truePath[2] = {1u, 2u};
    const TZrUInt32 falsePath[2] = {1u, 3u};

    TEST_ASSERT_TRUE(trueReport.fixtureBuilt);
    TEST_ASSERT_TRUE(falseReport.fixtureBuilt);
    TEST_ASSERT_TRUE(trueReport.oracleReturned);
    TEST_ASSERT_TRUE(falseReport.oracleReturned);
    TEST_ASSERT_EQUAL_INT64(42, trueReport.oracleInteger);
    TEST_ASSERT_EQUAL_INT64(-17, falseReport.oracleInteger);
    TEST_ASSERT_TRUE(trueReport.projectionSucceeded);
    TEST_ASSERT_TRUE(falseReport.projectionSucceeded);
    TEST_ASSERT_TRUE(trueReport.materializationSucceeded);
    TEST_ASSERT_TRUE(falseReport.materializationSucceeded);
    TEST_ASSERT_TRUE(trueReport.vmSucceeded);
    TEST_ASSERT_TRUE(falseReport.vmSucceeded);
    TEST_ASSERT_EQUAL_INT64(trueReport.oracleInteger, trueReport.vmInteger);
    TEST_ASSERT_EQUAL_INT64(falseReport.oracleInteger, falseReport.vmInteger);
    TEST_ASSERT_TRUE(trueReport.pcMapValid);
    TEST_ASSERT_TRUE(falseReport.pcMapValid);
    TEST_ASSERT_TRUE(trueReport.sawExpectedSource);
    TEST_ASSERT_TRUE(falseReport.sawExpectedSource);
    TEST_ASSERT_TRUE(path_equals(&trueReport, truePath, 2u));
    TEST_ASSERT_TRUE(path_equals(&falseReport, falsePath, 2u));
}

static void test_compare_phi_executes_true_and_false_vm_paths(void) {
    SZrTestExecBcReport trueReport = run_success_fixture(
            ZR_TEST_EXECBC_FIXTURE_COMPARE_PHI, 5, 9, ZR_FALSE);
    SZrTestExecBcReport falseReport = run_success_fixture(
            ZR_TEST_EXECBC_FIXTURE_COMPARE_PHI, 9, 5, ZR_FALSE);
    const TZrUInt32 truePath[3] = {1u, 2u, 4u};
    const TZrUInt32 falsePath[3] = {1u, 3u, 4u};

    TEST_ASSERT_TRUE(trueReport.fixtureBuilt);
    TEST_ASSERT_TRUE(falseReport.fixtureBuilt);
    TEST_ASSERT_TRUE(trueReport.oracleReturned);
    TEST_ASSERT_TRUE(falseReport.oracleReturned);
    TEST_ASSERT_EQUAL_INT64(111, trueReport.oracleInteger);
    TEST_ASSERT_EQUAL_INT64(222, falseReport.oracleInteger);
    TEST_ASSERT_EQUAL_UINT32(0u, trueReport.oracleEventCount);
    TEST_ASSERT_EQUAL_UINT32(0u, falseReport.oracleEventCount);
    TEST_ASSERT_TRUE(trueReport.returnValueUsesSlotZero);
    TEST_ASSERT_TRUE(falseReport.returnValueUsesSlotZero);
    TEST_ASSERT_TRUE(trueReport.vmSucceeded);
    TEST_ASSERT_TRUE(falseReport.vmSucceeded);
    TEST_ASSERT_EQUAL_INT64(trueReport.oracleInteger, trueReport.vmInteger);
    TEST_ASSERT_EQUAL_INT64(falseReport.oracleInteger, falseReport.vmInteger);
    TEST_ASSERT_TRUE(trueReport.pcMapValid);
    TEST_ASSERT_TRUE(falseReport.pcMapValid);
    TEST_ASSERT_TRUE(trueReport.sawExpectedSource);
    TEST_ASSERT_TRUE(falseReport.sawExpectedSource);
    TEST_ASSERT_TRUE(path_equals(&trueReport, truePath, 3u));
    TEST_ASSERT_TRUE(path_equals(&falseReport, falsePath, 3u));
}

static void test_no_temp_phi_move_reads_physical_slot_zero(void) {
    SZrTestExecBcReport report = run_success_fixture(
            ZR_TEST_EXECBC_FIXTURE_SLOT_ZERO_PHI, 0, 0, ZR_FALSE);
    const TZrUInt32 expectedPath[3] = {1u, 2u, 4u};

    TEST_ASSERT_TRUE(report.fixtureBuilt);
    TEST_ASSERT_TRUE(report.oracleReturned);
    TEST_ASSERT_EQUAL_INT64(42, report.oracleInteger);
    TEST_ASSERT_TRUE(report.projectionSucceeded);
    TEST_ASSERT_TRUE(report.phiReadsPhysicalSlotZero);
    TEST_ASSERT_TRUE(report.materializationSucceeded);
    TEST_ASSERT_TRUE(report.vmSucceeded);
    TEST_ASSERT_EQUAL_INT64(report.oracleInteger, report.vmInteger);
    TEST_ASSERT_TRUE(report.pcMapValid);
    TEST_ASSERT_TRUE(report.sawExpectedSource);
    TEST_ASSERT_TRUE(path_equals(&report, expectedPath, 3u));
}

static void test_critical_edge_phi_runs_through_synthetic_block(void) {
    SZrTestExecBcReport report = run_success_fixture(
            ZR_TEST_EXECBC_FIXTURE_CRITICAL_EDGE, 0, 0, ZR_FALSE);

    TEST_ASSERT_TRUE(report.fixtureBuilt);
    TEST_ASSERT_TRUE(report.oracleReturned);
    TEST_ASSERT_EQUAL_INT64(13, report.oracleInteger);
    TEST_ASSERT_EQUAL_UINT32(0u, report.oracleEventCount);
    TEST_ASSERT_TRUE(report.projectionSucceeded);
    TEST_ASSERT_TRUE(report.materializationSucceeded);
    TEST_ASSERT_TRUE(report.vmSucceeded);
    TEST_ASSERT_EQUAL_INT64(report.oracleInteger, report.vmInteger);
    TEST_ASSERT_TRUE(report.pcMapValid);
    TEST_ASSERT_TRUE(report.sawExpectedSource);
    TEST_ASSERT_TRUE(report.sawSyntheticBlock);
    TEST_ASSERT_GREATER_THAN_UINT32(2u, report.pathCount);
    TEST_ASSERT_EQUAL_UINT32(1u, report.path[0]);
    TEST_ASSERT_EQUAL_UINT32(3u, report.path[report.pathCount - 1u]);
}

static void test_loop_swap_phi_uses_vm_temporary_slot(void) {
    SZrTestExecBcReport report = run_success_fixture(
            ZR_TEST_EXECBC_FIXTURE_LOOP_SWAP, 0, 0, ZR_FALSE);
    const TZrUInt32 expectedPath[9] = {
        1u, 2u, 3u, 2u, 3u, 2u, 3u, 2u, 4u
    };

    TEST_ASSERT_TRUE(report.fixtureBuilt);
    TEST_ASSERT_TRUE(report.oracleReturned);
    TEST_ASSERT_EQUAL_INT64(3, report.oracleInteger);
    TEST_ASSERT_EQUAL_UINT32(0u, report.oracleEventCount);
    TEST_ASSERT_TRUE(report.projectionSucceeded);
    TEST_ASSERT_TRUE(report.phiTemporaryIsAllocated);
    TEST_ASSERT_GREATER_THAN_UINT32(1u, report.phiTemporaryMoveCount);
    TEST_ASSERT_TRUE(report.materializationSucceeded);
    TEST_ASSERT_TRUE(report.frameCoversProjection);
    TEST_ASSERT_TRUE(report.frameClearsAllSlots);
    TEST_ASSERT_TRUE(report.phiTemporaryFitsFrame);
    TEST_ASSERT_TRUE(report.vmSucceeded);
    TEST_ASSERT_EQUAL_INT64(report.oracleInteger, report.vmInteger);
    TEST_ASSERT_TRUE(report.pcMapValid);
    TEST_ASSERT_TRUE(report.sawExpectedSource);
    TEST_ASSERT_TRUE(path_equals(&report, expectedPath, 9u));
}

static void test_sparse_physical_slots_run_and_clear_complete_frame(void) {
    SZrTestExecBcReport report = run_success_fixture(
            ZR_TEST_EXECBC_FIXTURE_ADD_SUB, 0, 0, ZR_TRUE);

    TEST_ASSERT_TRUE(report.fixtureBuilt);
    TEST_ASSERT_TRUE(report.oracleReturned);
    TEST_ASSERT_EQUAL_INT64(35, report.oracleInteger);
    TEST_ASSERT_TRUE(report.projectionSucceeded);
    TEST_ASSERT_TRUE(report.returnValueUsesSparseSlot);
    TEST_ASSERT_TRUE(report.materializationSucceeded);
    TEST_ASSERT_TRUE(report.frameCoversProjection);
    TEST_ASSERT_TRUE(report.frameClearsAllSlots);
    TEST_ASSERT_TRUE(report.vmSucceeded);
    TEST_ASSERT_EQUAL_INT64(report.oracleInteger, report.vmInteger);
    TEST_ASSERT_TRUE(report.pcMapValid);
}

static void test_call_is_unsupported_and_never_published(void) {
    SZrTestExecBcReport report = run_rejected_fixture(
            ZR_TEST_EXECBC_FIXTURE_CALL);

    TEST_ASSERT_TRUE(report.fixtureBuilt);
    TEST_ASSERT_TRUE(report.projectionSucceeded);
    TEST_ASSERT_FALSE(report.materializationSucceeded);
    TEST_ASSERT_EQUAL_INT(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                          report.materializationCode);
    TEST_ASSERT_TRUE(report.outputWasEmptyOnFailure);
}

static void test_double_type_is_unsupported_and_never_published(void) {
    SZrTestExecBcReport report = run_rejected_fixture(
            ZR_TEST_EXECBC_FIXTURE_DOUBLE);

    TEST_ASSERT_TRUE(report.fixtureBuilt);
    TEST_ASSERT_TRUE(report.oracleReturned);
    TEST_ASSERT_EQUAL_DOUBLE(4.0, report.oracleFloat);
    TEST_ASSERT_EQUAL_UINT32(0u, report.oracleEventCount);
    TEST_ASSERT_TRUE(report.projectionSucceeded);
    TEST_ASSERT_FALSE(report.materializationSucceeded);
    TEST_ASSERT_EQUAL_INT(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                          report.materializationCode);
    TEST_ASSERT_TRUE(report.outputWasEmptyOnFailure);
}

static void test_invalid_opcode_encoding_is_rejected_and_never_published(void) {
    SZrTestExecBcReport report = run_rejected_fixture(
            ZR_TEST_EXECBC_FIXTURE_ADD_SUB);

    TEST_ASSERT_TRUE(report.fixtureBuilt);
    TEST_ASSERT_TRUE(report.projectionSucceeded);
    TEST_ASSERT_FALSE(report.materializationSucceeded);
    TEST_ASSERT_EQUAL_INT(ZR_EXEC_IR_DIAGNOSTIC_UNKNOWN_OPCODE,
                          report.materializationCode);
    TEST_ASSERT_TRUE(report.outputWasEmptyOnFailure);
}

static void test_instruction_deopt_metadata_is_unsupported(void) {
    SZrTestExecBcReport report = run_rejected_fixture(
            ZR_TEST_EXECBC_FIXTURE_DEOPT_ID);

    TEST_ASSERT_TRUE(report.fixtureBuilt);
    TEST_ASSERT_TRUE(report.projectionSucceeded);
    TEST_ASSERT_TRUE(report.mutationApplied);
    TEST_ASSERT_FALSE(report.materializationSucceeded);
    TEST_ASSERT_EQUAL_INT(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                          report.materializationCode);
    TEST_ASSERT_EQUAL_UINT32(1u, report.materializationInstructionId);
    TEST_ASSERT_EQUAL_UINT32(101u, report.materializationSourceId);
    TEST_ASSERT_TRUE(report.outputWasEmptyOnFailure);
}

static void test_missing_nonidentity_phi_move_is_rejected(void) {
    SZrTestExecBcReport report = run_rejected_fixture(
            ZR_TEST_EXECBC_FIXTURE_PHI_MISSING_MOVE);

    TEST_ASSERT_TRUE(report.fixtureBuilt);
    TEST_ASSERT_TRUE(report.oracleReturned);
    TEST_ASSERT_EQUAL_INT64(42, report.oracleInteger);
    TEST_ASSERT_TRUE(report.projectionSucceeded);
    TEST_ASSERT_TRUE(report.mutationApplied);
    TEST_ASSERT_FALSE(report.materializationSucceeded);
    TEST_ASSERT_EQUAL_INT(ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                          report.materializationCode);
    TEST_ASSERT_TRUE(report.outputWasEmptyOnFailure);
}

static void test_wrong_phi_move_source_is_rejected(void) {
    SZrTestExecBcReport report = run_rejected_fixture(
            ZR_TEST_EXECBC_FIXTURE_PHI_WRONG_SOURCE);

    TEST_ASSERT_TRUE(report.fixtureBuilt);
    TEST_ASSERT_TRUE(report.oracleReturned);
    TEST_ASSERT_EQUAL_INT64(42, report.oracleInteger);
    TEST_ASSERT_TRUE(report.projectionSucceeded);
    TEST_ASSERT_TRUE(report.mutationApplied);
    TEST_ASSERT_FALSE(report.materializationSucceeded);
    TEST_ASSERT_EQUAL_INT(ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                          report.materializationCode);
    TEST_ASSERT_TRUE(report.outputWasEmptyOnFailure);
}

static void test_phi_rejects_cfg_edge_missing_from_target_predecessors(void) {
    SZrTestExecBcReport report = run_rejected_fixture(
            ZR_TEST_EXECBC_FIXTURE_PHI_CFG_MISSING_PREDECESSOR);

    TEST_ASSERT_TRUE(report.fixtureBuilt);
    TEST_ASSERT_TRUE(report.oracleReturned);
    TEST_ASSERT_TRUE(report.projectionSucceeded);
    TEST_ASSERT_TRUE(report.mutationApplied);
    TEST_ASSERT_FALSE(report.materializationSucceeded);
    TEST_ASSERT_TRUE(
            report.materializationCode == ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK ||
            report.materializationCode ==
                    ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH);
    TEST_ASSERT_TRUE(report.outputWasEmptyOnFailure);
}

static void test_synthetic_block_cannot_own_an_instruction_body(void) {
    SZrTestExecBcReport report = run_rejected_fixture(
            ZR_TEST_EXECBC_FIXTURE_SYNTHETIC_BLOCK_BODY);

    TEST_ASSERT_TRUE(report.fixtureBuilt);
    TEST_ASSERT_TRUE(report.oracleReturned);
    TEST_ASSERT_EQUAL_INT64(13, report.oracleInteger);
    TEST_ASSERT_TRUE(report.projectionSucceeded);
    TEST_ASSERT_TRUE(report.mutationApplied);
    TEST_ASSERT_FALSE(report.materializationSucceeded);
    TEST_ASSERT_EQUAL_INT(ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                          report.materializationCode);
    TEST_ASSERT_EQUAL_UINT32(4u, report.materializationBlockId);
    TEST_ASSERT_TRUE(report.outputWasEmptyOnFailure);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_zero_parameter_i64_add_sub_uses_frame_slot_zero);
    RUN_TEST(test_nonfirst_entry_and_parallel_cfg_edges_run_through_vm);
    RUN_TEST(test_i64_and_bool_constant_pool_entries_match_oracle);
    RUN_TEST(test_compare_phi_executes_true_and_false_vm_paths);
    RUN_TEST(test_no_temp_phi_move_reads_physical_slot_zero);
    RUN_TEST(test_critical_edge_phi_runs_through_synthetic_block);
    RUN_TEST(test_loop_swap_phi_uses_vm_temporary_slot);
    RUN_TEST(test_sparse_physical_slots_run_and_clear_complete_frame);
    RUN_TEST(test_call_is_unsupported_and_never_published);
    RUN_TEST(test_double_type_is_unsupported_and_never_published);
    RUN_TEST(test_invalid_opcode_encoding_is_rejected_and_never_published);
    RUN_TEST(test_instruction_deopt_metadata_is_unsupported);
    RUN_TEST(test_missing_nonidentity_phi_move_is_rejected);
    RUN_TEST(test_wrong_phi_move_source_is_rejected);
    RUN_TEST(test_phi_rejects_cfg_edge_missing_from_target_predecessors);
    RUN_TEST(test_synthetic_block_cannot_own_an_instruction_body);
    return UNITY_END();
}
