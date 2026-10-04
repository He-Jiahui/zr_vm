#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Parse Windows CRT attributes before Unity introduces C11's noreturn macro. */
#include "unity.h"

#include "harness/runtime_support.h"
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/exec_ir.h"
#include "zr_vm_core/exec_ir_interpreter.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/exec_ir_builder.h"
#include "zr_vm_parser/exec_ir_execbc_vm.h"
#include "zr_vm_parser/exec_ir_projections.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic_ir.h"

#define SOURCE_EXECBC_TRACE_CAPACITY 128u
#define SOURCE_EXECBC_PATH_CAPACITY 128u

typedef struct SZrSourceExecBcTrace {
    const SZrFunction *expectedFunction;
    TZrUInt32 offsets[SOURCE_EXECBC_TRACE_CAPACITY];
    TZrUInt32 count;
    TZrExecIrBlockId path[SOURCE_EXECBC_PATH_CAPACITY];
    TZrUInt32 pathCount;
    TZrBool overflow;
    TZrBool invalidProgramCounter;
} SZrSourceExecBcTrace;

typedef struct SZrSourceExecBcOraclePlaceContext {
    const SZrSemanticIrFunction *semanticFunction;
    TZrUInt32 placeProviderCalls;
} SZrSourceExecBcOraclePlaceContext;

typedef enum EZrSourceExecBcStage {
    ZR_SOURCE_EXECBC_STAGE_NONE = 0,
    ZR_SOURCE_EXECBC_STAGE_BUILDER,
    ZR_SOURCE_EXECBC_STAGE_IDENTITY,
    ZR_SOURCE_EXECBC_STAGE_VERIFY,
    ZR_SOURCE_EXECBC_STAGE_ORACLE,
    ZR_SOURCE_EXECBC_STAGE_PROJECTION,
    ZR_SOURCE_EXECBC_STAGE_MATERIALIZER
} EZrSourceExecBcStage;

typedef struct SZrSourceExecBcReport {
    TZrBool parsedAndCompiled;
    TZrBool preSemanticIrValidated;
    TZrBool sourceCfgValidated;
    TZrBool execIrBuilt;
    TZrBool hasConditionalBranch;
    TZrBool hasAdd;
    TZrBool hasSubtract;
    TZrBool hasMultiply;
    TZrBool multiplyHasSignedType;
    TZrBool projectionHasSignedMultiply;
    TZrUInt32 multiplyCount;
    TZrUInt32 projectionMultiplyCount;
    TZrBool oracleReturned;
    TZrBool projectionBuilt;
    TZrBool materialized;
    TZrBool vmReturned;
    TZrBool pcSourceTraceValid;
    TZrBool vmEndsAtOracleBlock;
    TZrInt64 oracleInteger;
    TZrInt64 vmInteger;
    TZrExecIrBlockId oracleBlock;
    TZrExecIrBlockId vmLastBlock;
    TZrUInt32 execIrPhiCount;
    TZrUInt32 vmPathCount;
    TZrUInt32 traceCount;
    TZrUInt32 oraclePlaceProviderCalls;
    TZrUInt32 oracleEventCount;
    TZrUInt32 conditionalBranchCount;
    TZrUInt32 loopBreakCount;
    TZrBool loopBreakTargetsValid;
    TZrUInt32 vmBreakCount;
    TZrUInt32 vmBreakOffset;
    TZrBool identityPublished;
    EZrSourceExecBcStage diagnosticStage;
    EZrExecutionDiagnosticCode diagnosticCode;
    TZrUInt32 diagnosticFunctionToken;
    TZrUInt32 diagnosticBlockId;
    TZrUInt32 diagnosticInstructionId;
    TZrUInt32 diagnosticSourceId;
    TZrUInt32 diagnosticExpectedVersion;
    TZrUInt32 diagnosticActualVersion;
    TZrUInt64 diagnosticExpectedHash;
    TZrUInt64 diagnosticActualHash;
} SZrSourceExecBcReport;

static SZrState *g_state;

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
    }
    g_state = ZR_NULL;
}

static TZrDebugSignal record_vm_instruction(
        SZrState *state,
        SZrFunction *function,
        const TZrInstruction *programCounter,
        TZrUInt32 instructionOffset,
        TZrUInt32 line,
        TZrPtr userData) {
    SZrSourceExecBcTrace *trace = (SZrSourceExecBcTrace *)userData;
    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(line);
    if (trace == ZR_NULL || function != trace->expectedFunction ||
        programCounter == ZR_NULL ||
        instructionOffset >= function->instructionsLength ||
        programCounter != &function->instructionsList[instructionOffset]) {
        if (trace != ZR_NULL) {
            trace->invalidProgramCounter = ZR_TRUE;
        }
        return ZR_DEBUG_SIGNAL_NONE;
    }
    if (trace->count >= SOURCE_EXECBC_TRACE_CAPACITY) {
        trace->overflow = ZR_TRUE;
        return ZR_DEBUG_SIGNAL_NONE;
    }
    trace->offsets[trace->count++] = instructionOffset;
    return ZR_DEBUG_SIGNAL_NONE;
}

