#include "zr_vm_parser/exec_ir_branch_facts.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct SFixture {
    SZrExecIrModule module;
    SZrExecIrFunction function;
    SZrExecIrValue values[8];
    SZrExecIrInstruction instructions[12];
    SZrExecIrBlock blocks[6];
    TZrExecIrValueId operands[24], results[12], signedValues[8];
    TZrExecIrBlockId successors[12], predecessors[12];
    SZrExecIrPhi phis[2];
    SZrExecIrPhiIncoming incoming[4];
    SZrExecIrConstant constant;
    SZrExecIrBranchEntryFact entries[4];
    SZrExecIrBranchFactsInput input;
    SZrExecIrBranchFacts facts;
    SZrExecIrDiagnostic diagnostic;
} SFixture;

static unsigned failures, cases;
static const char *caseName;
#define REQUIRE(c) do { if (!(c)) { \
    fprintf(stderr, "FAIL %s:%u [%s]: %s\n", __FILE__, \
            (unsigned)__LINE__, caseName, #c); ++failures; return; \
} } while (0)

static void init(SFixture *f, TZrUInt32 selector, TZrInt64 threshold) {
    memset(f, 0, sizeof(*f));
    f->module.id = 1u;
    f->module.functions = &f->function;
    f->module.functionCount = f->module.functionCapacity = 1u;
    f->function.id = 1u;
    f->function.functionToken = 1u;
    f->function.entryBlockId = 1u;
    f->function.values = f->values;
    f->function.valueCount = f->function.valueCapacity = 3u;
    f->function.instructions = f->instructions;
    f->function.instructionCount = f->function.instructionCapacity = 4u;
    f->function.blocks = f->blocks;
    f->function.blockCount = f->function.blockCapacity = 3u;
    f->function.operands = f->operands;
    f->function.operandCount = f->function.operandCapacity = 5u;
    f->function.results = f->results;
    f->function.resultCount = f->function.resultCapacity = 1u;
    f->function.successors = f->successors;
    f->function.successorCount = f->function.successorCapacity = 2u;
    f->function.predecessors = f->predecessors;
    f->function.predecessorCount = f->function.predecessorCapacity = 2u;
    for (unsigned i = 0; i < 3u; ++i) {
        f->values[i].id = i + 1u;
        f->values[i].typeToken = 0xABC000u + i; /* Opaque identities. */
        f->blocks[i].id = i + 1u;
    }
    f->values[0].flags = f->values[1].flags = ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY;
    f->values[2].definition = 1u;
    f->blocks[0].flags = ZR_EXEC_IR_BLOCK_FLAG_ENTRY;
    f->blocks[0].instructions = (SZrExecIrRange){0u, 2u};
    f->blocks[0].successors = (SZrExecIrRange){0u, 2u};
    f->blocks[0].terminatorInstructionId = 2u;
    for (unsigned i = 1; i < 3u; ++i) {
        f->blocks[i].instructions = (SZrExecIrRange){i + 1u, 1u};
        f->blocks[i].predecessors = (SZrExecIrRange){i - 1u, 1u};
        f->blocks[i].terminatorInstructionId = i + 2u;
        f->blocks[i].immediateDominator = 1u;
    }
    f->successors[0] = 2u; f->successors[1] = 3u;
    f->predecessors[0] = f->predecessors[1] = 1u;
    f->instructions[0].opcode = ZR_EXEC_IR_OPCODE_COMPARE;
    f->instructions[0].typeToken = selector;
    f->instructions[0].operands = (SZrExecIrRange){0u, 2u};
    f->instructions[0].results = (SZrExecIrRange){0u, 1u};
    f->instructions[1].opcode = ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH;
    f->instructions[1].operands = (SZrExecIrRange){2u, 1u};
    f->instructions[1].successorRange = (SZrExecIrRange){0u, 2u};
    f->instructions[2].opcode = f->instructions[3].opcode = ZR_EXEC_IR_OPCODE_RETURN;
    f->instructions[2].operands = (SZrExecIrRange){3u, 1u};
    f->instructions[3].operands = (SZrExecIrRange){4u, 1u};
    f->operands[0] = 1u; f->operands[1] = 2u; f->operands[2] = 3u;
    f->operands[3] = f->operands[4] = 1u; f->results[0] = 3u;
    f->signedValues[0] = 1u; f->signedValues[1] = 2u;
    f->entries[0].valueId = 2u;
    f->entries[0].fact.signedInteger = ZR_TRUE;
    f->entries[0].fact.hasLower = f->entries[0].fact.hasUpper = ZR_TRUE;
    f->entries[0].fact.lower = f->entries[0].fact.upper = threshold;
    f->input.revision = 7u; f->input.generation = 9u;
    f->input.signedValues = f->signedValues; f->input.signedValueCount = 2u;
    f->input.entries = f->entries; f->input.entryCount = 1u;
    ZrParser_ExecIr_BranchFactsInit(&f->facts);
}

