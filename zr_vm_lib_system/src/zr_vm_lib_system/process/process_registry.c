/**
 * @file process_registry.c
 * @brief 组装 `zr.system.process` 的静态模块描述符与回调表。
 * @details GetModule 返回的对象与其引用表均具有静态存储期，供系统聚合注册器和原生注册表借用；
 *          运行期函数回调由 process.c 实现，`arguments` VM 数组由模块物化器按常量种类创建。
 * @see zr_vm_lib_system/include/zr_vm_lib_system/process_registry.h; zr_vm_lib_system/src/zr_vm_lib_system/module.c
 */

#include "zr_vm_lib_system/process_registry.h"

#ifndef ZR_ARRAY_COUNT
/**
 * @def ZR_ARRAY_COUNT(value)
 *
 * ## Summary
 * 为未通过共享头文件取得 `ZR_ARRAY_COUNT` 定义的编译入口提供固定 C 数组计数回退。
 *
 * ## Parameters
 * @param value 必须是数组表达式；数组退化为指针后此宏不再表示元素数。
 *
 * ## Returns
 * 展开为 `sizeof(value) / sizeof((value)[0])`，即编译期数组元素数；不产生运行时操作。
 *
 * ## Constraints and lifecycle
 * `value` 必须仍保有数组类型，不能传入退化后的指针。正常 include 链从
 * `zr_vm_common/zr_array_conf.h` 提供同名宏，此处仅保留防御性回退。
 *
 * ## References
 * 本翻译单元在 `kModule` 中对常量、函数和类型提示数组分别使用该宏。
 */
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

/**
 * ## Summary
 * 实现公开 getter，组装并返回 `zr.system.process` 的静态原生模块描述符，供系统聚合注册器传给原生注册表。
 *
 * ## Returns
 * @return 返回 kModule 地址；此实现不分配且不失败，指针与其中所有表项/字符串均为静态借用数据。
 *
 * ## Constraints and lifecycle
 * 本函数只取静态地址，没有运行时失败分支；描述符声明固定 ABI/version 且无额外 capability，数组计数
 * 对应真实静态数组。注册器保存 descriptor 指针，因此对象不可改为临时存储或释放；注册兼容性失败
 * 由上游处理。函数内常量表和字符串在提供者模块映像存续期内有效，注册方须在借用期间保持映像驻留。
 *
 * ## References
 * `ZrVmLibSystem_Register` 调用此 getter；原生注册表在 `native_binding_support.c` 保存 descriptor 借用指针。
 */
const ZrLibModuleDescriptor *ZrSystem_ProcessRegistry_GetModule(void) {
    /* arguments 是 VM 数组常量的静态声明；CLI 在 project/inline/REPL 入口执行前写入入口标识及用户参数。 */
    static const ZrLibConstantDescriptor kConstants[] = {
            {"arguments", ZR_LIB_CONSTANT_KIND_ARRAY, 0, 0.0, ZR_NULL, ZR_FALSE, "Process argument list.", "string[]"},
    };
    /* 两个回调各固定一个参数；parameters=NULL 表示此表只给出元数，不提供参数描述符。
     * 类型提示另存于 kHints；未显式初始化的尾部字段为零，dispatchFlags=0 采用分派器 GC-aware 默认模式。 */
    static const ZrLibFunctionDescriptor kFunctions[] = {
            {"sleepMilliseconds", 1, 1, ZrSystem_Process_SleepMilliseconds, "null", "Sleep for the requested number of milliseconds.", ZR_NULL, 0},
            {"exit", 1, 1, ZrSystem_Process_Exit, "null", "Terminate the process with the given exit code.", ZR_NULL, 0},
    };
    /* 可读签名作为独立 hint 元数据导出；它们不替代回调描述符的元数检查/ReadInt 转换契约。
     * TODO: parser 当前只将 symbolKind=property 的 hint 加入导入类型成员；确认这些 function 签名
     * 是否应约束编译期参数推断，特别是 descriptor.parameters=NULL 时。见 type_inference_native.c。 */
    static const ZrLibTypeHintDescriptor kHints[] = {
            {"arguments", "constant", "arguments: string[]", "Process argument list."},
            {"sleepMilliseconds", "function", "sleepMilliseconds(milliseconds: int): null", "Sleep for the requested number of milliseconds."},
            {"exit", "function", "exit(code: int): null", "Terminate the process with the given exit code."},
    };
    /* 导出 schema/module 标识的静态 JSON 字符串；C 相邻字符串字面量在编译期拼接。 */
    static const TZrChar kHintsJson[] =
            "{\n"
            "  \"schema\": \"zr.native.hints/v1\",\n"
            "  \"module\": \"zr.system.process\"\n"
            "}\n";
    /* 聚合描述符只借用本函数内所有 static const 表；其余可选尾字段按 C 聚合初始化规则清零。 */
    static const ZrLibModuleDescriptor kModule = {
            ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
            "zr.system.process",
            kConstants,
            ZR_ARRAY_COUNT(kConstants),
            kFunctions,
            ZR_ARRAY_COUNT(kFunctions),
            ZR_NULL,
            0,
            kHints,
            ZR_ARRAY_COUNT(kHints),
            kHintsJson,
            "Process helpers.",
            ZR_NULL,
            0,
            "1.0.0",
            ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
            0,
    };

    return &kModule;
}
