#include "zr_vm_core/exec_ir.h"
#include "zr_vm_core/exec_ir_interpreter.h"
#include "zr_vm_parser/exec_ir_builder.h"
#include "../../zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_internal.h"

#include <stdio.h>
#include <string.h>

/* Opaque identities: no numeric evaluator, constant pool or runtime invocation. */
enum { V_SOURCE = 1, V_OTHER = 2, V_CONDITION = 3, V_COPY = 4,
       V_CHAIN = 5, V_PHI = 6, V_BOUNDARY_RESULT = 7,
       MAX_VALUES = 7, MAX_BLOCKS = 4, MAX_INSTRUCTIONS = 10,
       MAX_OPERANDS = 16, MAX_RESULTS = 8, MAX_EDGES = 8,
       MAX_MEMORY = 4, MAX_INCOMING = 2 };
enum ECase { C_COPY, C_CHAIN, C_DOMINATING, C_PARALLEL, C_OWNED,
             C_MOVE, C_DROP, C_MAPPED, C_INVOKE, C_SUSPEND,
             C_BAD_PREDECESSOR, C_BAD_AVAILABILITY,
             C_INTERIOR_SUSPEND_CALL, C_INTERIOR_CALL_ORDERING };

typedef struct SFixture {
    SZrExecIrFunction function;
    SZrExecIrInstruction instructions[MAX_INSTRUCTIONS];
    SZrExecIrValue values[MAX_VALUES];
    SZrExecIrBlock blocks[MAX_BLOCKS];
    TZrExecIrValueId operands[MAX_OPERANDS], results[MAX_RESULTS];
    TZrExecIrBlockId predecessors[MAX_EDGES], successors[MAX_EDGES];
    TZrExecIrMemoryTokenId memory[MAX_MEMORY];
    SZrExecIrPhi phi;
    SZrExecIrPhiIncoming incoming[MAX_INCOMING];
    SZrExecIrStateMap map;
    TZrExecIrBlockId join;
} SFixture;

static unsigned cases, failures, precondition_failures;
static const char *case_name;
static TZrBool case_ok;
#define EXPECT(c) do { if (!(c)) { case_ok = ZR_FALSE; \
    fprintf(stderr, "SEMANTIC FAIL [%s] line=%u: %s\n", case_name, (unsigned)__LINE__, #c); \
} } while (0)

static void init(SFixture *s, TZrUInt32 blocks) {
    SZrExecIrFunction *f;
    memset(s, 0, sizeof(*s)); f = &s->function;
    f->id = 1u; f->functionToken = 1u; f->entryBlockId = 1u;
    f->values = s->values; f->valueCount = f->valueCapacity = MAX_VALUES;
    f->blocks = s->blocks; f->blockCount = f->blockCapacity = blocks;
    f->instructions = s->instructions; f->operands = s->operands; f->results = s->results;
    f->predecessors = s->predecessors; f->successors = s->successors;
    f->phiPool = &s->phi; f->phiIncoming = s->incoming;
    f->memoryTokenPool = s->memory;
    for (TZrUInt32 i = 0u; i < MAX_VALUES; ++i) {
        s->values[i].id = i + 1u; s->values[i].typeToken = 0xCA110u;
        /* Unused fixture values are also explicit external definitions. */
        s->values[i].flags = ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY;
    }
    for (TZrUInt32 i = 0u; i < blocks; ++i) s->blocks[i].id = i + 1u;
    s->blocks[0].flags = ZR_EXEC_IR_BLOCK_FLAG_ENTRY;
}

static void op(SFixture *s, TZrExecIrBlockId block, EZrExecIrOpcode opcode,
               TZrExecIrValueId operand, TZrExecIrValueId result) {
    SZrExecIrFunction *f = &s->function;
    TZrUInt32 i = f->instructionCount++;
    SZrExecIrInstruction *instruction = &s->instructions[i];
    SZrExecIrBlock *container = &s->blocks[block - 1u];
    if (!container->instructions.count) container->instructions.offset = i;
    ++container->instructions.count; container->terminatorInstructionId = i + 1u;
    instruction->opcode = (TZrUInt16)opcode; instruction->sourceId = 101u + i;
    instruction->operands.offset = f->operandCount;
    if (operand) { s->operands[f->operandCount++] = operand; instruction->operands.count = 1u; }
    instruction->results.offset = f->resultCount;
    if (result) {
        s->results[f->resultCount++] = result; instruction->results.count = 1u;
        s->values[result - 1u].definition = i + 1u;
        s->values[result - 1u].flags = 0u;
    }
    f->instructionCapacity = f->instructionCount;
    f->operandCapacity = f->operandCount; f->resultCapacity = f->resultCount;
}