static TZrBool analyze(SFixture *f) {
    TZrBool ok = ZrParser_ExecIr_AnalyzeBranchFacts(&f->module, &f->function,
            &f->input, &f->facts, &f->diagnostic);
    if (!ok) fprintf(stderr,
            "ANALYZE [%s] code=%u block=%u instruction=%u source=%u expected=%u actual=%u\n",
            caseName, (unsigned)f->diagnostic.code, (unsigned)f->diagnostic.blockId,
            (unsigned)f->diagnostic.instructionId, (unsigned)f->diagnostic.sourceId,
            (unsigned)f->diagnostic.expectedVersion, (unsigned)f->diagnostic.actualVersion);
    return ok;
}

static const SZrExecIrBranchValueFact *edge(SFixture *f, unsigned ordinal) {
    return ZrParser_ExecIr_BranchFactsAtEdge(&f->facts, 1u, ordinal, 1u);
}

static void define_threshold(SFixture *f, EZrExecIrOpcode opcode) {
    f->instructions[4] = f->instructions[3];
    f->instructions[3] = f->instructions[2];
    f->instructions[2] = f->instructions[1];
    f->instructions[1] = f->instructions[0];
    memset(&f->instructions[0], 0, sizeof(f->instructions[0]));
    f->instructions[0].opcode = (TZrUInt16)opcode;
    f->instructions[0].layoutId = 10u;
    f->instructions[0].results = (SZrExecIrRange){0u, 1u};
    f->instructions[1].results = (SZrExecIrRange){1u, 1u};
    f->results[0] = 2u; f->results[1] = 3u;
    f->function.resultCount = f->function.resultCapacity = 2u;
    f->values[1].flags = 0u; f->values[1].definition = 1u;
    f->values[2].definition = 2u;
    f->function.instructionCount = f->function.instructionCapacity = 5u;
    f->blocks[0].instructions.count = 3u; f->blocks[0].terminatorInstructionId = 3u;
    for (unsigned i = 1u; i < 3u; ++i) {
        ++f->blocks[i].instructions.offset; ++f->blocks[i].terminatorInstructionId;
    }
    f->input.entryCount = 0u;
}

static void predicate(unsigned selector) {
    SFixture f;
    const SZrExecIrBranchValueFact *a, *b;
    init(&f, selector, 10);
    REQUIRE(analyze(&f));
    a = edge(&f, 0u); b = edge(&f, 1u);
    REQUIRE(a != ZR_NULL && b != ZR_NULL);
    if (selector == 0u || selector == 5u) {
        const SZrExecIrBranchValueFact *equal = selector == 0u ? a : b;
        const SZrExecIrBranchValueFact *unequal = selector == 0u ? b : a;
        REQUIRE(equal->hasLower && equal->hasUpper && equal->lower == 10 && equal->upper == 10);
        REQUIRE(!unequal->hasLower && !unequal->hasUpper);
    } else if (selector == 1u || selector == 2u) {
        REQUIRE(a->hasUpper && a->upper == (selector == 1u ? 9 : 10));
        REQUIRE(b->hasLower && b->lower == (selector == 1u ? 10 : 11));
    } else {
        REQUIRE(a->hasLower && a->lower == (selector == 3u ? 11 : 10));
        REQUIRE(b->hasUpper && b->upper == (selector == 3u ? 10 : 9));
    }
    ZrParser_ExecIr_BranchFactsFree(&f.facts);
}

