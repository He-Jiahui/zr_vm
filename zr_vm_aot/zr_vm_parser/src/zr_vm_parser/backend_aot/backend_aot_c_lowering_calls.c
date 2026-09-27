#include "backend_aot_c_emitter.h"
#include "backend_aot_c_scalar_locals.h"
#include "backend_aot_internal.h"

/* 目标函数元数据不再满足 typed direct 条件时，从原始函数槽走通用调用并恢复 i64 镜像。 */
static void backend_aot_write_c_static_direct_i64_deopt_fallback(FILE *file,
                                                                 TZrUInt32 destinationSlot,
                                                                 TZrUInt32 functionSlot,
                                                                 TZrUInt32 argumentCount,
                                                                 TZrUInt32 calleeFlatIndex,
                                                                 const char *label) {
    if (file == ZR_NULL || label == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "        } else {\n"
            "            /* zr_aot_static_typed_direct_call_deopt_sync_compact */\n"
            "            ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_DeoptTypedDirectCall(state,\n"
            "                                                                        &frame,\n"
            "                                                                        %u,\n"
            "                                                                        %u,\n"
            "                                                                        %u,\n"
            "                                                                        %u,\n"
            "                                                                        \"%s\") &&\n"
            "                            ZrLibrary_AotRuntime_SyncSignedIntLocal(state, &frame, %u, &zr_aot_s%u));\n"
            "        }\n",
            (unsigned)destinationSlot,
            (unsigned)functionSlot,
            (unsigned)argumentCount,
            (unsigned)calleeFlatIndex,
            label,
            (unsigned)destinationSlot,
            (unsigned)destinationSlot);
}

/* u64 路径的去优化也必须恢复生成 C 的局部镜像，后续算术不能读取旧值。 */
static void backend_aot_write_c_static_direct_u64_deopt_fallback(FILE *file,
                                                                 TZrUInt32 destinationSlot,
                                                                 TZrUInt32 functionSlot,
                                                                 TZrUInt32 argumentCount,
                                                                 TZrUInt32 calleeFlatIndex,
                                                                 const char *label) {
    if (file == ZR_NULL || label == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "        } else {\n"
            "            /* zr_aot_static_typed_direct_call_deopt_sync_compact */\n"
            "            ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_DeoptTypedDirectCall(state,\n"
            "                                                                        &frame,\n"
            "                                                                        %u,\n"
            "                                                                        %u,\n"
            "                                                                        %u,\n"
            "                                                                        %u,\n"
            "                                                                        \"%s\") &&\n"
            "                            ZrLibrary_AotRuntime_SyncUnsignedIntLocal(state, &frame, %u, &zr_aot_u%u));\n"
            "        }\n",
            (unsigned)destinationSlot,
            (unsigned)functionSlot,
            (unsigned)argumentCount,
            (unsigned)calleeFlatIndex,
            label,
            (unsigned)destinationSlot,
            (unsigned)destinationSlot);
}

/* f64 路径回退时由 runtime 执行真实调用，再把值槽结果同步到标量局部量。 */
static void backend_aot_write_c_static_direct_f64_deopt_fallback(FILE *file,
                                                                 TZrUInt32 destinationSlot,
                                                                 TZrUInt32 functionSlot,
                                                                 TZrUInt32 argumentCount,
                                                                 TZrUInt32 calleeFlatIndex,
                                                                 const char *label) {
    if (file == ZR_NULL || label == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "        } else {\n"
            "            /* zr_aot_static_typed_direct_call_deopt_sync_compact */\n"
            "            ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_DeoptTypedDirectCall(state,\n"
            "                                                                        &frame,\n"
            "                                                                        %u,\n"
            "                                                                        %u,\n"
            "                                                                        %u,\n"
            "                                                                        %u,\n"
            "                                                                        \"%s\") &&\n"
            "                            ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, %u, &zr_aot_f%u));\n"
            "        }\n",
            (unsigned)destinationSlot,
            (unsigned)functionSlot,
            (unsigned)argumentCount,
            (unsigned)calleeFlatIndex,
            label,
            (unsigned)destinationSlot,
            (unsigned)destinationSlot);
}

