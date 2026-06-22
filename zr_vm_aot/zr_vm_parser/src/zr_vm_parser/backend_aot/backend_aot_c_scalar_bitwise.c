#include "backend_aot_c_scalar_bitwise.h"

#include "backend_aot_c_scalar_locals.h"

static const char *backend_aot_c_scalar_bitwise_operator(TZrUInt32 semIrOpcode) {
    switch ((EZrSemIrOpcode)semIrOpcode) {
        case ZR_SEMIR_OPCODE_BIT_AND:
            return "&";
        case ZR_SEMIR_OPCODE_BIT_OR:
            return "|";
        case ZR_SEMIR_OPCODE_BIT_XOR:
            return "^";
        default:
            return ZR_NULL;
    }
}

static TZrBool backend_aot_c_scalar_decode_bit_not_operands(const TZrInstruction *instruction,
                                                            TZrUInt32 *outDestinationSlot,
                                                            TZrUInt32 *outSourceSlot) {
    if (instruction == ZR_NULL || outDestinationSlot == ZR_NULL || outSourceSlot == ZR_NULL) {
        return ZR_FALSE;
    }

    if ((EZrInstructionCode)instruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(BITWISE_NOT)) {
        return ZR_FALSE;
    }

    *outDestinationSlot = instruction->instruction.operandExtra;
    *outSourceSlot = instruction->instruction.operand.operand1[0];
    return ZR_TRUE;
}

static TZrBool backend_aot_c_scalar_decode_bitwise_binary_operands(const TZrInstruction *instruction,
                                                                   TZrUInt32 *outDestinationSlot,
                                                                   TZrUInt32 *outLeftSlot,
                                                                   TZrUInt32 *outRightSlot) {
    EZrInstructionCode opcode;

    if (instruction == ZR_NULL ||
        outDestinationSlot == ZR_NULL ||
        outLeftSlot == ZR_NULL ||
        outRightSlot == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(BITWISE_AND):
        case ZR_INSTRUCTION_ENUM(BITWISE_OR):
        case ZR_INSTRUCTION_ENUM(BITWISE_XOR):
            *outDestinationSlot = instruction->instruction.operandExtra;
            *outLeftSlot = instruction->instruction.operand.operand1[0];
            *outRightSlot = instruction->instruction.operand.operand1[1];
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_c_scalar_decode_shift_operands(const TZrInstruction *instruction,
                                                          TZrUInt32 *outDestinationSlot,
                                                          TZrUInt32 *outLeftSlot,
                                                          TZrUInt32 *outRightSlot) {
    EZrInstructionCode opcode;

    if (instruction == ZR_NULL ||
        outDestinationSlot == ZR_NULL ||
        outLeftSlot == ZR_NULL ||
        outRightSlot == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(SHIFT_LEFT):
        case ZR_INSTRUCTION_ENUM(SHIFT_LEFT_INT):
        case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT):
        case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT_INT):
        case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_LEFT):
        case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_RIGHT):
            *outDestinationSlot = instruction->instruction.operandExtra;
            *outLeftSlot = instruction->instruction.operand.operand1[0];
            *outRightSlot = instruction->instruction.operand.operand1[1];
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static void backend_aot_write_c_scalar_plain_i64_result(FILE *file, const char *resultExpression) {
    if (file == ZR_NULL || resultExpression == ZR_NULL) {
        return;
    }

    fprintf(file,
            "        if (zr_aot_destination->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE ||\n"
            "            zr_aot_destination->isGarbageCollectable) {\n"
            "            ZrCore_Ownership_ReleaseValue(state, zr_aot_destination);\n"
            "        }\n"
            "        zr_aot_destination->type = ZR_VALUE_TYPE_INT64;\n"
            "        zr_aot_destination->value.nativeObject.nativeInt64 = %s;\n"
            "        zr_aot_destination->isGarbageCollectable = ZR_FALSE;\n"
            "        zr_aot_destination->isNative = ZR_TRUE;\n"
            "        zr_aot_destination->ownershipKind = ZR_OWNERSHIP_VALUE_KIND_NONE;\n"
            "        zr_aot_destination->ownershipControl = ZR_NULL;\n"
            "        zr_aot_destination->ownershipWeakRef = ZR_NULL;\n",
            resultExpression);
}

static void backend_aot_write_c_scalar_plain_u64_result(FILE *file, const char *resultExpression) {
    if (file == ZR_NULL || resultExpression == ZR_NULL) {
        return;
    }

    fprintf(file,
            "        if (zr_aot_destination->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE ||\n"
            "            zr_aot_destination->isGarbageCollectable) {\n"
            "            ZrCore_Ownership_ReleaseValue(state, zr_aot_destination);\n"
            "        }\n"
            "        zr_aot_destination->type = ZR_VALUE_TYPE_UINT64;\n"
            "        zr_aot_destination->value.nativeObject.nativeUInt64 = %s;\n"
            "        zr_aot_destination->isGarbageCollectable = ZR_FALSE;\n"
            "        zr_aot_destination->isNative = ZR_TRUE;\n"
            "        zr_aot_destination->ownershipKind = ZR_OWNERSHIP_VALUE_KIND_NONE;\n"
            "        zr_aot_destination->ownershipControl = ZR_NULL;\n"
            "        zr_aot_destination->ownershipWeakRef = ZR_NULL;\n",
            resultExpression);
}

