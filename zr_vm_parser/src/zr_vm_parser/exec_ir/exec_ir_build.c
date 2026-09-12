#include "zr_vm_parser/exec_ir_builder.h"
#include "zr_vm_parser/semantic_ir.h"

#include <stdlib.h>
#include <string.h>

static void diag_missing(SZrExecIrDiagnostic *d, const SZrExecIrFunction *f,
                         TZrUInt32 block, TZrUInt32 instruction) {
    if (d != ZR_NULL) {
        memset(d, 0, sizeof(*d));
        d->code = ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;
        d->functionToken = f != ZR_NULL ? f->functionToken : 0u;
        d->blockId = block;
        d->instructionId = instruction;
    }
}

static EZrExecIrOpcode map_opcode(EZrSemanticIrOpcode op) {
    switch (op) {
        case ZR_SEMANTIC_IR_CONSTANT: return ZR_EXEC_IR_OPCODE_CONSTANT;
        case ZR_SEMANTIC_IR_CONVERT: return ZR_EXEC_IR_OPCODE_CONVERT;
        case ZR_SEMANTIC_IR_PLACE_BASE: return ZR_EXEC_IR_OPCODE_PLACE_BASE;
        case ZR_SEMANTIC_IR_PLACE_PROJECT: return ZR_EXEC_IR_OPCODE_PLACE_PROJECT;
        case ZR_SEMANTIC_IR_LOAD: return ZR_EXEC_IR_OPCODE_LOAD;
        case ZR_SEMANTIC_IR_STORE: return ZR_EXEC_IR_OPCODE_STORE;
        case ZR_SEMANTIC_IR_MOVE: return ZR_EXEC_IR_OPCODE_MOVE;
        case ZR_SEMANTIC_IR_COPY: return ZR_EXEC_IR_OPCODE_COPY;
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
        case ZR_SEMANTIC_IR_INITIALIZE: return ZR_EXEC_IR_OPCODE_COPY;
        default: return ZR_EXEC_IR_OPCODE_INVALID;
    }
}

