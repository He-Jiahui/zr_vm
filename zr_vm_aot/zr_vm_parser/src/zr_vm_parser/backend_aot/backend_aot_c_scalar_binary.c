#include "backend_aot_c_scalar_binary.h"
#include "backend_aot_c_scalar_locals.h"

static const char *backend_aot_c_scalar_i64_operator(TZrUInt32 semIrOpcode) {
    switch ((EZrSemIrOpcode)semIrOpcode) {
        case ZR_SEMIR_OPCODE_ADD:
            return "+";
        case ZR_SEMIR_OPCODE_SUB:
            return "-";
        case ZR_SEMIR_OPCODE_MUL:
            return "*";
        case ZR_SEMIR_OPCODE_DIV:
            return "/";
        case ZR_SEMIR_OPCODE_MOD:
            return "%";
        default:
            return ZR_NULL;
    }
}

static const char *backend_aot_c_scalar_u64_operator(TZrUInt32 semIrOpcode) {
    switch ((EZrSemIrOpcode)semIrOpcode) {
        case ZR_SEMIR_OPCODE_ADD:
            return "+";
        case ZR_SEMIR_OPCODE_SUB:
            return "-";
        case ZR_SEMIR_OPCODE_MUL:
            return "*";
        case ZR_SEMIR_OPCODE_DIV:
            return "/";
        case ZR_SEMIR_OPCODE_MOD:
            return "%";
        default:
            return ZR_NULL;
    }
}

static const char *backend_aot_c_scalar_f64_operator(TZrUInt32 semIrOpcode) {
    switch ((EZrSemIrOpcode)semIrOpcode) {
        case ZR_SEMIR_OPCODE_ADD:
            return "+";
        case ZR_SEMIR_OPCODE_SUB:
            return "-";
        case ZR_SEMIR_OPCODE_MUL:
            return "*";
        case ZR_SEMIR_OPCODE_DIV:
            return "/";
        case ZR_SEMIR_OPCODE_MOD:
            return "%";
        default:
            return ZR_NULL;
    }
}

static TZrBool backend_aot_c_scalar_opcode_is_binary_arithmetic(EZrInstructionCode opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(ADD_FLOAT):
        case ZR_INSTRUCTION_ENUM(SUB_FLOAT):
        case ZR_INSTRUCTION_ENUM(MUL_FLOAT):
        case ZR_INSTRUCTION_ENUM(DIV_FLOAT):
        case ZR_INSTRUCTION_ENUM(MOD_FLOAT):
            return ZR_TRUE;
        default:
            return ZR_FALSE;
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

static TZrBool backend_aot_c_scalar_read_u64_constant(const SZrFunction *function,
                                                      TZrUInt32 constantIndex,
                                                      TZrUInt64 *outValue) {
    const SZrTypeValue *constantValue;

    if (function == ZR_NULL ||
        function->constantValueList == ZR_NULL ||
        constantIndex >= function->constantValueLength ||
        outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    constantValue = &function->constantValueList[constantIndex];
    if (!ZR_VALUE_IS_TYPE_UNSIGNED_INT(constantValue->type)) {
        return ZR_FALSE;
    }

    *outValue = constantValue->value.nativeObject.nativeUInt64;
    return ZR_TRUE;
}

static TZrBool backend_aot_c_scalar_decode_float_binary_operands(const TZrInstruction *instruction,
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
    if (!backend_aot_c_scalar_opcode_is_binary_arithmetic(opcode)) {
        return ZR_FALSE;
    }

    *outDestinationSlot = instruction->instruction.operandExtra;
    *outLeftSlot = instruction->instruction.operand.operand1[0];
    *outRightSlot = instruction->instruction.operand.operand1[1];
    return ZR_TRUE;
}

static TZrBool backend_aot_c_scalar_decode_signed_binary_operands(const SZrFunction *function,
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
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
            return ZR_TRUE;

        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK):
            *outLeftSlot = instruction->instruction.operand.operand0[0];
            *outRightSlot = instruction->instruction.operand.operand0[1];
            return ZR_TRUE;

        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST_PLAIN_DEST):
            constantIndex = instruction->instruction.operand.operand1[1];
            break;

        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_LOAD_CONST):
            *outLeftSlot = instruction->instruction.operand.operand0[0];
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