static void backend_aot_write_c_scalar_i64_bit_not(FILE *file,
                                                   const SZrAotExecIrFunction *functionIr,
                                                   const SZrAotExecIrInstruction *semIrInstruction,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 sourceSlot) {
    TZrBool useScalarSource = backend_aot_c_scalar_locals_has_i64_slot(functionIr, sourceSlot);
    TZrBool useScalarDestination = backend_aot_c_scalar_locals_has_i64_slot(functionIr, destinationSlot);
    TZrBool sourceLocalWrittenBefore =
            useScalarSource &&
            backend_aot_c_scalar_locals_i64_written_before(
                    functionIr, sourceSlot, semIrInstruction->execInstructionIndex);
    TZrBool canSkipValueSlot =
            (TZrBool)(useScalarDestination &&
                      sourceLocalWrittenBefore &&
                      backend_aot_c_scalar_locals_i64_result_can_skip_value_slot(
                              functionIr, destinationSlot, semIrInstruction->execInstructionIndex));

    if (canSkipValueSlot) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_scalar_exec_i64_bit_not semirOpcode=%u dstSlot=%u sourceSlot=%u */\n"
                "        zr_aot_s%u = ~zr_aot_s%u;\n"
                "    }\n",
                (unsigned)semIrInstruction->semIrOpcode,
                (unsigned)destinationSlot,
                (unsigned)sourceSlot,
                (unsigned)destinationSlot,
                (unsigned)sourceSlot);
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_scalar_exec_i64_bit_not semirOpcode=%u dstSlot=%u sourceSlot=%u */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;\n"
            "        TZrInt64 zr_aot_s_result;\n"
            "        if (frame.slotBase == ZR_NULL || %u >= frame.generatedFrameSlotCount ||\n"
            "            %u >= frame.generatedFrameSlotCount) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_destination = &frame.slotBase[%u].value;\n",
            (unsigned)semIrInstruction->semIrOpcode,
            (unsigned)destinationSlot,
            (unsigned)sourceSlot,
            (unsigned)destinationSlot,
            (unsigned)sourceSlot,
            (unsigned)destinationSlot);
    if (!sourceLocalWrittenBefore) {
        fprintf(file,
                "        if (!ZR_VALUE_IS_TYPE_SIGNED_INT(frame.slotBase[%u].value.type)) {\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n",
                (unsigned)sourceSlot);
    }
    if (useScalarSource) {
        if (!sourceLocalWrittenBefore) {
            fprintf(file,
                    "        zr_aot_s%u = frame.slotBase[%u].value.value.nativeObject.nativeInt64;\n",
                    (unsigned)sourceSlot,
                    (unsigned)sourceSlot);
        }
        if (useScalarDestination) {
            fprintf(file,
                    "        zr_aot_s%u = ~zr_aot_s%u;\n"
                    "        zr_aot_s_result = zr_aot_s%u;\n",
                    (unsigned)destinationSlot,
                    (unsigned)sourceSlot,
                    (unsigned)destinationSlot);
        } else {
            fprintf(file,
                    "        zr_aot_s_result = ~zr_aot_s%u;\n",
                    (unsigned)sourceSlot);
        }
    } else {
        fprintf(file,
                "        TZrInt64 zr_aot_s_source;\n"
                "        zr_aot_s_source = frame.slotBase[%u].value.value.nativeObject.nativeInt64;\n"
                "        zr_aot_s_result = ~zr_aot_s_source;\n",
                (unsigned)sourceSlot);
    }
    if (!useScalarSource && useScalarDestination) {
        fprintf(file, "        zr_aot_s%u = zr_aot_s_result;\n", (unsigned)destinationSlot);
    }
    backend_aot_write_c_scalar_plain_i64_result(file, "zr_aot_s_result");
    fprintf(file, "    }\n");
}

