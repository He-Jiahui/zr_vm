//
// Shared configuration for zr.system public headers.
//

#ifndef ZR_VM_LIB_SYSTEM_CONF_H
#define ZR_VM_LIB_SYSTEM_CONF_H

#include "zr_vm_library.h"

#define ZR_VM_LIB_SYSTEM_API ZR_API

/* 文件 API 与 VM 路径上限共用同一边界；POSIX 建目录权限由仓库统一配置。 */
#define ZR_SYSTEM_FS_PATH_MAX ZR_VM_PATH_LENGTH_MAX
#define ZR_SYSTEM_FS_DIRECTORY_MODE ZR_VM_POSIX_DIRECTORY_CREATE_MODE

#endif // ZR_VM_LIB_SYSTEM_CONF_H