/* Match the source-CFG Oracle contract: source maps point back to the SemIR
 * place that produced PLACE_BASE, while the value remains a stable place ID
 * token owned by this fixture (never a host pointer).
 */
static TZrBool source_execbc_place_provider(
        void *userData,
        const SZrExecIrInstruction *instruction,
        const SZrExecIrOracleValue *operands,
        TZrUInt32 operandCount,
        SZrExecIrOracleValue *result) {
    SZrSourceExecBcOraclePlaceContext *context =
            (SZrSourceExecBcOraclePlaceContext *)userData;
    const SZrSemanticIrInstruction *sourceInstruction;

    (void)operands;
    if (context == ZR_NULL || context->semanticFunction == ZR_NULL ||
        instruction == ZR_NULL || result == ZR_NULL ||
        instruction->opcode != ZR_EXEC_IR_OPCODE_PLACE_BASE ||
        instruction->sourceId == 0u || operandCount != 1u) {
        return ZR_FALSE;
    }
    sourceInstruction = ZrParser_SemanticIr_InstructionAt(
            context->semanticFunction, instruction->sourceId - 1u);
    if (sourceInstruction == ZR_NULL ||
        sourceInstruction->opcode != ZR_SEMANTIC_IR_PLACE_BASE) {
        return ZR_FALSE;
    }

    result->kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    result->as.signedInteger = (TZrInt64)sourceInstruction->placeId;
    ++context->placeProviderCalls;
    return ZR_TRUE;
}

/* Builder place-provenance operands are external values. Seed only values
 * that are actually used by PLACE_BASE so this fixture does not manufacture
 * arguments or hide unrelated undefined ExecIR inputs.
 */
static TZrBool make_source_place_initial_values(
        const SZrExecIrFunction *function,
        SZrExecIrOracleValue **outInitialValues) {
    SZrExecIrOracleValue *initialValues;
    TZrUInt32 valueIndex;

    if (function == ZR_NULL || outInitialValues == ZR_NULL) {
        return ZR_FALSE;
    }
    *outInitialValues = ZR_NULL;
    if (function->valueCount == 0u) {
        return ZR_TRUE;
    }
    initialValues = (SZrExecIrOracleValue *)calloc(
            function->valueCount, sizeof(*initialValues));
    if (initialValues == ZR_NULL) {
        return ZR_FALSE;
    }

    for (valueIndex = 0u; valueIndex < function->valueCount; ++valueIndex) {
        TZrBool isPlaceProvenance = ZR_FALSE;
        TZrUInt32 instructionIndex;
        if (function->values[valueIndex].definition !=
            ZR_EXEC_IR_INSTRUCTION_ID_INVALID) {
            continue;
        }
        for (instructionIndex = 0u;
             instructionIndex < function->instructionCount;
             ++instructionIndex) {
            const SZrExecIrInstruction *instruction =
                    &function->instructions[instructionIndex];
            if (instruction->opcode == ZR_EXEC_IR_OPCODE_PLACE_BASE &&
                instruction->operandRange.count == 1u &&
                function->operandPool[instruction->operandRange.start] ==
                        valueIndex + 1u) {
                isPlaceProvenance = ZR_TRUE;
                break;
            }
        }
        if (isPlaceProvenance == ZR_FALSE) {
            free(initialValues);
            return ZR_FALSE;
        }
        initialValues[valueIndex].kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
        initialValues[valueIndex].as.signedInteger =
                (TZrInt64)valueIndex + 1;
    }

    *outInitialValues = initialValues;
    return ZR_TRUE;
}

static TZrExecIrBlockId source_execbc_projection_instruction_block(
        const SZrExecBcProjection *projection, TZrUInt32 instructionIndex) {
    TZrUInt32 blockIndex;
    if (projection == ZR_NULL || projection->blocks == ZR_NULL) {
        return 0u;
    }
    for (blockIndex = 0u; blockIndex < projection->blockCount; ++blockIndex) {
        const SZrExecBcBlock *block = &projection->blocks[blockIndex];
        if ((instructionIndex >= block->instructions.start &&
             instructionIndex - block->instructions.start <
                     block->instructions.count) ||
            block->terminatorInstructionId == instructionIndex + 1u) {
            return block->id;
        }
    }
    return 0u;
}

static void source_execbc_print_value_range(
        const TZrExecIrValueId *values,
        TZrUInt32 valueCount,
        SZrExecIrRange range) {
    TZrUInt32 index;
    (void)printf("[");
    if (range.count != 0u &&
        (values == ZR_NULL || range.start > valueCount ||
         range.count > valueCount - range.start)) {
        (void)printf("invalid-range");
    } else {
        for (index = 0u; index < range.count; ++index) {
            (void)printf("%s%u", index == 0u ? "" : ",",
                         (unsigned int)values[range.start + index]);
        }
    }
    (void)printf("]");
}

