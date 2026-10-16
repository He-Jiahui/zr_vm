#include "tests/harness/runtime_support.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/exec_ir_builder.h"
#include "exec_ir/execbc/exec_ir_execbc_vm.h"
#include "exec_ir/exec_ir_projections.h"
#include "zr_vm_parser/semantic_ir.h"
#include "compiler/semantic/compiler_semantic_compare.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "exec_ir/exec_ir_mutation_internal.h"

enum { FUNCTION_TOKEN = 0xC041u, LEFT = 1u, RIGHT = 2u, RESULT = 3u,
       COMPARE_ID = 3u, BRANCH_ID = 4u, FIXTURE_INSTRUCTIONS = 6u };

typedef struct SFixture {
    SZrState *state;
    SZrSemanticContext *context;
    SZrSemanticIrFunction semantic;
    SZrExecIrModule module;
    SZrExecBcProjection projection;
    TZrTypeId intType, boolType, floatType, narrowType;
    TZrBool semanticInitialized;
} SFixture;

typedef struct SBytes {
    const void *input;
    void *before;
    size_t size;
} SBytes;

static unsigned cases, failures, preconditions, preconditionFailures;
static const char *caseName;
static TZrBool caseOk;

#define EXPECT(condition) do { if (!(condition)) { caseOk = ZR_FALSE; \
    fprintf(stderr, "SEMANTIC FAIL [%s] line=%u: %s\n", caseName, \
            (unsigned)__LINE__, #condition); } } while (0)

static TZrBool prerequisite(const char *label, TZrBool passed) {
    ++preconditions;
    printf("PRECONDITION %s %s %s\n", caseName, label, passed ? "PASS" : "FAIL");
    if (!passed) { ++preconditionFailures; caseOk = ZR_FALSE; }
    return passed;
}

static void begin_case(const char *name) {
    caseName = name;
    caseOk = ZR_TRUE;
    ++cases;
}

static void end_case(void) {
    if (!caseOk) ++failures;
    printf("CASE %s %s\n", caseName, caseOk ? "PASS" : "FAIL");
}

static TZrBool save_bytes(SBytes *saved, const void *input, size_t size) {
    saved->input = input;
    saved->size = size;
    saved->before = size ? malloc(size) : ZR_NULL;
    if (size && (input == ZR_NULL || saved->before == ZR_NULL)) return ZR_FALSE;
    if (size) memcpy(saved->before, input, size);
    return ZR_TRUE;
}

static void check_bytes(SBytes *saved, size_t count) {
    for (size_t index = 0u; index < count; ++index) {
        EXPECT(saved[index].size == 0u || (saved[index].before != ZR_NULL &&
                memcmp(saved[index].input, saved[index].before, saved[index].size) == 0));
        free(saved[index].before);
    }
}

/* Include every projection side pool, even pools empty in this finite CFG.
 * Byte snapshots are test evidence, never alternate source or runtime input. */
static TZrBool snapshot_projection(const SFixture *f, SBytes saved[28]) {
    const SZrExecBcProjection *p = &f->projection;
    size_t next = 0u;
#define SAVE_RAW(pointer, bytes) do { if (!save_bytes(&saved[next++], pointer, bytes)) \
        return ZR_FALSE; } while (0)
#define SAVE_POOL(field, count) SAVE_RAW(p->field, (size_t)p->count * sizeof(*p->field))
    SAVE_RAW(p, sizeof(*p));
    SAVE_POOL(frameSlots, frameSlotCount);
    SAVE_POOL(opcodes, instructionCount);
    SAVE_POOL(instructions, instructionCount);
    SAVE_POOL(operands, operandCount);
    SAVE_POOL(results, resultCount);
    SAVE_POOL(memoryTokens, memoryTokenCount);
    SAVE_POOL(valueSlots, valueSlotCount);
    SAVE_POOL(slotValues, physicalSlotCount);
    SAVE_POOL(blocks, blockCount);
    SAVE_POOL(predecessors, predecessorCount);
    SAVE_POOL(successors, successorCount);
    SAVE_POOL(phis, phiCount);
    SAVE_POOL(phiIncomings, phiIncomingCount);
    SAVE_POOL(phiCopySources, phiCopyCount);
    SAVE_POOL(phiCopyDestinations, phiCopyCount);
    SAVE_POOL(phiCopyEdges, phiCopyCount);
    SAVE_POOL(phiMoves, phiMoveCount);
    SAVE_POOL(sourceMaps, sourceMapCount);
    SAVE_POOL(constants, constantCount);
    SAVE_POOL(layouts, layoutCount);
    SAVE_POOL(gcRoots, gcRootCount);
    SAVE_POOL(deoptStates, deoptStateCount);
    SAVE_POOL(deoptValues, deoptValueCount);
    SAVE_POOL(deoptAggregates, deoptAggregateCount);
    SAVE_POOL(deoptAggregateFields, deoptAggregateFieldCount);
    SAVE_RAW(&f->context->canonicalTypes, sizeof(f->context->canonicalTypes));
    SAVE_RAW(f->context->canonicalTypes.head,
            f->context->canonicalTypes.length * sizeof(SZrCanonicalTypeNode));
#undef SAVE_POOL
#undef SAVE_RAW
    return ZR_TRUE;
}

static SZrSemanticIrInstruction *instruction(SFixture *f, TZrUInt32 id) {
    return (SZrSemanticIrInstruction *)ZrCore_Array_Get(&f->semantic.instructions, id - 1u);
}

static SZrSemanticIrValue *value(SFixture *f, TZrUInt32 id) {
    return (SZrSemanticIrValue *)ZrCore_Array_Get(&f->semantic.values, id - 1u);
}

static TZrBool emit(SFixture *f, EZrSemanticIrOpcode opcode, TZrTypeId type,
                    TZrValueId result, const TZrValueId *operands, TZrSize count,
                    TZrUInt32 constant, TZrUInt32 predicate) {
    SZrSemanticIrInstructionSpec spec = {0};
    spec.opcode = opcode;
    spec.typeId = type;
    spec.resultValueId = result;
    spec.operands = operands;
    spec.operandCount = count;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange.start.offset = (TZrUInt32)f->semantic.instructions.length * 10u;
    spec.sourceRange.end.offset = spec.sourceRange.start.offset + 5u;
    if (opcode == ZR_SEMANTIC_IR_CONSTANT) {
        spec.hasConstantPoolIndex = ZR_TRUE;
        spec.constantPoolIndex = constant;
    }
    if (opcode == ZR_SEMANTIC_IR_COMPARE) {
        spec.comparisonPredicate = predicate;
        spec.comparisonOperandTypeId = f->intType;
    }
    return (TZrBool)(ZrParser_SemanticIr_Emit(&f->semantic, &spec) != 0u);
}

/* All arrays and IDs originate in actual production constructors. The stable
 * function/signature identities are fixture-only, not source callable proof. */
static TZrBool initialize_fixture(SFixture *f) {
    SZrFileRange range = {0};
    memset(f, 0, sizeof(*f));
    ZrCore_ExecIr_ModuleInit(&f->module);
    f->state = ZrTests_Runtime_State_Create(ZR_NULL);
    if (f->state == ZR_NULL) return ZR_FALSE;
    f->context = ZrParser_SemanticContext_New(f->state);
    if (f->context == ZR_NULL) return ZR_FALSE;
    for (TZrUInt32 index = 0u; index < 32u; ++index)
        if (ZrParser_Semantic_ReserveTypeId(f->context) == 0u) return ZR_FALSE;
    f->intType = ZrParser_CanonicalType_InternPrimitive(f->context, ZR_VALUE_TYPE_INT64);
    f->boolType = ZrParser_CanonicalType_InternPrimitive(f->context, ZR_VALUE_TYPE_BOOL);
    f->floatType = ZrParser_CanonicalType_InternPrimitive(f->context, ZR_VALUE_TYPE_DOUBLE);
    f->narrowType = ZrParser_CanonicalType_InternPrimitive(f->context, ZR_VALUE_TYPE_INT8);
    if (!f->intType || !f->boolType || !f->floatType || !f->narrowType ||
        f->intType == ZR_VALUE_TYPE_INT64 || f->boolType == ZR_VALUE_TYPE_BOOL)
        return ZR_FALSE;
    ZrParser_SemanticIrFunction_Init(f->state, &f->semantic, FUNCTION_TOKEN, 0xC0410001u);
    f->semanticInitialized = ZR_TRUE;
    if (ZrParser_SemanticIr_AddValue(&f->semantic, f->intType, range) != LEFT ||
        ZrParser_SemanticIr_AddValue(&f->semantic, f->intType, range) != RIGHT ||
        ZrParser_SemanticIr_AddValue(&f->semantic, f->boolType, range) != RESULT)
        return ZR_FALSE;
    return (TZrBool)(emit(f, ZR_SEMANTIC_IR_CONSTANT, f->intType, LEFT, ZR_NULL, 0u, 0u, 0u) &&
            emit(f, ZR_SEMANTIC_IR_CONSTANT, f->intType, RIGHT, ZR_NULL, 0u, 1u, 0u));
}

static TZrBool finish_semantic(SFixture *f, TZrUInt32 predicate) {
    TZrValueId operands[2] = {LEFT, RIGHT}, condition = RESULT;
    SZrParserCfg *cfg = &f->semantic.cfg;
    if (!emit(f, ZR_SEMANTIC_IR_COMPARE, f->boolType, RESULT, operands, 2u, 0u, predicate) ||
        !emit(f, ZR_SEMANTIC_IR_BRANCH, 0u, 0u, &condition, 1u, 0u, 0u) ||
        !emit(f, ZR_SEMANTIC_IR_RETURN, f->intType, 0u, &operands[0], 1u, 0u, 0u) ||
        !emit(f, ZR_SEMANTIC_IR_RETURN, f->intType, 0u, &operands[1], 1u, 0u, 0u))
        return ZR_FALSE;
    if (ZrParser_Cfg_AppendBlock(f->state, cfg, ZR_PARSER_CFG_BLOCK_ENTRY, ZR_NULL) != 0u ||
        ZrParser_Cfg_AppendBlock(f->state, cfg, ZR_PARSER_CFG_BLOCK_EXIT, ZR_NULL) != 1u ||
        ZrParser_Cfg_AppendBlock(f->state, cfg, ZR_PARSER_CFG_BLOCK_EXIT, ZR_NULL) != 2u)
        return ZR_FALSE;
    cfg->entryBlockId = 0u;
    cfg->exitBlockId = 2u;
    return (TZrBool)(ZrParser_Cfg_Connect(cfg, 0u, 1u, ZR_PARSER_CFG_EDGE_TRUE_BRANCH, ZR_NULL) &&
            ZrParser_Cfg_Connect(cfg, 0u, 2u, ZR_PARSER_CFG_EDGE_FALSE_BRANCH, ZR_NULL) &&
            ZrParser_SemanticIr_BindBlockRange(&f->semantic, cfg, 0u, 0u, 4u, ZR_PARSER_CFG_TERMINATOR_BRANCH) &&
            ZrParser_SemanticIr_BindBlockRange(&f->semantic, cfg, 1u, 4u, 1u, ZR_PARSER_CFG_TERMINATOR_RETURN) &&
            ZrParser_SemanticIr_BindBlockRange(&f->semantic, cfg, 2u, 5u, 1u, ZR_PARSER_CFG_TERMINATOR_RETURN));
}

