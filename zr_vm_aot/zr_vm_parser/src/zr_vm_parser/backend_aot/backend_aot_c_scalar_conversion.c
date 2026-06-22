#include "backend_aot_c_scalar_conversion.h"

#include "backend_aot_c_scalar_locals.h"

static TZrBool backend_aot_c_scalar_conversion_decode_to_u64(const TZrInstruction *instruction,
                                                             TZrUInt32 *outDestinationSlot,
                                                             TZrUInt32 *outSourceSlot,
                                                             EZrInstructionCode *outOpcode) {
    EZrInstructionCode opcode;

    if (instruction == ZR_NULL ||
        outDestinationSlot == ZR_NULL ||
        outSourceSlot == ZR_NULL ||
        outOpcode == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(TO_UINT):
        case ZR_INSTRUCTION_ENUM(TO_UINT_FLOAT):
        case ZR_INSTRUCTION_ENUM(TO_UINT_SIGNED):
            *outOpcode = opcode;
            *outDestinationSlot = instruction->instruction.operandExtra;
            *outSourceSlot = instruction->instruction.operand.operand1[0];
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_c_scalar_conversion_decode_to_i64(const TZrInstruction *instruction,
                                                             TZrUInt32 *outDestinationSlot,
                                                             TZrUInt32 *outSourceSlot,
                                                             EZrInstructionCode *outOpcode) {
    EZrInstructionCode opcode;

    if (instruction == ZR_NULL ||
        outDestinationSlot == ZR_NULL ||
        outSourceSlot == ZR_NULL ||
        outOpcode == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(TO_INT):
        case ZR_INSTRUCTION_ENUM(TO_INT_FLOAT):
        case ZR_INSTRUCTION_ENUM(TO_INT_UNSIGNED):
            *outOpcode = opcode;
            *outDestinationSlot = instruction->instruction.operandExtra;
            *outSourceSlot = instruction->instruction.operand.operand1[0];
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_c_scalar_conversion_decode_to_f64(const TZrInstruction *instruction,
                                                             TZrUInt32 *outDestinationSlot,
                                                             TZrUInt32 *outSourceSlot,
                                                             EZrInstructionCode *outOpcode) {
    EZrInstructionCode opcode;

    if (instruction == ZR_NULL ||
        outDestinationSlot == ZR_NULL ||
        outSourceSlot == ZR_NULL ||
        outOpcode == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(TO_FLOAT):
        case ZR_INSTRUCTION_ENUM(TO_FLOAT_SIGNED):
        case ZR_INSTRUCTION_ENUM(TO_FLOAT_UNSIGNED):
            *outOpcode = opcode;
            *outDestinationSlot = instruction->instruction.operandExtra;
            *outSourceSlot = instruction->instruction.operand.operand1[0];
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static void backend_aot_c_scalar_conversion_write_plain_i64_result(FILE *file, const char *resultExpression) {
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

static void backend_aot_c_scalar_conversion_write_plain_u64_result(FILE *file, const char *resultExpression) {
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

static void backend_aot_c_scalar_conversion_write_plain_f64_result(FILE *file, const char *resultExpression) {
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

static void backend_aot_write_c_scalar_to_i64(FILE *file,
                                              const SZrAotExecIrFunction *functionIr,
                                              EZrInstructionCode opcode,
                                              TZrUInt32 destinationSlot,
                                              TZrUInt32 sourceSlot,
                                              TZrUInt32 execInstructionIndex) {
    TZrBool useF64Source =
            opcode == ZR_INSTRUCTION_ENUM(TO_INT_FLOAT) &&
            backend_aot_c_scalar_locals_has_f64_slot(functionIr, sourceSlot);
    TZrBool useU64Source =
            opcode == ZR_INSTRUCTION_ENUM(TO_INT_UNSIGNED) &&
            backend_aot_c_scalar_locals_has_u64_slot(functionIr, sourceSlot);
    TZrBool useI64Destination = backend_aot_c_scalar_locals_has_i64_slot(functionIr, destinationSlot);
    TZrBool useWrittenF64Source =
            useF64Source &&
            backend_aot_c_scalar_locals_f64_written_before(functionIr, sourceSlot, execInstructionIndex);
    TZrBool useWrittenU64Source =
            useU64Source &&
            backend_aot_c_scalar_locals_u64_written_before(functionIr, sourceSlot, execInstructionIndex);
    TZrBool useWrittenScalarSource =
            (TZrBool)((opcode == ZR_INSTRUCTION_ENUM(TO_INT_FLOAT) && useWrittenF64Source) ||
                      (opcode == ZR_INSTRUCTION_ENUM(TO_INT_UNSIGNED) && useWrittenU64Source));
    TZrBool canSkipValueSlot =
            (TZrBool)(useI64Destination &&
                      useWrittenScalarSource &&
                      backend_aot_c_scalar_locals_i64_result_can_skip_value_slot(
                              functionIr, destinationSlot, execInstructionIndex));

    if (canSkipValueSlot) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_scalar_exec_to_i64 opcode=%u dstSlot=%u srcSlot=%u */\n",
                (unsigned)opcode,
                (unsigned)destinationSlot,
                (unsigned)sourceSlot);
        if (opcode == ZR_INSTRUCTION_ENUM(TO_INT_FLOAT)) {
            fprintf(file,
                    "        zr_aot_s%u = (TZrInt64)zr_aot_f%u;\n",
                    (unsigned)destinationSlot,
                    (unsigned)sourceSlot);
        } else {
            fprintf(file,
                    "        {\n"
                    "            TZrUInt64 zr_aot_limit = (TZrUInt64)ZR_TYPE_RANGE_INT64_MAX + (TZrUInt64)1u;\n"
                    "            if (zr_aot_u%u >= zr_aot_limit) {\n"
                    "                zr_aot_s%u = ZR_TYPE_RANGE_INT64_MIN + (TZrInt64)(zr_aot_u%u - zr_aot_limit);\n"
                    "            } else {\n"
                    "                zr_aot_s%u = (TZrInt64)zr_aot_u%u;\n"
                    "            }\n"
                    "        }\n",
                    (unsigned)sourceSlot,
                    (unsigned)destinationSlot,
                    (unsigned)sourceSlot,
                    (unsigned)destinationSlot,
                    (unsigned)sourceSlot);
        }
        fprintf(file, "    }\n");
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_scalar_exec_to_i64 opcode=%u dstSlot=%u srcSlot=%u */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;\n"
            "        const SZrTypeValue *zr_aot_source = ZR_NULL;\n"
            "        TZrInt64 zr_aot_s_result = 0;\n"
            "        if (frame.slotBase == ZR_NULL || %u >= frame.generatedFrameSlotCount ||\n"
            "            %u >= frame.generatedFrameSlotCount) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_destination = &frame.slotBase[%u].value;\n",
            (unsigned)opcode,
            (unsigned)destinationSlot,
            (unsigned)sourceSlot,
            (unsigned)destinationSlot,
            (unsigned)sourceSlot,
            (unsigned)destinationSlot);

    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(TO_INT_FLOAT):
            if (!useWrittenF64Source) {
                fprintf(file,
                        "        zr_aot_source = &frame.slotBase[%u].value;\n"
                        "        if (!ZR_VALUE_IS_TYPE_FLOAT(zr_aot_source->type)) {\n"
                        "            ZR_AOT_C_FAIL();\n"
                        "        }\n",
                        (unsigned)sourceSlot);
            }
            if (useF64Source) {
                if (!useWrittenF64Source) {
                    fprintf(file,
                            "        zr_aot_f%u = zr_aot_source->value.nativeObject.nativeDouble;\n",
                            (unsigned)sourceSlot);
                }
                if (useI64Destination) {
                    fprintf(file,
                            "        zr_aot_s%u = (TZrInt64)zr_aot_f%u;\n"
                            "        zr_aot_s_result = zr_aot_s%u;\n",
                            (unsigned)destinationSlot,
                            (unsigned)sourceSlot,
                            (unsigned)destinationSlot);
                } else {
                    fprintf(file,
                            "        zr_aot_s_result = (TZrInt64)zr_aot_f%u;\n",
                            (unsigned)sourceSlot);
                }
            } else {
                fprintf(file,
                        "        zr_aot_s_result = (TZrInt64)zr_aot_source->value.nativeObject.nativeDouble;\n");
            }
            break;
        case ZR_INSTRUCTION_ENUM(TO_INT_UNSIGNED):
            if (!useWrittenU64Source) {
                fprintf(file,
                        "        zr_aot_source = &frame.slotBase[%u].value;\n"
                        "        if (!ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_source->type)) {\n"
                        "            ZR_AOT_C_FAIL();\n"
                        "        }\n",
                        (unsigned)sourceSlot);
            }
            if (useU64Source) {
                if (!useWrittenU64Source) {
                    fprintf(file,
                            "        zr_aot_u%u = zr_aot_source->value.nativeObject.nativeUInt64;\n",
                            (unsigned)sourceSlot);
                }
                fprintf(file,
                        "        {\n"
                        "            TZrUInt64 zr_aot_limit = (TZrUInt64)ZR_TYPE_RANGE_INT64_MAX + (TZrUInt64)1u;\n"
                        "            if (zr_aot_u%u >= zr_aot_limit) {\n",
                        (unsigned)sourceSlot);
                if (useI64Destination) {
                    fprintf(file,
                            "                zr_aot_s%u = ZR_TYPE_RANGE_INT64_MIN + (TZrInt64)(zr_aot_u%u - zr_aot_limit);\n"
                            "                zr_aot_s_result = zr_aot_s%u;\n"
                            "            } else {\n"
                            "                zr_aot_s%u = (TZrInt64)zr_aot_u%u;\n"
                            "                zr_aot_s_result = zr_aot_s%u;\n",
                            (unsigned)destinationSlot,
                            (unsigned)sourceSlot,
                            (unsigned)destinationSlot,
                            (unsigned)destinationSlot,
                            (unsigned)sourceSlot,
                            (unsigned)destinationSlot);
                } else {
                    fprintf(file,
                            "                zr_aot_s_result = ZR_TYPE_RANGE_INT64_MIN + (TZrInt64)(zr_aot_u%u - zr_aot_limit);\n"
                            "            } else {\n"
                            "                zr_aot_s_result = (TZrInt64)zr_aot_u%u;\n",
                            (unsigned)sourceSlot,
                            (unsigned)sourceSlot);
                }
                fprintf(file,
                        "            }\n"
                        "        }\n");
            } else {
                fprintf(file,
                        "        {\n"
                        "            TZrUInt64 zr_aot_u_source = zr_aot_source->value.nativeObject.nativeUInt64;\n"
                        "            TZrUInt64 zr_aot_limit = (TZrUInt64)ZR_TYPE_RANGE_INT64_MAX + (TZrUInt64)1u;\n"
                        "            if (zr_aot_u_source >= zr_aot_limit) {\n"
                        "                zr_aot_s_result = ZR_TYPE_RANGE_INT64_MIN + (TZrInt64)(zr_aot_u_source - zr_aot_limit);\n"
                        "            } else {\n"
                        "                zr_aot_s_result = (TZrInt64)zr_aot_u_source;\n"
                        "            }\n"
                        "        }\n");
            }
            break;
        case ZR_INSTRUCTION_ENUM(TO_INT):
        default:
            fprintf(file,
                    "        zr_aot_source = &frame.slotBase[%u].value;\n"
                    "        if (ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_source->type)) {\n"
                    "            zr_aot_s_result = zr_aot_source->value.nativeObject.nativeInt64;\n"
                    "        } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_source->type)) {\n"
                    "            TZrUInt64 zr_aot_u_source = zr_aot_source->value.nativeObject.nativeUInt64;\n"
                    "            TZrUInt64 zr_aot_limit = (TZrUInt64)ZR_TYPE_RANGE_INT64_MAX + (TZrUInt64)1u;\n"
                    "            if (zr_aot_u_source >= zr_aot_limit) {\n"
                    "                zr_aot_s_result = ZR_TYPE_RANGE_INT64_MIN + (TZrInt64)(zr_aot_u_source - zr_aot_limit);\n"
                    "            } else {\n"
                    "                zr_aot_s_result = (TZrInt64)zr_aot_u_source;\n"
                    "            }\n"
                    "        } else if (ZR_VALUE_IS_TYPE_FLOAT(zr_aot_source->type)) {\n"
                    "            zr_aot_s_result = (TZrInt64)zr_aot_source->value.nativeObject.nativeDouble;\n"
                    "        } else if (ZR_VALUE_IS_TYPE_BOOL(zr_aot_source->type)) {\n"
                    "            zr_aot_s_result = zr_aot_source->value.nativeObject.nativeBool ? 1 : 0;\n"
                    "        } else {\n"
                    "            ZrCore_Debug_RunError(state, \"unsupported AOT int conversion\");\n"
                    "            ZR_AOT_C_FAIL();\n"
                    "        }\n",
                    (unsigned)sourceSlot);
            break;
    }

    if (useI64Destination) {
        fprintf(file,
                "        zr_aot_s%u = zr_aot_s_result;\n",
                (unsigned)destinationSlot);
    }
    backend_aot_c_scalar_conversion_write_plain_i64_result(file, "zr_aot_s_result");
    fprintf(file, "    }\n");
}

static void backend_aot_write_c_scalar_to_u64(FILE *file,
                                              const SZrAotExecIrFunction *functionIr,
                                              EZrInstructionCode opcode,
                                              TZrUInt32 destinationSlot,
                                              TZrUInt32 sourceSlot,
                                              TZrUInt32 execInstructionIndex) {
    TZrBool useU64Source =
            opcode == ZR_INSTRUCTION_ENUM(TO_UINT) &&
            backend_aot_c_scalar_locals_has_u64_slot(functionIr, sourceSlot);
    TZrBool useI64Source =
            (opcode == ZR_INSTRUCTION_ENUM(TO_UINT) || opcode == ZR_INSTRUCTION_ENUM(TO_UINT_SIGNED)) &&
            backend_aot_c_scalar_locals_has_i64_slot(functionIr, sourceSlot);
    TZrBool useF64Source =
            opcode == ZR_INSTRUCTION_ENUM(TO_UINT_FLOAT) &&
            backend_aot_c_scalar_locals_has_f64_slot(functionIr, sourceSlot);
    TZrBool useU64Destination = backend_aot_c_scalar_locals_has_u64_slot(functionIr, destinationSlot);
    TZrBool useWrittenI64Source =
            useI64Source &&
            backend_aot_c_scalar_locals_i64_written_before(functionIr, sourceSlot, execInstructionIndex);
    TZrBool useWrittenU64Source =
            useU64Source &&
            backend_aot_c_scalar_locals_u64_written_before(functionIr, sourceSlot, execInstructionIndex);
    TZrBool useWrittenF64Source =
            useF64Source &&
            backend_aot_c_scalar_locals_f64_written_before(functionIr, sourceSlot, execInstructionIndex);
    TZrBool useWrittenScalarSource =
            (TZrBool)((opcode == ZR_INSTRUCTION_ENUM(TO_UINT_SIGNED) && useWrittenI64Source) ||
                      (opcode == ZR_INSTRUCTION_ENUM(TO_UINT_FLOAT) && useWrittenF64Source));
    TZrBool canSkipValueSlot =
            (TZrBool)(useU64Destination &&
                      useWrittenScalarSource &&
                      backend_aot_c_scalar_locals_u64_result_can_skip_value_slot(
                              functionIr, destinationSlot, execInstructionIndex));

    if (canSkipValueSlot) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_scalar_exec_to_u64 opcode=%u dstSlot=%u srcSlot=%u */\n",
                (unsigned)opcode,
                (unsigned)destinationSlot,
                (unsigned)sourceSlot);
        if (opcode == ZR_INSTRUCTION_ENUM(TO_UINT_SIGNED)) {
            fprintf(file,
                    "        zr_aot_u%u = (TZrUInt64)zr_aot_s%u;\n",
                    (unsigned)destinationSlot,
                    (unsigned)sourceSlot);
        } else {
            fprintf(file,
                    "        zr_aot_u%u = (TZrUInt64)zr_aot_f%u;\n",
                    (unsigned)destinationSlot,
                    (unsigned)sourceSlot);
        }
        fprintf(file, "    }\n");
        return;
    }

    if (opcode == ZR_INSTRUCTION_ENUM(TO_UINT) && useWrittenI64Source && useU64Destination) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_scalar_exec_to_u64 opcode=%u dstSlot=%u srcSlot=%u */\n"
                "        zr_aot_u%u = (TZrUInt64)zr_aot_s%u;\n"
                "    }\n",
                (unsigned)opcode,
                (unsigned)destinationSlot,
                (unsigned)sourceSlot,
                (unsigned)destinationSlot,
                (unsigned)sourceSlot);
        return;
    }

    if (opcode == ZR_INSTRUCTION_ENUM(TO_UINT) && useWrittenU64Source && useU64Destination) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_scalar_exec_to_u64 opcode=%u dstSlot=%u srcSlot=%u */\n"
                "        zr_aot_u%u = zr_aot_u%u;\n"
                "    }\n",
                (unsigned)opcode,
                (unsigned)destinationSlot,
                (unsigned)sourceSlot,
                (unsigned)destinationSlot,
                (unsigned)sourceSlot);
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_scalar_exec_to_u64 opcode=%u dstSlot=%u srcSlot=%u */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;\n"
            "        const SZrTypeValue *zr_aot_source = ZR_NULL;\n"
            "        TZrUInt64 zr_aot_u_result = 0u;\n"
            "        if (frame.slotBase == ZR_NULL || %u >= frame.generatedFrameSlotCount ||\n"
            "            %u >= frame.generatedFrameSlotCount) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_destination = &frame.slotBase[%u].value;\n",
            (unsigned)opcode,
            (unsigned)destinationSlot,
            (unsigned)sourceSlot,
            (unsigned)destinationSlot,
            (unsigned)sourceSlot,
            (unsigned)destinationSlot);

    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(TO_UINT_SIGNED):
            if (!useWrittenI64Source) {
                fprintf(file,
                        "        zr_aot_source = &frame.slotBase[%u].value;\n"
                        "        if (!ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_source->type)) {\n"
                        "            ZR_AOT_C_FAIL();\n"
                        "        }\n",
                        (unsigned)sourceSlot);
            }
            if (useI64Source) {
                if (!useWrittenI64Source) {
                    fprintf(file,
                            "        zr_aot_s%u = zr_aot_source->value.nativeObject.nativeInt64;\n",
                            (unsigned)sourceSlot);
                }
                if (useU64Destination) {
                    fprintf(file,
                            "        zr_aot_u%u = (TZrUInt64)zr_aot_s%u;\n"
                            "        zr_aot_u_result = zr_aot_u%u;\n",
                            (unsigned)destinationSlot,
                            (unsigned)sourceSlot,
                            (unsigned)destinationSlot);
                } else {
                    fprintf(file,
                            "        zr_aot_u_result = (TZrUInt64)zr_aot_s%u;\n",
                            (unsigned)sourceSlot);
                }
            } else {
                fprintf(file,
                        "        zr_aot_u_result = (TZrUInt64)zr_aot_source->value.nativeObject.nativeInt64;\n");
            }
            break;
        case ZR_INSTRUCTION_ENUM(TO_UINT_FLOAT):
            if (!useWrittenF64Source) {
                fprintf(file,
                        "        zr_aot_source = &frame.slotBase[%u].value;\n"
                        "        if (!ZR_VALUE_IS_TYPE_FLOAT(zr_aot_source->type)) {\n"
                        "            ZR_AOT_C_FAIL();\n"
                        "        }\n",
                        (unsigned)sourceSlot);
            }
            if (useF64Source) {
                if (!useWrittenF64Source) {
                    fprintf(file,
                            "        zr_aot_f%u = zr_aot_source->value.nativeObject.nativeDouble;\n",
                            (unsigned)sourceSlot);
                }
                if (useU64Destination) {
                    fprintf(file,
                            "        zr_aot_u%u = (TZrUInt64)zr_aot_f%u;\n"
                            "        zr_aot_u_result = zr_aot_u%u;\n",
                            (unsigned)destinationSlot,
                            (unsigned)sourceSlot,
                            (unsigned)destinationSlot);
                } else {
                    fprintf(file,
                            "        zr_aot_u_result = (TZrUInt64)zr_aot_f%u;\n",
                            (unsigned)sourceSlot);
                }
            } else {
                fprintf(file,
                        "        zr_aot_u_result = (TZrUInt64)zr_aot_source->value.nativeObject.nativeDouble;\n");
            }
            break;
        case ZR_INSTRUCTION_ENUM(TO_UINT):
        default:
            fprintf(file,
                    "        zr_aot_source = &frame.slotBase[%u].value;\n"
                    "        if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_source->type)) {\n",
                    (unsigned)sourceSlot);
            if (useU64Source) {
                fprintf(file,
                        "            zr_aot_u%u = zr_aot_source->value.nativeObject.nativeUInt64;\n",
                        (unsigned)sourceSlot);
                if (useU64Destination && destinationSlot != sourceSlot) {
                    fprintf(file,
                            "            zr_aot_u%u = zr_aot_u%u;\n"
                            "            zr_aot_u_result = zr_aot_u%u;\n",
                            (unsigned)destinationSlot,
                            (unsigned)sourceSlot,
                            (unsigned)destinationSlot);
                } else {
                    fprintf(file,
                            "            zr_aot_u_result = zr_aot_u%u;\n",
                            (unsigned)sourceSlot);
                }
            } else {
                fprintf(file,
                        "            zr_aot_u_result = zr_aot_source->value.nativeObject.nativeUInt64;\n");
            }
            fprintf(file,
                    "        } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_source->type)) {\n"
                    "            zr_aot_u_result = (TZrUInt64)zr_aot_source->value.nativeObject.nativeInt64;\n"
                    "        } else if (ZR_VALUE_IS_TYPE_FLOAT(zr_aot_source->type)) {\n"
                    "            zr_aot_u_result = (TZrUInt64)zr_aot_source->value.nativeObject.nativeDouble;\n"
                    "        } else if (ZR_VALUE_IS_TYPE_BOOL(zr_aot_source->type)) {\n"
                    "            zr_aot_u_result = zr_aot_source->value.nativeObject.nativeBool ? (TZrUInt64)1u : (TZrUInt64)0u;\n"
                    "        } else {\n"
                    "            ZrCore_Debug_RunError(state, \"unsupported AOT uint conversion\");\n"
                    "            ZR_AOT_C_FAIL();\n"
                    "        }\n");
            break;
    }

    if (useU64Destination) {
        fprintf(file,
                "        zr_aot_u%u = zr_aot_u_result;\n",
                (unsigned)destinationSlot);
    }
    backend_aot_c_scalar_conversion_write_plain_u64_result(file, "zr_aot_u_result");
    fprintf(file, "    }\n");
}