static void source_execbc_print_block_range(
        const TZrExecIrBlockId *values,
        TZrUInt32 valueCount,
        SZrExecIrRange range) {
    TZrUInt32 index;
    (void)printf("[");
    if (range.count != 0u &&
        (values == ZR_NULL || range.start > valueCount ||
         range.count > valueCount - range.start)) {
        (void)printf("invalid-range");
    } else {
        for (index = 0u; index < range.count; ++index) {
            (void)printf("%s%u", index == 0u ? "" : ",",
                         (unsigned int)values[range.start + index]);
        }
    }
    (void)printf("]");
}

#include "ssa_source_execbc_vm_diagnostics.inc"

static const SZrExecBcVmPcMapEntry *find_pc_map_entry(
        const SZrExecBcVmEmission *emission, TZrUInt32 pc) {
    TZrUInt32 index;
    for (index = 0u; index < emission->pcMapCount; ++index) {
        if (emission->pcMap[index].pc == pc) {
            return &emission->pcMap[index];
        }
    }
    return ZR_NULL;
}

static TZrBool make_source_constants(
        SZrCompilerState *compiler,
        SZrExecIrConstant **outConstants,
        SZrExecIrOracleValue **outOracleValues,
        TZrUInt32 *outCount) {
    TZrSize index;
    TZrUInt32 count;
    SZrExecIrConstant *constants;
    SZrExecIrOracleValue *oracleValues;

    if (compiler == ZR_NULL || outConstants == ZR_NULL ||
        outOracleValues == ZR_NULL || outCount == ZR_NULL ||
        compiler->constants.length > UINT32_MAX) {
        return ZR_FALSE;
    }
    count = (TZrUInt32)compiler->constants.length;
    constants = count == 0u ? ZR_NULL :
            (SZrExecIrConstant *)calloc(count, sizeof(*constants));
    oracleValues = count == 0u ? ZR_NULL :
            (SZrExecIrOracleValue *)calloc(count, sizeof(*oracleValues));
    if (count != 0u && (constants == ZR_NULL || oracleValues == ZR_NULL)) {
        free(constants);
        free(oracleValues);
        return ZR_FALSE;
    }

    for (index = 0u; index < compiler->constants.length; ++index) {
        const SZrTypeValue *constant = (const SZrTypeValue *)ZrCore_Array_Get(
                (SZrArray *)&compiler->constants, index);
        if (constant == ZR_NULL) {
            free(constants);
            free(oracleValues);
            return ZR_FALSE;
        }
        if (ZR_VALUE_IS_TYPE_BOOL(constant->type)) {
            TZrBool value = (TZrBool)(constant->value.nativeObject.nativeBool != 0);
            constants[index].typeToken = ZrParser_CanonicalType_InternPrimitive(
                    compiler->semanticContext, ZR_VALUE_TYPE_BOOL);
            constants[index].bits = value != ZR_FALSE ? 1u : 0u;
            oracleValues[index].kind = ZR_EXEC_IR_ORACLE_VALUE_BOOL;
            oracleValues[index].as.boolean = value;
        } else if (constant->type == ZR_VALUE_TYPE_INT64) {
            TZrInt64 value = constant->value.nativeObject.nativeInt64;
            constants[index].typeToken = ZrParser_CanonicalType_InternPrimitive(
                    compiler->semanticContext, ZR_VALUE_TYPE_INT64);
            constants[index].bits = (TZrUInt64)value;
            oracleValues[index].kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
            oracleValues[index].as.signedInteger = value;
        } else {
            free(constants);
            free(oracleValues);
            return ZR_FALSE;
        }
    }
    *outConstants = constants;
    *outOracleValues = oracleValues;
    *outCount = count;
    return ZR_TRUE;
}

static void capture_diagnostic(
        SZrSourceExecBcReport *report,
        EZrSourceExecBcStage stage,
        const SZrExecIrDiagnostic *diagnostic) {
    if (report == ZR_NULL || diagnostic == ZR_NULL) {
        return;
    }
    report->diagnosticStage = stage;
    report->diagnosticCode = diagnostic->code;
    report->diagnosticFunctionToken = diagnostic->functionToken;
    report->diagnosticBlockId = diagnostic->blockId;
    report->diagnosticInstructionId = diagnostic->instructionId;
    report->diagnosticSourceId = diagnostic->sourceId;
    report->diagnosticExpectedVersion = diagnostic->expectedVersion;
    report->diagnosticActualVersion = diagnostic->actualVersion;
    report->diagnosticExpectedHash = diagnostic->expectedHash;
    report->diagnosticActualHash = diagnostic->actualHash;
}

static const char *source_execbc_stage_name(EZrSourceExecBcStage stage) {
    switch (stage) {
        case ZR_SOURCE_EXECBC_STAGE_BUILDER: return "builder";
        case ZR_SOURCE_EXECBC_STAGE_IDENTITY: return "identity";
        case ZR_SOURCE_EXECBC_STAGE_VERIFY: return "verify";
        case ZR_SOURCE_EXECBC_STAGE_ORACLE: return "oracle";
        case ZR_SOURCE_EXECBC_STAGE_PROJECTION: return "projection";
        case ZR_SOURCE_EXECBC_STAGE_MATERIALIZER: return "materializer";
        default: return "none";
    }
}

