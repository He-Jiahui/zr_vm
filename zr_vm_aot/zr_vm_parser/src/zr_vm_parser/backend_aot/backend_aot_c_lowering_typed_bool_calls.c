#include "backend_aot_c_emitter.h"

#include "backend_aot_internal.h"

static void backend_aot_write_c_static_direct_bool_deopt_fallback(FILE *file,
                                                                  TZrUInt32 destinationSlot,
                                                                  TZrUInt32 functionSlot,
                                                                  TZrUInt32 argumentCount,
                                                                  TZrUInt32 calleeFlatIndex,
                                                                  const char *label) {
    if (file == ZR_NULL || label == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "        } else {\n"
            "            ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_DeoptTypedDirectCall(state,\n"
            "                                                                        &frame,\n"
            "                                                                        %u,\n"
            "                                                                        %u,\n"
            "                                                                        %u,\n"
            "                                                                        %u,\n"
            "                                                                        \"%s\"));\n"
            "            ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, %u, &zr_aot_b%u));\n"
            "        }\n",
            (unsigned)destinationSlot,
            (unsigned)functionSlot,
            (unsigned)argumentCount,
            (unsigned)calleeFlatIndex,
            label,
            (unsigned)destinationSlot,
            (unsigned)destinationSlot);
}

void backend_aot_write_c_static_direct_bool_no_arg_function_call(FILE *file,
                                                                 TZrUInt32 destinationSlot,
                                                                 TZrUInt32 functionSlot,
                                                                 TZrUInt32 calleeFlatIndex,
                                                                 TZrBool syncStackSlot) {
    if (file == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_static_bool_no_arg_direct_call */\n"
            "        /* zr_aot_static_bool_no_arg_direct_call_metadata_guard */\n"
            "        if (ZrLibrary_AotRuntime_CanUseTypedDirectCall(state, &frame, %u)) {\n",
            (unsigned)calleeFlatIndex);
    if (syncStackSlot) {
        fprintf(file,
                "            SZrTypeValue *zr_aot_typed_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
                "            if (zr_aot_typed_destination == ZR_NULL) {\n"
                "                ZR_AOT_C_FAIL();\n"
                "            }\n",
                (unsigned)destinationSlot);
    }
    fprintf(file,
            "            zr_aot_b%u = zr_aot_typed_bool_fn_%u();\n",
            (unsigned)destinationSlot,
            (unsigned)calleeFlatIndex);
    if (syncStackSlot) {
        fprintf(file,
                "            /* zr_aot_static_bool_no_arg_direct_call_sync_stack_slot */\n"
                "            ZR_VALUE_FAST_SET(zr_aot_typed_destination,\n"
                "                              nativeBool,\n"
                "                              zr_aot_b%u,\n"
                "                              ZR_VALUE_TYPE_BOOL);\n",
                (unsigned)destinationSlot);
    }
    backend_aot_write_c_static_direct_bool_deopt_fallback(file,
                                                          destinationSlot,
                                                          functionSlot,
                                                          0u,
                                                          calleeFlatIndex,
                                                          "typed bool direct call");
    fprintf(file, "    }\n");
}