static void links(SFixture *s, TZrExecIrBlockId block,
                  TZrExecIrBlockId a, TZrExecIrBlockId b) {
    SZrExecIrFunction *f = &s->function;
    SZrExecIrBlock *container = &s->blocks[block - 1u];
    container->successors.offset = f->successorCount;
    if (a) { s->successors[f->successorCount++] = a; ++container->successors.count; }
    if (b) { s->successors[f->successorCount++] = b; ++container->successors.count; }
    s->instructions[container->terminatorInstructionId - 1u].successorRange = container->successors;
    f->successorCapacity = f->successorCount;
}

static void finish(SFixture *s, TZrExecIrBlockId join,
                   TZrExecIrValueId first, TZrExecIrValueId second) {
    SZrExecIrFunction *f = &s->function;
    s->join = join;
    for (TZrUInt32 target = 1u; target <= f->blockCount; ++target) {
        SZrExecIrBlock *container = &s->blocks[target - 1u];
        container->predecessors.offset = f->predecessorCount;
        for (TZrUInt32 pred = 1u; pred <= f->blockCount; ++pred) {
            SZrExecIrRange range = s->blocks[pred - 1u].successors;
            for (TZrUInt32 j = 0u; j < range.count; ++j)
                if (s->successors[range.offset + j] == target) {
                    s->predecessors[f->predecessorCount++] = pred;
                    ++container->predecessors.count;
                }
        }
    }
    f->predecessorCapacity = f->predecessorCount;
    s->blocks[join - 1u].phis.count = 1u;
    s->phi.result = V_PHI;
    s->phi.incomings.count = s->blocks[join - 1u].predecessors.count;
    for (TZrUInt32 i = 0u; i < s->phi.incomings.count; ++i) {
        s->incoming[i].predecessor = s->predecessors[s->blocks[join - 1u].predecessors.offset + i];
        s->incoming[i].value = i == 0u ? first : second;
    }
    s->values[V_PHI - 1u].flags = 0u; s->values[V_PHI - 1u].definition = 0u;
    f->phiCount = f->phiCapacity = 1u;
    f->phiIncomingCount = f->phiIncomingCapacity = s->phi.incomings.count;
}

static void memory_boundary(SFixture *s, TZrUInt32 instruction,
                            EZrExecIrMemoryClass a, EZrExecIrMemoryClass b,
                            TZrUInt32 regions) {
    SZrExecIrInstruction *op = &s->instructions[instruction];
    EZrExecIrMemoryClass classes[2] = {a, b};
    for (TZrUInt32 i = 0u; i < regions; ++i) {
        s->memory[i] = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(classes[i], 1u);
        s->memory[regions + i] = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(classes[i], 2u);
    }
    op->memoryIn.count = regions;
    op->memoryOut.offset = regions; op->memoryOut.count = regions;
    op->effectIn = 1u; op->effectOut = 2u;
    s->function.memoryTokenCount = s->function.memoryTokenCapacity = 2u * regions;
}

