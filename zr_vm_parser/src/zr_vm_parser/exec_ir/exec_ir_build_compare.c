#include "exec_ir_build_compare.h"
#include "../semantic_ir_compare_internal.h"

#include <string.h>

EZrExecIrOpcode zr_parser_exec_ir_map_semantic_opcode(const SZrSemanticIrInstruction *instruction) {
    if (instruction == ZR_NULL) {
        return ZR_EXEC_IR_OPCODE_INVALID;
    }
    switch (instruction->opcode) {
        case ZR_SEMANTIC_IR_CONSTANT: return ZR_EXEC_IR_OPCODE_CONSTANT;
        case ZR_SEMANTIC_IR_CONVERT: return ZR_EXEC_IR_OPCODE_CONVERT;
        case ZR_SEMANTIC_IR_TYPE_TEST: return ZR_EXEC_IR_OPCODE_TYPE_TEST;
        case ZR_SEMANTIC_IR_PLACE_BASE: return ZR_EXEC_IR_OPCODE_PLACE_BASE;
        case ZR_SEMANTIC_IR_PLACE_PROJECT: return ZR_EXEC_IR_OPCODE_PLACE_PROJECT;
        case ZR_SEMANTIC_IR_LOAD: return ZR_EXEC_IR_OPCODE_LOAD;
        case ZR_SEMANTIC_IR_STORE: return ZR_EXEC_IR_OPCODE_STORE;
        case ZR_SEMANTIC_IR_MOVE: return ZR_EXEC_IR_OPCODE_MOVE;
        case ZR_SEMANTIC_IR_COPY: return ZR_EXEC_IR_OPCODE_COPY;
        case ZR_SEMANTIC_IR_BORROW_SHARED:
        case ZR_SEMANTIC_IR_BORROW_MUT:
        case ZR_SEMANTIC_IR_RESERVE_BORROW_MUT:
        case ZR_SEMANTIC_IR_REBORROW:
        case ZR_SEMANTIC_IR_DEREFERENCE:
            return ZR_EXEC_IR_OPCODE_COPY;
        case ZR_SEMANTIC_IR_ACTIVATE_LOAN:
        case ZR_SEMANTIC_IR_END_LOAN:
            return ZR_EXEC_IR_OPCODE_NOP;
        case ZR_SEMANTIC_IR_OWN_CONSTRUCT:
            return instruction->ownershipOperation == ZR_SEMANTIC_OWNERSHIP_WAKE
                    ? ZR_EXEC_IR_OPCODE_WAKE
                    : instruction->ownershipOperation ==
                                   ZR_SEMANTIC_OWNERSHIP_UNIQUE ||
                           instruction->ownershipOperation ==
                                   ZR_SEMANTIC_OWNERSHIP_INTO_GC_BOX ||
                           instruction->ownershipOperation ==
                                   ZR_SEMANTIC_OWNERSHIP_RETURN_TO_GC
                    ? ZR_EXEC_IR_OPCODE_MOVE
                    : ZR_EXEC_IR_OPCODE_COPY;
        case ZR_SEMANTIC_IR_DROP: return ZR_EXEC_IR_OPCODE_DROP;
        case ZR_SEMANTIC_IR_CALL_TYPED:
        case ZR_SEMANTIC_IR_CALL_VIRTUAL:
        case ZR_SEMANTIC_IR_CALL_DYNAMIC:
        case ZR_SEMANTIC_IR_CALL_META: return ZR_EXEC_IR_OPCODE_CALL;
        case ZR_SEMANTIC_IR_BRANCH: return ZR_EXEC_IR_OPCODE_BRANCH;
        case ZR_SEMANTIC_IR_SWITCH: return ZR_EXEC_IR_OPCODE_SWITCH;
        case ZR_SEMANTIC_IR_RETURN: return ZR_EXEC_IR_OPCODE_RETURN;
        case ZR_SEMANTIC_IR_THROW: return ZR_EXEC_IR_OPCODE_THROW;
        case ZR_SEMANTIC_IR_YIELD_SUSPEND: return ZR_EXEC_IR_OPCODE_SUSPEND;
        case ZR_SEMANTIC_IR_GC_NEW: return ZR_EXEC_IR_OPCODE_ALLOC;
        case ZR_SEMANTIC_IR_INITIALIZE: return ZR_EXEC_IR_OPCODE_STORE;
        case ZR_SEMANTIC_IR_ITER_INIT: return ZR_EXEC_IR_OPCODE_ITER_INIT;
        case ZR_SEMANTIC_IR_ITER_MOVE_NEXT:
            return ZR_EXEC_IR_OPCODE_ITER_MOVE_NEXT;
        case ZR_SEMANTIC_IR_ITER_CURRENT:
            return ZR_EXEC_IR_OPCODE_ITER_CURRENT;
        case ZR_SEMANTIC_IR_EXCEPTION_PAYLOAD:
            return ZR_EXEC_IR_OPCODE_EXCEPTION_PAYLOAD;
        case ZR_SEMANTIC_IR_ADD: return ZR_EXEC_IR_OPCODE_ADD;
        case ZR_SEMANTIC_IR_SUB: return ZR_EXEC_IR_OPCODE_SUB;
        case ZR_SEMANTIC_IR_MUL: return ZR_EXEC_IR_OPCODE_MUL;
        case ZR_SEMANTIC_IR_DIV: return ZR_EXEC_IR_OPCODE_DIV;
        case ZR_SEMANTIC_IR_COMPARE: return ZR_EXEC_IR_OPCODE_COMPARE;
        default: return ZR_EXEC_IR_OPCODE_INVALID;
    }
}

TZrBool zr_parser_exec_ir_lower_compare_metadata(
        const SZrSemanticIrFunction *source, const SZrSemanticIrInstruction *in,
        SZrExecIrInstruction *out, const SZrExecIrFunction *function,
        TZrExecIrBlockId block, SZrExecIrDiagnostic *diagnostic) {
    if (semantic_ir_compare_instruction_valid(source, in)) {
        if (in->opcode == ZR_SEMANTIC_IR_COMPARE)
            out->typeToken = in->comparisonPredicate;
        return ZR_TRUE;
    }
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE;
        diagnostic->functionToken = function->functionToken;
        diagnostic->blockId = block;
        diagnostic->instructionId = in->id;
        diagnostic->sourceId = in->id;
        diagnostic->expectedVersion = ZR_EXEC_IR_COMPARE_KIND_LESS;
        diagnostic->actualVersion = in->comparisonPredicate;
    }
    return ZR_FALSE;
}

