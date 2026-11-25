#include "compiler/strict_scalar.h"

#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/function_exec_ir.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/strict_no_gc.h"

static TZrBool supports_expression(const SZrAstNode *node, void *context) {
    const TZrChar *op;
    (void)context;
    if (node->type == ZR_AST_INTEGER_LITERAL) return ZR_TRUE;
    if (node->type != ZR_AST_BINARY_EXPRESSION) return ZR_FALSE;
    op = node->data.binaryExpression.op.op;
    return op && (!strcmp(op, "+") || !strcmp(op, "-") || !strcmp(op, "*"));
}

TZrBool ZrCli_StrictScalar_SupportsSource(SZrAstNode *root) {
    const SZrAstNodeArray *statements;
    const SZrAstNode *entry;
    if (!root || root->type != ZR_AST_SCRIPT) return ZR_FALSE;
    statements = root->data.script.statements;
    if (!statements || statements->count != 1u || !statements->nodes) return ZR_FALSE;
    entry = statements->nodes[0];
    return entry && entry->type == ZR_AST_RETURN_STATEMENT && !entry->data.returnStatement.isReferenceReturn &&
           ZrParser_Ast_Visit(entry->data.returnStatement.expr, supports_expression, ZR_NULL);
}

/* Map a verified compiler-owned SSA block into the dependency-free strict
 * wire model. Fail closed on effects, call bindings, roots, and unknown data. */
static ZrStrictProgramStatus project_scalar(const SZrFunction *function, ZrStrictProgram *out) {
    const SZrExecIrModule *module = ZrCore_Function_GetExecIr(function);
    TZrExecIrTypeToken scalarType;
    ZrStrictProgram p = {0};
    SZrExecIrDiagnostic diagnostic;
    const SZrExecIrFunction *f;
    ZrStrictProgramStatus status = ZR_STRICT_PROGRAM_BAD_LAYOUT;
    if (!function->hasSourceCallableIdentity || function->sourceCallableIdentity.returnPrimitive != ZR_VALUE_TYPE_INT64 ||
        !module || module->functionCount != 1u || !module->functions || module->layoutCount != 1u ||
        !module->layouts || module->layouts[0].byteSize != sizeof(int64_t) ||
        !ZrCore_ExecIr_VerifyModule(module, &diagnostic)) return status;
    f = &module->functions[0];
    scalarType = module->layouts[0].typeToken;
    if (f->blockCount != 1u || !f->blocks || f->blocks[0].id != f->entryBlockId ||
        f->blocks[0].instructions.offset != 0u || f->blocks[0].instructions.count != f->instructionCount ||
        !f->frameLayout || f->frameLayout->parameterCount || f->gcMap || f->gcRootCount || f->deoptStateCount ||
        f->bindingRowCount || f->memoryTokenCount || f->phiCount || f->contract.declaredEffects ||
        !f->valueCount || f->valueCount > ZR_STRICT_MAX_SLOTS ||
        !f->instructionCount || f->instructionCount > ZR_STRICT_MAX_INSTRUCTIONS ||
        !ZrCore_ExecIr_VerifyFunction(f, ZR_EXEC_IR_VERIFY_ALL, &diagnostic)) return status;
    p.kinds = malloc(f->valueCount * sizeof(*p.kinds));
    p.instructions = calloc(f->instructionCount, sizeof(*p.instructions));
    p.slotCount = f->valueCount;
    if (!p.kinds || !p.instructions) {
        status = ZR_STRICT_PROGRAM_OUT_OF_MEMORY;
        goto fail;
    }
    for (TZrUInt32 v = 0; v < f->valueCount; ++v) {
        const SZrExecIrValue *value = &f->values[v];
        if (value->id != v + 1u || value->ownership != ZR_EXEC_IR_OWNERSHIP_UNKNOWN ||
            value->nullability != ZR_EXEC_IR_NULLABILITY_NONNULL || value->flags || value->typeToken != scalarType) {
            goto fail;
        }
        p.kinds[v] = ZR_STRICT_VALUE_I64;
    }
    status = ZR_STRICT_PROGRAM_BAD_INSTRUCTION;
    for (TZrUInt32 pc = 0; pc < f->instructionCount; ++pc) {
        const SZrExecIrInstruction *in = &f->instructions[pc];
        ZrStrictInstruction i = {0};
        TZrUInt32 argc = 0;
        TZrUInt32 results = 1;
        if (in->flags || in->effectIn || in->effectOut || in->memoryIn.count || in->memoryOut.count ||
            in->bindingRow || in->deoptId || in->phiRange.count || in->successorRange.count || in->matchTypeToken) goto fail;
        switch (in->opcode) {
            case ZR_EXEC_IR_OPCODE_NOP: continue;
            case ZR_EXEC_IR_OPCODE_CONSTANT: {
                const SZrExecIrConstant *c;
                if (in->layoutId >= module->constantCount || in->results.count != 1u) goto fail;
                c = &module->constants[in->layoutId];
                if (c->flags || c->typeToken != in->typeToken) goto fail;
                i.op = ZR_STRICT_OP_CONST;
                i.immediate = c->bits;
                break;
            }
            case ZR_EXEC_IR_OPCODE_COPY: i.op = ZR_STRICT_OP_COPY; argc = 1; break;
            case ZR_EXEC_IR_OPCODE_ADD: i.op = ZR_STRICT_OP_ADD; argc = 2; break;
            case ZR_EXEC_IR_OPCODE_SUB: i.op = ZR_STRICT_OP_SUB; argc = 2; break;
            case ZR_EXEC_IR_OPCODE_MUL: i.op = ZR_STRICT_OP_MUL; argc = 2; break;
            case ZR_EXEC_IR_OPCODE_DIV: i.op = ZR_STRICT_OP_DIV; argc = 2; break;
            case ZR_EXEC_IR_OPCODE_NEG: i.op = ZR_STRICT_OP_NEG; argc = 1; break;
            case ZR_EXEC_IR_OPCODE_RETURN: i.op = ZR_STRICT_OP_RETURN; argc = 1; results = 0; break;
            default: goto fail;
        }
        if (in->operands.count != argc || in->results.count != results ||
            (in->opcode != ZR_EXEC_IR_OPCODE_CONSTANT && in->layoutId)) goto fail;
        if (results) {
            i.dst = f->resultPool[in->results.offset] - 1u;
            if (in->typeToken != scalarType) goto fail;
        }
        if (argc) i.lhs = f->operandPool[in->operands.offset] - 1u;
        if (argc == 2) i.rhs = f->operandPool[in->operands.offset + 1u] - 1u;
        p.instructions[p.instructionCount++] = i;
    }
    status = ZrStrictProgram_Verify(&p, NULL);
    if (status == ZR_STRICT_PROGRAM_OK) {
        *out = p;
        return status;
    }
fail:
    ZrStrictProgram_Free(&p);
    return status;
}

ZrStrictProgramStatus ZrCli_StrictScalar_Compile(SZrState *state, const TZrChar *source, TZrSize length,
                                                const TZrChar *path, ZrStrictProgram *out) {
    SZrString *sourceName = ZrCore_String_Create(state, (TZrNativeString)path, strlen(path));
    SZrFunction *result;
    ZrStrictProgramStatus status;
    if (!sourceName) return ZR_STRICT_PROGRAM_OUT_OF_MEMORY;
    result = ZrParser_StrictNoGc_CompileScalarIr(state, source, length, sourceName);
    if (!result) return ZR_STRICT_PROGRAM_BAD_INSTRUCTION;
    status = project_scalar(result, out);
    ZrCore_Function_Free(state, result);
    return status;
}
