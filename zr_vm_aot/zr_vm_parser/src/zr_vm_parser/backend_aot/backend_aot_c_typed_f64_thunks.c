#include "backend_aot_c_typed_f64_thunks.h"

#include "backend_aot_c_emitter.h"
#include "backend_aot_c_typed_f64_thunk_shapes.h"

TZrBool backend_aot_c_can_emit_typed_f64_no_arg_thunk(const SZrFunction *function) {
    TZrFloat64 ignored;

    return backend_aot_c_try_get_f64_constant_return(function, &ignored);
}

TZrBool backend_aot_c_can_emit_typed_f64_one_arg_thunk(const SZrFunction *function) {
    TZrFloat64 ignored;

    return (TZrBool)(backend_aot_c_try_get_f64_identity_return(function) ||
                     backend_aot_c_try_get_f64_arg0_negate_return(function) ||
                     backend_aot_c_try_get_f64_arg0_add_constant_return(function, &ignored) ||
                     backend_aot_c_try_get_f64_arg0_subtract_constant_return(function, &ignored) ||
                     backend_aot_c_try_get_f64_arg0_multiply_constant_return(function, &ignored) ||
                     backend_aot_c_try_get_f64_arg0_divide_constant_return(function, &ignored) ||
                     backend_aot_c_try_get_f64_arg0_modulo_constant_return(function, &ignored));
}

TZrBool backend_aot_c_can_emit_typed_f64_two_arg_thunk(const SZrFunction *function) {
    return (TZrBool)(backend_aot_c_can_emit_typed_f64_two_arg_state_free_thunk(function) ||
                     backend_aot_c_try_get_f64_arg0_arg1_divide_return(function) ||
                     backend_aot_c_try_get_f64_arg0_arg1_modulo_return(function));
}

TZrBool backend_aot_c_can_emit_typed_f64_two_arg_state_free_thunk(const SZrFunction *function) {
    return (TZrBool)(backend_aot_c_try_get_f64_arg0_arg1_add_return(function) ||
                     backend_aot_c_try_get_f64_arg0_arg1_subtract_return(function) ||
                     backend_aot_c_try_get_f64_arg0_arg1_multiply_return(function));
}

TZrBool backend_aot_c_can_emit_typed_f64_three_arg_thunk(const SZrFunction *function) {
    return (TZrBool)(backend_aot_c_can_emit_typed_f64_three_arg_state_free_thunk(function) ||
                     backend_aot_c_try_get_f64_arg0_arg1_arg2_divide_return(function) ||
                     backend_aot_c_try_get_f64_arg0_arg1_arg2_modulo_return(function));
}

TZrBool backend_aot_c_can_emit_typed_f64_three_arg_state_free_thunk(const SZrFunction *function) {
    return (TZrBool)(backend_aot_c_try_get_f64_arg0_arg1_arg2_add_return(function) ||
                     backend_aot_c_try_get_f64_arg0_arg1_arg2_subtract_return(function) ||
                     backend_aot_c_try_get_f64_arg0_arg1_arg2_multiply_return(function));
}

static void backend_aot_c_write_f64_no_arg_thunk_definition(FILE *file,
                                                            TZrUInt32 flatIndex,
                                                            TZrFloat64 returnValue) {
    fprintf(file,
            "static TZrFloat64 zr_aot_typed_f64_fn_%u(void) {\n"
            "    return (TZrFloat64)%.17g;\n"
            "}\n",
            (unsigned)flatIndex,
            (double)returnValue);
}

static void backend_aot_c_write_f64_one_arg_thunk_definition(FILE *file,
                                                             TZrUInt32 flatIndex,
                                                             const char *returnExpression) {
    fprintf(file,
            "static TZrFloat64 zr_aot_typed_f64_fn_%u(TZrFloat64 zr_aot_arg0) {\n"
            "%s"
            "}\n",
            (unsigned)flatIndex,
            returnExpression);
}