static void backend_aot_write_c_scalar_u64_bit_not(FILE *file,
                                                   const SZrAotExecIrFunction *functionIr,
                                                   const SZrAotExecIrInstruction *semIrInstruction,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 sourceSlot) {
    TZrBool useScalarSource = backend_aot_c_scalar_locals_has_u64_slot(functionIr, sourceSlot);
    TZrBool useScalarDestination = backend_aot_c_scalar_locals_has_u64_slot(functionIr, destinationSlot);
    TZrBool sourceLocalWrittenBefore =
            useScalarSource &&
            backend_aot_c_scalar_locals_u64_written_before(
                    functionIr, sourceSlot, semIrInstruction->execInstructionIndex);
    TZrBool canSkipValueSlot =
            (TZrBool)(useScalarDestination &&
                      sourceLocalWrittenBefore &&
                      backend_aot_c_scalar_locals_u64_result_can_skip_value_slot(
                              functionIr, destinationSlot, semIrInstruction->execInstructionIndex));

    if (canSkipValueSlot) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_scalar_exec_u64_bit_not semirOpcode=%u dstSlot=%u sourceSlot=%u */\n"
                "        zr_aot_u%u = ~zr_aot_u%u;\n"
                "    }\n",
                (unsigned)semIrInstruction->semIrOpcode,
                (unsigned)destinationSlot,
                (unsigned)sourceSlot,
                (unsigned)destinationSlot,
                (unsigned)sourceSlot);
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_scalar_exec_u64_bit_not semirOpcode=%u dstSlot=%u sourceSlot=%u */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;\n"
            "        TZrUInt64 zr_aot_u_result;\n"
            "        if (frame.slotBase == ZR_NULL || %u >= frame.generatedFrameSlotCount ||\n"
            "            %u >= frame.generatedFrameSlotCount) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_destination = &frame.slotBase[%u].value;\n",
            (unsigned)semIrInstruction->semIrOpcode,
            (unsigned)destinationSlot,
            (unsigned)sourceSlot,
            (unsigned)destinationSlot,
            (unsigned)sourceSlot,
            (unsigned)destinationSlot);
    if (!sourceLocalWrittenBefore) {
        fprintf(file,
                "        if (!ZR_VALUE_IS_TYPE_UNSIGNED_INT(frame.slotBase[%u].value.type)) {\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n",
                (unsigned)sourceSlot);
    }
    if (useScalarSource) {
        if (!sourceLocalWrittenBefore) {
            fprintf(file,
                    "        zr_aot_u%u = frame.slotBase[%u].value.value.nativeObject.nativeUInt64;\n",
                    (unsigned)sourceSlot,
                    (unsigned)sourceSlot);
        }
        if (useScalarDestination) {
            fprintf(file,
                    "        zr_aot_u%u = ~zr_aot_u%u;\n"
                    "        zr_aot_u_result = zr_aot_u%u;\n",
                    (unsigned)destinationSlot,
                    (unsigned)sourceSlot,
                    (unsigned)destinationSlot);
        } else {
            fprintf(file,
                    "        zr_aot_u_result = ~zr_aot_u%u;\n",
                    (unsigned)sourceSlot);
        }
    } else {
        fprintf(file,
                "        TZrUInt64 zr_aot_u_source;\n"
                "        zr_aot_u_source = frame.slotBase[%u].value.value.nativeObject.nativeUInt64;\n"
                "        zr_aot_u_result = ~zr_aot_u_source;\n",
                (unsigned)sourceSlot);
    }
    if (!useScalarSource && useScalarDestination) {
        fprintf(file, "        zr_aot_u%u = zr_aot_u_result;\n", (unsigned)destinationSlot);
    }
    backend_aot_write_c_scalar_plain_u64_result(file, "zr_aot_u_result");
    fprintf(file, "    }\n");
}

static void backend_aot_write_c_scalar_i64_bitwise(FILE *file,
                                                   const SZrAotExecIrFunction *functionIr,
                                                   const SZrAotExecIrInstruction *semIrInstruction,
                                                   const char *operatorText,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 leftSlot,
                                                   TZrUInt32 rightSlot) {
    TZrBool useScalarDestination = backend_aot_c_scalar_locals_has_i64_slot(functionIr, destinationSlot);
    TZrBool useScalarLocals =
            useScalarDestination &&
            backend_aot_c_scalar_locals_has_i64_slot(functionIr, leftSlot) &&
            backend_aot_c_scalar_locals_has_i64_slot(functionIr, rightSlot);
    TZrBool leftLocalWrittenBefore = ZR_FALSE;
    TZrBool rightLocalWrittenBefore = ZR_FALSE;
    TZrBool useWrittenScalarSources = ZR_FALSE;
    TZrBool canSkipValueSlot = ZR_FALSE;

    if (useScalarLocals) {
        leftLocalWrittenBefore =
                backend_aot_c_scalar_locals_i64_written_before(functionIr, leftSlot, semIrInstruction->execInstructionIndex);
        rightLocalWrittenBefore =
                backend_aot_c_scalar_locals_i64_written_before(functionIr, rightSlot, semIrInstruction->execInstructionIndex);
        useWrittenScalarSources = (TZrBool)(leftLocalWrittenBefore && rightLocalWrittenBefore);
        canSkipValueSlot =
                (TZrBool)(useWrittenScalarSources &&
                          backend_aot_c_scalar_locals_i64_result_can_skip_value_slot(
                                  functionIr, destinationSlot, semIrInstruction->execInstructionIndex));
    }

    if (canSkipValueSlot) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_scalar_exec_i64_bitwise semirOpcode=%u dstSlot=%u leftSlot=%u rightSlot=%u */\n"
                "        zr_aot_s%u = zr_aot_s%u %s zr_aot_s%u;\n"
                "    }\n",
                (unsigned)semIrInstruction->semIrOpcode,
                (unsigned)destinationSlot,
                (unsigned)leftSlot,
                (unsigned)rightSlot,
                (unsigned)destinationSlot,
                (unsigned)leftSlot,
                operatorText,
                (unsigned)rightSlot);
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_scalar_exec_i64_bitwise semirOpcode=%u dstSlot=%u leftSlot=%u rightSlot=%u */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;\n"
            "        TZrInt64 zr_aot_s_result;\n"
            "        if (frame.slotBase == ZR_NULL || %u >= frame.generatedFrameSlotCount ||\n"
            "            %u >= frame.generatedFrameSlotCount ||\n"
            "            %u >= frame.generatedFrameSlotCount) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_destination = &frame.slotBase[%u].value;\n",
            (unsigned)semIrInstruction->semIrOpcode,
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot,
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot,
            (unsigned)destinationSlot);
    if (!useWrittenScalarSources) {
        fprintf(file, "        if (");
        if (!leftLocalWrittenBefore) {
            fprintf(file,
                    "!ZR_VALUE_IS_TYPE_SIGNED_INT(frame.slotBase[%u].value.type)",
                    (unsigned)leftSlot);
        }
        if (!rightLocalWrittenBefore) {
            if (!leftLocalWrittenBefore) {
                fprintf(file, " ||\n            ");
            }
            fprintf(file,
                    "!ZR_VALUE_IS_TYPE_SIGNED_INT(frame.slotBase[%u].value.type)",
                    (unsigned)rightSlot);
        }
        fprintf(file,
                ") {\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n");
    }
    if (useScalarLocals) {
        if (!leftLocalWrittenBefore) {
            fprintf(file,
                    "        zr_aot_s%u = frame.slotBase[%u].value.value.nativeObject.nativeInt64;\n",
                    (unsigned)leftSlot,
                    (unsigned)leftSlot);
        }
        if (!rightLocalWrittenBefore) {
            fprintf(file,
                    "        zr_aot_s%u = frame.slotBase[%u].value.value.nativeObject.nativeInt64;\n",
                    (unsigned)rightSlot,
                    (unsigned)rightSlot);
        }
        fprintf(file,
                "        zr_aot_s%u = zr_aot_s%u %s zr_aot_s%u;\n"
                "        zr_aot_s_result = zr_aot_s%u;\n",
                (unsigned)destinationSlot,
                (unsigned)leftSlot,
                operatorText,
                (unsigned)rightSlot,
                (unsigned)destinationSlot);
    } else {
        fprintf(file,
                "        TZrInt64 zr_aot_s_left;\n"
                "        TZrInt64 zr_aot_s_right;\n"
                "        zr_aot_s_left = frame.slotBase[%u].value.value.nativeObject.nativeInt64;\n"
                "        zr_aot_s_right = frame.slotBase[%u].value.value.nativeObject.nativeInt64;\n"
                "        zr_aot_s_result = zr_aot_s_left %s zr_aot_s_right;\n",
                (unsigned)leftSlot,
                (unsigned)rightSlot,
                operatorText);
    }
    if (!useScalarLocals && useScalarDestination) {
        fprintf(file, "        zr_aot_s%u = zr_aot_s_result;\n", (unsigned)destinationSlot);
    }
    backend_aot_write_c_scalar_plain_i64_result(file, "zr_aot_s_result");
    fprintf(file, "    }\n");
}

