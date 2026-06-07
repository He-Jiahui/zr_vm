#include "backend_aot_c_emitter.h"

#include "zr_vm_common/zr_instruction_conf.h"

#include <inttypes.h>

static void backend_aot_c_write_super_array_unsupported(FILE *file) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "            ZrCore_Debug_RunError(state, \"unsupported AOT super-array integer fast path\");\n"
            "            ZR_AOT_C_FAIL();\n");
}

static TZrBool backend_aot_c_get_signed_int_constant(const SZrFunction *function,
                                                     TZrUInt32 constantIndex,
                                                     TZrInt64 *outValue) {
    const SZrTypeValue *constantValue;

    if (outValue != ZR_NULL) {
        *outValue = 0;
    }
    constantValue = backend_aot_c_get_constant_value(function, (TZrInt32)constantIndex);
    if (constantValue == ZR_NULL || outValue == ZR_NULL || !ZR_VALUE_IS_TYPE_SIGNED_INT(constantValue->type)) {
        return ZR_FALSE;
    }

    *outValue = constantValue->value.nativeObject.nativeInt64;
    return ZR_TRUE;
}

static void backend_aot_c_write_super_array_invalid_constant(FILE *file) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        ZrCore_Debug_RunError(state, \"unsupported AOT super-array integer fast path\");\n"
            "        ZR_AOT_C_FAIL();\n"
            "    } while (0);\n");
}

void backend_aot_write_c_direct_super_array_get_int(FILE *file,
                                                    TZrUInt32 destinationSlot,
                                                    TZrUInt32 receiverSlot,
                                                    TZrUInt32 keySlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_super_array_get_int */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;\n"
            "        SZrTypeValue *zr_aot_receiver = ZR_NULL;\n"
            "        SZrTypeValue *zr_aot_key = ZR_NULL;\n"
            "        TZrBool zr_aot_applicable = ZR_FALSE;\n"
            "        if (state == ZR_NULL || frame.function == ZR_NULL || frame.slotBase == ZR_NULL ||\n"
            "            %u >= frame.generatedFrameSlotCount || %u >= frame.generatedFrameSlotCount ||\n"
            "            %u >= frame.generatedFrameSlotCount) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        zr_aot_receiver = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        zr_aot_key = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_receiver == ZR_NULL || zr_aot_key == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (!ZrCore_Object_SuperArrayTryGetIntFast(state, zr_aot_receiver, zr_aot_key, zr_aot_destination, &zr_aot_applicable) ||\n"
            "            !zr_aot_applicable) {\n",
            (unsigned)destinationSlot,
            (unsigned)receiverSlot,
            (unsigned)keySlot,
            (unsigned)destinationSlot,
            (unsigned)receiverSlot,
            (unsigned)keySlot);
    backend_aot_c_write_super_array_unsupported(file);
    fprintf(file,
            "        }\n"
            "    } while (0);\n");
}

void backend_aot_write_c_direct_super_array_set_int(FILE *file,
                                                    TZrUInt32 sourceSlot,
                                                    TZrUInt32 receiverSlot,
                                                    TZrUInt32 keySlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_super_array_set_int */\n"
            "        SZrTypeValue *zr_aot_source = ZR_NULL;\n"
            "        SZrTypeValue *zr_aot_receiver = ZR_NULL;\n"
            "        SZrTypeValue *zr_aot_key = ZR_NULL;\n"
            "        TZrBool zr_aot_applicable = ZR_FALSE;\n"
            "        if (state == ZR_NULL || frame.function == ZR_NULL || frame.slotBase == ZR_NULL ||\n"
            "            %u >= frame.generatedFrameSlotCount || %u >= frame.generatedFrameSlotCount ||\n"
            "            %u >= frame.generatedFrameSlotCount) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_source = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        zr_aot_receiver = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        zr_aot_key = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        if (zr_aot_source == ZR_NULL || zr_aot_receiver == ZR_NULL || zr_aot_key == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (!ZrCore_Object_SuperArrayTrySetIntFast(state, zr_aot_receiver, zr_aot_key, zr_aot_source, &zr_aot_applicable) ||\n"
            "            !zr_aot_applicable) {\n",
            (unsigned)sourceSlot,
            (unsigned)receiverSlot,
            (unsigned)keySlot,
            (unsigned)sourceSlot,
            (unsigned)receiverSlot,
            (unsigned)keySlot);
    backend_aot_c_write_super_array_unsupported(file);
    fprintf(file,
            "        }\n"
            "    } while (0);\n");
}

