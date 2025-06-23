//
// Created by HeJiahui on 2025/6/21.
//

#ifndef ZR_ARRAY_CONF_H
#define ZR_ARRAY_CONF_H
#include "zr_common_conf.h"

// 列表扩容的百分比 不得小于100
#define ZR_ARRAY_INCREASEMENT_MULTIPLIER_PERCENT 200


struct SZrArray {
    TBytePtr head;
    TZrSize elementSize;
    TZrSize length;
    TZrSize capacity;
    TBool isValid;
};

typedef struct SZrArray SZrArray;

#endif //ZR_ARRAY_CONF_H