static void backend_aot_write_c_scalar_u64_bitwise(FILE *file,
                                                   const SZrAotExecIrFunction *functionIr,
                                                   const SZrAotExecIrInstruction *semIrInstruction,
                                                   const char *operatorText,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 leftSlot,
                                                   TZrUInt32 rightSlot) {
    TZrBool useScalarDestination = backend_aot_c_scalar_locals_has_u64_slot(functionIr, destinationSlot);
    TZrBool useScalarLocals =
            useScalarDestination &&
            backend_aot_c_scalar_locals_has_u64_slot(functionIr, leftSlot) &&
            backend_aot_c_scalar_locals_has_u64_slot(functionIr, rightSlot);
    TZrBool leftLocalWrittenBefore = ZR_FALSE;
    TZrBool rightLocalWrittenBefore = ZR_FALSE;
    TZrBool useWrittenScalarSources = ZR_FALSE;
    TZrBool canSkipValueSlot = ZR_FALSE;

    if (useScalarLocals) {
        leftLocalWrittenBefore =
                backend_aot_c_scalar_locals_u64_written_before(functionIr, leftSlot, semIrInstruction->execInstructionIndex);
        rightLocalWrittenBefore =
                backend_aot_c_scalar_locals_u64_written_before(functionIr, rightSlot, semIrInstruction->execInstructionIndex);
        useWrittenScalarSources = (TZrBool)(leftLocalWrittenBefore && rightLocalWrittenBefore);
        canSkipValueSlot =
                (TZrBool)(useWrittenScalarSources &&
                          backend_aot_c_scalar_locals_u64_result_can_skip_value_slot(
                                  functionIr, destinationSlot, semIrInstruction->execInstructionIndex));
    }

    if (canSkipValueSlot) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_scalar_exec_u64_bitwise semirOpcode=%u dstSlot=%u leftSlot=%u rightSlot=%u */\n"
                "        zr_aot_u%u = zr_aot_u%u %s zr_aot_u%u;\n"
                "    }\n",
                (unsigned)semIrInstruction->semIrOpcode,
                (unsigned)destinationSlot,
                (unsigned)leftSlot,
                (unsigned)rightSlot,
                (unsigned)destinationSlot,
                (unsigned)leftSlot,
                operatorText,
                (unsigned)rightSlot);
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_scalar_exec_u64_bitwise semirOpcode=%u dstSlot=%u leftSlot=%u rightSlot=%u */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;\n"
            "        TZrUInt64 zr_aot_u_result;\n"
            "        if (frame.slotBase == ZR_NULL || %u >= frame.generatedFrameSlotCount ||\n"
            "            %u >= frame.generatedFrameSlotCount ||\n"
            "            %u >= frame.generatedFrameSlotCount) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_destination = &frame.slotBase[%u].value;\n",
            (unsigned)semIrInstruction->semIrOpcode,
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot,
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot,
            (unsigned)destinationSlot);
    if (!useWrittenScalarSources) {
        fprintf(file, "        if (");
        if (!leftLocalWrittenBefore) {
            fprintf(file,
                    "!ZR_VALUE_IS_TYPE_UNSIGNED_INT(frame.slotBase[%u].value.type)",
                    (unsigned)leftSlot);
        }
        if (!rightLocalWrittenBefore) {
            if (!leftLocalWrittenBefore) {
                fprintf(file, " ||\n            ");
            }
            fprintf(file,
                    "!ZR_VALUE_IS_TYPE_UNSIGNED_INT(frame.slotBase[%u].value.type)",
                    (unsigned)rightSlot);
        }
        fprintf(file,
                ") {\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n");
    }
    if (useScalarLocals) {
        if (!leftLocalWrittenBefore) {
            fprintf(file,
                    "        zr_aot_u%u = frame.slotBase[%u].value.value.nativeObject.nativeUInt64;\n",
                    (unsigned)leftSlot,
                    (unsigned)leftSlot);
        }
        if (!rightLocalWrittenBefore) {
            fprintf(file,
                    "        zr_aot_u%u = frame.slotBase[%u].value.value.nativeObject.nativeUInt64;\n",
                    (unsigned)rightSlot,
                    (unsigned)rightSlot);
        }
        fprintf(file,
                "        zr_aot_u%u = zr_aot_u%u %s zr_aot_u%u;\n"
                "        zr_aot_u_result = zr_aot_u%u;\n",
                (unsigned)destinationSlot,
                (unsigned)leftSlot,
                operatorText,
                (unsigned)rightSlot,
                (unsigned)destinationSlot);
    } else {
        fprintf(file,
                "        TZrUInt64 zr_aot_u_left;\n"
                "        TZrUInt64 zr_aot_u_right;\n"
                "        zr_aot_u_left = frame.slotBase[%u].value.value.nativeObject.nativeUInt64;\n"
                "        zr_aot_u_right = frame.slotBase[%u].value.value.nativeObject.nativeUInt64;\n"
                "        zr_aot_u_result = zr_aot_u_left %s zr_aot_u_right;\n",
                (unsigned)leftSlot,
                (unsigned)rightSlot,
                operatorText);
    }
    if (!useScalarLocals && useScalarDestination) {
        fprintf(file, "        zr_aot_u%u = zr_aot_u_result;\n", (unsigned)destinationSlot);
    }
    backend_aot_write_c_scalar_plain_u64_result(file, "zr_aot_u_result");
    fprintf(file, "    }\n");
}

