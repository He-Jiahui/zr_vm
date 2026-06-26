#include "backend_aot_c_emitter.h"

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
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SuperArrayGetInt(state, &frame, %u, %u, %u));\n"
            "    } while (0);\n",
            (unsigned)destinationSlot,
            (unsigned)receiverSlot,
            (unsigned)keySlot);
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
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SuperArraySetInt(state, &frame, %u, %u, %u));\n"
            "    } while (0);\n",
            (unsigned)sourceSlot,
            (unsigned)receiverSlot,
            (unsigned)keySlot);
}

void backend_aot_write_c_direct_super_array_set_int_new_owner_no_write_barrier(FILE *file,
                                                                               TZrUInt32 sourceSlot,
                                                                               TZrUInt32 receiverSlot,
                                                                               TZrUInt32 keySlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_super_array_set_int_new_owner_no_write_barrier */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SuperArraySetIntNewOwnerNoWriteBarrier(state, &frame, %u, %u, %u));\n"
            "    } while (0);\n",
            (unsigned)sourceSlot,
            (unsigned)receiverSlot,
            (unsigned)keySlot);
}

void backend_aot_write_c_direct_super_array_add_int(FILE *file,
                                                    TZrUInt32 destinationSlot,
                                                    TZrUInt32 receiverSlot,
                                                    TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_super_array_add_int */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SuperArrayAddInt(state, &frame, %u, %u, %u));\n"
            "    } while (0);\n",
            (unsigned)destinationSlot,
            (unsigned)receiverSlot,
            (unsigned)sourceSlot);
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
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SuperArrayAddInt4(state, &frame, %u, %u));\n"
            "    } while (0);\n",
            (unsigned)receiverBaseSlot,
            (unsigned)sourceSlot);
}

void backend_aot_write_c_direct_super_array_add_int4_const(FILE *file,
                                                           const SZrFunction *function,
                                                           TZrUInt32 receiverBaseSlot,
                                                           TZrUInt32 constantIndex) {
    ZR_UNUSED_PARAMETER(function);

    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_super_array_add_int4 */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SuperArrayAddInt4Const(state, &frame, %u, %u));\n"
            "    } while (0);\n",
            (unsigned)receiverBaseSlot,
            (unsigned)constantIndex);
}

void backend_aot_write_c_direct_super_array_fill_int4_const(FILE *file,
                                                            const SZrFunction *function,
                                                            TZrUInt32 receiverBaseSlot,
                                                            TZrUInt32 countSlot,
                                                            TZrUInt32 constantIndex) {
    ZR_UNUSED_PARAMETER(function);

    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_super_array_fill_int4_const */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SuperArrayFillInt4Const(state, &frame, %u, %u, %u));\n"
            "    } while (0);\n",
            (unsigned)receiverBaseSlot,
            (unsigned)countSlot,
            (unsigned)constantIndex);
}