static SZrSourceExecBcReport run_source_branch(const TZrChar *source) {
    SZrSourceExecBcReport report;
    SZrCompilerState compiler;
    SZrAstNode *ast = ZR_NULL;
    SZrString *sourceName = ZR_NULL;
    SZrExecIrBuildInput buildInput;
    SZrExecIrModule execIrModule;
    const SZrExecIrFunction *execIr = ZR_NULL;
    SZrExecIrOracleInput oracleInput;
    SZrExecIrOracleExecutionResult oracleResult;
    SZrExecBcProjection projection;
    SZrExecBcVmEmission emission;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrConstant *constants = ZR_NULL;
    SZrExecIrOracleValue *oracleConstants = ZR_NULL;
    SZrExecIrOracleValue *oracleInitialValues = ZR_NULL;
    TZrUInt32 constantCount = 0u;
    TZrUInt32 index;
    TZrUInt32 lastBlock = 0u;
    TZrBool compilerInitialized = ZR_FALSE;
    TZrBool execIrModuleInitialized = ZR_FALSE;
    TZrBool functionRooted = ZR_FALSE;
    SZrSourceExecBcTrace trace;
    SZrSourceExecBcOraclePlaceContext oraclePlaceContext;

    memset(&report, 0, sizeof(report));
    memset(&compiler, 0, sizeof(compiler));
    memset(&buildInput, 0, sizeof(buildInput));
    memset(&execIrModule, 0, sizeof(execIrModule));
    memset(&oracleInput, 0, sizeof(oracleInput));
    memset(&oracleResult, 0, sizeof(oracleResult));
    memset(&projection, 0, sizeof(projection));
    memset(&emission, 0, sizeof(emission));
    memset(&diagnostic, 0, sizeof(diagnostic));
    memset(&trace, 0, sizeof(trace));
    memset(&oraclePlaceContext, 0, sizeof(oraclePlaceContext));
    ZrCore_ExecIr_OracleResultInit(&oracleResult);

    sourceName = ZrCore_String_CreateFromNative(
            g_state, "ssa_source_execbc_vm.zr");
    if (sourceName == ZR_NULL || source == ZR_NULL) {
        goto cleanup;
    }
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    if (ast == ZR_NULL) {
        goto cleanup;
    }
    ZrParser_CompilerState_Init(&compiler, g_state);
    compilerInitialized = ZR_TRUE;
    compiler.currentAst = ast;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    if (compiler.currentFunction == ZR_NULL) {
        goto cleanup;
    }
    for (index = 0u; index < ast->data.script.statements->count; ++index) {
        ZrParser_Statement_Compile(
                &compiler, ast->data.script.statements->nodes[index]);
        if (compiler.hasError != ZR_FALSE) {
            goto cleanup;
        }
    }
    report.parsedAndCompiled = ZR_TRUE;
    report.preSemanticIrValidated =
            ZrParser_Compiler_ValidatePreSemanticIr(&compiler);
    if (report.preSemanticIrValidated == ZR_FALSE ||
        compiler.preSemanticIrCfgActive == ZR_FALSE) {
        goto cleanup;
    }
    report.sourceCfgValidated = ZR_TRUE;
    if (!make_source_constants(
                &compiler, &constants, &oracleConstants, &constantCount)) {
        goto cleanup;
    }

    ZrCore_ExecIr_ModuleInit(&execIrModule);
    execIrModuleInitialized = ZR_TRUE;
    buildInput.semanticFunction = &compiler.preSemanticIr;
    buildInput.functionToken = 0x5105u;
    buildInput.signatureHash = 0x51050001u;
    buildInput.options.preserveSourceMaps = ZR_TRUE;
    if (!ZrParser_ExecIr_BuildModule(
                &buildInput, &execIrModule, &diagnostic)) {
        capture_diagnostic(
                &report, ZR_SOURCE_EXECBC_STAGE_BUILDER, &diagnostic);
        goto cleanup;
    }
    execIr = ZrCore_ExecIr_ModuleFunctionAtConst(&execIrModule, 1u);
    if (execIr == ZR_NULL || execIr->id == 0u ||
        execIr->functionToken != buildInput.functionToken ||
        execIr->signatureHash != buildInput.signatureHash ||
        execIr->contract.targetToken != buildInput.functionToken ||
        execIr->contract.signatureHash != buildInput.signatureHash ||
        execIr->contract.generation == 0u) {
        capture_diagnostic(
                &report, ZR_SOURCE_EXECBC_STAGE_IDENTITY, &diagnostic);
        goto cleanup;
    }
    report.identityPublished = ZR_TRUE;
    if (!ZrParser_ExecIr_VerifyFunction(
                execIr, ZR_EXEC_IR_VERIFY_ALL, &diagnostic)) {
        capture_diagnostic(
                &report, ZR_SOURCE_EXECBC_STAGE_VERIFY, &diagnostic);
        goto cleanup;
    }
    report.execIrBuilt = ZR_TRUE;
    for (index = 0u; index < execIr->instructionCount; ++index) {
        if (execIr->instructions[index].opcode ==
                ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH) {
            report.hasConditionalBranch = ZR_TRUE;
            ++report.conditionalBranchCount;
        }
        if (execIr->instructions[index].opcode == ZR_EXEC_IR_OPCODE_ADD) {
            report.hasAdd = ZR_TRUE;
        }
        if (execIr->instructions[index].opcode == ZR_EXEC_IR_OPCODE_SUB) {
            report.hasSubtract = ZR_TRUE;
        }
        if (execIr->instructions[index].opcode == ZR_EXEC_IR_OPCODE_MUL) {
            const SZrCanonicalTypeNode *type = ZrParser_CanonicalType_Find(
                    compiler.semanticContext,
                    execIr->instructions[index].typeToken);
            TZrBool hasSignedType = (TZrBool)(
                    type != ZR_NULL &&
                    type->kind == ZR_CANONICAL_TYPE_PRIMITIVE &&
                    type->data.primitive.valueType == ZR_VALUE_TYPE_INT64);
            report.multiplyHasSignedType = (TZrBool)(hasSignedType &&
                    (report.hasMultiply == ZR_FALSE ||
                     report.multiplyHasSignedType != ZR_FALSE));
            report.hasMultiply = ZR_TRUE;
            ++report.multiplyCount;
        }
    }
    report.execIrPhiCount = execIr->phiCount;
    if (report.hasConditionalBranch == ZR_FALSE) {
        goto cleanup;
    }

    if (!make_source_place_initial_values(execIr, &oracleInitialValues)) {
        goto cleanup;
    }

    oracleInput.function = execIr;
    oracleInput.constants = oracleConstants;
    oracleInput.constantCount = constantCount;
    oracleInput.initialValues = oracleInitialValues;
    oracleInput.initialValueCount = execIr->valueCount;
    oraclePlaceContext.semanticFunction = &compiler.preSemanticIr;
    oracleInput.place = source_execbc_place_provider;
    oracleInput.placeUserData = &oraclePlaceContext;
    oracleInput.maxSteps = 128u;
    if (!ZrCore_ExecIr_RunOracleEx(
                &oracleInput, &oracleResult, &diagnostic)) {
        report.oraclePlaceProviderCalls =
                oraclePlaceContext.placeProviderCalls;
        capture_diagnostic(
                &report, ZR_SOURCE_EXECBC_STAGE_ORACLE, &diagnostic);
        goto cleanup;
    }
    report.oraclePlaceProviderCalls = oraclePlaceContext.placeProviderCalls;
    if (oracleResult.returned == ZR_FALSE ||
        oracleResult.returnValue.kind != ZR_EXEC_IR_ORACLE_VALUE_SIGNED) {
        goto cleanup;
    }
    report.oracleReturned = ZR_TRUE;
    report.oracleInteger = oracleResult.returnValue.as.signedInteger;
    report.oracleBlock = oracleResult.currentBlock;
    report.oracleEventCount = oracleResult.eventCount;
    report.loopBreakTargetsValid = ZR_TRUE;
    for (index = 0u; index < execIr->sourceMapCount; ++index) {
        const SZrExecIrSourceMap *map = &execIr->sourceMaps[index];
        const SZrExecIrInstruction *instruction;
        if (map->startOffset >= strlen(source) ||
            strncmp(source + map->startOffset, "break", 5u) != 0 ||
            map->instructionId == 0u ||
            map->instructionId > execIr->instructionCount) {
            continue;
        }
        instruction = &execIr->instructions[map->instructionId - 1u];
        if (instruction->opcode != ZR_EXEC_IR_OPCODE_BRANCH) continue;
        ++report.loopBreakCount;
        if (instruction->successorRange.count != 1u ||
            execIr->successors[instruction->successorRange.start] !=
                    report.oracleBlock) {
            report.loopBreakTargetsValid = ZR_FALSE;
        }
    }

    if (!ZrParser_ExecIr_BuildProjectionWithConstants(
                execIr, constants, constantCount, &projection, &diagnostic)) {
        capture_diagnostic(
                &report, ZR_SOURCE_EXECBC_STAGE_PROJECTION, &diagnostic);
        goto cleanup;
    }
    report.projectionBuilt = ZR_TRUE;
    for (index = 0u; index < projection.instructionCount; ++index) {
        if (projection.instructions[index].opcode == ZR_EXEC_IR_OPCODE_MUL) {
            const SZrCanonicalTypeNode *type = ZrParser_CanonicalType_Find(
                    compiler.semanticContext,
                    projection.instructions[index].typeToken);
            TZrBool hasSignedType = (TZrBool)(
                    type != ZR_NULL &&
                    type->kind == ZR_CANONICAL_TYPE_PRIMITIVE &&
                    type->data.primitive.valueType == ZR_VALUE_TYPE_INT64);
            report.projectionHasSignedMultiply = (TZrBool)(hasSignedType &&
                    (report.projectionMultiplyCount == 0u ||
                     report.projectionHasSignedMultiply != ZR_FALSE));
            ++report.projectionMultiplyCount;
        }
    }
    if (!ZrParser_ExecBcProjection_MaterializeVmFunctionWithCanonicalTypes(
                g_state, &projection, compiler.semanticContext,
                &emission, &diagnostic)) {
        source_execbc_report_materializer_input(
                execIr, &compiler.preSemanticIr, constants, constantCount,
                &oracleResult, &projection);
        capture_diagnostic(
                &report, ZR_SOURCE_EXECBC_STAGE_MATERIALIZER, &diagnostic);
        goto cleanup;
    }
    report.materialized = ZR_TRUE;
    if (emission.function == ZR_NULL || emission.pcMap == ZR_NULL ||
        emission.pcMapCount == 0u ||
        !ZrCore_GarbageCollector_IgnoreObject(
                g_state, ZR_CAST_RAW_OBJECT_AS_SUPER(emission.function))) {
        goto cleanup;
    }
    functionRooted = ZR_TRUE;

    trace.expectedFunction = emission.function;
    ZrCore_Debug_SetTraceObserver(g_state, record_vm_instruction, &trace);
    report.vmReturned = ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, emission.function, &report.vmInteger);
    ZrCore_Debug_SetTraceObserver(g_state, ZR_NULL, ZR_NULL);
    report.traceCount = trace.count;
    if (report.vmReturned == ZR_FALSE || trace.count == 0u ||
        trace.overflow != ZR_FALSE || trace.invalidProgramCounter != ZR_FALSE) {
        goto cleanup;
    }

    report.pcSourceTraceValid = ZR_TRUE;
    for (index = 0u; index < trace.count; ++index) {
        const SZrExecBcVmPcMapEntry *entry =
                find_pc_map_entry(&emission, trace.offsets[index]);
        if (entry == ZR_NULL || entry->instructionId == 0u ||
            entry->instructionId > execIr->instructionCount ||
            entry->blockId == 0u || entry->sourceId == 0u ||
            execIr->instructions[entry->instructionId - 1u].sourceId !=
                    entry->sourceId) {
            report.pcSourceTraceValid = ZR_FALSE;
            break;
        }
        lastBlock = entry->blockId;
        {
            TZrUInt32 sourceIndex;
            for (sourceIndex = 0u; sourceIndex < execIr->sourceMapCount;
                 ++sourceIndex) {
                const SZrExecIrSourceMap *map = &execIr->sourceMaps[sourceIndex];
                if (map->instructionId == entry->instructionId &&
                    map->startOffset < strlen(source) &&
                    strncmp(source + map->startOffset, "break", 5u) == 0 &&
                    execIr->instructions[entry->instructionId - 1u].opcode ==
                            ZR_EXEC_IR_OPCODE_BRANCH) {
                    ++report.vmBreakCount;
                    report.vmBreakOffset = map->startOffset;
                    break;
                }
            }
        }
        if (trace.pathCount == 0u ||
            trace.path[trace.pathCount - 1u] != entry->blockId) {
            if (trace.pathCount >= SOURCE_EXECBC_PATH_CAPACITY) {
                report.pcSourceTraceValid = ZR_FALSE;
                break;
            }
            trace.path[trace.pathCount++] = entry->blockId;
        }
    }
    report.vmPathCount = trace.pathCount;
    report.vmLastBlock = lastBlock;
    if (report.pcSourceTraceValid != ZR_FALSE && trace.pathCount != 0u &&
        trace.path[0] == execIr->entryBlockId) {
        TZrUInt32 pathIndex;
        report.vmEndsAtOracleBlock = (TZrBool)(
                lastBlock == report.oracleBlock);
        for (pathIndex = 1u; pathIndex < trace.pathCount; ++pathIndex) {
            TZrExecIrBlockId from = trace.path[pathIndex - 1u];
            TZrExecIrBlockId to = trace.path[pathIndex];
            const SZrExecIrBlock *block;
            TZrUInt32 successorIndex;
            TZrBool edgeFound = ZR_FALSE;
            if (from == 0u || from > execIr->blockCount ||
                to == 0u || to > execIr->blockCount) {
                report.vmEndsAtOracleBlock = ZR_FALSE;
                break;
            }
            block = &execIr->blocks[from - 1u];
            for (successorIndex = 0u;
                 successorIndex < block->successorRange.count;
                 ++successorIndex) {
                TZrUInt32 successor = execIr->successors[
                        block->successorRange.start + successorIndex];
                if (successor == to) {
                    edgeFound = ZR_TRUE;
                    break;
                }
            }
            if (edgeFound == ZR_FALSE) {
                report.vmEndsAtOracleBlock = ZR_FALSE;
                break;
            }
        }
    }