/* typed direct 调用族由选择器保证 calleeFlatIndex、参数镜像及 thunk 形状匹配；
 * 每次调用仍做元数据守卫。syncStackSlot 仅在后续消费者需要值槽时为真。
 */
void backend_aot_write_c_static_direct_i64_no_arg_function_call(FILE *file,
                                                                TZrUInt32 destinationSlot,
                                                                TZrUInt32 functionSlot,
                                                                TZrUInt32 calleeFlatIndex,
                                                                TZrBool syncStackSlot) {
    if (file == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_static_i64_no_arg_direct_call */\n"
            "        /* zr_aot_static_i64_no_arg_direct_call_metadata_guard */\n"
            "        if (ZrLibrary_AotRuntime_CanUseTypedDirectCall(state, &frame, %u)) {\n",
            (unsigned)calleeFlatIndex);
    if (syncStackSlot) {
        fprintf(file,
            "            SZrTypeValue *zr_aot_typed_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "            if (zr_aot_typed_destination == ZR_NULL) {\n"
            "                ZR_AOT_C_FAIL();\n"
            "            }\n",
            (unsigned)destinationSlot);
    }
    fprintf(file,
            "            zr_aot_s%u = zr_aot_typed_i64_fn_%u();\n",
            (unsigned)destinationSlot,
            (unsigned)calleeFlatIndex);
    if (syncStackSlot) {
        fprintf(file,
            "            /* zr_aot_static_i64_no_arg_direct_call_sync_stack_slot */\n"
            "            ZR_VALUE_FAST_SET(zr_aot_typed_destination,\n"
            "                              nativeInt64,\n"
            "                              zr_aot_s%u,\n"
            "                              ZR_VALUE_TYPE_INT64);\n",
            (unsigned)destinationSlot);
    }
    backend_aot_write_c_static_direct_i64_deopt_fallback(file,
                                                         destinationSlot,
                                                         functionSlot,
                                                         0u,
                                                         calleeFlatIndex,
                                                         "typed i64 direct call");
    fprintf(file, "    }\n");
}

/* 单参数 i64 thunk 使用已有的标量参数镜像，守卫失败时回退到原函数槽。 */
void backend_aot_write_c_static_direct_i64_one_arg_function_call(FILE *file,
                                                                 TZrUInt32 destinationSlot,
                                                                 TZrUInt32 functionSlot,
                                                                 TZrUInt32 calleeFlatIndex,
                                                                 TZrUInt32 argumentSlot,
                                                                 TZrBool syncStackSlot) {
    if (file == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_static_i64_one_arg_direct_call */\n"
            "        /* zr_aot_static_i64_one_arg_direct_call_metadata_guard */\n"
            "        if (ZrLibrary_AotRuntime_CanUseTypedDirectCall(state, &frame, %u)) {\n",
            (unsigned)calleeFlatIndex);
    if (syncStackSlot) {
        fprintf(file,
            "            SZrTypeValue *zr_aot_typed_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "            if (zr_aot_typed_destination == ZR_NULL) {\n"
            "                ZR_AOT_C_FAIL();\n"
            "            }\n",
            (unsigned)destinationSlot);
    }
    fprintf(file,
            "            zr_aot_s%u = zr_aot_typed_i64_fn_%u(zr_aot_s%u);\n",
            (unsigned)destinationSlot,
            (unsigned)calleeFlatIndex,
            (unsigned)argumentSlot);
    if (syncStackSlot) {
        fprintf(file,
            "            /* zr_aot_static_i64_one_arg_direct_call_sync_stack_slot */\n"
            "            ZR_VALUE_FAST_SET(zr_aot_typed_destination,\n"
            "                              nativeInt64,\n"
            "                              zr_aot_s%u,\n"
            "                              ZR_VALUE_TYPE_INT64);\n",
            (unsigned)destinationSlot);
    }
    backend_aot_write_c_static_direct_i64_deopt_fallback(file,
                                                         destinationSlot,
                                                         functionSlot,
                                                         1u,
                                                         calleeFlatIndex,
                                                         "typed i64 direct call");
    fprintf(file, "    }\n");
}

/* 双参数 i64 thunk 是否接收 state 取决于选定的 ABI 形状，不能只按参数个数推断。 */
void backend_aot_write_c_static_direct_i64_two_arg_function_call(FILE *file,
                                                                 TZrUInt32 destinationSlot,
                                                                 TZrUInt32 functionSlot,
                                                                 TZrUInt32 calleeFlatIndex,
                                                                 TZrUInt32 firstArgumentSlot,
                                                                 TZrUInt32 secondArgumentSlot,
                                                                 TZrBool syncStackSlot,
                                                                 TZrBool passStateToThunk) {
    if (file == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_static_i64_two_arg_direct_call */\n"
            "        /* zr_aot_static_i64_two_arg_direct_call_metadata_guard */\n"
            "        if (ZrLibrary_AotRuntime_CanUseTypedDirectCall(state, &frame, %u)) {\n",
            (unsigned)calleeFlatIndex);
    if (syncStackSlot) {
        fprintf(file,
            "            SZrTypeValue *zr_aot_typed_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "            if (zr_aot_typed_destination == ZR_NULL) {\n"
            "                ZR_AOT_C_FAIL();\n"
            "            }\n",
            (unsigned)destinationSlot);
    }
    if (passStateToThunk) {
        fprintf(file,
                "            zr_aot_s%u = zr_aot_typed_i64_fn_%u(state, zr_aot_s%u, zr_aot_s%u);\n",
                (unsigned)destinationSlot,
                (unsigned)calleeFlatIndex,
                (unsigned)firstArgumentSlot,
                (unsigned)secondArgumentSlot);
    } else {
        fprintf(file,
                "            zr_aot_s%u = zr_aot_typed_i64_fn_%u(zr_aot_s%u, zr_aot_s%u);\n",
                (unsigned)destinationSlot,
                (unsigned)calleeFlatIndex,
                (unsigned)firstArgumentSlot,
                (unsigned)secondArgumentSlot);
    }
    if (syncStackSlot) {
        fprintf(file,
            "            /* zr_aot_static_i64_two_arg_direct_call_sync_stack_slot */\n"
            "            ZR_VALUE_FAST_SET(zr_aot_typed_destination,\n"
            "                              nativeInt64,\n"
            "                              zr_aot_s%u,\n"
            "                              ZR_VALUE_TYPE_INT64);\n",
            (unsigned)destinationSlot);
    }
    backend_aot_write_c_static_direct_i64_deopt_fallback(file,
                                                         destinationSlot,
                                                         functionSlot,
                                                         2u,
                                                         calleeFlatIndex,
                                                         "typed i64 direct call");
    fprintf(file, "    }\n");
}

/* 三参数 i64 入口与双参数入口共享元数据守卫、可选 state 和去优化契约。 */
void backend_aot_write_c_static_direct_i64_three_arg_function_call(FILE *file,
                                                                   TZrUInt32 destinationSlot,
                                                                   TZrUInt32 functionSlot,
                                                                   TZrUInt32 calleeFlatIndex,
                                                                   TZrUInt32 firstArgumentSlot,
                                                                   TZrUInt32 secondArgumentSlot,
                                                                   TZrUInt32 thirdArgumentSlot,
                                                                   TZrBool syncStackSlot,
                                                                   TZrBool passStateToThunk) {
    if (file == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_static_i64_three_arg_direct_call */\n"
            "        /* zr_aot_static_i64_three_arg_direct_call_metadata_guard */\n"
            "        if (ZrLibrary_AotRuntime_CanUseTypedDirectCall(state, &frame, %u)) {\n",
            (unsigned)calleeFlatIndex);
    if (syncStackSlot) {
        fprintf(file,
                "            SZrTypeValue *zr_aot_typed_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
                "            if (zr_aot_typed_destination == ZR_NULL) {\n"
                "                ZR_AOT_C_FAIL();\n"
                "            }\n",
                (unsigned)destinationSlot);
    }
    if (passStateToThunk) {
        fprintf(file,
                "            zr_aot_s%u = zr_aot_typed_i64_fn_%u(state, zr_aot_s%u, zr_aot_s%u, zr_aot_s%u);\n",
                (unsigned)destinationSlot,
                (unsigned)calleeFlatIndex,
                (unsigned)firstArgumentSlot,
                (unsigned)secondArgumentSlot,
                (unsigned)thirdArgumentSlot);
    } else {
        fprintf(file,
                "            zr_aot_s%u = zr_aot_typed_i64_fn_%u(zr_aot_s%u, zr_aot_s%u, zr_aot_s%u);\n",
                (unsigned)destinationSlot,
                (unsigned)calleeFlatIndex,
                (unsigned)firstArgumentSlot,
                (unsigned)secondArgumentSlot,
                (unsigned)thirdArgumentSlot);
    }
    if (syncStackSlot) {
        fprintf(file,
                "            /* zr_aot_static_i64_three_arg_direct_call_sync_stack_slot */\n"
                "            ZR_VALUE_FAST_SET(zr_aot_typed_destination,\n"
                "                              nativeInt64,\n"
                "                              zr_aot_s%u,\n"
                "                              ZR_VALUE_TYPE_INT64);\n",
                (unsigned)destinationSlot);
    }
    backend_aot_write_c_static_direct_i64_deopt_fallback(file,
                                                         destinationSlot,
                                                         functionSlot,
                                                         3u,
                                                         calleeFlatIndex,
                                                         "typed i64 direct call");
    fprintf(file, "    }\n");
}

/* u64 调用族保留无符号结果类型；通用调用回退后仍须同步 u64 镜像。 */
void backend_aot_write_c_static_direct_u64_no_arg_function_call(FILE *file,
                                                                TZrUInt32 destinationSlot,
                                                                TZrUInt32 functionSlot,
                                                                TZrUInt32 calleeFlatIndex,
                                                                TZrBool syncStackSlot) {
    if (file == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_static_u64_no_arg_direct_call */\n"
            "        /* zr_aot_static_u64_no_arg_direct_call_metadata_guard */\n"
            "        if (ZrLibrary_AotRuntime_CanUseTypedDirectCall(state, &frame, %u)) {\n",
            (unsigned)calleeFlatIndex);
    if (syncStackSlot) {
        fprintf(file,
                "            SZrTypeValue *zr_aot_typed_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
                "            if (zr_aot_typed_destination == ZR_NULL) {\n"
                "                ZR_AOT_C_FAIL();\n"
                "            }\n",
                (unsigned)destinationSlot);
    }
    fprintf(file,
            "            zr_aot_u%u = zr_aot_typed_u64_fn_%u();\n",
            (unsigned)destinationSlot,
            (unsigned)calleeFlatIndex);
    if (syncStackSlot) {
        fprintf(file,
                "            /* zr_aot_static_u64_no_arg_direct_call_sync_stack_slot */\n"
                "            ZR_VALUE_FAST_SET(zr_aot_typed_destination,\n"
                "                              nativeUInt64,\n"
                "                              zr_aot_u%u,\n"
                "                              ZR_VALUE_TYPE_UINT64);\n",
                (unsigned)destinationSlot);
    }
    backend_aot_write_c_static_direct_u64_deopt_fallback(file,
                                                         destinationSlot,
                                                         functionSlot,
                                                         0u,
                                                         calleeFlatIndex,
                                                         "typed u64 direct call");
    fprintf(file, "    }\n");
}

/* 单参数 u64 thunk 由 typed-call 选择器在参数镜像已准备好时启用。 */
void backend_aot_write_c_static_direct_u64_one_arg_function_call(FILE *file,
                                                                 TZrUInt32 destinationSlot,
                                                                 TZrUInt32 functionSlot,
                                                                 TZrUInt32 calleeFlatIndex,
                                                                 TZrUInt32 argumentSlot,
                                                                 TZrBool syncStackSlot) {
    if (file == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_static_u64_one_arg_direct_call */\n"
            "        /* zr_aot_static_u64_one_arg_direct_call_metadata_guard */\n"
            "        if (ZrLibrary_AotRuntime_CanUseTypedDirectCall(state, &frame, %u)) {\n",
            (unsigned)calleeFlatIndex);
    if (syncStackSlot) {
        fprintf(file,
                "            SZrTypeValue *zr_aot_typed_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
                "            if (zr_aot_typed_destination == ZR_NULL) {\n"
                "                ZR_AOT_C_FAIL();\n"
                "            }\n",
                (unsigned)destinationSlot);
    }
    fprintf(file,
            "            zr_aot_u%u = zr_aot_typed_u64_fn_%u(zr_aot_u%u);\n",
            (unsigned)destinationSlot,
            (unsigned)calleeFlatIndex,
            (unsigned)argumentSlot);
    if (syncStackSlot) {
        fprintf(file,
                "            /* zr_aot_static_u64_one_arg_direct_call_sync_stack_slot */\n"
                "            ZR_VALUE_FAST_SET(zr_aot_typed_destination,\n"
                "                              nativeUInt64,\n"
                "                              zr_aot_u%u,\n"
                "                              ZR_VALUE_TYPE_UINT64);\n",
                (unsigned)destinationSlot);
    }
    backend_aot_write_c_static_direct_u64_deopt_fallback(file,
                                                         destinationSlot,
                                                         functionSlot,
                                                         1u,
                                                         calleeFlatIndex,
                                                         "typed u64 direct call");
    fprintf(file, "    }\n");
}

/* 双参数 u64 thunk 的 state 参数必须与编译出的签名一致。 */
void backend_aot_write_c_static_direct_u64_two_arg_function_call(FILE *file,
                                                                 TZrUInt32 destinationSlot,
                                                                 TZrUInt32 functionSlot,
                                                                 TZrUInt32 calleeFlatIndex,
                                                                 TZrUInt32 firstArgumentSlot,
                                                                 TZrUInt32 secondArgumentSlot,
                                                                 TZrBool syncStackSlot,
                                                                 TZrBool passStateToThunk) {
    if (file == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_static_u64_two_arg_direct_call */\n"
            "        /* zr_aot_static_u64_two_arg_direct_call_metadata_guard */\n"
            "        if (ZrLibrary_AotRuntime_CanUseTypedDirectCall(state, &frame, %u)) {\n",
            (unsigned)calleeFlatIndex);
    if (syncStackSlot) {
        fprintf(file,
                "            SZrTypeValue *zr_aot_typed_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
                "            if (zr_aot_typed_destination == ZR_NULL) {\n"
                "                ZR_AOT_C_FAIL();\n"
                "            }\n",
                (unsigned)destinationSlot);
    }
    if (passStateToThunk) {
        fprintf(file,
                "            zr_aot_u%u = zr_aot_typed_u64_fn_%u(state, zr_aot_u%u, zr_aot_u%u);\n",
                (unsigned)destinationSlot,
                (unsigned)calleeFlatIndex,
                (unsigned)firstArgumentSlot,
                (unsigned)secondArgumentSlot);
    } else {
        fprintf(file,
                "            zr_aot_u%u = zr_aot_typed_u64_fn_%u(zr_aot_u%u, zr_aot_u%u);\n",
                (unsigned)destinationSlot,
                (unsigned)calleeFlatIndex,
                (unsigned)firstArgumentSlot,
                (unsigned)secondArgumentSlot);
    }
    if (syncStackSlot) {
        fprintf(file,
                "            /* zr_aot_static_u64_two_arg_direct_call_sync_stack_slot */\n"
                "            ZR_VALUE_FAST_SET(zr_aot_typed_destination,\n"
                "                              nativeUInt64,\n"
                "                              zr_aot_u%u,\n"
                "                              ZR_VALUE_TYPE_UINT64);\n",
                (unsigned)destinationSlot);
    }
    backend_aot_write_c_static_direct_u64_deopt_fallback(file,
                                                         destinationSlot,
                                                         functionSlot,
                                                         2u,
                                                         calleeFlatIndex,
                                                         "typed u64 direct call");
    fprintf(file, "    }\n");
}

/* 三参数 u64 入口仅用于选择器认可的静态目标；元数据变化时走去优化。 */
void backend_aot_write_c_static_direct_u64_three_arg_function_call(FILE *file,
                                                                   TZrUInt32 destinationSlot,
                                                                   TZrUInt32 functionSlot,
                                                                   TZrUInt32 calleeFlatIndex,
                                                                   TZrUInt32 firstArgumentSlot,
                                                                   TZrUInt32 secondArgumentSlot,
                                                                   TZrUInt32 thirdArgumentSlot,
                                                                   TZrBool syncStackSlot,
                                                                   TZrBool passStateToThunk) {
    if (file == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_static_u64_three_arg_direct_call */\n"
            "        /* zr_aot_static_u64_three_arg_direct_call_metadata_guard */\n"
            "        if (ZrLibrary_AotRuntime_CanUseTypedDirectCall(state, &frame, %u)) {\n",
            (unsigned)calleeFlatIndex);
    if (syncStackSlot) {
        fprintf(file,
                "            SZrTypeValue *zr_aot_typed_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
                "            if (zr_aot_typed_destination == ZR_NULL) {\n"
                "                ZR_AOT_C_FAIL();\n"
                "            }\n",
                (unsigned)destinationSlot);
    }
    if (passStateToThunk) {
        fprintf(file,
                "            zr_aot_u%u = zr_aot_typed_u64_fn_%u(state, zr_aot_u%u, zr_aot_u%u, zr_aot_u%u);\n",
                (unsigned)destinationSlot,
                (unsigned)calleeFlatIndex,
                (unsigned)firstArgumentSlot,
                (unsigned)secondArgumentSlot,
                (unsigned)thirdArgumentSlot);
    } else {
        fprintf(file,
                "            zr_aot_u%u = zr_aot_typed_u64_fn_%u(zr_aot_u%u, zr_aot_u%u, zr_aot_u%u);\n",
                (unsigned)destinationSlot,
                (unsigned)calleeFlatIndex,
                (unsigned)firstArgumentSlot,
                (unsigned)secondArgumentSlot,
                (unsigned)thirdArgumentSlot);
    }
    if (syncStackSlot) {
        fprintf(file,
                "            /* zr_aot_static_u64_three_arg_direct_call_sync_stack_slot */\n"
                "            ZR_VALUE_FAST_SET(zr_aot_typed_destination,\n"
                "                              nativeUInt64,\n"
                "                              zr_aot_u%u,\n"
                "                              ZR_VALUE_TYPE_UINT64);\n",
                (unsigned)destinationSlot);
    }
    backend_aot_write_c_static_direct_u64_deopt_fallback(file,
                                                         destinationSlot,
                                                         functionSlot,
                                                         3u,
                                                         calleeFlatIndex,
                                                         "typed u64 direct call");
    fprintf(file, "    }\n");
}

/* f64 调用族用 double 局部镜像和 ZR_VALUE_TYPE_DOUBLE 值槽协议。 */
void backend_aot_write_c_static_direct_f64_no_arg_function_call(FILE *file,
                                                                TZrUInt32 destinationSlot,
                                                                TZrUInt32 functionSlot,
                                                                TZrUInt32 calleeFlatIndex,
                                                                TZrBool syncStackSlot) {
    if (file == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_static_f64_no_arg_direct_call */\n"
            "        /* zr_aot_static_f64_no_arg_direct_call_metadata_guard */\n"
            "        if (ZrLibrary_AotRuntime_CanUseTypedDirectCall(state, &frame, %u)) {\n",
            (unsigned)calleeFlatIndex);
    if (syncStackSlot) {
        fprintf(file,
                "            SZrTypeValue *zr_aot_typed_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
                "            if (zr_aot_typed_destination == ZR_NULL) {\n"
                "                ZR_AOT_C_FAIL();\n"
                "            }\n",
                (unsigned)destinationSlot);
    }
    fprintf(file,
            "            zr_aot_f%u = zr_aot_typed_f64_fn_%u();\n",
            (unsigned)destinationSlot,
            (unsigned)calleeFlatIndex);
    if (syncStackSlot) {
        fprintf(file,
                "            /* zr_aot_static_f64_no_arg_direct_call_sync_stack_slot */\n"
                "            ZR_VALUE_FAST_SET(zr_aot_typed_destination,\n"
                "                              nativeDouble,\n"
                "                              zr_aot_f%u,\n"
                "                              ZR_VALUE_TYPE_DOUBLE);\n",
                (unsigned)destinationSlot);
    }
    backend_aot_write_c_static_direct_f64_deopt_fallback(file,
                                                         destinationSlot,
                                                         functionSlot,
                                                         0u,
                                                         calleeFlatIndex,
                                                         "typed f64 direct call");
    fprintf(file, "    }\n");
}

/* 单参数 f64 thunk 的参数与返回值都沿标量局部量流动。 */
void backend_aot_write_c_static_direct_f64_one_arg_function_call(FILE *file,
                                                                 TZrUInt32 destinationSlot,
                                                                 TZrUInt32 functionSlot,
                                                                 TZrUInt32 calleeFlatIndex,
                                                                 TZrUInt32 argumentSlot,
                                                                 TZrBool syncStackSlot) {
    if (file == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_static_f64_one_arg_direct_call */\n"
            "        /* zr_aot_static_f64_one_arg_direct_call_metadata_guard */\n"
            "        if (ZrLibrary_AotRuntime_CanUseTypedDirectCall(state, &frame, %u)) {\n",
            (unsigned)calleeFlatIndex);
    if (syncStackSlot) {
        fprintf(file,
                "            SZrTypeValue *zr_aot_typed_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
                "            if (zr_aot_typed_destination == ZR_NULL) {\n"
                "                ZR_AOT_C_FAIL();\n"
                "            }\n",
                (unsigned)destinationSlot);
    }
    fprintf(file,
            "            zr_aot_f%u = zr_aot_typed_f64_fn_%u(zr_aot_f%u);\n",
            (unsigned)destinationSlot,
            (unsigned)calleeFlatIndex,
            (unsigned)argumentSlot);
    if (syncStackSlot) {
        fprintf(file,
                "            /* zr_aot_static_f64_one_arg_direct_call_sync_stack_slot */\n"
                "            ZR_VALUE_FAST_SET(zr_aot_typed_destination,\n"
                "                              nativeDouble,\n"
                "                              zr_aot_f%u,\n"
                "                              ZR_VALUE_TYPE_DOUBLE);\n",
                (unsigned)destinationSlot);
    }
    backend_aot_write_c_static_direct_f64_deopt_fallback(file,
                                                         destinationSlot,
                                                         functionSlot,
                                                         1u,
                                                         calleeFlatIndex,
                                                         "typed f64 direct call");
    fprintf(file, "    }\n");
}

/* 双参数 f64 thunk 仅在选择器确认 ABI 时选择带 state 的签名。 */
void backend_aot_write_c_static_direct_f64_two_arg_function_call(FILE *file,
                                                                 TZrUInt32 destinationSlot,
                                                                 TZrUInt32 functionSlot,
                                                                 TZrUInt32 calleeFlatIndex,
                                                                 TZrUInt32 firstArgumentSlot,
                                                                 TZrUInt32 secondArgumentSlot,
                                                                 TZrBool syncStackSlot,
                                                                 TZrBool passStateToThunk) {
    if (file == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_static_f64_two_arg_direct_call */\n"
            "        /* zr_aot_static_f64_two_arg_direct_call_metadata_guard */\n"
            "        if (ZrLibrary_AotRuntime_CanUseTypedDirectCall(state, &frame, %u)) {\n",
            (unsigned)calleeFlatIndex);
    if (syncStackSlot) {
        fprintf(file,
                "            SZrTypeValue *zr_aot_typed_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
                "            if (zr_aot_typed_destination == ZR_NULL) {\n"
                "                ZR_AOT_C_FAIL();\n"
                "            }\n",
                (unsigned)destinationSlot);
    }
    if (passStateToThunk) {
        fprintf(file,
                "            zr_aot_f%u = zr_aot_typed_f64_fn_%u(state, zr_aot_f%u, zr_aot_f%u);\n",
                (unsigned)destinationSlot,
                (unsigned)calleeFlatIndex,
                (unsigned)firstArgumentSlot,
                (unsigned)secondArgumentSlot);
    } else {
        fprintf(file,
                "            zr_aot_f%u = zr_aot_typed_f64_fn_%u(zr_aot_f%u, zr_aot_f%u);\n",
                (unsigned)destinationSlot,
                (unsigned)calleeFlatIndex,
                (unsigned)firstArgumentSlot,
                (unsigned)secondArgumentSlot);
    }
    if (syncStackSlot) {
        fprintf(file,
                "            /* zr_aot_static_f64_two_arg_direct_call_sync_stack_slot */\n"
                "            ZR_VALUE_FAST_SET(zr_aot_typed_destination,\n"
                "                              nativeDouble,\n"
                "                              zr_aot_f%u,\n"
                "                              ZR_VALUE_TYPE_DOUBLE);\n",
                (unsigned)destinationSlot);
    }
    backend_aot_write_c_static_direct_f64_deopt_fallback(file,
                                                         destinationSlot,
                                                         functionSlot,
                                                         2u,
                                                         calleeFlatIndex,
                                                         "typed f64 direct call");
    fprintf(file, "    }\n");
}

/* 三参数 f64 入口保留守卫与通用调用回退，避免目标元数据变化后继续直调旧 thunk。 */
void backend_aot_write_c_static_direct_f64_three_arg_function_call(FILE *file,
                                                                   TZrUInt32 destinationSlot,
                                                                   TZrUInt32 functionSlot,
                                                                   TZrUInt32 calleeFlatIndex,
                                                                   TZrUInt32 firstArgumentSlot,
                                                                   TZrUInt32 secondArgumentSlot,
                                                                   TZrUInt32 thirdArgumentSlot,
                                                                   TZrBool syncStackSlot,
                                                                   TZrBool passStateToThunk) {
    if (file == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_static_f64_three_arg_direct_call */\n"
            "        /* zr_aot_static_f64_three_arg_direct_call_metadata_guard */\n"
            "        if (ZrLibrary_AotRuntime_CanUseTypedDirectCall(state, &frame, %u)) {\n",
            (unsigned)calleeFlatIndex);
    if (syncStackSlot) {
        fprintf(file,
                "            SZrTypeValue *zr_aot_typed_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
                "            if (zr_aot_typed_destination == ZR_NULL) {\n"
                "                ZR_AOT_C_FAIL();\n"
                "            }\n",
                (unsigned)destinationSlot);
    }
    if (passStateToThunk) {
        fprintf(file,
                "            zr_aot_f%u = zr_aot_typed_f64_fn_%u(state, zr_aot_f%u, zr_aot_f%u, zr_aot_f%u);\n",
                (unsigned)destinationSlot,
                (unsigned)calleeFlatIndex,
                (unsigned)firstArgumentSlot,
                (unsigned)secondArgumentSlot,
                (unsigned)thirdArgumentSlot);
    } else {
        fprintf(file,
                "            zr_aot_f%u = zr_aot_typed_f64_fn_%u(zr_aot_f%u, zr_aot_f%u, zr_aot_f%u);\n",
                (unsigned)destinationSlot,
                (unsigned)calleeFlatIndex,
                (unsigned)firstArgumentSlot,
                (unsigned)secondArgumentSlot,
                (unsigned)thirdArgumentSlot);
    }
    if (syncStackSlot) {
        fprintf(file,
                "            /* zr_aot_static_f64_three_arg_direct_call_sync_stack_slot */\n"
                "            ZR_VALUE_FAST_SET(zr_aot_typed_destination,\n"
                "                              nativeDouble,\n"
                "                              zr_aot_f%u,\n"
                "                              ZR_VALUE_TYPE_DOUBLE);\n",
                (unsigned)destinationSlot);
    }
    backend_aot_write_c_static_direct_f64_deopt_fallback(file,
                                                         destinationSlot,
                                                         functionSlot,
                                                         3u,
                                                         calleeFlatIndex,
                                                         "typed f64 direct call");
    fprintf(file, "    }\n");
}