static void backend_aot_write_c_scalar_i64_shift(FILE *file,
                                                 const SZrAotExecIrFunction *functionIr,
                                                 const SZrAotExecIrInstruction *semIrInstruction,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 leftSlot,
                                                 TZrUInt32 rightSlot) {
    TZrBool useScalarOperands =
            backend_aot_c_scalar_locals_has_i64_slot(functionIr, leftSlot) &&
            backend_aot_c_scalar_locals_has_i64_slot(functionIr, rightSlot);
    TZrBool useScalarDestination =
            backend_aot_c_scalar_locals_has_i64_slot(functionIr, destinationSlot);
    TZrBool leftLocalWrittenBefore = ZR_FALSE;
    TZrBool rightLocalWrittenBefore = ZR_FALSE;
    TZrBool useWrittenScalarSources = ZR_FALSE;
    TZrBool canSkipValueSlot = ZR_FALSE;

    if (useScalarOperands) {
        leftLocalWrittenBefore =
                backend_aot_c_scalar_locals_i64_written_before(functionIr, leftSlot, semIrInstruction->execInstructionIndex);
        rightLocalWrittenBefore =
                backend_aot_c_scalar_locals_i64_written_before(functionIr, rightSlot, semIrInstruction->execInstructionIndex);
        useWrittenScalarSources = (TZrBool)(leftLocalWrittenBefore && rightLocalWrittenBefore);
        canSkipValueSlot =
                (TZrBool)(useScalarDestination &&
                          useWrittenScalarSources &&
                          backend_aot_c_scalar_locals_i64_result_can_skip_value_slot(
                                  functionIr, destinationSlot, semIrInstruction->execInstructionIndex));
    }

    if (canSkipValueSlot) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_scalar_exec_i64_shift semirOpcode=%u dstSlot=%u leftSlot=%u rightSlot=%u */\n"
                "        if (ZR_UNLIKELY(zr_aot_s%u < 0 || zr_aot_s%u >= 64)) {\n"
                "            ZrCore_Debug_RunError(state, \"generated AOT scalar shift count out of range\");\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n",
                (unsigned)semIrInstruction->semIrOpcode,
                (unsigned)destinationSlot,
                (unsigned)leftSlot,
                (unsigned)rightSlot,
                (unsigned)rightSlot,
                (unsigned)rightSlot);
        if (semIrInstruction->semIrOpcode == ZR_SEMIR_OPCODE_SHL) {
            fprintf(file,
                    "        zr_aot_s%u = (TZrInt64)((TZrUInt64)zr_aot_s%u << zr_aot_s%u);\n",
                    (unsigned)destinationSlot,
                    (unsigned)leftSlot,
                    (unsigned)rightSlot);
        } else {
            fprintf(file,
                    "        zr_aot_s%u = zr_aot_s%u >> zr_aot_s%u;\n",
                    (unsigned)destinationSlot,
                    (unsigned)leftSlot,
                    (unsigned)rightSlot);
        }
        fprintf(file, "    }\n");
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_scalar_exec_i64_shift semirOpcode=%u dstSlot=%u leftSlot=%u rightSlot=%u */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;\n"
            "        TZrInt64 zr_aot_s_result;\n"
            "        if (frame.slotBase == ZR_NULL || %u >= frame.generatedFrameSlotCount ||\n"
            "            %u >= frame.generatedFrameSlotCount ||\n"
            "            %u >= frame.generatedFrameSlotCount) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_destination = &frame.slotBase[%u].value;\n",
            (unsigned)semIrInstruction->semIrOpcode,
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot,
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot,
            (unsigned)destinationSlot);
    if (!useWrittenScalarSources) {
        fprintf(file, "        if (");
        if (!leftLocalWrittenBefore) {
            fprintf(file,
                    "!ZR_VALUE_IS_TYPE_SIGNED_INT(frame.slotBase[%u].value.type)",
                    (unsigned)leftSlot);
        }
        if (!rightLocalWrittenBefore) {
            if (!leftLocalWrittenBefore) {
                fprintf(file, " ||\n            ");
            }
            fprintf(file,
                    "!ZR_VALUE_IS_TYPE_SIGNED_INT(frame.slotBase[%u].value.type)",
                    (unsigned)rightSlot);
        }
        fprintf(file,
                ") {\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n");
    }
    if (useScalarOperands) {
        if (!leftLocalWrittenBefore) {
            fprintf(file,
                    "        zr_aot_s%u = frame.slotBase[%u].value.value.nativeObject.nativeInt64;\n",
                    (unsigned)leftSlot,
                    (unsigned)leftSlot);
        }
        if (!rightLocalWrittenBefore) {
            fprintf(file,
                    "        zr_aot_s%u = frame.slotBase[%u].value.value.nativeObject.nativeInt64;\n",
                    (unsigned)rightSlot,
                    (unsigned)rightSlot);
        }
        fprintf(file,
                "        if (ZR_UNLIKELY(zr_aot_s%u < 0 || zr_aot_s%u >= 64)) {\n"
                "            ZrCore_Debug_RunError(state, \"generated AOT scalar shift count out of range\");\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n",
                (unsigned)rightSlot,
                (unsigned)rightSlot);
        if (useScalarDestination) {
            if (semIrInstruction->semIrOpcode == ZR_SEMIR_OPCODE_SHL) {
                fprintf(file,
                        "        zr_aot_s%u = (TZrInt64)((TZrUInt64)zr_aot_s%u << zr_aot_s%u);\n",
                        (unsigned)destinationSlot,
                        (unsigned)leftSlot,
                        (unsigned)rightSlot);
            } else {
                fprintf(file,
                        "        zr_aot_s%u = zr_aot_s%u >> zr_aot_s%u;\n",
                        (unsigned)destinationSlot,
                        (unsigned)leftSlot,
                        (unsigned)rightSlot);
            }
            fprintf(file,
                    "        zr_aot_s_result = zr_aot_s%u;\n",
                    (unsigned)destinationSlot);
        } else if (semIrInstruction->semIrOpcode == ZR_SEMIR_OPCODE_SHL) {
            fprintf(file,
                    "        zr_aot_s_result = (TZrInt64)((TZrUInt64)zr_aot_s%u << zr_aot_s%u);\n",
                    (unsigned)leftSlot,
                    (unsigned)rightSlot);
        } else {
            fprintf(file,
                    "        zr_aot_s_result = zr_aot_s%u >> zr_aot_s%u;\n",
                    (unsigned)leftSlot,
                    (unsigned)rightSlot);
        }
    } else {
        fprintf(file,
                "        TZrInt64 zr_aot_s_left;\n"
                "        TZrInt64 zr_aot_s_shift;\n"
                "        zr_aot_s_left = frame.slotBase[%u].value.value.nativeObject.nativeInt64;\n"
                "        zr_aot_s_shift = frame.slotBase[%u].value.value.nativeObject.nativeInt64;\n"
                "        if (ZR_UNLIKELY(zr_aot_s_shift < 0 || zr_aot_s_shift >= 64)) {\n"
                "            ZrCore_Debug_RunError(state, \"generated AOT scalar shift count out of range\");\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n",
                (unsigned)leftSlot,
                (unsigned)rightSlot);
        if (semIrInstruction->semIrOpcode == ZR_SEMIR_OPCODE_SHL) {
            fprintf(file, "        zr_aot_s_result = (TZrInt64)((TZrUInt64)zr_aot_s_left << zr_aot_s_shift);\n");
        } else {
            fprintf(file, "        zr_aot_s_result = zr_aot_s_left >> zr_aot_s_shift;\n");
        }
    }
    if (!useScalarOperands && useScalarDestination) {
        fprintf(file, "        zr_aot_s%u = zr_aot_s_result;\n", (unsigned)destinationSlot);
    }
    backend_aot_write_c_scalar_plain_i64_result(file, "zr_aot_s_result");
    fprintf(file, "    }\n");
}

