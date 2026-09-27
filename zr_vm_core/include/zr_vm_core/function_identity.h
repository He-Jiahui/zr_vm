#ifndef ZR_VM_CORE_FUNCTION_IDENTITY_H
#define ZR_VM_CORE_FUNCTION_IDENTITY_H

#include "zr_vm_core/function.h"

/** @brief 判定两个函数是否指向同一定义，供子函数图、AOT 表及导入绑定去重。
 * @note 函数常量的地址可在别名重绑前不同；普通字面量仍参与比较。 */
ZR_CORE_API TZrBool ZrCore_Function_HasSameDefinition(const SZrFunction *left, const SZrFunction *right);
/** @brief 从 callable constant 元数据记录查找明确的子函数别名。
 * @pre function、childIndex 和 hasDefinition 均非空。
 * @return 成功时写入 childIndex；失败时仅保证 hasDefinition 已更新。 */
ZR_CORE_API TZrBool ZrCore_Function_FindConstantChildAlias(const SZrFunction *function,
        TZrUInt32 constantIndex, TZrUInt32 *childIndex, TZrBool *hasDefinition);

#endif
