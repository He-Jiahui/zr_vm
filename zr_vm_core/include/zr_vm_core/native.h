//
// Created by HeJiahui on 2025/6/19.
//

#ifndef ZR_VM_CORE_NATIVE_H
#define ZR_VM_CORE_NATIVE_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/value.h"


/** @brief 托管原生载荷对象；valueLength 决定尾随 valueExtend 的有效元素数。 */
struct ZR_STRUCT_ALIGN SZrNativeData {
    SZrRawObject super;
    TZrUInt32 valueLength;
    // SZrRawObject *gcList;
    SZrTypeValue valueExtend[1];
};

#endif // ZR_VM_CORE_NATIVE_H
