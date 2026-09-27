#include "backend_aot_c_emitter.h"
#include "backend_aot_c_scalar_locals.h"

/* 泛型转换的值槽仍由运行时负责；只有静态证明安全的 TO_BOOL 才完全留在标量局部变量。 */

/* 运行时转换先写 VM 值槽，再刷新可能存在的标量缓存，供后续快路径读取同一结果。 */
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

/* 仅在源缓存已写入且目标值槽可跳过时生成本地转换，避免从未物化的值槽读旧值。 */
static TZrBool backend_aot_write_c_scalar_to_bool(FILE *file,
                                                  const SZrAotExecIrFunction *functionIr,
                                                  TZrUInt32 destinationSlot,
                                                  TZrUInt32 sourceSlot,
                                                  TZrUInt32 execInstructionIndex) {
    TZrBool useWrittenBoolSource;
    TZrBool useWrittenI64Source;
    TZrBool useWrittenU64Source;
    TZrBool useWrittenF64Source;

    if (file == ZR_NULL ||
        !backend_aot_c_scalar_locals_has_bool_slot(functionIr, destinationSlot) ||
        !backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex)) {
        return ZR_FALSE;
    }

    useWrittenBoolSource = (TZrBool)(backend_aot_c_scalar_locals_has_bool_slot(functionIr, sourceSlot) &&
                                     backend_aot_c_scalar_locals_bool_written_before(functionIr, sourceSlot, execInstructionIndex));
    useWrittenI64Source = (TZrBool)(backend_aot_c_scalar_locals_has_i64_slot(functionIr, sourceSlot) &&
                                    backend_aot_c_scalar_locals_i64_written_before(functionIr, sourceSlot, execInstructionIndex));
    useWrittenU64Source = (TZrBool)(backend_aot_c_scalar_locals_has_u64_slot(functionIr, sourceSlot) &&
                                    backend_aot_c_scalar_locals_u64_written_before(functionIr, sourceSlot, execInstructionIndex));
    useWrittenF64Source = (TZrBool)(backend_aot_c_scalar_locals_has_f64_slot(functionIr, sourceSlot) &&
                                    backend_aot_c_scalar_locals_f64_written_before(functionIr, sourceSlot, execInstructionIndex));
    if (!useWrittenBoolSource && !useWrittenI64Source && !useWrittenU64Source && !useWrittenF64Source) {
        return ZR_FALSE;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_scalar_exec_to_bool dstSlot=%u srcSlot=%u */\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
    if (useWrittenBoolSource) {
        fprintf(file,
                "        zr_aot_b%u = (TZrBool)(zr_aot_b%u != 0u);\n",
                (unsigned)destinationSlot,
                (unsigned)sourceSlot);
    } else if (useWrittenI64Source) {
        fprintf(file,
                "        zr_aot_b%u = (TZrBool)(zr_aot_s%u != (TZrInt64)0);\n",
                (unsigned)destinationSlot,
                (unsigned)sourceSlot);
    } else if (useWrittenU64Source) {
        fprintf(file,
                "        zr_aot_b%u = (TZrBool)(zr_aot_u%u != (TZrUInt64)0u);\n",
                (unsigned)destinationSlot,
                (unsigned)sourceSlot);
    } else {
        fprintf(file,
                "        zr_aot_b%u = (TZrBool)(zr_aot_f%u != 0.0);\n",
                (unsigned)destinationSlot,
                (unsigned)sourceSlot);
    }
    fprintf(file, "    }\n");
    return ZR_TRUE;
}

/* TO_BOOL 优先沿标量链传播；未知来源交给现役 AOT runtime 判断并同步目标缓存。 */
/* BUG: compile_expression.c 可把字符串或对象源转为 TO_BOOL；解释器支持字符串
 * 真值、对象默认真及 ZR_META_TO_BOOL，现役 ConvertGenericToBool 对它们报错。 */
void backend_aot_write_c_direct_to_bool(FILE *file,
                                        const SZrAotExecIrFunction *functionIr,
                                        TZrUInt32 destinationSlot,
                                        TZrUInt32 sourceSlot,
                                        TZrUInt32 execInstructionIndex) {
    if (file == ZR_NULL) {
        return;
    }

    if (backend_aot_write_c_scalar_to_bool(file, functionIr, destinationSlot, sourceSlot, execInstructionIndex)) {
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

/* TO_INT 必须保留运行时类型分派，生成器不能从 opcode 推断源值一定是整数。 */
/* BUG: 字符串、对象或 null 可由显式 cast 编译为 TO_INT；解释器可调用
 * ZR_META_TO_INT 或返回默认 0，ConvertGenericToInt 对它们却报 unsupported。 */
/* BUG: float 源可进入 ConvertGenericToInt，而该入口未检查 NaN、无穷及 int64
 * 值域即执行 C 强转；AOT 执行在该 runtime 强转处有未定义行为，见 aot_runtime_values.c。 */
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

/* TO_UINT 与 TO_INT 共用“运行时写值槽、随后同步缓存”的跨层协议。 */
/* BUG: 字符串、对象或 null 可进入 TO_UINT；解释器可调用 ZR_META_TO_UINT
 * 或返回默认 0，ConvertGenericToUInt 对它们却报 unsupported。 */
/* BUG: float 源可进入 ConvertGenericToUInt，而该入口未检查截断后仍小于零的
 * 负数（如 -1.0）、NaN、无穷及 uint64 上界即执行 C 强转，导致未定义行为。 */
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

/* TO_FLOAT 在动态源类型下经运行时生成 boxed 结果，再供后续浮点局部快路径复用。 */
/* BUG: 字符串、对象或 null 可进入 TO_FLOAT；解释器可调用 ZR_META_TO_FLOAT
 * 或返回默认 0.0，ConvertGenericToFloat 对它们却报 unsupported。 */
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
