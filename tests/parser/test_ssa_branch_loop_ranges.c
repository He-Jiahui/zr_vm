#include "zr_vm_parser/exec_ir_branch_facts.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Fixture-owned identities. All number formats come from entry witnesses. */
enum { V_INITIAL = 1, V_STEP = 2, V_LIMIT = 3, V_INDEX = 4,
       V_NEXT = 5, V_CONDITION = 6, V_EXTRA = 7, V_INNER_NEXT = 8,
       V_INNER_CONDITION = 9, B_ENTRY = 1, B_HEADER = 2, B_BODY = 3,
       B_EXIT = 4, MAX_VALUES = 9, MAX_BLOCKS = 6, MAX_INSTRUCTIONS = 12,
       MAX_OPERANDS = 32, MAX_RESULTS = 8, MAX_EDGES = 12, MAX_PHIS = 2,
       MAX_INCOMING = 4, ENTRY_SEEDS = 3 };
enum ECase { C_UP, C_ZERO, C_DESC_ADD, C_DESC_SUB, C_MAX_SAFE,
             C_MAX_OVERFLOW, C_MISSING_STEP, C_POISON, C_TWO_ENTRIES,
             C_NESTED, C_MALFORMED_PHI };

typedef struct SFixture {
    SZrExecIrModule module;
    SZrExecIrFunction function;
    SZrExecIrValue values[MAX_VALUES];
    SZrExecIrInstruction instructions[MAX_INSTRUCTIONS];
    SZrExecIrBlock blocks[MAX_BLOCKS];
    TZrExecIrValueId operands[MAX_OPERANDS], results[MAX_RESULTS], signedValues[MAX_VALUES];
    TZrExecIrBlockId successors[MAX_EDGES], predecessors[MAX_EDGES];
    SZrExecIrPhi phis[MAX_PHIS];
    SZrExecIrPhiIncoming incoming[MAX_INCOMING];
    SZrExecIrBranchEntryFact entries[ENTRY_SEEDS];
    SZrExecIrBranchFactsInput input;
    SZrExecIrBranchFacts facts;
    SZrExecIrDiagnostic diagnostic;
} SFixture;

static unsigned cases, failures, precondition_failures;
static const char *case_name;
#define EXPECT(c) do { if (!(c)) { \
    fprintf(stderr, "SEMANTIC FAIL [%s] line=%u: %s\n", case_name, (unsigned)__LINE__, #c); \
    return ZR_FALSE; } } while (0)

static void seed(SZrExecIrBranchEntryFact *entry, TZrExecIrValueId id, TZrInt64 value) {
    entry->valueId = id; entry->fact.available = ZR_TRUE;
    entry->fact.signedInteger = ZR_TRUE;
    entry->fact.hasLower = entry->fact.hasUpper = ZR_TRUE;
    entry->fact.lower = entry->fact.upper = value;
}
static void init(SFixture *f, TZrUInt32 block_count, TZrUInt32 value_count,
                 TZrInt64 initial, TZrInt64 step, TZrInt64 limit) {
    memset(f, 0, sizeof(*f));
    f->module.id = 1u; f->module.functions = &f->function;
    f->module.functionCount = f->module.functionCapacity = 1u;
    f->function.id = 1u; f->function.functionToken = 1u;
    f->function.entryBlockId = B_ENTRY;
    f->function.values = f->values;
    f->function.valueCount = f->function.valueCapacity = value_count;
    f->function.blocks = f->blocks;
    f->function.blockCount = f->function.blockCapacity = block_count;
    f->function.instructions = f->instructions;
    f->function.operands = f->operands; f->function.results = f->results;
    f->function.successors = f->successors; f->function.predecessors = f->predecessors;
    f->function.phiPool = f->phis; f->function.phiIncoming = f->incoming;
    for (TZrUInt32 i = 0u; i < value_count; ++i) {
        f->values[i].id = i + 1u;
        f->values[i].typeToken = 0xCAB000u + i; /* Opaque, not an integer format. */
        if (i < ENTRY_SEEDS) f->values[i].flags = ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY;
    }
    for (TZrUInt32 i = 0u; i < block_count; ++i) {
        f->blocks[i].id = i + 1u;
        if (i == 0u) f->blocks[i].flags = ZR_EXEC_IR_BLOCK_FLAG_ENTRY;
    }
    seed(&f->entries[0], V_INITIAL, initial);
    seed(&f->entries[1], V_STEP, step);
    seed(&f->entries[2], V_LIMIT, limit);
    f->signedValues[0] = V_INITIAL; f->signedValues[1] = V_STEP;
    f->signedValues[2] = V_LIMIT; f->signedValues[3] = V_INDEX;
    f->signedValues[4] = V_NEXT;
    f->input.revision = 7u; f->input.generation = 9u;
    f->input.signedValues = f->signedValues; f->input.signedValueCount = 5u;
    f->input.entries = f->entries; f->input.entryCount = ENTRY_SEEDS;
    ZrParser_ExecIr_BranchFactsInit(&f->facts);
}