cleanup:
    if (report.diagnosticCode != ZR_EXECUTION_DIAGNOSTIC_NONE) {
        (void)printf(
                "source ExecBC failure stage=%s code=%u function=%u block=%u "
                "instruction=%u source=%u expectedVersion=%u actualVersion=%u "
                "expectedHash=%llu actualHash=%llu\n",
                source_execbc_stage_name(report.diagnosticStage),
                (unsigned int)report.diagnosticCode,
                (unsigned int)report.diagnosticFunctionToken,
                (unsigned int)report.diagnosticBlockId,
                (unsigned int)report.diagnosticInstructionId,
                (unsigned int)report.diagnosticSourceId,
                (unsigned int)report.diagnosticExpectedVersion,
                (unsigned int)report.diagnosticActualVersion,
                (unsigned long long)report.diagnosticExpectedHash,
                (unsigned long long)report.diagnosticActualHash);
    }
    ZrCore_Debug_SetTraceObserver(g_state, ZR_NULL, ZR_NULL);
    if (functionRooted != ZR_FALSE && emission.function != ZR_NULL) {
        ZrCore_GarbageCollector_UnignoreObject(
                g_state->global,
                ZR_CAST_RAW_OBJECT_AS_SUPER(emission.function));
    }
    ZrParser_ExecBcVmEmission_Free(g_state, &emission);
    ZrParser_ExecBcProjection_Free(&projection);
    ZrCore_ExecIr_OracleResultFree(&oracleResult);
    if (execIrModuleInitialized != ZR_FALSE) {
        ZrCore_ExecIr_FreeModule(&execIrModule);
    }
    free(constants);
    free(oracleConstants);
    free(oracleInitialValues);
    if (compilerInitialized != ZR_FALSE) {
        if (compiler.currentFunction != ZR_NULL) {
            ZrCore_Function_Free(g_state, compiler.currentFunction);
            compiler.currentFunction = ZR_NULL;
        }
        ZrParser_CompilerState_Free(&compiler);
    }
    if (ast != ZR_NULL) {
        ZrParser_Ast_Free(g_state, ast);
    }
    return report;
}

