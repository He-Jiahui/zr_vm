//
// zr.system.env descriptor registry.
//

#include "zr_vm_lib_system/env_registry.h"

#ifndef ZR_ARRAY_COUNT
/* 仅对本文件中的静态数组求元素数；传入指针不会得到数组长度。 */
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

/* module.c 在注册阶段借用该静态描述符；嵌套表和 callback 指针依赖 provider image 持续驻留，浅拷贝 descriptor 不延长其寿命。 */
const ZrLibModuleDescriptor *ZrSystem_EnvRegistry_GetModule(void) {
    /* BUG: 缺失变量会由 callback 正常返回 null，且提示签名声明 string?；描述符的非空 string 却被 runtime metadata、parser 与 LSP 当作返回契约发布。 */
    static const ZrLibFunctionDescriptor kFunctions[] = {
            {"getVariable", 1, 1, ZrSystem_Env_GetVariable, "string", "Read an environment variable by name.", ZR_NULL, 0},
    };
    /* 面向工具的签名保留可空返回值；它与函数描述符独立发布，不能覆盖 runtime metadata 所读的 returnTypeName。 */
    static const ZrLibTypeHintDescriptor kHints[] = {
            {"getVariable", "function", "getVariable(name: string): string?", "Read an environment variable by name."},
    };
    /* 该协议标识随 module metadata 单独导出，不是函数签名表的替代物。 */
    static const TZrChar kHintsJson[] =
            "{\n"
            "  \"schema\": \"zr.native.hints/v1\",\n"
            "  \"module\": \"zr.system.env\"\n"
            "}\n";
    /* 聚合模块身份、ABI 版本、导出函数和工具提示；子表指针均借用上述静态对象。 */
    static const ZrLibModuleDescriptor kModule = {
            ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
            "zr.system.env",
            ZR_NULL,
            0,
            kFunctions,
            ZR_ARRAY_COUNT(kFunctions),
            ZR_NULL,
            0,
            kHints,
            ZR_ARRAY_COUNT(kHints),
            kHintsJson,
            "Environment helpers.",
            ZR_NULL,
            0,
            "1.0.0",
            ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
            0,
    };

    return &kModule;
}
