#include "backend_aot_c_emitter.h"

#include "zr_vm_core/closure.h"

static void backend_aot_write_c_direct_ownership_call(FILE *file,
                                                      const char *helperName,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 sourceSlot) {
    if (file == ZR_NULL || helperName == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    ZR_AOT_C_GUARD(%s(state, &frame, %u, %u));\n",
            helperName,
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
}

void backend_aot_write_c_direct_own_unique(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    backend_aot_write_c_direct_ownership_call(file,
                                              "ZrLibrary_AotRuntime_OwnUnique",
                                              destinationSlot,
                                              sourceSlot);
}

void backend_aot_write_c_direct_own_borrow(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    backend_aot_write_c_direct_ownership_call(file,
                                              "ZrLibrary_AotRuntime_OwnBorrow",
                                              destinationSlot,
                                              sourceSlot);
}

void backend_aot_write_c_direct_own_loan(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    backend_aot_write_c_direct_ownership_call(file,
                                              "ZrLibrary_AotRuntime_OwnLoan",
                                              destinationSlot,
                                              sourceSlot);
}

void backend_aot_write_c_direct_own_share(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    backend_aot_write_c_direct_ownership_call(file,
                                              "ZrLibrary_AotRuntime_OwnShare",
                                              destinationSlot,
                                              sourceSlot);
}

void backend_aot_write_c_direct_own_weak(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    backend_aot_write_c_direct_ownership_call(file,
                                              "ZrLibrary_AotRuntime_OwnWeak",
                                              destinationSlot,
                                              sourceSlot);
}

void backend_aot_write_c_direct_own_detach(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    backend_aot_write_c_direct_ownership_call(file,
                                              "ZrLibrary_AotRuntime_OwnDetach",
                                              destinationSlot,
                                              sourceSlot);
}

void backend_aot_write_c_direct_own_upgrade(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    backend_aot_write_c_direct_ownership_call(file,
                                              "ZrLibrary_AotRuntime_OwnUpgrade",
                                              destinationSlot,
                                              sourceSlot);
}

void backend_aot_write_c_direct_own_release(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    backend_aot_write_c_direct_ownership_call(file,
                                              "ZrLibrary_AotRuntime_OwnRelease",
                                              destinationSlot,
                                              sourceSlot);
}

void backend_aot_write_c_direct_mul_signed(FILE *file,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 leftSlot,
                                           TZrUInt32 rightSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_MulSigned(state, &frame, %u, %u, %u));\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
}

void backend_aot_write_c_direct_mul_signed_const(FILE *file,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 leftSlot,
                                                 TZrUInt32 constantIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_MulSignedConst(state, &frame, %u, %u, %u));\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)constantIndex);
}

void backend_aot_write_c_direct_mul(FILE *file,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 leftSlot,
                                    TZrUInt32 rightSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_Mul(state, &frame, %u, %u, %u));\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
}

void backend_aot_write_c_direct_sub(FILE *file,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 leftSlot,
                                    TZrUInt32 rightSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_Sub(state, &frame, %u, %u, %u));\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
}

void backend_aot_write_c_direct_sub_int(FILE *file,
                                        TZrUInt32 destinationSlot,
                                        TZrUInt32 leftSlot,
                                        TZrUInt32 rightSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_left = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_right = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrBool zr_aot_use_runtime_sub = ZR_FALSE;\n"
            "        if (ZR_VALUE_IS_TYPE_INT(zr_aot_left->type) && ZR_VALUE_IS_TYPE_INT(zr_aot_right->type)) {\n"
            "            TZrInt64 zr_aot_left_int = ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_left->type)\n"
            "                                              ? zr_aot_left->value.nativeObject.nativeInt64\n"
            "                                              : (TZrInt64)zr_aot_left->value.nativeObject.nativeUInt64;\n"
            "            TZrInt64 zr_aot_right_int = ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_right->type)\n"
            "                                               ? zr_aot_right->value.nativeObject.nativeInt64\n"
            "                                               : (TZrInt64)zr_aot_right->value.nativeObject.nativeUInt64;\n"
            "            ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                              nativeInt64,\n"
            "                              zr_aot_left_int - zr_aot_right_int,\n"
            "                              zr_aot_left->type);\n"
            "        } else {\n"
            "            zr_aot_use_runtime_sub = ZR_TRUE;\n"
            "        }\n"
            "        if (zr_aot_use_runtime_sub) {\n"
            "            ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SubInt(state, &frame, %u, %u, %u));\n"
            "        }\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot,
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
}

