//
// Created by HeJiahui on 2025/6/5.
//

#ifndef ZR_VM_COMMON_H
#define ZR_VM_COMMON_H

/* 汇集 core、parser、library 共享的类型、字节码与宿主 ABI 契约；单独使用某一协议时可直接包含对应头文件。 */
#include "zr_vm_common/zr_abi_conf.h"
#include "zr_vm_common/zr_api_conf.h"
#include "zr_vm_common/zr_array_conf.h"
#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_common/zr_constant_reference_conf.h"
#include "zr_vm_common/zr_contract_conf.h"
#include "zr_vm_common/zr_gc_internal_conf.h"
#include "zr_vm_common/zr_hash_conf.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_common/zr_io_conf.h"
#include "zr_vm_common/zr_log_conf.h"
#include "zr_vm_common/zr_memory_conf.h"
#include "zr_vm_common/zr_meta_conf.h"
#include "zr_vm_common/zr_object_conf.h"
#include "zr_vm_common/zr_parser_conf.h"
#include "zr_vm_common/zr_path_conf.h"
#include "zr_vm_common/zr_runtime_limits_conf.h"
#include "zr_vm_common/zr_runtime_sentinel_conf.h"
#include "zr_vm_common/zr_string_conf.h"
#include "zr_vm_common/zr_thread_conf.h"
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_common/zr_version_info.h"
#include "zr_vm_common/zr_vm_conf.h"
#include "zr_vm_common/ssa_platform_contract.h"
/* 两种参数标记都作为完整语句使用，供跨平台回调保留统一签名。 */
#define ZR_TODO_PARAMETER(PARAMETER) ((void) PARAMETER);

#define ZR_UNUSED_PARAMETER(PARAMETER) ((void) PARAMETER);

#endif // ZR_VM_COMMON_H
