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
                                               TZrUInt32 argumentCount,
                                               TZrUInt32 deoptId) {
    TZrBool syncI64Local;
    TZrBool syncBoolLocal;
    TZrBool syncU64Local;
    TZrBool syncF64Local;

    if (file == ZR_NULL) {
        return;
    }

    syncI64Local = backend_aot_c_scalar_locals_has_i64_slot(functionIr, destinationSlot);
    syncBoolLocal = backend_aot_c_scalar_locals_has_bool_slot(functionIr, destinationSlot);
    syncU64Local = backend_aot_c_scalar_locals_has_u64_slot(functionIr, destinationSlot);
    syncF64Local = backend_aot_c_scalar_locals_has_f64_slot(functionIr, destinationSlot);
    fprintf(file,
            "    {\n"
            "        /* zr_aot_direct_dynamic_function_call */\n"
            "        /* zr_aot_dynamic_deopt_bridge deopt=%u */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_CallDynamicDeoptBridge(state,\n"
            "                                                                     &frame,\n"
            "                                                                     %u,\n"
            "                                                                     %u,\n"
            "                                                                     %u,\n"
            "                                                                     %u,\n"
            "                                                                     \"dynamic call\"));\n",
            (unsigned)deoptId,
            (unsigned)destinationSlot,
            (unsigned)functionSlot,
            (unsigned)argumentCount,
            (unsigned)deoptId);
    if (syncI64Local) {
        fprintf(file,
                "        /* zr_aot_direct_dynamic_function_call_sync_i64_local_boundary */\n"
                "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SyncSignedIntLocal(state, &frame, %u, &zr_aot_s%u));\n",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncBoolLocal) {
        fprintf(file,
                "        /* zr_aot_direct_dynamic_function_call_sync_bool_local_boundary */\n"
                "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, %u, &zr_aot_b%u));\n",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncU64Local) {
        fprintf(file,
                "        /* zr_aot_direct_dynamic_function_call_sync_u64_local_boundary */\n"
                "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SyncUnsignedIntLocal(state, &frame, %u, &zr_aot_u%u));\n",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncF64Local) {
        fprintf(file,
                "        /* zr_aot_direct_dynamic_function_call_sync_f64_local_boundary */\n"
                "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, %u, &zr_aot_f%u));\n",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    fprintf(file, "    }\n");
}