static TZrBool backend_aot_c_scalar_decode_unsigned_binary_operands(const SZrFunction *function,
                                                                    const TZrInstruction *instruction,
                                                                    TZrUInt32 *outDestinationSlot,
                                                                    TZrUInt32 *outLeftSlot,
                                                                    TZrUInt32 *outRightSlot,
                                                                    TZrBool *outHasRightLiteral,
                                                                    TZrUInt64 *outRightLiteral) {
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
    *outRightLiteral = 0u;

    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED):
            return ZR_TRUE;

        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST_PLAIN_DEST):
            constantIndex = instruction->instruction.operand.operand1[1];
            break;

        default:
            return ZR_FALSE;
    }

    if (!backend_aot_c_scalar_read_u64_constant(function, constantIndex, outRightLiteral)) {
        return ZR_FALSE;
    }

    *outHasRightLiteral = ZR_TRUE;
    *outRightSlot = UINT32_MAX;
    return ZR_TRUE;
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

static void backend_aot_write_c_scalar_plain_f64_result(FILE *file, const char *resultExpression) {
    if (file == ZR_NULL || resultExpression == ZR_NULL) {
        return;
    }

    fprintf(file,
            "        if (zr_aot_destination->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE ||\n"
            "            zr_aot_destination->isGarbageCollectable) {\n"
            "            ZrCore_Ownership_ReleaseValue(state, zr_aot_destination);\n"
            "        }\n"
            "        zr_aot_destination->type = ZR_VALUE_TYPE_DOUBLE;\n"
            "        zr_aot_destination->value.nativeObject.nativeDouble = %s;\n"
            "        zr_aot_destination->isGarbageCollectable = ZR_FALSE;\n"
            "        zr_aot_destination->isNative = ZR_TRUE;\n"
            "        zr_aot_destination->ownershipKind = ZR_OWNERSHIP_VALUE_KIND_NONE;\n"
            "        zr_aot_destination->ownershipControl = ZR_NULL;\n"
            "        zr_aot_destination->ownershipWeakRef = ZR_NULL;\n",
            resultExpression);
}

