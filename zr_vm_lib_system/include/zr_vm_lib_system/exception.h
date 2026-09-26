//
// zr.system.exception native callbacks.
//

#ifndef ZR_VM_LIB_SYSTEM_EXCEPTION_H
#define ZR_VM_LIB_SYSTEM_EXCEPTION_H

#include "zr_vm_lib_system/conf.h"

/** @brief 在全局状态保留一个未处理异常观察函数，后续注册会替换旧值。
 *  @note 存储不等于分发；执行宿主须在未处理异常出口读取并调用该观察函数。 */
TZrBool ZrSystem_Exception_RegisterUnhandledException(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 初始化 Error 及其派生实例共享的 message、stacks、exception 字段。
 *  @pre 由类型描述符的构造元方法调用，self 必须是当前异常对象。 */
TZrBool ZrSystem_Exception_Constructor(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_SYSTEM_EXCEPTION_H
