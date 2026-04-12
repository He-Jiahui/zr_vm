#include "backend_aot_c_emitter.h"
#include "backend_aot_internal.h"

void backend_aot_write_c_direct_meta_call(FILE *file,
                                          TZrUInt32 destinationSlot,
                                          TZrUInt32 receiverSlot,
                                          TZrUInt32 argumentCount) {
    TZrUInt32 callableArgumentCount = argumentCount + 1;

    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        ZrAotGeneratedDirectCall zr_aot_direct_call;\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_PrepareMetaCall(state,\n"
            "                                                          &frame,\n"
            "                                                          %u,\n"
            "                                                          %u,\n"
            "                                                          %u,\n"
            "                                                          &zr_aot_direct_call));\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_CallPreparedOrGeneric(state,\n"
            "                                                               &frame,\n"
            "                                                               &zr_aot_direct_call,\n"
            "                                                               %u,\n"
            "                                                               %u,\n"
            "                                                               %u,\n"
            "                                                               1));\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)receiverSlot,
            (unsigned)argumentCount,
            (unsigned)destinationSlot,
            (unsigned)receiverSlot,
            (unsigned)callableArgumentCount);
}

void backend_aot_write_c_static_direct_function_call(FILE *file,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 functionSlot,
                                                     TZrUInt32 argumentCount,
                                                     TZrUInt32 calleeFlatIndex) {
    if (file == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        ZrAotGeneratedDirectCall zr_aot_direct_call;\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_PrepareStaticDirectCall(state,\n"
            "                                                                  &frame,\n"
            "                                                                  %u,\n"
            "                                                                  %u,\n"
            "                                                                  %u,\n"
            "                                                                  %u,\n"
            "                                                                  &zr_aot_direct_call));\n"
            "        ZR_AOT_C_GUARD(zr_aot_fn_%u(state));\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_FinishDirectCall(state, &frame, &zr_aot_direct_call, 1));\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)functionSlot,
            (unsigned)argumentCount,
            (unsigned)calleeFlatIndex,
            (unsigned)calleeFlatIndex);
}

void backend_aot_write_c_direct_function_call(FILE *file,
                                              TZrUInt32 destinationSlot,
                                              TZrUInt32 functionSlot,
                                              TZrUInt32 argumentCount) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        ZrAotGeneratedDirectCall zr_aot_direct_call;\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_PrepareDirectCall(state,\n"
            "                                                            &frame,\n"
            "                                                            %u,\n"
            "                                                            %u,\n"
            "                                                            %u,\n"
            "                                                            &zr_aot_direct_call));\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_CallPreparedOrGeneric(state,\n"
            "                                                               &frame,\n"
            "                                                               &zr_aot_direct_call,\n"
            "                                                               %u,\n"
            "                                                               %u,\n"
            "                                                               %u,\n"
            "                                                               1));\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)functionSlot,
            (unsigned)argumentCount,
            (unsigned)destinationSlot,
            (unsigned)functionSlot,
            (unsigned)argumentCount);
}

void backend_aot_write_c_dynamic_function_call(FILE *file,
                                               TZrUInt32 destinationSlot,
                                               TZrUInt32 functionSlot,
                                               TZrUInt32 argumentCount) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_Call(state, &frame, %u, %u, %u));\n",
            (unsigned)destinationSlot,
            (unsigned)functionSlot,
            (unsigned)argumentCount);
}
