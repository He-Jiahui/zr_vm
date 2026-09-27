#ifndef ZR_VM_LIBRARY_NATIVE_BINDING_CALL_BINDING_H
#define ZR_VM_LIBRARY_NATIVE_BINDING_CALL_BINDING_H

#include "zr_vm_library/native_binding.h"
#include "zr_vm_core/call_binding.h"

/** @brief 将函数、方法和元方法描述符投影为同一编译期调用绑定契约。 */
typedef enum EZrNativeCallBindingDescriptorKind {
    ZR_NATIVE_CALL_BINDING_FUNCTION = 0,
    ZR_NATIVE_CALL_BINDING_METHOD = 1,
    ZR_NATIVE_CALL_BINDING_META_METHOD = 2
} EZrNativeCallBindingDescriptorKind;

/** @brief 为指定原生描述符建立稳定的调用契约，供编译器和运行时验证身份。
 * @pre descriptor 必须与 kind 匹配，module/type 在调用期间保持有效。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_NativeCallBinding_GetDescriptorContract(
        const ZrLibModuleDescriptor *module,
        const ZrLibTypeDescriptor *type,
        EZrNativeCallBindingDescriptorKind kind,
        const void *descriptor,
        SZrCallBindingContract *outContract);

#endif
