#include "zr_vm_parser/exec_ir_branch_facts.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

/* Fixture-owned IDs, not inferred representation/type tokens. */
enum { F_NONE = 0, F_LEFT = 1, F_RIGHT = 2, F_RESULT = 3, F_CONDITION = 4,
       F_ENTRY = 1, F_TRUE = 2, F_FALSE = 3, F_MAX_VALUES = 4,
       F_MAX_INSTRUCTIONS = 4, F_MAX_BLOCKS = 3, F_MAX_OPERANDS = 5 };

typedef struct SFixture {
    SZrExecIrModule module;
    SZrExecIrFunction function;
    SZrExecIrValue values[F_MAX_VALUES];
    SZrExecIrInstruction instructions[F_MAX_INSTRUCTIONS];
    SZrExecIrBlock blocks[F_MAX_BLOCKS];
    TZrExecIrValueId operands[F_MAX_OPERANDS], results[1], signedValues[3];
    TZrExecIrBlockId successors[2], predecessors[2];
    SZrExecIrBranchEntryFact entries[2];
    SZrExecIrBranchFactsInput input;
    SZrExecIrBranchFacts facts;
    SZrExecIrDiagnostic diagnostic;
} SFixture;

static unsigned failures, cases;
static const char *case_name;
#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "FAIL [%s] line=%u: %s\n", case_name, (unsigned)__LINE__, #c); \
    return ZR_FALSE; } } while (0)

static void seed(SZrExecIrBranchEntryFact *entry, TZrExecIrValueId id,
                 TZrInt64 lower, TZrInt64 upper) {
    entry->valueId = id;
    entry->fact.available = ZR_TRUE;
    entry->fact.signedInteger = ZR_TRUE;
    entry->fact.hasLower = entry->fact.hasUpper = ZR_TRUE;
    entry->fact.lower = lower; entry->fact.upper = upper;
}

static void init(SFixture *f, TZrBool comparison, EZrExecIrOpcode opcode,
                 TZrUInt32 selector, TZrInt64 al, TZrInt64 au,
                 TZrInt64 bl, TZrInt64 bu) {
    TZrUInt32 block_count = comparison ? 3u : 2u;
    TZrUInt32 result_id = comparison ? F_CONDITION : F_RESULT;
    memset(f, 0, sizeof(*f));
    f->module.id = 1u; f->module.functions = &f->function;
    f->module.functionCount = f->module.functionCapacity = 1u;
    f->function.id = 1u; f->function.functionToken = 1u;
    f->function.entryBlockId = F_ENTRY;
    f->function.values = f->values;
    f->function.valueCount = f->function.valueCapacity = result_id;
    f->function.instructions = f->instructions;
    f->function.instructionCount = f->function.instructionCapacity = comparison ? 4u : 3u;
    f->function.blocks = f->blocks;
    f->function.blockCount = f->function.blockCapacity = block_count;
    f->function.operands = f->operands;
    f->function.operandCount = f->function.operandCapacity = comparison ? 5u : 3u;
    f->function.results = f->results;
    f->function.resultCount = f->function.resultCapacity = 1u;
    f->function.successors = f->successors;
    f->function.successorCount = f->function.successorCapacity = block_count - 1u;
    f->function.predecessors = f->predecessors;
    f->function.predecessorCount = f->function.predecessorCapacity = block_count - 1u;
    for (TZrUInt32 i = 0u; i < result_id; ++i) {
        f->values[i].id = i + 1u;
        f->values[i].typeToken = 0xABC000u + i; /* Opaque identity. */
    }
    f->values[F_LEFT - 1u].flags = f->values[F_RIGHT - 1u].flags =
            ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY;
    if (comparison) f->values[F_RESULT - 1u].flags = ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY;
    f->values[result_id - 1u].definition = 1u;
    for (TZrUInt32 i = 0u; i < block_count; ++i) {
        f->blocks[i].id = i + 1u;
        if (i == 0u) {
            f->blocks[i].flags = ZR_EXEC_IR_BLOCK_FLAG_ENTRY;
            f->blocks[i].instructions = (SZrExecIrRange){0u, 2u};
            f->blocks[i].successors = (SZrExecIrRange){0u, block_count - 1u};
            f->blocks[i].terminatorInstructionId = 2u;
        } else {
            f->blocks[i].instructions = (SZrExecIrRange){i + 1u, 1u};
            f->blocks[i].predecessors = (SZrExecIrRange){i - 1u, 1u};
            f->blocks[i].terminatorInstructionId = i + 2u;
            f->blocks[i].immediateDominator = F_ENTRY;
            f->successors[i - 1u] = i + 1u; f->predecessors[i - 1u] = F_ENTRY;
        }
    }
    f->instructions[0].opcode = (TZrUInt16)(comparison ? ZR_EXEC_IR_OPCODE_COMPARE : opcode);
    f->instructions[0].typeToken = comparison ? selector : 0u;
    f->instructions[0].sourceId = 101u;
    f->instructions[0].operands = (SZrExecIrRange){0u, 2u};
    f->instructions[0].results = (SZrExecIrRange){0u, 1u};
    f->results[0] = result_id;
    f->operands[0] = F_LEFT; f->operands[1] = F_RIGHT;
    f->instructions[1].opcode = comparison ? ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH : ZR_EXEC_IR_OPCODE_BRANCH;
    f->instructions[1].sourceId = 102u;
    f->instructions[1].successorRange = f->blocks[0].successors;
    if (comparison) {
        f->instructions[1].operands = (SZrExecIrRange){2u, 1u};
        f->operands[2] = F_CONDITION;
    }
    for (TZrUInt32 i = 2u; i < f->function.instructionCount; ++i) {
        f->instructions[i].opcode = ZR_EXEC_IR_OPCODE_RETURN;
        f->instructions[i].sourceId = 101u + i;
        f->instructions[i].operands = (SZrExecIrRange){comparison ? i + 1u : 2u, 1u};
        f->operands[comparison ? i + 1u : 2u] = comparison ? F_LEFT : F_RESULT;
    }
    seed(&f->entries[0], F_LEFT, al, au); seed(&f->entries[1], F_RIGHT, bl, bu);
    f->signedValues[0] = F_LEFT; f->signedValues[1] = F_RIGHT; f->signedValues[2] = F_RESULT;
    f->input.revision = 7u; f->input.generation = 9u;
    f->input.entries = f->entries; f->input.entryCount = 2u;
    f->input.signedValues = f->signedValues; f->input.signedValueCount = comparison ? 2u : 3u;
    ZrParser_ExecIr_BranchFactsInit(&f->facts);
}

