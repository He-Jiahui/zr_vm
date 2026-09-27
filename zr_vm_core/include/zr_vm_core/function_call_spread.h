#ifndef ZR_VM_CORE_FUNCTION_CALL_SPREAD_H
#define ZR_VM_CORE_FUNCTION_CALL_SPREAD_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/value.h"

/** @brief 供解释器和 AOT 在预留调用窗口前读取末尾展开数组的元素数。
 * @pre spreadValue 是仍有效的值视图；outArgumentCount 不可为空。
 * @return 仅接受内部类型为数组的对象；失败时输出计数清零。
 */
ZR_CORE_API TZrBool ZrCore_Function_CallSpread_TryGetArgumentCount(
        const SZrTypeValue *spreadValue,
        TZrSize *outArgumentCount);

/** @brief 将已预留的调用窗口中最后一个数组实参替换为逐元素实参。
 * @pre callWindow 的首槽是 callable，随后有 prefixArgumentCount 个实参及一个展开数组；
 *      调用方已为数组元素预留足够栈槽，且调用期间保持窗口指针有效。
 * @note 解释器由 PreCallPrepared 间接使用，AOT 在扩栈后直接使用；成功时窗口顶和实参数改变。
 */
ZR_CORE_API TZrBool ZrCore_Function_CallSpread_ExpandPrepared(
        struct SZrState *state,
        TZrStackValuePointer callWindow,
        TZrSize prefixArgumentCount,
        TZrSize *outArgumentCount);

#endif
