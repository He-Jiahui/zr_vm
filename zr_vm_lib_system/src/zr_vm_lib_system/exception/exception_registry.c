//
// zr.system.exception descriptor registry.
//

#include "zr_vm_lib_system/exception_registry.h"

/* 公共 conf.h 头链已导入 zr_vm_common/zr_array_conf.h；此处仅保留脱离该头链时的条件备用定义。 */
#ifndef ZR_ARRAY_COUNT
/**
 * ## Summary
 * 在公共数组计数宏不可见时，以编译期数组表达式计算元素个数，供本文件静态描述符的 count 字段使用。
 *
 * ## Parameters
 * @param[in] value 数组表达式；不得传入指针，因为宏用 `sizeof` 推导完整数组长度。
 *
 * ## Returns
 * @return 返回数组元素个数的编译期整数表达式，不读取或遍历数组内容。
 *
 * ## Constraints and lifecycle
 * 本地定义只在 `ZR_ARRAY_COUNT` 尚未定义时启用；当前产品 include 链通过 `zr_vm_common.h` 先提供共享定义，因此本 fallback 不参与该构建。
 *
 * ## References
 * `zr_vm_common/include/zr_vm_common/zr_array_conf.h:13` 是当前共享定义来源；本地展开点位于本文件静态 descriptor 表。
 */
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

/**
 * ## Summary
 * 提供 `zr.system.exception` 叶模块的静态描述符，供 System 注册流程登记异常类型、构造器和 handler 注册函数。
 *
 * ## Returns
 * @return 返回进程映像内静态只读的 `kModule` 地址；调用者借用该指针，不得释放或修改其表格。
 *
 * ## Constraints and lifecycle
 * 描述符及表格具有静态存储期，但 provider image 必须在 registry 使用其中的 callback、字符串及子表指针期间保持驻留。
 * 本函数不创建 VM 对象；System 注册流程登记叶描述符后再导入模块以物化 Error 原型。
 *
 * ## References
 * `zr_vm_lib_system/src/zr_vm_lib_system/module.c:88` 登记返回值，`:108` 导入异常模块；
 * `ZrSystem_Exception_Constructor` 是构造器描述符引用的 callback。
 */
