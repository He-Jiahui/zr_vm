#include "backend_aot_c_emitter.h"

static void backend_aot_write_c_dynamic_value_access_deopt_bridge(FILE *file,
                                                                  TZrUInt32 deoptId,
                                                                  const char *errorLabel) {
    const char *safeErrorLabel;

    if (file == ZR_NULL) {
        return;
    }

    safeErrorLabel = errorLabel != ZR_NULL ? errorLabel : "dynamic value access";
    fprintf(file,
            "        /* zr_aot_value_dynamic_deopt_bridge deopt=%u */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_ValidateDynamicDeoptBridge(state, &frame, %u, \"%s\"));\n",
            (unsigned)deoptId,
            (unsigned)deoptId,
            safeErrorLabel);
}

void backend_aot_write_c_unsupported_meta_value_access(FILE *file,
                                                       const char *opcodeName,
                                                       TZrUInt32 primarySlot,
                                                       TZrUInt32 secondarySlot,
                                                       TZrUInt32 memberOrCacheIndex) {
    const char *safeOpcodeName;

    if (file == ZR_NULL) {
        return;
    }

    safeOpcodeName = opcodeName != ZR_NULL ? opcodeName : "META_VALUE_ACCESS";
    fprintf(file,
            "    {\n"
            "        /* zr_aot_value_unsupported_meta_value_access */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_UnsupportedMetaValueAccess(state,\n"
            "                                                                           &frame,\n"
            "                                                                           %u,\n"
            "                                                                           %u,\n"
            "                                                                           %u,\n"
            "                                                                           \"%s\"));\n"
            "    }\n",
            (unsigned)primarySlot,
            (unsigned)secondarySlot,
            (unsigned)memberOrCacheIndex,
            safeOpcodeName);
}

void backend_aot_write_c_unsupported_dynamic_value_access(FILE *file,
                                                          const char *opcodeName,
                                                          TZrUInt32 primarySlot,
                                                          TZrUInt32 secondarySlot,
                                                          TZrUInt32 operandIndex) {
    const char *safeOpcodeName;

    if (file == ZR_NULL) {
        return;
    }

    safeOpcodeName = opcodeName != ZR_NULL ? opcodeName : "DYNAMIC_VALUE_ACCESS";
    fprintf(file,
            "    {\n"
            "        /* zr_aot_value_unsupported_dynamic_value_access */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_UnsupportedDynamicValueAccess(state,\n"
            "                                                                              &frame,\n"
            "                                                                              %u,\n"
            "                                                                              %u,\n"
            "                                                                              %u,\n"
            "                                                                              \"%s\"));\n"
            "    }\n",
            (unsigned)primarySlot,
            (unsigned)secondarySlot,
            (unsigned)operandIndex,
            safeOpcodeName);
}

void backend_aot_write_c_direct_get_member(FILE *file,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 receiverSlot,
                                           TZrUInt32 memberId,
                                           TZrUInt32 deoptId) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_dynamic_get_member_boundary */\n");
    backend_aot_write_c_dynamic_value_access_deopt_bridge(file, deoptId, "dynamic get member");
    fprintf(file,
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_GetMember(state, &frame, %u, %u, %u));\n"
            "    } while (0);\n",
            (unsigned)destinationSlot,
            (unsigned)receiverSlot,
            (unsigned)memberId);
}

void backend_aot_write_c_direct_set_member(FILE *file,
                                           TZrUInt32 sourceSlot,
                                           TZrUInt32 receiverSlot,
                                           TZrUInt32 memberId,
                                           TZrUInt32 deoptId) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_dynamic_set_member_boundary */\n");
    backend_aot_write_c_dynamic_value_access_deopt_bridge(file, deoptId, "dynamic set member");
    fprintf(file,
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SetMember(state, &frame, %u, %u, %u));\n"
            "    } while (0);\n",
            (unsigned)sourceSlot,
            (unsigned)receiverSlot,
            (unsigned)memberId);
}

void backend_aot_write_c_direct_set_member_new_owner_no_write_barrier(FILE *file,
                                                                      TZrUInt32 sourceSlot,
                                                                      TZrUInt32 receiverSlot,
                                                                      TZrUInt32 memberId,
                                                                      TZrUInt32 deoptId) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_dynamic_set_member_new_owner_no_write_barrier */\n");
    backend_aot_write_c_dynamic_value_access_deopt_bridge(file, deoptId, "dynamic set member");
    fprintf(file,
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SetMemberNewOwnerNoWriteBarrier(state, &frame, %u, %u, %u));\n"
            "    } while (0);\n",
            (unsigned)sourceSlot,
            (unsigned)receiverSlot,
            (unsigned)memberId);
}

