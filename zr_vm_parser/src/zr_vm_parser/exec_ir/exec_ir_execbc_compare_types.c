#include "exec_ir_execbc_compare_types.h"

static TZrBool canonical_value_type(
        const SZrExecBcProjection *projection, const SZrSemanticContext *context,
        TZrExecIrValueId id, EZrValueType expected, TZrExecIrTypeToken *type) {
    TZrUInt32 slot;
    const SZrExecIrValue *value;
    const SZrCanonicalTypeNode *node;
    if (id == 0u || id > projection->valueSlotCount ||
        projection->valueSlots == ZR_NULL || projection->slotValues == ZR_NULL)
        return ZR_FALSE;
    slot = projection->valueSlots[id - 1u];
    if (slot >= projection->physicalSlotCount) return ZR_FALSE;
    value = &projection->slotValues[slot];
    if (value->id != id || value->typeToken == 0u) return ZR_FALSE;
    node = ZrParser_CanonicalType_Find(context, value->typeToken);
    if (node == ZR_NULL || node->id != value->typeToken ||
        node->kind != ZR_CANONICAL_TYPE_PRIMITIVE ||
        node->data.primitive.valueType != expected) return ZR_FALSE;
    *type = value->typeToken;
    return ZR_TRUE;
}

TZrBool execbc_vm_prepare_canonical_compare_types(
        const SZrExecBcProjection *projection, const SZrSemanticContext *context,
        SZrExecBcInstruction *staged, SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 index;
    if (projection == ZR_NULL || context == ZR_NULL ||
        (projection->instructionCount != 0u && staged == ZR_NULL)) return ZR_FALSE;
    for (index = 0u; index < projection->instructionCount; ++index) {
        const SZrExecBcInstruction *in = &projection->instructions[index];
        TZrExecIrTypeToken leftType = 0u, rightType = 0u, resultType = 0u;
        if (in->opcode != ZR_EXEC_IR_OPCODE_COMPARE) continue;
        if ((in->typeToken != ZR_EXEC_IR_COMPARE_KIND_LESS &&
             in->typeToken != ZR_EXEC_IR_COMPARE_KIND_GREATER) ||
            in->operands.count != 2u ||
            in->results.count != 1u || projection->operands == ZR_NULL ||
            projection->results == ZR_NULL ||
            in->operands.start > projection->operandCount ||
            in->operands.count > projection->operandCount - in->operands.start ||
            in->results.start >= projection->resultCount ||
            !canonical_value_type(projection, context,
                    projection->operands[in->operands.start], ZR_VALUE_TYPE_INT64, &leftType) ||
            !canonical_value_type(projection, context,
                    projection->operands[in->operands.start + 1u], ZR_VALUE_TYPE_INT64, &rightType) ||
            !canonical_value_type(projection, context,
                    projection->results[in->results.start], ZR_VALUE_TYPE_BOOL, &resultType) ||
            leftType != rightType ||
            (in->matchTypeToken != 0u && in->matchTypeToken != leftType)) {
            return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                    projection, 0u, index + 1u, in->sourceId,
                    ZR_VALUE_TYPE_BOOL, resultType);
        }
        /* Derive absent metadata or preserve the proven canonical operand ID.
         * The ordinary resolver maps this staged ID to runtime INT64. */
        staged[index].matchTypeToken = leftType;
    }
    return ZR_TRUE;
}
