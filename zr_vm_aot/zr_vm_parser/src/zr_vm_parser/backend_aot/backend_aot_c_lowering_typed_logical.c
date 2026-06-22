#include "backend_aot_c_emitter.h"
#include "backend_aot_c_scalar_locals.h"

static void backend_aot_write_c_direct_bool_comparison(FILE *file,
                                                       const char *expressionText,
                                                       const char *operatorText,
                                                       const SZrAotExecIrFunction *functionIr,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 leftSlot,
                                                       TZrUInt32 rightSlot,
                                                       TZrUInt32 execInstructionIndex) {
    TZrBool useScalarLocal;
    TZrBool syncBoolLocal;

    if (file == ZR_NULL || expressionText == ZR_NULL || operatorText == ZR_NULL) {
        return;
    }

    useScalarLocal = (TZrBool)(
            backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(functionIr, destinationSlot, execInstructionIndex) &&
            backend_aot_c_scalar_locals_bool_written_before(functionIr, leftSlot, execInstructionIndex) &&
            backend_aot_c_scalar_locals_bool_written_before(functionIr, rightSlot, execInstructionIndex));
    if (useScalarLocal) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_bool_compare_scalar_local */\n"
                "        zr_aot_b%u = (TZrBool)((zr_aot_b%u %s zr_aot_b%u) != 0u);\n"
                "    }\n",
                (unsigned)destinationSlot,
                (unsigned)leftSlot,
                operatorText,
                (unsigned)rightSlot);
        return;
    }

    syncBoolLocal = backend_aot_c_scalar_locals_has_bool_slot(functionIr, destinationSlot);
    fprintf(file,
            "    {\n"
            "        /* zr_aot_bool_compare_exec */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_left = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_right = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrBool zr_aot_left_bool;\n"
            "        TZrBool zr_aot_right_bool;\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_left == ZR_NULL || zr_aot_right == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (!ZR_VALUE_IS_TYPE_BOOL(zr_aot_left->type) || !ZR_VALUE_IS_TYPE_BOOL(zr_aot_right->type)) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_left_bool = (TZrBool)(zr_aot_left->value.nativeObject.nativeBool != 0u);\n"
            "        zr_aot_right_bool = (TZrBool)(zr_aot_right->value.nativeObject.nativeBool != 0u);\n"
            "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                          nativeBool,\n"
            "                          %s,\n"
            "                          ZR_VALUE_TYPE_BOOL);\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot,
            expressionText);
    if (syncBoolLocal) {
        fprintf(file,
                "        zr_aot_b%u = (TZrBool)((%s) != 0u);\n",
                (unsigned)destinationSlot,
                expressionText);
    }
    fprintf(file, "    }\n");
}

void backend_aot_write_c_direct_logical_equal_bool(FILE *file,
                                                   const SZrAotExecIrFunction *functionIr,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 leftSlot,
                                                   TZrUInt32 rightSlot,
                                                   TZrUInt32 execInstructionIndex) {
    backend_aot_write_c_direct_bool_comparison(file,
                                               "zr_aot_left_bool == zr_aot_right_bool",
                                               "==",
                                               functionIr,
                                               destinationSlot,
                                               leftSlot,
                                               rightSlot,
                                               execInstructionIndex);
}

void backend_aot_write_c_direct_logical_not_equal_bool(FILE *file,
                                                       const SZrAotExecIrFunction *functionIr,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 leftSlot,
                                                       TZrUInt32 rightSlot,
                                                       TZrUInt32 execInstructionIndex) {
    backend_aot_write_c_direct_bool_comparison(file,
                                               "zr_aot_left_bool != zr_aot_right_bool",
                                               "!=",
                                               functionIr,
                                               destinationSlot,
                                               leftSlot,
                                               rightSlot,
                                               execInstructionIndex);
}

void backend_aot_write_c_direct_logical_not_bool(FILE *file,
                                                 const SZrAotExecIrFunction *functionIr,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 sourceSlot,
                                                 TZrUInt32 execInstructionIndex) {
    TZrBool useScalarLocal;
    TZrBool syncBoolLocal;

    if (file == ZR_NULL) {
        return;
    }

    useScalarLocal = (TZrBool)(
            backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(functionIr, destinationSlot, execInstructionIndex) &&
            backend_aot_c_scalar_locals_bool_written_before(functionIr, sourceSlot, execInstructionIndex));
    if (useScalarLocal) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_bool_not_scalar_local */\n"
                "        zr_aot_b%u = (TZrBool)(!zr_aot_b%u != 0u);\n"
                "    }\n",
                (unsigned)destinationSlot,
                (unsigned)sourceSlot);
        return;
    }

    syncBoolLocal = backend_aot_c_scalar_locals_has_bool_slot(functionIr, destinationSlot);
    fprintf(file,
            "    {\n"
            "        /* zr_aot_bool_not_exec */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_source = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrBool zr_aot_source_bool;\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_source == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (!ZR_VALUE_IS_TYPE_BOOL(zr_aot_source->type)) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_source_bool = (TZrBool)(zr_aot_source->value.nativeObject.nativeBool != 0u);\n"
            "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                          nativeBool,\n"
            "                          !zr_aot_source_bool,\n"
            "                          ZR_VALUE_TYPE_BOOL);\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
    if (syncBoolLocal) {
        fprintf(file,
                "        zr_aot_b%u = (TZrBool)(!zr_aot_source_bool != 0u);\n",
                (unsigned)destinationSlot);
    }
    fprintf(file, "    }\n");
}
