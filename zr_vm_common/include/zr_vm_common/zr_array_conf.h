//
// Created by HeJiahui on 2025/6/21.
//

#ifndef ZR_ARRAY_CONF_H
#define ZR_ARRAY_CONF_H
#include "zr_common_conf.h"

/* core 数组按容量比例扩张；调用方不能把该比例当作元素个数或字节数。 */
#define ZR_ARRAY_INCREASEMENT_MULTIPLIER_PERCENT 200

/** @brief 计算编译期固定数组的元素个数；value 必须是数组而非退化后的指针。 */
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))

/** @brief core 动态数组的原生存储视图；head 由数组 API 分配/释放，length 不得超过 capacity。
 *  这是 C 容器，不是 VM 的 GC SZrObject；持有其地址的对象需另行管理原生内存生命周期。 */
struct SZrArray {
    TZrBytePtr head;
    TZrSize elementSize;
    TZrSize length;
    TZrSize capacity;
    TZrBool isValid;
};

typedef struct SZrArray SZrArray;

#endif //ZR_ARRAY_CONF_H
