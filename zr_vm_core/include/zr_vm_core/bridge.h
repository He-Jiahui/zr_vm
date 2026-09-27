#ifndef ZR_VM_CORE_BRIDGE_H
#define ZR_VM_CORE_BRIDGE_H

#include "zr_vm_core/conf.h"

struct SZrCallInfo;
struct SZrState;
struct SZrTypeValue;

/** @brief AOT 的 TO_OBJECT 边界：将帧槽值和类型名常量交给解释执行的装箱语义。
 * @pre state、destination、source、typeNameValue 有效；typeNameValue 应为字符串。
 * @return false 表示无效实参；true 时结果仍可能为空，目标原型缺失时对象源也可能原样复制。 */
ZR_CORE_API TZrBool ZrCore_Bridge_BoxTyped(struct SZrState *state,
                                           struct SZrCallInfo *callInfo,
                                           struct SZrTypeValue *destination,
                                           const struct SZrTypeValue *source,
                                           const struct SZrTypeValue *typeNameValue);

/** @brief AOT 的 TO_STRUCT 边界：复用解释执行的结构体拆箱及值复制约定。
 * @pre state、destination、source、typeNameValue 有效；typeNameValue 应为字符串。
 * @return false 表示无效实参；true 时结果仍可能为空，目标原型缺失时对象源也可能原样复制。
 * TODO: 目标 struct 原型存在时 ToStruct 会复制任意对象源的字段；核查 typed 边界是否要求源原型匹配。 */
ZR_CORE_API TZrBool ZrCore_Bridge_UnboxTyped(struct SZrState *state,
                                             struct SZrCallInfo *callInfo,
                                             struct SZrTypeValue *destination,
                                             const struct SZrTypeValue *source,
                                             const struct SZrTypeValue *typeNameValue);

#endif