static void other(unsigned n) {
    SFixture f;
    const SZrExecIrBranchValueFact *a;
    init(&f, 1u, 10);
    switch (n) {
    case 0: /* Swapped threshold operand. */
        f.operands[0] = 2u; f.operands[1] = 1u;
        REQUIRE(analyze(&f)); a = edge(&f, 0u);
        REQUIRE(a && a->hasLower && a->lower == 11); break;
    case 1: case 2: case 3: case 4: /* Exact signed extrema. */
        f.instructions[0].typeToken = n == 1u ? 1u : n == 2u ? 3u : n == 3u ? 2u : 4u;
        f.entries[0].fact.lower = f.entries[0].fact.upper = (n == 1u || n == 4u) ? INT64_MIN : INT64_MAX;
        REQUIRE(analyze(&f));
        REQUIRE(edge(&f, n <= 2u ? 0u : 1u) == ZR_NULL);
        a = edge(&f, n <= 2u ? 1u : 0u); REQUIRE(a && a->signedInteger); break;
    case 5: /* Naked numeric identity does not establish signed domain. */
        f.input.signedValueCount = 0u; f.values[0].typeToken = 8u;
        REQUIRE(analyze(&f)); a = edge(&f, 0u);
        REQUIRE(a && !a->signedInteger && !a->hasUpper); break;
    case 6: /* Null-looking identity does not establish nullability. */
        f.input.signedValueCount = 0u; f.values[0].typeToken = 0u;
        REQUIRE(analyze(&f)); a = edge(&f, 0u);
        REQUIRE(a && a->nullState == ZR_EXEC_IR_NULL_FACT_UNKNOWN); break;
    case 7: /* A mixed/floating operand cannot narrow the signed one. */
        f.entries[0].fact.signedInteger = ZR_FALSE;
        f.entries[0].fact.hasLower = f.entries[0].fact.hasUpper = ZR_FALSE;
        f.input.signedValueCount = 1u;
        REQUIRE(analyze(&f)); a = edge(&f, 0u); REQUIRE(a && !a->hasUpper); break;
    case 8: /* Unsupported selector. */
        f.instructions[0].typeToken = 99u;
        REQUIRE(analyze(&f)); a = edge(&f, 0u); REQUIRE(a && !a->hasUpper); break;
    case 9: /* NULLABLE is not NULL. */
        f.values[0].nullability = ZR_EXEC_IR_NULLABILITY_NULLABLE;
        REQUIRE(analyze(&f)); a = edge(&f, 0u);
        REQUIRE(a && a->nullState == ZR_EXEC_IR_NULL_FACT_UNKNOWN); break;
    case 10: /* Explicit external NONNULL is usable. */
        f.values[0].nullability = ZR_EXEC_IR_NULLABILITY_NONNULL;
        REQUIRE(analyze(&f)); a = edge(&f, 0u);
        REQUIRE(a && a->nullState == ZR_EXEC_IR_NULL_FACT_NONNULL); break;
    case 11: /* Current result, independent revision/generation rejection. */
        REQUIRE(analyze(&f));
        REQUIRE(ZrParser_ExecIr_BranchFactsIsCurrent(&f.module, &f.function, &f.facts, 7u, 9u));
        REQUIRE(!ZrParser_ExecIr_BranchFactsIsCurrent(&f.module, &f.function, &f.facts, 8u, 9u));
        REQUIRE(!ZrParser_ExecIr_BranchFactsIsCurrent(&f.module, &f.function, &f.facts, 7u, 10u)); break;
    case 12: /* Mutation at unchanged revision invalidates fingerprint. */
        REQUIRE(analyze(&f)); f.instructions[0].typeToken = 3u;
        REQUIRE(!ZrParser_ExecIr_BranchFactsIsCurrent(&f.module, &f.function, &f.facts, 7u, 9u)); break;
    case 13: /* Module binding cannot be forged by a matching function id. */
        f.module.functionCount = 0u;
        REQUIRE(!analyze(&f)); REQUIRE(f.diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT); break;
    case 14: /* Invalid operand has IR position. */
        f.operands[0] = 999u;
        REQUIRE(!analyze(&f)); REQUIRE(f.diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE);
        REQUIRE(f.diagnostic.instructionId == 1u); break;
    case 15: /* Invalid successor has block position. */
        f.successors[0] = 99u;
        REQUIRE(!analyze(&f)); REQUIRE(f.diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK);
        REQUIRE(f.diagnostic.blockId == 1u); break;
    case 16: /* Invalid operand range has IR position. */
        f.instructions[0].operands.count = UINT32_MAX;
        REQUIRE(!analyze(&f)); REQUIRE(f.diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE);
        REQUIRE(f.diagnostic.instructionId == 1u); break;
    case 17: /* Seed bounds cannot make a local compare available at entry. */
        f.entries[0].valueId = 3u;
        REQUIRE(!analyze(&f)); REQUIRE(f.diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE); break;
    case 18: /* Witness storage is copied. */
        REQUIRE(analyze(&f)); f.signedValues[0] = 999u; f.entries[0].fact.upper = 100;
        a = edge(&f, 0u); REQUIRE(a && a->upper == 9);
        REQUIRE(f.facts.entryWitnesses && f.facts.entryWitnesses[0].fact.upper == 10); break;
    case 19: /* Failed rebuild retires old proof. */
        REQUIRE(analyze(&f)); f.operands[0] = 999u;
        REQUIRE(!analyze(&f)); REQUIRE(f.facts.blockEntries == ZR_NULL && f.facts.function == ZR_NULL); break;
    case 20: /* Rebuild really replaces the threshold. */
        REQUIRE(analyze(&f)); f.entries[0].fact.lower = f.entries[0].fact.upper = 20;
        ++f.input.generation; REQUIRE(analyze(&f));
        a = edge(&f, 0u); REQUIRE(a && a->upper == 19); break;
    case 21: /* Local definitions unavailable at block entry. */
        REQUIRE(analyze(&f));
        REQUIRE(ZrParser_ExecIr_BranchFactsAtBlockEntry(&f.facts, 1u, 3u) == ZR_NULL); break;
    case 22: /* Impossible entry interval is rejected. */
        f.entries[0].fact.lower = 11;
        REQUIRE(!analyze(&f)); REQUIRE(f.diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE); break;
    case 23: /* Signed integer domain alone is not a null proof. */
        REQUIRE(analyze(&f)); a = edge(&f, 0u);
        REQUIRE(a && a->signedInteger && a->nullState == ZR_EXEC_IR_NULL_FACT_UNKNOWN); break;
    case 24: /* Pooled bits and numeric token coincidence are opaque. */
        define_threshold(&f, ZR_EXEC_IR_OPCODE_CONSTANT);
        f.module.constants = &f.constant; f.module.constantCount = f.module.constantCapacity = 1u;
        f.constant.typeToken = 8u; f.constant.bits = 10u;
        f.instructions[0].layoutId = 0u;
        REQUIRE(analyze(&f)); a = edge(&f, 0u);
        REQUIRE(a && !a->hasUpper); break;
    case 25: /* No-pool opcode does not prove signed representation. */
        define_threshold(&f, ZR_EXEC_IR_OPCODE_CONSTANT);
        f.input.signedValueCount = 1u;
        REQUIRE(analyze(&f)); a = edge(&f, 0u);
        REQUIRE(a && !a->hasUpper); break;
    case 26: /* Explicit domain witness enables exact immediate threshold. */
        define_threshold(&f, ZR_EXEC_IR_OPCODE_CONSTANT);
        REQUIRE(analyze(&f)); a = edge(&f, 0u);
        REQUIRE(a && a->hasUpper && a->upper == 9); break;
    case 27: case 28: /* Copy/move preserve explicit nullability. */
        define_threshold(&f, n == 27u ? ZR_EXEC_IR_OPCODE_COPY : ZR_EXEC_IR_OPCODE_MOVE);
        f.instructions[0].operands = (SZrExecIrRange){5u, 1u};
        f.operands[5] = 1u; f.function.operandCount = f.function.operandCapacity = 6u;
        f.values[0].nullability = ZR_EXEC_IR_NULLABILITY_NONNULL;
        REQUIRE(analyze(&f));
        a = ZrParser_ExecIr_BranchFactsAtEdge(&f.facts, 1u, 0u, 2u);
        REQUIRE(a && a->nullState == ZR_EXEC_IR_NULL_FACT_NONNULL); break;
    case 29: /* Local domain witness cannot create entry availability. */
        define_threshold(&f, ZR_EXEC_IR_OPCODE_CONSTANT);
        REQUIRE(analyze(&f));
        REQUIRE(ZrParser_ExecIr_BranchFactsAtBlockEntry(&f.facts, 1u, 2u) == ZR_NULL); break;
    case 30: case 31: /* An excluded endpoint can shrink a convex interval. */
        f.instructions[0].typeToken = 5u;
        f.entries[1].valueId = 1u; f.entries[1].fact.signedInteger = ZR_TRUE;
        f.entries[1].fact.hasLower = f.entries[1].fact.hasUpper = ZR_TRUE;
        f.entries[1].fact.lower = n == 30u ? 10 : 0;
        f.entries[1].fact.upper = n == 30u ? 20 : 10; f.input.entryCount = 2u;
        REQUIRE(analyze(&f)); a = edge(&f, 0u);
        REQUIRE(a && (n == 30u ? a->lower == 11 : a->upper == 9)); break;
    case 32: /* An overflow witness forbids narrowing even a signed domain. */
        f.entries[1].valueId = 1u; f.entries[1].fact.signedInteger = ZR_TRUE;
        f.entries[1].fact.overflowed = ZR_TRUE; f.input.entryCount = 2u;
        REQUIRE(analyze(&f)); a = edge(&f, 0u); REQUIRE(a && a->overflowed && !a->hasUpper); break;
    case 33: /* Arithmetic does not inherit exact operand bounds. */
        define_threshold(&f, ZR_EXEC_IR_OPCODE_ADD);
        f.instructions[0].operands = (SZrExecIrRange){5u, 2u};
        f.operands[5] = f.operands[6] = 1u;
        f.function.operandCount = f.function.operandCapacity = 7u;
        REQUIRE(analyze(&f)); a = edge(&f, 0u); REQUIRE(a && !a->hasUpper); break;
    case 34: /* Null evidence cannot contradict explicit NONNULL metadata. */
        f.values[1].nullability = ZR_EXEC_IR_NULLABILITY_NONNULL;
        f.entries[0].fact.nullState = ZR_EXEC_IR_NULL_FACT_NULL;
        REQUIRE(!analyze(&f)); REQUIRE(f.diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE); break;
    case 35: /* Pool changes at unchanged IR revision reject currentness. */
        f.module.constants = &f.constant; f.module.constantCount = f.module.constantCapacity = 1u;
        REQUIRE(analyze(&f)); f.constant.bits = 987u;
        REQUIRE(!ZrParser_ExecIr_BranchFactsIsCurrent(&f.module, &f.function, &f.facts, 7u, 9u)); break;
    }
    ZrParser_ExecIr_BranchFactsFree(&f.facts);
    ZrParser_ExecIr_BranchFactsFree(&f.facts);
}