void backend_aot_write_c_direct_get_member_slot(FILE *file,
                                                TZrUInt32 destinationSlot,
                                                TZrUInt32 receiverSlot,
                                                TZrUInt32 cacheIndex,
                                                TZrUInt32 deoptId) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_dynamic_get_member_slot_boundary */\n");
    backend_aot_write_c_dynamic_value_access_deopt_bridge(file, deoptId, "dynamic get member slot");
    fprintf(file,
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_GetMemberSlot(state, &frame, %u, %u, %u));\n"
            "    } while (0);\n",
            (unsigned)destinationSlot,
            (unsigned)receiverSlot,
            (unsigned)cacheIndex);
}

void backend_aot_write_c_direct_set_member_slot(FILE *file,
                                                TZrUInt32 sourceSlot,
                                                TZrUInt32 receiverSlot,
                                                TZrUInt32 cacheIndex,
                                                TZrUInt32 deoptId) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_dynamic_set_member_slot_boundary */\n");
    backend_aot_write_c_dynamic_value_access_deopt_bridge(file, deoptId, "dynamic set member slot");
    fprintf(file,
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SetMemberSlot(state, &frame, %u, %u, %u));\n"
            "    } while (0);\n",
            (unsigned)sourceSlot,
            (unsigned)receiverSlot,
            (unsigned)cacheIndex);
}

void backend_aot_write_c_direct_set_member_slot_new_owner_no_write_barrier(FILE *file,
                                                                           TZrUInt32 sourceSlot,
                                                                           TZrUInt32 receiverSlot,
                                                                           TZrUInt32 cacheIndex,
                                                                           TZrUInt32 deoptId) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_dynamic_set_member_slot_new_owner_no_write_barrier */\n");
    backend_aot_write_c_dynamic_value_access_deopt_bridge(file, deoptId, "dynamic set member slot");
    fprintf(file,
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SetMemberSlotNewOwnerNoWriteBarrier(state, &frame, %u, %u, %u));\n"
            "    } while (0);\n",
            (unsigned)sourceSlot,
            (unsigned)receiverSlot,
            (unsigned)cacheIndex);
}

void backend_aot_write_c_direct_get_by_index(FILE *file,
                                             TZrUInt32 destinationSlot,
                                             TZrUInt32 receiverSlot,
                                             TZrUInt32 keySlot,
                                             TZrUInt32 deoptId) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_dynamic_get_by_index_boundary */\n");
    backend_aot_write_c_dynamic_value_access_deopt_bridge(file, deoptId, "dynamic get by index");
    fprintf(file,
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_GetByIndex(state, &frame, %u, %u, %u));\n"
            "    } while (0);\n",
            (unsigned)destinationSlot,
            (unsigned)receiverSlot,
            (unsigned)keySlot);
}

void backend_aot_write_c_direct_set_by_index_new_owner_no_write_barrier(FILE *file,
                                                                        TZrUInt32 sourceSlot,
                                                                        TZrUInt32 receiverSlot,
                                                                        TZrUInt32 keySlot,
                                                                        TZrUInt32 deoptId) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_dynamic_set_by_index_new_owner_no_write_barrier */\n");
    backend_aot_write_c_dynamic_value_access_deopt_bridge(file, deoptId, "dynamic set by index");
    fprintf(file,
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SetByIndexNewOwnerNoWriteBarrier(state, &frame, %u, %u, %u));\n"
            "    } while (0);\n",
            (unsigned)sourceSlot,
            (unsigned)receiverSlot,
            (unsigned)keySlot);
}

void backend_aot_write_c_direct_set_by_index(FILE *file,
                                             TZrUInt32 sourceSlot,
                                             TZrUInt32 receiverSlot,
                                             TZrUInt32 keySlot,
                                             TZrUInt32 deoptId) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_dynamic_set_by_index_boundary */\n");
    backend_aot_write_c_dynamic_value_access_deopt_bridge(file, deoptId, "dynamic set by index");
    fprintf(file,
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SetByIndex(state, &frame, %u, %u, %u));\n"
            "    } while (0);\n",
            (unsigned)sourceSlot,
            (unsigned)receiverSlot,
            (unsigned)keySlot);
}
