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

/* 隐藏高槽登记源槽的关闭代理，登记时不搬走已有局部变量；
 * 关闭时由 runtime 按资源种类处理源槽和物理镜像，需清理的值在回调前消费，
 * 无清理职责的普通值保持可读，借用值只释放视图。 */
void backend_aot_write_c_direct_mark_close_proxy(FILE *file,
                                                TZrUInt32 proxySlot,
                                                TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }
    fprintf(file,
            "    {\n"
            "        /* zr_aot_scope_mark_close_proxy */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_MarkCloseProxy(state, &frame, %u, %u));\n"
            "    }\n",
            (unsigned)proxySlot,
            (unsigned)sourceSlot);
}

/* CLOSE_SCOPE 使用字节码记录的清理数，实际关闭过程仍由 runtime 管理异常与所有权。 */
/* TODO: runtime 目前未核对实际关闭数；需沿 scope 注册及提前退出路径
 * 验证 cleanupCount 与活动登记项一致，不能把 guard 成功视为清理数量已获验证。 */
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
