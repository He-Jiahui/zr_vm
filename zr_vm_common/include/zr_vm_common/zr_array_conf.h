//
// Created by HeJiahui on 2025/6/21.
//

#ifndef ZR_ARRAY_CONF_H
#define ZR_ARRAY_CONF_H
#include "zr_common_conf.h"

// 列表扩容的百分比 不得小于100
#define ZR_ARRAY_INCREASEMENT_MULTIPLIER_PERCENT 200

// 数组结构体
// 注意：SZrArray 并非是 SZrObject，它们是两个独立的类型
// - SZrArray 是一个独立的 C 结构体，用于存储动态数组数据
// - SZrObject 是 VM 对象类型，用于垃圾回收和元方法系统
// - SZrArray 不能嵌入在 SZrObject 中，需要单独分配内存
// - 如果需要在 Object 的 nodeMap 中存储数组，应该使用 ZR_VALUE_TYPE_NATIVE_POINTER 存储 SZrArray 指针
struct SZrArray {
    TBytePtr head;
    TZrSize elementSize;
    TZrSize length;
    TZrSize capacity;
    TBool isValid;
};

typedef struct SZrArray SZrArray;

#endif //ZR_ARRAY_CONF_H