static TZrBool canonical_semantic_valid(const SFixture *f) {
    SZrCompilerState borrowed = {0};
    borrowed.semanticContext = f->context;
    borrowed.preSemanticIr = f->semantic;
    return compiler_semantic_compare_validate(&borrowed);
}

static TZrBool build_module(SFixture *f, SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrBuildInput input = {0};
    input.semanticFunction = &f->semantic;
    input.functionToken = FUNCTION_TOKEN;
    input.signatureHash = 0xC0410001u;
    return ZrParser_ExecIr_BuildModule(&input, &f->module, diagnostic);
}

static TZrBool build_projection(SFixture *f) {
    SZrExecIrDiagnostic diagnostic = {0};
    SZrExecIrConstant constants[2] = {{0}};
    SZrExecIrFunction *function;
    if (!prerequisite("SemIRValidate", ZrParser_SemanticIr_Validate(&f->semantic)) ||
        !prerequisite("CanonicalCompare", canonical_semantic_valid(f)) ||
        !prerequisite("BuildModule", build_module(f, &diagnostic))) return ZR_FALSE;
    function = ZrCore_ExecIr_ModuleFunctionAt(&f->module, 1u);
    if (!prerequisite("CoreVerifyAll", ZrCore_ExecIr_VerifyFunction(
                    function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic))) return ZR_FALSE;
    EXPECT(function->instructions[COMPARE_ID - 1u].matchTypeToken == 0u);
    EXPECT(function->values[LEFT - 1u].typeToken == f->intType);
    EXPECT(function->values[RESULT - 1u].typeToken == f->boolType);
    constants[0].typeToken = constants[1].typeToken = f->intType;
    constants[0].bits = (TZrUInt64)(TZrInt64)-7;
    constants[1].bits = 11u;
    if (!prerequisite("RealConstantPoolIndexes", (TZrBool)(
            function->instructions[LEFT - 1u].layoutId == 0u &&
            function->instructions[RIGHT - 1u].layoutId == 1u))) return ZR_FALSE;
    return prerequisite("BuildProjectionWithConstants", ZrParser_ExecIr_BuildProjectionWithConstants(
            function, constants, 2u, &f->projection, &diagnostic));
}

static void free_fixture(SFixture *f) {
    ZrParser_ExecBcProjection_Free(&f->projection);
    ZrCore_ExecIr_FreeModule(&f->module);
    if (f->semanticInitialized) ZrParser_SemanticIrFunction_Free(f->state, &f->semantic);
    if (f->context != ZR_NULL) ZrParser_SemanticContext_Free(f->context);
    if (f->state != ZR_NULL) ZrTests_Runtime_State_Destroy(f->state);
}

typedef enum EBadSemantic {
    BAD_PREDICATE_ZERO, BAD_PREDICATE_UNKNOWN, BAD_OPERAND_METADATA_ZERO,
    BAD_OPERAND_METADATA_BOOL, BAD_RESULT_METADATA_INT, BAD_RESULT_ZERO,
    BAD_RESULT_RANGE, BAD_OPERAND_ZERO, BAD_OPERAND_RANGE, BAD_OPERAND_SELF,
    BAD_ARITY_ZERO, BAD_ARITY_ONE, BAD_ARITY_THREE, BAD_ALREADY_DEFINED_RESULT,
    BAD_UNRELATED_PREDICATE, BAD_UNRELATED_OPERAND_METADATA, BAD_MATCH,
    BAD_OPERAND_VALUE_TYPE, BAD_RESULT_VALUE_TYPE, BAD_RESULT_DEFINITION,
    BAD_SEMANTIC_COUNT
} EBadSemantic;

static const char *semanticNames[BAD_SEMANTIC_COUNT] = {
    "predicate-eq-outside-source-slice", "predicate-unknown", "operand-metadata-zero",
    "operand-metadata-bool", "result-metadata-int", "result-id-zero",
    "result-id-range", "operand-id-zero", "operand-id-range", "operand-is-result",
    "arity-zero", "arity-one", "arity-three", "already-defined-result",
    "unrelated-predicate", "unrelated-operand-metadata", "shared-match-nonzero",
    "operand-value-type", "result-value-type", "result-definition"
};

static void mutate_spec(SFixture *f, EBadSemantic bad,
                         SZrSemanticIrInstructionSpec *spec, TZrValueId operands[3]) {
    switch (bad) {
        case BAD_PREDICATE_ZERO: spec->comparisonPredicate = 0u; break;
        case BAD_PREDICATE_UNKNOWN: spec->comparisonPredicate = UINT32_MAX; break;
        case BAD_OPERAND_METADATA_ZERO: spec->comparisonOperandTypeId = 0u; break;
        case BAD_OPERAND_METADATA_BOOL: spec->comparisonOperandTypeId = f->boolType; break;
        case BAD_RESULT_METADATA_INT: spec->typeId = f->intType; break;
        case BAD_RESULT_ZERO: spec->resultValueId = 0u; break;
        case BAD_RESULT_RANGE: spec->resultValueId = UINT32_MAX; break;
        case BAD_OPERAND_ZERO: operands[0] = 0u; break;
        case BAD_OPERAND_RANGE: operands[1] = UINT32_MAX; break;
        case BAD_OPERAND_SELF: operands[0] = RESULT; break;
        case BAD_ARITY_ZERO: spec->operandCount = 0u; break;
        case BAD_ARITY_ONE: spec->operandCount = 1u; break;
        case BAD_ARITY_THREE: spec->operandCount = 3u; break;
        case BAD_ALREADY_DEFINED_RESULT: value(f, RESULT)->definitionInstructionId = 1u; break;
        case BAD_UNRELATED_PREDICATE:
            spec->opcode = ZR_SEMANTIC_IR_CONSTANT; spec->comparisonOperandTypeId = 0u; break;
        case BAD_UNRELATED_OPERAND_METADATA:
            spec->opcode = ZR_SEMANTIC_IR_CONSTANT; spec->comparisonPredicate = 0u; break;
        case BAD_MATCH: spec->matchTypeId = f->intType; break;
        case BAD_OPERAND_VALUE_TYPE: value(f, RIGHT)->typeId = f->boolType; break;
        case BAD_RESULT_VALUE_TYPE: value(f, RESULT)->typeId = f->intType; break;
        case BAD_RESULT_DEFINITION: value(f, RESULT)->definitionInstructionId = UINT32_MAX; break;
        default: break;
    }
}

static void emission_case(EBadSemantic bad) {
    SFixture f;
    SZrSemanticIrInstructionSpec spec = {0};
    TZrValueId operands[3] = {LEFT, RIGHT, LEFT};
    SBytes before[5] = {{0}};
    char name[96];
    snprintf(name, sizeof(name), "emit/%s", semanticNames[bad]);
    begin_case(name);
    if (!prerequisite("RealConstructors", initialize_fixture(&f))) goto done;
    spec.opcode = ZR_SEMANTIC_IR_COMPARE;
    spec.typeId = f.boolType;
    spec.resultValueId = RESULT;
    spec.operands = operands;
    spec.operandCount = 2u;
    spec.comparisonPredicate = ZR_EXEC_IR_COMPARE_KIND_LESS;
    spec.comparisonOperandTypeId = f.intType;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    mutate_spec(&f, bad, &spec, operands);
    if (!prerequisite("Snapshot", (TZrBool)(
            save_bytes(&before[0], &f.semantic, sizeof(f.semantic)) &&
            save_bytes(&before[1], f.semantic.instructions.head, f.semantic.instructions.length * sizeof(SZrSemanticIrInstruction)) &&
            save_bytes(&before[2], f.semantic.values.head, f.semantic.values.length * sizeof(SZrSemanticIrValue)) &&
            save_bytes(&before[3], f.semantic.valueOperands.head, f.semantic.valueOperands.length * sizeof(TZrValueId)) &&
            save_bytes(&before[4], f.semantic.sourceMap.head, f.semantic.sourceMap.length * sizeof(SZrSemanticIrSourceMapEntry))))) {
        check_bytes(before, 5u); goto done;
    }
    EXPECT(ZrParser_SemanticIr_Emit(&f.semantic, &spec) == ZR_SEMANTIC_INSTRUCTION_ID_INVALID);
    check_bytes(before, 5u);
done:
    free_fixture(&f);
    end_case();
}

static void stored_case(EBadSemantic bad) {
    SFixture f;
    SZrSemanticIrInstruction *in;
    SZrExecIrDiagnostic diagnostic = {0};
    TZrValueId operands[3] = {LEFT, RIGHT, LEFT};
    SBytes before[6] = {{0}};
    char name[96];
    TZrUInt32 rejectedId = COMPARE_ID;
    snprintf(name, sizeof(name), "stored/%s", semanticNames[bad]);
    begin_case(name);
    if (!prerequisite("RealConstructors", initialize_fixture(&f)) ||
        !prerequisite("FinishCFG", finish_semantic(&f, ZR_EXEC_IR_COMPARE_KIND_LESS)) ||
        !prerequisite("BaselineSemIR", ZrParser_SemanticIr_Validate(&f.semantic))) goto done;
    in = instruction(&f, COMPARE_ID);
    if (bad == BAD_UNRELATED_PREDICATE || bad == BAD_UNRELATED_OPERAND_METADATA) {
        in = instruction(&f, LEFT); rejectedId = LEFT;
        if (bad == BAD_UNRELATED_PREDICATE) in->comparisonPredicate = ZR_EXEC_IR_COMPARE_KIND_LESS;
        else in->comparisonOperandTypeId = f.intType;
    } else {
        SZrSemanticIrInstructionSpec spec = {0};
        spec.opcode = in->opcode; spec.typeId = in->typeId;
        spec.resultValueId = in->resultValueId; spec.operandCount = in->operandCount;
        spec.comparisonPredicate = in->comparisonPredicate;
        spec.comparisonOperandTypeId = in->comparisonOperandTypeId;
        mutate_spec(&f, bad, &spec, operands);
        in->typeId = spec.typeId; in->resultValueId = spec.resultValueId;
        in->operandCount = (TZrUInt32)spec.operandCount;
        in->comparisonPredicate = spec.comparisonPredicate;
        in->comparisonOperandTypeId = spec.comparisonOperandTypeId;
        in->matchTypeId = spec.matchTypeId;
        {
            TZrValueId *pool = (TZrValueId *)ZrCore_Array_Get(
                    &f.semantic.valueOperands, in->operandStart);
            pool[0] = operands[0]; pool[1] = operands[1];
        }
    }
    if (!prerequisite("Snapshot", (TZrBool)(
            save_bytes(&before[0], &f.semantic, sizeof(f.semantic)) &&
            save_bytes(&before[1], f.semantic.instructions.head, f.semantic.instructions.length * sizeof(SZrSemanticIrInstruction)) &&
            save_bytes(&before[2], f.semantic.values.head, f.semantic.values.length * sizeof(SZrSemanticIrValue)) &&
            save_bytes(&before[3], f.semantic.valueOperands.head, f.semantic.valueOperands.length * sizeof(TZrValueId)) &&
            save_bytes(&before[4], f.semantic.sourceMap.head, f.semantic.sourceMap.length * sizeof(SZrSemanticIrSourceMapEntry)) &&
            save_bytes(&before[5], &f.module, sizeof(f.module))))) {
        check_bytes(before, 6u); goto done;
    }
    EXPECT(!ZrParser_SemanticIr_Validate(&f.semantic));
    EXPECT(!build_module(&f, &diagnostic));
    check_bytes(before, 6u);
    EXPECT(f.module.functionCount == 0u);
    EXPECT(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE);
    EXPECT(diagnostic.functionToken == FUNCTION_TOKEN);
    EXPECT(diagnostic.instructionId == rejectedId);
    /* Shared match metadata is rejected in generic preflight before the
     * comparison-specific source diagnostic is constructed. */
    EXPECT(diagnostic.sourceId == (bad == BAD_MATCH ? 0u : rejectedId));
    printf("EXPECTED_DIAGNOSTIC %s BuildModule code=%u block=%u instruction=%u source=%u\n",
            caseName, (unsigned)diagnostic.code, (unsigned)diagnostic.blockId,
            (unsigned)diagnostic.instructionId, (unsigned)diagnostic.sourceId);
done:
    free_fixture(&f);
    end_case();
}

