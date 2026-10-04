/* Actual Oracle execution, with independent pooled branch and arithmetic values.
 * Semantic failures return EXIT_FAILURE; asserts are fixture construction gates. */
#include "zr_vm_core/exec_ir_state_map_liveness.h"

typedef struct SZrLicmScalarCase {
    const char *name;
    EZrExecIrOpcode opcode;
    SZrExecIrOracleValue left, right;
    TZrBool zeroTrip, strength, context, differentFunction;
    TZrBool expectedSuccess, expectedTransform;
    TZrUInt32 mapped;
    EZrExecutionDiagnosticCode expectedDiagnosticCode;
} SZrLicmScalarCase;

static SZrExecIrOracleValue signed_value(TZrInt64 value) {
    SZrExecIrOracleValue result;
    memset(&result, 0, sizeof(result));
    result.kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    result.as.signedInteger = value;
    return result;
}

static SZrExecIrOracleValue unsigned_value(TZrUInt64 value) {
    SZrExecIrOracleValue result;
    memset(&result, 0, sizeof(result));
    result.kind = ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED;
    result.as.unsignedInteger = value;
    return result;
}

static SZrExecIrOracleValue float_value(TZrFloat64 value) {
    SZrExecIrOracleValue result;
    memset(&result, 0, sizeof(result));
    result.kind = ZR_EXEC_IR_ORACLE_VALUE_FLOAT;
    result.as.floating = value;
    return result;
}

static TZrBool scalar_value_equal(const SZrExecIrOracleValue *a,
                                 const SZrExecIrOracleValue *b) {
    if (a->kind != b->kind) return ZR_FALSE;
    switch (a->kind) {
        case ZR_EXEC_IR_ORACLE_VALUE_SIGNED:
            return (TZrBool)(a->as.signedInteger == b->as.signedInteger);
        case ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED:
            return (TZrBool)(a->as.unsignedInteger == b->as.unsignedInteger);
        case ZR_EXEC_IR_ORACLE_VALUE_FLOAT:
            return (TZrBool)(memcmp(&a->as.floating, &b->as.floating,
                                   sizeof(a->as.floating)) == 0);
        case ZR_EXEC_IR_ORACLE_VALUE_BOOL:
            return (TZrBool)(a->as.boolean == b->as.boolean);
        default: return (TZrBool)(a->kind == ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED);
    }
}

static TZrBool scalar_events_equal(const SZrExecIrOracleExecutionResult *a,
                                  const SZrExecIrOracleExecutionResult *b) {
    TZrUInt32 i, j;
    if (a->eventCount != b->eventCount) return ZR_FALSE;
    for (i = 0u; i < a->eventCount; ++i) {
        const SZrExecIrOracleEvent *x = &a->events[i], *y = &b->events[i];
        if (x->kind != y->kind || x->sourceId != y->sourceId ||
            x->operandCount != y->operandCount) return ZR_FALSE;
        for (j = 0u; j < x->operandCount; ++j)
            if (!scalar_value_equal(&x->operands[j], &y->operands[j])) return ZR_FALSE;
    }
    return ZR_TRUE;
}

