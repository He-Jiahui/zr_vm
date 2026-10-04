#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/semantic.h"
#include "exec_ir_execbc_vm_internal.h"

static TZrBool execbc_vm_resolve_canonical_type(
        const SZrSemanticContext *context,
        TZrExecIrTypeToken canonicalToken,
        TZrExecIrTypeToken *runtimeToken) {
    const SZrCanonicalTypeNode *node = ZrParser_CanonicalType_Find(
            context, (TZrTypeId)canonicalToken);
    if (node == ZR_NULL || node->id != (TZrTypeId)canonicalToken ||
        node->kind != ZR_CANONICAL_TYPE_PRIMITIVE ||
        (node->data.primitive.valueType != ZR_VALUE_TYPE_BOOL &&
         node->data.primitive.valueType != ZR_VALUE_TYPE_INT64)) {
        return ZR_FALSE;
    }
    *runtimeToken = (TZrExecIrTypeToken)node->data.primitive.valueType;
    return ZR_TRUE;
}

static void *execbc_vm_copy_type_array(
        const void *source, TZrUInt32 count, size_t elementSize,
        const SZrExecBcProjection *projection,
        SZrExecIrDiagnostic *diagnostic) {
    void *copy;
    if (count == 0u) return ZR_NULL;
    if (source == ZR_NULL) {
        execbc_vm_set_diagnostic(diagnostic,
                ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                projection, 0u, 0u, 0u, count, 0u);
        return ZR_NULL;
    }
    if ((size_t)count > SIZE_MAX / elementSize) {
        execbc_vm_set_diagnostic(diagnostic,
                ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                projection, 0u, 0u, 0u, UINT32_MAX, count);
        return ZR_NULL;
    }
    copy = malloc((size_t)count * elementSize);
    if (copy == ZR_NULL) {
        execbc_vm_set_diagnostic(diagnostic,
                ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                projection, 0u, 0u, 0u, count, 0u);
        return ZR_NULL;
    }
    memcpy(copy, source, (size_t)count * elementSize);
    return copy;
}

