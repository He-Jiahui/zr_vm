#ifndef ZR_VM_LIB_CONTAINER_POOLING_H
#define ZR_VM_LIB_CONTAINER_POOLING_H

#include "zr_vm_lib_container/conf.h"

/** @brief 返回供容器注册器安装的静态 zr.pooling 描述符。
 *  @return 借用的静态描述符；调用方不释放，注册顺序须先满足 zr.container 依赖。
 */
const ZrLibModuleDescriptor *ZrVmLibContainer_Pooling_GetModuleDescriptor(void);

#endif