static void backend_aot_write_c_scalar_u64_shift(FILE *file,
                                                 const SZrAotExecIrFunction *functionIr,
                                                 const SZrAotExecIrInstruction *semIrInstruction,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 leftSlot,
                                                 TZrUInt32 rightSlot) {
    TZrBool useScalarOperands =
            backend_aot_c_scalar_locals_has_u64_slot(functionIr, leftSlot) &&
            backend_aot_c_scalar_locals_has_i64_slot(functionIr, rightSlot);
    TZrBool useScalarDestination =
            backend_aot_c_scalar_locals_has_u64_slot(functionIr, destinationSlot);
    TZrBool leftLocalWrittenBefore = ZR_FALSE;
    TZrBool rightLocalWrittenBefore = ZR_FALSE;
    TZrBool useWrittenScalarSources = ZR_FALSE;
    TZrBool canSkipValueSlot = ZR_FALSE;

    if (useScalarOperands) {
        leftLocalWrittenBefore =
                backend_aot_c_scalar_locals_u64_written_before(functionIr, leftSlot, semIrInstruction->execInstructionIndex);
        rightLocalWrittenBefore =
                backend_aot_c_scalar_locals_i64_written_before(functionIr, rightSlot, semIrInstruction->execInstructionIndex);
        useWrittenScalarSources = (TZrBool)(leftLocalWrittenBefore && rightLocalWrittenBefore);
        canSkipValueSlot =
                (TZrBool)(useScalarDestination &&
                          useWrittenScalarSources &&
                          backend_aot_c_scalar_locals_u64_result_can_skip_value_slot(
                                  functionIr, destinationSlot, semIrInstruction->execInstructionIndex));
    }

    if (canSkipValueSlot) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_scalar_exec_u64_shift semirOpcode=%u dstSlot=%u leftSlot=%u rightSlot=%u */\n"
                "        if (ZR_UNLIKELY(zr_aot_s%u < 0 || zr_aot_s%u >= 64)) {\n"
                "            ZrCore_Debug_RunError(state, \"generated AOT scalar shift count out of range\");\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n",
                (unsigned)semIrInstruction->semIrOpcode,
                (unsigned)destinationSlot,
                (unsigned)leftSlot,
                (unsigned)rightSlot,
                (unsigned)rightSlot,
                (unsigned)rightSlot);
        if (semIrInstruction->semIrOpcode == ZR_SEMIR_OPCODE_SHL) {
            fprintf(file,
                    "        zr_aot_u%u = zr_aot_u%u << zr_aot_s%u;\n",
                    (unsigned)destinationSlot,
                    (unsigned)leftSlot,
                    (unsigned)rightSlot);
        } else {
            fprintf(file,
                    "        zr_aot_u%u = zr_aot_u%u >> zr_aot_s%u;\n",
                    (unsigned)destinationSlot,
                    (unsigned)leftSlot,
                    (unsigned)rightSlot);
        }
        fprintf(file, "    }\n");
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_scalar_exec_u64_shift semirOpcode=%u dstSlot=%u leftSlot=%u rightSlot=%u */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;\n"
            "        TZrUInt64 zr_aot_u_result;\n"
            "        if (frame.slotBase == ZR_NULL || %u >= frame.generatedFrameSlotCount ||\n"
            "            %u >= frame.generatedFrameSlotCount ||\n"
            "            %u >= frame.generatedFrameSlotCount) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_destination = &frame.slotBase[%u].value;\n",
            (unsigned)semIrInstruction->semIrOpcode,
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot,
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot,
            (unsigned)destinationSlot);
    if (!useWrittenScalarSources) {
        fprintf(file, "        if (");
        if (!leftLocalWrittenBefore) {
            fprintf(file,
                    "!ZR_VALUE_IS_TYPE_UNSIGNED_INT(frame.slotBase[%u].value.type)",
                    (unsigned)leftSlot);
        }
        if (!rightLocalWrittenBefore) {
            if (!leftLocalWrittenBefore) {
                fprintf(file, " ||\n            ");
            }
            fprintf(file,
                    "!ZR_VALUE_IS_TYPE_SIGNED_INT(frame.slotBase[%u].value.type)",
                    (unsigned)rightSlot);
        }
        fprintf(file,
                ") {\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n");
    }
    if (useScalarOperands) {
        if (!leftLocalWrittenBefore) {
            fprintf(file,
                    "        zr_aot_u%u = frame.slotBase[%u].value.value.nativeObject.nativeUInt64;\n",
                    (unsigned)leftSlot,
                    (unsigned)leftSlot);
        }
        if (!rightLocalWrittenBefore) {
            fprintf(file,
                    "        zr_aot_s%u = frame.slotBase[%u].value.value.nativeObject.nativeInt64;\n",
                    (unsigned)rightSlot,
                    (unsigned)rightSlot);
        }
        fprintf(file,
                "        if (ZR_UNLIKELY(zr_aot_s%u < 0 || zr_aot_s%u >= 64)) {\n"
                "            ZrCore_Debug_RunError(state, \"generated AOT scalar shift count out of range\");\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n",
                (unsigned)rightSlot,
                (unsigned)rightSlot);
        if (useScalarDestination) {
            if (semIrInstruction->semIrOpcode == ZR_SEMIR_OPCODE_SHL) {
                fprintf(file,
                        "        zr_aot_u%u = zr_aot_u%u << zr_aot_s%u;\n",
                        (unsigned)destinationSlot,
                        (unsigned)leftSlot,
                        (unsigned)rightSlot);
            } else {
                fprintf(file,
                        "        zr_aot_u%u = zr_aot_u%u >> zr_aot_s%u;\n",
                        (unsigned)destinationSlot,
                        (unsigned)leftSlot,
                        (unsigned)rightSlot);
            }
            fprintf(file,
                    "        zr_aot_u_result = zr_aot_u%u;\n",
                    (unsigned)destinationSlot);
        } else if (semIrInstruction->semIrOpcode == ZR_SEMIR_OPCODE_SHL) {
            fprintf(file,
                    "        zr_aot_u_result = zr_aot_u%u << zr_aot_s%u;\n",
                    (unsigned)leftSlot,
                    (unsigned)rightSlot);
        } else {
            fprintf(file,
                    "        zr_aot_u_result = zr_aot_u%u >> zr_aot_s%u;\n",
                    (unsigned)leftSlot,
                    (unsigned)rightSlot);
        }
    } else {
        fprintf(file,
                "        TZrUInt64 zr_aot_u_left;\n"
                "        TZrInt64 zr_aot_s_shift;\n"
                "        zr_aot_u_left = frame.slotBase[%u].value.value.nativeObject.nativeUInt64;\n"
                "        zr_aot_s_shift = frame.slotBase[%u].value.value.nativeObject.nativeInt64;\n"
                "        if (ZR_UNLIKELY(zr_aot_s_shift < 0 || zr_aot_s_shift >= 64)) {\n"
                "            ZrCore_Debug_RunError(state, \"generated AOT scalar shift count out of range\");\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n",
                (unsigned)leftSlot,
                (unsigned)rightSlot);
        if (semIrInstruction->semIrOpcode == ZR_SEMIR_OPCODE_SHL) {
            fprintf(file, "        zr_aot_u_result = zr_aot_u_left << zr_aot_s_shift;\n");
        } else {
            fprintf(file, "        zr_aot_u_result = zr_aot_u_left >> zr_aot_s_shift;\n");
        }
    }
    if (!useScalarOperands && useScalarDestination) {
        fprintf(file, "        zr_aot_u%u = zr_aot_u_result;\n", (unsigned)destinationSlot);
    }
    backend_aot_write_c_scalar_plain_u64_result(file, "zr_aot_u_result");
    fprintf(file, "    }\n");
}

