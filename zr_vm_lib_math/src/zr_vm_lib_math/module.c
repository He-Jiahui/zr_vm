/** @file @brief 聚合 zr.math 的静态注册契约；函数声明、映像借用期及首次初始化同步前提见 module.h。 */
// 数值 callback 仍由各类型 registry 绑定，本层不执行计算或管理 VM 对象。
//

#include "zr_vm_lib_math/module.h"

#include "zr_vm_lib_math/complex_registry.h"
#include "zr_vm_lib_math/math_common.h"
#include "zr_vm_lib_math/matrix3x3_registry.h"
#include "zr_vm_lib_math/matrix4x4_registry.h"
#include "zr_vm_lib_math/quaternion_registry.h"
#include "zr_vm_lib_math/scalar_registry.h"
#include "zr_vm_lib_math/tensor_registry.h"
#include "zr_vm_lib_math/vector2_registry.h"
#include "zr_vm_lib_math/vector3_registry.h"
#include "zr_vm_lib_math/vector4_registry.h"

#include <math.h>
#include <string.h>

/* 当前 registry 合计 functions=25、types=8、hints=13；静态数组只容纳描述符浅拷贝，嵌套表仍借用原 registry。 */
/* TODO: append 未检查 offset/count 上界；新增 registry 时核对 48/8/32 容量及下方三组调用，尤其 types 当前恰满。 */
enum {
    ZR_MATH_MAX_FUNCTIONS = 48,
    ZR_MATH_MAX_TYPES = 8,
    ZR_MATH_MAX_HINTS = 32
};

static const ZrLibConstantDescriptor g_math_constants[] = {
        {"PI", ZR_LIB_CONSTANT_KIND_FLOAT, 0, ZR_MATH_PI, ZR_NULL, ZR_FALSE, ZR_NULL, "float"},
        {"TAU", ZR_LIB_CONSTANT_KIND_FLOAT, 0, ZR_MATH_TAU, ZR_NULL, ZR_FALSE, ZR_NULL, "float"},
        {"E", ZR_LIB_CONSTANT_KIND_FLOAT, 0, ZR_MATH_E, ZR_NULL, ZR_FALSE, ZR_NULL, "float"},
        {"EPSILON", ZR_LIB_CONSTANT_KIND_FLOAT, 0, ZR_MATH_EPSILON, ZR_NULL, ZR_FALSE, ZR_NULL, "float"},
        {"INF", ZR_LIB_CONSTANT_KIND_FLOAT, 0, INFINITY, ZR_NULL, ZR_FALSE, ZR_NULL, "float"},
        {"NAN", ZR_LIB_CONSTANT_KIND_FLOAT, 0, NAN, ZR_NULL, ZR_FALSE, ZR_NULL, "float"},
};

/* 这些表在模块映像装载期间供 registry 借用；首次聚合须串行，之后仅查询，不随任一 VM 销毁。 */
static ZrLibFunctionDescriptor g_math_functions[ZR_MATH_MAX_FUNCTIONS]; /* 仅模块级函数；实例/静态类型方法留在各 type descriptor 内。 */
static ZrLibTypeDescriptor g_math_types[ZR_MATH_MAX_TYPES]; /* 浅拷贝保留 methods/fields/meta 等 registry 静态指针。 */
static ZrLibTypeHintDescriptor g_math_hints[ZR_MATH_MAX_HINTS]; /* 结构化提示与可执行 descriptor 分开聚合。 */
static ZrLibModuleDescriptor g_math_module_descriptor; /* 静态零初始化保留未赋值的可选协议字段，不从某个 VM 借用状态。 */
static TZrBool g_math_initialized = ZR_FALSE; /* 只表示串行初始化完成，不是原子发布或锁。 */

