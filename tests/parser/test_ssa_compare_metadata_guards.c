#include "tests/harness/runtime_support.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/exec_ir_builder.h"
#include "zr_vm_parser/exec_ir_execbc_vm.h"
#include "zr_vm_parser/exec_ir_projections.h"
#include "zr_vm_parser/semantic_ir.h"
#include "zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_compare.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
