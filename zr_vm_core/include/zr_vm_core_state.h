//
// Created by HeJiahui on 2025/6/5.
//

#ifndef ZR_VM_CORE_STATE_H
#define ZR_VM_CORE_STATE_H
#include "zr_type_conf.h"

struct SZrState {
    ZR_GC_HEADER;

    TStackIndicator stackTop;
    TStackIndicator stackTail;
    TStackIndicator stackHead;

    TUInt64 lastProgramCounter;

};

#endif //ZR_VM_CORE_STATE_H
