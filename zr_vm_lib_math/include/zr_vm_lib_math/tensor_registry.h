//
// Tensor registry accessors.
//

#ifndef ZR_VM_LIB_MATH_TENSOR_REGISTRY_H
#define ZR_VM_LIB_MATH_TENSOR_REGISTRY_H

#include "zr_vm_lib_math/tensor.h"

/** @file
 * ## Summary
 * 将 Tensor 的静态类型、空模块函数集合和类型形状提示暴露给 Math module 聚合器。
 * ## Constraints and lifecycle
 * 返回的静态表由 provider 映像拥有，仅在该映像保持加载期间借用有效；count 可为空。
 * ## References
 * zr_vm_lib_math/src/zr_vm_lib_math/module.c 的类型/函数/hint 聚合与 native metadata 导出。
 */
const ZrLibTypeDescriptor *ZrMath_TensorRegistry_GetType(void);
const ZrLibFunctionDescriptor *ZrMath_TensorRegistry_GetFunctions(TZrSize *count);
const ZrLibTypeHintDescriptor *ZrMath_TensorRegistry_GetHints(TZrSize *count);

/**
 * @fn const ZrLibTypeDescriptor *ZrMath_TensorRegistry_GetType(void)
 * ## Summary
 * 向 Math module 聚合器提供 Tensor 的类型描述符，供注册器建立公开字段、方法和元方法绑定。
 *
 * ## Returns
 * @return 借用的静态类型描述符；调用者不得释放或在 provider 映像卸载后继续使用。
 *
 * ## Constraints and lifecycle
 * 无参数；descriptor 及引用的静态表仅在 provider 映像驻留期间有效。BUG: kType.constructorSignature 的第二参数文本为 fillValue: float，而 constructor meta arity 为 2 且 callback 用 ReadArray 读取第二个 data 数组；这是导出 metadata 失真。metadata 导出器复制签名，type inference 仅保存文本并检查是否为空，不代表 dispatcher 拒绝调用或将 float 作为实际参数类型。
 *
 * ## References
 * zr_vm_lib_math/src/zr_vm_lib_math/module.c:132；tensor_registry.c:39,42-61；native_binding_metadata.c:801,808,817,826；type_inference_native.c:3420,3497；type_inference_core.c:1575-1599。
 */
/**
 * @fn const ZrLibFunctionDescriptor *ZrMath_TensorRegistry_GetFunctions(TZrSize *count)
 * ## Summary
 * 实现 Math module 统一函数表接口；Tensor 不向模块级函数表增加条目，方法仍由 GetType 返回。
 *
 * ## Parameters
 * @param[out] count 可为空；非空时返回模块级函数数量，本 getter 写零。
 *
 * ## Returns
 * @return 始终为 ZR_NULL，表示本类型注册表没有模块级函数描述符；调用者不得解引用或释放返回值。
 *
 * ## Constraints and lifecycle
 * 该接口由模块聚合宏统一调用；它不影响 Tensor 实例的 kMethods 表。count 只在非空时写入。
 *
 * ## References
 * zr_vm_lib_math/src/zr_vm_lib_math/module.c 的 ZR_MATH_APPEND_FUNCTION_REGISTRY；tensor_registry.c 的 getter 实现与 GetType.kMethods。
 */
/**
 * @fn const ZrLibTypeHintDescriptor *ZrMath_TensorRegistry_GetHints(TZrSize *count)
 * ## Summary
 * 向 Math module 聚合器提供一条 Tensor 类型形状提示，供 native module metadata 导出。
 *
 * ## Parameters
 * @param[out] count 可为空；非空时写入返回的静态提示条数。
 *
 * ## Returns
 * @return 借用的静态提示表；调用者不得释放，且仅在 provider 映像驻留期间有效。
 *
 * ## Constraints and lifecycle
 * 当前只导出 class Tensor 的形状提示；已证实的消费路径是 ModuleInfo.typeHints 元数据，不据此声称 parser 或 LSP 将其作为编译期约束。
 *
 * ## References
 * zr_vm_lib_math/src/zr_vm_lib_math/module.c 的 ZR_MATH_APPEND_HINT_REGISTRY；zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c 的 typeHints 导出。
 */

#endif // ZR_VM_LIB_MATH_TENSOR_REGISTRY_H