static void op(SFixture *f, TZrExecIrBlockId block, EZrExecIrOpcode opcode,
               TZrExecIrValueId result, TZrExecIrValueId a,
               TZrExecIrValueId b, TZrUInt32 selector) {
    TZrUInt32 i = f->function.instructionCount++;
    SZrExecIrInstruction *instruction = &f->instructions[i];
    SZrExecIrBlock *container = &f->blocks[block - 1u];
    if (!container->instructions.count) container->instructions.offset = i;
    ++container->instructions.count; container->terminatorInstructionId = i + 1u;
    instruction->opcode = (TZrUInt16)opcode; instruction->typeToken = selector;
    instruction->sourceId = 101u + i;
    instruction->operands.offset = f->function.operandCount;
    if (a) { f->operands[f->function.operandCount++] = a; ++instruction->operands.count; }
    if (b) { f->operands[f->function.operandCount++] = b; ++instruction->operands.count; }
    instruction->results.offset = f->function.resultCount;
    if (result) {
        f->results[f->function.resultCount++] = result; instruction->results.count = 1u;
        f->values[result - 1u].definition = i + 1u;
    }
    f->function.instructionCapacity = f->function.instructionCount;
    f->function.operandCapacity = f->function.operandCount;
    f->function.resultCapacity = f->function.resultCount;
}
static void links(SFixture *f, TZrExecIrBlockId block,
                  TZrExecIrBlockId a, TZrExecIrBlockId b) {
    SZrExecIrBlock *container = &f->blocks[block - 1u];
    container->successors.offset = f->function.successorCount;
    if (a) { f->successors[f->function.successorCount++] = a; ++container->successors.count; }
    if (b) { f->successors[f->function.successorCount++] = b; ++container->successors.count; }
    f->instructions[container->terminatorInstructionId - 1u].successorRange = container->successors;
    f->function.successorCapacity = f->function.successorCount;
}
static void predecessors(SFixture *f) {
    for (TZrUInt32 target = 1u; target <= f->function.blockCount; ++target) {
        SZrExecIrBlock *container = &f->blocks[target - 1u];
        container->predecessors.offset = f->function.predecessorCount;
        for (TZrUInt32 pred = 1u; pred <= f->function.blockCount; ++pred) {
            SZrExecIrRange range = f->blocks[pred - 1u].successors;
            for (TZrUInt32 j = 0u; j < range.count; ++j) {
                if (f->successors[range.offset + j] == target) {
                    f->predecessors[f->function.predecessorCount++] = pred;
                    ++container->predecessors.count;
                }
            }
        }
    }
    f->function.predecessorCapacity = f->function.predecessorCount;
}
/* Incoming rows follow the exact ordered predecessor occurrences, matching
 * Core's PHI contract. A PHI result has definition=0 and no external flag. */
