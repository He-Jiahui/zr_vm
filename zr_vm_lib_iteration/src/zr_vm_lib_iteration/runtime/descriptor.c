#include "zr_vm_lib_iteration/module.h"

#include "zr_vm_core/object.h"

/**
 * ## Summary
 * 本文件集中定义 zr.iteration 的官方协议表；具体集合的同步迭代回调由 zr.container 等 provider 提供。
 * ## Constraints and lifecycle
 * 表项均为静态只读元数据。带 NULL callback 的方法只描述协议，不会由本模块生成可调用原生方法。
 * ## References
 * @ref g_iteration_types; @ref g_iteration_module_descriptor; native_binding_metadata.c::native_registry_add_type.
 */
/**
 * ## Summary
 * 为迭代协议类型提供共享泛型参数 T，表示游标产出的元素类型；此处没有附加边界或默认值。
 * ## Constraints and lifecycle
 * 静态数组由各协议类型描述符借用，不应单独释放或修改。
 * ## References
 * @ref g_iteration_types.
 */
static const ZrLibGenericParameterDescriptor g_iteration_generic_parameter[] = {
        {"T", "Iteration element type.", ZR_NULL, 0},
};

/**
 * ## Summary
 * 声明编译器生成的 Iterator<T> 载体实现规范 Enumerator<T> 协议，使消费者依协议事实识别其能力。
 * ## Constraints and lifecycle
 * Iterator 是不透明的编译器载体，不是脚本可直接构造的普通接口实例；字符串由静态表持有。
 * ## References
 * @ref g_iteration_types; @ref ZR_PROTOCOL_ID_ITERATOR; @ref ZrParser_EnumeratorBinding_ResolveElementType.
 */
static const TZrChar *g_iterator_implements[] = {
        "zr.iteration.Enumerator<T>",
};

/**
 * ## Summary
 * 发布同步 Enumerator<T> 的 current 字段，并以 ITERATOR_CURRENT_FIELD 角色让原生 prototype 记录当前值成员。
 * ## Constraints and lifecycle
 * 字段描述符为静态元数据；ROLE_INIT 当前将 isReadonly 设为 false，元数据因此投影为可写，但这本身不能证明
 * 运行时赋值会被接受。TODO: 确认脚本能否改写 current。核查入口：用真实 Enumerator 在 moveNext 成功后执行
 * current = value，并沿原生属性赋值器与 prototype 元数据投影确认写入结果；当前协议测试未覆盖此赋值。
 * ## References
 * native_binding_metadata.c::native_registry_add_field_descriptors; @ref ZR_MEMBER_CONTRACT_ROLE_ITERATOR_CURRENT_FIELD;
 * @ref test_iteration_descriptor_owns_public_enumerator_contract.
 */
static const ZrLibFieldDescriptor g_enumerator_fields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_ROLE_INIT("current", "T", "Current element after a successful moveNext.",
                                          ZR_MEMBER_CONTRACT_ROLE_ITERATOR_CURRENT_FIELD),
};

/**
 * ## Summary
 * 仅为 AsyncIterator<T> 描述 current 成员角色，供未来异步协议消费者识别。
 * ## Constraints and lifecycle
 * 当前字段只进入静态描述符；同步 foreach 绑定器只查同步 ITERATOR/ITERABLE 能力，不消费 ASYNC_ITERATOR。
 * ## References
 * @ref g_iteration_types; @ref ZrParser_EnumeratorBinding_ResolveElementType; @ref ZR_MEMBER_CONTRACT_ROLE_ASYNC_ITERATOR_CURRENT_FIELD.
 */
static const ZrLibFieldDescriptor g_async_iterator_fields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_ROLE_INIT("current", "T", "Current element after a successful asynchronous moveNext.",
                                          ZR_MEMBER_CONTRACT_ROLE_ASYNC_ITERATOR_CURRENT_FIELD),
};

/**
 * ## Summary
 * 描述 Iterable<T> 的规范初始化成员 getEnumerator，返回 Enumerator<T>。
 * ## Constraints and lifecycle
 * callback 为 NULL，因此元数据构建器跳过该方法，不会由本描述符生成可调用成员；容器通过 getIterator callback
 * 并绑定相同的 ITERABLE_INIT 角色提供实际同步生产者。表项静态只读。
 * ## References
 * native_binding_metadata.c::native_registry_add_methods; @ref g_enumerator_methods; @ref ZR_MEMBER_CONTRACT_ROLE_ITERABLE_INIT;
 * @ref ZrVmLibContainer_Register.
 */
