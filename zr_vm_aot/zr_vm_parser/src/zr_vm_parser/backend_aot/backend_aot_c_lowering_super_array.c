#include "backend_aot_c_emitter.h"

/* 超级数组的 items 视图由 runtime 绑定，后续 *_ITEMS 指令必须使用绑定结果槽。 */
void backend_aot_write_c_direct_super_array_bind_items(FILE *file,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 receiverSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_super_array_bind_items */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SuperArrayBindItems(state, &frame, %u, %u));\n"
            "    } while (0);\n",
            (unsigned)destinationSlot,
            (unsigned)receiverSlot);
}

/* 绑定 items 视图后的整数索引读写仍走 runtime，以保留边界检查和帧槽所有权。
 * GET/SET_INT 的普通 receiver 版本则由字节码分派器按操作码选择。
 */
void backend_aot_write_c_direct_super_array_get_int_bound_items(FILE *file,
                                                                TZrUInt32 destinationSlot,
                                                                TZrUInt32 itemsSlot,
                                                                TZrUInt32 keySlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_super_array_get_int_bound_items */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SuperArrayGetIntBoundItems(state, &frame, %u, %u, %u));\n"
            "    } while (0);\n",
            (unsigned)destinationSlot,
            (unsigned)itemsSlot,
            (unsigned)keySlot);
}

void backend_aot_write_c_direct_super_array_set_int_bound_items(FILE *file,
                                                                TZrUInt32 sourceSlot,
                                                                TZrUInt32 itemsSlot,
                                                                TZrUInt32 keySlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_super_array_set_int_bound_items */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SuperArraySetIntBoundItems(state, &frame, %u, %u, %u));\n"
            "    } while (0);\n",
            (unsigned)sourceSlot,
            (unsigned)itemsSlot,
            (unsigned)keySlot);
}

/* 普通 receiver 索引访问保留 runtime 的类型验证；PLAIN_DEST 复用此入口。 */
void backend_aot_write_c_direct_super_array_get_int(FILE *file,
                                                    TZrUInt32 destinationSlot,
                                                    TZrUInt32 receiverSlot,
                                                    TZrUInt32 keySlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_super_array_get_int */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SuperArrayGetInt(state, &frame, %u, %u, %u));\n"
            "    } while (0);\n",
            (unsigned)destinationSlot,
            (unsigned)receiverSlot,
            (unsigned)keySlot);
}

void backend_aot_write_c_direct_super_array_set_int(FILE *file,
                                                    TZrUInt32 sourceSlot,
                                                    TZrUInt32 receiverSlot,
                                                    TZrUInt32 keySlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_super_array_set_int */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SuperArraySetInt(state, &frame, %u, %u, %u));\n"
            "    } while (0);\n",
            (unsigned)sourceSlot,
            (unsigned)receiverSlot,
            (unsigned)keySlot);
}

/* 分派器只对当前 receiver 槽做反向线性扫描：遇到新建 owner 且其间没有
 * 未识别的槽使用、调用或控制流边界时，才选择免写屏障路径。
 * TODO: 局部扫描尚未覆盖绑定 items 别名；需对照 runtime 的相同疑点补 GC 压测。
 */
void backend_aot_write_c_direct_super_array_set_int_new_owner_no_write_barrier(FILE *file,
                                                                               TZrUInt32 sourceSlot,
                                                                               TZrUInt32 receiverSlot,
                                                                               TZrUInt32 keySlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_super_array_set_int_new_owner_no_write_barrier */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SuperArraySetIntNewOwnerNoWriteBarrier(state, &frame, %u, %u, %u));\n"
            "    } while (0);\n",
            (unsigned)sourceSlot,
            (unsigned)receiverSlot,
            (unsigned)keySlot);
}

/* 增量和四槽批量操作由 runtime 维护数组长度及类型不变量。 */
void backend_aot_write_c_direct_super_array_add_int(FILE *file,
                                                    TZrUInt32 destinationSlot,
                                                    TZrUInt32 receiverSlot,
                                                    TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_super_array_add_int */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SuperArrayAddInt(state, &frame, %u, %u, %u));\n"
            "    } while (0);\n",
            (unsigned)destinationSlot,
            (unsigned)receiverSlot,
            (unsigned)sourceSlot);
}

void backend_aot_write_c_direct_super_array_add_int4(FILE *file,
                                                     TZrUInt32 receiverBaseSlot,
                                                     TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_super_array_add_int4 */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SuperArrayAddInt4(state, &frame, %u, %u));\n"
            "    } while (0);\n",
            (unsigned)receiverBaseSlot,
            (unsigned)sourceSlot);
}

/* 常量索引指向当前函数的常量表；生成代码须随该函数元数据一起装载。 */
void backend_aot_write_c_direct_super_array_add_int4_const(FILE *file,
                                                           const SZrFunction *function,
                                                           TZrUInt32 receiverBaseSlot,
                                                           TZrUInt32 constantIndex) {
    ZR_UNUSED_PARAMETER(function);

    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_super_array_add_int4 */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SuperArrayAddInt4Const(state, &frame, %u, %u));\n"
            "    } while (0);\n",
            (unsigned)receiverBaseSlot,
            (unsigned)constantIndex);
}

/* 批量填充共用常量表与帧槽协议，不能把常量索引解释成即时值。 */
void backend_aot_write_c_direct_super_array_fill_int4_const(FILE *file,
                                                            const SZrFunction *function,
                                                            TZrUInt32 receiverBaseSlot,
                                                            TZrUInt32 countSlot,
                                                            TZrUInt32 constantIndex) {
    ZR_UNUSED_PARAMETER(function);

    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_super_array_fill_int4_const */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SuperArrayFillInt4Const(state, &frame, %u, %u, %u));\n"
            "    } while (0);\n",
            (unsigned)receiverBaseSlot,
            (unsigned)countSlot,
            (unsigned)constantIndex);
}
