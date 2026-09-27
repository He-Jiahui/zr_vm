#ifndef ZR_VM_CORE_TYPED_CALL_BINDING_H
#define ZR_VM_CORE_TYPED_CALL_BINDING_H

#include "zr_vm_core/function.h"

/** @brief 链接类型化函数值的签名记录，保留运行时目标为空以等待实际可调用值。 */
ZR_CORE_API TZrBool ZrCore_CallBinding_LinkTypedSignature(
        SZrFunction *function, SZrFunctionCallSiteCacheEntry *entry,
        SZrCallBindingDiagnostic *diagnostic);
/** @brief 每次调用按实际值验证结构化签名并记录 GC 可见的见证；不会替换原闭包。 */
ZR_CORE_API TZrBool ZrCore_CallBinding_PrepareTypedCall(
        struct SZrState *state, SZrFunction *function, SZrFunctionCallSiteCacheEntry *entry,
        const SZrTypeValue *callable, SZrCallBindingDiagnostic *diagnostic);

#endif