static void backend_aot_write_c_scalar_to_f64(FILE *file,
                                              const SZrAotExecIrFunction *functionIr,
                                              EZrInstructionCode opcode,
                                              TZrUInt32 destinationSlot,
                                              TZrUInt32 sourceSlot,
                                              TZrUInt32 execInstructionIndex) {
    TZrBool useI64Source =
            (opcode == ZR_INSTRUCTION_ENUM(TO_FLOAT) || opcode == ZR_INSTRUCTION_ENUM(TO_FLOAT_SIGNED)) &&
            backend_aot_c_scalar_locals_has_i64_slot(functionIr, sourceSlot);
    TZrBool useU64Source =
            (opcode == ZR_INSTRUCTION_ENUM(TO_FLOAT) || opcode == ZR_INSTRUCTION_ENUM(TO_FLOAT_UNSIGNED)) &&
            backend_aot_c_scalar_locals_has_u64_slot(functionIr, sourceSlot);
    TZrBool useF64Destination = backend_aot_c_scalar_locals_has_f64_slot(functionIr, destinationSlot);
    TZrBool useWrittenI64Source =
            useI64Source &&
            backend_aot_c_scalar_locals_i64_written_before(functionIr, sourceSlot, execInstructionIndex);
    TZrBool useWrittenU64Source =
            useU64Source &&
            backend_aot_c_scalar_locals_u64_written_before(functionIr, sourceSlot, execInstructionIndex);
    TZrBool useWrittenScalarSource =
            (TZrBool)((opcode == ZR_INSTRUCTION_ENUM(TO_FLOAT_SIGNED) && useWrittenI64Source) ||
                      (opcode == ZR_INSTRUCTION_ENUM(TO_FLOAT_UNSIGNED) && useWrittenU64Source));
    TZrBool canSkipValueSlot =
            (TZrBool)(useF64Destination &&
                      useWrittenScalarSource &&
                      backend_aot_c_scalar_locals_f64_result_can_skip_value_slot(
                              functionIr, destinationSlot, execInstructionIndex));

    if (canSkipValueSlot) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_scalar_exec_to_f64 opcode=%u dstSlot=%u srcSlot=%u */\n",
                (unsigned)opcode,
                (unsigned)destinationSlot,
                (unsigned)sourceSlot);
        if (opcode == ZR_INSTRUCTION_ENUM(TO_FLOAT_SIGNED)) {
            fprintf(file,
                    "        zr_aot_f%u = (TZrFloat64)zr_aot_s%u;\n",
                    (unsigned)destinationSlot,
                    (unsigned)sourceSlot);
        } else {
            fprintf(file,
                    "        zr_aot_f%u = (TZrFloat64)zr_aot_u%u;\n",
                    (unsigned)destinationSlot,
                    (unsigned)sourceSlot);
        }
        fprintf(file, "    }\n");
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_scalar_exec_to_f64 opcode=%u dstSlot=%u srcSlot=%u */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;\n"
            "        const SZrTypeValue *zr_aot_source = ZR_NULL;\n"
            "        TZrFloat64 zr_aot_f_result = 0.0;\n"
            "        if (frame.slotBase == ZR_NULL || %u >= frame.generatedFrameSlotCount ||\n"
            "            %u >= frame.generatedFrameSlotCount) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_destination = &frame.slotBase[%u].value;\n",
            (unsigned)opcode,
            (unsigned)destinationSlot,
            (unsigned)sourceSlot,
            (unsigned)destinationSlot,
            (unsigned)sourceSlot,
            (unsigned)destinationSlot);

    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(TO_FLOAT_SIGNED):
            if (!useWrittenI64Source) {
                fprintf(file,
                        "        zr_aot_source = &frame.slotBase[%u].value;\n"
                        "        if (!ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_source->type)) {\n"
                        "            ZR_AOT_C_FAIL();\n"
                        "        }\n",
                        (unsigned)sourceSlot);
            }
            if (useI64Source) {
                if (!useWrittenI64Source) {
                    fprintf(file,
                            "        zr_aot_s%u = zr_aot_source->value.nativeObject.nativeInt64;\n",
                            (unsigned)sourceSlot);
                }
                if (useF64Destination) {
                    fprintf(file,
                            "        zr_aot_f%u = (TZrFloat64)zr_aot_s%u;\n"
                            "        zr_aot_f_result = zr_aot_f%u;\n",
                            (unsigned)destinationSlot,
                            (unsigned)sourceSlot,
                            (unsigned)destinationSlot);
                } else {
                    fprintf(file,
                            "        zr_aot_f_result = (TZrFloat64)zr_aot_s%u;\n",
                            (unsigned)sourceSlot);
                }
            } else {
                fprintf(file,
                        "        zr_aot_f_result = (TZrFloat64)zr_aot_source->value.nativeObject.nativeInt64;\n");
            }
            break;
        case ZR_INSTRUCTION_ENUM(TO_FLOAT_UNSIGNED):
            if (!useWrittenU64Source) {
                fprintf(file,
                        "        zr_aot_source = &frame.slotBase[%u].value;\n"
                        "        if (!ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_source->type)) {\n"
                        "            ZR_AOT_C_FAIL();\n"
                        "        }\n",
                        (unsigned)sourceSlot);
            }
            if (useU64Source) {
                if (!useWrittenU64Source) {
                    fprintf(file,
                            "        zr_aot_u%u = zr_aot_source->value.nativeObject.nativeUInt64;\n",
                            (unsigned)sourceSlot);
                }
                if (useF64Destination) {
                    fprintf(file,
                            "        zr_aot_f%u = (TZrFloat64)zr_aot_u%u;\n"
                            "        zr_aot_f_result = zr_aot_f%u;\n",
                            (unsigned)destinationSlot,
                            (unsigned)sourceSlot,
                            (unsigned)destinationSlot);
                } else {
                    fprintf(file,
                            "        zr_aot_f_result = (TZrFloat64)zr_aot_u%u;\n",
                            (unsigned)sourceSlot);
                }
            } else {
                fprintf(file,
                        "        zr_aot_f_result = (TZrFloat64)zr_aot_source->value.nativeObject.nativeUInt64;\n");
            }
            break;
        case ZR_INSTRUCTION_ENUM(TO_FLOAT):
        default:
            fprintf(file,
                    "        zr_aot_source = &frame.slotBase[%u].value;\n"
                    "        if (ZR_VALUE_IS_TYPE_FLOAT(zr_aot_source->type)) {\n"
                    "            zr_aot_f_result = zr_aot_source->value.nativeObject.nativeDouble;\n"
                    "        } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_source->type)) {\n",
                    (unsigned)sourceSlot);
            if (useI64Source) {
                fprintf(file,
                        "            zr_aot_s%u = zr_aot_source->value.nativeObject.nativeInt64;\n"
                        "            zr_aot_f_result = (TZrFloat64)zr_aot_s%u;\n",
                        (unsigned)sourceSlot,
                        (unsigned)sourceSlot);
            } else {
                fprintf(file,
                        "            zr_aot_f_result = (TZrFloat64)zr_aot_source->value.nativeObject.nativeInt64;\n");
            }
            fprintf(file,
                    "        } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_source->type)) {\n");
            if (useU64Source) {
                fprintf(file,
                        "            zr_aot_u%u = zr_aot_source->value.nativeObject.nativeUInt64;\n"
                        "            zr_aot_f_result = (TZrFloat64)zr_aot_u%u;\n",
                        (unsigned)sourceSlot,
                        (unsigned)sourceSlot);
            } else {
                fprintf(file,
                        "            zr_aot_f_result = (TZrFloat64)zr_aot_source->value.nativeObject.nativeUInt64;\n");
            }
            fprintf(file,
                    "        } else if (ZR_VALUE_IS_TYPE_BOOL(zr_aot_source->type)) {\n"
                    "            zr_aot_f_result = zr_aot_source->value.nativeObject.nativeBool ? 1.0 : 0.0;\n"
                    "        } else {\n"
                    "            ZrCore_Debug_RunError(state, \"unsupported AOT float conversion\");\n"
                    "            ZR_AOT_C_FAIL();\n"
                    "        }\n");
            break;
    }

    if (useF64Destination) {
        fprintf(file,
                "        zr_aot_f%u = zr_aot_f_result;\n",
                (unsigned)destinationSlot);
    }
    backend_aot_c_scalar_conversion_write_plain_f64_result(file, "zr_aot_f_result");
    fprintf(file, "    }\n");
}

