#include "zr_vm_core/exec_ir_interpreter.h"
#include "zr_vm_parser/exec_ir_execbc.h"
#include "zr_vm_parser/exec_ir_pass_manager.h"
#include "../../zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_internal.h"
#include "zr_vm_common/zr_type_conf.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static SZrExecIrRange empty_range(void) {
    SZrExecIrRange range = {0u, 0u};
    return range;
}

static void append(SZrExecIrFunction *function, EZrExecIrOpcode opcode,
                   TZrExecIrValueId operand, TZrExecIrValueId result,
                   TZrExecIrTypeToken type, TZrUInt32 immediate) {
    SZrExecIrInstruction instruction = {0};
    TZrExecIrInstructionId id;
    instruction.opcode = (TZrUInt16)opcode;
    instruction.typeToken = type;
    instruction.layoutId = immediate;
    instruction.sourceId = function->instructionCount + 101u;
    instruction.operands = empty_range();
    instruction.results = empty_range();
    if (operand != 0u)
        CHECK(ZrCore_ExecIr_FunctionAppendOperands(function, &operand, 1u,
                                                      &instruction.operands));
    if (result != 0u)
        CHECK(ZrCore_ExecIr_FunctionAppendResults(function, &result, 1u,
                                                     &instruction.results));
    CHECK(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, &id));
}

