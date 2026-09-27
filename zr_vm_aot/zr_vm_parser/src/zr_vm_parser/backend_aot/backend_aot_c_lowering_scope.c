#include "backend_aot_c_emitter.h"

/* 作用域退出资源先登记到 runtime 清理链，异常和正常离开路径才能共享关闭顺序。 */
void backend_aot_write_c_direct_mark_to_be_closed(FILE *file, TZrUInt32 slotIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_scope_mark_to_be_closed */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_MarkToBeClosed(state, &frame, %u));\n"
            "    }\n",
            (unsigned)slotIndex);
}

/* CLOSE_SCOPE 使用字节码记录的清理数，实际关闭过程仍由 runtime 管理异常与所有权。 */
void backend_aot_write_c_direct_close_scope(FILE *file, TZrUInt32 cleanupCount) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_scope_close_scope */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_CloseScope(state, &frame, %u));\n"
            "    }\n",
            (unsigned)cleanupCount);
}
