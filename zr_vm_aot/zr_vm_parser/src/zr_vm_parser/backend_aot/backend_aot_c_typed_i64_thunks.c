#include "backend_aot_c_typed_i64_thunks.h"

#include "backend_aot_c_emitter.h"
#include "backend_aot_c_typed_i64_loop_thunks.h"
#include "backend_aot_c_typed_i64_thunk_shapes.h"

/* 形状识别、前置声明和定义生成共用判定，保持反射与直接调用的 ABI 一致。 */
TZrBool backend_aot_c_can_emit_typed_i64_no_arg_thunk(const SZrFunction *function) {
    TZrInt64 ignored;

    return backend_aot_c_try_get_i64_constant_return(function, &ignored);
}

TZrBool backend_aot_c_can_emit_typed_i64_one_arg_thunk(const SZrFunction *function) {
    TZrInt64 ignored;

    return (TZrBool)(backend_aot_c_try_get_i64_identity_return(function) ||
                     backend_aot_c_can_emit_typed_i64_counting_sum_loop_thunk(function) ||
                     backend_aot_c_try_get_i64_arg0_negate_return(function) ||
                     backend_aot_c_try_get_i64_arg0_bitwise_not_return(function) ||
                     backend_aot_c_try_get_i64_arg0_bitwise_and_constant_return(function, &ignored) ||
                     backend_aot_c_try_get_i64_arg0_bitwise_or_constant_return(function, &ignored) ||
                     backend_aot_c_try_get_i64_arg0_bitwise_xor_constant_return(function, &ignored) ||
                     backend_aot_c_try_get_i64_arg0_add_constant_return(function, &ignored) ||
                     backend_aot_c_try_get_i64_arg0_subtract_constant_return(function, &ignored) ||
                     backend_aot_c_try_get_i64_arg0_multiply_constant_return(function, &ignored));
}

TZrBool backend_aot_c_can_emit_typed_i64_two_arg_thunk(const SZrFunction *function) {
    return (TZrBool)(backend_aot_c_can_emit_typed_i64_two_arg_state_free_thunk(function) ||
                     backend_aot_c_try_get_i64_arg0_arg1_divide_return(function) ||
                     backend_aot_c_try_get_i64_arg0_arg1_modulo_return(function));
}

TZrBool backend_aot_c_can_emit_typed_i64_two_arg_state_free_thunk(const SZrFunction *function) {
    return (TZrBool)(backend_aot_c_try_get_i64_arg0_arg1_add_return(function) ||
                     backend_aot_c_try_get_i64_arg0_arg1_subtract_return(function) ||
                     backend_aot_c_try_get_i64_arg0_arg1_multiply_return(function) ||
                     backend_aot_c_try_get_i64_arg0_arg1_bitwise_and_return(function) ||
                     backend_aot_c_try_get_i64_arg0_arg1_bitwise_or_return(function) ||
                     backend_aot_c_try_get_i64_arg0_arg1_bitwise_xor_return(function));
}

TZrBool backend_aot_c_can_emit_typed_i64_three_arg_thunk(const SZrFunction *function) {
    return (TZrBool)(backend_aot_c_can_emit_typed_i64_three_arg_state_free_thunk(function) ||
                     backend_aot_c_try_get_i64_arg0_arg1_arg2_divide_return(function) ||
                     backend_aot_c_try_get_i64_arg0_arg1_arg2_modulo_return(function));
}

TZrBool backend_aot_c_can_emit_typed_i64_three_arg_state_free_thunk(const SZrFunction *function) {
    return (TZrBool)(backend_aot_c_try_get_i64_arg0_arg1_arg2_add_return(function) ||
                     backend_aot_c_try_get_i64_arg0_arg1_arg2_subtract_return(function) ||
                     backend_aot_c_try_get_i64_arg0_arg1_arg2_multiply_return(function) ||
                     backend_aot_c_try_get_i64_arg0_arg1_arg2_bitwise_and_return(function) ||
                     backend_aot_c_try_get_i64_arg0_arg1_arg2_bitwise_or_return(function) ||
                     backend_aot_c_try_get_i64_arg0_arg1_arg2_bitwise_xor_return(function));
}