void backend_aot_write_c_static_direct_bool_one_arg_function_call(FILE *file,
                                                                  TZrUInt32 destinationSlot,
                                                                  TZrUInt32 functionSlot,
                                                                  TZrUInt32 calleeFlatIndex,
                                                                  TZrUInt32 argumentSlot,
                                                                  TZrBool syncStackSlot) {
    if (file == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_static_bool_one_arg_direct_call */\n"
            "        /* zr_aot_static_bool_one_arg_direct_call_metadata_guard */\n"
            "        if (ZrLibrary_AotRuntime_CanUseTypedDirectCall(state, &frame, %u)) {\n",
            (unsigned)calleeFlatIndex);
    if (syncStackSlot) {
        fprintf(file,
                "            SZrTypeValue *zr_aot_typed_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
                "            if (zr_aot_typed_destination == ZR_NULL) {\n"
                "                ZR_AOT_C_FAIL();\n"
                "            }\n",
                (unsigned)destinationSlot);
    }
    fprintf(file,
            "            zr_aot_b%u = zr_aot_typed_bool_fn_%u(zr_aot_b%u);\n",
            (unsigned)destinationSlot,
            (unsigned)calleeFlatIndex,
            (unsigned)argumentSlot);
    if (syncStackSlot) {
        fprintf(file,
                "            /* zr_aot_static_bool_one_arg_direct_call_sync_stack_slot */\n"
                "            ZR_VALUE_FAST_SET(zr_aot_typed_destination,\n"
                "                              nativeBool,\n"
                "                              zr_aot_b%u,\n"
                "                              ZR_VALUE_TYPE_BOOL);\n",
                (unsigned)destinationSlot);
    }
    backend_aot_write_c_static_direct_bool_deopt_fallback(file,
                                                          destinationSlot,
                                                          functionSlot,
                                                          1u,
                                                          calleeFlatIndex,
                                                          "typed bool direct call");
    fprintf(file, "    }\n");
}

void backend_aot_write_c_static_direct_bool_two_arg_function_call(FILE *file,
                                                                  TZrUInt32 destinationSlot,
                                                                  TZrUInt32 functionSlot,
                                                                  TZrUInt32 calleeFlatIndex,
                                                                  TZrUInt32 firstArgumentSlot,
                                                                  TZrUInt32 secondArgumentSlot,
                                                                  TZrBool syncStackSlot) {
    if (file == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_static_bool_two_arg_direct_call */\n"
            "        /* zr_aot_static_bool_two_arg_direct_call_metadata_guard */\n"
            "        if (ZrLibrary_AotRuntime_CanUseTypedDirectCall(state, &frame, %u)) {\n",
            (unsigned)calleeFlatIndex);
    if (syncStackSlot) {
        fprintf(file,
                "            SZrTypeValue *zr_aot_typed_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
                "            if (zr_aot_typed_destination == ZR_NULL) {\n"
                "                ZR_AOT_C_FAIL();\n"
                "            }\n",
                (unsigned)destinationSlot);
    }
    fprintf(file,
            "            zr_aot_b%u = zr_aot_typed_bool_fn_%u(zr_aot_b%u, zr_aot_b%u);\n",
            (unsigned)destinationSlot,
            (unsigned)calleeFlatIndex,
            (unsigned)firstArgumentSlot,
            (unsigned)secondArgumentSlot);
    if (syncStackSlot) {
        fprintf(file,
                "            /* zr_aot_static_bool_two_arg_direct_call_sync_stack_slot */\n"
                "            ZR_VALUE_FAST_SET(zr_aot_typed_destination,\n"
                "                              nativeBool,\n"
                "                              zr_aot_b%u,\n"
                "                              ZR_VALUE_TYPE_BOOL);\n",
                (unsigned)destinationSlot);
    }
    backend_aot_write_c_static_direct_bool_deopt_fallback(file,
                                                          destinationSlot,
                                                          functionSlot,
                                                          2u,
                                                          calleeFlatIndex,
                                                          "typed bool direct call");
    fprintf(file, "    }\n");
}

