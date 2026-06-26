#include "backend_aot_c_scalar_semir.h"
#include "backend_aot_c_scalar_binary.h"
#include "backend_aot_c_scalar_bitwise.h"
#include "backend_aot_c_scalar_conversion.h"
#include "backend_aot_c_scalar_locals.h"

static const SZrAotExecIrInstruction *backend_aot_c_scalar_find_exec_ir_instruction(
        const SZrAotExecIrModule *module,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 execInstructionIndex) {
    TZrUInt32 instructionIndex;

    if (module == ZR_NULL || functionIr == ZR_NULL || module->instructions == ZR_NULL) {
        return ZR_NULL;
    }

    for (instructionIndex = 0; instructionIndex < functionIr->instructionCount; instructionIndex++) {
        TZrUInt32 moduleInstructionIndex = functionIr->firstInstructionOffset + instructionIndex;
        const SZrAotExecIrInstruction *instruction;

        if (moduleInstructionIndex >= module->instructionCount) {
            break;
        }

        instruction = &module->instructions[moduleInstructionIndex];
        if (instruction->execInstructionIndex == execInstructionIndex) {
            return instruction;
        }
    }

    return ZR_NULL;
}

static EZrStaticCType backend_aot_c_scalar_static_type(
        const SZrAotExecIrFunction *functionIr,
        const SZrAotExecIrInstruction *instruction) {
    const SZrFunction *function;

    if (functionIr == ZR_NULL || instruction == ZR_NULL) {
        return ZR_STATIC_C_TYPE_DYNAMIC;
    }

    function = functionIr->function;
    if (function == ZR_NULL ||
        function->semIrTypeTable == ZR_NULL ||
        instruction->typeTableIndex >= function->semIrTypeTableLength) {
        return ZR_STATIC_C_TYPE_DYNAMIC;
    }

    return function->semIrTypeTable[instruction->typeTableIndex].staticCType;
}

static const char *backend_aot_c_scalar_u64_compare_operator(TZrUInt32 semIrOpcode) {
    switch ((EZrSemIrOpcode)semIrOpcode) {
        case ZR_SEMIR_OPCODE_EQ:
            return "==";
        case ZR_SEMIR_OPCODE_NE:
            return "!=";
        case ZR_SEMIR_OPCODE_LT:
            return "<";
        case ZR_SEMIR_OPCODE_LE:
            return "<=";
        case ZR_SEMIR_OPCODE_GT:
            return ">";
        case ZR_SEMIR_OPCODE_GE:
            return ">=";
        default:
            return ZR_NULL;
    }
}

static const char *backend_aot_c_scalar_i64_compare_operator(TZrUInt32 semIrOpcode) {
    switch ((EZrSemIrOpcode)semIrOpcode) {
        case ZR_SEMIR_OPCODE_EQ:
            return "==";
        case ZR_SEMIR_OPCODE_NE:
            return "!=";
        case ZR_SEMIR_OPCODE_LT:
            return "<";
        case ZR_SEMIR_OPCODE_LE:
            return "<=";
        case ZR_SEMIR_OPCODE_GT:
            return ">";
        case ZR_SEMIR_OPCODE_GE:
            return ">=";
        default:
            return ZR_NULL;
    }
}

static const char *backend_aot_c_scalar_f64_compare_operator(TZrUInt32 semIrOpcode) {
    switch ((EZrSemIrOpcode)semIrOpcode) {
        case ZR_SEMIR_OPCODE_EQ:
            return "==";
        case ZR_SEMIR_OPCODE_NE:
            return "!=";
        case ZR_SEMIR_OPCODE_LT:
            return "<";
        case ZR_SEMIR_OPCODE_LE:
            return "<=";
        case ZR_SEMIR_OPCODE_GT:
            return ">";
        case ZR_SEMIR_OPCODE_GE:
            return ">=";
        default:
            return ZR_NULL;
    }
}

