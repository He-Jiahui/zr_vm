//
// Created by HeJiahui on 2025/6/4.
//
#include "zr_vm_core.h"
#include "zr_vm_core/log.h"
#include "zr_vm_core/value.h"

#include "zr_vm_common/zr_version_info.h"

/** @brief 经无状态元日志向默认输出写入核心库的编译版本与模块名。 */
void Hello(void) { ZrCore_Log_Metaf(ZR_NULL, "zr_vm version is %s\nmodule is %s\n", ZR_VM_VERSION_FULL, ZR_CURRENT_MODULE); }