void backend_aot_write_c_direct_sub_int_const(FILE *file,
                                              TZrUInt32 destinationSlot,
                                              TZrUInt32 leftSlot,
                                              TZrUInt32 constantIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SubIntConst(state, &frame, %u, %u, %u));\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)constantIndex);
}

void backend_aot_write_c_direct_bitwise_xor(FILE *file,
                                            TZrUInt32 destinationSlot,
                                            TZrUInt32 leftSlot,
                                            TZrUInt32 rightSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_left = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_right = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrBool zr_aot_use_runtime_xor = ZR_FALSE;\n"
            "        if (ZR_VALUE_IS_TYPE_INT(zr_aot_left->type) && ZR_VALUE_IS_TYPE_INT(zr_aot_right->type)) {\n"
            "            TZrInt64 zr_aot_left_int = ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_left->type)\n"
            "                                              ? zr_aot_left->value.nativeObject.nativeInt64\n"
            "                                              : (TZrInt64)zr_aot_left->value.nativeObject.nativeUInt64;\n"
            "            TZrInt64 zr_aot_right_int = ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_right->type)\n"
            "                                               ? zr_aot_right->value.nativeObject.nativeInt64\n"
            "                                               : (TZrInt64)zr_aot_right->value.nativeObject.nativeUInt64;\n"
            "            ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                              nativeInt64,\n"
            "                              zr_aot_left_int ^ zr_aot_right_int,\n"
            "                              ZR_VALUE_TYPE_INT64);\n"
            "        } else {\n"
            "            zr_aot_use_runtime_xor = ZR_TRUE;\n"
            "        }\n"
            "        if (zr_aot_use_runtime_xor) {\n"
            "            ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_BitwiseXor(state, &frame, %u, %u, %u));\n"
            "        }\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot,
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
}

static void backend_aot_write_c_direct_cached_meta_call(FILE *file,
                                                        const char *helperName,
                                                        TZrUInt32 destinationSlot,
                                                        TZrUInt32 operandSlot,
                                                        TZrUInt32 memberOrCacheIndex) {
    if (file == ZR_NULL || helperName == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    ZR_AOT_C_GUARD(%s(state, &frame, %u, %u, %u));\n",
            helperName,
            (unsigned)destinationSlot,
            (unsigned)operandSlot,
            (unsigned)memberOrCacheIndex);
}

void backend_aot_write_c_direct_meta_get(FILE *file,
                                         TZrUInt32 destinationSlot,
                                         TZrUInt32 receiverSlot,
                                         TZrUInt32 memberId) {
    backend_aot_write_c_direct_cached_meta_call(file,
                                                "ZrLibrary_AotRuntime_MetaGet",
                                                destinationSlot,
                                                receiverSlot,
                                                memberId);
}

void backend_aot_write_c_direct_meta_set(FILE *file,
                                         TZrUInt32 receiverAndResultSlot,
                                         TZrUInt32 assignedValueSlot,
                                         TZrUInt32 memberId) {
    backend_aot_write_c_direct_cached_meta_call(file,
                                                "ZrLibrary_AotRuntime_MetaSet",
                                                receiverAndResultSlot,
                                                assignedValueSlot,
                                                memberId);
}

void backend_aot_write_c_direct_meta_get_cached(FILE *file,
                                                TZrUInt32 destinationSlot,
                                                TZrUInt32 receiverSlot,
                                                TZrUInt32 cacheIndex) {
    backend_aot_write_c_direct_cached_meta_call(file,
                                                "ZrLibrary_AotRuntime_MetaGetCached",
                                                destinationSlot,
                                                receiverSlot,
                                                cacheIndex);
}

void backend_aot_write_c_direct_meta_set_cached(FILE *file,
                                                TZrUInt32 receiverAndResultSlot,
                                                TZrUInt32 assignedValueSlot,
                                                TZrUInt32 cacheIndex) {
    backend_aot_write_c_direct_cached_meta_call(file,
                                                "ZrLibrary_AotRuntime_MetaSetCached",
                                                receiverAndResultSlot,
                                                assignedValueSlot,
                                                cacheIndex);
}

void backend_aot_write_c_direct_meta_get_static_cached(FILE *file,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 receiverSlot,
                                                       TZrUInt32 cacheIndex) {
    backend_aot_write_c_direct_cached_meta_call(file,
                                                "ZrLibrary_AotRuntime_MetaGetStaticCached",
                                                destinationSlot,
                                                receiverSlot,
                                                cacheIndex);
}

