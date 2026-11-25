#include "compiler/strict_semantic_flow.h"

#include <stdlib.h>

#include "zr_vm_parser/semantic_ir.h"

typedef struct StrictPlace {
    TZrPlaceId place;
    TZrTypeId type;
    TZrLoanId loan;
    TZrValueId loanValue;
    uint32_t owner;
} StrictPlace;

typedef struct StrictFlow {
    SZrSemanticIrFunction function;
    const ZrStrictProgram *program;
    StrictPlace *places;
    SZrFileRange range;
} StrictFlow;

static TZrBool is_loan(uint32_t op) {
    return op == ZR_STRICT_OP_BORROW || op == ZR_STRICT_OP_BORROW_FIELD;
}

static TZrBool is_writable(uint8_t kind) {
    return kind == ZR_STRICT_VALUE_BORROW || kind == ZR_STRICT_VALUE_BYTE_VIEW;
}

static TZrBool is_jump(uint32_t op) {
    return op == ZR_STRICT_OP_JUMP || op == ZR_STRICT_OP_JUMP_IF_FALSE;
}

static TZrBool is_exit(uint32_t op) {
    return op == ZR_STRICT_OP_RETURN || op == ZR_STRICT_OP_HALT;
}

static TZrBool step(StrictFlow *flow, uint32_t slot, EZrSemanticIrOpcode op, const StrictPlace *borrow) {
    if (slot >= flow->program->slotCount) return ZR_FALSE;
    const StrictPlace *item = &flow->places[slot];
    /* Scalar definitions and uses still receive type and availability checks
     * in the independent executable verifier. */
    if (!item->place) return ZR_TRUE;
    SZrSemanticIrInstructionSpec spec = {.opcode = op, .placeId = item->place,
        .typeId = item->type, .targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID, .sourceRange = flow->range};
    if (op == ZR_SEMANTIC_IR_INITIALIZE || op == ZR_SEMANTIC_IR_STORE) {
        spec.valueId = ZrParser_SemanticIr_AddValue(&flow->function, item->type, flow->range);
        if (!spec.valueId) return ZR_FALSE;
    } else if (op == ZR_SEMANTIC_IR_LOAD || op == ZR_SEMANTIC_IR_MOVE) {
        spec.resultValueId = ZrParser_SemanticIr_AddValue(&flow->function, item->type, flow->range);
        if (!spec.resultValueId) return ZR_FALSE;
    }
    if (borrow) {
        spec.loanId = borrow->loan;
        if (op != ZR_SEMANTIC_IR_END_LOAN) {
            spec.typeId = borrow->type;
            spec.resultValueId = borrow->loanValue;
        }
    }
    return ZrParser_SemanticIr_Emit(&flow->function, &spec) != ZR_SEMANTIC_INSTRUCTION_ID_INVALID;
}

static TZrBool project_instruction(StrictFlow *flow, const ZrStrictInstruction *i) {
    const ZrStrictProgram *p = flow->program;
    const uint32_t dst = i->dst, src = i->lhs;
    switch (i->op) {
        case ZR_STRICT_OP_INIT_RECORD:
        case ZR_STRICT_OP_OWN_RECORD: {
            if (dst >= p->slotCount || !p->slotLayouts[dst] || p->slotLayouts[dst] > p->layoutCount)
                return ZR_FALSE;
            uint32_t fields = p->layouts[p->slotLayouts[dst] - 1].fieldCount;
            if (src > p->slotCount || fields > p->slotCount - src) return ZR_FALSE;
            for (uint32_t f = 0; f < fields; ++f)
                if (!step(flow, src + f, ZR_SEMANTIC_IR_MOVE, NULL)) return ZR_FALSE;
            return step(flow, dst, ZR_SEMANTIC_IR_INITIALIZE, NULL);
        }
        case ZR_STRICT_OP_MANUAL_TRY_ALLOC:
            return step(flow, dst, ZR_SEMANTIC_IR_INITIALIZE, NULL);
        case ZR_STRICT_OP_TRANSFER:
        case ZR_STRICT_OP_SHARE:
            return step(flow, src, ZR_SEMANTIC_IR_MOVE, NULL) && step(flow, dst, ZR_SEMANTIC_IR_INITIALIZE, NULL);
        case ZR_STRICT_OP_CLONE:
        case ZR_STRICT_OP_DEGRADE:
        case ZR_STRICT_OP_WAKE:
        case ZR_STRICT_OP_UNWRAP_SHARED:
        case ZR_STRICT_OP_FIELD_READ:
            return step(flow, src, ZR_SEMANTIC_IR_LOAD, NULL) && step(flow, dst, ZR_SEMANTIC_IR_INITIALIZE, NULL);
        case ZR_STRICT_OP_IS_NULL:
        case ZR_STRICT_OP_BYTE_LENGTH:
        case ZR_STRICT_OP_BYTE_READ:
            return step(flow, src, ZR_SEMANTIC_IR_LOAD, NULL);
        case ZR_STRICT_OP_DROP:
            return step(flow, src, ZR_SEMANTIC_IR_DROP, NULL);
        case ZR_STRICT_OP_BORROW:
        case ZR_STRICT_OP_BORROW_FIELD:
            return step(flow, src, is_writable(p->kinds[dst]) ? ZR_SEMANTIC_IR_BORROW_MUT : ZR_SEMANTIC_IR_BORROW_SHARED,
                        &flow->places[dst]) && step(flow, dst, ZR_SEMANTIC_IR_INITIALIZE, NULL);
        case ZR_STRICT_OP_END_BORROW:
            return src < p->slotCount && flow->places[src].loan && step(flow, src, ZR_SEMANTIC_IR_DROP, NULL) &&
                   step(flow, flow->places[src].owner, ZR_SEMANTIC_IR_END_LOAN, &flow->places[src]);
        case ZR_STRICT_OP_FIELD_WRITE:
            return step(flow, dst, ZR_SEMANTIC_IR_LOAD, NULL) && step(flow, dst, ZR_SEMANTIC_IR_STORE, NULL) &&
                   step(flow, src, ZR_SEMANTIC_IR_MOVE, NULL);
        case ZR_STRICT_OP_BYTE_WRITE:
            return step(flow, dst, ZR_SEMANTIC_IR_LOAD, NULL) && step(flow, dst, ZR_SEMANTIC_IR_STORE, NULL);
        default:
            return i->op >= ZR_STRICT_OP_CONST && i->op <= ZR_STRICT_OP_HALT;
    }
}