static void build(SFixture *s, enum ECase kind) {
    TZrExecIrValueId incoming = kind == C_CHAIN || kind == C_BAD_AVAILABILITY ? V_CHAIN : V_COPY;
    init(s, kind == C_DOMINATING || kind == C_INVOKE ? 3u : 2u);
    op(s, 1u, ZR_EXEC_IR_OPCODE_COPY, V_SOURCE, V_COPY);
    if (kind == C_CHAIN || kind == C_BAD_AVAILABILITY)
        op(s, 1u, ZR_EXEC_IR_OPCODE_COPY, V_COPY, V_CHAIN);
    if (kind == C_PARALLEL) op(s, 1u, ZR_EXEC_IR_OPCODE_COPY, V_OTHER, V_CHAIN);
    if (kind == C_OWNED) {
        s->values[V_SOURCE - 1u].ownership = ZR_EXEC_IR_OWNERSHIP_UNIQUE;
        s->values[V_COPY - 1u].ownership = ZR_EXEC_IR_OWNERSHIP_UNIQUE;
    }
    if (kind == C_MAPPED) {
        s->function.contract.generation = 7u;
        s->map.functionToken = 1u; s->map.generation = 7u;
        s->function.stateMap = &s->map;
    }
    if (kind == C_MOVE) op(s, 1u, ZR_EXEC_IR_OPCODE_MOVE, V_SOURCE, V_BOUNDARY_RESULT);
    if (kind == C_DROP) {
        op(s, 1u, ZR_EXEC_IR_OPCODE_DROP, V_SOURCE, 0u);
        memory_boundary(s, 1u, ZR_EXEC_IR_MEMORY_OWNERSHIP, ZR_EXEC_IR_MEMORY_OWNERSHIP, 1u);
    }
    if (kind == C_INVOKE) {
        op(s, 1u, ZR_EXEC_IR_OPCODE_INVOKE, 0u, V_BOUNDARY_RESULT);
        s->instructions[1].flags = ZR_EXEC_IR_FLAG_MAY_THROW | ZR_EXEC_IR_FLAG_MAY_ALLOCATE;
        memory_boundary(s, 1u, ZR_EXEC_IR_MEMORY_MANAGED_HEAP, ZR_EXEC_IR_MEMORY_NATIVE_FFI, 2u);
        s->blocks[2].flags = ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION;
        links(s, 1u, 2u, 3u);
        op(s, 2u, ZR_EXEC_IR_OPCODE_RETURN, V_SOURCE, 0u);
        op(s, 3u, ZR_EXEC_IR_OPCODE_RETURN, V_PHI, 0u);
        finish(s, 3u, V_COPY, 0u); return;
    }
    /* Analysis-only interior CALL boundaries. A later ordinary BRANCH must
     * not hide suspension or memory/effect ordering inside this predecessor. */
    if (kind == C_INTERIOR_SUSPEND_CALL || kind == C_INTERIOR_CALL_ORDERING) {
        op(s, 1u, ZR_EXEC_IR_OPCODE_CALL, 0u, V_BOUNDARY_RESULT);
        s->instructions[1].flags = ZR_EXEC_IR_FLAG_MAY_THROW | ZR_EXEC_IR_FLAG_MAY_ALLOCATE;
        if (kind == C_INTERIOR_SUSPEND_CALL)
            s->instructions[1].flags |= ZR_EXEC_IR_FLAG_MAY_SUSPEND;
        /* Recognized CALL schema classes, with actual input/output versions
         * and effect ordering. No provider or CALL runtime is invoked. */
        memory_boundary(s, 1u, ZR_EXEC_IR_MEMORY_MANAGED_HEAP, ZR_EXEC_IR_MEMORY_NATIVE_FFI, 2u);
    }
    if (kind == C_SUSPEND) {
        op(s, 1u, ZR_EXEC_IR_OPCODE_SUSPEND, 0u, V_BOUNDARY_RESULT);
        s->instructions[1].flags = ZR_EXEC_IR_FLAG_MAY_SUSPEND;
        memory_boundary(s, 1u, ZR_EXEC_IR_MEMORY_SCHEDULER_TASK, ZR_EXEC_IR_MEMORY_SCHEDULER_TASK, 1u);
    } else if (kind == C_PARALLEL)
        op(s, 1u, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH, V_CONDITION, 0u);
    else op(s, 1u, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u);
    links(s, 1u, 2u, kind == C_PARALLEL ? 2u : 0u);
    if (kind == C_DOMINATING) {
        op(s, 2u, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u); links(s, 2u, 3u, 0u);
        op(s, 3u, ZR_EXEC_IR_OPCODE_RETURN, V_PHI, 0u);
        finish(s, 3u, incoming, 0u);
    } else {
        op(s, 2u, ZR_EXEC_IR_OPCODE_RETURN, V_PHI, 0u);
        finish(s, 2u, incoming, kind == C_PARALLEL ? V_CHAIN : 0u);
    }
}

