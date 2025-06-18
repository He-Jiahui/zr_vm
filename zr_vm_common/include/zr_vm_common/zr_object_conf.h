//
// Created by HeJiahui on 2025/6/18.
//

#ifndef ZR_OBJECT_CONF_H
#define ZR_OBJECT_CONF_H

#include "zr_vm_common/zr_type_conf.h"

struct ZR_STRUCT_ALIGN SZrRawObject {
    struct SZrRawObject *next;
    EZrValueType type;
};

typedef struct SZrRawObject SZrRawObject;

ZR_FORCE_INLINE void ZrObjectInit(SZrRawObject *super, EZrValueType type) {
    super->next = ZR_NULL;
    super->type = type;
}

#endif //ZR_OBJECT_CONF_H
