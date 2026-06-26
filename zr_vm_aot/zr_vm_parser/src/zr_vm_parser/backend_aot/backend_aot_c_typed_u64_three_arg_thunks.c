#include "backend_aot_c_typed_u64_three_arg_thunks.h"

#include "backend_aot_c_typed_u64_thunk_shapes.h"

TZrBool backend_aot_c_can_emit_typed_u64_three_arg_thunk(const SZrFunction *function) {
    return (TZrBool)(backend_aot_c_can_emit_typed_u64_three_arg_state_free_thunk(function) ||
                     backend_aot_c_try_get_u64_arg0_arg1_arg2_divide_return(function) ||
                     backend_aot_c_try_get_u64_arg0_arg1_arg2_modulo_return(function));
}

TZrBool backend_aot_c_can_emit_typed_u64_three_arg_state_free_thunk(const SZrFunction *function) {
    return (TZrBool)(backend_aot_c_try_get_u64_arg0_arg1_arg2_add_return(function) ||
                     backend_aot_c_try_get_u64_arg0_arg1_arg2_multiply_return(function) ||
                     backend_aot_c_try_get_u64_arg0_arg1_arg2_subtract_return(function) ||
                     backend_aot_c_try_get_u64_arg0_arg1_arg2_bitwise_and_return(function) ||
                     backend_aot_c_try_get_u64_arg0_arg1_arg2_bitwise_or_return(function) ||
                     backend_aot_c_try_get_u64_arg0_arg1_arg2_bitwise_xor_return(function));
}

void backend_aot_c_write_u64_three_arg_thunk_forward_decl(FILE *file, TZrUInt32 flatIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "static TZrUInt64 zr_aot_typed_u64_fn_%u(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1, TZrUInt64 zr_aot_arg2);\n",
            (unsigned)flatIndex);
}

void backend_aot_c_write_u64_three_arg_state_free_thunk_forward_decl(FILE *file, TZrUInt32 flatIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "static TZrUInt64 zr_aot_typed_u64_fn_%u(TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1, TZrUInt64 zr_aot_arg2);\n",
            (unsigned)flatIndex);
}

static void backend_aot_c_write_u64_three_arg_thunk_definition(FILE *file,
                                                               TZrUInt32 flatIndex,
                                                               const char *returnExpression) {
    fprintf(file,
            "static TZrUInt64 zr_aot_typed_u64_fn_%u(TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1, TZrUInt64 zr_aot_arg2) {\n"
            "%s"
            "}\n",
            (unsigned)flatIndex,
            returnExpression);
}

static void backend_aot_c_write_u64_three_arg_divide_thunk_definition(FILE *file, TZrUInt32 flatIndex) {
    fprintf(file,
            "static TZrUInt64 zr_aot_typed_u64_fn_%u(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1, TZrUInt64 zr_aot_arg2) {\n"
            "    if (ZR_UNLIKELY(zr_aot_arg1 == 0u || zr_aot_arg2 == 0u)) {\n"
            "        ZrCore_Debug_RunError(state, \"generated AOT unsigned three-arg divide by zero\");\n"
            "        return (TZrUInt64)0;\n"
            "    }\n"
            "    return (TZrUInt64)(zr_aot_arg0 / zr_aot_arg1 / zr_aot_arg2);\n"
            "}\n",
            (unsigned)flatIndex);
}

static void backend_aot_c_write_u64_three_arg_modulo_thunk_definition(FILE *file, TZrUInt32 flatIndex) {
    fprintf(file,
            "static TZrUInt64 zr_aot_typed_u64_fn_%u(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1, TZrUInt64 zr_aot_arg2) {\n"
            "    if (ZR_UNLIKELY(zr_aot_arg1 == 0u || zr_aot_arg2 == 0u)) {\n"
            "        ZrCore_Debug_RunError(state, \"generated AOT unsigned three-arg modulo by zero\");\n"
            "        return (TZrUInt64)0;\n"
            "    }\n"
            "    return (TZrUInt64)(zr_aot_arg0 %% zr_aot_arg1 %% zr_aot_arg2);\n"
            "}\n",
            (unsigned)flatIndex);
}

TZrBool backend_aot_c_try_write_u64_three_arg_thunk_definition(FILE *file, const SZrAotFunctionEntry *entry) {
    if (file == ZR_NULL || entry == ZR_NULL) {
        return ZR_FALSE;
    }

    if (backend_aot_c_try_get_u64_arg0_arg1_arg2_add_return(entry->function)) {
        backend_aot_c_write_u64_three_arg_thunk_definition(
                file,
                entry->flatIndex,
                "    return (TZrUInt64)(zr_aot_arg0 + zr_aot_arg1 + zr_aot_arg2);\n");
        return ZR_TRUE;
    }

    if (backend_aot_c_try_get_u64_arg0_arg1_arg2_multiply_return(entry->function)) {
        backend_aot_c_write_u64_three_arg_thunk_definition(
                file,
                entry->flatIndex,
                "    return (TZrUInt64)(zr_aot_arg0 * zr_aot_arg1 * zr_aot_arg2);\n");
        return ZR_TRUE;
    }

    if (backend_aot_c_try_get_u64_arg0_arg1_arg2_subtract_return(entry->function)) {
        backend_aot_c_write_u64_three_arg_thunk_definition(
                file,
                entry->flatIndex,
                "    return (TZrUInt64)(zr_aot_arg0 - zr_aot_arg1 - zr_aot_arg2);\n");
        return ZR_TRUE;
    }

    if (backend_aot_c_try_get_u64_arg0_arg1_arg2_divide_return(entry->function)) {
        backend_aot_c_write_u64_three_arg_divide_thunk_definition(file, entry->flatIndex);
        return ZR_TRUE;
    }

    if (backend_aot_c_try_get_u64_arg0_arg1_arg2_modulo_return(entry->function)) {
        backend_aot_c_write_u64_three_arg_modulo_thunk_definition(file, entry->flatIndex);
        return ZR_TRUE;
    }

    if (backend_aot_c_try_get_u64_arg0_arg1_arg2_bitwise_and_return(entry->function)) {
        backend_aot_c_write_u64_three_arg_thunk_definition(
                file,
                entry->flatIndex,
                "    return (TZrUInt64)(zr_aot_arg0 & zr_aot_arg1 & zr_aot_arg2);\n");
        return ZR_TRUE;
    }

    if (backend_aot_c_try_get_u64_arg0_arg1_arg2_bitwise_or_return(entry->function)) {
        backend_aot_c_write_u64_three_arg_thunk_definition(
                file,
                entry->flatIndex,
                "    return (TZrUInt64)(zr_aot_arg0 | zr_aot_arg1 | zr_aot_arg2);\n");
        return ZR_TRUE;
    }

    if (backend_aot_c_try_get_u64_arg0_arg1_arg2_bitwise_xor_return(entry->function)) {
        backend_aot_c_write_u64_three_arg_thunk_definition(
                file,
                entry->flatIndex,
                "    return (TZrUInt64)(zr_aot_arg0 ^ zr_aot_arg1 ^ zr_aot_arg2);\n");
        return ZR_TRUE;
    }

    return ZR_FALSE;
}
