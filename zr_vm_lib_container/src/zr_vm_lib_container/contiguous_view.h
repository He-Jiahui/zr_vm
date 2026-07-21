//
// Runtime callbacks for protocol-driven contiguous views.
//

#ifndef ZR_VM_LIB_CONTAINER_CONTIGUOUS_VIEW_H
#define ZR_VM_LIB_CONTAINER_CONTIGUOUS_VIEW_H

#include "zr_vm_lib_container/conf.h"

TZrBool ZrVmLibContainer_ContiguousView_FromArray(
        ZrLibCallContext *context,
        SZrTypeValue *result);
TZrBool ZrVmLibContainer_ContiguousView_Construct(
        ZrLibCallContext *context,
        SZrTypeValue *result);
TZrBool ZrVmLibContainer_ContiguousView_Slice(
        ZrLibCallContext *context,
        SZrTypeValue *result);
TZrBool ZrVmLibContainer_ContiguousView_AsReadOnly(
        ZrLibCallContext *context,
        SZrTypeValue *result);
TZrBool ZrVmLibContainer_ContiguousView_GetItem(
        ZrLibCallContext *context,
        SZrTypeValue *result);
TZrBool ZrVmLibContainer_ContiguousView_SetItem(
        ZrLibCallContext *context,
        SZrTypeValue *result);

#endif // ZR_VM_LIB_CONTAINER_CONTIGUOUS_VIEW_H