TZrBool backend_aot_try_write_c_scalar_conversion(FILE *file,
                                                  const SZrAotExecIrFunction *functionIr,
                                                  const TZrInstruction *execInstruction,
                                                  TZrUInt32 execInstructionIndex) {
    TZrUInt32 destinationSlot;
    TZrUInt32 sourceSlot;
    EZrInstructionCode opcode;

    if (file == ZR_NULL || execInstruction == ZR_NULL) {
        return ZR_FALSE;
    }

    if (backend_aot_c_scalar_conversion_decode_to_i64(execInstruction, &destinationSlot, &sourceSlot, &opcode)) {
        backend_aot_write_c_scalar_to_i64(file, functionIr, opcode, destinationSlot, sourceSlot, execInstructionIndex);
        return ZR_TRUE;
    }

    if (backend_aot_c_scalar_conversion_decode_to_u64(execInstruction, &destinationSlot, &sourceSlot, &opcode)) {
        backend_aot_write_c_scalar_to_u64(file, functionIr, opcode, destinationSlot, sourceSlot, execInstructionIndex);
        return ZR_TRUE;
    }

    if (backend_aot_c_scalar_conversion_decode_to_f64(execInstruction, &destinationSlot, &sourceSlot, &opcode)) {
        backend_aot_write_c_scalar_to_f64(file, functionIr, opcode, destinationSlot, sourceSlot, execInstructionIndex);
        return ZR_TRUE;
    }

    return ZR_FALSE;
}