static void backend_aot_c_write_i64_no_arg_thunk_definition(FILE *file,
                                                            TZrUInt32 flatIndex,
                                                            TZrInt64 returnValue) {
    fprintf(file,
            "static TZrInt64 zr_aot_typed_i64_fn_%u(void) {\n"
            "    return (TZrInt64)%lld;\n"
            "}\n",
            (unsigned)flatIndex,
            (long long)returnValue);
}

static void backend_aot_c_write_i64_one_arg_thunk_definition(FILE *file,
                                                             TZrUInt32 flatIndex,
                                                             const char *returnExpressionFormat,
                                                             TZrBool hasReturnValue,
                                                             TZrInt64 returnValue) {
    fprintf(file,
            "static TZrInt64 zr_aot_typed_i64_fn_%u(TZrInt64 zr_aot_arg0) {\n",
            (unsigned)flatIndex);
    if (hasReturnValue) {
        fprintf(file, returnExpressionFormat, (long long)returnValue);
    } else {
        fputs(returnExpressionFormat, file);
    }
    fprintf(file, "}\n");
}

static void backend_aot_c_write_i64_two_arg_thunk_definition(FILE *file,
                                                             TZrUInt32 flatIndex,
                                                             const char *returnExpression) {
    fprintf(file,
            "static TZrInt64 zr_aot_typed_i64_fn_%u(TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1) {\n"
            "%s"
            "}\n",
            (unsigned)flatIndex,
            returnExpression);
}

/* BUG: 参数为 INT64_MIN/-1 时仅防零仍会执行 C 有符号除法，生成库可能崩溃或出现未定义结果；
 * test_aot_c_typed_direct_call_arithmetic_shared_library_smoke.c 已证明此 thunk 可从普通静态调用到达。 */
static void backend_aot_c_write_i64_two_arg_divide_thunk_definition(FILE *file, TZrUInt32 flatIndex) {
    fprintf(file,
            "static TZrInt64 zr_aot_typed_i64_fn_%u(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1) {\n"
            "    if (ZR_UNLIKELY(zr_aot_arg1 == 0)) {\n"
            "        ZrCore_Debug_RunError(state, \"generated AOT signed divide by zero\");\n"
            "        return (TZrInt64)0;\n"
            "    }\n"
            "    return (TZrInt64)(zr_aot_arg0 / zr_aot_arg1);\n"
            "}\n",
            (unsigned)flatIndex);
}

/* BUG: INT64_MIN%-1 未被零除检查拦住，生成的有符号取模同样具有未定义行为。 */
static void backend_aot_c_write_i64_two_arg_modulo_thunk_definition(FILE *file, TZrUInt32 flatIndex) {
    fprintf(file,
            "static TZrInt64 zr_aot_typed_i64_fn_%u(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1) {\n"
            "    if (ZR_UNLIKELY(zr_aot_arg1 == 0)) {\n"
            "        ZrCore_Debug_RunError(state, \"generated AOT signed modulo by zero\");\n"
            "        return (TZrInt64)0;\n"
            "    }\n"
            "    return (TZrInt64)(zr_aot_arg0 %% zr_aot_arg1);\n"
            "}\n",
            (unsigned)flatIndex);
}

static void backend_aot_c_write_i64_three_arg_thunk_definition(FILE *file,
                                                               TZrUInt32 flatIndex,
                                                               const char *returnExpression) {
    fprintf(file,
            "static TZrInt64 zr_aot_typed_i64_fn_%u(TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1, TZrInt64 zr_aot_arg2) {\n"
            "%s"
            "}\n",
            (unsigned)flatIndex,
            returnExpression);
}

/* BUG: 左结合的任一除法步骤可遇到 INT64_MIN/-1；这里只检查零除。 */
static void backend_aot_c_write_i64_three_arg_divide_thunk_definition(FILE *file, TZrUInt32 flatIndex) {
    fprintf(file,
            "static TZrInt64 zr_aot_typed_i64_fn_%u(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1, TZrInt64 zr_aot_arg2) {\n"
            "    if (ZR_UNLIKELY(zr_aot_arg1 == 0 || zr_aot_arg2 == 0)) {\n"
            "        ZrCore_Debug_RunError(state, \"generated AOT signed three-arg divide by zero\");\n"
            "        return (TZrInt64)0;\n"
            "    }\n"
            "    return (TZrInt64)(zr_aot_arg0 / zr_aot_arg1 / zr_aot_arg2);\n"
            "}\n",
            (unsigned)flatIndex);
}

