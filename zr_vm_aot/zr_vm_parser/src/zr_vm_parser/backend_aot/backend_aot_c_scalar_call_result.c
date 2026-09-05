#include "backend_aot_c_scalar_call_result.h"

static TZrBool backend_aot_c_scalar_call_result_type_is_nonprimitive(
        const SZrFunctionTypedTypeRef *type) {
    if (type->ownershipQualifier != ZR_OWNERSHIP_QUALIFIER_NONE) {
        return ZR_TRUE;
    }

    switch (type->baseType) {
        case ZR_VALUE_TYPE_UNKNOWN:
        case ZR_VALUE_TYPE_BOOL:
        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64:
        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64:
        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            return ZR_FALSE;
        default:
            return ZR_TRUE;
    }
}

TZrBool backend_aot_c_scalar_call_result_has_nonprimitive_type(
        const SZrFunction *function,
        const SZrFunction *calleeFunction,
        TZrUInt32 execInstructionIndex,
        TZrUInt32 destinationSlot) {
    if (calleeFunction != ZR_NULL && calleeFunction->hasCallableReturnType == ZR_TRUE &&
        backend_aot_c_scalar_call_result_type_is_nonprimitive(&calleeFunction->callableReturnType)) {
        return ZR_TRUE;
    }
    if (function == ZR_NULL || function->semIrInstructions == ZR_NULL ||
        function->semIrTypeTable == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < function->semIrInstructionLength; index++) {
        const SZrSemIrInstruction *instruction = &function->semIrInstructions[index];

        if (instruction->execInstructionIndex == execInstructionIndex &&
            instruction->destinationSlot == destinationSlot &&
            instruction->typeTableIndex < function->semIrTypeTableLength &&
            backend_aot_c_scalar_call_result_type_is_nonprimitive(
                    &function->semIrTypeTable[instruction->typeTableIndex])) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}