static void opaque_primitive_case(TZrBool wrongOperand) {
    SFixture f;
    begin_case(wrongOperand ? "canonical-semantic/consistent-double-operands" : "canonical-semantic/consistent-double-result");
    if (!prerequisite("RealConstructors", initialize_fixture(&f)) ||
        !prerequisite("FinishCFG", finish_semantic(&f, ZR_EXEC_IR_COMPARE_KIND_LESS)) ||
        !prerequisite("BaselineCanonical", canonical_semantic_valid(&f))) goto done;
    if (wrongOperand) {
        value(&f, LEFT)->typeId = value(&f, RIGHT)->typeId = f.floatType;
        instruction(&f, LEFT)->typeId = instruction(&f, RIGHT)->typeId = f.floatType;
        instruction(&f, COMPARE_ID)->comparisonOperandTypeId = f.floatType;
        instruction(&f, 5u)->typeId = instruction(&f, 6u)->typeId = f.floatType;
    } else {
        value(&f, RESULT)->typeId = f.floatType;
        instruction(&f, COMPARE_ID)->typeId = f.floatType;
    }
    /* Structural equality of opaque IDs remains valid. Primitive meaning is
     * rejected by the actual canonical context consumer, not guessed here. */
    EXPECT(ZrParser_SemanticIr_Validate(&f.semantic));
    EXPECT(!canonical_semantic_valid(&f));
    printf("EXPECTED_REJECTION %s compiler_semantic_compare_validate\n", caseName);
done:
    free_fixture(&f);
    end_case();
}

typedef enum EVmCase {
    VM_ZERO_LT_TRUE, VM_ZERO_LT_FALSE, VM_ZERO_GT_TRUE, VM_ZERO_GT_FALSE,
    VM_EXPLICIT_INT_MATCH, VM_MATCH_BOOL, VM_MATCH_NARROW, VM_MATCH_UNKNOWN,
    VM_OPERAND_BOOL, VM_OPERAND_FLOAT, VM_RESULT_INT, VM_BRANCH_INT,
    VM_PREDICATE_UNKNOWN, VM_OPERAND_ZERO, VM_RESULT_ZERO, VM_ARITY_ONE,
    VM_RESULT_ARITY_ZERO, VM_OPERAND_RANGE, VM_RESULT_RANGE,
    VM_VALUE_SLOT_RANGE, VM_SLOT_VALUE_ID, VM_CASE_COUNT
} EVmCase;

static const char *vmNames[VM_CASE_COUNT] = {
    "zero-lt-true", "zero-lt-false", "zero-gt-true", "zero-gt-false",
    "explicit-canonical-int-match", "match-bool", "match-narrow", "match-unknown",
    "operand-bool", "operand-double", "result-int", "branch-int",
    "predicate-unknown", "operand-zero", "result-zero", "operand-arity-one",
    "result-arity-zero", "operand-range", "result-range", "value-slot-range", "slot-value-id"
};

static void vm_case(EVmCase test) {
    SFixture f;
    SZrExecBcVmEmission output = {0};
    SZrExecIrDiagnostic diagnostic = {0};
    SZrExecBcInstruction *compare, *branch;
    SBytes before[28] = {{0}};
    TZrUInt32 predicate = (test == VM_ZERO_GT_TRUE || test == VM_ZERO_GT_FALSE)
            ? ZR_EXEC_IR_COMPARE_KIND_GREATER : ZR_EXEC_IR_COMPARE_KIND_LESS;
    TZrBool positive = (TZrBool)(test <= VM_EXPLICIT_INT_MATCH), rooted = ZR_FALSE;
    TZrInt64 result = 0;
    char name[96];
    snprintf(name, sizeof(name), "vm/%s", vmNames[test]);
    begin_case(name);
    if (!prerequisite("RealConstructors", initialize_fixture(&f)) ||
        !prerequisite("FinishCFG", finish_semantic(&f, predicate)) ||
        !build_projection(&f)) goto done;
    compare = &f.projection.instructions[COMPARE_ID - 1u];
    branch = &f.projection.instructions[BRANCH_ID - 1u];
    if (!prerequisite("RealProjectionShape", (TZrBool)(
            f.projection.instructionCount == FIXTURE_INSTRUCTIONS &&
            compare->opcode == ZR_EXEC_IR_OPCODE_COMPARE &&
            branch->opcode == ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH &&
            f.projection.constantCount == 2u &&
            f.projection.constants != ZR_NULL))) goto done;
    EXPECT(compare->matchTypeToken == 0u);
    switch (test) {
        case VM_ZERO_LT_FALSE: case VM_ZERO_GT_TRUE:
            f.projection.constants[0].bits = 11u;
            f.projection.constants[1].bits = (TZrUInt64)(TZrInt64)-7; break;
        case VM_EXPLICIT_INT_MATCH: compare->matchTypeToken = f.intType; break;
        case VM_MATCH_BOOL: compare->matchTypeToken = f.boolType; break;
        case VM_MATCH_NARROW: compare->matchTypeToken = f.narrowType; break;
        case VM_MATCH_UNKNOWN: compare->matchTypeToken = UINT32_MAX; break;
        case VM_OPERAND_BOOL: case VM_OPERAND_FLOAT:
            f.projection.slotValues[f.projection.valueSlots[LEFT - 1u]].typeToken =
                    test == VM_OPERAND_BOOL ? f.boolType : f.floatType; break;
        case VM_RESULT_INT:
            f.projection.slotValues[f.projection.valueSlots[RESULT - 1u]].typeToken = f.intType; break;
        case VM_BRANCH_INT: f.projection.operands[branch->operands.start] = LEFT; break;
        case VM_PREDICATE_UNKNOWN: compare->typeToken = UINT32_MAX; break;
        case VM_OPERAND_ZERO: f.projection.operands[compare->operands.start] = 0u; break;
        case VM_RESULT_ZERO: f.projection.results[compare->results.start] = 0u; break;
        case VM_ARITY_ONE: compare->operands.count = 1u; break;
        case VM_RESULT_ARITY_ZERO: compare->results.count = 0u; break;
        case VM_OPERAND_RANGE: compare->operands.start = UINT32_MAX; break;
        case VM_RESULT_RANGE: compare->results.start = UINT32_MAX; break;
        case VM_VALUE_SLOT_RANGE: f.projection.valueSlots[LEFT - 1u] = f.projection.physicalSlotCount; break;
        case VM_SLOT_VALUE_ID: f.projection.slotValues[f.projection.valueSlots[LEFT - 1u]].id = RIGHT; break;
        default: break;
    }
    if (!prerequisite("Snapshot", snapshot_projection(&f, before))) {
        check_bytes(before, 28u); goto done;
    }
    {
        TZrBool succeeded = ZrParser_ExecBcProjection_MaterializeVmFunctionWithCanonicalTypes(
                f.state, &f.projection, f.context, &output, &diagnostic);
        check_bytes(before, 28u);
        if (positive) {
            if (!prerequisite("MaterializeCanonicalVm", succeeded)) goto done;
            EXPECT(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_NONE);
            rooted = ZrCore_GarbageCollector_IgnoreObject(f.state, ZR_CAST_RAW_OBJECT_AS_SUPER(output.function));
            if (!prerequisite("RootFunction", rooted)) goto done;
            EXPECT(ZrTests_Runtime_Function_ExecuteExpectInt64(f.state, output.function, &result));
            /* The true arm returns LEFT; the false arm returns RIGHT. Swapping
             * the operands therefore keeps LT at -7 and GT at 11. */
            EXPECT(result == ((test == VM_ZERO_GT_TRUE || test == VM_ZERO_GT_FALSE)
                              ? 11 : -7));
            printf("VM_RESULT %s integer=%lld\n", caseName, (long long)result);
        } else {
            EXPECT(!succeeded);
            EXPECT(output.function == ZR_NULL && output.pcMap == ZR_NULL && output.pcMapCount == 0u);
            EXPECT(diagnostic.code == (test == VM_BRANCH_INT
                    ? ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED : ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE));
            EXPECT(diagnostic.functionToken == FUNCTION_TOKEN);
            EXPECT(diagnostic.instructionId == (test == VM_BRANCH_INT ? BRANCH_ID : COMPARE_ID));
            EXPECT(diagnostic.sourceId == (test == VM_BRANCH_INT ? BRANCH_ID : COMPARE_ID));
            EXPECT(diagnostic.blockId == (test == VM_BRANCH_INT ? 1u : 0u));
            if (test == VM_BRANCH_INT) {
                EXPECT(diagnostic.expectedVersion == ZR_VALUE_TYPE_BOOL);
                EXPECT(diagnostic.actualVersion == ZR_VALUE_TYPE_INT64);
            } else {
                EXPECT(diagnostic.expectedVersion == ZR_VALUE_TYPE_BOOL);
                /* Invalid match metadata is tested after resolving the BOOL
                 * result. All other mutations stop before that resolution. */
                EXPECT(diagnostic.actualVersion == ((test == VM_MATCH_BOOL ||
                        test == VM_MATCH_NARROW || test == VM_MATCH_UNKNOWN)
                        ? f.boolType : 0u));
            }
            printf("EXPECTED_DIAGNOSTIC %s CanonicalVm code=%u block=%u instruction=%u source=%u expected=%u actual=%u\n",
                    caseName, (unsigned)diagnostic.code, (unsigned)diagnostic.blockId,
                    (unsigned)diagnostic.instructionId, (unsigned)diagnostic.sourceId,
                    (unsigned)diagnostic.expectedVersion, (unsigned)diagnostic.actualVersion);
        }
    }
done:
    if (rooted) (void)ZrCore_GarbageCollector_UnignoreObject(f.state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(output.function));
    ZrParser_ExecBcVmEmission_Free(f.state, &output);
    free_fixture(&f);
    end_case();
}

/* Unity's runner references these even when this file supplies its own main. */
void setUp(void) {}

void tearDown(void) {}

int main(void) {
    (void)setvbuf(stdout, ZR_NULL, _IONBF, 0u);
    for (EBadSemantic bad = BAD_PREDICATE_ZERO; bad < BAD_SEMANTIC_COUNT; ++bad) {
        emission_case(bad);
        stored_case(bad);
    }
    opaque_primitive_case(ZR_TRUE);
    opaque_primitive_case(ZR_FALSE);
    for (EVmCase test = VM_ZERO_LT_TRUE; test < VM_CASE_COUNT; ++test) vm_case(test);
    printf("COMPARE metadata guards: %u cases, %u failures, %u precondition failures, %u preconditions\n",
            cases, failures, preconditionFailures, preconditions);
    return failures || preconditionFailures ? EXIT_FAILURE : EXIT_SUCCESS;
}