static void phi(SFixture *f, TZrExecIrBlockId block, TZrExecIrValueId result,
                TZrExecIrBlockId latch, TZrExecIrValueId back_value) {
    SZrExecIrBlock *container = &f->blocks[block - 1u];
    SZrExecIrPhi *p = &f->phis[f->function.phiCount];
    container->phis.offset = f->function.phiCount++; container->phis.count = 1u;
    p->result = result; p->incomings.offset = f->function.phiIncomingCount;
    p->incomings.count = container->predecessors.count;
    for (TZrUInt32 i = 0u; i < container->predecessors.count; ++i) {
        TZrExecIrBlockId pred = f->predecessors[container->predecessors.offset + i];
        SZrExecIrPhiIncoming *in = &f->incoming[f->function.phiIncomingCount++];
        in->predecessor = pred;
        in->value = pred == latch ? back_value : V_INITIAL;
    }
    f->function.phiCapacity = f->function.phiCount;
    f->function.phiIncomingCapacity = f->function.phiIncomingCount;
}

static void simple(SFixture *f, enum ECase kind) {
    TZrInt64 initial = 0, step = 1, limit = 10;
    TZrUInt32 selector = 1u;
    EZrExecIrOpcode update = ZR_EXEC_IR_OPCODE_ADD;
    TZrBool two_entries = (TZrBool)(kind == C_TWO_ENTRIES);
    if (kind == C_ZERO) limit = 0;
    if (kind == C_DESC_ADD || kind == C_DESC_SUB) {
        initial = 10; limit = 0; selector = 3u;
        if (kind == C_DESC_ADD) step = -1;
        else update = ZR_EXEC_IR_OPCODE_SUB;
    }
    if (kind == C_MAX_SAFE || kind == C_MAX_OVERFLOW) {
        initial = INT64_MAX - 1; limit = INT64_MAX;
        if (kind == C_MAX_OVERFLOW) selector = 2u;
    }
    init(f, two_entries ? 6u : 4u, two_entries ? 7u : 6u, initial, step, limit);
    if (two_entries) {
        f->values[V_EXTRA - 1u].flags = ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY;
        op(f, B_ENTRY, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH, 0u, V_EXTRA, 0u, 0u);
    } else op(f, B_ENTRY, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u, 0u);
    op(f, B_HEADER, ZR_EXEC_IR_OPCODE_COMPARE, V_CONDITION, V_INDEX, V_LIMIT, selector);
    op(f, B_HEADER, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH, 0u, V_CONDITION, 0u, 0u);
    op(f, B_BODY, update, V_NEXT, V_INDEX, V_STEP, 0u);
    op(f, B_BODY, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u, 0u);
    op(f, B_EXIT, ZR_EXEC_IR_OPCODE_RETURN, 0u, V_INDEX, 0u, 0u);
    if (two_entries) {
        op(f, 5u, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u, 0u);
        op(f, 6u, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u, 0u);
        links(f, B_ENTRY, 5u, 6u); links(f, 5u, B_HEADER, 0u); links(f, 6u, B_HEADER, 0u);
    } else links(f, B_ENTRY, B_HEADER, 0u);
    links(f, B_HEADER, B_BODY, B_EXIT); links(f, B_BODY, B_HEADER, 0u);
    predecessors(f); phi(f, B_HEADER, V_INDEX, B_BODY, V_NEXT);
    f->blocks[B_HEADER - 1u].immediateDominator = B_ENTRY;
    f->blocks[B_BODY - 1u].immediateDominator = B_HEADER;
    f->blocks[B_EXIT - 1u].immediateDominator = B_HEADER;
    if (two_entries) f->blocks[4].immediateDominator = f->blocks[5].immediateDominator = B_ENTRY;
    if (kind == C_MISSING_STEP) {
        f->entries[1].fact.signedInteger = ZR_FALSE;
        f->entries[1].fact.hasLower = f->entries[1].fact.hasUpper = ZR_FALSE;
        f->signedValues[1] = V_NEXT; f->input.signedValueCount = 4u;
    }
    if (kind == C_POISON) f->entries[0].fact.overflowed = ZR_TRUE;
}
static void nested(SFixture *f) {
    /* Outer: header2 -> inner3 -> innerbody4/back3 -> outerlatch5/back2;
     * outer false -> exit6. Both PHIs have real edge-local definitions. */
    init(f, 6u, 9u, 0, 1, 10);
    f->signedValues[5] = V_EXTRA; f->signedValues[6] = V_INNER_NEXT;
    f->input.signedValueCount = 7u;
    op(f, 1u, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u, 0u);
    op(f, 2u, ZR_EXEC_IR_OPCODE_COMPARE, V_CONDITION, V_INDEX, V_LIMIT, 1u);
    op(f, 2u, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH, 0u, V_CONDITION, 0u, 0u);
    op(f, 3u, ZR_EXEC_IR_OPCODE_COMPARE, V_INNER_CONDITION, V_EXTRA, V_LIMIT, 1u);
    op(f, 3u, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH, 0u, V_INNER_CONDITION, 0u, 0u);
    op(f, 4u, ZR_EXEC_IR_OPCODE_ADD, V_INNER_NEXT, V_EXTRA, V_STEP, 0u);
    op(f, 4u, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u, 0u);
    op(f, 5u, ZR_EXEC_IR_OPCODE_ADD, V_NEXT, V_INDEX, V_STEP, 0u);
    op(f, 5u, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u, 0u);
    op(f, 6u, ZR_EXEC_IR_OPCODE_RETURN, 0u, V_INDEX, 0u, 0u);
    links(f, 1u, 2u, 0u); links(f, 2u, 3u, 6u); links(f, 3u, 4u, 5u);
    links(f, 4u, 3u, 0u); links(f, 5u, 2u, 0u);
    predecessors(f); phi(f, 2u, V_INDEX, 5u, V_NEXT);
    phi(f, 3u, V_EXTRA, 4u, V_INNER_NEXT);
    f->blocks[1].immediateDominator = 1u; f->blocks[2].immediateDominator = 2u;
    f->blocks[3].immediateDominator = f->blocks[4].immediateDominator = 3u;
    f->blocks[5].immediateDominator = 2u;
}

