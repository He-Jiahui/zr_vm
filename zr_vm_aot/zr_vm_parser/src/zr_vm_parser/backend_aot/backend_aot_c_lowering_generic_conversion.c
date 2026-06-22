#include "backend_aot_c_emitter.h"
#include "backend_aot_c_scalar_locals.h"

/* backend_aot_c_lowering_generic_conversion.c */

static void backend_aot_write_c_generic_conversion_sync_locals(FILE *file,
                                                               const SZrAotExecIrFunction *functionIr,
                                                               TZrUInt32 destinationSlot) {
    TZrBool syncBoolLocal;
    TZrBool syncI64Local;
    TZrBool syncU64Local;
    TZrBool syncF64Local;

    if (file == ZR_NULL) {
        return;
    }

    syncBoolLocal = backend_aot_c_scalar_locals_has_bool_slot(functionIr, destinationSlot);
    syncI64Local = backend_aot_c_scalar_locals_has_i64_slot(functionIr, destinationSlot);
    syncU64Local = backend_aot_c_scalar_locals_has_u64_slot(functionIr, destinationSlot);
    syncF64Local = backend_aot_c_scalar_locals_has_f64_slot(functionIr, destinationSlot);
    if (syncBoolLocal) {
        fprintf(file,
                "        /* zr_aot_convert_generic_sync_bool_local_boundary */\n"
                "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, %u, &zr_aot_b%u));\n",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncI64Local) {
        fprintf(file,
                "        /* zr_aot_convert_generic_sync_i64_local_boundary */\n"
                "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SyncSignedIntLocal(state, &frame, %u, &zr_aot_s%u));\n",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncU64Local) {
        fprintf(file,
                "        /* zr_aot_convert_generic_sync_u64_local_boundary */\n"
                "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SyncUnsignedIntLocal(state, &frame, %u, &zr_aot_u%u));\n",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncF64Local) {
        fprintf(file,
                "        /* zr_aot_convert_generic_sync_f64_local_boundary */\n"
                "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, %u, &zr_aot_f%u));\n",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
}

void backend_aot_write_c_direct_to_bool(FILE *file,
                                        const SZrAotExecIrFunction *functionIr,
                                        TZrUInt32 destinationSlot,
                                        TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_convert_generic_to_bool */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_ConvertGenericToBool(state, &frame, %u, %u));\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
    backend_aot_write_c_generic_conversion_sync_locals(file, functionIr, destinationSlot);
    fprintf(file, "    }\n");
}

void backend_aot_write_c_direct_to_int(FILE *file,
                                       const SZrAotExecIrFunction *functionIr,
                                       TZrUInt32 destinationSlot,
                                       TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_convert_generic_to_int */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_ConvertGenericToInt(state, &frame, %u, %u));\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
    backend_aot_write_c_generic_conversion_sync_locals(file, functionIr, destinationSlot);
    fprintf(file, "    }\n");
}

void backend_aot_write_c_direct_to_uint(FILE *file,
                                        const SZrAotExecIrFunction *functionIr,
                                        TZrUInt32 destinationSlot,
                                        TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_convert_generic_to_uint */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_ConvertGenericToUInt(state, &frame, %u, %u));\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
    backend_aot_write_c_generic_conversion_sync_locals(file, functionIr, destinationSlot);
    fprintf(file, "    }\n");
}

void backend_aot_write_c_direct_to_float(FILE *file,
                                         const SZrAotExecIrFunction *functionIr,
                                         TZrUInt32 destinationSlot,
                                         TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_convert_generic_to_float */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_ConvertGenericToFloat(state, &frame, %u, %u));\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
    backend_aot_write_c_generic_conversion_sync_locals(file, functionIr, destinationSlot);
    fprintf(file, "    }\n");
}