TZrBool ZrParser_ExecBcProjection_MaterializeVmFunctionWithCanonicalTypes(
        SZrState *state, const SZrExecBcProjection *projection,
        const SZrSemanticContext *context,
        SZrExecBcVmEmission *output, SZrExecIrDiagnostic *diagnostic) {
    SZrExecBcProjection resolved;
    SZrExecBcInstruction *instructions = ZR_NULL;
    SZrExecIrValue *values = ZR_NULL;
    SZrExecIrConstant *constants = ZR_NULL;
    TZrUInt32 index;
    TZrBool succeeded = ZR_FALSE;

    if (output != ZR_NULL) memset(output, 0, sizeof(*output));
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (state == ZR_NULL || projection == ZR_NULL || context == ZR_NULL ||
        output == ZR_NULL) {
        return execbc_vm_fail(diagnostic,
                ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                projection, 0u, 0u, 0u, 1u, 0u);
    }
    if (!context->canonicalTypes.isValid ||
        context->canonicalTypes.elementSize != sizeof(SZrCanonicalTypeNode) ||
        context->canonicalTypes.length > context->canonicalTypes.capacity ||
        context->canonicalTypes.capacity > SIZE_MAX / sizeof(SZrCanonicalTypeNode) ||
        (context->canonicalTypes.length != 0u &&
         context->canonicalTypes.head == ZR_NULL) ||
        projection->physicalSlotCount > UINT16_MAX + 1u) {
        return execbc_vm_fail(diagnostic,
                ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                projection, 0u, 0u, 0u, UINT16_MAX + 1u,
                projection->physicalSlotCount);
    }

    resolved = *projection;
    instructions = (SZrExecBcInstruction *)execbc_vm_copy_type_array(
            projection->instructions, projection->instructionCount,
            sizeof(*instructions), projection, diagnostic);
    if (projection->instructionCount != 0u && instructions == ZR_NULL) goto cleanup;
    values = (SZrExecIrValue *)execbc_vm_copy_type_array(
            projection->slotValues, projection->physicalSlotCount,
            sizeof(*values), projection, diagnostic);
    if (projection->physicalSlotCount != 0u && values == ZR_NULL) goto cleanup;
    constants = (SZrExecIrConstant *)execbc_vm_copy_type_array(
            projection->constants, projection->constantCount,
            sizeof(*constants), projection, diagnostic);
    if (projection->constantCount != 0u && constants == ZR_NULL) goto cleanup;
    resolved.instructions = instructions;
    resolved.slotValues = values;
    resolved.constants = constants;

    /* Only the three type-bearing arrays are owned here; all CFG, source,
     * effect, GC and phi metadata remains borrowed and fully validated. */
    for (index = 0u; index < projection->physicalSlotCount; ++index) {
        if (values[index].id == 0u) continue;
        if (!execbc_vm_resolve_canonical_type(context,
                values[index].typeToken, &values[index].typeToken)) {
            execbc_vm_set_diagnostic(diagnostic,
                    ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                    projection, 0u, values[index].definition, 0u,
                    ZR_CANONICAL_TYPE_PRIMITIVE,
                    projection->slotValues[index].typeToken);
            goto cleanup;
        }
    }
    for (index = 0u; index < projection->instructionCount; ++index) {
        if (instructions[index].matchTypeToken != 0u &&
            !execbc_vm_resolve_canonical_type(context,
                instructions[index].matchTypeToken,
                &instructions[index].matchTypeToken)) {
            execbc_vm_set_diagnostic(diagnostic,
                    ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                    projection, 0u, index + 1u, instructions[index].sourceId,
                    ZR_CANONICAL_TYPE_PRIMITIVE,
                    projection->instructions[index].matchTypeToken);
            goto cleanup;
        }
        if (instructions[index].typeToken != 0u &&
            instructions[index].opcode != ZR_EXEC_IR_OPCODE_COMPARE &&
            !execbc_vm_resolve_canonical_type(context,
                instructions[index].typeToken, &instructions[index].typeToken)) {
            execbc_vm_set_diagnostic(diagnostic,
                    ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                    projection, 0u, index + 1u, instructions[index].sourceId,
                    ZR_CANONICAL_TYPE_PRIMITIVE,
                    projection->instructions[index].typeToken);
            goto cleanup;
        }
    }
    for (index = 0u; index < projection->constantCount; ++index) {
        const SZrCanonicalTypeNode *node = ZrParser_CanonicalType_Find(
                context, (TZrTypeId)constants[index].typeToken);
        if (node != ZR_NULL &&
            node->id == (TZrTypeId)constants[index].typeToken &&
            node->kind == ZR_CANONICAL_TYPE_PRIMITIVE &&
            node->data.primitive.valueType == ZR_VALUE_TYPE_NULL) {
            if (constants[index].flags != 0u || constants[index].bits != 0u) {
                execbc_vm_set_diagnostic(diagnostic,
                        ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                        projection, 0u, 0u, 0u, 0u,
                        constants[index].flags != 0u
                                ? constants[index].flags
                                : (constants[index].bits > UINT32_MAX
                                        ? UINT32_MAX
                                        : (TZrUInt32)constants[index].bits));
                goto cleanup;
            }
            for (TZrUInt32 instructionIndex = 0u;
                 instructionIndex < projection->instructionCount;
                 ++instructionIndex) {
                if (instructions[instructionIndex].opcode ==
                            ZR_EXEC_IR_OPCODE_CONSTANT &&
                    instructions[instructionIndex].layoutId == index) {
                    execbc_vm_set_diagnostic(diagnostic,
                            ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                            projection, 0u, instructionIndex + 1u,
                            instructions[instructionIndex].sourceId,
                            ZR_VALUE_TYPE_INT64, ZR_VALUE_TYPE_NULL);
                    goto cleanup;
                }
            }
            /* Preserve the unused pool descriptor; NULL is not a scalar value. */
            constants[index].typeToken = ZR_VALUE_TYPE_NULL;
            continue;
        }
        if (!execbc_vm_resolve_canonical_type(context,
                constants[index].typeToken, &constants[index].typeToken)) {
            execbc_vm_set_diagnostic(diagnostic,
                    ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                    projection, 0u, 0u, 0u, ZR_CANONICAL_TYPE_PRIMITIVE,
                    projection->constants[index].typeToken);
            goto cleanup;
        }
    }
    succeeded = ZrParser_ExecBcProjection_MaterializeVmFunction(
            state, &resolved, output, diagnostic);

cleanup:
    free(instructions);
    free(values);
    free(constants);
    return succeeded;
}