/* BUG: 左结合取模中的 INT64_MIN%-1 仍可到达宿主 C 的未定义边界。 */
static void backend_aot_c_write_i64_three_arg_modulo_thunk_definition(FILE *file, TZrUInt32 flatIndex) {
    fprintf(file,
            "static TZrInt64 zr_aot_typed_i64_fn_%u(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1, TZrInt64 zr_aot_arg2) {\n"
            "    if (ZR_UNLIKELY(zr_aot_arg1 == 0 || zr_aot_arg2 == 0)) {\n"
            "        ZrCore_Debug_RunError(state, \"generated AOT signed three-arg modulo by zero\");\n"
            "        return (TZrInt64)0;\n"
            "    }\n"
            "    return (TZrInt64)(zr_aot_arg0 %% zr_aot_arg1 %% zr_aot_arg2);\n"
            "}\n",
            (unsigned)flatIndex);
}

void backend_aot_write_c_typed_i64_thunk_forward_decls(FILE *file, const SZrAotFunctionTable *table) {
    TZrUInt32 index;

    if (file == ZR_NULL || table == ZR_NULL || table->entries == ZR_NULL) {
        return;
    }

    for (index = 0u; index < table->count; index++) {
        const SZrAotFunctionEntry *entry = &table->entries[index];
        if (backend_aot_c_can_emit_typed_i64_no_arg_thunk(entry->function)) {
            fprintf(file,
                    "static TZrInt64 zr_aot_typed_i64_fn_%u(void);\n",
                    (unsigned)entry->flatIndex);
        } else if (backend_aot_c_can_emit_typed_i64_one_arg_thunk(entry->function)) {
            fprintf(file,
                    "static TZrInt64 zr_aot_typed_i64_fn_%u(TZrInt64 zr_aot_arg0);\n",
                    (unsigned)entry->flatIndex);
        /* 无 state 的原型必须先于带异常报告能力的同参数数原型判定。 */
        } else if (backend_aot_c_can_emit_typed_i64_two_arg_state_free_thunk(entry->function)) {
            fprintf(file,
                    "static TZrInt64 zr_aot_typed_i64_fn_%u(TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1);\n",
                    (unsigned)entry->flatIndex);
        } else if (backend_aot_c_can_emit_typed_i64_two_arg_thunk(entry->function)) {
            fprintf(file,
                    "static TZrInt64 zr_aot_typed_i64_fn_%u(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1);\n",
                    (unsigned)entry->flatIndex);
        } else if (backend_aot_c_can_emit_typed_i64_three_arg_state_free_thunk(entry->function)) {
            fprintf(file,
                    "static TZrInt64 zr_aot_typed_i64_fn_%u(TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1, TZrInt64 zr_aot_arg2);\n",
                    (unsigned)entry->flatIndex);
        } else if (backend_aot_c_can_emit_typed_i64_three_arg_thunk(entry->function)) {
            fprintf(file,
                    "static TZrInt64 zr_aot_typed_i64_fn_%u(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1, TZrInt64 zr_aot_arg2);\n",
                    (unsigned)entry->flatIndex);
        }
    }
}

/* BUG: 可达的一参数取负及加减乘模板直接使用 C signed 算术，边界输入溢出时行为未定义；
 * 形状判定不含取值范围证明，需与执行器数值契约和边界测试逐项对齐。 */
