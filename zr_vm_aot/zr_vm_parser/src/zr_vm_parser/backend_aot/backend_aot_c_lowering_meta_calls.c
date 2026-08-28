#include "backend_aot_c_emitter.h"
#include "backend_aot_c_scalar_locals.h"

void backend_aot_write_c_meta_call(FILE *file,
                                   const SZrAotExecIrFunction *functionIr,
                                   TZrUInt32 destinationSlot,
                                   TZrUInt32 receiverSlot,
                                   TZrUInt32 argumentCount) {
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
            "        ZrAotGeneratedDirectCall zr_aot_meta_direct_call = {0};\n"
            "        /* zr_aot_meta_call_prepare_and_dispatch */\n"
            "%s"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_PrepareMetaCall(state,\n"
            "                                                               &frame,\n"
            "                                                               %u,\n"
            "                                                               %u,\n"
            "                                                               %u,\n"
            "                                                               &zr_aot_meta_direct_call) &&\n"
            "                        ZrLibrary_AotRuntime_CallPreparedOrGeneric(state,\n"
            "                                                                    &frame,\n"
            "                                                                    &zr_aot_meta_direct_call,\n"
            "                                                                    %u,\n"
            "                                                                    %u,\n"
            "                                                                    %u,\n"
            "                                                                    1)",
            syncI64Local || syncBoolLocal || syncU64Local || syncF64Local
                    ? "        /* zr_aot_call_result_sync_compact */\n"
                    : "",
            (unsigned)destinationSlot,
            (unsigned)receiverSlot,
            (unsigned)argumentCount,
            (unsigned)destinationSlot,
            (unsigned)receiverSlot,
            (unsigned)(argumentCount + 1u));
    if (syncI64Local) {
        fprintf(file,
                " &&\n"
                "                        /* zr_aot_meta_call_sync_i64_local_boundary */\n"
                "                        ZrLibrary_AotRuntime_SyncSignedIntLocal(state, &frame, %u, &zr_aot_s%u)",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncBoolLocal) {
        fprintf(file,
                " &&\n"
                "                        /* zr_aot_meta_call_sync_bool_local_boundary */\n"
                "                        ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, %u, &zr_aot_b%u)",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncU64Local) {
        fprintf(file,
                " &&\n"
                "                        /* zr_aot_meta_call_sync_u64_local_boundary */\n"
                "                        ZrLibrary_AotRuntime_SyncUnsignedIntLocal(state, &frame, %u, &zr_aot_u%u)",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncF64Local) {
        fprintf(file,
                " &&\n"
                "                        /* zr_aot_meta_call_sync_f64_local_boundary */\n"
                "                        ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, %u, &zr_aot_f%u)",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    fprintf(file, ");\n"
                  "    }\n");
}