void backend_aot_write_c_direct_meta_set_static_cached(FILE *file,
                                                       TZrUInt32 receiverAndResultSlot,
                                                       TZrUInt32 assignedValueSlot,
                                                       TZrUInt32 cacheIndex) {
    backend_aot_write_c_direct_cached_meta_call(file,
                                                "ZrLibrary_AotRuntime_MetaSetStaticCached",
                                                receiverAndResultSlot,
                                                assignedValueSlot,
                                                cacheIndex);
}

void backend_aot_write_c_direct_logical_equal(FILE *file,
                                              TZrUInt32 destinationSlot,
                                              TZrUInt32 leftSlot,
                                              TZrUInt32 rightSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_LogicalEqual(state, &frame, %u, %u, %u));\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
}

void backend_aot_write_c_direct_logical_not_equal(FILE *file,
                                                  TZrUInt32 destinationSlot,
                                                  TZrUInt32 leftSlot,
                                                  TZrUInt32 rightSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        SZrTypeValue *zr_aot_destination;\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_LogicalEqual(state, &frame, %u, %u, %u));\n"
            "        zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        if (zr_aot_destination == ZR_NULL || !ZR_VALUE_IS_TYPE_BOOL(zr_aot_destination->type)) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_destination->value.nativeObject.nativeBool =\n"
            "                (TZrBool)!zr_aot_destination->value.nativeObject.nativeBool;\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot,
            (unsigned)destinationSlot);
}

void backend_aot_write_c_direct_logical_less_signed(FILE *file,
                                                    TZrUInt32 destinationSlot,
                                                    TZrUInt32 leftSlot,
                                                    TZrUInt32 rightSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_left = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_right = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrInt64 zr_aot_left_int;\n"
            "        TZrInt64 zr_aot_right_int;\n"
            "        if (ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_left->type)) {\n"
            "            zr_aot_left_int = zr_aot_left->value.nativeObject.nativeInt64;\n"
            "        } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_left->type)) {\n"
            "            zr_aot_left_int = (TZrInt64)zr_aot_left->value.nativeObject.nativeUInt64;\n"
            "        } else {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_right->type)) {\n"
            "            zr_aot_right_int = zr_aot_right->value.nativeObject.nativeInt64;\n"
            "        } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_right->type)) {\n"
            "            zr_aot_right_int = (TZrInt64)zr_aot_right->value.nativeObject.nativeUInt64;\n"
            "        } else {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                          nativeBool,\n"
            "                          zr_aot_left_int < zr_aot_right_int,\n"
            "                          ZR_VALUE_TYPE_BOOL);\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
}

void backend_aot_write_c_direct_logical_greater_signed(FILE *file,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 leftSlot,
                                                       TZrUInt32 rightSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_LogicalGreaterSigned(state, &frame, %u, %u, %u));\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
}

void backend_aot_write_c_direct_logical_less_equal_signed(FILE *file,
                                                          TZrUInt32 destinationSlot,
                                                          TZrUInt32 leftSlot,
                                                          TZrUInt32 rightSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_LogicalLessEqualSigned(state, &frame, %u, %u, %u));\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
}

void backend_aot_write_c_direct_logical_greater_equal_signed(FILE *file,
                                                             TZrUInt32 destinationSlot,
                                                             TZrUInt32 leftSlot,
                                                             TZrUInt32 rightSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_LogicalGreaterEqualSigned(state, &frame, %u, %u, %u));\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
}

void backend_aot_write_c_direct_mod(FILE *file,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 leftSlot,
                                    TZrUInt32 rightSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_Mod(state, &frame, %u, %u, %u));\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
}

void backend_aot_write_c_direct_mod_signed_const(FILE *file,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 leftSlot,
                                                 TZrUInt32 constantIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_ModSignedConst(state, &frame, %u, %u, %u));\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)constantIndex);
}

void backend_aot_write_c_direct_div(FILE *file,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 leftSlot,
                                    TZrUInt32 rightSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_Div(state, &frame, %u, %u, %u));\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
}

