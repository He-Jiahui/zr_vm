#ifndef ZR_VM_CORE_MODULE_CALL_BINDING_H
#define ZR_VM_CORE_MODULE_CALL_BINDING_H

#include "zr_vm_core/call_binding.h"

struct SZrObjectModule;

/** @brief 取得模块的反射入口函数，供重载失效和导入重定位寻找 provider 图。 */
ZR_CORE_API struct SZrFunction *ZrCore_CallBinding_GetModuleFunction(struct SZrState *state,
        struct SZrObjectModule *module);

/** @brief 为导入类型推断从 provider 的常量及成员元数据还原可持久化调用契约。 */
ZR_CORE_API TZrBool ZrCore_CallBinding_ModuleConstantContract(const struct SZrFunction *provider,
        TZrUInt32 constantIndex, SZrCallBindingContract *contract);
/** @brief 导入时按 token 与签名重定位 consumer 函数图中的 VM_MODULE 调用点；重载后可重绑。 */
ZR_CORE_API TZrBool ZrCore_CallBinding_LinkImportedModule(struct SZrState *state,
        struct SZrFunction *consumer, struct SZrObjectModule *module,
        SZrCallBindingDiagnostic *diagnostic);

#endif