/* Builders keep storage ownership and definition positions explicit. */
static void graph_begin(SFixture *f, unsigned blocks, unsigned values) {
    init(f, 1u, 10);
    memset(f->instructions, 0, sizeof(f->instructions));
    memset(f->blocks, 0, sizeof(f->blocks));
    memset(f->values, 0, sizeof(f->values));
    f->function.instructionCount = f->function.instructionCapacity = 0u;
    f->function.operandCount = f->function.operandCapacity = 0u;
    f->function.resultCount = f->function.resultCapacity = 0u;
    f->function.successorCount = f->function.successorCapacity = 0u;
    f->function.predecessorCount = f->function.predecessorCapacity = 0u;
    f->function.blockCount = f->function.blockCapacity = blocks;
    f->function.valueCount = f->function.valueCapacity = values;
    for (unsigned i = 0u; i < blocks; ++i) {
        f->blocks[i].id = i + 1u;
        if (i == 0u) f->blocks[i].flags = ZR_EXEC_IR_BLOCK_FLAG_ENTRY;
    }
    for (unsigned i = 0u; i < values; ++i) {
        f->values[i].id = i + 1u; f->values[i].typeToken = 0xCAB000u + i;
    }
    f->values[0].flags = f->values[1].flags = ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY;
}

static void op(SFixture *f, unsigned block, EZrExecIrOpcode opcode,
        unsigned result, unsigned a, unsigned b, unsigned selector) {
    unsigned i = f->function.instructionCount++;
    SZrExecIrBlock *bb = &f->blocks[block - 1u];
    SZrExecIrInstruction *ii = &f->instructions[i];
    if (bb->instructions.count == 0u) bb->instructions.offset = i;
    ++bb->instructions.count; bb->terminatorInstructionId = i + 1u;
    ii->opcode = (TZrUInt16)opcode; ii->typeToken = selector;
    ii->operands.offset = f->function.operandCount;
    if (a) { f->operands[f->function.operandCount++] = a; ++ii->operands.count; }
    if (b) { f->operands[f->function.operandCount++] = b; ++ii->operands.count; }
    ii->results.offset = f->function.resultCount;
    if (result) {
        f->results[f->function.resultCount++] = result; ii->results.count = 1u;
        f->values[result - 1u].definition = i + 1u;
    }
    f->function.instructionCapacity = f->function.instructionCount;
    f->function.operandCapacity = f->function.operandCount;
    f->function.resultCapacity = f->function.resultCount;
}