TZrBool backend_aot_try_write_c_scalar_bitwise(FILE *file,
                                               const SZrAotExecIrFunction *functionIr,
                                               const SZrAotExecIrInstruction *semIrInstruction,
                                               const TZrInstruction *execInstruction,
                                               EZrStaticCType staticCType) {
    const char *operatorText;
    TZrUInt32 destinationSlot;
    TZrUInt32 leftSlot;
    TZrUInt32 rightSlot;

    if (file == ZR_NULL || semIrInstruction == ZR_NULL || execInstruction == ZR_NULL) {
        return ZR_FALSE;
    }

    if (semIrInstruction->semIrOpcode == ZR_SEMIR_OPCODE_BIT_NOT &&
        backend_aot_c_scalar_decode_bit_not_operands(execInstruction, &destinationSlot, &leftSlot)) {
        if (staticCType == ZR_STATIC_C_TYPE_I64) {
            backend_aot_write_c_scalar_i64_bit_not(file, functionIr, semIrInstruction, destinationSlot, leftSlot);
            return ZR_TRUE;
        }
        if (staticCType == ZR_STATIC_C_TYPE_U64) {
            backend_aot_write_c_scalar_u64_bit_not(file, functionIr, semIrInstruction, destinationSlot, leftSlot);
            return ZR_TRUE;
        }
    }

    operatorText = backend_aot_c_scalar_bitwise_operator(semIrInstruction->semIrOpcode);
    if (operatorText != ZR_NULL &&
        backend_aot_c_scalar_decode_bitwise_binary_operands(execInstruction,
                                                            &destinationSlot,
                                                            &leftSlot,
                                                            &rightSlot)) {
        if (staticCType == ZR_STATIC_C_TYPE_I64) {
            backend_aot_write_c_scalar_i64_bitwise(file,
                                                   functionIr,
                                                   semIrInstruction,
                                                   operatorText,
                                                   destinationSlot,
                                                   leftSlot,
                                                   rightSlot);
            return ZR_TRUE;
        }
        if (staticCType == ZR_STATIC_C_TYPE_U64) {
            backend_aot_write_c_scalar_u64_bitwise(file,
                                                   functionIr,
                                                   semIrInstruction,
                                                   operatorText,
                                                   destinationSlot,
                                                   leftSlot,
                                                   rightSlot);
            return ZR_TRUE;
        }
    }

    if ((semIrInstruction->semIrOpcode == ZR_SEMIR_OPCODE_SHL ||
         semIrInstruction->semIrOpcode == ZR_SEMIR_OPCODE_SHR) &&
        backend_aot_c_scalar_decode_shift_operands(execInstruction, &destinationSlot, &leftSlot, &rightSlot)) {
        if (staticCType == ZR_STATIC_C_TYPE_I64) {
            backend_aot_write_c_scalar_i64_shift(file,
                                                 functionIr,
                                                 semIrInstruction,
                                                 destinationSlot,
                                                 leftSlot,
                                                 rightSlot);
            return ZR_TRUE;
        }
        if (staticCType == ZR_STATIC_C_TYPE_U64) {
            backend_aot_write_c_scalar_u64_shift(file,
                                                 functionIr,
                                                 semIrInstruction,
                                                 destinationSlot,
                                                 leftSlot,
                                                 rightSlot);
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}
