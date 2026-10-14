//
// Vector4 registry accessors.
//

#ifndef ZR_VM_LIB_MATH_VECTOR4_REGISTRY_H
#define ZR_VM_LIB_MATH_VECTOR4_REGISTRY_H

#include "zr_vm_lib_math/vector4.h"

/** @file
 * ## Summary
 * 声明 Vector4 的 type、空顶层 function 表和语言服务 hint 静态注册表 getter，供 zr.math 聚合器调用。
 *
 * ## Constraints and lifecycle
 * 返回的 descriptor、嵌套表、字符串字面量和 callback 地址均由 math provider image 持有；调用方只借用，
 * 不得释放或在 provider 卸载后使用。count 是可选输出参数；传入时须指向本次调用期间可写的有效地址。
 *
 * ## References
 * ZrVmLibMath_GetModuleDescriptor；native_binding_metadata；lsp_metadata_provider；
 * lsp_native_declaration_projection
 */
/**
 * ## Summary
 * 向 zr.math 聚合器提供 Vector4 的静态 type descriptor，包含字段、方法与 meta callback 表。
 *
 * ## Parameters
 * 无显式参数。
 *
 * ## Returns
 * @return 指向本翻译单元静态 `kType` 的借用指针；调用方不得释放，且不得越过 provider image 生命周期保存。
 *
 * ## Constraints and lifecycle
 * `kType` 及其 `kFields`、`kMethods`、`kMeta` 和嵌套字符串都使用静态存储；注册与 metadata 读取方必须在
 * provider 有效期间消费这些指针。函数不创建 VM 实例或动态分配表。
 *
 * ## References
 * ZrVmLibMath_GetModuleDescriptor 的 type 聚合；native_binding_metadata 的 type/method materializer；
 * ZrMath_Vector4_Construct 与其他 descriptor callback
 */
const ZrLibTypeDescriptor *ZrMath_Vector4Registry_GetType(void);
/**
 * ## Summary
 * 声明 Vector4 registry 没有独立的 module-level function；类型方法和 meta callback 由 GetType 提供。
 *
 * ## Parameters
 * @param[out] count 可传 ZR_NULL；非空时写入 0，调用方须提供可写地址。
 *
 * ## Returns
 * @return 始终返回 ZR_NULL，表示顶层 function 表为空；不会影响 GetType 中的方法或 meta 表。
 *
 * ## Constraints and lifecycle
 * 不分配或转移资源；非空 count 只在本次调用内写入。函数行为不依赖 VM state。
 *
 * ## References
 * ZrVmLibMath_GetModuleDescriptor 的 function 聚合；ZR_MATH_APPEND_FUNCTION_REGISTRY；本文件的 kMethods/kMeta
 */
const ZrLibFunctionDescriptor *ZrMath_Vector4Registry_GetFunctions(TZrSize *count);
/**
 * ## Summary
 * 向 math module descriptor 聚合器提供 Vector4 的单项静态 type hint，供语言服务读取。
 *
 * ## Parameters
 * @param[out] count 可传 ZR_NULL；非空时写入 hint 数量 1，调用方须提供可写地址。
 *
 * ## Returns
 * @return 指向本翻译单元静态 `kHints` 的借用指针；不得释放，且 provider image 卸载后失效。
 *
 * ## Constraints and lifecycle
 * hint 数组与描述文字均为静态存储。module 聚合器及 LSP metadata/declaration projection 只读借用，
 * 不取得所有权，也不能延长 provider image 生命周期。
 *
 * ## References
 * ZrVmLibMath_GetModuleDescriptor 的 hint 聚合；lsp_metadata_provider 的 hint 查询；
 * lsp_native_declaration_projection 的声明投影
 */
const ZrLibTypeHintDescriptor *ZrMath_Vector4Registry_GetHints(TZrSize *count);

#endif // ZR_VM_LIB_MATH_VECTOR4_REGISTRY_H
