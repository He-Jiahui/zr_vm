#include "backend_aot_c_emitter.h"
#include "backend_aot_internal.h"

void backend_aot_write_c_unsupported_meta_call(FILE *file,
                                               TZrUInt32 destinationSlot,
                                               TZrUInt32 receiverSlot,
                                               TZrUInt32 argumentCount) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_unsupported_meta_call */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_UnsupportedMetaCall(state,\n"
            "                                                                    &frame,\n"
            "                                                                    %u,\n"
            "                                                                    %u,\n"
            "                                                                    %u,\n"
            "                                                                    \"unsupported AOT meta call\"));\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)receiverSlot,
            (unsigned)argumentCount);
}

void backend_aot_write_c_static_direct_i64_no_arg_function_call(FILE *file,
                                                                TZrUInt32 destinationSlot,
                                                                TZrUInt32 calleeFlatIndex,
                                                                TZrBool syncStackSlot) {
    if (file == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_static_i64_no_arg_direct_call */\n");
    if (syncStackSlot) {
        fprintf(file,
            "        SZrTypeValue *zr_aot_typed_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        if (zr_aot_typed_destination == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n",
            (unsigned)destinationSlot);
    }
    fprintf(file,
            "        zr_aot_s%u = zr_aot_typed_i64_fn_%u();\n",
            (unsigned)destinationSlot,
            (unsigned)calleeFlatIndex);
    if (syncStackSlot) {
        fprintf(file,
            "        /* zr_aot_static_i64_no_arg_direct_call_sync_stack_slot */\n"
            "        ZR_VALUE_FAST_SET(zr_aot_typed_destination,\n"
            "                          nativeInt64,\n"
            "                          zr_aot_s%u,\n"
            "                          ZR_VALUE_TYPE_INT64);\n",
            (unsigned)destinationSlot);
    }
    fprintf(file, "    }\n");
}

void backend_aot_write_c_static_direct_i64_one_arg_function_call(FILE *file,
                                                                 TZrUInt32 destinationSlot,
                                                                 TZrUInt32 calleeFlatIndex,
                                                                 TZrUInt32 argumentSlot,
                                                                 TZrBool syncStackSlot) {
    if (file == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_static_i64_one_arg_direct_call */\n");
    if (syncStackSlot) {
        fprintf(file,
            "        SZrTypeValue *zr_aot_typed_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        if (zr_aot_typed_destination == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n",
            (unsigned)destinationSlot);
    }
    fprintf(file,
            "        zr_aot_s%u = zr_aot_typed_i64_fn_%u(zr_aot_s%u);\n",
            (unsigned)destinationSlot,
            (unsigned)calleeFlatIndex,
            (unsigned)argumentSlot);
    if (syncStackSlot) {
        fprintf(file,
            "        /* zr_aot_static_i64_one_arg_direct_call_sync_stack_slot */\n"
            "        ZR_VALUE_FAST_SET(zr_aot_typed_destination,\n"
            "                          nativeInt64,\n"
            "                          zr_aot_s%u,\n"
            "                          ZR_VALUE_TYPE_INT64);\n",
            (unsigned)destinationSlot);
    }
    fprintf(file, "    }\n");
}

void backend_aot_write_c_static_direct_i64_two_arg_function_call(FILE *file,
                                                                 TZrUInt32 destinationSlot,
                                                                 TZrUInt32 calleeFlatIndex,
                                                                 TZrUInt32 firstArgumentSlot,
                                                                 TZrUInt32 secondArgumentSlot,
                                                                 TZrBool syncStackSlot,
                                                                 TZrBool passStateToThunk) {
    if (file == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_static_i64_two_arg_direct_call */\n");
    if (syncStackSlot) {
        fprintf(file,
            "        SZrTypeValue *zr_aot_typed_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        if (zr_aot_typed_destination == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n",
            (unsigned)destinationSlot);
    }
    if (passStateToThunk) {
        fprintf(file,
                "        zr_aot_s%u = zr_aot_typed_i64_fn_%u(state, zr_aot_s%u, zr_aot_s%u);\n",
                (unsigned)destinationSlot,
                (unsigned)calleeFlatIndex,
                (unsigned)firstArgumentSlot,
                (unsigned)secondArgumentSlot);
    } else {
        fprintf(file,
                "        zr_aot_s%u = zr_aot_typed_i64_fn_%u(zr_aot_s%u, zr_aot_s%u);\n",
                (unsigned)destinationSlot,
                (unsigned)calleeFlatIndex,
                (unsigned)firstArgumentSlot,
                (unsigned)secondArgumentSlot);
    }
    if (syncStackSlot) {
        fprintf(file,
            "        /* zr_aot_static_i64_two_arg_direct_call_sync_stack_slot */\n"
            "        ZR_VALUE_FAST_SET(zr_aot_typed_destination,\n"
            "                          nativeInt64,\n"
            "                          zr_aot_s%u,\n"
            "                          ZR_VALUE_TYPE_INT64);\n",
            (unsigned)destinationSlot);
    }
    fprintf(file, "    }\n");
}

void backend_aot_write_c_static_direct_i64_three_arg_function_call(FILE *file,
                                                                   TZrUInt32 destinationSlot,
                                                                   TZrUInt32 calleeFlatIndex,
                                                                   TZrUInt32 firstArgumentSlot,
                                                                   TZrUInt32 secondArgumentSlot,
                                                                   TZrUInt32 thirdArgumentSlot,
                                                                   TZrBool syncStackSlot,
                                                                   TZrBool passStateToThunk) {
    if (file == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_static_i64_three_arg_direct_call */\n");
    if (syncStackSlot) {
        fprintf(file,
                "        SZrTypeValue *zr_aot_typed_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
                "        if (zr_aot_typed_destination == ZR_NULL) {\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n",
                (unsigned)destinationSlot);
    }
    if (passStateToThunk) {
        fprintf(file,
                "        zr_aot_s%u = zr_aot_typed_i64_fn_%u(state, zr_aot_s%u, zr_aot_s%u, zr_aot_s%u);\n",
                (unsigned)destinationSlot,
                (unsigned)calleeFlatIndex,
                (unsigned)firstArgumentSlot,
                (unsigned)secondArgumentSlot,
                (unsigned)thirdArgumentSlot);
    } else {
        fprintf(file,
                "        zr_aot_s%u = zr_aot_typed_i64_fn_%u(zr_aot_s%u, zr_aot_s%u, zr_aot_s%u);\n",
                (unsigned)destinationSlot,
                (unsigned)calleeFlatIndex,
                (unsigned)firstArgumentSlot,
                (unsigned)secondArgumentSlot,
                (unsigned)thirdArgumentSlot);
    }
    if (syncStackSlot) {
        fprintf(file,
                "        /* zr_aot_static_i64_three_arg_direct_call_sync_stack_slot */\n"
                "        ZR_VALUE_FAST_SET(zr_aot_typed_destination,\n"
                "                          nativeInt64,\n"
                "                          zr_aot_s%u,\n"
                "                          ZR_VALUE_TYPE_INT64);\n",
                (unsigned)destinationSlot);
    }
    fprintf(file, "    }\n");
}

void backend_aot_write_c_static_direct_u64_no_arg_function_call(FILE *file,
                                                                TZrUInt32 destinationSlot,
                                                                TZrUInt32 calleeFlatIndex,
                                                                TZrBool syncStackSlot) {
    if (file == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_static_u64_no_arg_direct_call */\n");
    if (syncStackSlot) {
        fprintf(file,
                "        SZrTypeValue *zr_aot_typed_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
                "        if (zr_aot_typed_destination == ZR_NULL) {\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n",
                (unsigned)destinationSlot);
    }
    fprintf(file,
            "        zr_aot_u%u = zr_aot_typed_u64_fn_%u();\n",
            (unsigned)destinationSlot,
            (unsigned)calleeFlatIndex);
    if (syncStackSlot) {
        fprintf(file,
                "        /* zr_aot_static_u64_no_arg_direct_call_sync_stack_slot */\n"
                "        ZR_VALUE_FAST_SET(zr_aot_typed_destination,\n"
                "                          nativeUInt64,\n"
                "                          zr_aot_u%u,\n"
                "                          ZR_VALUE_TYPE_UINT64);\n",
                (unsigned)destinationSlot);
    }
    fprintf(file, "    }\n");
}

void backend_aot_write_c_static_direct_u64_one_arg_function_call(FILE *file,
                                                                 TZrUInt32 destinationSlot,
                                                                 TZrUInt32 calleeFlatIndex,
                                                                 TZrUInt32 argumentSlot,
                                                                 TZrBool syncStackSlot) {
    if (file == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_static_u64_one_arg_direct_call */\n");
    if (syncStackSlot) {
        fprintf(file,
                "        SZrTypeValue *zr_aot_typed_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
                "        if (zr_aot_typed_destination == ZR_NULL) {\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n",
                (unsigned)destinationSlot);
    }
    fprintf(file,
            "        zr_aot_u%u = zr_aot_typed_u64_fn_%u(zr_aot_u%u);\n",
            (unsigned)destinationSlot,
            (unsigned)calleeFlatIndex,
            (unsigned)argumentSlot);
    if (syncStackSlot) {
        fprintf(file,
                "        /* zr_aot_static_u64_one_arg_direct_call_sync_stack_slot */\n"
                "        ZR_VALUE_FAST_SET(zr_aot_typed_destination,\n"
                "                          nativeUInt64,\n"
                "                          zr_aot_u%u,\n"
                "                          ZR_VALUE_TYPE_UINT64);\n",
                (unsigned)destinationSlot);
    }
    fprintf(file, "    }\n");
}

void backend_aot_write_c_static_direct_u64_two_arg_function_call(FILE *file,
                                                                 TZrUInt32 destinationSlot,
                                                                 TZrUInt32 calleeFlatIndex,
                                                                 TZrUInt32 firstArgumentSlot,
                                                                 TZrUInt32 secondArgumentSlot,
                                                                 TZrBool syncStackSlot,
                                                                 TZrBool passStateToThunk) {
    if (file == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_static_u64_two_arg_direct_call */\n");
    if (syncStackSlot) {
        fprintf(file,
                "        SZrTypeValue *zr_aot_typed_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
                "        if (zr_aot_typed_destination == ZR_NULL) {\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n",
                (unsigned)destinationSlot);
    }
    if (passStateToThunk) {
        fprintf(file,
                "        zr_aot_u%u = zr_aot_typed_u64_fn_%u(state, zr_aot_u%u, zr_aot_u%u);\n",
                (unsigned)destinationSlot,
                (unsigned)calleeFlatIndex,
                (unsigned)firstArgumentSlot,
                (unsigned)secondArgumentSlot);
    } else {
        fprintf(file,
                "        zr_aot_u%u = zr_aot_typed_u64_fn_%u(zr_aot_u%u, zr_aot_u%u);\n",
                (unsigned)destinationSlot,
                (unsigned)calleeFlatIndex,
                (unsigned)firstArgumentSlot,
                (unsigned)secondArgumentSlot);
    }
    if (syncStackSlot) {
        fprintf(file,
                "        /* zr_aot_static_u64_two_arg_direct_call_sync_stack_slot */\n"
                "        ZR_VALUE_FAST_SET(zr_aot_typed_destination,\n"
                "                          nativeUInt64,\n"
                "                          zr_aot_u%u,\n"
                "                          ZR_VALUE_TYPE_UINT64);\n",
                (unsigned)destinationSlot);
    }
    fprintf(file, "    }\n");
}

void backend_aot_write_c_static_direct_u64_three_arg_function_call(FILE *file,
                                                                   TZrUInt32 destinationSlot,
                                                                   TZrUInt32 calleeFlatIndex,
                                                                   TZrUInt32 firstArgumentSlot,
                                                                   TZrUInt32 secondArgumentSlot,
                                                                   TZrUInt32 thirdArgumentSlot,
                                                                   TZrBool syncStackSlot,
                                                                   TZrBool passStateToThunk) {
    if (file == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_static_u64_three_arg_direct_call */\n");
    if (syncStackSlot) {
        fprintf(file,
                "        SZrTypeValue *zr_aot_typed_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
                "        if (zr_aot_typed_destination == ZR_NULL) {\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n",
                (unsigned)destinationSlot);
    }
    if (passStateToThunk) {
        fprintf(file,
                "        zr_aot_u%u = zr_aot_typed_u64_fn_%u(state, zr_aot_u%u, zr_aot_u%u, zr_aot_u%u);\n",
                (unsigned)destinationSlot,
                (unsigned)calleeFlatIndex,
                (unsigned)firstArgumentSlot,
                (unsigned)secondArgumentSlot,
                (unsigned)thirdArgumentSlot);
    } else {
        fprintf(file,
                "        zr_aot_u%u = zr_aot_typed_u64_fn_%u(zr_aot_u%u, zr_aot_u%u, zr_aot_u%u);\n",
                (unsigned)destinationSlot,
                (unsigned)calleeFlatIndex,
                (unsigned)firstArgumentSlot,
                (unsigned)secondArgumentSlot,
                (unsigned)thirdArgumentSlot);
    }
    if (syncStackSlot) {
        fprintf(file,
                "        /* zr_aot_static_u64_three_arg_direct_call_sync_stack_slot */\n"
                "        ZR_VALUE_FAST_SET(zr_aot_typed_destination,\n"
                "                          nativeUInt64,\n"
                "                          zr_aot_u%u,\n"
                "                          ZR_VALUE_TYPE_UINT64);\n",
                (unsigned)destinationSlot);
    }
    fprintf(file, "    }\n");
}

void backend_aot_write_c_static_direct_f64_no_arg_function_call(FILE *file,
                                                                TZrUInt32 destinationSlot,
                                                                TZrUInt32 calleeFlatIndex,
                                                                TZrBool syncStackSlot) {
    if (file == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_static_f64_no_arg_direct_call */\n");
    if (syncStackSlot) {
        fprintf(file,
                "        SZrTypeValue *zr_aot_typed_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
                "        if (zr_aot_typed_destination == ZR_NULL) {\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n",
                (unsigned)destinationSlot);
    }
    fprintf(file,
            "        zr_aot_f%u = zr_aot_typed_f64_fn_%u();\n",
            (unsigned)destinationSlot,
            (unsigned)calleeFlatIndex);
    if (syncStackSlot) {
        fprintf(file,
                "        /* zr_aot_static_f64_no_arg_direct_call_sync_stack_slot */\n"
                "        ZR_VALUE_FAST_SET(zr_aot_typed_destination,\n"
                "                          nativeDouble,\n"
                "                          zr_aot_f%u,\n"
                "                          ZR_VALUE_TYPE_DOUBLE);\n",
                (unsigned)destinationSlot);
    }
    fprintf(file, "    }\n");
}

void backend_aot_write_c_static_direct_f64_one_arg_function_call(FILE *file,
                                                                 TZrUInt32 destinationSlot,
                                                                 TZrUInt32 calleeFlatIndex,
                                                                 TZrUInt32 argumentSlot,
                                                                 TZrBool syncStackSlot) {
    if (file == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_static_f64_one_arg_direct_call */\n");
    if (syncStackSlot) {
        fprintf(file,
                "        SZrTypeValue *zr_aot_typed_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
                "        if (zr_aot_typed_destination == ZR_NULL) {\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n",
                (unsigned)destinationSlot);
    }
    fprintf(file,
            "        zr_aot_f%u = zr_aot_typed_f64_fn_%u(zr_aot_f%u);\n",
            (unsigned)destinationSlot,
            (unsigned)calleeFlatIndex,
            (unsigned)argumentSlot);
    if (syncStackSlot) {
        fprintf(file,
                "        /* zr_aot_static_f64_one_arg_direct_call_sync_stack_slot */\n"
                "        ZR_VALUE_FAST_SET(zr_aot_typed_destination,\n"
                "                          nativeDouble,\n"
                "                          zr_aot_f%u,\n"
                "                          ZR_VALUE_TYPE_DOUBLE);\n",
                (unsigned)destinationSlot);
    }
    fprintf(file, "    }\n");
}

void backend_aot_write_c_static_direct_f64_two_arg_function_call(FILE *file,
                                                                 TZrUInt32 destinationSlot,
                                                                 TZrUInt32 calleeFlatIndex,
                                                                 TZrUInt32 firstArgumentSlot,
                                                                 TZrUInt32 secondArgumentSlot,
                                                                 TZrBool syncStackSlot,
                                                                 TZrBool passStateToThunk) {
    if (file == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_static_f64_two_arg_direct_call */\n");
    if (syncStackSlot) {
        fprintf(file,
                "        SZrTypeValue *zr_aot_typed_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
                "        if (zr_aot_typed_destination == ZR_NULL) {\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n",
                (unsigned)destinationSlot);
    }
    if (passStateToThunk) {
        fprintf(file,
                "        zr_aot_f%u = zr_aot_typed_f64_fn_%u(state, zr_aot_f%u, zr_aot_f%u);\n",
                (unsigned)destinationSlot,
                (unsigned)calleeFlatIndex,
                (unsigned)firstArgumentSlot,
                (unsigned)secondArgumentSlot);
    } else {
        fprintf(file,
                "        zr_aot_f%u = zr_aot_typed_f64_fn_%u(zr_aot_f%u, zr_aot_f%u);\n",
                (unsigned)destinationSlot,
                (unsigned)calleeFlatIndex,
                (unsigned)firstArgumentSlot,
                (unsigned)secondArgumentSlot);
    }
    if (syncStackSlot) {
        fprintf(file,
                "        /* zr_aot_static_f64_two_arg_direct_call_sync_stack_slot */\n"
                "        ZR_VALUE_FAST_SET(zr_aot_typed_destination,\n"
                "                          nativeDouble,\n"
                "                          zr_aot_f%u,\n"
                "                          ZR_VALUE_TYPE_DOUBLE);\n",
                (unsigned)destinationSlot);
    }
    fprintf(file, "    }\n");
}

void backend_aot_write_c_static_direct_f64_three_arg_function_call(FILE *file,
                                                                   TZrUInt32 destinationSlot,
                                                                   TZrUInt32 calleeFlatIndex,
                                                                   TZrUInt32 firstArgumentSlot,
                                                                   TZrUInt32 secondArgumentSlot,
                                                                   TZrUInt32 thirdArgumentSlot,
                                                                   TZrBool syncStackSlot,
                                                                   TZrBool passStateToThunk) {
    if (file == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_static_f64_three_arg_direct_call */\n");
    if (syncStackSlot) {
        fprintf(file,
                "        SZrTypeValue *zr_aot_typed_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
                "        if (zr_aot_typed_destination == ZR_NULL) {\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n",
                (unsigned)destinationSlot);
    }
    if (passStateToThunk) {
        fprintf(file,
                "        zr_aot_f%u = zr_aot_typed_f64_fn_%u(state, zr_aot_f%u, zr_aot_f%u, zr_aot_f%u);\n",
                (unsigned)destinationSlot,
                (unsigned)calleeFlatIndex,
                (unsigned)firstArgumentSlot,
                (unsigned)secondArgumentSlot,
                (unsigned)thirdArgumentSlot);
    } else {
        fprintf(file,
                "        zr_aot_f%u = zr_aot_typed_f64_fn_%u(zr_aot_f%u, zr_aot_f%u, zr_aot_f%u);\n",
                (unsigned)destinationSlot,
                (unsigned)calleeFlatIndex,
                (unsigned)firstArgumentSlot,
                (unsigned)secondArgumentSlot,
                (unsigned)thirdArgumentSlot);
    }
    if (syncStackSlot) {
        fprintf(file,
                "        /* zr_aot_static_f64_three_arg_direct_call_sync_stack_slot */\n"
                "        ZR_VALUE_FAST_SET(zr_aot_typed_destination,\n"
                "                          nativeDouble,\n"
                "                          zr_aot_f%u,\n"
                "                          ZR_VALUE_TYPE_DOUBLE);\n",
                (unsigned)destinationSlot);
    }
    fprintf(file, "    }\n");
}