/**
 * @anchor zr_compare_metadata_fixture_constants
 *
 * ## Summary
 * 定义本TU固定fixture身份、一基value/instruction定位与六条指令计数，供构造、突变和诊断预期共同使用。
 *
 * ## Fields and invariants
 * FUNCTION_TOKEN为fixture metadata token；LEFT/RIGHT/RESULT为1/2/3；COMPARE_ID/BRANCH_ID为3/4；FIXTURE_INSTRUCTIONS为6。
 *
 * ## Constraints and lifecycle
 * 这些数字只说明这个手工fixture的稳定对应；FUNCTION_TOKEN不是可调用来源证明，COMPARE/BRANCH ID也不是任意程序的通用编号。不存在独立内存所有权、callback或运行期可变状态。
 *
 * ## References
 * @see tests/parser/test_ssa_compare_metadata_guards.c:15
 * @see tests/parser/test_ssa_compare_metadata_guards.c:16
 * @see tests/parser/test_ssa_compare_metadata_guards.c:169
 * @see tests/parser/test_ssa_compare_metadata_guards.c:171
 * @see tests/parser/test_ssa_compare_metadata_guards.c:448
 * @see tests/parser/test_ssa_compare_metadata_guards.c:506
 */

/**
 * @typedef SFixture
 *
 * ## Summary
 * 集中持有一次comparison guard场景的Runtime State、SemanticContext、semantic函数、Core module与ExecBC projection，另保存canonical primitive ID和semantic初始化标记。
 *
 * ## Fields and invariants
 * state/context：真实创建并由free_fixture销毁的依赖owner；semantic/module/projection：嵌入的独立记录，各自拥有其API建立的池。intType/boolType/floatType/narrowType：借context解释的canonical ID。semanticInitialized：决定是否调用SemanticIrFunction_Free，不代替其他成员nonnull/合法性。
 *
 * ## Constraints and lifecycle
 * 它是一次构造/一次释放的owner聚合，不能浅拷贝后双重Free；initialize失败允许保留前缀。semantic/context依赖State生命周期，projection/module分别按API释放；vm output及忽略根在此类型之外由vm_case先收尾。各type ID仅在对应context存活时解释，不能据ID数值猜primitive或跨context复用。本TU的case和计数器只按main顺序使用；没有同步、并发调用或跨线程借用契约。
 *
 * ## References
 * @see tests/parser/test_ssa_compare_metadata_guards.c:18
 * @see tests/parser/test_ssa_compare_metadata_guards.c:19
 * @see tests/parser/test_ssa_compare_metadata_guards.c:25
 * @see tests/parser/test_ssa_compare_metadata_guards.c:154
 * @see tests/parser/test_ssa_compare_metadata_guards.c:169
 * @see tests/parser/test_ssa_compare_metadata_guards.c:238
 * @see tests/parser/test_ssa_compare_metadata_guards.c:527
 */

/**
 * @typedef SBytes
 *
 * ## Summary
 * 描述一个借用输入字节区间及其独立malloc副本，用来证明某次拒绝调用没有修改选定测试输入。
 *
 * ## Fields and invariants
 * input为稳定借用地址；before为需free的测试证据副本或NULL；size为要比较的明确字节数。数组未填槽通过初始全零表示空记录。
 *
 * ## Constraints and lifecycle
 * input不拥有原对象；before非NULL时由本记录的单次消费路径free。size非零时原输入必须保持可读，size0允许NULL。save_bytes失败后字段可能已部分填写；check_bytes消费后不清指针，记录不可再次消费，也不作为替代runtime输入或源码文件保存。本TU的case和计数器只按main顺序使用；没有同步、并发调用或跨线程借用契约。
 *
 * ## References
 * @see tests/parser/test_ssa_compare_metadata_guards.c:28
 * @see tests/parser/test_ssa_compare_metadata_guards.c:60
 * @see tests/parser/test_ssa_compare_metadata_guards.c:64
 * @see tests/parser/test_ssa_compare_metadata_guards.c:69
 * @see tests/parser/test_ssa_compare_metadata_guards.c:73
 */

/**
 * @var cases
 *
 * ## Summary
 * 累计已begin_case的场景数。
 *
 * ## Fields and invariants
 * 单一串行测试进程中的TU静态状态；不是公共API、线程同步对象或生产缓存。
 *
 * ## Constraints and lifecycle
 * 静态存储初值0，只在begin_case增加，main末尾汇总。既包括前提失败的场景，也不是已执行全部下层断言的数量。本TU的case和计数器只按main顺序使用；没有同步、并发调用或跨线程借用契约。
 *
 * ## References
 * @see tests/parser/test_ssa_compare_metadata_guards.c:34
 * @see tests/parser/test_ssa_compare_metadata_guards.c:52
 * @see tests/parser/test_ssa_compare_metadata_guards.c:548
 */

/**
 * @var failures
 *
 * ## Summary
 * 累计end_case时caseOk为假的场景数。
 *
 * ## Fields and invariants
 * 单一串行测试进程中的TU静态状态；不是公共API、线程同步对象或生产缓存。
 *
 * ## Constraints and lifecycle
 * 静态初值0，end_case对每场景至多加一次；不等于失败EXPECT条数。main读取其非零状态决定失败exit。本TU的case和计数器只按main顺序使用；没有同步、并发调用或跨线程借用契约。
 *
 * ## References
 * @see tests/parser/test_ssa_compare_metadata_guards.c:34
 * @see tests/parser/test_ssa_compare_metadata_guards.c:56
 * @see tests/parser/test_ssa_compare_metadata_guards.c:548
 * @see tests/parser/test_ssa_compare_metadata_guards.c:549
 */

/**
 * @var preconditions
 *
 * ## Summary
 * 累计实际调用prerequisite的次数。
 *
 * ## Fields and invariants
 * 单一串行测试进程中的TU静态状态；不是公共API、线程同步对象或生产缓存。
 *
 * ## Constraints and lifecycle
 * 静态初值0，在prerequisite先自增。短路和goto使后续前提不调用，因此不能按case数量推固定前提次数。本TU的case和计数器只按main顺序使用；没有同步、并发调用或跨线程借用契约。
 *
 * ## References
 * @see tests/parser/test_ssa_compare_metadata_guards.c:34
 * @see tests/parser/test_ssa_compare_metadata_guards.c:43
 * @see tests/parser/test_ssa_compare_metadata_guards.c:548
 */

/**
 * @var preconditionFailures
 *
 * ## Summary
 * 累计prerequisite收到假值的次数。
 *
 * ## Fields and invariants
 * 单一串行测试进程中的TU静态状态；不是公共API、线程同步对象或生产缓存。
 *
 * ## Constraints and lifecycle
 * 静态初值0，假前提同时令caseOk为假；main另读该计数决定失败，不依赖case是否继续目标断言。本TU的case和计数器只按main顺序使用；没有同步、并发调用或跨线程借用契约。
 *
 * ## References
 * @see tests/parser/test_ssa_compare_metadata_guards.c:34
 * @see tests/parser/test_ssa_compare_metadata_guards.c:45
 * @see tests/parser/test_ssa_compare_metadata_guards.c:548
 * @see tests/parser/test_ssa_compare_metadata_guards.c:549
 */

/**
 * @var caseName
 *
 * ## Summary
 * 借用当前case名称供EXPECT/前提/结束日志定位。
 *
 * ## Fields and invariants
 * 单一串行测试进程中的TU静态状态；不是公共API、线程同步对象或生产缓存。
 *
 * ## Constraints and lifecycle
 * begin_case不复制字符串；各参数化case用栈内name并在其作用域内end_case，opaque场景用字面量。返回后旧指针不再读取，下一场景覆盖它；无拥有或free责任。本TU的case和计数器只按main顺序使用；没有同步、并发调用或跨线程借用契约。
 *
 * ## References
 * @see tests/parser/test_ssa_compare_metadata_guards.c:35
 * @see tests/parser/test_ssa_compare_metadata_guards.c:39
 * @see tests/parser/test_ssa_compare_metadata_guards.c:44
 * @see tests/parser/test_ssa_compare_metadata_guards.c:50
 * @see tests/parser/test_ssa_compare_metadata_guards.c:57
 * @see tests/parser/test_ssa_compare_metadata_guards.c:300
 * @see tests/parser/test_ssa_compare_metadata_guards.c:335
 * @see tests/parser/test_ssa_compare_metadata_guards.c:443
 */

/**
 * @var caseOk
 *
 * ## Summary
 * 保存当前case是否仍满足全部已执行前提和EXPECT。
 *
 * ## Fields and invariants
 * 单一串行测试进程中的TU静态状态；不是公共API、线程同步对象或生产缓存。
 *
 * ## Constraints and lifecycle
 * begin_case置TRUE，任一EXPECT或假prerequisite只置FALSE，end_case读取；当前case内不再复位，因此可汇总多个非终止检查。本TU的case和计数器只按main顺序使用；没有同步、并发调用或跨线程借用契约。
 *
 * ## References
 * @see tests/parser/test_ssa_compare_metadata_guards.c:36
 * @see tests/parser/test_ssa_compare_metadata_guards.c:38
 * @see tests/parser/test_ssa_compare_metadata_guards.c:45
 * @see tests/parser/test_ssa_compare_metadata_guards.c:51
 * @see tests/parser/test_ssa_compare_metadata_guards.c:56
 */

/**
 * @def EXPECT(condition)
 *
 * ## Summary
 * 把单个条件的失败记到caseOk并打印caseName、源行和表达式，继续执行当前case。
 *
 * ## Parameters
 * @param[in] condition 只求值一次的布尔表达式；假值令caseOk为FALSE。
 *
 * ## Returns
 * 宏正常执行后继续调用方控制流，无C返回值；main最终根据累计状态返回失败。
 *
 * ## Constraints and lifecycle
 * condition实际求值一次，字符串化只打印原表达式；不会exit、return或longjmp，也不会自动释放任何资源。须先begin_case且caseName有效。控制流和清理由每个caller显式完成，不能把后续检查当作已由前一个EXPECT失败阻止。本TU的case和计数器只按main顺序使用；没有同步、并发调用或跨线程借用契约。
 *
 * ## References
 * @see tests/parser/test_ssa_compare_metadata_guards.c:38
 * @see tests/parser/test_ssa_compare_metadata_guards.c:39
 * @see tests/parser/test_ssa_compare_metadata_guards.c:40
 * @see tests/parser/test_ssa_compare_metadata_guards.c:71
 * @see tests/parser/test_ssa_compare_metadata_guards.c:320
 * @see tests/parser/test_ssa_compare_metadata_guards.c:494
 */

/**
 * @fn TZrBool prerequisite(const char *label, TZrBool passed)
 *
 * ## Summary
 * 记录一个已求值的构造/验证前提及其标签，累计preconditions；失败时同时增加preconditionFailures并将caseOk置假，返回原passed供调用方选择goto清理。
 *
 * ## Parameters
 * @param[in] label 本次前提检查的借用消息标签。
 * @param[in] passed 调用前已经求值的前提结果；不重新执行被检查操作。
 *
 * ## Returns
 * @return 原样返回passed；失败副作用是全局计数和caseOk更新，不是异常/资源返回。
 *
 * ## Constraints and lifecycle
 * 须在begin_case之后调用，caseName和label在本次printf期间有效。它不退出case、不清理fixture，也不会因为passed为假而中止调用方后续语句；当前caller使用短路条件或返回值显式分派。本TU的case和计数器只按main顺序使用；没有同步、并发调用或跨线程借用契约。
 *
 * ## References
 * @see tests/parser/test_ssa_compare_metadata_guards.c:42
 * @see tests/parser/test_ssa_compare_metadata_guards.c:43
 * @see tests/parser/test_ssa_compare_metadata_guards.c:44
 * @see tests/parser/test_ssa_compare_metadata_guards.c:45
 * @see tests/parser/test_ssa_compare_metadata_guards.c:46
 * @see tests/parser/test_ssa_compare_metadata_guards.c:219
 * @see tests/parser/test_ssa_compare_metadata_guards.c:302
 * @see tests/parser/test_ssa_compare_metadata_guards.c:337
 * @see tests/parser/test_ssa_compare_metadata_guards.c:445
 */