static void links(SFixture *f, unsigned block, unsigned a, unsigned b) {
    SZrExecIrBlock *bb = &f->blocks[block - 1u];
    bb->successors.offset = f->function.successorCount;
    if (a) { f->successors[f->function.successorCount++] = a; ++bb->successors.count; }
    if (b) { f->successors[f->function.successorCount++] = b; ++bb->successors.count; }
    f->instructions[bb->terminatorInstructionId - 1u].successorRange = bb->successors;
    f->function.successorCapacity = f->function.successorCount;
}

static void predecessors(SFixture *f) {
    f->function.predecessorCount = 0u;
    for (unsigned target = 1u; target <= f->function.blockCount; ++target) {
        SZrExecIrBlock *b = &f->blocks[target - 1u];
        b->predecessors.offset = f->function.predecessorCount;
        for (unsigned pred = 1u; pred <= f->function.blockCount; ++pred) {
            SZrExecIrRange range = f->blocks[pred - 1u].successors;
            for (unsigned j = 0u; j < range.count; ++j)
                if (f->successors[range.offset + j] == target) {
                    f->predecessors[f->function.predecessorCount++] = pred;
                    ++b->predecessors.count;
                }
        }
    }
    f->function.predecessorCapacity = f->function.predecessorCount;
}

static void diamond(SFixture *f) {
    graph_begin(f, 4u, 7u);
    f->values[6].flags = ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY;
    op(f, 1u, ZR_EXEC_IR_OPCODE_COMPARE, 3u, 1u, 2u, 1u);
    op(f, 1u, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH, 0u, 3u, 0u, 0u);
    op(f, 2u, ZR_EXEC_IR_OPCODE_COPY, 4u, 2u, 0u, 0u);
    op(f, 2u, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u, 0u);
    op(f, 3u, ZR_EXEC_IR_OPCODE_COPY, 5u, 7u, 0u, 0u);
    op(f, 3u, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u, 0u);
    op(f, 4u, ZR_EXEC_IR_OPCODE_RETURN, 0u, 6u, 0u, 0u);
    links(f, 1u, 2u, 3u); links(f, 2u, 4u, 0u); links(f, 3u, 4u, 0u);
    predecessors(f);
    f->function.phiPool = f->phis; f->function.phiCount = f->function.phiCapacity = 1u;
    f->function.phiIncoming = f->incoming;
    f->function.phiIncomingCount = f->function.phiIncomingCapacity = 2u;
    f->blocks[3].phis = (SZrExecIrRange){0u, 1u};
    f->phis[0].result = 6u; f->phis[0].incomings = (SZrExecIrRange){0u, 2u};
    f->incoming[0] = (SZrExecIrPhiIncoming){2u, 4u};
    f->incoming[1] = (SZrExecIrPhiIncoming){3u, 5u};
    f->entries[1] = f->entries[0]; f->entries[1].valueId = 7u;
    f->entries[1].fact.lower = f->entries[1].fact.upper = 20;
    f->input.entryCount = 2u;
}

