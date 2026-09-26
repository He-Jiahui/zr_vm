//
// Created by Auto on 2025/01/XX.
//

#ifndef ZR_DEBUG_CONF_H
#define ZR_DEBUG_CONF_H

#include "zr_vm_common/zr_common_conf.h"

// Debug相关常量
#define ZR_DEBUG_MAX_FIELDS 50
#define ZR_DEBUG_SEARCH_THRESHOLD 100

/* 三项运行时检查在默认构建关闭；启用时影响边界、类型及数值路径的诊断成本。
 * 配置者须在包含公共头前统一定义，避免不同编译单元采用不同契约。 */
// 运行时检查默认配置
#ifndef ZR_ENABLE_RUNTIME_BOUNDS_CHECK
#define ZR_ENABLE_RUNTIME_BOUNDS_CHECK ZR_FALSE  // 默认关闭边界检查
#endif

#ifndef ZR_ENABLE_RUNTIME_TYPE_CHECK
#define ZR_ENABLE_RUNTIME_TYPE_CHECK ZR_FALSE   // 默认关闭类型检查
#endif

#ifndef ZR_ENABLE_RUNTIME_RANGE_CHECK
#define ZR_ENABLE_RUNTIME_RANGE_CHECK ZR_FALSE  // 默认关闭范围检查
#endif

#endif // ZR_DEBUG_CONF_H