void backend_aot_write_c_direct_div_signed(FILE *file,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 leftSlot,
                                           TZrUInt32 rightSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_left = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_right = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrBool zr_aot_use_runtime_div = ZR_FALSE;\n"
            "        if (ZR_VALUE_IS_TYPE_INT(zr_aot_left->type) && ZR_VALUE_IS_TYPE_INT(zr_aot_right->type)) {\n"
            "            TZrInt64 zr_aot_left_int = ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_left->type)\n"
            "                                              ? zr_aot_left->value.nativeObject.nativeInt64\n"
            "                                              : (TZrInt64)zr_aot_left->value.nativeObject.nativeUInt64;\n"
            "            TZrInt64 zr_aot_right_int = ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_right->type)\n"
            "                                               ? zr_aot_right->value.nativeObject.nativeInt64\n"
            "                                               : (TZrInt64)zr_aot_right->value.nativeObject.nativeUInt64;\n"
            "            if (ZR_UNLIKELY(zr_aot_right_int == 0)) {\n"
            "                ZrCore_Debug_RunError(state, \"divide by zero\");\n"
            "            }\n"
            "            ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                              nativeInt64,\n"
            "                              zr_aot_left_int / zr_aot_right_int,\n"
            "                              ZR_VALUE_TYPE_INT64);\n"
            "        } else {\n"
            "            zr_aot_use_runtime_div = ZR_TRUE;\n"
            "        }\n"
            "        if (zr_aot_use_runtime_div) {\n"
            "            ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_DivSigned(state, &frame, %u, %u, %u));\n"
            "        }\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot,
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
}

void backend_aot_write_c_direct_div_signed_const(FILE *file,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 leftSlot,
                                                 TZrUInt32 constantIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_DivSignedConst(state, &frame, %u, %u, %u));\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)constantIndex);
}

void backend_aot_write_c_direct_neg(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_source = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrBool zr_aot_use_runtime_neg = ZR_FALSE;\n"
            "        if (ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_source->type)) {\n"
            "            ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                              nativeInt64,\n"
            "                              -zr_aot_source->value.nativeObject.nativeInt64,\n"
            "                              zr_aot_source->type);\n"
            "        } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_source->type)) {\n"
            "            ZrCore_Value_InitAsInt(state,\n"
            "                                  zr_aot_destination,\n"
            "                                  -(TZrInt64)zr_aot_source->value.nativeObject.nativeUInt64);\n"
            "        } else if (ZR_VALUE_IS_TYPE_FLOAT(zr_aot_source->type)) {\n"
            "            ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                              nativeDouble,\n"
            "                              -zr_aot_source->value.nativeObject.nativeDouble,\n"
            "                              zr_aot_source->type);\n"
            "        } else {\n"
            "            zr_aot_use_runtime_neg = ZR_TRUE;\n"
            "        }\n"
            "        if (zr_aot_use_runtime_neg) {\n"
            "            ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_Neg(state, &frame, %u, %u));\n"
            "        }\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot,
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
}

void backend_aot_write_c_direct_to_string(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_ToString(state, &frame, %u, %u));\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
}

const SZrTypeValue *backend_aot_c_get_constant_value(const SZrFunction *function, TZrInt32 constantIndex) {
    if (function == ZR_NULL || constantIndex < 0 || (TZrUInt32)constantIndex >= function->constantValueLength ||
        function->constantValueList == ZR_NULL) {
        return ZR_NULL;
    }

    return &function->constantValueList[(TZrUInt32)constantIndex];
}

TZrBool backend_aot_c_constant_requires_materialization(SZrState *state,
                                                        const SZrFunction *function,
                                                        TZrInt32 constantIndex) {
    const SZrTypeValue *constantValue;

    constantValue = backend_aot_c_get_constant_value(function, constantIndex);
    if (state == ZR_NULL || constantValue == ZR_NULL) {
        return ZR_TRUE;
    }

    return ZrCore_Closure_GetMetadataFunctionFromValue(state, constantValue) != ZR_NULL;
}

static const TZrChar *backend_aot_c_value_type_literal(EZrValueType type) {
    switch (type) {
        case ZR_VALUE_TYPE_BOOL:
            return "ZR_VALUE_TYPE_BOOL";
        case ZR_VALUE_TYPE_INT8:
            return "ZR_VALUE_TYPE_INT8";
        case ZR_VALUE_TYPE_INT16:
            return "ZR_VALUE_TYPE_INT16";
        case ZR_VALUE_TYPE_INT32:
            return "ZR_VALUE_TYPE_INT32";
        case ZR_VALUE_TYPE_INT64:
            return "ZR_VALUE_TYPE_INT64";
        case ZR_VALUE_TYPE_UINT8:
            return "ZR_VALUE_TYPE_UINT8";
        case ZR_VALUE_TYPE_UINT16:
            return "ZR_VALUE_TYPE_UINT16";
        case ZR_VALUE_TYPE_UINT32:
            return "ZR_VALUE_TYPE_UINT32";
        case ZR_VALUE_TYPE_UINT64:
            return "ZR_VALUE_TYPE_UINT64";
        case ZR_VALUE_TYPE_FLOAT:
            return "ZR_VALUE_TYPE_FLOAT";
        case ZR_VALUE_TYPE_DOUBLE:
            return "ZR_VALUE_TYPE_DOUBLE";
        default:
            return "ZR_VALUE_TYPE_UNKNOWN";
    }
}

