#ifndef ZR_VM_PARSER_EXEC_IR_SOURCE_INTEGER_EXPRESSION_INTERNAL_H
#define ZR_VM_PARSER_EXEC_IR_SOURCE_INTEGER_EXPRESSION_INTERNAL_H
#include <stddef.h>
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_ir.h"
#include "zr_vm_core/exec_ir.h"
#include "zr_vm_core/function.h"

typedef enum ESourceIntegerPhase {
    SOURCE_INTEGER_ORIGINAL = 0,
    SOURCE_INTEGER_COMPACTED = 1
} ESourceIntegerPhase;
struct SSourceIntegerVisit;
/* Per-call, externally serialized borrowed witnesses. Only scratch is owned.
 * Initialize before prove and free before borrowed inputs change or die.
 * Never shallow-copy a live proof or use a flag as cross-call authorization. */
typedef struct SSourceIntegerProof {
    const SZrSemanticIrFunction *semantic;
    const SZrSemanticContext *context;
    TZrMetadataToken functionToken;
    TZrTypeId callableTypeId, integerTypeId;
    TZrUInt64 signatureHash;
    TZrUInt32 instructionCount, valueCount, placeCount;
    TZrUInt32 constantInstructionCount, returnInstructionId;
    TZrValueId returnValueId;
    void *scratch;
    size_t scratchBytes;
    const SZrAstNode **astByValue;
    TZrUInt32 *placeInstructionIds;
    TZrUInt32 *instructionIds[2];
    struct SSourceIntegerVisit *visits;
    const SZrExecIrFunction *checkedFunctions[2];
    const SZrAstNode *checkedScript;
    const SZrFunctionSourceCallableIdentity *checkedIdentity;
    const SZrArray *compilerConstants;
    const SZrTypeValue *functionConstants;
    TZrUInt32 functionConstantCount;
    TZrBool canonicalProven, astProven, poolsProven;
} SSourceIntegerProof;
void source_integer_proof_init(SSourceIntegerProof *proof);
TZrBool source_integer_prove(const SZrSemanticIrFunction *semantic,
        const SZrSemanticContext *context, TZrMetadataToken functionToken,
        SSourceIntegerProof *output, SZrExecIrDiagnostic *diagnostic);
TZrBool source_integer_check_ast(SSourceIntegerProof *proof,
        const SZrAstNode *script, const SZrFunctionSourceCallableIdentity *identity,
        SZrExecIrDiagnostic *diagnostic);
TZrBool source_integer_check_pools(SSourceIntegerProof *proof,
        const SZrArray *compilerConstants, const SZrTypeValue *functionConstants,
        TZrUInt32 functionConstantCount, SZrExecIrDiagnostic *diagnostic);
TZrBool source_integer_check_function(SSourceIntegerProof *proof,
        const SZrExecIrFunction *function, ESourceIntegerPhase phase,
        SZrExecIrDiagnostic *diagnostic);
TZrBool source_integer_constant_at(const SSourceIntegerProof *proof,
        TZrUInt32 poolIndex, SZrExecIrConstant *output,
        SZrExecIrDiagnostic *diagnostic);
TZrBool source_integer_pure_instruction(const SZrExecIrInstruction *instruction);
/* Opcode/metadata only: no source, canonical type, operand or frame authority. */
static inline TZrBool source_integer_pure_fields(const SZrExecIrInstruction *r) {
    if (r == ZR_NULL || r->flags != 0u || r->phiRange.count != 0u ||
        r->successorRange.count != 0u || r->matchTypeToken != 0u ||
        r->memoryIn.count != 0u || r->memoryOut.count != 0u ||
        r->effectIn != 0u || r->effectOut != 0u ||
        r->deoptId != 0u || r->bindingRow != 0u) return ZR_FALSE;
    switch (r->opcode) {
        case ZR_EXEC_IR_OPCODE_CONSTANT: return ZR_TRUE;
        case ZR_EXEC_IR_OPCODE_NOP:
        case ZR_EXEC_IR_OPCODE_ADD:
        case ZR_EXEC_IR_OPCODE_SUB:
        case ZR_EXEC_IR_OPCODE_MUL:
        case ZR_EXEC_IR_OPCODE_RETURN: return (TZrBool)(r->layoutId == 0u);
        default: return ZR_FALSE;
    }
}
void source_integer_proof_free(SSourceIntegerProof *proof);
#endif
