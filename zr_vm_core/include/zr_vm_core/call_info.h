//
// Created by HeJiahui on 2025/6/15.
//

#ifndef ZR_VM_CORE_CALL_INFO_H
#define ZR_VM_CORE_CALL_INFO_H
#include "zr_vm_common/zr_type_conf.h"
struct SZrCallInfo {

    TStackIndicator functionIndex;
    TStackIndicator functionTop;

    struct SZrCallInfo *previous;

    struct SZrCallInfo *next;



};


typedef struct SZrCallInfo SZrCallInfo;
#endif //ZR_VM_CORE_CALL_INFO_H
