#include "compiler_semantic_ir_scalar_scratch_internal.h"
#include "compiler_semantic_ir_scalar_scratch_rules.h"

#include <string.h>

#include "zr_vm_parser/canonical_type.h"
#include "../semantic_ir_scalar_scratch_internal.h"

TZrBool compiler_semantic_ir_record_literal_scalar_scratch_proof(
        SZrCompilerState *compiler,
        TZrPlaceId placeId,
        TZrValueId valueId,
        EZrValueType literalType) {
    const SZrSemanticIrValue *value;
    const SZrSemanticIrInstruction *constant;
    const SZrCanonicalTypeNode *canonicalType;
    const SZrTypeValue *pooledConstant;
    SZrSemanticIrScalarScratchProof proof;

    if (compiler == ZR_NULL || compiler->state == ZR_NULL ||
        compiler->semanticContext == ZR_NULL ||
        !compiler->preSemanticIrInitialized ||
        (literalType != ZR_VALUE_TYPE_BOOL &&
         literalType != ZR_VALUE_TYPE_INT64) ||
        placeId == ZR_PLACE_ID_INVALID || valueId == ZR_VALUE_ID_INVALID) {
        return ZR_FALSE;
    }

    value = ZrParser_SemanticIr_Value(&compiler->preSemanticIr, valueId);
    if (value == ZR_NULL ||
        value->definitionInstructionId ==
                ZR_SEMANTIC_INSTRUCTION_ID_INVALID) {
        return ZR_FALSE;
    }
    constant = ZrParser_SemanticIr_InstructionAt(
            &compiler->preSemanticIr,
            value->definitionInstructionId - 1U);
    if (constant == ZR_NULL ||
        constant->opcode != ZR_SEMANTIC_IR_CONSTANT ||
        constant->resultValueId != valueId ||
        constant->typeId != value->typeId ||
        constant->hasConstantPoolIndex == ZR_FALSE ||
        constant->constantPoolIndex >= compiler->constants.length) {
        return ZR_FALSE;
    }

    canonicalType = ZrParser_CanonicalType_Find(
            compiler->semanticContext, value->typeId);
    if (canonicalType == ZR_NULL ||
        canonicalType->kind != ZR_CANONICAL_TYPE_PRIMITIVE) {
        return ZR_FALSE;
    }

    pooledConstant = (const SZrTypeValue *)ZrCore_Array_Get(
            (SZrArray *)&compiler->constants,
            constant->constantPoolIndex);
    if (pooledConstant == ZR_NULL ||
        !compiler_semantic_ir_scalar_scratch_literal_types_match(
                canonicalType->data.primitive.valueType,
                literalType,
                (TZrBool)(constant->constantPoolIndex <
                          compiler->constants.length),
                pooledConstant->type)) {
        return ZR_FALSE;
    }

    memset(&proof, 0, sizeof(proof));
    proof.placeId = placeId;
    proof.constantValueId = valueId;
    proof.typeId = value->typeId;
    proof.valueType = literalType;
    if (!zr_parser_semantic_ir_scalar_scratch_matches_direct_initializer(
                &compiler->preSemanticIr, &proof)) {
        return ZR_FALSE;
    }
    return zr_parser_semantic_ir_scalar_scratch_append_proof(
            &compiler->preSemanticIr, &proof);
}