static void backend_aot_c_write_f64_two_arg_thunk_definition(FILE *file,
                                                             TZrUInt32 flatIndex,
                                                             const char *returnExpression) {
    fprintf(file,
            "static TZrFloat64 zr_aot_typed_f64_fn_%u(TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1) {\n"
            "%s"
            "}\n",
            (unsigned)flatIndex,
            returnExpression);
}

static void backend_aot_c_write_f64_three_arg_thunk_definition(FILE *file,
                                                               TZrUInt32 flatIndex,
                                                               const char *returnExpression) {
    fprintf(file,
            "static TZrFloat64 zr_aot_typed_f64_fn_%u(TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1, TZrFloat64 zr_aot_arg2) {\n"
            "%s"
            "}\n",
            (unsigned)flatIndex,
            returnExpression);
}

static void backend_aot_c_write_f64_three_arg_divide_thunk_definition(FILE *file, TZrUInt32 flatIndex) {
    fprintf(file,
            "static TZrFloat64 zr_aot_typed_f64_fn_%u(struct SZrState *state, TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1, TZrFloat64 zr_aot_arg2) {\n"
            "    if (ZR_UNLIKELY(zr_aot_arg1 == 0.0 || zr_aot_arg2 == 0.0)) {\n"
            "        ZrCore_Debug_RunError(state, \"generated AOT float three-arg divide by zero\");\n"
            "        return (TZrFloat64)0.0;\n"
            "    }\n"
            "    return (TZrFloat64)(zr_aot_arg0 / zr_aot_arg1 / zr_aot_arg2);\n"
            "}\n",
            (unsigned)flatIndex);
}

static void backend_aot_c_write_f64_three_arg_modulo_thunk_definition(FILE *file, TZrUInt32 flatIndex) {
    fprintf(file,
            "static TZrFloat64 zr_aot_typed_f64_fn_%u(struct SZrState *state, TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1, TZrFloat64 zr_aot_arg2) {\n"
            "    if (ZR_UNLIKELY(zr_aot_arg1 == 0.0 || zr_aot_arg2 == 0.0)) {\n"
            "        ZrCore_Debug_RunError(state, \"generated AOT float three-arg modulo by zero\");\n"
            "        return (TZrFloat64)0.0;\n"
            "    }\n"
            "    return (TZrFloat64)fmod(fmod(zr_aot_arg0, zr_aot_arg1), zr_aot_arg2);\n"
            "}\n",
            (unsigned)flatIndex);
}

static void backend_aot_c_write_f64_two_arg_divide_thunk_definition(FILE *file, TZrUInt32 flatIndex) {
    fprintf(file,
            "static TZrFloat64 zr_aot_typed_f64_fn_%u(struct SZrState *state, TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1) {\n"
            "    if (ZR_UNLIKELY(zr_aot_arg1 == 0.0)) {\n"
            "        ZrCore_Debug_RunError(state, \"generated AOT float divide by zero\");\n"
            "        return (TZrFloat64)0.0;\n"
            "    }\n"
            "    return (TZrFloat64)(zr_aot_arg0 / zr_aot_arg1);\n"
            "}\n",
            (unsigned)flatIndex);
}

static void backend_aot_c_write_f64_two_arg_modulo_thunk_definition(FILE *file, TZrUInt32 flatIndex) {
    fprintf(file,
            "static TZrFloat64 zr_aot_typed_f64_fn_%u(struct SZrState *state, TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1) {\n"
            "    if (ZR_UNLIKELY(zr_aot_arg1 == 0.0)) {\n"
            "        ZrCore_Debug_RunError(state, \"generated AOT float modulo by zero\");\n"
            "        return (TZrFloat64)0.0;\n"
            "    }\n"
            "    return (TZrFloat64)fmod(zr_aot_arg0, zr_aot_arg1);\n"
            "}\n",
            (unsigned)flatIndex);
}