void backend_aot_write_c_static_direct_bool_three_arg_function_call(FILE *file,
                                                                    TZrUInt32 destinationSlot,
                                                                    TZrUInt32 functionSlot,
                                                                    TZrUInt32 calleeFlatIndex,
                                                                    TZrUInt32 firstArgumentSlot,
                                                                    TZrUInt32 secondArgumentSlot,
                                                                    TZrUInt32 thirdArgumentSlot,
                                                                    TZrBool syncStackSlot) {
    if (file == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_static_bool_three_arg_direct_call */\n"
            "        /* zr_aot_static_bool_three_arg_direct_call_metadata_guard */\n"
            "        if (ZrLibrary_AotRuntime_CanUseTypedDirectCall(state, &frame, %u)) {\n",
            (unsigned)calleeFlatIndex);
    if (syncStackSlot) {
        fprintf(file,
                "            SZrTypeValue *zr_aot_typed_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
                "            if (zr_aot_typed_destination == ZR_NULL) {\n"
                "                ZR_AOT_C_FAIL();\n"
                "            }\n",
                (unsigned)destinationSlot);
    }
    fprintf(file,
            "            zr_aot_b%u = zr_aot_typed_bool_fn_%u(zr_aot_b%u, zr_aot_b%u, zr_aot_b%u);\n",
            (unsigned)destinationSlot,
            (unsigned)calleeFlatIndex,
            (unsigned)firstArgumentSlot,
            (unsigned)secondArgumentSlot,
            (unsigned)thirdArgumentSlot);
    if (syncStackSlot) {
        fprintf(file,
                "            /* zr_aot_static_bool_three_arg_direct_call_sync_stack_slot */\n"
                "            ZR_VALUE_FAST_SET(zr_aot_typed_destination,\n"
                "                              nativeBool,\n"
                "                              zr_aot_b%u,\n"
                "                              ZR_VALUE_TYPE_BOOL);\n",
                (unsigned)destinationSlot);
    }
    backend_aot_write_c_static_direct_bool_deopt_fallback(file,
                                                          destinationSlot,
                                                          functionSlot,
                                                          3u,
                                                          calleeFlatIndex,
                                                          "typed bool direct call");
    fprintf(file, "    }\n");
}

void backend_aot_write_c_static_direct_i64_bool_two_arg_function_call(FILE *file,
                                                                      TZrUInt32 destinationSlot,
                                                                      TZrUInt32 functionSlot,
                                                                      TZrUInt32 calleeFlatIndex,
                                                                      TZrUInt32 firstArgumentSlot,
                                                                      TZrUInt32 secondArgumentSlot,
                                                                      TZrBool syncStackSlot) {
    if (file == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_static_i64_bool_two_arg_direct_call */\n"
            "        /* zr_aot_static_i64_bool_two_arg_direct_call_metadata_guard */\n"
            "        if (ZrLibrary_AotRuntime_CanUseTypedDirectCall(state, &frame, %u)) {\n",
            (unsigned)calleeFlatIndex);
    if (syncStackSlot) {
        fprintf(file,
                "            SZrTypeValue *zr_aot_typed_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
                "            if (zr_aot_typed_destination == ZR_NULL) {\n"
                "                ZR_AOT_C_FAIL();\n"
                "            }\n",
                (unsigned)destinationSlot);
    }
    fprintf(file,
            "            zr_aot_b%u = zr_aot_typed_bool_fn_%u(zr_aot_s%u, zr_aot_s%u);\n",
            (unsigned)destinationSlot,
            (unsigned)calleeFlatIndex,
            (unsigned)firstArgumentSlot,
            (unsigned)secondArgumentSlot);
    if (syncStackSlot) {
        fprintf(file,
                "            /* zr_aot_static_i64_bool_two_arg_direct_call_sync_stack_slot */\n"
                "            ZR_VALUE_FAST_SET(zr_aot_typed_destination,\n"
                "                              nativeBool,\n"
                "                              zr_aot_b%u,\n"
                "                              ZR_VALUE_TYPE_BOOL);\n",
                (unsigned)destinationSlot);
    }
    backend_aot_write_c_static_direct_bool_deopt_fallback(file,
                                                          destinationSlot,
                                                          functionSlot,
                                                          2u,
                                                          calleeFlatIndex,
                                                          "typed bool direct call");
    fprintf(file, "    }\n");
}