/* JSON 是导出/反射附带的精简摘要，仅列 abs、invokeCallback，不是完整可执行表或已证编译器输入。BUG: 摘要同样把 invokeCallback 标为 float，而 CallValue 成功透传 bool/字符串等实际VM标签；见 Scalar 契约批次。 */
static const TZrChar *g_math_type_hints_json =
        "{\n"
        "  \"schema\": \"zr.native.hints/v1\",\n"
        "  \"module\": \"zr.math\",\n"
        "  \"functions\": [\n"
        "    {\"name\":\"abs\",\"signature\":\"abs(value: float): float\"},\n"
        "    {\"name\":\"invokeCallback\",\"signature\":\"invokeCallback(callback: function, value: float): float\"}\n"
        "  ],\n"
        "  \"types\": [\n"
        "    {\"name\":\"Vector2\",\"kind\":\"struct\"},\n"
        "    {\"name\":\"Vector3\",\"kind\":\"struct\"},\n"
        "    {\"name\":\"Vector4\",\"kind\":\"struct\"},\n"
        "    {\"name\":\"Quaternion\",\"kind\":\"struct\"},\n"
        "    {\"name\":\"Complex\",\"kind\":\"struct\"},\n"
        "    {\"name\":\"Matrix3x3\",\"kind\":\"struct\"},\n"
        "    {\"name\":\"Matrix4x4\",\"kind\":\"struct\"},\n"
        "    {\"name\":\"Tensor\",\"kind\":\"class\"}\n"
        "  ]\n"
        "}\n";
/* zr_math_append_functions 的完整调用契约见本文件末尾同名 @fn 文档；此处仅保留实现。 */
static void zr_math_append_functions(const ZrLibFunctionDescriptor *descriptors, TZrSize count, TZrSize *offset) {
    if (descriptors == ZR_NULL || offset == ZR_NULL || count == 0) {
        return;
    }
    memcpy(&g_math_functions[*offset], descriptors, sizeof(ZrLibFunctionDescriptor) * count);
    *offset += count;
}
/* zr_math_append_type 的完整调用契约见本文件末尾同名 @fn 文档；此处仅保留实现。 */
static void zr_math_append_type(const ZrLibTypeDescriptor *descriptor, TZrSize *offset) {
    if (descriptor == ZR_NULL || offset == ZR_NULL) {
        return;
    }
    g_math_types[*offset] = *descriptor;
    (*offset)++;
}
/* zr_math_append_hints 的完整调用契约见本文件末尾同名 @fn 文档；此处仅保留实现。 */
static void zr_math_append_hints(const ZrLibTypeHintDescriptor *descriptors, TZrSize count, TZrSize *offset) {
    if (descriptors == ZR_NULL || offset == ZR_NULL || count == 0) {
        return;
    }
    memcpy(&g_math_hints[*offset], descriptors, sizeof(ZrLibTypeHintDescriptor) * count);
    *offset += count;
}
/* 宏共享 initialize 的 count/offset 局部变量；getter 必须每次写 count，包括空表，避免带入上一组计数。 */
#define ZR_MATH_APPEND_FUNCTION_REGISTRY(getter)             \
    do {                                                     \
        const ZrLibFunctionDescriptor *descriptors = getter(&count); \
        zr_math_append_functions(descriptors, count, &offset);        \
    } while (0)
/* hint 宏使用独立重置后的 offset；两种追加宏只在本文件初始化函数内展开。 */
#define ZR_MATH_APPEND_HINT_REGISTRY(getter)                 \
    do {                                                     \
        const ZrLibTypeHintDescriptor *descriptors = getter(&count);  \
        zr_math_append_hints(descriptors, count, &offset);            \
    } while (0)