static void graphs(unsigned n) {
    SFixture f;
    const SZrExecIrBranchValueFact *a, *b;
    if (n < 5u) {
        diamond(&f);
        if (n == 1u) { f.input.entryCount = 1u; } /* UNKNOWN right incoming. */
        if (n == 2u || n == 3u) {
            f.values[1].nullability = ZR_EXEC_IR_NULLABILITY_NONNULL;
            if (n == 3u) f.values[6].nullability = ZR_EXEC_IR_NULLABILITY_NONNULL;
        }
        if (n == 4u) { /* x is always 0, so only left PHI input is reachable. */
            f.entries[2].valueId = 1u;
            f.entries[2].fact.signedInteger = ZR_TRUE;
            f.entries[2].fact.hasLower = f.entries[2].fact.hasUpper = ZR_TRUE;
            f.entries[2].fact.lower = f.entries[2].fact.upper = 0;
            f.input.entryCount = 3u;
        }
        REQUIRE(analyze(&f));
        a = ZrParser_ExecIr_BranchFactsAtBlockEntry(&f.facts, 4u, 6u);
        REQUIRE(a);
        if (n == 0u) REQUIRE(a->hasLower && a->hasUpper && a->lower == 10 && a->upper == 20);
        if (n == 1u) REQUIRE(!a->signedInteger && !a->hasLower && !a->hasUpper);
        if (n == 2u) REQUIRE(a->nullState == ZR_EXEC_IR_NULL_FACT_UNKNOWN);
        if (n == 3u) REQUIRE(a->nullState == ZR_EXEC_IR_NULL_FACT_NONNULL);
        if (n == 4u) {
            REQUIRE(a->hasLower && a->hasUpper && a->lower == 10 && a->upper == 10);
            REQUIRE(ZrParser_ExecIr_BranchFactsAtBlockEntry(&f.facts, 3u, 1u) == ZR_NULL);
        }
        b = ZrParser_ExecIr_BranchFactsAtBlockEntry(&f.facts, 4u, 4u);
        if (n != 4u) REQUIRE(b == ZR_NULL); /* Left local does not dominate join. */
    } else if (n == 5u) {
        init(&f, 1u, 10);
        f.function.blockCount = f.function.blockCapacity = 2u;
        f.function.instructionCount = f.function.instructionCapacity = 3u;
        f.function.operandCount = f.function.operandCapacity = 4u;
        f.successors[1] = 2u; f.blocks[1].predecessors.count = 2u;
        REQUIRE(analyze(&f)); a = edge(&f, 0u); b = edge(&f, 1u);
        REQUIRE(a && b && a->hasUpper && a->upper == 9 && b->hasLower && b->lower == 10);
        a = ZrParser_ExecIr_BranchFactsAtBlockEntry(&f.facts, 2u, 1u);
        REQUIRE(a && !a->hasLower && !a->hasUpper);
    } else if (n == 6u || n == 7u) {
        graph_begin(&f, n == 6u ? 2u : 4u, 3u);
        op(&f, 1u, ZR_EXEC_IR_OPCODE_COMPARE, 3u, 1u, 2u, 1u);
        op(&f, 1u, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH, 0u, 3u, 0u, 0u);
        op(&f, 2u, ZR_EXEC_IR_OPCODE_RETURN, 0u, 1u, 0u, 0u);
        if (n == 6u) {
            /* Entry backedge, reachable cycle: all proof domains forgotten. */
            links(&f, 1u, 1u, 2u);
        } else {
            op(&f, 3u, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u, 0u);
            op(&f, 4u, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u, 0u);
            links(&f, 1u, 2u, 2u); links(&f, 3u, 4u, 0u); links(&f, 4u, 3u, 0u);
        }
        predecessors(&f); REQUIRE(analyze(&f));
        a = edge(&f, 0u); REQUIRE(a);
        if (n == 6u) REQUIRE(f.facts.cyclicFallback && !a->signedInteger && !a->hasUpper);
        else {
            REQUIRE(!f.facts.cyclicFallback && a->hasUpper && a->upper == 9);
            REQUIRE(ZrParser_ExecIr_BranchFactsAtBlockEntry(&f.facts, 3u, 1u) == ZR_NULL);
        }
    } else { /* Nested conditions, then contradictory lower bound. */
        graph_begin(&f, 5u, 5u);
        f.values[3].flags = ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY;
        f.entries[1] = f.entries[0]; f.entries[1].valueId = 4u;
        f.entries[1].fact.lower = f.entries[1].fact.upper = 0;
        f.input.entryCount = 2u;
        op(&f, 1u, ZR_EXEC_IR_OPCODE_COMPARE, 3u, 1u, 4u, 4u);
        op(&f, 1u, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH, 0u, 3u, 0u, 0u);
        op(&f, 2u, ZR_EXEC_IR_OPCODE_COMPARE, 5u, 1u, 2u, 1u);
        op(&f, 2u, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH, 0u, 5u, 0u, 0u);
        op(&f, 3u, ZR_EXEC_IR_OPCODE_RETURN, 0u, 1u, 0u, 0u);
        op(&f, 4u, ZR_EXEC_IR_OPCODE_RETURN, 0u, 1u, 0u, 0u);
        op(&f, 5u, ZR_EXEC_IR_OPCODE_RETURN, 0u, 1u, 0u, 0u);
        links(&f, 1u, 2u, 3u); links(&f, 2u, 4u, 5u); predecessors(&f);
        if (n == 9u) f.entries[0].fact.lower = f.entries[0].fact.upper = -1;
        REQUIRE(analyze(&f));
        a = ZrParser_ExecIr_BranchFactsAtEdge(&f.facts, 2u, 0u, 1u);
        if (n == 8u) REQUIRE(a && a->hasLower && a->hasUpper && a->lower == 0 && a->upper == 9);
        else {
            REQUIRE(a == ZR_NULL);
            b = ZrParser_ExecIr_BranchFactsAtEdge(&f.facts, 2u, 1u, 1u);
            REQUIRE(b && b->hasLower && b->lower == 0);
        }
    }
    ZrParser_ExecIr_BranchFactsFree(&f.facts);
}

