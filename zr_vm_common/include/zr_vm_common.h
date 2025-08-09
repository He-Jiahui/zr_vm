//
// Created by HeJiahui on 2025/6/5.
//

#ifndef ZR_VM_COMMON_H
#define ZR_VM_COMMON_H

// all in one to include zr_vm_common
#include "zr_vm_common/zr_api_conf.h"
#include "zr_vm_common/zr_array_conf.h"
#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_common/zr_io_conf.h"
#include "zr_vm_common/zr_log_conf.h"
#include "zr_vm_common/zr_memory_conf.h"
#include "zr_vm_common/zr_meta_conf.h"
#include "zr_vm_common/zr_object_conf.h"
#include "zr_vm_common/zr_string_conf.h"
#include "zr_vm_common/zr_thread_conf.h"
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_common/zr_version_info.h"
#include "zr_vm_common/zr_vm_conf.h"


#define ZR_EMPTY_MODULE(FILE_NAME) ZR_API void EMPTY_##FILE_NAME(void);
#define ZR_EMPTY_FILE(FILE_NAME)                                                                                       \
    void EMPTY_##FILE_NAME(void) {}

#define ZR_TODO_PARAMETER(PARAMETER) ((void) PARAMETER);

#define ZR_UNUSED_PARAMETER(PARAMETER) ((void) PARAMETER);

ZR_EMPTY_MODULE(zr_vm_common)

#endif // ZR_VM_COMMON_H