TZrBool backend_aot_c_constant_can_emit_immediate(const SZrFunction *function, TZrInt32 constantIndex) {
    const SZrTypeValue *constantValue = backend_aot_c_get_constant_value(function, constantIndex);

    if (constantValue == ZR_NULL) {
        return ZR_FALSE;
    }

    return ZR_VALUE_IS_TYPE_NULL(constantValue->type) || ZR_VALUE_IS_TYPE_BOOL(constantValue->type) ||
           ZR_VALUE_IS_TYPE_INT(constantValue->type) || ZR_VALUE_IS_TYPE_FLOAT(constantValue->type);
}

void backend_aot_write_c_immediate_constant_copy(FILE *file,
                                                 TZrUInt32 destinationSlot,
                                                 const SZrTypeValue *constantValue) {
    if (file == ZR_NULL || constantValue == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        SZrTypeValue zr_aot_constant;\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n",
            (unsigned)destinationSlot);
    fprintf(file, "        ZrCore_Value_ResetAsNull(&zr_aot_constant);\n");
    fprintf(file,
            "        if (zr_aot_destination == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n");

    if (ZR_VALUE_IS_TYPE_NULL(constantValue->type)) {
        fprintf(file, "        ZrCore_Value_Copy(state, zr_aot_destination, &zr_aot_constant);\n");
    } else if (ZR_VALUE_IS_TYPE_BOOL(constantValue->type)) {
        fprintf(file,
                "        ZR_VALUE_FAST_SET(&zr_aot_constant, nativeBool, %s, ZR_VALUE_TYPE_BOOL);\n"
                "        ZrCore_Value_Copy(state, zr_aot_destination, &zr_aot_constant);\n",
                constantValue->value.nativeObject.nativeBool ? "ZR_TRUE" : "ZR_FALSE");
    } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(constantValue->type)) {
        fprintf(file,
                "        ZR_VALUE_FAST_SET(&zr_aot_constant, nativeInt64, (TZrInt64)%lld, %s);\n"
                "        ZrCore_Value_Copy(state, zr_aot_destination, &zr_aot_constant);\n",
                (long long)constantValue->value.nativeObject.nativeInt64,
                backend_aot_c_value_type_literal(constantValue->type));
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(constantValue->type)) {
        fprintf(file,
                "        ZR_VALUE_FAST_SET(&zr_aot_constant, nativeUInt64, (TZrUInt64)%llu, %s);\n"
                "        ZrCore_Value_Copy(state, zr_aot_destination, &zr_aot_constant);\n",
                (unsigned long long)constantValue->value.nativeObject.nativeUInt64,
                backend_aot_c_value_type_literal(constantValue->type));
    } else if (ZR_VALUE_IS_TYPE_FLOAT(constantValue->type)) {
        fprintf(file,
                "        ZR_VALUE_FAST_SET(&zr_aot_constant, nativeDouble, (TZrFloat64)%.17g, %s);\n"
                "        ZrCore_Value_Copy(state, zr_aot_destination, &zr_aot_constant);\n",
                constantValue->value.nativeObject.nativeDouble,
                backend_aot_c_value_type_literal(constantValue->type));
    }

    fprintf(file, "    }\n");
}

void backend_aot_write_c_direct_constant_copy(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 constantIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    ZrCore_Value_Copy(state,\n"
            "                      ZrCore_Stack_GetValue(frame.slotBase + %u),\n"
            "                      &frame.function->constantValueList[%u]);\n",
            (unsigned)destinationSlot,
            (unsigned)constantIndex);
}

