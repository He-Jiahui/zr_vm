//
// zr.system.console native callbacks.
//

#ifndef ZR_VM_LIB_SYSTEM_CONSOLE_H
#define ZR_VM_LIB_SYSTEM_CONSOLE_H

#include "zr_vm_lib_system/conf.h"

/** @brief 将可转为字符串的值写入标准输出，不补换行；经 VM 日志通道供宿主拦截。 */
TZrBool ZrSystem_Console_Print(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 向标准输出写入文本并补换行，沿用同一日志通道。 */
TZrBool ZrSystem_Console_PrintLine(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 将诊断文本写入标准错误通道，不补换行。 */
TZrBool ZrSystem_Console_PrintError(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 将诊断文本写入标准错误通道并补换行。 */
TZrBool ZrSystem_Console_PrintErrorLine(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 从标准输入读一个 UTF-8 码点，EOF 返回 null，非法序列抛错。
 *  @note 读取前先刷新默认输出，保证交互提示在等待输入前可见。 */
TZrBool ZrSystem_Console_Read(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 从标准输入读一行 UTF-8 文本；行首 EOF 返回 null，末尾 CRLF 会规范成无换行文本。 */
TZrBool ZrSystem_Console_ReadLine(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_SYSTEM_CONSOLE_H
