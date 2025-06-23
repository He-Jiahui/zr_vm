//
// Created by HeJiahui on 2025/6/24.
//

#ifndef ZR_HASH_CONF_H
#define ZR_HASH_CONF_H
#include "zr_common_conf.h"

struct SZrHashSet {
    SZrHashRawObject **buckets;
    TZrSize bucketCount;
    TZrSize elementCount;
    TZrSize capacity;
    TZrSize resizeThreshold;
    TBool isValid;
};

typedef struct SZrHashSet SZrHashSet;

#endif //ZR_HASH_CONF_H
