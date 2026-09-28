#include "zr_vm_library/native_binding.h"

#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

/* combine 的形参类型与返回类型同步变化；签名测试据此检查参数提示是否取自
 * 当前项目加载的描述符，而非同名模块的旧缓存。 */
static const ZrLibParameterDescriptor g_descriptor_plugin_combine_parameters[] = {
    {"left", "float", "Left value to combine."},
    {"right", "float", "Right value to combine."},
};

/* LSP 只读取这些元数据生成悬停、签名及导航；callback 均为空，不能把此夹具
 * 当作可执行原生库。两份二进制通过 answer/combine 的类型差异测试项目隔离。 */
static const ZrLibFunctionDescriptor g_descriptor_plugin_functions[] = {
    {
        .name = "answer",
        .minArgumentCount = 0,
        .maxArgumentCount = 0,
        .callback = ZR_NULL,
        .returnTypeName = "float",
        .documentation = "Descriptor plugin fixture that exposes a float answer().",
        .parameters = ZR_NULL,
        .parameterCount = 0,
    },
    /* 返回类型把模块函数与 ProbePoint 的字段、方法投影连成同一查询链。 */
    {
        .name = "makePoint",
        .minArgumentCount = 0,
        .maxArgumentCount = 0,
        .callback = ZR_NULL,
        .returnTypeName = "ProbePoint",
        .documentation = "Descriptor plugin fixture that exposes a ProbePoint factory.",
        .parameters = ZR_NULL,
        .parameterCount = 0,
    },
    {
        .name = "combine",
        .minArgumentCount = 2,
        .maxArgumentCount = 2,
        .callback = ZR_NULL,
        .returnTypeName = "float",
        .documentation = "Combines two floating-point values for callable contract tests.",
        .parameters = g_descriptor_plugin_combine_parameters,
        .parameterCount = ZR_ARRAY_COUNT(g_descriptor_plugin_combine_parameters),
    },
    /* 故意缺少完整可调用契约，用于验证查询侧的 unavailable 结果。 */
    {
        .name = "incomplete_callable",
        .minArgumentCount = 0,
        .maxArgumentCount = 0,
        .callback = ZR_NULL,
        .returnTypeName = "unknown",
        .documentation = "Deliberately incomplete callable contract for unavailable tests.",
        .parameters = ZR_NULL,
        .parameterCount = 0,
    },
};

/* 类型成员导航从 makePoint 的 ProbePoint 返回类型继续解析到 x/y。 */
static const ZrLibFieldDescriptor g_descriptor_plugin_probe_point_fields[] = {
    ZR_LIB_FIELD_DESCRIPTOR_INIT("x", "int", "Probe point x coordinate."),
    ZR_LIB_FIELD_DESCRIPTOR_INIT("y", "int", "Probe point y coordinate."),
};

/* echo 的自由泛型 T 与 constrained_echo 的受限 T 分开登记，供签名查询区分。 */
static const ZrLibGenericParameterDescriptor
        g_descriptor_plugin_echo_generic_parameters[] = {
            {"T", "Echoed value type.", ZR_NULL, 0},
        };

/* 两种 echo 方法复用 value: T 形参；约束由各自泛型表表达。 */
static const ZrLibParameterDescriptor g_descriptor_plugin_echo_parameters[] = {
    {"value", "T", "Value returned with its closed generic type."},
};

/* constrained_echo 的 T 只接受当前模块描述的 ProbePoint。 */
static const TZrChar *g_descriptor_plugin_constrained_echo_constraints[] = {
    "ProbePoint",
};

/* 接收者方法签名查询须把此约束与自由泛型 echo 区分。 */
static const ZrLibGenericParameterDescriptor
        g_descriptor_plugin_constrained_echo_generic_parameters[] = {
            {
                "T",
                "Echoed value constrained to the receiver type.",
                g_descriptor_plugin_constrained_echo_constraints,
                ZR_ARRAY_COUNT(g_descriptor_plugin_constrained_echo_constraints),
            },
        };

/* 实例方法组成接收者调用查询的候选集；total 类型随模块版本变化，
 * echo 两种泛型验证闭包，而 incomplete_total 验证不完整契约的退化路径。 */
