#include "backend_aot_c_emitter.h"

/* 模块入口与尾返回共用此边界：返回给调用方前，把当前模块帧中的导出发布到项目记录。
 * BUG: 递归 import 可扩容 runtime 的 records；PublishModuleExports 在回查前解引用
 * activeRecord，记录指针失效时此处生成的调用会触发悬空指针访问。
 */
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