static void diamond(SFixture *s) {
    init(s, 4u);
    op(s, 1u, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH, V_CONDITION, 0u); links(s, 1u, 2u, 3u);
    op(s, 2u, ZR_EXEC_IR_OPCODE_COPY, V_SOURCE, V_COPY);
    op(s, 2u, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u); links(s, 2u, 4u, 0u);
    op(s, 3u, ZR_EXEC_IR_OPCODE_COPY, V_OTHER, V_CHAIN);
    op(s, 3u, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u); links(s, 3u, 4u, 0u);
    op(s, 4u, ZR_EXEC_IR_OPCODE_RETURN, V_PHI, 0u); finish(s, 4u, V_COPY, V_CHAIN);
}

static TZrBool precondition(const char *name, TZrBool success, const SZrExecIrDiagnostic *d) {
    printf("PRECONDITION %s %s %s\n", case_name, name, success ? "PASS" : "FAIL");
    if (!success) {
        ++precondition_failures; case_ok = ZR_FALSE;
        fprintf(stderr, "PRECONDITION FAIL [%s] %s code=%u function=%u block=%u instruction=%u source=%u\n",
                case_name, name, (unsigned)d->code, (unsigned)d->functionToken,
                (unsigned)d->blockId, (unsigned)d->instructionId, (unsigned)d->sourceId);
    }
    return success;
}

static TZrBool verify(SFixture *s, const char *name) {
    SZrExecIrDiagnostic d = {0};
    TZrBool success = ZrCore_ExecIr_VerifyFunction(&s->function,
            ZR_EXEC_IR_VERIFY_STRUCTURE | ZR_EXEC_IR_VERIFY_SSA, &d);
    return precondition(name, (TZrBool)(success && d.code == ZR_EXECUTION_DIAGNOSTIC_NONE), &d);
}

/* Only the pure COPY/conditional/PHI/RETURN fixture enters the pointer-free
 * oracle. INVOKE, DROP, SUSPEND and interior CALL fixtures are analysis-only. */
static TZrBool parallel_paths(SFixture *s, const char *stage) {
    for (TZrUInt32 ordinal = 0u; ordinal < 2u; ++ordinal) {
        SZrExecIrOracleValue initial[MAX_VALUES] = {{0}};
        SZrExecIrOracleInput input = {0};
        SZrExecIrOracleExecutionResult result = {0};
        SZrExecIrDiagnostic d = {0};
        char label[64];
        TZrBool success;
        initial[V_SOURCE - 1u].kind = initial[V_OTHER - 1u].kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
        initial[V_SOURCE - 1u].as.signedInteger = 7;
        initial[V_OTHER - 1u].as.signedInteger = 11;
        initial[V_CONDITION - 1u].kind = ZR_EXEC_IR_ORACLE_VALUE_BOOL;
        initial[V_CONDITION - 1u].as.boolean = ordinal == 0u ? ZR_TRUE : ZR_FALSE;
        input.function = &s->function; input.initialValues = initial;
        input.initialValueCount = MAX_VALUES; input.maxSteps = MAX_INSTRUCTIONS;
        success = ZrCore_ExecIr_RunOracleEx(&input, &result, &d);
        success = (TZrBool)(success && result.returned && result.eventCount == 0u &&
                result.returnValue.kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED &&
                result.returnValue.as.signedInteger == (ordinal == 0u ? 7 : 11));
        snprintf(label, sizeof(label), "%sOrdinal%u", stage, (unsigned)ordinal);
        ZrCore_ExecIr_OracleResultFree(&result);
        if (!precondition(label, success, &d)) return ZR_FALSE;
        printf("PATH %s %s successorOrdinal=%u value=%u PASS\n", case_name, stage,
                (unsigned)ordinal, ordinal == 0u ? 7u : 11u);
    }
    return ZR_TRUE;
}

static TZrBool mapped_precondition(SFixture *s, const char *name) {
    SZrExecIrDiagnostic d = {0};
    TZrBool valid = ZrCore_ExecIr_ValidateStateMap(&s->function, &s->map, &d);
    return precondition(name, valid, &d);
}