void backend_aot_write_c_static_direct_u64_bool_two_arg_function_call(FILE *file,
                                                                      TZrUInt32 destinationSlot,
                                                                      TZrUInt32 functionSlot,
                                                                      TZrUInt32 calleeFlatIndex,
                                                                      TZrUInt32 firstArgumentSlot,
                                                                      TZrUInt32 secondArgumentSlot,
                                                                      TZrBool syncStackSlot) {
    if (file == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_static_u64_bool_two_arg_direct_call */\n"
            "        /* zr_aot_static_u64_bool_two_arg_direct_call_metadata_guard */\n"
            "        if (ZrLibrary_AotRuntime_CanUseTypedDirectCall(state, &frame, %u)) {\n",
            (unsigned)calleeFlatIndex);
    if (syncStackSlot) {
        fprintf(file,
                "            SZrTypeValue *zr_aot_typed_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
                "            if (zr_aot_typed_destination == ZR_NULL) {\n"
                "                ZR_AOT_C_FAIL();\n"
                "            }\n",
                (unsigned)destinationSlot);
    }
    fprintf(file,
            "            zr_aot_b%u = zr_aot_typed_bool_fn_%u(zr_aot_u%u, zr_aot_u%u);\n",
            (unsigned)destinationSlot,
            (unsigned)calleeFlatIndex,
            (unsigned)firstArgumentSlot,
            (unsigned)secondArgumentSlot);
    if (syncStackSlot) {
        fprintf(file,
                "            /* zr_aot_static_u64_bool_two_arg_direct_call_sync_stack_slot */\n"
                "            ZR_VALUE_FAST_SET(zr_aot_typed_destination,\n"
                "                              nativeBool,\n"
                "                              zr_aot_b%u,\n"
                "                              ZR_VALUE_TYPE_BOOL);\n",
                (unsigned)destinationSlot);
    }
    backend_aot_write_c_static_direct_bool_deopt_fallback(file,
                                                          destinationSlot,
                                                          functionSlot,
                                                          2u,
                                                          calleeFlatIndex,
                                                          "typed bool direct call");
    fprintf(file, "    }\n");
}

void backend_aot_write_c_static_direct_f64_bool_two_arg_function_call(FILE *file,
                                                                      TZrUInt32 destinationSlot,
                                                                      TZrUInt32 functionSlot,
                                                                      TZrUInt32 calleeFlatIndex,
                                                                      TZrUInt32 firstArgumentSlot,
                                                                      TZrUInt32 secondArgumentSlot,
                                                                      TZrBool syncStackSlot) {
    if (file == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_static_f64_bool_two_arg_direct_call */\n"
            "        /* zr_aot_static_f64_bool_two_arg_direct_call_metadata_guard */\n"
            "        if (ZrLibrary_AotRuntime_CanUseTypedDirectCall(state, &frame, %u)) {\n",
            (unsigned)calleeFlatIndex);
    if (syncStackSlot) {
        fprintf(file,
                "            SZrTypeValue *zr_aot_typed_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
                "            if (zr_aot_typed_destination == ZR_NULL) {\n"
                "                ZR_AOT_C_FAIL();\n"
                "            }\n",
                (unsigned)destinationSlot);
    }
    fprintf(file,
            "            zr_aot_b%u = zr_aot_typed_bool_fn_%u(zr_aot_f%u, zr_aot_f%u);\n",
            (unsigned)destinationSlot,
            (unsigned)calleeFlatIndex,
            (unsigned)firstArgumentSlot,
            (unsigned)secondArgumentSlot);
    if (syncStackSlot) {
        fprintf(file,
                "            /* zr_aot_static_f64_bool_two_arg_direct_call_sync_stack_slot */\n"
                "            ZR_VALUE_FAST_SET(zr_aot_typed_destination,\n"
                "                              nativeBool,\n"
                "                              zr_aot_b%u,\n"
                "                              ZR_VALUE_TYPE_BOOL);\n",
                (unsigned)destinationSlot);
    }
    backend_aot_write_c_static_direct_bool_deopt_fallback(file,
                                                          destinationSlot,
                                                          functionSlot,
                                                          2u,
                                                          calleeFlatIndex,
                                                          "typed bool direct call");
    fprintf(file, "    }\n");
}