const ZrLibModuleDescriptor *ZrSystem_ExceptionRegistry_GetModule(void) {
    /**
     * ## Summary
     * 声明异常实例共享的动态字段布局；类型描述符引用这些名称和值类别，构造器/异常规范化负责实际写值。
     *
     * ## Constraints and lifecycle
     * 这是模块静态描述元数据，不是对象实例，也不在注册时为字段分配存储。`message` 保存诊断文本，`stacks` 保存
     * 结构化 StackFrame 序列，`exception` 保存关联的异常对象或原始抛出值。TODO: `message` 的 `string` 元数据与零参构造/Core 默认 null 不符；沿 constructor、Core exception、native_binding_metadata 和 LSP metadata projection 核实应否标为 nullable，或改变缺省消息策略。
     *
     * ## References
     * `kTypes` 将此字段表用于 Error 及派生类型；
     * `zr_vm_lib_system/src/zr_vm_lib_system/exception/exception.c:ZrSystem_Exception_Constructor` 初始化构造阶段字段。
     */
    static const ZrLibFieldDescriptor kErrorFields[] = {
            ZR_LIB_FIELD_DESCRIPTOR_INIT("message", "string", "Human-readable exception message."),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("stacks", "StackFrame[]", "Structured stack frames, throw-site first."),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("exception", "value", "Original thrown payload or exception object."),
    };
    /**
     * ## Summary
     * 描述异常 stack frame 的公开字段名称和值类别，供脚本反射和类型提示读取。
     *
     * ## Constraints and lifecycle
     * `sourceLine` 按 1 起算；此表只提供描述符，不负责创建或更新运行期 frame 对象。
     *
     * ## References
     * `kTypes` 将该表绑定到 `StackFrame` struct 类型；实际栈收集由 Core 抛出/规范化路径负责。
     */
    static const ZrLibFieldDescriptor kStackFrameFields[] = {
            ZR_LIB_FIELD_DESCRIPTOR_INIT("functionName", "string", "Function name for this frame."),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("sourceFile", "string", "Source file for this frame."),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("sourceLine", "int", "1-based source line for this frame."),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("instructionOffset", "int", "Instruction offset inside the function."),
    };
    /**
     * ## Summary
     * 把统一异常构造 callback 注册为 Error 及派生异常类的构造元方法。
     *
     * ## Constraints and lifecycle
     * 元数范围为 0..1，callback 返回 null 作为构造约定；反射调用者保留并交付 receiver。此表被 7 个异常类描述符复用，
     * 不应用到 `StackFrame`。
     *
     * ## References
     * `zr_vm_lib_system/include/zr_vm_lib_system/exception.h:ZrSystem_Exception_Constructor` 定义 callback 契约；
     * `kTypes` 中 Error 与 6 个派生类引用本表。
     */
    static const ZrLibMetaMethodDescriptor kExceptionConstructors[] = {
            {ZR_META_CONSTRUCTOR, 0, 1, ZrSystem_Exception_Constructor, "null", "Initialize an exception instance.", ZR_NULL, 0},
    };
    /**
     * ## Summary
     * 发布 Error/StackFrame 类型及异常分类层级；派生类型复用统一字段与构造器描述符。
     *
     * ## Constraints and lifecycle
     * Error 是基类；RuntimeError 继承 Error，NullReferenceError 与 IOException 继承 RuntimeError；TypeError、MemoryError、
     * ExceptionError 继承 Error。`StackFrame` 是独立 struct，无异常构造 callback。描述符文字和 hint 不会代替运行期值校验。
     * System 注册器随后导入叶模块；描述符声明本身不表示原型已经物化。
     *
     * ## References
     * `kErrorFields`、`kStackFrameFields` 与 `kExceptionConstructors` 提供本表引用的静态数据；
     * `zr_vm_lib_system/src/zr_vm_lib_system/module.c:108` 执行后续导入。
     */
    static const ZrLibTypeDescriptor kTypes[] = {
            ZR_LIB_TYPE_DESCRIPTOR_INIT("Error", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, kErrorFields,
                                        ZR_ARRAY_COUNT(kErrorFields), ZR_NULL, 0, kExceptionConstructors,
                                        ZR_ARRAY_COUNT(kExceptionConstructors), "Base VM exception type.", ZR_NULL,
                                        ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE, "Error(message?: any)",
                                        ZR_NULL, 0),
            ZR_LIB_TYPE_DESCRIPTOR_INIT("StackFrame", ZR_OBJECT_PROTOTYPE_TYPE_STRUCT, kStackFrameFields,
                                        ZR_ARRAY_COUNT(kStackFrameFields), ZR_NULL, 0, ZR_NULL, 0,
                                        "Structured VM stack frame.", ZR_NULL, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL,
                                        ZR_TRUE, ZR_TRUE, "StackFrame()", ZR_NULL, 0),
            ZR_LIB_TYPE_DESCRIPTOR_INIT("RuntimeError", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, kErrorFields,
                                        ZR_ARRAY_COUNT(kErrorFields), ZR_NULL, 0, kExceptionConstructors,
                                        ZR_ARRAY_COUNT(kExceptionConstructors), "Runtime execution error.", "Error",
                                        ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE,
                                        "RuntimeError(message?: any)", ZR_NULL, 0),
            ZR_LIB_TYPE_DESCRIPTOR_INIT("NullReferenceError", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, kErrorFields,
                                        ZR_ARRAY_COUNT(kErrorFields), ZR_NULL, 0, kExceptionConstructors,
                                        ZR_ARRAY_COUNT(kExceptionConstructors), "Null receiver access error.",
                                        "RuntimeError", ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE,
                                        "NullReferenceError(message?: any)", ZR_NULL, 0),
            ZR_LIB_TYPE_DESCRIPTOR_INIT("IOException", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, kErrorFields,
                                        ZR_ARRAY_COUNT(kErrorFields), ZR_NULL, 0, kExceptionConstructors,
                                        ZR_ARRAY_COUNT(kExceptionConstructors), "Filesystem and stream I/O error.",
                                        "RuntimeError", ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE,
                                        "IOException(message?: any)", ZR_NULL, 0),
            ZR_LIB_TYPE_DESCRIPTOR_INIT("TypeError", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, kErrorFields,
                                        ZR_ARRAY_COUNT(kErrorFields), ZR_NULL, 0, kExceptionConstructors,
                                        ZR_ARRAY_COUNT(kExceptionConstructors), "Type mismatch error.", "Error",
                                        ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE,
                                        "TypeError(message?: any)", ZR_NULL, 0),
            ZR_LIB_TYPE_DESCRIPTOR_INIT("MemoryError", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, kErrorFields,
                                        ZR_ARRAY_COUNT(kErrorFields), ZR_NULL, 0, kExceptionConstructors,
                                        ZR_ARRAY_COUNT(kExceptionConstructors), "Allocation or memory pressure error.",
                                        "Error", ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE,
                                        "MemoryError(message?: any)", ZR_NULL, 0),
            ZR_LIB_TYPE_DESCRIPTOR_INIT("ExceptionError", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, kErrorFields,
                                        ZR_ARRAY_COUNT(kErrorFields), ZR_NULL, 0, kExceptionConstructors,
                                        ZR_ARRAY_COUNT(kExceptionConstructors), "Generic VM exception state error.",
                                        "Error", ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE,
                                        "ExceptionError(message?: any)", ZR_NULL, 0),
    };
    /**
     * ## Summary
     * 暴露唯一脚本入口 `registerUnhandledException`，把 ReadFunction 接受的可调用值交给全局 handler 注册 callback。
     *
     * ## Constraints and lifecycle
     * 绑定描述符要求 1..1 个参数；callback 存储 VM 函数、闭包或原生指针，不校验 hint 签名。描述符不会自动派发 handler。
     *
     * ## References
     * `zr_vm_lib_system/src/zr_vm_lib_system/exception/exception.c:ZrSystem_Exception_RegisterUnhandledException`
     * 实现存储；`kHints` 提供工具侧签名文本。
     */
    static const ZrLibFunctionDescriptor kFunctions[] = {
            {"registerUnhandledException",
             1,
             1,
             ZrSystem_Exception_RegisterUnhandledException,
             "null",
             "Register a single global unhandled exception observer.",
             ZR_NULL,
             0},
    };
    /**
     * ## Summary
     * 为异常类型与注册函数提供静态工具提示；该表供查询/编辑器读取，不负责运行期类型校验或回调派发。
     *
     * ## Constraints and lifecycle
     * hint 名称、继承关系和签名文本应与 `kTypes`、`kFunctions` 保持一致；实际元数与 callable 检查由 descriptor/dispatch 路径执行。
     *
     * ## References
     * `kModule` 将此表作为 type hints 发布；对应运行期 descriptor 位于 `kTypes` 与 `kFunctions`。
     */
    static const ZrLibTypeHintDescriptor kHints[] = {
            {"registerUnhandledException",
             "function",
             "registerUnhandledException(handler: (error: Error) -> bool): null",
             "Register a single global unhandled exception observer."},
            {"Error", "type", "class Error { message, stacks, exception }", "Base VM exception type."},
            {"StackFrame",
             "type",
             "struct StackFrame { functionName, sourceFile, sourceLine, instructionOffset }",
             "Structured VM stack frame."},
            {"RuntimeError", "type", "class RuntimeError extends Error", "Runtime execution error."},
            {"NullReferenceError", "type", "class NullReferenceError extends RuntimeError",
             "Null receiver access error."},
            {"IOException", "type", "class IOException extends RuntimeError", "Filesystem and stream I/O error."},
            {"TypeError", "type", "class TypeError extends Error", "Type mismatch error."},
            {"MemoryError", "type", "class MemoryError extends Error", "Allocation or memory pressure error."},
            {"ExceptionError", "type", "class ExceptionError extends Error", "Generic VM exception state error."},
    };
    /* JSON hint payload 只携带 schema 与模块身份；类型/函数细节由上面的原生描述符表提供。 */
    static const TZrChar kHintsJson[] =
            "{\n"
            "  \"schema\": \"zr.native.hints/v1\",\n"
            "  \"module\": \"zr.system.exception\"\n"
            "}\n";
    /**
     * ## Summary
     * 聚合本叶模块的 ABI 版本、名称、函数表、类型表和工具提示，供 native registry 读取并登记。
     *
     * ## Constraints and lifecycle
     * 此对象及其引用的表均具有静态存储期；provider image 必须在 registry 使用这些 callback、字符串和表指针期间保持驻留。
     * descriptor 是元数据，不会自行触发模块导入或执行 callback。
     *
     * ## References
     * `ZrSystem_ExceptionRegistry_GetModule` 返回该对象；
     * `zr_vm_lib_system/src/zr_vm_lib_system/module.c:88` 将其登记到全局 registry。
     */
    static const ZrLibModuleDescriptor kModule = {
            ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
            "zr.system.exception",
            ZR_NULL,
            0,
            kFunctions,
            ZR_ARRAY_COUNT(kFunctions),
            kTypes,
            ZR_ARRAY_COUNT(kTypes),
            kHints,
            ZR_ARRAY_COUNT(kHints),
            kHintsJson,
            "Exception hierarchy and unhandled exception hooks.",
            ZR_NULL,
            0,
            "1.0.0",
            ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
            0,
    };

    return &kModule;
}
