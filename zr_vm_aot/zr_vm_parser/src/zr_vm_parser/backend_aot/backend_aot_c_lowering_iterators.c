#include "backend_aot_c_emitter.h"

void backend_aot_write_c_direct_iter_init(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 iterableSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_iter_init */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_IterInit(state, &frame, %u, %u));\n"
            "    } while (0);\n",
            (unsigned)destinationSlot,
            (unsigned)iterableSlot);
}

void backend_aot_write_c_direct_iter_move_next(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 iteratorSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_iter_move_next */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_IterMoveNext(state, &frame, %u, %u));\n"
            "    } while (0);\n",
            (unsigned)destinationSlot,
            (unsigned)iteratorSlot);
}

void backend_aot_write_c_direct_iter_current(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 iteratorSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_iter_current */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_IterCurrent(state, &frame, %u, %u));\n"
            "    } while (0);\n",
            (unsigned)destinationSlot,
            (unsigned)iteratorSlot);
}

void backend_aot_write_c_direct_iter_move_next_jump_if_false(FILE *file,
                                                             TZrUInt32 functionIndex,
                                                             TZrUInt32 destinationSlot,
                                                             TZrUInt32 iteratorSlot,
                                                             TZrUInt32 targetInstructionIndex,
                                                             TZrBool isBackEdge) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_iter_move_next_jump_if_false */\n"
            "        TZrBool zr_aot_branch_taken = ZR_FALSE;\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_IterMoveNextJumpIfFalse(state, &frame, %u, %u, &zr_aot_branch_taken));\n"
            "        if (zr_aot_branch_taken) {\n",
            (unsigned)destinationSlot,
            (unsigned)iteratorSlot);
    if (isBackEdge) {
        backend_aot_write_c_gc_safepoint(file, "            ", "zr_aot_gc_safepoint_back_edge");
    }
    fprintf(file,
            "            goto zr_aot_fn_%u_ins_%u;\n"
            "        }\n"
            "    } while (0);\n",
            (unsigned)functionIndex,
            (unsigned)targetInstructionIndex);
}
