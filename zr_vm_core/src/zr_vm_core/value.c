//
// Created by HeJiahui on 2025/6/20.
//
#include "zr_vm_core/value.h"

void ZrValueInitAsNull(SZrTypeValue *value) {
    value->type = ZR_VALUE_TYPE_NULL;
}