static TZrBool append_source(SZrExecIrFunction *f, const SZrSemanticIrInstruction *in) {
    SZrExecIrSourceMap *p;
    if (f->sourceMapCount == f->sourceMapCapacity) {
        TZrUInt32 cap = f->sourceMapCapacity == 0u ? 8u : f->sourceMapCapacity * 2u;
        p = (SZrExecIrSourceMap *)realloc(f->sourceMaps, (size_t)cap * sizeof(*p));
        if (p == ZR_NULL) return ZR_FALSE;
        f->sourceMaps = p; f->sourceMapCapacity = cap;
    }
    p = &f->sourceMaps[f->sourceMapCount++];
    p->sourceId = in->id; p->instructionId = in->id;
    p->startOffset = (TZrUInt32)in->sourceRange.start.offset;
    p->endOffset = (TZrUInt32)in->sourceRange.end.offset;
    p->startLine = (TZrUInt32)in->sourceRange.start.line;
    p->startColumn = (TZrUInt32)in->sourceRange.start.column;
    p->endLine = (TZrUInt32)in->sourceRange.end.line;
    p->endColumn = (TZrUInt32)in->sourceRange.end.column;
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_Build(const struct SZrSemanticIrFunction *semanticFunction,
                              const SZrExecIrBuildOptions *options,
                              SZrExecIrFunction *output,
                              SZrExecIrDiagnostic *diagnostic) {
    const SZrSemanticIrFunction *s = semanticFunction;
    TZrUInt32 i;
    (void)options;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (s == ZR_NULL || output == ZR_NULL || !s->instructions.isValid || !s->cfg.blocks.isValid) {
        diag_missing(diagnostic, output, 0u, 0u); return ZR_FALSE;
    }
    ZrCore_ExecIr_FunctionInit(output);
    output->functionToken = (TZrMetadataToken)s->symbolId;
    output->signatureHash = (TZrUInt64)s->callableTypeId;
    output->contract.schemaVersion = ZR_EXECUTION_CONTRACT_SCHEMA_VERSION;
    output->contract.abiVersion = ZR_EXECUTION_CONTRACT_ABI_VERSION;
    output->contract.logicalVersion = ZR_EXECUTION_CONTRACT_LOGICAL_VERSION;
    output->contract.targetToken = output->functionToken;
    output->contract.signatureHash = output->signatureHash;

    for (i = 0u; i < (TZrUInt32)s->values.length; ++i) {
        const SZrSemanticIrValue *v = (const SZrSemanticIrValue *)ZrCore_Array_Get((SZrArray *)&s->values, i);
        if (ZrCore_ExecIr_FunctionAddValue(output, (TZrMetadataToken)v->typeId,
                    ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN) == ZR_EXEC_IR_VALUE_ID_INVALID) {
            diag_missing(diagnostic, output, 0u, 0u); ZrCore_ExecIr_FreeFunction(output); return ZR_FALSE;
        }
    }
    for (i = 0u; i < (TZrUInt32)s->cfg.blocks.length; ++i) {
        const SZrParserCfgBlock *b = (const SZrParserCfgBlock *)ZrCore_Array_Get((SZrArray *)&s->cfg.blocks, i);
        TZrUInt32 id = ZrCore_ExecIr_FunctionAddBlock(output,
                (i == s->cfg.entryBlockId ? ZR_EXEC_IR_BLOCK_FLAG_ENTRY : 0u) |
                (b->kind == ZR_PARSER_CFG_BLOCK_CLEANUP ? ZR_EXEC_IR_BLOCK_FLAG_CLEANUP : 0u));
        if (id == ZR_EXEC_IR_BLOCK_ID_INVALID) { ZrCore_ExecIr_FreeFunction(output); return ZR_FALSE; }
    }
    output->entryBlockId = s->cfg.entryBlockId < s->cfg.blocks.length
                               ? s->cfg.entryBlockId + 1u : ZR_EXEC_IR_BLOCK_ID_INVALID;
    for (i = 0u; i < (TZrUInt32)s->cfg.blocks.length; ++i) {
        const SZrParserCfgBlock *b = (const SZrParserCfgBlock *)ZrCore_Array_Get((SZrArray *)&s->cfg.blocks, i);
        SZrExecIrBlock *db = ZrCore_ExecIr_FunctionBlockAt(output, i + 1u);
        TZrUInt32 j;
        TZrUInt32 edgeCount = b->outgoingEdges.isValid ? (TZrUInt32)b->outgoingEdges.length : b->successorCount;
        for (j = 0u; j < edgeCount; ++j) {
            TZrUInt32 succ;
            if (b->outgoingEdges.isValid) {
                const SZrParserCfgEdge *edge = (const SZrParserCfgEdge *)ZrCore_Array_Get((SZrArray *)&b->outgoingEdges, j);
                succ = edge->toBlockId;
            } else {
                succ = b->successors[j < ZR_PARSER_CFG_INLINE_SUCCESSOR_CAPACITY ? j : 0u];
            }
            if (succ < s->cfg.blocks.length) {
                TZrExecIrBlockId sid = succ + 1u;
                ZrCore_ExecIr_FunctionAppendSuccessors(output, &sid, 1u, &db->successorRange);
                SZrExecIrBlock *sb = ZrCore_ExecIr_FunctionBlockAt(output, sid);
                ZrCore_ExecIr_FunctionAppendPredecessors(output, &db->id, 1u, &sb->predecessorRange);
            }
        }
        if (b->instructionCount != 0u && b->firstInstructionIndex + b->instructionCount <= s->instructions.length) {
            for (j = 0u; j < b->instructionCount; ++j) {
                const SZrSemanticIrInstruction *in = (const SZrSemanticIrInstruction *)ZrCore_Array_Get((SZrArray *)&s->instructions, b->firstInstructionIndex + j);
                SZrExecIrInstruction x; SZrExecIrRange rr = {0u, 0u}, orr = {0u, 0u};
                memset(&x, 0, sizeof(x)); x.opcode = (TZrUInt16)map_opcode(in->opcode); x.sourceId = in->id;
                if (x.opcode == ZR_EXEC_IR_OPCODE_INVALID) { diag_missing(diagnostic, output, db->id, in->id); ZrCore_ExecIr_FreeFunction(output); return ZR_FALSE; }
                x.typeToken = (TZrExecIrTypeToken)in->typeId;
                if (in->resultValueId != ZR_VALUE_ID_INVALID) {
                    TZrExecIrValueId v = in->resultValueId;
                    if (!ZrCore_ExecIr_FunctionAppendResults(output, &v, 1u, &rr)) { diag_missing(diagnostic, output, db->id, in->id); ZrCore_ExecIr_FreeFunction(output); return ZR_FALSE; }
                }
                if (in->operandCount != 0u && in->operandStart + in->operandCount <= s->valueOperands.length) {
                    const TZrValueId *ops = (const TZrValueId *)ZrCore_Array_Get((SZrArray *)&s->valueOperands, in->operandStart);
                    if (!ZrCore_ExecIr_FunctionAppendOperands(output, ops, in->operandCount, &orr)) { ZrCore_ExecIr_FreeFunction(output); return ZR_FALSE; }
                }
                x.results = rr; x.operands = orr;
                if (!ZrCore_ExecIr_FunctionAppendInstruction(output, &x, ZR_NULL) || !append_source(output, in)) { diag_missing(diagnostic, output, db->id, in->id); ZrCore_ExecIr_FreeFunction(output); return ZR_FALSE; }
            }
            db->instructionRange.start = b->firstInstructionIndex + 1u;
            db->instructionRange.count = b->instructionCount;
            db->terminatorInstructionId = db->instructionRange.start + db->instructionRange.count - 1u;
        }
    }
    return ZrParser_ExecIr_ComputeDominators(output, diagnostic) && ZrParser_ExecIr_BuildSsa(output, diagnostic);
}

TZrBool ZrParser_ExecIr_BuildModule(const SZrExecIrBuildInput *input, SZrExecIrModule *output, SZrExecIrDiagnostic *diagnostic) {
    TZrExecIrFunctionId id; SZrExecIrFunction *f;
    if (input == ZR_NULL || output == ZR_NULL || input->semanticFunction == ZR_NULL) return ZR_FALSE;
    if (!ZrCore_ExecIr_ModuleAddFunction(output, input->functionToken, input->signatureHash, &id)) return ZR_FALSE;
    f = ZrCore_ExecIr_ModuleFunctionAt(output, id);
    return ZrParser_ExecIr_Build(input->semanticFunction, &input->options, f, diagnostic);
}