static TZrBool analyze(SFixture *f) {
    CHECK(ZrCore_ExecIr_VerifyFunction(&f->function,
            (EZrExecIrVerifyLevel)(ZR_EXEC_IR_VERIFY_STRUCTURE | ZR_EXEC_IR_VERIFY_SSA), &f->diagnostic));
    CHECK(ZrParser_ExecIr_AnalyzeBranchFacts(&f->module, &f->function,
            &f->input, &f->facts, &f->diagnostic));
    CHECK(ZrParser_ExecIr_BranchFactsIsCurrent(&f->module, &f->function,
            &f->facts, f->input.revision, f->input.generation));
    return ZR_TRUE;
}

typedef struct SArithmeticCase {
    const char *name;
    EZrExecIrOpcode opcode;
    TZrInt64 al, au, bl, bu, lower, upper;
    unsigned mode; /* 0=proof, 1=possible overflow, 2=missing bound,
                    * 3=unwitnessed result, 4=unwitnessed operand,
                    * 5=flag, 6=effect, 7=poisoned input. */
} SArithmeticCase;

static TZrBool arithmetic(SFixture *f, const SArithmeticCase *c) {
    const SZrExecIrBranchValueFact *a;
    SZrExecIrInstruction before;
    init(f, ZR_FALSE, c->opcode, 0u, c->al, c->au, c->bl, c->bu);
    if (c->mode == 2u) f->entries[0].fact.hasUpper = ZR_FALSE;
    if (c->mode == 3u) f->input.signedValueCount = 2u;
    if (c->mode == 4u) {
        f->entries[0].fact.signedInteger = ZR_FALSE;
        f->entries[0].fact.hasLower = f->entries[0].fact.hasUpper = ZR_FALSE;
        f->signedValues[0] = F_RIGHT; f->signedValues[1] = F_RESULT;
        f->input.signedValueCount = 2u;
    }
    if (c->mode == 5u) f->instructions[0].flags = ZR_EXEC_IR_FLAG_DEBUG_POLL;
    if (c->mode == 6u) f->instructions[0].effectIn = 1u;
    if (c->mode == 7u) f->entries[0].fact.overflowed = ZR_TRUE;
    before = f->instructions[0];
    CHECK(analyze(f));
    CHECK(memcmp(&before, &f->instructions[0], sizeof(before)) == 0);
    CHECK(ZrParser_ExecIr_BranchFactsAtBlockEntry(&f->facts, F_ENTRY, F_RESULT) == ZR_NULL);
    a = ZrParser_ExecIr_BranchFactsAtEdge(&f->facts, F_ENTRY, 0u, F_RESULT);
    CHECK(a && a->available);
    if (c->mode == 0u) {
        CHECK(a->signedInteger && !a->overflowed && a->hasLower && a->hasUpper);
        CHECK(a->lower == c->lower && a->upper == c->upper);
    } else {
        CHECK(!a->hasLower && !a->hasUpper);
        if (c->mode == 1u || c->mode == 7u) CHECK(a->overflowed);
        if (c->mode == 3u) CHECK(!a->signedInteger);
    }
    return ZR_TRUE;
}

