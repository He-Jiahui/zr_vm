//
// Created by HeJiahui on 2025/6/19.
//

#ifndef ZR_VM_CORE_NATIVE_H
#define ZR_VM_CORE_NATIVE_H

#include "zr_vm_core/value.h"
#include "zr_vm_core/conf.h"


struct ZR_STRUCT_ALIGN SZrNativeData {
    SZrRawObject super;
    TUInt32 valueLength;
    SZrRawObject *gcList;
    SZrTypeValue valueExtend[1];
};

#endif //ZR_VM_CORE_NATIVE_H
