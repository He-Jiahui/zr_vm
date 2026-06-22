#include "backend_aot_c_emitter.h"

void backend_aot_write_c_direct_mark_to_be_closed(FILE *file, TZrUInt32 slotIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_scope_mark_to_be_closed */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_MarkToBeClosed(state, &frame, %u));\n"
            "    }\n",
            (unsigned)slotIndex);
}

void backend_aot_write_c_direct_close_scope(FILE *file, TZrUInt32 cleanupCount) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_scope_close_scope */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_CloseScope(state, &frame, %u));\n"
            "    }\n",
            (unsigned)cleanupCount);
}