typedef struct SCompareCase {
    const char *name;
    TZrUInt32 selector, ordinal;
    TZrInt64 al, au, bl, bu, out_al, out_au, out_bl, out_bu;
    unsigned mode; /* 0=reachable, 1=unreachable, 2=swapped,
                    * 3=flag, 4=effect, 5=poisoned input. */
} SCompareCase;

static TZrBool comparison(SFixture *f, const SCompareCase *c) {
    const SZrExecIrBranchValueFact *a, *b;
    SZrExecIrInstruction before;
    init(f, ZR_TRUE, ZR_EXEC_IR_OPCODE_INVALID, c->selector, c->al, c->au, c->bl, c->bu);
    if (c->mode == 2u) { f->operands[0] = F_RIGHT; f->operands[1] = F_LEFT; }
    if (c->mode == 3u) f->instructions[0].flags = ZR_EXEC_IR_FLAG_DEBUG_POLL;
    if (c->mode == 4u) f->instructions[0].effectIn = 1u;
    if (c->mode == 5u) f->entries[0].fact.overflowed = ZR_TRUE;
    before = f->instructions[0];
    CHECK(analyze(f));
    CHECK(memcmp(&before, &f->instructions[0], sizeof(before)) == 0);
    a = ZrParser_ExecIr_BranchFactsAtEdge(&f->facts, F_ENTRY, c->ordinal, F_LEFT);
    b = ZrParser_ExecIr_BranchFactsAtEdge(&f->facts, F_ENTRY, c->ordinal, F_RIGHT);
    if (c->mode == 1u) CHECK(!a && !b);
    else {
        CHECK(a && b && a->hasLower && a->hasUpper && b->hasLower && b->hasUpper);
        CHECK(a->lower == c->out_al && a->upper == c->out_au);
        CHECK(b->lower == c->out_bl && b->upper == c->out_bu);
    }
    return ZR_TRUE;
}