static void assert_source_branch_report(
        SZrSourceExecBcReport report, TZrInt64 expectedReturn,
        TZrUInt32 expectedArithmetic) {
    TEST_ASSERT_EQUAL_INT_MESSAGE(
            ZR_EXECUTION_DIAGNOSTIC_NONE, report.diagnosticCode,
            "an ExecIR, Oracle, projection, or materializer stage diagnosed input");
    TEST_ASSERT_TRUE_MESSAGE(report.parsedAndCompiled,
                             "source parse and SemIR compilation failed");
    TEST_ASSERT_TRUE_MESSAGE(report.sourceCfgValidated,
                             "source CFG validation failed");
    TEST_ASSERT_TRUE_MESSAGE(report.execIrBuilt,
                             "SemIR to ExecIR builder failed");
    TEST_ASSERT_TRUE_MESSAGE(report.identityPublished,
                             "module builder did not publish function identity");
    TEST_ASSERT_TRUE_MESSAGE(report.hasConditionalBranch,
                             "source branch did not reach ExecIR");
    TEST_ASSERT_EQUAL_MESSAGE(expectedArithmetic == ZR_EXEC_IR_OPCODE_ADD,
                              report.hasAdd, "source ADD did not reach ExecIR");
    TEST_ASSERT_EQUAL_MESSAGE(expectedArithmetic == ZR_EXEC_IR_OPCODE_SUB,
                              report.hasSubtract, "source SUB did not reach ExecIR");
    TEST_ASSERT_EQUAL_MESSAGE(expectedArithmetic == ZR_EXEC_IR_OPCODE_MUL,
                              report.hasMultiply, "source MUL did not reach ExecIR");
    TEST_ASSERT_EQUAL_MESSAGE(expectedArithmetic == ZR_EXEC_IR_OPCODE_MUL,
                              report.multiplyHasSignedType,
                              "source MUL does not have canonical signed i64 type");
    TEST_ASSERT_EQUAL_MESSAGE(expectedArithmetic == ZR_EXEC_IR_OPCODE_MUL,
                              report.projectionHasSignedMultiply,
                              "signed i64 MUL did not reach ExecBC projection");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            expectedArithmetic == ZR_EXEC_IR_OPCODE_MUL ? 2u : 0u,
            report.multiplyCount, "both source MUL arms must reach ExecIR");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            report.multiplyCount, report.projectionMultiplyCount,
            "ExecBC projection did not preserve both source MUL arms");
    TEST_ASSERT_TRUE_MESSAGE(report.oracleReturned,
                             "ExecIR Oracle did not return signed i64");
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u, report.oraclePlaceProviderCalls,
            "source ExecIR Oracle did not resolve the SemIR place token");
    TEST_ASSERT_EQUAL_INT64(expectedReturn, report.oracleInteger);
    TEST_ASSERT_TRUE_MESSAGE(report.projectionBuilt,
                             "ExecBC projection failed");
    TEST_ASSERT_TRUE_MESSAGE(report.materialized,
                             "ExecBC VM materialization failed");
    TEST_ASSERT_TRUE_MESSAGE(report.vmReturned,
                             "Core dispatcher did not return signed i64");
    TEST_ASSERT_EQUAL_INT64(report.oracleInteger, report.vmInteger);
    TEST_ASSERT_GREATER_THAN_UINT32(1u, report.vmPathCount);
    TEST_ASSERT_TRUE_MESSAGE(report.pcSourceTraceValid,
                             "VM PC map did not match ExecIR source IDs");
    TEST_ASSERT_TRUE_MESSAGE(report.vmEndsAtOracleBlock,
                             "VM trace terminal block differs from Oracle");
}

