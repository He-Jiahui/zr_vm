//
// Created by HeJiahui on 2025/6/4.
//
#include "zr_vm_core.h"
#include "zr_vm_core/value.h"


#include <stdio.h>

#include "zr_vm_common/zr_version_info.h"

void Hello(void) { printf("zr_vm version is %s\nmodule is %s\n", ZR_VM_VERSION_FULL, ZR_CURRENT_MODULE); }