/**
 * @fn void begin_case(const char *name)
 *
 * ## Summary
 * 开始一次参数化场景：借用名称、重置caseOk为真并增加cases。
 *
 * ## Parameters
 * @param[in] name 当前场景的有效NUL结尾名称；仅借用，不复制或释放。
 *
 * ## Returns
 * 无C返回值；正常完成本地操作后返回。EXPECT只记录当前case失败并继续，最终由main汇总失败计数，不触发Unity longjmp或自动进程退出。
 *
 * ## Constraints and lifecycle
 * name须在该场景全部日志、EXPECT与end_case结束前保持有效。emission/stored/vm传入各自栈内name，函数返回后全局caseName可能仍存旧地址；当前程序在下次begin_case前不再读取它。不能嵌套/并发开始case，也不重置累计失败或前提计数。本TU的case和计数器只按main顺序使用；没有同步、并发调用或跨线程借用契约。
 *
 * ## References
 * @see tests/parser/test_ssa_compare_metadata_guards.c:49
 * @see tests/parser/test_ssa_compare_metadata_guards.c:50
 * @see tests/parser/test_ssa_compare_metadata_guards.c:51
 * @see tests/parser/test_ssa_compare_metadata_guards.c:52
 * @see tests/parser/test_ssa_compare_metadata_guards.c:301
 * @see tests/parser/test_ssa_compare_metadata_guards.c:336
 * @see tests/parser/test_ssa_compare_metadata_guards.c:392
 * @see tests/parser/test_ssa_compare_metadata_guards.c:444
 */

/**
 * @fn void end_case(void)
 *
 * ## Summary
 * 根据当前caseOk累计至多一次case失败，并输出该场景PASS/FAIL。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无C返回值；正常完成本地操作后返回。EXPECT只记录当前case失败并继续，最终由main汇总失败计数，不触发Unity longjmp或自动进程退出。
 *
 * ## Constraints and lifecycle
 * 调用者必须已经begin_case，并保持caseName借用有效。该函数不清理资源、不重置caseOk和名称、不以返回码报告失败；各case在各自done清理之后调用。本TU的case和计数器只按main顺序使用；没有同步、并发调用或跨线程借用契约。
 *
 * ## References
 * @see tests/parser/test_ssa_compare_metadata_guards.c:55
 * @see tests/parser/test_ssa_compare_metadata_guards.c:56
 * @see tests/parser/test_ssa_compare_metadata_guards.c:57
 * @see tests/parser/test_ssa_compare_metadata_guards.c:324
 * @see tests/parser/test_ssa_compare_metadata_guards.c:387
 * @see tests/parser/test_ssa_compare_metadata_guards.c:412
 * @see tests/parser/test_ssa_compare_metadata_guards.c:530
 */

/**
 * @fn TZrBool save_bytes(SBytes *saved, const void *input, size_t size)
 *
 * ## Summary
 * 把一个明确字节区间复制到单个SBytes测试证据记录，供后续比较被测调用前后输入是否变化。
 *
 * ## Parameters
 * @param[out] saved 输出input/size/owned before记录；须非NULL且不持有未释放旧副本。
 * @param[in] input 待比较输入的借用起点；非零size时稳定可读，保持至check_bytes完成。
 * @param[in] size 要复制的字节数；调用方负责真实对象边界及此前乘法不溢出。
 *
 * ## Returns
 * @return 零字节或有效输入复制完成返回TRUE；非零字节的input为空或malloc失败返回FALSE，输出仍可能含需free的before，调用方负责处理。
 *
 * ## Constraints and lifecycle
 * saved必须是可写的空记录或无待回收旧before；函数先写input/size，再分配before，失败不回滚这些字段或自行free已分配副本。size非零时input须覆盖可读字节，NULL仅被布尔拒绝，不能验证任意地址。若NULL输入导致FALSE且malloc成功，before仍须由调用者释放，不能把无效input盲目送入check_bytes；当前场景使用真实可读构造记录/池。size为零不要求input非NULL，也不分配。字节副本只是进程内测试数据，未保存源码副本。本TU的case和计数器只按main顺序使用；没有同步、并发调用或跨线程借用契约。
 *
 * ## References
 * @see tests/parser/test_ssa_compare_metadata_guards.c:60
 * @see tests/parser/test_ssa_compare_metadata_guards.c:61
 * @see tests/parser/test_ssa_compare_metadata_guards.c:63
 * @see tests/parser/test_ssa_compare_metadata_guards.c:64
 * @see tests/parser/test_ssa_compare_metadata_guards.c:65
 * @see tests/parser/test_ssa_compare_metadata_guards.c:313
 * @see tests/parser/test_ssa_compare_metadata_guards.c:82
 */

/**
 * @fn void check_bytes(SBytes *saved, size_t count)
 *
 * ## Summary
 * 逐项比较SBytes中记录的输入与before副本，并释放每个before；缺失非空副本或字节不等只通过EXPECT记录失败，仍继续后续记录清理。
 *
 * ## Parameters
 * @param[in,out] saved 输入证据记录数组；消费其before所有权并释放，记录字段本身不清零。
 * @param[in] count 数组中须检查和消费的记录个数；不得超出实际数组。
 *
 * ## Returns
 * 无C返回值；正常完成本地操作后返回。EXPECT只记录当前case失败并继续，最终由main汇总失败计数，不触发Unity longjmp或自动进程退出。
 *
 * ## Constraints and lifecycle
 * saved指向至少count个已初始化记录；非零size且before非NULL时input仍须可读且未被释放/重分配。零初始化的未填记录size0会跳过比较，free(NULL)可用；该函数不把before字段置NULL，因而每组记录只能消费一次，重复调用可能重复释放。只证明列出的原始字节区间，不读取指针可达的其他对象。本TU的case和计数器只按main顺序使用；没有同步、并发调用或跨线程借用契约。
 *
 * ## References
 * @see tests/parser/test_ssa_compare_metadata_guards.c:69
 * @see tests/parser/test_ssa_compare_metadata_guards.c:70
 * @see tests/parser/test_ssa_compare_metadata_guards.c:71
 * @see tests/parser/test_ssa_compare_metadata_guards.c:72
 * @see tests/parser/test_ssa_compare_metadata_guards.c:73
 * @see tests/parser/test_ssa_compare_metadata_guards.c:318
 * @see tests/parser/test_ssa_compare_metadata_guards.c:374
 * @see tests/parser/test_ssa_compare_metadata_guards.c:488
 */

/**
 * @fn TZrBool snapshot_projection(const SFixture *f, SBytes saved[28])
 *
 * ## Summary
 * 按固定28个证据区间依次保存projection记录、代码中列明的25个直接side-pool区间，以及canonicalTypes记录和head内容。
 *
 * ## Parameters
 * @param[in] f 只读fixture借用，包含有效context和已构造projection。
 * @param[out] saved 输出至少28个原先零初始化且无旧before所有权的证据槽。
 *
 * ## Returns
 * @return 全部28次save_bytes成功返回TRUE；任一次失败立即返回FALSE，caller仍须消费已经产生的副本。
 *
 * ## Constraints and lifecycle
 * f/context/projection及相应逻辑count均须来自本case已成功构造的稳定可读状态；数组长度乘法和saved28容量由固定fixture保证，本函数不做通用存储认证。前缀保存失败会立即FALSE，已填before和未填零记录由caller一次check_bytes消费；不在此回滚或free。projection中的嵌入GC记录随整体记录保存，但未额外深拷贝其可达GC嵌套数组；当前finite fixture为空的区间size0不构成非空分支证明。本TU的case和计数器只按main顺序使用；没有同步、并发调用或跨线程借用契约。
 *
 * ## References
 * @see tests/parser/test_ssa_compare_metadata_guards.c:79
 * @see tests/parser/test_ssa_compare_metadata_guards.c:82
 * @see tests/parser/test_ssa_compare_metadata_guards.c:85
 * @see tests/parser/test_ssa_compare_metadata_guards.c:110
 * @see tests/parser/test_ssa_compare_metadata_guards.c:111
 * @see tests/parser/test_ssa_compare_metadata_guards.c:112
 * @see tests/parser/test_ssa_compare_metadata_guards.c:116
 * @see tests/parser/test_ssa_compare_metadata_guards.c:482
 * @see tests/parser/test_ssa_compare_metadata_guards.c:488
 * @see zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projections.h:119
 */

/**
 * @def SAVE_RAW(pointer, bytes)
 *
 * ## Summary
 * 在snapshot_projection的当前next槽保存一个明确字节区间并递增next；save_bytes失败时立即从包围的snapshot函数返回FALSE。
 *
 * ## Parameters
 * @param[in] pointer 传给save_bytes的稳定输入地址表达式。
 * @param[in] bytes 传给save_bytes的字节长度表达式；由caller保证对象边界和乘法不溢出。
 *
 * ## Returns
 * 无独立C返回值；成功继续宏后的语句，失败执行return ZR_FALSE结束snapshot_projection，保留已保存前缀。
 *
 * ## Constraints and lifecycle
 * 这是词法作用域依赖宏，要求调用位置存在saved和next且至少28槽已零初始化；不检查next容量，不适合任意函数复用。pointer与bytes由save_bytes各求值一次，副本前缀不在宏内回收，由vm_case随后check_bytes。只在本函数内define并在115行undef。
 *
 * ## References
 * @see tests/parser/test_ssa_compare_metadata_guards.c:82
 * @see tests/parser/test_ssa_compare_metadata_guards.c:85
 * @see tests/parser/test_ssa_compare_metadata_guards.c:111
 * @see tests/parser/test_ssa_compare_metadata_guards.c:115
 * @see tests/parser/test_ssa_compare_metadata_guards.c:482
 */

/**
 * @def SAVE_POOL(field, count)
 *
 * ## Summary
 * 把projection的一个直接池指针及其逻辑count成员展开为SAVE_RAW，按元素sizeof形成当前池的明确字节区间。
 *
 * ## Parameters
 * @param[in] field projection直接池成员的标识符token。
 * @param[in] count 该池逻辑元素计数成员的标识符token。
 *
 * ## Returns
 * 无独立C返回值；继承SAVE_RAW的继续/从包围函数返回FALSE行为，失败前缀由caller处理。
 *
 * ## Constraints and lifecycle
 * field和count是p成员名token而非通用运行期参数；依赖snapshot_projection中的p/saved/next和SAVE_RAW。乘法在宏内不做溢出检查，当前固定已验证小fixture提供尺寸前提；空池size0允许NULL。114行undef，不会遍历嵌套指针或自动深拷贝整个图。
 *
 * ## References
 * @see tests/parser/test_ssa_compare_metadata_guards.c:84
 * @see tests/parser/test_ssa_compare_metadata_guards.c:86
 * @see tests/parser/test_ssa_compare_metadata_guards.c:110
 * @see tests/parser/test_ssa_compare_metadata_guards.c:114
 */