void backend_aot_write_c_direct_callable_constant(FILE *file,
                                                  TZrUInt32 destinationSlot,
                                                  TZrUInt32 constantIndex,
                                                  TZrUInt32 callableFlatIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        const SZrTypeValue *zr_aot_source = &frame.function->constantValueList[%u];\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        SZrFunction *zr_aot_metadata_function = ZrCore_Closure_GetMetadataFunctionFromValue(state, zr_aot_source);\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_metadata_function == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        ZrCore_Ownership_ReleaseValue(state, zr_aot_destination);\n"
            "        SZrClosureNative *zr_aot_closure = ZrCore_ClosureNative_New(state, 0);\n"
            "        if (zr_aot_closure == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_closure->nativeFunction = zr_aot_fn_%u;\n"
            "        zr_aot_closure->aotShimFunction = zr_aot_metadata_function;\n"
            "        ZrCore_Value_InitAsRawObject(state, zr_aot_destination, ZR_CAST_RAW_OBJECT_AS_SUPER(zr_aot_closure));\n"
            "        zr_aot_destination->type = ZR_VALUE_TYPE_CLOSURE;\n"
            "        zr_aot_destination->isGarbageCollectable = ZR_TRUE;\n"
            "        zr_aot_destination->isNative = ZR_TRUE;\n"
            "    }\n",
            (unsigned)constantIndex,
            (unsigned)destinationSlot,
            (unsigned)callableFlatIndex);
}

void backend_aot_write_c_direct_stack_copy(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    ZrCore_Value_Copy(state,\n"
            "                      ZrCore_Stack_GetValue(frame.slotBase + %u),\n"
            "                      ZrCore_Stack_GetValue(frame.slotBase + %u));\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
}

void backend_aot_write_c_get_closure_value(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 closureIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_GetClosureValue(state, &frame, %u, %u));\n",
            (unsigned)destinationSlot,
            (unsigned)closureIndex);
}

void backend_aot_write_c_set_closure_value(FILE *file, TZrUInt32 sourceSlot, TZrUInt32 closureIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SetClosureValue(state, &frame, %u, %u));\n",
            (unsigned)sourceSlot,
            (unsigned)closureIndex);
}

void backend_aot_write_c_direct_add_int(FILE *file,
                                        TZrUInt32 destinationSlot,
                                        TZrUInt32 leftSlot,
                                        TZrUInt32 rightSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_left = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_right = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrInt64 zr_aot_left_int;\n"
            "        TZrInt64 zr_aot_right_int;\n"
            "        if (ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_left->type)) {\n"
            "            zr_aot_left_int = zr_aot_left->value.nativeObject.nativeInt64;\n"
            "        } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_left->type)) {\n"
            "            zr_aot_left_int = (TZrInt64)zr_aot_left->value.nativeObject.nativeUInt64;\n"
            "        } else {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_right->type)) {\n"
            "            zr_aot_right_int = zr_aot_right->value.nativeObject.nativeInt64;\n"
            "        } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_right->type)) {\n"
            "            zr_aot_right_int = (TZrInt64)zr_aot_right->value.nativeObject.nativeUInt64;\n"
            "        } else {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                          nativeInt64,\n"
            "                          zr_aot_left_int + zr_aot_right_int,\n"
            "                          ZR_VALUE_TYPE_INT64);\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
}

void backend_aot_write_c_direct_add_int_const(FILE *file,
                                              TZrUInt32 destinationSlot,
                                              TZrUInt32 leftSlot,
                                              TZrUInt32 constantIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_AddIntConst(state, &frame, %u, %u, %u));\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)constantIndex);
}

