//
// Created by HeJiahui on 2025/6/5.
//

#ifndef ZR_VM_CORE_STATE_H
#define ZR_VM_CORE_STATE_H
#include "zr_vm_core/conf.h"
#include "zr_vm_common/zr_type_conf.h"

struct SZrState {
    ZR_GC_HEADER

    TStackIndicator stackTop;
    TStackIndicator stackBottom;
    TStackIndicator stackHead;

    TUInt64 lastProgramCounter;

};

typedef struct SZrState SZrState;


#endif //ZR_VM_CORE_STATE_H