static const ZrLibMethodDescriptor g_iterable_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_ROLE_INIT("getEnumerator", 0, 0, ZR_NULL,
                                           "zr.iteration.Enumerator<T>",
                                           "Create a new synchronous enumerator.", ZR_FALSE, ZR_NULL, 0,
                                           ZR_MEMBER_CONTRACT_ROLE_ITERABLE_INIT),
};

/**
 * ## Summary
 * 描述同步 Enumerator<T> 的 moveNext 成员及 bool 结果，用于统一协议角色检查。
 * ## Constraints and lifecycle
 * callback 为 NULL，本表只提供规范签名；容器 provider 创建实际闭包并绑定 ITERATOR_MOVE_NEXT 角色。
 * 元数据构建会跳过没有 callback 的方法，表项由静态描述符持有。
 * ## References
 * native_binding_metadata.c::native_registry_add_methods; @ref ZR_MEMBER_CONTRACT_ROLE_ITERATOR_MOVE_NEXT;
 * @ref ZrVmLibContainer_Register.
 */
static const ZrLibMethodDescriptor g_enumerator_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_ROLE_INIT("moveNext", 0, 0, ZR_NULL, "bool",
                                           "Advance the enumerator.", ZR_FALSE, ZR_NULL, 0,
                                           ZR_MEMBER_CONTRACT_ROLE_ITERATOR_MOVE_NEXT),
};

/**
 * ## Summary
 * 描述 AsyncIterator<T> 的 moveNext 与 close 协议签名，结果分别为 Task<bool> 和 Task<void>。
 * ## Constraints and lifecycle
 * 两项 callback 均为 NULL；当前这里只发布静态描述符，没有在本模块实现异步推进或关闭回调。
 * 同步 EnumeratorBinding 不消费异步角色，不能据此推断异步循环已实现。
 * ## References
 * native_binding_metadata.c::native_registry_add_methods; @ref ZrParser_EnumeratorBinding_ResolveElementType;
 * @ref ZR_MEMBER_CONTRACT_ROLE_ASYNC_ITERATOR_MOVE_NEXT; @ref ZR_MEMBER_CONTRACT_ROLE_ASYNC_ITERATOR_CLOSE.
 */
static const ZrLibMethodDescriptor g_async_iterator_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_ROLE_INIT("moveNext", 0, 0, ZR_NULL, "zr.task.Task<bool>",
                                           "Advance the asynchronous iterator.", ZR_FALSE, ZR_NULL, 0,
                                           ZR_MEMBER_CONTRACT_ROLE_ASYNC_ITERATOR_MOVE_NEXT),
        ZR_LIB_METHOD_DESCRIPTOR_ROLE_INIT("close", 0, 0, ZR_NULL, "zr.task.Task<void>",
                                           "Asynchronously release the iterator.", ZR_FALSE, ZR_NULL, 0,
                                           ZR_MEMBER_CONTRACT_ROLE_ASYNC_ITERATOR_CLOSE),
};

/**
 * ## Summary
 * 集中发布 Iterable、Enumerator、Iterator 和 AsyncIterator 四种规范类型及其能力位。
 * ## Constraints and lifecycle
 * Iterator 与 Enumerator 共用 ITERATOR 位，编译器载体因此不依赖具体类型名；AsyncIterator 使用独立能力位。
 * 所有嵌套数组和泛型参数由本文件静态持有，类型描述符仅引用它们。
 * ## References
 * @ref g_iteration_generic_parameter; @ref g_iterator_implements; @ref g_iterable_methods;
 * @ref g_enumerator_methods; @ref g_async_iterator_fields; @ref g_async_iterator_methods;
 * @ref ZrParser_EnumeratorBinding_ResolveElementType; native_binding_metadata.c::native_registry_add_type.
 */
