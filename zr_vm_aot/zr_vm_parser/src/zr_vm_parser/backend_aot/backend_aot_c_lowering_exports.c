#include "backend_aot_c_emitter.h"

void backend_aot_write_c_publish_exports(FILE *file) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_publish_exports_boundary */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_PublishModuleExports(state, &frame));\n"
            "    }\n");
}