/* 先分别聚合函数、类型、提示并发布各自 count，再填写 ABI 等模块字段；末尾标志只适用于已由宿主同步的调用。 */
/* TODO: g_math_initialized 非原子且三组表可写；沿 CLI 内建注册和插件 symbol() 调用核实多 VM 首次并发策略，不能假定布尔检查提供互斥。 */
static void zr_math_initialize_module_descriptor(void) { /* 首次调用不分配 VM 资源；容量前提见上方 TODO，完整使用契约见 module.h。 */
    TZrSize count = 0;
    TZrSize offset = 0;

    if (g_math_initialized) {
        return;
    }
    /* 固定顺序影响导出遍历；目前只有 Scalar 返回非空模块级函数，其余类型的方法由 GetType 绑定。 */
    ZR_MATH_APPEND_FUNCTION_REGISTRY(ZrMath_ScalarRegistry_GetFunctions);
    ZR_MATH_APPEND_FUNCTION_REGISTRY(ZrMath_Vector2Registry_GetFunctions);
    ZR_MATH_APPEND_FUNCTION_REGISTRY(ZrMath_Vector3Registry_GetFunctions);
    ZR_MATH_APPEND_FUNCTION_REGISTRY(ZrMath_Vector4Registry_GetFunctions);
    ZR_MATH_APPEND_FUNCTION_REGISTRY(ZrMath_QuaternionRegistry_GetFunctions);
    ZR_MATH_APPEND_FUNCTION_REGISTRY(ZrMath_ComplexRegistry_GetFunctions);
    ZR_MATH_APPEND_FUNCTION_REGISTRY(ZrMath_Matrix3x3Registry_GetFunctions);
    ZR_MATH_APPEND_FUNCTION_REGISTRY(ZrMath_Matrix4x4Registry_GetFunctions);
    ZR_MATH_APPEND_FUNCTION_REGISTRY(ZrMath_TensorRegistry_GetFunctions);
    g_math_module_descriptor.functions = g_math_functions;
    g_math_module_descriptor.functionCount = offset;

    offset = 0; /* 切换到类型表前重置游标；八个类型正好占满当前容量。 */
    zr_math_append_type(ZrMath_Vector2Registry_GetType(), &offset);
    zr_math_append_type(ZrMath_Vector3Registry_GetType(), &offset);
    zr_math_append_type(ZrMath_Vector4Registry_GetType(), &offset);
    zr_math_append_type(ZrMath_QuaternionRegistry_GetType(), &offset);
    zr_math_append_type(ZrMath_ComplexRegistry_GetType(), &offset);
    zr_math_append_type(ZrMath_Matrix3x3Registry_GetType(), &offset);
    zr_math_append_type(ZrMath_Matrix4x4Registry_GetType(), &offset);
    zr_math_append_type(ZrMath_TensorRegistry_GetType(), &offset);
    g_math_module_descriptor.types = g_math_types;
    g_math_module_descriptor.typeCount = offset;

    offset = 0; /* 提示独立计数，不能沿用函数数或类型数，也不从精简 JSON 反向生成。 */
    ZR_MATH_APPEND_HINT_REGISTRY(ZrMath_ScalarRegistry_GetHints);
    ZR_MATH_APPEND_HINT_REGISTRY(ZrMath_Vector2Registry_GetHints);
    ZR_MATH_APPEND_HINT_REGISTRY(ZrMath_Vector3Registry_GetHints);
    ZR_MATH_APPEND_HINT_REGISTRY(ZrMath_Vector4Registry_GetHints);
    ZR_MATH_APPEND_HINT_REGISTRY(ZrMath_QuaternionRegistry_GetHints);
    ZR_MATH_APPEND_HINT_REGISTRY(ZrMath_ComplexRegistry_GetHints);
    ZR_MATH_APPEND_HINT_REGISTRY(ZrMath_Matrix3x3Registry_GetHints);
    ZR_MATH_APPEND_HINT_REGISTRY(ZrMath_Matrix4x4Registry_GetHints);
    ZR_MATH_APPEND_HINT_REGISTRY(ZrMath_TensorRegistry_GetHints);
    g_math_module_descriptor.typeHints = g_math_hints;
    g_math_module_descriptor.typeHintCount = offset;
    /* 这些 ABI/名称/表指针交给 native registry 校验；本层不替加载器管理共享库句柄。 */
    g_math_module_descriptor.abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION;
    g_math_module_descriptor.moduleName = "zr.math";
    g_math_module_descriptor.constants = g_math_constants;
    g_math_module_descriptor.constantCount = ZR_ARRAY_COUNT(g_math_constants);
    g_math_module_descriptor.typeHintsJson = g_math_type_hints_json;
    g_math_module_descriptor.documentation =
            "Built-in numeric algorithms, vector and matrix types, complex values, quaternions and tensors.";
    g_math_module_descriptor.moduleLinks = ZR_NULL;
    g_math_module_descriptor.moduleLinkCount = 0;
    g_math_module_descriptor.moduleVersion = "1.0.0";
    g_math_module_descriptor.minRuntimeAbi = ZR_VM_NATIVE_RUNTIME_ABI_VERSION;
    g_math_module_descriptor.requiredCapabilities = 0;
    /* 完整写入后才置标志；普通赋值没有跨线程发布保证，首次初始化仍需宿主串行化。 */
    g_math_initialized = ZR_TRUE;
}