static TZrBool project_places(StrictFlow *flow, const SZrCliStrictLocation *locations, TZrBool receiverIsLive) {
    SZrSemanticIrFunction *f = &flow->function;
    const ZrStrictProgram *p = flow->program;
    for (uint32_t slot = 0; slot < p->slotCount; ++slot) {
        if (p->kinds[slot] < ZR_STRICT_VALUE_RECORD) continue;
        const TZrBool receiver = receiverIsLive && slot == 0;
        const SZrParserPlaceBase base = {receiver ? ZR_PARSER_PLACE_BASE_THIS : ZR_PARSER_PLACE_BASE_LOCAL, slot + 1};
        StrictPlace *item = &flow->places[slot];
        /* These are local type identities in the projection. The strict
         * verifier independently checks kind and nominal layout compatibility. */
        item->type = p->slotLayouts[slot] * (ZR_STRICT_VALUE_READONLY_BYTE_VIEW + 1) + p->kinds[slot];
        item->place = ZrParser_SemanticIr_AddLocal(f, slot + 1, &base, item->type, locations[0].range,
                                                   receiver);
        if (!item->place) return ZR_FALSE;
    }
    for (uint32_t pc = 0; pc < p->instructionCount; ++pc) {
        const ZrStrictInstruction *i = &p->instructions[pc];
        if (!is_loan(i->op)) continue;
        const SZrFileRange range = locations[pc].range;
        if (i->dst >= p->slotCount || i->lhs >= p->slotCount) return ZR_FALSE;
        StrictPlace *item = &flow->places[i->dst];
        if (!item->place || item->loan || !flow->places[i->lhs].place) return ZR_FALSE;
        TZrRegionId region = ZrParser_SemanticIr_AddRegion(f, ZR_SEMANTIC_REGION_ID_INVALID,
                                                          ZR_SEMANTIC_ESCAPE_LOCAL, range);
        if (!region) return ZR_FALSE;
        item->owner = i->lhs;
        item->loanValue = ZrParser_SemanticIr_AddValue(f, item->type, range);
        if (!item->loanValue) return ZR_FALSE;
        /* The strict ABI treats a field borrow as a borrow of the containing
         * slot. Reborrows use the alias slot, whose scope cannot outlive its owner. */
        item->loan = ZrParser_SemanticIr_AddLoan(f, flow->places[i->lhs].place,
            is_writable(p->kinds[i->dst]) ? ZR_SEMANTIC_LOAN_MUTABLE : ZR_SEMANTIC_LOAN_SHARED,
            region, range, range, item->loanValue);
        if (!item->loan) return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool project_cfg(StrictFlow *flow, const SZrCliStrictLocation *locations, uint32_t *leaders,
                          uint32_t *irEnds, uint32_t *badPc) {
    SZrParserCfg *cfg = &flow->function.cfg;
    const ZrStrictProgram *p = flow->program;
    const uint32_t n = p->instructionCount;
    leaders[0] = 1;
    for (uint32_t pc = 0; pc < n; ++pc) {
        const ZrStrictInstruction *i = &p->instructions[pc];
        if (is_jump(i->op)) {
            if (i->immediate >= n) return ZR_FALSE;
            leaders[i->immediate] = 1;
        }
        if ((is_jump(i->op) || is_exit(i->op)) && pc + 1 < n) leaders[pc + 1] = 1;
    }
    for (uint32_t pc = 0; pc < n; ++pc) {
        if (!leaders[pc]) continue;
        uint32_t id = ZrParser_Cfg_AppendBlock(cfg->state, cfg,
                          pc ? ZR_PARSER_CFG_BLOCK_STATEMENT : ZR_PARSER_CFG_BLOCK_ENTRY, NULL);
        if (id == ZR_PARSER_CFG_INVALID_BLOCK_ID) return ZR_FALSE;
        leaders[pc] = id + 1;
    }
    cfg->entryBlockId = 0;
    cfg->exitBlockId = ZrParser_Cfg_AppendBlock(cfg->state, cfg, ZR_PARSER_CFG_BLOCK_EXIT, NULL);
    if (cfg->exitBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID) return ZR_FALSE;
    for (uint32_t start = 0, end; start < n; start = end) {
        uint32_t id = leaders[start] - 1;
        uint32_t first = (uint32_t)flow->function.instructions.length;
        uint32_t pc = start;
        do {
            *badPc = pc;
            flow->range = locations[pc].range;
            if (!project_instruction(flow, &p->instructions[pc])) return ZR_FALSE;
            irEnds[pc] = (uint32_t)flow->function.instructions.length;
        } while (++pc < n && !leaders[pc]);
        end = pc;
        const ZrStrictInstruction *i = &p->instructions[end - 1];
        EZrParserCfgTerminatorKind term = ZR_PARSER_CFG_TERMINATOR_NONE;
        if (is_jump(i->op)) {
            term = ZR_PARSER_CFG_TERMINATOR_BRANCH;
            if (!ZrParser_Cfg_Connect(cfg, id, leaders[i->immediate] - 1,
                    i->op == ZR_STRICT_OP_JUMP ? ZR_PARSER_CFG_EDGE_NORMAL : ZR_PARSER_CFG_EDGE_FALSE_BRANCH, NULL))
                return ZR_FALSE;
        }
        if (is_exit(i->op)) {
            term = ZR_PARSER_CFG_TERMINATOR_RETURN;
            if (!ZrParser_Cfg_Connect(cfg, id, cfg->exitBlockId, ZR_PARSER_CFG_EDGE_RETURN, NULL)) return ZR_FALSE;
        } else if (i->op != ZR_STRICT_OP_JUMP) {
            if (end == n || !ZrParser_Cfg_Connect(cfg, id, leaders[end] - 1,
                    i->op == ZR_STRICT_OP_JUMP_IF_FALSE ? ZR_PARSER_CFG_EDGE_TRUE_BRANCH : ZR_PARSER_CFG_EDGE_NORMAL,
                    NULL)) return ZR_FALSE;
        }
        if (!ZrParser_SemanticIr_BindBlockRange(&flow->function, cfg, id, first, irEnds[end - 1] - first, term))
            return ZR_FALSE;
    }
    return ZrParser_SemanticIr_BindBlockRange(&flow->function, cfg, cfg->exitBlockId, irEnds[n - 1],
                                              0, ZR_PARSER_CFG_TERMINATOR_EXIT);
}

TZrBool ZrCli_StrictSemanticFlow_Check(SZrState *state, const ZrStrictProgram *p, const SZrCliStrictLocation *locations,
                                     TZrBool receiverIsLive, uint32_t *badPc, const char **message) {
    TZrBool hasPlaces = ZR_FALSE, ok = ZR_FALSE;
    for (uint32_t slot = 0; slot < p->slotCount; ++slot)
        if (p->kinds[slot] >= ZR_STRICT_VALUE_RECORD) { hasPlaces = ZR_TRUE; break; }
    if (!hasPlaces) return ZR_TRUE;
    StrictFlow flow = {.program = p, .places = calloc(p->slotCount, sizeof(*flow.places))};
    uint32_t *leaders = calloc(p->instructionCount, sizeof(*leaders));
    uint32_t *irEnds = calloc(p->instructionCount, sizeof(*irEnds));
    SZrSemanticFlowResult result;
    *badPc = 0;
    *message = "could not build strict ownership analysis";
    ZrParser_SemanticIrFunction_Init(state, &flow.function, 1, 1);
    ZrParser_SemanticFlowResult_Init(state, &result);
    if (!p->instructionCount || !flow.places || !leaders || !irEnds ||
        !project_places(&flow, locations, receiverIsLive) || !project_cfg(&flow, locations, leaders, irEnds, badPc) ||
        !ZrParser_SemanticFlow_Analyze(state, &flow.function, &flow.function.cfg, &result)) goto done;

    if (result.diagnostics.length) {
        const SZrSemanticFlowDiagnostic *diag = ZrCore_Array_Get(&result.diagnostics, 0);
        *message = diag->kind == ZR_SEMANTIC_FLOW_LOAN_CONFLICT ? "ownership analysis: conflicting borrow" :
            diag->kind == ZR_SEMANTIC_FLOW_UNINITIALIZED || diag->kind == ZR_SEMANTIC_FLOW_MAYBE_UNINITIALIZED ?
            "ownership analysis: use of an uninitialized value" : "ownership analysis: use after move or drop";
        *badPc = 0;
        while (*badPc + 1 < p->instructionCount && irEnds[*badPc] < diag->instructionId) ++*badPc;
        goto done;
    }
    ok = ZR_TRUE;
done:
    ZrParser_SemanticFlowResult_Free(state, &result);
    ZrParser_SemanticIrFunction_Free(state, &flow.function);
    free(flow.places);
    free(leaders);
    free(irEnds);
    return ok;
}