void backend_aot_write_c_direct_add(FILE *file,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 leftSlot,
                                    TZrUInt32 rightSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_left = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_right = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrBool zr_aot_use_runtime_add = ZR_FALSE;\n"
            "        if ((ZR_VALUE_IS_TYPE_NUMBER(zr_aot_left->type) || ZR_VALUE_IS_TYPE_BOOL(zr_aot_left->type)) &&\n"
            "            (ZR_VALUE_IS_TYPE_NUMBER(zr_aot_right->type) || ZR_VALUE_IS_TYPE_BOOL(zr_aot_right->type))) {\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
    fprintf(file,
            "            if (ZR_VALUE_IS_TYPE_FLOAT(zr_aot_left->type) || ZR_VALUE_IS_TYPE_FLOAT(zr_aot_right->type)) {\n"
            "                TZrFloat64 zr_aot_left_number = 0.0;\n"
            "                TZrFloat64 zr_aot_right_number = 0.0;\n"
            "                if (ZR_VALUE_IS_TYPE_FLOAT(zr_aot_left->type)) {\n"
            "                    zr_aot_left_number = zr_aot_left->value.nativeObject.nativeDouble;\n"
            "                } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_left->type)) {\n"
            "                    zr_aot_left_number = (TZrFloat64)zr_aot_left->value.nativeObject.nativeInt64;\n"
            "                } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_left->type)) {\n"
            "                    zr_aot_left_number = (TZrFloat64)zr_aot_left->value.nativeObject.nativeUInt64;\n"
            "                } else if (ZR_VALUE_IS_TYPE_BOOL(zr_aot_left->type)) {\n"
            "                    zr_aot_left_number = zr_aot_left->value.nativeObject.nativeBool ? 1.0 : 0.0;\n"
            "                } else {\n"
            "                    zr_aot_use_runtime_add = ZR_TRUE;\n"
            "                }\n"
            "                if (!zr_aot_use_runtime_add) {\n"
            "                    if (ZR_VALUE_IS_TYPE_FLOAT(zr_aot_right->type)) {\n"
            "                        zr_aot_right_number = zr_aot_right->value.nativeObject.nativeDouble;\n"
            "                    } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_right->type)) {\n"
            "                        zr_aot_right_number = (TZrFloat64)zr_aot_right->value.nativeObject.nativeInt64;\n"
            "                    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_right->type)) {\n"
            "                        zr_aot_right_number = (TZrFloat64)zr_aot_right->value.nativeObject.nativeUInt64;\n"
            "                    } else if (ZR_VALUE_IS_TYPE_BOOL(zr_aot_right->type)) {\n"
            "                        zr_aot_right_number = zr_aot_right->value.nativeObject.nativeBool ? 1.0 : 0.0;\n"
            "                    } else {\n"
            "                        zr_aot_use_runtime_add = ZR_TRUE;\n"
            "                    }\n"
            "                }\n"
            "                if (!zr_aot_use_runtime_add) {\n"
            "                    ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                                      nativeDouble,\n"
            "                                      zr_aot_left_number + zr_aot_right_number,\n"
            "                                      ZR_VALUE_TYPE_DOUBLE);\n"
            "                }\n"
            "            } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_left->type) || ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_right->type) ||\n"
            "                       ZR_VALUE_IS_TYPE_BOOL(zr_aot_left->type) || ZR_VALUE_IS_TYPE_BOOL(zr_aot_right->type)) {\n");
    fprintf(file,
            "                TZrInt64 zr_aot_left_number = 0;\n"
            "                TZrInt64 zr_aot_right_number = 0;\n"
            "                if (ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_left->type)) {\n"
            "                    zr_aot_left_number = zr_aot_left->value.nativeObject.nativeInt64;\n"
            "                } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_left->type)) {\n"
            "                    zr_aot_left_number = (TZrInt64)zr_aot_left->value.nativeObject.nativeUInt64;\n"
            "                } else if (ZR_VALUE_IS_TYPE_BOOL(zr_aot_left->type)) {\n"
            "                    zr_aot_left_number = zr_aot_left->value.nativeObject.nativeBool ? 1 : 0;\n"
            "                } else {\n"
            "                    zr_aot_use_runtime_add = ZR_TRUE;\n"
            "                }\n"
            "                if (!zr_aot_use_runtime_add) {\n"
            "                    if (ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_right->type)) {\n"
            "                        zr_aot_right_number = zr_aot_right->value.nativeObject.nativeInt64;\n"
            "                    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_right->type)) {\n"
            "                        zr_aot_right_number = (TZrInt64)zr_aot_right->value.nativeObject.nativeUInt64;\n"
            "                    } else if (ZR_VALUE_IS_TYPE_BOOL(zr_aot_right->type)) {\n"
            "                        zr_aot_right_number = zr_aot_right->value.nativeObject.nativeBool ? 1 : 0;\n"
            "                    } else {\n"
            "                        zr_aot_use_runtime_add = ZR_TRUE;\n"
            "                    }\n"
            "                }\n"
            "                if (!zr_aot_use_runtime_add) {\n"
            "                    ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                                      nativeInt64,\n"
            "                                      zr_aot_left_number + zr_aot_right_number,\n"
            "                                      ZR_VALUE_TYPE_INT64);\n"
            "                }\n"
            "            } else {\n"
            "                ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                                  nativeUInt64,\n"
            "                                  zr_aot_left->value.nativeObject.nativeUInt64 + zr_aot_right->value.nativeObject.nativeUInt64,\n"
            "                                  ZR_VALUE_TYPE_UINT64);\n"
            "            }\n"
            "        } else if (ZR_VALUE_IS_TYPE_STRING(zr_aot_left->type) || ZR_VALUE_IS_TYPE_STRING(zr_aot_right->type)) {\n"
            "            zr_aot_use_runtime_add = ZR_TRUE;\n"
            "        } else {\n"
            "            zr_aot_use_runtime_add = ZR_TRUE;\n"
            "        }\n");
    fprintf(file,
            "        if (zr_aot_use_runtime_add) {\n"
            "            ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_Add(state, &frame, %u, %u, %u));\n"
            "        }\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
}

void backend_aot_write_c_direct_get_global(FILE *file, TZrUInt32 destinationSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_GetGlobal(state, &frame, %u));\n",
            (unsigned)destinationSlot);
}