static const ZrLibTypeDescriptor g_iteration_types[] = {
        ZR_LIB_TYPE_DESCRIPTOR_PROTOCOL_INIT("Iterable", ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE,
                                             ZR_NULL, 0, g_iterable_methods, ZR_ARRAY_COUNT(g_iterable_methods),
                                             ZR_NULL, 0, "Repeatable synchronous iteration capability.",
                                             ZR_NULL, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE,
                                             ZR_NULL, g_iteration_generic_parameter,
                                             ZR_ARRAY_COUNT(g_iteration_generic_parameter),
                                             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE)),
        ZR_LIB_TYPE_DESCRIPTOR_PROTOCOL_INIT("Enumerator", ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE,
                                             g_enumerator_fields, ZR_ARRAY_COUNT(g_enumerator_fields),
                                             g_enumerator_methods, ZR_ARRAY_COUNT(g_enumerator_methods),
                                             ZR_NULL, 0, "Single-pass synchronous cursor capability.",
                                             ZR_NULL, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE,
                                             ZR_NULL, g_iteration_generic_parameter,
                                             ZR_ARRAY_COUNT(g_iteration_generic_parameter),
                                             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERATOR)),
        ZR_LIB_TYPE_DESCRIPTOR_PROTOCOL_INIT("Iterator", ZR_OBJECT_PROTOTYPE_TYPE_STRUCT,
                                             ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, 0,
                                             "Compiler-backed opaque synchronous iterator carrier.",
                                             ZR_NULL, g_iterator_implements, ZR_ARRAY_COUNT(g_iterator_implements),
                                             ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, ZR_NULL,
                                             g_iteration_generic_parameter,
                                             ZR_ARRAY_COUNT(g_iteration_generic_parameter),
                                             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERATOR)),
        ZR_LIB_TYPE_DESCRIPTOR_PROTOCOL_INIT("AsyncIterator", ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE,
                                             g_async_iterator_fields, ZR_ARRAY_COUNT(g_async_iterator_fields),
                                             g_async_iterator_methods, ZR_ARRAY_COUNT(g_async_iterator_methods),
                                             ZR_NULL, 0, "Single-pass asynchronous cursor capability.",
                                             ZR_NULL, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE,
                                             ZR_NULL, g_iteration_generic_parameter,
                                             ZR_ARRAY_COUNT(g_iteration_generic_parameter),
                                             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ASYNC_ITERATOR)),
};

/**
 * ## Summary
 * 定义官方 zr.iteration Runtime provider 描述符，发布上述规范类型并提供稳定的 ABI、版本及公共契约哈希。
 * ## Constraints and lifecycle
 * 描述符和被引用的静态类型数组由本模块持有；注册表借用该地址，调用方不得释放或修改。
 * providerPhase 为 Runtime，无模块链接、能力要求或 onMaterialize 回调；兼容性由 ABI/version/hash 字段校验。
 * ## References
 * @ref ZrVmLibIteration_Runtime_GetModuleDescriptor; @ref ZrLibrary_NativeRegistry_RegisterModule;
 * @ref ZrLibrary_NativeRegistry_ValidateModuleDescriptor; native_binding_official_inventory.c::native_registry_validate_official_descriptor.
 */
static const ZrLibModuleDescriptor g_iteration_module_descriptor = {
        .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
        .moduleName = "zr.iteration",
        .constants = ZR_NULL,
        .constantCount = 0,
        .functions = ZR_NULL,
        .functionCount = 0,
        .types = g_iteration_types,
        .typeCount = ZR_ARRAY_COUNT(g_iteration_types),
        .typeHints = ZR_NULL,
        .typeHintCount = 0,
        .typeHintsJson = ZR_NULL,
        .documentation = "Canonical synchronous and asynchronous iteration capabilities.",
        .moduleLinks = ZR_NULL,
        .moduleLinkCount = 0,
        .moduleVersion = "1.0.0",
        .minRuntimeAbi = ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
        .requiredCapabilities = 0,
        .onMaterialize = ZR_NULL,
        .providerPhase = ZR_LIBRARY_PROVIDER_PHASE_RUNTIME,
        .publicContractHash = "zr.iteration:v1:canonical-iterator-protocols",
};

/**
 * ## Summary
 * 返回本模块唯一的静态 Runtime 描述符，供公开 facade 与注册入口转发。
 * ## Returns
 * @return @ref g_iteration_module_descriptor 的借用只读地址；不分配内存，不执行 provider 回调，无失败返回。
 * ## Constraints and lifecycle
 * 指针有效期与 provider 映像一致；调用方不得释放或修改描述符。当前仓内直接调用者为 module.c facade。
 * ## References
 * @ref g_iteration_module_descriptor; @ref ZrVmLibIteration_GetModuleDescriptor;
 * @ref ZrVmLibIteration_Register.
 */
const ZrLibModuleDescriptor *ZrVmLibIteration_Runtime_GetModuleDescriptor(void) {
    return &g_iteration_module_descriptor;
}