static const ZrLibMethodDescriptor g_descriptor_plugin_probe_point_methods[] = {
    {
        .name = "total",
        .minArgumentCount = 0,
        .maxArgumentCount = 0,
        .callback = ZR_NULL,
        .returnTypeName = "float",
        .documentation = "Returns the total coordinate value.",
        .isStatic = ZR_FALSE,
        .parameters = ZR_NULL,
        .parameterCount = 0,
        .contractRole = 0U,
        .genericParameters = ZR_NULL,
        .genericParameterCount = 0,
    },
    {
        .name = "echo",
        .minArgumentCount = 1,
        .maxArgumentCount = 1,
        .callback = ZR_NULL,
        .returnTypeName = "T",
        .documentation = "Returns a value using an unconstrained method generic.",
        .isStatic = ZR_FALSE,
        .parameters = g_descriptor_plugin_echo_parameters,
        .parameterCount = ZR_ARRAY_COUNT(g_descriptor_plugin_echo_parameters),
        .contractRole = 0U,
        .genericParameters = g_descriptor_plugin_echo_generic_parameters,
        .genericParameterCount =
                ZR_ARRAY_COUNT(g_descriptor_plugin_echo_generic_parameters),
    },
    {
        .name = "constrained_echo",
        .minArgumentCount = 1,
        .maxArgumentCount = 1,
        .callback = ZR_NULL,
        .returnTypeName = "T",
        .documentation = "Returns a value through a constrained method generic.",
        .isStatic = ZR_FALSE,
        .parameters = g_descriptor_plugin_echo_parameters,
        .parameterCount = ZR_ARRAY_COUNT(g_descriptor_plugin_echo_parameters),
        .contractRole = 0U,
        .genericParameters =
                g_descriptor_plugin_constrained_echo_generic_parameters,
        .genericParameterCount = ZR_ARRAY_COUNT(
                g_descriptor_plugin_constrained_echo_generic_parameters),
    },
    {
        .name = "incomplete_total",
        .minArgumentCount = 0,
        .maxArgumentCount = 0,
        .callback = ZR_NULL,
        .returnTypeName = "unknown",
        .documentation = "Deliberately incomplete receiver callable contract.",
        .isStatic = ZR_FALSE,
        .parameters = ZR_NULL,
        .parameterCount = 0,
        .contractRole = 0U,
        .genericParameters = ZR_NULL,
        .genericParameterCount = 0,
    },
};

/* 将字段与方法绑定为原生 ProbePoint 结构体，供类型成员定义和语义 token 投影。 */
static const ZrLibTypeDescriptor g_descriptor_plugin_types[] = {
    ZR_LIB_TYPE_DESCRIPTOR_INIT("ProbePoint",
                                ZR_OBJECT_PROTOTYPE_TYPE_STRUCT,
                                g_descriptor_plugin_probe_point_fields,
                                ZR_ARRAY_COUNT(g_descriptor_plugin_probe_point_fields),
                                g_descriptor_plugin_probe_point_methods,
                                ZR_ARRAY_COUNT(g_descriptor_plugin_probe_point_methods),
                                ZR_NULL,
                                0,
                                "Descriptor plugin fixture native struct for member navigation tests.",
                                ZR_NULL,
                                ZR_NULL,
                                0,
                                ZR_NULL,
                                0,
                                ZR_NULL,
                                ZR_FALSE,
                                ZR_TRUE,
                                ZR_NULL,
                                ZR_NULL,
                                0),
};

/* plugin.console 跨到内建 console 子模块，验证虚拟声明的链式解析。 */
static const ZrLibModuleLinkDescriptor g_descriptor_plugin_module_links[] = {
    {"console", "zr.system.console", "Linked builtin console submodule for chained metadata resolution tests."},
};

/* 两份动态库故意共享模块名与 ABI；项目 fixture 复制不同库后，
 * LSP 应以本项目来源选择该二进制的 float 签名。 */
static const ZrLibModuleDescriptor g_descriptor_plugin_module = {
    .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
    .moduleName = "zr.pluginprobe",
    .constants = ZR_NULL,
    .constantCount = 0,
    .functions = g_descriptor_plugin_functions,
    .functionCount = ZR_ARRAY_COUNT(g_descriptor_plugin_functions),
    .types = g_descriptor_plugin_types,
    .typeCount = ZR_ARRAY_COUNT(g_descriptor_plugin_types),
    .typeHints = ZR_NULL,
    .typeHintCount = 0,
    .typeHintsJson = ZR_NULL,
    .documentation = "Descriptor plugin fixture module with answer(): float.",
    .moduleLinks = g_descriptor_plugin_module_links,
    .moduleLinkCount = ZR_ARRAY_COUNT(g_descriptor_plugin_module_links),
    .moduleVersion = "1.0.0",
    .minRuntimeAbi = ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
    .requiredCapabilities = 0,
};

/** @brief 供原生插件加载器通过固定符号取得静态模块描述符。
 *  @note 返回指针的生命周期覆盖动态库加载期间；调用方不得释放或修改。 */
const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void);

const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void) {
    return &g_descriptor_plugin_module;
}
