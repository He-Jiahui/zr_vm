//
// Created by HeJiahui on 2025/6/24.
//

#ifndef ZR_HASH_CONF_H
#define ZR_HASH_CONF_H
#include "zr_common_conf.h"

/* 文件、元数据及跨构建摘要共享稳定哈希参数；修改种子会改变持久化身份与缓存键。 */
#define ZR_STABLE_HASH_FILE_CHUNK_BUFFER_LENGTH 4096U
#define ZR_STABLE_HASH_HEX_BUFFER_LENGTH 32U
#define ZR_STABLE_HASH_HEX_PRINTF_FORMAT "%016llx"

#define ZR_STABLE_HASH_FNV1A64_OFFSET_BASIS 1469598103934665603ULL
#define ZR_STABLE_HASH_FNV1A64_PRIME 1099511628211ULL

/* 反射成员/方法键使用不同起始偏移，并结合 owner 与类别生成身份；偏移本身不保证无碰撞。 */
#define ZR_RUNTIME_REFLECTION_MEMBER_HASH_BASE 1ULL
#define ZR_RUNTIME_REFLECTION_METHOD_HASH_BASE 100ULL


#endif // ZR_HASH_CONF_H