/**
 * @fn SZrSemanticIrInstruction *instruction(SFixture *f, TZrUInt32 id)
 *
 * ## Summary
 * 按本fixture的一基语义instruction ID返回SemanticIr instruction数组内的可变借用，供受控突变。
 *
 * ## Parameters
 * @param[in] f 包含稳定semantic.instructions存储的fixture，只读描述；返回借用允许调用方随后改其元素。
 * @param[in] id 当前fixture中有效的一基语义instruction ID。
 *
 * ## Returns
 * @return Array_Get返回的池内可变指针；本函数不分配、不复制，也不接管元素所有权。
 *
 * ## Constraints and lifecycle
 * 本函数只读取f及数组描述，不检查f非NULL或fixture初始化。当前caller保证id在已构造instruction长度内；id-1传给Array_Get，不能把这个薄封装视为任意无效ID的完整契约。返回指针的存储仍归semantic数组，数组扩容/Free或fixture销毁后不可再用；需要写入时由caller保证独占。本TU的case和计数器只按main顺序使用；没有同步、并发调用或跨线程借用契约。
 *
 * ## References
 * @see tests/parser/test_ssa_compare_metadata_guards.c:119
 * @see tests/parser/test_ssa_compare_metadata_guards.c:120
 * @see tests/parser/test_ssa_compare_metadata_guards.c:340
 * @see tests/parser/test_ssa_compare_metadata_guards.c:342
 * @see tests/parser/test_ssa_compare_metadata_guards.c:398
 * @see tests/parser/test_ssa_compare_metadata_guards.c:399
 */

/**
 * @fn SZrSemanticIrValue *value(SFixture *f, TZrUInt32 id)
 *
 * ## Summary
 * 按本fixture的一基ValueId返回semantic.values内可变借用，用于故意改动值类型或definition元数据。
 *
 * ## Parameters
 * @param[in] f 具有稳定semantic.values池的fixture；函数只读取描述，后续元素写入由caller负责。
 * @param[in] id 当前fixture有效的一基ValueId。
 *
 * ## Returns
 * @return Array_Get给出的可变池元素指针；没有新资源或失败诊断。
 *
 * ## Constraints and lifecycle
 * f必须有效且池初始化；当前caller只传LEFT/RIGHT/RESULT等已建立的ID。函数不验证完整ID/fixture，不延长池生命周期；任何扩容、Free或fixture销毁均结束对应借用。只读描述不等于允许与借用写者并发。本TU的case和计数器只按main顺序使用；没有同步、并发调用或跨线程借用契约。
 *
 * ## References
 * @see tests/parser/test_ssa_compare_metadata_guards.c:123
 * @see tests/parser/test_ssa_compare_metadata_guards.c:124
 * @see tests/parser/test_ssa_compare_metadata_guards.c:281
 * @see tests/parser/test_ssa_compare_metadata_guards.c:287
 * @see tests/parser/test_ssa_compare_metadata_guards.c:397
 * @see tests/parser/test_ssa_compare_metadata_guards.c:402
 */

/**
 * @fn TZrBool emit(SFixture *f, EZrSemanticIrOpcode opcode, TZrTypeId type, TZrValueId result, const TZrValueId *operands, TZrSize count, TZrUInt32 constant, TZrUInt32 predicate)
 *
 * ## Summary
 * 将测试参数填入SemanticIrInstructionSpec，并通过真实SemanticIr_Emit追加一条指令；只对CONSTANT填写constant pool索引、对COMPARE填写predicate与fixture intType。
 *
 * ## Parameters
 * @param[in,out] f 待追加的fixture，semantic所有数组由其持有。
 * @param[in] opcode 本次语义opcode。
 * @param[in] type 写入spec.typeId的类型ID。
 * @param[in] result 写入resultValueId的一基值ID或本场景允许的零。
 * @param[in] operands 下层调用期间借用的operand数组；count非零时须可读。
 * @param[in] count operand数量。
 * @param[in] constant 仅CONSTANT使用的模块常量池索引。
 * @param[in] predicate 仅COMPARE使用的比较predicate。
 *
 * ## Returns
 * @return SemanticIr_Emit返回非INVALID ID时为TRUE，否则FALSE；不返回新数组的独立所有权。
 *
 * ## Constraints and lifecycle
 * f须已初始化并拥有可写semantic数组；operands只在下层调用中借用，数量和ID须由调用方按场景提供。固定targetBlockId无效，source offset按当前instruction数乘10再加5，仅适用于这个小fixture。FALSE不承诺通用下层OOM回滚；caller遇构造失败走free_fixture。返回TRUE仅代表获得非零instruction ID，不是完整semantic/CFG/primitive校验。本TU的case和计数器只按main顺序使用；没有同步、并发调用或跨线程借用契约。
 *
 * ## References
 * @see tests/parser/test_ssa_compare_metadata_guards.c:127
 * @see tests/parser/test_ssa_compare_metadata_guards.c:130
 * @see tests/parser/test_ssa_compare_metadata_guards.c:137
 * @see tests/parser/test_ssa_compare_metadata_guards.c:139
 * @see tests/parser/test_ssa_compare_metadata_guards.c:143
 * @see tests/parser/test_ssa_compare_metadata_guards.c:147
 * @see tests/parser/test_ssa_compare_metadata_guards.c:175
 * @see tests/parser/test_ssa_compare_metadata_guards.c:182
 * @see zr_vm_parser/src/zr_vm_parser/semantic_ir/semantic_ir.c:583
 */

/**
 * @fn TZrBool initialize_fixture(SFixture *f)
 *
 * ## Summary
 * 清零fixture并逐步创建真实Runtime State、SemanticContext、canonical primitive类型、SemanticIr函数及三个值，最后发出左右两个常量。
 *
 * ## Parameters
 * @param[out] f 空的可写fixture输出，失败时也保留已建立资源供free_fixture。
 *
 * ## Returns
 * @return 所有显式创建/ID检查及两个emit成功为TRUE；任一显式前提失败返回FALSE，允许留下部分初始化f。
 *
 * ## Constraints and lifecycle
 * 只能对无待保留资源的fixture开始；memset不释放旧owner。先预留32个类型ID，再实际intern INT64/BOOL/DOUBLE/INT8，且显式检查INT64/BOOL ID不等于runtime枚举；这些身份只用于fixture，不构成来源可调用性证明。semanticInitialized只在Init后置真，已失败的部分构造仍归f，由调用者free_fixture一次回收；本函数FALSE不自行清理。运行时fatal异常不在本报告保证的布尔失败清理内。本TU的case和计数器只按main顺序使用；没有同步、并发调用或跨线程借用契约。
 *
 * ## References
 * @see tests/parser/test_ssa_compare_metadata_guards.c:152
 * @see tests/parser/test_ssa_compare_metadata_guards.c:154
 * @see tests/parser/test_ssa_compare_metadata_guards.c:156
 * @see tests/parser/test_ssa_compare_metadata_guards.c:158
 * @see tests/parser/test_ssa_compare_metadata_guards.c:160
 * @see tests/parser/test_ssa_compare_metadata_guards.c:162
 * @see tests/parser/test_ssa_compare_metadata_guards.c:166
 * @see tests/parser/test_ssa_compare_metadata_guards.c:169
 * @see tests/parser/test_ssa_compare_metadata_guards.c:170
 * @see tests/parser/test_ssa_compare_metadata_guards.c:171
 * @see tests/parser/test_ssa_compare_metadata_guards.c:175
 * @see tests/harness/runtime_support.c:385
 * @see zr_vm_parser/src/zr_vm_parser/semantic_ir/semantic_ir.c:263
 */

/**
 * @fn TZrBool finish_semantic(SFixture *f, TZrUInt32 predicate)
 *
 * ## Summary
 * 在已有两个常量之后追加COMPARE、条件BRANCH及两条RETURN，再建立entry和两个exit块、两条带真假flags边及对应指令区间。
 *
 * ## Parameters
 * @param[in,out] f 持有可写semantic/CFG的部分fixture。
 * @param[in] predicate 用于COMPARE的测试predicate；当前caller选LT或GT。
 *
 * ## Returns
 * @return 四条emit、三个按预期ID的block及所有Connect/Bind都成功为TRUE；任一失败为FALSE，不回滚之前追加内容。
 *
 * ## Constraints and lifecycle
 * 调用方须提供仅含左右常量/三个值且CFG为空的初始化fixture。predicate来自有限LT/GT场景；本函数不清除已存在块/指令，非幂等。任何短路FALSE都可留下追加前缀，caller随后销毁整个fixture，不把它视为事务式构造。成功仍由SemIR/Canonical/Core/Projection前提各自复核。本TU的case和计数器只按main顺序使用；没有同步、并发调用或跨线程借用契约。
 *
 * ## References
 * @see tests/parser/test_ssa_compare_metadata_guards.c:179
 * @see tests/parser/test_ssa_compare_metadata_guards.c:180
 * @see tests/parser/test_ssa_compare_metadata_guards.c:182
 * @see tests/parser/test_ssa_compare_metadata_guards.c:187
 * @see tests/parser/test_ssa_compare_metadata_guards.c:191
 * @see tests/parser/test_ssa_compare_metadata_guards.c:193
 * @see tests/parser/test_ssa_compare_metadata_guards.c:195
 * @see tests/parser/test_ssa_compare_metadata_guards.c:197
 * @see tests/parser/test_ssa_compare_metadata_guards.c:338
 * @see tests/parser/test_ssa_compare_metadata_guards.c:446
 */

/**
 * @fn TZrBool canonical_semantic_valid(const SFixture *f)
 *
 * ## Summary
 * 在清零的临时CompilerState中浅借fixture context与semantic记录，只调用实际compiler_semantic_compare_validate检查canonical primitive意义。
 *
 * ## Parameters
 * @param[in] f 只读fixture借用，context与semantic在调用期间有效。
 *
 * ## Returns
 * @return 原样返回实际canonical compare validator的结果，不分配或转移fixture所有权。
 *
 * ## Constraints and lifecycle
 * borrowed不拥有context或任何semantic池，不能调用CompilerState/semantic Free；f及其存储在整个验证期间保持稳定。这里没有调用compiler lower、parse或函数执行。此实际消费者只检查COMPARE operand primitive INT64和result BOOL，因此TRUE不等价于完整SemIR/CFG合法，需与相应前提组合。本TU的case和计数器只按main顺序使用；没有同步、并发调用或跨线程借用契约。
 *
 * ## References
 * @see tests/parser/test_ssa_compare_metadata_guards.c:200
 * @see tests/parser/test_ssa_compare_metadata_guards.c:201
 * @see tests/parser/test_ssa_compare_metadata_guards.c:202
 * @see tests/parser/test_ssa_compare_metadata_guards.c:203
 * @see tests/parser/test_ssa_compare_metadata_guards.c:204
 * @see zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_semantic_compare.c:93
 * @see zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_semantic_compare.c:101
 * @see zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_semantic_compare.c:103
 */

