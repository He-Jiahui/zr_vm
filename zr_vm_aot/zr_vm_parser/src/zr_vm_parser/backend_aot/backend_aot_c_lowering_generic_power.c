#include "backend_aot_c_emitter.h"
#include "backend_aot_c_scalar_locals.h"

/* POW 的泛型形式允许非数字操作数；此入口必须经运行时处理元方法分派。 */

/* BUG: 有 ZR_META_POW 的值可由 POW opcode 到达 GenericPower，但现役 runtime
 * 对元方法一律报 unsupported；解释器会调用该元方法，AOT 结果不一致。 */
void backend_aot_write_c_direct_pow(FILE *file,
                                    const SZrAotExecIrFunction *functionIr,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 leftSlot,
                                    TZrUInt32 rightSlot) {
    TZrBool syncI64Local;
    TZrBool syncU64Local;
    TZrBool syncF64Local;

    if (file == ZR_NULL) {
        return;
    }

    syncI64Local = backend_aot_c_scalar_locals_has_i64_slot(functionIr, destinationSlot);
    syncU64Local = backend_aot_c_scalar_locals_has_u64_slot(functionIr, destinationSlot);
    syncF64Local = backend_aot_c_scalar_locals_has_f64_slot(functionIr, destinationSlot);
    fprintf(file,
            "    {\n"
            "        /* zr_aot_generic_power_boundary */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_GenericPower(state, &frame, %u, %u, %u));\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
    if (syncI64Local) {
        fprintf(file,
                "        /* zr_aot_generic_power_sync_i64_local_boundary */\n"
                "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SyncSignedIntLocal(state, &frame, %u, &zr_aot_s%u));\n",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncU64Local) {
        fprintf(file,
                "        /* zr_aot_generic_power_sync_u64_local_boundary */\n"
                "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SyncUnsignedIntLocal(state, &frame, %u, &zr_aot_u%u));\n",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncF64Local) {
        fprintf(file,
                "        /* zr_aot_generic_power_sync_f64_local_boundary */\n"
                "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, %u, &zr_aot_f%u));\n",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    fprintf(file, "    }\n");
}
