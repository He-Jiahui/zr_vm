//
// zr.system.assembly native callbacks.
//

#ifndef ZR_VM_LIB_SYSTEM_ASSEMBLY_H
#define ZR_VM_LIB_SYSTEM_ASSEMBLY_H

#include "zr_vm_lib_system/conf.h"

/** @brief 查询当前项目输出程序集中的逻辑资源；程序集不可用或资源缺失均返回 false。 */
TZrBool ZrSystem_Assembly_ResourceExists(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 将当前项目程序集资源作为文本交给脚本；资源缺失或读取失败会抛运行时错误。
 *  @note 与 readResourceBytes 共用逻辑资源名校验，调用方无需提供磁盘路径。 */
TZrBool ZrSystem_Assembly_ReadResourceText(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 将同一资源保留为逐字节整数数组，供二进制消费者使用。 */
TZrBool ZrSystem_Assembly_ReadResourceBytes(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_SYSTEM_ASSEMBLY_H