/**
 * @fn TZrBool build_module(SFixture *f, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 把fixture semantic与固定functionToken/signatureHash装入BuildInput，调用真实BuildModule写入fixture module并输出诊断。
 *
 * ## Parameters
 * @param[in,out] f 输入semantic借用与待构造module所在的独占fixture。
 * @param[out] diagnostic 下层诊断输出；独立于fixture输入/池的可写槽。
 *
 * ## Returns
 * @return 原样返回BuildModule布尔值；成功资源归f.module，失败不由此包装额外释放fixture。
 *
 * ## Constraints and lifecycle
 * f.semantic须在本次调用内可读，f.module是先ModuleInit得到的独立owner；diagnostic须独立可写，不能覆盖输入记录或其池。此包装不实现额外事务或校验，失败状态由BuildModule决定；本批stored case直接断言空module及输入字节保留，不外推任意旧module事务。本TU的case和计数器只按main顺序使用；没有同步、并发调用或跨线程借用契约。
 *
 * ## References
 * @see tests/parser/test_ssa_compare_metadata_guards.c:207
 * @see tests/parser/test_ssa_compare_metadata_guards.c:208
 * @see tests/parser/test_ssa_compare_metadata_guards.c:209
 * @see tests/parser/test_ssa_compare_metadata_guards.c:210
 * @see tests/parser/test_ssa_compare_metadata_guards.c:212
 * @see tests/parser/test_ssa_compare_metadata_guards.c:373
 * @see zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c:890
 * @see zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c:902
 * @see zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c:903
 * @see zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c:910
 */

/**
 * @fn TZrBool build_projection(SFixture *f)
 *
 * ## Summary
 * 顺序要求SemIRValidate、CanonicalCompare、BuildModule及CoreVerifyAll，然后检查有限Core元数据/真实常量索引，并用两项canonical INT64常量构建projection。
 *
 * ## Parameters
 * @param[in,out] f 已完成semantic/CFG的可写fixture，接收module和projection所有权。
 *
 * ## Returns
 * @return 前提失败或最终projection构造失败为FALSE；最终构造前提成功为TRUE，但此前EXPECT失败仍可能令caseOk为FALSE。
 *
 * ## Constraints and lifecycle
 * 只有prerequisite失败会使本函数短路FALSE；三项EXPECT元数据检查即使失败也可能继续并最终返回TRUE，caseOk才记录其失败。常量数组是局部借用，下层projection构造应拥有所需副本，caller最后free_fixture。Core仅验证这个手工function，不能当作源码解析、生产编译或AOT运行证明。本TU的case和计数器只按main顺序使用；没有同步、并发调用或跨线程借用契约。
 *
 * ## References
 * @see tests/parser/test_ssa_compare_metadata_guards.c:215
 * @see tests/parser/test_ssa_compare_metadata_guards.c:219
 * @see tests/parser/test_ssa_compare_metadata_guards.c:220
 * @see tests/parser/test_ssa_compare_metadata_guards.c:221
 * @see tests/parser/test_ssa_compare_metadata_guards.c:223
 * @see tests/parser/test_ssa_compare_metadata_guards.c:225
 * @see tests/parser/test_ssa_compare_metadata_guards.c:228
 * @see tests/parser/test_ssa_compare_metadata_guards.c:231
 * @see tests/parser/test_ssa_compare_metadata_guards.c:234
 * @see tests/parser/test_ssa_compare_metadata_guards.c:447
 */

/**
 * @fn void free_fixture(SFixture *f)
 *
 * ## Summary
 * 按依赖顺序释放projection与module、可选semantic、context和Runtime State，支持initialize_fixture已清零后的布尔失败前缀。
 *
 * ## Parameters
 * @param[in,out] f 本case独占的完整或部分fixture；释放其已建立的资源，记录自身仍是调用方栈对象。
 *
 * ## Returns
 * 无C返回值；正常完成后旧projection/module/semantic/context/state借用均不可继续使用；未把记录重置为空。
 *
 * ## Constraints and lifecycle
 * f须指向初始化路径至少执行过memset的记录；semantic只在semanticInitialized为真时Free，context/state分别按nonnull处理。该函数不把成员或semanticInitialized清零，调用后记录不可再次重复Free或使用旧借用。VM emission/忽略根属于vm_case，在本函数前另行处理；这里没有拥有外部传入的浅借用副本。本TU的case和计数器只按main顺序使用；没有同步、并发调用或跨线程借用契约。
 *
 * ## References
 * @see tests/parser/test_ssa_compare_metadata_guards.c:238
 * @see tests/parser/test_ssa_compare_metadata_guards.c:239
 * @see tests/parser/test_ssa_compare_metadata_guards.c:240
 * @see tests/parser/test_ssa_compare_metadata_guards.c:241
 * @see tests/parser/test_ssa_compare_metadata_guards.c:242
 * @see tests/parser/test_ssa_compare_metadata_guards.c:243
 * @see tests/parser/test_ssa_compare_metadata_guards.c:323
 * @see tests/parser/test_ssa_compare_metadata_guards.c:529
 * @see tests/harness/runtime_support.c:403
 */

/**
 * @typedef EBadSemantic
 *
 * ## Summary
 * 为20种有限COMPARE metadata畸形情形建立稳定enum顺序，并以COUNT界定两组参数化场景的遍历与名称表索引。
 *
 * ## Fields and invariants
 * 有效标签从BAD_PREDICATE_ZERO到BAD_RESULT_DEFINITION，共20项；BAD_SEMANTIC_COUNT为排他上界，不可作为semanticNames下标。
 *
 * ## Constraints and lifecycle
 * 枚举值不是生产诊断码。ZERO predicate实际上是共享比较枚举中源切片未支持的EQ；UNKNOWN、arity、ID、definition、type及非COMPARE携带metadata各自可能先被不同层拒绝。一个enum项同时驱动emit/stored两个场景，但不是两个新函数定义。
 *
 * ## References
 * @see tests/parser/test_ssa_compare_metadata_guards.c:246
 * @see tests/parser/test_ssa_compare_metadata_guards.c:253
 * @see tests/parser/test_ssa_compare_metadata_guards.c:256
 * @see tests/parser/test_ssa_compare_metadata_guards.c:267
 * @see tests/parser/test_ssa_compare_metadata_guards.c:540
 */

/**
 * @var semanticNames
 *
 * ## Summary
 * 为EBadSemantic提供20个emit/stored日志名。
 *
 * ## Fields and invariants
 * 单一串行测试进程中的TU静态状态；不是公共API、线程同步对象或生产缓存。
 *
 * ## Constraints and lifecycle
 * 数组元素是借用的静态字符串，本TU实际只读，不分配/释放；const char*不意味着指针数组类型本身不可写。有效enum下标与表顺序一一对应。本TU的case和计数器只按main顺序使用；没有同步、并发调用或跨线程借用契约。
 *
 * ## References
 * @see tests/parser/test_ssa_compare_metadata_guards.c:256
 * @see tests/parser/test_ssa_compare_metadata_guards.c:300
 * @see tests/parser/test_ssa_compare_metadata_guards.c:335
 * @see tests/parser/test_ssa_compare_metadata_guards.c:540
 */

/**
 * @fn void mutate_spec(SFixture *f, EBadSemantic bad, SZrSemanticIrInstructionSpec *spec, TZrValueId operands[3])
 *
 * ## Summary
 * 按一个EBadSemantic标签改写待emit spec、三项operand栈数组，必要时直接改fixture value元数据，以构造指定畸形场景。
 *
 * ## Parameters
 * @param[in,out] f 存在三个fixture值的可写owner，部分标签改其value.type/definition。
 * @param[in] bad 选择单项突变的有效enum标签。
 * @param[in,out] spec 已填基本COMPARE元数据的可写spec。
 * @param[in,out] operands 至少三项的可写caller栈数组，部分标签改前两项。
 *
 * ## Returns
 * 无C返回值；正常完成本地操作后返回。EXPECT只记录当前case失败并继续，最终由main汇总失败计数，不触发Unity longjmp或自动进程退出。
 *
 * ## Constraints and lifecycle
 * bad须来自有限enum；默认分支不改输入。f须已有LEFT/RIGHT/RESULT；spec和operands须为独立可写对象，operands至少三项，不能与fixture持有池混为所有权。此函数刻意不保持语义合法性，也不返回验证结果、分配或回滚；snapshot在突变之后采集。本TU的case和计数器只按main顺序使用；没有同步、并发调用或跨线程借用契约。
 *
 * ## References
 * @see tests/parser/test_ssa_compare_metadata_guards.c:265
 * @see tests/parser/test_ssa_compare_metadata_guards.c:267
 * @see tests/parser/test_ssa_compare_metadata_guards.c:268
 * @see tests/parser/test_ssa_compare_metadata_guards.c:281
 * @see tests/parser/test_ssa_compare_metadata_guards.c:282
 * @see tests/parser/test_ssa_compare_metadata_guards.c:287
 * @see tests/parser/test_ssa_compare_metadata_guards.c:289
 * @see tests/parser/test_ssa_compare_metadata_guards.c:311
 * @see tests/parser/test_ssa_compare_metadata_guards.c:351
 */

/**
 * @fn void emission_case(EBadSemantic bad)
 *
 * ## Summary
 * 对一个EBadSemantic标签建立真实空比较输出fixture，突变spec后保存semantic记录及四个池的当前字节，要求SemanticIr_Emit拒绝且保存区间不变。
 *
 * ## Parameters
 * @param[in] bad main提供的有效EBadSemantic枚举项；每调用一次产生一个emit场景。
 *
 * ## Returns
 * 无C返回值；场景通过/失败记录在caseOk及累计计数。布尔前提失败会走done，EXPECT失败不立刻返回或exit。
 *
 * ## Constraints and lifecycle
 * main遍历20个有效标签；semanticNames下标必须在界内。snapshot是在故意突变后取样，比较证明拒绝操作没有改这些选定输入，不是恢复到合法输入，也不覆盖OOM事务。前提失败显式goto done；snapshot部分失败仍消费五个零初始化证据槽，正常检查后也只消费一次，再free_fixture/end_case。EXPECT失败会继续清理和后续case。本TU的case和计数器只按main顺序使用；没有同步、并发调用或跨线程借用契约。
 *
 * ## References
 * @see tests/parser/test_ssa_compare_metadata_guards.c:294
 * @see tests/parser/test_ssa_compare_metadata_guards.c:300
 * @see tests/parser/test_ssa_compare_metadata_guards.c:302
 * @see tests/parser/test_ssa_compare_metadata_guards.c:311
 * @see tests/parser/test_ssa_compare_metadata_guards.c:312
 * @see tests/parser/test_ssa_compare_metadata_guards.c:318
 * @see tests/parser/test_ssa_compare_metadata_guards.c:320
 * @see tests/parser/test_ssa_compare_metadata_guards.c:321
 * @see tests/parser/test_ssa_compare_metadata_guards.c:323
 * @see tests/parser/test_ssa_compare_metadata_guards.c:324
 * @see tests/parser/test_ssa_compare_metadata_guards.c:541
 * @see zr_vm_parser/src/zr_vm_parser/semantic_ir/semantic_ir.c:590
 * @see zr_vm_parser/src/zr_vm_parser/semantic_ir/semantic_ir.c:609
 * @see zr_vm_parser/src/zr_vm_parser/semantic_ir/semantic_ir.c:616
 */