static void long_dag(void) {
    enum { BLOCKS = 128 };
    SZrExecIrModule m = {0}; SZrExecIrFunction f = {0};
    SZrExecIrBlock blocks[BLOCKS] = {0};
    SZrExecIrInstruction instructions[BLOCKS] = {0};
    TZrExecIrBlockId next[BLOCKS - 1], prev[BLOCKS - 1];
    SZrExecIrValue v = {0}; TZrExecIrValueId operand = 1u;
    SZrExecIrBranchEntryFact seed = {0};
    SZrExecIrBranchFactsInput in = {0}; SZrExecIrBranchFacts result = {0};
    SZrExecIrDiagnostic d = {0};
    const SZrExecIrBranchValueFact *a;
    m.id = f.id = f.entryBlockId = 1u; m.functions = &f;
    f.functionToken = 1u;
    m.functionCount = m.functionCapacity = 1u;
    v.id = 1u; v.typeToken = 0xFFA001u; v.flags = ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY;
    f.values = &v; f.valueCount = f.valueCapacity = 1u;
    f.blocks = blocks; f.blockCount = f.blockCapacity = BLOCKS;
    f.instructions = instructions; f.instructionCount = f.instructionCapacity = BLOCKS;
    f.successors = next; f.successorCount = f.successorCapacity = BLOCKS - 1u;
    f.predecessors = prev; f.predecessorCount = f.predecessorCapacity = BLOCKS - 1u;
    f.operands = &operand; f.operandCount = f.operandCapacity = 1u;
    for (unsigned i = 0u; i < BLOCKS; ++i) {
        blocks[i].id = i + 1u; blocks[i].instructions = (SZrExecIrRange){i, 1u};
        blocks[i].terminatorInstructionId = i + 1u;
        if (i == 0u) blocks[i].flags = ZR_EXEC_IR_BLOCK_FLAG_ENTRY;
        if (i) { blocks[i].predecessors = (SZrExecIrRange){i - 1u, 1u}; prev[i - 1u] = i; }
        if (i + 1u < BLOCKS) {
            blocks[i].successors = (SZrExecIrRange){i, 1u}; next[i] = i + 2u;
            instructions[i].opcode = ZR_EXEC_IR_OPCODE_BRANCH;
            instructions[i].successorRange = blocks[i].successors;
        } else {
            instructions[i].opcode = ZR_EXEC_IR_OPCODE_RETURN;
            instructions[i].operands = (SZrExecIrRange){0u, 1u};
        }
    }
    seed.valueId = 1u; seed.fact.signedInteger = ZR_TRUE;
    seed.fact.hasLower = seed.fact.hasUpper = ZR_TRUE; seed.fact.lower = 0; seed.fact.upper = 3;
    in.revision = in.generation = 1u; in.entries = &seed; in.entryCount = 1u;
    REQUIRE(ZrParser_ExecIr_AnalyzeBranchFacts(&m, &f, &in, &result, &d));
    a = ZrParser_ExecIr_BranchFactsAtBlockEntry(&result, BLOCKS, 1u);
    REQUIRE(a && a->hasLower && a->hasUpper && a->lower == 0 && a->upper == 3);
    REQUIRE(ZrParser_ExecIr_BranchFactsIsCurrent(&m, &f, &result, 1u, 1u));
    ZrParser_ExecIr_BranchFactsFree(&result);
}