static void build_conversion(SZrExecIrFunction *function,
                             TZrExecIrTypeToken sourceType,
                             TZrExecIrTypeToken targetType,
                             TZrUInt32 immediate, TZrBool implicitTarget) {
    TZrExecIrValueId source, result;
    ZrCore_ExecIr_FunctionInit(function);
    function->id = 1u;
    function->functionToken = 7u;
    function->signatureHash = 99u;
    source = ZrCore_ExecIr_FunctionAddValue(function, sourceType,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    result = ZrCore_ExecIr_FunctionAddValue(function, targetType,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    CHECK(source != 0u && result != 0u);
    append(function, ZR_EXEC_IR_OPCODE_CONSTANT, 0u, source, sourceType, immediate);
    append(function, ZR_EXEC_IR_OPCODE_CONVERT, source, result,
           implicitTarget ? 0u : targetType, 0u);
    append(function, ZR_EXEC_IR_OPCODE_RETURN, result, 0u, 0u, 0u);
}

static void check_runners(const SZrExecIrFunction *function,
                          const SZrExecIrOracleValue *constant,
                          EZrExecIrOracleValueKind kind, TZrFloat64 expected) {
    SZrExecIrOracleInput input = {0};
    SZrExecIrOracleExecutionResult oracle = {0};
    SZrExecBcProjection projection = {0};
    SZrExecBcExecutionInput bcInput = {0};
    SZrExecBcExecutionResult bcResult;
    SZrExecIrDiagnostic diagnostic;
    input.function = function;
    input.constants = constant;
    input.constantCount = constant != NULL ? 1u : 0u;
    CHECK(ZrCore_ExecIr_RunOracleEx(&input, &oracle, &diagnostic));
    CHECK(oracle.returned && oracle.returnValue.kind == kind);
    if (kind == ZR_EXEC_IR_ORACLE_VALUE_FLOAT)
        CHECK(oracle.returnValue.as.floating == expected);
    else if (kind == ZR_EXEC_IR_ORACLE_VALUE_BOOL)
        CHECK(oracle.returnValue.as.boolean == (TZrBool)expected);
    else if (kind == ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED)
        CHECK(oracle.returnValue.as.unsignedInteger == (TZrUInt64)expected);
    else
        CHECK(oracle.returnValue.as.signedInteger == (TZrInt64)expected);
    ZrCore_ExecIr_OracleResultFree(&oracle);
    CHECK(ZrParser_ExecIr_LowerExecBc(function, &projection, &diagnostic));
    bcInput.constants = constant;
    bcInput.constantCount = constant != NULL ? 1u : 0u;
    ZrParser_ExecBcExecutionResult_Init(&bcResult);
    CHECK(ZrParser_ExecBcProjection_Run(&projection, &bcInput, &bcResult,
                                          &diagnostic));
    CHECK(bcResult.returned && bcResult.returnValue.kind == kind);
    if (kind == ZR_EXEC_IR_ORACLE_VALUE_FLOAT)
        CHECK(bcResult.returnValue.as.floating == expected);
    else if (kind == ZR_EXEC_IR_ORACLE_VALUE_BOOL)
        CHECK(bcResult.returnValue.as.boolean == (TZrBool)expected);
    else if (kind == ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED)
        CHECK(bcResult.returnValue.as.unsignedInteger == (TZrUInt64)expected);
    else
        CHECK(bcResult.returnValue.as.signedInteger == (TZrInt64)expected);
    ZrParser_ExecBcExecutionResult_Free(&bcResult);
    ZrParser_ExecBcProjection_Free(&projection);
}

static void run_sccp(SZrExecIrFunction *function,
                      const SZrExecIrConstant *constant,
                      SZrExecIrAnalysisCache *cache) {
    SZrExecIrPassContext context = {0};
    SZrExecIrDiagnostic diagnostic;
    const SZrExecIrPassInfo *passes = ZrParser_ExecIr_GetScalarPasses(NULL);
    context.cache = cache;
    context.constants = constant;
    context.constantCount = constant != NULL ? 1u : 0u;
    if (!ZrParser_ExecIr_RunPassPipeline(function, passes, 1u, &context,
                                           &diagnostic)) {
        fprintf(stderr,
                "SCCP pipeline diagnostic: code=%u function=%u block=%u instruction=%u source=%u expected=%u actual=%u\n",
                (unsigned)diagnostic.code, (unsigned)diagnostic.functionToken,
                (unsigned)diagnostic.blockId, (unsigned)diagnostic.instructionId,
                (unsigned)diagnostic.sourceId,
                (unsigned)diagnostic.expectedVersion,
                (unsigned)diagnostic.actualVersion);
        exit(EXIT_FAILURE);
    }
}

static void test_immediate_int_to_double_preserves_conversion(TZrBool implicitTarget) {
    SZrExecIrFunction function;
    SZrExecIrAnalysisCache cache;
    build_conversion(&function, ZR_VALUE_TYPE_INT64, ZR_VALUE_TYPE_DOUBLE,
                     7u, implicitTarget);
    ZrParser_ExecIr_AnalysisCacheInit(&cache);
    check_runners(&function, NULL, ZR_EXEC_IR_ORACLE_VALUE_FLOAT, 7.0);
    run_sccp(&function, NULL, &cache);
    /* Retaining the runtime conversion preserves its numeric result kind. */
    check_runners(&function, NULL, ZR_EXEC_IR_ORACLE_VALUE_FLOAT, 7.0);
    CHECK(function.instructions[1].opcode == ZR_EXEC_IR_OPCODE_CONVERT);
    CHECK(cache.sccpValueCount == 2u);
    CHECK(cache.sccpValues[1].kind == ZR_EXEC_IR_SCCP_OVERDEFINED);
    ZrParser_ExecIr_AnalysisCacheFree(&cache);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_pool_conversion(TZrExecIrTypeToken sourceType,
                                 TZrExecIrTypeToken targetType,
                                 SZrExecIrOracleValue value,
                                 EZrExecIrOracleValueKind expectedKind,
                                 TZrFloat64 expected,
                                 EZrExecIrSccpLatticeKind expectedLattice) {
    SZrExecIrFunction function;
    SZrExecIrConstant constant = {0};
    SZrExecIrAnalysisCache cache;
    build_conversion(&function, sourceType, targetType, 0u, ZR_FALSE);
    constant.typeToken = sourceType;
    if (value.kind == ZR_EXEC_IR_ORACLE_VALUE_FLOAT) {
        memcpy(&constant.bits, &value.as.floating, sizeof(constant.bits));
    } else {
        constant.bits = (TZrUInt64)value.as.signedInteger;
    }
    ZrParser_ExecIr_AnalysisCacheInit(&cache);
    check_runners(&function, &value, expectedKind, expected);
    run_sccp(&function, &constant, &cache);
    check_runners(&function, &value, expectedKind, expected);
    CHECK(cache.sccpValueCount == 2u);
    if (cache.sccpValues[1].kind != expectedLattice) {
        fprintf(stderr, "conversion %u -> %u: lattice %u, expected %u\n",
                (unsigned)sourceType, (unsigned)targetType,
                (unsigned)cache.sccpValues[1].kind, (unsigned)expectedLattice);
    }
    CHECK(cache.sccpValues[1].kind == expectedLattice);
    if (expectedLattice == ZR_EXEC_IR_SCCP_CONSTANT) {
        CHECK(cache.sccpValues[1].bits == constant.bits);
        CHECK(cache.sccpValues[1].typeToken == targetType);
    }
    ZrParser_ExecIr_AnalysisCacheFree(&cache);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_same_type_immediate_still_folds(void) {
    SZrExecIrFunction function;
    SZrExecIrAnalysisCache cache;
    build_conversion(&function, ZR_VALUE_TYPE_INT64, ZR_VALUE_TYPE_INT64,
                     7u, ZR_FALSE);
    ZrParser_ExecIr_AnalysisCacheInit(&cache);
    run_sccp(&function, NULL, &cache);
    CHECK(function.instructions[1].opcode == ZR_EXEC_IR_OPCODE_CONSTANT);
    check_runners(&function, NULL, ZR_EXEC_IR_ORACLE_VALUE_SIGNED, 7.0);
    ZrParser_ExecIr_AnalysisCacheFree(&cache);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_int_double_int_roundtrip_does_not_keep_original_bits(void) {
    SZrExecIrFunction function;
    SZrExecIrAnalysisCache cache;
    SZrExecIrConstant constant = {0};
    SZrExecIrOracleValue value = {0};
    TZrExecIrValueId result;
    build_conversion(&function, ZR_VALUE_TYPE_INT64, ZR_VALUE_TYPE_DOUBLE,
                     0u, ZR_FALSE);
    /* Replace RETURN with a second supported conversion and a new RETURN. */
    --function.instructionCount;
    result = ZrCore_ExecIr_FunctionAddValue(&function, ZR_VALUE_TYPE_INT64,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    CHECK(result != 0u);
    append(&function, ZR_EXEC_IR_OPCODE_CONVERT, 2u, result,
           ZR_VALUE_TYPE_INT64, 0u);
    append(&function, ZR_EXEC_IR_OPCODE_RETURN, result, 0u, 0u, 0u);
    constant.typeToken = ZR_VALUE_TYPE_INT64;
    constant.bits = UINT64_C(9007199254740993);
    value.kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    value.as.signedInteger = INT64_C(9007199254740993);
    ZrParser_ExecIr_AnalysisCacheInit(&cache);
    check_runners(&function, &value, ZR_EXEC_IR_ORACLE_VALUE_SIGNED,
                  9007199254740992.0);
    run_sccp(&function, &constant, &cache);
    check_runners(&function, &value, ZR_EXEC_IR_ORACLE_VALUE_SIGNED,
                  9007199254740992.0);
    CHECK(cache.sccpValueCount == 3u);
    CHECK(cache.sccpValues[1].kind == ZR_EXEC_IR_SCCP_OVERDEFINED);
    CHECK(cache.sccpValues[2].kind == ZR_EXEC_IR_SCCP_OVERDEFINED);
    ZrParser_ExecIr_AnalysisCacheFree(&cache);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_unknown_type_conversion_does_not_claim_constant(void) {
    SZrExecIrFunction function;
    SZrExecIrAnalysisCache cache;
    /* Zero denotes unspecified type information, accepted by the verifier. */
    build_conversion(&function, 0u, 0u, 7u, ZR_FALSE);
    ZrParser_ExecIr_AnalysisCacheInit(&cache);
    run_sccp(&function, NULL, &cache);
    CHECK(function.instructions[1].opcode == ZR_EXEC_IR_OPCODE_CONVERT);
    CHECK(cache.sccpValueCount == 2u);
    CHECK(cache.sccpValues[1].kind == ZR_EXEC_IR_SCCP_OVERDEFINED);
    ZrParser_ExecIr_AnalysisCacheFree(&cache);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_explicit_target_overrides_result_annotation(void) {
    SZrExecIrFunction function;
    SZrExecIrAnalysisCache cache;
    /* CONVERT's explicit scalar target takes precedence over its annotation.
     * The producer can carry a type identity and scalar target separately. */
    build_conversion(&function, ZR_VALUE_TYPE_INT64, ZR_VALUE_TYPE_INT64,
                     7u, ZR_FALSE);
    function.instructions[1].typeToken = ZR_VALUE_TYPE_DOUBLE;
    ZrParser_ExecIr_AnalysisCacheInit(&cache);
    check_runners(&function, NULL, ZR_EXEC_IR_ORACLE_VALUE_FLOAT, 7.0);
    run_sccp(&function, NULL, &cache);
    check_runners(&function, NULL, ZR_EXEC_IR_ORACLE_VALUE_FLOAT, 7.0);
    CHECK(function.instructions[1].opcode == ZR_EXEC_IR_OPCODE_CONVERT);
    CHECK(cache.sccpValueCount == 2u);
    CHECK(cache.sccpValues[1].kind == ZR_EXEC_IR_SCCP_OVERDEFINED);
    ZrParser_ExecIr_AnalysisCacheFree(&cache);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_same_token_representation(TZrExecIrTypeToken token,
                                          EZrExecIrOracleValueKind kind,
                                          TZrFloat64 expected,
                                          TZrBool implicitTarget,
                                          TZrBool folds) {
    SZrExecIrFunction function;
    SZrExecIrAnalysisCache cache;
    build_conversion(&function, token, token, 7u, implicitTarget);
    ZrParser_ExecIr_AnalysisCacheInit(&cache);
    check_runners(&function, NULL, kind, expected);
    run_sccp(&function, NULL, &cache);
    check_runners(&function, NULL, kind, expected);
    CHECK(function.instructions[1].opcode ==
          (folds ? ZR_EXEC_IR_OPCODE_CONSTANT : ZR_EXEC_IR_OPCODE_CONVERT));
    if (!folds) {
        CHECK(cache.sccpValueCount == 2u);
        CHECK(cache.sccpValues[1].kind == ZR_EXEC_IR_SCCP_OVERDEFINED);
    }
    ZrParser_ExecIr_AnalysisCacheFree(&cache);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_retagged_pool_representation(EZrExecIrOpcode opcode) {
    SZrExecIrFunction function;
    SZrExecIrAnalysisCache cache;
    SZrExecIrPassContext context = {0};
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrConstant constant = {0};
    SZrExecIrOracleValue value = {0};
    TZrExecIrValueId converted;
    TZrBool changed = ZR_FALSE;
    build_conversion(&function, ZR_VALUE_TYPE_DOUBLE, ZR_VALUE_TYPE_INT64,
                     0u, ZR_FALSE);
    function.instructions[1].opcode = (TZrUInt16)opcode;
    function.instructions[1].typeToken = ZR_VALUE_TYPE_INT64;
    --function.instructionCount;
    converted = ZrCore_ExecIr_FunctionAddValue(&function, ZR_VALUE_TYPE_INT64,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    CHECK(converted == 3u);
    append(&function, ZR_EXEC_IR_OPCODE_CONVERT, 2u, converted,
           ZR_VALUE_TYPE_INT64, 0u);
    append(&function, ZR_EXEC_IR_OPCODE_RETURN, converted, 0u, 0u, 0u);
    value.kind = ZR_EXEC_IR_ORACLE_VALUE_FLOAT;
    value.as.floating = -7.75;
    constant.typeToken = ZR_VALUE_TYPE_DOUBLE;
    memcpy(&constant.bits, &value.as.floating, sizeof(constant.bits));
    ZrParser_ExecIr_AnalysisCacheInit(&cache);
    context.cache = &cache;
    context.constants = &constant;
    context.constantCount = 1u;
    check_runners(&function, &value, ZR_EXEC_IR_ORACLE_VALUE_SIGNED,
                  opcode == ZR_EXEC_IR_OPCODE_NEG ? 7.0 : -7.0);
    /* Analyze before alias rewriting: a COPY can retag FLOAT storage as INT64,
     * and NEG preserves FLOAT storage even with a signed result annotation. */
    CHECK(ZrParser_ExecIr_ComputeSccp(&function, &context, ZR_FALSE, &changed,
                                   &diagnostic));
    CHECK(!changed && cache.sccpValueCount == 3u);
    CHECK(cache.sccpValues[2].kind == ZR_EXEC_IR_SCCP_OVERDEFINED);
    run_sccp(&function, &constant, &cache);
    CHECK(function.instructions[2].opcode == ZR_EXEC_IR_OPCODE_CONVERT);
    check_runners(&function, &value, ZR_EXEC_IR_ORACLE_VALUE_SIGNED,
                  opcode == ZR_EXEC_IR_OPCODE_NEG ? 7.0 : -7.0);
    ZrParser_ExecIr_AnalysisCacheFree(&cache);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_signed_copy_keeps_representation_proof(void) {
    SZrExecIrFunction function;
    SZrExecIrAnalysisCache cache;
    TZrExecIrValueId copied;
    build_conversion(&function, ZR_VALUE_TYPE_INT64, ZR_VALUE_TYPE_INT64,
                     7u, ZR_FALSE);
    function.instructions[1].opcode = ZR_EXEC_IR_OPCODE_COPY;
    --function.instructionCount;
    copied = ZrCore_ExecIr_FunctionAddValue(&function, ZR_VALUE_TYPE_INT64,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    CHECK(copied == 3u);
    append(&function, ZR_EXEC_IR_OPCODE_CONVERT, 2u, copied,
           ZR_VALUE_TYPE_INT64, 0u);
    append(&function, ZR_EXEC_IR_OPCODE_RETURN, copied, 0u, 0u, 0u);
    ZrParser_ExecIr_AnalysisCacheInit(&cache);
    check_runners(&function, NULL, ZR_EXEC_IR_ORACLE_VALUE_SIGNED, 7.0);
    run_sccp(&function, NULL, &cache);
    CHECK(function.instructions[2].opcode == ZR_EXEC_IR_OPCODE_CONSTANT);
    check_runners(&function, NULL, ZR_EXEC_IR_ORACLE_VALUE_SIGNED, 7.0);
    ZrParser_ExecIr_AnalysisCacheFree(&cache);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_loop_phi_representation_merge(TZrBool signedBackedge) {
    SZrExecIrFunction function;
    SZrExecIrAnalysisCache cache;
    SZrExecIrPassContext context = {0};
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrPhi phi = {0};
    SZrExecIrPhiIncoming incoming[2] = {{1u, 1u}, {3u, 6u}};
    TZrExecIrBlockId entryTarget = 2u, headerTargets[2] = {3u, 4u};
    TZrExecIrBlockId headerPredecessors[2] = {1u, 3u}, header = 2u;
    TZrExecIrValueId comparisonOperands[2] = {1u, 2u};
    TZrBool changed = ZR_FALSE;
    ZrCore_ExecIr_FunctionInit(&function);
    function.id = 1u; function.functionToken = 7u; function.signatureHash = 99u;
    for (unsigned i = 0u; i < 6u; ++i)
        CHECK(ZrCore_ExecIr_FunctionAddValue(&function,
                i == 2u ? ZR_VALUE_TYPE_BOOL : ZR_VALUE_TYPE_INT64,
                ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN) == i + 1u);
    for (unsigned i = 0u; i < 4u; ++i)
        CHECK(ZrCore_ExecIr_FunctionAddBlock(&function,
                i == 0u ? ZR_EXEC_IR_BLOCK_FLAG_ENTRY : 0u) == i + 1u);
    function.entryBlockId = 1u;
    CHECK(ZrCore_ExecIr_FunctionAppendSuccessors(&function, &entryTarget, 1u,
                                               &function.blocks[0].successors));
    CHECK(ZrCore_ExecIr_FunctionAppendSuccessors(&function, headerTargets, 2u,
                                               &function.blocks[1].successors));
    CHECK(ZrCore_ExecIr_FunctionAppendSuccessors(&function, &header, 1u,
                                               &function.blocks[2].successors));
    CHECK(ZrCore_ExecIr_FunctionAppendPredecessors(&function, headerPredecessors, 2u,
                                                 &function.blocks[1].predecessors));
    for (unsigned i = 2u; i < 4u; ++i)
        CHECK(ZrCore_ExecIr_FunctionAppendPredecessors(&function, &header, 1u,
                                                     &function.blocks[i].predecessors));
    append(&function, ZR_EXEC_IR_OPCODE_CONSTANT, 0u, 1u, ZR_VALUE_TYPE_INT64, 1u);
    append(&function, ZR_EXEC_IR_OPCODE_CONSTANT, 0u, 2u, ZR_VALUE_TYPE_INT64, 0u);
    /* Runtime false, but BOOL conversion is OVERDEFINED for SCCP, so both
     * paths participate and the backedge fact arrives after the first PHI. */
    append(&function, ZR_EXEC_IR_OPCODE_CONVERT, 2u, 3u, ZR_VALUE_TYPE_BOOL, 0u);
    append(&function, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u, 0u);
    function.instructions[3].successorRange = function.blocks[0].successors;
    append(&function, ZR_EXEC_IR_OPCODE_CONVERT, 4u, 5u, ZR_VALUE_TYPE_INT64, 0u);
    append(&function, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH, 3u, 0u, 0u, 0u);
    function.instructions[5].successorRange = function.blocks[1].successors;
    append(&function, signedBackedge ? ZR_EXEC_IR_OPCODE_COPY : ZR_EXEC_IR_OPCODE_COMPARE,
           1u, 6u, signedBackedge ? ZR_VALUE_TYPE_INT64 : 5u, 0u);
    if (!signedBackedge)
        CHECK(ZrCore_ExecIr_FunctionAppendOperands(&function, comparisonOperands, 2u,
                                                  &function.instructions[6].operands));
    append(&function, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u, 0u);
    function.instructions[7].successorRange = function.blocks[2].successors;
    append(&function, ZR_EXEC_IR_OPCODE_RETURN, 5u, 0u, 0u, 0u);
    const unsigned starts[4] = {0u, 4u, 6u, 8u}, counts[4] = {4u, 2u, 2u, 1u};
    for (unsigned i = 0u; i < 4u; ++i) {
        function.blocks[i].instructions.start = starts[i];
        function.blocks[i].instructions.count = counts[i];
        function.blocks[i].terminatorInstructionId = starts[i] + counts[i];
    }
    phi.result = 4u;
    CHECK(ZrCore_ExecIr_FunctionAppendPhiIncoming(&function, incoming, 2u, &phi.incomings));
    CHECK(ZrCore_ExecIr_FunctionAppendPhis(&function, &phi, 1u, &function.blocks[1].phis));
    ZrParser_ExecIr_AnalysisCacheInit(&cache);
    context.cache = &cache;
    check_runners(&function, NULL, ZR_EXEC_IR_ORACLE_VALUE_SIGNED, 1.0);
    CHECK(ZrCore_ExecIr_VerifyFunction(&function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
    CHECK(ZrParser_ExecIr_ComputeSccp(&function, &context, ZR_FALSE, &changed, &diagnostic));
    CHECK(!changed && cache.sccpValueCount == 6u);
    CHECK(cache.sccpValues[3].kind == ZR_EXEC_IR_SCCP_CONSTANT);
    CHECK(cache.sccpValues[3].bits == 1u);
    CHECK(cache.sccpValues[4].kind == (signedBackedge ?
          ZR_EXEC_IR_SCCP_CONSTANT : ZR_EXEC_IR_SCCP_OVERDEFINED));
    run_sccp(&function, NULL, &cache);
    CHECK(function.instructions[4].opcode == (signedBackedge ?
          ZR_EXEC_IR_OPCODE_CONSTANT : ZR_EXEC_IR_OPCODE_CONVERT));
    check_runners(&function, NULL, ZR_EXEC_IR_ORACLE_VALUE_SIGNED, 1.0);
    ZrParser_ExecIr_AnalysisCacheFree(&cache);
    ZrCore_ExecIr_FreeFunction(&function);
}

int main(void) {
    SZrExecIrOracleValue floating = {0}, signedValue = {0};
    floating.kind = ZR_EXEC_IR_ORACLE_VALUE_FLOAT;
    floating.as.floating = -7.75;
    signedValue.kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    signedValue.as.signedInteger = 7;
    test_immediate_int_to_double_preserves_conversion(ZR_FALSE);
    test_immediate_int_to_double_preserves_conversion(ZR_TRUE);
    test_pool_conversion(ZR_VALUE_TYPE_DOUBLE, ZR_VALUE_TYPE_INT64, floating,
                         ZR_EXEC_IR_ORACLE_VALUE_SIGNED, -7.0,
                         ZR_EXEC_IR_SCCP_OVERDEFINED);
    test_pool_conversion(ZR_VALUE_TYPE_INT64, ZR_VALUE_TYPE_BOOL, signedValue,
                         ZR_EXEC_IR_ORACLE_VALUE_BOOL, 1.0,
                         ZR_EXEC_IR_SCCP_OVERDEFINED);
    test_pool_conversion(ZR_VALUE_TYPE_DOUBLE, ZR_VALUE_TYPE_DOUBLE, floating,
                         ZR_EXEC_IR_ORACLE_VALUE_FLOAT, -7.75,
                         ZR_EXEC_IR_SCCP_OVERDEFINED);
    test_pool_conversion(ZR_VALUE_TYPE_INT64, ZR_VALUE_TYPE_INT64, signedValue,
                         ZR_EXEC_IR_ORACLE_VALUE_SIGNED, 7.0,
                         ZR_EXEC_IR_SCCP_CONSTANT);
    test_same_type_immediate_still_folds();
    test_int_double_int_roundtrip_does_not_keep_original_bits();
    test_unknown_type_conversion_does_not_claim_constant();
    test_explicit_target_overrides_result_annotation();
    for (unsigned implicit = 0u; implicit < 2u; ++implicit) {
        test_same_token_representation(ZR_VALUE_TYPE_DOUBLE,
                ZR_EXEC_IR_ORACLE_VALUE_FLOAT, 7.0, (TZrBool)implicit, ZR_FALSE);
        test_same_token_representation(ZR_VALUE_TYPE_FLOAT,
                ZR_EXEC_IR_ORACLE_VALUE_FLOAT, 7.0, (TZrBool)implicit, ZR_FALSE);
        test_same_token_representation(ZR_VALUE_TYPE_BOOL,
                ZR_EXEC_IR_ORACLE_VALUE_BOOL, 1.0, (TZrBool)implicit, ZR_FALSE);
        test_same_token_representation(ZR_VALUE_TYPE_UINT64,
                ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED, 7.0, (TZrBool)implicit, ZR_FALSE);
        test_same_token_representation(0x9001u,
                ZR_EXEC_IR_ORACLE_VALUE_SIGNED, 7.0, (TZrBool)implicit, ZR_FALSE);
        for (unsigned token = ZR_VALUE_TYPE_INT8; token <= ZR_VALUE_TYPE_INT64; ++token)
            test_same_token_representation(token, ZR_EXEC_IR_ORACLE_VALUE_SIGNED,
                    7.0, (TZrBool)implicit, ZR_TRUE);
    }
    test_retagged_pool_representation(ZR_EXEC_IR_OPCODE_COPY);
    test_retagged_pool_representation(ZR_EXEC_IR_OPCODE_NEG);
    test_signed_copy_keeps_representation_proof();
    test_loop_phi_representation_merge(ZR_TRUE);
    test_loop_phi_representation_merge(ZR_FALSE);
    puts("SCCP conversion preservation: 33 cases passed");
    return 0;
}