#undef ZR_MATH_APPEND_FUNCTION_REGISTRY
#undef ZR_MATH_APPEND_HINT_REGISTRY

const ZrLibModuleDescriptor *ZrVmLibMath_GetModuleDescriptor(void) { /* 只查询/初始化静态描述符，不注册到 VM。 */
    zr_math_initialize_module_descriptor();
    return &g_math_module_descriptor;
}

TZrBool ZrVmLibMath_Register(SZrGlobalState *global) { /* 下层借用 descriptor；失败后的 Attach/错误状态遵循 native registry。 */
    zr_math_initialize_module_descriptor();
    return ZrLibrary_NativeRegistry_RegisterModule(global, &g_math_module_descriptor);
}

#if defined(ZR_LIBRARY_TYPE_SHARED)
const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void) { /* 动态加载器按协议符号调用，库句柄必须覆盖返回指针的借用期。 */
    return ZrVmLibMath_GetModuleDescriptor();
}
#endif

/**
 * @fn static void zr_math_append_functions(const ZrLibFunctionDescriptor *descriptors, TZrSize count, TZrSize *offset)
 * ## Summary
 * 在首次聚合模块函数表时复制一组 registry descriptor；类型方法仍留在各自 type descriptor。
 * ## Parameters
 * @param[in] descriptors 同步读取 count 项的来源表；NULL 时不写目标或游标。
 * @param[in] count 来源项数；0 时不写目标，非零时来源须覆盖全部项。
 * @param[in,out] offset 本次 initialize 的栈游标；NULL 时跳过，非空时指向已用项数。
 * ## Returns
 * 无返回值；有效非空输入复制到静态函数表并将游标增加 count；没有容量失败状态。
 * ## Constraints and lifecycle
 * 调用者须保证已用项数及 count 不超过 ZR_MATH_MAX_FUNCTIONS。只浅拷贝 descriptor，
 * 字符串、callback 和嵌套表仍由 registry 映像持有；本函数不取得或释放它们。
 * 游标不是全局状态；包含本调用的首次聚合须由宿主同步，不可由本 helper 推定线程安全。
 * ## References
 * ZR_MATH_APPEND_FUNCTION_REGISTRY 在 initialize 的函数组展开；zr_math_initialize_module_descriptor 发布表和 count。
 */

/**
 * @fn static void zr_math_append_type(const ZrLibTypeDescriptor *descriptor, TZrSize *offset)
 * ## Summary
 * 在类型聚合阶段复制单个类型 descriptor，保持原 registry 的字段、方法和 meta 关联。
 * ## Parameters
 * @param[in] descriptor 本次同步读取的来源；NULL 时跳过。
 * @param[in,out] offset initialize 的类型组栈游标；NULL 时跳过，非空时须小于 ZR_MATH_MAX_TYPES。
 * ## Returns
 * 无返回值；有效输入写一项并递增游标，不报告容量不足。
 * ## Constraints and lifecycle
 * 当前八个实际类型恰好用满八项容量；新增 getter 前必须核对上界。
 * 结构浅拷贝不取得嵌套资源所有权，registry 映像须覆盖所有 VM 对描述符的使用期；
 * 并发首次聚合的同步由调用者提供，helper 不加锁或分配 VM 对象。
 * ## References
 * zr_math_initialize_module_descriptor 依次调用八个类型 getter 后在此追加，并发布 typeCount。
 */

