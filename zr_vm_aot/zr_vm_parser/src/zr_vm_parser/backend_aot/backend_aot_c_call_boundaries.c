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
            "%s"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_CallStackValue(state,\n"
            "                                                             &frame,\n"
            "                                                             %u,\n"
            "                                                             %u,\n"
            "                                                             %u,\n"
            "                                                             \"%s\")",
            marker,
            syncI64Local || syncBoolLocal || syncU64Local || syncF64Local
                    ? "        /* zr_aot_call_result_sync_compact */\n"
                    : "",
            (unsigned)destinationSlot,
            (unsigned)functionSlot,
            (unsigned)argumentCount,
            errorLabel);
    if (syncI64Local) {
        fprintf(file,
                " &&\n"
                "                        /* %s */\n"
                "                        ZrLibrary_AotRuntime_SyncSignedIntLocal(state, &frame, %u, &zr_aot_s%u)",
                syncI64Marker,
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncBoolLocal) {
        fprintf(file,
                " &&\n"
                "                        /* %s */\n"
                "                        ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, %u, &zr_aot_b%u)",
                syncBoolMarker,
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncU64Local) {
        fprintf(file,
                " &&\n"
                "                        /* %s */\n"
                "                        ZrLibrary_AotRuntime_SyncUnsignedIntLocal(state, &frame, %u, &zr_aot_u%u)",
                syncU64Marker,
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncF64Local) {
        fprintf(file,
                " &&\n"
                "                        /* %s */\n"
                "                        ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, %u, &zr_aot_f%u)",
                syncF64Marker,
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    fprintf(file, ");\n"
                  "    }\n");
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
            "        ZrAotGeneratedDirectCall zr_aot_static_direct_call = {0};\n"
            "        TZrBool zr_aot_static_direct_succeeded;\n"
            "        /* zr_aot_direct_static_function_call */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_PrepareStaticDirectCall(state,\n"
            "                                                                      &frame,\n"
            "                                                                      %u,\n"
            "                                                                      %u,\n"
            "                                                                      %u,\n"
            "                                                                      %u,\n"
            "                                                                      &zr_aot_static_direct_call));\n"
            "        zr_aot_static_direct_succeeded = (TZrBool)(zr_aot_fn_%u(state) != 0);\n"
            "        zr_aot_next_instruction = ZR_AOT_RUNTIME_RESUME_FALLTHROUGH;\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_CompletePreparedDirectCallWithResume(\n"
            "                state,\n"
            "                &frame,\n"
            "                &zr_aot_static_direct_call,\n"
            "                zr_aot_static_direct_succeeded,\n"
            "                1,\n"
            "                &zr_aot_next_instruction));\n"
            "        if (zr_aot_next_instruction != ZR_AOT_RUNTIME_RESUME_FALLTHROUGH) {\n"
            "            goto zr_aot_fn_%u_dispatch;\n"
            "        }\n"
            "%s",
            (unsigned)destinationSlot,
            (unsigned)functionSlot,
            (unsigned)argumentCount,
            (unsigned)calleeFlatIndex,
            (unsigned)calleeFlatIndex,
            (unsigned)functionIr->flatIndex,
            syncI64Local || syncBoolLocal || syncU64Local || syncF64Local
                    ? "        /* zr_aot_call_result_sync_compact */\n"
                      "        ZR_AOT_C_GUARD(\n"
                    : "");
    if (syncI64Local) {
        fprintf(file,
                "                        /* zr_aot_direct_static_function_call_sync_i64_local_boundary */\n"
                "                        ZrLibrary_AotRuntime_SyncSignedIntLocal(state, &frame, %u, &zr_aot_s%u) &&\n",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncBoolLocal) {
        fprintf(file,
                "                        /* zr_aot_direct_static_function_call_sync_bool_local_boundary */\n"
                "                        ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, %u, &zr_aot_b%u) &&\n",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncU64Local) {
        fprintf(file,
                "                        /* zr_aot_direct_static_function_call_sync_u64_local_boundary */\n"
                "                        ZrLibrary_AotRuntime_SyncUnsignedIntLocal(state, &frame, %u, &zr_aot_u%u) &&\n",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncF64Local) {
        fprintf(file,
                "                        /* zr_aot_direct_static_function_call_sync_f64_local_boundary */\n"
                "                        ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, %u, &zr_aot_f%u) &&\n",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncI64Local || syncBoolLocal || syncU64Local || syncF64Local) {
        fprintf(file, "                        ZR_TRUE);\n");
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

void backend_aot_write_c_spread_function_call(
        FILE *file,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 functionSlot,
        TZrUInt32 prefixArgumentCount) {
    TZrBool syncI64Local;
    TZrBool syncBoolLocal;
    TZrBool syncU64Local;
    TZrBool syncF64Local;

    if (file == ZR_NULL) {
        return;
    }

    syncI64Local = backend_aot_c_scalar_locals_has_i64_slot(
            functionIr, destinationSlot);
    syncBoolLocal = backend_aot_c_scalar_locals_has_bool_slot(
            functionIr, destinationSlot);
    syncU64Local = backend_aot_c_scalar_locals_has_u64_slot(
            functionIr, destinationSlot);
    syncF64Local = backend_aot_c_scalar_locals_has_f64_slot(
            functionIr, destinationSlot);
    fprintf(file,
            "    {\n"
            "        /* zr_aot_spread_function_call */\n"
            "%s"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_CallSpread(state, &frame, %u, %u, %u, \"spread function call\")",
            syncI64Local || syncBoolLocal || syncU64Local || syncF64Local
                    ? "        /* zr_aot_call_result_sync_compact */\n"
                    : "",
            (unsigned)destinationSlot,
            (unsigned)functionSlot,
            (unsigned)prefixArgumentCount);
    if (syncI64Local) {
        fprintf(file,
                " &&\n"
                "                        ZrLibrary_AotRuntime_SyncSignedIntLocal(state, &frame, %u, &zr_aot_s%u)",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncBoolLocal) {
        fprintf(file,
                " &&\n"
                "                        ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, %u, &zr_aot_b%u)",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncU64Local) {
        fprintf(file,
                " &&\n"
                "                        ZrLibrary_AotRuntime_SyncUnsignedIntLocal(state, &frame, %u, &zr_aot_u%u)",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncF64Local) {
        fprintf(file,
                " &&\n"
                "                        ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, %u, &zr_aot_f%u)",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    fprintf(file, ");\n    }\n");
}

static void backend_aot_write_c_known_member_call(FILE *file,
                                                  const SZrAotExecIrFunction *functionIr,
                                                  TZrUInt32 destinationSlot,
                                                  TZrUInt32 cacheIndex,
                                                  TZrUInt32 argumentCount,
                                                  const char *marker) {
    TZrBool syncI64Local;
    TZrBool syncBoolLocal;
    TZrBool syncU64Local;
    TZrBool syncF64Local;

    if (file == ZR_NULL || functionIr == ZR_NULL || marker == ZR_NULL) {
        return;
    }

    syncI64Local = backend_aot_c_scalar_locals_has_i64_slot(functionIr, destinationSlot);
    syncBoolLocal = backend_aot_c_scalar_locals_has_bool_slot(functionIr, destinationSlot);
    syncU64Local = backend_aot_c_scalar_locals_has_u64_slot(functionIr, destinationSlot);
    syncF64Local = backend_aot_c_scalar_locals_has_f64_slot(functionIr, destinationSlot);
    fprintf(file,
            "    {\n"
            "        ZrAotGeneratedDirectCall zr_aot_known_member_direct_call = {0};\n"
            "        /* %s */\n"
            "        /* zr_aot_known_member_call_resolve */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_GetMemberSlot(state, &frame, %u, %u, %u));\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_PrepareDirectCall(state,\n"
            "                                                               &frame,\n"
            "                                                               %u,\n"
            "                                                               %u,\n"
            "                                                               %u,\n"
            "                                                               &zr_aot_known_member_direct_call));\n"
            "        zr_aot_next_instruction = ZR_AOT_RUNTIME_RESUME_FALLTHROUGH;\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_CallPreparedOrGenericWithResume(state,\n"
            "                                                                              &frame,\n"
            "                                                                              &zr_aot_known_member_direct_call,\n"
            "                                                                              %u,\n"
            "                                                                              %u,\n"
            "                                                                              %u,\n"
            "                                                                              1,\n"
            "                                                                              &zr_aot_next_instruction));\n"
            "        if (zr_aot_next_instruction != ZR_AOT_RUNTIME_RESUME_FALLTHROUGH) {\n"
            "            goto zr_aot_fn_%u_dispatch;\n"
            "        }\n"
            "%s",
            marker,
            (unsigned)destinationSlot,
            (unsigned)(destinationSlot + 1u),
            (unsigned)cacheIndex,
            (unsigned)destinationSlot,
            (unsigned)destinationSlot,
            (unsigned)argumentCount,
            (unsigned)destinationSlot,
            (unsigned)destinationSlot,
            (unsigned)argumentCount,
            (unsigned)functionIr->flatIndex,
            syncI64Local || syncBoolLocal || syncU64Local || syncF64Local
                    ? "        /* zr_aot_call_result_sync_compact */\n"
                      "        ZR_AOT_C_GUARD(\n"
                    : "");
    if (syncI64Local) {
        fprintf(file,
                "                        /* zr_aot_known_member_call_sync_i64_local_boundary */\n"
                "                        ZrLibrary_AotRuntime_SyncSignedIntLocal(state, &frame, %u, &zr_aot_s%u) &&\n",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncBoolLocal) {
        fprintf(file,
                "                        /* zr_aot_known_member_call_sync_bool_local_boundary */\n"
                "                        ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, %u, &zr_aot_b%u) &&\n",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncU64Local) {
        fprintf(file,
                "                        /* zr_aot_known_member_call_sync_u64_local_boundary */\n"
                "                        ZrLibrary_AotRuntime_SyncUnsignedIntLocal(state, &frame, %u, &zr_aot_u%u) &&\n",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncF64Local) {
        fprintf(file,
                "                        /* zr_aot_known_member_call_sync_f64_local_boundary */\n"
                "                        ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, %u, &zr_aot_f%u) &&\n",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncI64Local || syncBoolLocal || syncU64Local || syncF64Local) {
        fprintf(file, "                        ZR_TRUE);\n");
    }
    fprintf(file, "    }\n");
}

void backend_aot_write_c_known_native_member_call(FILE *file,
                                                  const SZrAotExecIrFunction *functionIr,
                                                  TZrUInt32 destinationSlot,
                                                  TZrUInt32 cacheIndex,
                                                  TZrUInt32 argumentCount) {
    backend_aot_write_c_known_member_call(file,
                                          functionIr,
                                          destinationSlot,
                                          cacheIndex,
                                          argumentCount,
                                          "zr_aot_known_native_member_call");
}

void backend_aot_write_c_known_vm_member_call(FILE *file,
                                              const SZrAotExecIrFunction *functionIr,
                                              TZrUInt32 destinationSlot,
                                              TZrUInt32 cacheIndex,
                                              TZrUInt32 argumentCount) {
    backend_aot_write_c_known_member_call(file,
                                          functionIr,
                                          destinationSlot,
                                          cacheIndex,
                                          argumentCount,
                                          "zr_aot_known_vm_member_call");
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
            "%s"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_CallDynamicDeoptBridge(state,\n"
            "                                                                     &frame,\n"
            "                                                                     %u,\n"
            "                                                                     %u,\n"
            "                                                                     %u,\n"
            "                                                                     %u,\n"
            "                                                                     \"dynamic call\")",
            (unsigned)deoptId,
            syncI64Local || syncBoolLocal || syncU64Local || syncF64Local
                    ? "        /* zr_aot_call_result_sync_compact */\n"
                    : "",
            (unsigned)destinationSlot,
            (unsigned)functionSlot,
            (unsigned)argumentCount,
            (unsigned)deoptId);
    if (syncI64Local) {
        fprintf(file,
                " &&\n"
                "                        /* zr_aot_direct_dynamic_function_call_sync_i64_local_boundary */\n"
                "                        ZrLibrary_AotRuntime_SyncSignedIntLocal(state, &frame, %u, &zr_aot_s%u)",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncBoolLocal) {
        fprintf(file,
                " &&\n"
                "                        /* zr_aot_direct_dynamic_function_call_sync_bool_local_boundary */\n"
                "                        ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, %u, &zr_aot_b%u)",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncU64Local) {
        fprintf(file,
                " &&\n"
                "                        /* zr_aot_direct_dynamic_function_call_sync_u64_local_boundary */\n"
                "                        ZrLibrary_AotRuntime_SyncUnsignedIntLocal(state, &frame, %u, &zr_aot_u%u)",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncF64Local) {
        fprintf(file,
                " &&\n"
                "                        /* zr_aot_direct_dynamic_function_call_sync_f64_local_boundary */\n"
                "                        ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, %u, &zr_aot_f%u)",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    fprintf(file, ");\n"
                  "    }\n");
}