int main(void) {
    static const char *predicates[] = {"eq", "lt", "le", "gt", "ge", "ne"};
    static const char *others[] = {"swapped", "min-lt", "max-gt", "max-le", "min-ge",
        "opaque-sign", "opaque-null", "mixed-domain", "unsupported-compare", "nullable",
        "nonnull", "revision-generation", "changed-ir", "module-binding", "bad-value",
        "bad-block", "bad-range", "local-seed", "owned-witnesses", "failed-rebuild",
        "successful-rebuild", "definition-liveness", "bad-interval", "sign-is-not-null",
        "opaque-pool", "opaque-immediate", "proved-immediate", "copy-nonnull",
        "move-nonnull", "local-domain-liveness", "neq-lower-endpoint", "neq-upper-endpoint",
        "overflow-witness", "arithmetic-conservative", "contradictory-null", "changed-pool"};
    for (unsigned i = 0u; i < 6u; ++i) { caseName = predicates[i]; ++cases; predicate(i); }
    for (unsigned i = 0u; i < 36u; ++i) { caseName = others[i]; ++cases; other(i); }
    static const char *graphNames[] = {"phi-hull", "phi-unknown", "phi-null-mixed",
        "phi-null-agree", "phi-unreachable", "parallel-edge-join", "reachable-cycle",
        "unreachable-cycle", "nested-interval", "contradictory-edge"};
    for (unsigned i = 0u; i < 10u; ++i) { caseName = graphNames[i]; ++cases; graphs(i); }
    caseName = "long-dag"; ++cases; long_dag();
    printf("branch range/null: %u cases, %u failures\n", cases, failures);
    return failures == 0u ? EXIT_SUCCESS : EXIT_FAILURE;
}
