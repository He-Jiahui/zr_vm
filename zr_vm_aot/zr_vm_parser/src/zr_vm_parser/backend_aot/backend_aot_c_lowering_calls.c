#include "backend_aot_c_emitter.h"
#include "backend_aot_c_scalar_locals.h"
#include "backend_aot_internal.h"

static void backend_aot_write_c_core_function_call(FILE *file,
                                                   const SZrAotExecIrFunction *functionIr,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 functionSlot,
                                                   TZrUInt32 argumentCount,
                                                   const char *marker,
                                                   const char *syncI64Marker,
                                                   const char *syncBoolMarker,
                                                   const char *syncU64Marker,
                                                   const char *syncF64Marker,
                                                   const char *errorLabel) {
    TZrBool syncI64Local;
    TZrBool syncBoolLocal;
    TZrBool syncU64Local;
    TZrBool syncF64Local;

    if (file == ZR_NULL ||
        marker == ZR_NULL ||
        syncI64Marker == ZR_NULL ||
        syncBoolMarker == ZR_NULL ||
        syncU64Marker == ZR_NULL ||
        syncF64Marker == ZR_NULL ||
        errorLabel == ZR_NULL) {
        return;
    }

    syncI64Local = backend_aot_c_scalar_locals_has_i64_slot(functionIr, destinationSlot);
    syncBoolLocal = backend_aot_c_scalar_locals_has_bool_slot(functionIr, destinationSlot);
    syncU64Local = backend_aot_c_scalar_locals_has_u64_slot(functionIr, destinationSlot);
    syncF64Local = backend_aot_c_scalar_locals_has_f64_slot(functionIr, destinationSlot);
    fprintf(file,
            "    {\n"
            "        /* %s */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_CallStackValue(state,\n"
            "                                                             &frame,\n"
            "                                                             %u,\n"
            "                                                             %u,\n"
            "                                                             %u,\n"
            "                                                             \"%s\"));\n",
            marker,
            (unsigned)destinationSlot,
            (unsigned)functionSlot,
            (unsigned)argumentCount,
            errorLabel);
    if (syncI64Local) {
        fprintf(file,
                "        /* %s */\n"
                "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SyncSignedIntLocal(state, &frame, %u, &zr_aot_s%u));\n",
                syncI64Marker,
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncBoolLocal) {
        fprintf(file,
                "        /* %s */\n"
                "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, %u, &zr_aot_b%u));\n",
                syncBoolMarker,
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncU64Local) {
        fprintf(file,
                "        /* %s */\n"
                "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SyncUnsignedIntLocal(state, &frame, %u, &zr_aot_u%u));\n",
                syncU64Marker,
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncF64Local) {
        fprintf(file,
                "        /* %s */\n"
                "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, %u, &zr_aot_f%u));\n",
                syncF64Marker,
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    fprintf(file, "    }\n");
}

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

void backend_aot_write_c_static_direct_function_call(FILE *file,
                                                     const SZrAotExecIrFunction *functionIr,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 functionSlot,
                                                     TZrUInt32 argumentCount,
                                                     TZrUInt32 execInstructionIndex,
                                                     TZrUInt32 calleeFlatIndex) {
    TZrBool syncI64Local;
    TZrBool syncBoolLocal;
    TZrBool syncU64Local;
    TZrBool syncF64Local;

    if (file == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    syncI64Local = backend_aot_c_scalar_locals_has_i64_slot(functionIr, destinationSlot);
    syncBoolLocal = backend_aot_c_scalar_locals_has_bool_slot(functionIr, destinationSlot);
    syncU64Local = backend_aot_c_scalar_locals_has_u64_slot(functionIr, destinationSlot);
    syncF64Local = backend_aot_c_scalar_locals_has_f64_slot(functionIr, destinationSlot);
    fprintf(file,
            "    {\n"
            "        /* zr_aot_direct_static_function_call */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_CallStaticDirect(state,\n"
            "                                                               &frame,\n"
            "                                                               %u,\n"
            "                                                               %u,\n"
            "                                                               %u,\n"
            "                                                               %u,\n"
            "                                                               zr_aot_fn_%u));\n",
            (unsigned)destinationSlot,
            (unsigned)functionSlot,
            (unsigned)argumentCount,
            (unsigned)calleeFlatIndex,
            (unsigned)calleeFlatIndex);
    if (syncI64Local) {
        fprintf(file,
                "        /* zr_aot_direct_static_function_call_sync_i64_local_boundary */\n"
                "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SyncSignedIntLocal(state, &frame, %u, &zr_aot_s%u));\n",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncBoolLocal) {
        fprintf(file,
                "        /* zr_aot_direct_static_function_call_sync_bool_local_boundary */\n"
                "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, %u, &zr_aot_b%u));\n",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncU64Local) {
        fprintf(file,
                "        /* zr_aot_direct_static_function_call_sync_u64_local_boundary */\n"
                "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SyncUnsignedIntLocal(state, &frame, %u, &zr_aot_u%u));\n",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncF64Local) {
        fprintf(file,
                "        /* zr_aot_direct_static_function_call_sync_f64_local_boundary */\n"
                "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, %u, &zr_aot_f%u));\n",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    (void)execInstructionIndex;
    fprintf(file, "    }\n");
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
            "        zr_aot_s%u = zr_aot_typed_i64_fn_%u(state);\n",
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
            "        zr_aot_s%u = zr_aot_typed_i64_fn_%u(state, zr_aot_s%u);\n",
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
                                                                 TZrBool syncStackSlot) {
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
    fprintf(file,
            "        zr_aot_s%u = zr_aot_typed_i64_fn_%u(state, zr_aot_s%u, zr_aot_s%u);\n",
            (unsigned)destinationSlot,
            (unsigned)calleeFlatIndex,
            (unsigned)firstArgumentSlot,
            (unsigned)secondArgumentSlot);
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

void backend_aot_write_c_static_direct_bool_no_arg_function_call(FILE *file,
                                                                 TZrUInt32 destinationSlot,
                                                                 TZrUInt32 calleeFlatIndex,
                                                                 TZrBool syncStackSlot) {
    if (file == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_static_bool_no_arg_direct_call */\n");
    if (syncStackSlot) {
        fprintf(file,
                "        SZrTypeValue *zr_aot_typed_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
                "        if (zr_aot_typed_destination == ZR_NULL) {\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n",
                (unsigned)destinationSlot);
    }
    fprintf(file,
            "        zr_aot_b%u = zr_aot_typed_bool_fn_%u(state);\n",
            (unsigned)destinationSlot,
            (unsigned)calleeFlatIndex);
    if (syncStackSlot) {
        fprintf(file,
                "        /* zr_aot_static_bool_no_arg_direct_call_sync_stack_slot */\n"
                "        ZR_VALUE_FAST_SET(zr_aot_typed_destination,\n"
                "                          nativeBool,\n"
                "                          zr_aot_b%u,\n"
                "                          ZR_VALUE_TYPE_BOOL);\n",
                (unsigned)destinationSlot);
    }
    fprintf(file, "    }\n");
}

void backend_aot_write_c_static_direct_bool_one_arg_function_call(FILE *file,
                                                                  TZrUInt32 destinationSlot,
                                                                  TZrUInt32 calleeFlatIndex,
                                                                  TZrUInt32 argumentSlot,
                                                                  TZrBool syncStackSlot) {
    if (file == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_static_bool_one_arg_direct_call */\n");
    if (syncStackSlot) {
        fprintf(file,
                "        SZrTypeValue *zr_aot_typed_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
                "        if (zr_aot_typed_destination == ZR_NULL) {\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n",
                (unsigned)destinationSlot);
    }
    fprintf(file,
            "        zr_aot_b%u = zr_aot_typed_bool_fn_%u(state, zr_aot_b%u);\n",
            (unsigned)destinationSlot,
            (unsigned)calleeFlatIndex,
            (unsigned)argumentSlot);
    if (syncStackSlot) {
        fprintf(file,
                "        /* zr_aot_static_bool_one_arg_direct_call_sync_stack_slot */\n"
                "        ZR_VALUE_FAST_SET(zr_aot_typed_destination,\n"
                "                          nativeBool,\n"
                "                          zr_aot_b%u,\n"
                "                          ZR_VALUE_TYPE_BOOL);\n",
                (unsigned)destinationSlot);
    }
    fprintf(file, "    }\n");
}

void backend_aot_write_c_static_direct_bool_two_arg_function_call(FILE *file,
                                                                  TZrUInt32 destinationSlot,
                                                                  TZrUInt32 calleeFlatIndex,
                                                                  TZrUInt32 firstArgumentSlot,
                                                                  TZrUInt32 secondArgumentSlot,
                                                                  TZrBool syncStackSlot) {
    if (file == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_static_bool_two_arg_direct_call */\n");
    if (syncStackSlot) {
        fprintf(file,
                "        SZrTypeValue *zr_aot_typed_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
                "        if (zr_aot_typed_destination == ZR_NULL) {\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n",
                (unsigned)destinationSlot);
    }
    fprintf(file,
            "        zr_aot_b%u = zr_aot_typed_bool_fn_%u(state, zr_aot_b%u, zr_aot_b%u);\n",
            (unsigned)destinationSlot,
            (unsigned)calleeFlatIndex,
            (unsigned)firstArgumentSlot,
            (unsigned)secondArgumentSlot);
    if (syncStackSlot) {
        fprintf(file,
                "        /* zr_aot_static_bool_two_arg_direct_call_sync_stack_slot */\n"
                "        ZR_VALUE_FAST_SET(zr_aot_typed_destination,\n"
                "                          nativeBool,\n"
                "                          zr_aot_b%u,\n"
                "                          ZR_VALUE_TYPE_BOOL);\n",
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
            "        zr_aot_u%u = zr_aot_typed_u64_fn_%u(state);\n",
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
            "        zr_aot_u%u = zr_aot_typed_u64_fn_%u(state, zr_aot_u%u);\n",
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
                                                                 TZrBool syncStackSlot) {
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
    fprintf(file,
            "        zr_aot_u%u = zr_aot_typed_u64_fn_%u(state, zr_aot_u%u, zr_aot_u%u);\n",
            (unsigned)destinationSlot,
            (unsigned)calleeFlatIndex,
            (unsigned)firstArgumentSlot,
            (unsigned)secondArgumentSlot);
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
            "        zr_aot_f%u = zr_aot_typed_f64_fn_%u(state);\n",
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
            "        zr_aot_f%u = zr_aot_typed_f64_fn_%u(state, zr_aot_f%u);\n",
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
                                                                 TZrBool syncStackSlot) {
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
    fprintf(file,
            "        zr_aot_f%u = zr_aot_typed_f64_fn_%u(state, zr_aot_f%u, zr_aot_f%u);\n",
            (unsigned)destinationSlot,
            (unsigned)calleeFlatIndex,
            (unsigned)firstArgumentSlot,
            (unsigned)secondArgumentSlot);
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

void backend_aot_write_c_direct_function_call(FILE *file,
                                              const SZrAotExecIrFunction *functionIr,
                                              TZrUInt32 destinationSlot,
                                              TZrUInt32 functionSlot,
                                              TZrUInt32 argumentCount) {
    backend_aot_write_c_core_function_call(file,
                                           functionIr,
                                           destinationSlot,
                                           functionSlot,
                                           argumentCount,
                                           "zr_aot_direct_function_call",
                                           "zr_aot_direct_function_call_sync_i64_local_boundary",
                                           "zr_aot_direct_function_call_sync_bool_local_boundary",
                                           "zr_aot_direct_function_call_sync_u64_local_boundary",
                                           "zr_aot_direct_function_call_sync_f64_local_boundary",
                                           "function call");
}

void backend_aot_write_c_dynamic_function_call(FILE *file,
                                               const SZrAotExecIrFunction *functionIr,
                                               TZrUInt32 destinationSlot,
                                               TZrUInt32 functionSlot,
                                               TZrUInt32 argumentCount) {
    backend_aot_write_c_core_function_call(file,
                                           functionIr,
                                           destinationSlot,
                                           functionSlot,
                                           argumentCount,
                                           "zr_aot_direct_dynamic_function_call",
                                           "zr_aot_direct_dynamic_function_call_sync_i64_local_boundary",
                                           "zr_aot_direct_dynamic_function_call_sync_bool_local_boundary",
                                           "zr_aot_direct_dynamic_function_call_sync_u64_local_boundary",
                                           "zr_aot_direct_dynamic_function_call_sync_f64_local_boundary",
                                           "dynamic call");
}
