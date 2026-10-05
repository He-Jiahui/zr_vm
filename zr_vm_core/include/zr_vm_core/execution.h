//
// Created by HeJiahui on 2025/6/15.
//

#ifndef ZR_VM_CORE_EXECUTION_H
#define ZR_VM_CORE_EXECUTION_H
#include "zr_vm_core/conf.h"
struct SZrState;
struct SZrCallInfo;
struct SZrTypeValue;
/** @brief 执行已准备好的 VM 调用帧，供 Function 调用入口进入解释器派发。
 * @pre state、callInfo 非空，帧的 callable 元数据、续行 PC 与栈范围由 PreCall 等入口准备。
 * @note 借用当前 state/帧，在所属执行现场运行；不接管帧所有权，也不另建异常捕获边界。
 * 调用方从 state/帧观察返回与异常状态；可重入的原生调用和 GC 边界可能改变栈地址。
 */
ZR_CORE_API void ZrCore_Execute(struct SZrState *state, struct SZrCallInfo *callInfo);
/** @brief 给 AOT 等调用方复用数值加法、字符串拼接及左操作数 @add 回退语义。
 * @pre state、destination、opA、opB 非空；值及其 GC 可达性由调用方维持。
 * @return 已处理时为真，也可能写入 null；参数或栈目的地恢复失败、元调用异常状态时为假。
 * @note callInfo 可为空以使用当前帧；分配/元调用可移动栈，调用后应从帧重新取得栈指针。
 * 此结果不表示非 null 成功值，也不保证失败时原目的值保持不变。
 */
ZR_CORE_API TZrBool ZrCore_Execution_Add(struct SZrState *state,
                                         struct SZrCallInfo *callInfo,
                                         struct SZrTypeValue *destination,
                                         const struct SZrTypeValue *opA,
                                         const struct SZrTypeValue *opB);
/** @brief 为解释器/AOT bridge 按类型名称查找 class/enum 原型并转换对象。
 * @pre state、destination、source、typeNameValue 非空；调用方维持借用值的有效性和结果可达性。
 * @return 空指针参数为假；正常处理为真，类型名无效或转换不成立也可写入 null。
 * @note callInfo 当前保留在接口形状中，实现不使用它；此 bool 不等同于构造成功或无分配失败。
 * 资源 class 转换还初始化 unique 所有权并置为 CONSTRUCTING；后续构造生命周期由调用方完成。
 */
ZR_CORE_API TZrBool ZrCore_Execution_ToObject(struct SZrState *state,
                                              struct SZrCallInfo *callInfo,
                                              struct SZrTypeValue *destination,
                                              const struct SZrTypeValue *source,
                                              const struct SZrTypeValue *typeNameValue);
/** @brief 为解释器/AOT bridge 按类型名称查找 struct 原型并转换对象。
 * @pre state、destination、source、typeNameValue 非空；调用方维持借用值的有效性和结果可达性。
 * @return 空指针参数为假；正常处理为真，类型名无效或转换不成立也可写入 null。
 * @note callInfo 当前保留在接口形状中，实现不使用它；此 bool 不等同于构造成功或无分配失败。
 * 未找到原型时仍可复制已有对象；该接口不验证所有 struct 字段的静态类型。
 */
ZR_CORE_API TZrBool ZrCore_Execution_ToStruct(struct SZrState *state,
                                              struct SZrCallInfo *callInfo,
                                              struct SZrTypeValue *destination,
                                              const struct SZrTypeValue *source,
                                              const struct SZrTypeValue *typeNameValue);


#endif //ZR_VM_CORE_EXECUTION_H
