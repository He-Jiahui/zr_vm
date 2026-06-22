#include "backend_aot_c_emitter.h"
#include "backend_aot_c_scalar_locals.h"

/* backend_aot_c_lowering_generic_numeric_arithmetic.c */

static void backend_aot_write_c_generic_numeric_sync_locals(FILE *file,
                                                            const SZrAotExecIrFunction *functionIr,
                                                            TZrUInt32 destinationSlot) {
    TZrBool syncI64;
    TZrBool syncU64;
    TZrBool syncF64;

    if (file == ZR_NULL) {
        return;
    }

    syncI64 = backend_aot_c_scalar_locals_has_i64_slot(functionIr, destinationSlot);
    syncU64 = backend_aot_c_scalar_locals_has_u64_slot(functionIr, destinationSlot);
    syncF64 = backend_aot_c_scalar_locals_has_f64_slot(functionIr, destinationSlot);

    if (syncI64) {
        fprintf(file,
                "        /* zr_aot_generic_numeric_sync_i64_local_boundary */\n"
                "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SyncSignedIntLocal(state, &frame, %u, &zr_aot_s%u));\n",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncU64) {
        fprintf(file,
                "        /* zr_aot_generic_numeric_sync_u64_local_boundary */\n"
                "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SyncUnsignedIntLocal(state, &frame, %u, &zr_aot_u%u));\n",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncF64) {
        fprintf(file,
                "        /* zr_aot_generic_numeric_sync_f64_local_boundary */\n"
                "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, %u, &zr_aot_f%u));\n",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
}

static void backend_aot_write_c_generic_numeric_binary_boundary(FILE *file,
                                                                const SZrAotExecIrFunction *functionIr,
                                                                const char *runtimeHelper,
                                                                TZrUInt32 destinationSlot,
                                                                TZrUInt32 leftSlot,
                                                                TZrUInt32 rightSlot) {
    if (file == ZR_NULL || runtimeHelper == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_arith_exec_generic_numeric_binary_boundary */\n"
            "        ZR_AOT_C_GUARD(%s(state, &frame, %u, %u, %u));\n",
            runtimeHelper,
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
    backend_aot_write_c_generic_numeric_sync_locals(file, functionIr, destinationSlot);
    fprintf(file, "    }\n");
}

static void backend_aot_write_c_generic_numeric_unary_boundary(FILE *file,
                                                               const SZrAotExecIrFunction *functionIr,
                                                               const char *runtimeHelper,
                                                               TZrUInt32 destinationSlot,
                                                               TZrUInt32 sourceSlot) {
    if (file == ZR_NULL || runtimeHelper == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_arith_exec_generic_numeric_unary_boundary */\n"
            "        ZR_AOT_C_GUARD(%s(state, &frame, %u, %u));\n",
            runtimeHelper,
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
    backend_aot_write_c_generic_numeric_sync_locals(file, functionIr, destinationSlot);
    fprintf(file, "    }\n");
}

void backend_aot_write_c_direct_add(FILE *file,
                                    const SZrAotExecIrFunction *functionIr,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 leftSlot,
                                    TZrUInt32 rightSlot) {
    backend_aot_write_c_generic_numeric_binary_boundary(file,
                                                        functionIr,
                                                        "ZrLibrary_AotRuntime_GenericNumericAdd",
                                                        destinationSlot,
                                                        leftSlot,
                                                        rightSlot);
}

void backend_aot_write_c_direct_sub(FILE *file,
                                    const SZrAotExecIrFunction *functionIr,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 leftSlot,
                                    TZrUInt32 rightSlot) {
    backend_aot_write_c_generic_numeric_binary_boundary(file,
                                                        functionIr,
                                                        "ZrLibrary_AotRuntime_GenericNumericSub",
                                                        destinationSlot,
                                                        leftSlot,
                                                        rightSlot);
}

void backend_aot_write_c_direct_mul(FILE *file,
                                    const SZrAotExecIrFunction *functionIr,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 leftSlot,
                                    TZrUInt32 rightSlot) {
    backend_aot_write_c_generic_numeric_binary_boundary(file,
                                                        functionIr,
                                                        "ZrLibrary_AotRuntime_GenericNumericMul",
                                                        destinationSlot,
                                                        leftSlot,
                                                        rightSlot);
}

void backend_aot_write_c_direct_div(FILE *file,
                                    const SZrAotExecIrFunction *functionIr,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 leftSlot,
                                    TZrUInt32 rightSlot) {
    backend_aot_write_c_generic_numeric_binary_boundary(file,
                                                        functionIr,
                                                        "ZrLibrary_AotRuntime_GenericNumericDiv",
                                                        destinationSlot,
                                                        leftSlot,
                                                        rightSlot);
}

void backend_aot_write_c_direct_mod(FILE *file,
                                    const SZrAotExecIrFunction *functionIr,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 leftSlot,
                                    TZrUInt32 rightSlot) {
    backend_aot_write_c_generic_numeric_binary_boundary(file,
                                                        functionIr,
                                                        "ZrLibrary_AotRuntime_GenericNumericMod",
                                                        destinationSlot,
                                                        leftSlot,
                                                        rightSlot);
}

void backend_aot_write_c_direct_neg(FILE *file,
                                    const SZrAotExecIrFunction *functionIr,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 sourceSlot) {
    backend_aot_write_c_generic_numeric_unary_boundary(file,
                                                       functionIr,
                                                       "ZrLibrary_AotRuntime_GenericNumericNeg",
                                                       destinationSlot,
                                                       sourceSlot);
}