static TZrBool precondition(SFixture *f, TZrBool ok, const char *stage) {
    printf("PRECONDITION %s %s %s\n", case_name, stage, ok ? "PASS" : "FAIL");
    if (!ok) {
        ++precondition_failures;
        fprintf(stderr, "PRECONDITION FAIL [%s] %s code=%u function=%llu block=%u instruction=%u source=%u\n",
                case_name, stage, (unsigned)f->diagnostic.code,
                (unsigned long long)f->diagnostic.functionToken, (unsigned)f->diagnostic.blockId,
                (unsigned)f->diagnostic.instructionId, (unsigned)f->diagnostic.sourceId);
    }
    return ok;
}
static TZrBool analyze(SFixture *f) {
    if (!precondition(f, ZrCore_ExecIr_VerifyFunction(&f->function,
                (EZrExecIrVerifyLevel)(ZR_EXEC_IR_VERIFY_STRUCTURE | ZR_EXEC_IR_VERIFY_SSA),
                &f->diagnostic), "CoreVerify")) return ZR_FALSE;
    if (!precondition(f, ZrParser_ExecIr_AnalyzeBranchFacts(&f->module, &f->function,
                &f->input, &f->facts, &f->diagnostic), "Analyze")) return ZR_FALSE;
    return precondition(f, ZrParser_ExecIr_BranchFactsIsCurrent(&f->module, &f->function,
                &f->facts, f->input.revision, f->input.generation), "Freshness");
}
static TZrBool interval(const SZrExecIrBranchValueFact *a, TZrInt64 lower, TZrInt64 upper) {
    return (TZrBool)(a && a->available && a->signedInteger && !a->overflowed &&
            a->hasLower && a->hasUpper && a->lower == lower && a->upper == upper);
}
static TZrBool no_numeric_proof(const SZrExecIrBranchValueFact *a) {
    return (TZrBool)(!a || !a->available || !a->signedInteger || a->overflowed ||
                    (!a->hasLower && !a->hasUpper));
}