void backend_aot_write_c_typed_i64_thunks(FILE *file, const SZrAotFunctionTable *table) {
    TZrUInt32 index;

    if (file == ZR_NULL || table == ZR_NULL || table->entries == ZR_NULL) {
        return;
    }

    for (index = 0u; index < table->count; index++) {
        const SZrAotFunctionEntry *entry = &table->entries[index];
        TZrInt64 returnValue;
        if (!backend_aot_c_try_get_i64_constant_return(entry->function, &returnValue)) {
            if (backend_aot_c_can_emit_typed_i64_counting_sum_loop_thunk(entry->function)) {
                backend_aot_c_write_typed_i64_counting_sum_loop_thunk(file, entry->flatIndex);
            } else if (backend_aot_c_try_get_i64_identity_return(entry->function)) {
                backend_aot_c_write_i64_one_arg_thunk_definition(file,
                                                                 entry->flatIndex,
                                                                 "    return zr_aot_arg0;\n",
                                                                 ZR_FALSE,
                                                                 0);
            } else if (backend_aot_c_try_get_i64_arg0_negate_return(entry->function)) {
                backend_aot_c_write_i64_one_arg_thunk_definition(file,
                                                                 entry->flatIndex,
                                                                 "    return (TZrInt64)(-zr_aot_arg0);\n",
                                                                 ZR_FALSE,
                                                                 0);
            } else if (backend_aot_c_try_get_i64_arg0_bitwise_not_return(entry->function)) {
                backend_aot_c_write_i64_one_arg_thunk_definition(file,
                                                                 entry->flatIndex,
                                                                 "    return (TZrInt64)(~zr_aot_arg0);\n",
                                                                 ZR_FALSE,
                                                                 0);
            } else if (backend_aot_c_try_get_i64_arg0_bitwise_and_constant_return(entry->function, &returnValue)) {
                backend_aot_c_write_i64_one_arg_thunk_definition(file,
                                                                 entry->flatIndex,
                                                                 "    return (TZrInt64)(zr_aot_arg0 & (TZrInt64)%lld);\n",
                                                                 ZR_TRUE,
                                                                 returnValue);
            } else if (backend_aot_c_try_get_i64_arg0_bitwise_or_constant_return(entry->function, &returnValue)) {
                backend_aot_c_write_i64_one_arg_thunk_definition(file,
                                                                 entry->flatIndex,
                                                                 "    return (TZrInt64)(zr_aot_arg0 | (TZrInt64)%lld);\n",
                                                                 ZR_TRUE,
                                                                 returnValue);
            } else if (backend_aot_c_try_get_i64_arg0_bitwise_xor_constant_return(entry->function, &returnValue)) {
                backend_aot_c_write_i64_one_arg_thunk_definition(file,
                                                                 entry->flatIndex,
                                                                 "    return (TZrInt64)(zr_aot_arg0 ^ (TZrInt64)%lld);\n",
                                                                 ZR_TRUE,
                                                                 returnValue);
            } else if (backend_aot_c_try_get_i64_arg0_add_constant_return(entry->function, &returnValue)) {
                backend_aot_c_write_i64_one_arg_thunk_definition(file,
                                                                 entry->flatIndex,
                                                                 "    return (TZrInt64)(zr_aot_arg0 + (TZrInt64)%lld);\n",
                                                                 ZR_TRUE,
                                                                 returnValue);
            } else if (backend_aot_c_try_get_i64_arg0_subtract_constant_return(entry->function, &returnValue)) {
                backend_aot_c_write_i64_one_arg_thunk_definition(file,
                                                                 entry->flatIndex,
                                                                 "    return (TZrInt64)(zr_aot_arg0 - (TZrInt64)%lld);\n",
                                                                 ZR_TRUE,
                                                                 returnValue);
            } else if (backend_aot_c_try_get_i64_arg0_multiply_constant_return(entry->function, &returnValue)) {
                backend_aot_c_write_i64_one_arg_thunk_definition(file,
                                                                 entry->flatIndex,
                                                                 "    return (TZrInt64)(zr_aot_arg0 * (TZrInt64)%lld);\n",
                                                                 ZR_TRUE,
                                                                 returnValue);
            } else if (backend_aot_c_try_get_i64_arg0_arg1_add_return(entry->function)) {
                backend_aot_c_write_i64_two_arg_thunk_definition(file,
                                                                 entry->flatIndex,
                                                                 "    return (TZrInt64)(zr_aot_arg0 + zr_aot_arg1);\n");
            } else if (backend_aot_c_try_get_i64_arg0_arg1_subtract_return(entry->function)) {
                backend_aot_c_write_i64_two_arg_thunk_definition(file,
                                                                 entry->flatIndex,
                                                                 "    return (TZrInt64)(zr_aot_arg0 - zr_aot_arg1);\n");
            } else if (backend_aot_c_try_get_i64_arg0_arg1_multiply_return(entry->function)) {
                backend_aot_c_write_i64_two_arg_thunk_definition(file,
                                                                 entry->flatIndex,
                                                                 "    return (TZrInt64)(zr_aot_arg0 * zr_aot_arg1);\n");
            } else if (backend_aot_c_try_get_i64_arg0_arg1_divide_return(entry->function)) {
                backend_aot_c_write_i64_two_arg_divide_thunk_definition(file, entry->flatIndex);
            } else if (backend_aot_c_try_get_i64_arg0_arg1_modulo_return(entry->function)) {
                backend_aot_c_write_i64_two_arg_modulo_thunk_definition(file, entry->flatIndex);
            } else if (backend_aot_c_try_get_i64_arg0_arg1_bitwise_and_return(entry->function)) {
                backend_aot_c_write_i64_two_arg_thunk_definition(file,
                                                                 entry->flatIndex,
                                                                 "    return (TZrInt64)(zr_aot_arg0 & zr_aot_arg1);\n");
            } else if (backend_aot_c_try_get_i64_arg0_arg1_bitwise_or_return(entry->function)) {
                backend_aot_c_write_i64_two_arg_thunk_definition(file,
                                                                 entry->flatIndex,
                                                                 "    return (TZrInt64)(zr_aot_arg0 | zr_aot_arg1);\n");
            } else if (backend_aot_c_try_get_i64_arg0_arg1_bitwise_xor_return(entry->function)) {
                backend_aot_c_write_i64_two_arg_thunk_definition(file,
                                                                 entry->flatIndex,
                                                                 "    return (TZrInt64)(zr_aot_arg0 ^ zr_aot_arg1);\n");
            } else if (backend_aot_c_try_get_i64_arg0_arg1_arg2_add_return(entry->function)) {
                backend_aot_c_write_i64_three_arg_thunk_definition(file,
                                                                   entry->flatIndex,
                                                                   "    return (TZrInt64)(zr_aot_arg0 + zr_aot_arg1 + zr_aot_arg2);\n");
            } else if (backend_aot_c_try_get_i64_arg0_arg1_arg2_subtract_return(entry->function)) {
                backend_aot_c_write_i64_three_arg_thunk_definition(file,
                                                                   entry->flatIndex,
                                                                   "    return (TZrInt64)(zr_aot_arg0 - zr_aot_arg1 - zr_aot_arg2);\n");
            } else if (backend_aot_c_try_get_i64_arg0_arg1_arg2_multiply_return(entry->function)) {
                backend_aot_c_write_i64_three_arg_thunk_definition(file,
                                                                   entry->flatIndex,
                                                                   "    return (TZrInt64)(zr_aot_arg0 * zr_aot_arg1 * zr_aot_arg2);\n");
            } else if (backend_aot_c_try_get_i64_arg0_arg1_arg2_divide_return(entry->function)) {
                backend_aot_c_write_i64_three_arg_divide_thunk_definition(file, entry->flatIndex);
            } else if (backend_aot_c_try_get_i64_arg0_arg1_arg2_modulo_return(entry->function)) {
                backend_aot_c_write_i64_three_arg_modulo_thunk_definition(file, entry->flatIndex);
            } else if (backend_aot_c_try_get_i64_arg0_arg1_arg2_bitwise_and_return(entry->function)) {
                backend_aot_c_write_i64_three_arg_thunk_definition(file,
                                                                   entry->flatIndex,
                                                                   "    return (TZrInt64)(zr_aot_arg0 & zr_aot_arg1 & zr_aot_arg2);\n");
            } else if (backend_aot_c_try_get_i64_arg0_arg1_arg2_bitwise_or_return(entry->function)) {
                backend_aot_c_write_i64_three_arg_thunk_definition(file,
                                                                   entry->flatIndex,
                                                                   "    return (TZrInt64)(zr_aot_arg0 | zr_aot_arg1 | zr_aot_arg2);\n");
            } else if (backend_aot_c_try_get_i64_arg0_arg1_arg2_bitwise_xor_return(entry->function)) {
                backend_aot_c_write_i64_three_arg_thunk_definition(file,
                                                                   entry->flatIndex,
                                                                   "    return (TZrInt64)(zr_aot_arg0 ^ zr_aot_arg1 ^ zr_aot_arg2);\n");
            }
            continue;
        }

        backend_aot_c_write_i64_no_arg_thunk_definition(file, entry->flatIndex, returnValue);
    }
}