static void backend_aot_write_c_scalar_i64_binary(FILE *file,
                                                  const SZrAotExecIrFunction *functionIr,
                                                  const SZrAotExecIrInstruction *semIrInstruction,
                                                  const char *operatorText,
                                                  TZrUInt32 destinationSlot,
                                                  TZrUInt32 leftSlot,
                                                  TZrUInt32 rightSlot,
                                                  TZrBool hasRightLiteral,
                                                  TZrInt64 rightLiteral) {
    TZrBool useScalarDestination = backend_aot_c_scalar_locals_has_i64_slot(functionIr, destinationSlot);
    TZrBool useScalarLocals =
            useScalarDestination &&
            backend_aot_c_scalar_locals_has_i64_slot(functionIr, leftSlot) &&
            (hasRightLiteral || backend_aot_c_scalar_locals_has_i64_slot(functionIr, rightSlot));

    fprintf(file,
            "    {\n"
            "        /* zr_aot_scalar_exec_i64_binary semirOpcode=%u dstSlot=%u leftSlot=%u rightSlot=%u */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;\n"
            "        TZrInt64 zr_aot_s_result;\n"
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
            "        zr_aot_destination = &frame.slotBase[%u].value;\n"
            "        if (!ZR_VALUE_IS_TYPE_SIGNED_INT(frame.slotBase[%u].value.type)",
            (unsigned)destinationSlot,
            (unsigned)leftSlot);
    if (!hasRightLiteral) {
        fprintf(file,
                " ||\n"
                "            !ZR_VALUE_IS_TYPE_SIGNED_INT(frame.slotBase[%u].value.type)",
                (unsigned)rightSlot);
    }
    fprintf(file,
            ") {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n");
    if (useScalarLocals) {
        fprintf(file,
                "        zr_aot_s%u = frame.slotBase[%u].value.value.nativeObject.nativeInt64;\n",
                (unsigned)leftSlot,
                (unsigned)leftSlot);
        if (hasRightLiteral) {
            if (semIrInstruction->semIrOpcode == ZR_SEMIR_OPCODE_DIV ||
                semIrInstruction->semIrOpcode == ZR_SEMIR_OPCODE_MOD) {
                fprintf(file,
                        "        if ((TZrInt64)%lld == 0) {\n"
                        "            ZrCore_Debug_RunError(state, \"generated AOT integer divide by zero\");\n"
                        "            ZR_AOT_C_FAIL();\n"
                        "        }\n",
                        (long long)rightLiteral);
            }
            fprintf(file,
                    "        zr_aot_s%u = zr_aot_s%u %s (TZrInt64)%lld;\n",
                    (unsigned)destinationSlot,
                    (unsigned)leftSlot,
                    operatorText,
                    (long long)rightLiteral);
        } else {
            fprintf(file,
                    "        zr_aot_s%u = frame.slotBase[%u].value.value.nativeObject.nativeInt64;\n",
                    (unsigned)rightSlot,
                    (unsigned)rightSlot);
            if (semIrInstruction->semIrOpcode == ZR_SEMIR_OPCODE_DIV ||
                semIrInstruction->semIrOpcode == ZR_SEMIR_OPCODE_MOD) {
                fprintf(file,
                        "        if (zr_aot_s%u == 0) {\n"
                        "            ZrCore_Debug_RunError(state, \"generated AOT integer divide by zero\");\n"
                        "            ZR_AOT_C_FAIL();\n"
                        "        }\n",
                        (unsigned)rightSlot);
            }
            fprintf(file,
                    "        zr_aot_s%u = zr_aot_s%u %s zr_aot_s%u;\n",
                    (unsigned)destinationSlot,
                    (unsigned)leftSlot,
                    operatorText,
                    (unsigned)rightSlot);
        }
        fprintf(file,
                "        zr_aot_s_result = zr_aot_s%u;\n",
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
        if (semIrInstruction->semIrOpcode == ZR_SEMIR_OPCODE_DIV ||
            semIrInstruction->semIrOpcode == ZR_SEMIR_OPCODE_MOD) {
            fprintf(file,
                    "        if (zr_aot_s_right == 0) {\n"
                    "            ZrCore_Debug_RunError(state, \"generated AOT integer divide by zero\");\n"
                    "            ZR_AOT_C_FAIL();\n"
                    "        }\n");
        }
        fprintf(file,
                "        zr_aot_s_result = zr_aot_s_left %s zr_aot_s_right;\n",
                operatorText);
    }
    if (!useScalarLocals && useScalarDestination) {
        fprintf(file, "        zr_aot_s%u = zr_aot_s_result;\n", (unsigned)destinationSlot);
    }
    backend_aot_write_c_scalar_plain_i64_result(file, "zr_aot_s_result");
    fprintf(file, "    }\n");
}

static void backend_aot_write_c_scalar_u64_binary(FILE *file,
                                                  const SZrAotExecIrFunction *functionIr,
                                                  const SZrAotExecIrInstruction *semIrInstruction,
                                                  const char *operatorText,
                                                  TZrUInt32 destinationSlot,
                                                  TZrUInt32 leftSlot,
                                                   TZrUInt32 rightSlot,
                                                   TZrBool hasRightLiteral,
                                                   TZrUInt64 rightLiteral) {
    TZrBool useScalarDestination = backend_aot_c_scalar_locals_has_u64_slot(functionIr, destinationSlot);
    TZrBool useScalarLocals =
            useScalarDestination &&
            backend_aot_c_scalar_locals_has_u64_slot(functionIr, leftSlot) &&
            (hasRightLiteral || backend_aot_c_scalar_locals_has_u64_slot(functionIr, rightSlot));

    fprintf(file,
            "    {\n"
            "        /* zr_aot_scalar_exec_u64_binary semirOpcode=%u dstSlot=%u leftSlot=%u rightSlot=%u */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;\n"
            "        TZrUInt64 zr_aot_u_result;\n"
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
            "        zr_aot_destination = &frame.slotBase[%u].value;\n"
            "        if (!ZR_VALUE_IS_TYPE_UNSIGNED_INT(frame.slotBase[%u].value.type)",
            (unsigned)destinationSlot,
            (unsigned)leftSlot);
    if (!hasRightLiteral) {
        fprintf(file,
                " ||\n"
                "            !ZR_VALUE_IS_TYPE_UNSIGNED_INT(frame.slotBase[%u].value.type)",
                (unsigned)rightSlot);
    }
    fprintf(file,
            ") {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n");
    if (useScalarLocals) {
        fprintf(file,
                "        zr_aot_u%u = frame.slotBase[%u].value.value.nativeObject.nativeUInt64;\n",
                (unsigned)leftSlot,
                (unsigned)leftSlot);
        if (hasRightLiteral) {
            if (semIrInstruction->semIrOpcode == ZR_SEMIR_OPCODE_DIV ||
                semIrInstruction->semIrOpcode == ZR_SEMIR_OPCODE_MOD) {
                fprintf(file,
                        "        if ((TZrUInt64)%llu == 0u) {\n"
                        "            ZrCore_Debug_RunError(state, \"generated AOT unsigned integer divide by zero\");\n"
                        "            ZR_AOT_C_FAIL();\n"
                        "        }\n",
                        (unsigned long long)rightLiteral);
            }
            fprintf(file,
                    "        zr_aot_u%u = zr_aot_u%u %s (TZrUInt64)%llu;\n",
                    (unsigned)destinationSlot,
                    (unsigned)leftSlot,
                    operatorText,
                    (unsigned long long)rightLiteral);
        } else {
            fprintf(file,
                    "        zr_aot_u%u = frame.slotBase[%u].value.value.nativeObject.nativeUInt64;\n",
                    (unsigned)rightSlot,
                    (unsigned)rightSlot);
            if (semIrInstruction->semIrOpcode == ZR_SEMIR_OPCODE_DIV ||
                semIrInstruction->semIrOpcode == ZR_SEMIR_OPCODE_MOD) {
                fprintf(file,
                        "        if (zr_aot_u%u == 0u) {\n"
                        "            ZrCore_Debug_RunError(state, \"generated AOT unsigned integer divide by zero\");\n"
                        "            ZR_AOT_C_FAIL();\n"
                        "        }\n",
                        (unsigned)rightSlot);
            }
            fprintf(file,
                    "        zr_aot_u%u = zr_aot_u%u %s zr_aot_u%u;\n",
                    (unsigned)destinationSlot,
                    (unsigned)leftSlot,
                    operatorText,
                    (unsigned)rightSlot);
        }
        fprintf(file,
                "        zr_aot_u_result = zr_aot_u%u;\n",
                (unsigned)destinationSlot);
    } else {
        fprintf(file,
                "        TZrUInt64 zr_aot_u_left;\n"
                "        TZrUInt64 zr_aot_u_right;\n"
                "        zr_aot_u_left = frame.slotBase[%u].value.value.nativeObject.nativeUInt64;\n",
                (unsigned)leftSlot);
        if (hasRightLiteral) {
            fprintf(file, "        zr_aot_u_right = (TZrUInt64)%llu;\n", (unsigned long long)rightLiteral);
        } else {
            fprintf(file,
                    "        zr_aot_u_right = frame.slotBase[%u].value.value.nativeObject.nativeUInt64;\n",
                    (unsigned)rightSlot);
        }
        if (semIrInstruction->semIrOpcode == ZR_SEMIR_OPCODE_DIV ||
            semIrInstruction->semIrOpcode == ZR_SEMIR_OPCODE_MOD) {
            fprintf(file,
                    "        if (zr_aot_u_right == 0u) {\n"
                    "            ZrCore_Debug_RunError(state, \"generated AOT unsigned integer divide by zero\");\n"
                    "            ZR_AOT_C_FAIL();\n"
                    "        }\n");
        }
        fprintf(file,
                "        zr_aot_u_result = zr_aot_u_left %s zr_aot_u_right;\n",
                operatorText);
    }
    if (!useScalarLocals && useScalarDestination) {
        fprintf(file, "        zr_aot_u%u = zr_aot_u_result;\n", (unsigned)destinationSlot);
    }
    backend_aot_write_c_scalar_plain_u64_result(file, "zr_aot_u_result");
    fprintf(file, "    }\n");
}

static void backend_aot_write_c_scalar_f64_binary(FILE *file,
                                                  const SZrAotExecIrFunction *functionIr,
                                                  const SZrAotExecIrInstruction *semIrInstruction,
                                                  const char *operatorText,
                                                  TZrUInt32 destinationSlot,
                                                  TZrUInt32 leftSlot,
                                                  TZrUInt32 rightSlot) {
    TZrBool useScalarDestination = backend_aot_c_scalar_locals_has_f64_slot(functionIr, destinationSlot);
    TZrBool useScalarLocals =
            useScalarDestination &&
            backend_aot_c_scalar_locals_has_f64_slot(functionIr, leftSlot) &&
            backend_aot_c_scalar_locals_has_f64_slot(functionIr, rightSlot);

    fprintf(file,
            "    {\n"
            "        /* zr_aot_scalar_exec_f64_binary semirOpcode=%u dstSlot=%u leftSlot=%u rightSlot=%u */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;\n"
            "        TZrFloat64 zr_aot_f_result;\n"
            "        if (frame.slotBase == ZR_NULL || %u >= frame.generatedFrameSlotCount ||\n"
            "            %u >= frame.generatedFrameSlotCount ||\n"
            "            %u >= frame.generatedFrameSlotCount) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_destination = &frame.slotBase[%u].value;\n"
            "        if (!ZR_VALUE_IS_TYPE_FLOAT(frame.slotBase[%u].value.type) ||\n"
            "            !ZR_VALUE_IS_TYPE_FLOAT(frame.slotBase[%u].value.type)) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n",
            (unsigned)semIrInstruction->semIrOpcode,
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot,
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot,
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
    if (useScalarLocals) {
        fprintf(file,
                "        zr_aot_f%u = frame.slotBase[%u].value.value.nativeObject.nativeDouble;\n"
                "        zr_aot_f%u = frame.slotBase[%u].value.value.nativeObject.nativeDouble;\n",
                (unsigned)leftSlot,
                (unsigned)leftSlot,
                (unsigned)rightSlot,
                (unsigned)rightSlot);
        if (semIrInstruction->semIrOpcode == ZR_SEMIR_OPCODE_DIV ||
            semIrInstruction->semIrOpcode == ZR_SEMIR_OPCODE_MOD) {
            fprintf(file,
                    "        if (zr_aot_f%u == 0.0) {\n"
                    "            ZrCore_Debug_RunError(state, \"generated AOT float divide by zero\");\n"
                    "            ZR_AOT_C_FAIL();\n"
                    "        }\n",
                    (unsigned)rightSlot);
        }
        if (semIrInstruction->semIrOpcode == ZR_SEMIR_OPCODE_MOD) {
            fprintf(file,
                    "        zr_aot_f%u = fmod(zr_aot_f%u, zr_aot_f%u);\n",
                    (unsigned)destinationSlot,
                    (unsigned)leftSlot,
                    (unsigned)rightSlot);
        } else {
            fprintf(file,
                    "        zr_aot_f%u = zr_aot_f%u %s zr_aot_f%u;\n",
                    (unsigned)destinationSlot,
                    (unsigned)leftSlot,
                    operatorText,
                    (unsigned)rightSlot);
        }
        fprintf(file,
                "        zr_aot_f_result = zr_aot_f%u;\n",
                (unsigned)destinationSlot);
    } else {
        fprintf(file,
                "        TZrFloat64 zr_aot_f_left;\n"
                "        TZrFloat64 zr_aot_f_right;\n"
                "        zr_aot_f_left = frame.slotBase[%u].value.value.nativeObject.nativeDouble;\n"
                "        zr_aot_f_right = frame.slotBase[%u].value.value.nativeObject.nativeDouble;\n",
                (unsigned)leftSlot,
                (unsigned)rightSlot);
        if (semIrInstruction->semIrOpcode == ZR_SEMIR_OPCODE_DIV ||
            semIrInstruction->semIrOpcode == ZR_SEMIR_OPCODE_MOD) {
            fprintf(file,
                    "        if (zr_aot_f_right == 0.0) {\n"
                    "            ZrCore_Debug_RunError(state, \"generated AOT float divide by zero\");\n"
                    "            ZR_AOT_C_FAIL();\n"
                    "        }\n");
        }
        if (semIrInstruction->semIrOpcode == ZR_SEMIR_OPCODE_MOD) {
            fprintf(file,
                    "        zr_aot_f_result = fmod(zr_aot_f_left, zr_aot_f_right);\n");
        } else {
            fprintf(file,
                    "        zr_aot_f_result = zr_aot_f_left %s zr_aot_f_right;\n",
                    operatorText);
        }
    }
    if (!useScalarLocals && useScalarDestination) {
        fprintf(file, "        zr_aot_f%u = zr_aot_f_result;\n", (unsigned)destinationSlot);
    }
    backend_aot_write_c_scalar_plain_f64_result(file, "zr_aot_f_result");
    fprintf(file, "    }\n");
}

TZrBool backend_aot_try_write_c_scalar_binary(FILE *file,
                                              const SZrAotExecIrFunction *functionIr,
                                              const SZrAotExecIrInstruction *semIrInstruction,
                                              const TZrInstruction *execInstruction,
                                              EZrStaticCType staticCType) {
    const SZrFunction *function;
    const char *operatorText;
    TZrUInt32 destinationSlot;
    TZrUInt32 leftSlot;
    TZrUInt32 rightSlot;
    TZrBool hasRightLiteral;
    TZrInt64 rightLiteral;
    TZrUInt64 unsignedRightLiteral;

    if (file == ZR_NULL || functionIr == ZR_NULL || execInstruction == ZR_NULL || semIrInstruction == ZR_NULL) {
        return ZR_FALSE;
    }

    function = functionIr->function;
    operatorText = backend_aot_c_scalar_i64_operator(semIrInstruction->semIrOpcode);
    if (operatorText != ZR_NULL &&
        staticCType == ZR_STATIC_C_TYPE_I64 &&
        backend_aot_c_scalar_decode_signed_binary_operands(function,
                                                           execInstruction,
                                                           &destinationSlot,
                                                           &leftSlot,
                                                           &rightSlot,
                                                           &hasRightLiteral,
                                                           &rightLiteral)) {
        backend_aot_write_c_scalar_i64_binary(file,
                                              functionIr,
                                              semIrInstruction,
                                              operatorText,
                                              destinationSlot,
                                              leftSlot,
                                              rightSlot,
                                              hasRightLiteral,
                                              rightLiteral);
        return ZR_TRUE;
    }

    operatorText = backend_aot_c_scalar_u64_operator(semIrInstruction->semIrOpcode);
    if (operatorText != ZR_NULL &&
        staticCType == ZR_STATIC_C_TYPE_U64 &&
        backend_aot_c_scalar_decode_unsigned_binary_operands(function,
                                                             execInstruction,
                                                             &destinationSlot,
                                                             &leftSlot,
                                                             &rightSlot,
                                                             &hasRightLiteral,
                                                             &unsignedRightLiteral)) {
        backend_aot_write_c_scalar_u64_binary(file,
                                              functionIr,
                                              semIrInstruction,
                                              operatorText,
                                              destinationSlot,
                                              leftSlot,
                                              rightSlot,
                                              hasRightLiteral,
                                              unsignedRightLiteral);
        return ZR_TRUE;
    }

    operatorText = backend_aot_c_scalar_f64_operator(semIrInstruction->semIrOpcode);
    if (operatorText != ZR_NULL &&
        staticCType == ZR_STATIC_C_TYPE_F64 &&
        backend_aot_c_scalar_decode_float_binary_operands(execInstruction,
                                                          &destinationSlot,
                                                          &leftSlot,
                                                          &rightSlot)) {
        backend_aot_write_c_scalar_f64_binary(file,
                                              functionIr,
                                              semIrInstruction,
                                              operatorText,
                                              destinationSlot,
                                              leftSlot,
                                              rightSlot);
        return ZR_TRUE;
    }

    return ZR_FALSE;
}
