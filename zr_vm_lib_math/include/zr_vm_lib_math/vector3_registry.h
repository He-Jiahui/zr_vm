//
// Vector3 registry accessors.
//

#ifndef ZR_VM_LIB_MATH_VECTOR3_REGISTRY_H
#define ZR_VM_LIB_MATH_VECTOR3_REGISTRY_H

#include "zr_vm_lib_math/vector3.h"

/** @file
 * ## Summary
 * 声明 zr.math Vector3 的静态 type、空 module-function 表与语言服务 hint getter。
 *
 * ## Constraints and lifecycle
 * descriptor/table 是本模块只读静态数据；调用方只借用、不释放，provider image 必须覆盖注册与 hint 消费期。
 * count 参数可为空。
 *
 * ## References
 * ZrVmLibMath_GetModuleDescriptor 聚合 getter；native metadata materializer 消费 type descriptor。
 */
/**
 * ## Summary
 * 向 zr.math 模块聚合器提供包含字段、方法和 meta callback 表的静态 Vector3 type descriptor。

 * ## Parameters
 * 本 getter 无显式参数。

 * ## Returns
 * @return 指向本翻译单元只读 kType 的借用指针；调用方不得释放，provider image 卸载后失效。

 * ## Constraints and lifecycle
 * descriptor 及嵌套表使用 static storage；native metadata 读取，不能保存到 provider 生命周期之外。

 * ## References
 * ZrVmLibMath_GetModuleDescriptor；native_binding_metadata 的 type materializer；本文件 kType。
 */
const ZrLibTypeDescriptor *ZrMath_Vector3Registry_GetType(void);
/**
 * ## Summary
 * 报告此 Vector3 registry 没有独立 module-level function；类型方法和 meta callback 由 GetType 提供。

 * ## Parameters
 * @param[out] count 可为空；非空时写 0。

 * ## Returns
 * @return 始终返回 ZR_NULL，表示顶层函数表为空；不影响 type descriptor 的方法/meta 表。

 * ## Constraints and lifecycle
 * 不分配或转移资源；count 只在本次调用写入，调用方须提供有效可写地址或传 ZR_NULL。

 * ## References
 * ZrVmLibMath_GetModuleDescriptor 的 function registry 聚合入口；本文件 kMethods 与 kMeta。
 */
const ZrLibFunctionDescriptor *ZrMath_Vector3Registry_GetFunctions(TZrSize *count);
/**
 * ## Summary
 * 向 math module descriptor 聚合器提供 Vector3 静态语言服务提示。

 * ## Parameters
 * @param[out] count 可为空；非空时写 1。

 * ## Returns
 * @return 指向本翻译单元只读 kHints 的借用指针；不得释放，provider image 卸载后失效。

 * ## Constraints and lifecycle
 * 数组和字面量使用 static storage；LSP 只读借用内容，不能延长 provider image 生命周期。

 * ## References
 * ZrVmLibMath_GetModuleDescriptor 的 hint registry 聚合入口；lsp_metadata_provider hint lookup；lsp_native_declaration_projection 声明投影。
 */
const ZrLibTypeHintDescriptor *ZrMath_Vector3Registry_GetHints(TZrSize *count);

#endif // ZR_VM_LIB_MATH_VECTOR3_REGISTRY_H