static void assert_source_branch(
        const TZrChar *source, TZrInt64 expectedReturn,
        TZrUInt32 expectedArithmetic) {
    assert_source_branch_report(
            run_source_branch(source), expectedReturn, expectedArithmetic);
}

static void test_true_source_branch_runs_through_core_dispatcher(void) {
    assert_source_branch(
            "if (true) { return 9; }\nreturn 8;\n", 9, 0u);
}

static void test_false_source_branch_runs_through_core_dispatcher(void) {
    assert_source_branch(
            "if (false) { return 9; }\nreturn 8;\n", 8, 0u);
}

static void test_source_branch_arithmetic_reaches_core_dispatcher(void) {
    assert_source_branch(
            "if (true) { return 9 + 1; }\nreturn 8;\n", 10, ZR_EXEC_IR_OPCODE_ADD);
}

static void test_source_branch_subtraction_reaches_core_dispatcher(void) {
    assert_source_branch(
            "if (true) { return 9 - 1; }\nreturn 7;\n", 8, ZR_EXEC_IR_OPCODE_SUB);
}

static void test_source_branch_division_does_not_publish_execir(void) {
    SZrSourceExecBcReport report = run_source_branch(
            "if (true) { return 9 / 1; }\nreturn 8;\n");
    TEST_ASSERT_TRUE(report.parsedAndCompiled);
    TEST_ASSERT_FALSE(report.sourceCfgValidated);
    TEST_ASSERT_FALSE(report.execIrBuilt);
    TEST_ASSERT_FALSE(report.projectionBuilt);
    TEST_ASSERT_FALSE(report.materialized);
    TEST_ASSERT_FALSE(report.vmReturned);
}