static int run_scalar_case(const SZrLicmScalarCase *test) {
    SZrExecIrFunction function, otherFunction;
    SZrExecIrLoopInfo loops;
    SZrExecIrOracleInput input, context;
    SZrExecIrOracleValue constants[10], initialValues[8];
    SZrExecIrOracleExecutionResult before, after;
    SZrExecIrDiagnostic beforeDiagnostic, afterDiagnostic, diagnostic;
    TZrBool beforeSuccess, afterSuccess, passSuccess, valid = ZR_TRUE;
    TZrUInt32 i, targetIndex = UINT32_MAX, targetBlock = 0u;
    SZrExecIrStateMap logicalMap, logicalMapBefore;
    SZrExecIrStateMapEntry logicalEntry, logicalEntryBefore;
    SZrExecIrGcMap gcMap, gcMapBefore;
    SZrExecIrGcMapEntry gcEntry, gcEntryBefore;
    TZrExecIrValueId liveValues[8], liveValuesBefore[8];
    TZrUInt32 ownerStates[8], ownerStatesBefore[8];
    SZrExecIrInstruction instructionsBefore[16];
    SZrExecIrBlock blocksBefore[4];
    build_loop(&function, ZR_FALSE, ZR_FALSE, ZR_TRUE);
    memset(constants, 0, sizeof(constants));
    constants[1] = test->right;
    constants[9] = test->left;
    constants[2] = signed_value(test->zeroTrip ? 0 : 1);
    function.instructions[0].layoutId = 2u; /* Slot 2 can hold false. */
    function.instructions[5].opcode = (TZrUInt16)test->opcode;
    if (test->opcode == ZR_EXEC_IR_OPCODE_CONVERT) {
        function.instructions[5].operands.count = 1u;
        function.instructions[5].typeToken = ZR_VALUE_TYPE_INT64;
    }
    configure_loop_effect_tokens(&function);
    if (!ZrCore_ExecIr_VerifyFunction(&function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic))
        fail_with_exec_ir_diagnostic("scalar case fixture", &diagnostic);
    if (test->mapped == 1u) {
        SZrExecIrOwnerAnalysis ownership = {0};
        SZrStateMapLiveness liveness = {0};
        memset(&logicalMap, 0, sizeof(logicalMap));
        memset(&logicalEntry, 0, sizeof(logicalEntry));
        logicalMap.functionToken = function.functionToken;
        logicalMap.signatureHash = function.signatureHash;
        logicalMap.generation = function.contract.generation;
        logicalMap.entries = &logicalEntry;
        logicalMap.entryCount = logicalMap.entryCapacity = 1u;
        logicalMap.valuePool = liveValues;
        logicalMap.ownerStatePool = ownerStates;
        logicalMap.valueCapacity = logicalMap.ownerStateCapacity = 8u;
        logicalEntry.sourceId = 33u;
        logicalEntry.instructionId = 6u;
        logicalEntry.resumeId = 1u;
        logicalEntry.boundaryFlags = ZR_EXEC_IR_STATE_MAP_BOUNDARY_THROW;
        logicalEntry.exceptionState = ZR_EXEC_IR_STATE_MAP_BOUNDARY_THROW;
        logicalEntry.phase = ZR_EXEC_IR_STATE_BEFORE_EFFECT;
        logicalEntry.effectIn = function.instructions[5].effectIn;
        logicalEntry.effectOut = function.instructions[5].effectOut;
        assert(ZrCore_ExecIr_OwnerAnalysisBuild(&function, &ownership, &diagnostic));
        assert(ZrCore_ExecIr_StateMapLivenessBuild(&function, &liveness) ==
               ZR_EXECUTION_DIAGNOSTIC_NONE);
        for (i = 1u; i <= function.valueCount; ++i)
            if (ZrCore_ExecIr_StateMapLivenessContains(&liveness, 6u, i, ZR_FALSE)) {
                liveValues[logicalMap.valueCount++] = i;
                ownerStates[logicalMap.ownerStateCount++] =
                    (TZrUInt32)ZrCore_ExecIr_StateMapOwnerAt(&function, &ownership,
                        &liveness, 6u, i, ZR_EXEC_IR_STATE_BEFORE_EFFECT);
            }
        logicalEntry.liveValues.count = logicalMap.valueCount;
        logicalEntry.ownerStates.count = logicalMap.ownerStateCount;
        ZrCore_ExecIr_StateMapLivenessFree(&liveness);
        ZrCore_ExecIr_OwnerAnalysisFree(&ownership);
        if (!ZrCore_ExecIr_ValidateStateMap(&function, &logicalMap, &diagnostic))
            fail_with_exec_ir_diagnostic("mapped logical fixture", &diagnostic);
        function.stateMap = &logicalMap; /* Borrowed stack map: never modify/free. */
        logicalMapBefore = logicalMap;
        logicalEntryBefore = logicalEntry;
        memcpy(liveValuesBefore, liveValues, logicalMap.valueCount * sizeof(*liveValues));
        memcpy(ownerStatesBefore, ownerStates, logicalMap.ownerStateCount * sizeof(*ownerStates));
    } else if (test->mapped == 2u) {
        memset(&gcMap, 0, sizeof(gcMap));
        memset(&gcEntry, 0, sizeof(gcEntry));
        gcMap.entries = &gcEntry;
        gcMap.entryCount = gcMap.entryCapacity = 1u;
        gcEntry.site = 4u; /* Hoistable source31 CONSTANT. */
        function.gcMap = &gcMap; /* Borrowed stack map: never modify/free. */
        function.gcMapCount = function.gcMapCapacity = 1u;
        gcMapBefore = gcMap;
        gcEntryBefore = gcEntry;
    }
    if (test->mapped != 0u) {
        assert(function.instructionCount <= 16u && function.blockCount == 4u);
        memcpy(instructionsBefore, function.instructions,
               function.instructionCount * sizeof(*instructionsBefore));
        memcpy(blocksBefore, function.blocks, sizeof(blocksBefore));
    }
    memset(initialValues, 0, sizeof(initialValues));
    initialValues[1].kind = ZR_EXEC_IR_ORACLE_VALUE_BOOL;
    initialValues[1].as.boolean = ZR_FALSE;
    memset(&input, 0, sizeof(input));
    input.function = &function;
    input.constants = constants;
    input.constantCount = 10u;
    input.initialValues = initialValues;
    input.initialValueCount = function.valueCount;
    input.maxSteps = 100u;
    /* Optimization receives only the semantic pool view, never runtime values. */
    memset(&context, 0, sizeof(context));
    context.function = test->differentFunction ? &otherFunction : &function;
    context.constants = constants;
    context.constantCount = 10u;
    ZrCore_ExecIr_OracleResultInit(&before);
    ZrCore_ExecIr_OracleResultInit(&after);
    memset(&beforeDiagnostic, 0, sizeof(beforeDiagnostic));
    memset(&afterDiagnostic, 0, sizeof(afterDiagnostic));
    beforeSuccess = ZrCore_ExecIr_RunOracleEx(&input, &before, &beforeDiagnostic);
    ZrParser_ExecIr_LoopInfoInit(&loops);
    if (test->strength)
        passSuccess = ZrParser_ExecIr_StrengthReduceWithContextEx(&function, &loops,
                test->context ? &context : ZR_NULL, ZR_NULL, &diagnostic);
    else
        passSuccess = ZrParser_ExecIr_OptimizeLoopsWithContextEx(&function, &loops,
                test->context ? &context : ZR_NULL, ZR_NULL, &diagnostic);
    if (!passSuccess) fail_with_exec_ir_diagnostic(test->name, &diagnostic);
    if (!ZrCore_ExecIr_VerifyFunction(&function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic))
        fail_with_exec_ir_diagnostic("scalar case optimized verification", &diagnostic);
    if (test->mapped != 0u &&
        (loops.hoistedInstructionCount != 0u ||
         memcmp(instructionsBefore, function.instructions,
                function.instructionCount * sizeof(*instructionsBefore)) != 0 ||
         memcmp(blocksBefore, function.blocks, sizeof(blocksBefore)) != 0)) valid = ZR_FALSE;
    if (test->mapped == 1u) {
        if (function.stateMap != &logicalMap ||
            memcmp(&logicalMap, &logicalMapBefore, sizeof(logicalMap)) != 0 ||
            memcmp(&logicalEntry, &logicalEntryBefore, sizeof(logicalEntry)) != 0 ||
            memcmp(liveValues, liveValuesBefore, logicalMap.valueCount * sizeof(*liveValues)) != 0 ||
            memcmp(ownerStates, ownerStatesBefore, logicalMap.ownerStateCount * sizeof(*ownerStates)) != 0)
            valid = ZR_FALSE;
        if (!ZrCore_ExecIr_ValidateStateMap(&function, &logicalMap, &diagnostic))
            fail_with_exec_ir_diagnostic("mapped logical optimized validation", &diagnostic);
    } else if (test->mapped == 2u &&
        (function.gcMap != &gcMap || function.gcMapCount != 1u ||
         memcmp(&gcMap, &gcMapBefore, sizeof(gcMap)) != 0 ||
         memcmp(&gcEntry, &gcEntryBefore, sizeof(gcEntry)) != 0)) valid = ZR_FALSE;
    afterSuccess = ZrCore_ExecIr_RunOracleEx(&input, &after, &afterDiagnostic);
    for (i = 0u; i < function.instructionCount; ++i)
        if (function.instructions[i].sourceId == 33u) targetIndex = i;
    if (targetIndex == UINT32_MAX) valid = ZR_FALSE;
    else {
        for (i = 0u; i < function.blockCount; ++i)
            if (targetIndex >= function.blocks[i].instructionRange.start &&
                targetIndex - function.blocks[i].instructionRange.start <
                    function.blocks[i].instructionRange.count) targetBlock = i + 1u;
        if (test->strength) {
            if (loops.strengthReducedCount != (test->expectedTransform ? 1u : 0u) ||
                function.instructions[targetIndex].opcode !=
                    (test->expectedTransform ? ZR_EXEC_IR_OPCODE_COPY : test->opcode))
                valid = ZR_FALSE;
        } else if (targetBlock != (test->expectedTransform ? 1u : 3u) ||
                   function.instructions[targetIndex].opcode != test->opcode)
            valid = ZR_FALSE;
    }
    if (beforeSuccess != test->expectedSuccess || afterSuccess != beforeSuccess ||
        before.returned != after.returned ||
        before.terminatedByThrow != after.terminatedByThrow ||
        !scalar_events_equal(&before, &after)) valid = ZR_FALSE;
    if (beforeSuccess && (!before.returned ||
        !scalar_value_equal(&before.returnValue, &after.returnValue))) valid = ZR_FALSE;
    if (!beforeSuccess && (beforeDiagnostic.code != test->expectedDiagnosticCode ||
        beforeDiagnostic.code != afterDiagnostic.code ||
        beforeDiagnostic.sourceId != 33u || afterDiagnostic.sourceId != 33u ||
        beforeDiagnostic.blockId != 3u || afterDiagnostic.blockId != 3u)) valid = ZR_FALSE;
    fprintf(stderr, "%s %s before=%u after=%u targetBlock=%u reduced=%u hoisted=%u events=%u/%u source=%u/%u diagnostic=%u/%u block=%u/%u\n",
        valid ? "PASS" : "FAIL", test->name, (unsigned)beforeSuccess,
        (unsigned)afterSuccess, (unsigned)targetBlock, (unsigned)loops.strengthReducedCount,
        (unsigned)loops.hoistedInstructionCount, (unsigned)before.eventCount,
        (unsigned)after.eventCount, (unsigned)beforeDiagnostic.sourceId,
        (unsigned)afterDiagnostic.sourceId, (unsigned)beforeDiagnostic.code,
        (unsigned)afterDiagnostic.code, (unsigned)beforeDiagnostic.blockId,
        (unsigned)afterDiagnostic.blockId);
    ZrCore_ExecIr_OracleResultFree(&before);
    ZrCore_ExecIr_OracleResultFree(&after);
    ZrParser_ExecIr_LoopInfoFree(&loops);
    if (test->mapped == 1u) function.stateMap = ZR_NULL;
    if (test->mapped == 2u) {
        function.gcMap = ZR_NULL;
        function.gcMapCount = function.gcMapCapacity = 0u;
    }
    ZrCore_ExecIr_FreeFunction(&function);
    return valid ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int test_scalar_context_cases(void) {
    SZrLicmScalarCase cases[] = {
        {"signed identity pool1=1", ZR_EXEC_IR_OPCODE_MUL, {0}, {0}, 0, 1, 1, 0, 1, 1},
        {"signed nonidentity pool1=2", ZR_EXEC_IR_OPCODE_MUL, {0}, {0}, 0, 1, 1, 0, 1, 0},
        {"absent context identity", ZR_EXEC_IR_OPCODE_MUL, {0}, {0}, 0, 1, 0, 0, 1, 0},
        {"wrong function context", ZR_EXEC_IR_OPCODE_MUL, {0}, {0}, 0, 1, 1, 1, 1, 0},
        {"safe signed DIV", ZR_EXEC_IR_OPCODE_DIV, {0}, {0}, 0, 0, 1, 0, 1, 1},
        {"DIV zero", ZR_EXEC_IR_OPCODE_DIV, {0}, {0}, 0, 0, 1, 0, 0, 0},
        {"DIV MIN/-1", ZR_EXEC_IR_OPCODE_DIV, {0}, {0}, 0, 0, 1, 0, 0, 0},
        {"zero-trip DIV zero slot2=false", ZR_EXEC_IR_OPCODE_DIV, {0}, {0}, 1, 0, 1, 0, 1, 0},
        {"zero-trip DIV MIN/-1", ZR_EXEC_IR_OPCODE_DIV, {0}, {0}, 1, 0, 1, 0, 1, 0},
        {"signed ADD overflow", ZR_EXEC_IR_OPCODE_ADD, {0}, {0}, 0, 0, 1, 0, 0, 0},
        {"signed SUB overflow", ZR_EXEC_IR_OPCODE_SUB, {0}, {0}, 0, 0, 1, 0, 0, 0},
        {"signed MUL overflow", ZR_EXEC_IR_OPCODE_MUL, {0}, {0}, 0, 0, 1, 0, 0, 0},
        {"unsigned ADD overflow", ZR_EXEC_IR_OPCODE_ADD, {0}, {0}, 0, 0, 1, 0, 0, 0},
        {"unsigned SUB underflow", ZR_EXEC_IR_OPCODE_SUB, {0}, {0}, 0, 0, 1, 0, 0, 0},
        {"unsigned MUL overflow", ZR_EXEC_IR_OPCODE_MUL, {0}, {0}, 0, 0, 1, 0, 0, 0},
        {"float negative zero identity", ZR_EXEC_IR_OPCODE_MUL, {0}, {0}, 0, 1, 1, 0, 1, 0},
        {"mixed unsigned/signed identity", ZR_EXEC_IR_OPCODE_MUL, {0}, {0}, 0, 1, 1, 0, 1, 0},
        {"opaque undefined base", ZR_EXEC_IR_OPCODE_MUL, {0}, {0}, 0, 1, 1, 0, 0, 0},
        {"CONVERT NaN", ZR_EXEC_IR_OPCODE_CONVERT, {0}, {0}, 0, 0, 1, 0, 0, 0},
        {"CONVERT out of range", ZR_EXEC_IR_OPCODE_CONVERT, {0}, {0}, 0, 0, 1, 0, 0, 0},
        {"zero-trip CONVERT NaN", ZR_EXEC_IR_OPCODE_CONVERT, {0}, {0}, 1, 0, 1, 0, 1, 0},
        {"safe signed ADD", ZR_EXEC_IR_OPCODE_ADD, {0}, {0}, 0, 0, 1, 0, 1, 1},
        {"safe signed SUB", ZR_EXEC_IR_OPCODE_SUB, {0}, {0}, 0, 0, 1, 0, 1, 1},
        {"safe signed MUL", ZR_EXEC_IR_OPCODE_MUL, {0}, {0}, 0, 0, 1, 0, 1, 1},
        {"safe unsigned DIV", ZR_EXEC_IR_OPCODE_DIV, {0}, {0}, 0, 0, 1, 0, 1, 1},
        {"absent context safe DIV", ZR_EXEC_IR_OPCODE_DIV, {0}, {0}, 0, 0, 0, 0, 1, 0},
        {"unsigned identity", ZR_EXEC_IR_OPCODE_MUL, {0}, {0}, 0, 1, 1, 0, 1, 1},
        {"borrowed logical map preserves sites", ZR_EXEC_IR_OPCODE_DIV, {0}, {0}, 0, 0, 1, 0, 1, 0, 1},
        {"borrowed GC map preserves sites", ZR_EXEC_IR_OPCODE_MUL, {0}, {0}, 0, 0, 1, 0, 1, 0, 2}
    };
    TZrUInt32 i;
    int result = EXIT_SUCCESS;
    for (i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        cases[i].left = signed_value(9);
        cases[i].right = signed_value(1);
        cases[i].expectedDiagnosticCode = cases[i].expectedSuccess
            ? ZR_EXECUTION_DIAGNOSTIC_NONE : ZR_EXEC_IR_DIAGNOSTIC_ARITHMETIC_ERROR;
    }
    cases[1].right = signed_value(2);
    cases[4].right = signed_value(3);
    cases[5].right = signed_value(0);
    cases[6].left = signed_value(INT64_MIN); cases[6].right = signed_value(-1);
    cases[7].right = signed_value(0);
    cases[8].left = signed_value(INT64_MIN); cases[8].right = signed_value(-1);
    cases[9].left = signed_value(INT64_MAX);
    cases[10].left = signed_value(INT64_MIN);
    cases[11].left = signed_value(INT64_MAX); cases[11].right = signed_value(2);
    cases[12].left = unsigned_value(UINT64_MAX); cases[12].right = unsigned_value(1);
    cases[13].left = unsigned_value(0); cases[13].right = unsigned_value(1);
    cases[14].left = unsigned_value(UINT64_MAX); cases[14].right = unsigned_value(2);
    cases[15].left = float_value(-0.0); cases[15].right = float_value(1.0);
    cases[16].right = unsigned_value(1);
    memset(&cases[17].left, 0, sizeof(cases[17].left));
    /* Core rejects UNDEFINED while collecting operands, before arithmetic:
     * exec_ir_interpreter.c's operand loop reports INVALID_VALUE at source33. */
    cases[17].expectedDiagnosticCode = ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE;
    cases[18].left = float_value(NAN);
    cases[19].left = float_value(INFINITY);
    cases[20].left = float_value(NAN);
    cases[24].left = unsigned_value(9); cases[24].right = unsigned_value(3);
    cases[25].right = signed_value(3);
    cases[26].left = unsigned_value(9); cases[26].right = unsigned_value(1);
    cases[27].right = signed_value(3);
    for (i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i)
        if (run_scalar_case(&cases[i]) != EXIT_SUCCESS) result = EXIT_FAILURE;
    return result;
}