/**
 * @fn static void zr_math_append_hints(const ZrLibTypeHintDescriptor *descriptors, TZrSize count, TZrSize *offset)
 * ## Summary
 * 聚合工具可查询的结构化提示；此表与精简 JSON 摘要独立收集和计数。
 * ## Parameters
 * @param[in] descriptors 同步读取的 count 项提示表；NULL 时不写目标或游标。
 * @param[in] count 来源项数，0 时跳过。
 * @param[in,out] offset initialize 的提示组栈游标；NULL 时跳过。
 * ## Returns
 * 无返回值；有效输入复制提示并增加游标，不报告容量错误。
 * ## Constraints and lifecycle
 * 调用者保证 offset 加 count 不超过 ZR_MATH_MAX_HINTS；只复制 descriptor，
 * 其中字符串仍借用原 registry 的静态存储。必须在切换到提示组时重置游标，
 * 首次聚合与跨线程发布由宿主同步，本函数不创建 VM 根或持有调用栈。
 * ## References
 * ZR_MATH_APPEND_HINT_REGISTRY 在 initialize 的提示组展开，typeHintCount 使用独立结果。
 */

/**
 * @def ZR_MATH_APPEND_FUNCTION_REGISTRY(getter)
 * ## Summary
 * 在 initialize 内连接模块函数 getter 与浅拷贝 helper，统一收集各 registry 的函数项。
 * ## Parameters
 * @param[in] getter 接受 TZrSize 输出指针并返回静态函数表的函数；必须每次写 count，包括空表。
 * ## Returns
 * 无表达式返回值；更新同作用域的 count，并经 append 推进 offset 和写入函数数组。
 * ## Constraints and lifecycle
 * 只能在已声明 count/offset 的作用域展开；count 不由宏预先清零，getter 不可沿用上一次计数。
 * 调用前保证函数表剩余容量，聚合过程串行执行；来源表和嵌套资源继续由 registry 映像持有。
 * 宏在本文件聚合完成后的预处理位置 undef，不形成运行时入口。
 * ## References
 * zr_math_append_functions 执行复制；zr_math_initialize_module_descriptor 的函数组提供九次展开。
 */

/**
 * @def ZR_MATH_APPEND_HINT_REGISTRY(getter)
 * ## Summary
 * 在 initialize 内连接提示 getter 与提示表追加，保持提示与函数聚合的游标互不沿用。
 * ## Parameters
 * @param[in] getter 接受 TZrSize 输出指针并返回静态提示表的函数；空表也必须写 count。
 * ## Returns
 * 无表达式返回值；更新局部 count、提示组 offset 及静态提示数组，没有容量失败返回。
 * ## Constraints and lifecycle
 * 依赖同作用域 count/offset；进入提示组前 offset 必须重置，并保证剩余提示容量。
 * 只同步借用 getter 返回表；字符串不转移所有权，首次初始化须串行。
 * 本宏仅在本文件初始化中展开，并在末尾 undef。
 * ## References
 * zr_math_append_hints 执行浅拷贝；zr_math_initialize_module_descriptor 的提示组提供九次展开。
 */

/**
 * @fn static void zr_math_initialize_module_descriptor(void)
 * ## Summary
 * 供公开查询和注册入口共用的惰性初始化：分别聚合函数、类型及提示，再发布 Math 静态描述符。
 * ## Returns
 * 无返回值；已完成则直接返回，否则填写静态表/count/ABI 等字段并最后设置完成标志；
 * 不分配 VM 对象，也不提供容量失败或回滚状态。
 * ## Constraints and lifecycle
 * 当前实际数量为函数25/类型8/提示13，对应容量48/8/32；各组切换前独立重置栈游标。
 * 描述符只是浅借用，原 registry 的文本、字段、方法和 callback 依赖模块映像继续有效。
 * TODO: 完成标志和数组均非原子；沿宿主首次调用和插件发现核实同步政策。
 * 宿主必须串行首次初始化，并在后续跨线程读取前建立同步；普通布尔检查不是锁。
 * ## References
 * ZrVmLibMath_GetModuleDescriptor 与 ZrVmLibMath_Register 是实际调用者；
 * ZrVm_GetNativeModule_v1 经查询入口提供动态发现，NativeRegistry 负责 VM 注册和 ABI 校验。
 */