void backend_aot_write_c_direct_super_array_add_int(FILE *file,
                                                    TZrUInt32 destinationSlot,
                                                    TZrUInt32 receiverSlot,
                                                    TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    if (destinationSlot == (TZrUInt32)ZR_INSTRUCTION_USE_RET_FLAG) {
        fprintf(file,
                "    do {\n"
                "        /* zr_aot_value_exec_super_array_add_int */\n"
                "        SZrTypeValue *zr_aot_receiver = ZR_NULL;\n"
                "        SZrTypeValue *zr_aot_source = ZR_NULL;\n"
                "        if (state == ZR_NULL || frame.function == ZR_NULL || frame.slotBase == ZR_NULL ||\n"
                "            %u >= frame.generatedFrameSlotCount || %u >= frame.generatedFrameSlotCount) {\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n"
                "        zr_aot_receiver = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
                "        zr_aot_source = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
                "        if (zr_aot_receiver == ZR_NULL || zr_aot_source == ZR_NULL ||\n"
                "            !ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_source->type) ||\n"
                "            !ZrCore_Object_SuperArrayAddIntDiscardResultAssumeFast(state, zr_aot_receiver, zr_aot_source)) {\n",
                (unsigned)receiverSlot,
                (unsigned)sourceSlot,
                (unsigned)receiverSlot,
                (unsigned)sourceSlot);
        backend_aot_c_write_super_array_unsupported(file);
        fprintf(file,
                "        }\n"
                "    } while (0);\n");
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_super_array_add_int */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;\n"
            "        SZrTypeValue *zr_aot_receiver = ZR_NULL;\n"
            "        SZrTypeValue *zr_aot_source = ZR_NULL;\n"
            "        if (state == ZR_NULL || frame.function == ZR_NULL || frame.slotBase == ZR_NULL ||\n"
            "            %u >= frame.generatedFrameSlotCount || %u >= frame.generatedFrameSlotCount ||\n"
            "            %u >= frame.generatedFrameSlotCount) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        zr_aot_receiver = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        zr_aot_source = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_receiver == ZR_NULL || zr_aot_source == ZR_NULL ||\n"
            "            !ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_source->type) ||\n"
            "            !ZrCore_Object_SuperArrayAddIntAssumeFast(state, zr_aot_receiver, zr_aot_source, zr_aot_destination)) {\n",
            (unsigned)destinationSlot,
            (unsigned)receiverSlot,
            (unsigned)sourceSlot,
            (unsigned)destinationSlot,
            (unsigned)receiverSlot,
            (unsigned)sourceSlot);
    backend_aot_c_write_super_array_unsupported(file);
    fprintf(file,
            "        }\n"
            "    } while (0);\n");
}

void backend_aot_write_c_direct_super_array_add_int4(FILE *file,
                                                     TZrUInt32 receiverBaseSlot,
                                                     TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_super_array_add_int4 */\n"
            "        TZrUInt32 zr_aot_index;\n"
            "        SZrTypeValue *zr_aot_source = ZR_NULL;\n"
            "        if (state == ZR_NULL || frame.function == ZR_NULL || frame.slotBase == ZR_NULL ||\n"
            "            %u > frame.generatedFrameSlotCount || frame.generatedFrameSlotCount - %u < 4u ||\n"
            "            %u >= frame.generatedFrameSlotCount) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_source = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        if (zr_aot_source == ZR_NULL || !ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_source->type)) {\n",
            (unsigned)receiverBaseSlot,
            (unsigned)receiverBaseSlot,
            (unsigned)sourceSlot,
            (unsigned)sourceSlot);
    backend_aot_c_write_super_array_unsupported(file);
    fprintf(file,
            "        }\n"
            "        for (zr_aot_index = 0; zr_aot_index < 4u; zr_aot_index++) {\n"
            "            SZrTypeValue *zr_aot_receiver = ZrCore_Stack_GetValue(frame.slotBase + %u + zr_aot_index);\n"
            "            if (zr_aot_receiver == ZR_NULL ||\n"
            "                !ZrCore_Object_SuperArrayAddIntDiscardResultAssumeFast(state, zr_aot_receiver, zr_aot_source)) {\n",
            (unsigned)receiverBaseSlot);
    backend_aot_c_write_super_array_unsupported(file);
    fprintf(file,
            "            }\n"
            "        }\n"
            "    } while (0);\n");
}