static void expected_diagnostic(SFixture *s, const char *label, EZrExecutionDiagnosticCode code,
                                TZrExecIrBlockId block, TZrExecIrInstructionId instruction) {
    SZrExecIrDiagnostic d = {0};
    TZrUInt64 before = ZrParser_ExecIr_FunctionHash(&s->function);
    TZrBool success = ZrCore_ExecIr_VerifyFunction(&s->function,
            ZR_EXEC_IR_VERIFY_STRUCTURE | ZR_EXEC_IR_VERIFY_SSA, &d);
    TZrBool matches = (TZrBool)(!success && d.code == code && d.functionToken == 1u &&
            d.blockId == block && d.instructionId == instruction &&
            d.sourceId == 100u + instruction);
    EXPECT(matches);
    EXPECT(before == ZrParser_ExecIr_FunctionHash(&s->function));
    printf("EXPECTED_DIAGNOSTIC %s %s %s code=%u block=%u instruction=%u source=%u\n",
           case_name, label, matches ? "PASS" : "FAIL",
           (unsigned)d.code, (unsigned)d.blockId, (unsigned)d.instructionId, (unsigned)d.sourceId);
    /* Invalid IR is never sent to the optimizer. */
}

static void run(enum ECase kind, const char *name) {
    SFixture s;
    SZrExecIrDiagnostic d = {0};
    TZrBool changed[2] = {0};
    TZrBool positive = (TZrBool)(kind == C_COPY || kind == C_CHAIN || kind == C_PARALLEL);
    TZrBool interiorBoundary = (TZrBool)(kind == C_INTERIOR_SUSPEND_CALL ||
                                        kind == C_INTERIOR_CALL_ORDERING);
    TZrUInt64 boundaryHash = 0u;
    TZrUInt32 instructionCount, incomingCount;
    case_name = name; case_ok = ZR_TRUE; ++cases;
    build(&s, kind);
    instructionCount = s.function.instructionCount; incomingCount = s.function.phiIncomingCount;
    if (!verify(&s, "CoreVerify")) goto done;
    if (kind == C_PARALLEL && !parallel_paths(&s, "Original")) goto done;
    if (kind == C_MAPPED && !mapped_precondition(&s, "OriginalStateMap")) goto done;
    if (!precondition("Dominators", ZrParser_ExecIr_ComputeDominators(&s.function, &d), &d)) goto done;
    if (interiorBoundary) boundaryHash = ZrParser_ExecIr_FunctionHash(&s.function);
    for (TZrUInt32 iteration = 0u; iteration < 2u; ++iteration) {
        memset(&d, 0, sizeof(d));
        if (!precondition(iteration ? "RepeatSccp" : "Sccp",
                ZrParser_ExecIr_ComputeSccp(&s.function, ZR_NULL, ZR_TRUE, &changed[iteration], &d), &d)) goto done;
        if (!verify(&s, iteration ? "RepeatCoreVerify" : "PostCoreVerify")) goto done;
        if (kind == C_PARALLEL && !parallel_paths(&s, iteration ? "Repeat" : "Optimized")) goto done;
        if (kind == C_MAPPED && !mapped_precondition(&s, iteration ? "RepeatStateMap" : "PostStateMap")) goto done;
        EXPECT(s.function.instructionCount == instructionCount && s.function.phiIncomingCount == incomingCount);
        EXPECT(s.phi.result == V_PHI && s.phi.incomings.count == incomingCount);
        EXPECT(s.instructions[0].opcode == ZR_EXEC_IR_OPCODE_COPY);
        if (interiorBoundary) {
            EXPECT(s.instructions[1].opcode == ZR_EXEC_IR_OPCODE_CALL);
            EXPECT(s.instructions[2].opcode == ZR_EXEC_IR_OPCODE_BRANCH);
            EXPECT(s.blocks[0].terminatorInstructionId == 3u);
            EXPECT(boundaryHash == ZrParser_ExecIr_FunctionHash(&s.function));
            for (TZrUInt32 i = 0u; i < instructionCount; ++i)
                EXPECT(s.instructions[i].sourceId == 101u + i);
            if (kind == C_INTERIOR_SUSPEND_CALL)
                EXPECT((s.instructions[1].flags & ZR_EXEC_IR_FLAG_MAY_SUSPEND) != 0u);
            else {
                EXPECT(s.instructions[1].flags ==
                       (ZR_EXEC_IR_FLAG_MAY_THROW | ZR_EXEC_IR_FLAG_MAY_ALLOCATE));
                EXPECT(s.instructions[1].memoryIn.count == 2u &&
                       s.instructions[1].memoryOut.count == 2u);
                EXPECT(s.memory[0] == ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 1u));
                EXPECT(s.memory[1] == ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_NATIVE_FFI, 1u));
                EXPECT(s.memory[2] == ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 2u));
                EXPECT(s.memory[3] == ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_NATIVE_FFI, 2u));
                EXPECT(s.instructions[1].effectIn == 1u && s.instructions[1].effectOut == 2u);
            }
        }
        for (TZrUInt32 i = 0u; i < incomingCount; ++i)
            EXPECT(s.incoming[i].predecessor == s.predecessors[s.blocks[s.join - 1u].predecessors.offset + i]);
        EXPECT(s.operands[s.instructions[s.blocks[s.join - 1u].terminatorInstructionId - 1u].operands.offset] == V_PHI);
        if (positive) {
            EXPECT(s.incoming[0].value == V_SOURCE);
            if (kind == C_PARALLEL) EXPECT(s.incoming[1].value == V_OTHER);
        } else if (kind != C_BAD_AVAILABILITY && kind != C_BAD_PREDECESSOR)
            EXPECT(s.incoming[0].value == V_COPY);
    }
    if (positive) EXPECT(changed[0] && !changed[1]);
    else if (kind != C_BAD_AVAILABILITY && kind != C_BAD_PREDECESSOR) EXPECT(!changed[0] && !changed[1]);
    if (kind == C_BAD_PREDECESSOR) {
        s.incoming[0].predecessor = s.join;
        expected_diagnostic(&s, "predecessor", ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                s.join, s.blocks[s.join - 1u].terminatorInstructionId);
    }
    if (kind == C_BAD_AVAILABILITY) {
        s.operands[s.instructions[0].operands.offset] = V_CHAIN;
        expected_diagnostic(&s, "definition-order", ZR_EXEC_IR_DIAGNOSTIC_DOMINANCE, 1u, 1u);
        diamond(&s);
        if (!verify(&s, "DiamondCoreVerify")) goto done;
        s.incoming[1].value = V_COPY;
        expected_diagnostic(&s, "non-dominating-edge", ZR_EXEC_IR_DIAGNOSTIC_DOMINANCE,
                4u, s.blocks[3].terminatorInstructionId);
    }