void backend_aot_write_c_direct_create_object(FILE *file, TZrUInt32 destinationSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        SZrObject *zr_aot_object = ZrCore_Object_New(state, ZR_NULL);\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        if (zr_aot_destination == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        ZrCore_Ownership_ReleaseValue(state, zr_aot_destination);\n"
            "        if (zr_aot_object != ZR_NULL) {\n"
            "            ZrCore_Object_Init(state, zr_aot_object);\n"
            "            ZrCore_Value_InitAsRawObject(state,\n"
            "                                         zr_aot_destination,\n"
            "                                         ZR_CAST_RAW_OBJECT_AS_SUPER(zr_aot_object));\n"
            "        } else {\n"
            "            ZrCore_Value_ResetAsNull(zr_aot_destination);\n"
            "        }\n"
            "    }\n",
            (unsigned)destinationSlot);
}

void backend_aot_write_c_direct_create_array(FILE *file, TZrUInt32 destinationSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        SZrObject *zr_aot_array = ZrCore_Object_NewCustomized(state,\n"
            "                                                             sizeof(SZrObject),\n"
            "                                                             ZR_OBJECT_INTERNAL_TYPE_ARRAY);\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        if (zr_aot_destination == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        ZrCore_Ownership_ReleaseValue(state, zr_aot_destination);\n"
            "        if (zr_aot_array != ZR_NULL) {\n"
            "            ZrCore_Object_Init(state, zr_aot_array);\n"
            "            ZrCore_Value_InitAsRawObject(state,\n"
            "                                         zr_aot_destination,\n"
            "                                         ZR_CAST_RAW_OBJECT_AS_SUPER(zr_aot_array));\n"
            "            zr_aot_destination->type = ZR_VALUE_TYPE_ARRAY;\n"
            "        } else {\n"
            "            ZrCore_Value_ResetAsNull(zr_aot_destination);\n"
            "        }\n"
            "    }\n",
            (unsigned)destinationSlot);
}

void backend_aot_write_c_direct_typeof(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_TypeOf(state, &frame, %u, %u));\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
}

void backend_aot_write_c_direct_to_object(FILE *file,
                                          TZrUInt32 destinationSlot,
                                          TZrUInt32 sourceSlot,
                                          TZrUInt32 typeNameConstantIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_ToObject(state, &frame, %u, %u, %u));\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot,
            (unsigned)typeNameConstantIndex);
}

void backend_aot_write_c_direct_to_struct(FILE *file,
                                          TZrUInt32 destinationSlot,
                                          TZrUInt32 sourceSlot,
                                          TZrUInt32 typeNameConstantIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_ToStruct(state, &frame, %u, %u, %u));\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot,
            (unsigned)typeNameConstantIndex);
}

void backend_aot_write_c_direct_to_int(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_source = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        if (ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_source->type)) {\n"
            "            ZrCore_Value_Copy(state, zr_aot_destination, zr_aot_source);\n"
            "        } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_source->type)) {\n"
            "            ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                              nativeInt64,\n"
            "                              (TZrInt64)zr_aot_source->value.nativeObject.nativeUInt64,\n"
            "                              ZR_VALUE_TYPE_INT64);\n"
            "        } else if (ZR_VALUE_IS_TYPE_FLOAT(zr_aot_source->type)) {\n"
            "            ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                              nativeInt64,\n"
            "                              (TZrInt64)zr_aot_source->value.nativeObject.nativeDouble,\n"
            "                              ZR_VALUE_TYPE_INT64);\n"
            "        } else if (ZR_VALUE_IS_TYPE_BOOL(zr_aot_source->type)) {\n"
            "            ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                              nativeInt64,\n"
            "                              zr_aot_source->value.nativeObject.nativeBool ? 1 : 0,\n"
            "                              ZR_VALUE_TYPE_INT64);\n"
            "        } else {\n"
            "            ZR_VALUE_FAST_SET(zr_aot_destination, nativeInt64, 0, ZR_VALUE_TYPE_INT64);\n"
            "        }\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
}
