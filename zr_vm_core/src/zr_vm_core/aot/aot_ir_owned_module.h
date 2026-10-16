#ifndef ZR_VM_CORE_AOT_IR_OWNED_MODULE_H
#define ZR_VM_CORE_AOT_IR_OWNED_MODULE_H

#include "zr_vm_core/aot_ir.h"

/** @brief 独立拥有模块及可达数组的 Core arena；不得浅拷贝所有权。 */
typedef struct SZrAotIrOwnedModule SZrAotIrOwnedModule;

/**
 * @brief 校验并复制模块，成功后替换 destination 中的唯一所有者。
 * @pre source 及完整数组范围调用期间可读且无并发修改；destination 指向
 *      初始化为 NULL 或本接口返回的唯一 live owner 的指针槽。该槽和
 *      可选 diagnostic 不覆盖 source、旧 arena 或彼此。
 * @note source 可为旧 owner 的 view；成功时建立新的单一 arena 后再释放旧
 *       owner，失败不改变旧 owner。零长度数组规范化为 NULL，map 容量规范化
 *       为对应计数；保留存在但为空的 map header。本接口不授予执行权限。
 */
ZR_CORE_API EZrAotIrStatus ZrCore_AotIr_CloneOwnedModule(
        const SZrAotIrModule *source, SZrAotIrOwnedModule **destination,
        SZrAotIrDiagnostic *diagnostic);

/** @brief 借用模块至释放或成功替换；NULL owner 返回 NULL。 */
ZR_CORE_API const SZrAotIrModule *ZrCore_AotIr_OwnedModuleView(
        const SZrAotIrOwnedModule *owner);

/** @brief 释放唯一 arena 并清空槽；NULL 参数和空槽可重复调用。 */
ZR_CORE_API void ZrCore_AotIr_OwnedModuleFree(SZrAotIrOwnedModule **owner);

#endif /* ZR_VM_CORE_AOT_IR_OWNED_MODULE_H */