static TZrBool run(SFixture *f, enum ECase kind) {
    SZrExecIrInstruction original_instructions[MAX_INSTRUCTIONS];
    SZrExecIrPhiIncoming original_incoming[MAX_INCOMING];
    const SZrExecIrBranchValueFact *header, *body, *exit, *update;
    if (kind == C_NESTED) nested(f); else simple(f, kind);
    memcpy(original_instructions, f->instructions, sizeof(original_instructions));
    memcpy(original_incoming, f->incoming, sizeof(original_incoming));
    if (!analyze(f)) return ZR_FALSE;
    EXPECT(memcmp(original_instructions, f->instructions, sizeof(original_instructions)) == 0);
    EXPECT(memcmp(original_incoming, f->incoming, sizeof(original_incoming)) == 0);
    if (kind == C_MALFORMED_PHI) {
        TZrExecIrInstructionId site = f->blocks[B_HEADER - 1u].terminatorInstructionId;
        TZrExecIrSourceId source = f->instructions[site - 1u].sourceId;
        /* A valid block identity which is not this PHI's backedge occurrence. */
        f->incoming[1].predecessor = B_EXIT;
        EXPECT(!ZrCore_ExecIr_VerifyFunction(&f->function,
                (EZrExecIrVerifyLevel)(ZR_EXEC_IR_VERIFY_STRUCTURE | ZR_EXEC_IR_VERIFY_SSA), &f->diagnostic));
        EXPECT(f->diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH);
        EXPECT(f->diagnostic.functionToken == f->function.functionToken);
        EXPECT(f->diagnostic.blockId == B_HEADER && f->diagnostic.instructionId == site);
        EXPECT(f->diagnostic.sourceId == source);
        EXPECT(!ZrParser_ExecIr_BranchFactsIsCurrent(&f->module, &f->function, &f->facts,
                f->input.revision, f->input.generation));
        EXPECT(!ZrParser_ExecIr_AnalyzeBranchFacts(&f->module, &f->function,
                &f->input, &f->facts, &f->diagnostic));
        EXPECT(f->diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH);
        EXPECT(f->diagnostic.blockId == B_HEADER && f->diagnostic.instructionId == site);
        EXPECT(f->diagnostic.sourceId == source);
        EXPECT(f->facts.blockEntries == ZR_NULL && f->facts.edges == ZR_NULL);
        printf("EXPECTED_DIAGNOSTIC %s PHI_PREDECESSOR_MISMATCH PASS\n", case_name);
        return ZR_TRUE;
    }
    if (kind == C_TWO_ENTRIES || kind == C_NESTED) {
        EXPECT(f->facts.cyclicFallback);
        for (TZrUInt32 b = 1u; b <= f->function.blockCount; ++b)
            EXPECT(no_numeric_proof(ZrParser_ExecIr_BranchFactsAtBlockEntry(&f->facts, b, V_INDEX)));
        return ZR_TRUE;
    }
    header = ZrParser_ExecIr_BranchFactsAtBlockEntry(&f->facts, B_HEADER, V_INDEX);
    body = ZrParser_ExecIr_BranchFactsAtEdge(&f->facts, B_HEADER, 0u, V_INDEX);
    exit = ZrParser_ExecIr_BranchFactsAtEdge(&f->facts, B_HEADER, 1u, V_INDEX);
    update = ZrParser_ExecIr_BranchFactsAtEdge(&f->facts, B_BODY, 0u, V_NEXT);
    if (kind == C_MAX_OVERFLOW || kind == C_MISSING_STEP || kind == C_POISON) {
        EXPECT(no_numeric_proof(header) && no_numeric_proof(body));
        EXPECT(no_numeric_proof(exit) && no_numeric_proof(update));
        return ZR_TRUE;
    }
    /* Actual feature counterexamples start here, after separately logged valid
     * CoreVerify, Analyze and Freshness preconditions. */
    EXPECT(!f->facts.cyclicFallback);
    EXPECT(ZrParser_ExecIr_BranchFactsAtBlockEntry(&f->facts, B_ENTRY, V_INDEX) == ZR_NULL);
    if (kind == C_ZERO) {
        EXPECT(interval(header, 0, 0)); EXPECT(interval(exit, 0, 0));
        EXPECT(body == ZR_NULL && update == ZR_NULL);
        EXPECT(ZrParser_ExecIr_BranchFactsAtBlockEntry(&f->facts, B_BODY, V_INDEX) == ZR_NULL);
        EXPECT(interval(ZrParser_ExecIr_BranchFactsAtBlockEntry(&f->facts, B_EXIT, V_INDEX), 0, 0));
    } else if (kind == C_MAX_SAFE) {
        EXPECT(interval(header, INT64_MAX - 1, INT64_MAX));
        EXPECT(interval(body, INT64_MAX - 1, INT64_MAX - 1));
        EXPECT(interval(update, INT64_MAX, INT64_MAX));
        EXPECT(interval(exit, INT64_MAX, INT64_MAX));
        EXPECT(interval(ZrParser_ExecIr_BranchFactsAtBlockEntry(&f->facts, B_EXIT, V_INDEX), INT64_MAX, INT64_MAX));
    } else {
        EXPECT(interval(header, 0, 10));
        if (kind == C_DESC_ADD || kind == C_DESC_SUB) {
            EXPECT(interval(body, 1, 10)); EXPECT(interval(update, 0, 9));
            EXPECT(interval(exit, 0, 0));
            EXPECT(interval(ZrParser_ExecIr_BranchFactsAtBlockEntry(&f->facts, B_BODY, V_INDEX), 1, 10));
            EXPECT(interval(ZrParser_ExecIr_BranchFactsAtBlockEntry(&f->facts, B_EXIT, V_INDEX), 0, 0));
        } else {
            EXPECT(interval(body, 0, 9)); EXPECT(interval(update, 1, 10));
            EXPECT(interval(exit, 10, 10));
            EXPECT(interval(ZrParser_ExecIr_BranchFactsAtBlockEntry(&f->facts, B_BODY, V_INDEX), 0, 9));
            EXPECT(interval(ZrParser_ExecIr_BranchFactsAtBlockEntry(&f->facts, B_EXIT, V_INDEX), 10, 10));
        }
    }
    return ZR_TRUE;
}

