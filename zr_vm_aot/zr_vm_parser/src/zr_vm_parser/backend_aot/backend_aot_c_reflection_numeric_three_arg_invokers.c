#include "backend_aot_c_reflection_numeric_three_arg_invokers.h"

#include "backend_aot_c_typed_f64_thunks.h"
#include "backend_aot_c_typed_i64_thunks.h"
#include "backend_aot_c_typed_u64_three_arg_thunks.h"

static TZrBool backend_aot_c_method_metadata_has_i64_three_arg_reflection_case(
        const SZrAotFunctionTable *table) {
    if (table == ZR_NULL || table->entries == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < table->count; index++) {
        const SZrAotFunctionEntry *entry = &table->entries[index];
        if (backend_aot_c_can_emit_typed_i64_three_arg_thunk(entry->function)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void backend_aot_write_c_reflection_i64_three_arg_cases(FILE *file,
                                                               const SZrAotFunctionTable *table) {
    if (file == ZR_NULL ||
        table == ZR_NULL ||
        table->entries == ZR_NULL ||
        !backend_aot_c_method_metadata_has_i64_three_arg_reflection_case(table)) {
        return;
    }

    for (TZrUInt32 index = 0u; index < table->count; index++) {
        const SZrAotFunctionEntry *entry = &table->entries[index];
        if (!backend_aot_c_can_emit_typed_i64_three_arg_thunk(entry->function)) {
            continue;
        }

        if (backend_aot_c_can_emit_typed_i64_three_arg_state_free_thunk(entry->function)) {
            fprintf(file,
                    "        case %uu: {\n"
                    "            TZrInt64 zr_aot_arg0 = args[0].value.nativeObject.nativeInt64;\n"
                    "            TZrInt64 zr_aot_arg1 = args[1].value.nativeObject.nativeInt64;\n"
                    "            TZrInt64 zr_aot_arg2 = args[2].value.nativeObject.nativeInt64;\n"
                    "            TZrInt64 zr_aot_return_value = zr_aot_typed_i64_fn_%u(zr_aot_arg0, zr_aot_arg1, zr_aot_arg2);\n"
                    "            ZrCore_Value_InitAsInt(state, outReturn, zr_aot_return_value);\n"
                    "            return ZR_TRUE;\n"
                    "        }\n",
                    (unsigned)entry->flatIndex,
                    (unsigned)entry->flatIndex);
            continue;
        }

        fprintf(file,
                "        case %uu: {\n"
                "            TZrInt64 zr_aot_arg0 = args[0].value.nativeObject.nativeInt64;\n"
                "            TZrInt64 zr_aot_arg1 = args[1].value.nativeObject.nativeInt64;\n"
                "            TZrInt64 zr_aot_arg2 = args[2].value.nativeObject.nativeInt64;\n"
                "            TZrInt64 zr_aot_return_value = zr_aot_typed_i64_fn_%u(state, zr_aot_arg0, zr_aot_arg1, zr_aot_arg2);\n"
                "            ZrCore_Value_InitAsInt(state, outReturn, zr_aot_return_value);\n"
                "            return ZR_TRUE;\n"
                "        }\n",
                (unsigned)entry->flatIndex,
                (unsigned)entry->flatIndex);
    }
}

void backend_aot_write_c_reflection_i64_three_arg_invoker(FILE *file, const SZrAotFunctionTable *table) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "static TZrBool zr_aot_try_invoke_i64_three_arg(struct SZrState *state,\n"
            "                                               const SZrAotMethodInfo *method,\n"
            "                                               SZrTypeValue *args,\n"
            "                                               SZrTypeValue *outReturn) {\n"
            "    if (method == ZR_NULL ||\n"
            "        method->signature == ZR_NULL ||\n"
            "        !method->signature->hasReturnValue ||\n"
            "        method->signature->returnType == ZR_NULL ||\n"
            "        method->signature->returnType->baseType != (TZrUInt16)ZR_VALUE_TYPE_INT64 ||\n"
            "        method->signature->parameterCount != 3u ||\n"
            "        method->signature->parameterTypes == ZR_NULL ||\n"
            "        method->signature->parameterTypes[0].baseType != (TZrUInt16)ZR_VALUE_TYPE_INT64 ||\n"
            "        method->signature->parameterTypes[1].baseType != (TZrUInt16)ZR_VALUE_TYPE_INT64 ||\n"
            "        method->signature->parameterTypes[2].baseType != (TZrUInt16)ZR_VALUE_TYPE_INT64 ||\n"
            "        args == ZR_NULL ||\n"
            "        args[0].type != (TZrUInt16)ZR_VALUE_TYPE_INT64 ||\n"
            "        args[1].type != (TZrUInt16)ZR_VALUE_TYPE_INT64 ||\n"
            "        args[2].type != (TZrUInt16)ZR_VALUE_TYPE_INT64 ||\n"
            "        outReturn == ZR_NULL) {\n"
            "        return ZR_FALSE;\n"
            "    }\n"
            "    switch (method->functionIndex) {\n");
    backend_aot_write_c_reflection_i64_three_arg_cases(file, table);
    fprintf(file,
            "        default:\n"
            "            break;\n"
            "    }\n"
            "    return ZR_FALSE;\n"
            "}\n");
}

static TZrBool backend_aot_c_method_metadata_has_u64_three_arg_reflection_case(
        const SZrAotFunctionTable *table) {
    if (table == ZR_NULL || table->entries == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < table->count; index++) {
        const SZrAotFunctionEntry *entry = &table->entries[index];
        if (backend_aot_c_can_emit_typed_u64_three_arg_thunk(entry->function)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void backend_aot_write_c_reflection_u64_three_arg_cases(FILE *file,
                                                               const SZrAotFunctionTable *table) {
    if (file == ZR_NULL ||
        table == ZR_NULL ||
        table->entries == ZR_NULL ||
        !backend_aot_c_method_metadata_has_u64_three_arg_reflection_case(table)) {
        return;
    }

    for (TZrUInt32 index = 0u; index < table->count; index++) {
        const SZrAotFunctionEntry *entry = &table->entries[index];
        if (!backend_aot_c_can_emit_typed_u64_three_arg_thunk(entry->function)) {
            continue;
        }

        if (backend_aot_c_can_emit_typed_u64_three_arg_state_free_thunk(entry->function)) {
            fprintf(file,
                    "        case %uu: {\n"
                    "            TZrUInt64 zr_aot_arg0 = args[0].value.nativeObject.nativeUInt64;\n"
                    "            TZrUInt64 zr_aot_arg1 = args[1].value.nativeObject.nativeUInt64;\n"
                    "            TZrUInt64 zr_aot_arg2 = args[2].value.nativeObject.nativeUInt64;\n"
                    "            TZrUInt64 zr_aot_return_value = zr_aot_typed_u64_fn_%u(zr_aot_arg0, zr_aot_arg1, zr_aot_arg2);\n"
                    "            ZrCore_Value_InitAsUInt(state, outReturn, zr_aot_return_value);\n"
                    "            return ZR_TRUE;\n"
                    "        }\n",
                    (unsigned)entry->flatIndex,
                    (unsigned)entry->flatIndex);
            continue;
        }

        fprintf(file,
                "        case %uu: {\n"
                "            TZrUInt64 zr_aot_arg0 = args[0].value.nativeObject.nativeUInt64;\n"
                "            TZrUInt64 zr_aot_arg1 = args[1].value.nativeObject.nativeUInt64;\n"
                "            TZrUInt64 zr_aot_arg2 = args[2].value.nativeObject.nativeUInt64;\n"
                "            TZrUInt64 zr_aot_return_value = zr_aot_typed_u64_fn_%u(state, zr_aot_arg0, zr_aot_arg1, zr_aot_arg2);\n"
                "            ZrCore_Value_InitAsUInt(state, outReturn, zr_aot_return_value);\n"
                "            return ZR_TRUE;\n"
                "        }\n",
                (unsigned)entry->flatIndex,
                (unsigned)entry->flatIndex);
    }
}

void backend_aot_write_c_reflection_u64_three_arg_invoker(FILE *file, const SZrAotFunctionTable *table) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "static TZrBool zr_aot_try_invoke_u64_three_arg(struct SZrState *state,\n"
            "                                               const SZrAotMethodInfo *method,\n"
            "                                               SZrTypeValue *args,\n"
            "                                               SZrTypeValue *outReturn) {\n"
            "    if (method == ZR_NULL ||\n"
            "        method->signature == ZR_NULL ||\n"
            "        !method->signature->hasReturnValue ||\n"
            "        method->signature->returnType == ZR_NULL ||\n"
            "        method->signature->returnType->baseType != (TZrUInt16)ZR_VALUE_TYPE_UINT64 ||\n"
            "        method->signature->parameterCount != 3u ||\n"
            "        method->signature->parameterTypes == ZR_NULL ||\n"
            "        method->signature->parameterTypes[0].baseType != (TZrUInt16)ZR_VALUE_TYPE_UINT64 ||\n"
            "        method->signature->parameterTypes[1].baseType != (TZrUInt16)ZR_VALUE_TYPE_UINT64 ||\n"
            "        method->signature->parameterTypes[2].baseType != (TZrUInt16)ZR_VALUE_TYPE_UINT64 ||\n"
            "        args == ZR_NULL ||\n"
            "        args[0].type != (TZrUInt16)ZR_VALUE_TYPE_UINT64 ||\n"
            "        args[1].type != (TZrUInt16)ZR_VALUE_TYPE_UINT64 ||\n"
            "        args[2].type != (TZrUInt16)ZR_VALUE_TYPE_UINT64 ||\n"
            "        outReturn == ZR_NULL) {\n"
            "        return ZR_FALSE;\n"
            "    }\n"
            "    switch (method->functionIndex) {\n");
    backend_aot_write_c_reflection_u64_three_arg_cases(file, table);
    fprintf(file,
            "        default:\n"
            "            break;\n"
            "    }\n"
            "    return ZR_FALSE;\n"
            "}\n");
}

static TZrBool backend_aot_c_method_metadata_has_f64_three_arg_reflection_case(
        const SZrAotFunctionTable *table) {
    if (table == ZR_NULL || table->entries == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < table->count; index++) {
        const SZrAotFunctionEntry *entry = &table->entries[index];
        if (backend_aot_c_can_emit_typed_f64_three_arg_thunk(entry->function)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void backend_aot_write_c_reflection_f64_three_arg_cases(FILE *file,
                                                               const SZrAotFunctionTable *table) {
    if (file == ZR_NULL ||
        table == ZR_NULL ||
        table->entries == ZR_NULL ||
        !backend_aot_c_method_metadata_has_f64_three_arg_reflection_case(table)) {
        return;
    }

    for (TZrUInt32 index = 0u; index < table->count; index++) {
        const SZrAotFunctionEntry *entry = &table->entries[index];
        if (!backend_aot_c_can_emit_typed_f64_three_arg_thunk(entry->function)) {
            continue;
        }

        if (backend_aot_c_can_emit_typed_f64_three_arg_state_free_thunk(entry->function)) {
            fprintf(file,
                    "        case %uu: {\n"
                    "            TZrFloat64 zr_aot_arg0 = args[0].value.nativeObject.nativeDouble;\n"
                    "            TZrFloat64 zr_aot_arg1 = args[1].value.nativeObject.nativeDouble;\n"
                    "            TZrFloat64 zr_aot_arg2 = args[2].value.nativeObject.nativeDouble;\n"
                    "            TZrFloat64 zr_aot_return_value = zr_aot_typed_f64_fn_%u(zr_aot_arg0, zr_aot_arg1, zr_aot_arg2);\n"
                    "            ZrCore_Value_InitAsFloat(state, outReturn, zr_aot_return_value);\n"
                    "            return ZR_TRUE;\n"
                    "        }\n",
                    (unsigned)entry->flatIndex,
                    (unsigned)entry->flatIndex);
            continue;
        }

        fprintf(file,
                "        case %uu: {\n"
                "            TZrFloat64 zr_aot_arg0 = args[0].value.nativeObject.nativeDouble;\n"
                "            TZrFloat64 zr_aot_arg1 = args[1].value.nativeObject.nativeDouble;\n"
                "            TZrFloat64 zr_aot_arg2 = args[2].value.nativeObject.nativeDouble;\n"
                "            TZrFloat64 zr_aot_return_value = zr_aot_typed_f64_fn_%u(state, zr_aot_arg0, zr_aot_arg1, zr_aot_arg2);\n"
                "            ZrCore_Value_InitAsFloat(state, outReturn, zr_aot_return_value);\n"
                "            return ZR_TRUE;\n"
                "        }\n",
                (unsigned)entry->flatIndex,
                (unsigned)entry->flatIndex);
    }
}

void backend_aot_write_c_reflection_f64_three_arg_invoker(FILE *file, const SZrAotFunctionTable *table) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "static TZrBool zr_aot_try_invoke_f64_three_arg(struct SZrState *state,\n"
            "                                               const SZrAotMethodInfo *method,\n"
            "                                               SZrTypeValue *args,\n"
            "                                               SZrTypeValue *outReturn) {\n"
            "    if (method == ZR_NULL ||\n"
            "        method->signature == ZR_NULL ||\n"
            "        !method->signature->hasReturnValue ||\n"
            "        method->signature->returnType == ZR_NULL ||\n"
            "        method->signature->returnType->baseType != (TZrUInt16)ZR_VALUE_TYPE_DOUBLE ||\n"
            "        method->signature->parameterCount != 3u ||\n"
            "        method->signature->parameterTypes == ZR_NULL ||\n"
            "        method->signature->parameterTypes[0].baseType != (TZrUInt16)ZR_VALUE_TYPE_DOUBLE ||\n"
            "        method->signature->parameterTypes[1].baseType != (TZrUInt16)ZR_VALUE_TYPE_DOUBLE ||\n"
            "        method->signature->parameterTypes[2].baseType != (TZrUInt16)ZR_VALUE_TYPE_DOUBLE ||\n"
            "        args == ZR_NULL ||\n"
            "        args[0].type != (TZrUInt16)ZR_VALUE_TYPE_DOUBLE ||\n"
            "        args[1].type != (TZrUInt16)ZR_VALUE_TYPE_DOUBLE ||\n"
            "        args[2].type != (TZrUInt16)ZR_VALUE_TYPE_DOUBLE ||\n"
            "        outReturn == ZR_NULL) {\n"
            "        return ZR_FALSE;\n"
            "    }\n"
            "    switch (method->functionIndex) {\n");
    backend_aot_write_c_reflection_f64_three_arg_cases(file, table);
    fprintf(file,
            "        default:\n"
            "            break;\n"
            "    }\n"
            "    return ZR_FALSE;\n"
            "}\n");
}