void backend_aot_write_c_typed_f64_thunk_forward_decls(FILE *file, const SZrAotFunctionTable *table) {
    TZrUInt32 index;

    if (file == ZR_NULL || table == ZR_NULL || table->entries == ZR_NULL) {
        return;
    }

    for (index = 0u; index < table->count; index++) {
        const SZrAotFunctionEntry *entry = &table->entries[index];
        if (backend_aot_c_can_emit_typed_f64_no_arg_thunk(entry->function)) {
            fprintf(file,
                    "static TZrFloat64 zr_aot_typed_f64_fn_%u(void);\n",
                    (unsigned)entry->flatIndex);
        } else if (backend_aot_c_can_emit_typed_f64_one_arg_thunk(entry->function)) {
            fprintf(file,
                    "static TZrFloat64 zr_aot_typed_f64_fn_%u(TZrFloat64 zr_aot_arg0);\n",
                    (unsigned)entry->flatIndex);
        } else if (backend_aot_c_can_emit_typed_f64_two_arg_state_free_thunk(entry->function)) {
            fprintf(file,
                    "static TZrFloat64 zr_aot_typed_f64_fn_%u(TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1);\n",
                    (unsigned)entry->flatIndex);
        } else if (backend_aot_c_can_emit_typed_f64_two_arg_thunk(entry->function)) {
            fprintf(file,
                    "static TZrFloat64 zr_aot_typed_f64_fn_%u(struct SZrState *state, TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1);\n",
                    (unsigned)entry->flatIndex);
        } else if (backend_aot_c_can_emit_typed_f64_three_arg_state_free_thunk(entry->function)) {
            fprintf(file,
                    "static TZrFloat64 zr_aot_typed_f64_fn_%u(TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1, TZrFloat64 zr_aot_arg2);\n",
                    (unsigned)entry->flatIndex);
        } else if (backend_aot_c_can_emit_typed_f64_three_arg_thunk(entry->function)) {
            fprintf(file,
                    "static TZrFloat64 zr_aot_typed_f64_fn_%u(struct SZrState *state, TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1, TZrFloat64 zr_aot_arg2);\n",
                    (unsigned)entry->flatIndex);
        }
    }
}

