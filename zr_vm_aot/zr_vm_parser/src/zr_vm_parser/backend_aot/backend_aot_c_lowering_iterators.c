#include "backend_aot_c_emitter.h"

/* 字节码迭代协议统一经 runtime 执行，使动态迭代器和普通迭代器共享帧槽语义。 */
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

/* 结果留在帧槽，后续 ITER_CURRENT 或分支继续沿用同一个迭代器状态。 */
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

/* 当前项读取交给 runtime，以保留迭代器类型与异常语义。 */
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

/* 复合迭代分支先由 runtime 写入布尔结果，再依分支结果跳转；回边必须保留 GC 安全点。 */
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