void backend_aot_write_c_direct_super_array_add_int4_const(FILE *file,
                                                           const SZrFunction *function,
                                                           TZrUInt32 receiverBaseSlot,
                                                           TZrUInt32 constantIndex) {
    TZrInt64 constantValue;

    if (file == ZR_NULL) {
        return;
    }
    if (!backend_aot_c_get_signed_int_constant(function, constantIndex, &constantValue)) {
        backend_aot_c_write_super_array_invalid_constant(file);
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_super_array_add_int4 */\n"
            "        TZrStackValuePointer zr_aot_receiver_base = ZR_NULL;\n"
            "        if (state == ZR_NULL || frame.function == ZR_NULL || frame.slotBase == ZR_NULL ||\n"
            "            %u > frame.generatedFrameSlotCount || frame.generatedFrameSlotCount - %u < 4u) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_receiver_base = frame.slotBase + %u;\n"
            "        if (!ZrCore_Object_SuperArrayAddInt4ConstAssumeFast(state, zr_aot_receiver_base, (TZrInt64)%" PRId64 ")) {\n",
            (unsigned)receiverBaseSlot,
            (unsigned)receiverBaseSlot,
            (unsigned)receiverBaseSlot,
            (int64_t)constantValue);
    backend_aot_c_write_super_array_unsupported(file);
    fprintf(file,
            "        }\n"
            "    } while (0);\n");
}

void backend_aot_write_c_direct_super_array_fill_int4_const(FILE *file,
                                                            const SZrFunction *function,
                                                            TZrUInt32 receiverBaseSlot,
                                                            TZrUInt32 countSlot,
                                                            TZrUInt32 constantIndex) {
    TZrInt64 constantValue;

    if (file == ZR_NULL) {
        return;
    }
    if (!backend_aot_c_get_signed_int_constant(function, constantIndex, &constantValue)) {
        backend_aot_c_write_super_array_invalid_constant(file);
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_super_array_fill_int4_const */\n"
            "        TZrStackValuePointer zr_aot_receiver_base = ZR_NULL;\n"
            "        SZrTypeValue *zr_aot_count = ZR_NULL;\n"
            "        if (state == ZR_NULL || frame.function == ZR_NULL || frame.slotBase == ZR_NULL ||\n"
            "            %u > frame.generatedFrameSlotCount || frame.generatedFrameSlotCount - %u < 4u ||\n"
            "            %u >= frame.generatedFrameSlotCount) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_receiver_base = frame.slotBase + %u;\n"
            "        zr_aot_count = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        if (zr_aot_count == ZR_NULL || !ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_count->type) ||\n"
            "            !ZrCore_Object_SuperArrayFillInt4ConstAssumeFast(state, zr_aot_receiver_base, zr_aot_count->value.nativeObject.nativeInt64, (TZrInt64)%" PRId64 ")) {\n",
            (unsigned)receiverBaseSlot,
            (unsigned)receiverBaseSlot,
            (unsigned)countSlot,
            (unsigned)receiverBaseSlot,
            (unsigned)countSlot,
            (int64_t)constantValue);
    backend_aot_c_write_super_array_unsupported(file);
    fprintf(file,
            "        }\n"
            "    } while (0);\n");
}