static TZrBool backend_aot_c_scalar_read_i64_constant(const SZrFunction *function,
                                                      TZrUInt32 constantIndex,
                                                      TZrInt64 *outValue) {
    const SZrTypeValue *constantValue;

    if (function == ZR_NULL ||
        function->constantValueList == ZR_NULL ||
        constantIndex >= function->constantValueLength ||
        outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    constantValue = &function->constantValueList[constantIndex];
    if (!ZR_VALUE_IS_TYPE_SIGNED_INT(constantValue->type)) {
        return ZR_FALSE;
    }

    *outValue = constantValue->value.nativeObject.nativeInt64;
    return ZR_TRUE;
}

static TZrBool backend_aot_c_scalar_decode_signed_compare_operands(const SZrFunction *function,
                                                                   const TZrInstruction *instruction,
                                                                   TZrUInt32 *outDestinationSlot,
                                                                   TZrUInt32 *outLeftSlot,
                                                                   TZrUInt32 *outRightSlot,
                                                                   TZrBool *outHasRightLiteral,
                                                                   TZrInt64 *outRightLiteral) {
    EZrInstructionCode opcode;
    TZrUInt32 constantIndex;

    if (instruction == ZR_NULL ||
        outDestinationSlot == ZR_NULL ||
        outLeftSlot == ZR_NULL ||
        outRightSlot == ZR_NULL ||
        outHasRightLiteral == ZR_NULL ||
        outRightLiteral == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    *outDestinationSlot = instruction->instruction.operandExtra;
    *outLeftSlot = instruction->instruction.operand.operand1[0];
    *outRightSlot = instruction->instruction.operand.operand1[1];
    *outHasRightLiteral = ZR_FALSE;
    *outRightLiteral = 0;

    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_SIGNED):
            return ZR_TRUE;

        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_SIGNED_CONST):
            constantIndex = instruction->instruction.operand.operand1[1];
            break;

        default:
            return ZR_FALSE;
    }

    if (!backend_aot_c_scalar_read_i64_constant(function, constantIndex, outRightLiteral)) {
        return ZR_FALSE;
    }

    *outHasRightLiteral = ZR_TRUE;
    *outRightSlot = UINT32_MAX;
    return ZR_TRUE;
}

static TZrBool backend_aot_c_scalar_decode_unsigned_compare_operands(const TZrInstruction *instruction,
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
    *outDestinationSlot = instruction->instruction.operandExtra;
    *outLeftSlot = instruction->instruction.operand.operand1[0];
    *outRightSlot = instruction->instruction.operand.operand1[1];

    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_UNSIGNED):
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_c_scalar_decode_float_compare_operands(const TZrInstruction *instruction,
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
    *outDestinationSlot = instruction->instruction.operandExtra;
    *outLeftSlot = instruction->instruction.operand.operand1[0];
    *outRightSlot = instruction->instruction.operand.operand1[1];

    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_FLOAT):
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static void backend_aot_write_c_scalar_plain_bool_result(FILE *file, const char *resultExpression) {
    if (file == ZR_NULL || resultExpression == ZR_NULL) {
        return;
    }

    fprintf(file,
            "        if (zr_aot_destination->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE ||\n"
            "            zr_aot_destination->isGarbageCollectable) {\n"
            "            ZrCore_Ownership_ReleaseValue(state, zr_aot_destination);\n"
            "        }\n"
            "        zr_aot_destination->type = ZR_VALUE_TYPE_BOOL;\n"
            "        zr_aot_destination->value.nativeObject.nativeBool = (TZrBool)(%s != 0);\n"
            "        zr_aot_destination->isGarbageCollectable = ZR_FALSE;\n"
            "        zr_aot_destination->isNative = ZR_TRUE;\n"
            "        zr_aot_destination->ownershipKind = ZR_OWNERSHIP_VALUE_KIND_NONE;\n"
            "        zr_aot_destination->ownershipControl = ZR_NULL;\n"
            "        zr_aot_destination->ownershipWeakRef = ZR_NULL;\n",
            resultExpression);
}

