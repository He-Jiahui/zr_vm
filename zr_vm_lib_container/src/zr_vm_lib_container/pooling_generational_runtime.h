#ifndef ZR_VM_LIB_CONTAINER_POOLING_GENERATIONAL_RUNTIME_H
#define ZR_VM_LIB_CONTAINER_POOLING_GENERATIONAL_RUNTIME_H

#include "zr_vm_library/native_binding.h"

TZrBool ZrPooling_Generational_Deliver(
        ZrLibCallContext *context,
        SZrTypeValue *result);
TZrBool ZrPooling_Generational_IsLive(
        ZrLibCallContext *context,
        SZrTypeValue *result);
TZrBool ZrPooling_Generational_Recycle(
        ZrLibCallContext *context,
        SZrTypeValue *result);
TZrBool ZrPooling_Generational_TryRead(
        ZrLibCallContext *context,
        SZrTypeValue *result);
TZrBool ZrPooling_Generational_TryBorrow(
        ZrLibCallContext *context,
        SZrTypeValue *result);
TZrBool ZrPooling_Generational_RefClose(
        ZrLibCallContext *context,
        SZrTypeValue *result);
TZrBool ZrPooling_Generational_RefValue(
        ZrLibCallContext *context,
        SZrTypeValue *result);

#endif // ZR_VM_LIB_CONTAINER_POOLING_GENERATIONAL_RUNTIME_H
