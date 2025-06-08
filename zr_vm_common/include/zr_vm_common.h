//
// Created by HeJiahui on 2025/6/5.
//

#ifndef ZR_VM_COMMON_H
#define ZR_VM_COMMON_H

#include "zr_vm_common/zr_version_info.h"
#include "zr_vm_common/zr_api_conf.h"


#define ZR_EMPTY_MODULE(FILE_NAME) ZR_API void EMPTY_##FILE_NAME();
#define ZR_EMPTY_FILE(FILE_NAME) void EMPTY_##FILE_NAME(){}


ZR_EMPTY_MODULE(zr_vm_common)

#endif //ZR_VM_COMMON_H