static void backend_aot_write_c_scalar_i64_compare(FILE *file,
                                                   const SZrAotExecIrFunction *functionIr,
                                                   const SZrAotExecIrInstruction *semIrInstruction,
                                                   const char *operatorText,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 leftSlot,
                                                   TZrUInt32 rightSlot,
                                                   TZrBool hasRightLiteral,
                                                   TZrInt64 rightLiteral) {
    TZrBool useScalarDestination = backend_aot_c_scalar_locals_has_bool_slot(functionIr, destinationSlot);
    TZrBool useScalarLocals =
            useScalarDestination &&
            backend_aot_c_scalar_locals_has_i64_slot(functionIr, leftSlot) &&
            (hasRightLiteral || backend_aot_c_scalar_locals_has_i64_slot(functionIr, rightSlot));
    TZrBool leftLocalWrittenBefore = ZR_FALSE;
    TZrBool rightLocalWrittenBefore = ZR_FALSE;
    TZrBool useWrittenScalarSources = ZR_FALSE;
    TZrBool canSkipValueSlot = ZR_FALSE;

    if (useScalarLocals) {
        leftLocalWrittenBefore =
                backend_aot_c_scalar_locals_i64_written_before(functionIr, leftSlot, semIrInstruction->execInstructionIndex);
        rightLocalWrittenBefore =
                hasRightLiteral ||
                backend_aot_c_scalar_locals_i64_written_before(functionIr, rightSlot, semIrInstruction->execInstructionIndex);
        useWrittenScalarSources = (TZrBool)(leftLocalWrittenBefore && rightLocalWrittenBefore);
        canSkipValueSlot =
                (TZrBool)(useScalarDestination &&
                          useWrittenScalarSources &&
                          backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(
                                  functionIr, destinationSlot, semIrInstruction->execInstructionIndex));
    }

    if (canSkipValueSlot) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_scalar_exec_i64_compare semirOpcode=%u dstSlot=%u leftSlot=%u rightSlot=%u */\n",
                (unsigned)semIrInstruction->semIrOpcode,
                (unsigned)destinationSlot,
                (unsigned)leftSlot,
                hasRightLiteral ? 0u : (unsigned)rightSlot);
        if (hasRightLiteral) {
            fprintf(file,
                    "        zr_aot_b%u = (TZrBool)(zr_aot_s%u %s (TZrInt64)%lld);\n",
                    (unsigned)destinationSlot,
                    (unsigned)leftSlot,
                    operatorText,
                    (long long)rightLiteral);
        } else {
            fprintf(file,
                    "        zr_aot_b%u = (TZrBool)(zr_aot_s%u %s zr_aot_s%u);\n",
                    (unsigned)destinationSlot,
                    (unsigned)leftSlot,
                    operatorText,
                    (unsigned)rightSlot);
        }
        fprintf(file, "    }\n");
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_scalar_exec_i64_compare semirOpcode=%u dstSlot=%u leftSlot=%u rightSlot=%u */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;\n"
            "        TZrBool zr_aot_s_result;\n"
            "        if (frame.slotBase == ZR_NULL || %u >= frame.generatedFrameSlotCount ||\n"
            "            %u >= frame.generatedFrameSlotCount",
            (unsigned)semIrInstruction->semIrOpcode,
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            hasRightLiteral ? 0u : (unsigned)rightSlot,
            (unsigned)destinationSlot,
            (unsigned)leftSlot);
    if (!hasRightLiteral) {
        fprintf(file, " ||\n            %u >= frame.generatedFrameSlotCount", (unsigned)rightSlot);
    }
    fprintf(file,
            ") {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_destination = &frame.slotBase[%u].value;\n",
            (unsigned)destinationSlot);
    if (!useWrittenScalarSources) {
        TZrBool wroteSourceCheck = ZR_FALSE;

        fprintf(file, "        if (");
        if (!leftLocalWrittenBefore) {
            fprintf(file,
                    "!ZR_VALUE_IS_TYPE_SIGNED_INT(frame.slotBase[%u].value.type)",
                    (unsigned)leftSlot);
            wroteSourceCheck = ZR_TRUE;
        }
        if (!hasRightLiteral && !rightLocalWrittenBefore) {
            fprintf(file,
                    "%s!ZR_VALUE_IS_TYPE_SIGNED_INT(frame.slotBase[%u].value.type)",
                    wroteSourceCheck ? " ||\n            " : "",
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
        if (hasRightLiteral) {
            fprintf(file,
                    "        zr_aot_b%u = (TZrBool)(zr_aot_s%u %s (TZrInt64)%lld);\n",
                    (unsigned)destinationSlot,
                    (unsigned)leftSlot,
                    operatorText,
                    (long long)rightLiteral);
        } else {
            if (!rightLocalWrittenBefore) {
                fprintf(file,
                        "        zr_aot_s%u = frame.slotBase[%u].value.value.nativeObject.nativeInt64;\n",
                        (unsigned)rightSlot,
                        (unsigned)rightSlot);
            }
            fprintf(file,
                    "        zr_aot_b%u = (TZrBool)(zr_aot_s%u %s zr_aot_s%u);\n",
                    (unsigned)destinationSlot,
                    (unsigned)leftSlot,
                    operatorText,
                    (unsigned)rightSlot);
        }
        fprintf(file,
                "        zr_aot_s_result = zr_aot_b%u;\n",
                (unsigned)destinationSlot);
    } else {
        fprintf(file,
                "        TZrInt64 zr_aot_s_left;\n"
                "        TZrInt64 zr_aot_s_right;\n"
                "        zr_aot_s_left = frame.slotBase[%u].value.value.nativeObject.nativeInt64;\n",
                (unsigned)leftSlot);
        if (hasRightLiteral) {
            fprintf(file, "        zr_aot_s_right = (TZrInt64)%lld;\n", (long long)rightLiteral);
        } else {
            fprintf(file,
                    "        zr_aot_s_right = frame.slotBase[%u].value.value.nativeObject.nativeInt64;\n",
                    (unsigned)rightSlot);
        }
        fprintf(file,
                "        zr_aot_s_result = (TZrBool)(zr_aot_s_left %s zr_aot_s_right);\n",
                operatorText);
    }
    if (!useScalarLocals && useScalarDestination) {
        fprintf(file, "        zr_aot_b%u = zr_aot_s_result;\n", (unsigned)destinationSlot);
    }
    backend_aot_write_c_scalar_plain_bool_result(file, "zr_aot_s_result");
    fprintf(file, "    }\n");
}

static void backend_aot_write_c_scalar_u64_compare(FILE *file,
                                                   const SZrAotExecIrFunction *functionIr,
                                                   const SZrAotExecIrInstruction *semIrInstruction,
                                                   const char *operatorText,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 leftSlot,
                                                   TZrUInt32 rightSlot,
                                                   TZrUInt32 execInstructionIndex) {
    TZrBool useScalarOperands =
            backend_aot_c_scalar_locals_has_u64_slot(functionIr, leftSlot) &&
            backend_aot_c_scalar_locals_has_u64_slot(functionIr, rightSlot);
    TZrBool useScalarDestination = backend_aot_c_scalar_locals_has_bool_slot(functionIr, destinationSlot);
    TZrBool leftLocalWrittenBefore = ZR_FALSE;
    TZrBool rightLocalWrittenBefore = ZR_FALSE;
    TZrBool useWrittenScalarSources = ZR_FALSE;
    TZrBool canSkipValueSlot = ZR_FALSE;

    if (useScalarOperands) {
        leftLocalWrittenBefore =
                backend_aot_c_scalar_locals_u64_written_before(functionIr, leftSlot, execInstructionIndex);
        rightLocalWrittenBefore =
                backend_aot_c_scalar_locals_u64_written_before(functionIr, rightSlot, execInstructionIndex);
        useWrittenScalarSources = (TZrBool)(leftLocalWrittenBefore && rightLocalWrittenBefore);
        canSkipValueSlot =
                (TZrBool)(useScalarDestination &&
                          useWrittenScalarSources &&
                          backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(
                                  functionIr, destinationSlot, execInstructionIndex));
    }

    if (canSkipValueSlot) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_scalar_exec_u64_compare semirOpcode=%u dstSlot=%u leftSlot=%u rightSlot=%u */\n"
                "        zr_aot_b%u = (TZrBool)(zr_aot_u%u %s zr_aot_u%u);\n"
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
            "        /* zr_aot_scalar_exec_u64_compare semirOpcode=%u dstSlot=%u leftSlot=%u rightSlot=%u */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;\n"
            "        TZrBool zr_aot_u_result;\n"
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
    if (!leftLocalWrittenBefore || !rightLocalWrittenBefore) {
        TZrBool wroteSourceCheck = ZR_FALSE;
        fprintf(file, "        if (");
        if (!leftLocalWrittenBefore) {
            fprintf(file,
                    "!ZR_VALUE_IS_TYPE_UNSIGNED_INT(frame.slotBase[%u].value.type)",
                    (unsigned)leftSlot);
            wroteSourceCheck = ZR_TRUE;
        }
        if (!rightLocalWrittenBefore) {
            if (wroteSourceCheck) {
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
    if (useScalarOperands) {
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
        if (useScalarDestination) {
            fprintf(file,
                    "        zr_aot_b%u = (TZrBool)(zr_aot_u%u %s zr_aot_u%u);\n"
                    "        zr_aot_u_result = zr_aot_b%u;\n",
                    (unsigned)destinationSlot,
                    (unsigned)leftSlot,
                    operatorText,
                    (unsigned)rightSlot,
                    (unsigned)destinationSlot);
        } else {
            fprintf(file,
                    "        zr_aot_u_result = (TZrBool)(zr_aot_u%u %s zr_aot_u%u);\n",
                    (unsigned)leftSlot,
                    operatorText,
                    (unsigned)rightSlot);
        }
    } else {
        fprintf(file,
                "        TZrUInt64 zr_aot_u_left;\n"
                "        TZrUInt64 zr_aot_u_right;\n"
                "        zr_aot_u_left = frame.slotBase[%u].value.value.nativeObject.nativeUInt64;\n"
                "        zr_aot_u_right = frame.slotBase[%u].value.value.nativeObject.nativeUInt64;\n"
                "        zr_aot_u_result = (TZrBool)(zr_aot_u_left %s zr_aot_u_right);\n",
                (unsigned)leftSlot,
                (unsigned)rightSlot,
                operatorText);
    }
    if (!useScalarOperands && useScalarDestination) {
        fprintf(file, "        zr_aot_b%u = zr_aot_u_result;\n", (unsigned)destinationSlot);
    }
    backend_aot_write_c_scalar_plain_bool_result(file, "zr_aot_u_result");
    fprintf(file, "    }\n");
}

static void backend_aot_write_c_scalar_f64_compare(FILE *file,
                                                   const SZrAotExecIrFunction *functionIr,
                                                   const SZrAotExecIrInstruction *semIrInstruction,
                                                   const char *operatorText,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 leftSlot,
                                                   TZrUInt32 rightSlot,
                                                   TZrUInt32 execInstructionIndex) {
    TZrBool useScalarOperands =
            backend_aot_c_scalar_locals_has_f64_slot(functionIr, leftSlot) &&
            backend_aot_c_scalar_locals_has_f64_slot(functionIr, rightSlot);
    TZrBool useScalarDestination = backend_aot_c_scalar_locals_has_bool_slot(functionIr, destinationSlot);
    TZrBool leftLocalWrittenBefore = ZR_FALSE;
    TZrBool rightLocalWrittenBefore = ZR_FALSE;
    TZrBool useWrittenScalarSources = ZR_FALSE;
    TZrBool canSkipValueSlot = ZR_FALSE;

    if (useScalarOperands) {
        leftLocalWrittenBefore =
                backend_aot_c_scalar_locals_f64_written_before(functionIr, leftSlot, execInstructionIndex);
        rightLocalWrittenBefore =
                backend_aot_c_scalar_locals_f64_written_before(functionIr, rightSlot, execInstructionIndex);
        useWrittenScalarSources = (TZrBool)(leftLocalWrittenBefore && rightLocalWrittenBefore);
        canSkipValueSlot =
                (TZrBool)(useScalarDestination &&
                          useWrittenScalarSources &&
                          backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(
                                  functionIr, destinationSlot, execInstructionIndex));
    }

    if (canSkipValueSlot) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_scalar_exec_f64_compare semirOpcode=%u dstSlot=%u leftSlot=%u rightSlot=%u */\n"
                "        zr_aot_b%u = (TZrBool)(zr_aot_f%u %s zr_aot_f%u);\n"
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
            "        /* zr_aot_scalar_exec_f64_compare semirOpcode=%u dstSlot=%u leftSlot=%u rightSlot=%u */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;\n"
            "        TZrBool zr_aot_b_result;\n"
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
    if (!leftLocalWrittenBefore || !rightLocalWrittenBefore) {
        TZrBool wroteSourceCheck = ZR_FALSE;
        fprintf(file, "        if (");
        if (!leftLocalWrittenBefore) {
            fprintf(file,
                    "!ZR_VALUE_IS_TYPE_FLOAT(frame.slotBase[%u].value.type)",
                    (unsigned)leftSlot);
            wroteSourceCheck = ZR_TRUE;
        }
        if (!rightLocalWrittenBefore) {
            if (wroteSourceCheck) {
                fprintf(file, " ||\n            ");
            }
            fprintf(file,
                    "!ZR_VALUE_IS_TYPE_FLOAT(frame.slotBase[%u].value.type)",
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
                    "        zr_aot_f%u = frame.slotBase[%u].value.value.nativeObject.nativeDouble;\n",
                    (unsigned)leftSlot,
                    (unsigned)leftSlot);
        }
        if (!rightLocalWrittenBefore) {
            fprintf(file,
                    "        zr_aot_f%u = frame.slotBase[%u].value.value.nativeObject.nativeDouble;\n",
                    (unsigned)rightSlot,
                    (unsigned)rightSlot);
        }
        if (useScalarDestination) {
            fprintf(file,
                    "        zr_aot_b%u = (TZrBool)(zr_aot_f%u %s zr_aot_f%u);\n"
                    "        zr_aot_b_result = zr_aot_b%u;\n",
                    (unsigned)destinationSlot,
                    (unsigned)leftSlot,
                    operatorText,
                    (unsigned)rightSlot,
                    (unsigned)destinationSlot);
        } else {
            fprintf(file,
                    "        zr_aot_b_result = (TZrBool)(zr_aot_f%u %s zr_aot_f%u);\n",
                    (unsigned)leftSlot,
                    operatorText,
                    (unsigned)rightSlot);
        }
    } else {
        fprintf(file,
                "        TZrFloat64 zr_aot_f_left;\n"
                "        TZrFloat64 zr_aot_f_right;\n"
                "        zr_aot_f_left = frame.slotBase[%u].value.value.nativeObject.nativeDouble;\n"
                "        zr_aot_f_right = frame.slotBase[%u].value.value.nativeObject.nativeDouble;\n"
                "        zr_aot_b_result = (TZrBool)(zr_aot_f_left %s zr_aot_f_right);\n",
                (unsigned)leftSlot,
                (unsigned)rightSlot,
                operatorText);
    }
    if (!useScalarOperands && useScalarDestination) {
        fprintf(file, "        zr_aot_b%u = zr_aot_b_result;\n", (unsigned)destinationSlot);
    }
    backend_aot_write_c_scalar_plain_bool_result(file, "zr_aot_b_result");
    fprintf(file, "    }\n");
}

static TZrBool backend_aot_c_scalar_semir_file_contains_frame_reference(FILE *file) {
    static const char needle[] = "frame.";
    TZrSize matchedLength = 0u;
    int character;

    if (file == ZR_NULL) {
        return ZR_TRUE;
    }

    rewind(file);
    while ((character = fgetc(file)) != EOF) {
        if ((char)character == needle[matchedLength]) {
            matchedLength++;
            if (matchedLength == (TZrSize)(sizeof(needle) - 1u)) {
                return ZR_TRUE;
            }
            continue;
        }

        matchedLength = ((char)character == needle[0]) ? 1u : 0u;
    }

    return ferror(file) ? ZR_TRUE : ZR_FALSE;
}

TZrBool backend_aot_try_write_c_scalar_semir_for_exec_instruction(FILE *file,
                                                                  const SZrAotExecIrModule *module,
                                                                  const SZrAotExecIrFunction *functionIr,
                                                                  const TZrInstruction *execInstruction,
                                                                  TZrUInt32 execInstructionIndex) {
    const SZrAotExecIrInstruction *semIrInstruction;
    const SZrFunction *function;
    const char *unsignedCompareOperatorText;
    const char *compareOperatorText;
    const char *floatCompareOperatorText;
    TZrUInt32 destinationSlot;
    TZrUInt32 leftSlot;
    TZrUInt32 rightSlot;
    TZrBool hasRightLiteral;
    TZrInt64 rightLiteral;
    EZrStaticCType staticCType;

    if (file == ZR_NULL || functionIr == ZR_NULL || execInstruction == ZR_NULL) {
        return ZR_FALSE;
    }

    if (backend_aot_try_write_c_scalar_conversion(file, functionIr, execInstruction, execInstructionIndex)) {
        return ZR_TRUE;
    }

    semIrInstruction = backend_aot_c_scalar_find_exec_ir_instruction(module, functionIr, execInstructionIndex);
    staticCType = backend_aot_c_scalar_static_type(functionIr, semIrInstruction);
    if (backend_aot_try_write_c_scalar_bitwise(file, functionIr, semIrInstruction, execInstruction, staticCType)) {
        return ZR_TRUE;
    }

    if (backend_aot_try_write_c_scalar_binary(file, functionIr, semIrInstruction, execInstruction, staticCType)) {
        return ZR_TRUE;
    }

    floatCompareOperatorText = semIrInstruction != ZR_NULL
            ? backend_aot_c_scalar_f64_compare_operator(semIrInstruction->semIrOpcode)
            : ZR_NULL;
    if (floatCompareOperatorText != ZR_NULL &&
        backend_aot_c_scalar_decode_float_compare_operands(execInstruction,
                                                           &destinationSlot,
                                                           &leftSlot,
                                                           &rightSlot)) {
        backend_aot_write_c_scalar_f64_compare(file,
                                               functionIr,
                                               semIrInstruction,
                                               floatCompareOperatorText,
                                               destinationSlot,
                                               leftSlot,
                                               rightSlot,
                                               execInstructionIndex);
        return ZR_TRUE;
    }

    function = functionIr->function;
    unsignedCompareOperatorText = semIrInstruction != ZR_NULL
            ? backend_aot_c_scalar_u64_compare_operator(semIrInstruction->semIrOpcode)
            : ZR_NULL;
    if (unsignedCompareOperatorText != ZR_NULL &&
        backend_aot_c_scalar_decode_unsigned_compare_operands(execInstruction,
                                                              &destinationSlot,
                                                              &leftSlot,
                                                              &rightSlot)) {
        backend_aot_write_c_scalar_u64_compare(file,
                                               functionIr,
                                               semIrInstruction,
                                               unsignedCompareOperatorText,
                                               destinationSlot,
                                               leftSlot,
                                               rightSlot,
                                               execInstructionIndex);
        return ZR_TRUE;
    }

    compareOperatorText = semIrInstruction != ZR_NULL
            ? backend_aot_c_scalar_i64_compare_operator(semIrInstruction->semIrOpcode)
            : ZR_NULL;
    if (compareOperatorText != ZR_NULL &&
        backend_aot_c_scalar_decode_signed_compare_operands(function,
                                                            execInstruction,
                                                            &destinationSlot,
                                                            &leftSlot,
                                                            &rightSlot,
                                                            &hasRightLiteral,
                                                            &rightLiteral)) {
        backend_aot_write_c_scalar_i64_compare(file,
                                               functionIr,
                                               semIrInstruction,
                                               compareOperatorText,
                                               destinationSlot,
                                               leftSlot,
                                               rightSlot,
                                               hasRightLiteral,
                                               rightLiteral);
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

TZrBool backend_aot_c_scalar_semir_can_write_frame_free_for_exec_instruction(
        const SZrAotExecIrModule *module,
        const SZrAotExecIrFunction *functionIr,
        const TZrInstruction *execInstruction,
        TZrUInt32 execInstructionIndex) {
    FILE *scratchFile;
    TZrBool wroteInstruction;
    TZrBool containsFrameReference;

    scratchFile = tmpfile();
    if (scratchFile == ZR_NULL) {
        return ZR_FALSE;
    }

    wroteInstruction = backend_aot_try_write_c_scalar_semir_for_exec_instruction(
            scratchFile, module, functionIr, execInstruction, execInstructionIndex);
    if (!wroteInstruction || fflush(scratchFile) != 0) {
        fclose(scratchFile);
        return ZR_FALSE;
    }

    containsFrameReference = backend_aot_c_scalar_semir_file_contains_frame_reference(scratchFile);
    fclose(scratchFile);
    return (TZrBool)!containsFrameReference;
}