void backend_aot_write_c_typed_f64_thunks(FILE *file, const SZrAotFunctionTable *table) {
    TZrUInt32 index;

    if (file == ZR_NULL || table == ZR_NULL || table->entries == ZR_NULL) {
        return;
    }

    for (index = 0u; index < table->count; index++) {
        const SZrAotFunctionEntry *entry = &table->entries[index];
        TZrFloat64 returnValue;
        if (backend_aot_c_try_get_f64_constant_return(entry->function, &returnValue)) {
            backend_aot_c_write_f64_no_arg_thunk_definition(file, entry->flatIndex, returnValue);
        } else if (backend_aot_c_try_get_f64_identity_return(entry->function)) {
            backend_aot_c_write_f64_one_arg_thunk_definition(
                    file,
                    entry->flatIndex,
                    "    return zr_aot_arg0;\n");
        } else if (backend_aot_c_try_get_f64_arg0_negate_return(entry->function)) {
            backend_aot_c_write_f64_one_arg_thunk_definition(
                    file,
                    entry->flatIndex,
                    "    return (TZrFloat64)(-zr_aot_arg0);\n");
        } else if (backend_aot_c_try_get_f64_arg0_add_constant_return(entry->function, &returnValue)) {
            fprintf(file,
                    "static TZrFloat64 zr_aot_typed_f64_fn_%u(TZrFloat64 zr_aot_arg0) {\n"
                    "    return (TZrFloat64)(zr_aot_arg0 + (TZrFloat64)%.17g);\n"
                    "}\n",
                    (unsigned)entry->flatIndex,
                    (double)returnValue);
        } else if (backend_aot_c_try_get_f64_arg0_subtract_constant_return(entry->function, &returnValue)) {
            fprintf(file,
                    "static TZrFloat64 zr_aot_typed_f64_fn_%u(TZrFloat64 zr_aot_arg0) {\n"
                    "    return (TZrFloat64)(zr_aot_arg0 - (TZrFloat64)%.17g);\n"
                    "}\n",
                    (unsigned)entry->flatIndex,
                    (double)returnValue);
        } else if (backend_aot_c_try_get_f64_arg0_multiply_constant_return(entry->function, &returnValue)) {
            fprintf(file,
                    "static TZrFloat64 zr_aot_typed_f64_fn_%u(TZrFloat64 zr_aot_arg0) {\n"
                    "    return (TZrFloat64)(zr_aot_arg0 * (TZrFloat64)%.17g);\n"
                    "}\n",
                    (unsigned)entry->flatIndex,
                    (double)returnValue);
        } else if (backend_aot_c_try_get_f64_arg0_divide_constant_return(entry->function, &returnValue)) {
            fprintf(file,
                    "static TZrFloat64 zr_aot_typed_f64_fn_%u(TZrFloat64 zr_aot_arg0) {\n"
                    "    return (TZrFloat64)(zr_aot_arg0 / (TZrFloat64)%.17g);\n"
                    "}\n",
                    (unsigned)entry->flatIndex,
                    (double)returnValue);
        } else if (backend_aot_c_try_get_f64_arg0_modulo_constant_return(entry->function, &returnValue)) {
            fprintf(file,
                    "static TZrFloat64 zr_aot_typed_f64_fn_%u(TZrFloat64 zr_aot_arg0) {\n"
                    "    return (TZrFloat64)fmod(zr_aot_arg0, (TZrFloat64)%.17g);\n"
                    "}\n",
                    (unsigned)entry->flatIndex,
                    (double)returnValue);
        } else if (backend_aot_c_try_get_f64_arg0_arg1_add_return(entry->function)) {
            backend_aot_c_write_f64_two_arg_thunk_definition(
                    file,
                    entry->flatIndex,
                    "    return (TZrFloat64)(zr_aot_arg0 + zr_aot_arg1);\n");
        } else if (backend_aot_c_try_get_f64_arg0_arg1_subtract_return(entry->function)) {
            backend_aot_c_write_f64_two_arg_thunk_definition(
                    file,
                    entry->flatIndex,
                    "    return (TZrFloat64)(zr_aot_arg0 - zr_aot_arg1);\n");
        } else if (backend_aot_c_try_get_f64_arg0_arg1_multiply_return(entry->function)) {
            backend_aot_c_write_f64_two_arg_thunk_definition(
                    file,
                    entry->flatIndex,
                    "    return (TZrFloat64)(zr_aot_arg0 * zr_aot_arg1);\n");
        } else if (backend_aot_c_try_get_f64_arg0_arg1_divide_return(entry->function)) {
            backend_aot_c_write_f64_two_arg_divide_thunk_definition(file, entry->flatIndex);
        } else if (backend_aot_c_try_get_f64_arg0_arg1_modulo_return(entry->function)) {
            backend_aot_c_write_f64_two_arg_modulo_thunk_definition(file, entry->flatIndex);
        } else if (backend_aot_c_try_get_f64_arg0_arg1_arg2_add_return(entry->function)) {
            backend_aot_c_write_f64_three_arg_thunk_definition(
                    file,
                    entry->flatIndex,
                    "    return (TZrFloat64)(zr_aot_arg0 + zr_aot_arg1 + zr_aot_arg2);\n");
        } else if (backend_aot_c_try_get_f64_arg0_arg1_arg2_subtract_return(entry->function)) {
            backend_aot_c_write_f64_three_arg_thunk_definition(
                    file,
                    entry->flatIndex,
                    "    return (TZrFloat64)(zr_aot_arg0 - zr_aot_arg1 - zr_aot_arg2);\n");
        } else if (backend_aot_c_try_get_f64_arg0_arg1_arg2_multiply_return(entry->function)) {
            backend_aot_c_write_f64_three_arg_thunk_definition(
                    file,
                    entry->flatIndex,
                    "    return (TZrFloat64)(zr_aot_arg0 * zr_aot_arg1 * zr_aot_arg2);\n");
        } else if (backend_aot_c_try_get_f64_arg0_arg1_arg2_divide_return(entry->function)) {
            backend_aot_c_write_f64_three_arg_divide_thunk_definition(file, entry->flatIndex);
        } else if (backend_aot_c_try_get_f64_arg0_arg1_arg2_modulo_return(entry->function)) {
            backend_aot_c_write_f64_three_arg_modulo_thunk_definition(file, entry->flatIndex);
        }
    }
}