int main(void) {
    static const struct { const char *name; enum ECase kind; } matrix[] = {
        {"induction_0_to_10", C_UP}, {"zero_trip", C_ZERO},
        {"descending_add_10_to_0", C_DESC_ADD}, {"descending_sub_10_to_0", C_DESC_SUB},
        {"strict_max_safe", C_MAX_SAFE}, {"inclusive_max_overflow_conservative", C_MAX_OVERFLOW},
        {"missing_step_witness", C_MISSING_STEP}, {"poison_seed", C_POISON},
        {"two_header_entry_occurrences_conservative", C_TWO_ENTRIES},
        {"nested_loops_conservative", C_NESTED}, {"malformed_phi_diagnostic", C_MALFORMED_PHI}
    };
    for (size_t i = 0u; i < sizeof(matrix)/sizeof(matrix[0]); ++i) {
        SFixture fixture; TZrBool ok;
        case_name = matrix[i].name; ++cases;
        ok = run(&fixture, matrix[i].kind);
        ZrParser_ExecIr_BranchFactsFree(&fixture.facts);
        if (!ok) ++failures;
        printf("CASE %s %s\n", case_name, ok ? "PASS" : "FAIL");
    }
    printf("branch loop ranges: %u cases, %u failures, %u precondition failures\n",
            cases, failures, precondition_failures);
    return failures ? 1 : 0;
}
