#include "backend_aot_c_value_semir_field_scalar_locals.h"

#include "backend_aot_c_scalar_locals.h"
#include "backend_aot_internal.h"

TZrBool backend_aot_try_write_c_value_field_scalar_local_store_exec(
        FILE *file,
        const SZrAotExecIrFunction *functionIr,
        const SZrAotExecIrFrameSlotLayout *destinationLayout,
        const SZrAotExecIrInstruction *instruction,
        const SZrFunctionFrameFieldLayout *fieldLayout,
        const char *fieldTypeName) {
    TZrUInt32 sourceSlot;
    TZrUInt32 instructionIndex;
    const char *scalarPrefix = ZR_NULL;

    if (file == ZR_NULL || functionIr == ZR_NULL || destinationLayout == ZR_NULL ||
        instruction == ZR_NULL || fieldLayout == ZR_NULL || fieldTypeName == ZR_NULL) {
        return ZR_FALSE;
    }

    sourceSlot = instruction->operand0;
    instructionIndex = instruction->execInstructionIndex;
    switch (fieldLayout->valueType) {
        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64:
            if (backend_aot_c_scalar_locals_i64_written_before(
                        functionIr, sourceSlot, instructionIndex)) {
                scalarPrefix = "zr_aot_s";
            } else if (backend_aot_c_scalar_locals_u64_written_before(
                               functionIr, sourceSlot, instructionIndex)) {
                scalarPrefix = "zr_aot_u";
            }
            break;
        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64:
            if (backend_aot_c_scalar_locals_u64_written_before(
                        functionIr, sourceSlot, instructionIndex)) {
                scalarPrefix = "zr_aot_u";
            } else if (backend_aot_c_scalar_locals_i64_written_before(
                               functionIr, sourceSlot, instructionIndex)) {
                scalarPrefix = "zr_aot_s";
            }
            break;
        case ZR_VALUE_TYPE_BOOL:
            if (backend_aot_c_scalar_locals_bool_written_before(
                        functionIr, sourceSlot, instructionIndex)) {
                scalarPrefix = "zr_aot_b";
            }
            break;
        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            if (backend_aot_c_scalar_locals_f64_written_before(
                        functionIr, sourceSlot, instructionIndex)) {
                scalarPrefix = "zr_aot_f";
            }
            break;
        default:
            break;
    }

    if (scalarPrefix == ZR_NULL) {
        return ZR_FALSE;
    }

    fprintf(file,
            "    /* zr_aot_value_exec_field_store_scalar_local dstSlot=%u sourceSlot=%u"
            " fieldOffset=%u fieldSize=%u valueType=%u */\n"
            "    {\n"
            "        TZrByte *zr_aot_field = (TZrByte *)frame.slotBase + %u + %u;\n"
            "        %s zr_aot_stored_value = (%s)%s%u;\n"
            "        memcpy(zr_aot_field, &zr_aot_stored_value, sizeof(zr_aot_stored_value));\n"
            "    }\n",
            (unsigned)instruction->destinationSlot,
            (unsigned)sourceSlot,
            (unsigned)fieldLayout->byteOffset,
            (unsigned)fieldLayout->byteSize,
            (unsigned)fieldLayout->valueType,
            (unsigned)destinationLayout->byteOffset,
            (unsigned)fieldLayout->byteOffset,
            fieldTypeName,
            fieldTypeName,
            scalarPrefix,
            (unsigned)sourceSlot);
    return ZR_TRUE;
}