static void test_true_source_branch_multiplication_reaches_core_dispatcher(void) {
    assert_source_branch(
            "if (true) { return 9 * 2; } else { return 7 * 3; }\n",
            18, ZR_EXEC_IR_OPCODE_MUL);
}

static void test_false_source_branch_multiplication_reaches_core_dispatcher(void) {
    assert_source_branch(
            "if (false) { return 9 * 2; } else { return 7 * 3; }\n",
            21, ZR_EXEC_IR_OPCODE_MUL);
}

#include "ssa_source_execbc_vm_loop_break.inc"
#include "ssa_source_execbc_vm_compare.inc"

int main(int argc, char **argv) {
    TZrBool comparisonsOnly = ZR_FALSE;
    TZrBool regressionsOnly = ZR_FALSE;
    if (argc == 2 && strcmp(argv[1], "--comparisons-only") == 0) {
        comparisonsOnly = ZR_TRUE;
    } else if (argc == 2 && strcmp(argv[1], "--regressions-only") == 0) {
        regressionsOnly = ZR_TRUE;
    } else if (argc != 1) {
        (void)fprintf(stderr,
                "usage: %s [--comparisons-only|--regressions-only]\n", argv[0]);
        return 2;
    }
    UNITY_BEGIN();
    if (comparisonsOnly == ZR_FALSE) {
        RUN_TEST(test_true_source_branch_runs_through_core_dispatcher);
        RUN_TEST(test_false_source_branch_runs_through_core_dispatcher);
        RUN_TEST(test_source_branch_arithmetic_reaches_core_dispatcher);
        RUN_TEST(test_source_branch_subtraction_reaches_core_dispatcher);
        RUN_TEST(test_source_branch_division_does_not_publish_execir);
        RUN_TEST(test_true_source_branch_multiplication_reaches_core_dispatcher);
        RUN_TEST(test_false_source_branch_multiplication_reaches_core_dispatcher);
        RUN_TEST(test_source_while_takes_conditional_break);
        RUN_TEST(test_source_while_takes_fallthrough_break);
        RUN_TEST(test_source_conditional_continue_remains_unsupported);
        RUN_TEST(test_source_conditional_break_cleanup_remains_unsupported);
        RUN_TEST(test_source_for_conditional_break_remains_unsupported);
        RUN_TEST(test_source_foreach_conditional_break_remains_unsupported);
    }
    if (regressionsOnly == ZR_FALSE) {
        RUN_TEST(test_source_signed_less_true_reaches_core_dispatcher);
        RUN_TEST(test_source_signed_less_false_reaches_core_dispatcher);
        RUN_TEST(test_source_signed_greater_true_reaches_core_dispatcher);
        RUN_TEST(test_source_signed_greater_false_reaches_core_dispatcher);
    }
    return UNITY_END();
}
