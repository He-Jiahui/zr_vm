#include "backend_aot_c_typed_i64_thunks.h"

#include "backend_aot_c_emitter.h"
#include "backend_aot_c_typed_i64_thunk_shapes.h"

TZrBool backend_aot_c_can_emit_typed_i64_no_arg_thunk(const SZrFunction *function) {
    TZrInt64 ignored;

    return backend_aot_c_try_get_i64_constant_return(function, &ignored);
}

TZrBool backend_aot_c_can_emit_typed_i64_one_arg_thunk(const SZrFunction *function) {
    TZrInt64 ignored;

    return (TZrBool)(backend_aot_c_try_get_i64_identity_return(function) ||
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
    return (TZrBool)(backend_aot_c_try_get_i64_arg0_arg1_add_return(function) ||
                     backend_aot_c_try_get_i64_arg0_arg1_subtract_return(function) ||
                     backend_aot_c_try_get_i64_arg0_arg1_multiply_return(function) ||
                     backend_aot_c_try_get_i64_arg0_arg1_divide_return(function) ||
                     backend_aot_c_try_get_i64_arg0_arg1_modulo_return(function) ||
                     backend_aot_c_try_get_i64_arg0_arg1_bitwise_and_return(function) ||
                     backend_aot_c_try_get_i64_arg0_arg1_bitwise_or_return(function) ||
                     backend_aot_c_try_get_i64_arg0_arg1_bitwise_xor_return(function));
}

TZrBool backend_aot_c_can_emit_typed_i64_three_arg_thunk(const SZrFunction *function) {
    return (TZrBool)(backend_aot_c_try_get_i64_arg0_arg1_arg2_add_return(function) ||
                     backend_aot_c_try_get_i64_arg0_arg1_arg2_subtract_return(function) ||
                     backend_aot_c_try_get_i64_arg0_arg1_arg2_multiply_return(function) ||
                     backend_aot_c_try_get_i64_arg0_arg1_arg2_divide_return(function) ||
                     backend_aot_c_try_get_i64_arg0_arg1_arg2_modulo_return(function) ||
                     backend_aot_c_try_get_i64_arg0_arg1_arg2_bitwise_and_return(function) ||
                     backend_aot_c_try_get_i64_arg0_arg1_arg2_bitwise_or_return(function) ||
                     backend_aot_c_try_get_i64_arg0_arg1_arg2_bitwise_xor_return(function));
}

static void backend_aot_c_write_i64_no_arg_thunk_definition(FILE *file,
                                                            TZrUInt32 flatIndex,
                                                            TZrInt64 returnValue) {
    fprintf(file,
            "static TZrInt64 zr_aot_typed_i64_fn_%u(struct SZrState *state) {\n"
            "    (void)state;\n"
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
            "static TZrInt64 zr_aot_typed_i64_fn_%u(struct SZrState *state, TZrInt64 zr_aot_arg0) {\n"
            "    (void)state;\n",
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
            "static TZrInt64 zr_aot_typed_i64_fn_%u(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1) {\n"
            "    (void)state;\n"
            "%s"
            "}\n",
            (unsigned)flatIndex,
            returnExpression);
}

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
            "static TZrInt64 zr_aot_typed_i64_fn_%u(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1, TZrInt64 zr_aot_arg2) {\n"
            "    (void)state;\n"
            "%s"
            "}\n",
            (unsigned)flatIndex,
            returnExpression);
}

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
                    "static TZrInt64 zr_aot_typed_i64_fn_%u(struct SZrState *state);\n",
                    (unsigned)entry->flatIndex);
        } else if (backend_aot_c_can_emit_typed_i64_one_arg_thunk(entry->function)) {
            fprintf(file,
                    "static TZrInt64 zr_aot_typed_i64_fn_%u(struct SZrState *state, TZrInt64 zr_aot_arg0);\n",
                    (unsigned)entry->flatIndex);
        } else if (backend_aot_c_can_emit_typed_i64_two_arg_thunk(entry->function)) {
            fprintf(file,
                    "static TZrInt64 zr_aot_typed_i64_fn_%u(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1);\n",
                    (unsigned)entry->flatIndex);
        } else if (backend_aot_c_can_emit_typed_i64_three_arg_thunk(entry->function)) {
            fprintf(file,
                    "static TZrInt64 zr_aot_typed_i64_fn_%u(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1, TZrInt64 zr_aot_arg2);\n",
                    (unsigned)entry->flatIndex);
        }
    }
}

void backend_aot_write_c_typed_i64_thunks(FILE *file, const SZrAotFunctionTable *table) {
    TZrUInt32 index;

    if (file == ZR_NULL || table == ZR_NULL || table->entries == ZR_NULL) {
        return;
    }

    for (index = 0u; index < table->count; index++) {
        const SZrAotFunctionEntry *entry = &table->entries[index];
        TZrInt64 returnValue;
        if (!backend_aot_c_try_get_i64_constant_return(entry->function, &returnValue)) {
            if (backend_aot_c_try_get_i64_identity_return(entry->function)) {
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