/**
 * @fn void stored_case(EBadSemantic bad)
 *
 * ## Summary
 * 先构造并验证完整合法SemIR，再按标签突变已存instruction/operand或value；保存五类semantic区间和module记录，要求SemIRValidate及BuildModule拒绝、module保持空并给出有限精确诊断。
 *
 * ## Parameters
 * @param[in] bad main遍历的有效EBadSemantic标签；每调用生成一个stored场景。
 *
 * ## Returns
 * 无C返回值；通过case状态与汇总计数报告，正常及显式前提失败均经done清理。
 *
 * ## Constraints and lifecycle
 * 20标签逐个独立fixture；非COMPARE元数据标签改LEFT常量而非COMPARE，rejectedId相应为LEFT。snapshot在突变之后，故不证明恢复原合法状态。BuildModule的早期result-definition一致性等门禁可能先于compare lowering拒绝；BAD_MATCH诊断source0，其余期望rejectedId。前提或snapshot失败均显式走一次清理，EXPECT不中断后续检查。本TU的case和计数器只按main顺序使用；没有同步、并发调用或跨线程借用契约。
 *
 * ## References
 * @see tests/parser/test_ssa_compare_metadata_guards.c:327
 * @see tests/parser/test_ssa_compare_metadata_guards.c:337
 * @see tests/parser/test_ssa_compare_metadata_guards.c:339
 * @see tests/parser/test_ssa_compare_metadata_guards.c:340
 * @see tests/parser/test_ssa_compare_metadata_guards.c:341
 * @see tests/parser/test_ssa_compare_metadata_guards.c:351
 * @see tests/parser/test_ssa_compare_metadata_guards.c:363
 * @see tests/parser/test_ssa_compare_metadata_guards.c:372
 * @see tests/parser/test_ssa_compare_metadata_guards.c:373
 * @see tests/parser/test_ssa_compare_metadata_guards.c:375
 * @see tests/parser/test_ssa_compare_metadata_guards.c:376
 * @see tests/parser/test_ssa_compare_metadata_guards.c:378
 * @see tests/parser/test_ssa_compare_metadata_guards.c:381
 * @see tests/parser/test_ssa_compare_metadata_guards.c:386
 * @see tests/parser/test_ssa_compare_metadata_guards.c:542
 * @see zr_vm_parser/src/zr_vm_parser/semantic_ir/semantic_ir.c:699
 * @see zr_vm_parser/src/zr_vm_parser/semantic_ir/semantic_ir.c:831
 * @see zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c:212
 * @see zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build_compare.c:72
 * @see zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c:653
 */

/**
 * @fn void opaque_primitive_case(TZrBool wrongOperand)
 *
 * ## Summary
 * 分别把一套自洽opaque canonical ID关系改成DOUBLE operands或DOUBLE result，要求关系型SemIRValidate仍接受，而实际canonical compare消费者拒绝其primitive意义。
 *
 * ## Parameters
 * @param[in] wrongOperand 真选择DOUBLE操作数，假选择DOUBLE结果；不是任意类型枚举。
 *
 * ## Returns
 * 无C返回值；两种场景分别通过caseOk/计数记录检查结果，资源走done释放。
 *
 * ## Constraints and lifecycle
 * wrongOperand真时同时修改左右value、对应CONSTANT、COMPARE operand metadata和RETURN type以保持关系一致；假时同时修改RESULT value与COMPARE result type。只调用canonical compare validator，不lower或执行源码。前提成功后两项EXPECT是直接检查，失败仍清理fixture并记录case。本TU的case和计数器只按main顺序使用；没有同步、并发调用或跨线程借用契约。
 *
 * ## References
 * @see tests/parser/test_ssa_compare_metadata_guards.c:390
 * @see tests/parser/test_ssa_compare_metadata_guards.c:392
 * @see tests/parser/test_ssa_compare_metadata_guards.c:393
 * @see tests/parser/test_ssa_compare_metadata_guards.c:396
 * @see tests/parser/test_ssa_compare_metadata_guards.c:397
 * @see tests/parser/test_ssa_compare_metadata_guards.c:402
 * @see tests/parser/test_ssa_compare_metadata_guards.c:407
 * @see tests/parser/test_ssa_compare_metadata_guards.c:408
 * @see tests/parser/test_ssa_compare_metadata_guards.c:411
 * @see tests/parser/test_ssa_compare_metadata_guards.c:544
 * @see tests/parser/test_ssa_compare_metadata_guards.c:545
 * @see zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_semantic_compare.c:101
 * @see zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_semantic_compare.c:103
 */

/**
 * @typedef EVmCase
 *
 * ## Summary
 * 枚举21个canonical VM边界场景，使前五项正例与后16项负例按main的有限顺序运行。
 *
 * ## Fields and invariants
 * 前4项为LT/GT真/假，VM_EXPLICIT_INT_MATCH为第五正例；其余16项均应拒绝。VM_CASE_COUNT为排他上界，不能传给vm_case。
 *
 * ## Constraints and lifecycle
 * 分类条件依赖当前枚举顺序test<=VM_EXPLICIT_INT_MATCH；不能随意重排而保留此判断。仅含LT/GT真假及显式match正例，其余为type/ID/arity/range/slot和branch mutation。无生产dispatch值或对象所有权。
 *
 * ## References
 * @see tests/parser/test_ssa_compare_metadata_guards.c:415
 * @see tests/parser/test_ssa_compare_metadata_guards.c:421
 * @see tests/parser/test_ssa_compare_metadata_guards.c:424
 * @see tests/parser/test_ssa_compare_metadata_guards.c:440
 * @see tests/parser/test_ssa_compare_metadata_guards.c:457
 * @see tests/parser/test_ssa_compare_metadata_guards.c:546
 */

/**
 * @var vmNames
 *
 * ## Summary
 * 为EVmCase提供21个VM日志名。
 *
 * ## Fields and invariants
 * 单一串行测试进程中的TU静态状态；不是公共API、线程同步对象或生产缓存。
 *
 * ## Constraints and lifecycle
 * 元素指向静态字面量，当前调用只读；enum有效范围保证索引。名称不证明某个下层分支已到达，须结合前提及实际断言。本TU的case和计数器只按main顺序使用；没有同步、并发调用或跨线程借用契约。
 *
 * ## References
 * @see tests/parser/test_ssa_compare_metadata_guards.c:424
 * @see tests/parser/test_ssa_compare_metadata_guards.c:443
 * @see tests/parser/test_ssa_compare_metadata_guards.c:546
 */

/**
 * @fn void vm_case(EVmCase test)
 *
 * ## Summary
 * 从真实构造并验证的projection出发，对21个有限标签检查canonical VM物化边界；五个正例忽略根保护function后执行并检查返回整数，其余负例检查空emission和精确诊断，并比较28项输入证据。
 *
 * ## Parameters
 * @param[in] test main提供的有效EVmCase标签；每调用产生一个vm场景。
 *
 * ## Returns
 * 无C返回值；显式前提失败和正常路径都经done；非终止EXPECT将失败留给main汇总，不表示失败输入会执行VM。
 *
 * ## Constraints and lifecycle
 * test须为有效EVmCase，main顺序遍历。LT/GT分别覆盖真假选择，显式canonical INT64 match为兼容正例；原共享Core MATCH0没有被测试代码伪造为runtime枚举。所有输入突变后才snapshot；snapshot/前提失败走done。正例只有Materialize和IgnoreObject前提成功才执行；EXPECT执行失败会继续检查result，但caseOk已标失败。rooted真时先Unignore，随后Emission_Free只free pcMap并清记录，GC function最终由fixture State销毁管理。调用方记录output本身为局部清零记录，不测试保留已有emission。本TU的case和计数器只按main顺序使用；没有同步、并发调用或跨线程借用契约。
 *
 * ## References
 * @see tests/parser/test_ssa_compare_metadata_guards.c:432
 * @see tests/parser/test_ssa_compare_metadata_guards.c:438
 * @see tests/parser/test_ssa_compare_metadata_guards.c:440
 * @see tests/parser/test_ssa_compare_metadata_guards.c:445
 * @see tests/parser/test_ssa_compare_metadata_guards.c:450
 * @see tests/parser/test_ssa_compare_metadata_guards.c:457
 * @see tests/parser/test_ssa_compare_metadata_guards.c:482
 * @see tests/parser/test_ssa_compare_metadata_guards.c:486
 * @see tests/parser/test_ssa_compare_metadata_guards.c:488
 * @see tests/parser/test_ssa_compare_metadata_guards.c:490
 * @see tests/parser/test_ssa_compare_metadata_guards.c:492
 * @see tests/parser/test_ssa_compare_metadata_guards.c:494
 * @see tests/parser/test_ssa_compare_metadata_guards.c:497
 * @see tests/parser/test_ssa_compare_metadata_guards.c:501
 * @see tests/parser/test_ssa_compare_metadata_guards.c:502
 * @see tests/parser/test_ssa_compare_metadata_guards.c:503
 * @see tests/parser/test_ssa_compare_metadata_guards.c:510
 * @see tests/parser/test_ssa_compare_metadata_guards.c:516
 * @see tests/parser/test_ssa_compare_metadata_guards.c:527
 * @see tests/parser/test_ssa_compare_metadata_guards.c:528
 * @see tests/parser/test_ssa_compare_metadata_guards.c:529
 * @see tests/parser/test_ssa_compare_metadata_guards.c:546
 * @see zr_vm_parser/src/zr_vm_parser/exec_ir/execbc/exec_ir_execbc_vm_canonical_types.c:66
 * @see zr_vm_parser/src/zr_vm_parser/exec_ir/execbc/exec_ir_execbc_vm_canonical_types.c:103
 * @see zr_vm_parser/src/zr_vm_parser/exec_ir/execbc/exec_ir_execbc_compare_types.c:34
 * @see zr_vm_parser/src/zr_vm_parser/exec_ir/execbc/exec_ir_execbc_compare_types.c:49
 * @see zr_vm_parser/src/zr_vm_parser/exec_ir/execbc/exec_ir_execbc_vm_validate.c:712
 * @see zr_vm_parser/src/zr_vm_parser/exec_ir/execbc/exec_ir_execbc_vm.c:714
 * @see tests/harness/runtime_support.c:451
 */

/**
 * @fn int main(void)
 *
 * ## Summary
 * 将stdout设为无缓冲，按enum顺序运行20个emit和20个stored场景、两种opaque primitive场景及21个VM场景，汇总63次case结果与前提计数。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * @return 无累计case/前提失败时EXIT_SUCCESS，否则EXIT_FAILURE；没有把完整执行程序失败隐去为void case的“无返回码”。
 *
 * ## Constraints and lifecycle
 * 这是独立C main，不调用Unity RUN_TEST/UnityDefaultTestRun；setUp/tearDown仅作为链接兼容空hook存在。每个参数化调用内部begin/end一次，未满足前提仍记录为失败场景并清理，不等于后续目标断言已经执行。仅依据failures或preconditionFailures决定EXIT码；setvbuf失败结果被忽略。本TU的case和计数器只按main顺序使用；没有同步、并发调用或跨线程借用契约。
 *
 * ## References
 * @see tests/parser/test_ssa_compare_metadata_guards.c:538
 * @see tests/parser/test_ssa_compare_metadata_guards.c:539
 * @see tests/parser/test_ssa_compare_metadata_guards.c:540
 * @see tests/parser/test_ssa_compare_metadata_guards.c:541
 * @see tests/parser/test_ssa_compare_metadata_guards.c:542
 * @see tests/parser/test_ssa_compare_metadata_guards.c:544
 * @see tests/parser/test_ssa_compare_metadata_guards.c:545
 * @see tests/parser/test_ssa_compare_metadata_guards.c:546
 * @see tests/parser/test_ssa_compare_metadata_guards.c:547
 * @see tests/parser/test_ssa_compare_metadata_guards.c:549
 * @see tests/cmake/ssa-compare-metadata-guards.cmake:2
 * @see tests/cmake/ssa-compare-metadata-guards.cmake:15
 * @see tests/cmake/ssa-source-direct-validation/CMakeLists.txt:497
 */
