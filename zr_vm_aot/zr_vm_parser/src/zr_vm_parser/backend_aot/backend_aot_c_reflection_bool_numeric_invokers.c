#include "backend_aot_c_reflection_bool_numeric_invokers.h"

#include "backend_aot_c_typed_bool_thunks.h"

static TZrBool backend_aot_c_method_metadata_has_bool_i64_two_arg_reflection_case(
        const SZrAotFunctionTable *table) {
    if (table == ZR_NULL || table->entries == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < table->count; index++) {
        const SZrAotFunctionEntry *entry = &table->entries[index];
        if (backend_aot_c_can_emit_typed_bool_i64_two_arg_thunk(entry->function)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void backend_aot_write_c_reflection_bool_i64_two_arg_cases(FILE *file,
                                                                  const SZrAotFunctionTable *table) {
    if (file == ZR_NULL ||
        table == ZR_NULL ||
        table->entries == ZR_NULL ||
        !backend_aot_c_method_metadata_has_bool_i64_two_arg_reflection_case(table)) {
        return;
    }

    for (TZrUInt32 index = 0u; index < table->count; index++) {
        const SZrAotFunctionEntry *entry = &table->entries[index];
        if (!backend_aot_c_can_emit_typed_bool_i64_two_arg_thunk(entry->function)) {
            continue;
        }

        fprintf(file,
                "        case %uu: {\n"
                "            TZrInt64 zr_aot_arg0 = args[0].value.nativeObject.nativeInt64;\n"
                "            TZrInt64 zr_aot_arg1 = args[1].value.nativeObject.nativeInt64;\n"
                "            TZrBool zr_aot_return_value = zr_aot_typed_bool_fn_%u(zr_aot_arg0, zr_aot_arg1);\n"
                "            ZrCore_Value_InitAsBool(state, outReturn, zr_aot_return_value);\n"
                "            return ZR_TRUE;\n"
                "        }\n",
                (unsigned)entry->flatIndex,
                (unsigned)entry->flatIndex);
    }
}

static TZrBool backend_aot_c_method_metadata_has_bool_u64_two_arg_reflection_case(
        const SZrAotFunctionTable *table) {
    if (table == ZR_NULL || table->entries == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < table->count; index++) {
        const SZrAotFunctionEntry *entry = &table->entries[index];
        if (backend_aot_c_can_emit_typed_bool_u64_two_arg_thunk(entry->function)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void backend_aot_write_c_reflection_bool_u64_two_arg_cases(FILE *file,
                                                                  const SZrAotFunctionTable *table) {
    if (file == ZR_NULL ||
        table == ZR_NULL ||
        table->entries == ZR_NULL ||
        !backend_aot_c_method_metadata_has_bool_u64_two_arg_reflection_case(table)) {
        return;
    }

    for (TZrUInt32 index = 0u; index < table->count; index++) {
        const SZrAotFunctionEntry *entry = &table->entries[index];
        if (!backend_aot_c_can_emit_typed_bool_u64_two_arg_thunk(entry->function)) {
            continue;
        }

        fprintf(file,
                "        case %uu: {\n"
                "            TZrUInt64 zr_aot_arg0 = args[0].value.nativeObject.nativeUInt64;\n"
                "            TZrUInt64 zr_aot_arg1 = args[1].value.nativeObject.nativeUInt64;\n"
                "            TZrBool zr_aot_return_value = zr_aot_typed_bool_fn_%u(zr_aot_arg0, zr_aot_arg1);\n"
                "            ZrCore_Value_InitAsBool(state, outReturn, zr_aot_return_value);\n"
                "            return ZR_TRUE;\n"
                "        }\n",
                (unsigned)entry->flatIndex,
                (unsigned)entry->flatIndex);
    }
}

static TZrBool backend_aot_c_method_metadata_has_bool_f64_two_arg_reflection_case(
        const SZrAotFunctionTable *table) {
    if (table == ZR_NULL || table->entries == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < table->count; index++) {
        const SZrAotFunctionEntry *entry = &table->entries[index];
        if (backend_aot_c_can_emit_typed_bool_f64_two_arg_thunk(entry->function)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void backend_aot_write_c_reflection_bool_f64_two_arg_cases(FILE *file,
                                                                  const SZrAotFunctionTable *table) {
    if (file == ZR_NULL ||
        table == ZR_NULL ||
        table->entries == ZR_NULL ||
        !backend_aot_c_method_metadata_has_bool_f64_two_arg_reflection_case(table)) {
        return;
    }

    for (TZrUInt32 index = 0u; index < table->count; index++) {
        const SZrAotFunctionEntry *entry = &table->entries[index];
        if (!backend_aot_c_can_emit_typed_bool_f64_two_arg_thunk(entry->function)) {
            continue;
        }

        fprintf(file,
                "        case %uu: {\n"
                "            TZrFloat64 zr_aot_arg0 = args[0].value.nativeObject.nativeDouble;\n"
                "            TZrFloat64 zr_aot_arg1 = args[1].value.nativeObject.nativeDouble;\n"
                "            TZrBool zr_aot_return_value = zr_aot_typed_bool_fn_%u(zr_aot_arg0, zr_aot_arg1);\n"
                "            ZrCore_Value_InitAsBool(state, outReturn, zr_aot_return_value);\n"
                "            return ZR_TRUE;\n"
                "        }\n",
                (unsigned)entry->flatIndex,
                (unsigned)entry->flatIndex);
    }
}

void backend_aot_write_c_reflection_bool_i64_two_arg_invoker(FILE *file, const SZrAotFunctionTable *table) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "static TZrBool zr_aot_try_invoke_bool_i64_two_arg(struct SZrState *state,\n"
            "                                                  const SZrAotMethodInfo *method,\n"
            "                                                  SZrTypeValue *args,\n"
            "                                                  SZrTypeValue *outReturn) {\n"
            "    if (method == ZR_NULL ||\n"
            "        method->signature == ZR_NULL ||\n"
            "        !method->signature->hasReturnValue ||\n"
            "        method->signature->returnType == ZR_NULL ||\n"
            "        method->signature->returnType->baseType != (TZrUInt16)ZR_VALUE_TYPE_BOOL ||\n"
            "        method->signature->parameterCount != 2u ||\n"
            "        method->signature->parameterTypes == ZR_NULL ||\n"
            "        method->signature->parameterTypes[0].baseType != (TZrUInt16)ZR_VALUE_TYPE_INT64 ||\n"
            "        method->signature->parameterTypes[1].baseType != (TZrUInt16)ZR_VALUE_TYPE_INT64 ||\n"
            "        args == ZR_NULL ||\n"
            "        args[0].type != (TZrUInt16)ZR_VALUE_TYPE_INT64 ||\n"
            "        args[1].type != (TZrUInt16)ZR_VALUE_TYPE_INT64 ||\n"
            "        outReturn == ZR_NULL) {\n"
            "        return ZR_FALSE;\n"
            "    }\n"
            "    switch (method->functionIndex) {\n");
    backend_aot_write_c_reflection_bool_i64_two_arg_cases(file, table);
    fprintf(file,
            "        default:\n"
            "            break;\n"
            "    }\n"
            "    return ZR_FALSE;\n"
            "}\n");
}

void backend_aot_write_c_reflection_bool_u64_two_arg_invoker(FILE *file, const SZrAotFunctionTable *table) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "static TZrBool zr_aot_try_invoke_bool_u64_two_arg(struct SZrState *state,\n"
            "                                                  const SZrAotMethodInfo *method,\n"
            "                                                  SZrTypeValue *args,\n"
            "                                                  SZrTypeValue *outReturn) {\n"
            "    if (method == ZR_NULL ||\n"
            "        method->signature == ZR_NULL ||\n"
            "        !method->signature->hasReturnValue ||\n"
            "        method->signature->returnType == ZR_NULL ||\n"
            "        method->signature->returnType->baseType != (TZrUInt16)ZR_VALUE_TYPE_BOOL ||\n"
            "        method->signature->parameterCount != 2u ||\n"
            "        method->signature->parameterTypes == ZR_NULL ||\n"
            "        method->signature->parameterTypes[0].baseType != (TZrUInt16)ZR_VALUE_TYPE_UINT64 ||\n"
            "        method->signature->parameterTypes[1].baseType != (TZrUInt16)ZR_VALUE_TYPE_UINT64 ||\n"
            "        args == ZR_NULL ||\n"
            "        args[0].type != (TZrUInt16)ZR_VALUE_TYPE_UINT64 ||\n"
            "        args[1].type != (TZrUInt16)ZR_VALUE_TYPE_UINT64 ||\n"
            "        outReturn == ZR_NULL) {\n"
            "        return ZR_FALSE;\n"
            "    }\n"
            "    switch (method->functionIndex) {\n");
    backend_aot_write_c_reflection_bool_u64_two_arg_cases(file, table);
    fprintf(file,
            "        default:\n"
            "            break;\n"
            "    }\n"
            "    return ZR_FALSE;\n"
            "}\n");
}

void backend_aot_write_c_reflection_bool_f64_two_arg_invoker(FILE *file, const SZrAotFunctionTable *table) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "static TZrBool zr_aot_try_invoke_bool_f64_two_arg(struct SZrState *state,\n"
            "                                                  const SZrAotMethodInfo *method,\n"
            "                                                  SZrTypeValue *args,\n"
            "                                                  SZrTypeValue *outReturn) {\n"
            "    if (method == ZR_NULL ||\n"
            "        method->signature == ZR_NULL ||\n"
            "        !method->signature->hasReturnValue ||\n"
            "        method->signature->returnType == ZR_NULL ||\n"
            "        method->signature->returnType->baseType != (TZrUInt16)ZR_VALUE_TYPE_BOOL ||\n"
            "        method->signature->parameterCount != 2u ||\n"
            "        method->signature->parameterTypes == ZR_NULL ||\n"
            "        method->signature->parameterTypes[0].baseType != (TZrUInt16)ZR_VALUE_TYPE_DOUBLE ||\n"
            "        method->signature->parameterTypes[1].baseType != (TZrUInt16)ZR_VALUE_TYPE_DOUBLE ||\n"
            "        args == ZR_NULL ||\n"
            "        args[0].type != (TZrUInt16)ZR_VALUE_TYPE_DOUBLE ||\n"
            "        args[1].type != (TZrUInt16)ZR_VALUE_TYPE_DOUBLE ||\n"
            "        outReturn == ZR_NULL) {\n"
            "        return ZR_FALSE;\n"
            "    }\n"
            "    switch (method->functionIndex) {\n");
    backend_aot_write_c_reflection_bool_f64_two_arg_cases(file, table);
    fprintf(file,
            "        default:\n"
            "            break;\n"
            "    }\n"
            "    return ZR_FALSE;\n"
            "}\n");
}