done:
    if (!case_ok) ++failures;
    printf("CASE %s %s\n", case_name, case_ok ? "PASS" : "FAIL");
}

int main(void) {
    (void)setvbuf(stdout, ZR_NULL, _IONBF, 0u);
    run(C_COPY, "predecessor-copy");
    run(C_CHAIN, "predecessor-copy-chain");
    run(C_DOMINATING, "cross-block-copy-preserved");
    run(C_PARALLEL, "parallel-occurrences-distinct-values");
    run(C_OWNED, "ownership-preserved");
    run(C_MOVE, "move-source-preserved");
    run(C_DROP, "drop-source-preserved");
    run(C_MAPPED, "mapped-function-preserved");
    run(C_INVOKE, "exception-predecessor-preserved");
    run(C_SUSPEND, "suspend-predecessor-preserved");
    run(C_BAD_PREDECESSOR, "malformed-predecessor-diagnostic");
    run(C_BAD_AVAILABILITY, "malformed-definition-availability-diagnostics");
    run(C_INTERIOR_SUSPEND_CALL, "interior-suspend-call-preserved");
    run(C_INTERIOR_CALL_ORDERING, "interior-memory-ordering-preserved");
    printf("SCCP PHI copy edges: %u cases, %u failures, %u precondition failures\n",
           cases, failures, precondition_failures);
    return failures || precondition_failures ? 1 : 0;
}