int main(void) {
    static const SArithmeticCase arithmetic_cases[] = {
        {"add-one", ZR_EXEC_IR_OPCODE_ADD, 0,7,1,1,1,8,0},
        {"add-intervals", ZR_EXEC_IR_OPCODE_ADD, -4,7,-3,2,-7,9,0},
        {"sub-intervals", ZR_EXEC_IR_OPCODE_SUB, 0,7,1,3,-3,6,0},
        {"add-max-zero", ZR_EXEC_IR_OPCODE_ADD, INT64_MAX-1,INT64_MAX,0,0,INT64_MAX-1,INT64_MAX,0},
        {"sub-min-zero", ZR_EXEC_IR_OPCODE_SUB, INT64_MIN,INT64_MIN+1,0,0,INT64_MIN,INT64_MIN+1,0},
        {"add-min-one", ZR_EXEC_IR_OPCODE_ADD, INT64_MIN,INT64_MIN+1,1,1,INT64_MIN+1,INT64_MIN+2,0},
        {"sub-max-one", ZR_EXEC_IR_OPCODE_SUB, INT64_MAX-1,INT64_MAX,1,1,INT64_MAX-2,INT64_MAX-1,0},
        {"add-possible-high-overflow", ZR_EXEC_IR_OPCODE_ADD, INT64_MAX-1,INT64_MAX,1,1,0,0,1},
        {"add-possible-low-overflow", ZR_EXEC_IR_OPCODE_ADD, INT64_MIN,INT64_MIN+1,-1,-1,0,0,1},
        {"sub-possible-low-overflow", ZR_EXEC_IR_OPCODE_SUB, INT64_MIN,INT64_MIN+1,1,1,0,0,1},
        {"sub-possible-high-overflow", ZR_EXEC_IR_OPCODE_SUB, INT64_MAX-1,INT64_MAX,-1,-1,0,0,1},
        {"sub-negative-extreme-safe", ZR_EXEC_IR_OPCODE_SUB, INT64_MIN,INT64_MIN+1,INT64_MIN,INT64_MIN,0,1,0},
        {"missing-upper", ZR_EXEC_IR_OPCODE_ADD, 0,7,1,1,0,0,2},
        {"opaque-result-domain", ZR_EXEC_IR_OPCODE_ADD, 0,7,1,1,0,0,3},
        {"opaque-left-domain", ZR_EXEC_IR_OPCODE_ADD, 0,7,1,1,0,0,4},
        {"arithmetic-debug-flag", ZR_EXEC_IR_OPCODE_ADD, 0,7,1,1,0,0,5},
        {"arithmetic-effect", ZR_EXEC_IR_OPCODE_ADD, 0,7,1,1,0,0,6},
        {"arithmetic-poisoned-input", ZR_EXEC_IR_OPCODE_ADD, 0,7,1,1,0,0,7}
    };
    static const SCompareCase compare_cases[] = {
        {"lt-true",1,0,0,100,5,10,0,9,5,10,0},
        {"lt-false",1,1,0,100,5,10,5,100,5,10,0},
        {"le-true",2,0,0,100,5,10,0,10,5,10,0},
        {"le-false",2,1,0,100,5,10,6,100,5,10,0},
        {"gt-true",3,0,0,100,5,10,6,100,5,10,0},
        {"gt-false",3,1,0,100,5,10,0,10,5,10,0},
        {"ge-true",4,0,0,100,5,10,5,100,5,10,0},
        {"ge-false",4,1,0,100,5,10,0,9,5,10,0},
        {"lt-tightens-both",1,0,5,100,0,10,5,9,6,10,0},
        {"lt-false-tightens-both",1,1,0,10,5,100,5,10,5,10,0},
        {"swapped-lt",1,0,0,100,5,10,6,100,5,10,2},
        {"swapped-false-lt",1,1,0,100,5,10,0,10,5,10,2},
        {"eq-intersection",0,0,0,100,5,10,5,10,5,10,0},
        {"ne-false-intersection",5,1,0,100,5,10,5,10,5,10,0},
        {"ne-no-interior-hole",5,0,0,100,5,10,0,100,5,10,0},
        {"eq-disjoint-unreachable",0,0,0,4,5,10,0,0,0,0,1},
        {"lt-impossible-unreachable",1,0,10,20,0,9,0,0,0,0,1},
        {"lt-min-bound",1,0,INT64_MIN,INT64_MIN+2,INT64_MIN,INT64_MIN+1,INT64_MIN,INT64_MIN,INT64_MIN+1,INT64_MIN+1,0},
        {"gt-max-bound",3,0,INT64_MAX-2,INT64_MAX,INT64_MAX-1,INT64_MAX,INT64_MAX,INT64_MAX,INT64_MAX-1,INT64_MAX-1,0},
        {"compare-debug-flag",1,0,0,100,5,10,0,100,5,10,3},
        {"compare-effect",1,0,0,100,5,10,0,100,5,10,4},
        {"compare-poisoned-input",1,0,0,100,5,10,0,100,5,10,5}
    };
    for (size_t i = 0u; i < sizeof(arithmetic_cases)/sizeof(arithmetic_cases[0]); ++i) {
        SFixture fixture; TZrBool ok;
        case_name = arithmetic_cases[i].name; ++cases;
        ok = arithmetic(&fixture, &arithmetic_cases[i]);
        ZrParser_ExecIr_BranchFactsFree(&fixture.facts);
        if (!ok) ++failures;
        printf("CASE %s %s\n", case_name, ok ? "PASS" : "FAIL");
    }
    for (size_t i = 0u; i < sizeof(compare_cases)/sizeof(compare_cases[0]); ++i) {
        SFixture fixture; TZrBool ok;
        case_name = compare_cases[i].name; ++cases;
        ok = comparison(&fixture, &compare_cases[i]);
        ZrParser_ExecIr_BranchFactsFree(&fixture.facts);
        if (!ok) ++failures;
        printf("CASE %s %s\n", case_name, ok ? "PASS" : "FAIL");
    }
    printf("branch arithmetic constraints: %u cases, %u failures\n", cases, failures);
    return failures ? 1 : 0;
}
