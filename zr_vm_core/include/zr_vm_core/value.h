//
// Created by HeJiahui on 2025/6/15.
//

#ifndef ZR_VM_CORE_VALUE_H
#define ZR_VM_CORE_VALUE_H

#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/type.h"

struct SZrValue {
    EZrValueType type;
    union {
        ZGcObject *object;
        TUInt64 uint64;
        TInt64 int64;
        TDouble float64;
        TFloat float32;
        TBool bool;

    };
};

typedef struct SZrValue SZrValue;

#endif //ZR_VM_CORE_VALUE_H
